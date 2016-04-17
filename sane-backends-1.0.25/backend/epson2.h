/*
 * epson2.h - SANE library for Epson scanners.
 *
 * Based on Kazuhiro Sasayama previous
 * Work on epson.[ch] file from the SANE package.
 * Please see those files for original copyrights.
 *
 * Copyright (C) 2006 Tower Technologies
 * Author: Alessandro Zummo <a.zummo@towertech.it>
 *
 * This file is part of the SANE package.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2.
 */

#ifndef epson2_h
#define epson2_h

#undef BACKEND_NAME
#define BACKEND_NAME epson2
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

#ifdef __GNUC__
#define __func__ __FUNCTION__
#else
#define __func__ "(undef)"
/* I cast my vote for C99... :) */
#endif

#define EPSON2_CONFIG_FILE "epson2.conf"

#ifndef PATH_MAX
#define PATH_MAX (1024)
#endif

#ifndef XtNumber
#define XtNumber(x)  (sizeof(x) / sizeof(x[0]))
#define XtOffset(p_type, field)  ((size_t)&(((p_type)NULL)->field))
#define XtOffsetOf(s_type, field)  XtOffset(s_type*, field)
#endif

#define NUM_OF_HEX_ELEMENTS (16)        /* number of hex numbers per line for data dump */
#define DEVICE_NAME_LEN (16)    /* length of device name in extended status */


/* string constants for GUI elements that are not defined SANE-wide */

#define SANE_NAME_GAMMA_CORRECTION "gamma-correction"
#define SANE_TITLE_GAMMA_CORRECTION SANE_I18N("Gamma Correction")
#define SANE_DESC_GAMMA_CORRECTION SANE_I18N("Selects the gamma correction value from a list of pre-defined devices or the user defined table, which can be downloaded to the scanner")

#define SANE_EPSON_FOCUS_NAME "focus-position"
#define SANE_EPSON_FOCUS_TITLE SANE_I18N("Focus Position")
#define SANE_EPSON_FOCUS_DESC SANE_I18N("Sets the focus position to either the glass or 2.5mm above the glass")
#define SANE_EPSON_WAIT_FOR_BUTTON_NAME "wait-for-button"
#define SANE_EPSON_WAIT_FOR_BUTTON_TITLE SANE_I18N("Wait for Button")
#define SANE_EPSON_WAIT_FOR_BUTTON_DESC SANE_I18N("After sending the scan command, wait until the button on the scanner is pressed to actually start the scan process.");

/* misc constants */

#define LINES_SHUFFLE_MAX	17	/* 2 x 8 lines plus 1 */
#define SANE_EPSON_MAX_RETRIES	14	/* warmup max retry */
#define CMD_SIZE_EXT_STATUS	42

/* NOTE: you can find these codes with "man ascii". */
#define STX	0x02
#define ACK	0x06
#define NAK	0x15
#define CAN	0x18
#define ESC	0x1B
#define PF	0x19
#define FS	0x1C

#define	S_ACK	"\006"
#define	S_CAN	"\030"

/* status bits */

#define STATUS_FER		0x80	/* fatal error */
#define STATUS_NOT_READY	0x40	/* scanner is in use on another interface */
#define STATUS_AREA_END		0x20	/* area end */
#define STATUS_OPTION		0x10	/* option installed */
#define STATUS_EXT_COMMANDS	0x02	/* scanners supports extended commands */
#define STATUS_RESERVED		0x01	/* this should be always 0 */

#define EXT_STATUS_FER		0x80	/* fatal error */
#define EXT_STATUS_FBF		0x40	/* flat bed scanner */
#define EXT_STATUS_ADFT		0x20	/* page type ADF */
#define EXT_STATUS_ADFS		0x10	/* ADF is duplex capable */
#define EXT_STATUS_ADFO		0x08	/* ADF loads from the first sheet (page type only) */
#define EXT_STATUS_LID		0x04	/* lid is open */
#define EXT_STATUS_WU		0x02	/* warming up */
#define EXT_STATUS_PB		0x01	/* scanner has a push button */

