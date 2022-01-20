#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <X11/Xlib.h>

extern EGLDisplay display;
extern EGLSurface surface;

#define TARGET_SIZE 256

void InitGLES(int width, int height);
void Render(void);

EGLNativeWindowType CreateNativeWindow(void)
{
	Display *xdisplay;
	assert((xdisplay = XOpenDisplay(NULL)) != NULL);

	int screen = DefaultScreen(xdisplay);
	Window root = DefaultRootWindow(xdisplay);
	Window xwindow = XCreateWindow(xdisplay, root, 0, 0, TARGET_SIZE, TARGET_SIZE, 0,
				       DefaultDepth(xdisplay, screen), InputOutput,
				       DefaultVisual(xdisplay, screen),
				       0, NULL);
	XMapWindow(xdisplay, xwindow);
	XFlush(xdisplay);
	return xwindow;
}

void RenderTargetInit(void)
{
	EGLNativeWindowType window;
	window = CreateNativeWindow();

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
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_SWAP_BEHAVIOR_PRESERVED_BIT,
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
	assert((surface = eglCreateWindowSurface(display, config, window, attribList)) != EGL_NO_SURFACE);

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

int main(void)
{
	RenderTargetInit();
	InitGLES(TARGET_SIZE, TARGET_SIZE);
	Render();

	sleep(1);
	return 0;
}
