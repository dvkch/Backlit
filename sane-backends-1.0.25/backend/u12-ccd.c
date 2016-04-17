/* @file u12-ccd.c
 * @brief here we have the whole code to intialize the CCD and DAC stuff
 *
 * based on sources acquired from Plustek Inc.
 * Copyright (c) 2003-2004 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * History:
 * - 0.01 - initial version
 * - 0.02 - no changes
 * .
 * <hr>
 * This file is part of the SANE package.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 *
 * As a special exception, the authors of SANE give permission for
 * additional uses of the libraries contained in this release of SANE.
 *
 * The exception is that, if you link a SANE library with other files
 * to produce an executable, this does not by itself cause the
 * resulting executable to be covered by the GNU General Public
 * License.  Your use of that executable is in no way restricted on
 * account of linking the SANE library code into it.
 *
 * This exception does not, however, invalidate any other reasons why
 * the executable file might be covered by the GNU General Public
 * License.
 *
 * If you submit changes to SANE to the maintainers to be included in
 * a subsequent release, you agree by submitting the changes that
 * those changes may be distributed with this exception intact.
 *
 * If you write modifications of your own for SANE, it is your choice
 * whether to permit this exception to apply to your modifications.
 * If you do not wish that, delete this exception notice.
 * <hr>
 */

/*************************** some definitions ********************************/

#define _REFLECTION     0
#define _TRANSPARENCY   1
#define _NEGATIVE       2

#define _NUM_OF_CCDREGS_W8143    25
#define _NUM_OF_DACREGS_W8143    10

#define _NUM_OF_CCDREGS_S1224    29
#define _NUM_OF_DACREGS_S1224    7

#define _NUM_OF_CCDREGS_S8531    29
#define _NUM_OF_DACREGS_S8531    9

/*************************** some local vars *********************************/

static pFnVoid u12ccd_InitFunc = NULL;

static RegDef W3797CCDParams[4][_NUM_OF_CCDREGS_W8143] = {
    {
        {0x2c, 0x02}, {0x39, 0x2a}, {0x3a, 0x0a}, {0x3b, 0x37},
        {0x3c, 0x16}, {0x41, 0x0e}, {0x42, 0x90}, {0x43, 0x01},
        {0x44, 0x27}, {0x45, 0x27}, {0x46, 0x01}, {0x47, 0x03},
        {0x48, 0x27}, {0x49, 0x2f}, {0x4a, 0x09}, {0x4b, 0x03},
        {0x4c, 0x07}, {0x4d, 0x06}, {0x4e, 0x06}, {0x67, 0x00},
        {0x50, 0x08}, {0x51, 0x0e}, {0x52, 0x0c}, {0x53, 0x0b},
        {0xf0, 0x00}
    }, {
        {0x2c, 0x02}, {0x39, 0x3d}, {0x3a, 0x04}, {0x3b, 0x46},
        {0x3c, 0x06}, {0x41, 0x1f}, {0x42, 0x8c}, {0x43, 0x01},
        {0x44, 0x13}, {0x45, 0x13}, {0x46, 0xf2}, {0x47, 0x02},
        {0x48, 0x13}, {0x49, 0x13}, {0x4a, 0xfa}, {0x4b, 0x00},
        {0x4c, 0x07}, {0x4d, 0x05}, {0x4e, 0x0e}, {0x67, 0x00},
        {0x50, 0x01}, {0x51, 0x06}, {0x52, 0x09}, {0x53, 0x0b},
        {0xf0, 0x00}
    }, {
        {0x2c, 0x00}, {0x39, 0x16}, {0x3a, 0x03}, {0x3b, 0x1f},
        {0x3c, 0x07}, {0x41, 0x04}, {0x42, 0x1e}, {0x43, 0x01},
        {0x44, 0x13}, {0x45, 0x13}, {0x46, 0xf1}, {0x47, 0x02},
        {0x48, 0x13}, {0x49, 0x13}, {0x4a, 0xf9}, {0x4b, 0x04},
        {0x4c, 0x07}, {0x4d, 0x05}, {0x4e, 0x03}, {0x67, 0x00},
        {0x50, 0x06}, {0x51, 0x03}, {0x52, 0x09}, {0x53, 0x0b},
        {0xf0, 0x00}
    }, {
        {0x2c, 0x02}, {0x39, 0x18}, {0x3a, 0x04}, {0x3b, 0x1d},
        {0x3c, 0x03}, {0x41, 0x0c}, {0x42, 0x84}, {0x43, 0x03},
        {0x44, 0x0a}, {0x45, 0x08}, {0x46, 0xfa}, {0x47, 0x04},
        {0x48, 0x0a}, {0x49, 0x08}, {0x4a, 0xf2}, {0x4b, 0x02},
        {0x4c, 0x03}, {0x4d, 0x02}, {0x4e, 0x0e}, {0x67, 0x00},
        {0x50, 0x00}, {0x51, 0x09}, {0x52, 0x03}, {0x53, 0x03},
        {0xf0, 0x00}
    }
};

static RegDef W3799CCDParams[4][_NUM_OF_CCDREGS_W8143] = {
    {
        {0x2c, 0x02}, {0x39, 0x2c}, {0x3a, 0x05}, {0x3b, 0x3c},
        {0x3c, 0x0e}, {0x41, 0x0e}, {0x42, 0x90}, {0x43, 0x01},
        {0x44, 0x27}, {0x45, 0x27}, {0x46, 0x01}, {0x47, 0x02},
        {0x48, 0x27}, {0x49, 0x2f}, {0x4a, 0x09}, {0x4b, 0x05},
        {0x4c, 0x07}, {0x4d, 0x05}, {0x4e, 0x06}, {0x67, 0x00},
        {0x50, 0x08}, {0x51, 0x0d}, {0x52, 0x0c}, {0x53, 0x0b},
        {0xf0, 0x00}
    }, {
        {0x2c, 0x02}, {0x39, 0x3d}, {0x3a, 0x04}, {0x3b, 0x46},
        {0x3c, 0x06}, {0x41, 0x1f}, {0x42, 0x8c}, {0x43, 0x01},
        {0x44, 0x13}, {0x45, 0x13}, {0x46, 0xf2}, {0x47, 0x01},
        {0x48, 0x13}, {0x49, 0x13}, {0x4a, 0xfa}, {0x4b, 0x00},
        {0x4c, 0x07}, {0x4d, 0x05}, {0x4e, 0x0e}, {0x67, 0x00},
        {0x50, 0x01}, {0x51, 0x06}, {0x52, 0x12}, {0x53, 0x0b},
        {0xf0, 0x00}
    }, {
        {0x2c, 0x00}, {0x39, 0x16}, {0x3a, 0x02}, {0x3b, 0x1a},
        {0x3c, 0x05}, {0x41, 0x04}, {0x42, 0x1e}, {0x43, 0x01},
        {0x44, 0x13}, {0x45, 0x13}, {0x46, 0xf1}, {0x47, 0x01},
        {0x48, 0x13}, {0x49, 0x13}, {0x4a, 0xf9}, {0x4b, 0x04},
        {0x4c, 0x07}, {0x4d, 0x05}, {0x4e, 0x03}, {0x67, 0x00},
        {0x50, 0x06}, {0x51, 0x03}, {0x52, 0x09}, {0x53, 0x0b},
        {0xf0, 0x00}
    }, {
        {0x2c, 0x02}, {0x39, 0x18}, {0x3a, 0x04}, {0x3b, 0x1d},
        {0x3c, 0x03}, {0x41, 0x0c}, {0x42, 0x84}, {0x43, 0x03},
        {0x44, 0x0a}, {0x45, 0x08}, {0x46, 0xfa}, {0x47, 0x03},
        {0x48, 0x0a}, {0x49, 0x08}, {0x4a, 0xf2}, {0x4b, 0x02},
        {0x4c, 0x03}, {0x4d, 0x02}, {0x4e, 0x0e}, {0x67, 0x00},
        {0x50, 0x00}, {0x51, 0x09}, {0x52, 0x03}, {0x53, 0x03},
        {0xf0, 0x00}
    }
};

