/*
 * SANE - Scanner Access Now Easy.
 * coolscan3.c
 *
 * This file implements a SANE backend for Nikon Coolscan film scanners.
 *
 * coolscan3.c is based on coolscan2.c, a work of András Major, Ariel Garcia
 * and Giuseppe Sacco.
 *
 * Copyright (C) 2007-08 Tower Technologies
 * Author: Alessandro Zummo <a.zummo@towertech.it>
 *
 * This file is part of the SANE package.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2.
 *
 */

/* ========================================================================= */

#include "../include/sane/config.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>

#include "../include/_stdint.h"

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_scsi.h"
#include "../include/sane/sanei_usb.h"
#include "../include/sane/sanei_debug.h"
#include "../include/sane/sanei_config.h"

#define BACKEND_NAME coolscan3
#include "../include/sane/sanei_backend.h"	/* must be last */

#define CS3_VERSION_MAJOR 1
#define CS3_VERSION_MINOR 0
#define CS3_REVISION 0
#define CS3_CONFIG_FILE "coolscan3.conf"

#define WSIZE (sizeof (SANE_Word))


/* ========================================================================= */
/* typedefs */

typedef enum
{
	CS3_TYPE_UNKOWN,
	CS3_TYPE_LS30,
	CS3_TYPE_LS40,
	CS3_TYPE_LS50,
	CS3_TYPE_LS2000,
	CS3_TYPE_LS4000,
	CS3_TYPE_LS5000,
	CS3_TYPE_LS8000
}
cs3_type_t;

typedef enum
{
	CS3_INTERFACE_UNKNOWN,
	CS3_INTERFACE_SCSI,	/* includes IEEE1394 via SBP2 */
	CS3_INTERFACE_USB
}
cs3_interface_t;

typedef enum
{
	CS3_PHASE_NONE = 0x00,
	CS3_PHASE_STATUS = 0x01,
	CS3_PHASE_OUT = 0x02,
	CS3_PHASE_IN = 0x03,
	CS3_PHASE_BUSY = 0x04
}
cs3_phase_t;

typedef enum
{
	CS3_SCAN_NORMAL,
	CS3_SCAN_AE,
	CS3_SCAN_AE_WB
}
cs3_scan_t;

typedef enum
{
	CS3_STATUS_READY = 0,
	CS3_STATUS_BUSY = 1,
	CS3_STATUS_NO_DOCS = 2,
	CS3_STATUS_PROCESSING = 4,
	CS3_STATUS_ERROR = 8,
	CS3_STATUS_REISSUE = 16,
	CS3_STATUS_ALL = 31	/* sum of all others */
}
cs3_status_t;

typedef enum
{
	CS3_OPTION_NUM = 0,

	CS3_OPTION_PREVIEW,

	CS3_OPTION_NEGATIVE,

	CS3_OPTION_INFRARED,

	CS3_OPTION_SAMPLES_PER_SCAN,

	CS3_OPTION_DEPTH,

	CS3_OPTION_EXPOSURE,
	CS3_OPTION_EXPOSURE_R,
	CS3_OPTION_EXPOSURE_G,
	CS3_OPTION_EXPOSURE_B,
	CS3_OPTION_SCAN_AE,
	CS3_OPTION_SCAN_AE_WB,

	CS3_OPTION_LUT_R,
	CS3_OPTION_LUT_G,
	CS3_OPTION_LUT_B,

	CS3_OPTION_RES,
	CS3_OPTION_RESX,
	CS3_OPTION_RESY,
	CS3_OPTION_RES_INDEPENDENT,

	CS3_OPTION_PREVIEW_RESOLUTION,

	CS3_OPTION_FRAME,
	CS3_OPTION_FRAME_COUNT,
	CS3_OPTION_SUBFRAME,
	CS3_OPTION_XMIN,
	CS3_OPTION_XMAX,
	CS3_OPTION_YMIN,
	CS3_OPTION_YMAX,

	CS3_OPTION_LOAD,
	CS3_OPTION_AUTOLOAD,
	CS3_OPTION_EJECT,
	CS3_OPTION_RESET,

	CS3_OPTION_FOCUS_ON_CENTRE,
	CS3_OPTION_FOCUS,
	CS3_OPTION_AUTOFOCUS,
	CS3_OPTION_FOCUSX,
	CS3_OPTION_FOCUSY,

	CS3_N_OPTIONS		/* must be last -- counts number of enum items */
}
cs3_option_t;

typedef unsigned int cs3_pixel_t;

#define CS3_COLOR_MAX 10	/* 9 + 1, see cs3_colors */

/* Given that there is no way to give scanner vendor
 * and model to the calling software, I have to use
 * an ugly hack here. :( That's very sad. Suggestions
 * that can provide the same features are appreciated.
 */

#ifndef SANE_COOKIE
#define SANE_COOKIE 0x0BADCAFE

struct SANE_Cookie
{
	uint16_t version;
	const char *vendor;
	const char *model;
	const char *revision;
};
#endif

typedef struct
{
	/* magic bits :( */
	uint32_t magic;
	struct SANE_Cookie *cookie_ptr;
	struct SANE_Cookie cookie;

	/* interface */
	cs3_interface_t interface;
	int fd;
	SANE_Byte *send_buf, *recv_buf;
	size_t send_buf_size, recv_buf_size;
	size_t n_cmd, n_send, n_recv;

	/* device characteristics */
	char vendor_string[9], product_string[17], revision_string[5];
	cs3_type_t type;
	int maxbits;
	unsigned int resx_optical, resx_min, resx_max, *resx_list,
		resx_n_list;
	unsigned int resy_optical, resy_min, resy_max, *resy_list,
		resy_n_list;
	unsigned long boundaryx, boundaryy;
	unsigned long frame_offset;
	unsigned int unit_dpi;
	double unit_mm;
	int n_frames;

	int focus_min, focus_max;

	/* settings */
	SANE_Bool preview, negative, infrared, autoload, autofocus, ae, aewb;
	int samples_per_scan, depth, real_depth, bytes_per_pixel, shift_bits,
		n_colors;
	cs3_pixel_t n_lut;
	cs3_pixel_t *lut_r, *lut_g, *lut_b, *lut_neutral;
	unsigned long resx, resy, res, res_independent, res_preview;
	unsigned long xmin, xmax, ymin, ymax;
	int i_frame, frame_count;
	double subframe;

	unsigned int real_resx, real_resy, real_pitchx, real_pitchy;
	unsigned long real_xoffset, real_yoffset, real_width, real_height,
		logical_width, logical_height;
	int odd_padding;
	int block_padding;

	double exposure, exposure_r, exposure_g, exposure_b;
	unsigned long real_exposure[CS3_COLOR_MAX];


	SANE_Bool focus_on_centre;
	unsigned long focusx, focusy, real_focusx, real_focusy;
	int focus;

	/* status */
	SANE_Bool scanning;
	SANE_Byte *line_buf;
	ssize_t n_line_buf, i_line_buf;
	unsigned long sense_key, sense_asc, sense_ascq, sense_info;
	unsigned long sense_code;
	cs3_status_t status;
	size_t xfer_position, xfer_bytes_total;

	/* SANE stuff */
	SANE_Option_Descriptor option_list[CS3_N_OPTIONS];
}
cs3_t;


/* ========================================================================= */
/* prototypes */

static SANE_Status cs3_open(const char *device, cs3_interface_t interface,
			    cs3_t ** sp);
static void cs3_close(cs3_t * s);
static SANE_Status cs3_attach(const char *dev);
static SANE_Status cs3_scsi_sense_handler(int fd, u_char * sense_buffer,
					  void *arg);
static SANE_Status cs3_parse_sense_data(cs3_t * s);
static void cs3_init_buffer(cs3_t * s);
static SANE_Status cs3_pack_byte(cs3_t * s, SANE_Byte byte);
static void cs3_pack_long(cs3_t * s, unsigned long val);
static void cs3_pack_word(cs3_t * s, unsigned long val);
static SANE_Status cs3_parse_cmd(cs3_t * s, char *text);
static SANE_Status cs3_grow_send_buffer(cs3_t * s);
static SANE_Status cs3_issue_cmd(cs3_t * s);
static cs3_phase_t cs3_phase_check(cs3_t * s);
static SANE_Status cs3_set_boundary(cs3_t * s);
static SANE_Status cs3_scanner_ready(cs3_t * s, int flags);
static SANE_Status cs3_page_inquiry(cs3_t * s, int page);
static SANE_Status cs3_full_inquiry(cs3_t * s);
static SANE_Status cs3_mode_select(cs3_t * s);
static SANE_Status cs3_reserve_unit(cs3_t * s);
static SANE_Status cs3_release_unit(cs3_t * s);
static SANE_Status cs3_execute(cs3_t * s);
static SANE_Status cs3_load(cs3_t * s);
static SANE_Status cs3_eject(cs3_t * s);
static SANE_Status cs3_reset(cs3_t * s);
static SANE_Status cs3_set_focus(cs3_t * s);
static SANE_Status cs3_autofocus(cs3_t * s);
static SANE_Status cs3_autoexposure(cs3_t * s, int wb);
static SANE_Status cs3_get_exposure(cs3_t * s);
static SANE_Status cs3_set_window(cs3_t * s, cs3_scan_t type);
static SANE_Status cs3_convert_options(cs3_t * s);
static SANE_Status cs3_scan(cs3_t * s, cs3_scan_t type);
static void *cs3_xmalloc(size_t size);
static void *cs3_xrealloc(void *p, size_t size);
static void cs3_xfree(const void *p);


/* ========================================================================= */
/* global variables */

static int cs3_colors[] = { 1, 2, 3, 9 };

static SANE_Device **device_list = NULL;
static int n_device_list = 0;
static cs3_interface_t try_interface = CS3_INTERFACE_UNKNOWN;
static int open_devices = 0;


/* ========================================================================= */
/* SANE entry points */

SANE_Status
sane_init(SANE_Int * version_code, SANE_Auth_Callback authorize)
{
	DBG_INIT();
	DBG(1, "coolscan3 backend, version %i.%i.%i initializing.\n",
	    CS3_VERSION_MAJOR, CS3_VERSION_MINOR, CS3_REVISION);

	authorize = authorize;	/* to shut up compiler */

	if (version_code)
		*version_code = SANE_VERSION_CODE(SANE_CURRENT_MAJOR, V_MINOR, 0);

	sanei_usb_init();

	return SANE_STATUS_GOOD;
}

void
sane_exit(void)
{
	int i;

	DBG(10, "%s\n", __func__);

	for (i = 0; i < n_device_list; i++) {
		cs3_xfree(device_list[i]->name);
		cs3_xfree(device_list[i]->vendor);
		cs3_xfree(device_list[i]->model);
		cs3_xfree(device_list[i]);
	}
	cs3_xfree(device_list);
}

SANE_Status
sane_get_devices(const SANE_Device *** list, SANE_Bool local_only)
{
	char line[PATH_MAX], *p;
	FILE *config;

	local_only = local_only;	/* to shut up compiler */

	DBG(10, "%s\n", __func__);

	if (device_list)
		DBG(6,
		    "sane_get_devices(): Device list already populated, not probing again.\n");
	else {
		if (open_devices) {
			DBG(4,
			    "sane_get_devices(): Devices open, not scanning for scanners.\n");
			return SANE_STATUS_IO_ERROR;
		}

		config = sanei_config_open(CS3_CONFIG_FILE);
		if (config) {
			DBG(4, "sane_get_devices(): Reading config file.\n");
			while (sanei_config_read(line, sizeof(line), config)) {
				p = line;
				p += strspn(line, " \t");
				if (strlen(p) && (p[0] != '\n')
				    && (p[0] != '#'))
					cs3_open(line, CS3_INTERFACE_UNKNOWN,
						 NULL);
			}
			fclose(config);
		} else {
			DBG(4, "sane_get_devices(): No config file found.\n");
			cs3_open("auto", CS3_INTERFACE_UNKNOWN, NULL);
		}

		DBG(6, "%s: %i device(s) detected.\n",
		    __func__, n_device_list);
	}

	*list = (const SANE_Device **) device_list;

	return SANE_STATUS_GOOD;
}

