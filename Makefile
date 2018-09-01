LDLIBS+=-lEGL -lGLESv2 -lm -lgbm -lepoxy -lpng
CFLAGS+=-Wall -Wextra -DMESA_EGL_NO_X11_HEADERS
CFLAGS+= -g -Og $(shell pkg-config --cflags libdrm)
LDLIBS+= $(shell pkg-config --libs libdrm) -lX11
TARGETS+=egl-color-kms
TARGETS+=egl-color-png
TARGETS+=egl-color-x11

all: $(TARGETS)

egl-color-kms egl-color-png egl-color-x11 : egl-color.o

clean:
	rm -fv $(TARGETS) *.tif

.PHONY: clean