/* Genius ColorPage Vivid III */
static RegDef W548CCDParams[4][_NUM_OF_CCDREGS_W8143] = {
    {
        {0x2c, 0x02}, {0x39, 0x2c}, {0x3a, 0x05}, {0x3b, 0x3c},
        {0x3c, 0x0e}, {0x41, 0x0e}, {0x42, 0x90}, {0x43, 0x01},
        {0x44, 0x27}, {0x45, 0x27}, {0x46, 0x01}, {0x47, 0x02},
        {0x48, 0x27}, {0x49, 0x2f}, {0x4a, 0x09}, {0x4b, 0x05},
        {0x4c, 0x07}, {0x4d, 0x05}, {0x4e, 0x06}, {0x67, 0x00},
        {0x50, 0x08}, {0x51, 0x0d}, {0x52, 0x0c}, {0x53, 0x0b},
        {0xf0, 0x00}
    }, {
        {0x2c, 0x02}, {0x39, 0x3d}, {0x3a, 0x04}, {0x3b, 0x46},
        {0x3c, 0x06}, {0x41, 0x1f}, {0x42, 0x8c}, {0x43, 0x01},
        {0x44, 0x13}, {0x45, 0x13}, {0x46, 0xf2}, {0x47, 0x01},
        {0x48, 0x13}, {0x49, 0x13}, {0x4a, 0xfa}, {0x4b, 0x00},
        {0x4c, 0x07}, {0x4d, 0x05}, {0x4e, 0x0e}, {0x67, 0x00},
        {0x50, 0x01}, {0x51, 0x06}, {0x52, 0x12}, {0x53, 0x0b},
        {0xf0, 0x00}
    }, {
        {0x2c, 0x00}, {0x39, 0x16}, {0x3a, 0x02}, {0x3b, 0x1a},
        {0x3c, 0x05}, {0x41, 0x04}, {0x42, 0x1e}, {0x43, 0x01},
        {0x44, 0x13}, {0x45, 0x13}, {0x46, 0xf1}, {0x47, 0x01},
        {0x48, 0x13}, {0x49, 0x13}, {0x4a, 0xf9}, {0x4b, 0x04},
        {0x4c, 0x07}, {0x4d, 0x05}, {0x4e, 0x03}, {0x67, 0x00},
        {0x50, 0x06}, {0x51, 0x03}, {0x52, 0x09}, {0x53, 0x0b},
        {0xf0, 0x00}
    }, {
        {0x2c, 0x02}, {0x39, 0x18}, {0x3a, 0x04}, {0x3b, 0x1d},
        {0x3c, 0x03}, {0x41, 0x0c}, {0x42, 0x84}, {0x43, 0x03},
        {0x44, 0x0a}, {0x45, 0x08}, {0x46, 0xfa}, {0x47, 0x03},
        {0x48, 0x0a}, {0x49, 0x08}, {0x4a, 0xf2}, {0x4b, 0x02},
        {0x4c, 0x03}, {0x4d, 0x02}, {0x4e, 0x0e}, {0x67, 0x00},
        {0x50, 0x00}, {0x51, 0x09}, {0x52, 0x03}, {0x53, 0x03},
        {0xf0, 0x00}
    }
};

static RegDef S3797CCDParams[4][_NUM_OF_CCDREGS_S1224] = {
    {
        {0x2c, 0x00}, {0x39, 0x2a}, {0x3a, 0x0a}, {0x3b, 0x37},
        {0x3c, 0x16}, {0x41, 0x2c}, {0x42, 0x9f}, {0x43, 0x01},
        {0x44, 0x27}, {0x45, 0x27}, {0x46, 0x01}, {0x47, 0x03},
        {0x48, 0x27}, {0x49, 0x27}, {0x4a, 0x09}, {0x4b, 0x0b},
        {0x4c, 0x06}, {0x4d, 0x06}, {0x4e, 0x0b}, {0x50, 0x13},
        {0x51, 0x06}, {0x52, 0x06}, {0x53, 0x0b}, {0x67, 0x00},
        {0x6f, 0x20}, {0x70, 0x06}, {0x60, 0x07}, {0x61, 0x9f},
        {0x65, 0x01}
    }, {
        {0x2c, 0x00}, {0x39, 0x3d}, {0x3a, 0x06}, {0x3b, 0x46},
        {0x3c, 0x06}, {0x41, 0x3d}, {0x42, 0x92}, {0x43, 0x01},
        {0x44, 0x13}, {0x45, 0x13}, {0x46, 0xf2}, {0x47, 0x02},
        {0x48, 0x13}, {0x49, 0x13}, {0x4a, 0xfa}, {0x4b, 0x1b},
        {0x4c, 0x06}, {0x4d, 0x06}, {0x4e, 0x0b}, {0x50, 0x23},
        {0x51, 0x06}, {0x52, 0x06}, {0x53, 0x0b}, {0x67, 0x00},
        {0x6f, 0x30}, {0x70, 0x06}, {0x60, 0x17}, {0x61, 0x9f},
        {0x65, 0x01}
    }, {
        {0x2c, 0x02}, {0x39, 0x16}, {0x3a, 0x03}, {0x3b, 0x1f},
        {0x3c, 0x07}, {0x41, 0x1c}, {0x42, 0x99}, {0x43, 0x01},
        {0x44, 0x13}, {0x45, 0x13}, {0x46, 0xf1}, {0x47, 0x02},
        {0x48, 0x13}, {0x49, 0x13}, {0x4a, 0xfa}, {0x4b, 0x09},
        {0x4c, 0x13}, {0x4d, 0x14}, {0x4e, 0x09}, {0x50, 0x09},
        {0x51, 0x14}, {0x52, 0x13}, {0x53, 0x01}, {0x67, 0x00},
        {0x6f, 0xff}, {0x70, 0x7f}, {0x60, 0x04}, {0x61, 0x8f},
        {0x65, 0x01}
    }, {
        {0x2c, 0x02}, {0x39, 0x16}, {0x3a, 0x03}, {0x3b, 0x1f},
        {0x3c, 0x07}, {0x41, 0x1c}, {0x42, 0x99}, {0x43, 0x01},
        {0x44, 0x13}, {0x45, 0x13}, {0x46, 0xf1}, {0x47, 0x02},
        {0x48, 0x13}, {0x49, 0x13}, {0x4a, 0xfa}, {0x4b, 0x09},
        {0x4c, 0x13}, {0x4d, 0x14}, {0x4e, 0x09}, {0x50, 0x09},
        {0x51, 0x14}, {0x52, 0x13}, {0x53, 0x01}, {0x67, 0x00},
        {0x6f, 0xff}, {0x70, 0x7f}, {0x60, 0x04}, {0x61, 0x8f},
        {0x65, 0x01}
    }
};

static RegDef S3799CCDParams[4][_NUM_OF_CCDREGS_S1224] = {
    {
        {0x2c, 0x00}, {0x39, 0x2a}, {0x3a, 0x0a}, {0x3b, 0x37},
        {0x3c, 0x16}, {0x41, 0x2c}, {0x42, 0x8f}, {0x43, 0x01},
	    {0x44, 0x27}, {0x45, 0x27}, {0x46, 0x01}, {0x47, 0x01},
	    {0x48, 0x27}, {0x49, 0x27}, {0x4a, 0x09}, {0x4b, 0x0b},
	    {0x4c, 0x06}, {0x4d, 0x06}, {0x4e, 0x0b}, {0x50, 0x13},
	    {0x51, 0x06}, {0x52, 0x06}, {0x53, 0x0b}, {0x67, 0x00},
	    {0x6f, 0x20}, {0x70, 0x06}, {0x60, 0x07}, {0x61, 0x9f},
	    {0x65, 0x01}
    }, {
        {0x2c, 0x00}, {0x39, 0x3d}, {0x3a, 0x06}, {0x3b, 0x46},
	    {0x3c, 0x06}, {0x41, 0x3d}, {0x42, 0x92}, {0x43, 0x01},
	    {0x44, 0x13}, {0x45, 0x13}, {0x46, 0xf2}, {0x47, 0x01},
	    {0x48, 0x13}, {0x49, 0x13}, {0x4a, 0xfa}, {0x4b, 0x1b},
	    {0x4c, 0x06}, {0x4d, 0x06}, {0x4e, 0x0b}, {0x50, 0x23},
	    {0x51, 0x06}, {0x52, 0x06}, {0x53, 0x0b}, {0x67, 0x00},
	    {0x6f, 0x30}, {0x70, 0x06}, {0x60, 0x17}, {0x61, 0x9f},
	    {0x65, 0x01}
    }, {
        {0x2c, 0x02}, {0x39, 0x16}, {0x3a, 0x03}, {0x3b, 0x1f},
	    {0x3c, 0x07}, {0x41, 0x1c}, {0x42, 0x99}, {0x43, 0x01},
	    {0x44, 0x13}, {0x45, 0x13}, {0x46, 0xf1}, {0x47, 0x01},
	    {0x48, 0x13}, {0x49, 0x13}, {0x4a, 0xfa}, {0x4b, 0x09},
	    {0x4c, 0x13}, {0x4d, 0x14}, {0x4e, 0x09}, {0x50, 0x09},
	    {0x51, 0x14}, {0x52, 0x13}, {0x53, 0x01}, {0x67, 0x00},
	    {0x6f, 0xff}, {0x70, 0x7f}, {0x60, 0x04}, {0x61, 0x8f},
	    {0x65, 0x01}
    }, {
        {0x2c, 0x00}, {0x39, 0x16}, {0x3a, 0x03}, {0x3b, 0x1f},
	    {0x3c, 0x07}, {0x41, 0x1c}, {0x42, 0x99}, {0x43, 0x03},
	    {0x44, 0x0a}, {0x45, 0x08}, {0x46, 0xfa}, {0x47, 0x03},
	    {0x48, 0x0a}, {0x49, 0x08}, {0x4a, 0xf2}, {0x4b, 0x02},
	    {0x4c, 0x03}, {0x4d, 0x02}, {0x4e, 0x0e}, {0x50, 0x00},
	    {0x51, 0x09}, {0x52, 0x03}, {0x53, 0x03}, {0x67, 0x00},
	    {0x6f, 0xff}, {0x70, 0x7f}, {0x60, 0x04}, {0x61, 0x8f},
	    {0x65, 0x01}
    }
};

