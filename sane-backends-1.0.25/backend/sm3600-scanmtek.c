/* sane - Scanner Access Now Easy.
   Copyright (C) Marian Eichholz 2001
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
*/

/* ======================================================================

Userspace scan tool for the Microtek 3600 scanner

$Id$

====================================================================== */

#include "sm3600-scantool.h"

static struct {
  TModel         model;
  unsigned short idProduct;
 } aScanners[]={
  { sm3600, 0x40B3 },
  { sm3600, 0x40CA },
  { sm3600, 0x40FF },
  { sm3700, 0x40B8 },
  { sm3700, 0x40CB },
  { sm3750, 0x40dd },
  { sm3600, 0x40FF }, /* unknown */
  { unknown, 0x0000 } };

__SM3600EXPORT__
TModel GetScannerModel(unsigned short idVendor,
		       unsigned short idProduct)
{
  int i;
  if (idVendor!=SCANNER_VENDOR) return unknown;
  for (i=0; aScanners[i].model!=unknown; i++)
    if (aScanners[i].idProduct==idProduct)
      return aScanners[i].model;
  return unknown;
}

/* **********************************************************************

DoInit()

Replay the first initialisation block (no slider movement).

********************************************************************** */

__SM3600EXPORT__
TState DoInit(TInstance *this)
{
  unsigned char uchRegs2466[]={
      0x00 /*0x01*/, 0x00 /*0x02*/, 0x3F /*0x03*/,
      0x10 /*0x04*/, 0xC0 /*0x05*/, 0x00 /*0x06*/,
      0x00 /*0x07*/, 0xFF /*0x08*/, 0xFF /*0x09*/,
      0x20 /*0x0A*/, 0x00 /*0x0B*/, 0x6D /*0x0C*/,
      0x70 /*0x0D*/, 0x69 /*0x0E*/, 0xD0 /*0x0F*/,
      0x00 /*0x10*/, 0x00 /*0x11*/, 0x40 /*0x12*/,
      0x15 /*0x13*/, 0x80 /*0x14*/, 0x2A /*0x15*/,
      0xC0 /*0x16*/, 0x40 /*0x17*/, 0xC0 /*0x18*/,
      0x40 /*0x19*/, 0xFF /*0x1A*/, 0x01 /*0x1B*/,
      0x88 /*0x1C*/, 0x40 /*0x1D*/, 0x4C /*0x1E*/,
      0x50 /*0x1F*/, 0x00 /*0x20*/, 0x0C /*0x21*/,
      0x21 /*0x22*/, 0xF0 /*0x23*/, 0x40 /*0x24*/,
      0x00 /*0x25*/, 0x0A /*0x26*/, 0xF0 /*0x27*/,
      0x00 /*0x28*/, 0x00 /*0x29*/, 0x4E /*0x2A*/,
      0xF0 /*0x2B*/, 0x00 /*0x2C*/, 0x00 /*0x2D*/,
      0x4E /*0x2E*/, 0x80 /*R_CCAL*/, 0x80 /*R_CCAL2*/,
      0x80 /*R_CCAL3*/, 0x29 /* */, 0x35 /* */,
      0x63 /*0x34*/, 0x29 /*0x35*/, 0x00 /*0x36*/,
      0x00 /*0x37*/, 0x00 /*0x38*/, 0x00 /*0x39*/,
      0x00 /*0x3A*/, 0x00 /*0x3B*/, 0xFF /*0x3C*/,
      0x0F /*0x3D*/, 0x00 /*0x3E*/, 0x00 /*0x3F*/,
      0x01 /*0x40*/, 0x00 /*0x41*/, 0x00 /*R_CSTAT*/,
      0x03 /*R_SPD*/, 0x01 /*0x44*/, 0x00 /*0x45*/,
      0x39 /*R_CTL*/, 0xC0 /*0x47*/, 0x40 /*0x48*/,
      0x96 /*0x49*/, 0xD8 /*0x4A*/ };
  dprintf(DEBUG_SCAN,"general init...\n");
  return RegWriteArray(this, R_ALL, 74, uchRegs2466);
}

/* **********************************************************************

DoReset()

Resets Scanner after CANCEL in current scan job.

********************************************************************** */

