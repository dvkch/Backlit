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

color scan routine

(C) Marian Eichholz 2001

====================================================================== */

#include "sm3600-scantool.h"

#define ORDER_RGB             "012"
#define ORDER_BRG             "120"

/* **********************************************************************

ReadNextColorLine()

********************************************************************** */

static TState ReadNextColorLine(PTInstance this)
{
  int           iWrite,i;
  int           iRead; /* read position in raw line */
  int           nInterpolator;
  int           iOffsetR,iOffsetG,iOffsetB;
  short        *pchLineSwap;
  TBool         bVisible;

  bVisible=false;
  do {
      iWrite=0;
      while (iWrite<3*this->state.cxMax) /* max 1 time in reality */
	{
	  while (iWrite<3*this->state.cxMax &&
		 this->state.iBulkReadPos<this->state.cchBulk)
	    this->state.ppchLines[0][iWrite++] =
	      this->state.pchBuf[this->state.iBulkReadPos++];
	  if (iWrite<3*this->state.cxMax) /* we need an additional chunk */
	    {
	      if (this->state.bLastBulk)
		return SANE_STATUS_EOF;
	      this->state.cchBulk=BulkReadBuffer(this,this->state.pchBuf,
						 USB_CHUNK_SIZE);
	      dprintf(DEBUG_SCAN,"bulk read: %d byte(s), line #%d\n",
		      this->state.cchBulk, this->state.iLine);
	      if (this->bWriteRaw)
		fwrite(this->state.pchBuf,1,this->state.cchBulk,this->fhScan);
	      INST_ASSERT();
	      if (this->state.cchBulk!=USB_CHUNK_SIZE)
		this->state.bLastBulk=true;
	      this->state.iBulkReadPos=0;
	    }
	} /* while raw line buffer acquiring */
      this->state.iLine++;
      if (this->state.iLine>2*this->state.ySensorSkew)
	{
	  bVisible=true;
	  iOffsetR=(this->state.szOrder[0]-'0')*this->state.cxMax;
	  iOffsetG=(this->state.szOrder[1]-'0')*this->state.cxMax;
	  iOffsetB=(this->state.szOrder[2]-'0')*this->state.cxMax;
	  for (nInterpolator=100, iWrite=0, iRead=0;
	       iRead<3*this->state.cxMax && iWrite<this->state.cchLineOut;
	       iRead++)
	    {
	      nInterpolator+=this->state.nFixAspect;
	      if (nInterpolator<100) continue; /* res. reduction */
	      nInterpolator-=100;
	      /* dprintf(DEBUG_SCAN," i=%d",iTo); */
	      /* the first scan lines only fill the line backlog buffer */
	      {
		/* dprintf(DEBUG_SCAN,"assembling line %d\n",++this->state.iLine); */
		this->state.pchLineOut[iWrite++]=
		  this->state.ppchLines[2*this->state.ySensorSkew][iRead+iOffsetR];
		this->state.pchLineOut[iWrite++]=
		  this->state.ppchLines[1*this->state.ySensorSkew][iRead+iOffsetG];
		this->state.pchLineOut[iWrite++]=
		  this->state.ppchLines[0*this->state.ySensorSkew][iRead+iOffsetB];
	      }
	    }
	} /* if visible line */
      /* cycle backlog buffers */
      pchLineSwap=this->state.ppchLines[this->state.cBacklog-1];
      for (i=this->state.cBacklog-2; i>=0; i--)
	this->state.ppchLines[i+1]=this->state.ppchLines[i];
      this->state.ppchLines[0]=pchLineSwap;
  } while (!bVisible);
  return SANE_STATUS_GOOD;
}

/* ======================================================================

StartScanColor()

====================================================================== */