static RegDef WolfsonDAC8143[_NUM_OF_DACREGS_W8143] = {
    {0x01, 0x01}, {0x02, 0x04}, {0x03, 0x42}, {0x05, 0x10}, {0x20, 0xd0},
    {0x21, 0xd0}, {0x22, 0xd0}, {0x24, 0x00}, {0x25, 0x00},	{0x26, 0x00}
};

static RegDef SamsungDAC8531[_NUM_OF_DACREGS_S8531] = {
    {0x00, 0x51}, {0x02, 0x01}, {0x01, 0x80},
    {0x00, 0x55}, {0x02, 0x01}, {0x01, 0x80},
    {0x00, 0x59}, {0x02, 0x01}, {0x01, 0x80}
};

static RegDef SamsungDAC1224[_NUM_OF_DACREGS_S1224] ={
    {0x00, 0x00}, {0x01, 0x80}, {0x02, 0x80},
    {0x03, 0x80}, {0x04, 0x06}, {0x05, 0x06}, {0x06, 0x06}
};

static ShadingVarDef ShadingVar3797[3] = {
    {
        {{99, 100, 94}}, {{0x30, 0x30, 0x30}},
        {{0x20, 0x20, 0x20}}, {{0x04, 0x00, 0x00}}, {{0xcc, 0xcc, 0xcc}}, 0
    }, {
        {{100, 90, 100}}, {{0x30, 0x30, 0x30}},
        {{0x20, 0x20, 0x20}}, {{0x10, 0x10, 0x10}}, {{0xcc, 0xcc, 0xcc}}, 0
    }, {
        {{90, 90, 90}}, {{0x30, 0x30, 0x30}}, {{0x20, 0x20, 0x20}},
        {{0x10, 0x10, 0x10}}, {{0x80, 0x80, 0x80}}, 0
    }
};

static ShadingVarDef ShadingVar3799[3] = {
    {
        {{100, 97, 92}}, {{0x90, 0xe0, 0x80}},
        {{0x70, 0xc0, 0x60}}, {{0x90, 0x34, 0x3c}}, {{0x80, 0x80, 0x80}}, 0
    }, {
        {{75, 75, 75}}, {{0x30, 0x30, 0x30}},
        {{0x10, 0x10, 0x10}}, {{0x20, 0x20, 0x20}}, {{0x80, 0x80, 0x80}}, 0
    }, {
        {{80, 75, 64}}, {{0x30, 0x30, 0x30}},
        {{0x20, 0x20, 0x20}}, {{0x10, 0x10, 0x10}}, {{0x80, 0x80, 0x80}}, 0
    }
};

/* Genius ColorPage Vivid III */
static ShadingVarDef ShadingVar548[3] = {
    {
        {{100, 97, 92}}, {{0x90, 0xe0, 0x80}},
        {{0x70, 0xc0, 0x60}}, {{0x90, 0x34, 0x3c}}, {{0x80, 0x80, 0x80}}, 0
    }, {
        {{75, 75, 75}}, {{0x30, 0x30, 0x30}},
        {{0x10, 0x10, 0x10}}, {{0x20, 0x20, 0x20}}, {{0x80, 0x80, 0x80}}, 0
    }, {
        {{80, 75, 64}}, {{0x30, 0x30, 0x30}},
        {{0x20, 0x20, 0x20}}, {{0x10, 0x10, 0x10}}, {{0x80, 0x80, 0x80}}, 0
    }
};

static ShadingVarDef ShadingVar3777[3] = {
    {
        {{100, 100, 100}}, {{0x90, 0xe0, 0x80}},
        {{0x70, 0xc0, 0x60}}, {{0x90, 0x34, 0x3c}}, {{0x80, 0x80, 0x80}}, 0
    }, {
        {{75, 75, 75}}, {{0x30, 0x30, 0x30}},
        {{0x10, 0x10, 0x10}}, {{0x20, 0x20, 0x20}}, {{0x80, 0x80, 0x80}}, 0
    }, {
        {{80, 75, 64}}, {{0x30, 0x30, 0x30}},
        {{0x20, 0x20, 0x20}}, {{0x10, 0x10, 0x10}}, {{0x80, 0x80, 0x80}}, 0
    }
};

/*************************** local functions *********************************/

/**
 */
static void fnCCDInitWolfson3797( U12_Device *dev )
{
	if( dev->shade.intermediate & _ScanMode_Mono )
		dev->shade.pCcdDac->DarkDAC.Colors.Green = 0xcc;
	else
		if( dev->shade.intermediate & _ScanMode_AverageOut)
			dev->shade.pCcdDac->DarkDAC.Colors.Green = 0x68;
		else
			dev->shade.pCcdDac->DarkDAC.Colors.Green = 0xa0;

	if((dev->shade.intermediate & _ScanMode_AverageOut) ||
		                       (dev->DataInf.dwScanFlag & _SCANDEF_Negative)) {
		WolfsonDAC8143[3].val = 0x12;
	} else {
		WolfsonDAC8143[3].val = 0x10;
	}
}

/**
 */