#define EXT_STATUS_IST		0x80	/* option detected */
#define EXT_STATUS_EN		0x40	/* option enabled */
#define EXT_STATUS_ERR		0x20	/* other error */
#define EXT_STATUS_PE		0x08	/* no paper */
#define EXT_STATUS_PJ		0x04	/* paper jam */
#define EXT_STATUS_OPN		0x02	/* cover open */

#define EXT_IDTY_CAP1_DLF	0x80
#define EXT_IDTY_CAP1_NOTFBF	0x40	/* not a flat bed scanner */
#define EXT_IDTY_CAP1_ADFT	0x20	/* page type ADF ? */
#define EXT_IDTY_CAP1_ADFS	0x10	/* ADF is duplex capable */
#define EXT_IDTY_CAP1_ADFO	0x08	/* ADF loads from the first sheet (page type only) */
#define EXT_IDTY_CAP1_LID	0x04	/* lid type option ? */
#define EXT_IDTY_CAP1_TPIR	0x02	/* TPU with infrared */
#define EXT_IDTY_CAP1_PB	0x01	/* scanner has a push button */

#define EXT_IDTY_CAP2_AFF	0x04	/* auto form feed */
#define EXT_IDTY_CAP2_DFD	0x08	/* double feed detection */
#define EXT_IDTY_CAP2_ADFAS	0x10	/* ADF with auto scan support */

#define FSF_STATUS_MAIN_FER	0x80	/* system error */
#define FSF_STATUS_MAIN_NR	0x40	/* not ready */
#define FSF_STATUS_MAIN_WU	0x02	/* warming up */
#define FSF_STATUS_MAIN_CWU	0x01	/* warm up can be cancelled (?) */

#define FSF_STATUS_ADF_IST	0x80	/* installed */
#define FSF_STATUS_ADF_EN	0x40	/* enabled */
#define FSF_STATUS_ADF_ERR	0x20	/* system error */
#define FSF_STATUS_ADF_PE	0x08	/* paper empty */
#define FSF_STATUS_ADF_PJ	0x04	/* paper jam */
#define FSF_STATUS_ADF_OPN	0x02	/* cover open */
#define FSF_STATUS_ADF_PAG	0x01	/* duplex */

#define FSF_STATUS_TPU_IST	0x80	/* installed */
#define FSF_STATUS_TPU_EN	0x40	/* enabled */
#define FSF_STATUS_TPU_ERR	0x20	/* system error */
#define FSF_STATUS_TPU_OPN	0x02	/* cover open */

#define FSF_STATUS_MAIN2_ERR	0x20	/* system error */
#define FSF_STATUS_MAIN2_PE	0x08	/* paper empty */
#define FSF_STATUS_MAIN2_PJ	0x04	/* paper jam */
#define FSF_STATUS_MAIN2_OPN	0x02	/* cover open */

#define FSG_STATUS_FER		0x80
#define FSG_STATUS_NOT_READY	0x40	/* in use via other interface */
#define FSG_STATUS_CANCEL_REQ	0x10	/* cancel request from scanner */

#define EPSON_LEVEL_A1		 0
#define EPSON_LEVEL_A2		 1
#define EPSON_LEVEL_B1		 2
#define	EPSON_LEVEL_B2		 3
#define	EPSON_LEVEL_B3		 4
#define	EPSON_LEVEL_B4		 5
#define	EPSON_LEVEL_B5		 6
#define	EPSON_LEVEL_B6		 7
#define	EPSON_LEVEL_B7		 8
#define	EPSON_LEVEL_B8		 9
#define	EPSON_LEVEL_F5		10
#define EPSON_LEVEL_D1		11
#define EPSON_LEVEL_D7		12
#define EPSON_LEVEL_D8		13

/* there is also a function level "A5", which I'm igoring here until somebody can
 * convince me that this is still needed. The A5 level was for the GT-300, which
 * was (is) a monochrome only scanner. So if somebody really wants to use this
 * scanner with SANE get in touch with me and we can work something out - khk
 */

