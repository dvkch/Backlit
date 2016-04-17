/*
 * epson2.c - SANE library for Epson scanners.
 *
 * Based on Kazuhiro Sasayama previous
 * Work on epson.[ch] file from the SANE package.
 * Please see those files for additional copyrights.
 *
 * Copyright (C) 2006-10 Tower Technologies
 * Author: Alessandro Zummo <a.zummo@towertech.it>
 *
 * This file is part of the SANE package.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2.
 */

#define EPSON2_VERSION	1
#define EPSON2_REVISION	0
#define EPSON2_BUILD	124

/* debugging levels:
 *
 *     127	e2_recv buffer
 *     125	e2_send buffer
 *	32	more network progression
 *	24	network header
 *	23	network info
 *	20	usb cmd counters
 *	18	sane_read
 *	17	setvalue, getvalue, control_option
 *	16	gamma table
 *	15	e2_send, e2_recv calls
 *	13	e2_cmd_info_block
 *	12	epson_cmd_simple
 *	11	even more
 *	10	more debug in ESC/I commands
 *	 9	ESC x/FS x in e2_send
 *	 8	ESC/I commands
 *	 7	open/close/attach
 *	 6	print_params
 *	 5	basic functions
 *	 3	status information
 *	 1	scanner info and capabilities
 *		warnings
 */

#include "sane/config.h"

#include "epson2.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#include "sane/saneopts.h"
#include "sane/sanei_scsi.h"
#include "sane/sanei_usb.h"
#include "sane/sanei_pio.h"
#include "sane/sanei_tcp.h"
#include "sane/sanei_udp.h"
#include "sane/sanei_backend.h"
#include "sane/sanei_config.h"

#include "epson2-io.h"
#include "epson2-commands.h"
#include "epson2-ops.h"

#include "epson2_scsi.h"
#include "epson_usb.h"
#include "epson2_net.h"

/*
 * Definition of the mode_param struct, that is used to
 * specify the valid parameters for the different scan modes.
 *
 * The depth variable gets updated when the bit depth is modified.
 */

struct mode_param mode_params[] = {
	{0, 0x00, 0x30, 1},
	{0, 0x00, 0x30, 8},
	{1, 0x02, 0x00, 8},
	{0, 0x00, 0x30, 1}
};

static SANE_String_Const mode_list[] = {
	SANE_VALUE_SCAN_MODE_LINEART,
	SANE_VALUE_SCAN_MODE_GRAY,
	SANE_VALUE_SCAN_MODE_COLOR,
#ifdef SANE_FRAME_IR
	SANE_I18N("Infrared"),
#endif
	NULL
};

static const SANE_String_Const adf_mode_list[] = {
	SANE_I18N("Simplex"),
	SANE_I18N("Duplex"),
	NULL
};

/* Define the different scan sources */

#define FBF_STR	SANE_I18N("Flatbed")
#define TPU_STR	SANE_I18N("Transparency Unit")
#define TPU_STR2 SANE_I18N("TPU8x10")
#define ADF_STR	SANE_I18N("Automatic Document Feeder")

/*
 * source list need one dummy entry (save device settings is crashing).
 * NOTE: no const - this list gets created while exploring the capabilities
 * of the scanner.
 */

SANE_String_Const source_list[] = {
	FBF_STR,
	NULL,
	NULL,
	NULL
};

static const SANE_String_Const film_list[] = {
	SANE_I18N("Positive Film"),
	SANE_I18N("Negative Film"),
	SANE_I18N("Positive Slide"),
	SANE_I18N("Negative Slide"),
	NULL
};

static const SANE_String_Const focus_list[] = {
	SANE_I18N("Focus on glass"),
	SANE_I18N("Focus 2.5mm above glass"),
	NULL
};

#define HALFTONE_NONE 0x01
#define HALFTONE_TET 0x03

const int halftone_params[] = {
	HALFTONE_NONE,
	0x00,
	0x10,
	0x20,
	0x80,
	0x90,
	0xa0,
	0xb0,
	HALFTONE_TET,
	0xc0,
	0xd0
};

static const SANE_String_Const halftone_list[] = {
	SANE_I18N("None"),
	SANE_I18N("Halftone A (Hard Tone)"),
	SANE_I18N("Halftone B (Soft Tone)"),
	SANE_I18N("Halftone C (Net Screen)"),
	NULL
};

static const SANE_String_Const halftone_list_4[] = {
	SANE_I18N("None"),
	SANE_I18N("Halftone A (Hard Tone)"),
	SANE_I18N("Halftone B (Soft Tone)"),
	SANE_I18N("Halftone C (Net Screen)"),
	SANE_I18N("Dither A (4x4 Bayer)"),
	SANE_I18N("Dither B (4x4 Spiral)"),
	SANE_I18N("Dither C (4x4 Net Screen)"),
	SANE_I18N("Dither D (8x4 Net Screen)"),
	NULL
};

static const SANE_String_Const halftone_list_7[] = {
	SANE_I18N("None"),
	SANE_I18N("Halftone A (Hard Tone)"),
	SANE_I18N("Halftone B (Soft Tone)"),
	SANE_I18N("Halftone C (Net Screen)"),
	SANE_I18N("Dither A (4x4 Bayer)"),
	SANE_I18N("Dither B (4x4 Spiral)"),
	SANE_I18N("Dither C (4x4 Net Screen)"),
	SANE_I18N("Dither D (8x4 Net Screen)"),
	SANE_I18N("Text Enhanced Technology"),
	SANE_I18N("Download pattern A"),
	SANE_I18N("Download pattern B"),
	NULL
};

static const SANE_String_Const dropout_list[] = {
	SANE_I18N("None"),
	SANE_I18N("Red"),
	SANE_I18N("Green"),
	SANE_I18N("Blue"),
	NULL
};

static const SANE_Bool correction_userdefined[] = {
	SANE_FALSE,
	SANE_TRUE,
	SANE_TRUE,
};

static const SANE_String_Const correction_list[] = {
	SANE_I18N("None"),
	SANE_I18N("Built in CCT profile"),
	SANE_I18N("User defined CCT profile"),
	NULL
};

enum {
	CORR_NONE, CORR_AUTO, CORR_USER
};

static const SANE_String_Const cct_mode_list[] = {
        "Automatic",
        "Reflective",
        "Colour negatives",
        "Monochrome negatives",
        "Colour positives",
        NULL
};

enum {
	CCT_AUTO, CCT_REFLECTIVE, CCT_COLORNEG, CCT_MONONEG,
	CCT_COLORPOS
};

/*
 * Gamma correction:
 * The A and B level scanners work differently than the D level scanners,
 * therefore I define two different sets of arrays, plus one set of
 * variables that get set to the actally used params and list arrays at runtime.
 */
 
static int gamma_params_ab[] = {
	0x01,
	0x03,
	0x00,
	0x10,
	0x20
};

static const SANE_String_Const gamma_list_ab[] = {
	SANE_I18N("Default"),
	SANE_I18N("User defined"),
	SANE_I18N("High density printing"),
	SANE_I18N("Low density printing"),
	SANE_I18N("High contrast printing"),
	NULL
};

static SANE_Bool gamma_userdefined_ab[] = {
	SANE_FALSE,
	SANE_TRUE,
	SANE_FALSE,
	SANE_FALSE,
	SANE_FALSE,
};

static int gamma_params_d[] = {
	0x03,
	0x04
};

static const SANE_String_Const gamma_list_d[] = {
	SANE_I18N("User defined (Gamma=1.0)"),
	SANE_I18N("User defined (Gamma=1.8)"),
	NULL
};

static SANE_Bool gamma_userdefined_d[] = {
	SANE_TRUE,
	SANE_TRUE
};

static SANE_Bool *gamma_userdefined;
int *gamma_params;

/* Bay list:
 * this is used for the FilmScan
 * XXX Add APS loader support
 */

static const SANE_String_Const bay_list[] = {
	"1",
	"2",
	"3",
	"4",
	"5",
	"6",
	NULL
};

/* minimum, maximum, quantization */
static const SANE_Range u8_range = { 0, 255, 0 };
static const SANE_Range s8_range = { -127, 127, 0 };
static const SANE_Range fx_range = { SANE_FIX(-2.0), SANE_FIX(2.0), 0 };

static const SANE_Range outline_emphasis_range = { -2, 2, 0 };


/*
 * List of pointers to devices - will be dynamically allocated depending
 * on the number of devices found.
 */
static const SANE_Device **devlist;


/* Some utility functions */

static size_t
max_string_size(const SANE_String_Const strings[])
{
	size_t size, max_size = 0;
	int i;

	for (i = 0; strings[i]; i++) {
		size = strlen(strings[i]) + 1;
		if (size > max_size)
			max_size = size;
	}
	return max_size;
}

static SANE_Status attach_one_usb(SANE_String_Const devname);
static SANE_Status attach_one_net(SANE_String_Const devname);

static void
print_params(const SANE_Parameters params)
{
	DBG(6, "params.format          = %d\n", params.format);
	DBG(6, "params.last_frame      = %d\n", params.last_frame);
	DBG(6, "params.bytes_per_line  = %d\n", params.bytes_per_line);
	DBG(6, "params.pixels_per_line = %d\n", params.pixels_per_line);
	DBG(6, "params.lines           = %d\n", params.lines);
	DBG(6, "params.depth           = %d\n", params.depth);
}

/*
 * close_scanner()
 *
 * Close the open scanner. Depending on the connection method, a different
 * close function is called.
 */