static void fnCCDInitSamsung3797( U12_Device *dev )
{
	if(!(dev->DataInf.dwScanFlag & _SCANDEF_TPA)) {

		if (!(dev->shade.intermediate & _ScanMode_AverageOut)) {

			if( dev->PCBID == _OPTICWORKS2000 ) {
        		dev->shade.pCcdDac->GainResize.Colors.Red = 102;
	        	dev->shade.pCcdDac->GainResize.Colors.Green = 102;
        		dev->shade.pCcdDac->GainResize.Colors.Blue = 97;
        		dev->shade.pCcdDac->DarkDAC.Colors.Red = 0x40;
        		dev->shade.pCcdDac->DarkDAC.Colors.Green = 0x40;
        		dev->shade.pCcdDac->DarkDAC.Colors.Blue = 0x40;
		        dev->shade.pCcdDac->DarkCmpHi.Colors.Red = 0x48;
        		dev->shade.pCcdDac->DarkCmpHi.Colors.Green = 0x40;
        		dev->shade.pCcdDac->DarkCmpHi.Colors.Blue = 0x40;
        		dev->shade.pCcdDac->DarkCmpLo.Colors.Red = 0x38;
        		dev->shade.pCcdDac->DarkCmpLo.Colors.Green = 0x30;
        		dev->shade.pCcdDac->DarkCmpLo.Colors.Blue = 0x30;
        		dev->shade.pCcdDac->DarkOffSub.Colors.Red = 0x48;
        		dev->shade.pCcdDac->DarkOffSub.Colors.Green = 0x38;
        		dev->shade.pCcdDac->DarkOffSub.Colors.Blue = 0x40;
	        } else {
        		dev->shade.pCcdDac->GainResize.Colors.Red = 99;
        		dev->shade.pCcdDac->GainResize.Colors.Green = 101;
		        dev->shade.pCcdDac->GainResize.Colors.Blue = 94;
        		dev->shade.pCcdDac->DarkDAC.Colors.Red = 0x40;
        		dev->shade.pCcdDac->DarkDAC.Colors.Green = 0x40;
		        dev->shade.pCcdDac->DarkDAC.Colors.Blue = 0x40;
        		dev->shade.pCcdDac->DarkCmpHi.Colors.Red = 0x30;
        		dev->shade.pCcdDac->DarkCmpHi.Colors.Green = 0x30;
        		dev->shade.pCcdDac->DarkCmpHi.Colors.Blue = 0x30;
        		dev->shade.pCcdDac->DarkCmpLo.Colors.Red = 0x20;
        		dev->shade.pCcdDac->DarkCmpLo.Colors.Green = 0x20;
		        dev->shade.pCcdDac->DarkCmpLo.Colors.Blue = 0x20;
        		dev->shade.pCcdDac->DarkOffSub.Colors.Red = 0x04;
		        dev->shade.pCcdDac->DarkOffSub.Colors.Green = 0x00;
        		dev->shade.pCcdDac->DarkOffSub.Colors.Blue = 0x00;
	        }
    	} else {
            if( dev->PCBID == _OPTICWORKS2000 ) {
    	    	dev->shade.pCcdDac->GainResize.Colors.Red = 100;
    	    	dev->shade.pCcdDac->GainResize.Colors.Green = 100;
	    	    dev->shade.pCcdDac->GainResize.Colors.Blue = 96;
    	    	dev->shade.pCcdDac->DarkDAC.Colors.Red = 0x30;
        		dev->shade.pCcdDac->DarkDAC.Colors.Green = 0x30;
	        	dev->shade.pCcdDac->DarkDAC.Colors.Blue = 0x30;
    	    	dev->shade.pCcdDac->DarkCmpHi.Colors.Red = 0x48;
    	    	dev->shade.pCcdDac->DarkCmpHi.Colors.Green = 0x48;
        		dev->shade.pCcdDac->DarkCmpHi.Colors.Blue = 0x48;
	        	dev->shade.pCcdDac->DarkCmpLo.Colors.Red = 0x38;
        		dev->shade.pCcdDac->DarkCmpLo.Colors.Green = 0x38;
	        	dev->shade.pCcdDac->DarkCmpLo.Colors.Blue = 0x38;
    		    dev->shade.pCcdDac->DarkOffSub.Colors.Red = 0x48;
        		dev->shade.pCcdDac->DarkOffSub.Colors.Green = 0x48;
    	    	dev->shade.pCcdDac->DarkOffSub.Colors.Blue = 0x48;
	        } else {
    	    	dev->shade.pCcdDac->GainResize.Colors.Red = 100;   /*  98 */
    	    	dev->shade.pCcdDac->GainResize.Colors.Green = 103; /* 106 */
	    	    dev->shade.pCcdDac->GainResize.Colors.Blue = 96;	  /*  96 */
    	    	dev->shade.pCcdDac->DarkDAC.Colors.Red = 0x20;
	    	    dev->shade.pCcdDac->DarkDAC.Colors.Green = 0x10;
    		    dev->shade.pCcdDac->DarkDAC.Colors.Blue = 0x10;
        		dev->shade.pCcdDac->DarkCmpHi.Colors.Red = 0x110;
	        	dev->shade.pCcdDac->DarkCmpHi.Colors.Green = 0x1f0;
    		    dev->shade.pCcdDac->DarkCmpHi.Colors.Blue = 0x190;
        		dev->shade.pCcdDac->DarkCmpLo.Colors.Red = 0x100;
	        	dev->shade.pCcdDac->DarkCmpLo.Colors.Green = 0x1e0;
    		    dev->shade.pCcdDac->DarkCmpLo.Colors.Blue = 0x180;
        		dev->shade.pCcdDac->DarkOffSub.Colors.Red = 0x20;
	        	dev->shade.pCcdDac->DarkOffSub.Colors.Green = 0x10;
    		    dev->shade.pCcdDac->DarkOffSub.Colors.Blue = 0x20;
	        }
       	}
    }
}

/**
 */
static void fnCCDInitWolfson3799( U12_Device *dev )
{
    if(!(dev->DataInf.dwScanFlag & _SCANDEF_Negative)) {

        if (!(dev->shade.intermediate & _ScanMode_AverageOut)) {

    	    dev->shade.pCcdDac->GainResize.Colors.Red = 103;
	        dev->shade.pCcdDac->GainResize.Colors.Green = 102;
    	    dev->shade.pCcdDac->GainResize.Colors.Blue = 99;
	        dev->shade.pCcdDac->DarkDAC.Colors.Red = 0xc8;
	        dev->shade.pCcdDac->DarkDAC.Colors.Green = 0xc8;
    	    dev->shade.pCcdDac->DarkDAC.Colors.Blue = 0xc8;
	        dev->shade.pCcdDac->DarkCmpHi.Colors.Red = 0x48;
	        dev->shade.pCcdDac->DarkCmpHi.Colors.Green = 0x30;
    	    dev->shade.pCcdDac->DarkCmpHi.Colors.Blue = 0x30;
	        dev->shade.pCcdDac->DarkCmpLo.Colors.Red = 0x40;
	        dev->shade.pCcdDac->DarkCmpLo.Colors.Green = 0x28;
    	    dev->shade.pCcdDac->DarkCmpLo.Colors.Blue = 0x28;
	        dev->shade.pCcdDac->DarkOffSub.Colors.Red = 0x48;
    	    dev->shade.pCcdDac->DarkOffSub.Colors.Green = 0x18;
	        dev->shade.pCcdDac->DarkOffSub.Colors.Blue = 0x2c;
    	} else {
	        dev->shade.pCcdDac->GainResize.Colors.Red = 100;
    	    dev->shade.pCcdDac->GainResize.Colors.Green = 98;
	        dev->shade.pCcdDac->GainResize.Colors.Blue = 95;
	        dev->shade.pCcdDac->DarkDAC.Colors.Red = 0xd0;
    	    dev->shade.pCcdDac->DarkDAC.Colors.Green = 0xd0;
	        dev->shade.pCcdDac->DarkDAC.Colors.Blue = 0xd0;
	        dev->shade.pCcdDac->DarkCmpHi.Colors.Red = 0x30;
    	    dev->shade.pCcdDac->DarkCmpHi.Colors.Green = 0x30;
	        dev->shade.pCcdDac->DarkCmpHi.Colors.Blue = 0x30;
    	    dev->shade.pCcdDac->DarkCmpLo.Colors.Red = 0x28;
	        dev->shade.pCcdDac->DarkCmpLo.Colors.Green = 0x28;
    	    dev->shade.pCcdDac->DarkCmpLo.Colors.Blue = 0x28;
	        dev->shade.pCcdDac->DarkOffSub.Colors.Red = 0x0;
    	    dev->shade.pCcdDac->DarkOffSub.Colors.Green = 0x0;
	        dev->shade.pCcdDac->DarkOffSub.Colors.Blue = 0x0;
    	}
    } else {
    	dev->shade.pCcdDac->DarkDAC.Colors.Red = 0x80;
	    dev->shade.pCcdDac->DarkDAC.Colors.Green = 0x80;
    	dev->shade.pCcdDac->DarkDAC.Colors.Blue = 0x80;
	    dev->shade.pCcdDac->DarkCmpHi.Colors.Red = 0x28;
    	dev->shade.pCcdDac->DarkCmpHi.Colors.Green = 0x28;
	    dev->shade.pCcdDac->DarkCmpHi.Colors.Blue = 0x28;
    	dev->shade.pCcdDac->DarkCmpLo.Colors.Red = 0x20;
	    dev->shade.pCcdDac->DarkCmpLo.Colors.Green = 0x20;
    	dev->shade.pCcdDac->DarkCmpLo.Colors.Blue = 0x20;
	    dev->shade.pCcdDac->DarkOffSub.Colors.Red = -0x38;
    	dev->shade.pCcdDac->DarkOffSub.Colors.Green = -0x108;
	    dev->shade.pCcdDac->DarkOffSub.Colors.Blue = -0x1c8;
    }
}

/** 
 */
