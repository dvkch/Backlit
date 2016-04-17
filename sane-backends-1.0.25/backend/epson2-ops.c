/*
 * epson2.c - SANE library for Epson scanners.
 *
 * Based on Kazuhiro Sasayama previous
 * Work on epson.[ch] file from the SANE package.
 * Please see those files for additional copyrights.
 *
 * Copyright (C) 2006-09 Tower Technologies
 * Author: Alessandro Zummo <a.zummo@towertech.it>
 *
 * This file is part of the SANE package.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2.
 */

#define DEBUG_DECLARE_ONLY

#include "sane/config.h"

#include <unistd.h>		/* sleep */
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include "byteorder.h"

#include "epson2.h"
#include "epson2-ops.h"

#include "epson2-io.h"
#include "epson2-commands.h"

/*
 *       request identity
 *       |   request identity2
 *       |   |   request status
 *       |   |   |   request condition
 *       |   |   |   |   set color mode
 *       |   |   |   |   |   start scanning
 *       |   |   |   |   |   |   set data format
 *       |   |   |   |   |   |   |   set resolution
 *       |   |   |   |   |   |   |   |   set zoom
 *       |   |   |   |   |   |   |   |   |   set scan area
 *       |   |   |   |   |   |   |   |   |   |   set brightness
 *       |   |   |   |   |   |   |   |   |   |   |              set gamma
 *       |   |   |   |   |   |   |   |   |   |   |              |   set halftoning
 *       |   |   |   |   |   |   |   |   |   |   |              |   |   set color correction
 *       |   |   |   |   |   |   |   |   |   |   |              |   |   |   initialize scanner
 *       |   |   |   |   |   |   |   |   |   |   |              |   |   |   |   set speed
 *       |   |   |   |   |   |   |   |   |   |   |              |   |   |   |   |   set lcount
 *       |   |   |   |   |   |   |   |   |   |   |              |   |   |   |   |   |   mirror image
 *       |   |   |   |   |   |   |   |   |   |   |              |   |   |   |   |   |   |   set gamma table
 *       |   |   |   |   |   |   |   |   |   |   |              |   |   |   |   |   |   |   |   set outline emphasis
 *       |   |   |   |   |   |   |   |   |   |   |              |   |   |   |   |   |   |   |   |   set dither
 *       |   |   |   |   |   |   |   |   |   |   |              |   |   |   |   |   |   |   |   |   |   set color correction coefficients
 *       |   |   |   |   |   |   |   |   |   |   |              |   |   |   |   |   |   |   |   |   |   |   request extension status
 *       |   |   |   |   |   |   |   |   |   |   |              |   |   |   |   |   |   |   |   |   |   |   |   control an extension
 *       |   |   |   |   |   |   |   |   |   |   |              |   |   |   |   |   |   |   |   |   |   |   |   |    forward feed / eject
 *       |   |   |   |   |   |   |   |   |   |   |              |   |   |   |   |   |   |   |   |   |   |   |   |    |   feed
 *       |   |   |   |   |   |   |   |   |   |   |              |   |   |   |   |   |   |   |   |   |   |   |   |    |   |     request push button status
 *       |   |   |   |   |   |   |   |   |   |   |              |   |   |   |   |   |   |   |   |   |   |   |   |    |   |     |   control auto area segmentation
 *       |   |   |   |   |   |   |   |   |   |   |              |   |   |   |   |   |   |   |   |   |   |   |   |    |   |     |   |   set film type
 *       |   |   |   |   |   |   |   |   |   |   |              |   |   |   |   |   |   |   |   |   |   |   |   |    |   |     |   |   |   set exposure time
 *       |   |   |   |   |   |   |   |   |   |   |              |   |   |   |   |   |   |   |   |   |   |   |   |    |   |     |   |   |   |   set bay
 *       |   |   |   |   |   |   |   |   |   |   |              |   |   |   |   |   |   |   |   |   |   |   |   |    |   |     |   |   |   |   |   set threshold
 *       |   |   |   |   |   |   |   |   |   |   |              |   |   |   |   |   |   |   |   |   |   |   |   |    |   |     |   |   |   |   |   |   set focus position
 *       |   |   |   |   |   |   |   |   |   |   |              |   |   |   |   |   |   |   |   |   |   |   |   |    |   |     |   |   |   |   |   |   |   request focus position 
 *       |   |   |   |   |   |   |   |   |   |   |              |   |   |   |   |   |   |   |   |   |   |   |   |    |   |     |   |   |   |   |   |   |   |   request extended identity
 *       |   |   |   |   |   |   |   |   |   |   |              |   |   |   |   |   |   |   |   |   |   |   |   |    |   |     |   |   |   |   |   |   |   |   |   request scanner status
 *       |   |   |   |   |   |   |   |   |   |   |              |   |   |   |   |   |   |   |   |   |   |   |   |    |   |     |   |   |   |   |   |   |   |   |   |
 *       |   |   |   |   |   |   |   |   |   |   |              |   |   |   |   |   |   |   |   |   |   |   |   |    |   |     |   |   |   |   |   |   |   |   |   |
 */

static struct EpsonCmd epson_cmd[] = {
  {"A1",'I', 0 ,'F','S', 0 ,'G', 0 ,'R', 0 ,'A', 0 ,{ 0, 0, 0}, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,  0 , 0   , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 },
  {"A2",'I', 0 ,'F','S', 0 ,'G','D','R','H','A','L',{-3, 3, 0},'Z','B', 0 ,'@', 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,  0 , 0   , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 },
  {"B1",'I', 0 ,'F','S','C','G','D','R', 0 ,'A', 0 ,{ 0, 0, 0}, 0 ,'B', 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,  0 , 0   , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 },
  {"B2",'I', 0 ,'F','S','C','G','D','R','H','A','L',{-3, 3, 0},'Z','B', 0 ,'@', 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,  0 , 0   , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 },
  {"B3",'I', 0 ,'F','S','C','G','D','R','H','A','L',{-3, 3, 0},'Z','B','M','@', 0 , 0 , 0 , 0 , 0 , 0 ,'m','f','e',  0 , 0   , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 },
  {"B4",'I', 0 ,'F','S','C','G','D','R','H','A','L',{-3, 3, 0},'Z','B','M','@','g','d', 0 ,'z','Q','b','m','f','e',  0 , 0   , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 },
  {"B5",'I', 0 ,'F','S','C','G','D','R','H','A','L',{-3, 3, 0},'Z','B','M','@','g','d','K','z','Q','b','m','f','e',  0 , 0   , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 },
  {"B6",'I', 0 ,'F','S','C','G','D','R','H','A','L',{-3, 3, 0},'Z','B','M','@','g','d','K','z','Q','b','m','f','e',  0 , 0   , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 },
  {"B7",'I', 0 ,'F','S','C','G','D','R','H','A','L',{-4, 3, 0},'Z','B','M','@','g','d','K','z','Q','b','m','f','e','\f', 0   ,'!','s','N', 0 , 0 ,'t', 0 , 0 ,'I','F'},
  {"B8",'I', 0 ,'F','S','C','G','D','R','H','A','L',{-4, 3, 0},'Z','B','M','@','g','d','K','z','Q','b','m','f','e','\f', 0x19,'!','s','N', 0 , 0 ,'t','p','q','I','F'},
/* XXX 'f' probably not supported on F5 */
  {"F5",'I', 0 ,'F','S','C','G','D','R','H','A','L',{-3, 3, 0},'Z', 0 ,'M','@','g','d','K','z','Q', 0 ,'m','f','e','\f', 0   , 0 , 0 ,'N','T','P', 0 , 0 , 0 , 0 , 0 },
  {"D1",'I','i','F', 0 ,'C','G','D','R', 0 ,'A', 0 ,{ 0, 0, 0},'Z', 0 , 0 ,'@','g','d', 0 ,'z', 0 , 0 , 0 ,'f', 0 ,  0 , 0   ,'!', 0 , 0 , 0 , 0 ,'t', 0 , 0 , 0 , 0 },
  {"D2",'I','i','F', 0 ,'C','G','D','R', 0 ,'A', 0 ,{ 0, 0, 0},'Z', 0 , 0 ,'@','g','d', 0 ,'z', 0 , 0 , 0 ,'f','e',  0 , 0   ,'!', 0 ,'N', 0 , 0 ,'t', 0 , 0 , 0 , 0 },
  {"D7",'I','i','F', 0 ,'C','G','D','R', 0 ,'A', 0 ,{ 0, 0, 0},'Z', 0 , 0 ,'@','g','d', 0 ,'z', 0 , 0 , 0 ,'f','e','\f', 0   ,'!', 0 ,'N', 0 , 0 ,'t', 0 , 0 , 0 , 0 },
  {"D8",'I','i','F', 0 ,'C','G','D','R', 0 ,'A', 0 ,{ 0, 0, 0},'Z', 0 , 0 ,'@','g','d', 0 ,'z', 0 , 0 , 0 ,'f','e','\f', 0   ,'!', 0 ,'N', 0 , 0 ,'t', 0 , 0 , 0 , 0 },
};



extern struct mode_param mode_params[];

/* Define the different scan sources */

