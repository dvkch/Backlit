/* sane - Scanner Access Now Easy.
   Copyright (C) 2003 Martijn van Oosterhout <kleptog@svana.org>
   Copyright (C) 2003 Thomas Soumarmon <thomas.soumarmon@cogitae.net>
   Copyright (c) 2003 Henning Meier-Geinitz, <henning@meier-geinitz.de>

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

   HP5400/5470 Test util.
   Currently is only able to read back the scanner version string,
   but this basically demonstrates ability to communicate with the scanner.

   Massively expanded. Can do calibration scan, upload gamma and calibration
   tables and stores the results of a scan. - 19/02/2003 Martijn
*/

#include <stdio.h>		/* for printf */
#include <stdlib.h>		/* for exit */
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <netinet/in.h>		/* for htons */
#include <string.h>


#include "hp5400.h"
#include "hp5400_xfer.h"
#include "hp5400_internal.h"
#include "hp5400_debug.h" /* debug functions */


#ifndef min
#define min(A,B) (((A)<(B)) ? (A) : (B))
#endif
#ifndef max
#define max(A,B) (((A)>(B)) ? (A) : (B))
#endif


#ifndef STRING_VERSION_MATCH
#define NO_STRING_VERSION_MATCH 1
#endif

#ifdef __GNUC__
#define PACKED __attribute__ ((packed))
#else
#define PACKED
#endif

/* If this is enabled, a copy of the raw data from the scanner will be saved to
   imagedebug.dat and the attempted conversion to imagedebug.ppm */
/* #define IMAGE_DEBUG */

/* If this is defined you get extra info on the calibration */
/* #define CALIB_DEBUG */


typedef struct versionString {
	char strVersion[128];
} versionString;

const int numVersions = 3;
versionString *MatchVersions;


static TScannerModel Model_HP54xx =
  { "Hewlett-Packard", "HP54xx Flatbed Scanner" };


HP5400_SANE_STATIC
int
InitHp5400_internal() {

	MatchVersions = malloc( sizeof(versionString) * numVersions );
	strcpy( MatchVersions[0].strVersion, "SilitekIBlizd C3 ScannerV0.84");
	strcpy( MatchVersions[1].strVersion, "SilitekIBlizd C3 ScannerV0.86");
	strcpy( MatchVersions[2].strVersion, "SilitekIBlizd C3 ScannerV0.87");

	return 1;
}

HP5400_SANE_STATIC
int
FreeHp5400_internal() {

	free(MatchVersions);
	MatchVersions = NULL;

	return 1;
}


HP5400_SANE_STATIC
int
WriteByte (int iHandle, int cmd, char data)
{
  if (hp5400_command_write (iHandle, cmd, 1, &data) < 0)
    {
      HP5400_DBG (DBG_MSG, "failed to send byte (cmd=%04X)\n", cmd);
      return -1;
    }
  return 0;
}

HP5400_SANE_STATIC
int
SetLamp (THWParams * pHWParams, int fLampOn)
{
  if (fLampOn)
    {
      if (WriteByte (pHWParams->iXferHandle, 0x0000, 0x01) == 0)
	return 0;
    }
  return -1;
}

HP5400_SANE_STATIC
int
WarmupLamp (int iHandle)
{
  int i = 30;			/* Max 30 seconds, 15 is typical for cold start? */
  int couldRead;
  unsigned char dataVerify[0x02];

  /* Keep writing 01 to 0000 until no error... */
  unsigned char data0000[] = {
    0x01
  };
  unsigned char data0300[3];


  hp5400_command_write_noverify (iHandle, 0x0000, data0000, 1);

  do
    {
      hp5400_command_read_noverify (iHandle, 0x0300, 3, data0300);

      hp5400_command_write_noverify (iHandle, 0x0000, data0000, 1);

      couldRead =
	hp5400_command_read_noverify (iHandle, 0xc500, 0x02, dataVerify);
      if ((dataVerify[0] != 0) || (dataVerify[1] != 0))
	sleep (1);
    }
  while ((i-- > 0) && (couldRead >= 0)
	 && ((dataVerify[0] != 0) || (dataVerify[1] != 0)));

  if (i > 0)
    return 0;

  /*

     while( i > 0 )
     {
     if( WriteByte( iHandle, 0x0000, 0x01 ) == 0 )
     return 0;
     sleep(1);
     i--;
     }
   */

  HP5400_DBG (DBG_MSG, "***WARNING*** Warmup lamp failed...\n");
  return -1;
}

#define CALPIXBYBLOCK  42

