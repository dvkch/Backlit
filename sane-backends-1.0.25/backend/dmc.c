/* sane - Scanner Access Now Easy.
   Copyright (C) 1998 David F. Skoll
   Heavily based on "hp.c" driver for HP Scanners, by 
   David Mosberger-Tang.

   This file is part of the SANE package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.

   As a special exception, the authors of SANE give permission for
   additional uses of the libraries contained in this release of SANE.

   The exception is that, if you link a SANE library with other files
   to produce an executable, this does not by itself cause the
   resulting executable to be covered by the GNU General Public
   License.  Your use of that executable is in no way restricted on
   account of linking the SANE library code into it.

   This exception does not, however, invalidate any other reasons why
   the executable file might be covered by the GNU General Public
   License.

   If you submit changes to SANE to the maintainers to be included in
   a subsequent release, you agree by submitting the changes that
   those changes may be distributed with this exception intact.

   If you write modifications of your own for SANE, it is your choice
   whether to permit this exception to apply to your modifications.
   If you do not wish that, delete this exception notice.

   This file implements a SANE backend for the Polaroid Digital
   Microscope Camera. */

/* $Id$ */

#include "../include/sane/config.h"

#include <limits.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "../include/_stdint.h"

#include "../include/sane/sane.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_scsi.h"

#define BACKEND_NAME	dmc
#include "../include/sane/sanei_backend.h"

#ifndef PATH_MAX
# define PATH_MAX	1024
#endif

#include "../include/sane/sanei_config.h"
#define DMC_CONFIG_FILE "dmc.conf"

#include "dmc.h"

/* A linked-list of attached devices and handles */
static DMC_Device *FirstDevice = NULL;
static DMC_Camera *FirstHandle = NULL;
static int NumDevices = 0;
static SANE_Device const **devlist = NULL;

static SANE_String_Const ValidModes[] = { "Full frame", "Viewfinder",
					  "Raw", "Thumbnail",
					  "Super-Resolution",
					  NULL };

static SANE_String_Const ValidBalances[] = { "Daylight", "Incandescent",
					     "Fluorescent", NULL };

static SANE_Word ValidASAs[] = { 3, 25, 50, 100 };

/* Convert between 32-us ticks and milliseconds */
#define MS_TO_TICKS(x) (((x) * 1000 + 16) / 32)
#define TICKS_TO_MS(x) (((x) * 32) / 1000)

/* Macros for stepping along the raw lines for super-resolution mode
   They are very ugly because they handle boundary conditions at
   the edges of the image.  Yuck... */

#define PREV_RED(i) (((i)/3)*3)
#define NEXT_RED(i) (((i) >= BYTES_PER_RAW_LINE-3) ? BYTES_PER_RAW_LINE-3 : \
		     PREV_RED(i)+3)
#define PREV_GREEN(i) ((i)<1 ? 1 : PREV_RED((i)-1)+1)
#define NEXT_GREEN(i) ((i)<1 ? 1 : ((i) >= BYTES_PER_RAW_LINE-2) ? \
		       BYTES_PER_RAW_LINE-2 : PREV_GREEN(i)+3)
#define PREV_BLUE(i) ((i)<2 ? 2 : PREV_RED((i)-2)+2)
#define NEXT_BLUE(i) ((i)<2 ? 2 : ((i) >= BYTES_PER_RAW_LINE-1) ? \
		      BYTES_PER_RAW_LINE-1 : PREV_BLUE(i)+3)

#define ADVANCE_COEFF(i) (((i)==1) ? 3 : (i)-1);

/**********************************************************************
//%FUNCTION: DMCRead
//%ARGUMENTS:
// fd -- file descriptor
// typecode -- data type code
// qualifier -- data type qualifier
// maxlen -- tranfer length
// buf -- buffer to store data in
// len -- set to actual length of data
//%RETURNS:
// A SANE status code
//%DESCRIPTION:
// Reads the particular data selected by typecode and qualifier
// *********************************************************************/
static SANE_Status
DMCRead(int fd, unsigned int typecode, unsigned int qualifier,
	SANE_Byte *buf, size_t maxlen, size_t *len)
{
    uint8_t readCmd[10];
    SANE_Status status;

    readCmd[0] = 0x28;
    readCmd[1] = 0;
    readCmd[2] = typecode;
    readCmd[3] = 0;
    readCmd[4] = (qualifier >> 8) & 0xFF;
    readCmd[5] = qualifier & 0xFF;
    readCmd[6] = (maxlen >> 16) & 0xFF;
    readCmd[7] = (maxlen >> 8) & 0xFF;
    readCmd[8] = maxlen & 0xFF;
    readCmd[9] = 0;
    DBG(3, "DMCRead: typecode=%x, qualifier=%x, maxlen=%lu\n",
	typecode, qualifier, (u_long) maxlen);

    *len = maxlen;
    status = sanei_scsi_cmd(fd, readCmd, sizeof(readCmd), buf, len);
    DBG(3, "DMCRead: Read %lu bytes\n", (u_long) *len);
    return status;
}

/**********************************************************************
//%FUNCTION: DMCWrite
//%ARGUMENTS:
// fd -- file descriptor
// typecode -- data type code
// qualifier -- data type qualifier
// maxlen -- tranfer length
// buf -- buffer to store data in
//%RETURNS:
// A SANE status code
//%DESCRIPTION:
// Writes the particular data selected by typecode and qualifier
// *********************************************************************/
static SANE_Status
DMCWrite(int fd, unsigned int typecode, unsigned int qualifier,
	SANE_Byte *buf, size_t maxlen)
{
    uint8_t *writeCmd;
    SANE_Status status;

    writeCmd = malloc(maxlen + 10);
    if (!writeCmd) return SANE_STATUS_NO_MEM;

    writeCmd[0] = 0x2A;
    writeCmd[1] = 0;
    writeCmd[2] = typecode;
    writeCmd[3] = 0;
    writeCmd[4] = (qualifier >> 8) & 0xFF;
    writeCmd[5] = qualifier & 0xFF;
    writeCmd[6] = (maxlen >> 16) & 0xFF;
    writeCmd[7] = (maxlen >> 8) & 0xFF;
    writeCmd[8] = maxlen & 0xFF;
    writeCmd[9] = 0;
    memcpy(writeCmd+10, buf, maxlen);

    DBG(3, "DMCWrite: typecode=%x, qualifier=%x, maxlen=%lu\n",
	typecode, qualifier, (u_long) maxlen);

    status = sanei_scsi_cmd(fd, writeCmd, 10+maxlen, NULL, NULL);
    free(writeCmd);
    return status;
}