static void fnCCDInitWolfson548( U12_Device *dev )
{
    if (!(dev->shade.intermediate & _ScanMode_AverageOut)) {

 	    dev->shade.pCcdDac->GainResize.Colors.Red   = 103;
        dev->shade.pCcdDac->GainResize.Colors.Green = 102;
   	    dev->shade.pCcdDac->GainResize.Colors.Blue  = 99;
        dev->shade.pCcdDac->DarkDAC.Colors.Red      = 0xc8;
        dev->shade.pCcdDac->DarkDAC.Colors.Green    = 0xc8;
   	    dev->shade.pCcdDac->DarkDAC.Colors.Blue     = 0xc8;
        dev->shade.pCcdDac->DarkCmpHi.Colors.Red    = 0x48;
        dev->shade.pCcdDac->DarkCmpHi.Colors.Green  = 0x30;
   	    dev->shade.pCcdDac->DarkCmpHi.Colors.Blue   = 0x30;
        dev->shade.pCcdDac->DarkCmpLo.Colors.Red    = 0x40;
        dev->shade.pCcdDac->DarkCmpLo.Colors.Green  = 0x28;
   	    dev->shade.pCcdDac->DarkCmpLo.Colors.Blue   = 0x28;
        dev->shade.pCcdDac->DarkOffSub.Colors.Red   = 0x48;
   	    dev->shade.pCcdDac->DarkOffSub.Colors.Green = 0x18;
        dev->shade.pCcdDac->DarkOffSub.Colors.Blue  = 0x2c;

   	} else {
        dev->shade.pCcdDac->GainResize.Colors.Red   = 100;
   	    dev->shade.pCcdDac->GainResize.Colors.Green = 98;
        dev->shade.pCcdDac->GainResize.Colors.Blue  = 95;
        dev->shade.pCcdDac->DarkDAC.Colors.Red      = 0xd0;
   	    dev->shade.pCcdDac->DarkDAC.Colors.Green    = 0xd0;
        dev->shade.pCcdDac->DarkDAC.Colors.Blue     = 0xd0;
        dev->shade.pCcdDac->DarkCmpHi.Colors.Red    = 0x30;
   	    dev->shade.pCcdDac->DarkCmpHi.Colors.Green  = 0x30;
        dev->shade.pCcdDac->DarkCmpHi.Colors.Blue   = 0x30;
   	    dev->shade.pCcdDac->DarkCmpLo.Colors.Red    = 0x28;
        dev->shade.pCcdDac->DarkCmpLo.Colors.Green  = 0x28;
   	    dev->shade.pCcdDac->DarkCmpLo.Colors.Blue   = 0x28;
        dev->shade.pCcdDac->DarkOffSub.Colors.Red   = 0x0;
   	    dev->shade.pCcdDac->DarkOffSub.Colors.Green = 0x0;
        dev->shade.pCcdDac->DarkOffSub.Colors.Blue  = 0x0;
   	}
}

/**
 */
static void fnCCDInitSamsung3777( U12_Device *dev )
{
    if(!(dev->DataInf.dwScanFlag & _SCANDEF_Negative)) {

        if (!(dev->shade.intermediate & _ScanMode_AverageOut)) {

    	    dev->shade.pCcdDac->GainResize.Colors.Red = 109;
	        dev->shade.pCcdDac->GainResize.Colors.Green = 108;
	        dev->shade.pCcdDac->GainResize.Colors.Blue = 105;
    	    dev->shade.pCcdDac->DarkDAC.Colors.Red = 0x4a;
	        dev->shade.pCcdDac->DarkDAC.Colors.Green = 0x4a;
            dev->shade.pCcdDac->DarkDAC.Colors.Blue = 0x4a;
	        dev->shade.pCcdDac->DarkCmpHi.Colors.Red = 0x3c;
    	    dev->shade.pCcdDac->DarkCmpHi.Colors.Green = 0x38;
	        dev->shade.pCcdDac->DarkCmpHi.Colors.Blue = 0x38;
	        dev->shade.pCcdDac->DarkCmpLo.Colors.Red = 0x28;
    	    dev->shade.pCcdDac->DarkCmpLo.Colors.Green = 0x2c;
	        dev->shade.pCcdDac->DarkCmpLo.Colors.Blue = 0x28;
	        dev->shade.pCcdDac->DarkOffSub.Colors.Red = 0x30;
    	    dev->shade.pCcdDac->DarkOffSub.Colors.Green = 0x30;
	        dev->shade.pCcdDac->DarkOffSub.Colors.Blue = 0x3C;
    	} else {
    	    dev->shade.pCcdDac->GainResize.Colors.Red = 108;
	        dev->shade.pCcdDac->GainResize.Colors.Green = 107;
    	    dev->shade.pCcdDac->GainResize.Colors.Blue = 104;
	        dev->shade.pCcdDac->DarkDAC.Colors.Red = 0x50;
	        dev->shade.pCcdDac->DarkDAC.Colors.Green = 0x50;
    	    dev->shade.pCcdDac->DarkDAC.Colors.Blue = 0x50;
    	    dev->shade.pCcdDac->DarkCmpHi.Colors.Red = 0x40;
	        dev->shade.pCcdDac->DarkCmpHi.Colors.Green = 0x40;
	        dev->shade.pCcdDac->DarkCmpHi.Colors.Blue = 0x40;
    	    dev->shade.pCcdDac->DarkCmpLo.Colors.Red = 0x30;
	        dev->shade.pCcdDac->DarkCmpLo.Colors.Green = 0x30;
	        dev->shade.pCcdDac->DarkCmpLo.Colors.Blue = 0x30;
	        dev->shade.pCcdDac->DarkOffSub.Colors.Red = 0x20;
    	    dev->shade.pCcdDac->DarkOffSub.Colors.Green = 0x20;
	        dev->shade.pCcdDac->DarkOffSub.Colors.Blue = 0x20;
	    }
    } else {
    	dev->shade.pCcdDac->DarkDAC.Colors.Red = 0x80;
	    dev->shade.pCcdDac->DarkDAC.Colors.Green = 0x80;
    	dev->shade.pCcdDac->DarkDAC.Colors.Blue = 0x80;
	    dev->shade.pCcdDac->DarkCmpHi.Colors.Red = 0x28;
    	dev->shade.pCcdDac->DarkCmpHi.Colors.Green = 0x28;
    	dev->shade.pCcdDac->DarkCmpHi.Colors.Blue = 0x28;
	    dev->shade.pCcdDac->DarkCmpLo.Colors.Red = 0x20;
    	dev->shade.pCcdDac->DarkCmpLo.Colors.Green = 0x20;
	    dev->shade.pCcdDac->DarkCmpLo.Colors.Blue = 0x20;
    	dev->shade.pCcdDac->DarkOffSub.Colors.Red = -0x38;
	    dev->shade.pCcdDac->DarkOffSub.Colors.Green = -0x108;
    	dev->shade.pCcdDac->DarkOffSub.Colors.Blue = -0x1c8;
    }
}

/**
 */