#define	EPSON_LEVEL_DEFAULT EPSON_LEVEL_B3

struct EpsonCmd
{
        char *level;

        unsigned char request_identity;
	unsigned char request_identity2; /* new request identity level Dx */
	unsigned char request_status;
	unsigned char request_condition;
	unsigned char set_color_mode;
	unsigned char start_scanning;
	unsigned char set_data_format;
	unsigned char set_resolution;
	unsigned char set_zoom;
	unsigned char set_scan_area;
	unsigned char set_bright;
	SANE_Range bright_range;
	unsigned char set_gamma;
	unsigned char set_halftoning;
	unsigned char set_color_correction;
	unsigned char initialize_scanner;
	unsigned char set_speed;	/* B4 and later */
	unsigned char set_lcount;
	unsigned char mirror_image;	/* B5 and later */
	unsigned char set_gamma_table;	/* B4 and later */
	unsigned char set_outline_emphasis; /* B4 and later */
	unsigned char set_dither;	/* B4 and later */
	unsigned char set_color_correction_coefficients; /* B3 and later */
	unsigned char request_extended_status;	/* get extended status from scanner */
	unsigned char control_an_extension;	/* for extension control */
	unsigned char eject;			/* for extension control */
	unsigned char feed;
	unsigned char request_push_button_status;
	unsigned char control_auto_area_segmentation;
	unsigned char set_film_type;		/* for extension control */
	unsigned char set_exposure_time;	/* F5 only */
	unsigned char set_bay;			/* F5 only */
	unsigned char set_threshold;
	unsigned char set_focus_position;	/* B8 only */
	unsigned char request_focus_position;	/* B8 only */
	unsigned char request_extended_identity;
	unsigned char request_scanner_status;
};

enum {
	OPT_NUM_OPTS = 0,
	OPT_MODE_GROUP,
	OPT_MODE,
	OPT_BIT_DEPTH,
	OPT_HALFTONE,
	OPT_DROPOUT,
	OPT_BRIGHTNESS,
	OPT_SHARPNESS,
	OPT_GAMMA_CORRECTION,
	OPT_COLOR_CORRECTION,
	OPT_RESOLUTION,
	OPT_THRESHOLD,
	OPT_ADVANCED_GROUP,
	OPT_MIRROR,
	OPT_AAS,
	OPT_GAMMA_VECTOR_R,
	OPT_GAMMA_VECTOR_G,
	OPT_GAMMA_VECTOR_B,
	OPT_WAIT_FOR_BUTTON,
	OPT_CCT_GROUP,
	OPT_CCT_MODE,
	OPT_CCT_PROFILE,
	OPT_PREVIEW_GROUP,
	OPT_PREVIEW,
	OPT_GEOMETRY_GROUP,
	OPT_TL_X,
	OPT_TL_Y,
	OPT_BR_X,
	OPT_BR_Y,
	OPT_EQU_GROUP,
	OPT_SOURCE,
	OPT_AUTO_EJECT,
	OPT_FILM_TYPE,
	OPT_FOCUS,
	OPT_BAY,
	OPT_EJECT,
	OPT_ADF_MODE,
	NUM_OPTIONS
};

typedef enum
{	/* hardware connection to the scanner */
	SANE_EPSON_NODEV,	/* default, no HW specified yet */
	SANE_EPSON_SCSI,	/* SCSI interface */
	SANE_EPSON_PIO,		/* parallel interface */
	SANE_EPSON_USB,		/* USB interface */
	SANE_EPSON_NET		/* network interface */
} Epson_Connection_Type;

struct epson_profile
{
	unsigned int	model;
	double		cct[4][9];
};

enum {
	CCTP_REFLECTIVE = 0, CCTP_COLORNEG,
	CCTP_MONONEG, CCTP_COLORPOS
};

struct epson_profile_map
{
        char		*name;
        unsigned int	id;
};

extern const struct epson_profile epson_cct_profiles[];
extern const struct epson_profile_map epson_cct_models[];

