
/* Test program for offcreen OpenGL ES rendering with EGL, created by glueing a
 * bunch of other test programs together. Outputs image to a tif image. */

#if 1
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <gbm.h>
#include <epoxy/gl.h>
#include <epoxy/egl.h>
#else
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#endif
#include <tiffio.h>
#include <string.h>
#include <assert.h>

static void _check_gl_error(const char *file, int line)
{
	const char *error = NULL;
	GLenum err;

	while((err = glGetError()) != GL_NO_ERROR) {

		switch(err) {
			case GL_INVALID_OPERATION:
				error="GL_INVALID_OPERATION";
				break;
			case GL_INVALID_ENUM:
				error="GL_INVALID_ENUM";
				break;
			case GL_INVALID_VALUE:
				error="GL_INVALID_VALUE";
				break;
			case GL_OUT_OF_MEMORY:
				error="GL_OUT_OF_MEMORY";
				break;
			case GL_INVALID_FRAMEBUFFER_OPERATION:
				error="GL_INVALID_FRAMEBUFFER_OPERATION";
				break;
		}

		fprintf(stderr, "%s - %s:%d\n", error, file, line);
	}
	if (error)
		exit(1);
}
#define CHECK_GL_ERROR() _check_gl_error(__FILE__,__LINE__)