HP5400_SANE_STATIC
int
SetCalibration (int iHandle, int numPixels, unsigned int *low_vals[3],
		unsigned int *high_vals[3], int dpi)
{
  char cmd[8];
  /* unsigned char cmd[8]; */ /* should fix the compilation warning
				 but I don't have a scanner right now 
				 to check that the fix does not break
				 calibration */
  
  int i, j, k;
  struct CalPixel
  {
    char highr[2], highg[2], highb[2];
    char lowr[2], lowg[2], lowb[2];
  };
  struct CalBlock
  {
    struct CalPixel pixels[CALPIXBYBLOCK];
    char pad[8];
  }
  PACKED;

  struct CalBlock *calinfo;

  /**
   we did scan test at 300 dpi, so we don't have all the needed pixels.
   To fill the gap, we loop
   */
  int numLoop = max (1, dpi / 300);
  int calBlockSize = CALPIXBYBLOCK * (6 * sizeof (short)) + 8 * sizeof (char);	/* = sizeof(calBlock) */
  int numCalBlock =
    ((numPixels / CALPIXBYBLOCK) +
     ((numPixels % CALPIXBYBLOCK != 0) ? 1 : 0));
  int calSize = numLoop * calBlockSize * numCalBlock;

  calinfo = malloc (calSize);
  memset (calinfo, 0, calSize);

  for (j = 0; j < numLoop * numCalBlock * CALPIXBYBLOCK; j++)
    {
      struct CalPixel *pixel =
	&calinfo[j / CALPIXBYBLOCK].pixels[j % CALPIXBYBLOCK];

      /*
         i = ( j % (int)( 0.80 * numPixels ) ) + (int)(0.10 * numPixels );
       */
      /* better solution : stretch the actual scan size to the calibration size */
      i = j / numLoop;

      /* This is obviously not quite right. The values on
       * the right are approximatly what windows sends */
      k = (high_vals[0][i] > 0x4000) ? 1000000000 / high_vals[0][i] : 0;	/* 0x6700 */
      pixel->highr[0] = k;
      pixel->highr[1] = k >> 8;
      k = (high_vals[1][i] > 0x4000) ? 1000000000 / high_vals[1][i] : 0;	/* 0x5e00 */
      pixel->highg[0] = k;
      pixel->highg[1] = k >> 8;
      k = (high_vals[2][i] > 0x4000) ? 1000000000 / high_vals[2][i] : 0;	/* 0x6000 */
      pixel->highb[0] = k;
      pixel->highb[1] = k >> 8;

      pixel->lowr[0] = low_vals[0][i];		/* 0x0530 */
      pixel->lowr[1] = low_vals[0][i] >> 8;
      pixel->lowg[0] = low_vals[1][i];		/* 0x0530 */
      pixel->lowg[1] = low_vals[1][i] >> 8;
      pixel->lowb[0] = low_vals[2][i];		/* 0x0530 */
      pixel->lowb[1] = low_vals[2][i] >> 8;
    }

  cmd[0] = 0xff & (calSize >> 16);
  cmd[1] = 0xff & (calSize >> 8);
  cmd[2] = 0xff & (calSize >> 0);
  cmd[3] = 0x00;
  cmd[4] = 0x54;
  cmd[5] = 0x02;
  cmd[6] = 0x80;
  cmd[7] = 0x00;

  hp5400_bulk_command_write (iHandle, 0xE603, cmd, 8, calSize, calSize,
			     (void *) calinfo);

  free (calinfo);
  return 0;
}

/* Write a gamma table */
HP5400_SANE_STATIC
void
WriteGammaCalibTable (int iHandle, const int *pabGammaR, const int *pabGammaG,
		      const int *pabGammaB)
{
  char cmd[3];
  char *buffer;
  int i, j;

  /* Setup dummy gamma correction table */
  buffer = malloc (2 * 65536);

  cmd[0] = 2;
  cmd[1] = 0;
  cmd[2] = 0;

  for (i = 0; i < 3; i++)
    {
      const int *ptr = (i == 0) ? pabGammaR :
	(i == 1) ? pabGammaG : pabGammaB;

      for (j = 0; j < 65536; j++) {
	buffer[2 * j] = ptr[j];
	buffer[2 * j + 1] = ptr[j] >> 8;
      }

      hp5400_bulk_command_write (iHandle, 0x2A01 + i, cmd, 3, 2 * 65536,
				 65536, (void *) buffer);
    }
  free (buffer);

  return;
}

#ifdef STANDALONE
HP5400_SANE_STATIC
void
SetDefaultGamma (int iHandle)
{
  int *buffer = malloc (sizeof (int) * 65536);
  int i;

  for (i = 0; i < 65336; i++)
    buffer[i] = i;

  WriteGammaCalibTable (iHandle, buffer, buffer, buffer);
}
#endif

#define BUFFER_SIZE (6*65536)

#ifdef IMAGE_DEBUG
FILE *temp;
#endif

/* Bytes per line is the number of pixels. The actual bytes is one more */
HP5400_SANE_STATIC
void
CircBufferInit (int iHandle, TDataPipe * p, int iBytesPerLine,
		int bpp, int iMisAlignment, int blksize, int iTransferSize)
{
  iHandle = iHandle; /* to avoid compilation warning */
  p->buffersize = max (BUFFER_SIZE, 3 * blksize);

  if (p->buffer)
    {
      free (p->buffer);
    }

  /* Allocate a large enough buffer for transfer */
  p->buffer = malloc (p->buffersize);

  p->pixels = (iBytesPerLine / 3) / bpp;

  /* These three must always be positive */
  p->roff = 0;
  p->goff = p->pixels * bpp + 1;
  p->boff = 2 * p->pixels * bpp + 2;;

  p->linelength = iBytesPerLine + 3;	/* NUL at end of each row */
  p->bpp = bpp;
  p->bufstart = p->bufend = 0;

  if (iMisAlignment > 0)
    {
      p->roff += 0;
      p->goff += p->linelength * iMisAlignment;
      p->boff += p->linelength * iMisAlignment * 2;
    }

  if (iMisAlignment < 0)
    {
      p->roff -= p->linelength * iMisAlignment * 2;
      p->goff -= p->linelength * iMisAlignment;
      p->boff -= 0;
    }

  p->blksize = blksize;
  p->transfersize = iTransferSize;

#ifdef IMAGE_DEBUG
  temp = fopen ("imagedebug.dat", "w+b");
#endif

  HP5400_DBG (DBG_MSG,
       "Begin: line=%d (%X), pixels=%d (%X), r=%d (%X), g=%d (%X), b=%d (%X), bpp=%d, step=%d\n",
       p->linelength, p->linelength, p->pixels, p->pixels, p->roff, p->roff,
       p->goff, p->goff, p->boff, p->boff, bpp, iMisAlignment);
}


