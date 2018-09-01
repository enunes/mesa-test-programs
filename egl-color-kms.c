/*
 * Reference: https://cgit.freedesktop.org/mesa/demos/tree/src/egl/opengl/eglkms.c?id=mesa-demos-8.3.0
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gbm.h>
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

extern EGLDisplay display;
extern EGLSurface surface;

void InitGLES(int width, int height);
void Render(void);

struct kms {
   drmModeConnector *connector;
   drmModeEncoder *encoder;
   drmModeModeInfo mode;
   uint32_t fb_id;
};

static EGLBoolean
setup_kms(int fd, struct kms *kms)
{
   drmModeRes *resources = NULL;
   drmModeConnector *connector = NULL;
   drmModeEncoder *encoder = NULL;
   int i;

   resources = drmModeGetResources(fd);
   if (!resources) {
      fprintf(stderr, "drmModeGetResources failed\n");
      return EGL_FALSE;
   }

   for (i = 0; i < resources->count_connectors; i++) {
      connector = drmModeGetConnector(fd, resources->connectors[i]);
      if (connector == NULL)
         continue;

      if (connector->connection == DRM_MODE_CONNECTED && connector->count_modes > 0)
         break;

      drmModeFreeConnector(connector);
   }

   if (i == resources->count_connectors) {
      fprintf(stderr, "No currently active connector found.\n");
      return EGL_FALSE;
   }

   for (i = 0; i < resources->count_encoders; i++) {
      encoder = drmModeGetEncoder(fd, resources->encoders[i]);

      if (encoder == NULL)
         continue;

      if (encoder->encoder_id == connector->encoder_id)
         break;

      drmModeFreeEncoder(encoder);
   }

   kms->connector = connector;
   kms->encoder = encoder;
   kms->mode = connector->modes[0];

   return EGL_TRUE;
}

static const char device_name[] = "/dev/dri/card0";

static const EGLint attribs[] = {
   EGL_BUFFER_SIZE, 32,
   EGL_DEPTH_SIZE, EGL_DONT_CARE,
   EGL_STENCIL_SIZE, EGL_DONT_CARE,
   EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
   EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
   EGL_NONE
};

int main(void)
{
   EGLConfig config;
   EGLint major, minor, n;
   const char *ver;
   uint32_t handle, stride;
   struct kms kms;
   int ret, fd;
   struct gbm_device *gbm;
   struct gbm_bo *bo;
   drmModeCrtcPtr saved_crtc;
   struct gbm_surface *gs;

   fd = open(device_name, O_RDWR);
   if (fd < 0) {
      /* Probably permissions error */
      fprintf(stderr, "couldn't open %s, skipping\n", device_name);
      return -1;
   }

   gbm = gbm_create_device(fd);
   if (gbm == NULL) {
      fprintf(stderr, "couldn't create gbm device\n");
      ret = -1;
      goto close_fd;
   }

   display = eglGetDisplay(gbm);
   if (display == EGL_NO_DISPLAY) {
      fprintf(stderr, "eglGetDisplay() failed\n");
      ret = -1;
      goto destroy_gbm_device;
   }

   if (!eglInitialize(display, &major, &minor)) {
      printf("eglInitialize() failed\n");
      ret = -1;
      goto egl_terminate;
   }

   ver = eglQueryString(display, EGL_VERSION);
   printf("EGL_VERSION = %s\n", ver);

   if (!setup_kms(fd, &kms)) {
      ret = -1;
      goto egl_terminate;
   }

   eglBindAPI(EGL_OPENGL_ES_API);

   if (!eglChooseConfig(display, attribs, &config, 1, &n) || n != 1) {
      fprintf(stderr, "failed to choose argb config\n");
      ret = -1;
      goto egl_terminate;
   }

   static const EGLint context_attribs[] = {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE
   };
   EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribs);
   if (context == NULL) {
      fprintf(stderr, "failed to create context\n");
      ret = -1;
      goto egl_terminate;
   }

   gs = gbm_surface_create(gbm, kms.mode.hdisplay, kms.mode.vdisplay,
            GBM_BO_FORMAT_XRGB8888,
            GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
   surface = eglCreateWindowSurface(display, config, gs, NULL);

   if (!eglMakeCurrent(display, surface, surface, context)) {
      fprintf(stderr, "failed to make context current\n");
      ret = -1;
      goto destroy_context;
   }

   InitGLES(kms.mode.hdisplay, kms.mode.vdisplay);
   Render();

   bo = gbm_surface_lock_front_buffer(gs);
   handle = gbm_bo_get_handle(bo).u32;
   stride = gbm_bo_get_stride(bo);

   printf("handle=%d, stride=%d\n", handle, stride);

   ret = drmModeAddFB(fd,
            kms.mode.hdisplay, kms.mode.vdisplay,
            24, 32, stride, handle, &kms.fb_id);
   if (ret) {
      fprintf(stderr, "failed to create fb\n");
      goto rm_fb;
   }

   saved_crtc = drmModeGetCrtc(fd, kms.encoder->crtc_id);
   if (saved_crtc == NULL)
      goto rm_fb;

   ret = drmModeSetCrtc(fd, kms.encoder->crtc_id, kms.fb_id, 0, 0,
         &kms.connector->connector_id, 1, &kms.mode);
   if (ret) {
      fprintf(stderr, "failed to set mode: %m\n");
      goto free_saved_crtc;
   }

   sleep(1);

   ret = drmModeSetCrtc(fd, saved_crtc->crtc_id, saved_crtc->buffer_id,
                        saved_crtc->x, saved_crtc->y,
                        &kms.connector->connector_id, 1, &saved_crtc->mode);
   if (ret) {
      fprintf(stderr, "failed to restore crtc: %m\n");
   }

free_saved_crtc:
   drmModeFreeCrtc(saved_crtc);
rm_fb:
   drmModeRmFB(fd, kms.fb_id);
   eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
destroy_context:
   eglDestroyContext(display, context);
egl_terminate:
   eglTerminate(display);
destroy_gbm_device:
   gbm_device_destroy(gbm);
close_fd:
   close(fd);

   return ret;
}