static void fnCCDInitSamsung3799( U12_Device *dev )
{
    if(!(dev->DataInf.dwScanFlag & _SCANDEF_TPA)) {

        if (!(dev->shade.intermediate & _ScanMode_AverageOut)) {

    	    if( dev->PCBID == _SCANNER2Button ) {
        		dev->shade.pCcdDac->GainResize.Colors.Red = 109;
        		dev->shade.pCcdDac->GainResize.Colors.Green = 109;
		        dev->shade.pCcdDac->GainResize.Colors.Blue = 105;
        		dev->shade.pCcdDac->DarkDAC.Colors.Red = 0x68;
		        dev->shade.pCcdDac->DarkDAC.Colors.Green = 0x68;
        		dev->shade.pCcdDac->DarkDAC.Colors.Blue = 0x68;
		        dev->shade.pCcdDac->DarkCmpHi.Colors.Red = 0x30;
        		dev->shade.pCcdDac->DarkCmpHi.Colors.Green = 0x30;
		        dev->shade.pCcdDac->DarkCmpHi.Colors.Blue = 0x30;
        		dev->shade.pCcdDac->DarkCmpLo.Colors.Red = 0x28;
		        dev->shade.pCcdDac->DarkCmpLo.Colors.Green = 0x28;
        		dev->shade.pCcdDac->DarkCmpLo.Colors.Blue = 0x28;
		        dev->shade.pCcdDac->DarkOffSub.Colors.Red = 0x24;    /* 0 */
        		dev->shade.pCcdDac->DarkOffSub.Colors.Green = 0x20;  /* 0 */
		        dev->shade.pCcdDac->DarkOffSub.Colors.Blue = 0x1c;   /* 0 */
    	    } else {
        		dev->shade.pCcdDac->GainResize.Colors.Red = 98;
		        dev->shade.pCcdDac->GainResize.Colors.Green = 97;
        		dev->shade.pCcdDac->GainResize.Colors.Blue = 92;
		        dev->shade.pCcdDac->DarkDAC.Colors.Red = 0x90;
        		dev->shade.pCcdDac->DarkDAC.Colors.Green = 0x90;
		        dev->shade.pCcdDac->DarkDAC.Colors.Blue = 0x90;
        		dev->shade.pCcdDac->DarkCmpHi.Colors.Red = 0xc0;	    /* 0x90 */
		        dev->shade.pCcdDac->DarkCmpHi.Colors.Green = 0xc0;   /* 0xe0 */
        		dev->shade.pCcdDac->DarkCmpHi.Colors.Blue = 0xc0;    /* 0x80 */
		        dev->shade.pCcdDac->DarkCmpLo.Colors.Red = 0xb0;	    /* 0x70 */
        		dev->shade.pCcdDac->DarkCmpLo.Colors.Green = 0xb0;   /* 0xc0 */
		        dev->shade.pCcdDac->DarkCmpLo.Colors.Blue = 0xb0;    /* 0x60 */
        		dev->shade.pCcdDac->DarkOffSub.Colors.Red = 0x24;    /* 0x90 */
		        dev->shade.pCcdDac->DarkOffSub.Colors.Green = 0x00;	/* 0x34 */
        		dev->shade.pCcdDac->DarkOffSub.Colors.Blue = 0x0c;	/* 0x3c */
	        }
    	} else {
    	    if( dev->PCBID == _SCANNER2Button ) {
        		dev->shade.pCcdDac->GainResize.Colors.Red = 107;
		        dev->shade.pCcdDac->GainResize.Colors.Green = 106;
        		dev->shade.pCcdDac->GainResize.Colors.Blue = 103;
		        dev->shade.pCcdDac->DarkDAC.Colors.Red = 0x48;
        		dev->shade.pCcdDac->DarkDAC.Colors.Green = 0x48;
		        dev->shade.pCcdDac->DarkDAC.Colors.Blue = 0x48;
        		dev->shade.pCcdDac->DarkCmpHi.Colors.Red = 0x30;
		        dev->shade.pCcdDac->DarkCmpHi.Colors.Green = 0x30;
        		dev->shade.pCcdDac->DarkCmpHi.Colors.Blue = 0x30;
		        dev->shade.pCcdDac->DarkCmpLo.Colors.Red = 0x28;
        		dev->shade.pCcdDac->DarkCmpLo.Colors.Green = 0x28;
		        dev->shade.pCcdDac->DarkCmpLo.Colors.Blue = 0x28;
        		dev->shade.pCcdDac->DarkOffSub.Colors.Red = 0x28;    /* 0 */
		        dev->shade.pCcdDac->DarkOffSub.Colors.Green = 0x18;  /* 0 */
        		dev->shade.pCcdDac->DarkOffSub.Colors.Blue = 0x20;   /* 0 */
	        } else {
        		dev->shade.pCcdDac->GainResize.Colors.Red = 104;
		        dev->shade.pCcdDac->GainResize.Colors.Green = 107;
        		dev->shade.pCcdDac->GainResize.Colors.Blue = 99;
		        dev->shade.pCcdDac->DarkDAC.Colors.Red = 0x30;	    /* 0x80 */
        		dev->shade.pCcdDac->DarkDAC.Colors.Green = 0x30;
		        dev->shade.pCcdDac->DarkDAC.Colors.Blue = 0x30;	    /* 0x0a0 */
        		dev->shade.pCcdDac->DarkCmpHi.Colors.Red = 0x150;    /* 0x170 */
		        dev->shade.pCcdDac->DarkCmpHi.Colors.Green = 0x130;  /* 0x90  */
        		dev->shade.pCcdDac->DarkCmpHi.Colors.Blue = 0x110;   /* 0x130 */
		        dev->shade.pCcdDac->DarkCmpLo.Colors.Red = 0x140;    /* 0x150 */
        		dev->shade.pCcdDac->DarkCmpLo.Colors.Green = 0x120;  /* 0x70  */
		        dev->shade.pCcdDac->DarkCmpLo.Colors.Blue = 0x100;   /* 0x120 */
        		dev->shade.pCcdDac->DarkOffSub.Colors.Red = 0xF0;    /* 0x90  */
		        dev->shade.pCcdDac->DarkOffSub.Colors.Green = 0xD4;  /* 0x50  */
        		dev->shade.pCcdDac->DarkOffSub.Colors.Blue = 0xCC;   /* 0x60  */
	        }
        }
    }
}

/**
 */
static void fnCCDInitESIC3799( U12_Device *dev )
{
    if(!(dev->DataInf.dwScanFlag & _SCANDEF_Negative)) {

        if (!(dev->shade.intermediate & _ScanMode_AverageOut)) {

    	    dev->shade.pCcdDac->GainResize.Colors.Red = 100;
	        dev->shade.pCcdDac->GainResize.Colors.Green = 99;
	        dev->shade.pCcdDac->GainResize.Colors.Blue = 94;
    	    dev->shade.pCcdDac->DarkDAC.Colors.Red = 0xc8;
	        dev->shade.pCcdDac->DarkDAC.Colors.Green = 0xc8;
	        dev->shade.pCcdDac->DarkDAC.Colors.Blue = 0xc8;
    	    dev->shade.pCcdDac->DarkCmpHi.Colors.Red = 0x58;
	        dev->shade.pCcdDac->DarkCmpHi.Colors.Green = 0x38;
	        dev->shade.pCcdDac->DarkCmpHi.Colors.Blue = 0x48;
    	    dev->shade.pCcdDac->DarkCmpLo.Colors.Red = 0x48;
	        dev->shade.pCcdDac->DarkCmpLo.Colors.Green = 0x28;
	        dev->shade.pCcdDac->DarkCmpLo.Colors.Blue = 0x38;
    	    dev->shade.pCcdDac->DarkOffSub.Colors.Red = 0x58;
	        dev->shade.pCcdDac->DarkOffSub.Colors.Green = 0x38;
	        dev->shade.pCcdDac->DarkOffSub.Colors.Blue = 0x48;
    	} else {
    	    dev->shade.pCcdDac->GainResize.Colors.Red = 100;
	        dev->shade.pCcdDac->GainResize.Colors.Green = 98;
	        dev->shade.pCcdDac->GainResize.Colors.Blue = 93;
    	    dev->shade.pCcdDac->DarkDAC.Colors.Red = 0xd0;
	        dev->shade.pCcdDac->DarkDAC.Colors.Green = 0xd0;
	        dev->shade.pCcdDac->DarkDAC.Colors.Blue = 0xd0;
    	    dev->shade.pCcdDac->DarkCmpHi.Colors.Red = 0x108;
	        dev->shade.pCcdDac->DarkCmpHi.Colors.Green = 0xf8;
	        dev->shade.pCcdDac->DarkCmpHi.Colors.Blue = 0xc8;
    	    dev->shade.pCcdDac->DarkCmpLo.Colors.Red = 0x100;
	        dev->shade.pCcdDac->DarkCmpLo.Colors.Green = 0xf0;
	        dev->shade.pCcdDac->DarkCmpLo.Colors.Blue = 0xc0;
    	    dev->shade.pCcdDac->DarkOffSub.Colors.Red = 0x108;
	        dev->shade.pCcdDac->DarkOffSub.Colors.Green = 0xf8;
	        dev->shade.pCcdDac->DarkOffSub.Colors.Blue = 0xc8;
    	}
    } else {
    	dev->shade.pCcdDac->DarkDAC.Colors.Red = 0x80;
	    dev->shade.pCcdDac->DarkDAC.Colors.Green = 0x80;
    	dev->shade.pCcdDac->DarkDAC.Colors.Blue = 0x80;
	    dev->shade.pCcdDac->DarkCmpHi.Colors.Red = 0x28;
    	dev->shade.pCcdDac->DarkCmpHi.Colors.Green = 0x28;
	    dev->shade.pCcdDac->DarkCmpHi.Colors.Blue = 0x28;
    	dev->shade.pCcdDac->DarkCmpLo.Colors.Red = 0x20;
	    dev->shade.pCcdDac->DarkCmpLo.Colors.Green = 0x20;
    	dev->shade.pCcdDac->DarkCmpLo.Colors.Blue = 0x20;
	    dev->shade.pCcdDac->DarkOffSub.Colors.Red = -0x38;
    	dev->shade.pCcdDac->DarkOffSub.Colors.Green = -0x38;
	    dev->shade.pCcdDac->DarkOffSub.Colors.Blue = -0x38;
    }
}