#define FBF_STR	SANE_I18N("Flatbed")
#define TPU_STR	SANE_I18N("Transparency Unit")
#define TP2_STR SANE_I18N("TPU8x10")
#define ADF_STR	SANE_I18N("Automatic Document Feeder")

/*
 * source list need one dummy entry (save device settings is crashing).
 * NOTE: no const - this list gets created while exploring the capabilities
 * of the scanner.
 */

extern SANE_String_Const source_list[];

static int film_params[] = { 0, 1, 2, 3 };

extern const int halftone_params[];

static const int dropout_params[] = {
	0x00,	/* none */
	0x10,	/* red */
	0x20,	/* green */
	0x30	/* blue */
};

/*
 * Color correction:
 * One array for the actual parameters that get sent to the scanner (color_params[]),
 * one array for the strings that get displayed in the user interface (correction_list[])
 * and one array to mark the user defined color correction (correction_userdefined[]).
 */
static const int correction_params[] = {
	0x00,	/* None */
	0x01,	/* Auto */
	0x01,	/* User defined */
};

void
e2_dev_init(Epson_Device *dev, const char *devname, int conntype)
{
	DBG(5, "%s\n", __func__);

	dev->name = NULL;
	dev->model = NULL;
	dev->connection = conntype;

	dev->model_id = 0;

	dev->sane.name = devname;
	dev->sane.model = NULL;

	dev->sane.type = "flatbed scanner";
	dev->sane.vendor = "Epson";

	dev->optical_res = 0;	/* just to have it initialized */
	dev->color_shuffle = SANE_FALSE;
	dev->extension = SANE_FALSE;
	dev->use_extension = SANE_FALSE;

	dev->need_color_reorder = SANE_FALSE;
	dev->need_double_vertical = SANE_FALSE;

	dev->cct_profile = &epson_cct_profiles[0]; /* default profile */
	
	dev->cmd = &epson_cmd[EPSON_LEVEL_DEFAULT];

	/* Change default level when using a network connection */
	if (dev->connection == SANE_EPSON_NET)
		dev->cmd = &epson_cmd[EPSON_LEVEL_B7];

	dev->last_res = 0;
	dev->last_res_preview = 0;	/* set resolution to safe values */

	dev->res_list_size = 0;
	dev->res_list = NULL;
}

SANE_Status
e2_dev_post_init(struct Epson_Device *dev)
{
	int i, last;

	DBG(5, "%s\n", __func__);

	/* find cct model id */
	for (i = 0; epson_cct_models[i].name != NULL; i++) {
		if (strcmp(epson_cct_models[i].name, dev->model) == 0) {
			dev->model_id = epson_cct_models[i].id;
			break;
		}
	}

	/* find cct profile */
	for (i = 0; epson_cct_profiles[i].model != 0xFF; i++) {
		if (epson_cct_profiles[i].model == dev->model_id) {
			dev->cct_profile = &epson_cct_profiles[i];
			break;
		}
	}

	DBG(1, "CCT model id is 0x%02x, profile offset %d\n", dev->model_id, i);

	/* If we have been unable to obtain supported resolutions
	 * due to the fact we are on the network transport,
	 * add some convenient ones
	 */

	if (dev->res_list_size == 0) {

		int val = (dev->dpi_range.min < 150) ? 150 : dev->dpi_range.min;

		DBG(1, "cannot obtain resolution list, faking (%d-%d)\n",
			dev->dpi_range.min, dev->dpi_range.max);

		if (dev->dpi_range.min <= 25)
			e2_add_resolution(dev, 25);

		if (dev->dpi_range.min <= 50)
			e2_add_resolution(dev, 50);

		if (dev->dpi_range.min <= 75)
			e2_add_resolution(dev, 75);

		if (dev->dpi_range.min <= 100)
			e2_add_resolution(dev, 100);

		while (val <= dev->dpi_range.max) {
			e2_add_resolution(dev, val);
			val *= 2;
		}
	}

	/* try to expand the resolution list where appropriate */

	last = dev->res_list[dev->res_list_size - 1];

	DBG(1, "highest available resolution: %d\n", last);

	if (dev->optical_res > last) {
		DBG(1, "adding optical resolution (%d)\n", dev->optical_res);
		e2_add_resolution(dev, dev->optical_res);
	}

	/* add missing resolutions for known scanners */

	if (e2_dev_model(dev, "GT-X800") || e2_dev_model(dev, "GT-X700")) {

		DBG(1, "known scanner, integrating resolution list\n");
		e2_add_resolution(dev, 4800);
		e2_add_resolution(dev, 6400);
		e2_add_resolution(dev, 9600);
		e2_add_resolution(dev, 12800);

		last = dev->res_list[dev->res_list_size - 1];
	}

	/* guess for the others */

	if (dev->dpi_range.max > last && dev->dpi_range.max != dev->optical_res) {

		int val = last + last;

		DBG(1, "integrating resolution list (%d-%d)\n",
			val, dev->dpi_range.max);

		while (val <= dev->dpi_range.max) {
			e2_add_resolution(dev, val);
			val += last;
		}
	}


	/*
	 * Copy the resolution list to the resolution_list array so that the frontend can
	 * display the correct values
	 */

	dev->resolution_list =
		malloc((dev->res_list_size + 1) * sizeof(SANE_Word));

	if (dev->resolution_list == NULL)
		return SANE_STATUS_NO_MEM;


	*(dev->resolution_list) = dev->res_list_size;

	memcpy(&(dev->resolution_list[1]), dev->res_list,
	       dev->res_list_size * sizeof(SANE_Word));


	/* establish defaults */
	dev->need_reset_on_source_change = SANE_FALSE;

	if (e2_dev_model(dev, "ES-9000H") || e2_dev_model(dev, "GT-30000")) {
		dev->cmd->set_focus_position = 0;
		dev->cmd->feed = 0x19;
	}

	if (e2_dev_model(dev, "GT-8200") || e2_dev_model(dev, "Perfection1650")
	    || e2_dev_model(dev, "Perfection1640") || e2_dev_model(dev, "GT-8700")) {
		dev->cmd->feed = 0;
		dev->cmd->set_focus_position = 0;
		dev->need_reset_on_source_change = SANE_TRUE;
	}

	return SANE_STATUS_GOOD;
}


SANE_Bool
e2_dev_model(Epson_Device *dev, const char *model)
{
	if (dev->model == NULL)
		return SANE_FALSE;

	if (strncmp(dev->model, model, strlen(model)) == 0)
		return SANE_TRUE;

	return SANE_FALSE;
}

void
e2_set_cmd_level(SANE_Handle handle, unsigned char *level)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;
	Epson_Device *dev = s->hw;

	int n;

	DBG(1, "%s: %c%c\n", __func__, level[0], level[1]);

	/* set command type and level */
	for (n = 0; n < NELEMS(epson_cmd); n++) {
		char type_level[3];
		sprintf(type_level, "%c%c", level[0], level[1]);
		if (!strncmp(type_level, epson_cmd[n].level, 2))
			break;
	}

	if (n < NELEMS(epson_cmd)) {
		dev->cmd = &epson_cmd[n];
	} else {
		dev->cmd = &epson_cmd[EPSON_LEVEL_DEFAULT];
		DBG(1, " unknown type %c or level %c, using %s\n",
		    level[0], level[1], dev->cmd->level);
	}

	s->hw->level = dev->cmd->level[1] - '0';
}

SANE_Status
e2_set_model(Epson_Scanner * s, unsigned char *model, size_t len)
{
	unsigned char *buf;
	unsigned char *p;
	struct Epson_Device *dev = s->hw;

	buf = malloc(len + 1);
	if (buf == NULL)
		return SANE_STATUS_NO_MEM;

	memcpy(buf, model, len);
	buf[len] = '\0';

	p = &buf[len - 1];

	while (*p == ' ') {
		*p = '\0';
		p--;
	}

	if (dev->model)
		free(dev->model);

	dev->model = strndup((const char *) buf, len);
	dev->sane.model = dev->model;

	DBG(10, "%s: model is '%s'\n", __func__, dev->model);

	free(buf);

	return SANE_STATUS_GOOD;
}

SANE_Status
e2_add_resolution(Epson_Device *dev, int r)
{
	dev->res_list_size++;
	dev->res_list = (SANE_Int *) realloc(dev->res_list,
					     dev->res_list_size *
					     sizeof(SANE_Word));

	DBG(10, "%s: add (dpi): %d\n", __func__, r);

	if (dev->res_list == NULL)
		return SANE_STATUS_NO_MEM;

	dev->res_list[dev->res_list_size - 1] = (SANE_Int) r;

	return SANE_STATUS_GOOD;
}

void
e2_set_fbf_area(Epson_Scanner * s, int x, int y, int unit)
{
	struct Epson_Device *dev = s->hw;

	if (x == 0 || y == 0)
		return;

	dev->fbf_x_range.min = 0;
	dev->fbf_x_range.max = SANE_FIX(x * MM_PER_INCH / unit);
	dev->fbf_x_range.quant = 0;

	dev->fbf_y_range.min = 0;
	dev->fbf_y_range.max = SANE_FIX(y * MM_PER_INCH / unit);
	dev->fbf_y_range.quant = 0;

	DBG(5, "%s: %f,%f %f,%f %d [mm]\n",
	    __func__,
	    SANE_UNFIX(dev->fbf_x_range.min),
	    SANE_UNFIX(dev->fbf_y_range.min),
	    SANE_UNFIX(dev->fbf_x_range.max),
	    SANE_UNFIX(dev->fbf_y_range.max), unit);
}

