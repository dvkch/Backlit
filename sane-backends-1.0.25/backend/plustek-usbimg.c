/*.............................................................................
 * Project : SANE library for Plustek flatbed scanners.
 *.............................................................................
 */

/** @file plustek-usbimg.c
 *  @brief Image processing functions for copying and scaling image lines.
 *
 * Based on sources acquired from Plustek Inc.<br>
 * Copyright (C) 2001-2007 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * History:
 * - 0.40 - starting version of the USB support
 * - 0.41 - fixed the 14bit problem for LM9831 devices
 * - 0.42 - no changes
 * - 0.43 - no changes
 * - 0.44 - added CIS parts and dumpPic function
 * - 0.45 - added gray scaling functions for CIS devices
 *        - fixed usb_GrayScale16 function
 *        - fixed a bug in usb_ColorScale16_2 function
 *        - fixed endless loop bug
 *        - fixed a bug in usb_GrayScalePseudo16 function
 *        - fixed a bug in usb_GrayDuplicatePseudo16 function
 *        - removed the scaler stuff for CIS devices
 * - 0.46 - minor fixes
 * - 0.47 - added big-endian/little endian stuff
 * - 0.48 - fixed usb_ColorDuplicateGray16() and
 *          usb_ColorScaleGray16()
 *        - added usb_BWScaleFromColor() and usb_BWDuplicateFromColor()
 *        - cleanup
 * - 0.49 - a_bRegs is now part of the device structure
 * - 0.50 - cleanup
 * - 0.51 - added usb_ColorDuplicateGray16_2(), usb_ColorScaleGray16_2()
 *          usb_BWScaleFromColor_2() and usb_BWDuplicateFromColor_2()
 * - 0.52 - cleanup
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

#define _SCALER  1000

static u_char   bShift, Shift;
static u_short  wSum, Mask;

/*
 */
static u_char BitTable[8] = {
	0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01
};

