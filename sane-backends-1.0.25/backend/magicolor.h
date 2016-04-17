/*
 * magicolor.h - SANE library for Magicolor scanners.
 *
 * (C) 2010 Reinhold Kainhofer <reinhold@kainhofer.com>
 *
 * Based on the epson2 sane backend:
 * Based on Kazuhiro Sasayama previous
 * Work on epson.[ch] file from the SANE package.
 * Please see those files for original copyrights.
 * Copyright (C) 2006 Tower Technologies
 * Author: Alessandro Zummo <a.zummo@towertech.it>
 *
 * This file is part of the SANE package.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2.
 */

#ifndef magicolor_h
#define magicolor_h

#undef BACKEND_NAME
#define BACKEND_NAME magicolor
#define DEBUG_NOT_STATIC

#include <sys/ioctl.h>

#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef NEED_SYS_TYPES_H
#include <sys/types.h>
#endif

#include <stdio.h>

#include "../include/sane/sane.h"
#include "../include/sane/sanei_debug.h"
#include "../include/sane/sanei_backend.h"

#ifdef __GNUC__
#define __func__ __FUNCTION__
#else
#define __func__ "(undef)"
/* I cast my vote for C99... :) */
#endif

/* Silence the compiler for unused arguments */
#define NOT_USED(x) ( (void)(x) )

#define MAGICOLOR_CONFIG_FILE "magicolor.conf"

#define NUM_OF_HEX_ELEMENTS (16)        /* number of hex numbers per line for data dump */
#define DEVICE_NAME_LEN (16)    /* length of device name in extended status */



/* misc constants */

#define NET	0x04
#define CMD	0x03


/* status values */
#define STATUS_READY		0x00	/* scanner is ready */
#define STATUS_ADF_JAM		0x01	/* ADF paper jam */
#define STATUS_OPEN		0x02	/* scanner is open */
#define STATUS_NOT_READY	0x03	/* scanner is in use on another interface */

#define ADF_LOADED		0x01	/* ADF is loaded */

#define MAGICOLOR_CAP_DEFAULT 0

#define MAGICOLOR_LEVEL_1690mf	0
#define	MAGICOLOR_LEVEL_DEFAULT MAGICOLOR_LEVEL_1690mf
#define	MAGICOLOR_LEVEL_NET     MAGICOLOR_LEVEL_1690mf

/* Structure holding the command set for a device */
struct MagicolorCmd
{
	const char *level;
	unsigned char scanner_cmd;
	unsigned char start_scanning;
	unsigned char request_error;
	unsigned char stop_scanning;
	unsigned char request_scan_parameters;
	unsigned char set_scan_parameters;
	unsigned char request_status;
	unsigned char request_data;
	unsigned char unknown1;
	unsigned char unknown2;

	unsigned char net_wrapper_cmd;
	unsigned char net_welcome;
	unsigned char net_lock;
	unsigned char net_lock_ack;
	unsigned char net_unlock;
};

/* Structure holding the device capabilities */
struct MagicolorCap
{
	unsigned int id;
	const char *cmds;
	const char *model;
	const char *OID;
	SANE_Int out_ep, in_ep;		/* USB bulk out/in endpoints */

	SANE_Int optical_res;		/* optical resolution */
	SANE_Range dpi_range;		/* max/min resolutions */

	SANE_Int *res_list;		/* list of resolutions */
	SANE_Int res_list_size;		/* number of entries in this list */

	SANE_Int maxDepth;		/* max. color depth */
	SANE_Word *depth_list;		/* list of color depths */

	SANE_Range brightness;		/* brightness range */

	SANE_Range fbf_x_range;		/* flattbed x range */
	SANE_Range fbf_y_range;		/* flattbed y range */

	SANE_Bool ADF;			/* ADF is installed */
	SANE_Bool adf_duplex;		/* does the ADF handle duplex scanning */
	SANE_Range adf_x_range;		/* autom. document feeder x range */
	SANE_Range adf_y_range;		/* autom. document feeder y range */
};

enum {
	OPT_NUM_OPTS = 0,
	OPT_MODE_GROUP,
	OPT_MODE,
	OPT_BIT_DEPTH,
	OPT_BRIGHTNESS,
	OPT_RESOLUTION,
	OPT_PREVIEW,
	OPT_SOURCE,
	OPT_ADF_MODE,
	OPT_GEOMETRY_GROUP,
	OPT_TL_X,
	OPT_TL_Y,
	OPT_BR_X,
	OPT_BR_Y,
	NUM_OPTIONS
};

typedef enum
{	/* hardware connection to the scanner */
	SANE_MAGICOLOR_NODEV,	/* default, no HW specified yet */
	SANE_MAGICOLOR_USB,	/* USB interface */
	SANE_MAGICOLOR_NET	/* network interface */
} Magicolor_Connection_Type;


/* Structure holding the hardware description */

struct Magicolor_Device
{
	struct Magicolor_Device *next;
	int missing;

	char *name;
	char *model;

	SANE_Device sane;

	SANE_Range *x_range;	/* x range w/out extension */
	SANE_Range *y_range;	/* y range w/out extension */

	Magicolor_Connection_Type connection;

	struct MagicolorCmd *cmd;
	struct MagicolorCap *cap;
};

typedef struct Magicolor_Device Magicolor_Device;

/* Structure holding an instance of a scanner (i.e. scanner has been opened) */
struct Magicolor_Scanner
{
	struct Magicolor_Scanner *next;
	struct Magicolor_Device *hw;

	int fd;

	SANE_Option_Descriptor opt[NUM_OPTIONS];
	Option_Value val[NUM_OPTIONS];
	SANE_Parameters params;

	SANE_Bool eof;
	SANE_Byte *buf, *end, *ptr;
	SANE_Bool canceling;

	SANE_Int left, top;
	SANE_Int width, height;

	/* image block data */
	SANE_Int data_len;
	SANE_Int block_len;
	SANE_Int last_len;
	SANE_Int blocks;
	SANE_Int counter;

	/* store how many bytes of the current pixel line we have already
	 * read in previous read attempts. Since each line will be padded
	 * to multiples of 512 bytes, this is needed to know which bytes
	 * to ignore */
	SANE_Int bytes_read_in_line;
	SANE_Byte *line_buffer;
	/* How many bytes are scanned per line (multiple of 512 bytes */
	SANE_Int scan_bytes_per_line;
};

typedef struct Magicolor_Scanner Magicolor_Scanner;

struct mode_param
{
	int flags;
	int colors;
	int depth;
};

enum {
	MODE_BINARY, MODE_GRAY, MODE_COLOR
};

#endif