void
e2_set_adf_area(struct Epson_Scanner *s, int x, int y, int unit)
{
	struct Epson_Device *dev = s->hw;

	dev->adf_x_range.min = 0;
	dev->adf_x_range.max = SANE_FIX(x * MM_PER_INCH / unit);
	dev->adf_x_range.quant = 0;

	dev->adf_y_range.min = 0;
	dev->adf_y_range.max = SANE_FIX(y * MM_PER_INCH / unit);
	dev->adf_y_range.quant = 0;

	DBG(5, "%s: %f,%f %f,%f %d [mm]\n",
	    __func__,
	    SANE_UNFIX(dev->adf_x_range.min),
	    SANE_UNFIX(dev->adf_y_range.min),
	    SANE_UNFIX(dev->adf_x_range.max),
	    SANE_UNFIX(dev->adf_y_range.max), unit);
}

void
e2_set_tpu_area(struct Epson_Scanner *s, int x, int y, int unit)
{
	struct Epson_Device *dev = s->hw;

	dev->tpu_x_range.min = 0;
	dev->tpu_x_range.max = SANE_FIX(x * MM_PER_INCH / unit);
	dev->tpu_x_range.quant = 0;

	dev->tpu_y_range.min = 0;
	dev->tpu_y_range.max = SANE_FIX(y * MM_PER_INCH / unit);
	dev->tpu_y_range.quant = 0;

	DBG(5, "%s: %f,%f %f,%f %d [mm]\n",
	    __func__,
	    SANE_UNFIX(dev->tpu_x_range.min),
	    SANE_UNFIX(dev->tpu_y_range.min),
	    SANE_UNFIX(dev->tpu_x_range.max),
	    SANE_UNFIX(dev->tpu_y_range.max), unit);
}

void
e2_set_tpu2_area(struct Epson_Scanner *s, int x, int y, int unit)
{
	struct Epson_Device *dev = s->hw;

	dev->tpu2_x_range.min = 0;
	dev->tpu2_x_range.max = SANE_FIX(x * MM_PER_INCH / unit);
	dev->tpu2_x_range.quant = 0;

	dev->tpu2_y_range.min = 0;
	dev->tpu2_y_range.max = SANE_FIX(y * MM_PER_INCH / unit);
	dev->tpu2_y_range.quant = 0;

	DBG(5, "%s: %f,%f %f,%f %d [mm]\n",
	    __func__,
	    SANE_UNFIX(dev->tpu2_x_range.min),
	    SANE_UNFIX(dev->tpu2_y_range.min),
	    SANE_UNFIX(dev->tpu2_x_range.max),
	    SANE_UNFIX(dev->tpu2_y_range.max), unit);
}

void
e2_add_depth(Epson_Device *dev, SANE_Int depth)
{
	DBG(10, "%s: add (bpp): %d\n", __func__, depth);

	if (depth > dev->maxDepth)
		dev->maxDepth = depth;

	dev->depth_list[0]++;
	dev->depth_list[dev->depth_list[0]] = depth;
}

/* A little helper function to correct the extended status reply
 * gotten from scanners with known buggy firmware.
 */
static void
fix_up_extended_status_reply(Epson_Scanner * s, unsigned char *buf)
{
	if (e2_model(s, "ES-9000H") || e2_model(s, "GT-30000")) {
		DBG(1, "fixing up buggy ADF max scan dimensions.\n");
		buf[2] = 0xB0;
		buf[3] = 0x6D;
		buf[4] = 0x60;
		buf[5] = 0x9F;
	}
}