static void
close_scanner(Epson_Scanner *s)
{
	int i;

	DBG(7, "%s: fd = %d\n", __func__, s->fd);

	if (s->fd == -1)
		goto free;

	/* send a request_status. This toggles w_cmd_count and r_cmd_count */
	if (r_cmd_count % 2)
		esci_request_status(s, NULL);

	/* request extended status. This toggles w_cmd_count only */
	if (w_cmd_count % 2)
		esci_request_extended_status(s, NULL, NULL);

	if (s->hw->connection == SANE_EPSON_NET) {
		sanei_epson_net_unlock(s);
		sanei_tcp_close(s->fd);
	} else if (s->hw->connection == SANE_EPSON_SCSI) {
		sanei_scsi_close(s->fd);
	} else if (s->hw->connection == SANE_EPSON_PIO) {
		sanei_pio_close(s->fd);
	} else if (s->hw->connection == SANE_EPSON_USB) {
		sanei_usb_close(s->fd);
	}

	s->fd = -1;

free:
	for (i = 0; i < LINES_SHUFFLE_MAX; i++) {
		if (s->line_buffer[i] != NULL)
			free(s->line_buffer[i]);
	}

	free(s);
}

static void
e2_network_discovery(void)
{
	fd_set rfds;
	int fd, len;
	SANE_Status status;

	char *ip, *query = "EPSONP\x00\xff\x00\x00\x00\x00\x00\x00\x00";
	unsigned char buf[76];

	struct timeval to;

	status = sanei_udp_open_broadcast(&fd);
	if (status != SANE_STATUS_GOOD)
		return;

	sanei_udp_write_broadcast(fd, 3289, (unsigned char *) query, 15);

	DBG(5, "%s, sent discovery packet\n", __func__);

	to.tv_sec = 1;
	to.tv_usec = 0;

	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);

	sanei_udp_set_nonblock(fd, SANE_TRUE);
	while (select(fd + 1, &rfds, NULL, NULL, &to) > 0) {
		if ((len = sanei_udp_recvfrom(fd, buf, 76, &ip)) == 76) {
			DBG(5, " response from %s\n", ip);

			/* minimal check, protocol unknown */
			if (strncmp((char *) buf, "EPSON", 5) == 0)
				attach_one_net(ip);
		}
	}

	DBG(5, "%s, end\n", __func__);

	sanei_udp_close(fd);
}

/*
 * open_scanner()
 *
 * Open the scanner device. Depending on the connection method,
 * different open functions are called.
 */

static SANE_Status
open_scanner(Epson_Scanner *s)
{
	SANE_Status status = 0;

	DBG(7, "%s: %s\n", __func__, s->hw->sane.name);

	if (s->fd != -1) {
		DBG(5, "scanner is already open: fd = %d\n", s->fd);
		return SANE_STATUS_GOOD;	/* no need to open the scanner */
	}

	if (s->hw->connection == SANE_EPSON_NET) {
		unsigned char buf[5];

		/* device name has the form net:ipaddr */
		status = sanei_tcp_open(&s->hw->sane.name[4], 1865, &s->fd);
		if (status == SANE_STATUS_GOOD) {

			ssize_t read;
			struct timeval tv;

			tv.tv_sec = 5;
			tv.tv_usec = 0;

			setsockopt(s->fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,  sizeof(tv));

			s->netlen = 0;

			DBG(32, "awaiting welcome message\n");

			/* the scanner sends a kind of welcome msg */
			read = e2_recv(s, buf, 5, &status);
			if (read != 5) {
				sanei_tcp_close(s->fd);
				s->fd = -1;
				return SANE_STATUS_IO_ERROR;
			}

			DBG(32, "welcome message received, locking the scanner...\n");

			/* lock the scanner for use by sane */
			status = sanei_epson_net_lock(s);
			if (status != SANE_STATUS_GOOD) {
				DBG(1, "%s cannot lock scanner: %s\n", s->hw->sane.name,
					sane_strstatus(status));

				sanei_tcp_close(s->fd);
				s->fd = -1;

				return status;
			}

			DBG(32, "scanner locked\n");
		}
		
	} else if (s->hw->connection == SANE_EPSON_SCSI)
		status = sanei_scsi_open(s->hw->sane.name, &s->fd,
					 sanei_epson2_scsi_sense_handler,
					 NULL);
	else if (s->hw->connection == SANE_EPSON_PIO)
		/* device name has the form pio:0xnnn */
		status = sanei_pio_open(&s->hw->sane.name[4], &s->fd);

	else if (s->hw->connection == SANE_EPSON_USB)
		status = sanei_usb_open(s->hw->sane.name, &s->fd);

	if (status == SANE_STATUS_ACCESS_DENIED) {
		DBG(1, "please check that you have permissions on the device.\n");
		DBG(1, "if this is a multi-function device with a printer,\n");
		DBG(1, "disable any conflicting driver (like usblp).\n");
	}

	if (status != SANE_STATUS_GOOD) 
		DBG(1, "%s open failed: %s\n", s->hw->sane.name,
			sane_strstatus(status));
	else
		DBG(5, "scanner opened\n");

	return status;
}

static SANE_Status detect_scsi(struct Epson_Scanner *s)
{
	SANE_Status status;
	struct Epson_Device *dev = s->hw;

	char buf[INQUIRY_BUF_SIZE + 1];
	size_t buf_size = INQUIRY_BUF_SIZE;

	char *vendor = buf + 8;
	char *model = buf + 16;
	char *rev = buf + 32;

	status = sanei_epson2_scsi_inquiry(s->fd, buf, &buf_size);
	if (status != SANE_STATUS_GOOD) {
		DBG(1, "%s: inquiry failed: %s\n", __func__,
		    sane_strstatus(status));
		return status;
	}

	buf[INQUIRY_BUF_SIZE] = 0;
	DBG(1, "inquiry data:\n");
	DBG(1, " vendor  : %.8s\n", vendor);
	DBG(1, " model   : %.16s\n", model);
	DBG(1, " revision: %.4s\n", rev);

	if (buf[0] != TYPE_PROCESSOR) {
		DBG(1, "%s: device is not of processor type (%d)\n",
		    __func__, buf[0]);
		return SANE_STATUS_INVAL;
	}

	if (strncmp(vendor, "EPSON", 5) != 0) {
		DBG(1,
		    "%s: device doesn't look like an EPSON scanner\n",
		    __func__);
		return SANE_STATUS_INVAL;
	}

	if (strncmp(model, "SCANNER ", 8) != 0
	    && strncmp(model, "FilmScan 200", 12) != 0
	    && strncmp(model, "Perfection", 10) != 0
	    && strncmp(model, "Expression", 10) != 0
	    && strncmp(model, "GT", 2) != 0) {
		DBG(1, "%s: this EPSON scanner is not supported\n",
		    __func__);
		return SANE_STATUS_INVAL;
	}

	if (strncmp(model, "FilmScan 200", 12) == 0) {
		dev->sane.type = "film scanner";
		e2_set_model(s, (unsigned char *) model, 12);
	}

	/* Issue a test unit ready SCSI command. The FilmScan 200
	 * requires it for a sort of "wake up". We might eventually
	 * get the return code and reissue it in case of failure.
	 */
	sanei_epson2_scsi_test_unit_ready(s->fd);

	return SANE_STATUS_GOOD;
}

static SANE_Status
detect_usb(struct Epson_Scanner *s, SANE_Bool assume_valid)
{
	SANE_Status status;
	int vendor, product;
	int i, numIds;
	SANE_Bool is_valid = assume_valid;

	/* if the sanei_usb_get_vendor_product call is not supported,
	 * then we just ignore this and rely on the user to config
	 * the correct device.
	 */

	status = sanei_usb_get_vendor_product(s->fd, &vendor, &product);
	if (status != SANE_STATUS_GOOD) {
		DBG(1, "the device cannot be verified - will continue\n");
	 	return SANE_STATUS_GOOD;
	}
	
	/* check the vendor ID to see if we are dealing with an EPSON device */
	if (vendor != SANE_EPSON_VENDOR_ID) {
		/* this is not a supported vendor ID */
		DBG(1, "not an Epson device at %s (vendor id=0x%x)\n",
			s->hw->sane.name, vendor);
		return SANE_STATUS_INVAL;
	}

	numIds = sanei_epson_getNumberOfUSBProductIds();
	i = 0;

	/* check all known product IDs to verify that we know
	   about the device */
	while (i != numIds) {
		if (product == sanei_epson_usb_product_ids[i]) {
			is_valid = SANE_TRUE;
			break;
		}
		i++;
	}

	if (is_valid == SANE_FALSE) {
		DBG(1, "the device at %s is not supported (product id=0x%x)\n",
			s->hw->sane.name, product);
		return SANE_STATUS_INVAL;
	}
	
	DBG(1, "found valid Epson scanner: 0x%x/0x%x (vendorID/productID)\n",
		vendor, product);

	return SANE_STATUS_GOOD;	    
}

static int num_devices;		/* number of scanners attached to backend */
static Epson_Device *first_dev;	/* first EPSON scanner in list */

static struct Epson_Scanner *
scanner_create(struct Epson_Device *dev, SANE_Status *status)
{
	struct Epson_Scanner *s;

	s = malloc(sizeof(struct Epson_Scanner));
	if (s == NULL) {
		*status = SANE_STATUS_NO_MEM;
		return NULL;
	}

	memset(s, 0x00, sizeof(struct Epson_Scanner));

	s->fd = -1;
	s->hw = dev;

	return s;
}

