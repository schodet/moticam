/* Moticam 3+ viewer.
 *
 * Copyright (C) 2019 Nicolas Schodet
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Contact :
 *        Web: http://ni.fr.eu.org/
 *      Email: <nico at ni.fr.eu.org>
 */
#define _GNU_SOURCE
#include <assert.h>
#include <libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <error.h>
#include <getopt.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>

#define ID_VENDOR 0x232f
#define ID_PRODUCT 0x0100

struct options {
    int width;
    int height;
    double exposure;
    double gain;
    int count;
    const char *out;
};

void
usage(int status, const char *msg)
{
    if (msg)
	error(0, 0, "%s", msg);
    fprintf(status == EXIT_SUCCESS ? stdout : stderr,
	    "usage: %s [options] [FILE]\n"
	    "\n"
	    "Moticam 3+ viewer.\n"
	    "\n"
	    "positional arguments:\n"
	    "  FILE               output file (default: out)\n"
	    "\n"
	    "optional arguments:\n"
	    "  -h, --help         show this help message and exit\n"
	    "  -w, --width VALUE  image width (512, 1024 or 2048,"
	    " default: 1024)\n"
	    "  -e, --exposure MS  exposure value (1 to 5000,"
	    " default: 100)\n"
	    "  -g, --gain VALUE   gain value (0.33 to 42.66,"
	    " default: 1)\n"
	    "  -n, --count N      number of image to take (default: 30)\n"
	    , program_invocation_name);
    exit(status);
}

void
parse_options(int argc, char **argv, struct options *options)
{
    options->width = 1024;
    options->height = 768;
    options->exposure = 100.0;
    options->gain = 1.0;
    options->count = 30;
    options->out = NULL;
    char *tail;
    while (1) {
	static struct option long_options[] = {
	    { "help", no_argument, 0, 'h' },
	    { "width", required_argument, 0, 'w' },
	    { "exposure", required_argument, 0, 'e' },
	    { "gain", required_argument, 0, 'g' },
	    { "count", required_argument, 0, 'n' },
	    { NULL },
	};
	int option_index = 0;
	int c = getopt_long(argc, argv, "hw:e:g:n:", long_options,
		&option_index);
	if (c == -1)
	    break;
	switch (c) {
	case 'h':
	    usage(EXIT_SUCCESS, NULL);
	    break;
	case 'w':
	    if (strcmp(optarg, "512") == 0) {
		options->width = 512;
		options->height = 384;
	    } else if (strcmp(optarg, "1024") == 0) {
		options->width = 1024;
		options->height = 768;
	    } else if (strcmp(optarg, "2048") == 0) {
		options->width = 2048;
		options->height = 1536;
	    } else
		usage(EXIT_FAILURE, "bad width value");
	    break;
	case 'e':
	    errno = 0;
	    options->exposure = strtod(optarg, &tail);
	    if (*tail != '\0' || errno || options->exposure < 1
		    || options->exposure > 5000)
		usage(EXIT_FAILURE, "bad exposure value");
	    break;
	case 'g':
	    errno = 0;
	    options->gain = strtod(optarg, &tail);
	    if (*tail != '\0' || errno || options->gain <= 0.0
		    || options->gain >= 43.0)
		usage(EXIT_FAILURE, "bad gain value");
	    break;
	case 'n':
	    errno = 0;
	    options->count = strtoul(optarg, &tail, 10);
	    if (*tail != '\0' || errno)
		usage(EXIT_FAILURE, "bad count value");
	    break;
	case '?':
	    usage(EXIT_FAILURE, NULL);
	    break;
	default:
	    abort();
	}
    }
    if (optind < argc)
	options->out = argv[optind++];
    if (optind < argc)
	usage(EXIT_FAILURE, "too many arguments");
    if (!options->out)
	options->out = "out";
}

libusb_device_handle *
device_open()
{
    libusb_device **list;
    libusb_device *found = NULL;
    int nfound = 0;
    ssize_t cnt = libusb_get_device_list(NULL, &list);
    ssize_t i = 0;
    if (cnt < 0)
	error(EXIT_FAILURE, 0, "can not list devices: %s",
		libusb_strerror(cnt));
    for (i = 0; i < cnt; i++) {
	libusb_device *device = list[i];
	struct libusb_device_descriptor desc;
	int r = libusb_get_device_descriptor(device, &desc);
	if (r)
	    error(EXIT_FAILURE, 0, "can not get device descriptor: %s",
		    libusb_strerror(r));
	if (desc.idVendor == ID_VENDOR && desc.idProduct == ID_PRODUCT) {
	    found = device;
	    nfound++;
	    break;
	}
    }
    if (nfound > 1)
	error(EXIT_FAILURE, 0,
		"more than one matching device, not supported");
    libusb_device_handle *handle = NULL;
    if (found) {
	int r = libusb_open(found, &handle);
	if (r)
	    error(EXIT_FAILURE, 0, "can not open device: %s",
		    libusb_strerror(r));
    }
    libusb_free_device_list(list, 1);
    return handle;
}

void
device_control_vendor(libusb_device_handle *handle, uint16_t wValue,
	const uint8_t *data, int data_cnt)
{
    int r = libusb_control_transfer(handle, LIBUSB_REQUEST_TYPE_VENDOR, 240,
	    wValue, 0, (uint8_t *) data, data_cnt, 0);
    if (r < 0)
	error(EXIT_FAILURE, 0, "can not send vendor: %s", libusb_strerror(r));
}

