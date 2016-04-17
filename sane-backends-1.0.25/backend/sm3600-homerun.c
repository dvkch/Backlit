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

slider movement

(C) Marian Eichholz 2001

====================================================================== */

#include "sm3600-scantool.h"

#include <math.h>

/* tuning constants for DoOriginate */
#define CCH_BONSAI              60
#define BLACK_HOLE_GRAY         30
#define BLACK_BED_LEVEL         10

/* changed by user request from 100, there are probably darker stripes */
#define CHASSIS_GRAY_LEVEL      75

typedef enum { ltHome, ltUnknown, ltBed, ltError } TLineType;

#define INST_ASSERT2() { if (this->nErrorState) return ltError; }

static unsigned char auchRegsSingleLine[]={
  0x00 /*0x01*/, 0x00 /*0x02*/, 0x3F /*0x03*/,
  0xB4 /*!!0x04!!*/, 0x14 /*!!0x05!!*/, 0,0, 
  0x00 /*0x08*/, 0x3F /*!!0x09!!*/,
  1,0,
  0x6D /*0x0C*/,
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
  0x4E /*0x2E*/, 0x88 /*R_CCAL*/, 0x88 /*R_CCAL2*/,
  0x84 /*R_CCAL3*/, 0xEA /*R_LEN*/, 0x24 /*R_LENH*/,
  0x63 /*0x34*/, 0x29 /*0x35*/, 0x00 /*0x36*/,
  0x00 /*0x37*/, 0x00 /*0x38*/, 0x00 /*0x39*/,
  0x00 /*0x3A*/, 0x00 /*0x3B*/, 0xFF /*0x3C*/,
  0x0F /*0x3D*/, 0x00 /*0x3E*/, 0x00 /*0x3F*/,
  0x01 /*0x40*/, 0x00 /*0x41*/, 0x00 /*R_CSTAT*/,
  0x03 /*R_SPD*/, 0x01 /*0x44*/, 0x00 /*0x45*/,
  0x59 /*!!R_CTL!!*/, 0xC0 /*0x47*/, 0x40 /*0x48*/,
  0x96 /*!!0x49!!*/, 0xD8 /*0x4A*/ };

/* ======================================================================

GetLineType()

Reads a scan line at the actual position and classifies it as
"on the flatbed area" or "at home position" or "elsewhere".
This can be used to calculate the proper stepping width

====================================================================== */