static struct Epson_Scanner *
device_detect(const char *name, int type, SANE_Bool assume_valid, SANE_Status *status)
{
	struct Epson_Scanner *s;
	struct Epson_Device *dev;

	/* try to find the device in our list */
	for (dev = first_dev; dev; dev = dev->next) {
		if (strcmp(dev->sane.name, name) == 0) {

			/* the device might have been just probed,
			 * sleep a bit.
			 */
			if (dev->connection == SANE_EPSON_NET)
				sleep(1);

			return scanner_create(dev, status);
		}
	}

	if (type == SANE_EPSON_NODEV) {
		*status = SANE_STATUS_INVAL;
		return NULL;
	}
	
	/* alloc and clear our device structure */
	dev = malloc(sizeof(*dev));
	if (!dev) {
		*status = SANE_STATUS_NO_MEM;
		return NULL;
	}
	memset(dev, 0x00, sizeof(struct Epson_Device));

	s = scanner_create(dev, status);
	if (s == NULL)
		return NULL;

	e2_dev_init(dev, name, type);

	*status = open_scanner(s);
	if (*status != SANE_STATUS_GOOD) {
		free(s);
		return NULL;
	}

	/* from now on, close_scanner() must be called */

	/* SCSI and USB requires special care */
	if (dev->connection == SANE_EPSON_SCSI) {

		*status = detect_scsi(s);

	} else if (dev->connection == SANE_EPSON_USB) {

		*status = detect_usb(s, assume_valid);
	}

	if (*status != SANE_STATUS_GOOD)
		goto close;

	/* set name and model (if not already set) */
	if (dev->model == NULL)
		e2_set_model(s, (unsigned char *) "generic", 7);

	dev->name = strdup(name);
	dev->sane.name = dev->name;

	/* ESC @, reset */
	*status = esci_reset(s);
	if (*status != SANE_STATUS_GOOD)
		goto close;

	*status = e2_discover_capabilities(s);
	if (*status != SANE_STATUS_GOOD)
		goto close;

	if (source_list[0] == NULL || dev->dpi_range.min == 0) {
		DBG(1, "something is wrong in the discovery process, aborting.\n");
		*status = SANE_STATUS_IO_ERROR;
		goto close;
	}

	e2_dev_post_init(dev);

	*status = esci_reset(s);
	if (*status != SANE_STATUS_GOOD)
		goto close;

	DBG(1, "scanner model: %s\n", dev->model);

	/* add this scanner to the device list */

	num_devices++;
	dev->next = first_dev;
	first_dev = dev;

	return s;

close:
      	close_scanner(s);
	return NULL;
}


static SANE_Status
attach(const char *name, int type)
{
	SANE_Status status;
	Epson_Scanner *s;

	DBG(7, "%s: devname = %s, type = %d\n", __func__, name, type);

	s = device_detect(name, type, 0, &status);
	if(s == NULL)
		return status;

      	close_scanner(s);
	return status;
}

static SANE_Status
attach_one_scsi(const char *dev)
{
	DBG(7, "%s: dev = %s\n", __func__, dev);
	return attach(dev, SANE_EPSON_SCSI);
}

SANE_Status
attach_one_usb(const char *dev)
{
	DBG(7, "%s: dev = %s\n", __func__, dev);
	return attach(dev, SANE_EPSON_USB);
}

static SANE_Status
attach_one_net(const char *dev)
{
        char name[39+4]; 

	DBG(7, "%s: dev = %s\n", __func__, dev);

	strcpy(name, "net:");
	strcat(name, dev);
	return attach(name, SANE_EPSON_NET);
}

static SANE_Status
attach_one_pio(const char *dev)
{
	DBG(7, "%s: dev = %s\n", __func__, dev);
	return attach(dev, SANE_EPSON_PIO);
}

static SANE_Status
attach_one_config(SANEI_Config __sane_unused__ *config, const char *line)
{
	int vendor, product;

	int len = strlen(line);

	DBG(7, "%s: len = %d, line = %s\n", __func__, len, line);
	
	if (sscanf(line, "usb %i %i", &vendor, &product) == 2) {

		/* add the vendor and product IDs to the list of
		   known devices before we call the attach function */

		int numIds = sanei_epson_getNumberOfUSBProductIds();

		if (vendor != 0x4b8)
			return SANE_STATUS_INVAL; /* this is not an EPSON device */

		sanei_epson_usb_product_ids[numIds - 1] = product;
		sanei_usb_attach_matching_devices(line, attach_one_usb);

	} else if (strncmp(line, "usb", 3) == 0 && len == 3) {

		int i, numIds;

		numIds = sanei_epson_getNumberOfUSBProductIds();

		for (i = 0; i < numIds; i++) {
			sanei_usb_find_devices(0x4b8,
					sanei_epson_usb_product_ids[i], attach_one_usb);
		}

	} else if (strncmp(line, "net", 3) == 0) {

		/* remove the "net" sub string */
		const char *name = sanei_config_skip_whitespace(line + 3);

		if (strncmp(name, "autodiscovery", 13) == 0)
			e2_network_discovery();
		else
			attach_one_net(name);

	} else if (strncmp(line, "pio", 3) == 0) {

		/* remove the "pio" sub string */
		const char *name = sanei_config_skip_whitespace(line + 3);

		attach_one_pio(name);

	} else {
		sanei_config_attach_matching_devices(line, attach_one_scsi);
	}

	return SANE_STATUS_GOOD;
}

static void
free_devices(void)
{
	Epson_Device *dev, *next;

	DBG(5, "%s\n", __func__);

	for (dev = first_dev; dev; dev = next) {
		next = dev->next;
		free(dev->name);
		free(dev->model);
		free(dev);
	}

	free(devlist);

	first_dev = NULL;
}

static void
probe_devices(void)
{
	DBG(5, "%s\n", __func__);

	free_devices();

	sanei_configure_attach(EPSON2_CONFIG_FILE, NULL,
		attach_one_config);
}

SANE_Status
sane_init(SANE_Int *version_code, SANE_Auth_Callback __sane_unused__ authorize)
{
	DBG_INIT();
	DBG(2, "%s: " PACKAGE " " VERSION "\n", __func__);

	DBG(1, "epson2 backend, version %i.%i.%i\n",
		EPSON2_VERSION, EPSON2_REVISION, EPSON2_BUILD);

	if (version_code != NULL)
		*version_code = SANE_VERSION_CODE(SANE_CURRENT_MAJOR, V_MINOR,
					  EPSON2_BUILD);

	sanei_usb_init();

	return SANE_STATUS_GOOD;
}

/* Clean up the list of attached scanners. */
void
sane_exit(void)
{
	DBG(5, "%s\n", __func__);
	free_devices();
}

SANE_Status
sane_get_devices(const SANE_Device ***device_list, SANE_Bool __sane_unused__ local_only)
{
	Epson_Device *dev;
	int i;

	DBG(5, "%s\n", __func__);

	probe_devices();

	devlist = malloc((num_devices + 1) * sizeof(devlist[0]));
	if (!devlist) {
		DBG(1, "out of memory (line %d)\n", __LINE__);
		return SANE_STATUS_NO_MEM;
	}

	DBG(5, "%s - results:\n", __func__);

	for (i = 0, dev = first_dev; i < num_devices && dev; dev = dev->next, i++) {
		DBG(1, " %d (%d): %s\n", i, dev->connection, dev->model);
		devlist[i] = &dev->sane;
	}

	devlist[i] = NULL;

	*device_list = devlist;

	return SANE_STATUS_GOOD;
}