void
device_control_vendor_w(libusb_device_handle *handle, uint16_t wValue,
	const uint16_t data0)
{
    uint8_t data[] = { (uint8_t) (data0 >> 8), (uint8_t) data0 };
    device_control_vendor(handle, wValue, data, sizeof(data));
}

void
device_reset(libusb_device_handle *handle)
{
    device_control_vendor_w(handle, 0xba00, 0x0000);
    device_control_vendor_w(handle, 0xba00, 0x0001);
}

void
device_set_gain(libusb_device_handle *handle, double gain)
{
    double gmin, gmax;
    int xmin, xmax;
    bool up = false;
    if (gain <= 1.34) {
	gmin = 0.33;
	gmax = 1.33;
	xmin = 0x08;
	xmax = 0x20;
    } else if (gain <= 2.68) {
	gmin = 1.42;
	gmax = 2.67;
	xmin = 0x51;
	xmax = 0x60;
    } else {
	gmin = 3;
	gmax = 42.67;
	xmin = 0x1;
	xmax = 0x78;
	up = true;
    }
    int x = (gain - gmin) / (gmax - gmin) * (xmax - xmin) + xmin;
    if (x < xmin)
	x = xmin;
    else if (x > xmax)
	x = xmax;
    if (up)
	x = (x << 8) | 0x60;
    device_control_vendor_w(handle, 0xba2d, x);
    device_control_vendor_w(handle, 0xba2b, x);
    device_control_vendor_w(handle, 0xba2e, x);
    device_control_vendor_w(handle, 0xba2c, x);
}

void
device_set_exposure(libusb_device_handle *handle, double exposure)
{
    int exposure_w = exposure * 12.82;
    if (exposure_w < 0x000c)
	exposure_w = 0x000c;
    else if (exposure_w > 0xffff)
	exposure_w = 0xffff;
    device_control_vendor_w(handle, 0xba09, exposure_w);
}

void
device_set_resolution(libusb_device_handle *handle, int width, int height)
{
    static const uint8_t control_init[] = {
	0x00, 0x14, 0x00, 0x20, 0x05, 0xff, 0x07, 0xff };
    device_control_vendor(handle, 0xba01, control_init,
	    sizeof(control_init));
    static const uint8_t control_512x384[] = { 0x00, 0x03, 0x00, 0x03 };
    static const uint8_t control_1024x768[] = { 0x00, 0x11, 0x00, 0x11 };
    static const uint8_t control_2048x1536[] = { 0x00, 0x00, 0x00, 0x00 };
    switch (width)
    {
    case 512:
	assert(height == 384);
	device_control_vendor(handle, 0xba22, control_512x384,
		sizeof(control_512x384));
	break;
    case 1024:
	assert(height == 768);
	device_control_vendor(handle, 0xba22, control_1024x768,
		sizeof(control_1024x768));
	break;
    case 2048:
	assert(height == 1536);
	device_control_vendor(handle, 0xba22, control_2048x1536,
		sizeof(control_2048x1536));
	break;
    default:
	assert(0);
    }
}

void
delay_us(int us)
{
    struct timespec delay, remaining;
    delay.tv_sec = us / 1000000;
    delay.tv_nsec = us % 1000000 * 1000;
    while (nanosleep(&delay, &remaining) == -1) {
	if (errno == EINTR)
	    delay = remaining;
	else
	    error(EXIT_FAILURE, errno, "can not sleep");
    }
}

void
device_init(libusb_device_handle *handle, struct options *options)
{
    device_reset(handle);
    device_set_gain(handle, options->gain);
    device_set_exposure(handle, 30.0);
    device_set_resolution(handle, options->width, options->height);
    device_set_exposure(handle, options->exposure);
    delay_us(100000);
}

void
device_uninit(libusb_device_handle *handle)
{
    device_set_exposure(handle, 0.0);
    device_set_exposure(handle, 0.0);
    device_set_exposure(handle, 0.0);
}

void
run(libusb_device_handle *handle, struct options *options)
{
    int image_size = options->width * options->height;
    int frame_size = 16384;
    // Request an extra frame to read the zero length packet.
    uint8_t data[(image_size + frame_size) / frame_size * frame_size];
    FILE *out = fopen(options->out, "wb");
    if (!out)
	error(EXIT_FAILURE, errno, "can not open output file `%s'",
		options->out);
    for (int i = 0; i < options->count;) {
	int transfered = 0;
	int r = libusb_bulk_transfer(handle, 0x83, data, sizeof(data),
		&transfered, 0);
	if (r)
	    error(EXIT_FAILURE, 0, "can not read data: %s",
		    libusb_strerror(r));
	if (transfered != image_size)
	    fprintf(stderr, "bad image size (%d), drop\n", transfered);
	else {
	    fprintf(stderr, "write %d (%d)\n", i, transfered);
	    r = fwrite(data, transfered, 1, out);
	    if (r < 0)
		error(EXIT_FAILURE, errno, "can not write");
	    i++;
	}
    }
    fclose(out);
}

int
main(int argc, char **argv)
{
    struct options options;
    parse_options(argc, argv, &options);
    libusb_context *usb;
    int r = libusb_init(&usb);
    if (r)
	error(EXIT_FAILURE, 0, "unable to initialize libusb: %s",
		libusb_strerror(r));
    libusb_device_handle *handle = device_open();
    if (!handle)
	error(EXIT_FAILURE, 0, "unable to find device");
    device_init(handle, &options);
    run(handle, &options);
    device_uninit(handle);
    libusb_close(handle);
    libusb_exit(usb);
    return EXIT_SUCCESS;
}
