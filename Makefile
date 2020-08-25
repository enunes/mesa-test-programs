LDLIBS+=-lEGL -lGLESv2 -lm -lgbm -lepoxy -lpng
CFLAGS+=-Wall -Wextra -DEGL_NO_X11
CFLAGS+= -g -Og $(shell pkg-config --cflags libdrm)
LDLIBS+= $(shell pkg-config --libs libdrm) -lX11
TARGETS+=egl-color-kms

all: $(TARGETS)

egl-color-kms egl-color-png egl-color-x11 : egl-color.o

clean:
	rm -fv $(TARGETS) *.tif

.PHONY: clean
