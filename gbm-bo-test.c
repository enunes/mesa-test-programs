#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <gbm.h>
#include <png.h>

#include <epoxy/gl.h>
#include <epoxy/egl.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

GLuint program;

#define TARGET_SIZE 1024

void Render(void)
{
	GLfloat vertex[] = {
		-1, -1, 0,
		-1, 1, 0,
		1, 1, 0,
		1, -1, 0
	};

	GLint position = glGetAttribLocation(program, "positionIn");
	glEnableVertexAttribArray(position);
	glVertexAttribPointer(position, 3, GL_FLOAT, 0, 0, vertex);

	assert(glGetError() == GL_NO_ERROR);

	glClear(GL_COLOR_BUFFER_BIT
		//| GL_DEPTH_BUFFER_BIT
		);
	printf("%x\n", glGetError());
	assert(glGetError() == GL_NO_ERROR);

	glDrawArrays(GL_TRIANGLES, 0, 3);

	assert(glGetError() == GL_NO_ERROR);

//	eglSwapBuffers(display, surface);

	GLubyte result[TARGET_SIZE * TARGET_SIZE * 4] = {0};
	glReadPixels(0, 0, TARGET_SIZE, TARGET_SIZE, GL_RGBA, GL_UNSIGNED_BYTE, result);
	assert(glGetError() == GL_NO_ERROR);
}

GLuint LoadShader(const char *name, GLenum type)
{
	FILE *f;
	size_t size;
	char *buff;
	GLuint shader;
	GLint compiled;
	const GLchar *source[1];

	assert((f = fopen(name, "r")) != NULL);

	// get file size
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);

	assert((buff = malloc(size)) != NULL);
	assert(fread(buff, 1, size, f) == size);
	source[0] = buff;
	fclose(f);
	shader = glCreateShader(type);
	glShaderSource(shader, 1, source, (GLint *)&size);
	glCompileShader(shader);
	free(buff);
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (!compiled) {
		GLint infoLen = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
		if (infoLen > 1) {
			char *infoLog = malloc(infoLen);
			glGetShaderInfoLog(shader, infoLen, NULL, infoLog);
			fprintf(stderr, "Error compiling shader %s:\n%s\n", name, infoLog);
			free(infoLog);
		}
		glDeleteShader(shader);
		return 0;
	}

	return shader;
}

void InitGLES(void)
{
	GLint linked;
	GLuint vertexShader;
	GLuint fragmentShader;
	assert((vertexShader = LoadShader("vert.glsl", GL_VERTEX_SHADER)) != 0);
	assert((fragmentShader = LoadShader("frag.glsl", GL_FRAGMENT_SHADER)) != 0);
	assert((program = glCreateProgram()) != 0);
	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);
	glLinkProgram(program);
	glGetProgramiv(program, GL_LINK_STATUS, &linked);
	if (!linked) {
		GLint infoLen = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLen);
		if (infoLen > 1) {
			char *infoLog = malloc(infoLen);
			glGetProgramInfoLog(program, infoLen, NULL, infoLog);
			fprintf(stderr, "Error linking program:\n%s\n", infoLog);
			free(infoLog);
		}
		glDeleteProgram(program);
		exit(1);
	}

	glClearColor(0, 0, 0, 0);
	glViewport(0, 0, TARGET_SIZE, TARGET_SIZE);

	glUseProgram(program);
}

