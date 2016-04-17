/*
  Copyright (C) 2001 Bertrik Sikken (bertrik@zonnet.nl)

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

  $Id$
*/

/*
    Provides a simple interface to read and write data from the scanner,
    without any knowledge whether it's a parallel or USB scanner
*/

#ifndef _NIASH_XFER_H_
#define _NIASH_XFER_H_

#include <stdio.h>		/* for FILE * */

/* register codes for the USB - IEEE1284 bridge */
#define USB_SETUP       0x82
#define EPP_ADDR        0x83
#define EPP_DATA_READ   0x84
#define EPP_DATA_WRITE  0x85
#define SPP_STATUS      0x86
#define SPP_CONTROL     0x87
#define SPP_DATA        0x88


typedef enum
{
  eUnknownModel = 0,
  eHp3300c,
  eHp3400c,
  eHp4300c,
  eAgfaTouch
} EScannerModel;


typedef struct
{
  char *pszVendor;
  char *pszName;
  int iVendor;
  int iProduct;
  EScannerModel eModel;
} TScannerModel;


typedef int (TFnReportDevice) (TScannerModel * pModel,
			       const char *pszDeviceName);


/* Creates our own DBG definitions, externs are define in main.c*/
#ifndef WITH_NIASH
#define DBG fprintf
extern FILE *DBG_MSG;
extern FILE *DBG_ERR;
extern FILE *BG_ASSERT;
#endif /* NO WITH_NIASH */

/* we do not make data prototypes */
#ifndef WITH_NIASH
/* list of supported models, the actual list is in niash_xfer.c */
extern TScannerModel ScannerModels[];
#endif /* NO WITH_NIASH */

STATIC void NiashXferInit (TFnReportDevice * pfnReport);
STATIC int NiashXferOpen (const char *pszName, EScannerModel * peModel);
STATIC void NiashXferClose (int iXferHandle);

STATIC void NiashWriteReg (int iXferHandle, unsigned char bReg,
			   unsigned char bData);
STATIC void NiashReadReg (int iXferHandle, unsigned char bReg,
			  unsigned char *pbData);
STATIC void NiashWriteBulk (int iXferHandle, unsigned char *pabBuf,
			    int iSize);
STATIC void NiashReadBulk (int iXferHandle, unsigned char *pabBuf, int iSize);
STATIC void NiashWakeup (int iXferHandle);

STATIC SANE_Bool MatchUsbDevice (int iVendor, int iProduct,
				 TScannerModel ** ppeModel);

#endif /* _NIASH_XFER_H_ */