SANE_Status
e2_discover_capabilities(Epson_Scanner *s)
{
	SANE_Status status;

	unsigned char scanner_status;
	Epson_Device *dev = s->hw;

	SANE_String_Const *source_list_add = source_list;

	DBG(5, "%s\n", __func__);

	/* always add flatbed */
	*source_list_add++ = FBF_STR;

	/* ESC I, request identity
	 * this must be the first command on the FilmScan 200
	 */
	if (dev->connection != SANE_EPSON_NET) {
		unsigned int n, k, x = 0, y = 0;
		unsigned char *buf, *area;
		size_t len;

		status = esci_request_identity(s, &buf, &len);
		if (status != SANE_STATUS_GOOD)
			return status;

		e2_set_cmd_level(s, &buf[0]);

		/* Setting available resolutions and xy ranges for sane frontend. */
		/* cycle thru the resolutions, saving them in a list */
		for (n = 2, k = 0; n < len; n += k) {

			area = buf + n;

			switch (*area) {
			case 'R':
			{
				int val = area[2] << 8 | area[1];

				status = e2_add_resolution(s->hw, val);
				k = 3;
				continue;
			}
			case 'A':
			{
				x = area[2] << 8 | area[1];
				y = area[4] << 8 | area[3];

				DBG(1, "maximum scan area: %dx%d\n", x, y);
				k = 5;
				continue;
			}
			default:
				break;
			}
		}

		/* min and max dpi */
		dev->dpi_range.min = dev->res_list[0];
		dev->dpi_range.max = dev->res_list[dev->res_list_size - 1];
		dev->dpi_range.quant = 0;

		e2_set_fbf_area(s, x, y, dev->dpi_range.max);

		free(buf);
	}

	/* ESC F, request status */
	status = esci_request_status(s, &scanner_status);
	if (status != SANE_STATUS_GOOD)
		return status;;

	/* set capabilities */
	if (scanner_status & STATUS_OPTION)
		dev->extension = SANE_TRUE;

	if (scanner_status & STATUS_EXT_COMMANDS)
		dev->extended_commands = 1;

	/*
	 * Extended status flag request (ESC f).
	 * this also requests the scanner device name from the scanner.
	 * It seems unsupported on the network transport (CX11NF/LP-A500).
	 */

	if (dev->cmd->request_extended_status && dev->connection != SANE_EPSON_NET) {

		unsigned char *es;
		size_t es_len;

		DBG(1, "detection with request_extended_status\n");

		status = esci_request_extended_status(s, &es, &es_len);
		if (status != SANE_STATUS_GOOD)
			return status;

		/*
		 * Get the device name and copy it to dev->sane.model.
		 * The device name starts at es[0x1A] and is up to 16 bytes long
		 * We are overwriting whatever was set previously!
		 */
		if (es_len == CMD_SIZE_EXT_STATUS)	/* 42 */
			e2_set_model(s, es + 0x1A, 16);

		if (es[0] & EXT_STATUS_LID)
			DBG(1, "LID detected\n");

		if (es[0] & EXT_STATUS_PB)
			DBG(1, "push button detected\n");
		else
			dev->cmd->request_push_button_status = 0;

		/* Flatbed */
		e2_set_fbf_area(s, es[13] << 8 | es[12], es[15] << 8 | es[14],
				dev->dpi_range.max);

		/* ADF */
		if (dev->extension && (es[1] & EXT_STATUS_IST)) {
			DBG(1, "ADF detected\n");

			fix_up_extended_status_reply(s, es);

			dev->duplex = (es[0] & EXT_STATUS_ADFS) != 0;
			if (dev->duplex)
				DBG(1, "ADF supports duplex\n");

			if (es[1] & EXT_STATUS_EN) {
				DBG(1, "ADF is enabled\n");
				dev->x_range = &dev->adf_x_range;
				dev->y_range = &dev->adf_y_range;
			}

			e2_set_adf_area(s, es[3] << 8 | es[2],
					es[5] << 8 | es[4],
					dev->dpi_range.max);
			*source_list_add++ = ADF_STR;

			dev->ADF = SANE_TRUE;
		}

		/* TPU */
		if (dev->extension && (es[6] & EXT_STATUS_IST)) {
			DBG(1, "TPU detected\n");

			if (es[6] & EXT_STATUS_EN) {
				DBG(1, "TPU is enabled\n");
				dev->x_range = &dev->tpu_x_range;
				dev->y_range = &dev->tpu_y_range;
			}

			e2_set_tpu_area(s,
					(es[8] << 8 | es[7]),
					(es[10] << 8 | es[9]),
					dev->dpi_range.max);

			*source_list_add++ = TPU_STR;
			dev->TPU = SANE_TRUE;
		}

		free(es);

		*source_list_add = NULL; /* add end marker to source list */
	}

	/* FS I, request extended identity (B7/B8) */
	if (dev->extended_commands && dev->cmd->request_extended_identity) {
		unsigned char buf[80];

		DBG(1, "detection with request_extended_identity\n");

		status = esci_request_extended_identity(s, buf);
		if (status != SANE_STATUS_GOOD)
			return status;

		e2_set_cmd_level(s, &buf[0]);

		dev->maxDepth = buf[67];

		/* set model name. it will probably be
		 * different than the one reported by request_identity
		 * for the same unit (i.e. LP-A500 vs CX11) .
		 */
		e2_set_model(s, &buf[46], 16);

		dev->optical_res = le32atoh(&buf[4]);

		dev->dpi_range.min = le32atoh(&buf[8]);
		dev->dpi_range.max = le32atoh(&buf[12]);

		/* Flatbed */
		e2_set_fbf_area(s, le32atoh(&buf[20]),
				le32atoh(&buf[24]), dev->optical_res);

		/* ADF */
		if (le32atoh(&buf[28]) > 0) {
			e2_set_adf_area(s, le32atoh(&buf[28]),
					le32atoh(&buf[32]), dev->optical_res);

			if (!dev->ADF) {
				*source_list_add++ = ADF_STR;
				dev->ADF = SANE_TRUE;
			}

			if (buf[44] & EXT_IDTY_CAP1_ADFS) {
				dev->duplex = SANE_TRUE;
			}

		}

		/* TPU */
		if (le32atoh(&buf[36]) > 0 && !dev->TPU) {
			e2_set_tpu_area(s,
					le32atoh(&buf[36]),
					le32atoh(&buf[40]), dev->optical_res);

			*source_list_add++ = TPU_STR;
			dev->TPU = SANE_TRUE;
		}

                /* TPU2 */
		if (e2_model(s, "GT-X800") || e2_model(s, "GT-X900") || e2_model(s, "GT-X980")) {
			if (le32atoh(&buf[68]) > 0 ) {
				e2_set_tpu2_area(s,
					  	 le32atoh(&buf[68]),
						 le32atoh(&buf[72]),
						 dev->optical_res);

				*source_list_add++ = TP2_STR;
			}
		}

		*source_list_add = NULL; /* add end marker to source list */

	} else {
		DBG(1, "no command available to detect capabilities\n");
	}

	/*
	 * request identity 2 (ESC i), if available will
	 * get the information from the scanner and store it in dev
	 */

	if (dev->cmd->request_identity2 && dev->connection != SANE_EPSON_NET) {
		unsigned char *buf;
		status = esci_request_identity2(s, &buf);
		if (status != SANE_STATUS_GOOD)
			return status;

		/* the first two bytes of the buffer contain the optical resolution */
		dev->optical_res = buf[1] << 8 | buf[0];

		/*
		 * the 4th and 5th byte contain the line distance. Both values have to
		 * be identical, otherwise this software can not handle this scanner.
		 */
		if (buf[4] != buf[5]) {
			status = SANE_STATUS_INVAL;
			return status;
		}

		dev->max_line_distance = buf[4];
	}

	/*
	 * Check for the max. supported color depth and assign
	 * the values to the bitDepthList.
	 */
	dev->depth_list = malloc(sizeof(SANE_Int) * (4 + 1));
	if (dev->depth_list == NULL) {
		DBG(1, "out of memory (line %d)\n", __LINE__);
		return SANE_STATUS_NO_MEM;
	}

	dev->depth_list[0] = 0;

	/* maximum depth discovery */
	DBG(3, "discovering max depth, NAKs are expected\n");

	/* add default depth */
	e2_add_depth(dev, 8);

	if (dev->maxDepth >= 12 || dev->maxDepth == 0) {
		if (esci_set_data_format(s, 12) == SANE_STATUS_GOOD)
			e2_add_depth(dev, 12);
	}

	if (dev->maxDepth >= 14 || dev->maxDepth == 0) {
		if (esci_set_data_format(s, 14) == SANE_STATUS_GOOD)
			e2_add_depth(dev, 14);
	}

	if (dev->maxDepth >= 16 || dev->maxDepth == 0) {
		if (esci_set_data_format(s, 16) == SANE_STATUS_GOOD)
			e2_add_depth(dev, 16);
	}

	DBG(1, "maximum supported color depth: %d\n", dev->maxDepth);

	/*
	 * Check for "request focus position" command. If this command is
	 * supported, then the scanner does also support the "set focus
	 * position" command.
	 * XXX ???
	 */

	if (esci_request_focus_position(s, &s->currentFocusPosition) ==
	    SANE_STATUS_GOOD) {
		DBG(1, "setting focus is supported\n");
		dev->focusSupport = SANE_TRUE;
		s->opt[OPT_FOCUS].cap &= ~SANE_CAP_INACTIVE;

		/* reflect the current focus position in the GUI */
		if (s->currentFocusPosition < 0x4C) {
			/* focus on glass */
			s->val[OPT_FOCUS].w = 0;
		} else {
			/* focus 2.5mm above glass */
			s->val[OPT_FOCUS].w = 1;
		}

	} else {
		DBG(1, "setting focus is not supported\n");
		dev->focusSupport = SANE_FALSE;
		s->opt[OPT_FOCUS].cap |= SANE_CAP_INACTIVE;
		s->val[OPT_FOCUS].w = 0;	/* on glass - just in case */
	}

	/* Set defaults for no extension. */
	dev->x_range = &dev->fbf_x_range;
	dev->y_range = &dev->fbf_y_range;

	/*
	 * Correct for a firmware bug in some Perfection 1650 scanners:
	 * Firmware version 1.08 reports only half the vertical scan area, we have
	 * to double the number. To find out if we have to do this, we just compare
	 * is the vertical range is smaller than the horizontal range.
	 */

	if ((dev->x_range->max - dev->x_range->min) >
	    (dev->y_range->max - dev->y_range->min)) {
		DBG(1, "found buggy scan area, doubling it.\n");
		dev->y_range->max += (dev->y_range->max - dev->y_range->min);
		dev->need_double_vertical = SANE_TRUE;
		dev->need_color_reorder = SANE_TRUE;
	}

	/* FS F, request scanner status */
	if (dev->extended_commands) {
		unsigned char buf[16];

		status = esci_request_scanner_status(s, buf);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	return status;
}


SANE_Status
e2_set_extended_scanning_parameters(Epson_Scanner * s)
{
	unsigned char buf[64];

	const struct mode_param *mparam;

	DBG(1, "%s\n", __func__);

	mparam = &mode_params[s->val[OPT_MODE].w];

	memset(buf, 0x00, sizeof(buf));

	/* ESC R, resolution */
	htole32a(&buf[0], s->val[OPT_RESOLUTION].w);
	htole32a(&buf[4], s->val[OPT_RESOLUTION].w);

	/* ESC A, scanning area */
	htole32a(&buf[8], s->left);
	htole32a(&buf[12], s->top);
	htole32a(&buf[16], s->params.pixels_per_line);
	htole32a(&buf[20], s->params.lines);

	/*
	 * The byte sequence mode was introduced in B5,
	 *for B[34] we need line sequence mode
	 */

	/* ESC C, set color */
	if ((s->hw->cmd->level[0] == 'D'
	     || (s->hw->cmd->level[0] == 'B' && s->hw->level >= 5))
	    && mparam->flags == 0x02) {
		buf[24] = 0x13;
	} else {
		buf[24] = mparam->flags | (mparam->dropout_mask
					   & dropout_params[s->
							    val[OPT_DROPOUT].
							    w]);
	}

	/* ESC D, set data format */
	mparam = &mode_params[s->val[OPT_MODE].w];
	buf[25] = mparam->depth;

	/* ESC e, control option */
	if (s->hw->extension) {

		char extensionCtrl;
		extensionCtrl = (s->hw->use_extension ? 1 : 0);
		if (s->hw->use_extension && (s->val[OPT_ADF_MODE].w == 1)) {
			extensionCtrl = 2;
		}

		/* Test for TPU2
		 * Epson Perfection 4990 Command Specifications
		 * JZIS-0075 Rev. A, page 31
		 */
		if (s->hw->use_extension && s->hw->TPU2) {
			extensionCtrl = 5;
		}

		if (s->val[OPT_MODE].w == MODE_INFRARED) {
                        /* only infrared in TPU mode (NOT in TPU2 or flatbeth) 
	                 * XXX investigate this ... only tested on GT-X800 
                         */

                        if (extensionCtrl == 1)   /* test for TPU */
				extensionCtrl = 3;
                        else
				return SANE_STATUS_UNSUPPORTED;
		}

		/* ESC e */
		buf[26] = extensionCtrl;

		/* XXX focus */
	}

	/* ESC g, scanning mode (normal or high speed) */
	if (s->val[OPT_PREVIEW].w)
		buf[27] = 1;	/* High speed */
	else
		buf[27] = 0;

	/* ESC d, block line number */
	buf[28] = s->lcount;

	/* ESC Z, set gamma correction */
	buf[29] = 0x01;		/* default */

	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_GAMMA_CORRECTION].cap)) {
		char val;
		if (s->hw->cmd->level[0] == 'D') {
			/* The D1 level has only the two user defined gamma
			 * settings.
			 */
			val = gamma_params[s->val[OPT_GAMMA_CORRECTION].w];
		} else {
			val = gamma_params[s->val[OPT_GAMMA_CORRECTION].w];

			/*
			 * If "Default" is selected then determine the actual value
			 * to send to the scanner: If bilevel mode, just send the
			 * value from the table (0x01), for grayscale or color mode
			 * add one and send 0x02.
			 */

			if (s->val[OPT_GAMMA_CORRECTION].w == 0) {
				val += mparam->depth == 1 ? 0 : 1;
			}
		}

		buf[29] = val;
	}

	/* ESC L, set brightness */
	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_BRIGHTNESS].cap))
		buf[30] = s->val[OPT_BRIGHTNESS].w;

	/* ESC B, set halftoning mode / halftone processing */
	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_HALFTONE].cap))
		buf[32] = halftone_params[s->val[OPT_HALFTONE].w];

	/* ESC s, auto area segmentation */
	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_AAS].cap))
		buf[34] = s->val[OPT_AAS].w;

	/* ESC Q, set sharpness / sharpness control */
	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_SHARPNESS].cap))
		buf[35] = s->val[OPT_SHARPNESS].w;

	/* ESC K, set data order / mirroring */
	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_MIRROR].cap))
		buf[36] = s->val[OPT_MIRROR].w;

	/* ESC N, film type */
	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_FILM_TYPE].cap))
		buf[37] = film_params[s->val[OPT_FILM_TYPE].w];

	/* ESC M, color correction */
	buf[31] = correction_params[s->val[OPT_COLOR_CORRECTION].w];

	/* ESC t, threshold */
	buf[33] = s->val[OPT_THRESHOLD].w;

	return esci_set_scanning_parameter(s, buf);
}

