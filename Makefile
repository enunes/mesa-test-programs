LDLIBS+=-lEGL -lGLESv2 -ltiff -lm -lgbm -lepoxy -lpng
CFLAGS+=-Wall -Wextra -DMESA_EGL_NO_X11_HEADERS
CFLAGS+= -g -Og $(shell pkg-config --cflags libdrm)
LDLIBS+= $(shell pkg-config --libs libdrm) -lX11
TARGETS=egl-tiff test-drm-prime-dumb-kms gbm-bo-test eglkms egltri

all: $(TARGETS)

clean:
	rm -fv $(TARGETS) *.tif

.PHONY: clean