static TLineType GetLineType(TInstance *this)
{
  unsigned char  achLine[CCH_BONSAI+1];
  unsigned char *puchBuffer;
  int            cchBulk,i,iHole;
  int            axHoles[3];
  long           lSum;
  TBool          bHolesOk;
  int            lMedian;
  bHolesOk=false;
  RegWriteArray(this,R_ALL, 74, auchRegsSingleLine);
  INST_ASSERT2();
  /*     dprintf(DEBUG_SCAN,"originate-%d...",iStripe); */
  RegWrite(this,R_CTL, 1, 0x59);    /* #2496[062.5] */
  RegWrite(this,R_CTL, 1, 0xD9);    /* #2497[062.5] */
  i=WaitWhileScanning(this,5); if (i) return i;
    
  cchBulk=MAX_PIXEL_PER_SCANLINE;
  /*
  cchBulk=RegRead(this,R_STAT, 2);
  if (cchBulk!=MAX_PIXEL_PER_SCANLINE)
    return SetError(this,SANE_STATUS_INVAL,
		    "illegal scan line width reported (%d)",cchBulk);
  */
  puchBuffer=(unsigned char*)calloc(1,cchBulk);
  CHECK_POINTER(puchBuffer);
  if (BulkReadBuffer(this,puchBuffer, cchBulk)!=cchBulk)
    {
      free(puchBuffer);
      return SetError(this,SANE_STATUS_IO_ERROR,"truncated bulk");
    }
  lSum=0;
  for (i=0; i<cchBulk; i++)
    lSum+=puchBuffer[i]; /* gives total white level */
  for (i=0; i<CCH_BONSAI; i++)
    {
      int iBulk=i*(cchBulk)/CCH_BONSAI;
      achLine[i]=puchBuffer[iBulk+40]; /* simple, basta */
    }
  /* the bonsai line is supported only for curiosity */
  for (i=0; i<CCH_BONSAI; i++)
    achLine[i]=achLine[i]/26+'0'; /* '0'...'9' */
  achLine[CCH_BONSAI]='\0';

  i=200;
  iHole=0;
  dprintf(DEBUG_ORIG,"");
  while (i<MAX_PIXEL_PER_SCANLINE && iHole<3)
    {
      int c;
      while (i<MAX_PIXEL_PER_SCANLINE && puchBuffer[i]>BLACK_HOLE_GRAY) i++; /* not very black */
      c=0;
      dprintf(DEBUG_ORIG,"~ i=%d",i);
      while (i<MAX_PIXEL_PER_SCANLINE && puchBuffer[i]<=BLACK_HOLE_GRAY) { i++; c++; }
      dprintf(DEBUG_ORIG,"~ c=%d",c);
      if (c>90) /* 90% of min hole diameter */
	{
	  axHoles[iHole]=i-c/2; /* store the middle of the hole */
	  dprintf(DEBUG_ORIG,"~ #%d=%d",iHole,axHoles[iHole]);
	  iHole++;
	  i+=10; /* some hysteresis */
	}
    }
  if (iHole==3)
    {
      bHolesOk=true;
      for (i=0; i<2; i++)
	{
	  int xDistance=axHoles[i+1]-axHoles[i];
	  if (xDistance<1050 || xDistance>1400)
	    bHolesOk=false;
	}
      if (axHoles[0]<350 || axHoles[0]>900) /* >2 cm tolerance */
	bHolesOk=false;
    }
  else
    bHolesOk=false;
  lMedian=lSum/cchBulk;
  /* this is *definitly* dirty style. We should pass the information
     by other means... */
  if (bHolesOk)
    {
      /* black reference */
      this->calibration.nHoleGray=puchBuffer[axHoles[0]];
      switch (this->model)
	{
	case sm3600:
	  /* bed corner */
	  this->calibration.xMargin=axHoles[0]-480;
	  this->calibration.yMargin=413;
	  break;
	case sm3700:
	case sm3750: /* basically unknown sub-brand */
	default:
	  this->calibration.xMargin=axHoles[0]-462;
	  this->calibration.yMargin=330;
	  break;
	} /* switch */
    }
  dprintf(DEBUG_ORIG,"~ %s - %d\n",
	  achLine,
	  lMedian);
  free(puchBuffer);
  i=WaitWhileBusy(this,2); if (i) return i;
  if (bHolesOk && lMedian>CHASSIS_GRAY_LEVEL)
    return ltHome;
  if (lMedian<=BLACK_BED_LEVEL)
    return ltBed;
  return ltUnknown;
}

#ifdef INSANE_VERSION

/* **********************************************************************

FakeCalibration()

If DoOriginate() and this Calibration code is skipped,
we should at least provide for some fake measurements.
Thus a test scan of the scanner's inside is possible.

********************************************************************** */

__SM3600EXPORT__
TState FakeCalibration(TInstance *this)
{
  if (this->calibration.bCalibrated)
    return SANE_STATUS_GOOD;
  this->calibration.bCalibrated=true;
  if (!this->calibration.achStripeY)
    {
      this->calibration.achStripeY=calloc(1,MAX_PIXEL_PER_SCANLINE);
      if (!this->calibration.achStripeY)
	return SetError(this,SANE_STATUS_NO_MEM,"no memory for calib Y");
    }
  memset(this->calibration.achStripeY,0xC0,MAX_PIXEL_PER_SCANLINE);
  /* scan *every* nonsense */
  this->calibration.xMargin=this->calibration.yMargin=0;
  return SANE_STATUS_GOOD;
}

#endif

/* **********************************************************************

DoCalibration() and friends

********************************************************************** */

#define SM3600_CALIB_USE_MEDIAN
#define SM3600_CALIB_APPLY_HANNING_WINDOW

#ifdef SM3600_CALIB_USE_MEDIAN
typedef int (*TQSortProc)(const void *, const void *);

static
int CompareProc(const unsigned char *p1, const unsigned char *p2)
{
  return *p1 - *p2;
}
#endif

#define MAX_CALIB_STRIPES 8