SANE_Status
e2_set_scanning_parameters(Epson_Scanner * s)
{
	SANE_Status status;
	struct mode_param *mparam = &mode_params[s->val[OPT_MODE].w];
	unsigned char color_mode;

	DBG(1, "%s\n", __func__);

	/*
	 *  There is some undocumented special behavior with the TPU enable/disable.
	 *      TPU power  ESC e      status
	 *      on           0        NAK
	 *      on           1        ACK
	 *      off          0        ACK
	 *      off          1        NAK
	 *
	 * It makes no sense to scan with TPU powered on and source flatbed, because
	 * light will come from both sides.
	 */

	if (s->hw->extension) {

		int extensionCtrl;
		extensionCtrl = (s->hw->use_extension ? 1 : 0);
		if (s->hw->use_extension && (s->val[OPT_ADF_MODE].w == 1))
			extensionCtrl = 2;

		status = esci_control_extension(s, extensionCtrl);
		if (status != SANE_STATUS_GOOD) {
			DBG(1, "you may have to power %s your TPU\n",
			    s->hw->use_extension ? "on" : "off");
			DBG(1,
			    "and you may also have to restart the SANE frontend.\n");
			return status;
		}

		/* XXX use request_extended_status and analyze
		 * buffer to set the scan area for
		 * ES-9000H and GT-30000
		 */

		/*
		 * set the focus position according to the extension used:
		 * if the TPU is selected, then focus 2.5mm above the glass,
		 * otherwise focus on the glass. Scanners that don't support
		 * this feature, will just ignore these calls.
		 */

		if (s->hw->focusSupport == SANE_TRUE) {
			if (s->val[OPT_FOCUS].w == 0) {
				DBG(1, "setting focus to glass surface\n");
				esci_set_focus_position(s, 0x40);
			} else {
				DBG(1,
				    "setting focus to 2.5mm above glass\n");
				esci_set_focus_position(s, 0x59);
			}
		}
	}

	/* ESC C, Set color */
	color_mode = mparam->flags | (mparam->dropout_mask
				      & dropout_params[s->val[OPT_DROPOUT].
						       w]);

	/*
	 * The byte sequence mode was introduced in B5, for B[34] we need line sequence mode
	 * XXX Check what to do for the FilmScan 200
	 */
	if ((s->hw->cmd->level[0] == 'D'
	     || (s->hw->cmd->level[0] == 'B' && s->hw->level >= 5))
	    && mparam->flags == 0x02)
		color_mode = 0x13;

	status = esci_set_color_mode(s, color_mode);
	if (status != SANE_STATUS_GOOD)
		return status;

	/* ESC D, set data format */
	DBG(1, "%s: setting data format to %d bits\n", __func__,
	    mparam->depth);
	status = esci_set_data_format(s, mparam->depth);
	if (status != SANE_STATUS_GOOD)
		return status;

	/* ESC B, set halftoning mode */
	if (s->hw->cmd->set_halftoning
	    && SANE_OPTION_IS_ACTIVE(s->opt[OPT_HALFTONE].cap)) {
		status = esci_set_halftoning(s,
					     halftone_params[s->
							     val
							     [OPT_HALFTONE].
							     w]);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	/* ESC L, set brightness */
	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_BRIGHTNESS].cap)) {
		status = esci_set_bright(s, s->val[OPT_BRIGHTNESS].w);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_AAS].cap)) {
		status = esci_set_auto_area_segmentation(s,
							 s->val[OPT_AAS].w);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_FILM_TYPE].cap)) {
		status = esci_set_film_type(s,
					film_params[s->val[OPT_FILM_TYPE].w]);

		if (status != SANE_STATUS_GOOD)
			return status;
	}

	if (s->hw->cmd->set_gamma
	    && SANE_OPTION_IS_ACTIVE(s->opt[OPT_GAMMA_CORRECTION].cap)) {
		int val;
		if (s->hw->cmd->level[0] == 'D') {
			/*
			 * The D1 level has only the two user defined gamma
			 * settings.
			 */
			val = gamma_params[s->val[OPT_GAMMA_CORRECTION].w];
		} else {
			val = gamma_params[s->val[OPT_GAMMA_CORRECTION].w];

			/*
			 * If "Default" is selected then determine the actual value
			 * to send to the scanner: If bilevel mode, just send the
			 * value from the table (0x01), for grayscale or color mode
			 * add one and send 0x02.
			 */

/*			if( s->val[ OPT_GAMMA_CORRECTION].w <= 1) { */
			if (s->val[OPT_GAMMA_CORRECTION].w == 0) {
				val += mparam->depth == 1 ? 0 : 1;
			}
		}

		status = esci_set_gamma(s, val);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	if (s->hw->cmd->set_threshold != 0
	    && SANE_OPTION_IS_ACTIVE(s->opt[OPT_THRESHOLD].cap)) {
		status = esci_set_threshold(s, s->val[OPT_THRESHOLD].w);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	/* XXX ESC Z here */

	/* ESC M, set color correction */
	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_COLOR_CORRECTION].cap)) {

		status = esci_set_color_correction(s,
			correction_params[s->val[OPT_COLOR_CORRECTION].w]);

		if (status != SANE_STATUS_GOOD)
			return status;
	}

	/* ESC Q, set sharpness */
	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_SHARPNESS].cap)) {

		status = esci_set_sharpness(s, s->val[OPT_SHARPNESS].w);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	/* ESC g, set scanning mode */
	if (s->val[OPT_PREVIEW].w)
		status = esci_set_speed(s, 1);
	else
		status = esci_set_speed(s, 0);

	if (status != SANE_STATUS_GOOD)
		return status;

	/* ESC K, set data order */
	if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_MIRROR].cap)) {
		status = esci_mirror_image(s, s->val[OPT_MIRROR].w);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	/* ESC R */
	status = esci_set_resolution(s, s->val[OPT_RESOLUTION].w,
				     s->val[OPT_RESOLUTION].w);
	if (status != SANE_STATUS_GOOD)
		return status;

	/* ESC H, set zoom */
	/* not implemented */

	/* ESC A, set scanning area */

	/*
	 * Modify the scan area: If the scanner requires color shuffling, then we try to
	 * scan more lines to compensate for the lines that will be removed from the scan
	 * due to the color shuffling algorithm.
	 */

	if (s->hw->color_shuffle == SANE_TRUE) {

		unsigned int lines = s->params.lines + (2 * s->line_distance);
		int top = s->top - (1 * s->line_distance);

		if (top < 0)
			top = 0;

		status = esci_set_scan_area(s, s->left, top,
				    s->params.pixels_per_line,
				    lines);

	} else {

		status = esci_set_scan_area(s, s->left, s->top,
				    s->params.pixels_per_line,
				    s->params.lines);
	}

	if (status != SANE_STATUS_GOOD)
		return status;

	/* ESC d, set block line number / set line counter */
	status = esci_set_lcount(s, s->lcount);
	if (status != SANE_STATUS_GOOD)
		return status;

	return SANE_STATUS_GOOD;
}

