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

#define EPSONDS_VERSION		1
#define EPSONDS_REVISION	0
#define EPSONDS_BUILD		35

/* debugging levels:
 *
 *	32	eds_send
 *	30	eds_recv
 *	20	sane_read and related
 *	18	sane_read and related
 *	17	setvalue, getvalue, control_option
 *	16
 *	15	esci2_img
 *	13	image_cb
 *	12	eds_control
 *	11	all received params
 *	10	some received params
 *	 9
 *	 8	esci2_xxx
 *	 7	open/close/attach
 *	 6	print_params
 *	 5	basic functions
 *	 3	JPEG decompressor
 *	 1	scanner info and capabilities
 *	 0	errors
 */

#include "sane/config.h"

#include <ctype.h>

#include "sane/saneopts.h"
#include "sane/sanei_config.h"

#include "epsonds.h"
#include "epsonds-usb.h"
#include "epsonds-io.h"
#include "epsonds-cmd.h"
#include "epsonds-ops.h"
#include "epsonds-jpeg.h"

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
	NULL
};

static const SANE_String_Const adf_mode_list[] = {
	SANE_I18N("Simplex"),
	SANE_I18N("Duplex"),
	NULL
};

/* Define the different scan sources */

#define FBF_STR	SANE_I18N("Flatbed")
#define ADF_STR	SANE_I18N("Automatic Document Feeder")

/* order will be fixed: fb, adf, tpu */
SANE_String_Const source_list[] = {
	NULL,
	NULL,
	NULL,
	NULL
};

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

static void
close_scanner(epsonds_scanner *s)
{
	DBG(7, "%s: fd = %d\n", __func__, s->fd);

	if (s->fd == -1)
		goto free;

	if (s->locked) {
		DBG(7, " unlocking scanner\n");
		esci2_fin(s);
	}

	if (s->hw->connection == SANE_EPSONDS_USB) {
		sanei_usb_close(s->fd);
	}

free:

	free(s->front.ring);
	free(s->back.ring);
	free(s->line_buffer);
	free(s);

	DBG(7, "%s: ZZZ\n", __func__);
}

static SANE_Status
open_scanner(epsonds_scanner *s)
{
	SANE_Status status = SANE_STATUS_INVAL;

	DBG(7, "%s: %s\n", __func__, s->hw->sane.name);

	if (s->fd != -1) {
		DBG(5, "scanner is already open: fd = %d\n", s->fd);
		return SANE_STATUS_GOOD;	/* no need to open the scanner */
	}

	if (s->hw->connection == SANE_EPSONDS_USB) {

		status = sanei_usb_open(s->hw->sane.name, &s->fd);
		sanei_usb_set_timeout(USB_TIMEOUT);

	} else {
		DBG(1, "unknown connection type: %d\n", s->hw->connection);
	}

	if (status == SANE_STATUS_ACCESS_DENIED) {
		DBG(1, "please check that you have permissions on the device.\n");
		DBG(1, "if this is a multi-function device with a printer,\n");
		DBG(1, "disable any conflicting driver (like usblp).\n");
	}

	if (status != SANE_STATUS_GOOD)
		DBG(1, "%s open failed: %s\n",
			s->hw->sane.name,
			sane_strstatus(status));
	else
		DBG(5, " opened correctly\n");

	return status;
}

static int num_devices;			/* number of scanners attached to backend */
static epsonds_device *first_dev;	/* first EPSON scanner in list */

static struct epsonds_scanner *
scanner_create(struct epsonds_device *dev, SANE_Status *status)
{
	struct epsonds_scanner *s;

	s = malloc(sizeof(struct epsonds_scanner));
	if (s == NULL) {
		*status = SANE_STATUS_NO_MEM;
		return NULL;
	}

	/* clear verything */
	memset(s, 0x00, sizeof(struct epsonds_scanner));

	s->fd = -1;
	s->hw = dev;

	return s;
}

