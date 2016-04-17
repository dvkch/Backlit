/* sane - Scanner Access Now Easy.
   Copyright (C) 2003 Martijn van Oosterhout <kleptog@svana.org>
   Copyright (C) 2003 Thomas Soumarmon <thomas.soumarmon@cogitae.net>

   Originally copied from HP3300 testtools. Original notice follows:

   Copyright (C) 2001 Bertrik Sikken (bertrik@zonnet.nl)

   This file is part of the SANE package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

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

   $Id$
*/


/*
    Core HP5400 functions.
*/


#ifndef _HP5400_H_
#define _HP5400_H_

#include <unistd.h>

#include "hp5400_xfer.h"	/* for EScannerModel */

#define HW_DPI      300		/* horizontal resolution of hardware */
#define HW_LPI      300		/* vertical resolution of hardware */

enum ScanType
{
  SCAN_TYPE_CALIBRATION,
  SCAN_TYPE_PREVIEW,
  SCAN_TYPE_NORMAL
};

/* In case we ever need to track multiple models */
typedef struct
{
  char *pszVendor;
  char *pszName;
}
TScannerModel;

typedef struct
{
  /* transfer buffer */
  void *buffer;			/* Pointer to memory allocated for buffer */
  int roff, goff, boff;		/* Offset into buffer of rows to be copied *next* */
  int bufstart, bufend;		/* What is currently the valid buffer */
  int bpp;			/* Bytes per pixel per colour (1 or 2) */
  int linelength, pixels;	/* Bytes per line from scanner */
  int transfersize;		/* Number of bytes to transfer resulting image */
  int blksize;			/* Size of blocks to pull from scanner */
  int buffersize;		/* Size of the buffer */
}
TDataPipe;

typedef struct
{
  int iXferHandle;		/* handle used for data transfer to HW */
  TDataPipe pipe;		/* Pipe for data */

  int iTopLeftX;		/* in mm */
  int iTopLeftY;		/* in mm */
  /*  int           iSensorSkew;   *//* in units of 1/1200 inch */
  /*  int           iSkipLines;    *//* lines of garbage to skip */
  /*  int           fReg07;        *//* NIASH00019 */
  /*  int           fGamma16;      *//* if TRUE, gamma entries are 16 bit */
/*  int           iExpTime;      */
  /*  int           iReversedHead; *//* Head is reversed */
  /*  int           iBufferSize;   *//* Size of internal scan buffer */
/*  EScannerModel eModel;        */
}
THWParams;

/* The scanner needs a Base DPI off which all it's calibration and
 * offset/size parameters are based.  For the time being this is the same as
 * the iDpi but maybe we want it seperate. This is because while this field
 * would have limited values (300,600,1200,2400) the x/y dpi can vary. The
 * windows interface seems to allow 200dpi (though I've never tried it). We
 * need to decide how these values are related to the HW coordinates. */


typedef struct
{
  int iDpi;			/* horizontal resolution */
  int iLpi;			/* vertical resolution */
  int iTop;			/* in HW coordinates (units HW_LPI) */
  int iLeft;			/* in HW coordinates (units HW_LPI) */
  int iWidth;			/* in HW coordinates (units HW_LPI) */
  int iHeight;			/* in HW coordinates (units HW_LPI) */

  int iBytesPerLine;		/* Resulting bytes per line */
  int iLines;			/* Resulting lines of image */
  int iLinesRead;		/* Lines of image already read */

  int iColourOffset;		/* How far the colours are offset. Currently this is
				 * set by the caller. This doesn't seem to be
				 * necessary anymore since the scanner is doing it
				 * internally. Leave it for the time being as it
				 * may be needed later. */
}
TScanParams;



#endif /* NO _HP5400_H_ */