/* hardware description */

struct Epson_Device
{
	struct Epson_Device *next;

	char *name;
	char *model;

	unsigned int model_id;

	SANE_Device sane;
	SANE_Int level;
	SANE_Range dpi_range;

	SANE_Range *x_range;	        /* x range w/out extension */
	SANE_Range *y_range;	        /* y range w/out extension */

	SANE_Range fbf_x_range;	        /* flattbed x range */
	SANE_Range fbf_y_range;	        /* flattbed y range */
	SANE_Range adf_x_range;	        /* autom. document feeder x range */
	SANE_Range adf_y_range;	        /* autom. document feeder y range */
	SANE_Range tpu_x_range;	        /* transparency unit x range */
	SANE_Range tpu_y_range;	        /* transparency unit y range */
	SANE_Range tpu2_x_range;	/* transparency unit 2 x range */
	SANE_Range tpu2_y_range;	/* transparency unit 2 y range */

	Epson_Connection_Type connection;

	SANE_Int *res_list;		/* list of resolutions */
	SANE_Int res_list_size;		/* number of entries in this list */
	SANE_Int last_res;		/* last selected resolution */
	SANE_Int last_res_preview;	/* last selected preview resolution */

	SANE_Word *resolution_list;	/* for display purposes we store a second copy */

	SANE_Bool extension;		/* extension is installed */
	SANE_Int use_extension;		/* use the installed extension */
	SANE_Bool TPU;			/* TPU is installed */
	SANE_Bool TPU2;			/* TPU2 is installed */
	SANE_Bool ADF;			/* ADF is installed */
	SANE_Bool duplex;		/* does the ADF handle duplex scanning */
	SANE_Bool focusSupport;		/* does this scanner have support for "set focus position" ? */
	SANE_Bool color_shuffle;	/* does this scanner need color shuffling */

	SANE_Int maxDepth;		/* max. color depth */
	SANE_Int *depth_list;

	SANE_Int optical_res;		/* optical resolution */
	SANE_Int max_line_distance;

	SANE_Bool need_double_vertical;
	SANE_Bool need_color_reorder;
	SANE_Bool need_reset_on_source_change;

	SANE_Bool wait_for_button;	/* do we have to wait until the scanner button is pressed? */

	SANE_Bool extended_commands;

	struct EpsonCmd *cmd;
	const struct epson_profile *cct_profile;
};

typedef struct Epson_Device Epson_Device;

/* an instance of a scanner */

struct Epson_Scanner
{
	struct Epson_Scanner *next;
	struct Epson_Device *hw;

	int fd;

	SANE_Option_Descriptor opt[NUM_OPTIONS];
	Option_Value val[NUM_OPTIONS];
	SANE_Parameters params;

	SANE_Bool block;
	SANE_Bool eof;
	SANE_Byte *buf, *end, *ptr;
	SANE_Bool canceling;
	SANE_Word gamma_table[3][256];
	SANE_Word cct_table[9];
	SANE_Int retry_count;

	/* buffer lines for color shuffling */
	SANE_Byte *line_buffer[LINES_SHUFFLE_MAX];
	SANE_Int color_shuffle_line;	/* current line number for color shuffling */
	SANE_Int line_distance;		/* current line distance */
	SANE_Int current_output_line;	/* line counter when color shuffling */
	SANE_Int lines_written;		/* debug variable */

	SANE_Int left, top, lcount;
	SANE_Bool focusOnGlass;
	SANE_Byte currentFocusPosition;

	/* network buffers */
	unsigned char *netbuf, *netptr;
	size_t netlen;

	/* extended image data handshaking */
	SANE_Int ext_block_len;
	SANE_Int ext_last_len;
	SANE_Int ext_blocks;
	SANE_Int ext_counter;
};

typedef struct Epson_Scanner Epson_Scanner;

struct mode_param
{
	int color;
	int flags;
	int dropout_mask;
	int depth;
};

enum {
	MODE_BINARY, MODE_GRAY, MODE_COLOR, MODE_INFRARED
};

#endif