void
e2_setup_block_mode(Epson_Scanner * s)
{
	int maxreq;

	DBG(5, "%s\n", __func__);

	s->block = SANE_TRUE;

	if (s->hw->connection == SANE_EPSON_SCSI)
		maxreq = sanei_scsi_max_request_size;
	else if (s->hw->connection == SANE_EPSON_USB)
		maxreq = 128 * 1024;
	else
		maxreq = 32 * 1024;

	/* XXX verify if this can b extended to other models */
	if (s->hw->connection == SANE_EPSON_NET && e2_model(s, "LP-A500"))
		maxreq = 64 * 1024;

	s->lcount = maxreq / s->params.bytes_per_line;

	DBG(1, "max req size: %d, line count: %d\n", maxreq, s->lcount);

	/* XXX investigate this */
	if (s->lcount < 3 && (e2_model(s, "GT-X800") || e2_model(s, "GT-X900") || e2_model(s, "GT-X980"))) {
		s->lcount = 21;
		DBG(17,
		    "%s: set lcount = %i bigger than sanei_scsi_max_request_size\n",
		    __func__, s->lcount);
	}

	if (s->lcount >= 255)
		s->lcount = 255;

	/* XXX why this? */
	if (s->hw->TPU && s->hw->use_extension && s->lcount > 32)
		s->lcount = 32;

	/*
	 * The D1 series of scanners only allow an even line number
	 * for bi-level scanning. If a bit depth of 1 is selected, then
	 * make sure the next lower even number is selected.
	 */

	/* XXX check bith depth? */
	if (s->hw->cmd->level[0] == 'D' && s->lcount > 3 && s->lcount % 2)
		s->lcount -= 1;

	DBG(1, "final line count is %d\n", s->lcount);
}

SANE_Status
e2_init_parameters(Epson_Scanner * s)
{
	int dpi, bytes_per_pixel;
	struct mode_param *mparam;

	DBG(5, "%s\n", __func__);

	memset(&s->params, 0, sizeof(SANE_Parameters));

	dpi = s->val[OPT_RESOLUTION].w;

	mparam = &mode_params[s->val[OPT_MODE].w];

	if (SANE_UNFIX(s->val[OPT_BR_Y].w) == 0 ||
		SANE_UNFIX(s->val[OPT_BR_X].w) == 0)
		return SANE_STATUS_INVAL;

	s->left = ((SANE_UNFIX(s->val[OPT_TL_X].w) / MM_PER_INCH) *
		s->val[OPT_RESOLUTION].w) + 0.5;

	s->top = ((SANE_UNFIX(s->val[OPT_TL_Y].w) / MM_PER_INCH) *
		s->val[OPT_RESOLUTION].w) + 0.5;

	s->params.pixels_per_line =
		((SANE_UNFIX(s->val[OPT_BR_X].w -
			   s->val[OPT_TL_X].w) / MM_PER_INCH) * dpi) + 0.5;
	s->params.lines =
		((SANE_UNFIX(s->val[OPT_BR_Y].w -
			   s->val[OPT_TL_Y].w) / MM_PER_INCH) * dpi) + 0.5;


	DBG(1, "%s: resolution = %d, preview = %d\n",
		__func__, s->val[OPT_RESOLUTION].w, s->val[OPT_PREVIEW].w);

	DBG(1, "%s: %p %p tlx %f tly %f brx %f bry %f [mm]\n",
	    __func__, (void *) s, (void *) s->val,
	    SANE_UNFIX(s->val[OPT_TL_X].w), SANE_UNFIX(s->val[OPT_TL_Y].w),
	    SANE_UNFIX(s->val[OPT_BR_X].w), SANE_UNFIX(s->val[OPT_BR_Y].w));

	/*
	 * Calculate bytes_per_pixel and bytes_per_line for
	 * any color depths.
	 *
	 * The default color depth is stored in mode_params.depth:
	 */

	if (mode_params[s->val[OPT_MODE].w].depth == 1)
		s->params.depth = 1;
	else
		s->params.depth = s->val[OPT_BIT_DEPTH].w;

	if (s->params.depth > 8) {
		s->params.depth = 16;	/*
					 * The frontends can only handle 8 or 16 bits
					 * for gray or color - so if it's more than 8,
					 * it gets automatically set to 16. This works
					 * as long as EPSON does not come out with a
					 * scanner that can handle more than 16 bits
					 * per color channel.
					 */
	}

	/* this works because it can only be set to 1, 8 or 16 */
	bytes_per_pixel = s->params.depth / 8;
	if (s->params.depth % 8) {	/* just in case ... */
		bytes_per_pixel++;
	}

	/* pixels_per_line is rounded to the next 8bit boundary */
	s->params.pixels_per_line = s->params.pixels_per_line & ~7;

	s->params.last_frame = SANE_TRUE;

	switch (s->val[OPT_MODE].w) {
	case MODE_BINARY:
	case MODE_GRAY:
		s->params.format = SANE_FRAME_GRAY;
		s->params.bytes_per_line =
			s->params.pixels_per_line * s->params.depth / 8;
		break;
	case MODE_COLOR:
		s->params.format = SANE_FRAME_RGB;
		s->params.bytes_per_line =
			3 * s->params.pixels_per_line * bytes_per_pixel;
		break;
#ifdef SANE_FRAME_IR
	case MODE_INFRARED:
		s->params.format = SANE_FRAME_IR;
		s->params.bytes_per_line =
			s->params.pixels_per_line * s->params.depth / 8;
		break;
#endif
	}

	if (s->params.bytes_per_line == 0)
		return SANE_STATUS_INVAL;

	/*
	 * Calculate correction for line_distance in D1 scanner:
	 * Start line_distance lines earlier and add line_distance lines at the end
	 *
	 * Because the actual line_distance is not yet calculated we have to do this
	 * first.
	 */

	s->hw->color_shuffle = SANE_FALSE;

	s->lines_written = 0;
	s->color_shuffle_line = 0;
	s->current_output_line = 0;

	if ((s->hw->optical_res != 0) && (mparam->depth == 8)
	    && (mparam->flags != 0)) {

		s->line_distance =
			s->hw->max_line_distance * dpi / s->hw->optical_res;

		if (s->line_distance != 0) {

			s->hw->color_shuffle = SANE_TRUE;

			DBG(1, "%s: color shuffling required\n",  __func__);
		}
	}

	/*
	 * If (s->top + s->params.lines) is larger than the max scan area, reset
	 * the number of scan lines:
	 * XXX: precalculate the maximum scanning area elsewhere (use dev max_y)
	 */

	if (SANE_UNFIX(s->val[OPT_BR_Y].w) / MM_PER_INCH * dpi <
	    (s->params.lines + s->top)) {
		s->params.lines =
			((int) SANE_UNFIX(s->val[OPT_BR_Y].w) / MM_PER_INCH *
			 dpi + 0.5) - s->top;
	}

	s->block = SANE_FALSE;
	s->lcount = 1;

	/*
	 * The set line count commands needs to be sent for certain scanners in
	 * color mode. The D1 level requires it, we are however only testing for
	 * 'D' and not for the actual numeric level.
	 */

	if ((s->hw->cmd->level[0] == 'B') && (s->hw->level >= 5)) /* >= B5 */
		e2_setup_block_mode(s);

	else if ((s->hw->cmd->level[0] == 'B') && (s->hw->level == 4) /* B4 !color */
		&& (!mode_params[s->val[OPT_MODE].w].color))
		e2_setup_block_mode(s);

	else if (s->hw->cmd->level[0] == 'D') /* Dx */
		e2_setup_block_mode(s);

	return (s->params.lines > 0) ? SANE_STATUS_GOOD : SANE_STATUS_INVAL;
}

void
e2_wait_button(Epson_Scanner * s)
{
	DBG(5, "%s\n", __func__);

	s->hw->wait_for_button = SANE_TRUE;

	while (s->hw->wait_for_button == SANE_TRUE) {
		unsigned char button_status = 0;

		if (s->canceling == SANE_TRUE)
			s->hw->wait_for_button = SANE_FALSE;

		/* get the button status from the scanner */
		else if (esci_request_push_button_status(s, &button_status) ==
			 SANE_STATUS_GOOD) {
			if (button_status)
				s->hw->wait_for_button = SANE_FALSE;
			else
				sleep(1);
		} else {
			/* we run into an error condition, just continue */
			s->hw->wait_for_button = SANE_FALSE;
		}
	}
}

/*
SANE_Status
e2_check_extended_status(Epson_Scanner *s)
{

	SANE_Status status = esci_request_scanner_status(s, buf);
	if (status != SANE_STATUS_GOOD)
			return status;

	if (buf[0] & FSF_STATUS_MAIN_WU)

	main -> 0
	fbf -> 3
	adf -> 1, 10
	tpu -> 2
}
*/

SANE_Status
e2_check_warm_up(Epson_Scanner * s, SANE_Bool * wup)
{
	SANE_Status status;

	DBG(5, "%s\n", __func__);

	*wup = SANE_FALSE;

	if (s->hw->extended_commands) {
		unsigned char buf[16];

		status = esci_request_scanner_status(s, buf);
		if (status != SANE_STATUS_GOOD)
			return status;

		if (buf[0] & FSF_STATUS_MAIN_WU)
			*wup = SANE_TRUE;

	} else {
		unsigned char *es;

		/* this command is not available on some scanners */
		if (!s->hw->cmd->request_extended_status)
			return SANE_STATUS_GOOD;

		status = esci_request_extended_status(s, &es, NULL);
		if (status != SANE_STATUS_GOOD)
			return status;

		if (es[0] & EXT_STATUS_WU)
			*wup = SANE_TRUE;

		free(es);
	}

	return status;
}