HP5400_SANE_STATIC
int
CircBufferGetLine (int iHandle, TDataPipe * p, void *pabLine)
{
  int i;
  int maxoff = 0;
  char* buftmp = (char*) (p->buffer);
  
/*  HP5400_DBG(DBG_MSG, "CircBufferGetLine:\n");   */

  if (p->roff > maxoff)
    maxoff = p->roff;
  if (p->goff > maxoff)
    maxoff = p->goff;
  if (p->boff > maxoff)
    maxoff = p->boff;

  maxoff += p->pixels * p->bpp;

  if (maxoff < p->linelength)
    maxoff = p->linelength;


  /* resize buffer if needed */
  if (p->bufstart + maxoff >= p->buffersize + p->blksize)
    {
      /* store actual buffer pointer */
      void *tmpBuf = p->buffer;
      /* calculate new size for buffer (oversize a bit) */
      int newsize = p->bufstart + maxoff + 2 * p->blksize;

      p->buffer = malloc (newsize);
      memcpy (p->buffer, tmpBuf, p->buffersize);
      p->buffersize = newsize;

      /* free old buffer */
      free (tmpBuf);
      buftmp = (char*)(p->buffer);
    }

  while (p->bufstart + maxoff >= p->bufend)	/* Not enough data in buffer */
    {
      int res;
      unsigned char cmd[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
      cmd[4] = p->blksize;
      cmd[5] = p->blksize >> 8;

      assert ((p->bufend + p->blksize) <= p->buffersize);

      HP5400_DBG (DBG_MSG, "Reading block, %d bytes remain\n", p->transfersize);
      p->transfersize -= p->blksize;

      res =
	hp5400_bulk_read_block (iHandle, CMD_INITBULK3, cmd, sizeof (cmd),
    				buftmp + p->bufend, p->blksize);
      if (res != p->blksize)
	{
	  HP5400_DBG (DBG_ERR, "*** ERROR: Read returned %d. FATAL.\n", res);
	  return -1;
	}
#ifdef IMAGE_DEBUG
      fwrite (
    	buftmp + p->bufend
	,p->blksize
	,1
	,temp
	);
#endif
      p->bufend += p->blksize;
    }

  assert (p->bufstart + maxoff < p->bufend);

  /* Copy a line into the result buffer */
  if (p->bpp == 1)
    {
      char *itPix = (char *) pabLine;
      char *itR = (char *) (buftmp + p->bufstart + p->roff);
      char *itG = (char *) (buftmp + p->bufstart + p->goff);
      char *itB = (char *) (buftmp + p->bufstart + p->boff);
      for (i = 0; i < p->pixels; i++)
	{
	  /* pointer move goes a little bit faster than vector access */
	  /* Although I wouldn't be surprised if the compiler worked that out anyway.
	   * No matter, this is easier to read :) */
	  *(itPix++) = *(itR++);
	  *(itPix++) = *(itG++);
	  *(itPix++) = *(itB++);
	}
    }
  else
    {
      short *itPix = (short *) pabLine;
      short *itR = (short *) (buftmp + p->bufstart + p->roff);
      short *itG = (short *) (buftmp + p->bufstart + p->goff);
      short *itB = (short *) (buftmp + p->bufstart + p->boff);
      for (i = 0; i < p->pixels; i++)
	{
#if 0
	  /* This code, while correct for PBM is not correct for the host and
	   * since we need it correct for calibration, it has to go */
	  ((short *) pabLine)[3 * i + 0] =
	    *((short *) (p->buffer + p->bufstart + p->roff + 2 * i));
	  ((short *) pabLine)[3 * i + 1] =
	    *((short *) (p->buffer + p->bufstart + p->goff + 2 * i));
	  ((short *) pabLine)[3 * i + 2] =
	    *((short *) (p->buffer + p->bufstart + p->boff + 2 * i));
#else
	  /* pointer move goes a little bit faster than vector access */
	  *(itPix++) = htons (*(itR++));
	  *(itPix++) = htons (*(itG++));
	  *(itPix++) = htons (*(itB++));
#endif
	}
    }

  p->bufstart += p->linelength;

  assert (p->bufstart <= p->bufend);

  /* If we've used a whole block at the beginning, move it */
  if (p->bufstart > p->blksize)
    {
      memmove (
	buftmp
	,buftmp + p->bufstart
	,p->bufend - p->bufstart
	);

      p->bufend -= p->bufstart;
      p->bufstart = 0;
    }

  return 0;
}

HP5400_SANE_STATIC
void
CircBufferExit (TDataPipe * p)
{
  free (p->buffer);
  p->buffer = NULL;
#ifdef IMAGE_DEBUG
  fclose (temp);
  temp = NULL;
#endif
  return;
}


#ifdef STANDALONE
/* bpp is BYTES per pixel */
HP5400_SANE_STATIC
void
DecodeImage (FILE * file, int planes, int bpp, int xsize, int ysize,
	     const char *filename)
{
  char *in, *buf;
  char *p[3];
  FILE *output;
  int i, j, k;

  /* xsize is byte width, not pixel width */
  xsize /= planes * bpp;

  HP5400_DBG (DBG_MSG,
       "DecodeImage(planes=%d,bpp=%d,xsize=%d,ysize=%d) => %d (file=%s)\n",
       planes, bpp, xsize, ysize, planes * bpp * xsize * ysize, filename);

  in = malloc (planes * (xsize * bpp + 1));

  for (i = 0; i < planes; i++)
    p[i] = in + i * (xsize * bpp + 1);

  buf = malloc (3 * xsize * bpp);

  output = fopen (filename, "wb");

  fprintf (output, "P%d\n%d %d\n", (planes == 3) ? 6 : 5, xsize, ysize);
  fprintf (output, "%d\n", (bpp == 1) ? 255 : 0xb000);

  for (i = 0; i < ysize; i++)
    {
      fread (in, planes * (xsize * bpp + 1), 1, file);

      for (j = 0; j < xsize; j++)
	{
	  for (k = 0; k < planes; k++)
	    {
	      if (bpp == 1)
		{
		  buf[j * planes + k] = p[k][j];
		}
	      else if (bpp == 2)
		{
		  buf[j * planes * 2 + k * 2 + 0] = p[k][2 * j + 0];
		  buf[j * planes * 2 + k * 2 + 1] = p[k][2 * j + 1];
		}
	    }
	}
      fwrite (buf, planes * xsize * bpp, 1, output);
    }

  fclose (output);

  free (in);
  free (buf);
}

HP5400_SANE_STATIC
int
hp5400_test_scan_response (struct ScanResponse *resp, struct ScanRequest *req)
{
  req = req; /* to avoid compilation warning */
  HP5400_DBG (DBG_MSG, "Scan response:\n");
  HP5400_DBG (DBG_MSG, "  transfersize=%d   htonl-> %d\n", resp->transfersize,
       htonl (resp->transfersize));
  HP5400_DBG (DBG_MSG, "  xsize=%d    htonl-> %d\n", resp->xsize,
       htonl (resp->xsize));
  HP5400_DBG (DBG_MSG, "  ysize=%d    htons-> %d\n", resp->ysize,
       htons (resp->ysize));
  return 1;
}
#endif

/* This is a very specialised scanning function. It does the scan request as
 * usual but instead of producing an image it returns three arrays of ints.
 * These are averages of the R,G,B values for each column.
 *
 * Note the array argument should point to an array of three NULL. These
 * will be overwritten with allocated pointers. */

HP5400_SANE_STATIC
int
DoAverageScan (int iHandle, struct ScanRequest *req, int code,
	       unsigned int **array)
{
  THWParams HWParams;
  struct ScanResponse res;
  unsigned short *buffer;
  int i, j, k, length;

  memset (&HWParams, 0, sizeof (HWParams));

  HWParams.iXferHandle = iHandle;

  if (InitScan2 (SCAN_TYPE_CALIBRATION, req, &HWParams, &res, 0, code) != 0)
    return -1;			/* No colour offseting, we want raw */

  length = htonl (res.xsize) / 6;

  HP5400_DBG (DBG_MSG, "Calibration scan: %d pixels wide\n", length);

  for (j = 0; j < 3; j++)
    {
      array[j] = malloc (sizeof (int) * length);
      memset (array[j], 0, sizeof (int) * length);	/* Clear array */
    }

  buffer = malloc (htonl (res.xsize) + 1);

  /* First we just sum them all */
  for (i = 0; i < htons (res.ysize); i++)
    {
      CircBufferGetLine (iHandle, &HWParams.pipe, buffer);

      for (j = 0; j < length; j++)
	for (k = 0; k < 3; k++)
	  array[k][j] += buffer[3 * j + k];
    }

  free (buffer);
  FinishScan (&HWParams);

  /* Now divide by the height to get the average */
  for (j = 0; j < length; j++)
    for (k = 0; k < 3; k++)
      array[k][j] /= htons (res.ysize);

  return 0;
}

#ifdef STANDALONE
HP5400_SANE_STATIC
int
DoScan (int iHandle, struct ScanRequest *req, const char *filename, int code,
	struct ScanResponse *res)
{
  FILE *file;
  THWParams HWParams;
  struct ScanResponse res_temp;
  void *buffer;
/*  int bpp, planes; */
  int i;

  code = code; /*to avoid compilation warning*/

  if (res == NULL)
    res = &res_temp;

  memset (&HWParams, 0, sizeof (HWParams));

  file = fopen (filename, "w+b");
  if (!file)
    {
      HP5400_DBG (DBG_MSG, "Couldn't open outputfile (%s)\n", strerror (errno));
      return -1;
    }

  HWParams.iXferHandle = iHandle;

  if (InitScan2 (SCAN_TYPE_NORMAL, req, &HWParams, res, 1, 0x40) != 0)
    return -1;

  fprintf (file, "P%d\n%d %d\n", 6, htonl (res->xsize) / 3,
	   htons (res->ysize));
  fprintf (file, "%d\n", 255);

  buffer = malloc (htonl (res->xsize) + 1);
  for (i = 0; i < htons (res->ysize); i++)
    {
      CircBufferGetLine (iHandle, &HWParams.pipe, buffer);

      fwrite (buffer, htonl (res->xsize), 1, file);
    }
  free (buffer);

  FinishScan (&HWParams);

  fclose (file);
  return 0;
}
#endif

HP5400_SANE_STATIC
int
Calibrate (int iHandle, int dpi)
{
  struct ScanRequest req;

  unsigned int *low_array[3];
  unsigned int *high_array[3];
#ifdef CALIB_DEBUG
  char buffer[512];
  int i, j;
#endif

/* The first calibration scan. Finds maximum of each CCD */
  memset(&req, 0, sizeof(req));

  req.x1 = 0x08;
  req.dpix = htons (300);	/* = 300 dpi */
  req.dpiy = htons (300);	/* = 300 dpi */
  req.offx = htons (0);		/* = 0cm */
  req.offy = htons (0);		/* = 0cm */
  req.lenx = htons (2690);	/* = 22.78cm */
  req.leny = htons (50);	/* =  0.42cm */

  req.flags1 = htons (0x0000);
  req.flags2 = htons (0x0010);
  req.flags3 = htons (0x3020);	/* First calibration scan, 48bpp */

  req.gamma[0] = htons (100);
  req.gamma[1] = htons (100);
  req.gamma[2] = htons (100);

  if (DoAverageScan (iHandle, &req, 0x40, high_array) != 0)
    return -1;

#ifdef CALIB_DEBUG
  for (i = 0; i < 3; i++)
    {
      int len;
      buffer[0] = 0;
      sprintf (buffer, "Average %d: \n", i);
      len = strlen (buffer);

      for (j = 0; j < 24; j++)
	{
	  sprintf (buffer + len, "%04X ", high_array[i][j]);
	  len += 5;
	}
      strcat (buffer, " ... \n");
      len += 6;

      for (j = 1000; j < 1024; j++)
	{
	  sprintf (buffer + len, "%04X ", high_array[i][j]);
	  len += 5;
	}
      strcat (buffer, " ... \n");
      len += 6;

      for (j = 2000; j < 2024; j++)
	{
	  sprintf (buffer + len, "%04X ", high_array[i][j]);
	  len += 5;
	}
      strcat (buffer, " ... \n");
      len += 6;

      HP5400_DBG (DBG_MSG, buffer);
    }
#endif

/* The second calibration scan. Finds minimum of each CCD */
  memset(&req, 0, sizeof(req));

  req.x1 = 0x08;
  req.dpix = htons (300);	/* = 300 dpi */
  req.dpiy = htons (300);	/* = 300 dpi */
  req.offx = htons (0);		/* = 0cm */
  req.offy = htons (0);		/* = 0cm */
  req.lenx = htons (2690);	/* = 22.78cm */
  req.leny = htons (16);	/* =  0.14cm */

  req.flags1 = htons (0x0000);
  req.flags2 = htons (0x0010);
  req.flags3 = htons (0x3024);	/* Second calibration scan, 48bpp */

  req.gamma[0] = htons (100);
  req.gamma[1] = htons (100);
  req.gamma[2] = htons (100);

  if (DoAverageScan (iHandle, &req, 0x00, low_array) != 0)
    return -1;

#ifdef CALIB_DEBUG
  for (i = 0; i < 3; i++)
    {
      int len;
      buffer[0] = 0;
      sprintf (buffer, "Average %d: \n", i);
      len = strlen (buffer);

      for (j = 0; j < 24; j++)
	{
	  sprintf (buffer + len, "%04X ", low_array[i][j]);
	  len += 5;
	}
      strcat (buffer, " ... \n");
      len += 6;

      for (j = 1000; j < 1024; j++)
	{
	  sprintf (buffer + len, "%04X ", low_array[i][j]);
	  len += 5;
	}
      strcat (buffer, " ... \n");
      len += 6;

      for (j = 2000; j < 2024; j++)
	{
	  sprintf (buffer + len, "%04X ", low_array[i][j]);
	  len += 5;
	}
      strcat (buffer, " ... \n");
      len += 6;

      HP5400_DBG (DBG_MSG, buffer);
    }
#endif

  SetCalibration (iHandle, 2690, low_array, high_array, dpi);

  return 0;
}

#ifdef STANDALONE
HP5400_SANE_STATIC
int
hp5400_scan (int iHandle, TScanParams * params, THWParams * pHWParams,
	     const char *filename)
{
  struct ScanRequest req;
  struct ScanResponse res;
  int result;

  pHWParams = pHWParams; /*to avoid compilation warning*/

  HP5400_DBG (DBG_MSG, "\n");
  HP5400_DBG (DBG_MSG, "Scanning :\n");
  HP5400_DBG (DBG_MSG, "   dpi(x) : %d\n", params->iDpi);
  HP5400_DBG (DBG_MSG, "   dpi(y) : %d\n", params->iLpi);
  HP5400_DBG (DBG_MSG, "   x0 : %d\n", params->iLeft);
  HP5400_DBG (DBG_MSG, "   y0 : %d\n", params->iTop);
  HP5400_DBG (DBG_MSG, "   width : %d\n", params->iWidth);
  HP5400_DBG (DBG_MSG, "   height : %d\n", params->iHeight);
  HP5400_DBG (DBG_MSG, "\n");

  memset(&req, 0, sizeof(req));

  req.x1 = 0x08;
  req.dpix = htons (params->iDpi);
  req.dpiy = htons (params->iLpi);

  /* These offsets and lengths should all be in the reference DPI which
   * is set to HW_LPI */
  req.offx = htons (params->iLeft);
  req.offy = htons (params->iTop);
  req.lenx = htons (params->iWidth);
  req.leny = htons (params->iHeight);

  req.flags1 = htons (0x0080);
  req.flags2 = htons (0x0000);
  req.flags3 = htons ((params->iDpi < 2400) ? 0x18E8 : 0x18C0);	/* Try preview scan */

  req.gamma[0] = htons (100);
  req.gamma[1] = htons (100);
  req.gamma[2] = htons (100);

  if (Calibrate (iHandle, params->iDpi) != 0)
    return -1;
  SetDefaultGamma (iHandle);

  result = DoScan (iHandle, &req, filename, 0x40, &res);

  /* Pass the results back to the parent */
  params->iBytesPerLine = htonl (res.xsize);
  params->iLines = htons (res.ysize);

#if 0
  imageFile = fopen ("output.dat", "r+b");
  if (imageFile)
    {
      int planes = 3;
      int bpp = 1;
      fseek (imageFile, 0, SEEK_SET);
      DecodeImage (imageFile, planes, bpp, planes * bpp * params->iWidth,
		   params->iHeight, filename);
      fclose (imageFile);
    }
#endif
  return result;
}

HP5400_SANE_STATIC
int
PreviewScan (int iHandle)
{
  TScanParams params;
  THWParams pHWParams;

  /* Reference LPI is 300dpi, remember */
  params.iDpi = 75;
  params.iLpi = 75;
  params.iLeft = 0;
  params.iTop = 0;
  params.iWidth = 2552;		/* = 21.61cm * 300dpi */
  params.iHeight = 3510;	/* = 29.72cm * 300dpi */
  params.iColourOffset = 1;

  return hp5400_scan (iHandle, &params, &pHWParams, "output.ppm");
}


static char UISetup1[] = {
/* Offset 40 */
  0x50, 0x72, 0x6F, 0x63, 0x65, 0x73, 0x73, 0x69, 0x6E, 0x67, 0x14, 0x00,
    0x52, 0x65, 0x61, 0x64,
  0x79, 0x00, 0x53, 0x63, 0x61, 0x6E, 0x6E, 0x65, 0x72, 0x20, 0x4C, 0x6F,
    0x63, 0x6B, 0x65, 0x64,
  0x00, 0x45, 0x72, 0x72, 0x6F, 0x72, 0x00, 0x43, 0x61, 0x6E, 0x63, 0x65,
    0x6C, 0x69, 0x6E, 0x67,
  0x14, 0x00, 0x50, 0x6F, 0x77, 0x65, 0x72, 0x53, 0x61, 0x76, 0x65, 0x20,
    0x4F, 0x6E, 0x00, 0x53,
};

static char UISetup2[] = {

  0x63, 0x61, 0x6E, 0x6E, 0x69, 0x6E, 0x67, 0x14, 0x00, 0x41, 0x44, 0x46,
    0x20, 0x70, 0x61, 0x70,
  0x65, 0x72, 0x20, 0x6A, 0x61, 0x6D, 0x00, 0x43, 0x6F, 0x70, 0x69, 0x65,
    0x73, 0x00, 0x00,
};

HP5400_SANE_STATIC
int
InitScanner (int iHandle)
{
  WarmupLamp (iHandle);

  if (WriteByte (iHandle, 0xF200, 0x40) < 0)
    return -1;

  if (hp5400_command_write (iHandle, 0xF10B, sizeof (UISetup1), UISetup1) < 0)
    {
      HP5400_DBG (DBG_MSG, "failed to send UISetup1 (%lu)\n", (u_long) sizeof (UISetup1));
      return -1;
    }

  if (WriteByte (iHandle, 0xF200, 0x00) < 0)
    return -1;

  if (hp5400_command_write (iHandle, 0xF10C, sizeof (UISetup2), UISetup2) < 0)
    {
      HP5400_DBG (DBG_MSG, "failed to send UISetup2\n");
      return -1;
    }
  return 0;
}
#endif

/* Warning! The caller must have configured the gamma tables at this stage */
HP5400_SANE_STATIC
int
InitScan (enum ScanType scantype, TScanParams * pParams,
	  THWParams * pHWParams)
{
  struct ScanRequest req;
  struct ScanResponse res;
  int ret;
  
  memset(&req, 0, sizeof(req));

  req.x1 = 0x08;
  req.dpix = htons (pParams->iDpi);	/* = 300 dpi */
  req.dpiy = htons (pParams->iLpi);	/* = 300 dpi */
  req.offx = htons (pParams->iLeft);	/* = 0cm */
  req.offy = htons (pParams->iTop);	/* = 0cm */
  req.lenx = htons (pParams->iWidth);	/* = 22.78cm */

  /* Scan a few extra rows so we can colour offset properly. At 300dpi the
   * difference is 4 lines */

  req.leny = htons (pParams->iHeight);	/* =  0.42cm */

  req.flags1 = htons ((scantype == SCAN_TYPE_CALIBRATION) ? 0x0000 : 0x0080);
  req.flags2 = htons ((scantype == SCAN_TYPE_CALIBRATION) ? 0x0010 :
		      (scantype == SCAN_TYPE_PREVIEW) ? 0x0000 :
		      /* SCAN_TYPE_NORMAL  */ 0x0040);
  req.flags3 = htons (0x18E8);

  req.gamma[0] = htons (100);
  req.gamma[1] = htons (100);
  req.gamma[2] = htons (100);

  if (Calibrate (pHWParams->iXferHandle, pParams->iDpi) != 0)
    return -1;
/*  SetDefaultGamma( pHWParams->iXferHandle ); ** Must be done by caller */

  HP5400_DBG (DBG_MSG, "Calibration complete\n");
  ret =
    InitScan2 (scantype, &req, pHWParams, &res, pParams->iColourOffset, 0x40);

  HP5400_DBG (DBG_MSG, "InitScan2 returned %d\n", ret);

  /* Pass the results back to the parent */
  pParams->iBytesPerLine = htonl (res.xsize);

  /* Hide the extra lines we're scanning */
  pParams->iLines = htons (res.ysize);

  return ret;			/* 0 is good, -1 is bad */
}

/* For want of a better name ... */
HP5400_SANE_STATIC
int
InitScan2 (enum ScanType scantype, struct ScanRequest *req,
	   THWParams * pHWParams, struct ScanResponse *result,
	   int iColourOffset, int code)
{
  struct ScanResponse res;
  int iHandle = pHWParams->iXferHandle;

  memset(&res, 0, sizeof(res));

  /* Protect scanner from damage. This stops stpuid errors.  It basically
   * limits you to the scanner glass. Stuff like calibrations which need
   * more access do it safely by fiddling other paramters. Note you can
   * still break things by fiddling the ScanOffset, but that is not yet
   * under user control */

  if (scantype != SCAN_TYPE_CALIBRATION)
    {
      HP5400_DBG (DBG_MSG, "Off(%d,%d) : Len(%d,%d)\n", htons (req->offx),
	   htons (req->offy), htons (req->lenx), htons (req->leny));
      /* Yes, all the htons() is silly but we want this check as late as possible */
      if (htons (req->offx) > 0x09F8)
	req->offx = htons (0x09F8);
      if (htons (req->offy) > 0x0DB6)
	req->offy = htons (0x0DB6);
      /* These tests are meaningless as htons() returns unsigned 
         if( htons( req->offx ) < 0      ) req->offx = 0;
         if( htons( req->offy ) < 0      ) req->offy = 0;
       */
      if (htons (req->offx) + htons (req->lenx) > 0x09F8)
	req->lenx = htons (0x09F8 - htons (req->offx));
      if (htons (req->offy) + htons (req->leny) > 0x0DB6)
	req->leny = htons (0x0DB6 - htons (req->offy));
      if (htons (req->lenx) <= 1)
	return -1;
      if (htons (req->leny) <= 1)
	return -1;
    }

  WarmupLamp (iHandle);

  {				/* Try to set it to make scan succeed, URB 53 *//*  0x1B01 => 0x40  */
    /* I think this tries to cancel any existing scan */
    char flag = 0x40;
    if (hp5400_command_write (iHandle, CMD_STOPSCAN, sizeof (flag), &flag) <
	0)
      {
	HP5400_DBG (DBG_MSG, "failed to cancel scan flag\n");
	return -1;
      }
  }

  {				/* Try to set it to make scan succeed, URB 55  */
    char data[4] = { 0x02, 0x03, 0x03, 0x3C };
    if (hp5400_command_write (iHandle, CMD_UNKNOWN3, sizeof (data), data) < 0)
      {
	HP5400_DBG (DBG_MSG, "failed to set unknown1\n");
	return -1;
      }
  }

  {				/* Try to set it to make scan succeed, URB 59 */
    char flag = 0x04;
    if (hp5400_command_write (iHandle, CMD_UNKNOWN2, sizeof (flag), &flag) <
	0)
      {
	HP5400_DBG (DBG_MSG, "failed to set unknown2\n");
	return -1;
      }
  }

  {				/* URB 67 *//* Reference DPI = 300 currently */
    short dpi = htons (HW_LPI);
    if (hp5400_command_write (iHandle, CMD_SETDPI, sizeof (dpi), &dpi) < 0)
      {
	HP5400_DBG (DBG_MSG, "failed to set dpi\n");
	return -1;
      }
  }

  if (scantype != SCAN_TYPE_CALIBRATION)
    {				/* Setup scan offsets - Should only apply to non-calibration scans */
      short offsets[2];

      offsets [0] = htons (0x0054);
      offsets [1] = htons (0x0282);

      if (hp5400_command_write
	  (iHandle, CMD_SETOFFSET, sizeof (offsets), offsets) < 0)
	{
	  HP5400_DBG (DBG_MSG, "failed to set offsets\n");
	  return -1;
	}
    }

  HP5400_DBG (DBG_MSG, "Scan request: \n  ");
  {
    size_t i;
    for (i = 0; i < sizeof (*req); i++)
      {
	HP5400_DBG (DBG_MSG, "%02X ", ((unsigned char *) req)[i]);
      }
    HP5400_DBG (DBG_MSG, "\n");
  }

  if (hp5400_command_write
      (iHandle,
       (scantype !=
	SCAN_TYPE_CALIBRATION) ? CMD_SCANREQUEST2 : CMD_SCANREQUEST,
       sizeof (*req), req) < 0)
    {
      HP5400_DBG (DBG_MSG, "failed to send scan request\n");
      return -1;
    }

  {				/* Try to set it to make scan succeed, URB 71 */
    char flag = code;		/* Start scan with light on or off as requested */
    if (hp5400_command_write (iHandle, CMD_STARTSCAN, sizeof (flag), &flag) <
	0)
      {
	HP5400_DBG (DBG_MSG, "failed to set gamma flag\n");
	return -1;
      }
  }

  if (hp5400_command_read (iHandle, CMD_SCANRESPONSE, sizeof (res), &res) < 0)
    {
      HP5400_DBG (DBG_MSG, "failed to read scan response\n");
      return -1;
    }

  HP5400_DBG (DBG_MSG, "Scan response: \n  ");
  {
    size_t i;
    for (i = 0; i < sizeof (res); i++)
      {
	HP5400_DBG (DBG_MSG, "%02X ", ((unsigned char *) &res)[i]);
      }
    HP5400_DBG (DBG_MSG, "\n");
  }

  HP5400_DBG (DBG_MSG, "Bytes to transfer: %d\nBitmap resolution: %d x %d\n",
       htonl (res.transfersize), htonl (res.xsize), htons (res.ysize));

  HP5400_DBG (DBG_MSG, "Proceeding to scan\n");

  if (htonl (res.transfersize) == 0)
    {
      HP5400_DBG (DBG_MSG, "Hmm, size is zero. Obviously a problem. Aborting...\n");
      return -1;
    }

  {
    char x1 = 0x14, x2 = 0x24;

    float pixels = ((float) htons (req->lenx) * (float) htons (req->leny)) *
      ((float) htons (req->dpix) * (float) htons (req->dpiy)) /
      ((float) HW_LPI * (float) HW_LPI);
    int bpp = (int) ((float) htonl (res.transfersize) / pixels + 0.5);
    int planes = (bpp == 1) ? 1 : 3;
    bpp /= planes;

    HP5400_DBG (DBG_MSG, "bpp = %d / ( (%d * %d) * (%d * %d) / (%d * %d) ) = %d\n",
	 htonl (res.transfersize),
	 htons (req->lenx), htons (req->leny),
	 htons (req->dpix), htons (req->dpiy), HW_LPI, HW_LPI, bpp);

    hp5400_command_write_noverify (iHandle, CMD_INITBULK1, &x1, 1);
    hp5400_command_write_noverify (iHandle, CMD_INITBULK2, &x2, 1);

    if (bpp > 2)		/* Bug!! */
      bpp = 2;

    CircBufferInit (pHWParams->iXferHandle, &pHWParams->pipe,
		    htonl (res.xsize), bpp, iColourOffset, 0xF000,
		    htonl (res.transfersize) + 3 * htons (res.ysize));
  }

  if (result)			/* copy ScanResult to parent if they asked for it */
    memcpy (result, &res, sizeof (*result));

  return 0;
}

HP5400_SANE_STATIC
void
FinishScan (THWParams * pHWParams)
{
  int iHandle = pHWParams->iXferHandle;

  CircBufferExit (&pHWParams->pipe);

  {				/* Finish scan request */
    char flag = 0x40;
    if (hp5400_command_write (iHandle, CMD_STOPSCAN, sizeof (flag), &flag) <
	0)
      {
	HP5400_DBG (DBG_MSG, "failed to set gamma flag\n");
	return;
      }
  }
}

HP5400_SANE_STATIC
int
HP5400Open (THWParams * params, const char *filename)
{
  int iHandle = hp5400_open (filename);
  char szVersion[32];
  int i;
#ifndef NO_STRING_VERSION_MATCH
  int versionMatched;
#endif

  if (iHandle < 0)
    {
      HP5400_DBG (DBG_MSG, "hp5400_open failed\n");
      return -1;
    }

  params->iXferHandle = 0;

  /* read version info */
  if (hp5400_command_read
      (iHandle, CMD_GETVERSION, sizeof (szVersion), szVersion) < 0)
    {
      HP5400_DBG (DBG_MSG, "failed to read version string\n");
      goto hp5400_close_exit;
    }

	  HP5400_DBG (DBG_MSG, "version String :\n");
    for (i=0; i < 32; i++) {
      HP5400_DBG (DBG_MSG, "%c\n", szVersion[i]);
    }
    HP5400_DBG (DBG_MSG, "\n");

#ifndef NO_STRING_VERSION_MATCH
  i = 0;
  versionMatched = 0;
  while ( !versionMatched && (i < numVersions) ) {
	if (!strncmp (szVersion + 1, MatchVersions[i] .strVersion, strlen(MatchVersions[i] .strVersion) - 4)) {
		versionMatched = 1;
	}
  	i++;
  }
  if ( !versionMatched ) {
	HP5400_DBG (DBG_MSG,
	       "Sorry, unknown scanner version. Attempted match on :\n");
	i = 0;
	while ( i < numVersions ) {
		HP5400_DBG (DBG_MSG, "* '%s'\n", MatchVersions[i] .strVersion);
		i++;
	}
	HP5400_DBG (DBG_MSG, "Version is '%s'\n", szVersion);
	goto hp5400_close_exit;
    }
#else
  HP5400_DBG (DBG_MSG, "Warning, Version match is disabled. Version is '%s'\n",
       szVersion);
#endif /* NO_STRING_VERSION_MATCH */

  params->iXferHandle = iHandle;

  /* Start the lamp warming up */
  /* No point checking the return value, we don't care anyway */
  WriteByte (iHandle, 0x0000, 0x01);

  /* Success */
  return 0;

hp5400_close_exit:
  hp5400_close (iHandle);
  return -1;
}

HP5400_SANE_STATIC
void
HP5400Close (THWParams * params)
{
  hp5400_close (params->iXferHandle);
}

HP5400_SANE_STATIC
int
HP5400Detect (const char *filename,
	      int (*_ReportDevice) (TScannerModel * pModel,
				    const char *pszDeviceName))
{
  int iHandle = hp5400_open (filename);

  char szVersion[32];
  int ret = 0;
#ifndef NO_STRING_VERSION_MATCH
  int versionMatched = 0;
  int i = 0;
#endif

  if (iHandle < 0)
    {
      HP5400_DBG (DBG_MSG, "hp5400_open failed\n");
      return -1;
    }

  /* read version info */
  if (hp5400_command_read
      (iHandle, CMD_GETVERSION, sizeof (szVersion), szVersion) < 0)
    {
      HP5400_DBG (DBG_MSG, "failed to read version string\n");
      ret = -1;
      goto hp5400_close_exit;
    }

#ifndef NO_STRING_VERSION_MATCH
  i = 0;
  versionMatched = 0;
  while ( !versionMatched && (i < numVersions) ) {
	if (!memcmp (szVersion + 1, MatchVersions[i] .strVersion, strlen (MatchVersions[i] .strVersion) - 4)) {
		versionMatched = 1;
	}
  	i++;
  }
  if ( !versionMatched ) {
	HP5400_DBG (DBG_MSG,
	       "Sorry, unknown scanner version. Attempted match on :\n");
	i = 0;
	while ( i < numVersions ) {
		HP5400_DBG (DBG_MSG, "* '%s'\n", MatchVersions[i] .strVersion);
		i++;
	}
	HP5400_DBG (DBG_MSG, "Version is '%s'\n", szVersion);
	goto hp5400_close_exit;
    }
#else
  HP5400_DBG (DBG_MSG, "Warning, Version match is disabled. Version is '%s'\n",
       szVersion);
#endif /* NO_STRING_VERSION_MATCH */

  if (_ReportDevice)
    _ReportDevice (&Model_HP54xx, filename);

hp5400_close_exit:
  hp5400_close (iHandle);
  return ret;
}

#ifdef STANDALONE
int
main (int argc, char *argv[])
{
  THWParams scanner;

  assert (sizeof (struct ScanRequest) == 32);
  assert (sizeof (struct ScanResponse) == 16);

  hp5400_dbg_start();

  HP5400_DBG (DBG_MSG,
       "HP5400/5470C sample scan utility, by Martijn van Oosterhout <kleptog@svana.org>\n");
  HP5400_DBG (DBG_MSG,
       "Based on the testutils by Bertrik Sikken (bertrik@zonnet.nl)\n");

  if ((argc == 6) && (!strcmp (argv[1], "-decode")))
    {
      int width = atoi (argv[3]);
      int height = atoi (argv[4]);
      FILE *temp = fopen (argv[2], "r+b");
      if (temp)
	{
	  int planes = 3;
	  int bpp = 1;
	  fseek (temp, 0, SEEK_SET);
	  DecodeImage (temp, planes, bpp, planes * bpp * width, height,
		       argv[5]);
	  fclose (temp);
	}
      return 1;
    }

  if (HP5400Open (&scanner, NULL) < 0)
    {
      return 1;
    }

  PreviewScan (scanner.iXferHandle);

  HP5400Close (&scanner);
  fprintf (stderr, "Note: output is in output.ppm\n");
  return 0;
}

#endif