static struct epsonds_scanner *
device_detect(const char *name, int type, SANE_Status *status)
{
	struct epsonds_scanner *s;
	struct epsonds_device *dev;

	DBG(1, "%s\n", __func__);

	/* try to find the device in our list */
	for (dev = first_dev; dev; dev = dev->next) {
		if (strcmp(dev->sane.name, name) == 0) {
			DBG(1, " found cached device\n");
			return scanner_create(dev, status);
		}
	}

	/* not found, create new if valid */
	if (type == SANE_EPSONDS_NODEV) {
		*status = SANE_STATUS_INVAL;
		return NULL;
	}

	/* alloc and clear our device structure */
	dev = malloc(sizeof(*dev));
	if (!dev) {
		*status = SANE_STATUS_NO_MEM;
		return NULL;
	}
	memset(dev, 0x00, sizeof(struct epsonds_device));

	s = scanner_create(dev, status);
	if (s == NULL)
		return NULL;

	dev->connection = type;
	dev->model = strdup("(undetermined)");

	dev->sane.name = name;
	dev->sane.vendor = "Epson";
	dev->sane.model = dev->model;
	dev->sane.type = "ESC/I-2";

	*status = open_scanner(s);
	if (*status != SANE_STATUS_GOOD) {
		free(s);
		return NULL;
	}

	eds_dev_init(dev);

	/* lock scanner */
	*status = eds_lock(s);
	if (*status != SANE_STATUS_GOOD) {
		goto close;
	}

	/* discover capabilities */
	*status = esci2_info(s);
	if (*status != SANE_STATUS_GOOD)
		goto close;

	*status = esci2_capa(s);
	if (*status != SANE_STATUS_GOOD)
		goto close;

	*status = esci2_resa(s);
	if (*status != SANE_STATUS_GOOD)
		goto close;

	/* assume 1 and 8 bit are always supported */
	eds_add_depth(s->hw, 1);
	eds_add_depth(s->hw, 8);

	/* setup area according to available options */
	if (s->hw->has_fb) {

		dev->x_range = &dev->fbf_x_range;
		dev->y_range = &dev->fbf_y_range;
		dev->alignment = dev->fbf_alignment;

	} else if (s->hw->has_adf) {

		dev->x_range = &dev->adf_x_range;
		dev->y_range = &dev->adf_y_range;
		dev->alignment = dev->adf_alignment;

	} else {
		DBG(0, "unable to lay on the flatbed or feed the feeder. is that a scanner??\n");
	}

	*status = eds_dev_post_init(dev);
	if (*status != SANE_STATUS_GOOD)
		goto close;

	DBG(1, "scanner model: %s\n", dev->model);

	/* add this scanner to the device list */

	num_devices++;
	dev->next = first_dev;
	first_dev = dev;

	return s;

close:
	DBG(1, " failed\n");

	close_scanner(s);
	return NULL;
}


static SANE_Status
attach(const char *name, int type)
{
	SANE_Status status;
	epsonds_scanner * s;

	DBG(7, "%s: devname = %s, type = %d\n", __func__, name, type);

	s = device_detect(name, type, &status);
	if (s == NULL)
		return status;

	close_scanner(s);
	return status;
}

SANE_Status
attach_one_usb(const char *dev)
{
	DBG(7, "%s: dev = %s\n", __func__, dev);
	return attach(dev, SANE_EPSONDS_USB);
}

static SANE_Status
attach_one_config(SANEI_Config __sane_unused__ *config, const char *line)
{
	int vendor, product;

	int len = strlen(line);

	DBG(7, "%s: len = %d, line = %s\n", __func__, len, line);

	if (sscanf(line, "usb %i %i", &vendor, &product) == 2) {

		DBG(7, " user configured device\n");

		if (vendor != SANE_EPSONDS_VENDOR_ID)
			return SANE_STATUS_INVAL; /* this is not an Epson device */

		sanei_usb_attach_matching_devices(line, attach_one_usb);

	} else if (strncmp(line, "usb", 3) == 0 && len == 3) {

		int i, numIds;

		DBG(7, " probing usb devices\n");

		numIds = epsonds_get_number_of_ids();

		for (i = 0; i < numIds; i++) {
			sanei_usb_find_devices(SANE_EPSONDS_VENDOR_ID,
					epsonds_usb_product_ids[i], attach_one_usb);
		}

	} else {
		DBG(0, "unable to parse config line: %s\n", line);
	}

	return SANE_STATUS_GOOD;
}