SANE_Status
e2_wait_warm_up(Epson_Scanner * s)
{
	SANE_Status status;
	SANE_Bool wup;

	DBG(5, "%s\n", __func__);

	s->retry_count = 0;

	while (1) {

		if (s->canceling)
			return SANE_STATUS_CANCELLED;

		status = e2_check_warm_up(s, &wup);
		if (status != SANE_STATUS_GOOD)
			return status;

		if (wup == SANE_FALSE)
			break;

		s->retry_count++;

		if (s->retry_count > SANE_EPSON_MAX_RETRIES) {
			DBG(1, "max retry count exceeded (%d)\n",
			    s->retry_count);
			return SANE_STATUS_DEVICE_BUSY;
		}
		sleep(5);
	}

	return SANE_STATUS_GOOD;
}

SANE_Status
e2_check_adf(Epson_Scanner * s)
{
	SANE_Status status;

	DBG(5, "%s\n", __func__);

	if (s->hw->use_extension == SANE_FALSE)
		return SANE_STATUS_GOOD;

	if (s->hw->extended_commands) {
		unsigned char buf[16];

		status = esci_request_scanner_status(s, buf);
		if (status != SANE_STATUS_GOOD)
			return status;

		if (buf[1] & FSF_STATUS_ADF_PE)
			return SANE_STATUS_NO_DOCS;

		if (buf[1] & FSF_STATUS_ADF_PJ)
			return SANE_STATUS_JAMMED;

	} else {
		unsigned char *buf, t;

		status = esci_request_extended_status(s, &buf, NULL);
		if (status != SANE_STATUS_GOOD)
			return status;;

		t = buf[1];

		free(buf);

		if (t & EXT_STATUS_PE)
			return SANE_STATUS_NO_DOCS;

		if (t & EXT_STATUS_PJ)
			return SANE_STATUS_JAMMED;
	}

	return SANE_STATUS_GOOD;
}

SANE_Status
e2_start_std_scan(Epson_Scanner * s)
{
	SANE_Status status;
	unsigned char params[2];

	DBG(5, "%s\n", __func__);

	/* ESC g */
	params[0] = ESC;
	params[1] = s->hw->cmd->start_scanning;

	e2_send(s, params, 2, 6 + (s->lcount * s->params.bytes_per_line),
		&status);

	return status;
}

SANE_Status
e2_start_ext_scan(Epson_Scanner * s)
{
	SANE_Status status;
	unsigned char params[2];
	unsigned char buf[14];

	DBG(5, "%s\n", __func__);

	params[0] = FS;
	params[1] = 'G';

	status = e2_txrx(s, params, 2, buf, 14);
	if (status != SANE_STATUS_GOOD)
		return status;

	if (buf[0] != STX)
		return SANE_STATUS_INVAL;

	if (buf[1] & 0x80) {
		DBG(1, "%s: fatal error\n", __func__);
		return SANE_STATUS_IO_ERROR;
	}

	s->ext_block_len = le32atoh(&buf[2]);
	s->ext_blocks = le32atoh(&buf[6]);
	s->ext_last_len = le32atoh(&buf[10]);

	s->ext_counter = 0;

	DBG(5, " status         : 0x%02x\n", buf[1]);
	DBG(5, " block size     : %u\n", (unsigned int) le32atoh(&buf[2]));
	DBG(5, " block count    : %u\n", (unsigned int) le32atoh(&buf[6]));
	DBG(5, " last block size: %u\n", (unsigned int) le32atoh(&buf[10]));

	if (s->ext_last_len) {
		s->ext_blocks++;
		DBG(1, "adjusted block count: %d\n", s->ext_blocks);
	}

	/* adjust block len if we have only one block to read */
	if (s->ext_block_len == 0 && s->ext_last_len)
		s->ext_block_len = s->ext_last_len;

	return status;
}

void
e2_scan_finish(Epson_Scanner * s)
{
	DBG(5, "%s\n", __func__);

	free(s->buf);
	s->buf = NULL;

	if (s->hw->ADF && s->hw->use_extension && s->val[OPT_AUTO_EJECT].w)
		if (e2_check_adf(s) == SANE_STATUS_NO_DOCS)
			esci_eject(s);
}

void
e2_copy_image_data(Epson_Scanner * s, SANE_Byte * data, SANE_Int max_length,
		   SANE_Int * length)
{
	if (!s->block && s->params.format == SANE_FRAME_RGB) {

		max_length /= 3;

		if (max_length > s->end - s->ptr)
			max_length = s->end - s->ptr;

		*length = 3 * max_length;

		while (max_length-- != 0) {
			*data++ = s->ptr[0];
			*data++ = s->ptr[s->params.pixels_per_line];
			*data++ = s->ptr[2 * s->params.pixels_per_line];
			++s->ptr;
		}

	} else {
		if (max_length > s->end - s->ptr)
			max_length = s->end - s->ptr;

		*length = max_length;

		if (s->params.depth == 1) {
			while (max_length-- != 0)
				*data++ = ~*s->ptr++;
		} else {
			memcpy(data, s->ptr, max_length);
			s->ptr += max_length;
		}
	}
}

SANE_Status
e2_ext_read(struct Epson_Scanner *s)
{
	struct Epson_Device *dev = s->hw;
	SANE_Status status = SANE_STATUS_GOOD;
	ssize_t buf_len = 0, read;

	DBG(18, "%s: begin\n", __func__);

	/* did we passed everything we read to sane? */
	if (s->ptr == s->end) {

		if (s->eof)
			return SANE_STATUS_EOF;

		s->ext_counter++;

		/* sane has already got the data, read some more, the final
		 * error byte must not be included in buf_len
		 */
		buf_len = s->ext_block_len;

		if (s->ext_counter == s->ext_blocks && s->ext_last_len)
			buf_len = s->ext_last_len;

		DBG(18, "%s: block %d/%d, size %lu\n", __func__,
			s->ext_counter, s->ext_blocks,
			(unsigned long) buf_len);

		/* receive image data + error code */
		read = e2_recv(s, s->buf, buf_len + 1, &status);

		DBG(18, "%s: read %lu bytes, status: %d\n", __func__, (unsigned long) read, status);

		if (status != SANE_STATUS_GOOD) {
			e2_cancel(s);
			return status;
		}

		if (e2_dev_model(dev, "GT-8200") || e2_dev_model(dev, "Perfection1650")) {
			/* See http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=597922#127 */
			s->buf[buf_len] &= 0xc0;
		}

		if (s->buf[buf_len] & FSG_STATUS_CANCEL_REQ) {
			DBG(0, "%s: cancel request received\n", __func__);
			e2_cancel(s);
			return SANE_STATUS_CANCELLED;
		}

		if (s->buf[buf_len] & (FSG_STATUS_FER | FSG_STATUS_NOT_READY))
			return SANE_STATUS_IO_ERROR;

		/* ack every block except the last one */
		if (s->ext_counter < s->ext_blocks) {
			size_t next_len = s->ext_block_len;

			if (s->ext_counter == (s->ext_blocks - 1))
				next_len = s->ext_last_len;

			if (s->canceling) {
				e2_cancel(s);
				return SANE_STATUS_CANCELLED;
			}

			status = e2_ack_next(s, next_len + 1);
		} else
			s->eof = SANE_TRUE;

		s->end = s->buf + buf_len;
		s->ptr = s->buf;
	}

	return status;
}

/* XXXX use routine from sane-evolution */

typedef struct
{
	unsigned char code;
	unsigned char status;

	unsigned char buf[4];

} EpsonDataRec;


/* XXX this routine is ugly and should be avoided */
static SANE_Status
read_info_block(Epson_Scanner * s, EpsonDataRec * result)
{
	SANE_Status status;
	unsigned char params[2];

      retry:
	e2_recv(s, result, s->block ? 6 : 4, &status);
	if (status != SANE_STATUS_GOOD)
		return status;

	if (result->code != STX) {
		DBG(1, "error: got %02x, expected STX\n", result->code);
		return SANE_STATUS_INVAL;
	}

	/* XXX */
	if (result->status & STATUS_FER) {
		unsigned char *ext_status;

		DBG(1, "fatal error, status = %02x\n", result->status);

		if (s->retry_count > SANE_EPSON_MAX_RETRIES) {
			DBG(1, "max retry count exceeded (%d)\n",
			    s->retry_count);
			return SANE_STATUS_INVAL;
		}

		/* if the scanner is warming up, retry after a few secs */
		status = esci_request_extended_status(s, &ext_status, NULL);
		if (status != SANE_STATUS_GOOD)
			return status;

		if (ext_status[0] & EXT_STATUS_WU) {
			free(ext_status);

			sleep(5);	/* for the next attempt */

			DBG(1, "retrying ESC G - %d\n", ++(s->retry_count));

			params[0] = ESC;
			params[1] = s->hw->cmd->start_scanning;

			e2_send(s, params, 2, 0, &status);
			if (status != SANE_STATUS_GOOD)
				return status;

			goto retry;
		} else
			free(ext_status);
	}

	return status;
}

