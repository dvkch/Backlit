/*
 * kodakaio.c - SANE library for Kodak ESP Aio scanners.
 *
 * Copyright (C)  2011-2013 Paul Newall
 *
 * Based on the Magicolor sane backend:
 * Based on the epson2 sane backend:
 * Based on Kazuhiro Sasayama previous
 * work on epson.[ch] file from the SANE package.
 * Please see those files for additional copyrights.
 * Author: Paul Newall
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2.

	29/12/12 added KodakAio_Scanner.ack
	2/1/13 added KodakAio_Scanner.background[]
 */

#ifndef kodakaio_h
#define kodakaio_h

#undef BACKEND_NAME
#define BACKEND_NAME kodakaio
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
#endif

/* Silence the compiler for unused arguments */
#define NOT_USED(x) ( (void)(x) )

#define KODAKAIO_CONFIG_FILE "kodakaio.conf"

#define NUM_OF_HEX_ELEMENTS (16)        /* number of hex numbers per line for data dump */
#define DEVICE_NAME_LEN (16)    /* length of device name in extended status */

#define CAP_DEFAULT 0

/* Structure holding the device capabilities */
struct KodakaioCap
{
	SANE_Word id;			/* USB pid */
	const char *cmds;		/* may be used for different command sets in future */
	const char *model;
	SANE_Int out_ep, in_ep;		/* USB bulk out/in endpoints */

	SANE_Int optical_res;		/* optical resolution */
	SANE_Range dpi_range;		/* max/min resolutions */

	SANE_Int *res_list;		/* list of resolutions */
	SANE_Int res_list_size;		/* number of entries in this list */

	SANE_Int maxDepth;		/* max. color depth */
	SANE_Word *depth_list;		/* list of color depths */

	 /* SANE_Range brightness;		brightness range */

	SANE_Range fbf_x_range;		/* flattbed x range */
	SANE_Range fbf_y_range;		/* flattbed y range */

	SANE_Bool ADF;			/* ADF is installed */
	SANE_Bool adf_duplex;		/* does the ADF handle duplex scanning */
	SANE_Range adf_x_range;		/* autom. document feeder x range */
	SANE_Range adf_y_range;		/* autom. document feeder y range */
};

/*
Options:OPT_BRIGHTNESS, used to be after BIT_DEPTH
*/
enum {
	OPT_NUM_OPTS = 0,
	OPT_MODE_GROUP,
	OPT_MODE,
	OPT_THRESHOLD,
	OPT_BIT_DEPTH,
	OPT_RESOLUTION,
	OPT_TRIALOPT, /* for debuggging */
	OPT_PREVIEW,
	OPT_SOURCE,
	OPT_ADF_MODE,
	OPT_PADDING,		/* Selects padding of adf pages to the specified length */
	OPT_GEOMETRY_GROUP,
	OPT_TL_X,
	OPT_TL_Y,
	OPT_BR_X,
	OPT_BR_Y,
	NUM_OPTIONS
};

typedef enum
{	/* hardware connection to the scanner */
	SANE_KODAKAIO_NODEV,	/* default, no HW specified yet */
	SANE_KODAKAIO_USB,	/* USB interface */
	SANE_KODAKAIO_NET	/* network interface */
} Kodakaio_Connection_Type;


/* Structure holding the hardware description */

struct Kodak_Device
{
	struct Kodak_Device *next;
	int missing;

	char *name;
	char *model;

	SANE_Device sane;

	SANE_Range *x_range;	/* x range w/out extension */
	SANE_Range *y_range;	/* y range w/out extension */

	Kodakaio_Connection_Type connection;

	struct KodakaioCap *cap;
};

typedef struct Kodak_Device Kodak_Device;

/* Structure holding an instance of a scanner (i.e. scanner has been opened) */
struct KodakAio_Scanner
{
	struct KodakAio_Scanner *next;
	struct Kodak_Device *hw;

	int fd;

	SANE_Option_Descriptor opt[NUM_OPTIONS];
	Option_Value val[NUM_OPTIONS];
	SANE_Parameters params;

	SANE_Bool ack; /* scanner has finished a page (happens early with adf and padding) */
	SANE_Bool eof; /* backend has finished a page (after padding with adf) */
	SANE_Byte *buf, *end, *ptr;
	SANE_Bool canceling;
	SANE_Bool scanning; /* scan in progress */
	SANE_Bool adf_loaded; /* paper in adf */
	SANE_Int background[3]; /* stores background RGB components for padding */

	SANE_Int left, top; /* in optres units? */
	SANE_Int width, height; /* in optres units? */
	/* SANE_Int threshold;  0..255 for lineart*/

	/* image block data */
	SANE_Int data_len;
	SANE_Int block_len;
	SANE_Int last_len; /* to be phased out */
	SANE_Int blocks;  /* to be phased out */
	SANE_Int counter;
	SANE_Int bytes_unread; /* to track when to stop */

	/* Used to store how many bytes of the current pixel line we have already
	 * read in previous read attempts. Since each line will be padded
	 * to multiples of 512 bytes, this is needed to know which bytes
	 * to ignore. NOT NEEDED FOR KODAKAIO */
	SANE_Int bytes_read_in_line;
	SANE_Byte *line_buffer;
	/* How many bytes are scanned per line */
	SANE_Int scan_bytes_per_line;
};

typedef struct KodakAio_Scanner KodakAio_Scanner;

struct mode_param
{
	int flags;
	int colors;
	int depth;
};

enum {
	MODE_COLOR, MODE_GRAY, MODE_LINEART
};

#endif