__SM3600EXPORT__
TState DoCalibration(TInstance *this)
{
#ifdef SM3600_CALIB_USE_RMS
  long   aulSum[MAX_PIXEL_PER_SCANLINE];
#endif
#ifdef SM3600_CALIB_USE_MEDIAN
  unsigned char aauchY[MAX_CALIB_STRIPES][MAX_PIXEL_PER_SCANLINE];
  unsigned char auchRow[MAX_CALIB_STRIPES];
#endif
#ifdef SM3600_CALIB_APPLY_HANNING_WINDOW
  unsigned char auchHanning[MAX_PIXEL_PER_SCANLINE];
#endif

  int    iLine,i;
  int    yStart,cStripes,cyGap;
  TState rc;
  if (this->calibration.bCalibrated)
    return SANE_STATUS_GOOD;

  switch (this->model)
    {
    case sm3600:
      yStart=200;
      cStripes=MAX_CALIB_STRIPES;
      cyGap=10;
      break;
    case sm3700: /* in fact, the 3600 calibration should do!!! */
    case sm3750:
    default:
      yStart=100;  /* 54 is perimeter */
      cStripes=MAX_CALIB_STRIPES;  
      cyGap=10;
      break;
    } /* switch */

  DoJog(this,yStart);
  /* scan a gray line at 600 DPI */
  if (!this->calibration.achStripeY)
    {
      this->calibration.achStripeY=calloc(1,MAX_PIXEL_PER_SCANLINE);
      if (!this->calibration.achStripeY)
	return SetError(this,SANE_STATUS_NO_MEM,"no memory for calib Y");
    }
#ifdef SM3600_CALIB_USE_RMS
  memset(aulSum,0,sizeof(aulSum));
#endif
  for (iLine=0; iLine<cStripes; iLine++)
    {
      dprintf(DEBUG_CALIB,"calibrating %i...\n",iLine);
      RegWriteArray(this,R_ALL, 74, auchRegsSingleLine);
      INST_ASSERT();
      RegWrite(this,R_CTL, 1, 0x59);    /* #2496[062.5] */
      RegWrite(this,R_CTL, 1, 0xD9);    /* #2497[062.5] */
      rc=WaitWhileScanning(this,5); if (rc) { return rc; }
      if (BulkReadBuffer(this,
#ifdef SM3600_CALIB_USE_RMS
			 this->calibration.achStripeY,
#endif
#ifdef SM3600_CALIB_USE_MEDIAN
			 aauchY[iLine],
#endif
			 MAX_PIXEL_PER_SCANLINE)
	  !=MAX_PIXEL_PER_SCANLINE)
	return SetError(this,SANE_STATUS_IO_ERROR,"truncated bulk");
#ifdef SM3600_CALIB_USE_RMS
      for (i=0; i<MAX_PIXEL_PER_SCANLINE; i++)
	aulSum[i]+=(long)this->calibration.achStripeY[i]*
	  (long)this->calibration.achStripeY[i];
#endif
      DoJog(this,cyGap);
    }
#ifdef SM3600_CALIB_USE_RMS
  for (i=0; i<MAX_PIXEL_PER_SCANLINE; i++)
    this->calibration.achStripeY[i]=(unsigned char)(int)sqrt(aulSum[i]/cStripes);
#endif
#ifdef SM3600_CALIB_USE_MEDIAN
  /* process the collected lines rowwise. Use intermediate buffer for qsort */
  for (i=0; i<MAX_PIXEL_PER_SCANLINE; i++)
    {
      for (iLine=0; iLine<cStripes; iLine++)
	auchRow[iLine]=aauchY[iLine][i];
      qsort(auchRow,cStripes, sizeof(unsigned char), (TQSortProc)CompareProc);
      this->calibration.achStripeY[i]=auchRow[(cStripes-1)/2];
    }
#endif
#ifdef SM3600_CALIB_APPLY_HANNING_WINDOW
  memcpy(auchHanning,this->calibration.achStripeY,sizeof(auchHanning));
  for (i=1; i<MAX_PIXEL_PER_SCANLINE-1; i++)
    this->calibration.achStripeY[i]=(unsigned char)
      ((2*(int)auchHanning[i]+auchHanning[i-1]+auchHanning[i+1])/4);
#endif
  
  DoJog(this,-yStart-cStripes*cyGap);
  INST_ASSERT();
  this->calibration.bCalibrated=true;
  return SANE_STATUS_GOOD;
}

/* **********************************************************************

DoOriginate()

*shall* one time move the slider safely back to its origin.
No idea, hoiw to achieve this, for now...

********************************************************************** */

