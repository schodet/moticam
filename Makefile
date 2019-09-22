CFLAGS := -Wall $(shell pkg-config libusb-1.0 --cflags)
LDLIBS := $(shell pkg-config libusb-1.0 --libs)

all: moticam