static int buffer_to_tiff(const char *fname, unsigned char *buf, unsigned int w, unsigned int h)
{
	unsigned int i;
	TIFF* new_tif;

	new_tif = TIFFOpen(fname, "w");

	if (!new_tif) {
		fprintf(stderr, "buffer_to_tiff: error opening %s\n", fname);
		return -1;
	}

	TIFFSetField(new_tif, TIFFTAG_IMAGEWIDTH, w);  // set the width of the image
	TIFFSetField(new_tif, TIFFTAG_IMAGELENGTH, h);  // set the height of the image
	TIFFSetField(new_tif, TIFFTAG_SAMPLESPERPIXEL, 4);   // set number of channels per pixel
	TIFFSetField(new_tif, TIFFTAG_BITSPERSAMPLE, 8);    // set the size of the channels
	TIFFSetField(new_tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);    // set the origin of the image.

	TIFFSetField(new_tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
	TIFFSetField(new_tif, TIFFTAG_PHOTOMETRIC, 2); // 1 = b&w , 2 = rgb , 6 = ycbcr

	// We set the strip size of the file to be size of one row of pixels
	TIFFSetField(new_tif, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(new_tif, w));

	//Now writing image to the file one strip at a time
	for (i = 0; i < h; i++) {
		unsigned int pos = i*w*4;
		if (TIFFWriteScanline(new_tif, &buf[pos], i, 0) < 0) {
			fprintf(stderr, "buffer_to_tiff: something happened\n");
			break;
		}
	}

	TIFFClose(new_tif);
	return 0;
}

static void printinfo(EGLDisplay *egl_dpy)
{
	const char *s;
	s = eglQueryString(*egl_dpy, EGL_VERSION);
	printf("EGL_VERSION = %s\n", s);

	s = eglQueryString(*egl_dpy, EGL_VENDOR);
	printf("EGL_VENDOR = %s\n", s);

	s = eglQueryString(*egl_dpy, EGL_EXTENSIONS);
	printf("EGL_EXTENSIONS = %s\n", s);

	s = eglQueryString(*egl_dpy, EGL_CLIENT_APIS);
	printf("EGL_CLIENT_APIS = %s\n", s);

	printf("GL_RENDERER   = %s\n", (char *) glGetString(GL_RENDERER));
	printf("GL_VERSION    = %s\n", (char *) glGetString(GL_VERSION));
	printf("GL_VENDOR     = %s\n", (char *) glGetString(GL_VENDOR));
	printf("GL_EXTENSIONS = %s\n", (char *) glGetString(GL_EXTENSIONS));
}

/* new window size or exposure */
static void reshape(int width, int height)
{
	glViewport(0, 0, (GLint) width, (GLint) height);
}

#define FLOAT_TO_FIXED(X)   ((X) * 65535.0)

static GLfloat view_rotx = 0.0;

static GLint u_matrix = -1;
static GLint attr_pos = 0, attr_color = 1;


static void make_z_rot_matrix(GLfloat angle, GLfloat *m)
{
	float c = cos(angle * M_PI / 180.0);
	float s = sin(angle * M_PI / 180.0);
	int i;
	for (i = 0; i < 16; i++)
		m[i] = 0.0;
	m[0] = m[5] = m[10] = m[15] = 1.0;

	m[0] = c;
	m[1] = s;
	m[4] = -s;
	m[5] = c;
}

static void make_scale_matrix(GLfloat xs, GLfloat ys, GLfloat zs, GLfloat *m)
{
	int i;
	for (i = 0; i < 16; i++)
		m[i] = 0.0;
	m[0] = xs;
	m[5] = ys;
	m[10] = zs;
	m[15] = 1.0;
}

static void mul_matrix(GLfloat *prod, const GLfloat *a, const GLfloat *b)
{
#define A(row,col)  a[(col<<2)+row]
#define B(row,col)  b[(col<<2)+row]
#define P(row,col)  p[(col<<2)+row]
	GLfloat p[16];
	GLint i;
	for (i = 0; i < 4; i++) {
		const GLfloat ai0=A(i,0),  ai1=A(i,1),  ai2=A(i,2),  ai3=A(i,3);
		P(i,0) = ai0 * B(0,0) + ai1 * B(1,0) + ai2 * B(2,0) + ai3 * B(3,0);
		P(i,1) = ai0 * B(0,1) + ai1 * B(1,1) + ai2 * B(2,1) + ai3 * B(3,1);
		P(i,2) = ai0 * B(0,2) + ai1 * B(1,2) + ai2 * B(2,2) + ai3 * B(3,2);
		P(i,3) = ai0 * B(0,3) + ai1 * B(1,3) + ai2 * B(2,3) + ai3 * B(3,3);
	}
	memcpy(prod, p, sizeof(p));
#undef A
#undef B
#undef PROD
}


static void draw(void)
{
	static const GLfloat verts[3][2] = {
		{ -0.9f, -0.7f },
		{  0.6f, -0.8f },
		{  0.1f,  0.6f }
	};
	static const GLfloat colors[3][3] = {
		{ 0.0f, 0.0f, 1.0f },
		{ 0.0f, 0.0f, 1.0f },
		{ 0.0f, 0.0f, 1.0f }
	};
	GLfloat mat[16], rot[16], scale[16];

	/* Set modelview/projection matrix */
	make_z_rot_matrix(view_rotx, rot);
	make_scale_matrix(0.5, 0.5, 0.5, scale);
	mul_matrix(mat, rot, scale);
//	glUniformMatrix4fv(u_matrix, 1, GL_FALSE, mat);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glVertexAttribPointer(attr_pos, 2, GL_FLOAT, GL_FALSE, 0, verts);
	glVertexAttribPointer(attr_color, 3, GL_FLOAT, GL_FALSE, 0, colors);
	glEnableVertexAttribArray(attr_pos);
	glEnableVertexAttribArray(attr_color);

	glDrawArrays(GL_TRIANGLES, 0, 3);

	glDisableVertexAttribArray(attr_pos);
	glDisableVertexAttribArray(attr_color);
}

static void create_shaders(void)
{
	static const char *vertShaderText =
//		"uniform mat4 modelviewProjection;\n"
		"attribute vec4 pos;\n"
		"attribute vec4 color;\n"
		"varying vec4 v_color;\n"
		"void main() {\n"
//		"   gl_Position = modelviewProjection * pos;\n"
		"   gl_Position = pos;\n"
		"   v_color = color;\n"
		"}\n";
	static const char *fragShaderText =
		"precision mediump float;\n"
		"varying vec4 v_color;\n"
		"void main() {\n"
		"   gl_FragColor = vec4(0.0, 1.0, 0.0, 0.0);\n"
		"}\n";

	GLuint vertShader, fragShader,  program;
	GLint stat;

	vertShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertShader, 1, (const char **) &vertShaderText, NULL);
	glCompileShader(vertShader);
	glGetShaderiv(vertShader, GL_COMPILE_STATUS, &stat);
	if (!stat) {
		fprintf(stderr, "Error: vertex shader did not compile!\n");
		exit(1);
	}

	fragShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragShader, 1, (const char **) &fragShaderText, NULL);
	glCompileShader(fragShader);
	glGetShaderiv(fragShader, GL_COMPILE_STATUS, &stat);
	if (!stat) {
		fprintf(stderr, "Error: fragment shader did not compile!\n");
		exit(1);
	}

	program = glCreateProgram();
	glAttachShader(program, vertShader);
	glAttachShader(program, fragShader);
	glLinkProgram(program);

	glGetProgramiv(program, GL_LINK_STATUS, &stat);
	if (!stat) {
		char log[1000];
		GLsizei len;
		glGetProgramInfoLog(program, 1000, &len, log);
		fprintf(stderr, "Error: linking:\n%s\n", log);
		exit(1);
	}

	glUseProgram(program);

	/* test setting attrib locations */
	glBindAttribLocation(program, attr_pos, "pos");
	glBindAttribLocation(program, attr_color, "color");
	glLinkProgram(program);  /* needed to put attribs into effect */

