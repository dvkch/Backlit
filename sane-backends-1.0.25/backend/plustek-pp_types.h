/* @file plustek-pp_types.h
 * @brief some typedefs and error codes
 *
 * Copyright (C) 2000-2013 Gerhard Jaeger <gerhard@gjaeger.de>
 *
 * History:
 * 0.30 - initial version
 * 0.31 - no changes
 * 0.32 - added _VAR_NOT_USED()
 * 0.33 - no changes
 * 0.34 - no changes
 * 0.35 - no changes
 * 0.36 - added _E_ABORT and _E_VERSION
 * 0.37 - moved _MAX_DEVICES to plustek_scan.h
 *        added pChar and TabDef
 * 0.38 - comment change for _E_NOSUPP
 *        added RGBByteDef, RGBWordDef and RGBULongDef
 *        replaced AllPointer by DataPointer
 *        replaced AllType by DataType
 *        added _LOBYTE and _HIBYTE stuff
 *        added _E_NO_ASIC and _E_NORESOURCE
 * 0.39 - no changes
 * 0.40 - moved _VAR_NOT_USED and TabDef to plustek-share.h
 * 0.41 - no changes
 * 0.42 - moved errorcodes to plustek-share.h
 * 0.43 - no changes
 * 0.44 - define Long and ULong types to use int32_t, so
 *        the code should still work on 64 bit machines
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
#ifndef __DRV_TYPES_H__
#define __DRV_TYPES_H__

/* define some useful types */
typedef int 			Bool;
typedef char 			Char;
typedef char 		   *pChar;
typedef unsigned char 	UChar;
typedef UChar 		   *pUChar;
typedef unsigned char   Byte;
typedef Byte 		   *pByte;

typedef short 			Short;
typedef unsigned short	UShort;
typedef UShort 		   *pUShort;

typedef unsigned int	UInt;
typedef UInt		   *pUInt;

/* these definitions will fail for 64 bit machines! */
#if 0
typedef long 		  	Long;
typedef long  		   *pLong;
typedef unsigned long 	ULong;
#endif

typedef int32_t		  	Long;
typedef int32_t 	   *pLong;
typedef uint32_t	 	ULong;
typedef ULong 		   *pULong;

typedef void 		   *pVoid;

/*
 * the boolean values
 */
#ifndef _TRUE
#	define _TRUE    1
#endif
#ifndef _FALSE
#	define _FALSE   0
#endif

#define _LOWORD(x)	((UShort)(x & 0xffff))
#define _HIWORD(x)	((UShort)(x >> 16))
#define _LOBYTE(x)  ((Byte)((x) & 0xFF))
#define _HIBYTE(x)  ((Byte)((x) >> 8))

/*
 * some useful things...
 */
typedef struct
{
	Byte b1st;
	Byte b2nd;
} WordVal, *pWordVal;

typedef struct
{
    WordVal w1st;
    WordVal w2nd;
} DWordVal, *pDWordVal;

/* useful for RGB-values */
typedef struct {
	Byte Red;
	Byte Green;
	Byte Blue;
} RGBByteDef, *pRGBByteDef;

typedef struct {
	UShort Red;
	UShort Green;
	UShort Blue;
} RGBUShortDef, *pRGBUShortDef;

typedef struct {

    union {
        pUChar  bp;
        pUShort usp;
        pULong  ulp;
    } red;
    union {
        pUChar  bp;
        pUShort usp;
        pULong  ulp;
    } green;
    union {
        pUChar  bp;
        pUShort usp;
        pULong  ulp;
    } blue;

} RBGPtrDef;

typedef struct {
	ULong Red;
	ULong Green;
	ULong Blue;
} RGBULongDef, *pRGBULongDef;

typedef union {
	pUChar	       pb;
	pUShort	       pw;
	pULong	       pdw;
    pRGBByteDef    pbrgb;
    pRGBUShortDef  pusrgb;
    pRGBULongDef   pulrgb;
} DataPointer, *pDataPointer;

typedef union {
	WordVal	 wOverlap;
	DWordVal dwOverlap;
	ULong	 dwValue;
	UShort	 wValue;
	Byte	 bValue;
} DataType, *pDataType;

#endif /* guard __DRV_TYPES_H__ */

/* END PLUSTEK-PP_TYPES.H ...................................................*/