static u_char BitsReverseTable[256] = {
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
	0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
	0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
	0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
	0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
	0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
	0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
	0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
	0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
	0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
	0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
	0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
	0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
	0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
	0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
	0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
	0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

/************************ some helper functions ******************************/

static void ReverseBits( int b, u_char **pTar, int *iByte, int *iWeightSum,
                         int iSrcWeight, int iTarWeight, int cMax )
{
	int bit;

	cMax = (1 << cMax);
	if(iSrcWeight == iTarWeight) {
		for( bit = 1; bit < cMax; bit <<= 1) {
			*iByte <<= 1;
			if(b & bit)
				*iByte |= 1;
			if(*iByte >= 0x100)	{
				**pTar++ = (u_char)*iByte;
				*iByte = 1;
			}
		}
	} else {
		for( bit = 1; bit < cMax; bit <<= 1 ) {

			*iWeightSum += iTarWeight;
			while(*iWeightSum >= iSrcWeight)
			{
				*iWeightSum -= iSrcWeight;
				*iByte <<= 1;
				if(b & bit)
					*iByte |= 1;
				if(*iByte >= 0x100)
				{
					**pTar++ = (u_char)*iByte;
					*iByte = 1;
				}
			}
		}
	}
}

static void usb_ReverseBitStream( u_char *pSrc, u_char *pTar, int iPixels,
                                  int iBufSize, int iSrcWeight/* = 0*/,
                                  int iTarWeight/* = 0*/, int iPadBit/* = 1*/)
{
	int i;
	int iByte = 1;
	int cBytes = iPixels / 8;
	int cBits = iPixels % 8;
	u_char bPad = iPadBit? 0xff: 0;
	u_char *pTarget = pTar;
	int iWeightSum = 0;

	if(iSrcWeight == iTarWeight) {
		if(cBits)
		{
			int cShift = 8 - cBits;
			for(i = cBytes, pSrc = pSrc + cBytes - 1; i > 0; i--, pSrc--, pTar++)
				*pTar = BitsReverseTable[(u_char)((*pSrc << cBits) | (*(pSrc+1) >> cShift))];
			ReverseBits(*(pSrc+1) >> cShift, &pTar, &iByte, &iWeightSum, iSrcWeight, iTarWeight, cBits);
		}
		else /* byte boundary */
		{
			for(i = cBytes, pSrc = pSrc + cBytes - 1; i > 0; i--, pSrc--, pTar++)
				*pTar = BitsReverseTable[*pSrc];
		}
	}
	else /* To shrink or enlarge */
	{
		if(cBits) {
			int cShift = 8 - cBits;
			for(i = cBytes, pSrc = pSrc + cBytes - 1; i > 0; i--, pSrc--)
				ReverseBits((*pSrc << cBits) | (*(pSrc+1) >> cShift), &pTar, &iByte, &iWeightSum, iSrcWeight, iTarWeight, 8);
			ReverseBits(*(pSrc+1) >> cShift, &pTar, &iByte, &iWeightSum, iSrcWeight, iTarWeight, cBits);
		}
		else /* byte boundary */
		{
			for(i = cBytes, pSrc = pSrc + cBytes - 1; i > 0; i--, pSrc--)
				ReverseBits(*pSrc, &pTar, &iByte, &iWeightSum, iSrcWeight, iTarWeight, 8);
		}
	}

	if(iByte != 1) {
		while(iByte < 0x100)
		{
			iByte <<= 1;
			iByte |= iPadBit;
		}
		*pTar++ = (u_char)iByte;
	}

	cBytes = (int)(pTar - pTarget);

	for(i = iBufSize - cBytes; i > 0; i--, pTar++)
		*pTar = bPad;
}

/**
 */
static void usb_AverageColorByte( Plustek_Device *dev )
{
	u_long   dw;
	ScanDef *scan = &dev->scanning;

	if((scan->sParam.bSource == SOURCE_Negative ||
	    scan->sParam.bSource == SOURCE_Transparency) &&
	    scan->sParam.PhyDpi.x > 800) {

		for (dw = 0; dw < (scan->sParam.Size.dwPhyPixels - 1); dw++)
		{
			scan->Red.pcb[dw].a_bColor[0] =
			                 (u_char)(((u_short)scan->Red.pcb[dw].a_bColor[0] +
			                 (u_short)scan->Red.pcb[dw + 1].a_bColor[0]) / 2);

			scan->Green.pcb[dw].a_bColor[0] =
			               (u_char)(((u_short)scan->Green.pcb[dw].a_bColor[0] +
			               (u_short)scan->Green.pcb[dw + 1].a_bColor[0]) / 2);

			scan->Blue.pcb[dw].a_bColor[0] =
			               (u_char)(((u_short)scan->Blue.pcb[dw].a_bColor[0] +
			               (u_short)scan->Blue.pcb[dw + 1].a_bColor[0]) / 2);
		}
	}
}

/**
 */
static void usb_AverageColorWord( Plustek_Device *dev )
{
	u_char   ls = 2;
	u_long   dw;
	ScanDef *scan = &dev->scanning;

	if((scan->sParam.bSource == SOURCE_Negative ||
		scan->sParam.bSource == SOURCE_Transparency) &&
		scan->sParam.PhyDpi.x > 800) {
		
		scan->Red.pcw[0].Colors[0]   = _HILO2WORD(scan->Red.pcw[0].HiLo[0])   >> ls;
		scan->Green.pcw[0].Colors[0] = _HILO2WORD(scan->Green.pcw[0].HiLo[0]) >> ls;
		scan->Blue.pcw[0].Colors[0]  = _HILO2WORD(scan->Blue.pcw[0].HiLo[0])  >> ls;

		for (dw = 0; dw < (scan->sParam.Size.dwPhyPixels - 1); dw++)
		{
			scan->Red.pcw[dw + 1].Colors[0]   = _HILO2WORD(scan->Red.pcw[dw + 1].HiLo[0])   >> ls;
			scan->Green.pcw[dw + 1].Colors[0] = _HILO2WORD(scan->Green.pcw[dw + 1].HiLo[0]) >> ls;
			scan->Blue.pcw[dw + 1].Colors[0]  = _HILO2WORD(scan->Blue.pcw[dw + 1].HiLo[0])  >> ls;

			scan->Red.pcw[dw].Colors[0]   = (u_short)(((u_long)scan->Red.pcw[dw].Colors[0] +
			                                           (u_long)scan->Red.pcw[dw + 1].Colors[0]) / 2);
			scan->Green.pcw[dw].Colors[0] = (u_short)(((u_long)scan->Green.pcw[dw].Colors[0] +
			                                           (u_long)scan->Green.pcw[dw + 1].Colors[0]) / 2);
			scan->Blue.pcw[dw].Colors[0]  = (u_short)(((u_long)scan->Blue.pcw[dw].Colors[0] +
			                                           (u_long)scan->Blue.pcw[dw + 1].Colors[0]) / 2);

			scan->Red.pcw[dw].Colors[0]   = _HILO2WORD(scan->Red.pcw[dw].HiLo[0])   << ls;
			scan->Green.pcw[dw].Colors[0] = _HILO2WORD(scan->Green.pcw[dw].HiLo[0]) << ls;
			scan->Blue.pcw[dw].Colors[0]  = _HILO2WORD(scan->Blue.pcw[dw].HiLo[0])  << ls;
		}

		scan->Red.pcw[dw].Colors[0]   = _HILO2WORD(scan->Red.pcw[dw].HiLo[0])   << ls;
		scan->Green.pcw[dw].Colors[0] = _HILO2WORD(scan->Green.pcw[dw].HiLo[0]) << ls;
		scan->Blue.pcw[dw].Colors[0]  = _HILO2WORD(scan->Blue.pcw[dw].HiLo[0])  << ls;
	}
}

/**
 */
static void usb_AverageGrayByte( Plustek_Device *dev )
{
	u_long   dw;
	ScanDef *scan = &dev->scanning;

	if((scan->sParam.bSource == SOURCE_Negative ||
		scan->sParam.bSource == SOURCE_Transparency) &&
		scan->sParam.PhyDpi.x > 800)
	{
		for (dw = 0; dw < (scan->sParam.Size.dwPhyPixels - 1); dw++)
			scan->Green.pb[dw] = (u_char)(((u_short)scan->Green.pb[dw]+
			                           (u_short)scan->Green.pb[dw+1]) / 2);
	}
}

/**
 */
static void usb_AverageGrayWord( Plustek_Device *dev )
{
	u_long   dw;
	ScanDef *scan = &dev->scanning;

	if((scan->sParam.bSource == SOURCE_Negative ||
		scan->sParam.bSource == SOURCE_Transparency) &&
		scan->sParam.PhyDpi.x > 800)
	{
		scan->Green.pw[0] = _HILO2WORD(scan->Green.philo[0]) >> 2;

		for (dw = 0; dw < (scan->sParam.Size.dwPhyPixels - 1); dw++)
		{
			scan->Green.pw[dw + 1] = _HILO2WORD(scan->Green.philo[dw+1]) >> 2;
			scan->Green.pw[dw] = (u_short)(((u_long)scan->Green.pw[dw]+
			                                (u_long)scan->Green.pw[dw+1]) / 2);
			scan->Green.pw[dw] = _HILO2WORD(scan->Green.philo[dw]) << 2;
		}

		scan->Green.pw[dw] = _HILO2WORD(scan->Green.philo[dw]) << 2;
	}
}

/**
 * returns the zoom value, used for our scaling algorithm (DDA algo
 * digital differential analyzer).
 */
static int usb_GetScaler( ScanDef *scan )
{
	double ratio;

	ratio = (double)scan->sParam.UserDpi.x/
			(double)scan->sParam.PhyDpi.x;	

	return (int)(1.0/ratio * _SCALER);
}

/******************************* the copy functions **************************/

/** do a simple memcopy from scan-buffer to user buffer
 */
static void usb_ColorDuplicate8( Plustek_Device *dev )
{
	int      next;
	u_long   dw, pixels;
	ScanDef *scan = &dev->scanning;

	usb_AverageColorByte( dev );

	if( scan->sParam.bSource == SOURCE_ADF ) {
		next   = -1;
		pixels = scan->sParam.Size.dwPixels - 1;
	} else {
		next   = 1;
		pixels = 0;
	}

	for( dw = 0; dw < scan->sParam.Size.dwPixels; dw++, pixels += next ) {

		scan->UserBuf.pb_rgb[pixels].Red   = scan->Red.pcb[dw].a_bColor[0];
		scan->UserBuf.pb_rgb[pixels].Green = scan->Green.pcb[dw].a_bColor[0];
		scan->UserBuf.pb_rgb[pixels].Blue  = scan->Blue.pcb[dw].a_bColor[0];
	}
}

/** reorder from rgb line to rgb pixel (CIS scanner)
 */
static void usb_ColorDuplicate8_2( Plustek_Device *dev )
{
	int      next;
	u_long   dw, pixels;
	ScanDef *scan = &dev->scanning;

	if( scan->sParam.bSource == SOURCE_ADF ) {
		next  = -1;
		pixels = scan->sParam.Size.dwPixels - 1;
	} else {
		next   = 1;
		pixels = 0;
	}

	for( dw = 0; dw < scan->sParam.Size.dwPixels; dw++, pixels += next ) {

		scan->UserBuf.pb_rgb[pixels].Red   = (u_char)scan->Red.pb[dw];
		scan->UserBuf.pb_rgb[pixels].Green = (u_char)scan->Green.pb[dw];
		scan->UserBuf.pb_rgb[pixels].Blue  = (u_char)scan->Blue.pb[dw];
	}
}

/**
 */
static void usb_ColorDuplicate16( Plustek_Device *dev )
{
	int       next;
	u_char    ls;
	u_long    dw, pixels;
	ScanDef  *scan = &dev->scanning;
	SANE_Bool swap = usb_HostSwap();

	usb_AverageColorWord( dev );

	if( scan->sParam.bSource == SOURCE_ADF ) {
		next   = -1;
		pixels = scan->sParam.Size.dwPixels - 1;
	} else {
		next  = 1;
		pixels = 0;
	}

	if( scan->dwFlag & SCANFLAG_RightAlign )
		ls = Shift;
	else
		ls = 0;

	for (dw = 0; dw < scan->sParam.Size.dwPixels; dw++, pixels += next) {

		if( swap ) {
			scan->UserBuf.pw_rgb[pixels].Red =
			                  _HILO2WORD(scan->Red.pcw[dw].HiLo[0]) >> ls;
			scan->UserBuf.pw_rgb[pixels].Green =
			                  _HILO2WORD(scan->Green.pcw[dw].HiLo[0]) >> ls;
			scan->UserBuf.pw_rgb[pixels].Blue =
			                  _HILO2WORD(scan->Blue.pcw[dw].HiLo[0]) >> ls;
		} else {
			scan->UserBuf.pw_rgb[pixels].Red  = scan->Red.pw[dw]   >> ls;
			scan->UserBuf.pw_rgb[pixels].Green= scan->Green.pw[dw] >> ls;
			scan->UserBuf.pw_rgb[pixels].Blue = scan->Blue.pw[dw]  >> ls;
		}
	}
}

/**
 */
static void usb_ColorDuplicate16_2( Plustek_Device *dev )
{
	int       next;
	u_char    ls;
	HiLoDef   tmp;
	u_long    dw, pixels;
	ScanDef  *scan = &dev->scanning;
	SANE_Bool swap = usb_HostSwap();

	usb_AverageColorWord( dev );

	if( scan->sParam.bSource == SOURCE_ADF ) {
		next   = -1;
		pixels = scan->sParam.Size.dwPixels - 1;
	} else {
		next   = 1;
		pixels = 0;
	}

	if( scan->dwFlag & SCANFLAG_RightAlign )
		ls = Shift;
	else
		ls = 0;
		
	for( dw = 0; dw < scan->sParam.Size.dwPixels; dw++, pixels += next) {

		if( swap ) {
			tmp = *((HiLoDef*)&scan->Red.pw[dw]);
			scan->UserBuf.pw_rgb[pixels].Red = _HILO2WORD(tmp) >> ls;

			tmp = *((HiLoDef*)&scan->Green.pw[dw]);
			scan->UserBuf.pw_rgb[pixels].Green = _HILO2WORD(tmp) >> ls;

			tmp = *((HiLoDef*)&scan->Blue.pw[dw]);
			scan->UserBuf.pw_rgb[pixels].Blue = _HILO2WORD(tmp) >> ls;

		} else {

			scan->UserBuf.pw_rgb[pixels].Red   = scan->Red.pw[dw]   >> ls;
			scan->UserBuf.pw_rgb[pixels].Green = scan->Green.pw[dw] >> ls;
			scan->UserBuf.pw_rgb[pixels].Blue  = scan->Blue.pw[dw]  >> ls;
		}
	}
}

/**
 */
static void usb_ColorDuplicatePseudo16( Plustek_Device *dev )
{
	int      next; 
	u_short  wR, wG, wB;
	u_long   dw, pixels;
	ScanDef *scan = &dev->scanning;

	usb_AverageColorByte( dev );

	if (scan->sParam.bSource == SOURCE_ADF) {
		next   = -1;
		pixels = scan->sParam.Size.dwPixels - 1;
	} else {
		next   = 1;
		pixels = 0;
	}

	wR = (u_short)scan->Red.pcb[0].a_bColor[0];
	wG = (u_short)scan->Green.pcb[0].a_bColor[0];
	wB = (u_short)scan->Blue.pcb[0].a_bColor[0];

	for (dw = 0; dw < scan->sParam.Size.dwPixels; dw++, pixels += next) {

		scan->UserBuf.pw_rgb[pixels].Red   =
		                        (wR + scan->Red.pcb[dw].a_bColor[0]) << bShift;
		scan->UserBuf.pw_rgb[pixels].Green =
		                      (wG + scan->Green.pcb[dw].a_bColor[0]) << bShift;
		scan->UserBuf.pw_rgb[pixels].Blue  =
		                       (wB + scan->Blue.pcb[dw].a_bColor[0]) << bShift;

		wR = (u_short)scan->Red.pcb[dw].a_bColor[0];
		wG = (u_short)scan->Green.pcb[dw].a_bColor[0];
		wB = (u_short)scan->Blue.pcb[dw].a_bColor[0];
	}
}

/**
 */
static void usb_ColorDuplicateGray( Plustek_Device *dev )
{
	int      next;
	u_long   dw, pixels;
	ScanDef *scan = &dev->scanning;

	usb_AverageColorByte( dev );

	if (scan->sParam.bSource == SOURCE_ADF) {
		next   = -1;
		pixels = scan->sParam.Size.dwPixels - 1;
	} else {
		next   = 1;
		pixels = 0;
	}

	switch(scan->fGrayFromColor) {
		
	case 1:
		for (dw = 0; dw < scan->sParam.Size.dwPixels; dw++, pixels += next)
			scan->UserBuf.pb[pixels] = scan->Red.pcb[dw].a_bColor[0];
		break;
	case 2:
		for (dw = 0; dw < scan->sParam.Size.dwPixels; dw++, pixels += next)
			scan->UserBuf.pb[pixels] = scan->Green.pcb[dw].a_bColor[0];
		break;
	case 3:
		for (dw = 0; dw < scan->sParam.Size.dwPixels; dw++, pixels += next)
			scan->UserBuf.pb[pixels] = scan->Blue.pcb[dw].a_bColor[0];
		break;
	}
}

/**
 */
static void usb_ColorDuplicateGray_2( Plustek_Device *dev )
{
	int      next;
	u_long   dw, pixels;
	ScanDef *scan = &dev->scanning;

	usb_AverageColorByte( dev );

	if (scan->sParam.bSource == SOURCE_ADF) {
		next   = -1;
		pixels = scan->sParam.Size.dwPixels - 1;
	} else {
		next   = 1;
		pixels = 0;
	}

	switch(scan->fGrayFromColor)
	{
	case 1:
		for (dw = 0; dw < scan->sParam.Size.dwPixels; dw++, pixels += next)
			scan->UserBuf.pb[pixels] = scan->Red.pb[dw];
		break;
	case 3:
		for (dw = 0; dw < scan->sParam.Size.dwPixels; dw++, pixels += next)
			scan->UserBuf.pb[pixels] = scan->Blue.pb[dw];
		break;
	default:
		for (dw = 0; dw < scan->sParam.Size.dwPixels; dw++, pixels += next)
			scan->UserBuf.pb[pixels] = scan->Green.pb[dw];
		break;
	}
}

/**
 */
static void usb_ColorDuplicateGray16( Plustek_Device *dev )
{
	int       next;
	u_char    ls;
	u_long    dw, pixels;
	ScanDef  *scan = &dev->scanning;
	SANE_Bool swap = usb_HostSwap();

	usb_AverageColorWord( dev );

	if (scan->sParam.bSource == SOURCE_ADF) {
		next   = -1;
		pixels = scan->sParam.Size.dwPixels - 1;
	} else {
		next   = 1;
		pixels = 0;
	}
	if( scan->dwFlag & SCANFLAG_RightAlign )
		ls = Shift;
	else
		ls = 0;

	switch(scan->fGrayFromColor) {

	case 1:
		if( swap ) {
			for (dw = 0; dw < scan->sParam.Size.dwPixels; dw++, pixels += next)
				scan->UserBuf.pw[pixels] =
				                   _HILO2WORD(scan->Red.pcw[dw].HiLo[0]) >> ls;
		} else {
			for (dw = 0; dw < scan->sParam.Size.dwPixels; dw++, pixels += next)
				scan->UserBuf.pw[pixels] = scan->Red.pw[dw] >> ls;
		}
		break;
	case 2:
		if( swap ) {
			for (dw = 0; dw < scan->sParam.Size.dwPixels; dw++, pixels += next)
				scan->UserBuf.pw[pixels] =
				                 _HILO2WORD(scan->Green.pcw[dw].HiLo[0]) >> ls;
		} else {
			for (dw = 0; dw < scan->sParam.Size.dwPixels; dw++, pixels += next)
				scan->UserBuf.pw[pixels] = scan->Green.pw[dw] >> ls;
		}
		break;
	case 3:
		if( swap ) {
			for (dw = 0; dw < scan->sParam.Size.dwPixels; dw++, pixels += next)
				scan->UserBuf.pw[pixels] =
				                   _HILO2WORD(scan->Blue.pcw[dw].HiLo[0]) >> ls;
		} else {
			for (dw = 0; dw < scan->sParam.Size.dwPixels; dw++, pixels += next)
				scan->UserBuf.pw[pixels] = scan->Blue.pw[dw] >> ls;
		}
		break;
	}
}

/**
 */
static void usb_ColorDuplicateGray16_2( Plustek_Device *dev )
{
	int       next;
	u_char    ls;
	u_long    dw, pixels;
	HiLoDef   tmp;
	ScanDef  *scan = &dev->scanning;
	SANE_Bool swap = usb_HostSwap();

	usb_AverageColorWord( dev );

	if (scan->sParam.bSource == SOURCE_ADF) {
		next   = -1;
		pixels = scan->sParam.Size.dwPixels - 1;
	} else {
		next   = 1;
		pixels = 0;
	}
	if( scan->dwFlag & SCANFLAG_RightAlign )
		ls = Shift;
	else
		ls = 0;

	switch(scan->fGrayFromColor) {
	case 1:
		if( swap ) {
			for (dw = 0; dw < scan->sParam.Size.dwPixels; dw++, pixels += next) {
				tmp = *((HiLoDef*)&scan->Red.pw[dw]);
				scan->UserBuf.pw[pixels] = _HILO2WORD(tmp) >> ls;
			}
		} else {
			for (dw = 0; dw < scan->sParam.Size.dwPixels; dw++, pixels += next) {
				scan->UserBuf.pw[pixels] = scan->Red.pw[dw] >> ls;
			}
		}
		break;
	case 2:
		if( swap ) {
			for (dw = 0; dw < scan->sParam.Size.dwPixels; dw++, pixels += next) {
				tmp = *((HiLoDef*)&scan->Green.pw[dw]);
				scan->UserBuf.pw[pixels] = _HILO2WORD(tmp) >> ls;
			}
		} else {
			for (dw = 0; dw < scan->sParam.Size.dwPixels; dw++, pixels += next) {
				scan->UserBuf.pw[pixels] = scan->Green.pw[dw] >> ls;
			}
		}
		break;
	case 3:
		if( swap ) {
			for (dw = 0; dw < scan->sParam.Size.dwPixels; dw++, pixels += next) {
				tmp = *((HiLoDef*)&scan->Blue.pw[dw]);
				scan->UserBuf.pw[pixels] = _HILO2WORD(tmp) >> ls;
			}
		} else {
			for (dw = 0; dw < scan->sParam.Size.dwPixels; dw++, pixels += next) {
				scan->UserBuf.pw[pixels] = scan->Blue.pw[dw] >> ls;
			}
		}
		break;
	}
}

/**
 */
static void usb_GrayDuplicate8( Plustek_Device *dev )
{
	u_char  *dest, *src;
	u_long   pixels;
	ScanDef *scan = &dev->scanning;

	usb_AverageGrayByte( dev );

	if( scan->sParam.bSource == SOURCE_ADF ) {

		pixels = scan->sParam.Size.dwPixels;
		src    = scan->Green.pb;
		dest   = scan->UserBuf.pb + pixels - 1;

		for(; pixels; pixels--, src++, dest--)
			*dest = *src;
	} else {
		memcpy( scan->UserBuf.pb, scan->Green.pb, scan->sParam.Size.dwBytes );
	}
}

/**
 */
static void usb_GrayDuplicate16( Plustek_Device *dev )
{
	int       next;
	u_char    ls;
	u_short  *dest;
	u_long    pixels;
	HiLoDef  *pwm;
	ScanDef  *scan = &dev->scanning;
	SANE_Bool swap = usb_HostSwap();

	usb_AverageGrayWord( dev );

	if( scan->sParam.bSource == SOURCE_ADF ) {
		next = -1;
		dest = scan->UserBuf.pw + scan->sParam.Size.dwPixels - 1;
	} else {
		next = 1;
		dest = scan->UserBuf.pw;
	}

	if( scan->dwFlag & SCANFLAG_RightAlign )
		ls = Shift;
	else
		ls = 0;
	
	pwm = scan->Green.philo;
	for( pixels=scan->sParam.Size.dwPixels; pixels--; pwm++, dest += next ) {
		if( swap )
			*dest = (_PHILO2WORD(pwm)) >> ls;
		else
			*dest = (_PLOHI2WORD(pwm)) >> ls;
	}
}

/**
 */
static void usb_GrayDuplicatePseudo16( Plustek_Device *dev )
{
	u_char  *src;
	int      next;
	u_short  g;
	u_short *dest;
	u_long   pixels;
	ScanDef *scan = &dev->scanning;

	usb_AverageGrayByte( dev );

	if (scan->sParam.bSource == SOURCE_ADF) {
		next = -1;
		dest = scan->UserBuf.pw + scan->sParam.Size.dwPixels - 1;
	} else {
		next = 1;
		dest = scan->UserBuf.pw;
	}

	src = scan->Green.pb;
	g = (u_short)*src;

	for( pixels=scan->sParam.Size.dwPixels; pixels--; src++, dest += next ) {

		*dest = (g + *src) << bShift;
		g = (u_short)*src;
	}
}

/** copy binary data to the user buffer
 */
static void usb_BWDuplicate( Plustek_Device *dev )
{
	ScanDef *scan = &dev->scanning;

	if(scan->sParam.bSource == SOURCE_ADF)
	{
		usb_ReverseBitStream( scan->Green.pb, scan->UserBuf.pb,
		                      scan->sParam.Size.dwValidPixels,
		                      scan->dwBytesLine, 0, 0, 1 );
	} else {
		memcpy( scan->UserBuf.pb, scan->Green.pb, scan->sParam.Size.dwBytes );
	}
}

/** generate binary data from one of the three color inputs according to the
 *  value in fGrayFromColor (CCD version)
 */
static void usb_BWDuplicateFromColor( Plustek_Device *dev )
{
	int           next;
	u_char        d, s, *dest;
	u_short       j;
	u_long        pixels;
	ColorByteDef *src;
	ScanDef      *scan = &dev->scanning;

	if( scan->sParam.bSource == SOURCE_ADF ) {
		dest = scan->UserBuf.pb + scan->sParam.Size.dwPixels - 1;
		next = -1;
	} else {
		dest = scan->UserBuf.pb;
		next = 1;
	}

	switch(scan->fGrayFromColor) {
		case 1:  src = scan->Red.pcb;   break;
		case 3:  src = scan->Blue.pcb;  break;
		default: src = scan->Green.pcb; break;
	}

	d = j = 0;
	for( pixels = scan->sParam.Size.dwPixels; pixels; pixels--, src++ ) {

		s = src->a_bColor[0];
		if( s != 0 )
			d |= BitTable[j];
		j++;
		if( j == 8 ) {
			*dest = d;
			dest += next;

			d = j = 0;
		}
	}
}

/** generate binary data from one of the three color inputs according to the
 *  value in fGrayFromColor (CIS version)
 */
static void usb_BWDuplicateFromColor_2( Plustek_Device *dev )
{
	int      next;
	u_char   d, *dest, *src;
	u_short  j;
	u_long   pixels;
	ScanDef *scan = &dev->scanning;

	if( scan->sParam.bSource == SOURCE_ADF ) {
		dest = scan->UserBuf.pb + scan->sParam.Size.dwPixels - 1;
		next = -1;
	} else {
		dest = scan->UserBuf.pb;
		next = 1;
	}

	switch(scan->fGrayFromColor) {
		case 1:  src = scan->Red.pb;   break;
		case 3:  src = scan->Blue.pb;  break;
		default: src = scan->Green.pb; break;
	}

	d = j = 0;
	for( pixels = scan->sParam.Size.dwPixels; pixels; pixels--, src++ ) {

		if( *src != 0 )
			d |= BitTable[j];
		j++;
		if( j == 8 ) {
			*dest = d;
			dest += next;

			d = j = 0;
		}
	}
}

/************************** the scaling functions ****************************/

/**
 */
static void usb_ColorScaleGray( Plustek_Device *dev )
{
	int           izoom, ddax, next;
	u_long        dw, pixels;
	ColorByteDef *src;
	ScanDef      *scan = &dev->scanning;

	usb_AverageColorByte( dev );
	
	dw = scan->sParam.Size.dwPixels;

	if( scan->sParam.bSource == SOURCE_ADF ) {
		next   = -1;
		pixels = scan->sParam.Size.dwPixels - 1;
	} else {
		next   = 1;
		pixels = 0;
	}

	switch(scan->fGrayFromColor) {
		case 1:  src = scan->Red.pcb;   break;
		case 3:  src = scan->Blue.pcb;  break;
		default: src = scan->Green.pcb; break;
	}

	izoom = usb_GetScaler( scan );
	
	for( ddax = 0; dw; src++ ) {

		ddax -= _SCALER;
		while((ddax < 0) && (dw > 0)) {

			scan->UserBuf.pb[pixels] = src->a_bColor[0];
 
			pixels += next;
			ddax   += izoom;
			dw--;
		}
	} 
}

/**
 */
static void usb_ColorScaleGray_2( Plustek_Device *dev )
{
	u_char  *src;
	int      izoom, ddax, next;
	u_long   dw, pixels;
	ScanDef *scan = &dev->scanning;

	usb_AverageColorByte( dev );

	dw = scan->sParam.Size.dwPixels;

	if( scan->sParam.bSource == SOURCE_ADF ) {
		next   = -1;
		pixels = scan->sParam.Size.dwPixels - 1;
	} else {
		next   = 1;
		pixels = 0;
	}

	switch(scan->fGrayFromColor) {
		case 1:  src = scan->Red.pb;   break;
		case 3:  src = scan->Blue.pb;  break;
		default: src = scan->Green.pb; break;
	}

	izoom = usb_GetScaler( scan );

	for( ddax = 0; dw; src++ ) {

		ddax -= _SCALER;
		while((ddax < 0) && (dw > 0)) {

			scan->UserBuf.pb[pixels] = *src;

			pixels += next;
			ddax   += izoom;
			dw--;
		}
	}
}

/**
 */
static void usb_ColorScaleGray16( Plustek_Device *dev )
{
	u_char    ls;
	int       izoom, ddax, next;
	u_long    dw, pixels, bitsput;
	SANE_Bool swap = usb_HostSwap();
	ScanDef  *scan = &dev->scanning;

	usb_AverageColorByte( dev );

	dw = scan->sParam.Size.dwPixels;

	if( scan->sParam.bSource == SOURCE_ADF ) {
		next   = -1;
		pixels = scan->sParam.Size.dwPixels - 1;
	} else {
		next   = 1;
		pixels = 0;
	}

	izoom = usb_GetScaler( scan );

	if( scan->dwFlag & SCANFLAG_RightAlign )
		ls = Shift;
	else
		ls = 0;

	switch( scan->fGrayFromColor ) {

	case 1:
		for( bitsput = 0, ddax = 0; dw; bitsput++ ) {

			ddax -= _SCALER;

			while((ddax < 0) && (dw > 0)) {
				if( swap ) {
					scan->UserBuf.pw[pixels] =
					        _HILO2WORD(scan->Red.pcw[bitsput].HiLo[0]) >> ls;
				} else {
					scan->UserBuf.pw[pixels] = scan->Red.pw[bitsput] >> ls;
				}
				pixels += next;
				ddax   += izoom;
				dw--;
			}
		}
		break;

	case 2:
		for( bitsput = 0, ddax = 0; dw; bitsput++ ) {

			ddax -= _SCALER;

			while((ddax < 0) && (dw > 0)) {
				if( swap ) {
					scan->UserBuf.pw[pixels] =
					      _HILO2WORD(scan->Green.pcw[bitsput].HiLo[0]) >> ls;
				} else {
					scan->UserBuf.pw[pixels] = scan->Green.pw[bitsput] >> ls;
				}
				pixels += next;
				ddax   += izoom;
				dw--;
			}
		}
		break;

	case 3:
		for( bitsput = 0, ddax = 0; dw; bitsput++ ) {

			ddax -= _SCALER;

			while((ddax < 0) && (dw > 0)) {
				if( swap ) {
					scan->UserBuf.pw[pixels] =
					       _HILO2WORD(scan->Blue.pcw[bitsput].HiLo[0]) >> ls;
				} else {
					scan->UserBuf.pw[pixels] = scan->Blue.pw[bitsput] >> ls;
				}
				pixels += next;
				ddax   += izoom;
				dw--;
			}
		}
		break;
	}
}

/**
 */
static void usb_ColorScaleGray16_2( Plustek_Device *dev )
{
	u_char    ls;
	int       izoom, ddax, next;
	u_long    dw, pixels, bitsput;
	HiLoDef   tmp;
	SANE_Bool swap = usb_HostSwap();
	ScanDef  *scan = &dev->scanning;

	usb_AverageColorByte( dev );

	dw = scan->sParam.Size.dwPixels;

	if( scan->sParam.bSource == SOURCE_ADF ) {
		next   = -1;
		pixels = scan->sParam.Size.dwPixels - 1;
	} else {
		next   = 1;
		pixels = 0;
	}

	izoom = usb_GetScaler( scan );

	if( scan->dwFlag & SCANFLAG_RightAlign )
		ls = Shift;
	else
		ls = 0;

	switch( scan->fGrayFromColor ) {

	case 1:
		for( bitsput = 0, ddax = 0; dw; bitsput++ ) {

			ddax -= _SCALER;

			while((ddax < 0) && (dw > 0)) {
				if( swap ) {
					tmp = *((HiLoDef*)&scan->Red.pw[bitsput]);
					scan->UserBuf.pw[pixels] = _HILO2WORD(tmp) >> ls;
				} else {
					scan->UserBuf.pw[pixels] = scan->Red.pw[dw] >> ls;
				}
				pixels += next;
				ddax   += izoom;
				dw--;
			}
		}
		break;

	case 2:
		for( bitsput = 0, ddax = 0; dw; bitsput++ ) {

			ddax -= _SCALER;

			while((ddax < 0) && (dw > 0)) {
				if( swap ) {
					tmp = *((HiLoDef*)&scan->Green.pw[bitsput]);
					scan->UserBuf.pw[pixels] = _HILO2WORD(tmp) >> ls;
				} else {
					scan->UserBuf.pw[pixels] = scan->Green.pw[bitsput] >> ls;
				}
				pixels += next;
				ddax   += izoom;
				dw--;
			}
		}
		break;

	case 3:
		for( bitsput = 0, ddax = 0; dw; bitsput++ ) {

			ddax -= _SCALER;

			while((ddax < 0) && (dw > 0)) {
				if( swap ) {
					tmp = *((HiLoDef*)&scan->Blue.pw[bitsput]);
					scan->UserBuf.pw[pixels] = _HILO2WORD(tmp) >> ls;
				} else {
					scan->UserBuf.pw[pixels] = scan->Blue.pw[bitsput] >> ls;
				}
				pixels += next;
				ddax   += izoom;
				dw--;
			}
		}
		break;
	}
}

/** here we copy and scale from scanner world to user world...
 */
static void usb_ColorScale8( Plustek_Device *dev )
{
	int      izoom, ddax, next;
	u_long   dw, pixels, bitsput;
    ScanDef *scan = &dev->scanning;

	usb_AverageColorByte( dev );

	dw = scan->sParam.Size.dwPixels;

	if( scan->sParam.bSource == SOURCE_ADF ) {
		next   = -1;
		pixels = scan->sParam.Size.dwPixels - 1;
	} else {
		next   = 1;
		pixels = 0;
	}

	izoom = usb_GetScaler( scan );	

	for( bitsput = 0, ddax = 0; dw; bitsput++ ) {

		ddax -= _SCALER;

		while((ddax < 0) && (dw > 0)) {

			scan->UserBuf.pb_rgb[pixels].Red =
			                            scan->Red.pcb[bitsput].a_bColor[0];
			scan->UserBuf.pb_rgb[pixels].Green =
			                            scan->Green.pcb[bitsput].a_bColor[0];
			scan->UserBuf.pb_rgb[pixels].Blue =
			                            scan->Blue.pcb[bitsput].a_bColor[0];
			pixels += next;
			ddax   += izoom;
			dw--;
		}
	}
}

static void usb_ColorScale8_2( Plustek_Device *dev )
{
	int      izoom, ddax, next;
	u_long   dw, pixels, bitsput;
	ScanDef *scan = &dev->scanning;

	dw = scan->sParam.Size.dwPixels;

	if( scan->sParam.bSource == SOURCE_ADF ) {
		next   = -1;
		pixels = scan->sParam.Size.dwPixels - 1;
	} else {
		next   = 1;
		pixels = 0;
	}

	izoom = usb_GetScaler( scan );

	for( bitsput = 0, ddax = 0; dw; bitsput++ ) {

		ddax -= _SCALER;

		while((ddax < 0) && (dw > 0)) {

			scan->UserBuf.pb_rgb[pixels].Red   = scan->Red.pb[bitsput];
			scan->UserBuf.pb_rgb[pixels].Green = scan->Green.pb[bitsput];
			scan->UserBuf.pb_rgb[pixels].Blue  = scan->Blue.pb[bitsput];

			pixels += next;
			ddax   += izoom;
			dw--;
		}
	}
}

/**
 */
static void usb_ColorScale16( Plustek_Device *dev )
{
	u_char    ls;
	int       izoom, ddax, next;
	u_long    dw, pixels, bitsput;
	SANE_Bool swap = usb_HostSwap();
	ScanDef  *scan = &dev->scanning;

	usb_AverageColorWord( dev );

	dw = scan->sParam.Size.dwPixels;

	if( scan->sParam.bSource == SOURCE_ADF ) {
		next   = -1;
		pixels = scan->sParam.Size.dwPixels - 1;
	} else {
		next   = 1;
		pixels = 0;
	}

	izoom = usb_GetScaler( scan );

	if( scan->dwFlag & SCANFLAG_RightAlign )
		ls = Shift;
	else
		ls = 0;

	for( bitsput = 0, ddax = 0; dw; bitsput++ ) {

		ddax -= _SCALER;

		while((ddax < 0) && (dw > 0)) {

			if( swap ) {

				scan->UserBuf.pw_rgb[pixels].Red =
				          _HILO2WORD(scan->Red.pcw[bitsput].HiLo[0]) >> ls;

				scan->UserBuf.pw_rgb[pixels].Green =
					      _HILO2WORD(scan->Green.pcw[bitsput].HiLo[0]) >> ls;

				scan->UserBuf.pw_rgb[pixels].Blue =
					      _HILO2WORD(scan->Blue.pcw[bitsput].HiLo[0]) >> ls;

			} else {

				scan->UserBuf.pw_rgb[pixels].Red   = scan->Red.pw[bitsput]>>ls;
				scan->UserBuf.pw_rgb[pixels].Green = scan->Green.pw[bitsput] >> ls;
				scan->UserBuf.pw_rgb[pixels].Blue  = scan->Blue.pw[bitsput] >> ls;
			}
			pixels += next;
			ddax   += izoom;
			dw--;
		}
	}
}

/**
 */
static void usb_ColorScale16_2( Plustek_Device *dev )
{
	u_char     ls;
	HiLoDef    tmp;
	int        izoom, ddax, next;
	u_long     dw, pixels, bitsput;
	SANE_Bool  swap = usb_HostSwap();
	ScanDef   *scan = &dev->scanning;

	usb_AverageColorWord( dev );

	dw = scan->sParam.Size.dwPixels;

	if( scan->sParam.bSource == SOURCE_ADF ) {
		next   = -1;
		pixels = scan->sParam.Size.dwPixels - 1;
	} else {
		next   = 1;
		pixels = 0;
	}

	izoom = usb_GetScaler( scan );

	if( scan->dwFlag & SCANFLAG_RightAlign )
		ls = Shift;
	else
		ls = 0;

	for( bitsput = 0, ddax = 0; dw; bitsput++ ) {

		ddax -= _SCALER;

		while((ddax < 0) && (dw > 0)) {

			if( swap ) {
					
				tmp = *((HiLoDef*)&scan->Red.pw[bitsput]);
				scan->UserBuf.pw_rgb[pixels].Red = _HILO2WORD(tmp) >> ls;

				tmp = *((HiLoDef*)&scan->Green.pw[bitsput]);
				scan->UserBuf.pw_rgb[pixels].Green = _HILO2WORD(tmp) >> ls;

				tmp = *((HiLoDef*)&scan->Blue.pw[bitsput]);
				scan->UserBuf.pw_rgb[pixels].Blue = _HILO2WORD(tmp) >> ls;

			} else {

				scan->UserBuf.pw_rgb[pixels].Red   = scan->Red.pw[bitsput] >> ls;
				scan->UserBuf.pw_rgb[pixels].Green = scan->Green.pw[bitsput] >> ls;
				scan->UserBuf.pw_rgb[pixels].Blue  = scan->Blue.pw[bitsput] >> ls;
			}
			pixels += next;
			ddax   += izoom;
			dw--;
		}
	}
}

/**
 */
static void usb_ColorScalePseudo16( Plustek_Device *dev )
{
	int      izoom, ddax, next;
	u_short  wR, wG, wB;
	u_long   dw, pixels, bitsput;
	ScanDef *scan = &dev->scanning;

	usb_AverageColorByte( dev );

	dw = scan->sParam.Size.dwPixels;

	if( scan->sParam.bSource == SOURCE_ADF ) {
		next   = -1;
		pixels = scan->sParam.Size.dwPixels - 1;
	} else {
		next   = 1;
		pixels = 0;
	}
	
	izoom = usb_GetScaler( scan );

	wR = (u_short)scan->Red.pcb[0].a_bColor[0];
	wG = (u_short)scan->Green.pcb[0].a_bColor[1];
	wB = (u_short)scan->Blue.pcb[0].a_bColor[2];

	for( bitsput = 0, ddax = 0; dw; bitsput++ ) {

		ddax -= _SCALER;

		while((ddax < 0) && (dw > 0)) {

			scan->UserBuf.pw_rgb[pixels].Red =
				(wR + scan->Red.pcb[bitsput].a_bColor[0]) << bShift;
				
			scan->UserBuf.pw_rgb[pixels].Green =
				(wG + scan->Green.pcb[bitsput].a_bColor[0]) << bShift;
				
			scan->UserBuf.pw_rgb[pixels].Blue =
				(wB + scan->Blue.pcb[bitsput].a_bColor[0]) << bShift;
		
			pixels += next;
			ddax   += izoom;
			dw--;
		}

		wR = (u_short)scan->Red.pcb[bitsput].a_bColor[0];
		wG = (u_short)scan->Green.pcb[bitsput].a_bColor[0];
		wB = (u_short)scan->Blue.pcb[bitsput].a_bColor[0];
	}
}

/**
 */
static void usb_BWScale( Plustek_Device *dev )
{
	u_char   tmp, *dest, *src;
	int      izoom, ddax;
	u_long   i, dw;
	ScanDef *scan = &dev->scanning;

	src = scan->Green.pb;
	if( scan->sParam.bSource == SOURCE_ADF ) {
		int iSum = wSum;
		usb_ReverseBitStream(scan->Green.pb, scan->UserBuf.pb,
		                     scan->sParam.Size.dwValidPixels,
		                     scan->dwBytesLine, scan->sParam.PhyDpi.x,
		                     scan->sParam.UserDpi.x, 1 );
		wSum = iSum;
		return;
	} else {
		dest = scan->UserBuf.pb;
	}

	izoom = usb_GetScaler( scan );
	
	memset( dest, 0, scan->dwBytesLine );
	ddax = 0;
	dw   = 0;

	for( i = 0; i < scan->sParam.Size.dwValidPixels; i++ ) {

		ddax -= _SCALER;

		while( ddax < 0 ) {

			tmp = src[(i>>3)];

			if((dw>>3) < scan->sParam.Size.dwValidPixels ) {
			
				if( 0 != (tmp &= (1 << ((~(i & 0x7))&0x7))))
					dest[dw>>3] |= (1 << ((~(dw & 0x7))&0x7));
			}
			dw++;
			ddax += izoom;
		}
	}
}

/**
 */
static void usb_BWScaleFromColor( Plustek_Device *dev )
{
	u_char        d, s, *dest;
	u_short       j;
	u_long        pixels;
	int           izoom, ddax, next;
	ColorByteDef *src;
	ScanDef      *scan = &dev->scanning;

	if (scan->sParam.bSource == SOURCE_ADF) {
		dest = scan->UserBuf.pb + scan->sParam.Size.dwPixels - 1;
		next = -1;
	} else {
		dest = scan->UserBuf.pb;
		next = 1;
	}

	/* setup the source buffer */
	switch(scan->fGrayFromColor) {
	case 1:  src = scan->Red.pcb;   break;
	case 3:  src = scan->Blue.pcb;  break;
	default: src = scan->Green.pcb; break;
	}

	izoom = usb_GetScaler( scan );
	ddax  = 0;

	d = j = 0;
	for( pixels = scan->sParam.Size.dwPixels; pixels; src++ ) {
	
		ddax -= _SCALER;

		while((ddax < 0) && (pixels > 0)) {

			s = src->a_bColor[0];
			if( s != 0 )
				d |= BitTable[j];
			j++;
			if( j == 8 ) {
				*dest = d;
				dest += next;
				d = j = 0;
			}
			ddax   += izoom;
			pixels--;
		}
	}
}

/**
 */
static void usb_BWScaleFromColor_2( Plustek_Device *dev )
{
	u_char        d, *dest, *src;
	u_short       j;
	u_long        pixels;
	int           izoom, ddax, next;
	ScanDef      *scan = &dev->scanning;

	if (scan->sParam.bSource == SOURCE_ADF) {
		dest = scan->UserBuf.pb + scan->sParam.Size.dwPixels - 1;
		next = -1;
	} else {
		dest = scan->UserBuf.pb;
		next = 1;
	}

	/* setup the source buffer */
	switch(scan->fGrayFromColor) {
	case 1:  src = scan->Red.pb;   break;
	case 3:  src = scan->Blue.pb;  break;
	default: src = scan->Green.pb; break;
	}

	izoom = usb_GetScaler( scan );
	ddax  = 0;

	d = j = 0;
	for( pixels = scan->sParam.Size.dwPixels; pixels; src++ ) {
	
		ddax -= _SCALER;

		while((ddax < 0) && (pixels > 0)) {

			if( *src != 0 )
				d |= BitTable[j];
			j++;
			if( j == 8 ) {
				*dest = d;
				dest += next;
				d = j = 0;
			}
			ddax   += izoom;
			pixels--;
		}
	}
}

/**
 */
static void usb_GrayScale8( Plustek_Device *dev )
{
	u_char  *dest, *src;
	int      izoom, ddax, next;
	u_long   pixels;
	ScanDef *scan = &dev->scanning;

	usb_AverageGrayByte( dev );

	src = scan->Green.pb;
	if( scan->sParam.bSource == SOURCE_ADF ) {
		dest = scan->UserBuf.pb + scan->sParam.Size.dwPixels - 1;
		next = -1;
	} else {
		dest = scan->UserBuf.pb;
		next = 1;
	}
	
	izoom = usb_GetScaler( scan );
	ddax  = 0;

	for( pixels = scan->sParam.Size.dwPixels; pixels; src++ ) {

		ddax -= _SCALER;

		while((ddax < 0) && (pixels > 0)) {

			*dest = *src;
			dest += next;
			ddax   += izoom;
			pixels--;
		}
	}
}

/**
 */
static void usb_GrayScale16( Plustek_Device *dev )
{
	u_char    ls;
	int       izoom, ddax, next;
	u_short  *dest;
	u_long    pixels;
	HiLoDef  *pwm;
	ScanDef  *scan = &dev->scanning;
	SANE_Bool swap = usb_HostSwap();

	usb_AverageGrayWord( dev);

	pwm  = scan->Green.philo;
	wSum = scan->sParam.PhyDpi.x;

	if( scan->sParam.bSource == SOURCE_ADF ) {
		next = -1;
		dest = scan->UserBuf.pw + scan->sParam.Size.dwPixels - 1;
	} else {
		next = 1;
		dest = scan->UserBuf.pw;
	}
	
	izoom = usb_GetScaler( scan );
	ddax  = 0;

	if( scan->dwFlag & SCANFLAG_RightAlign )
		ls = Shift;
	else
		ls = 0;

	for( pixels = scan->sParam.Size.dwPixels; pixels; pwm++ ) {

		ddax -= _SCALER;

		while((ddax < 0) && (pixels > 0)) {

			if( swap )
				*dest = _PHILO2WORD(pwm) >> ls;
			else
				*dest = _PLOHI2WORD(pwm) >> ls;

			dest += next;
			ddax += izoom;
			pixels--;
		}
	}
}

/**
 */
static void usb_GrayScalePseudo16( Plustek_Device *dev )
{
	u_char  *src;
	int      izoom, ddax, next;
	u_short *dest, g;
	u_long   pixels;
	ScanDef *scan = &dev->scanning;

	usb_AverageGrayByte( dev );

	if( scan->sParam.bSource == SOURCE_ADF ) {
		next = -1;
		dest = scan->UserBuf.pw + scan->sParam.Size.dwPixels - 1;
	} else {
		next = 1;
		dest = scan->UserBuf.pw;
	}

	src = scan->Green.pb;
	g   = (u_short)*src;

	izoom = usb_GetScaler( scan );
	ddax  = 0;

	for( pixels = scan->sParam.Size.dwPixels; pixels; src++ ) {

		ddax -= _SCALER;

		while((ddax < 0) && (pixels > 0)) {

			*dest = (g + *src) << bShift;
			dest += next;
			ddax += izoom;
			pixels--;
		}
		g = (u_short)*src;
	}
}

/** function to select the apropriate pixel copy function
 */
static void usb_GetImageProc( Plustek_Device *dev )
{
    ScanDef  *scan = &dev->scanning;
	DCapsDef *sc   = &dev->usbDev.Caps;
	HWDef    *hw   = &dev->usbDev.HwSetting;

	bShift = 0;

	if( scan->sParam.UserDpi.x != scan->sParam.PhyDpi.x ) {

		/* Pixel scaling... */
		switch( scan->sParam.bDataType ) {

			case SCANDATATYPE_Color:
				if (scan->sParam.bBitDepth > 8) {

					if( usb_IsCISDevice(dev)){ 
						scan->pfnProcess = usb_ColorScale16_2;
						DBG( _DBG_INFO, "ImageProc is: ColorScale16_2\n" );
					} else {
						scan->pfnProcess = usb_ColorScale16;
						DBG( _DBG_INFO, "ImageProc is: ColorScale16\n" );
					}
					if (scan->fGrayFromColor) {
						if( usb_IsCISDevice(dev)){ 
							scan->pfnProcess = usb_ColorScaleGray16_2;
							DBG( _DBG_INFO, "ImageProc is: ColorScaleGray16_2\n" );
						} else {
							scan->pfnProcess = usb_ColorScaleGray16;
							DBG( _DBG_INFO, "ImageProc is: ColorScaleGray16\n" );
						}
					}
				} else if (scan->dwFlag & SCANFLAG_Pseudo48) {
					scan->pfnProcess = usb_ColorScalePseudo16;
					DBG( _DBG_INFO, "ImageProc is: ColorScalePseudo16\n" );
					
				} else if (scan->fGrayFromColor) {

					if( usb_IsCISDevice(dev)){
						if (scan->fGrayFromColor > 7 ) {
							scan->pfnProcess = usb_BWScaleFromColor_2;
							DBG( _DBG_INFO, "ImageProc is: BWScaleFromColor_2\n" );
						} else {
							scan->pfnProcess = usb_ColorScaleGray_2;
							DBG( _DBG_INFO, "ImageProc is: ColorScaleGray_2\n" );
						}
					} else {
						if (scan->fGrayFromColor > 7 ) {
							scan->pfnProcess = usb_BWScaleFromColor;
							DBG( _DBG_INFO, "ImageProc is: BWScaleFromColor\n" );
						} else {
							scan->pfnProcess = usb_ColorScaleGray;
							DBG( _DBG_INFO, "ImageProc is: ColorScaleGray\n" );
						}
					}
				} else {

					if( usb_IsCISDevice(dev)){ 
						scan->pfnProcess = usb_ColorScale8_2;
						DBG( _DBG_INFO, "ImageProc is: ColorScale8_2\n" );
					} else {
						scan->pfnProcess = usb_ColorScale8;
						DBG( _DBG_INFO, "ImageProc is: ColorScale8\n" );
					}
				}
				break;

			case SCANDATATYPE_Gray:
				if (scan->sParam.bBitDepth > 8) {
					scan->pfnProcess = usb_GrayScale16;
					DBG( _DBG_INFO, "ImageProc is: GrayScale16\n" );
				} else {

					if (scan->dwFlag & SCANFLAG_Pseudo48) {
						scan->pfnProcess = usb_GrayScalePseudo16;
						DBG( _DBG_INFO, "ImageProc is: GrayScalePseudo16\n" );
					} else {
						scan->pfnProcess = usb_GrayScale8;
						DBG( _DBG_INFO, "ImageProc is: GrayScale8\n" );
					}
				}
				break;

			default:
				scan->pfnProcess = usb_BWScale;
				DBG( _DBG_INFO, "ImageProc is: BWScale\n" );
				break;
		}

	} else {

		/* Pixel copy */
		switch( scan->sParam.bDataType ) {

			case SCANDATATYPE_Color:
				if (scan->sParam.bBitDepth > 8) {
					if( usb_IsCISDevice(dev)){
						scan->pfnProcess = usb_ColorDuplicate16_2;
						DBG( _DBG_INFO, "ImageProc is: ColorDuplicate16_2\n" );
					} else {
						scan->pfnProcess = usb_ColorDuplicate16;
						DBG( _DBG_INFO, "ImageProc is: ColorDuplicate16\n" );
					}
					if (scan->fGrayFromColor) {
						if( usb_IsCISDevice(dev)){
							scan->pfnProcess = usb_ColorDuplicateGray16_2;
							DBG( _DBG_INFO, "ImageProc is: ColorDuplicateGray16_2\n" );
						} else {
							scan->pfnProcess = usb_ColorDuplicateGray16;
							DBG( _DBG_INFO, "ImageProc is: ColorDuplicateGray16\n" );
						}
					}
				} else if (scan->dwFlag & SCANFLAG_Pseudo48) {
					scan->pfnProcess = usb_ColorDuplicatePseudo16;
					DBG( _DBG_INFO, "ImageProc is: ColorDuplicatePseudo16\n" );
				} else if (scan->fGrayFromColor) {
					if( usb_IsCISDevice(dev)){
						if (scan->fGrayFromColor > 7 ) {
							scan->pfnProcess = usb_BWDuplicateFromColor_2;
							DBG( _DBG_INFO, "ImageProc is: BWDuplicateFromColor_2\n" );
						} else {
							scan->pfnProcess = usb_ColorDuplicateGray_2;
							DBG( _DBG_INFO, "ImageProc is: ColorDuplicateGray_2\n" );
						}
					} else {
						if (scan->fGrayFromColor > 7 ) {
							scan->pfnProcess = usb_BWDuplicateFromColor;
							DBG( _DBG_INFO, "ImageProc is: BWDuplicateFromColor\n" );
						} else {
							scan->pfnProcess = usb_ColorDuplicateGray;
							DBG( _DBG_INFO, "ImageProc is: ColorDuplicateGray\n" );
						}
					}
				} else {
					if( usb_IsCISDevice(dev)){ 
						scan->pfnProcess = usb_ColorDuplicate8_2;
						DBG( _DBG_INFO, "ImageProc is: ColorDuplicate8_2\n" );
					} else {
						scan->pfnProcess = usb_ColorDuplicate8;
						DBG( _DBG_INFO, "ImageProc is: ColorDuplicate8\n" );
					}
				}
				break;

			case SCANDATATYPE_Gray:
				if (scan->sParam.bBitDepth > 8) {
					scan->pfnProcess = usb_GrayDuplicate16;
					DBG( _DBG_INFO, "ImageProc is: GrayDuplicate16\n" );
				} else {
					if (scan->dwFlag & SCANFLAG_Pseudo48) {
						scan->pfnProcess = usb_GrayDuplicatePseudo16;
						DBG( _DBG_INFO, "ImageProc is: GrayDuplicatePseudo16\n" );
					} else {
						scan->pfnProcess = usb_GrayDuplicate8;
						DBG( _DBG_INFO, "ImageProc is: GrayDuplicate8\n" );
					}
				}
				break;

			default:
				scan->pfnProcess = usb_BWDuplicate;
				DBG( _DBG_INFO, "ImageProc is: BWDuplicate\n" );
				break;
		}
	}
	
	if( scan->sParam.bBitDepth == 8 ) {
	
		if( scan->dwFlag & SCANFLAG_Pseudo48 ) {
			if( scan->dwFlag & SCANFLAG_RightAlign ) {
				bShift = 5;
			} else {

				/* this should fix the Bearpaw/U12 discrepancy
				 * in general the fix is needed, but not for the U12
				 * why? - no idea!
				 */
				if(_WAF_BSHIFT7_BUG == (_WAF_BSHIFT7_BUG & sc->workaroundFlag))
					bShift = 0; /* Holger Bischof 16.12.2001 */
				else
					bShift = 7;
			}
			DBG( _DBG_INFO, "bShift adjusted: %u\n", bShift );
		}
	}

	if( _LM9833 == hw->chip ) {
		Shift = 0;
		Mask  = 0xFFFF;
	} else {
		Shift = 2;
		Mask  = 0xFFFC;
	}
}

/**
 * here we read the image data into our intermediate buffer (in the NT version
 * the function was implemented as thread)
 */
static SANE_Int usb_ReadData( Plustek_Device *dev )
{
	u_long   dw, dwRet, dwBytes, pl;
	ScanDef *scan = &dev->scanning;
	HWDef   *hw   = &dev->usbDev.HwSetting;

	DBG( _DBG_READ, "usb_ReadData()\n" );

	pl = dev->usbDev.a_bRegs[0x4e] * hw->wDRAMSize/128;

	while( scan->sParam.Size.dwTotalBytes ) {

		if( usb_IsEscPressed()) {
			DBG( _DBG_INFO, "usb_ReadData() - Cancel detected...\n" );
			return 0;
		}

		if( scan->sParam.Size.dwTotalBytes > scan->dwBytesScanBuf )
			dw = scan->dwBytesScanBuf;
		else
			dw = scan->sParam.Size.dwTotalBytes;

		scan->sParam.Size.dwTotalBytes -= dw;

		if(!scan->sParam.Size.dwTotalBytes && dw < (pl * 1024))
		{
			if(!(dev->usbDev.a_bRegs[0x4e] = (u_char)ceil((double)dw /
			                                         (4.0 * hw->wDRAMSize)))) {
				dev->usbDev.a_bRegs[0x4e] = 1;
			}
			dev->usbDev.a_bRegs[0x4f] = 0;

			sanei_lm983x_write( dev->fd, 0x4e, &dev->usbDev.a_bRegs[0x4e], 2, SANE_TRUE );
		}

		while( scan->bLinesToSkip ) {

			DBG( _DBG_READ, "Skipping %u lines\n", scan->bLinesToSkip );

			dwBytes = scan->bLinesToSkip * scan->sParam.Size.dwPhyBytes;

			if (dwBytes > scan->dwBytesScanBuf) {

				dwBytes = scan->dwBytesScanBuf;
				scan->bLinesToSkip -= scan->dwLinesScanBuf;
			} else {
				scan->bLinesToSkip = 0;
			}

			if( !usb_ScanReadImage( dev, scan->pbGetDataBuf, dwBytes ))
				return 0;
		}

		if( usb_ScanReadImage( dev, scan->pbGetDataBuf, dw )) {

			dumpPic("plustek-pic.raw", scan->pbGetDataBuf, dw, 0);

			if( scan->dwLinesDiscard ) {

				DBG(_DBG_READ, "Discarding %lu lines\n", scan->dwLinesDiscard);

				dwRet = dw / scan->sParam.Size.dwPhyBytes;

				if (scan->dwLinesDiscard > dwRet) {
					scan->dwLinesDiscard -= dwRet;
					dwRet = 0;
				} else	{
					dwRet -= scan->dwLinesDiscard;
					scan->dwLinesDiscard = 0;
				}
			} else {

				dwRet = dw / scan->sParam.Size.dwPhyBytes;
			}

			scan->pbGetDataBuf += scan->dwBytesScanBuf;
			if( scan->pbGetDataBuf >= scan->pbScanBufEnd ) {
				scan->pbGetDataBuf = scan->pbScanBufBegin;
			}

			if( dwRet )
				return dwRet;
		}
	}
	return 0;
}

/* END PLUSTEK-USBIMG.C .....................................................*/
