/* @file plustek-pp_p12ccd.c
 * @brief here we have the whole code to intialize the CCD and DAC stuff
 *
 * based on sources acquired from Plustek Inc.
 * Copyright (C) 2000 Plustek Inc.
 * Copyright (C) 2001-2004 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * History:
 * - 0.38 - initial version
 * - 0.39 - using now fnDarkOffsetWolfson3797 instead of fnDarkOffsetWolfson3799
 *          This should provide a better picture quality on the GENIUS Colorpage
 *          Vivid V2
 * - 0.40 - no changes
 * - 0.41 - no changes
 * - 0.42 - changed include names
 * - 0.43 - no changes
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
#include "plustek-pp_scan.h"

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

#define _NUM_OF_CCDREGS_Max	    _NUM_OF_CCDREGS_S1224
#define _NUM_OF_DACREGS_Max	    _NUM_OF_DACREGS_W8143


/*************************** some local vars *********************************/

static pFnVoid p12ccdInitFunc = NULL;

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

static DACTblDef ShadingVar3797[3] = {
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

static DACTblDef ShadingVar3799[3] = {
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
static DACTblDef ShadingVar548[3] = {
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

static DACTblDef ShadingVar3777[3] = {
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

/*.............................................................................
 *
 */
static void fnCCDInitWolfson3797( pScanData ps )
{
    if( ps->Shade.bIntermediate & _ScanMode_Mono )
    	ps->Shade.pCcdDac->DarkDAC.Colors.Green = 0xcc;
    else
    	if (ps->Shade.bIntermediate & _ScanMode_AverageOut)
	        ps->Shade.pCcdDac->DarkDAC.Colors.Green = 0x68;
    	else
  	        ps->Shade.pCcdDac->DarkDAC.Colors.Green = 0xa0;

    if((ps->Shade.bIntermediate & _ScanMode_AverageOut) ||
        (ps->DataInf.dwScanFlag & SCANDEF_Negative))
    	WolfsonDAC8143[3].bParam = 0x12;
    else
        WolfsonDAC8143[3].bParam = 0x10;
}

/*.............................................................................
 *
 */
static void fnCCDInitSamsung3797( pScanData ps )
{
    if(!(ps->DataInf.dwScanFlag & SCANDEF_TPA)) {

        if (!(ps->Shade.bIntermediate & _ScanMode_AverageOut)) {

    	    if( ps->Device.bPCBID == _OPTICWORKS2000 ) {
        		ps->Shade.pCcdDac->GainResize.Colors.Red = 102;
	        	ps->Shade.pCcdDac->GainResize.Colors.Green = 102;
        		ps->Shade.pCcdDac->GainResize.Colors.Blue = 97;
        		ps->Shade.pCcdDac->DarkDAC.Colors.Red = 0x40;
        		ps->Shade.pCcdDac->DarkDAC.Colors.Green = 0x40;
        		ps->Shade.pCcdDac->DarkDAC.Colors.Blue = 0x40;
		        ps->Shade.pCcdDac->DarkCmpHi.Colors.Red = 0x48;
        		ps->Shade.pCcdDac->DarkCmpHi.Colors.Green = 0x40;
        		ps->Shade.pCcdDac->DarkCmpHi.Colors.Blue = 0x40;
        		ps->Shade.pCcdDac->DarkCmpLo.Colors.Red = 0x38;
        		ps->Shade.pCcdDac->DarkCmpLo.Colors.Green = 0x30;
        		ps->Shade.pCcdDac->DarkCmpLo.Colors.Blue = 0x30;
        		ps->Shade.pCcdDac->DarkOffSub.Colors.Red = 0x48;
        		ps->Shade.pCcdDac->DarkOffSub.Colors.Green = 0x38;
        		ps->Shade.pCcdDac->DarkOffSub.Colors.Blue = 0x40;
	        } else {
        		ps->Shade.pCcdDac->GainResize.Colors.Red = 99;
        		ps->Shade.pCcdDac->GainResize.Colors.Green = 101;
		        ps->Shade.pCcdDac->GainResize.Colors.Blue = 94;
        		ps->Shade.pCcdDac->DarkDAC.Colors.Red = 0x40;
        		ps->Shade.pCcdDac->DarkDAC.Colors.Green = 0x40;
		        ps->Shade.pCcdDac->DarkDAC.Colors.Blue = 0x40;
        		ps->Shade.pCcdDac->DarkCmpHi.Colors.Red = 0x30;
        		ps->Shade.pCcdDac->DarkCmpHi.Colors.Green = 0x30;
        		ps->Shade.pCcdDac->DarkCmpHi.Colors.Blue = 0x30;
        		ps->Shade.pCcdDac->DarkCmpLo.Colors.Red = 0x20;
        		ps->Shade.pCcdDac->DarkCmpLo.Colors.Green = 0x20;
		        ps->Shade.pCcdDac->DarkCmpLo.Colors.Blue = 0x20;
        		ps->Shade.pCcdDac->DarkOffSub.Colors.Red = 0x04;
		        ps->Shade.pCcdDac->DarkOffSub.Colors.Green = 0x00;
        		ps->Shade.pCcdDac->DarkOffSub.Colors.Blue = 0x00;
	        }
    	} else {
            if( ps->Device.bPCBID == _OPTICWORKS2000 ) {
    	    	ps->Shade.pCcdDac->GainResize.Colors.Red = 100;
    	    	ps->Shade.pCcdDac->GainResize.Colors.Green = 100;
	    	    ps->Shade.pCcdDac->GainResize.Colors.Blue = 96;
    	    	ps->Shade.pCcdDac->DarkDAC.Colors.Red = 0x30;
        		ps->Shade.pCcdDac->DarkDAC.Colors.Green = 0x30;
	        	ps->Shade.pCcdDac->DarkDAC.Colors.Blue = 0x30;
    	    	ps->Shade.pCcdDac->DarkCmpHi.Colors.Red = 0x48;
    	    	ps->Shade.pCcdDac->DarkCmpHi.Colors.Green = 0x48;
        		ps->Shade.pCcdDac->DarkCmpHi.Colors.Blue = 0x48;
	        	ps->Shade.pCcdDac->DarkCmpLo.Colors.Red = 0x38;
        		ps->Shade.pCcdDac->DarkCmpLo.Colors.Green = 0x38;
	        	ps->Shade.pCcdDac->DarkCmpLo.Colors.Blue = 0x38;
    		    ps->Shade.pCcdDac->DarkOffSub.Colors.Red = 0x48;
        		ps->Shade.pCcdDac->DarkOffSub.Colors.Green = 0x48;
    	    	ps->Shade.pCcdDac->DarkOffSub.Colors.Blue = 0x48;
	        } else {
    	    	ps->Shade.pCcdDac->GainResize.Colors.Red = 100;   /*  98 */
    	    	ps->Shade.pCcdDac->GainResize.Colors.Green = 103; /* 106 */
	    	    ps->Shade.pCcdDac->GainResize.Colors.Blue = 96;	  /*  96 */
    	    	ps->Shade.pCcdDac->DarkDAC.Colors.Red = 0x20;
	    	    ps->Shade.pCcdDac->DarkDAC.Colors.Green = 0x10;
    		    ps->Shade.pCcdDac->DarkDAC.Colors.Blue = 0x10;
        		ps->Shade.pCcdDac->DarkCmpHi.Colors.Red = 0x110;
	        	ps->Shade.pCcdDac->DarkCmpHi.Colors.Green = 0x1f0;
    		    ps->Shade.pCcdDac->DarkCmpHi.Colors.Blue = 0x190;
        		ps->Shade.pCcdDac->DarkCmpLo.Colors.Red = 0x100;
	        	ps->Shade.pCcdDac->DarkCmpLo.Colors.Green = 0x1e0;
    		    ps->Shade.pCcdDac->DarkCmpLo.Colors.Blue = 0x180;
        		ps->Shade.pCcdDac->DarkOffSub.Colors.Red = 0x20;
	        	ps->Shade.pCcdDac->DarkOffSub.Colors.Green = 0x10;
    		    ps->Shade.pCcdDac->DarkOffSub.Colors.Blue = 0x20;
	        }
       	}
    }
}

/*.............................................................................
 *
 */
static void fnCCDInitWolfson3799( pScanData ps )
{
    if(!(ps->DataInf.dwScanFlag & SCANDEF_Negative)) {

        if (!(ps->Shade.bIntermediate & _ScanMode_AverageOut)) {

    	    ps->Shade.pCcdDac->GainResize.Colors.Red = 103;
	        ps->Shade.pCcdDac->GainResize.Colors.Green = 102;
    	    ps->Shade.pCcdDac->GainResize.Colors.Blue = 99;
	        ps->Shade.pCcdDac->DarkDAC.Colors.Red = 0xc8;
	        ps->Shade.pCcdDac->DarkDAC.Colors.Green = 0xc8;
    	    ps->Shade.pCcdDac->DarkDAC.Colors.Blue = 0xc8;
	        ps->Shade.pCcdDac->DarkCmpHi.Colors.Red = 0x48;
	        ps->Shade.pCcdDac->DarkCmpHi.Colors.Green = 0x30;
    	    ps->Shade.pCcdDac->DarkCmpHi.Colors.Blue = 0x30;
	        ps->Shade.pCcdDac->DarkCmpLo.Colors.Red = 0x40;
	        ps->Shade.pCcdDac->DarkCmpLo.Colors.Green = 0x28;
    	    ps->Shade.pCcdDac->DarkCmpLo.Colors.Blue = 0x28;
	        ps->Shade.pCcdDac->DarkOffSub.Colors.Red = 0x48;
    	    ps->Shade.pCcdDac->DarkOffSub.Colors.Green = 0x18;
	        ps->Shade.pCcdDac->DarkOffSub.Colors.Blue = 0x2c;
    	} else {
	        ps->Shade.pCcdDac->GainResize.Colors.Red = 100;
    	    ps->Shade.pCcdDac->GainResize.Colors.Green = 98;
	        ps->Shade.pCcdDac->GainResize.Colors.Blue = 95;
	        ps->Shade.pCcdDac->DarkDAC.Colors.Red = 0xd0;
    	    ps->Shade.pCcdDac->DarkDAC.Colors.Green = 0xd0;
	        ps->Shade.pCcdDac->DarkDAC.Colors.Blue = 0xd0;
	        ps->Shade.pCcdDac->DarkCmpHi.Colors.Red = 0x30;
    	    ps->Shade.pCcdDac->DarkCmpHi.Colors.Green = 0x30;
	        ps->Shade.pCcdDac->DarkCmpHi.Colors.Blue = 0x30;
    	    ps->Shade.pCcdDac->DarkCmpLo.Colors.Red = 0x28;
	        ps->Shade.pCcdDac->DarkCmpLo.Colors.Green = 0x28;
    	    ps->Shade.pCcdDac->DarkCmpLo.Colors.Blue = 0x28;
	        ps->Shade.pCcdDac->DarkOffSub.Colors.Red = 0x0;
    	    ps->Shade.pCcdDac->DarkOffSub.Colors.Green = 0x0;
	        ps->Shade.pCcdDac->DarkOffSub.Colors.Blue = 0x0;
    	}
    } else {
    	ps->Shade.pCcdDac->DarkDAC.Colors.Red = 0x80;
	    ps->Shade.pCcdDac->DarkDAC.Colors.Green = 0x80;
    	ps->Shade.pCcdDac->DarkDAC.Colors.Blue = 0x80;
	    ps->Shade.pCcdDac->DarkCmpHi.Colors.Red = 0x28;
    	ps->Shade.pCcdDac->DarkCmpHi.Colors.Green = 0x28;
	    ps->Shade.pCcdDac->DarkCmpHi.Colors.Blue = 0x28;
    	ps->Shade.pCcdDac->DarkCmpLo.Colors.Red = 0x20;
	    ps->Shade.pCcdDac->DarkCmpLo.Colors.Green = 0x20;
    	ps->Shade.pCcdDac->DarkCmpLo.Colors.Blue = 0x20;
	    ps->Shade.pCcdDac->DarkOffSub.Colors.Red = -0x38;
    	ps->Shade.pCcdDac->DarkOffSub.Colors.Green = -0x108;
	    ps->Shade.pCcdDac->DarkOffSub.Colors.Blue = -0x1c8;
    }
}

/*.............................................................................
 *  Genius ColorPage VIVID III
 */
static void fnCCDInitWolfson548( pScanData ps )
{
    if (!(ps->Shade.bIntermediate & _ScanMode_AverageOut)) {

 	    ps->Shade.pCcdDac->GainResize.Colors.Red   = 103;
        ps->Shade.pCcdDac->GainResize.Colors.Green = 102;
   	    ps->Shade.pCcdDac->GainResize.Colors.Blue  = 99;
        ps->Shade.pCcdDac->DarkDAC.Colors.Red      = 0xc8;
        ps->Shade.pCcdDac->DarkDAC.Colors.Green    = 0xc8;
   	    ps->Shade.pCcdDac->DarkDAC.Colors.Blue     = 0xc8;
        ps->Shade.pCcdDac->DarkCmpHi.Colors.Red    = 0x48;
        ps->Shade.pCcdDac->DarkCmpHi.Colors.Green  = 0x30;
   	    ps->Shade.pCcdDac->DarkCmpHi.Colors.Blue   = 0x30;
        ps->Shade.pCcdDac->DarkCmpLo.Colors.Red    = 0x40;
        ps->Shade.pCcdDac->DarkCmpLo.Colors.Green  = 0x28;
   	    ps->Shade.pCcdDac->DarkCmpLo.Colors.Blue   = 0x28;
        ps->Shade.pCcdDac->DarkOffSub.Colors.Red   = 0x48;
   	    ps->Shade.pCcdDac->DarkOffSub.Colors.Green = 0x18;
        ps->Shade.pCcdDac->DarkOffSub.Colors.Blue  = 0x2c;

   	} else {
        ps->Shade.pCcdDac->GainResize.Colors.Red   = 100;
   	    ps->Shade.pCcdDac->GainResize.Colors.Green = 98;
        ps->Shade.pCcdDac->GainResize.Colors.Blue  = 95;
        ps->Shade.pCcdDac->DarkDAC.Colors.Red      = 0xd0;
   	    ps->Shade.pCcdDac->DarkDAC.Colors.Green    = 0xd0;
        ps->Shade.pCcdDac->DarkDAC.Colors.Blue     = 0xd0;
        ps->Shade.pCcdDac->DarkCmpHi.Colors.Red    = 0x30;
   	    ps->Shade.pCcdDac->DarkCmpHi.Colors.Green  = 0x30;
        ps->Shade.pCcdDac->DarkCmpHi.Colors.Blue   = 0x30;
   	    ps->Shade.pCcdDac->DarkCmpLo.Colors.Red    = 0x28;
        ps->Shade.pCcdDac->DarkCmpLo.Colors.Green  = 0x28;
   	    ps->Shade.pCcdDac->DarkCmpLo.Colors.Blue   = 0x28;
        ps->Shade.pCcdDac->DarkOffSub.Colors.Red   = 0x0;
   	    ps->Shade.pCcdDac->DarkOffSub.Colors.Green = 0x0;
        ps->Shade.pCcdDac->DarkOffSub.Colors.Blue  = 0x0;
   	}
}

/*.............................................................................
 *
 */
static void fnCCDInitSamsung3777( pScanData ps )
{
    if(!(ps->DataInf.dwScanFlag & SCANDEF_Negative)) {

        if (!(ps->Shade.bIntermediate & _ScanMode_AverageOut)) {

    	    ps->Shade.pCcdDac->GainResize.Colors.Red = 109;
	        ps->Shade.pCcdDac->GainResize.Colors.Green = 108;
	        ps->Shade.pCcdDac->GainResize.Colors.Blue = 105;
    	    ps->Shade.pCcdDac->DarkDAC.Colors.Red = 0x4a;
	        ps->Shade.pCcdDac->DarkDAC.Colors.Green = 0x4a;
            ps->Shade.pCcdDac->DarkDAC.Colors.Blue = 0x4a;
	        ps->Shade.pCcdDac->DarkCmpHi.Colors.Red = 0x3c;
    	    ps->Shade.pCcdDac->DarkCmpHi.Colors.Green = 0x38;
	        ps->Shade.pCcdDac->DarkCmpHi.Colors.Blue = 0x38;
	        ps->Shade.pCcdDac->DarkCmpLo.Colors.Red = 0x28;
    	    ps->Shade.pCcdDac->DarkCmpLo.Colors.Green = 0x2c;
	        ps->Shade.pCcdDac->DarkCmpLo.Colors.Blue = 0x28;
	        ps->Shade.pCcdDac->DarkOffSub.Colors.Red = 0x30;
    	    ps->Shade.pCcdDac->DarkOffSub.Colors.Green = 0x30;
	        ps->Shade.pCcdDac->DarkOffSub.Colors.Blue = 0x3C;
    	} else {
    	    ps->Shade.pCcdDac->GainResize.Colors.Red = 108;
	        ps->Shade.pCcdDac->GainResize.Colors.Green = 107;
    	    ps->Shade.pCcdDac->GainResize.Colors.Blue = 104;
	        ps->Shade.pCcdDac->DarkDAC.Colors.Red = 0x50;
	        ps->Shade.pCcdDac->DarkDAC.Colors.Green = 0x50;
    	    ps->Shade.pCcdDac->DarkDAC.Colors.Blue = 0x50;
    	    ps->Shade.pCcdDac->DarkCmpHi.Colors.Red = 0x40;
	        ps->Shade.pCcdDac->DarkCmpHi.Colors.Green = 0x40;
	        ps->Shade.pCcdDac->DarkCmpHi.Colors.Blue = 0x40;
    	    ps->Shade.pCcdDac->DarkCmpLo.Colors.Red = 0x30;
	        ps->Shade.pCcdDac->DarkCmpLo.Colors.Green = 0x30;
	        ps->Shade.pCcdDac->DarkCmpLo.Colors.Blue = 0x30;
	        ps->Shade.pCcdDac->DarkOffSub.Colors.Red = 0x20;
    	    ps->Shade.pCcdDac->DarkOffSub.Colors.Green = 0x20;
	        ps->Shade.pCcdDac->DarkOffSub.Colors.Blue = 0x20;
	    }
    } else {
    	ps->Shade.pCcdDac->DarkDAC.Colors.Red = 0x80;
	    ps->Shade.pCcdDac->DarkDAC.Colors.Green = 0x80;
    	ps->Shade.pCcdDac->DarkDAC.Colors.Blue = 0x80;
	    ps->Shade.pCcdDac->DarkCmpHi.Colors.Red = 0x28;
    	ps->Shade.pCcdDac->DarkCmpHi.Colors.Green = 0x28;
    	ps->Shade.pCcdDac->DarkCmpHi.Colors.Blue = 0x28;
	    ps->Shade.pCcdDac->DarkCmpLo.Colors.Red = 0x20;
    	ps->Shade.pCcdDac->DarkCmpLo.Colors.Green = 0x20;
	    ps->Shade.pCcdDac->DarkCmpLo.Colors.Blue = 0x20;
    	ps->Shade.pCcdDac->DarkOffSub.Colors.Red = -0x38;
	    ps->Shade.pCcdDac->DarkOffSub.Colors.Green = -0x108;
    	ps->Shade.pCcdDac->DarkOffSub.Colors.Blue = -0x1c8;
    }
}

/*.............................................................................
 *
 */
static void fnCCDInitSamsung3799( pScanData ps )
{
    if(!(ps->DataInf.dwScanFlag & SCANDEF_TPA)) {

        if (!(ps->Shade.bIntermediate & _ScanMode_AverageOut)) {

    	    if( ps->Device.bPCBID == _SCANNER2Button ) {
        		ps->Shade.pCcdDac->GainResize.Colors.Red = 109;
        		ps->Shade.pCcdDac->GainResize.Colors.Green = 109;
		        ps->Shade.pCcdDac->GainResize.Colors.Blue = 105;
        		ps->Shade.pCcdDac->DarkDAC.Colors.Red = 0x68;
		        ps->Shade.pCcdDac->DarkDAC.Colors.Green = 0x68;
        		ps->Shade.pCcdDac->DarkDAC.Colors.Blue = 0x68;
		        ps->Shade.pCcdDac->DarkCmpHi.Colors.Red = 0x30;
        		ps->Shade.pCcdDac->DarkCmpHi.Colors.Green = 0x30;
		        ps->Shade.pCcdDac->DarkCmpHi.Colors.Blue = 0x30;
        		ps->Shade.pCcdDac->DarkCmpLo.Colors.Red = 0x28;
		        ps->Shade.pCcdDac->DarkCmpLo.Colors.Green = 0x28;
        		ps->Shade.pCcdDac->DarkCmpLo.Colors.Blue = 0x28;
		        ps->Shade.pCcdDac->DarkOffSub.Colors.Red = 0x24;    /* 0 */
        		ps->Shade.pCcdDac->DarkOffSub.Colors.Green = 0x20;  /* 0 */
		        ps->Shade.pCcdDac->DarkOffSub.Colors.Blue = 0x1c;   /* 0 */
    	    } else {
        		ps->Shade.pCcdDac->GainResize.Colors.Red = 98;
		        ps->Shade.pCcdDac->GainResize.Colors.Green = 97;
        		ps->Shade.pCcdDac->GainResize.Colors.Blue = 92;
		        ps->Shade.pCcdDac->DarkDAC.Colors.Red = 0x90;
        		ps->Shade.pCcdDac->DarkDAC.Colors.Green = 0x90;
		        ps->Shade.pCcdDac->DarkDAC.Colors.Blue = 0x90;
        		ps->Shade.pCcdDac->DarkCmpHi.Colors.Red = 0xc0;	    /* 0x90 */
		        ps->Shade.pCcdDac->DarkCmpHi.Colors.Green = 0xc0;   /* 0xe0 */
        		ps->Shade.pCcdDac->DarkCmpHi.Colors.Blue = 0xc0;    /* 0x80 */
		        ps->Shade.pCcdDac->DarkCmpLo.Colors.Red = 0xb0;	    /* 0x70 */
        		ps->Shade.pCcdDac->DarkCmpLo.Colors.Green = 0xb0;   /* 0xc0 */
		        ps->Shade.pCcdDac->DarkCmpLo.Colors.Blue = 0xb0;    /* 0x60 */
        		ps->Shade.pCcdDac->DarkOffSub.Colors.Red = 0x24;    /* 0x90 */
		        ps->Shade.pCcdDac->DarkOffSub.Colors.Green = 0x00;	/* 0x34 */
        		ps->Shade.pCcdDac->DarkOffSub.Colors.Blue = 0x0c;	/* 0x3c */
	        }
    	} else {
    	    if( ps->Device.bPCBID == _SCANNER2Button ) {
        		ps->Shade.pCcdDac->GainResize.Colors.Red = 107;
		        ps->Shade.pCcdDac->GainResize.Colors.Green = 106;
        		ps->Shade.pCcdDac->GainResize.Colors.Blue = 103;
		        ps->Shade.pCcdDac->DarkDAC.Colors.Red = 0x48;
        		ps->Shade.pCcdDac->DarkDAC.Colors.Green = 0x48;
		        ps->Shade.pCcdDac->DarkDAC.Colors.Blue = 0x48;
        		ps->Shade.pCcdDac->DarkCmpHi.Colors.Red = 0x30;
		        ps->Shade.pCcdDac->DarkCmpHi.Colors.Green = 0x30;
        		ps->Shade.pCcdDac->DarkCmpHi.Colors.Blue = 0x30;
		        ps->Shade.pCcdDac->DarkCmpLo.Colors.Red = 0x28;
        		ps->Shade.pCcdDac->DarkCmpLo.Colors.Green = 0x28;
		        ps->Shade.pCcdDac->DarkCmpLo.Colors.Blue = 0x28;
        		ps->Shade.pCcdDac->DarkOffSub.Colors.Red = 0x28;    /* 0 */
		        ps->Shade.pCcdDac->DarkOffSub.Colors.Green = 0x18;  /* 0 */
        		ps->Shade.pCcdDac->DarkOffSub.Colors.Blue = 0x20;   /* 0 */
	        } else {
        		ps->Shade.pCcdDac->GainResize.Colors.Red = 104;
		        ps->Shade.pCcdDac->GainResize.Colors.Green = 107;
        		ps->Shade.pCcdDac->GainResize.Colors.Blue = 99;
		        ps->Shade.pCcdDac->DarkDAC.Colors.Red = 0x30;	    /* 0x80 */
        		ps->Shade.pCcdDac->DarkDAC.Colors.Green = 0x30;
		        ps->Shade.pCcdDac->DarkDAC.Colors.Blue = 0x30;	    /* 0x0a0 */
        		ps->Shade.pCcdDac->DarkCmpHi.Colors.Red = 0x150;    /* 0x170 */
		        ps->Shade.pCcdDac->DarkCmpHi.Colors.Green = 0x130;  /* 0x90  */
        		ps->Shade.pCcdDac->DarkCmpHi.Colors.Blue = 0x110;   /* 0x130 */
		        ps->Shade.pCcdDac->DarkCmpLo.Colors.Red = 0x140;    /* 0x150 */
        		ps->Shade.pCcdDac->DarkCmpLo.Colors.Green = 0x120;  /* 0x70  */
		        ps->Shade.pCcdDac->DarkCmpLo.Colors.Blue = 0x100;   /* 0x120 */
        		ps->Shade.pCcdDac->DarkOffSub.Colors.Red = 0xF0;    /* 0x90  */
		        ps->Shade.pCcdDac->DarkOffSub.Colors.Green = 0xD4;  /* 0x50  */
        		ps->Shade.pCcdDac->DarkOffSub.Colors.Blue = 0xCC;   /* 0x60  */
	        }
        }
    }
}

/*.............................................................................
 *
 */
static void fnCCDInitESIC3799( pScanData ps )
{
    if(!(ps->DataInf.dwScanFlag & SCANDEF_Negative)) {

        if (!(ps->Shade.bIntermediate & _ScanMode_AverageOut)) {

    	    ps->Shade.pCcdDac->GainResize.Colors.Red = 100;
	        ps->Shade.pCcdDac->GainResize.Colors.Green = 99;
	        ps->Shade.pCcdDac->GainResize.Colors.Blue = 94;
    	    ps->Shade.pCcdDac->DarkDAC.Colors.Red = 0xc8;
	        ps->Shade.pCcdDac->DarkDAC.Colors.Green = 0xc8;
	        ps->Shade.pCcdDac->DarkDAC.Colors.Blue = 0xc8;
    	    ps->Shade.pCcdDac->DarkCmpHi.Colors.Red = 0x58;
	        ps->Shade.pCcdDac->DarkCmpHi.Colors.Green = 0x38;
	        ps->Shade.pCcdDac->DarkCmpHi.Colors.Blue = 0x48;
    	    ps->Shade.pCcdDac->DarkCmpLo.Colors.Red = 0x48;
	        ps->Shade.pCcdDac->DarkCmpLo.Colors.Green = 0x28;
	        ps->Shade.pCcdDac->DarkCmpLo.Colors.Blue = 0x38;
    	    ps->Shade.pCcdDac->DarkOffSub.Colors.Red = 0x58;
	        ps->Shade.pCcdDac->DarkOffSub.Colors.Green = 0x38;
	        ps->Shade.pCcdDac->DarkOffSub.Colors.Blue = 0x48;
    	} else {
    	    ps->Shade.pCcdDac->GainResize.Colors.Red = 100;
	        ps->Shade.pCcdDac->GainResize.Colors.Green = 98;
	        ps->Shade.pCcdDac->GainResize.Colors.Blue = 93;
    	    ps->Shade.pCcdDac->DarkDAC.Colors.Red = 0xd0;
	        ps->Shade.pCcdDac->DarkDAC.Colors.Green = 0xd0;
	        ps->Shade.pCcdDac->DarkDAC.Colors.Blue = 0xd0;
    	    ps->Shade.pCcdDac->DarkCmpHi.Colors.Red = 0x108;
	        ps->Shade.pCcdDac->DarkCmpHi.Colors.Green = 0xf8;
	        ps->Shade.pCcdDac->DarkCmpHi.Colors.Blue = 0xc8;
    	    ps->Shade.pCcdDac->DarkCmpLo.Colors.Red = 0x100;
	        ps->Shade.pCcdDac->DarkCmpLo.Colors.Green = 0xf0;
	        ps->Shade.pCcdDac->DarkCmpLo.Colors.Blue = 0xc0;
    	    ps->Shade.pCcdDac->DarkOffSub.Colors.Red = 0x108;
	        ps->Shade.pCcdDac->DarkOffSub.Colors.Green = 0xf8;
	        ps->Shade.pCcdDac->DarkOffSub.Colors.Blue = 0xc8;
    	}
    } else {
    	ps->Shade.pCcdDac->DarkDAC.Colors.Red = 0x80;
	    ps->Shade.pCcdDac->DarkDAC.Colors.Green = 0x80;
    	ps->Shade.pCcdDac->DarkDAC.Colors.Blue = 0x80;
	    ps->Shade.pCcdDac->DarkCmpHi.Colors.Red = 0x28;
    	ps->Shade.pCcdDac->DarkCmpHi.Colors.Green = 0x28;
	    ps->Shade.pCcdDac->DarkCmpHi.Colors.Blue = 0x28;
    	ps->Shade.pCcdDac->DarkCmpLo.Colors.Red = 0x20;
	    ps->Shade.pCcdDac->DarkCmpLo.Colors.Green = 0x20;
    	ps->Shade.pCcdDac->DarkCmpLo.Colors.Blue = 0x20;
	    ps->Shade.pCcdDac->DarkOffSub.Colors.Red = -0x38;
    	ps->Shade.pCcdDac->DarkOffSub.Colors.Green = -0x38;
	    ps->Shade.pCcdDac->DarkOffSub.Colors.Blue = -0x38;
    }
}

/*.............................................................................
 *
 */
static void fnDarkOffsetWolfson3797( pScanData ps, pDACTblDef pDacTbl, ULong dwCh )
{
    if(( ps->Shade.DarkOffset.wColors[dwCh] -=
                pDacTbl->DarkOffSub.wColors[dwCh]) > 0xfff ) {
	    ps->Shade.DarkOffset.wColors[dwCh] = 0;
    }
}

/*.............................................................................
 * this function was defined in the original sources, but never used...
 */
#if 0
static void fnDarkOffsetWolfson3799( pScanData ps, pDACTblDef pDacTbl, ULong dwCh )
{
    if( ps->Shade.DarkOffset.wColors[dwCh] > pDacTbl->DarkOffSub.wColors[dwCh])
        ps->Shade.DarkOffset.wColors[dwCh] -= pDacTbl->DarkOffSub.wColors[dwCh];
    else
        ps->Shade.DarkOffset.wColors[dwCh] = 0;
}
#endif

/*.............................................................................
 *
 */
static void fnDarkOffsetSamsung3777( pScanData ps, pDACTblDef pDacTbl, ULong dwCh )
{
    ps->Shade.DarkOffset.wColors[dwCh] += pDacTbl->DarkOffSub.wColors [dwCh];
}

/*.............................................................................
 *
 */
static void fnDarkOffsetSamsung3797( pScanData ps, pDACTblDef pDacTbl, ULong dwCh )
{
    if( ps->Shade.DarkOffset.wColors[dwCh] > pDacTbl->DarkOffSub.wColors[dwCh] )
    	ps->Shade.DarkOffset.wColors[dwCh] -= pDacTbl->DarkOffSub.wColors[dwCh];
    else
    	ps->Shade.DarkOffset.wColors[dwCh] = 0;
}

/*.............................................................................
 *
 */
static void fnDarkOffsetSamsung3799( pScanData ps, pDACTblDef pDacTbl, ULong dwCh )
{
    if( ps->Shade.DarkOffset.wColors[dwCh] > pDacTbl->DarkOffSub.wColors[dwCh])
    	ps->Shade.DarkOffset.wColors[dwCh] -= pDacTbl->DarkOffSub.wColors[dwCh];
    else
        ps->Shade.DarkOffset.wColors[dwCh] = 0;
}

/*.............................................................................
 *
 */
static void fnDACDarkWolfson( pScanData ps, pDACTblDef pDacTbl,
                              ULong dwCh, UShort wDarkest )
{
    UShort w;

    w = ps->Shade.DarkDAC.bColors[dwCh];

    if (wDarkest > pDacTbl->DarkCmpHi.wColors[dwCh] ) {

    	wDarkest -= pDacTbl->DarkCmpHi.wColors[dwCh];
    	if (wDarkest > ps->Shade.wDarkLevels)
	        w += (UShort)wDarkest / ps->Shade.wDarkLevels;
    	else
	        w++;

        if (w > 0xff)
	        w = 0xff;

        if(w != (UShort)ps->Shade.DarkDAC.bColors[dwCh] ) {
    	    ps->Shade.DarkDAC.bColors[dwCh] = (Byte)w;
	        ps->Shade.fStop                 = _FALSE;
    	}
    } else
    	if((wDarkest < pDacTbl->DarkCmpLo.wColors[dwCh]) &&
                                	        ps->Shade.DarkDAC.bColors[dwCh]) {
    	    if (wDarkest)
	        	w = (UShort)ps->Shade.DarkDAC.bColors[dwCh] - 2U;
    	    else
	        	w = (UShort)ps->Shade.DarkDAC.bColors[dwCh] - ps->Shade.wDarkLevels;

    	    if ((short) w < 0)
	        	w = 0;
    	    if (w != (UShort)ps->Shade.DarkDAC.bColors[dwCh] ) {
        		ps->Shade.DarkDAC.bColors [dwCh] = (Byte)w;
		        ps->Shade.fStop                  = _FALSE;
	        }
	}
}

/*.............................................................................
 *
 */
static void fnDACDarkSamsung( pScanData ps, pDACTblDef pDacTbl,
                              ULong dwCh, UShort wDarkest )
{
    UShort w;

    if( wDarkest > pDacTbl->DarkCmpHi.wColors[dwCh] ) {

    	wDarkest -= pDacTbl->DarkCmpHi.wColors [dwCh];
    	if (wDarkest > ps->Shade.wDarkLevels)
	        w = (UShort)ps->Shade.DarkDAC.bColors[dwCh] -
	                                        wDarkest / ps->Shade.wDarkLevels;
    	else
	        w = (UShort)ps->Shade.DarkDAC.bColors[dwCh] - 1U;

    	if((short) w < 0)
	        w = 0;

    	if(w != (UShort)ps->Shade.DarkDAC.bColors[dwCh]) {
    	    ps->Shade.DarkDAC.bColors [dwCh] = (Byte)w;
	        ps->Shade.fStop                  = _FALSE;
    	}
    } else
    	if((wDarkest < pDacTbl->DarkCmpLo.wColors[dwCh]) &&
                                    	    ps->Shade.DarkDAC.bColors[dwCh]) {
    	    if (wDarkest)
	        	w = (UShort)ps->Shade.DarkDAC.bColors[dwCh] + 2U;
    	    else
	    	w = ps->Shade.wDarkLevels + (UShort)ps->Shade.DarkDAC.bColors [dwCh];

    	    if (w > 0xff)
	        	w = 0xff;

    	    if(w != (UShort)ps->Shade.DarkDAC.bColors[dwCh]) {
        		ps->Shade.DarkDAC.bColors[dwCh] = (Byte)w;
		        ps->Shade.fStop                 = _FALSE;
    	    }
	}
}

/************************ exported functions *********************************/

/*.............................................................................
 * according to detected CCD and DAC, we set their correct init values
 * and functions
 */
_LOC void P12InitCCDandDAC( pScanData ps, Bool shading )
{
    UShort     w;
    pDACTblDef pDAC_CCD;

    /* some presets */
    ps->Device.f0_8_16 = _FALSE;

    switch( ps->Device.bDACType ) {

	case _DA_WOLFSON8143:

       	DBG( DBG_LOW, "WOLFSON 8143\n" );
	    switch( ps->Device.bCCDID ) {

		case _CCD_3797:
           	DBG( DBG_LOW, "CCD-3797\n" );
    	    pDAC_CCD                 = ShadingVar3797;
		    p12ccdInitFunc           = fnCCDInitWolfson3797;
		    ps->Device.fnDarkOffset  = fnDarkOffsetWolfson3797;
		    ps->Device.fnDACDark     = fnDACDarkWolfson;
		    ps->Device.pCCDRegisters = (pRegDef)W3797CCDParams;
		    break;

		case _CCD_548:
           	DBG( DBG_LOW, "CCD-548\n" );
		    pDAC_CCD                 = ShadingVar548;
		    p12ccdInitFunc           = fnCCDInitWolfson548;
		    ps->Device.fnDarkOffset  = fnDarkOffsetWolfson3797;
		    ps->Device.fnDACDark     = fnDACDarkWolfson;
		    ps->Device.pCCDRegisters = (pRegDef)W548CCDParams;
		    break;


		default:
           	DBG( DBG_LOW, "CCD-3799\n" );
		    pDAC_CCD                 = ShadingVar3799;
		    p12ccdInitFunc           = fnCCDInitWolfson3799;
/* CHECK: org was to  fnDarkOffsetWolfson3797 */
		    ps->Device.fnDarkOffset  = fnDarkOffsetWolfson3797;
		    ps->Device.fnDACDark     = fnDACDarkWolfson;
		    ps->Device.pCCDRegisters = (pRegDef)W3799CCDParams;
	    }

        ps->Device.wNumCCDRegs   = _NUM_OF_CCDREGS_W8143;
        ps->Device.wNumDACRegs   = _NUM_OF_DACREGS_W8143;
        ps->Device.pDACRegisters = WolfsonDAC8143;
        ps->Device.RegDACOffset.Red   = 0x20;
        ps->Device.RegDACOffset.Green = 0x21;
        ps->Device.RegDACOffset.Blue  = 0x22;
        ps->Device.RegDACGain.Red     = 0x28;
        ps->Device.RegDACGain.Green   = 0x29;
        ps->Device.RegDACGain.Blue    = 0x2a;

        if( ps->Shade.bIntermediate & _ScanMode_AverageOut ) {
    		ps->Shade.bUniGain    = 1;
	    	ps->Shade.bGainDouble = 1;
	    } else {
    		ps->Shade.bUniGain    = 2;
	    	ps->Shade.bGainDouble = 4;
	    }
	    ps->Shade.bMinGain    = 1;
	    ps->Shade.bMaxGain    = 0x1f;
	    ps->Shade.wDarkLevels = 10;

	    if( ps->Shade.bIntermediate == _ScanMode_Color )
    		WolfsonDAC8143[2].bParam = 0x52;
	    else
    		WolfsonDAC8143[2].bParam = 0x42;

	    if (ps->Shade.bIntermediate == _ScanMode_Mono )
    		WolfsonDAC8143 [0].bParam = 7;
	    else
		    WolfsonDAC8143 [0].bParam = 3;

	    break;

	case _DA_SAMSUNG1224:

       	DBG( DBG_LOW, "Samsung 1224\n" );

	    switch(ps->Device.bCCDID )
	    {
		case _CCD_3797:
           	DBG( DBG_LOW, "CCD-3797\n" );
		    pDAC_CCD                 = ShadingVar3797;
		    p12ccdInitFunc           = fnCCDInitSamsung3797;
		    ps->Device.fnDarkOffset  = fnDarkOffsetSamsung3797;
		    ps->Device.fnDACDark     = fnDACDarkSamsung;
		    ps->Device.pCCDRegisters = (pRegDef)S3797CCDParams;
		    break;

		default:
           	DBG( DBG_LOW, "CCD-3799\n" );
		    pDAC_CCD                 = ShadingVar3799;
		    p12ccdInitFunc           = fnCCDInitSamsung3799;
		    ps->Device.fnDarkOffset  = fnDarkOffsetSamsung3799;
		    ps->Device.fnDACDark     = fnDACDarkSamsung;
		    ps->Device.pCCDRegisters = (pRegDef)S3799CCDParams;
	    }
	    ps->Device.wNumCCDRegs        = _NUM_OF_CCDREGS_S1224;
	    ps->Device.wNumDACRegs        = _NUM_OF_DACREGS_S1224;
	    ps->Device.pDACRegisters      = SamsungDAC1224;
	    ps->Device.RegDACOffset.Red   = 1;
	    ps->Device.RegDACOffset.Green = 2;
	    ps->Device.RegDACOffset.Blue  = 3;
	    ps->Device.RegDACGain.Red     = 4;
	    ps->Device.RegDACGain.Green   = 5;
	    ps->Device.RegDACGain.Blue    = 6;
	    ps->Shade.bGainDouble         = 6;
	    ps->Shade.bUniGain            = 7;
	    ps->Shade.bMinGain            = 0;
	    ps->Shade.bMaxGain            = 0x1f;
	    ps->Shade.wDarkLevels = 10;

	    if( ps->Shade.bIntermediate & _ScanMode_Mono )
    		SamsungDAC1224[0].bParam = 0x57;
	    else
	    	SamsungDAC1224[0].bParam = 0x51;
	    break;

	case _DA_ESIC:

       	DBG( DBG_LOW, "ESIC\n" );

	    switch( ps->Device.bCCDID ) {

		case _CCD_3797:
           	DBG( DBG_LOW, "CCD-3797\n" );
		    pDAC_CCD                 = ShadingVar3797;
		    p12ccdInitFunc           = fnCCDInitWolfson3797;
		    ps->Device.fnDarkOffset  = fnDarkOffsetWolfson3797;
		    ps->Device.fnDACDark     = fnDACDarkWolfson;
		    ps->Device.pCCDRegisters = (pRegDef)W3797CCDParams;
		    break;

		default:
           	DBG( DBG_LOW, "CCD-3799\n" );
		    pDAC_CCD                 = ShadingVar3799;
		    p12ccdInitFunc           = fnCCDInitESIC3799;
		    ps->Device.fnDarkOffset  = fnDarkOffsetWolfson3797;
		    ps->Device.fnDACDark     = fnDACDarkWolfson;
		    ps->Device.pCCDRegisters = (pRegDef)W3799CCDParams;
	    }

	    ps->Device.wNumCCDRegs        = _NUM_OF_CCDREGS_W8143;
	    ps->Device.wNumDACRegs        = _NUM_OF_DACREGS_W8143;
	    ps->Device.pDACRegisters      = WolfsonDAC8143;
	    ps->Device.RegDACOffset.Red   = 0x20;
	    ps->Device.RegDACOffset.Green = 0x21;
	    ps->Device.RegDACOffset.Blue  = 0x22;
	    ps->Device.RegDACGain.Red     = 0x28;
	    ps->Device.RegDACGain.Green   = 0x29;
	    ps->Device.RegDACGain.Blue    = 0x2a;

	    if( ps->Shade.bIntermediate & _ScanMode_AverageOut ) {
    		ps->Shade.bUniGain    = 1;
	    	ps->Shade.bGainDouble = 1;
	    } else {
    		ps->Shade.bUniGain    = 2;
	    	ps->Shade.bGainDouble = 4;
	    }
	    ps->Shade.bMinGain    = 1;
	    ps->Shade.bMaxGain    = 0x1f;
	    ps->Shade.wDarkLevels = 10;

	    if( ps->Shade.bIntermediate == _ScanMode_Color )
    		WolfsonDAC8143 [2].bParam = 0x52;
	    else
	    	WolfsonDAC8143 [2].bParam = 0x42;

	    if(ps->Shade.bIntermediate == _ScanMode_Mono )
    		WolfsonDAC8143[0].bParam = 7;
	    else
	    	WolfsonDAC8143[0].bParam = 3;
	    break;

	/* _DA_SAMSUNG8531 */
	default:

       	DBG( DBG_LOW, "SAMSUNG 8531\n" );
	    switch( ps->Device.bCCDID ) {

		case _CCD_3797:
           	DBG( DBG_LOW, "CCD-3797\n" );
		    pDAC_CCD                 = ShadingVar3797;
		    p12ccdInitFunc           = fnCCDInitSamsung3797;
		    ps->Device.fnDarkOffset  = fnDarkOffsetSamsung3797;
		    ps->Device.fnDACDark     = fnDACDarkSamsung;
		    ps->Device.pCCDRegisters = (pRegDef)S3797CCDParams;
		    break;

		case _CCD_3777:
           	DBG( DBG_LOW, "CCD-3777\n" );
		    pDAC_CCD                 = ShadingVar3777;
		    p12ccdInitFunc           = fnCCDInitSamsung3777;
		    ps->Device.fnDarkOffset  = fnDarkOffsetSamsung3777;
		    ps->Device.fnDACDark     = fnDACDarkSamsung;
		    ps->Device.pCCDRegisters = (pRegDef)S3797CCDParams;
		    ps->Device.f0_8_16 = _TRUE;
		    break;

		default:
          	DBG( DBG_LOW, "CCD-3799\n" );
		    pDAC_CCD                 = ShadingVar3799;
		    p12ccdInitFunc           = fnCCDInitSamsung3799;
		    ps->Device.fnDarkOffset  = fnDarkOffsetSamsung3799;
		    ps->Device.fnDACDark     = fnDACDarkSamsung;
		    ps->Device.pCCDRegisters = (pRegDef)S3799CCDParams;
	    }

	    ps->Device.wNumCCDRegs   = _NUM_OF_CCDREGS_S8531;
	    ps->Device.wNumDACRegs   = _NUM_OF_DACREGS_S8531;
	    ps->Device.pDACRegisters = SamsungDAC8531;
	    ps->Device.RegDACOffset.Red   =
	    ps->Device.RegDACOffset.Green =
	    ps->Device.RegDACOffset.Blue  = 1;
	    ps->Device.RegDACGain.Red   =
	    ps->Device.RegDACGain.Green =
	    ps->Device.RegDACGain.Blue  = 2;
	    ps->Shade.bGainDouble = 6;
	    ps->Shade.bMinGain    = 1;
	    ps->Shade.bMaxGain    = 0x1f;
	    if( ps->DataInf.dwScanFlag & SCANDEF_TPA )
    		ps->Shade.bUniGain = 2;
	    else
    		ps->Shade.bUniGain = 7;

   		ps->Shade.wDarkLevels = 10;

	    if( ps->Shade.bIntermediate & _ScanMode_Mono ) {
    		SamsungDAC8531[0].bParam = 0x57;
	    	SamsungDAC8531[3].bParam = 0x57;
		    SamsungDAC8531[6].bParam = 0x57;
	    } else {
    		SamsungDAC8531[0].bParam = 0x51;
	    	SamsungDAC8531[3].bParam = 0x55;
		    SamsungDAC8531[6].bParam = 0x59;
	    }
    }

    if( shading ) {

        if( !(ps->DataInf.dwScanFlag & SCANDEF_TPA))
	        ps->Shade.pCcdDac = &pDAC_CCD[_REFLECTION];
    	else {
	        if( ps->DataInf.dwScanFlag & SCANDEF_Transparency )
		        ps->Shade.pCcdDac = &pDAC_CCD[_TRANSPARENCY];
    	    else
                ps->Shade.pCcdDac = &pDAC_CCD[_NEGATIVE];
        }
    } else
	    ps->Shade.pCcdDac = &pDAC_CCD[_REFLECTION];

    /* as we now have the correct init function, call it */
    p12ccdInitFunc( ps );

    DBG( DBG_IO, "Programming DAC (%u regs)\n", ps->Device.wNumDACRegs );

    for(w = 0; w < ps->Device.wNumDACRegs; w++ ) {

        DBG( DBG_IO, "[0x%02x] = 0x%02x\n", ps->Device.pDACRegisters[w].bReg,
                                          ps->Device.pDACRegisters[w].bParam );

        IODataRegisterToDAC( ps, ps->Device.pDACRegisters[w].bReg,
                                 ps->Device.pDACRegisters[w].bParam );
    }
}

/* END PLUSTEK-PP_P12CCD.C ..................................................*/