#define ALIGN_ON_POW2(n, align) ((n + align - 1) & ~(align - 1))

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define bit_name_fn(res)					\
const char * res##_str(int type) {				\
	unsigned int i;						\
	const char *sep = "";					\
	for (i = 0; i < ARRAY_SIZE(res##_names); i++) {		\
		if (type & (1 << i)) {				\
			printf("%s%s", sep, res##_names[i]);	\
			sep = ", ";				\
		}						\
	}							\
	return NULL;						\
}

static const char *mode_type_names[] = {
	"builtin",
	"clock_c",
	"crtc_c",
	"preferred",
	"default",
	"userdef",
	"driver",
};

static bit_name_fn(mode_type)

static const char *mode_flag_names[] = {
	"phsync",
	"nhsync",
	"pvsync",
	"nvsync",
	"interlace",
	"dblscan",
	"csync",
	"pcsync",
	"ncsync",
	"hskew",
	"bcast",
	"pixmux",
	"dblclk",
	"clkdiv2"
};

static bit_name_fn(mode_flag)

static void dump_mode(drmModeModeInfo *mode)
{
	printf("  %s %d %d %d %d %d %d %d %d %d %d",
	       mode->name,
	       mode->vrefresh,
	       mode->hdisplay,
	       mode->hsync_start,
	       mode->hsync_end,
	       mode->htotal,
	       mode->vdisplay,
	       mode->vsync_start,
	       mode->vsync_end,
	       mode->vtotal,
	       mode->clock);

	printf(" flags: ");
	mode_flag_str(mode->flags);
	printf("; type: ");
	mode_type_str(mode->type);
	printf("\n");
}

int main(int argc, char **argv)
{
	putenv("EGL_LOG_LEVEL=warning"); putenv("MESA_DEBUG=1"); putenv("LIBGL_DEBUG=verbose");

	int gpu_alloc = 0;

	if (argc > 1) {
		if (strcmp(argv[1], "gpu_alloc") == 0) {
			gpu_alloc = 1;
		}
	}

	int ret;
	const char *hdmi_dev = "/dev/dri/card0";

	struct gbm_device *gpu_gbm;
	const char *gpu_dev = "/dev/dri/renderD128";

	printf("opening %s\n", hdmi_dev);
	int const hdmi_fd = open(hdmi_dev, O_RDWR | O_CLOEXEC);
	assert(hdmi_fd >= 0);

	drmModeRes * __restrict resources;
	drmModeConnector * __restrict connector = NULL;
	drmModeModeInfo* __restrict chosen_resolution = NULL;
	drmModeEncoder * __restrict encoder = NULL;
	int i;

	resources = drmModeGetResources(hdmi_fd);
	if (!resources) {
		fprintf(stderr, "drmModeGetResources failed\n");
		return 1;
	}

	for (i = 0; i < resources->count_connectors; i++) {
		connector = drmModeGetConnector(hdmi_fd, resources->connectors[i]);
		if (connector == NULL)
			continue;

		if (connector->connection == DRM_MODE_CONNECTED &&
				connector->count_modes > 0)
			break;

		drmModeFreeConnector(connector);
	}

	if (i == resources->count_connectors) {
		fprintf(stderr, "No currently active connector found.\n");
		return 1;
	}

	for (int_fast32_t m = 0; m < connector->count_modes; m++) {
		drmModeModeInfo *tested_mode = &connector->modes[m];

		dump_mode(tested_mode);

		if (tested_mode->type & DRM_MODE_TYPE_PREFERRED) {
			chosen_resolution = tested_mode;
			break;
		}
	}
	dump_mode(chosen_resolution);

	if (!chosen_resolution) {
		fprintf(stderr, "No preferred resolution on the selected connector %u ?\n",
				connector->connector_id);
		return 1;
	}


	for (i = 0; i < resources->count_encoders; i++) {
		encoder = drmModeGetEncoder(hdmi_fd, resources->encoders[i]);

		if (encoder == NULL)
			continue;

		if (encoder->encoder_id == connector->encoder_id)
			break;

		drmModeFreeEncoder(encoder);
	}

	printf("opening %s\n", gpu_dev);
	int const gpu_fd = open(gpu_dev, O_RDWR | O_CLOEXEC);
	assert(gpu_fd >= 0);

	gpu_gbm = gbm_create_device(gpu_fd);
	assert(gpu_gbm != NULL);

	int dma_buf_fd;
	int stride;
	uint32_t prime_handle;
	struct gbm_bo *gpu_bo;

	if (gpu_alloc) {
		gpu_bo = gbm_bo_create(gpu_gbm,
				chosen_resolution->hdisplay, chosen_resolution->vdisplay,
				GBM_FORMAT_XRGB8888,
				GBM_BO_USE_RENDERING | GBM_BO_USE_SCANOUT);

		if (gpu_bo == NULL) {
			printf("Could not create bo : %s (%d)\n",
					strerror(errno), errno);
			return -1;
		}
		dma_buf_fd = gbm_bo_get_fd(gpu_bo);
		printf("[gpu] Exported buffer FD : %d\n", dma_buf_fd);

		stride = gbm_bo_get_stride(gpu_bo);

		ret = drmPrimeFDToHandle(hdmi_fd, dma_buf_fd, &prime_handle);
		if (ret) {
			printf("Could not import buffer : %s (%d) - FD : %d\n",
					strerror(errno), errno, dma_buf_fd);
			return -1;
		}

		printf("[display] Imported buffer FD : %d\n", dma_buf_fd);
	}
	else {
		struct drm_mode_create_dumb create_request = {
			.width  = chosen_resolution->hdisplay,
			.height = chosen_resolution->vdisplay,
			.bpp    = 32
		};
		ret = ioctl(hdmi_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_request);
		if (ret) {
			printf("Dumb Buffer Object Allocation request of %ux%u@%u failed : %s\n",
					create_request.width, create_request.height, create_request.bpp, strerror(ret));
			return -1;
		}
		printf("Dumb Buffer Object Allocation request of %ux%u@%u succeeded!\n",
				create_request.width, create_request.height, create_request.bpp);

		struct drm_prime_handle prime_request = {
			.handle = create_request.handle,
			.flags  = DRM_CLOEXEC | DRM_RDWR,
			.fd     = -1
		};

		ret = ioctl(hdmi_fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &prime_request);
		dma_buf_fd = prime_request.fd;
		if (ret || dma_buf_fd < 0) {
			printf("Could not export buffer : %s (%d) - FD : %d\n",
					strerror(ret), ret, dma_buf_fd);
			return -1;
		}
		prime_handle = create_request.handle;
		stride = create_request.pitch;
		printf("[display] Exported buffer FD : %d\n", dma_buf_fd);

		struct gbm_import_fd_data data;
		data.fd = dma_buf_fd;
		data.width  = chosen_resolution->hdisplay;
		data.height = chosen_resolution->vdisplay;
		data.stride = create_request.pitch;
		data.format = GBM_FORMAT_XRGB8888;
		//	data.format = GBM_FORMAT_ARGB8888;
		gpu_bo = gbm_bo_import(gpu_gbm, GBM_BO_IMPORT_FD, &data, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
		if (gpu_bo == NULL) {
			printf("Could not import buffer : %s (%d) - FD : %d\n",
					strerror(errno), errno, dma_buf_fd);
			return -1;
		}

		printf("[gpu] Imported buffer FD : %d\n", dma_buf_fd);
	}

	uint32_t fb_id;
	ret = drmModeAddFB(
			hdmi_fd, chosen_resolution->hdisplay, chosen_resolution->vdisplay,
			24, 32, stride, prime_handle, &fb_id
			);
	if (ret) {
		printf("Could not add a framebuffer using drmModeAddFB : %s\n", strerror(ret));
		return -1;
	}

	drmModeSetCrtc(hdmi_fd, encoder->crtc_id, fb_id, 0, 0,
			&connector->connector_id, 1, chosen_resolution);

	EGLDisplay dpy;
	dpy = eglGetDisplay(gpu_gbm);

	EGLint major, minor;
	const char *ver;//, *extensions;
	assert(eglInitialize(dpy, &major, &minor) == EGL_TRUE);
	ver = eglQueryString(dpy, EGL_VERSION);
//	extensions = eglQueryString(dpy, EGL_EXTENSIONS);

	printf("ver = %s\n", ver);

	EGLContext ctx;
	static const EGLint ctx_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};
	EGLint egl_config_attribs[] = {
		EGL_BUFFER_SIZE,	32,
		EGL_DEPTH_SIZE,		EGL_DONT_CARE,
		EGL_STENCIL_SIZE,	EGL_DONT_CARE,
		EGL_RENDERABLE_TYPE,	EGL_OPENGL_ES2_BIT,
		EGL_SURFACE_TYPE,	EGL_WINDOW_BIT,
		EGL_NONE,
	};

	EGLint num_configs;
	assert(eglGetConfigs(dpy, NULL, 0, &num_configs) == EGL_TRUE);

	EGLConfig *configs = malloc(num_configs * sizeof(EGLConfig));
	assert(eglChooseConfig(dpy, egl_config_attribs, configs, num_configs, &num_configs) == EGL_TRUE);
	assert(num_configs);
	printf("num config %d\n", num_configs);

//	eglBindAPI(EGL_OPENGL_API);
	eglBindAPI(EGL_OPENGL_ES_API);
	ctx = eglCreateContext(dpy, configs[0], EGL_NO_CONTEXT, ctx_attribs);

	eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx);

	GLuint fb, color_rb, depth_rb;
	glGenFramebuffers(1, &fb);
	glBindFramebuffer(GL_FRAMEBUFFER_EXT, fb);

	EGLImageKHR image;
	image = eglCreateImageKHR(dpy, NULL, EGL_NATIVE_PIXMAP_KHR, gpu_bo, NULL);

	/* Set up render buffer... */
	glGenRenderbuffers(1, &color_rb);
	glBindRenderbuffer(GL_RENDERBUFFER_EXT, color_rb);
	glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, image);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, color_rb);

	/* and depth buffer */
	glGenRenderbuffers(1, &depth_rb);
	glBindRenderbuffer(GL_RENDERBUFFER_EXT, depth_rb);
	glRenderbufferStorage(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT16, chosen_resolution->hdisplay, chosen_resolution->vdisplay);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, depth_rb);

	if ((glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT)) != GL_FRAMEBUFFER_COMPLETE) {
		fprintf(stderr, "something bad happened\n");
		exit(-1);
	}

	InitGLES();
	Render();

	sleep(10);
	return 0;
}