__SM3600EXPORT__
TState DoOriginate(TInstance *this, TBool bStepOut)
{
  TLineType lt;
  if (this->bVerbose)
    fprintf(stderr,"carriage return...\n");
  DBG(DEBUG_INFO,"DoOriginate()\n");
  INST_ASSERT();
  lt=GetLineType(this);
  /* if we are already at home, fine. If not, first jump a bit forward */
  DBG(DEBUG_JUNK,"lt1=%d\n",(int)lt);
  if (lt!=ltHome && bStepOut) DoJog(this,150);
  while (lt!=ltHome && !this->state.bCanceled)
    {
      lt=GetLineType(this);
      DBG(DEBUG_JUNK,"lt2=%d\n",(int)lt);
      INST_ASSERT();
      switch (lt)
	{
	case ltHome: continue;
	case ltBed:  DoJog(this,-240); break; /* worst case: 1 cm */
	default:     DoJog(this,-15); break; /* 0.X mm */
	}
    }
  DoJog(this,1); INST_ASSERT(); /* Correction for 1 check line */
  DBG(DEBUG_JUNK,"lt3=%d\n",(int)lt);
  if (this->state.bCanceled)
    return SANE_STATUS_CANCELLED;
  return DoCalibration(this);
}

/* **********************************************************************

DoJog(nDistance)

The distance is given in 600 DPI.

********************************************************************** */

__SM3600EXPORT__
TState DoJog(TInstance *this, int nDistance)
{
  int cSteps;
  int nSpeed,nRest;
  dprintf(DEBUG_SCAN,"jogging %d units...\n",nDistance);
  if (!nDistance) return 0;
  RegWrite(this,0x34, 1, 0x63);
  RegWrite(this,0x49, 1, 0x96);
  WaitWhileBusy(this,2);
  RegWrite(this,0x34, 1, 0x63);
  RegWrite(this,0x49, 1, 0x9E); /* that is a difference! */
  INST_ASSERT();
  cSteps=(nDistance>0) ? nDistance : -nDistance;
  {
    unsigned char uchRegs2587[]={
      0x00 /*0x01*/, 0x00 /*0x02*/, 0x3F /*0x03*/,
      0x40 /*!!0x04!!*/, 0x00 /*!!0x05!!*/,
      0,0, /* steps */
      0x00 /*0x08*/, 0x00 /*!!0x09!!*/,
      0,0, /* y count */
      0x6D /*0x0C*/,
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
      0x4E /*0x2E*/, 0x88 /*R_CCAL*/, 0x88 /*R_CCAL2*/,
      0x84 /*R_CCAL3*/, 0xEA /*R_LEN*/, 0x24 /*R_LENH*/,
      0x63 /*0x34*/, 0x29 /*0x35*/, 0x00 /*0x36*/,
      0x00 /*0x37*/, 0x00 /*0x38*/, 0x00 /*0x39*/,
      0x00 /*0x3A*/, 0x00 /*0x3B*/, 0xFF /*0x3C*/,
      0x0F /*0x3D*/, 0x00 /*0x3E*/, 0x00 /*0x3F*/,
      0x01 /*0x40*/, 0x00 /*0x41*/, 0x80 /*R_CSTAT*/,
      0x03 /*R_SPD*/, 0x01 /*0x44*/, 0x00 /*0x45*/,
      0x79 /*!!R_CTL!!*/, 0xC0 /*0x47*/, 0x40 /*0x48*/,
      0x9E /*!!0x49!!*/, 0xD8 /*0x4A*/ };
    RegWriteArray(this,R_ALL, 74, uchRegs2587);
  }    /* #2587[065.4] */
  INST_ASSERT();
  RegWrite(this,R_STPS,2,cSteps);
  /* do some magic for slider acceleration */
  if (cSteps>600) /* only large movements are accelerated */
    {
      RegWrite(this,0x34, 1, 0xC3);
      RegWrite(this,0x47, 2, 0xA000);    /* initial speed */
    }
  /* start back or forth movement */
  if (nDistance>0)
    {
      RegWrite(this,R_CTL, 1, 0x39);    /* #2588[065.4] */
      RegWrite(this,R_CTL, 1, 0x79);    /* #2589[065.4] */
      RegWrite(this,R_CTL, 1, 0xF9);    /* #2590[065.4] */
    }
  else
    {
      RegWrite(this,R_CTL, 1, 0x59);
      RegWrite(this,R_CTL, 1, 0xD9);
    }
  INST_ASSERT();
  /* accelerate the slider each 100 us */
  if (cSteps>600)
    {
      nRest=cSteps;
      for (nSpeed=0x9800; nRest>600 && nSpeed>=0x4000; nSpeed-=0x800)
	{
	  nRest=RegRead(this,R_POS, 2);
	  usleep(100);
	  /* perhaps 40C0 is the fastest possible value */
	  RegWrite(this,0x47, 2, nSpeed>0x4000 ? nSpeed : 0x40C0);
	}
    }
  INST_ASSERT();
  usleep(100);
  return WaitWhileBusy(this,1000); /* thanks Mattias Ellert */
}