static SANE_Status
color_shuffle(SANE_Handle handle, int *new_length)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;
	SANE_Byte *buf = s->buf;
	int length = s->end - s->buf;

		SANE_Byte *data_ptr;	/* ptr to data to process */
		SANE_Byte *data_end;	/* ptr to end of processed data */
		SANE_Byte *out_data_ptr;	/* ptr to memory when writing data */
		int i;		/* loop counter */

		/*
		 * It looks like we are dealing with a scanner that has an odd way
		 * of dealing with colors... The red and blue scan lines are shifted
		 * up or down by a certain number of lines relative to the green line.
		 */
		DBG(5, "%s\n", __func__);

		/*
		 * Initialize the variables we are going to use for the
		 * copying of the data. data_ptr is the pointer to
		 * the currently worked on scan line. data_end is the
		 * end of the data area as calculated from adding *length
		 * to the start of data.
		 * out_data_ptr is used when writing out the processed data
		 * and always points to the beginning of the next line to
		 * write.
		 */
		data_ptr = out_data_ptr = buf;
		data_end = data_ptr + length;

		/*
		 * The image data is in *buf, we know that the buffer contains s->end - s->buf ( = length)
		 * bytes of data. The width of one line is in s->params.bytes_per_line
		 *
		 * The buffer area is supposed to have a number of full scan
		 * lines, let's test if this is the case.
		 */

		if (length % s->params.bytes_per_line != 0) {
			DBG(1, "error in buffer size: %d / %d\n", length,
			    s->params.bytes_per_line);
			return SANE_STATUS_INVAL;
		}

		while (data_ptr < data_end) {
			SANE_Byte *source_ptr, *dest_ptr;
			int loop;

			/* copy the green information into the current line */

			source_ptr = data_ptr + 1;
			dest_ptr = s->line_buffer[s->color_shuffle_line] + 1;

			for (i = 0; i < s->params.bytes_per_line / 3; i++) {
				*dest_ptr = *source_ptr;
				dest_ptr += 3;
				source_ptr += 3;
			}

			/* copy the red information n lines back */

			if (s->color_shuffle_line >= s->line_distance) {
				source_ptr = data_ptr + 2;
				dest_ptr =
					s->line_buffer[s->color_shuffle_line -
						       s->line_distance] + 2;

/*				while (source_ptr < s->line_buffer[s->color_shuffle_line] + s->params.bytes_per_line) */
				for (loop = 0;
				     loop < s->params.bytes_per_line / 3;
				     loop++) {
					*dest_ptr = *source_ptr;
					dest_ptr += 3;
					source_ptr += 3;
				}
			}

			/* copy the blue information n lines forward */

			source_ptr = data_ptr;
			dest_ptr =
				s->line_buffer[s->color_shuffle_line +
					       s->line_distance];

/*			while (source_ptr < s->line_buffer[s->color_shuffle_line] + s->params.bytes_per_line) */
			for (loop = 0; loop < s->params.bytes_per_line / 3;
			     loop++) {
				*dest_ptr = *source_ptr;
				dest_ptr += 3;
				source_ptr += 3;
			}

			data_ptr += s->params.bytes_per_line;

			if (s->color_shuffle_line == s->line_distance) {
				/*
				 * We just finished the line in line_buffer[0] - write it to the
				 * output buffer and continue.
				 *
				 * The ouput buffer ist still "buf", but because we are
				 * only overwriting from the beginning of the memory area
				 * we are not interfering with the "still to shuffle" data
				 * in the same area.
				 */

				/*
				 * Strip the first and last n lines and limit to
				 */
				if ((s->current_output_line >=
				     s->line_distance)
				    && (s->current_output_line <
					s->params.lines + s->line_distance)) {
					memcpy(out_data_ptr,
					       s->line_buffer[0],
					       s->params.bytes_per_line);
					out_data_ptr +=
						s->params.bytes_per_line;

					s->lines_written++;
				}

				s->current_output_line++;

				/*
				 * Now remove the 0-entry and move all other
				 * lines up by one. There are 2*line_distance + 1
				 * buffers, * therefore the loop has to run from 0
				 * to * 2*line_distance, and because we want to
				 * copy every n+1st entry to n the loop runs
				 * from - to 2*line_distance-1!
				 */

				free(s->line_buffer[0]);

				for (i = 0; i < s->line_distance * 2; i++) {
					s->line_buffer[i] =
						s->line_buffer[i + 1];
				}

				/*
				 * and create one new buffer at the end
				 */

				s->line_buffer[s->line_distance * 2] =
					malloc(s->params.bytes_per_line);
				if (s->line_buffer[s->line_distance * 2] ==
				    NULL) {
					DBG(1, "out of memory (line %d)\n",
					    __LINE__);
					return SANE_STATUS_NO_MEM;
				}
			} else {
				s->color_shuffle_line++;	/* increase the buffer number */
			}
		}

		/*
		 * At this time we've used up all the new data from the scanner, some of
		 * it is still in the line_buffers, but we are ready to return some of it
		 * to the front end software. To do so we have to adjust the size of the
		 * data area and the *new_length variable.
		 */

		*new_length = out_data_ptr - buf;

	return SANE_STATUS_GOOD;
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


SANE_Status
e2_block_read(struct Epson_Scanner *s)
{
	SANE_Status status;
	SANE_Bool reorder = SANE_FALSE;
	SANE_Bool needStrangeReorder = SANE_FALSE;

      START_READ:
	DBG(18, "%s: begin\n", __func__);

	if (s->ptr == s->end) {
		EpsonDataRec result;
		unsigned int buf_len;

		if (s->eof) {
			if (s->hw->color_shuffle) {
				DBG(1,
				    "written %d lines after color shuffle\n",
				    s->lines_written);
				DBG(1, "lines requested: %d\n",
				    s->params.lines);
			}

			return SANE_STATUS_EOF;
		}

		status = read_info_block(s, &result);
		if (status != SANE_STATUS_GOOD) {
			return status;
		}

		buf_len = result.buf[1] << 8 | result.buf[0];
		buf_len *= (result.buf[3] << 8 | result.buf[2]);

		DBG(18, "%s: buf len = %u\n", __func__, buf_len);

		{
			/* do we have to reorder the data ? */
			if (get_color(result.status) == 0x01)
				reorder = SANE_TRUE;

			e2_recv(s, s->buf, buf_len, &status);
			if (status != SANE_STATUS_GOOD) {
				return status;
			}
		}

		if (result.status & STATUS_AREA_END) {
			DBG(1, "%s: EOF\n", __func__);
			s->eof = SANE_TRUE;
		} else {
			if (s->canceling) {
				e2_cancel(s);
				return SANE_STATUS_CANCELLED;
			} else {
				status = e2_ack(s);
			}
		}

		s->end = s->buf + buf_len;
		s->ptr = s->buf;

		/*
		 * if we have to re-order the color components (GRB->RGB) we
		 * are doing this here:
		 */

		/*
		 * Some scanners (e.g. the Perfection 1640 and GT-2200) seem
		 * to have the R and G channels swapped.
		 * The GT-8700 is the Asian version of the Perfection 1640.
		 * If the scanner name is one of these and the scan mode is
		 * RGB then swap the colors.
		 */

		/* never used, beta testers required */
		needStrangeReorder =
			(strstr(s->hw->model, "GT-2200") ||
			 ((strstr(s->hw->model, "1640")
			   && strstr(s->hw->model, "Perfection"))
			  || strstr(s->hw->model, "GT-8700")))
			&& s->params.format == SANE_FRAME_RGB;

		/*
		 * Certain Perfection 1650 also need this re-ordering of the two
		 * color channels. These scanners are identified by the problem
		 * with the half vertical scanning area. When we corrected this,
		 * we also set the variable s->hw->need_color_reorder
		 */
		if (s->hw->need_color_reorder)
			reorder = SANE_FALSE;	/* reordering once is enough */

		if (reorder && s->params.format == SANE_FRAME_RGB) {
			SANE_Byte *ptr;

			ptr = s->buf;
			while (ptr < s->end) {
				if (s->params.depth > 8) {
					SANE_Byte tmp;

					/* R->G G->R */
					tmp = ptr[0];
					ptr[0] = ptr[2];	/* first Byte G */
					ptr[2] = tmp;	/* first Byte R */

					tmp = ptr[1];
					ptr[1] = ptr[3];	/* second Byte G */
					ptr[3] = tmp;	/* second Byte R */

					ptr += 6;	/* go to next pixel */
				} else {
					/* R->G G->R */
					SANE_Byte tmp;

					tmp = ptr[0];
					ptr[0] = ptr[1];	/* G */
					ptr[1] = tmp;	/* R */
					/* B stays the same */
					ptr += 3;	/* go to next pixel */
				}
			}
		}

		/*
		 * Do the color_shuffle if everything else is correct - at this time
		 * most of the stuff is hardcoded for the Perfection 610
		 */

		if (s->hw->color_shuffle) {
			int new_length = 0;

			status = color_shuffle(s, &new_length);
			/* XXX check status here */

			/*
			 * If no bytes are returned, check if the scanner is already done, if so,
			 * we'll probably just return, but if there is more data to process get
			 * the next batch.
			 */
			if (new_length == 0 && s->end != s->ptr)
				goto START_READ;

			s->end = s->buf + new_length;
			s->ptr = s->buf;
		}

		DBG(18, "%s: begin scan2\n", __func__);
	}

	DBG(18, "%s: end\n", __func__);

	return SANE_STATUS_GOOD;
}
