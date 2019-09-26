libs := libusb-1.0 libpng16 sdl2
CFLAGS := -g -Wall $(shell pkg-config $(libs) --cflags)
LDLIBS := $(shell pkg-config $(libs) --libs)

all: moticam