/**********************************************************************
//%FUNCTION: DMCAttach
//%ARGUMENTS:
// devname -- name of device file to open
// devp -- a DMC_Device structure which we fill in if it's not NULL.
//%RETURNS:
// SANE_STATUS_GOOD -- We have a Polaroid DMC attached and all looks good.
// SANE_STATUS_INVAL -- There's a problem.
//%DESCRIPTION:
// Verifies that a Polaroid DMC is attached.  Sets up device options in
// DMC_Device structure.
// *********************************************************************/
#define INQ_LEN 255
static SANE_Status
DMCAttach(char const *devname, DMC_Device **devp)
{
    DMC_Device *dev;
    SANE_Status status;
    int fd;
    size_t size;
    char result[INQ_LEN];

    uint8_t exposureCalculationResults[16];
    uint8_t userInterfaceSettings[16];

    static uint8_t const inquiry[] =
    { 0x12, 0x00, 0x00, 0x00, INQ_LEN, 0x00 };

    static uint8_t const test_unit_ready[] =
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    static uint8_t const no_viewfinder[] =
    { 0xC6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    /* If we're already attached, do nothing */

    for (dev = FirstDevice; dev; dev = dev->next) {
	if (!strcmp(dev->sane.name, devname)) {
	    if (devp) *devp = dev;
	    return SANE_STATUS_GOOD;
	}
    }

    DBG(3, "DMCAttach: opening `%s'\n", devname);
    status = sanei_scsi_open(devname, &fd, 0, 0);
    if (status != SANE_STATUS_GOOD) {
	DBG(1, "DMCAttach: open failed (%s)\n", sane_strstatus(status));
	return status;
    }

    DBG(3, "DMCAttach: sending INQUIRY\n");
    size = sizeof(result);
    status = sanei_scsi_cmd(fd, inquiry, sizeof(inquiry), result, &size);
    if (status != SANE_STATUS_GOOD || size < 32) {
	if (status == SANE_STATUS_GOOD) status = SANE_STATUS_INVAL;
	DBG(1, "DMCAttach: inquiry failed (%s)\n", sane_strstatus(status));
	sanei_scsi_close(fd);
	return status;
    }

    /* Verify that we have a Polaroid DMC */

    if (result[0] != 6 ||
	strncmp(result+8, "POLAROID", 8) ||
	strncmp(result+16, "DMC     ", 8)) {
	sanei_scsi_close(fd);
	DBG(1, "DMCAttach: Device does not look like a Polaroid DMC\n");
	return SANE_STATUS_INVAL;
    }

    DBG(3, "DMCAttach: sending TEST_UNIT_READY\n");
    status = sanei_scsi_cmd(fd, test_unit_ready, sizeof(test_unit_ready),
			    NULL, NULL);
    if (status != SANE_STATUS_GOOD) {
	DBG(1, "DMCAttach: test unit ready failed (%s)\n",
	    sane_strstatus(status));
	sanei_scsi_close(fd);
	return status;
    }

    /* Read current ASA and shutter speed settings */
    status = DMCRead(fd, 0x87, 0x4, exposureCalculationResults,
		     sizeof(exposureCalculationResults), &size);
    if (status != SANE_STATUS_GOOD ||
	size < sizeof(exposureCalculationResults)) {
	DBG(1, "DMCAttach: Couldn't read exposure calculation results (%s)\n",
	    sane_strstatus(status));
	sanei_scsi_close(fd);
	if (status == SANE_STATUS_GOOD) status = SANE_STATUS_IO_ERROR;
	return status;
    }

    /* Read current white balance settings */
    status = DMCRead(fd, 0x82, 0x0, userInterfaceSettings,
		     sizeof(userInterfaceSettings), &size);
    if (status != SANE_STATUS_GOOD ||
	size < sizeof(userInterfaceSettings)) {
	DBG(1, "DMCAttach: Couldn't read user interface settings (%s)\n",
	    sane_strstatus(status));
	sanei_scsi_close(fd);
	if (status == SANE_STATUS_GOOD) status = SANE_STATUS_IO_ERROR;
	return status;
    }

    /* Shut off viewfinder mode */
    status = sanei_scsi_cmd(fd, no_viewfinder, sizeof(no_viewfinder),
			    NULL, NULL);
    if (status != SANE_STATUS_GOOD) {
	sanei_scsi_close(fd);
	return status;
    }
    sanei_scsi_close(fd);

    DBG(3, "DMCAttach: Looks like we have a Polaroid DMC\n");

    dev = malloc(sizeof(*dev));
    if (!dev) return SANE_STATUS_NO_MEM;
    memset(dev, 0, sizeof(*dev));

    dev->sane.name = strdup(devname);
    dev->sane.vendor = "Polaroid";
    dev->sane.model = "DMC";
    dev->sane.type = "still camera";
    dev->next = FirstDevice;
    dev->whiteBalance = userInterfaceSettings[5];
    if (dev->whiteBalance > WHITE_BALANCE_FLUORESCENT) {
	dev->whiteBalance = WHITE_BALANCE_FLUORESCENT;
    }

    /* Bright Eyes documentation gives these as shutter speed ranges (ms) */
    /* dev->shutterSpeedRange.min = 8; */
    /* dev->shutterSpeedRange.max = 320; */

    /* User's manual says these are shutter speed ranges (ms) */
    dev->shutterSpeedRange.min = 8;
    dev->shutterSpeedRange.max = 1000;
    dev->shutterSpeedRange.quant = 2;
    dev->shutterSpeed =
	(exposureCalculationResults[10] << 8) +
	exposureCalculationResults[11];

    /* Convert from ticks to ms */
    dev->shutterSpeed = TICKS_TO_MS(dev->shutterSpeed);

    dev->asa = exposureCalculationResults[13];
    if (dev->asa > ASA_100) dev->asa = ASA_100;
    dev->asa = ValidASAs[dev->asa + 1];
    FirstDevice = dev;
    NumDevices++;
    if (devp) *devp = dev;
    return SANE_STATUS_GOOD;
}

/**********************************************************************
//%FUNCTION: ValidateHandle
//%ARGUMENTS:
// handle -- a handle for an opened camera
//%RETURNS:
// A validated pointer to the camera or NULL if handle is not valid.
// *********************************************************************/
static DMC_Camera *
ValidateHandle(SANE_Handle handle)
{
    DMC_Camera *c;
    for (c = FirstHandle; c; c = c->next) {
	if (c == handle) return c;
    }
    DBG(1, "ValidateHandle: invalid handle %p\n", handle);
    return NULL;
}

/**********************************************************************
//%FUNCTION: DMCInitOptions
//%ARGUMENTS:
// c -- a DMC camera device
//%RETURNS:
// SANE_STATUS_GOOD -- OK
// SANE_STATUS_INVAL -- There's a problem.
//%DESCRIPTION:
// Initializes the options in the DMC_Camera structure
// *********************************************************************/
static SANE_Status
DMCInitOptions(DMC_Camera *c)
{
    int i;

    /* Image is initially 801x600 */
    c->tl_x_range.min = 0;
    c->tl_x_range.max = c->tl_x_range.min;
    c->tl_x_range.quant = 1;
    c->tl_y_range.min = 0;
    c->tl_y_range.max = c->tl_y_range.min;
    c->tl_y_range.quant = 1;

    c->br_x_range.min = 800;
    c->br_x_range.max = c->br_x_range.min;
    c->br_x_range.quant = 1;
    c->br_y_range.min = 599;
    c->br_y_range.max = c->br_y_range.min;
    c->br_y_range.quant = 1;

    memset(c->opt, 0, sizeof(c->opt));
    memset(c->val, 0, sizeof(c->val));

    for (i=0; i<NUM_OPTIONS; i++) {
	c->opt[i].type = SANE_TYPE_INT;
	c->opt[i].size = sizeof(SANE_Word);
	c->opt[i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	c->opt[i].unit = SANE_UNIT_NONE;
    }

    c->opt[OPT_NUM_OPTS].name = SANE_NAME_NUM_OPTIONS;
    c->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
    c->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
    c->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
    c->opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
    c->opt[OPT_NUM_OPTS].constraint_type = SANE_CONSTRAINT_NONE;
    c->val[OPT_NUM_OPTS].w = NUM_OPTIONS;

    c->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
    c->opt[OPT_GEOMETRY_GROUP].name = "";
    c->opt[OPT_GEOMETRY_GROUP].title = "Geometry";
    c->opt[OPT_GEOMETRY_GROUP].desc = "";
    c->opt[OPT_GEOMETRY_GROUP].cap = SANE_CAP_ADVANCED;
    c->opt[OPT_GEOMETRY_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

    /* top-left x */
    c->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
    c->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
    c->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
    c->opt[OPT_TL_X].type = SANE_TYPE_INT;
    c->opt[OPT_TL_X].unit = SANE_UNIT_PIXEL;
    c->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
    c->opt[OPT_TL_X].constraint.range = &c->tl_x_range;
    c->val[OPT_TL_X].w = c->tl_x_range.min;
    
    /* top-left y */
    c->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
    c->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
    c->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
    c->opt[OPT_TL_Y].type = SANE_TYPE_INT;
    c->opt[OPT_TL_Y].unit = SANE_UNIT_PIXEL;
    c->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
    c->opt[OPT_TL_Y].constraint.range = &c->tl_y_range;
    c->val[OPT_TL_Y].w = c->tl_y_range.min;
    
    /* bottom-right x */
    c->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
    c->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
    c->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
    c->opt[OPT_BR_X].type = SANE_TYPE_INT;
    c->opt[OPT_BR_X].unit = SANE_UNIT_PIXEL;
    c->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
    c->opt[OPT_BR_X].constraint.range = &c->br_x_range;
    c->val[OPT_BR_X].w = c->br_x_range.min;
    
    /* bottom-right y */
    c->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
    c->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
    c->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
    c->opt[OPT_BR_Y].type = SANE_TYPE_INT;
    c->opt[OPT_BR_Y].unit = SANE_UNIT_PIXEL;
    c->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
    c->opt[OPT_BR_Y].constraint.range = &c->br_y_range;
    c->val[OPT_BR_Y].w = c->br_y_range.min;

    c->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
    c->opt[OPT_MODE_GROUP].name = "";
    c->opt[OPT_MODE_GROUP].title = "Imaging Mode";
    c->opt[OPT_MODE_GROUP].desc = "";
    c->opt[OPT_MODE_GROUP].cap = SANE_CAP_ADVANCED;
    c->opt[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

    c->opt[OPT_IMAGE_MODE].name = "imagemode";
    c->opt[OPT_IMAGE_MODE].title = "Image Mode";
    c->opt[OPT_IMAGE_MODE].desc = "Selects image mode: 800x600 full frame, 270x201 viewfinder mode, 1599x600 \"raw\" image, 80x60 thumbnail image or 1599x1200 \"super-resolution\" image";
    c->opt[OPT_IMAGE_MODE].type = SANE_TYPE_STRING;
    c->opt[OPT_IMAGE_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
    c->opt[OPT_IMAGE_MODE].constraint.string_list = ValidModes;
    c->opt[OPT_IMAGE_MODE].size = 16;
    c->val[OPT_IMAGE_MODE].s = "Full frame";

    c->opt[OPT_ASA].name = "asa";
    c->opt[OPT_ASA].title = "ASA Setting";
    c->opt[OPT_ASA].desc = "Equivalent ASA setting";
    c->opt[OPT_ASA].constraint_type = SANE_CONSTRAINT_WORD_LIST;
    c->opt[OPT_ASA].constraint.word_list = ValidASAs;
    c->val[OPT_ASA].w = c->hw->asa;

    c->opt[OPT_SHUTTER_SPEED].name = "shutterspeed";
    c->opt[OPT_SHUTTER_SPEED].title = "Shutter Speed (ms)";
    c->opt[OPT_SHUTTER_SPEED].desc = "Shutter Speed in milliseconds";
    c->opt[OPT_SHUTTER_SPEED].constraint_type = SANE_CONSTRAINT_RANGE;
    c->opt[OPT_SHUTTER_SPEED].constraint.range = &c->hw->shutterSpeedRange;
    c->val[OPT_SHUTTER_SPEED].w = c->hw->shutterSpeed;
    
    c->opt[OPT_WHITE_BALANCE].name = "whitebalance";
    c->opt[OPT_WHITE_BALANCE].title = "White Balance";
    c->opt[OPT_WHITE_BALANCE].desc = "Selects white balance";
    c->opt[OPT_WHITE_BALANCE].type = SANE_TYPE_STRING;
    c->opt[OPT_WHITE_BALANCE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
    c->opt[OPT_WHITE_BALANCE].constraint.string_list = ValidBalances;
    c->opt[OPT_WHITE_BALANCE].size = 16;
    c->val[OPT_WHITE_BALANCE].s = (SANE_String) ValidBalances[c->hw->whiteBalance];

    return SANE_STATUS_GOOD;
}

/**********************************************************************
//%FUNCTION: DMCSetMode
//%ARGUMENTS:
// c -- a DMC camera device
// mode -- Imaging mode
//%RETURNS:
// SANE_STATUS_GOOD -- OK
// SANE_STATUS_INVAL -- There's a problem.
//%DESCRIPTION:
// Sets the camera's imaging mode.
// *********************************************************************/
static SANE_Status
DMCSetMode(DMC_Camera *c, int mode)
{
    switch(mode) {
    case IMAGE_MFI:
	c->tl_x_range.min = 0;
	c->tl_x_range.max = c->tl_x_range.max;
	c->tl_y_range.min = 0;
	c->tl_y_range.max = c->tl_y_range.max;
	c->br_x_range.min = 800;
	c->br_x_range.max = c->br_x_range.max;
	c->br_y_range.min = 599;
	c->br_y_range.max = c->br_y_range.max;
	break;
    case IMAGE_VIEWFINDER:
	c->tl_x_range.min = 0;
	c->tl_x_range.max = c->tl_x_range.max;
	c->tl_y_range.min = 0;
	c->tl_y_range.max = c->tl_y_range.max;
	c->br_x_range.min = 269;
	c->br_x_range.max = c->br_x_range.max;
	c->br_y_range.min = 200;
	c->br_y_range.max = c->br_y_range.max;
	break;
    case IMAGE_RAW:
	c->tl_x_range.min = 0;
	c->tl_x_range.max = c->tl_x_range.max;
	c->tl_y_range.min = 0;
	c->tl_y_range.max = c->tl_y_range.max;
	c->br_x_range.min = 1598;
	c->br_x_range.max = c->br_x_range.max;
	c->br_y_range.min = 599;
	c->br_y_range.max = c->br_y_range.max;
	break;
    case IMAGE_THUMB:
	c->tl_x_range.min = 0;
	c->tl_x_range.max = c->tl_x_range.max;
	c->tl_y_range.min = 0;
	c->tl_y_range.max = c->tl_y_range.max;
	c->br_x_range.min = 79;
	c->br_x_range.max = c->br_x_range.max;
	c->br_y_range.min = 59;
	c->br_y_range.max = c->br_y_range.max;
	break;
    case IMAGE_SUPER_RES:
	c->tl_x_range.min = 0;
	c->tl_x_range.max = c->tl_x_range.max;
	c->tl_y_range.min = 0;
	c->tl_y_range.max = c->tl_y_range.max;
	c->br_x_range.min = 1598;
	c->br_x_range.max = c->br_x_range.max;
	c->br_y_range.min = 1199;
	c->br_y_range.max = c->br_y_range.max;
	break;
    default:
	return SANE_STATUS_INVAL;
    }
    c->imageMode = mode;
    c->val[OPT_TL_X].w = c->tl_x_range.min;
    c->val[OPT_TL_Y].w = c->tl_y_range.min;
    c->val[OPT_BR_X].w = c->br_x_range.min;
    c->val[OPT_BR_Y].w = c->br_y_range.min;
    return SANE_STATUS_GOOD;
}

/**********************************************************************
//%FUNCTION: DMCCancel
//%ARGUMENTS:
// c -- a DMC camera device
//%RETURNS:
// SANE_STATUS_CANCELLED
//%DESCRIPTION:
// Cancels DMC image acquisition
// *********************************************************************/
static SANE_Status
DMCCancel(DMC_Camera *c)
{
    if (c->fd >= 0) {
	sanei_scsi_close(c->fd);
	c->fd = -1;
    }
    return SANE_STATUS_CANCELLED;
}

/**********************************************************************
//%FUNCTION: DMCSetASA
//%ARGUMENTS:
// fd -- SCSI file descriptor
// asa -- the ASA to set
//%RETURNS:
// A sane status value
//%DESCRIPTION:
// Sets the equivalent ASA setting of the camera.
// *********************************************************************/
static SANE_Status
DMCSetASA(int fd, unsigned int asa)
{
    uint8_t exposureCalculationResults[16];
    SANE_Status status;
    size_t len;
    int i;

    DBG(3, "DMCSetAsa: %d\n", asa);
    for (i=1; i<=ASA_100+1; i++) {
	if (asa == (unsigned int) ValidASAs[i]) break;
    }

    if (i > ASA_100+1) return SANE_STATUS_INVAL;

    status = DMCRead(fd, 0x87, 0x4, exposureCalculationResults, 
		     sizeof(exposureCalculationResults), &len);
    if (status != SANE_STATUS_GOOD) return status;
    if (len < sizeof(exposureCalculationResults)) return SANE_STATUS_IO_ERROR;

    exposureCalculationResults[13] = (uint8_t) i - 1;

    return DMCWrite(fd, 0x87, 0x4, exposureCalculationResults,
		    sizeof(exposureCalculationResults));
}

/**********************************************************************
//%FUNCTION: DMCSetWhiteBalance
//%ARGUMENTS:
// fd -- SCSI file descriptor
// mode -- white balance mode
//%RETURNS:
// A sane status value
//%DESCRIPTION:
// Sets the equivalent ASA setting of the camera.
// *********************************************************************/
static SANE_Status
DMCSetWhiteBalance(int fd, int mode)
{
    uint8_t userInterfaceSettings[16];
    SANE_Status status;
    size_t len;

    DBG(3, "DMCSetWhiteBalance: %d\n", mode);
    status = DMCRead(fd, 0x82, 0x0, userInterfaceSettings, 
		     sizeof(userInterfaceSettings), &len);
    if (status != SANE_STATUS_GOOD) return status;
    if (len < sizeof(userInterfaceSettings)) return SANE_STATUS_IO_ERROR;

    userInterfaceSettings[5] = (uint8_t) mode;

    return DMCWrite(fd, 0x82, 0x0, userInterfaceSettings,
		    sizeof(userInterfaceSettings));
}

/**********************************************************************
//%FUNCTION: DMCSetShutterSpeed
//%ARGUMENTS:
// fd -- SCSI file descriptor
// speed -- shutter speed in ms
//%RETURNS:
// A sane status value
//%DESCRIPTION:
// Sets the shutter speed of the camera
// *********************************************************************/
static SANE_Status
DMCSetShutterSpeed(int fd, unsigned int speed)
{
    uint8_t exposureCalculationResults[16];
    SANE_Status status;
    size_t len;

    DBG(3, "DMCSetShutterSpeed: %u\n", speed);
    /* Convert from ms to ticks */
    speed = MS_TO_TICKS(speed);

    status = DMCRead(fd, 0x87, 0x4, exposureCalculationResults,
		     sizeof(exposureCalculationResults), &len);
    if (status != SANE_STATUS_GOOD) return status;
    if (len < sizeof(exposureCalculationResults)) return SANE_STATUS_IO_ERROR;

    exposureCalculationResults[10] = (speed >> 8) & 0xFF;
    exposureCalculationResults[11] = speed & 0xFF;

    return DMCWrite(fd, 0x87, 0x4, exposureCalculationResults,
		    sizeof(exposureCalculationResults));
}

/**********************************************************************
//%FUNCTION: DMCReadTwoSuperResolutionLines
//%ARGUMENTS:
// c -- DMC Camera
// buf -- where to put output.
// lastLine -- if true, these are the last two lines in the super-resolution
//             image to read.
//%RETURNS:
// Nothing
//%DESCRIPTION:
// Reads a single "raw" line from the camera (if needed) and constructs
// two "super-resolution" output lines in "buf"
// *********************************************************************/
static SANE_Status
DMCReadTwoSuperResolutionLines(DMC_Camera *c, SANE_Byte *buf, int lastLine)
{
    SANE_Status status;
    size_t len;

    SANE_Byte *output, *prev;
    int redCoeff, greenCoeff, blueCoeff;
    int red, green, blue;
    int i;

    if (c->nextRawLineValid) {
	memcpy(c->currentRawLine, c->nextRawLine, BYTES_PER_RAW_LINE);
    } else {
	status = DMCRead(c->fd, 0x00, IMAGE_RAW,
			 c->currentRawLine, BYTES_PER_RAW_LINE, &len);
	if (status != SANE_STATUS_GOOD) return status;
    }
    if (!lastLine) {
	status = DMCRead(c->fd, 0x00, IMAGE_RAW,
			 c->nextRawLine, BYTES_PER_RAW_LINE, &len);
	if (status != SANE_STATUS_GOOD) return status;
	c->nextRawLineValid = 1;
    }

    redCoeff = 3;
    greenCoeff = 1;
    blueCoeff = 2;

    /* Do the first super-resolution line */
    output = buf;
    for (i=0; i<BYTES_PER_RAW_LINE; i++) {
	red = redCoeff * c->currentRawLine[PREV_RED(i)] +
	    (3-redCoeff) * c->currentRawLine[NEXT_RED(i)];
	green = greenCoeff * c->currentRawLine[PREV_GREEN(i)] +
	    (3-greenCoeff) * c->currentRawLine[NEXT_GREEN(i)];
	blue = blueCoeff * c->currentRawLine[PREV_BLUE(i)] +
	    (3-blueCoeff) * c->currentRawLine[NEXT_BLUE(i)];
	*output++ = red/3;
	*output++ = green/3;
	*output++ = blue/3;
	redCoeff = ADVANCE_COEFF(redCoeff);
	greenCoeff = ADVANCE_COEFF(greenCoeff);
	blueCoeff = ADVANCE_COEFF(blueCoeff);
    }
    
    /* Do the next super-resolution line and interpolate vertically */
    if (lastLine) {
	memcpy(buf+BYTES_PER_RAW_LINE*3, buf, BYTES_PER_RAW_LINE*3);
	return SANE_STATUS_GOOD;
    }
    redCoeff = 3;
    greenCoeff = 1;
    blueCoeff = 2;

    prev = buf;
    for (i=0; i<BYTES_PER_RAW_LINE; i++) {
	red = redCoeff * c->nextRawLine[PREV_RED(i)] +
	    (3-redCoeff) * c->nextRawLine[NEXT_RED(i)];
	green = greenCoeff * c->nextRawLine[PREV_GREEN(i)] +
	    (3-greenCoeff) * c->nextRawLine[NEXT_GREEN(i)];
	blue = blueCoeff * c->nextRawLine[PREV_BLUE(i)] +
	    (3-blueCoeff) * c->nextRawLine[NEXT_BLUE(i)];
	*output++ = (red/3 + *prev++) / 2;
	*output++ = (green/3 + *prev++) / 2;
	*output++ = (blue/3 + *prev++) / 2;
	redCoeff = ADVANCE_COEFF(redCoeff);
	greenCoeff = ADVANCE_COEFF(greenCoeff);
	blueCoeff = ADVANCE_COEFF(blueCoeff);
    }
    return SANE_STATUS_GOOD;
}

/***********************************************************************
//%FUNCTION: attach_one (static function)
//%ARGUMENTS:
// dev -- device to attach
//%RETURNS:
// SANE_STATUS_GOOD
//%DESCRIPTION:
// tries to attach a device found by sanei_config_attach_matching_devices
// *********************************************************************/
static SANE_Status
attach_one (const char *dev)
{
  DMCAttach (dev, 0);
  return SANE_STATUS_GOOD;
}

/**********************************************************************
//%FUNCTION: sane_init
//%ARGUMENTS:
// version_code -- pointer to where we stick our version code
// authorize -- authorization function
//%RETURNS:
// A sane status value
//%DESCRIPTION:
// Initializes DMC sane system.
// *********************************************************************/
SANE_Status
sane_init(SANE_Int *version_code, SANE_Auth_Callback authorize)
{
    char dev_name[PATH_MAX];
    size_t len;
    FILE *fp;

    authorize = authorize;

    DBG_INIT();
    if (version_code) {
	*version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, 0);
    }

    fp = sanei_config_open(DMC_CONFIG_FILE);
    if (!fp) {
	/* default to /dev/camera instead of insisting on config file */
	if (DMCAttach ("/dev/camera", NULL) != SANE_STATUS_GOOD) {
	    /* OK, try /dev/scanner */
	    DMCAttach("/dev/scanner", NULL);
	}
	return SANE_STATUS_GOOD;
    }

    while (sanei_config_read (dev_name, sizeof (dev_name), fp)) {
	if (dev_name[0] == '#')	{	/* ignore line comments */
	    continue;
	}
	len = strlen (dev_name);

	if (!len) continue;			/* ignore empty lines */

	sanei_config_attach_matching_devices(dev_name, attach_one);
    }
    fclose (fp);
    return SANE_STATUS_GOOD;
}

/**********************************************************************
//%FUNCTION: sane_exit
//%ARGUMENTS:
// None
//%RETURNS:
// Nothing
//%DESCRIPTION:
// Cleans up all the SANE information
// *********************************************************************/
void
sane_exit(void)
{
    DMC_Device *dev, *next;

    /* Close all handles */
    while(FirstHandle) {
	sane_close(FirstHandle);
    }

    /* Free all devices */
    dev = FirstDevice;
    while(dev) {
	next = dev->next;
	free((char *) dev->sane.model);
	free(dev);
	dev = next;
    }
  
    if (devlist)
      free (devlist);
}

/**********************************************************************
//%FUNCTION: sane_get_devices
//%ARGUMENTS:
// device_list -- set to allocated list of devices
// local_only -- ignored
//%RETURNS:
// A SANE status
//%DESCRIPTION:
// Returns a list of all known DMC devices
// *********************************************************************/
SANE_Status
sane_get_devices(SANE_Device const ***device_list, SANE_Bool local_only)
{
    DMC_Device *dev;
    int i = 0;

    local_only = local_only;

    if (devlist) free(devlist);
    devlist = malloc((NumDevices+1) * sizeof(devlist[0]));
    if (!devlist) return SANE_STATUS_NO_MEM;

    for (dev=FirstDevice; dev; dev = dev->next) {
	devlist[i++] = &dev->sane;
    }
    devlist[i] = NULL;

    if (device_list) *device_list = devlist;

    return SANE_STATUS_GOOD;
}

/**********************************************************************
//%FUNCTION: sane_open
//%ARGUMENTS:
// name -- name of device to open
// handle -- set to a handle for the opened device
//%RETURNS:
// A SANE status
//%DESCRIPTION:
// Opens a DMC camera device
// *********************************************************************/
SANE_Status
sane_open(SANE_String_Const name, SANE_Handle *handle)
{
    SANE_Status status;
    DMC_Device *dev;
    DMC_Camera *c;

    /* If we're given a device name, search for it */
    if (*name) {
	for (dev = FirstDevice; dev; dev = dev->next) {
	    if (!strcmp(dev->sane.name, name)) {
		break;
	    }
	}
	if (!dev) {
	    status = DMCAttach(name, &dev);
	    if (status != SANE_STATUS_GOOD) return status;
	}
    } else {
	dev = FirstDevice;
    }

    if (!dev) return SANE_STATUS_INVAL;

    c = malloc(sizeof(*c));
    if (!c) return SANE_STATUS_NO_MEM;

    memset(c, 0, sizeof(*c));

    c->fd = -1;
    c->hw = dev;
    c->readBuffer = NULL;
    c->readPtr = NULL;
    c->imageMode = IMAGE_MFI;
    c->inViewfinderMode = 0;
    c->nextRawLineValid = 0;

    DMCInitOptions(c);

    c->next = FirstHandle;
    FirstHandle = c;
    if (handle) *handle = c;
    return SANE_STATUS_GOOD;
}

/**********************************************************************
//%FUNCTION: sane_close
//%ARGUMENTS:
// handle -- handle of device to close
//%RETURNS:
// A SANE status
//%DESCRIPTION:
// Closes a DMC camera device
// *********************************************************************/
void
sane_close(SANE_Handle handle)
{
    DMC_Camera *prev, *c;
    prev = NULL;
    for (c = FirstHandle; c; c = c->next) {
	if (c == handle) break;
	prev = c;
    }
    if (!c) {
	DBG(1, "close: invalid handle %p\n", handle);
	return;
    }
    DMCCancel(c);

    if (prev) prev->next = c->next;
    else FirstHandle = c->next;

    if (c->readBuffer) {
	free(c->readBuffer);
    }
    free(c);
}

/**********************************************************************
//%FUNCTION: sane_get_option_descriptor
//%ARGUMENTS:
// handle -- handle of device
// option -- option number to retrieve
//%RETURNS:
// An option descriptor or NULL on error
// *********************************************************************/
SANE_Option_Descriptor const *
sane_get_option_descriptor(SANE_Handle handle, SANE_Int option)
{
    DMC_Camera *c = ValidateHandle(handle);
    if (!c) return NULL;

    if ((unsigned) option >= NUM_OPTIONS) return NULL;
    return c->opt + option;
}

/**********************************************************************
//%FUNCTION: sane_control_option
//%ARGUMENTS:
// handle -- handle of device
// option -- option number to retrieve
// action -- what to do with the option
// val -- value to set option to
// info -- returned info flags
//%RETURNS:
// SANE status
//%DESCRIPTION:
// Sets or queries option values
// *********************************************************************/
SANE_Status
sane_control_option(SANE_Handle handle, SANE_Int option,
		    SANE_Action action, void *val, SANE_Int *info)
{
    DMC_Camera *c;
    SANE_Word cap;
    SANE_Status status;
    int i;

    if (info) *info = 0;

    c = ValidateHandle(handle);
    if (!c) return SANE_STATUS_INVAL;

    if (c->fd >= 0) return SANE_STATUS_DEVICE_BUSY;

    if (option >= NUM_OPTIONS) return SANE_STATUS_INVAL;

    cap = c->opt[option].cap;
    if (!SANE_OPTION_IS_ACTIVE(cap)) return SANE_STATUS_INVAL;

    if (action == SANE_ACTION_GET_VALUE) {
	switch(c->opt[option].type) {
	case SANE_TYPE_INT:
	    * (SANE_Int *) val = c->val[option].w;
	    return SANE_STATUS_GOOD;

	case SANE_TYPE_STRING:
	    strcpy(val, c->val[option].s);
	    return SANE_STATUS_GOOD;

	default:
	    DBG(3, "impossible option type!\n");
	    return SANE_STATUS_INVAL;
	}
    }

    if (action == SANE_ACTION_SET_AUTO) {
	return SANE_STATUS_UNSUPPORTED;
    }

    switch(option) {
    case OPT_IMAGE_MODE:
	for (i=0; i<NUM_IMAGE_MODES; i++) {
	    if (!strcmp(val, ValidModes[i])) {
		status = DMCSetMode(c, i);
		c->val[OPT_IMAGE_MODE].s = (SANE_String) ValidModes[i];
		if (info) *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
		return SANE_STATUS_GOOD;
	    }
	}
	break;
    case OPT_WHITE_BALANCE:
	for (i=0; i<=WHITE_BALANCE_FLUORESCENT; i++) {
	    if (!strcmp(val, ValidBalances[i])) {
		c->val[OPT_WHITE_BALANCE].s = (SANE_String) ValidBalances[i];
		return SANE_STATUS_GOOD;
	    }
	}
	break;
    case OPT_ASA:
	for (i=1; i<= ASA_100+1; i++) {
	    if (* ((SANE_Int *) val) == ValidASAs[i]) {
		c->val[OPT_ASA].w = ValidASAs[i];
		return SANE_STATUS_GOOD;
	    }
	}
	break;
    case OPT_SHUTTER_SPEED:
	if (* (SANE_Int *) val < c->hw->shutterSpeedRange.min ||
	    * (SANE_Int *) val > c->hw->shutterSpeedRange.max) {
	    return SANE_STATUS_INVAL;
	}
	c->val[OPT_SHUTTER_SPEED].w = * (SANE_Int *) val;
	/* Do any roundoff */
	c->val[OPT_SHUTTER_SPEED].w =
	    TICKS_TO_MS(MS_TO_TICKS(c->val[OPT_SHUTTER_SPEED].w));
	if (c->val[OPT_SHUTTER_SPEED].w != * (SANE_Int *) val) {
	    if (info) *info |= SANE_INFO_INEXACT;
	}
			
	return SANE_STATUS_GOOD;

    default:
	/* Should really be INVAL, but just bit-bucket set requests... */
	return SANE_STATUS_GOOD;
    }

    return SANE_STATUS_INVAL;
}

/**********************************************************************
//%FUNCTION: sane_get_parameters
//%ARGUMENTS:
// handle -- handle of device
// params -- set to device parameters
//%RETURNS:
// SANE status
//%DESCRIPTION:
// Returns parameters for current or next image.
// *********************************************************************/
SANE_Status
sane_get_parameters(SANE_Handle handle, SANE_Parameters *params)
{
    DMC_Camera *c = ValidateHandle(handle);
    if (!c) return SANE_STATUS_INVAL;

    if (c->fd < 0) {
	int width, height;
	memset(&c->params, 0, sizeof(c->params));

	width = c->val[OPT_BR_X].w - c->val[OPT_TL_X].w;
	height = c->val[OPT_BR_Y].w - c->val[OPT_TL_Y].w;
	c->params.pixels_per_line = width + 1;
	c->params.lines = height+1;
	c->params.depth = 8;
	c->params.last_frame = SANE_TRUE;
	switch(c->imageMode) {
	case IMAGE_SUPER_RES:
	case IMAGE_MFI:
	case IMAGE_THUMB:
	    c->params.format = SANE_FRAME_RGB;
	    c->params.bytes_per_line = c->params.pixels_per_line * 3;
	    break;
	case IMAGE_RAW:
	case IMAGE_VIEWFINDER:
	    c->params.format = SANE_FRAME_GRAY;
	    c->params.bytes_per_line = c->params.pixels_per_line;
	    break;
	}
    }
    if (params) *params = c->params;
    return SANE_STATUS_GOOD;
}

/**********************************************************************
//%FUNCTION: sane_start
//%ARGUMENTS:
// handle -- handle of device
//%RETURNS:
// SANE status
//%DESCRIPTION:
// Starts acquisition
// *********************************************************************/
SANE_Status
sane_start(SANE_Handle handle)
{
    static uint8_t const acquire[] =
    { 0xC1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    static uint8_t const viewfinder[] =
    { 0xCB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    static uint8_t const no_viewfinder[] =
    { 0xC6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    DMC_Camera *c = ValidateHandle(handle);
    SANE_Status status;
    int i;

    if (!c) return SANE_STATUS_INVAL;

    /* If we're already open, barf -- not sure this is the best status */
    if (c->fd >= 0) return SANE_STATUS_DEVICE_BUSY;

    /* Get rid of old read buffers */
    if (c->readBuffer) {
	free(c->readBuffer);
	c->readBuffer = NULL;
	c->readPtr = NULL;
    }

    c->nextRawLineValid = 0;

    /* Refresh parameter list */
    status = sane_get_parameters(c, NULL);
    if (status != SANE_STATUS_GOOD) return status;

    status = sanei_scsi_open(c->hw->sane.name, &c->fd, NULL, NULL);
    if (status != SANE_STATUS_GOOD) {
	c->fd = -1;
	DBG(1, "DMC: Open of `%s' failed: %s\n",
	    c->hw->sane.name, sane_strstatus(status));
	return status;
    }

    /* Set ASA and shutter speed if they're no longer current */
    if (c->val[OPT_ASA].w != c->hw->asa) {
	status = DMCSetASA(c->fd, c->val[OPT_ASA].w);
	if (status != SANE_STATUS_GOOD) {
	    DMCCancel(c);
	    return status;
	}
	c->hw->asa = c->val[OPT_ASA].w;
    }

    if ((unsigned int) c->val[OPT_SHUTTER_SPEED].w != c->hw->shutterSpeed) {
	status = DMCSetShutterSpeed(c->fd, c->val[OPT_SHUTTER_SPEED].w);
	if (status != SANE_STATUS_GOOD) {
	    DMCCancel(c);
	    return status;
	}
	c->hw->shutterSpeed = c->val[OPT_SHUTTER_SPEED].w;
    }

    /* Set white balance mode if needed */
    for (i=0; i<=WHITE_BALANCE_FLUORESCENT; i++) {
	if (!strcmp(ValidBalances[i], c->val[OPT_WHITE_BALANCE].s)) {
	    if (i != c->hw->whiteBalance) {
		status = DMCSetWhiteBalance(c->fd, i);
		if (status != SANE_STATUS_GOOD) {
		    DMCCancel(c);
		    return status;
		}
		c->hw->whiteBalance = i;
	    }
	}
    }

    /* Flip into viewfinder mode if needed */
    if (c->imageMode == IMAGE_VIEWFINDER && !c->inViewfinderMode) {
	status = sanei_scsi_cmd(c->fd, viewfinder, sizeof(viewfinder),
				NULL, NULL);
	if (status != SANE_STATUS_GOOD) {
	    DMCCancel(c);
	    return status;
	}
	c->inViewfinderMode = 1;
    }

    /* Flip out of viewfinder mode if needed */
    if (c->imageMode != IMAGE_VIEWFINDER && c->inViewfinderMode) {
	status = sanei_scsi_cmd(c->fd, no_viewfinder, sizeof(no_viewfinder),
				NULL, NULL);
	if (status != SANE_STATUS_GOOD) {
	    DMCCancel(c);
	    return status;
	}
	c->inViewfinderMode = 0;
    }


    status = sanei_scsi_cmd(c->fd, acquire, sizeof(acquire), NULL, NULL);
    if (status != SANE_STATUS_GOOD) {
	DMCCancel(c);
	return status;
    }
    c->bytes_to_read = c->params.bytes_per_line * c->params.lines;
    return SANE_STATUS_GOOD;
}

/**********************************************************************
//%FUNCTION: sane_read
//%ARGUMENTS:
// handle -- handle of device
// buf -- destination for data
// max_len -- maximum amount of data to store
// len -- set to actual amount of data stored.
//%RETURNS:
// SANE status
//%DESCRIPTION:
// Reads image data from the camera
// *********************************************************************/
SANE_Status
sane_read(SANE_Handle handle, SANE_Byte *buf, SANE_Int max_len, SANE_Int *len)
{
    SANE_Status status;
    DMC_Camera *c = ValidateHandle(handle);
    size_t size;
    SANE_Int i;

    if (!c) return SANE_STATUS_INVAL;

    if (c->fd < 0) return SANE_STATUS_INVAL;

    if (c->bytes_to_read == 0) {
	if (c->readBuffer) {
	    free(c->readBuffer);
	    c->readBuffer = NULL;
	    c->readPtr = NULL;
	}
	DMCCancel(c);
	return SANE_STATUS_EOF;
    }

    if (max_len == 0) {
	return SANE_STATUS_GOOD;
    }

    if (c->imageMode == IMAGE_SUPER_RES) {
	/* We have to read *two* complete rows... */
	max_len = (max_len / (2*c->params.bytes_per_line)) *
	    (2*c->params.bytes_per_line);
	/* If user is trying to read less than two complete lines, fail */
	if (max_len == 0) return SANE_STATUS_INVAL;
	if ((unsigned int) max_len > c->bytes_to_read) max_len = c->bytes_to_read;
	for (i=0; i<max_len; i += 2*c->params.bytes_per_line) {
	    c->bytes_to_read -= 2*c->params.bytes_per_line;
	    status = DMCReadTwoSuperResolutionLines(c, buf+i,
						    !c->bytes_to_read);
	    if (status != SANE_STATUS_GOOD) return status;
	}
	*len = max_len;
	return SANE_STATUS_GOOD;
    }
	
    if (c->imageMode == IMAGE_MFI || c->imageMode == IMAGE_RAW) {
	/* We have to read complete rows... */
	max_len = (max_len / c->params.bytes_per_line) * c->params.bytes_per_line;

	/* If user is trying to read less than one complete row, fail */
	if (max_len == 0) return SANE_STATUS_INVAL;
	if ((unsigned int) max_len > c->bytes_to_read) max_len = c->bytes_to_read;
	c->bytes_to_read -= (unsigned int) max_len;
	status = DMCRead(c->fd, 0x00, c->imageMode, buf, max_len, &size);
	*len = size;
	return status;
    }

    if ((unsigned int) max_len > c->bytes_to_read) max_len = c->bytes_to_read;
    if (c->readPtr) {
	*len = max_len;
	memcpy(buf, c->readPtr, max_len);
	c->readPtr += max_len;
	c->bytes_to_read -= max_len;
	return SANE_STATUS_GOOD;
    }

    /* Fill the read buffer completely */
    c->readBuffer = malloc(c->bytes_to_read);
    if (!c->readBuffer) return SANE_STATUS_NO_MEM;
    c->readPtr = c->readBuffer;
    status = DMCRead(c->fd, 0x00, c->imageMode, (SANE_Byte *) c->readBuffer,
		     c->bytes_to_read, &size);
    *len = size;
    if (status != SANE_STATUS_GOOD) return status;
    if ((unsigned int) *len != c->bytes_to_read) return SANE_STATUS_IO_ERROR;

    /* Now copy */
    *len = max_len;
    memcpy(buf, c->readPtr, max_len);
    c->readPtr += max_len;
    c->bytes_to_read -= max_len;
    return SANE_STATUS_GOOD;
}

/**********************************************************************
//%FUNCTION: sane_cancel
//%ARGUMENTS:
// handle -- handle of device
//%RETURNS:
// Nothing
//%DESCRIPTION:
// A quick cancellation of the scane
// *********************************************************************/
void
sane_cancel (SANE_Handle handle)
{
    DMC_Camera *c = ValidateHandle(handle);
    if (!c) return;

    DMCCancel(c);
}

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  handle = handle;
  non_blocking = non_blocking;

  return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int *fd)
{
  handle = handle;
  fd = fd;

  return SANE_STATUS_UNSUPPORTED;
}
