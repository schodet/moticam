libs := libusb-1.0 libpng16
CFLAGS := -Wall $(shell pkg-config $(libs) --cflags)
LDLIBS := $(shell pkg-config $(libs) --libs)

all: moticam
