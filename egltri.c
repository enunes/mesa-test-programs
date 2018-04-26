
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <png.h>

#define _X_WINDOW_SYSTEM_

GLuint program;
EGLDisplay display;
EGLSurface surface;

int windowWidth = 800;
int windowHeight = 600;

void render_init(int width, int height);
void render(void);

#ifdef _X_WINDOW_SYSTEM_

#include <X11/Xlib.h>

EGLNativeWindowType CreateNativeWindow(void)
{
	Display *display;
	assert((display = XOpenDisplay(NULL)) != NULL);

	int screen = DefaultScreen(display);
	Window root = DefaultRootWindow(display);
	Window window = XCreateWindow(display, root, 0, 0, windowWidth, windowHeight, 0,
				       DefaultDepth(display, screen), InputOutput,
				       DefaultVisual(display, screen),
				       0, NULL);
	XMapWindow(display, window);
	XFlush(display);
	return window;
}

#endif

GLuint LoadShader(const char *name, GLenum type)
{
	FILE *f;
	int size;
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
	glShaderSource(shader, 1, source, &size);
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

	glClearColor(0.2, 0.2, 0.4, 0);
	glViewport(0, 0, windowWidth, windowHeight);
	//glEnable(GL_DEPTH_TEST);

	glUseProgram(program);
}


int writeImage(char* filename, int width, int height, void *buffer, char* title)
{
	int code = 0;
	FILE *fp = NULL;
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;

	// Open file for writing (binary mode)
	fp = fopen(filename, "wb");
	if (fp == NULL) {
		fprintf(stderr, "Could not open file %s for writing\n", filename);
		code = 1;
		goto finalise;
	}

	// Initialize write structure
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL) {
		fprintf(stderr, "Could not allocate write struct\n");
		code = 1;
		goto finalise;
	}

	// Initialize info structure
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		fprintf(stderr, "Could not allocate info struct\n");
		code = 1;
		goto finalise;
	}

	// Setup Exception handling
	if (setjmp(png_jmpbuf(png_ptr))) {
		fprintf(stderr, "Error during png creation\n");
		code = 1;
		goto finalise;
	}

	png_init_io(png_ptr, fp);

	// Write header (8 bit colour depth)
	png_set_IHDR(png_ptr, info_ptr, width, height,
		     8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
		     PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	// Set title
	if (title != NULL) {
		png_text title_text;
		title_text.compression = PNG_TEXT_COMPRESSION_NONE;
		title_text.key = "Title";
		title_text.text = title;
		png_set_text(png_ptr, info_ptr, &title_text, 1);
	}

	png_write_info(png_ptr, info_ptr);

	// Write image data
	int i;
	for (i = 0; i < height; i++)
		png_write_row(png_ptr, (png_bytep)buffer + i * width * 4);

	// End write
	png_write_end(png_ptr, NULL);

finalise:
	if (fp != NULL) fclose(fp);
	if (info_ptr != NULL) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
	if (png_ptr != NULL) png_destroy_write_struct(&png_ptr, (png_infopp)NULL);

	return code;
}

void Render(void)
{
   GLfloat vertex[] = {
      -1, -1 , 0,
       1, -1 , 0,
       0,  1 , 0
   };

   GLint position = glGetAttribLocation(program, "positionIn");
   glEnableVertexAttribArray(position);
   glVertexAttribPointer(position, 3, GL_FLOAT, 0, 0, vertex);
   assert(glGetError() == GL_NO_ERROR);

   glClear(GL_COLOR_BUFFER_BIT);
   assert(glGetError() == GL_NO_ERROR);

   glDrawArrays(GL_TRIANGLES, 0, (sizeof(vertex)/sizeof(vertex[0]))/3);
   assert(glGetError() == GL_NO_ERROR);
}

void RenderTargetInit(EGLNativeWindowType nativeWindow)
{
	assert((display = eglGetDisplay(EGL_DEFAULT_DISPLAY)) != EGL_NO_DISPLAY);

	EGLint majorVersion;
	EGLint minorVersion;
	assert(eglInitialize(display, &majorVersion, &minorVersion) == EGL_TRUE);
	printf("EGL version %d.%d\n", majorVersion, minorVersion);

	printf("EGL Version: \"%s\"\n", eglQueryString(display, EGL_VERSION));
	printf("EGL Vendor: \"%s\"\n", eglQueryString(display, EGL_VENDOR));
	printf("EGL Extensions: \"%s\"\n", eglQueryString(display, EGL_EXTENSIONS));

	EGLConfig config;
	EGLint numConfigs;
	const EGLint configAttribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_DEPTH_SIZE, 24,
		EGL_NONE
	};
	assert(eglChooseConfig(display, configAttribs, &config, 1, &numConfigs) == EGL_TRUE);
	assert(numConfigs > 0);

	const EGLint attribList[] = {
		EGL_RENDER_BUFFER, EGL_BACK_BUFFER,
		EGL_NONE
	};
	assert((surface = eglCreateWindowSurface(display, config, nativeWindow, attribList)) != EGL_NO_SURFACE);

	EGLint width, height;
	assert(eglQuerySurface(display, surface, EGL_WIDTH, &width) == EGL_TRUE);
	assert(eglQuerySurface(display, surface, EGL_HEIGHT, &height) == EGL_TRUE);
	printf("Surface size: %dx%d\n", width, height);

	EGLContext context;
	const EGLint contextAttribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};
	assert((context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs)) != EGL_NO_CONTEXT);

	assert(eglMakeCurrent(display, surface, surface, context) == EGL_TRUE);
}

int main(int argc, char *argv[])
{
	EGLNativeWindowType window;
	window = CreateNativeWindow();
	RenderTargetInit(window);

	InitGLES();
	Render();
	eglSwapBuffers(display, surface);

	sleep(1);
	return 0;
}