SANE_Status
sane_open(SANE_String_Const name, SANE_Handle * h)
{
	SANE_Status status;
	cs3_t *s;
	int i_option;
	unsigned int i_list;
	SANE_Option_Descriptor o;
	SANE_Word *word_list;
	SANE_Range *range = NULL;
	int alloc_failed = 0;

	DBG(10, "%s\n", __func__);

	status = cs3_open(name, CS3_INTERFACE_UNKNOWN, &s);
	if (status != SANE_STATUS_GOOD)
		return status;

	*h = (SANE_Handle) s;

	/* get device properties */

	s->lut_r = s->lut_g = s->lut_b = s->lut_neutral = NULL;
	s->resx_list = s->resy_list = NULL;
	s->resx_n_list = s->resy_n_list = 0;

	status = cs3_full_inquiry(s);
	if (status != SANE_STATUS_GOOD)
		return status;

	status = cs3_mode_select(s);
	if (status != SANE_STATUS_GOOD)
		return status;

	/* option descriptors */

	for (i_option = 0; i_option < CS3_N_OPTIONS; i_option++) {
		o.name = o.title = o.desc = NULL;
		o.type = o.unit = o.cap = o.constraint_type = o.size = 0;
		o.constraint.range = NULL;	/* only one union member needs to be NULLed */
		switch (i_option) {
		case CS3_OPTION_NUM:
			o.name = "";
			o.title = SANE_TITLE_NUM_OPTIONS;
			o.desc = SANE_DESC_NUM_OPTIONS;
			o.type = SANE_TYPE_INT;
			o.size = WSIZE;
			o.cap = SANE_CAP_SOFT_DETECT;
			break;
		case CS3_OPTION_PREVIEW:
			o.name = "preview";
			o.title = "Preview mode";
			o.desc = "Preview mode";
			o.type = SANE_TYPE_BOOL;
			o.size = WSIZE;
			o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT |
				SANE_CAP_ADVANCED;
			break;
		case CS3_OPTION_NEGATIVE:
			o.name = "negative";
			o.title = "Negative";
			o.desc = "Negative film: make scanner invert colors";
			o.type = SANE_TYPE_BOOL;
			o.size = WSIZE;
			o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
			/*o.cap |= SANE_CAP_INACTIVE; */
			break;

		case CS3_OPTION_INFRARED:
			o.name = "infrared";
			o.title = "Read infrared channel";
			o.desc = "Read infrared channel in addition to scan colors";
			o.type = SANE_TYPE_BOOL;
			o.size = WSIZE;
			o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
#ifndef SANE_FRAME_RGBI
                        o.cap |= SANE_CAP_INACTIVE;
#endif
			break;

		case CS3_OPTION_SAMPLES_PER_SCAN:
			o.name = "samples-per-scan";
			o.title = "Samples per Scan";
			o.desc = "Number of samples per scan";
			o.type = SANE_TYPE_INT;
			o.unit = SANE_UNIT_NONE;
			o.size = WSIZE;
			o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
			if (s->type != CS3_TYPE_LS2000 && s->type != CS3_TYPE_LS4000
					&& s->type != CS3_TYPE_LS5000 && s->type != CS3_TYPE_LS8000)
				o.cap |= SANE_CAP_INACTIVE;
			o.constraint_type = SANE_CONSTRAINT_RANGE;
			range = (SANE_Range *) cs3_xmalloc (sizeof (SANE_Range));
			if (! range)
				  alloc_failed = 1;
			else
				  {
					range->min = 1;
					range->max = 16;
					range->quant = 1;
					o.constraint.range = range;
				  }
			break;

		case CS3_OPTION_DEPTH:
			o.name = "depth";
			o.title = "Bit depth per channel";
			o.desc = "Number of bits output by scanner for each channel";
			o.type = SANE_TYPE_INT;
			o.unit = SANE_UNIT_NONE;
			o.size = WSIZE;
			o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
			o.constraint_type = SANE_CONSTRAINT_WORD_LIST;
			word_list =
				(SANE_Word *) cs3_xmalloc(2 *
							  sizeof(SANE_Word));
			if (!word_list)
				alloc_failed = 1;
			else {
				word_list[1] = 8;
				word_list[2] = s->maxbits;
				word_list[0] = 2;
				o.constraint.word_list = word_list;
			}
			break;
		case CS3_OPTION_EXPOSURE:
			o.name = "exposure";
			o.title = "Exposure multiplier";
			o.desc = "Exposure multiplier for all channels";
			o.type = SANE_TYPE_FIXED;
			o.unit = SANE_UNIT_NONE;
			o.size = WSIZE;
			o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
			o.constraint_type = SANE_CONSTRAINT_RANGE;
			range = (SANE_Range *)
				cs3_xmalloc(sizeof(SANE_Range));
			if (!range)
				alloc_failed = 1;
			else {
				range->min = SANE_FIX(0.);
				range->max = SANE_FIX(10.);
				range->quant = SANE_FIX(0.1);
				o.constraint.range = range;
			}
			break;
		case CS3_OPTION_EXPOSURE_R:
			o.name = "red-exposure";
			o.title = "Red exposure time";
			o.desc = "Exposure time for red channel";
			o.type = SANE_TYPE_FIXED;
			o.unit = SANE_UNIT_MICROSECOND;
			o.size = WSIZE;
			o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
			o.constraint_type = SANE_CONSTRAINT_RANGE;
			range = (SANE_Range *)
				cs3_xmalloc(sizeof(SANE_Range));
			if (!range)
				alloc_failed = 1;
			else {
				range->min = SANE_FIX(50.);
				range->max = SANE_FIX(20000.);
				range->quant = SANE_FIX(10.);
				o.constraint.range = range;
			}
			break;
		case CS3_OPTION_EXPOSURE_G:
			o.name = "green-exposure";
			o.title = "Green exposure time";
			o.desc = "Exposure time for green channel";
			o.type = SANE_TYPE_FIXED;
			o.unit = SANE_UNIT_MICROSECOND;
			o.size = WSIZE;
			o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
			o.constraint_type = SANE_CONSTRAINT_RANGE;
			range = (SANE_Range *)
				cs3_xmalloc(sizeof(SANE_Range));
			if (!range)
				alloc_failed = 1;
			else {
				range->min = SANE_FIX(50.);
				range->max = SANE_FIX(20000.);
				range->quant = SANE_FIX(10.);
				o.constraint.range = range;
			}
			break;
		case CS3_OPTION_EXPOSURE_B:
			o.name = "blue-exposure";
			o.title = "Blue exposure time";
			o.desc = "Exposure time for blue channel";
			o.type = SANE_TYPE_FIXED;
			o.unit = SANE_UNIT_MICROSECOND;
			o.size = WSIZE;
			o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
			o.constraint_type = SANE_CONSTRAINT_RANGE;
			range = (SANE_Range *)
				cs3_xmalloc(sizeof(SANE_Range));
			if (!range)
				alloc_failed = 1;
			else {
				range->min = SANE_FIX(50.);
				range->max = SANE_FIX(20000.);
				range->quant = SANE_FIX(10.);
				o.constraint.range = range;
			}
			break;
		case CS3_OPTION_LUT_R:
			o.name = "red-gamma-table";
			o.title = "LUT for red channel";
			o.desc = "LUT for red channel";
			o.type = SANE_TYPE_INT;
			o.size = s->n_lut * WSIZE;
			o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
			o.constraint_type = SANE_CONSTRAINT_RANGE;
			range = (SANE_Range *)
				cs3_xmalloc(sizeof(SANE_Range));
			if (!range)
				alloc_failed = 1;
			else {
				range->min = 0;
				range->max = s->n_lut - 1;
				range->quant = 1;
				o.constraint.range = range;
			}
			break;
		case CS3_OPTION_LUT_G:
			o.name = "green-gamma-table";
			o.title = "LUT for green channel";
			o.desc = "LUT for green channel";
			o.type = SANE_TYPE_INT;
			o.size = s->n_lut * WSIZE;
			o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
			o.constraint_type = SANE_CONSTRAINT_RANGE;
			range = (SANE_Range *)
				cs3_xmalloc(sizeof(SANE_Range));
			if (!range)
				alloc_failed = 1;
			else {
				range->min = 0;
				range->max = s->n_lut - 1;
				range->quant = 1;
				o.constraint.range = range;
			}
			break;
		case CS3_OPTION_LUT_B:
			o.name = "blue-gamma-table";
			o.title = "LUT for blue channel";
			o.desc = "LUT for blue channel";
			o.type = SANE_TYPE_INT;
			o.size = s->n_lut * WSIZE;
			o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
			o.constraint_type = SANE_CONSTRAINT_RANGE;
			range = (SANE_Range *)
				cs3_xmalloc(sizeof(SANE_Range));
			if (!range)
				alloc_failed = 1;
			else {
				range->min = 0;
				range->max = s->n_lut - 1;
				range->quant = 1;
				o.constraint.range = range;
			}
			break;
		case CS3_OPTION_LOAD:
			o.name = "load";
			o.title = "Load";
			o.desc = "Load next slide";
			o.type = SANE_TYPE_BUTTON;
			o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
			if (s->n_frames > 1)
				o.cap |= SANE_CAP_INACTIVE;
			break;
		case CS3_OPTION_AUTOLOAD:
			o.name = "autoload";
			o.title = "Autoload";
			o.desc = "Autoload slide before each scan";
			o.type = SANE_TYPE_BOOL;
			o.size = WSIZE;
			o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
			if (s->n_frames > 1)
				o.cap |= SANE_CAP_INACTIVE;
			break;
		case CS3_OPTION_EJECT:
			o.name = "eject";
			o.title = "Eject";
			o.desc = "Eject loaded medium";
			o.type = SANE_TYPE_BUTTON;
			o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
			break;
		case CS3_OPTION_RESET:
			o.name = "reset";
			o.title = "Reset scanner";
			o.desc = "Initialize scanner";
			o.type = SANE_TYPE_BUTTON;
			o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
			break;
		case CS3_OPTION_RESX:
		case CS3_OPTION_RES:
		case CS3_OPTION_PREVIEW_RESOLUTION:
			if (i_option == CS3_OPTION_PREVIEW_RESOLUTION) {
				o.name = "preview-resolution";
				o.title = "Preview resolution";
				o.desc = "Scanning resolution for preview mode in dpi, affecting both x and y directions";
			} else if (i_option == CS3_OPTION_RES) {
				o.name = "resolution";
				o.title = "Resolution";
				o.desc = "Scanning resolution in dpi, affecting both x and y directions";
			} else {
				o.name = "x-resolution";
				o.title = "X resolution";
				o.desc = "Scanning resolution in dpi, affecting x direction only";
			}
			o.type = SANE_TYPE_INT;
			o.unit = SANE_UNIT_DPI;
			o.size = WSIZE;
			o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
			if (i_option == CS3_OPTION_RESX)
				o.cap |= SANE_CAP_INACTIVE |
					SANE_CAP_ADVANCED;
			if (i_option == CS3_OPTION_PREVIEW_RESOLUTION)
				o.cap |= SANE_CAP_ADVANCED;
			o.constraint_type = SANE_CONSTRAINT_WORD_LIST;
			word_list =
				(SANE_Word *) cs3_xmalloc((s->resx_n_list + 1)
							  *
							  sizeof(SANE_Word));
			if (!word_list)
				alloc_failed = 1;
			else {
				for (i_list = 0; i_list < s->resx_n_list;
				     i_list++)
					word_list[i_list + 1] =
						s->resx_list[i_list];
				word_list[0] = s->resx_n_list;
				o.constraint.word_list = word_list;
			}
			break;
		case CS3_OPTION_RESY:
			o.name = "y-resolution";
			o.title = "Y resolution";
			o.desc = "Scanning resolution in dpi, affecting y direction only";
			o.type = SANE_TYPE_INT;
			o.unit = SANE_UNIT_DPI;
			o.size = WSIZE;
			o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT |
				SANE_CAP_INACTIVE | SANE_CAP_ADVANCED;
			o.constraint_type = SANE_CONSTRAINT_WORD_LIST;
			word_list =
				(SANE_Word *) cs3_xmalloc((s->resy_n_list + 1)
							  *
							  sizeof(SANE_Word));
			if (!word_list)
				alloc_failed = 1;
			else {
				for (i_list = 0; i_list < s->resy_n_list;
				     i_list++)
					word_list[i_list + 1] =
						s->resy_list[i_list];
				word_list[0] = s->resy_n_list;
				o.constraint.word_list = word_list;
			}
			break;
		case CS3_OPTION_RES_INDEPENDENT:
			o.name = "independent-res";
			o.title = "Independent x/y resolutions";
			o.desc = "Enable independent controls for scanning resolution in x and y direction";
			o.type = SANE_TYPE_BOOL;
			o.size = WSIZE;
			o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT |
				SANE_CAP_INACTIVE | SANE_CAP_ADVANCED;
			break;
		case CS3_OPTION_FRAME:
			o.name = "frame";
			o.title = "Frame number";
			o.desc = "Number of frame to be scanned, starting with 1";
			o.type = SANE_TYPE_INT;
			o.unit = SANE_UNIT_NONE;
			o.size = WSIZE;
			o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
			if (s->n_frames <= 1)
				o.cap |= SANE_CAP_INACTIVE;
			o.constraint_type = SANE_CONSTRAINT_RANGE;
			range = (SANE_Range *)
				cs3_xmalloc(sizeof(SANE_Range));
			if (!range)
				alloc_failed = 1;
			else {
				range->min = 1;
				range->max = s->n_frames;
				range->quant = 1;
				o.constraint.range = range;
			}
			break;
		case CS3_OPTION_FRAME_COUNT:
			o.name = "frame-count";
			o.title = "Frame count";
			o.desc = "Amount of frames to scan";
			o.type = SANE_TYPE_INT;
			o.unit = SANE_UNIT_NONE;
			o.size = WSIZE;
			o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
			if (s->n_frames <= 1)
				o.cap |= SANE_CAP_INACTIVE;
			o.constraint_type = SANE_CONSTRAINT_RANGE;
			range = (SANE_Range *)
				cs3_xmalloc(sizeof(SANE_Range));
			if (!range)
				alloc_failed = 1;
			else {
				range->min = 1;
				range->max = s->n_frames - s->i_frame + 1;
				range->quant = 1;
				o.constraint.range = range;
			}
			break;
		case CS3_OPTION_SUBFRAME:
			o.name = "subframe";
			o.title = "Frame shift";
			o.desc = "Fine position within the selected frame";
			o.type = SANE_TYPE_FIXED;
			o.unit = SANE_UNIT_MM;
			o.size = WSIZE;
			o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
			o.constraint_type = SANE_CONSTRAINT_RANGE;
			range = (SANE_Range *)
				cs3_xmalloc(sizeof(SANE_Range));
			if (!range)
				alloc_failed = 1;
			else {
				range->min = SANE_FIX(0.);
				range->max =
					SANE_FIX((s->boundaryy -
						  1) * s->unit_mm);
				range->quant = SANE_FIX(0.);
				o.constraint.range = range;
			}
			break;
		case CS3_OPTION_XMIN:
			o.name = "tl-x";
			o.title = "Left x value of scan area";
			o.desc = "Left x value of scan area";
			o.type = SANE_TYPE_INT;
			o.unit = SANE_UNIT_PIXEL;
			o.size = WSIZE;
			o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
			o.constraint_type = SANE_CONSTRAINT_RANGE;
			if (!range)
				alloc_failed = 1;
			else {
				range = (SANE_Range *)
					cs3_xmalloc(sizeof(SANE_Range));
				range->min = 0;
				range->max = s->boundaryx - 1;
				range->quant = 1;
				o.constraint.range = range;
			}
			break;
		case CS3_OPTION_XMAX:
			o.name = "br-x";
			o.title = "Right x value of scan area";
			o.desc = "Right x value of scan area";
			o.type = SANE_TYPE_INT;
			o.unit = SANE_UNIT_PIXEL;
			o.size = WSIZE;
			o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
			o.constraint_type = SANE_CONSTRAINT_RANGE;
			range = (SANE_Range *)
				cs3_xmalloc(sizeof(SANE_Range));
			if (!range)
				alloc_failed = 1;
			else {
				range->min = 0;
				range->max = s->boundaryx - 1;
				range->quant = 1;
				o.constraint.range = range;
			}
			break;
		case CS3_OPTION_YMIN:
			o.name = "tl-y";
			o.title = "Top y value of scan area";
			o.desc = "Top y value of scan area";
			o.type = SANE_TYPE_INT;
			o.unit = SANE_UNIT_PIXEL;
			o.size = WSIZE;
			o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
			o.constraint_type = SANE_CONSTRAINT_RANGE;
			range = (SANE_Range *)
				cs3_xmalloc(sizeof(SANE_Range));
			if (!range)
				alloc_failed = 1;
			else {
				range->min = 0;
				range->max = s->boundaryy - 1;
				range->quant = 1;
				o.constraint.range = range;
			}
			break;
		case CS3_OPTION_YMAX:
			o.name = "br-y";
			o.title = "Bottom y value of scan area";
			o.desc = "Bottom y value of scan area";
			o.type = SANE_TYPE_INT;
			o.unit = SANE_UNIT_PIXEL;
			o.size = WSIZE;
			o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
			o.constraint_type = SANE_CONSTRAINT_RANGE;
			range = (SANE_Range *)
				cs3_xmalloc(sizeof(SANE_Range));
			if (!range)
				alloc_failed = 1;
			else {
				range->min = 0;
				range->max = s->boundaryy - 1;
				range->quant = 1;
				o.constraint.range = range;
			}
			break;
		case CS3_OPTION_FOCUS_ON_CENTRE:
			o.name = "focus-on-centre";
			o.title = "Use centre of scan area as AF point";
			o.desc = "Use centre of scan area as AF point instead of manual AF point selection";
			o.type = SANE_TYPE_BOOL;
			o.size = WSIZE;
			o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
			break;
		case CS3_OPTION_FOCUS:
			o.name = "focus";
			o.title = "Focus position";
			o.desc = "Focus position for manual focus";
			o.type = SANE_TYPE_INT;
			o.unit = SANE_UNIT_NONE;
			o.size = WSIZE;
			o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
			o.constraint_type = SANE_CONSTRAINT_RANGE;
			range = (SANE_Range *)
				cs3_xmalloc(sizeof(SANE_Range));
			if (!range)
				alloc_failed = 1;
			else {
				range->min = s->focus_min;
				range->max = s->focus_max;
				range->quant = 1;
				o.constraint.range = range;
			}
			break;
		case CS3_OPTION_AUTOFOCUS:
			o.name = "autofocus";
			o.title = "Autofocus";
			o.desc = "Perform autofocus before scan";
			o.type = SANE_TYPE_BOOL;
			o.size = WSIZE;
			o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
			break;
		case CS3_OPTION_FOCUSX:
			o.name = "focusx";
			o.title = "X coordinate of AF point";
			o.desc = "X coordinate of AF point";
			o.type = SANE_TYPE_INT;
			o.unit = SANE_UNIT_PIXEL;
			o.size = WSIZE;
			o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT |
				SANE_CAP_INACTIVE;
			o.constraint_type = SANE_CONSTRAINT_RANGE;
			range = (SANE_Range *)
				cs3_xmalloc(sizeof(SANE_Range));
			if (!range)
				alloc_failed = 1;
			else {
				range->min = 0;
				range->max = s->boundaryx - 1;
				range->quant = 1;
				o.constraint.range = range;
			}
			break;
		case CS3_OPTION_FOCUSY:
			o.name = "focusy";
			o.title = "Y coordinate of AF point";
			o.desc = "Y coordinate of AF point";
			o.type = SANE_TYPE_INT;
			o.unit = SANE_UNIT_PIXEL;
			o.size = WSIZE;
			o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT |
				SANE_CAP_INACTIVE;
			o.constraint_type = SANE_CONSTRAINT_RANGE;
			range = (SANE_Range *)
				cs3_xmalloc(sizeof(SANE_Range));
			if (!range)
				alloc_failed = 1;
			else {
				range->min = 0;
				range->max = s->boundaryy - 1;
				range->quant = 1;
				o.constraint.range = range;
			}
			break;
		case CS3_OPTION_SCAN_AE:
			o.name = "ae";
			o.title = "Auto-exposure";
			o.desc = "Perform auto-exposure before scan";
			o.type = SANE_TYPE_BOOL;
			o.size = WSIZE;
			o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
			break;
		case CS3_OPTION_SCAN_AE_WB:
			o.name = "ae-wb";
			o.title = "Auto-exposure with white balance";
			o.desc = "Perform auto-exposure with white balance before scan";
			o.type = SANE_TYPE_BOOL;
			o.size = WSIZE;
			o.cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
			break;
		default:
			DBG(1, "BUG: sane_open(): Unknown option number: %d\n", i_option);
			break;
		}
		s->option_list[i_option] = o;
	}

	s->scanning = SANE_FALSE;
	s->preview = SANE_FALSE;
	s->negative = SANE_FALSE;
	s->autoload = SANE_FALSE;
	s->infrared = SANE_FALSE;
	s->ae = SANE_FALSE;
	s->aewb = SANE_FALSE;
	s->samples_per_scan = 1;
	s->depth = 8;
	s->i_frame = 1;
	s->frame_count = 1;
	s->subframe = 0.;
	s->res = s->resx = s->resx_max;
	s->resy = s->resy_max;
	s->res_independent = SANE_FALSE;
	s->res_preview = s->resx_max / 10;
	if (s->res_preview < s->resx_min)
		s->res_preview = s->resx_min;
	s->xmin = 0;
	s->xmax = s->boundaryx - 1;
	s->ymin = 0;
	s->ymax = s->boundaryy - 1;
	s->focus_on_centre = SANE_TRUE;
	s->focus = 0;
	s->focusx = 0;
	s->focusy = 0;
	s->exposure = 1.;
	s->exposure_r = 1200.;
	s->exposure_g = 1200.;
	s->exposure_b = 1000.;
	s->line_buf = NULL;
	s->n_line_buf = 0;

	if (alloc_failed) {
		cs3_close(s);
		return SANE_STATUS_NO_MEM;
	}

	return cs3_reserve_unit(s);
}

