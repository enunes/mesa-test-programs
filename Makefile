LDLIBS+=-lEGL -lGLESv2 -ltiff -lm -lgbm -lepoxy -lpng
CFLAGS+=-Wall -Wextra -DMESA_EGL_NO_X11_HEADERS
CFLAGS+= -g -Og $(shell pkg-config --cflags libdrm)
LDLIBS+= $(shell pkg-config --libs libdrm)
TARGETS=egl-tiff test-drm-prime-dumb-kms

all: $(TARGETS)

clean:
	rm -fv $(TARGETS) *.tif

.PHONY: clean