static void
free_devices(void)
{
	epsonds_device *dev, *next;

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
	sanei_configure_attach(EPSONDS_CONFIG_FILE, NULL, attach_one_config);
}

/**** SANE API ****/

SANE_Status
sane_init(SANE_Int *version_code, SANE_Auth_Callback __sane_unused__ authorize)
{
	DBG_INIT();
	DBG(2, "%s: " PACKAGE " " VERSION "\n", __func__);

	DBG(1, "epsonds backend, version %i.%i.%i\n",
		EPSONDS_VERSION, EPSONDS_REVISION, EPSONDS_BUILD);

	if (version_code != NULL)
		*version_code = SANE_VERSION_CODE(SANE_CURRENT_MAJOR, V_MINOR,
					  EPSONDS_BUILD);

	sanei_usb_init();

	return SANE_STATUS_GOOD;
}

void
sane_exit(void)
{
	DBG(5, "** %s\n", __func__);
	free_devices();
}

SANE_Status
sane_get_devices(const SANE_Device ***device_list, SANE_Bool __sane_unused__ local_only)
{
	int i;
	epsonds_device *dev;

	DBG(5, "** %s\n", __func__);

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
init_options(epsonds_scanner *s)
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

	/* bit depth */
	s->opt[OPT_DEPTH].name = SANE_NAME_BIT_DEPTH;
	s->opt[OPT_DEPTH].title = SANE_TITLE_BIT_DEPTH;
	s->opt[OPT_DEPTH].desc = SANE_DESC_BIT_DEPTH;
	s->opt[OPT_DEPTH].type = SANE_TYPE_INT;
	s->opt[OPT_DEPTH].unit = SANE_UNIT_BIT;
	s->opt[OPT_DEPTH].constraint_type = SANE_CONSTRAINT_WORD_LIST;
	s->opt[OPT_DEPTH].constraint.word_list = s->hw->depth_list;
	s->val[OPT_DEPTH].w = s->hw->depth_list[1];	/* the first "real" element is the default */

	/* default is Lineart, disable depth selection */
	s->opt[OPT_DEPTH].cap |= SANE_CAP_INACTIVE;

	/* resolution */
	s->opt[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
	s->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
	s->opt[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;

	s->opt[OPT_RESOLUTION].type = SANE_TYPE_INT;
	s->opt[OPT_RESOLUTION].unit = SANE_UNIT_DPI;

	/* range */
	if (s->hw->dpi_range.quant) {
		s->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[OPT_RESOLUTION].constraint.range = &s->hw->dpi_range;
		s->val[OPT_RESOLUTION].w = s->hw->dpi_range.min;
	} else { /* list */
		s->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
		s->opt[OPT_RESOLUTION].constraint.word_list = s->hw->res_list;
		s->val[OPT_RESOLUTION].w = s->hw->res_list[1];
	}

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
	s->val[OPT_SOURCE].w = 0;

	s->opt[OPT_EJECT].name = "eject";
	s->opt[OPT_EJECT].title = SANE_I18N("Eject");
	s->opt[OPT_EJECT].desc = SANE_I18N("Eject the sheet in the ADF");
	s->opt[OPT_EJECT].type = SANE_TYPE_BUTTON;

	if (!s->hw->adf_has_eject)
		s->opt[OPT_EJECT].cap |= SANE_CAP_INACTIVE;

	s->opt[OPT_LOAD].name = "load";
	s->opt[OPT_LOAD].title = SANE_I18N("Load");
	s->opt[OPT_LOAD].desc = SANE_I18N("Load a sheet in the ADF");
	s->opt[OPT_LOAD].type = SANE_TYPE_BUTTON;

	if (!s->hw->adf_has_load)
		s->opt[OPT_LOAD].cap |= SANE_CAP_INACTIVE;

	s->opt[OPT_ADF_MODE].name = "adf-mode";
	s->opt[OPT_ADF_MODE].title = SANE_I18N("ADF Mode");
	s->opt[OPT_ADF_MODE].desc =
		SANE_I18N("Selects the ADF mode (simplex/duplex)");
	s->opt[OPT_ADF_MODE].type = SANE_TYPE_STRING;
	s->opt[OPT_ADF_MODE].size = max_string_size(adf_mode_list);
	s->opt[OPT_ADF_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
	s->opt[OPT_ADF_MODE].constraint.string_list = adf_mode_list;
	s->val[OPT_ADF_MODE].w = 0; /* simplex */

	if (!s->hw->adf_is_duplex)
		s->opt[OPT_ADF_MODE].cap |= SANE_CAP_INACTIVE;

	s->opt[OPT_ADF_SKEW].name = "adf-skew";
	s->opt[OPT_ADF_SKEW].title = SANE_I18N("ADF Skew Correction");
	s->opt[OPT_ADF_SKEW].desc =
		SANE_I18N("Enables ADF skew correction");
	s->opt[OPT_ADF_SKEW].type = SANE_TYPE_BOOL;
	s->val[OPT_ADF_SKEW].w = 0;

	if (!s->hw->adf_has_skew)
		s->opt[OPT_ADF_SKEW].cap |= SANE_CAP_INACTIVE;

	return SANE_STATUS_GOOD;
}

SANE_Status
sane_open(SANE_String_Const name, SANE_Handle *handle)
{
	SANE_Status status;
	epsonds_scanner *s = NULL;

	DBG(7, "** %s: name = '%s'\n", __func__, name);

	/* probe if empty device name provided */
	if (name[0] == '\0') {

		probe_devices();

		if (first_dev == NULL) {
			DBG(1, "no devices detected\n");
			return SANE_STATUS_INVAL;
		}

		s = device_detect(first_dev->sane.name, first_dev->connection,
					&status);
		if (s == NULL) {
			DBG(1, "cannot open a perfectly valid device (%s),"
				" please report to the authors\n", name);
			return SANE_STATUS_INVAL;
		}

	} else {

		if (strncmp(name, "libusb:", 7) == 0) {
			s = device_detect(name, SANE_EPSONDS_USB, &status);
			if (s == NULL)
				return status;
		} else {
			DBG(1, "invalid device name: %s\n", name);
			return SANE_STATUS_INVAL;
		}
	}

	/* s is always valid here */

	DBG(5, "%s: handle obtained\n", __func__);

	init_options(s);

	*handle = (SANE_Handle)s;

	status = open_scanner(s);
	if (status != SANE_STATUS_GOOD) {
		free(s);
		return status;
	}

	/* lock scanner if required */
	if (!s->locked) {
		status = eds_lock(s);
	}

	return status;
}

void
sane_close(SANE_Handle handle)
{
	epsonds_scanner *s = (epsonds_scanner *)handle;

	DBG(1, "** %s\n", __func__);

	close_scanner(s);
}

const SANE_Option_Descriptor *
sane_get_option_descriptor(SANE_Handle handle, SANE_Int option)
{
	epsonds_scanner *s = (epsonds_scanner *) handle;

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

static void
activateOption(epsonds_scanner *s, SANE_Int option, SANE_Bool *change)
{
	if (!SANE_OPTION_IS_ACTIVE(s->opt[option].cap)) {
		s->opt[option].cap &= ~SANE_CAP_INACTIVE;
		*change = SANE_TRUE;
	}
}

static void
deactivateOption(epsonds_scanner *s, SANE_Int option, SANE_Bool *change)
{
	if (SANE_OPTION_IS_ACTIVE(s->opt[option].cap)) {
		s->opt[option].cap |= SANE_CAP_INACTIVE;
		*change = SANE_TRUE;
	}
}

/*
 * Handles setting the source (flatbed, transparency adapter (TPU),
 * or auto document feeder (ADF)).
 *
 * For newer scanners it also sets the focus according to the
 * glass / TPU settings.
 */

static void
change_source(epsonds_scanner *s, SANE_Int optindex, char *value)
{
	int force_max = SANE_FALSE;
	SANE_Bool dummy;

	DBG(1, "%s: optindex = %d, source = '%s'\n", __func__, optindex,
	    value);

	s->val[OPT_SOURCE].w = optindex;

	/* if current selected area is the maximum available,
	 * keep this setting on the new source.
	 */
	if (s->val[OPT_TL_X].w == s->hw->x_range->min
	    && s->val[OPT_TL_Y].w == s->hw->y_range->min
	    && s->val[OPT_BR_X].w == s->hw->x_range->max
	    && s->val[OPT_BR_Y].w == s->hw->y_range->max) {
		force_max = SANE_TRUE;
	}

	if (strcmp(ADF_STR, value) == 0) {

		s->hw->x_range = &s->hw->adf_x_range;
		s->hw->y_range = &s->hw->adf_y_range;
		s->hw->alignment = s->hw->adf_alignment;

		if (s->hw->adf_is_duplex) {
			activateOption(s, OPT_ADF_MODE, &dummy);
		} else {
			deactivateOption(s, OPT_ADF_MODE, &dummy);
			s->val[OPT_ADF_MODE].w = 0;
		}

	} else if (strcmp(TPU_STR, value) == 0) {

		s->hw->x_range = &s->hw->tpu_x_range;
		s->hw->y_range = &s->hw->tpu_y_range;

		deactivateOption(s, OPT_ADF_MODE, &dummy);

	} else {

		/* neither ADF nor TPU active, assume FB */
		s->hw->x_range = &s->hw->fbf_x_range;
		s->hw->y_range = &s->hw->fbf_y_range;
		s->hw->alignment = s->hw->fbf_alignment;
	}

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
}

static SANE_Status
getvalue(SANE_Handle handle, SANE_Int option, void *value)
{
	epsonds_scanner *s = (epsonds_scanner *)handle;
	SANE_Option_Descriptor *sopt = &(s->opt[option]);
	Option_Value *sval = &(s->val[option]);

	DBG(17, "%s: option = %d\n", __func__, option);

	switch (option) {

	case OPT_NUM_OPTS:
	case OPT_RESOLUTION:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_DEPTH:
	case OPT_ADF_SKEW:
		*((SANE_Word *) value) = sval->w;
		break;

	case OPT_MODE:
	case OPT_ADF_MODE:
	case OPT_SOURCE:
		strcpy((char *) value, sopt->constraint.string_list[sval->w]);
		break;

	default:
		return SANE_STATUS_INVAL;
	}

	return SANE_STATUS_GOOD;
}

static SANE_Status
setvalue(SANE_Handle handle, SANE_Int option, void *value, SANE_Int *info)
{
	epsonds_scanner *s = (epsonds_scanner *) handle;
	SANE_Option_Descriptor *sopt = &(s->opt[option]);
	Option_Value *sval = &(s->val[option]);

	SANE_Status status;
	const SANE_String_Const *optval = NULL;
	int optindex = 0;
	SANE_Bool reload = SANE_FALSE;

	DBG(17, "** %s: option = %d, value = %p\n", __func__, option, value);

	status = sanei_constrain_value(sopt, value, info);
	if (status != SANE_STATUS_GOOD)
		return status;

	if (info && value && (*info & SANE_INFO_INEXACT)
	    && sopt->type == SANE_TYPE_INT)
		DBG(17, " constrained val = %d\n", *(SANE_Word *) value);

	if (sopt->constraint_type == SANE_CONSTRAINT_STRING_LIST) {
		optval = search_string_list(sopt->constraint.string_list,
					    (char *) value);
		if (optval == NULL)
			return SANE_STATUS_INVAL;
		optindex = optval - sopt->constraint.string_list;
	}

	/* block faulty frontends */
	if (sopt->cap & SANE_CAP_INACTIVE) {
		DBG(1, " tried to modify a disabled parameter");
		return SANE_STATUS_INVAL;
	}

	switch (option) {

	case OPT_ADF_MODE: /* simple lists */
		sval->w = optindex;
		break;

	case OPT_ADF_SKEW:
	case OPT_RESOLUTION:
		sval->w = *((SANE_Word *) value);
		reload = SANE_TRUE;
		break;

	case OPT_BR_X:
	case OPT_BR_Y:
		sval->w = *((SANE_Word *) value);
		if (SANE_UNFIX(sval->w) == 0) {
			DBG(17, " invalid br-x or br-y\n");
			return SANE_STATUS_INVAL;
		}
		/* passthru */
	case OPT_TL_X:
	case OPT_TL_Y:
		sval->w = *((SANE_Word *) value);
		if (NULL != info)
			*info |= SANE_INFO_RELOAD_PARAMS;
		break;

	case OPT_SOURCE:
		change_source(s, optindex, (char *) value);
		reload = SANE_TRUE;
		break;

	case OPT_MODE:
	{
		/* use JPEG mode if RAW is not available when bpp > 1 */
		if (optindex > 0 && !s->hw->has_raw) {
			s->mode_jpeg = 1;
		} else {
			s->mode_jpeg = 0;
		}

		sval->w = optindex;

		/* if binary, then disable the bit depth selection */
		if (optindex == 0) {
			s->opt[OPT_DEPTH].cap |= SANE_CAP_INACTIVE;
		} else {
			if (s->hw->depth_list[0] == 1)
				s->opt[OPT_DEPTH].cap |= SANE_CAP_INACTIVE;
			else {
				s->opt[OPT_DEPTH].cap &= ~SANE_CAP_INACTIVE;
				s->val[OPT_DEPTH].w =
					mode_params[optindex].depth;
			}
		}

		reload = SANE_TRUE;
		break;
	}

	case OPT_DEPTH:
		sval->w = *((SANE_Word *) value);
		mode_params[s->val[OPT_MODE].w].depth = sval->w;
		reload = SANE_TRUE;
		break;

	case OPT_LOAD:
		esci2_mech(s, "#ADFLOAD");
		break;

	case OPT_EJECT:
		esci2_mech(s, "#ADFEJCT");
		break;

	default:
		return SANE_STATUS_INVAL;
	}

	if (reload && info != NULL)
		*info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;

	return SANE_STATUS_GOOD;
}

SANE_Status
sane_control_option(SANE_Handle handle, SANE_Int option, SANE_Action action,
		    void *value, SANE_Int *info)
{
	DBG(17, "** %s: action = %x, option = %d\n", __func__, action, option);

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
	epsonds_scanner *s = (epsonds_scanner *)handle;

	DBG(5, "** %s\n", __func__);

	if (params == NULL)
		DBG(1, "%s: params is NULL\n", __func__);

	/*
	 * If sane_start was already called, then just retrieve the parameters
	 * from the scanner data structure
	 */
	if (s->scanning) {
		DBG(5, "scan in progress, returning saved params structure\n");
	} else {
		/* otherwise initialize the params structure */
		eds_init_parameters(s);
	}

	if (params != NULL)
		*params = s->params;

	print_params(s->params);

	return SANE_STATUS_GOOD;
}

/*
 * This function is part of the SANE API and gets called from the front end to
 * start the scan process.
 */

SANE_Status
sane_start(SANE_Handle handle)
{
	epsonds_scanner *s = (epsonds_scanner *)handle;
	char buf[64];
	char cmd[100]; /* take care not to overflow */
	SANE_Status status = 0;

	s->pages++;

	DBG(5, "** %s, pages = %d, scanning = %d, backside = %d, front fill: %d, back fill: %d\n",
		__func__, s->pages, s->scanning, s->backside,
		eds_ring_avail(&s->front),
		eds_ring_avail(&s->back));

	s->eof = 0;
	s->canceling = 0;

	if ((s->pages % 2) == 1) {
		s->current = &s->front;
		eds_ring_flush(s->current);
	} else if (eds_ring_avail(&s->back)) {
		DBG(5, "back side\n");
		s->current = &s->back;
	}

	/* prepare the JPEG decompressor */
	if (s->mode_jpeg) {
		status = eds_jpeg_start(s);
		if (status != SANE_STATUS_GOOD) {
			goto end;
	}	}

	/* scan already in progress? (one pass adf) */
	if (s->scanning) {
		DBG(5, " scan in progress, returning early\n");
		return SANE_STATUS_GOOD;
	}

	/* calc scanning parameters */
	status = eds_init_parameters(s);
	if (status != SANE_STATUS_GOOD) {
		DBG(1, " parameters initialization failed\n");
		return status;
	}

	/* allocate line buffer */
	s->line_buffer = realloc(s->line_buffer, s->params.bytes_per_line);
	if (s->line_buffer == NULL)
		return SANE_STATUS_NO_MEM;

	/* ring buffer for front page, twice bsz */
	/* XXX read value from scanner */
	status = eds_ring_init(&s->front, (65536 * 4) * 2);
	if (status != SANE_STATUS_GOOD) {
		return status;
	}

	/* transfer buffer, bsz */
	/* XXX read value from scanner */
	s->buf = realloc(s->buf, 65536 * 4);
	if (s->buf == NULL)
		return SANE_STATUS_NO_MEM;

	print_params(s->params);

	/* set scanning parameters */

	/* document source */
	if (strcmp(source_list[s->val[OPT_SOURCE].w], ADF_STR) == 0) {

		sprintf(buf, "#ADF%s%s",
			s->val[OPT_ADF_MODE].w ? "DPLX" : "",
			s->val[OPT_ADF_SKEW].w ? "SKEW" : "");

		if (s->hw->adf_has_dfd == 2) {
			strcat(buf, "DFL2");
		} else if (s->hw->adf_has_dfd == 1) {
			strcat(buf, "DFL1");
		}

	} else if (strcmp(source_list[s->val[OPT_SOURCE].w], FBF_STR) == 0) {

		strcpy(buf, "#FB ");

	} else {
		/* XXX */
	}

	strcpy(cmd, buf);

	if (s->params.format == SANE_FRAME_GRAY) {
		sprintf(buf, "#COLM%03d", s->params.depth);
	} else if (s->params.format == SANE_FRAME_RGB) {
		sprintf(buf, "#COLC%03d", s->params.depth * 3);
	}

	strcat(cmd, buf);

	/* image transfer format */
	if (!s->mode_jpeg) {
		if (s->params.depth > 1 || s->hw->has_raw) {
			strcat(cmd, "#FMTRAW ");
		}
	} else {
		strcat(cmd, "#FMTJPG #JPGd090");
	}

	/* resolution (RSMi not always supported) */

	if (s->val[OPT_RESOLUTION].w > 999) {
		sprintf(buf, "#RSMi%07d", s->val[OPT_RESOLUTION].w);
	} else {
		sprintf(buf, "#RSMd%03d", s->val[OPT_RESOLUTION].w);
	}

	strcat(cmd, buf);

	/* scanning area */
	sprintf(buf, "#ACQi%07di%07di%07di%07d",
		s->left, s->top, s->params.pixels_per_line, s->params.lines);

	strcat(cmd, buf);

	status = esci2_para(s, cmd);
	if (status != SANE_STATUS_GOOD) {
		goto end;
	}

	/* start scanning */
	DBG(1, "%s: scanning...\n", __func__);

	/* switch to data state */
	status = esci2_trdt(s);
	if (status != SANE_STATUS_GOOD) {
		goto end;
	}

	/* first page is page 1 */
	s->pages = 1;
	s->scanning = 1;

end:
	if (status != SANE_STATUS_GOOD) {
		DBG(1, "%s: start failed: %s\n", __func__, sane_strstatus(status));
	}

	return status;
}

/* this moves data from our buffers to SANE */

SANE_Status
sane_read(SANE_Handle handle, SANE_Byte *data, SANE_Int max_length,
	  SANE_Int *length)
{
	SANE_Int read = 0, tries = 3;
	SANE_Int available;
	SANE_Status status = 0;
	epsonds_scanner *s = (epsonds_scanner *)handle;

	*length = read = 0;

	DBG(20, "** %s: backside = %d\n", __func__, s->backside);

	/* sane_read called before sane_start? */
	if (s->current == NULL) {
		DBG(0, "%s: buffer is NULL", __func__);
		return SANE_STATUS_INVAL;
	}

	/* anything in the buffer? pass it to the frontend */
	available = eds_ring_avail(s->current);
	if (available) {

		DBG(18, "reading from ring buffer, %d left\n", available);

		if (s->mode_jpeg && !s->jpeg_header_seen) {

			status = eds_jpeg_read_header(s);
			if (status != SANE_STATUS_GOOD && --tries) {
				goto read_again;
			}
		}

		if (s->mode_jpeg) {
			eds_jpeg_read(handle, data, max_length, &read);
		} else {
			eds_copy_image_from_ring(s, data, max_length, &read);
		}

		if (read == 0) {
			goto read_again;
		}

		*length = read;

		return SANE_STATUS_GOOD;


	} else if (s->current == &s->back) {

		/* finished reading the back page, next
		 * command should give us the EOF
		 */
		DBG(18, "back side ring buffer empty\n");
	}

	/* read until data or error */

read_again:

	status = esci2_img(s, &read);
	if (status != SANE_STATUS_GOOD) {
		DBG(20, "read: %d, eof: %d, backside: %d, status: %d\n", read, s->eof, s->backside, status);
	}

	/* just got a back side page, alloc ring buffer if necessary
	 * we didn't before because dummy was not known
	 */
	if (s->backside) {

		int required = s->params.lines * (s->params.bytes_per_line + s->dummy);

		if (s->back.size < required) {

			DBG(20, "allocating buffer for the back side\n");

			status = eds_ring_init(&s->back, required);
			if (status != SANE_STATUS_GOOD) {
				return status;
			}
		}
	}

	/* abort scanning when appropriate */
	if (status == SANE_STATUS_CANCELLED) {
		esci2_can(s);
		return status;
	}

	if (s->eof && s->backside) {
		DBG(18, "back side scan finished\n");
	}

	/* read again if no error and no data */
	if (read == 0 && status == SANE_STATUS_GOOD) {
		goto read_again;
	}

	/* got something, write to ring */
	if (read) {

		DBG(20, " %d bytes read, %d lines, eof: %d, canceling: %d, status: %d, backside: %d\n",
			read, read / (s->params.bytes_per_line + s->dummy),
			s->canceling, s->eof, status, s->backside);

		/* move data to the appropriate ring */
		status = eds_ring_write(s->backside ? &s->back : &s->front, s->buf, read);

		if (0 && s->mode_jpeg && !s->jpeg_header_seen
			&& status == SANE_STATUS_GOOD) {

			status = eds_jpeg_read_header(s);
			if (status != SANE_STATUS_GOOD && --tries) {
				goto read_again;
			}
		}
	}

	/* continue reading if appropriate */
	if (status == SANE_STATUS_GOOD)
		return status;

	/* cleanup */
	DBG(5, "** %s: cleaning up\n", __func__);

	if (s->mode_jpeg) {
		eds_jpeg_finish(s);
	}

	eds_ring_flush(s->current);

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
	DBG(1, "** %s\n", __func__);
	((epsonds_scanner *)handle)->canceling = SANE_TRUE;
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