/**
 */
static void
fnDarkOffsetWolfson3797( U12_Device *dev, ShadingVarDef *sTbl, u_long dwCh )
{
	if(( dev->shade.DarkOffset.wColors[dwCh] -=
                sTbl->DarkOffSub.wColors[dwCh]) > 0xfff ) {
		dev->shade.DarkOffset.wColors[dwCh] = 0;
	}
}

/**
 */
static void
fnDarkOffsetSamsung3777( U12_Device *dev, ShadingVarDef *sTbl, u_long dwCh )
{
	dev->shade.DarkOffset.wColors[dwCh] += sTbl->DarkOffSub.wColors [dwCh];
}

/**
 */
static void
fnDarkOffsetSamsung3797( U12_Device *dev, ShadingVarDef *sTbl, u_long dwCh )
{
	if( dev->shade.DarkOffset.wColors[dwCh] > sTbl->DarkOffSub.wColors[dwCh] )
		dev->shade.DarkOffset.wColors[dwCh] -= sTbl->DarkOffSub.wColors[dwCh];
	else
		dev->shade.DarkOffset.wColors[dwCh] = 0;
}

/**
 */
static void
fnDarkOffsetSamsung3799( U12_Device *dev, ShadingVarDef *sTbl, u_long dwCh )
{
	if( dev->shade.DarkOffset.wColors[dwCh] > sTbl->DarkOffSub.wColors[dwCh])
		dev->shade.DarkOffset.wColors[dwCh] -= sTbl->DarkOffSub.wColors[dwCh];
	else
		dev->shade.DarkOffset.wColors[dwCh] = 0;
}

/**
 */
static void fnDACDarkWolfson( U12_Device *dev, ShadingVarDef *sTbl,
                              u_long dwCh, u_short wDarkest )
{
	u_short w = dev->shade.DarkDAC.bColors[dwCh];

	if( wDarkest > sTbl->DarkCmpHi.wColors[dwCh] ) {

		wDarkest -= sTbl->DarkCmpHi.wColors[dwCh];
		if( wDarkest > dev->shade.wDarkLevels)
	        w += (u_short)wDarkest / dev->shade.wDarkLevels;
		else
			w++;

		if (w > 0xff)
			w = 0xff;

		if(w != (u_short)dev->shade.DarkDAC.bColors[dwCh] ) {
			dev->shade.DarkDAC.bColors[dwCh] = (SANE_Byte)w;
			dev->shade.fStop                 = SANE_FALSE;
		}
	} else {
		if((wDarkest < sTbl->DarkCmpLo.wColors[dwCh]) &&
                                	        dev->shade.DarkDAC.bColors[dwCh]) {
			if( wDarkest )
				w = (u_short)dev->shade.DarkDAC.bColors[dwCh] - 2U;
			else
				w = (u_short)dev->shade.DarkDAC.bColors[dwCh] - dev->shade.wDarkLevels;

			if ((short) w < 0)
				w = 0;
				
			if( w != (u_short)dev->shade.DarkDAC.bColors[dwCh] ) {
				dev->shade.DarkDAC.bColors [dwCh] = (SANE_Byte)w;
				dev->shade.fStop                  = SANE_FALSE;
			}
		}
	}
}

/**
 */
static void fnDACDarkSamsung( U12_Device *dev, ShadingVarDef *sTbl,
                              u_long dwCh, u_short wDarkest )
{
	u_short w;

	if( wDarkest > sTbl->DarkCmpHi.wColors[dwCh] ) {

		wDarkest -= sTbl->DarkCmpHi.wColors[dwCh];
		if( wDarkest > dev->shade.wDarkLevels )
			w = (u_short)dev->shade.DarkDAC.bColors[dwCh] -
	                                        wDarkest / dev->shade.wDarkLevels;
		else
			w = (u_short)dev->shade.DarkDAC.bColors[dwCh] - 1U;

		if((short) w < 0)
			w = 0;

		if(w != (u_short)dev->shade.DarkDAC.bColors[dwCh]) {
			dev->shade.DarkDAC.bColors [dwCh] = (SANE_Byte)w;
	        dev->shade.fStop                  = SANE_FALSE;
		}
	} else {
		if((wDarkest < sTbl->DarkCmpLo.wColors[dwCh]) &&
		                                    dev->shade.DarkDAC.bColors[dwCh]) {
			if( wDarkest )
				w = (u_short)dev->shade.DarkDAC.bColors[dwCh] + 2U;
			else
				w = dev->shade.wDarkLevels + (u_short)dev->shade.DarkDAC.bColors[dwCh];

			if (w > 0xff)
				w = 0xff;

			if(w != (u_short)dev->shade.DarkDAC.bColors[dwCh]) {
				dev->shade.DarkDAC.bColors[dwCh] = (SANE_Byte)w;
				dev->shade.fStop                 = SANE_FALSE;
			}
		}
	}
}

/************************ exported functions *********************************/

/** according to detected CCD and DAC, we set their correct init values
 *  and functions
 */