//	u_matrix = glGetUniformLocation(program, "modelviewProjection");
//	printf("Uniform modelviewProjection at %d\n", u_matrix);
//	printf("Attrib pos at %d\n", attr_pos);
//	printf("Attrib color at %d\n", attr_color);
}

static void init(void)
{
	/* test code */
	typedef void (*proc)();
	proc p = eglGetProcAddress("glMapBufferOES");
	assert(p);
	/* test code */

	glClearColor(0.0, 0.0, 1.0, 1.0);

	create_shaders();
}

static const EGLint ctx_attribs[] = {
	EGL_CONTEXT_CLIENT_VERSION, 2,
	EGL_NONE
};

int main(int argc, char *argv[])
{
	(void)argc; (void)argv;
//	putenv("EGL_LOG_LEVEL=debug");
	putenv("EGL_LOG_LEVEL=warning");
	putenv("MESA_DEBUG=1");
	putenv("LIBGL_DEBUG=verbose");
	static const EGLint pi32ConfigAttribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER,
		EGL_NONE
	};

	EGLDisplay eglDisplay = eglGetDisplay((EGLNativeDisplayType)0);

	eglInitialize(eglDisplay, 0, 0);

	int iConfigs;
	EGLConfig eglConfig;
	printf("calling eglChooseConfig()\n");
	eglChooseConfig(eglDisplay, pi32ConfigAttribs, &eglConfig, 1,
			&iConfigs);

	printf("calling eglGetConfigs()\n");
	EGLint num_configs;
	eglGetConfigs(eglDisplay, NULL, 0, &num_configs);
	printf("configs: %d\n", num_configs);

	printf("eglChooseConfig(): %d configs\n", iConfigs);

	if (eglConfig == NULL) {
		printf("Error: eglChooseConfig(): config not found.\n");
		exit(-1);
	}

	eglBindAPI(EGL_OPENGL_ES_API);

	EGLSurface eglSurface;
	eglSurface = eglCreateWindowSurface(eglDisplay, eglConfig,
			(EGLNativeWindowType)NULL, NULL);

	EGLContext eglContext;
	eglContext = eglCreateContext(eglDisplay, eglConfig, EGL_NO_CONTEXT, ctx_attribs );

	eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);


	printinfo(&eglDisplay);

	GLuint fboId = 0;
	GLuint renderBufferWidth = 64;
	GLuint renderBufferHeight = 64;

	/* create a FBO */
	glGenFramebuffers(1, &fboId);
	glBindFramebuffer(GL_FRAMEBUFFER, fboId);

	CHECK_GL_ERROR();

	GLuint renderBuffer;
	glGenRenderbuffers(1, &renderBuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, renderBuffer);

	CHECK_GL_ERROR();

	glRenderbufferStorage(GL_RENDERBUFFER,
			GL_RGB565,
			renderBufferWidth,
			renderBufferHeight);

	CHECK_GL_ERROR();

	glFramebufferRenderbuffer(GL_FRAMEBUFFER,
			GL_COLOR_ATTACHMENT0,
			GL_RENDERBUFFER,
			renderBuffer);

	CHECK_GL_ERROR();

	GLuint depthRenderbuffer;
	glGenRenderbuffers(1, &depthRenderbuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, depthRenderbuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, renderBufferWidth, renderBufferHeight);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRenderbuffer);

	CHECK_GL_ERROR();

	/* check FBO status */
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		printf("Problem with OpenGL framebuffer after specifying color render buffer: \n%x\n", status);
	} else {
		printf("FBO creation succedded\n");
	}

	/* scene init and create shaders */
	init();

	CHECK_GL_ERROR();

	/*
	 * Set initial projection/viewing transformation.
	 * We can't be sure we'll get a ConfigureNotify event when the window
	 * first appears.
	 */
	reshape(renderBufferHeight, renderBufferHeight);

	CHECK_GL_ERROR();

	draw();

	CHECK_GL_ERROR();

	//eglSwapBuffers(eglDisplay, eglSurface);

	CHECK_GL_ERROR();

	/* output to a tiff image */

	int size = 4 * renderBufferHeight * renderBufferWidth;

	unsigned char *data2 = (unsigned char *)malloc(size);

	glReadPixels(0, 0, renderBufferWidth, renderBufferHeight, GL_RGBA, GL_UNSIGNED_BYTE, data2);

	CHECK_GL_ERROR();

	buffer_to_tiff("out.tif", data2, renderBufferWidth, renderBufferHeight);

	free(data2);

	return 0;
}