__SM3600EXPORT__
TState DoReset(TInstance *this)
{
  RegWrite(this,0x43, 1, 0x03);    /* #1533[038.1] */
  RegWrite(this,0x43, 1, 0x03);    /* #1534[038.1] */
  RegRead(this,R_POS, 2); /*=0x1375*/    /* #1535[038.6] */
  RegWrite(this,R_CTL, 1, 0x39);    /* #1536[038.6] */
  {
    unsigned char uchRegs1537[]={
      /*R_SPOS*/ 0x00, /*R_SPOSH*/ 0x00, /*0x03*/ 0x00,
      /*R_SWID*/ 0x00, /*R_SWIDH*/ 0x00, /*R_STPS*/ 0x00,
      /*R_STPSH*/ 0x00, /*0x08*/ 0x00, /*0x09*/ 0x00,
      /*R_LEN*/ 0x00, /*R_LENH*/ 0x00, /*0x0C*/ 0x6D,
      /*0x0D*/ 0x70, /*0x0E*/ 0x69, /*0x0F*/ 0xD0,
      /*0x10*/ 0x00, /*0x11*/ 0x00, /*0x12*/ 0x40,
      /*0x13*/ 0x15, /*0x14*/ 0x80, /*0x15*/ 0x2A,
      /*0x16*/ 0xC0, /*0x17*/ 0x40, /*0x18*/ 0xC0,
      /*0x19*/ 0x40, /*0x1A*/ 0xFF, /*0x1B*/ 0x01,
      /*0x1C*/ 0x88, /*0x1D*/ 0x40, /*0x1E*/ 0x4C,
      /*0x1F*/ 0x50, /*0x20*/ 0x00, /*0x21*/ 0x0C,
      /*0x22*/ 0x21, /*0x23*/ 0xF0, /*0x24*/ 0x40,
      /*0x25*/ 0x00, /*0x26*/ 0x0A, /*0x27*/ 0xF0,
      /*0x28*/ 0x00, /*0x29*/ 0x00, /*0x2A*/ 0x4E,
      /*0x2B*/ 0xF0, /*0x2C*/ 0x00, /*0x2D*/ 0x00,
      /*0x2E*/ 0x4E, /*R_CCAL*/ 0x80, /*R_CCAL2*/ 0x80,
      /*R_CCAL3*/ 0x80, /*0x32*/ 0x4D, /*0x33*/ 0x35,
      /*0x34*/ 0x83, /*0x35*/ 0x29, /*0x36*/ 0x00,
      /*0x37*/ 0x00, /*0x38*/ 0x00, /*0x39*/ 0x00,
      /*0x3A*/ 0x00, /*0x3B*/ 0x00, /*0x3C*/ 0xFF,
      /*0x3D*/ 0x0F, /*0x3E*/ 0x00, /*0x3F*/ 0x00,
      /*0x40*/ 0x01, /*0x41*/ 0x80, /*R_CSTAT*/ 0x00,
      /*0x43*/ 0x03, /*R_LMP*/ 0x01, /*0x45*/ 0x00,
      /*R_CTL*/ 0x39, /*0x47*/ 0xC0, /*0x48*/ 0x40,
      /*0x49*/ 0x9E, /*0x4A*/ 0x8C };
    RegWriteArray(this,R_ALL, 74, uchRegs1537);
  }    /* #1537[038.6] */
  INST_ASSERT();
  RegWrite(this,R_CTL, 1, 0x39);    /* #1538[038.6] */
  RegWrite(this,R_CTL, 1, 0x79);    /* #1539[038.7] */
  RegWrite(this,R_CTL, 1, 0xF9);    /* #1540[038.7] */
  WaitWhileScanning(this,2);
  INST_ASSERT();
  RegWrite(this,R_CTL, 1, 0x39);    /* #1542[038.7] */
  RegWrite(this,0x43, 1, 0x07);    /* #1543[038.7] */
  WaitWhileBusy(this,2);
  INST_ASSERT();
  RegWrite(this,0x32, 2, 0x354D);    /* #1545[038.7] */
  RegWrite(this,0x34, 1, 0xC3);    /* #1546[038.7] */
  RegWrite(this,0x49, 1, 0x9E);    /* #1547[038.7] */
  INST_ASSERT();
  return SANE_STATUS_GOOD;
}