void
sane_close(SANE_Handle h)
{
	cs3_t *s = (cs3_t *) h;

	DBG(10, "%s\n", __func__);

	cs3_release_unit(s);
	cs3_close(s);
}

const SANE_Option_Descriptor *
sane_get_option_descriptor(SANE_Handle h, SANE_Int n)
{
	cs3_t *s = (cs3_t *) h;

	DBG(24, "%s, option %i\n", __func__, n);

	if ((n >= 0) && (n < CS3_N_OPTIONS))
		return &s->option_list[n];
	else
		return NULL;
}

SANE_Status
sane_control_option(SANE_Handle h, SANE_Int n, SANE_Action a, void *v,
		    SANE_Int * i)
{
	cs3_t *s = (cs3_t *) h;
	SANE_Int flags = 0;
	cs3_pixel_t pixel;
	SANE_Option_Descriptor o = s->option_list[n];

	DBG(24, "%s, option %i, action %i.\n", __func__, n, a);

	switch (a) {
	case SANE_ACTION_GET_VALUE:

		switch (n) {
		case CS3_OPTION_NUM:
			*(SANE_Word *) v = CS3_N_OPTIONS;
			break;
		case CS3_OPTION_NEGATIVE:
			*(SANE_Word *) v = s->negative;
			break;
		case CS3_OPTION_INFRARED:
			*(SANE_Word *) v = s->infrared;
			break;
		case CS3_OPTION_SAMPLES_PER_SCAN:
			*(SANE_Word *) v = s->samples_per_scan;
			break;
		case CS3_OPTION_DEPTH:
			*(SANE_Word *) v = s->depth;
			break;
		case CS3_OPTION_PREVIEW:
			*(SANE_Word *) v = s->preview;
			break;
		case CS3_OPTION_AUTOLOAD:
			*(SANE_Word *) v = s->autoload;
			break;
		case CS3_OPTION_EXPOSURE:
			*(SANE_Word *) v = SANE_FIX(s->exposure);
			break;
		case CS3_OPTION_EXPOSURE_R:
			*(SANE_Word *) v = SANE_FIX(s->exposure_r);
			break;
		case CS3_OPTION_EXPOSURE_G:
			*(SANE_Word *) v = SANE_FIX(s->exposure_g);
			break;
		case CS3_OPTION_EXPOSURE_B:
			*(SANE_Word *) v = SANE_FIX(s->exposure_b);
			break;
		case CS3_OPTION_LUT_R:
			if (!(s->lut_r))
				return SANE_STATUS_INVAL;
			for (pixel = 0; pixel < s->n_lut; pixel++)
				((SANE_Word *) v)[pixel] = s->lut_r[pixel];
			break;
		case CS3_OPTION_LUT_G:
			if (!(s->lut_g))
				return SANE_STATUS_INVAL;
			for (pixel = 0; pixel < s->n_lut; pixel++)
				((SANE_Word *) v)[pixel] = s->lut_g[pixel];
			break;
		case CS3_OPTION_LUT_B:
			if (!(s->lut_b))
				return SANE_STATUS_INVAL;
			for (pixel = 0; pixel < s->n_lut; pixel++)
				((SANE_Word *) v)[pixel] = s->lut_b[pixel];
			break;
		case CS3_OPTION_EJECT:
			break;
		case CS3_OPTION_LOAD:
			break;
		case CS3_OPTION_RESET:
			break;
		case CS3_OPTION_FRAME:
			*(SANE_Word *) v = s->i_frame;
			break;
		case CS3_OPTION_FRAME_COUNT:
			*(SANE_Word *) v = s->frame_count;
			break;
		case CS3_OPTION_SUBFRAME:
			*(SANE_Word *) v = SANE_FIX(s->subframe);
			break;
		case CS3_OPTION_RES:
			*(SANE_Word *) v = s->res;
			break;
		case CS3_OPTION_RESX:
			*(SANE_Word *) v = s->resx;
			break;
		case CS3_OPTION_RESY:
			*(SANE_Word *) v = s->resy;
			break;
		case CS3_OPTION_RES_INDEPENDENT:
			*(SANE_Word *) v = s->res_independent;
			break;
		case CS3_OPTION_PREVIEW_RESOLUTION:
			*(SANE_Word *) v = s->res_preview;
			break;
		case CS3_OPTION_XMIN:
			*(SANE_Word *) v = s->xmin;
			break;
		case CS3_OPTION_XMAX:
			*(SANE_Word *) v = s->xmax;
			break;
		case CS3_OPTION_YMIN:
			*(SANE_Word *) v = s->ymin;
			break;
		case CS3_OPTION_YMAX:
			*(SANE_Word *) v = s->ymax;
			break;
		case CS3_OPTION_FOCUS_ON_CENTRE:
			*(SANE_Word *) v = s->focus_on_centre;
			break;
		case CS3_OPTION_FOCUS:
			*(SANE_Word *) v = s->focus;
			break;
		case CS3_OPTION_AUTOFOCUS:
			*(SANE_Word *) v = s->autofocus;
			break;
		case CS3_OPTION_FOCUSX:
			*(SANE_Word *) v = s->focusx;
			break;
		case CS3_OPTION_FOCUSY:
			*(SANE_Word *) v = s->focusy;
			break;
		case CS3_OPTION_SCAN_AE:
			*(SANE_Word *) v = s->ae;
			break;
		case CS3_OPTION_SCAN_AE_WB:
			*(SANE_Word *) v = s->aewb;
			break;
		default:
			DBG(4, "%s: Unknown option (bug?).\n", __func__);
			return SANE_STATUS_INVAL;
		}
		break;

	case SANE_ACTION_SET_VALUE:
		if (s->scanning)
			return SANE_STATUS_INVAL;
		/* XXX do this for all elements of arrays */
		switch (o.type) {
		case SANE_TYPE_BOOL:
			if ((*(SANE_Word *) v != SANE_TRUE)
			    && (*(SANE_Word *) v != SANE_FALSE))
				return SANE_STATUS_INVAL;
			break;
		case SANE_TYPE_INT:
		case SANE_TYPE_FIXED:
			switch (o.constraint_type) {
			case SANE_CONSTRAINT_RANGE:
				if (*(SANE_Word *) v <
				    o.constraint.range->min) {
					*(SANE_Word *) v =
						o.constraint.range->min;
					flags |= SANE_INFO_INEXACT;
				} else if (*(SANE_Word *) v >
					   o.constraint.range->max) {
					*(SANE_Word *) v =
						o.constraint.range->max;
					flags |= SANE_INFO_INEXACT;
				}
				break;
			case SANE_CONSTRAINT_WORD_LIST:
				break;
			default:
				break;
			}
			break;
		case SANE_TYPE_STRING:
			break;
		case SANE_TYPE_BUTTON:
			break;
		case SANE_TYPE_GROUP:
			break;
		}
		switch (n) {
		case CS3_OPTION_NUM:
			return SANE_STATUS_INVAL;
			break;
		case CS3_OPTION_NEGATIVE:
			s->negative = *(SANE_Word *) v;
			break;
		case CS3_OPTION_INFRARED:
			s->infrared = *(SANE_Word *) v;
			/*      flags |= SANE_INFO_RELOAD_PARAMS; XXX */
			break;
		case CS3_OPTION_SAMPLES_PER_SCAN:
			s->samples_per_scan = *(SANE_Word *) v;
			break;
		case CS3_OPTION_DEPTH:
			if (*(SANE_Word *) v > s->maxbits)
				return SANE_STATUS_INVAL;

			s->depth = *(SANE_Word *) v;
			flags |= SANE_INFO_RELOAD_PARAMS;
			break;

		case CS3_OPTION_PREVIEW:
			s->preview = *(SANE_Word *) v;
			break;

		case CS3_OPTION_AUTOLOAD:
			s->autoload = *(SANE_Word *) v;
			break;

		case CS3_OPTION_EXPOSURE:
			s->exposure = SANE_UNFIX(*(SANE_Word *) v);
			break;
		case CS3_OPTION_EXPOSURE_R:
			s->exposure_r = SANE_UNFIX(*(SANE_Word *) v);
			break;
		case CS3_OPTION_EXPOSURE_G:
			s->exposure_g = SANE_UNFIX(*(SANE_Word *) v);
			break;
		case CS3_OPTION_EXPOSURE_B:
			s->exposure_b = SANE_UNFIX(*(SANE_Word *) v);
			break;
		case CS3_OPTION_LUT_R:
			if (!(s->lut_r))
				return SANE_STATUS_INVAL;
			for (pixel = 0; pixel < s->n_lut; pixel++)
				s->lut_r[pixel] = ((SANE_Word *) v)[pixel];
			break;
		case CS3_OPTION_LUT_G:
			if (!(s->lut_g))
				return SANE_STATUS_INVAL;
			for (pixel = 0; pixel < s->n_lut; pixel++)
				s->lut_g[pixel] = ((SANE_Word *) v)[pixel];
			break;
		case CS3_OPTION_LUT_B:
			if (!(s->lut_b))
				return SANE_STATUS_INVAL;
			for (pixel = 0; pixel < s->n_lut; pixel++)
				s->lut_b[pixel] = ((SANE_Word *) v)[pixel];
			break;
		case CS3_OPTION_LOAD:
			cs3_load(s);
			break;
		case CS3_OPTION_EJECT:
			cs3_eject(s);
			break;
		case CS3_OPTION_RESET:
			cs3_reset(s);
			break;
		case CS3_OPTION_FRAME:
			s->i_frame = *(SANE_Word *) v;
			break;

		case CS3_OPTION_FRAME_COUNT:
			if (*(SANE_Word *) v > (s->n_frames - s->i_frame + 1))
				return SANE_STATUS_INVAL;
			s->frame_count = *(SANE_Word *) v;
			flags |= SANE_INFO_RELOAD_PARAMS;
			break;

		case CS3_OPTION_SUBFRAME:
			s->subframe = SANE_UNFIX(*(SANE_Word *) v);
			break;
		case CS3_OPTION_RES:
			s->res = *(SANE_Word *) v;
			flags |= SANE_INFO_RELOAD_PARAMS;
			break;
		case CS3_OPTION_RESX:
			s->resx = *(SANE_Word *) v;
			flags |= SANE_INFO_RELOAD_PARAMS;
			break;
		case CS3_OPTION_RESY:
			s->resy = *(SANE_Word *) v;
			flags |= SANE_INFO_RELOAD_PARAMS;
			break;
		case CS3_OPTION_RES_INDEPENDENT:
			s->res_independent = *(SANE_Word *) v;
			flags |= SANE_INFO_RELOAD_PARAMS;
			break;
		case CS3_OPTION_PREVIEW_RESOLUTION:
			s->res_preview = *(SANE_Word *) v;
			flags |= SANE_INFO_RELOAD_PARAMS;
			break;
		case CS3_OPTION_XMIN:
			s->xmin = *(SANE_Word *) v;
			flags |= SANE_INFO_RELOAD_PARAMS;
			break;
		case CS3_OPTION_XMAX:
			s->xmax = *(SANE_Word *) v;
			flags |= SANE_INFO_RELOAD_PARAMS;
			break;
		case CS3_OPTION_YMIN:
			s->ymin = *(SANE_Word *) v;
			flags |= SANE_INFO_RELOAD_PARAMS;
			break;
		case CS3_OPTION_YMAX:
			s->ymax = *(SANE_Word *) v;
			flags |= SANE_INFO_RELOAD_PARAMS;
			break;
		case CS3_OPTION_FOCUS_ON_CENTRE:
			s->focus_on_centre = *(SANE_Word *) v;
			if (s->focus_on_centre) {
				s->option_list[CS3_OPTION_FOCUSX].cap |=
					SANE_CAP_INACTIVE;
				s->option_list[CS3_OPTION_FOCUSY].cap |=
					SANE_CAP_INACTIVE;
			} else {
				s->option_list[CS3_OPTION_FOCUSX].cap &=
					~SANE_CAP_INACTIVE;
				s->option_list[CS3_OPTION_FOCUSY].cap &=
					~SANE_CAP_INACTIVE;
			}
			flags |= SANE_INFO_RELOAD_OPTIONS;
			break;
		case CS3_OPTION_FOCUS:
			s->focus = *(SANE_Word *) v;
			break;
		case CS3_OPTION_AUTOFOCUS:
			s->autofocus = *(SANE_Word *) v;
			break;
		case CS3_OPTION_FOCUSX:
			s->focusx = *(SANE_Word *) v;
			break;
		case CS3_OPTION_FOCUSY:
			s->focusy = *(SANE_Word *) v;
			break;
		case CS3_OPTION_SCAN_AE:
			s->ae = *(SANE_Word *) v;
			break;
		case CS3_OPTION_SCAN_AE_WB:
			s->aewb = *(SANE_Word *) v;
			break;
		default:
			DBG(4,
			    "Error: sane_control_option(): Unknown option number (bug?).\n");
			return SANE_STATUS_INVAL;
			break;
		}
		break;

	default:
		DBG(1,
		    "BUG: sane_control_option(): Unknown action number.\n");
		return SANE_STATUS_INVAL;
		break;
	}

	if (i)
		*i = flags;

	return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_parameters(SANE_Handle h, SANE_Parameters * p)
{
	cs3_t *s = (cs3_t *) h;
	SANE_Status status;

	DBG(10, "%s\n", __func__);

	if (!s->scanning) {	/* only recalculate when not scanning */
		status = cs3_convert_options(s);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	p->bytes_per_line =
		s->n_colors * s->logical_width * s->bytes_per_pixel;

#ifdef SANE_FRAME_RGBI
	if (s->infrared) {
		p->format = SANE_FRAME_RGBI;

	} else {
#endif
		p->format = SANE_FRAME_RGB;	/* XXXXXXXX CCCCCCCCCC */
#ifdef SANE_FRAME_RGBI
	}
#endif

	p->last_frame = SANE_TRUE;
	p->lines = s->logical_height;
	p->depth = 8 * s->bytes_per_pixel;
	p->pixels_per_line = s->logical_width;

	return SANE_STATUS_GOOD;
}

SANE_Status
sane_start(SANE_Handle h)
{
	cs3_t *s = (cs3_t *) h;
	SANE_Status status;

	DBG(10, "%s\n", __func__);

	if (s->scanning)
		return SANE_STATUS_INVAL;

	if (s->n_frames > 1 && s->frame_count == 0) {
		DBG(4, "%s: no more frames\n", __func__);
		return SANE_STATUS_NO_DOCS;
	}

	if (s->n_frames > 1) {
		DBG(4, "%s: scanning frame at position %d, %d to go\n",
		    __func__, s->i_frame, s->frame_count);
	}

	status = cs3_convert_options(s);
	if (status != SANE_STATUS_GOOD)
		return status;

	s->i_line_buf = 0;
	s->xfer_position = 0;

	s->scanning = SANE_TRUE;

	/* load if appropriate */
	if (s->autoload) {
		status = cs3_load(s);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	/* check for documents */
	status = cs3_scanner_ready(s, CS3_STATUS_NO_DOCS);
	if (status != SANE_STATUS_GOOD)
		return status;
	if (s->status & CS3_STATUS_NO_DOCS)
		return SANE_STATUS_NO_DOCS;

	if (s->autofocus) {
		status = cs3_autofocus(s);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	if (s->aewb) {
		status = cs3_autoexposure(s, 1);
		if (status != SANE_STATUS_GOOD)
			return status;
	} else if (s->ae) {
		status = cs3_autoexposure(s, 0);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	return cs3_scan(s, CS3_SCAN_NORMAL);
}

SANE_Status
sane_read(SANE_Handle h, SANE_Byte * buf, SANE_Int maxlen, SANE_Int * len)
{
	cs3_t *s = (cs3_t *) h;
	SANE_Status status;
	ssize_t xfer_len_in, xfer_len_line, xfer_len_out;
	unsigned long index;
	int color, sample_pass;
	uint8_t *s8 = NULL;
	uint16_t *s16 = NULL;
	double m_avg_sum;
	SANE_Byte *line_buf_new;

	DBG(32, "%s, maxlen = %i.\n", __func__, maxlen);

	if (!s->scanning) {
		*len = 0;
		return SANE_STATUS_CANCELLED;
	}

	/* transfer from buffer */
	if (s->i_line_buf > 0) {
		xfer_len_out = s->n_line_buf - s->i_line_buf;
		if (xfer_len_out > maxlen)
			xfer_len_out = maxlen;

		memcpy(buf, &(s->line_buf[s->i_line_buf]), xfer_len_out);

		s->i_line_buf += xfer_len_out;
		if (s->i_line_buf >= s->n_line_buf)
			s->i_line_buf = 0;

		*len = xfer_len_out;
		return SANE_STATUS_GOOD;
	}

	xfer_len_line = s->n_colors * s->logical_width * s->bytes_per_pixel;
	xfer_len_in = xfer_len_line + (s->n_colors * s->odd_padding);

	if ((xfer_len_in & 0x3f)) {
		int d = ((xfer_len_in / 512) * 512) + 512;
		s->block_padding = d - xfer_len_in;
	}

	DBG(22, "%s: block_padding = %d, odd_padding = %d\n",
	    __func__, s->block_padding, s->odd_padding);

	DBG(22,
	    "%s: colors = %d, logical_width = %ld, bytes_per_pixel = %d\n",
	    __func__, s->n_colors, s->logical_width, s->bytes_per_pixel);


	/* Do not change the behaviour of older models, pad to 512 */
	if ((s->type == CS3_TYPE_LS50) || (s->type == CS3_TYPE_LS5000)) {
		xfer_len_in += s->block_padding;
		if (xfer_len_in & 0x3f)
			DBG(1, "BUG: %s, not a multiple of 64. (0x%06lx)\n",
			    __func__, (long) xfer_len_in);
	}

	if (s->xfer_position + xfer_len_line > s->xfer_bytes_total)
		xfer_len_line = s->xfer_bytes_total - s->xfer_position;	/* just in case */

	if (xfer_len_line == 0) {	/* no more data */
		*len = 0;

		/* increment frame number if appropriate */
		if (s->n_frames > 1 && --s->frame_count) {
			s->i_frame++;
		}

		s->scanning = SANE_FALSE;
		return SANE_STATUS_EOF;
	}

	if (xfer_len_line != s->n_line_buf) {
		line_buf_new =
			(SANE_Byte *) cs3_xrealloc(s->line_buf,
						   xfer_len_line *
						   sizeof(SANE_Byte));
		if (!line_buf_new) {
			*len = 0;
			return SANE_STATUS_NO_MEM;
		}
		s->line_buf = line_buf_new;
		s->n_line_buf = xfer_len_line;
	}

	/* adapt for multi-sampling */
	xfer_len_in *= s->samples_per_scan;

	cs3_scanner_ready(s, CS3_STATUS_READY);
	cs3_init_buffer(s);
	cs3_parse_cmd(s, "28 00 00 00 00 00");
	cs3_pack_byte(s, (xfer_len_in >> 16) & 0xff);
	cs3_pack_byte(s, (xfer_len_in >> 8) & 0xff);
	cs3_pack_byte(s, xfer_len_in & 0xff);
	cs3_parse_cmd(s, "00");
	s->n_recv = xfer_len_in;

	status = cs3_issue_cmd(s);
	if (status != SANE_STATUS_GOOD) {
		*len = 0;
		return status;
	}

	for (index = 0; index < s->logical_width; index++) {
		for (color = 0; color < s->n_colors; color++) {
			int where = s->bytes_per_pixel
				* (s->n_colors * index + color);

			m_avg_sum = 0.0;

			switch (s->bytes_per_pixel) {
			case 1:
			{
				/* target address */
				s8 = (uint8_t *) & (s->line_buf[where]);

				if (s->samples_per_scan > 1) {
					/* calculate average of multi samples */
					for (sample_pass = 0;
							sample_pass < s->samples_per_scan;
							sample_pass++) {
						/* source index */
						int p8 = (sample_pass * s->n_colors + color)
							* s->logical_width
							+ (color + 1) * s->odd_padding
							+ index;
						m_avg_sum += (double) s->recv_buf[p8];
					}
					*s8 = (uint8_t) (m_avg_sum / s->samples_per_scan + 0.5);
				} else {
					/* shortcut for single sample */
					int p8 = s->logical_width * color
						+ (color + 1) * s->odd_padding
						+ index;
					*s8 = s->recv_buf[p8];
				}
			}
				break;
			case 2:
			{
				/* target address */
				s16 = (uint16_t *) & (s->line_buf[where]);

				if (s->samples_per_scan > 1) {
					/* calculate average of multi samples */
					for (sample_pass = 0;
							sample_pass < s->samples_per_scan;
							sample_pass++) {
						/* source index */
						int p16 = 2 * ((sample_pass * s->n_colors + color)
								* s->logical_width + index);
						m_avg_sum += (double) ((s->recv_buf[p16] << 8)
							+ s->recv_buf[p16 + 1]);
					}
					*s16 = (uint16_t) (m_avg_sum / s->samples_per_scan + 0.5);
				} else {
					/* shortcut for single sample */
					int p16 = 2 * (color * s->logical_width + index);

					*s16 = (s->recv_buf[p16] << 8)
						+ s->recv_buf[p16 + 1];
				}

				*s16 <<= s->shift_bits;
			}
				break;

			default:
				DBG(1,
				    "BUG: sane_read(): Unknown number of bytes per pixel.\n");
				*len = 0;
				return SANE_STATUS_INVAL;
				break;
			}
		}
	}

	s->xfer_position += xfer_len_line;

	xfer_len_out = xfer_len_line;
	if (xfer_len_out > maxlen)
		xfer_len_out = maxlen;

	memcpy(buf, s->line_buf, xfer_len_out);
	if (xfer_len_out < xfer_len_line)
		s->i_line_buf = xfer_len_out;	/* data left in the line buffer, read out next time */

	*len = xfer_len_out;
	return SANE_STATUS_GOOD;
}

void
sane_cancel(SANE_Handle h)
{
	cs3_t *s = (cs3_t *) h;

	DBG(10, "%s, scanning = %d.\n", __func__, s->scanning);

	if (s->scanning) {
		cs3_init_buffer(s);
		cs3_parse_cmd(s, "c0 00 00 00 00 00");
		cs3_issue_cmd(s);
	}

	s->scanning = SANE_FALSE;
}

SANE_Status
sane_set_io_mode(SANE_Handle h, SANE_Bool m)
{
	cs3_t *s = (cs3_t *) h;

	DBG(10, "%s\n", __func__);

	if (!s->scanning)
		return SANE_STATUS_INVAL;
	if (m == SANE_FALSE)
		return SANE_STATUS_GOOD;
	else
		return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
sane_get_select_fd(SANE_Handle h, SANE_Int * fd)
{
	cs3_t *s = (cs3_t *) h;

	DBG(10, "%s\n", __func__);

	fd = fd;		/* to shut up compiler */
	s = s;			/* to shut up compiler */

	return SANE_STATUS_UNSUPPORTED;
}


/* ========================================================================= */
/* private functions */

static void
cs3_trim(char *s)
{
	int i, l = strlen(s);

	for (i = l - 1; i > 0; i--) {
		if (s[i] == ' ')
			s[i] = '\0';
		else
			break;
	}
}

static SANE_Status
cs3_open(const char *device, cs3_interface_t interface, cs3_t ** sp)
{
	SANE_Status status;
	cs3_t *s;
	char *prefix = NULL, *line;
	int i;
	int alloc_failed = 0;
	SANE_Device **device_list_new;

	DBG(6, "%s, device = %s, interface = %i\n",
	    __func__, device, interface);

	if (!strncmp(device, "auto", 5)) {
		try_interface = CS3_INTERFACE_SCSI;
		sanei_config_attach_matching_devices("scsi Nikon *",
						     cs3_attach);
		try_interface = CS3_INTERFACE_USB;
		sanei_usb_attach_matching_devices("usb 0x04b0 0x4000",
						  cs3_attach);
		sanei_usb_attach_matching_devices("usb 0x04b0 0x4001",
						  cs3_attach);
		sanei_usb_attach_matching_devices("usb 0x04b0 0x4002",
						  cs3_attach);
		return SANE_STATUS_GOOD;
	}

	if ((s = (cs3_t *) cs3_xmalloc(sizeof(cs3_t))) == NULL)
		return SANE_STATUS_NO_MEM;
	memset(s, 0, sizeof(cs3_t));

	/* fill magic bits */
	s->magic = SANE_COOKIE;
	s->cookie_ptr = &s->cookie;

	s->cookie.version = 0x01;
	s->cookie.vendor = s->vendor_string;
	s->cookie.model = s->product_string;
	s->cookie.revision = s->revision_string;

	s->send_buf = s->recv_buf = NULL;
	s->send_buf_size = s->recv_buf_size = 0;

	switch (interface) {
	case CS3_INTERFACE_UNKNOWN:
		for (i = 0; i < 2; i++) {
			switch (i) {
			case 1:
				prefix = "usb:";
				try_interface = CS3_INTERFACE_USB;
				break;
			default:
				prefix = "scsi:";
				try_interface = CS3_INTERFACE_SCSI;
				break;
			}
			if (!strncmp(device, prefix, strlen(prefix))) {
				const void *p = device + strlen(prefix);
				cs3_xfree(s);
				return cs3_open(p, try_interface, sp);
			}
		}
		cs3_xfree(s);
		return SANE_STATUS_INVAL;
		break;
	case CS3_INTERFACE_SCSI:
		s->interface = CS3_INTERFACE_SCSI;
		DBG(6,
		    "%s, trying to open %s, assuming SCSI or SBP2 interface\n",
		    __func__, device);
		status = sanei_scsi_open(device, &s->fd,
					 cs3_scsi_sense_handler, s);
		if (status != SANE_STATUS_GOOD) {
			DBG(6, " ...failed: %s.\n", sane_strstatus(status));
			cs3_xfree(s);
			return status;
		}
		break;
	case CS3_INTERFACE_USB:
		s->interface = CS3_INTERFACE_USB;
		DBG(6, "%s, trying to open %s, assuming USB interface\n",
		    __func__, device);
		status = sanei_usb_open(device, &s->fd);
		if (status != SANE_STATUS_GOOD) {
			DBG(6, " ...failed: %s.\n", sane_strstatus(status));
			cs3_xfree(s);
			return status;
		}
		break;
	}

	open_devices++;
	DBG(6, "%s, trying to identify device.\n", __func__);

	/* identify scanner */
	status = cs3_page_inquiry(s, -1);
	if (status != SANE_STATUS_GOOD) {
		cs3_close(s);
		return status;
	}

	strncpy(s->vendor_string, (char *) s->recv_buf + 8, 8);
	s->vendor_string[8] = '\0';
	strncpy(s->product_string, (char *) s->recv_buf + 16, 16);
	s->product_string[16] = '\0';
	strncpy(s->revision_string, (char *) s->recv_buf + 32, 4);
	s->revision_string[4] = '\0';

	DBG(10,
	    "%s, vendor = '%s', product = '%s', revision = '%s'.\n",
	    __func__, s->vendor_string, s->product_string,
	    s->revision_string);

	if (!strncmp(s->product_string, "COOLSCANIII     ", 16))
		s->type = CS3_TYPE_LS30;
	else if (!strncmp(s->product_string, "LS-40 ED        ", 16))
		s->type = CS3_TYPE_LS40;
	else if (!strncmp(s->product_string, "LS-50 ED        ", 16))
		s->type = CS3_TYPE_LS50;
	else if (!strncmp(s->product_string, "LS-2000         ", 16))
		s->type = CS3_TYPE_LS2000;
	else if (!strncmp(s->product_string, "LS-4000 ED      ", 16))
		s->type = CS3_TYPE_LS4000;
	else if (!strncmp(s->product_string, "LS-5000 ED      ", 16))
		s->type = CS3_TYPE_LS5000;
	else if (!strncmp(s->product_string, "LS-8000 ED      ", 16))
		s->type = CS3_TYPE_LS8000;

	if (s->type != CS3_TYPE_UNKOWN)
		DBG(10,
		    "%s, device identified as coolscan3 type #%i.\n",
		    __func__, s->type);
	else {
		DBG(10, "%s, device not identified.\n", __func__);
		cs3_close(s);
		return SANE_STATUS_UNSUPPORTED;
	}

	cs3_trim(s->vendor_string);
	cs3_trim(s->product_string);
	cs3_trim(s->revision_string);

	if (sp)
		*sp = s;
	else {
		device_list_new =
			(SANE_Device **) cs3_xrealloc(device_list,
						      (n_device_list +
						       2) *
						      sizeof(SANE_Device *));
		if (!device_list_new)
			return SANE_STATUS_NO_MEM;
		device_list = device_list_new;
		device_list[n_device_list] =
			(SANE_Device *) cs3_xmalloc(sizeof(SANE_Device));
		if (!device_list[n_device_list])
			return SANE_STATUS_NO_MEM;
		switch (interface) {
		case CS3_INTERFACE_UNKNOWN:
			DBG(1, "BUG: cs3_open(): unknown interface.\n");
			cs3_close(s);
			return SANE_STATUS_UNSUPPORTED;
			break;
		case CS3_INTERFACE_SCSI:
			prefix = "scsi:";
			break;
		case CS3_INTERFACE_USB:
			prefix = "usb:";
			break;
		}

		line = (char *) cs3_xmalloc(strlen(device) + strlen(prefix) +
					    1);
		if (!line)
			alloc_failed = 1;
		else {
			strcpy(line, prefix);
			strcat(line, device);
			device_list[n_device_list]->name = line;
		}

		line = (char *) cs3_xmalloc(strlen(s->vendor_string) + 1);
		if (!line)
			alloc_failed = 1;
		else {
			strcpy(line, s->vendor_string);
			device_list[n_device_list]->vendor = line;
		}

		line = (char *) cs3_xmalloc(strlen(s->product_string) + 1);
		if (!line)
			alloc_failed = 1;
		else {
			strcpy(line, s->product_string);
			device_list[n_device_list]->model = line;
		}

		device_list[n_device_list]->type = "film scanner";

		if (alloc_failed) {
			cs3_xfree(device_list[n_device_list]->name);
			cs3_xfree(device_list[n_device_list]->vendor);
			cs3_xfree(device_list[n_device_list]->model);
			cs3_xfree(device_list[n_device_list]);
		} else
			n_device_list++;
		device_list[n_device_list] = NULL;

		cs3_close(s);
	}

	return SANE_STATUS_GOOD;
}

void
cs3_close(cs3_t * s)
{
	cs3_xfree(s->lut_r);
	cs3_xfree(s->lut_g);
	cs3_xfree(s->lut_b);
	cs3_xfree(s->lut_neutral);
	cs3_xfree(s->line_buf);

	switch (s->interface) {
	case CS3_INTERFACE_UNKNOWN:
		DBG(0, "BUG: %s: Unknown interface number.\n", __func__);
		break;
	case CS3_INTERFACE_SCSI:
		sanei_scsi_close(s->fd);
		open_devices--;
		break;
	case CS3_INTERFACE_USB:
		sanei_usb_close(s->fd);
		open_devices--;
		break;
	}

	cs3_xfree(s);
}

static SANE_Status
cs3_attach(const char *dev)
{
	SANE_Status status;

	if (try_interface == CS3_INTERFACE_UNKNOWN)
		return SANE_STATUS_UNSUPPORTED;

	status = cs3_open(dev, try_interface, NULL);
	return status;
}

static SANE_Status
cs3_scsi_sense_handler(int fd, u_char * sense_buffer, void *arg)
{
	cs3_t *s = (cs3_t *) arg;

	fd = fd;		/* to shut up compiler */

	/* sort this out ! XXX */

	s->sense_key = sense_buffer[2] & 0x0f;
	s->sense_asc = sense_buffer[12];
	s->sense_ascq = sense_buffer[13];
	s->sense_info = sense_buffer[3];

	return cs3_parse_sense_data(s);
}

static SANE_Status
cs3_parse_sense_data(cs3_t * s)
{
	SANE_Status status = SANE_STATUS_GOOD;

	s->sense_code =
		(s->sense_key << 24) + (s->sense_asc << 16) +
		(s->sense_ascq << 8) + s->sense_info;

	if (s->sense_key)
		DBG(14, "sense code: %02lx-%02lx-%02lx-%02lx\n", s->sense_key,
		    s->sense_asc, s->sense_ascq, s->sense_info);

	switch (s->sense_key) {
	case 0x00:
		s->status = CS3_STATUS_READY;
		break;

	case 0x02:
		switch (s->sense_asc) {
		case 0x04:
			DBG(15, " processing\n");
			s->status = CS3_STATUS_PROCESSING;
			break;
		case 0x3a:
			DBG(15, " no docs\n");
			s->status = CS3_STATUS_NO_DOCS;
			break;
		default:
			DBG(15, " default\n");
			s->status = CS3_STATUS_ERROR;
			status = SANE_STATUS_IO_ERROR;
			break;
		}
		break;

	case 0x09:
		if ((s->sense_code == 0x09800600)
		    || (s->sense_code == 0x09800601))
			s->status = CS3_STATUS_REISSUE;
		break;

	default:
		s->status = CS3_STATUS_ERROR;
		status = SANE_STATUS_IO_ERROR;
		break;
	}

	return status;
}

static void
cs3_init_buffer(cs3_t * s)
{
	s->n_cmd = 0;
	s->n_send = 0;
	s->n_recv = 0;
}

static SANE_Status
cs3_pack_byte(cs3_t * s, SANE_Byte byte)
{
	while (s->send_buf_size <= s->n_send) {
		s->send_buf_size += 16;
		s->send_buf =
			(SANE_Byte *) cs3_xrealloc(s->send_buf,
						   s->send_buf_size);
		if (!s->send_buf)
			return SANE_STATUS_NO_MEM;
	}

	s->send_buf[s->n_send++] = byte;

	return SANE_STATUS_GOOD;
}

static void
cs3_pack_long(cs3_t * s, unsigned long val)
{
	cs3_pack_byte(s, (val >> 24) & 0xff);
	cs3_pack_byte(s, (val >> 16) & 0xff);
	cs3_pack_byte(s, (val >> 8) & 0xff);
	cs3_pack_byte(s, val & 0xff);
}

static void
cs3_pack_word(cs3_t * s, unsigned long val)
{
	cs3_pack_byte(s, (val >> 8) & 0xff);
	cs3_pack_byte(s, val & 0xff);
}

static SANE_Status
cs3_parse_cmd(cs3_t * s, char *text)
{
	size_t i, j;
	char c, h;
	SANE_Status status;

	for (i = 0; i < strlen(text); i += 2)
		if (text[i] == ' ')
			i--;	/* a bit dirty... advance by -1+2=1 */
		else {
			if ((!isxdigit(text[i])) || (!isxdigit(text[i + 1])))
				DBG(1,
				    "BUG: cs3_parse_cmd(): Parser got invalid character.\n");
			c = 0;
			for (j = 0; j < 2; j++) {
				h = tolower(text[i + j]);
				if ((h >= 'a') && (h <= 'f'))
					c += 10 + h - 'a';
				else
					c += h - '0';
				if (j == 0)
					c <<= 4;
			}
			status = cs3_pack_byte(s, c);
			if (status != SANE_STATUS_GOOD)
				return status;
		}

	return SANE_STATUS_GOOD;
}

static SANE_Status
cs3_grow_send_buffer(cs3_t * s)
{
	if (s->n_send > s->send_buf_size) {
		s->send_buf_size = s->n_send;
		s->send_buf =
			(SANE_Byte *) cs3_xrealloc(s->send_buf,
						   s->send_buf_size);
		if (!s->send_buf)
			return SANE_STATUS_NO_MEM;
	}

	return SANE_STATUS_GOOD;
}

static SANE_Status
cs3_issue_cmd(cs3_t * s)
{
	SANE_Status status = SANE_STATUS_INVAL;
	size_t n_data, n_status;
	static SANE_Byte status_buf[8];
	int status_only = 0;

	DBG(20,
	    "cs3_issue_cmd(): opcode = 0x%02x, n_send = %lu, n_recv = %lu.\n",
	    s->send_buf[0], (unsigned long) s->n_send,
	    (unsigned long) s->n_recv);

	s->status = CS3_STATUS_READY;

	if (!s->n_cmd)
		switch (s->send_buf[0]) {
		case 0x00:
		case 0x12:
		case 0x15:
		case 0x16:
		case 0x17:
		case 0x1a:
		case 0x1b:
		case 0x1c:
		case 0x1d:
		case 0xc0:
		case 0xc1:
			s->n_cmd = 6;
			break;
		case 0x24:
		case 0x25:
		case 0x28:
		case 0x2a:
		case 0xe0:
		case 0xe1:
			s->n_cmd = 10;
			break;
		default:
			DBG(1,
			    "BUG: cs3_issue_cmd(): Unknown command opcode 0x%02x.\n",
			    s->send_buf[0]);
			break;
		}

	if (s->n_send < s->n_cmd) {
		DBG(1,
		    "BUG: cs3_issue_cmd(): Negative number of data out bytes requested.\n");
		return SANE_STATUS_INVAL;
	}

	n_data = s->n_send - s->n_cmd;
	if (s->n_recv > 0) {
		if (n_data > 0) {
			DBG(1,
			    "BUG: cs3_issue_cmd(): Both data in and data out requested.\n");
			return SANE_STATUS_INVAL;
		} else {
			n_data = s->n_recv;
		}
	}

	s->recv_buf = (SANE_Byte *) cs3_xrealloc(s->recv_buf, s->n_recv);
	if (!s->recv_buf)
		return SANE_STATUS_NO_MEM;

	switch (s->interface) {
	case CS3_INTERFACE_UNKNOWN:
		DBG(1,
		    "BUG: cs3_issue_cmd(): Unknown or uninitialized interface number.\n");
		break;

	case CS3_INTERFACE_SCSI:
		sanei_scsi_cmd2(s->fd, s->send_buf, s->n_cmd,
				s->send_buf + s->n_cmd, s->n_send - s->n_cmd,
				s->recv_buf, &s->n_recv);
		status = SANE_STATUS_GOOD;
		break;

	case CS3_INTERFACE_USB:
		status = sanei_usb_write_bulk(s->fd, s->send_buf, &s->n_cmd);
		if (status != SANE_STATUS_GOOD) {
			DBG(1,
			    "Error: cs3_issue_cmd(): Could not write command.\n");
			return SANE_STATUS_IO_ERROR;
		}

		switch (cs3_phase_check(s)) {
		case CS3_PHASE_OUT:
			if (s->n_send - s->n_cmd < n_data || !n_data) {
				DBG(4,
				    "Error: cs3_issue_cmd(): Unexpected data out phase.\n");
				return SANE_STATUS_IO_ERROR;
			}
			status = sanei_usb_write_bulk(s->fd,
						      s->send_buf + s->n_cmd,
						      &n_data);
			break;

		case CS3_PHASE_IN:
			if (s->n_recv < n_data || !n_data) {
				DBG(4,
				    "Error: cs3_issue_cmd(): Unexpected data in phase.\n");
				return SANE_STATUS_IO_ERROR;
			}
			status = sanei_usb_read_bulk(s->fd, s->recv_buf,
						     &n_data);
			s->n_recv = n_data;
			break;

		case CS3_PHASE_NONE:
			DBG(4, "%s: No command received!\n", __func__);
			return SANE_STATUS_IO_ERROR;

		default:
			if (n_data) {
				DBG(4,
				    "%s: Unexpected non-data phase, but n_data != 0 (%lu).\n",
				    __func__, (u_long) n_data);
				status_only = 1;
			}
			break;
		}

		n_status = 8;
		status = sanei_usb_read_bulk(s->fd, status_buf, &n_status);
		if (n_status != 8) {
			DBG(4,
			    "Error: cs3_issue_cmd(): Failed to read 8 status bytes from USB.\n");
			return SANE_STATUS_IO_ERROR;
		}

		s->sense_key = status_buf[1] & 0x0f;
		s->sense_asc = status_buf[2] & 0xff;
		s->sense_ascq = status_buf[3] & 0xff;
		s->sense_info = status_buf[4] & 0xff;
		status = cs3_parse_sense_data(s);
		break;
	}

	if (status_only)
		return SANE_STATUS_IO_ERROR;
	else
		return status;
}

static cs3_phase_t
cs3_phase_check(cs3_t * s)
{
	static SANE_Byte phase_send_buf[1] = { 0xd0 }, phase_recv_buf[1];
	SANE_Status status = 0;
	size_t n = 1;

	status = sanei_usb_write_bulk(s->fd, phase_send_buf, &n);
	status |= sanei_usb_read_bulk(s->fd, phase_recv_buf, &n);

	DBG(40, "%s: returned phase = 0x%02x.\n", __func__,
	    phase_recv_buf[0]);

	if (status != SANE_STATUS_GOOD)
		return -1;
	else
		return phase_recv_buf[0];
}

static SANE_Status
cs3_scanner_ready(cs3_t * s, int flags)
{
	SANE_Status status = SANE_STATUS_GOOD;
	int i = -1;
	unsigned long count = 0;
	int retry = 3;

	do {
		if (i >= 0)	/* dirty !!! */
			usleep(1000000);
		/* test unit ready */
		cs3_init_buffer(s);
		for (i = 0; i < 6; i++)
			cs3_pack_byte(s, 0x00);

		status = cs3_issue_cmd(s);
		if (status != SANE_STATUS_GOOD)
			if (--retry < 0)
				return status;

		if (++count > 120) {	/* 120s timeout */
			DBG(4, "Error: %s: Timeout expired.\n", __func__);
			status = SANE_STATUS_IO_ERROR;
			break;
		}
	}
	while (s->status & ~flags);	/* until all relevant bits are 0 */

	return status;
}

static SANE_Status
cs3_page_inquiry(cs3_t * s, int page)
{
	SANE_Status status;

	size_t n;

	if (page >= 0) {

		cs3_scanner_ready(s, CS3_STATUS_NO_DOCS);
		cs3_init_buffer(s);
		cs3_parse_cmd(s, "12 01");
		cs3_pack_byte(s, page);
		cs3_parse_cmd(s, "00 04 00");
		s->n_recv = 4;
		status = cs3_issue_cmd(s);
		if (status != SANE_STATUS_GOOD) {
			DBG(4,
			    "Error: cs3_page_inquiry(): Inquiry of page size failed: %s.\n",
			    sane_strstatus(status));
			return status;
		}

		n = s->recv_buf[3] + 4;

	} else
		n = 36;

	cs3_scanner_ready(s, CS3_STATUS_NO_DOCS);
	cs3_init_buffer(s);
	if (page >= 0) {
		cs3_parse_cmd(s, "12 01");
		cs3_pack_byte(s, page);
		cs3_parse_cmd(s, "00");
	} else
		cs3_parse_cmd(s, "12 00 00 00");
	cs3_pack_byte(s, n);
	cs3_parse_cmd(s, "00");
	s->n_recv = n;

	status = cs3_issue_cmd(s);
	if (status != SANE_STATUS_GOOD) {
		DBG(4, "Error: %s: inquiry of page failed: %s.\n",
		    __func__, sane_strstatus(status));
		return status;
	}

	return SANE_STATUS_GOOD;
}

static SANE_Status
cs3_full_inquiry(cs3_t * s)
{
	SANE_Status status;
	int pitch, pitch_max;
	cs3_pixel_t pixel;

	DBG(4, "%s\n", __func__);

	status = cs3_page_inquiry(s, 0xc1);
	if (status != SANE_STATUS_GOOD)
		return status;

	s->maxbits = s->recv_buf[82];
	if (s->type == CS3_TYPE_LS30)	/* must be overridden, LS-30 claims to have 12 bits */
		s->maxbits = 10;

	s->n_lut = 1;
	s->n_lut <<= s->maxbits;
	s->lut_r =
		(cs3_pixel_t *) cs3_xrealloc(s->lut_r,
					     s->n_lut * sizeof(cs3_pixel_t));
	s->lut_g =
		(cs3_pixel_t *) cs3_xrealloc(s->lut_g,
					     s->n_lut * sizeof(cs3_pixel_t));
	s->lut_b =
		(cs3_pixel_t *) cs3_xrealloc(s->lut_b,
					     s->n_lut * sizeof(cs3_pixel_t));
	s->lut_neutral =
		(cs3_pixel_t *) cs3_xrealloc(s->lut_neutral,
					     s->n_lut * sizeof(cs3_pixel_t));

	if (!s->lut_r || !s->lut_g || !s->lut_b || !s->lut_neutral) {
		cs3_xfree(s->lut_r);
		cs3_xfree(s->lut_g);
		cs3_xfree(s->lut_b);
		cs3_xfree(s->lut_neutral);
		return SANE_STATUS_NO_MEM;
	}

	for (pixel = 0; pixel < s->n_lut; pixel++) {
		s->lut_r[pixel] = s->lut_g[pixel] = s->lut_b[pixel] =
			s->lut_neutral[pixel] = pixel;
	}

	s->resx_optical = 256 * s->recv_buf[18] + s->recv_buf[19];
	s->resx_max = 256 * s->recv_buf[20] + s->recv_buf[21];
	s->resx_min = 256 * s->recv_buf[22] + s->recv_buf[23];
	s->boundaryx =
		65536 * (256 * s->recv_buf[36] + s->recv_buf[37]) +
		256 * s->recv_buf[38] + s->recv_buf[39];

	s->resy_optical = 256 * s->recv_buf[40] + s->recv_buf[41];
	s->resy_max = 256 * s->recv_buf[42] + s->recv_buf[43];
	s->resy_min = 256 * s->recv_buf[44] + s->recv_buf[45];
	s->boundaryy =
		65536 * (256 * s->recv_buf[58] + s->recv_buf[59]) +
		256 * s->recv_buf[60] + s->recv_buf[61];

	s->focus_min = 256 * s->recv_buf[76] + s->recv_buf[77];
	s->focus_max = 256 * s->recv_buf[78] + s->recv_buf[79];

	s->n_frames = s->recv_buf[75];

	s->frame_offset = s->resy_max * 1.5 + 1;	/* works for LS-30, maybe not for others */

	/* generate resolution list for x */
	s->resx_n_list = pitch_max =
		floor(s->resx_max / (double) s->resx_min);
	s->resx_list =
		(unsigned int *) cs3_xrealloc(s->resx_list,
					      pitch_max *
					      sizeof(unsigned int));
	for (pitch = 1; pitch <= pitch_max; pitch++)
		s->resx_list[pitch - 1] = s->resx_max / pitch;

	/* generate resolution list for y */
	s->resy_n_list = pitch_max =
		floor(s->resy_max / (double) s->resy_min);
	s->resy_list =
		(unsigned int *) cs3_xrealloc(s->resy_list,
					      pitch_max *
					      sizeof(unsigned int));

	for (pitch = 1; pitch <= pitch_max; pitch++)
		s->resy_list[pitch - 1] = s->resy_max / pitch;

	s->unit_dpi = s->resx_max;
	s->unit_mm = 25.4 / s->unit_dpi;

	DBG(4, " maximum depth:	%d\n", s->maxbits);
	DBG(4, " focus:		%d/%d\n", s->focus_min, s->focus_max);
	DBG(4, " resolution (x):	%d (%d-%d)\n", s->resx_optical,
	    s->resx_min, s->resx_max);
	DBG(4, " resolution (y):	%d (%d-%d)\n", s->resy_optical,
	    s->resy_min, s->resy_max);
	DBG(4, " frames:		%d\n", s->n_frames);
	DBG(4, " frame offset:	%ld\n", s->frame_offset);

	return SANE_STATUS_GOOD;
}

static SANE_Status
cs3_execute(cs3_t * s)
{
	DBG(16, "%s\n", __func__);

	cs3_scanner_ready(s, CS3_STATUS_NO_DOCS);
	cs3_init_buffer(s);
	cs3_parse_cmd(s, "c1 00 00 00 00 00");
	return cs3_issue_cmd(s);
}

static SANE_Status
cs3_issue_and_execute(cs3_t * s)
{
	SANE_Status status;

	DBG(10, "%s, opcode = %02x\n", __func__, s->send_buf[0]);

	status = cs3_issue_cmd(s);
	if (status != SANE_STATUS_GOOD)
		return status;

	return cs3_execute(s);
}

static SANE_Status
cs3_mode_select(cs3_t * s)
{
	DBG(4, "%s\n", __func__);

	cs3_scanner_ready(s, CS3_STATUS_NO_DOCS);
	cs3_init_buffer(s);

	cs3_parse_cmd(s,
		      "15 10 00 00 14 00 00 00 00 08 00 00 00 00 00 00 00 01 03 06 00 00");
	cs3_pack_word(s, s->unit_dpi);
	cs3_parse_cmd(s, "00 00");

	return cs3_issue_cmd(s);
}

static SANE_Status
cs3_load(cs3_t * s)
{
	SANE_Status status;

	DBG(6, "%s\n", __func__);

	cs3_scanner_ready(s, CS3_STATUS_NO_DOCS);
	cs3_init_buffer(s);
	cs3_parse_cmd(s, "e0 00 d1 00 00 00 00 00 0d 00");
	s->n_send += 13;

	status = cs3_grow_send_buffer(s);
	if (status != SANE_STATUS_GOOD)
		return status;

	return cs3_issue_and_execute(s);
}

static SANE_Status
cs3_eject(cs3_t * s)
{
	SANE_Status status;

	cs3_scanner_ready(s, CS3_STATUS_NO_DOCS);
	cs3_init_buffer(s);
	cs3_parse_cmd(s, "e0 00 d0 00 00 00 00 00 0d 00");
	s->n_send += 13;

	status = cs3_grow_send_buffer(s);
	if (status != SANE_STATUS_GOOD)
		return status;

	return cs3_issue_and_execute(s);
}

static SANE_Status
cs3_reset(cs3_t * s)
{
	SANE_Status status;

	cs3_scanner_ready(s, CS3_STATUS_NO_DOCS);
	cs3_init_buffer(s);
	cs3_parse_cmd(s, "e0 00 80 00 00 00 00 00 0d 00");
	s->n_send += 13;

	status = cs3_grow_send_buffer(s);
	if (status != SANE_STATUS_GOOD)
		return status;

	return cs3_issue_and_execute(s);
}


static SANE_Status
cs3_reserve_unit(cs3_t * s)
{
	DBG(10, "%s\n", __func__);

	cs3_init_buffer(s);
	cs3_parse_cmd(s, "16 00 00 00 00 00");
	return cs3_issue_cmd(s);
}

static SANE_Status
cs3_release_unit(cs3_t * s)
{
	DBG(10, "%s\n", __func__);

	cs3_init_buffer(s);
	cs3_parse_cmd(s, "17 00 00 00 00 00");
	return cs3_issue_cmd(s);
}


static SANE_Status
cs3_set_focus(cs3_t * s)
{
	DBG(6, "%s: setting focus to %d\n", __func__, s->focus);

	cs3_scanner_ready(s, CS3_STATUS_READY);
	cs3_init_buffer(s);
	cs3_parse_cmd(s, "e0 00 c1 00 00 00 00 00 09 00 00");
	cs3_pack_long(s, s->focus);
	cs3_parse_cmd(s, "00 00 00 00");

	return cs3_issue_and_execute(s);
}

static SANE_Status
cs3_read_focus(cs3_t * s)
{
	SANE_Status status;

	cs3_scanner_ready(s, CS3_STATUS_READY);
	cs3_init_buffer(s);
	cs3_parse_cmd(s, "e1 00 c1 00 00 00 00 00 0d 00");
	s->n_recv = 13;

	status = cs3_issue_cmd(s);
	if (status != SANE_STATUS_GOOD)
		return status;

	s->focus =
		65536 * (256 * s->recv_buf[1] + s->recv_buf[2]) +
		256 * s->recv_buf[3] + s->recv_buf[4];

	DBG(4, "%s: focus at %d\n", __func__, s->focus);

	return status;
}

static SANE_Status
cs3_autofocus(cs3_t * s)
{
	SANE_Status status;

	DBG(6, "%s: focusing at %ld,%ld\n", __func__,
	    s->real_focusx, s->real_focusy);

	cs3_convert_options(s);

	status = cs3_read_focus(s);
	if (status != SANE_STATUS_GOOD)
		return status;

	/* set parameter, autofocus */
	cs3_scanner_ready(s, CS3_STATUS_READY);
	cs3_init_buffer(s);
	cs3_parse_cmd(s, "e0 00 a0 00 00 00 00 00 09 00 00");
	cs3_pack_long(s, s->real_focusx);
	cs3_pack_long(s, s->real_focusy);
	/*cs3_parse_cmd(s, "00 00 00 00"); */

	status = cs3_issue_and_execute(s);
	if (status != SANE_STATUS_GOOD)
		return status;

	return cs3_read_focus(s);
}

static SANE_Status
cs3_autoexposure(cs3_t * s, int wb)
{
	SANE_Status status;

	DBG(6, "%s, wb = %d\n", __func__, wb);

	cs3_scanner_ready(s, CS3_STATUS_NO_DOCS);
	status = cs3_scan(s, wb ? CS3_SCAN_AE_WB : CS3_SCAN_AE);
	if (status != SANE_STATUS_GOOD)
		return status;

	status = cs3_get_exposure(s);
	if (status != SANE_STATUS_GOOD)
		return status;

	s->exposure = 1.;
	s->exposure_r = s->real_exposure[1] / 100.;
	s->exposure_g = s->real_exposure[2] / 100.;
	s->exposure_b = s->real_exposure[3] / 100.;

	return status;
}

static SANE_Status
cs3_get_exposure(cs3_t * s)
{
	SANE_Status status;
	int i_color, colors = s->n_colors;

	DBG(6, "%s\n", __func__);

	if ((s->type == CS3_TYPE_LS50) || (s->type == CS3_TYPE_LS5000))
		colors = 3;

	cs3_scanner_ready(s, CS3_STATUS_NO_DOCS);

	/* GET WINDOW */
	for (i_color = 0; i_color < colors; i_color++) {	/* XXXXXXXXXXXXX CCCCCCCCCCCCC */

		cs3_init_buffer(s);
		cs3_parse_cmd(s, "25 01 00 00 00");
		cs3_pack_byte(s, cs3_colors[i_color]);
		cs3_parse_cmd(s, "00 00 3a 00");
		s->n_recv = 58;
		status = cs3_issue_cmd(s);
		if (status != SANE_STATUS_GOOD)
			return status;

		s->real_exposure[cs3_colors[i_color]] =
			65536 * (256 * s->recv_buf[54] + s->recv_buf[55]) +
			256 * s->recv_buf[56] + s->recv_buf[57];

		DBG(6,
		    "%s, exposure for color %i: %li * 10ns\n",
		    __func__,
		    cs3_colors[i_color],
		    s->real_exposure[cs3_colors[i_color]]);

		DBG(6, "%02x %02x %02x %02x\n", s->recv_buf[48],
		    s->recv_buf[49], s->recv_buf[50], s->recv_buf[51]);
	}

	return SANE_STATUS_GOOD;
}

static SANE_Status
cs3_convert_options(cs3_t * s)
{
	int i_color;
	unsigned long xmin, xmax, ymin, ymax;

	DBG(4, "%s\n", __func__);

	s->real_depth = (s->preview ? 8 : s->depth);
	s->bytes_per_pixel = (s->real_depth > 8 ? 2 : 1);
	s->shift_bits = 8 * s->bytes_per_pixel - s->real_depth;

	DBG(12, " depth = %d, bpp = %d, shift = %d\n",
	    s->real_depth, s->bytes_per_pixel, s->shift_bits);

	if (s->preview) {
		s->real_resx = s->res_preview;
		s->real_resy = s->res_preview;
	} else if (s->res_independent) {
		s->real_resx = s->resx;
		s->real_resy = s->resy;
	} else {
		s->real_resx = s->res;
		s->real_resy = s->res;
	}

	s->real_pitchx = s->resx_max / s->real_resx;
	s->real_pitchy = s->resy_max / s->real_resy;

	s->real_resx = s->resx_max / s->real_pitchx;
	s->real_resy = s->resy_max / s->real_pitchy;

	DBG(12, " resx = %d, resy = %d, pitchx = %d, pitchy = %d\n",
	    s->real_resx, s->real_resy, s->real_pitchx, s->real_pitchy);

	/* The prefix "real_" refers to data in device units (1/maxdpi),
	 * "logical_" refers to resolution-dependent data.
	 */

	if (s->xmin < s->xmax) {
		xmin = s->xmin;
		xmax = s->xmax;
	} else {
		xmin = s->xmax;
		xmax = s->xmin;
	}

	if (s->ymin < s->ymax) {
		ymin = s->ymin;
		ymax = s->ymax;
	} else {
		ymin = s->ymax;
		ymax = s->ymin;
	}

	DBG(12, " xmin = %ld, xmax = %ld\n", xmin, xmax);
	DBG(12, " ymin = %ld, ymax = %ld\n", ymin, ymax);

	s->real_xoffset = xmin;
	s->real_yoffset =
		ymin + (s->i_frame - 1) * s->frame_offset +
		s->subframe / s->unit_mm;

	DBG(12, " xoffset = %ld, yoffset = %ld\n",
	    s->real_xoffset, s->real_yoffset);


	s->logical_width = (xmax - xmin + 1) / s->real_pitchx;	/* XXX use mm units */
	s->logical_height = (ymax - ymin + 1) / s->real_pitchy;
	s->real_width = s->logical_width * s->real_pitchx;
	s->real_height = s->logical_height * s->real_pitchy;

	DBG(12, " lw = %ld, lh = %ld, rw = %ld, rh = %ld\n",
	    s->logical_width, s->logical_height,
	    s->real_width, s->real_height);

	s->odd_padding = 0;
	if ((s->bytes_per_pixel == 1) && (s->logical_width & 0x01)
	    && (s->type != CS3_TYPE_LS30) && (s->type != CS3_TYPE_LS2000))
		s->odd_padding = 1;

	if (s->focus_on_centre) {
		s->real_focusx = s->real_xoffset + s->real_width / 2;
		s->real_focusy = s->real_yoffset + s->real_height / 2;
	} else {
		s->real_focusx = s->focusx;
		s->real_focusy =
			s->focusy + (s->i_frame - 1) * s->frame_offset +
			s->subframe / s->unit_mm;
	}

	DBG(12, " focusx = %ld, focusy = %ld\n",
	    s->real_focusx, s->real_focusy);

	s->real_exposure[1] = s->exposure * s->exposure_r * 100.;
	s->real_exposure[2] = s->exposure * s->exposure_g * 100.;
	s->real_exposure[3] = s->exposure * s->exposure_b * 100.;

	/* XXX IR? */
	for (i_color = 0; i_color < 3; i_color++)
		if (s->real_exposure[cs3_colors[i_color]] < 1)
			s->real_exposure[cs3_colors[i_color]] = 1;

	s->n_colors = 3;	/* XXXXXXXXXXXXXX CCCCCCCCCCCCCC */
	if (s->infrared)
		s->n_colors = 4;

	s->xfer_bytes_total =
		s->bytes_per_pixel * s->n_colors * s->logical_width *
		s->logical_height;

	if (s->preview)
		s->infrared = SANE_FALSE;

	return SANE_STATUS_GOOD;
}

static SANE_Status
cs3_set_boundary(cs3_t * s)
{
	SANE_Status status;
	int i_boundary;

	/* Ariel - Check this function */
	cs3_scanner_ready(s, CS3_STATUS_READY);
	cs3_init_buffer(s);
	cs3_parse_cmd(s, "2a 00 88 00 00 03");
	cs3_pack_byte(s, ((4 + s->n_frames * 16) >> 16) & 0xff);
	cs3_pack_byte(s, ((4 + s->n_frames * 16) >> 8) & 0xff);
	cs3_pack_byte(s, (4 + s->n_frames * 16) & 0xff);
	cs3_parse_cmd(s, "00");

	cs3_pack_byte(s, ((4 + s->n_frames * 16) >> 8) & 0xff);
	cs3_pack_byte(s, (4 + s->n_frames * 16) & 0xff);
	cs3_pack_byte(s, s->n_frames);
	cs3_pack_byte(s, s->n_frames);
	for (i_boundary = 0; i_boundary < s->n_frames; i_boundary++) {
		unsigned long lvalue = s->frame_offset * i_boundary +
			s->subframe / s->unit_mm;

		cs3_pack_long(s, lvalue);

		cs3_pack_long(s, 0);

		lvalue = s->frame_offset * i_boundary +
			s->subframe / s->unit_mm + s->frame_offset - 1;
		cs3_pack_long(s, lvalue);

		cs3_pack_long(s, s->boundaryx - 1);

	}
	status = cs3_issue_cmd(s);
	if (status != SANE_STATUS_GOOD)
		return status;

	return SANE_STATUS_GOOD;
}

static SANE_Status
cs3_send_lut(cs3_t * s)
{
	int color;
	SANE_Status status;
	cs3_pixel_t *lut, pixel;

	DBG(6, "%s\n", __func__);

	for (color = 0; color < s->n_colors; color++) {
		/*cs3_scanner_ready(s, CS3_STATUS_READY); */

		switch (color) {
		case 0:
			lut = s->lut_r;
			break;
		case 1:
			lut = s->lut_g;
			break;
		case 2:
			lut = s->lut_b;
			break;
		case 3:
			lut = s->lut_neutral;
			break;
		default:
			DBG(1,
			    "BUG: %s: Unknown color number for LUT download.\n",
			    __func__);
			return SANE_STATUS_INVAL;
			break;
		}

		cs3_init_buffer(s);
		cs3_parse_cmd(s, "2a 00 03 00");
		cs3_pack_byte(s, cs3_colors[color]);
		cs3_pack_byte(s, 2 - 1);	/* XXX number of bytes per data point - 1 */
		cs3_pack_byte(s, ((2 * s->n_lut) >> 16) & 0xff);	/* XXX 2 bytes per point */
		cs3_pack_byte(s, ((2 * s->n_lut) >> 8) & 0xff);	/* XXX 2 bytes per point */
		cs3_pack_byte(s, (2 * s->n_lut) & 0xff);	/* XXX 2 bytes per point */
		cs3_pack_byte(s, 0x00);

		for (pixel = 0; pixel < s->n_lut; pixel++) {	/* XXX 2 bytes per point */
			cs3_pack_word(s, lut[pixel]);
		}

		status = cs3_issue_cmd(s);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	return status;
}

static SANE_Status
cs3_set_window(cs3_t * s, cs3_scan_t type)
{
	int color;
	SANE_Status status = SANE_STATUS_INVAL;

	/* SET WINDOW */
	for (color = 0; color < s->n_colors; color++) {

		DBG(8, "%s: color %d\n", __func__, cs3_colors[color]);

		cs3_scanner_ready(s, CS3_STATUS_READY);

		cs3_init_buffer(s);
		if ((s->type == CS3_TYPE_LS40)
		    || (s->type == CS3_TYPE_LS4000)
		    || (s->type == CS3_TYPE_LS50)
		    || (s->type == CS3_TYPE_LS5000))
			cs3_parse_cmd(s, "24 00 00 00 00 00 00 00 3a 80");
		else
			cs3_parse_cmd(s, "24 00 00 00 00 00 00 00 3a 00");

		cs3_parse_cmd(s, "00 00 00 00 00 00 00 32");

		cs3_pack_byte(s, cs3_colors[color]);

		cs3_pack_byte(s, 0x00);

		cs3_pack_word(s, s->real_resx);
		cs3_pack_word(s, s->real_resy);
		cs3_pack_long(s, s->real_xoffset);
		cs3_pack_long(s, s->real_yoffset);
		cs3_pack_long(s, s->real_width);
		cs3_pack_long(s, s->real_height);
		cs3_pack_byte(s, 0x00);	/* brightness, etc. */
		cs3_pack_byte(s, 0x00);
		cs3_pack_byte(s, 0x00);
		cs3_pack_byte(s, 0x05);	/* image composition CCCCCCC */
		cs3_pack_byte(s, s->real_depth);	/* pixel composition */
		cs3_parse_cmd(s, "00 00 00 00 00 00 00 00 00 00 00 00 00");
		cs3_pack_byte(s, ((s->samples_per_scan - 1) << 4) | 0x00);	/* multiread, ordering */

		cs3_pack_byte(s, 0x80 | (s->negative ? 0 : 1));	/* averaging, pos/neg */

		switch (type) {	/* scanning kind */
		case CS3_SCAN_NORMAL:
			cs3_pack_byte(s, 0x01);
			break;
		case CS3_SCAN_AE:
			cs3_pack_byte(s, 0x20);
			break;
		case CS3_SCAN_AE_WB:
			cs3_pack_byte(s, 0x40);
			break;
		default:
			DBG(1, "BUG: cs3_scan(): Unknown scanning type.\n");
			return SANE_STATUS_INVAL;
		}
		if (s->samples_per_scan == 1)
			cs3_pack_byte(s, 0x02);	/* scanning mode single */
		else
			cs3_pack_byte(s, 0x10);	/* scanning mode multi */
		cs3_pack_byte(s, 0x02);	/* color interleaving */
		cs3_pack_byte(s, 0xff);	/* (ae) */
		if (color == 3)	/* infrared */
			cs3_parse_cmd(s, "00 00 00 00");	/* automatic */
		else {
			DBG(4, "%s: exposure = %ld * 10ns\n", __func__,
			    s->real_exposure[cs3_colors[color]]);
			cs3_pack_long(s, s->real_exposure[cs3_colors[color]]);
		}

		status = cs3_issue_cmd(s);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	return status;
}


static SANE_Status
cs3_scan(cs3_t * s, cs3_scan_t type)
{
	SANE_Status status;

	s->block_padding = 0;

	DBG(6, "%s, type = %d, colors = %d\n", __func__, type, s->n_colors);

	switch (type) {
	case CS3_SCAN_NORMAL:
		DBG(16, "%s: normal scan\n", __func__);
		break;
	case CS3_SCAN_AE:
		DBG(16, "%s: ae scan\n", __func__);
		break;
	case CS3_SCAN_AE_WB:
		DBG(16, "%s: ae wb scan\n", __func__);
		break;
	}

	/* wait for device to be ready with document, and set device unit */
	status = cs3_scanner_ready(s, CS3_STATUS_NO_DOCS);
	if (status != SANE_STATUS_GOOD)
		return status;

	if (s->status & CS3_STATUS_NO_DOCS)
		return SANE_STATUS_NO_DOCS;

	status = cs3_convert_options(s);
	if (status != SANE_STATUS_GOOD)
		return status;

	status = cs3_set_boundary(s);
	if (status != SANE_STATUS_GOOD)
		return status;

	cs3_set_focus(s);

	cs3_scanner_ready(s, CS3_STATUS_READY);

	if (type == CS3_SCAN_NORMAL)
		cs3_send_lut(s);

	status = cs3_set_window(s, type);
	if (status != SANE_STATUS_GOOD)
		return status;

	status = cs3_get_exposure(s);
	if (status != SANE_STATUS_GOOD)
		return status;

/*	cs3_scanner_ready(s, CS3_STATUS_READY); */

	cs3_init_buffer(s);
	switch (s->n_colors) {
	case 3:
		cs3_parse_cmd(s, "1b 00 00 00 03 00 01 02 03");
		break;
	case 4:
		cs3_parse_cmd(s, "1b 00 00 00 04 00 01 02 03 09");
		break;
	default:
		DBG(0, "BUG: %s: Unknown number of input colors.\n",
		    __func__);
		break;
	}

	status = cs3_issue_cmd(s);
	if (status != SANE_STATUS_GOOD) {
		DBG(6, "scan setup failed\n");
		return status;
	}

	if (s->status == CS3_STATUS_REISSUE) {
		status = cs3_issue_cmd(s);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	return SANE_STATUS_GOOD;
}

static void *
cs3_xmalloc(size_t size)
{
	register void *value = malloc(size);

	if (value == NULL) {
		DBG(0, "error: %s: failed to malloc() %lu bytes.\n",
		    __func__, (unsigned long) size);
	}
	return value;
}

static void *
cs3_xrealloc(void *p, size_t size)
{
	register void *value;

	if (!size)
		return p;

	value = realloc(p, size);

	if (value == NULL) {
		DBG(0, "error: %s: failed to realloc() %lu bytes.\n",
		    __func__, (unsigned long) size);
	}

	return value;
}

static void
cs3_xfree(const void *p)
{
	if (p)
		free(p);
}