static SANE_Status
init_options(Epson_Scanner *s)
{
	int i;

	for (i = 0; i < NUM_OPTIONS; i++) {
		s->opt[i].size = sizeof(SANE_Word);
		s->opt[i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	}

	s->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
	s->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
	s->opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
	s->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
	s->val[OPT_NUM_OPTS].w = NUM_OPTIONS;

	/* "Scan Mode" group: */

	s->opt[OPT_MODE_GROUP].title = SANE_I18N("Scan Mode");
	s->opt[OPT_MODE_GROUP].desc = "";
	s->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
	s->opt[OPT_MODE_GROUP].cap = 0;

	/* scan mode */
	s->opt[OPT_MODE].name = SANE_NAME_SCAN_MODE;
	s->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
	s->opt[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
	s->opt[OPT_MODE].type = SANE_TYPE_STRING;
	s->opt[OPT_MODE].size = max_string_size(mode_list);
	s->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
	s->opt[OPT_MODE].constraint.string_list = mode_list;
	s->val[OPT_MODE].w = 0;	/* Lineart */

	/* disable infrared on unsupported scanners */
	if (!e2_model(s, "GT-X800") && !e2_model(s, "GT-X700") && !e2_model(s, "GT-X900") && !e2_model(s, "GT-X980"))
		mode_list[MODE_INFRARED] = NULL;

	/* bit depth */
	s->opt[OPT_BIT_DEPTH].name = SANE_NAME_BIT_DEPTH;
	s->opt[OPT_BIT_DEPTH].title = SANE_TITLE_BIT_DEPTH;
	s->opt[OPT_BIT_DEPTH].desc = SANE_DESC_BIT_DEPTH;
	s->opt[OPT_BIT_DEPTH].type = SANE_TYPE_INT;
	s->opt[OPT_BIT_DEPTH].unit = SANE_UNIT_BIT;
	s->opt[OPT_BIT_DEPTH].constraint_type = SANE_CONSTRAINT_WORD_LIST;
	s->opt[OPT_BIT_DEPTH].constraint.word_list = s->hw->depth_list;
	s->val[OPT_BIT_DEPTH].w = 8; /* default to 8 bit */

	/* default is Lineart, disable depth selection */
	s->opt[OPT_BIT_DEPTH].cap |= SANE_CAP_INACTIVE;

	/* halftone */
	s->opt[OPT_HALFTONE].name = SANE_NAME_HALFTONE;
	s->opt[OPT_HALFTONE].title = SANE_TITLE_HALFTONE;
	s->opt[OPT_HALFTONE].desc = SANE_I18N("Selects the halftone.");

	s->opt[OPT_HALFTONE].type = SANE_TYPE_STRING;
	s->opt[OPT_HALFTONE].size = max_string_size(halftone_list_7);
	s->opt[OPT_HALFTONE].constraint_type = SANE_CONSTRAINT_STRING_LIST;

	/* XXX use defines */
	if (s->hw->level >= 7)
		s->opt[OPT_HALFTONE].constraint.string_list = halftone_list_7;
	else if (s->hw->level >= 4)
		s->opt[OPT_HALFTONE].constraint.string_list = halftone_list_4;
	else
		s->opt[OPT_HALFTONE].constraint.string_list = halftone_list;

	s->val[OPT_HALFTONE].w = 1;	/* Halftone A */

	if (!s->hw->cmd->set_halftoning)
		s->opt[OPT_HALFTONE].cap |= SANE_CAP_INACTIVE;

	/* dropout */
	s->opt[OPT_DROPOUT].name = "dropout";
	s->opt[OPT_DROPOUT].title = SANE_I18N("Dropout");
	s->opt[OPT_DROPOUT].desc = SANE_I18N("Selects the dropout.");

	s->opt[OPT_DROPOUT].type = SANE_TYPE_STRING;
	s->opt[OPT_DROPOUT].size = max_string_size(dropout_list);
	s->opt[OPT_DROPOUT].cap |= SANE_CAP_ADVANCED;
	s->opt[OPT_DROPOUT].constraint_type = SANE_CONSTRAINT_STRING_LIST;
	s->opt[OPT_DROPOUT].constraint.string_list = dropout_list;
	s->val[OPT_DROPOUT].w = 0;	/* None */

	/* brightness */
	s->opt[OPT_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
	s->opt[OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
	s->opt[OPT_BRIGHTNESS].desc = SANE_I18N("Selects the brightness.");

	s->opt[OPT_BRIGHTNESS].type = SANE_TYPE_INT;
	s->opt[OPT_BRIGHTNESS].unit = SANE_UNIT_NONE;
	s->opt[OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_BRIGHTNESS].constraint.range = &s->hw->cmd->bright_range;
	s->val[OPT_BRIGHTNESS].w = 0;	/* Normal */

	if (!s->hw->cmd->set_bright)
		s->opt[OPT_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;

	/* sharpness */
	s->opt[OPT_SHARPNESS].name = "sharpness";
	s->opt[OPT_SHARPNESS].title = SANE_I18N("Sharpness");
	s->opt[OPT_SHARPNESS].desc = "";

	s->opt[OPT_SHARPNESS].type = SANE_TYPE_INT;
	s->opt[OPT_SHARPNESS].unit = SANE_UNIT_NONE;
	s->opt[OPT_SHARPNESS].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_SHARPNESS].constraint.range = &outline_emphasis_range;
	s->val[OPT_SHARPNESS].w = 0;	/* Normal */

	if (!s->hw->cmd->set_outline_emphasis)
		s->opt[OPT_SHARPNESS].cap |= SANE_CAP_INACTIVE;

	/* gamma */
	s->opt[OPT_GAMMA_CORRECTION].name = SANE_NAME_GAMMA_CORRECTION;
	s->opt[OPT_GAMMA_CORRECTION].title = SANE_TITLE_GAMMA_CORRECTION;
	s->opt[OPT_GAMMA_CORRECTION].desc = SANE_DESC_GAMMA_CORRECTION;

	s->opt[OPT_GAMMA_CORRECTION].type = SANE_TYPE_STRING;
	s->opt[OPT_GAMMA_CORRECTION].constraint_type =
		SANE_CONSTRAINT_STRING_LIST;

	/*
	 * special handling for D1 function level - at this time I'm not
	 * testing for D1, I'm just assuming that all D level scanners will
	 * behave the same way. This has to be confirmed with the next D-level
	 * scanner
	 */
	if (s->hw->cmd->level[0] == 'D') {
		s->opt[OPT_GAMMA_CORRECTION].size =
			max_string_size(gamma_list_d);
		s->opt[OPT_GAMMA_CORRECTION].constraint.string_list =
			gamma_list_d;
		s->val[OPT_GAMMA_CORRECTION].w = 1;	/* Default */
		gamma_userdefined = gamma_userdefined_d;
		gamma_params = gamma_params_d;
	} else {
		s->opt[OPT_GAMMA_CORRECTION].size =
			max_string_size(gamma_list_ab);
		s->opt[OPT_GAMMA_CORRECTION].constraint.string_list =
			gamma_list_ab;
		s->val[OPT_GAMMA_CORRECTION].w = 0;	/* Default */
		gamma_userdefined = gamma_userdefined_ab;
		gamma_params = gamma_params_ab;
	}

	if (!s->hw->cmd->set_gamma)
		s->opt[OPT_GAMMA_CORRECTION].cap |= SANE_CAP_INACTIVE;

	/* red gamma vector */
	s->opt[OPT_GAMMA_VECTOR_R].name = SANE_NAME_GAMMA_VECTOR_R;
	s->opt[OPT_GAMMA_VECTOR_R].title = SANE_TITLE_GAMMA_VECTOR_R;
	s->opt[OPT_GAMMA_VECTOR_R].desc = SANE_DESC_GAMMA_VECTOR_R;

	s->opt[OPT_GAMMA_VECTOR_R].type = SANE_TYPE_INT;
	s->opt[OPT_GAMMA_VECTOR_R].unit = SANE_UNIT_NONE;
	s->opt[OPT_GAMMA_VECTOR_R].size = 256 * sizeof(SANE_Word);
	s->opt[OPT_GAMMA_VECTOR_R].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_GAMMA_VECTOR_R].constraint.range = &u8_range;
	s->val[OPT_GAMMA_VECTOR_R].wa = &s->gamma_table[0][0];

	/* green gamma vector */
	s->opt[OPT_GAMMA_VECTOR_G].name = SANE_NAME_GAMMA_VECTOR_G;
	s->opt[OPT_GAMMA_VECTOR_G].title = SANE_TITLE_GAMMA_VECTOR_G;
	s->opt[OPT_GAMMA_VECTOR_G].desc = SANE_DESC_GAMMA_VECTOR_G;

	s->opt[OPT_GAMMA_VECTOR_G].type = SANE_TYPE_INT;
	s->opt[OPT_GAMMA_VECTOR_G].unit = SANE_UNIT_NONE;
	s->opt[OPT_GAMMA_VECTOR_G].size = 256 * sizeof(SANE_Word);
	s->opt[OPT_GAMMA_VECTOR_G].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_GAMMA_VECTOR_G].constraint.range = &u8_range;
	s->val[OPT_GAMMA_VECTOR_G].wa = &s->gamma_table[1][0];


	/* red gamma vector */
	s->opt[OPT_GAMMA_VECTOR_B].name = SANE_NAME_GAMMA_VECTOR_B;
	s->opt[OPT_GAMMA_VECTOR_B].title = SANE_TITLE_GAMMA_VECTOR_B;
	s->opt[OPT_GAMMA_VECTOR_B].desc = SANE_DESC_GAMMA_VECTOR_B;

	s->opt[OPT_GAMMA_VECTOR_B].type = SANE_TYPE_INT;
	s->opt[OPT_GAMMA_VECTOR_B].unit = SANE_UNIT_NONE;
	s->opt[OPT_GAMMA_VECTOR_B].size = 256 * sizeof(SANE_Word);
	s->opt[OPT_GAMMA_VECTOR_B].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_GAMMA_VECTOR_B].constraint.range = &u8_range;
	s->val[OPT_GAMMA_VECTOR_B].wa = &s->gamma_table[2][0];

	if (s->hw->cmd->set_gamma_table
	    && gamma_userdefined[s->val[OPT_GAMMA_CORRECTION].w] ==
	    SANE_TRUE) {

		s->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
		s->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
		s->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
	} else {

		s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
		s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
		s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
	}

	/* initialize the Gamma tables */
	memset(&s->gamma_table[0], 0, 256 * sizeof(SANE_Word));
	memset(&s->gamma_table[1], 0, 256 * sizeof(SANE_Word));
	memset(&s->gamma_table[2], 0, 256 * sizeof(SANE_Word));

/*	memset(&s->gamma_table[3], 0, 256 * sizeof(SANE_Word)); */
	for (i = 0; i < 256; i++) {
		s->gamma_table[0][i] = i;
		s->gamma_table[1][i] = i;
		s->gamma_table[2][i] = i;

/*		s->gamma_table[3][i] = i; */
	}


	/* color correction */
	s->opt[OPT_COLOR_CORRECTION].name = "color-correction";
	s->opt[OPT_COLOR_CORRECTION].title = SANE_I18N("Color correction");
	s->opt[OPT_COLOR_CORRECTION].desc =
		SANE_I18N("Sets the color correction table for the selected output device.");

	s->opt[OPT_COLOR_CORRECTION].type = SANE_TYPE_STRING;
	s->opt[OPT_COLOR_CORRECTION].size = max_string_size(correction_list);
	s->opt[OPT_COLOR_CORRECTION].cap |= SANE_CAP_ADVANCED;
	s->opt[OPT_COLOR_CORRECTION].constraint_type = SANE_CONSTRAINT_STRING_LIST;
	s->opt[OPT_COLOR_CORRECTION].constraint.string_list = correction_list;
	s->val[OPT_COLOR_CORRECTION].w = CORR_AUTO;

	if (!s->hw->cmd->set_color_correction)
		s->opt[OPT_COLOR_CORRECTION].cap |= SANE_CAP_INACTIVE;

	/* resolution */
	s->opt[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
	s->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
	s->opt[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;

	s->opt[OPT_RESOLUTION].type = SANE_TYPE_INT;
	s->opt[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
	s->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
	s->opt[OPT_RESOLUTION].constraint.word_list = s->hw->resolution_list;
	s->val[OPT_RESOLUTION].w = s->hw->dpi_range.min;

	/* threshold */
	s->opt[OPT_THRESHOLD].name = SANE_NAME_THRESHOLD;
	s->opt[OPT_THRESHOLD].title = SANE_TITLE_THRESHOLD;
	s->opt[OPT_THRESHOLD].desc = SANE_DESC_THRESHOLD;

	s->opt[OPT_THRESHOLD].type = SANE_TYPE_INT;
	s->opt[OPT_THRESHOLD].unit = SANE_UNIT_NONE;
	s->opt[OPT_THRESHOLD].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_THRESHOLD].constraint.range = &u8_range;
	s->val[OPT_THRESHOLD].w = 0x80;

	if (!s->hw->cmd->set_threshold)
		s->opt[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;


	/* "Advanced" group: */
	s->opt[OPT_ADVANCED_GROUP].title = SANE_I18N("Advanced");
	s->opt[OPT_ADVANCED_GROUP].desc = "";
	s->opt[OPT_ADVANCED_GROUP].type = SANE_TYPE_GROUP;
	s->opt[OPT_ADVANCED_GROUP].cap = SANE_CAP_ADVANCED;

	/* "Color correction" group: */
	s->opt[OPT_CCT_GROUP].title = SANE_I18N("Color correction");
	s->opt[OPT_CCT_GROUP].desc = "";
	s->opt[OPT_CCT_GROUP].type = SANE_TYPE_GROUP;
	s->opt[OPT_CCT_GROUP].cap = SANE_CAP_ADVANCED;

	/* XXX disabled for now */
	s->opt[OPT_CCT_MODE].name = "cct-type";
	s->opt[OPT_CCT_MODE].title = "CCT Profile Type";
	s->opt[OPT_CCT_MODE].desc = "Color correction profile type";
	s->opt[OPT_CCT_MODE].type = SANE_TYPE_STRING;
	s->opt[OPT_CCT_MODE].cap  |= SANE_CAP_ADVANCED | SANE_CAP_INACTIVE;
	s->opt[OPT_CCT_MODE].size = max_string_size(cct_mode_list);
	s->opt[OPT_CCT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
	s->opt[OPT_CCT_MODE].constraint.string_list = cct_mode_list;
	s->val[OPT_CCT_MODE].w = CCT_AUTO;

	s->opt[OPT_CCT_PROFILE].name = "cct-profile";
	s->opt[OPT_CCT_PROFILE].title = "CCT Profile";
	s->opt[OPT_CCT_PROFILE].desc = "Color correction profile data";
	s->opt[OPT_CCT_PROFILE].type = SANE_TYPE_FIXED;
	s->opt[OPT_CCT_PROFILE].cap  |= SANE_CAP_ADVANCED;
	s->opt[OPT_CCT_PROFILE].unit = SANE_UNIT_NONE;
	s->opt[OPT_CCT_PROFILE].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_CCT_PROFILE].constraint.range = &fx_range;
	s->opt[OPT_CCT_PROFILE].size = 9 * sizeof(SANE_Word);
	s->val[OPT_CCT_PROFILE].wa = s->cct_table;

/*	if (!s->hw->cmd->set_color_correction)
		s->opt[OPT_FILM_TYPE].cap |= SANE_CAP_INACTIVE;
*/	

	/* mirror */
	s->opt[OPT_MIRROR].name = "mirror";
	s->opt[OPT_MIRROR].title = SANE_I18N("Mirror image");
	s->opt[OPT_MIRROR].desc = SANE_I18N("Mirror the image.");

	s->opt[OPT_MIRROR].type = SANE_TYPE_BOOL;
	s->val[OPT_MIRROR].w = SANE_FALSE;

	if (!s->hw->cmd->mirror_image)
		s->opt[OPT_MIRROR].cap |= SANE_CAP_INACTIVE;

	/* auto area segmentation */
	s->opt[OPT_AAS].name = "auto-area-segmentation";
	s->opt[OPT_AAS].title = SANE_I18N("Auto area segmentation");
	s->opt[OPT_AAS].desc =
		"Enables different dithering modes in image and text areas";

	s->opt[OPT_AAS].type = SANE_TYPE_BOOL;
	s->val[OPT_AAS].w = SANE_TRUE;

	if (!s->hw->cmd->control_auto_area_segmentation)
		s->opt[OPT_AAS].cap |= SANE_CAP_INACTIVE;

	/* "Preview settings" group: */
	s->opt[OPT_PREVIEW_GROUP].title = SANE_TITLE_PREVIEW;
	s->opt[OPT_PREVIEW_GROUP].desc = "";
	s->opt[OPT_PREVIEW_GROUP].type = SANE_TYPE_GROUP;
	s->opt[OPT_PREVIEW_GROUP].cap = SANE_CAP_ADVANCED;

	/* preview */
	s->opt[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
	s->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
	s->opt[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;

	s->opt[OPT_PREVIEW].type = SANE_TYPE_BOOL;
	s->val[OPT_PREVIEW].w = SANE_FALSE;

	/* "Geometry" group: */
	s->opt[OPT_GEOMETRY_GROUP].title = SANE_I18N("Geometry");
	s->opt[OPT_GEOMETRY_GROUP].desc = "";
	s->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
	s->opt[OPT_GEOMETRY_GROUP].cap = SANE_CAP_ADVANCED;

	/* top-left x */
	s->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
	s->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
	s->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;

	s->opt[OPT_TL_X].type = SANE_TYPE_FIXED;
	s->opt[OPT_TL_X].unit = SANE_UNIT_MM;
	s->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_TL_X].constraint.range = s->hw->x_range;
	s->val[OPT_TL_X].w = 0;

	/* top-left y */
	s->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
	s->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
	s->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;

	s->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
	s->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
	s->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_TL_Y].constraint.range = s->hw->y_range;
	s->val[OPT_TL_Y].w = 0;

	/* bottom-right x */
	s->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
	s->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
	s->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;

	s->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
	s->opt[OPT_BR_X].unit = SANE_UNIT_MM;
	s->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_BR_X].constraint.range = s->hw->x_range;
	s->val[OPT_BR_X].w = s->hw->x_range->max;

	/* bottom-right y */
	s->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
	s->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
	s->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;

	s->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
	s->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
	s->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
	s->opt[OPT_BR_Y].constraint.range = s->hw->y_range;
	s->val[OPT_BR_Y].w = s->hw->y_range->max;

	/* "Optional equipment" group: */
	s->opt[OPT_EQU_GROUP].title = SANE_I18N("Optional equipment");
	s->opt[OPT_EQU_GROUP].desc = "";
	s->opt[OPT_EQU_GROUP].type = SANE_TYPE_GROUP;
	s->opt[OPT_EQU_GROUP].cap = SANE_CAP_ADVANCED;

	/* source */
	s->opt[OPT_SOURCE].name = SANE_NAME_SCAN_SOURCE;
	s->opt[OPT_SOURCE].title = SANE_TITLE_SCAN_SOURCE;
	s->opt[OPT_SOURCE].desc = SANE_DESC_SCAN_SOURCE;

	s->opt[OPT_SOURCE].type = SANE_TYPE_STRING;
	s->opt[OPT_SOURCE].size = max_string_size(source_list);

	s->opt[OPT_SOURCE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
	s->opt[OPT_SOURCE].constraint.string_list = source_list;

	if (!s->hw->extension)
		s->opt[OPT_SOURCE].cap |= SANE_CAP_INACTIVE;

	s->val[OPT_SOURCE].w = 0;	/* always use Flatbed as default */


	/* film type */
	s->opt[OPT_FILM_TYPE].name = "film-type";
	s->opt[OPT_FILM_TYPE].title = SANE_I18N("Film type");
	s->opt[OPT_FILM_TYPE].desc = "";
	s->opt[OPT_FILM_TYPE].type = SANE_TYPE_STRING;
	s->opt[OPT_FILM_TYPE].size = max_string_size(film_list);
	s->opt[OPT_FILM_TYPE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
	s->opt[OPT_FILM_TYPE].constraint.string_list = film_list;
	s->val[OPT_FILM_TYPE].w = 0;

	if (!s->hw->cmd->set_bay)
		s->opt[OPT_FILM_TYPE].cap |= SANE_CAP_INACTIVE;

	/* focus position */
	s->opt[OPT_FOCUS].name = SANE_EPSON_FOCUS_NAME;
	s->opt[OPT_FOCUS].title = SANE_EPSON_FOCUS_TITLE;
	s->opt[OPT_FOCUS].desc = SANE_EPSON_FOCUS_DESC;
	s->opt[OPT_FOCUS].type = SANE_TYPE_STRING;
	s->opt[OPT_FOCUS].size = max_string_size(focus_list);
	s->opt[OPT_FOCUS].constraint_type = SANE_CONSTRAINT_STRING_LIST;
	s->opt[OPT_FOCUS].constraint.string_list = focus_list;
	s->val[OPT_FOCUS].w = 0;
	s->opt[OPT_FOCUS].cap |= SANE_CAP_ADVANCED;

	if (s->hw->focusSupport == SANE_TRUE)
		s->opt[OPT_FOCUS].cap &= ~SANE_CAP_INACTIVE;
	else
		s->opt[OPT_FOCUS].cap |= SANE_CAP_INACTIVE;

	/* forward feed / eject */
	s->opt[OPT_EJECT].name = "eject";
	s->opt[OPT_EJECT].title = SANE_I18N("Eject");
	s->opt[OPT_EJECT].desc = SANE_I18N("Eject the sheet in the ADF");
	s->opt[OPT_EJECT].type = SANE_TYPE_BUTTON;

	if ((!s->hw->ADF) && (!s->hw->cmd->set_bay)) {	/* Hack: Using set_bay to indicate. */
		s->opt[OPT_EJECT].cap |= SANE_CAP_INACTIVE;
	}


	/* auto forward feed / eject */
	s->opt[OPT_AUTO_EJECT].name = "auto-eject";
	s->opt[OPT_AUTO_EJECT].title = SANE_I18N("Auto eject");
	s->opt[OPT_AUTO_EJECT].desc =
		SANE_I18N("Eject document after scanning");

	s->opt[OPT_AUTO_EJECT].type = SANE_TYPE_BOOL;
	s->val[OPT_AUTO_EJECT].w = SANE_FALSE;

	if (!s->hw->ADF)
		s->opt[OPT_AUTO_EJECT].cap |= SANE_CAP_INACTIVE;


	s->opt[OPT_ADF_MODE].name = "adf-mode";
	s->opt[OPT_ADF_MODE].title = SANE_I18N("ADF Mode");
	s->opt[OPT_ADF_MODE].desc =
		SANE_I18N("Selects the ADF mode (simplex/duplex)");
	s->opt[OPT_ADF_MODE].type = SANE_TYPE_STRING;
	s->opt[OPT_ADF_MODE].size = max_string_size(adf_mode_list);
	s->opt[OPT_ADF_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
	s->opt[OPT_ADF_MODE].constraint.string_list = adf_mode_list;
	s->val[OPT_ADF_MODE].w = 0;	/* simplex */

	if ((!s->hw->ADF) || (s->hw->duplex == SANE_FALSE))
		s->opt[OPT_ADF_MODE].cap |= SANE_CAP_INACTIVE;

	/* select bay */
	s->opt[OPT_BAY].name = "bay";
	s->opt[OPT_BAY].title = SANE_I18N("Bay");
	s->opt[OPT_BAY].desc = SANE_I18N("Select bay to scan");
	s->opt[OPT_BAY].type = SANE_TYPE_STRING;
	s->opt[OPT_BAY].size = max_string_size(bay_list);
	s->opt[OPT_BAY].constraint_type = SANE_CONSTRAINT_STRING_LIST;
	s->opt[OPT_BAY].constraint.string_list = bay_list;
	s->val[OPT_BAY].w = 0;	/* Bay 1 */

	if (!s->hw->cmd->set_bay)
		s->opt[OPT_BAY].cap |= SANE_CAP_INACTIVE;


	s->opt[OPT_WAIT_FOR_BUTTON].name = SANE_EPSON_WAIT_FOR_BUTTON_NAME;
	s->opt[OPT_WAIT_FOR_BUTTON].title = SANE_EPSON_WAIT_FOR_BUTTON_TITLE;
	s->opt[OPT_WAIT_FOR_BUTTON].desc = SANE_EPSON_WAIT_FOR_BUTTON_DESC;

	s->opt[OPT_WAIT_FOR_BUTTON].type = SANE_TYPE_BOOL;
	s->opt[OPT_WAIT_FOR_BUTTON].unit = SANE_UNIT_NONE;
	s->opt[OPT_WAIT_FOR_BUTTON].constraint_type = SANE_CONSTRAINT_NONE;
	s->opt[OPT_WAIT_FOR_BUTTON].constraint.range = NULL;
	s->opt[OPT_WAIT_FOR_BUTTON].cap |= SANE_CAP_ADVANCED;

	if (!s->hw->cmd->request_push_button_status)
		s->opt[OPT_WAIT_FOR_BUTTON].cap |= SANE_CAP_INACTIVE;

	return SANE_STATUS_GOOD;
}

SANE_Status
sane_open(SANE_String_Const name, SANE_Handle *handle)
{
	SANE_Status status;
	Epson_Scanner *s = NULL;

	int l = strlen(name);

	DBG(7, "%s: name = %s\n", __func__, name);

	*handle = NULL;

	/* probe if empty device name provided */
	if (l == 0) {

		probe_devices();

		if (first_dev == NULL) {
			DBG(1, "no device detected\n");
			return SANE_STATUS_INVAL;
		}

		s = device_detect(first_dev->sane.name, first_dev->connection,
					0, &status);
		if (s == NULL) {
			DBG(1, "cannot open a perfectly valid device (%s),"
				" please report to the authors\n", name);
			return SANE_STATUS_INVAL;
		}

	} else {

		if (strncmp(name, "net:", 4) == 0) {
			s = device_detect(name, SANE_EPSON_NET, 0, &status);
			if (s == NULL)
				return status;
		} else if (strncmp(name, "libusb:", 7) == 0) {
			s = device_detect(name, SANE_EPSON_USB, 1, &status);
			if (s == NULL)
				return status;
		} else if (strncmp(name, "pio:", 4) == 0) {
			s = device_detect(name, SANE_EPSON_PIO, 0, &status);
			if (s == NULL)
				return status;
		} else {
		
			/* as a last resort, check for a match
			 * in the device list. This should handle SCSI
			 * devices and platforms without libusb.
			 */
			
			if (first_dev == NULL)
				probe_devices();

			s = device_detect(name, SANE_EPSON_NODEV, 0, &status);
			if (s == NULL) {
				DBG(1, "invalid device name: %s\n", name);
				return SANE_STATUS_INVAL;
			}
		}		
	}


	/* s is always valid here */

	DBG(1, "handle obtained\n");

	init_options(s);

	status = open_scanner(s);
	if (status != SANE_STATUS_GOOD) {
		free(s);
		return status;
	}

	status = esci_reset(s);
	if (status != SANE_STATUS_GOOD) {
		close_scanner(s);
		return status;
	}

	*handle = (SANE_Handle)s;
	
	return SANE_STATUS_GOOD;
}

void
sane_close(SANE_Handle handle)
{
	Epson_Scanner *s;

	DBG(1, "* %s\n", __func__);

	/*
	 * XXX Test if there is still data pending from
	 * the scanner. If so, then do a cancel
	 */

	s = (Epson_Scanner *) handle;

	close_scanner(s);
}

const SANE_Option_Descriptor *
sane_get_option_descriptor(SANE_Handle handle, SANE_Int option)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;

	if (option < 0 || option >= NUM_OPTIONS)
		return NULL;

	return s->opt + option;
}

static const SANE_String_Const *
search_string_list(const SANE_String_Const *list, SANE_String value)
{
	while (*list != NULL && strcmp(value, *list) != 0)
		list++;

	return ((*list == NULL) ? NULL : list);
}

/*
    Activate, deactivate an option. Subroutines so we can add
    debugging info if we want. The change flag is set to TRUE
    if we changed an option. If we did not change an option,
    then the value of the changed flag is not modified.
*/

static void
activateOption(Epson_Scanner *s, SANE_Int option, SANE_Bool *change)
{
	if (!SANE_OPTION_IS_ACTIVE(s->opt[option].cap)) {
		s->opt[option].cap &= ~SANE_CAP_INACTIVE;
		*change = SANE_TRUE;
	}
}

static void
deactivateOption(Epson_Scanner *s, SANE_Int option, SANE_Bool *change)
{
	if (SANE_OPTION_IS_ACTIVE(s->opt[option].cap)) {
		s->opt[option].cap |= SANE_CAP_INACTIVE;
		*change = SANE_TRUE;
	}
}

static void
setOptionState(Epson_Scanner *s, SANE_Bool state, SANE_Int option,
	       SANE_Bool *change)
{
	if (state)
		activateOption(s, option, change);
	else
		deactivateOption(s, option, change);
}

static SANE_Status
getvalue(SANE_Handle handle, SANE_Int option, void *value)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;
	SANE_Option_Descriptor *sopt = &(s->opt[option]);
	Option_Value *sval = &(s->val[option]);

	DBG(17, "%s: option = %d\n", __func__, option);

	switch (option) {

	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:
	case OPT_CCT_PROFILE:
		memcpy(value, sval->wa, sopt->size);
		break;

	case OPT_NUM_OPTS:
	case OPT_RESOLUTION:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_MIRROR:
	case OPT_AAS:
	case OPT_PREVIEW:
	case OPT_BRIGHTNESS:
	case OPT_SHARPNESS:
	case OPT_AUTO_EJECT:
	case OPT_THRESHOLD:
	case OPT_BIT_DEPTH:
	case OPT_WAIT_FOR_BUTTON:
		*((SANE_Word *) value) = sval->w;
		break;

	case OPT_MODE:
	case OPT_CCT_MODE:
	case OPT_ADF_MODE:
	case OPT_HALFTONE:
	case OPT_DROPOUT:
	case OPT_SOURCE:
	case OPT_FILM_TYPE:
	case OPT_GAMMA_CORRECTION:
	case OPT_COLOR_CORRECTION:
	case OPT_BAY:
	case OPT_FOCUS:
		strcpy((char *) value, sopt->constraint.string_list[sval->w]);
		break;

	default:
		return SANE_STATUS_INVAL;
	}

	return SANE_STATUS_GOOD;
}

/*
 * This routine handles common options between OPT_MODE and
 * OPT_HALFTONE.  These options are TET (a HALFTONE mode), AAS
 * - auto area segmentation, and threshold.  Apparently AAS
 * is some method to differentiate between text and photos.
 * Or something like that.
 *
 * AAS is available when the scan color depth is 1 and the
 * halftone method is not TET.
 *
 * Threshold is available when halftone is NONE, and depth is 1.
 */
static void
handle_depth_halftone(Epson_Scanner *s, SANE_Bool *reload)
{
	int hti = s->val[OPT_HALFTONE].w;
	int mdi = s->val[OPT_MODE].w;
	SANE_Bool aas = SANE_FALSE;
	SANE_Bool thresh = SANE_FALSE;

	/* this defaults to false */
	setOptionState(s, thresh, OPT_THRESHOLD, reload);

	if (!s->hw->cmd->control_auto_area_segmentation)
		return;

	if (mode_params[mdi].depth == 1) {

		if (halftone_params[hti] != HALFTONE_TET)
			aas = SANE_TRUE;

		if (halftone_params[hti] == HALFTONE_NONE)
			thresh = SANE_TRUE;
	}
	setOptionState(s, aas, OPT_AAS, reload);
	setOptionState(s, thresh, OPT_THRESHOLD, reload);
}

/*
 * Handles setting the source (flatbed, transparency adapter (TPU),
 * or auto document feeder (ADF)).
 *
 * For newer scanners it also sets the focus according to the
 * glass / TPU settings.
 */

static void
change_source(Epson_Scanner *s, SANE_Int optindex, char *value)
{
	int force_max = SANE_FALSE;
	SANE_Bool dummy;

	DBG(1, "%s: optindex = %d, source = '%s'\n", __func__, optindex,
	    value);

	/* reset the scanner when we are changing the source setting -
	   this is necessary for the Perfection 1650 */
	if (s->hw->need_reset_on_source_change)
		esci_reset(s);

	s->focusOnGlass = SANE_TRUE;	/* this is the default */

	if (s->val[OPT_SOURCE].w == optindex)
		return;

	s->val[OPT_SOURCE].w = optindex;

	if (s->val[OPT_TL_X].w == s->hw->x_range->min
	    && s->val[OPT_TL_Y].w == s->hw->y_range->min
	    && s->val[OPT_BR_X].w == s->hw->x_range->max
	    && s->val[OPT_BR_Y].w == s->hw->y_range->max) {
		force_max = SANE_TRUE;
	}

	if (strcmp(ADF_STR, value) == 0) {
		s->hw->x_range = &s->hw->adf_x_range;
		s->hw->y_range = &s->hw->adf_y_range;
		s->hw->use_extension = SANE_TRUE;
		/* disable film type option */
		deactivateOption(s, OPT_FILM_TYPE, &dummy);
		s->val[OPT_FOCUS].w = 0;
		if (s->hw->duplex) {
			activateOption(s, OPT_ADF_MODE, &dummy);
		} else {
			deactivateOption(s, OPT_ADF_MODE, &dummy);
			s->val[OPT_ADF_MODE].w = 0;
		}

		DBG(1, "adf activated (ext: %d, duplex: %d)\n",
			s->hw->use_extension,
			s->hw->duplex);

	} else if (strcmp(TPU_STR, value) == 0 || strcmp(TPU_STR2, value) == 0) {
	        if (strcmp(TPU_STR, value) == 0) {
	  	        s->hw->x_range = &s->hw->tpu_x_range;
		        s->hw->y_range = &s->hw->tpu_y_range;
			s->hw->TPU2 = SANE_FALSE;
                }
	        if (strcmp(TPU_STR2, value) == 0) {
	  	        s->hw->x_range = &s->hw->tpu2_x_range;
		        s->hw->y_range = &s->hw->tpu2_y_range;
			s->hw->TPU2 = SANE_TRUE;
                }
		s->hw->use_extension = SANE_TRUE;

		/* enable film type option only if the scanner supports it */
		if (s->hw->cmd->set_film_type != 0)
			activateOption(s, OPT_FILM_TYPE, &dummy);
		else
			deactivateOption(s, OPT_FILM_TYPE, &dummy);

		/* enable focus position if the scanner supports it */
		if (s->hw->cmd->set_focus_position != 0) {
			s->val[OPT_FOCUS].w = 1;
			s->focusOnGlass = SANE_FALSE;
		}

		deactivateOption(s, OPT_ADF_MODE, &dummy);
		deactivateOption(s, OPT_EJECT, &dummy);
		deactivateOption(s, OPT_AUTO_EJECT, &dummy);
	} else {
		/* neither ADF nor TPU active */
		s->hw->x_range = &s->hw->fbf_x_range;
		s->hw->y_range = &s->hw->fbf_y_range;
		s->hw->use_extension = SANE_FALSE;

		/* disable film type option */
		deactivateOption(s, OPT_FILM_TYPE, &dummy);
		s->val[OPT_FOCUS].w = 0;
		deactivateOption(s, OPT_ADF_MODE, &dummy);
	}

	/* special handling for FilmScan 200 */
	if (s->hw->cmd->level[0] == 'F')
		activateOption(s, OPT_FILM_TYPE, &dummy);

	s->opt[OPT_BR_X].constraint.range = s->hw->x_range;
	s->opt[OPT_BR_Y].constraint.range = s->hw->y_range;

	if (s->val[OPT_TL_X].w < s->hw->x_range->min || force_max)
		s->val[OPT_TL_X].w = s->hw->x_range->min;

	if (s->val[OPT_TL_Y].w < s->hw->y_range->min || force_max)
		s->val[OPT_TL_Y].w = s->hw->y_range->min;

	if (s->val[OPT_BR_X].w > s->hw->x_range->max || force_max)
		s->val[OPT_BR_X].w = s->hw->x_range->max;

	if (s->val[OPT_BR_Y].w > s->hw->y_range->max || force_max)
		s->val[OPT_BR_Y].w = s->hw->y_range->max;

	setOptionState(s, s->hw->ADF
		       && s->hw->use_extension, OPT_AUTO_EJECT, &dummy);
	setOptionState(s, s->hw->ADF
		       && s->hw->use_extension, OPT_EJECT, &dummy);
}

static SANE_Status
setvalue(SANE_Handle handle, SANE_Int option, void *value, SANE_Int *info)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;
	SANE_Option_Descriptor *sopt = &(s->opt[option]);
	Option_Value *sval = &(s->val[option]);

	SANE_Status status;
	const SANE_String_Const *optval = NULL;
	int optindex = 0;
	SANE_Bool reload = SANE_FALSE;

	DBG(17, "%s: option = %d, value = %p\n", __func__, option, value);

	status = sanei_constrain_value(sopt, value, info);
	if (status != SANE_STATUS_GOOD)
		return status;

	if (info && value && (*info & SANE_INFO_INEXACT)
	    && sopt->type == SANE_TYPE_INT)
		DBG(17, "%s: constrained val = %d\n", __func__,
		    *(SANE_Word *) value);

	if (sopt->constraint_type == SANE_CONSTRAINT_STRING_LIST) {
		optval = search_string_list(sopt->constraint.string_list,
					    (char *) value);
		if (optval == NULL)
			return SANE_STATUS_INVAL;
		optindex = optval - sopt->constraint.string_list;
	}

	switch (option) {

	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:
	case OPT_CCT_PROFILE:
		memcpy(sval->wa, value, sopt->size);	/* Word arrays */
		break;

	case OPT_CCT_MODE:
	case OPT_ADF_MODE:
	case OPT_DROPOUT:
	case OPT_FILM_TYPE:
	case OPT_BAY:
	case OPT_FOCUS:
		sval->w = optindex;	/* Simple lists */
		break;

	case OPT_EJECT:
		/* XXX required?  control_extension(s, 1); */
		esci_eject(s);
		break;

	case OPT_RESOLUTION:
		sval->w = *((SANE_Word *) value);
		DBG(17, "setting resolution to %d\n", sval->w);
		reload = SANE_TRUE;
		break;

	case OPT_BR_X:
	case OPT_BR_Y:
		sval->w = *((SANE_Word *) value);
		if (SANE_UNFIX(sval->w) == 0) {
			DBG(17, "invalid br-x or br-y\n");
			return SANE_STATUS_INVAL;
		}
		/* passthru */
	case OPT_TL_X:
	case OPT_TL_Y:
		sval->w = *((SANE_Word *) value);
		DBG(17, "setting size to %f\n", SANE_UNFIX(sval->w));
		if (NULL != info)
			*info |= SANE_INFO_RELOAD_PARAMS;
		break;

	case OPT_SOURCE:
		change_source(s, optindex, (char *) value);
		reload = SANE_TRUE;
		break;

	case OPT_MODE:
	{
		SANE_Bool isColor = mode_params[optindex].color;

		sval->w = optindex;

		DBG(17, "%s: setting mode to %d\n", __func__, optindex);

		/* halftoning available only on bw scans */
		if (s->hw->cmd->set_halftoning != 0)
			setOptionState(s, mode_params[optindex].depth == 1,
				       OPT_HALFTONE, &reload);

		/* disable dropout on non-color scans */
		setOptionState(s, !isColor, OPT_DROPOUT, &reload);

		if (s->hw->cmd->set_color_correction)
			setOptionState(s, isColor,
				       OPT_COLOR_CORRECTION, &reload);

		/* if binary, then disable the bit depth selection */
		if (optindex == 0) {
			DBG(17, "%s: disabling bit depth selection\n", __func__);
			s->opt[OPT_BIT_DEPTH].cap |= SANE_CAP_INACTIVE;
		} else {
			if (s->hw->depth_list[0] == 1) {
				DBG(17, "%s: only one depth is available\n", __func__);
				s->opt[OPT_BIT_DEPTH].cap |= SANE_CAP_INACTIVE;
			} else {

				DBG(17, "%s: enabling bit depth selection\n", __func__);

				s->opt[OPT_BIT_DEPTH].cap &= ~SANE_CAP_INACTIVE;
				s->val[OPT_BIT_DEPTH].w = mode_params[optindex].depth;
			}
		}

		handle_depth_halftone(s, &reload);
		reload = SANE_TRUE;

		break;
	}

	case OPT_BIT_DEPTH:
		sval->w = *((SANE_Word *) value);
		mode_params[s->val[OPT_MODE].w].depth = sval->w;
		reload = SANE_TRUE;
		break;

	case OPT_HALFTONE:
		sval->w = optindex;
		handle_depth_halftone(s, &reload);
		break;

	case OPT_COLOR_CORRECTION:
	{
		sval->w = optindex;
		break;
	}

	case OPT_GAMMA_CORRECTION:
	{
		SANE_Bool f = gamma_userdefined[optindex];

		sval->w = optindex;

		setOptionState(s, f, OPT_GAMMA_VECTOR_R, &reload);
		setOptionState(s, f, OPT_GAMMA_VECTOR_G, &reload);
		setOptionState(s, f, OPT_GAMMA_VECTOR_B, &reload);
		setOptionState(s, !f, OPT_BRIGHTNESS, &reload);	/* Note... */

		break;
	}

	case OPT_MIRROR:
	case OPT_AAS:
	case OPT_PREVIEW:	/* needed? */
	case OPT_BRIGHTNESS:
	case OPT_SHARPNESS:
	case OPT_AUTO_EJECT:
	case OPT_THRESHOLD:
	case OPT_WAIT_FOR_BUTTON:
		sval->w = *((SANE_Word *) value);
		break;

	default:
		return SANE_STATUS_INVAL;
	}

	if (reload && info != NULL)
		*info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;

	DBG(17, "%s: end\n", __func__);

	return SANE_STATUS_GOOD;
}

SANE_Status
sane_control_option(SANE_Handle handle, SANE_Int option, SANE_Action action,
		    void *value, SANE_Int *info)
{
	DBG(17, "%s: action = %x, option = %d\n", __func__, action, option);

	if (option < 0 || option >= NUM_OPTIONS)
		return SANE_STATUS_INVAL;

	if (info != NULL)
		*info = 0;

	switch (action) {
	case SANE_ACTION_GET_VALUE:
		return getvalue(handle, option, value);

	case SANE_ACTION_SET_VALUE:
		return setvalue(handle, option, value, info);

	default:
		return SANE_STATUS_INVAL;
	}

	return SANE_STATUS_INVAL;
}

SANE_Status
sane_get_parameters(SANE_Handle handle, SANE_Parameters *params)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;

	DBG(5, "%s\n", __func__);

	if (params == NULL)
		DBG(1, "%s: params is NULL\n", __func__);

	/*
	 * If sane_start was already called, then just retrieve the parameters
	 * from the scanner data structure
	 */

	if (!s->eof && s->ptr != NULL) {
		DBG(5, "scan in progress, returning saved params structure\n");
	} else {
		/* otherwise initialize the params structure and gather the data */
		e2_init_parameters(s);
	}

	if (params != NULL)
		*params = s->params;

	print_params(s->params);

	return SANE_STATUS_GOOD;
}

static void e2_load_cct_profile(struct Epson_Scanner *s, unsigned int index)
{
        s->cct_table[0] = SANE_FIX(s->hw->cct_profile->cct[index][0]);
        s->cct_table[1] = SANE_FIX(s->hw->cct_profile->cct[index][1]);
        s->cct_table[2] = SANE_FIX(s->hw->cct_profile->cct[index][2]);
        s->cct_table[3] = SANE_FIX(s->hw->cct_profile->cct[index][3]);
        s->cct_table[4] = SANE_FIX(s->hw->cct_profile->cct[index][4]);
        s->cct_table[5] = SANE_FIX(s->hw->cct_profile->cct[index][5]);
        s->cct_table[6] = SANE_FIX(s->hw->cct_profile->cct[index][6]);
        s->cct_table[7] = SANE_FIX(s->hw->cct_profile->cct[index][7]);
        s->cct_table[8] = SANE_FIX(s->hw->cct_profile->cct[index][8]);
}

/*
 * This function is part of the SANE API and gets called from the front end to
 * start the scan process.
 */

SANE_Status
sane_start(SANE_Handle handle)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;
	Epson_Device *dev = s->hw;
	SANE_Status status;

	DBG(5, "* %s\n", __func__);

	s->eof = SANE_FALSE;
	s->canceling = SANE_FALSE;

	/* check if we just have finished working with the ADF */
	status = e2_check_adf(s);
	if (status != SANE_STATUS_GOOD)
		return status;

	/* calc scanning parameters */
	status = e2_init_parameters(s);
	if (status != SANE_STATUS_GOOD)
		return status;

	print_params(s->params);

	/* enable infrared */
	if (s->val[OPT_MODE].w == MODE_INFRARED)
		esci_enable_infrared(handle);

	/* ESC , bay */
	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_BAY].cap)) {
		status = esci_set_bay(s, s->val[OPT_BAY].w);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	/* set scanning parameters */
	if (dev->extended_commands)
		status = e2_set_extended_scanning_parameters(s);
	else
		status = e2_set_scanning_parameters(s);

	if (status != SANE_STATUS_GOOD)
		return status;

	/* ESC z, user defined gamma table */
	if (dev->cmd->set_gamma_table
	    && gamma_userdefined[s->val[OPT_GAMMA_CORRECTION].w]) {
		status = esci_set_gamma_table(s);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	
	if (s->val[OPT_COLOR_CORRECTION].w == CORR_AUTO) { /* Automatic */

		DBG(1, "using built in CCT profile\n");

		if (dev->model_id == 0)
			DBG(1, " specific profile not available, using default\n");


		if (0) { /* XXX TPU */
 
		        /* XXX check this */
			if (s->val[OPT_FILM_TYPE].w == 0)
				e2_load_cct_profile(s, CCTP_COLORPOS);
			else
				e2_load_cct_profile(s, CCTP_COLORNEG);

		} else {
			e2_load_cct_profile(s, CCTP_REFLECTIVE);
		}
	}
                                                    
	/* ESC m, user defined color correction */
	if (s->hw->cmd->set_color_correction_coefficients
		&& correction_userdefined[s->val[OPT_COLOR_CORRECTION].w]) {

		status = esci_set_color_correction_coefficients(s,
                                                        s->cct_table);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	/* check if we just have finished working with the ADF.
	 * this seems to work only after the scanner has been
	 * set up with scanning parameters
	 */
	status = e2_check_adf(s);
	if (status != SANE_STATUS_GOOD)
		return status;

	/*
	 * If WAIT_FOR_BUTTON is active, then do just that:
	 * Wait until the button is pressed. If the button was already
	 * pressed, then we will get the button pressed event right away.
	 */
	if (s->val[OPT_WAIT_FOR_BUTTON].w == SANE_TRUE)
		e2_wait_button(s);

	/* for debug, request command parameter */
/*	if (DBG_LEVEL) {
		unsigned char buf[45];
		request_command_parameter(s, buf);
	}
*/
	/* set the retry count to 0 */
	s->retry_count = 0;

	/* allocate buffers for color shuffling */
	if (dev->color_shuffle == SANE_TRUE) {
		int i;
		/* initialize the line buffers */
		for (i = 0; i < s->line_distance * 2 + 1; i++) {

			if (s->line_buffer[i] != NULL)
				free(s->line_buffer[i]);

			s->line_buffer[i] = malloc(s->params.bytes_per_line);
			if (s->line_buffer[i] == NULL) {
				DBG(1, "out of memory (line %d)\n", __LINE__);
				return SANE_STATUS_NO_MEM;
			}
		}
	}

	/* prepare buffer here so that a memory allocation failure
	 * will leave the scanner in a sane state.
	 * the buffer will have to hold the image data plus
	 * an error code in the extended handshaking mode.
	 */
	s->buf = realloc(s->buf, (s->lcount * s->params.bytes_per_line) + 1);
	if (s->buf == NULL)
		return SANE_STATUS_NO_MEM;

	s->ptr = s->end = s->buf;

	/* feed the first sheet in the ADF */
	if (dev->ADF && dev->use_extension && dev->cmd->feed) {
		status = esci_feed(s);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	/* this seems to work only for some devices */
	status = e2_wait_warm_up(s);
	if (status != SANE_STATUS_GOOD)
		return status;

	/* start scanning */
	DBG(1, "%s: scanning...\n", __func__);

	if (dev->extended_commands) {
		status = e2_start_ext_scan(s);

		/* sometimes the scanner gives an io error when
		 * it's warming up.
		 */
		if (status == SANE_STATUS_IO_ERROR) {
			status = e2_wait_warm_up(s);
			if (status == SANE_STATUS_GOOD)
				status = e2_start_ext_scan(s);
		}
	} else
		status = e2_start_std_scan(s);

	if (status != SANE_STATUS_GOOD) {
		DBG(1, "%s: start failed: %s\n", __func__,
		    sane_strstatus(status));

		return status;
	}

	/* this is a kind of read request */
	if (dev->connection == SANE_EPSON_NET) {
		sanei_epson_net_write(s, 0x2000, NULL, 0,
		      s->ext_block_len + 1, &status);
	}

	return status;
}

static inline int
get_color(int status)
{
	switch ((status >> 2) & 0x03) {
	case 1:
		return 1;
	case 2:
		return 0;
	case 3:
		return 2;
	default:
		return 0;	/* required to make the compiler happy */
	}
}

/* this moves data from our buffers to SANE */

SANE_Status
sane_read(SANE_Handle handle, SANE_Byte *data, SANE_Int max_length,
	  SANE_Int *length)
{
	SANE_Status status;
	Epson_Scanner *s = (Epson_Scanner *) handle;

	DBG(18, "* %s: eof: %d, canceling: %d\n",
		__func__, s->eof, s->canceling);

	/* sane_read called before sane_start? */
	if (s->buf == NULL) {
		DBG(1, "%s: buffer is NULL", __func__);
		return SANE_STATUS_INVAL;
	}

	*length = 0;

	if (s->hw->extended_commands)
		status = e2_ext_read(s);
	else
		status = e2_block_read(s);

	/* The scanning operation might be canceled by the scanner itself
	 * or the fronted program
	 */
	if (status == SANE_STATUS_CANCELLED || s->canceling) {
		e2_scan_finish(s);
		return SANE_STATUS_CANCELLED;
	}

	/* XXX if FS G and STATUS_IOERR, use e2_check_extended_status */

	DBG(18, "moving data %p %p, %d (%d lines)\n",
		s->ptr, s->end,
		max_length, max_length / s->params.bytes_per_line);

	e2_copy_image_data(s, data, max_length, length);

	DBG(18, "%d lines read, eof: %d, canceling: %d, status: %d\n",
		*length / s->params.bytes_per_line,
		s->canceling, s->eof, status);

	/* continue reading if appropriate */
	if (status == SANE_STATUS_GOOD)
		return status;

	e2_scan_finish(s);

	return status;
}

/*
 * void sane_cancel(SANE_Handle handle)
 *
 * Set the cancel flag to true. The next time the backend requests data
 * from the scanner the CAN message will be sent.
 */

void
sane_cancel(SANE_Handle handle)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;

	DBG(1, "* %s\n", __func__);

	s->canceling = SANE_TRUE;
}

/*
 * SANE_Status sane_set_io_mode()
 *
 * not supported - for asynchronous I/O
 */

SANE_Status
sane_set_io_mode(SANE_Handle __sane_unused__ handle,
	SANE_Bool __sane_unused__ non_blocking)
{
	return SANE_STATUS_UNSUPPORTED;
}

/*
 * SANE_Status sane_get_select_fd()
 *
 * not supported - for asynchronous I/O
 */

SANE_Status
sane_get_select_fd(SANE_Handle __sane_unused__ handle,
	SANE_Int __sane_unused__ *fd)
{
	return SANE_STATUS_UNSUPPORTED;
}