/* **********************************************************************

WaitWhileBusy()

NOTE: Semantics changed: 0 on success, -1 else

********************************************************************** */

__SM3600EXPORT__
TState WaitWhileBusy(TInstance *this, int cSecs)
{
  int cTimeOut=cSecs*10;
  int value;
  INST_ASSERT();
  while (cTimeOut--)
    {
      if ((value=(int)RegRead(this,R_CTL,1)) & 0x80)
	usleep(50);
      else
	return 0;
    }
  return SetError(this,SANE_STATUS_IO_ERROR,"Timeout while waiting for CTL");
}

/* **********************************************************************

WaitWhileScanning()

NOTE: Semantics changed: 0 on success, -1 else

********************************************************************** */

__SM3600EXPORT__
TState WaitWhileScanning(TInstance *this, int cSecs)
{
  int cTimeOut=cSecs*10;
  int value;
  INST_ASSERT();
  while (cTimeOut--)
    {
      if ((value=(int)RegRead(this,R_CSTAT, 1)) & 0x80)
	return 0;
      else
	usleep(50);
    }
  return SetError(this,SANE_STATUS_IO_ERROR,"Timeout while waiting for CSTAT");
}

#ifdef INSANE_VERSION

/* **********************************************************************

DoLampSwitch(nRegister)

0x01 should switch the lamp ON
0x02 should swuitch it OFF

********************************************************************** */

__SM3600EXPORT__
TState DoLampSwitch(TInstance *this, int nPattern)
{
  return RegWrite(this, R_LMP, 1, nPattern);
}

#endif

/* **********************************************************************

UploadGammaTable()

********************************************************************** */

__SM3600EXPORT__
TState UploadGammaTable(TInstance *this, int iByteAddress, SANE_Int *pnGamma)
{
  unsigned char *puchGamma;
  TState         rc;
  int            i;
  rc=SANE_STATUS_GOOD;
  INST_ASSERT();
  puchGamma=malloc(0x2000);
  if (!puchGamma) return SetError(this,SANE_STATUS_NO_MEM,"gamma buffer");
  DBG(DEBUG_INFO,"uploading gamma to %d\n",iByteAddress);
  for (i=0; i<0x1000; i++)
    {
      int nVal=pnGamma[i];
      /* nVal=i; */
      puchGamma[2*i+1]=nVal>>8;
      puchGamma[2*i+0]=nVal&0xFF;
    }
  for (i=0; rc==SANE_STATUS_GOOD && i<0x2000; i+=0x1000)
    rc=MemWriteArray(this,(i+iByteAddress)>>1,0x1000,puchGamma+i);
  free(puchGamma);
  return rc;
}

/* **********************************************************************

UploadGainCorrection()

********************************************************************** */

__SM3600EXPORT__
TState UploadGainCorrection(TInstance *this, int iTableOffset)
{
  {
    struct TGain {
      unsigned char uchLow;
      unsigned char uchHigh;
    } aGain[0x2000]; /* 16 KB */
    int i,iOff;
    unsigned short uwGain;

    /*
      Oopsi: correction data starts at the left of the scanning window!
    */
    iOff=this->param.x/2+this->calibration.xMargin;
    memset(aGain,0xFF,sizeof(aGain));
    RegWrite(this,0x3D,1,0x0F | 0x80); /* 10XXXXXX : one offset table */
    RegWrite(this,0x3F,1, iTableOffset==0x6000 ? 0x18 : 0x08); /* 16KB gain at 0x06000 or 0x02000 */
    for (i=iOff; i<MAX_PIXEL_PER_SCANLINE; i++)
      {
	uwGain=this->calibration.achStripeY[i]<<4;
	aGain[i-iOff].uchLow =(unsigned char)(uwGain&0xFF);
	aGain[i-iOff].uchHigh=(unsigned char)(uwGain>>8);
      }
    for (i=0; i<0x4000; i+=0x1000)
      MemWriteArray(this,(iTableOffset+i)>>1,0x1000,((unsigned char*)aGain)+i);
  }
  return SANE_STATUS_GOOD;
}
