/*
 * epsonds.c - Epson ESC/I-2 driver.
 *
 * Copyright (C) 2015 Tower Technologies
 * Author: Alessandro Zummo <a.zummo@towertech.it>
 *
 * This file is part of the SANE package.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2.
 */

#ifndef epsonds_h
#define epsonds_h

#undef BACKEND_NAME
#define BACKEND_NAME epsonds
#define DEBUG_NOT_STATIC

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef NEED_SYS_TYPES_H
#include <sys/types.h>
#endif

#include <string.h> /* for memset and memcpy */
#include <stdio.h>

#include "sane/sane.h"
#include "sane/sanei_backend.h"
#include "sane/sanei_debug.h"
#include "sane/sanei_usb.h"
#include "sane/sanei_jpeg.h"

#ifdef __GNUC__
#define __func__ __FUNCTION__
#else
#define __func__ "(undef)"
/* I cast my vote for C99... :) */
#endif

#define EPSONDS_CONFIG_FILE "epsonds.conf"

#ifndef PATH_MAX
#define PATH_MAX (1024)
#endif

#ifndef XtNumber
#define XtNumber(x)  (sizeof(x) / sizeof(x[0]))
#define XtOffset(p_type, field)  ((size_t)&(((p_type)NULL)->field))
#define XtOffsetOf(s_type, field)  XtOffset(s_type*, field)
#endif

#define ACK	0x06
#define NAK	0x15
#define	FS	0x1C

#define FBF_STR SANE_I18N("Flatbed")
#define TPU_STR SANE_I18N("Transparency Unit")
#define ADF_STR SANE_I18N("Automatic Document Feeder")

enum {
	OPT_NUM_OPTS = 0,
	OPT_MODE_GROUP,
	OPT_MODE,
	OPT_DEPTH,
	OPT_RESOLUTION,
	OPT_GEOMETRY_GROUP,
	OPT_TL_X,
	OPT_TL_Y,
	OPT_BR_X,
	OPT_BR_Y,
	OPT_EQU_GROUP,
	OPT_SOURCE,
	OPT_EJECT,
	OPT_LOAD,
	OPT_ADF_MODE,
	OPT_ADF_SKEW,
	NUM_OPTIONS
};

typedef enum
{	/* hardware connection to the scanner */
	SANE_EPSONDS_NODEV,	/* default, no HW specified yet */
	SANE_EPSONDS_USB,	/* USB interface */
	SANE_EPSONDS_NET	/* network interface (unsupported)*/
} epsonds_conn_type;

/* hardware description */

struct epsonds_device
{
	struct epsonds_device *next;

	epsonds_conn_type connection;

	char *name;
	char *model;

	unsigned int model_id;

	SANE_Device sane;
	SANE_Range *x_range;
	SANE_Range *y_range;
	SANE_Range dpi_range;
	SANE_Byte alignment;


	SANE_Int *res_list;		/* list of resolutions */
	SANE_Int *depth_list;
	SANE_Int max_depth;		/* max. color depth */

	SANE_Bool has_raw;		/* supports RAW format */

	SANE_Bool has_fb;		/* flatbed */
	SANE_Range fbf_x_range;	        /* x range */
	SANE_Range fbf_y_range;	        /* y range */
	SANE_Byte fbf_alignment;	/* left, center, right */
	SANE_Bool fbf_has_skew;		/* supports skew correction */

	SANE_Bool has_adf;		/* adf */
	SANE_Range adf_x_range;	        /* x range */
	SANE_Range adf_y_range;	        /* y range */
	SANE_Bool adf_is_duplex;	/* supports duplex mode */
	SANE_Bool adf_singlepass;	/* supports single pass duplex */
	SANE_Bool adf_has_skew;		/* supports skew correction */
	SANE_Bool adf_has_load;		/* supports load command */
	SANE_Bool adf_has_eject;	/* supports eject command */
	SANE_Byte adf_alignment;	/* left, center, right */
	SANE_Byte adf_has_dfd;		/* supports double feed detection */

	SANE_Bool has_tpu;		/* tpu */
	SANE_Range tpu_x_range;	        /* transparency unit x range */
	SANE_Range tpu_y_range;	        /* transparency unit y range */
};

typedef struct epsonds_device epsonds_device;

typedef struct ring_buffer
{
	SANE_Byte *ring, *wp, *rp, *end;
	SANE_Int fill, size;

} ring_buffer;

/* an instance of a scanner */

struct epsonds_scanner
{
	struct epsonds_scanner *next;
	struct epsonds_device *hw;

	int fd;

	SANE_Option_Descriptor opt[NUM_OPTIONS];
	Option_Value val[NUM_OPTIONS];
	SANE_Parameters params;

	SANE_Byte *buf, *line_buffer;
	ring_buffer *current, front, back;

	SANE_Bool eof, scanning, canceling, locked, backside, mode_jpeg;

	SANE_Int left, top, pages, dummy;

	/* jpeg stuff */

	djpeg_dest_ptr jdst;
	struct jpeg_decompress_struct jpeg_cinfo;
	struct jpeg_error_mgr jpeg_err;
	SANE_Bool jpeg_header_seen;
};

typedef struct epsonds_scanner epsonds_scanner;

struct mode_param
{
	int color;
	int flags;
	int dropout_mask;
	int depth;
};

enum {
	MODE_BINARY, MODE_GRAY, MODE_COLOR
};

#endif