/* Parameter are in resolution units! */
__SM3600EXPORT__
TState StartScanColor(TInstance *this)
{

  /* live could be easy: Simple calculate a window, start the scan,
     get the data and get off. It dimply does not work.  We have to
     deal with the fact, that the pixels are reported with color scan
     line by color scan line, and have to be rearranged to RGB
     triples. They even ar enot always in RGB order, but mostly in BRG
     order. And they have a skew of 2/300 inch , given by the slider
     construction.  Thus, we have to deal with several buffers, some
     interpolation, and related management stuff. */
  int             i;
  if (this->state.bScanning)
    return SetError(this,SANE_STATUS_DEVICE_BUSY,"scan active");
  memset(&(this->state),0,sizeof(this->state));
  this->state.ReadProc  =ReadNextColorLine;
  this->state.ySensorSkew=0;

  GetAreaSize(this);

  switch (this->param.res)
    {
    case 200: this->state.ySensorSkew=1; break;
    case 300: this->state.ySensorSkew=2; break;
    case 600: this->state.ySensorSkew=4; break;
    default: break;
    }
  /* since we need 2*this->state.ySensorSkew additional scan lines for de-skewing of
     the sensor lines, we enlarge the window and shorten the initial movement
     accordingly */
  this->state.cyTotalPath =
    this->param.y/2-(2*this->state.ySensorSkew)*600/this->param.res;
  DoJog(this,this->state.cyTotalPath); INST_ASSERT();
  this->state.cyTotalPath += 
    (this->state.cyPixel+2*this->state.ySensorSkew)
    *600/this->param.res; /* for jogging back */

  /*
    regular scan is asynchronously, that is,
    the scanning is issued, and the driver does bulk reads,
    until there are no data left.
    Each line has a full R, G and B subline, 8bit each sample.
  */
  {
    unsigned char uchRegs[]={
      0xFC /*!!R_SPOS!!*/, 0x00 /*R_SPOSH*/, 0x24 /*!!0x03!!*/,
      0xB0 /*!!R_SWID!!*/, 0xC4 /*!!R_SWIDH!!*/,
      1,0,
      0xFF /*!!0x08!!*/, 0xFF /*!!0x09!!*/,
      0x22 /*!!R_LEN!!*/, 0x07 /*!!R_LENH!!*/, 0x6D /*0x0C*/,
      0x70 /*0x0D*/, 0x69 /*0x0E*/, 0xD0 /*0x0F*/,
      0x00 /*0x10*/, 0x00 /*0x11*/, 0x42 /*!!0x12!!*/,
      0x15 /*0x13*/, 0x84 /*!!0x14!!*/, 0x2A /*0x15*/,
      0xC5 /*!!0x16!!*/, 0x40 /*0x17*/, 0xC5 /*!!0x18!!*/,
      0x40 /*0x19*/, 0xFF /*0x1A*/, 0x01 /*0x1B*/,
      0x88 /*0x1C*/, 0x40 /*0x1D*/, 0x4C /*0x1E*/,
      0x50 /*0x1F*/, 0x00 /*0x20*/, 0x0C /*0x21*/,
      0x21 /*0x22*/, 0xF0 /*0x23*/, 0x40 /*0x24*/,
      0x00 /*0x25*/, 0x0A /*0x26*/, 0xF0 /*0x27*/,
      0x00 /*0x28*/, 0x00 /*0x29*/, 0x4E /*0x2A*/,
      0xF0 /*0x2B*/, 0x00 /*0x2C*/, 0x00 /*0x2D*/,
      0x4E /*0x2E*/, 0x80 /*R_CCAL*/, 0x80 /*R_CCAL2*/,
      0x80 /*R_CCAL3*/, 0x0B /*0x32*/, 0x2D /*0x33*/,
      0x43 /*!!0x34!!*/, 0x29 /*0x35*/, 0x00 /*0x36*/,
      0x00 /*0x37*/, 0x00 /*0x38*/, 0x00 /*0x39*/,
      0x00 /*0x3A*/, 0x00 /*0x3B*/, 0xFF /*0x3C*/,
      0x0F /*0x3D*/, 0x00 /*0x3E*/, 0x00 /*0x3F*/,
      0x01 /*0x40*/, 0x00 /*0x41*/, 0x80 /*R_CSTAT*/,
      0x03 /*0x43*/, 0x01 /*R_LMP*/, 0x00 /*0x45*/,
      0x39 /*R_CTL*/, 0xC5 /*!!0x47!!*/, 0x40 /*0x48*/,
      0x9E /*0x49*/, 0x8C /*0x4A*/ };
    RegWriteArray(this,R_ALL, NUM_SCANREGS, uchRegs);
    RegWrite(this,R_SPOS, 2, this->param.x/2 + this->calibration.xMargin);
    RegWrite(this,R_SLEN, 2,
	     this->state.cyWindow+
	     (2*this->state.ySensorSkew)*600/this->param.res);
    this->state.szOrder=ORDER_BRG; 
    RegWrite(this,R_CCAL, 3, this->calibration.rgbBias); INST_ASSERT(); /* 0xBBGGRR */
    switch (this->param.res)
      {
      case 75:
	RegWrite(this,R_XRES,1, 0x20); /* ups, can  do only 100 dpi horizontal */
	RegWrite(this,R_SWID, 2, 0xC000 | this->state.cxWindow);
	RegWrite(this,0x34, 1, 0x83); /* halfs the vertical resolution */
	RegWrite(this,0x47,1,0xC0); /* reduces the speed a bit */
	break;
      case 100:
	RegWrite(this,R_XRES,1, 0x20);
	RegWrite(this,R_SWID, 2, 0xC000 | this->state.cxWindow);
	RegWrite(this,0x34, 1, 0x63); /* halfs the vertical resolution */
	RegWrite(this,0x47,1,0xC0); /* reduces the speed a bit */
	/* I have no idea, what these differences are good for. The seem to produce
	   a slight blue presence.
	   RegWrite(this,0x16, 1, 0xC0);  RegWrite(this,0x18, 1, 0xC0);
	   RegWrite(this,0x12, 1, 0x40);  RegWrite(this,0x10, 2, 0x0728);
	   RegWrite(this,0x14, 1, 0x80); */
	break;
      case 200:
	RegWrite(this,R_XRES,1, 0x24);
	RegWrite(this,R_SWID, 2, 0xC000 | this->state.cxWindow);
	break;
      case 300:
	RegWrite(this,0x08,2, 0x6A6A);
	RegWrite(this,R_XRES,1, 0x2A);
	RegWrite(this,R_SWID, 2, 0x4000 | this->state.cxWindow);
	RegWrite(this,0x34, 1, 0x03); /* halfs the vertical resolution */
	RegWrite(this,0x47,1,0xC0); /* reduces the speed a bit */
	this->state.szOrder=ORDER_RGB;
	break;
      case 600:
	RegWrite(this,R_XRES,1, 0x3F);
	RegWrite(this,R_SWID, 2, 0xC000 | this->state.cxWindow);
	RegWrite(this,0x34, 1, 0x03); /* halfs the vertical resolution */
	RegWrite(this,0x47,1,0xC2); /* reduces the speed a bit */
	break;
      case 1200:
	/* not supported, since the driver supports only 600 dpi in color */
	break;
      }
  }

  /* setup gamma tables */
  RegWrite(this,0x41,1,0x03); /* gamma, RGB */
  RegWrite(this,0x40,1,0x28); /* offset FIFO 8*3 (GAMMA)+16 KB(gain) spared */
  /*
    hey, surprise: Although the color lines are sent in a strange order,
    the gamma tables are mapped constantly to the sensors (i.e. RGB)
  */
  UploadGammaTable(this,0x0000,this->agammaR);
  UploadGammaTable(this,0x2000,this->agammaG);
  UploadGammaTable(this,0x4000,this->agammaB);
  INST_ASSERT();

  UploadGainCorrection(this,0x6000);
  INST_ASSERT();

  /* enough for 1/100 inch sensor distance */
  this->state.cBacklog=1+2*this->state.ySensorSkew;

  /* allocate raw line buffers */
  this->state.ppchLines=calloc(this->state.cBacklog,sizeof(short*));
  this->state.pchBuf=malloc(0x8000);
  if (!this->state.ppchLines || !this->state.pchBuf)
    return FreeState(this,SetError(this,
				   SANE_STATUS_NO_MEM,"no buffers available"));

  for (i=0; i<this->state.cBacklog; i++)
  {
    this->state.ppchLines[i]=calloc(1,3*this->state.cxMax*sizeof(short)); /* must be less than 0x8000 */
    if (!this->state.ppchLines[i])
      return FreeState(this,
		       SetError(this,SANE_STATUS_NO_MEM,
				"no line buffer available"));
  }

  /* calculate and prepare intermediate line transfer buffer */
  
  this->state.cchLineOut=3*this->state.cxPixel;
  this->state.pchLineOut = malloc(this->state.cchLineOut);
  if (!this->state.pchLineOut)
    return FreeState(this,SetError(this,
				   SANE_STATUS_NO_MEM,
				   "no buffers available"));

  RegWrite(this,R_CTL, 1, 0x39);    /* #1532[005.0] */
  RegWrite(this,R_CTL, 1, 0x79);    /* #1533[005.0] */
  RegWrite(this,R_CTL, 1, 0xF9);    /* #1534[005.0] */
  INST_ASSERT();

  this->state.bScanning = true;
  return SANE_STATUS_GOOD;
}