static void u12ccd_InitCCDandDAC( U12_Device *dev, SANE_Bool shading )
{
	u_short        w;
	ShadingVarDef *pDAC_CCD;

	DBG( _DBG_INFO, "CCD & DAC init\n" );

	/* some presets */
	dev->f0_8_16 = SANE_FALSE;

	switch( dev->DACType ) {

	case _DA_WOLFSON8143:

		DBG( _DBG_INFO, "* DAC: WOLFSON 8143\n" );
		switch( dev->CCDID ) {

		case _CCD_3797:
			DBG( _DBG_INFO, "* CCD-3797\n" );
			pDAC_CCD          = ShadingVar3797;
			u12ccd_InitFunc   = fnCCDInitWolfson3797;
			dev->fnDarkOffset = fnDarkOffsetWolfson3797;
			dev->fnDACDark    = fnDACDarkWolfson;
			dev->CCDRegs      = (RegDef*)W3797CCDParams;
			break;

		case _CCD_548:
			DBG( _DBG_INFO, "* CCD-548\n" );
			pDAC_CCD          = ShadingVar548;
			u12ccd_InitFunc   = fnCCDInitWolfson548;
			dev->fnDarkOffset = fnDarkOffsetWolfson3797;
			dev->fnDACDark    = fnDACDarkWolfson;
			dev->CCDRegs      = (RegDef*)W548CCDParams;
			break;


		default:
			DBG( _DBG_INFO, "* CCD-3799\n" );
			pDAC_CCD          = ShadingVar3799;
			u12ccd_InitFunc   = fnCCDInitWolfson3799;
/* CHECK: org was to  fnDarkOffsetWolfson3797 */
			dev->fnDarkOffset = fnDarkOffsetWolfson3797;
			dev->fnDACDark    = fnDACDarkWolfson;
			dev->CCDRegs      = (RegDef*)W3799CCDParams;
		}

		dev->numCCDRegs         = _NUM_OF_CCDREGS_W8143;
		dev->numDACRegs         = _NUM_OF_DACREGS_W8143;
		dev->DACRegs            = WolfsonDAC8143;
		dev->RegDACOffset.Red   = 0x20;
		dev->RegDACOffset.Green = 0x21;
		dev->RegDACOffset.Blue  = 0x22;
		dev->RegDACGain.Red     = 0x28;
		dev->RegDACGain.Green   = 0x29;
		dev->RegDACGain.Blue    = 0x2a;

		if( dev->shade.intermediate & _ScanMode_AverageOut ) {
			dev->shade.bUniGain    = 1;
			dev->shade.bGainDouble = 1;
		} else {
			dev->shade.bUniGain    = 2;
			dev->shade.bGainDouble = 4;
		}
		dev->shade.bMinGain    = 1;
		dev->shade.bMaxGain    = 0x1f;
		dev->shade.wDarkLevels = 10;

		if( dev->shade.intermediate == _ScanMode_Color )
			WolfsonDAC8143[2].val = 0x52;
		else
			WolfsonDAC8143[2].val = 0x42;

		if (dev->shade.intermediate == _ScanMode_Mono )
			WolfsonDAC8143[0].val = 7;
		else
			WolfsonDAC8143[0].val = 3;
		break;

	case _DA_SAMSUNG1224:
		DBG( _DBG_INFO, "* DAC: Samsung 1224\n" );

		switch( dev->CCDID ) {
			
		case _CCD_3797:
			DBG( _DBG_INFO, "* CCD-3797\n" );
			pDAC_CCD          = ShadingVar3797;
			u12ccd_InitFunc   = fnCCDInitSamsung3797;
			dev->fnDarkOffset = fnDarkOffsetSamsung3797;
			dev->fnDACDark    = fnDACDarkSamsung;
			dev->CCDRegs      = (RegDef*)S3797CCDParams;
			break;

		default:
			DBG( _DBG_INFO, "* CCD-3799\n" );
			pDAC_CCD          = ShadingVar3799;
			u12ccd_InitFunc   = fnCCDInitSamsung3799;
			dev->fnDarkOffset = fnDarkOffsetSamsung3799;
			dev->fnDACDark    = fnDACDarkSamsung;
			dev->CCDRegs      = (RegDef*)S3799CCDParams;
		}
		dev->numCCDRegs         = _NUM_OF_CCDREGS_S1224;
		dev->numDACRegs         = _NUM_OF_DACREGS_S1224;
		dev->DACRegs            = SamsungDAC1224;
		dev->RegDACOffset.Red   = 1;
		dev->RegDACOffset.Green = 2;
		dev->RegDACOffset.Blue  = 3;
		dev->RegDACGain.Red     = 4;
		dev->RegDACGain.Green   = 5;
		dev->RegDACGain.Blue    = 6;
		dev->shade.bGainDouble  = 6;
		dev->shade.bUniGain     = 7;
		dev->shade.bMinGain     = 0;
		dev->shade.bMaxGain     = 0x1f;
		dev->shade.wDarkLevels  = 10;

		if( dev->shade.intermediate & _ScanMode_Mono )
			SamsungDAC1224[0].val = 0x57;
		else
			SamsungDAC1224[0].val = 0x51;
		break;

	case _DA_ESIC:
		DBG( _DBG_INFO, "* DAC: ESIC\n" );

		switch( dev->CCDID ) {

		case _CCD_3797:
			DBG( _DBG_INFO, "* CCD-3797\n" );
			pDAC_CCD          = ShadingVar3797;
			u12ccd_InitFunc   = fnCCDInitWolfson3797;
			dev->fnDarkOffset = fnDarkOffsetWolfson3797;
			dev->fnDACDark    = fnDACDarkWolfson;
			dev->CCDRegs      = (RegDef*)W3797CCDParams;
			break;

		default:
			DBG( _DBG_INFO, "* CCD-3799\n" );
			pDAC_CCD          = ShadingVar3799;
			u12ccd_InitFunc   = fnCCDInitESIC3799;
			dev->fnDarkOffset = fnDarkOffsetWolfson3797;
			dev->fnDACDark    = fnDACDarkWolfson;
			dev->CCDRegs      = (RegDef*)W3799CCDParams;
		}

		dev->numCCDRegs         = _NUM_OF_CCDREGS_W8143;
		dev->numDACRegs         = _NUM_OF_DACREGS_W8143;
		dev->DACRegs            = WolfsonDAC8143;
		dev->RegDACOffset.Red   = 0x20;
		dev->RegDACOffset.Green = 0x21;
		dev->RegDACOffset.Blue  = 0x22;
		dev->RegDACGain.Red     = 0x28;
		dev->RegDACGain.Green   = 0x29;
		dev->RegDACGain.Blue    = 0x2a;

		if( dev->shade.intermediate & _ScanMode_AverageOut ) {
			dev->shade.bUniGain    = 1;
			dev->shade.bGainDouble = 1;
		} else {
			dev->shade.bUniGain    = 2;
			dev->shade.bGainDouble = 4;
		}
		dev->shade.bMinGain    = 1;
		dev->shade.bMaxGain    = 0x1f;
		dev->shade.wDarkLevels = 10;

		if( dev->shade.intermediate == _ScanMode_Color )
			WolfsonDAC8143[2].val = 0x52;
		else
			WolfsonDAC8143[2].val = 0x42;

		if(dev->shade.intermediate == _ScanMode_Mono )
			WolfsonDAC8143[0].val = 7;
		else
			WolfsonDAC8143[0].val = 3;
		break;

	default:

		DBG( _DBG_INFO, "* DAC: SAMSUNG 8531\n" );
		switch( dev->CCDID ) {

		case _CCD_3797:
			DBG( _DBG_INFO, "* CCD-3797\n" );
			pDAC_CCD          = ShadingVar3797;
			u12ccd_InitFunc   = fnCCDInitSamsung3797;
			dev->fnDarkOffset = fnDarkOffsetSamsung3797;
			dev->fnDACDark    = fnDACDarkSamsung;
			dev->CCDRegs      = (RegDef*)S3797CCDParams;
			break;

		case _CCD_3777:
			DBG( _DBG_INFO, "* CCD-3777\n" );
			pDAC_CCD          = ShadingVar3777;
			u12ccd_InitFunc   = fnCCDInitSamsung3777;
			dev->fnDarkOffset = fnDarkOffsetSamsung3777;
			dev->fnDACDark    = fnDACDarkSamsung;
			dev->CCDRegs      = (RegDef*)S3797CCDParams;
			dev->f0_8_16      = SANE_TRUE;
			break;

		default:
			DBG( _DBG_INFO, "* CCD-3799\n" );
			pDAC_CCD          = ShadingVar3799;
			u12ccd_InitFunc   = fnCCDInitSamsung3799;
			dev->fnDarkOffset = fnDarkOffsetSamsung3799;
			dev->fnDACDark    = fnDACDarkSamsung;
			dev->CCDRegs      = (RegDef*)S3799CCDParams;
		}

		dev->numCCDRegs         = _NUM_OF_CCDREGS_S8531;
		dev->numDACRegs         = _NUM_OF_DACREGS_S8531;
		dev->DACRegs            = SamsungDAC8531;
		dev->RegDACOffset.Red   = 1;
		dev->RegDACOffset.Green = 1;
		dev->RegDACOffset.Blue  = 1;
		dev->RegDACGain.Red     = 2;
		dev->RegDACGain.Green   = 2;
		dev->RegDACGain.Blue    = 2;
		dev->shade.bGainDouble  = 6;
		dev->shade.bMinGain     = 1;
		dev->shade.bMaxGain     = 0x1f;
		if( dev->DataInf.dwScanFlag & _SCANDEF_TPA )
			dev->shade.bUniGain = 2;
		else
			dev->shade.bUniGain = 7;

		dev->shade.wDarkLevels = 10;

		if( dev->shade.intermediate & _ScanMode_Mono ) {
			SamsungDAC8531[0].val = 0x57;
			SamsungDAC8531[3].val = 0x57;
			SamsungDAC8531[6].val = 0x57;
		} else {
			SamsungDAC8531[0].val = 0x51;
			SamsungDAC8531[3].val = 0x55;
			SamsungDAC8531[6].val = 0x59;
		}
	}

	if( shading ) {

		if( !(dev->DataInf.dwScanFlag & _SCANDEF_TPA))
			dev->shade.pCcdDac = &pDAC_CCD[_REFLECTION];
		else {
			if( dev->DataInf.dwScanFlag & _SCANDEF_Transparency )
				dev->shade.pCcdDac = &pDAC_CCD[_TRANSPARENCY];
			else
				dev->shade.pCcdDac = &pDAC_CCD[_NEGATIVE];
		}
	} else {
		dev->shade.pCcdDac = &pDAC_CCD[_REFLECTION];
	}

	/* as we now have the correct init function, call it */
	u12ccd_InitFunc( dev );

	DBG( _DBG_INFO, "* Programming DAC (%u regs)\n", dev->numDACRegs );

	for( w = 0; w < dev->numDACRegs; w++ ) {

		DBG( _DBG_INFO, "* [0x%02x] = 0x%02x\n", dev->DACRegs[w].reg,
		                                         dev->DACRegs[w].val );
#if 0
		u12io_DataRegisterToDAC( dev, dev->DACRegs[w].reg,
		                              dev->DACRegs[w].val );
#else
		u12io_DataToRegister( dev, REG_ADCADDR, dev->DACRegs[w].reg );
		u12io_DataToRegister( dev, REG_ADCDATA, dev->DACRegs[w].val );
		u12io_DataToRegister( dev, REG_ADCSERIALOUT, dev->DACRegs[w].val );
#endif
	}
	DBG( _DBG_INFO, "CCD & DAC init done.\n" );
}

/* END U12-CCD.C ............................................................*/
