/* HP Scanjet 3900 series - Debugging functions for standalone

   Copyright (C) 2005-2008 Jonathan Bravo Lopez <jkdsoft@gmail.com>

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
*/

/* debugging level messages */
#define DBG_ERR             0x00	/* Only important errors         */
#define DBG_VRB             0x01	/* verbose messages              */
#define DBG_FNC             0x02	/* Function names and parameters */
#define DBG_CTL             0x03	/* USB Ctl data                  */
#define DBG_BLK             0x04	/* USB Bulk data                 */

#include <stdarg.h>
#ifdef HAVE_TIFFIO_H
#include <tiffio.h>		/* dbg_tiff_save */
#endif

/* headers */

static void dump_shading (struct st_calibration *myCalib);
static char *dbg_scantype (SANE_Int type);
static void dbg_scanmodes (struct st_device *dev);
static void dbg_motorcurves (struct st_device *dev);
static void dbg_motormoves (struct st_device *dev);
static void dbg_hwdcfg (struct st_hwdconfig *params);
static void dbg_ScanParams (struct st_scanparams *params);
static void dbg_calibtable (struct st_gain_offset *params);
static char *dbg_colour (SANE_Int colour);
static void dbg_motorcfg (struct st_motorcfg *motorcfg);
static void dbg_buttons (struct st_buttons *buttons);
static void dbg_sensor (struct st_sensorcfg *sensor);
static void dbg_timing (struct st_timing *mt);
static void dbg_sensorclock (struct st_cph *cph);
static void dbg_tiff_save (char *sFile, SANE_Int width, SANE_Int height,
			   SANE_Int depth, SANE_Int colortype, SANE_Int res_x,
			   SANE_Int res_y, SANE_Byte * buffer, SANE_Int size);
static void dbg_autoref (struct st_scanparams *scancfg, SANE_Byte * pattern,
			 SANE_Int ser1, SANE_Int ser2, SANE_Int ler);

#ifdef developing
static void dbg_buffer (SANE_Int level, char *title, SANE_Byte * buffer,
			SANE_Int size, SANE_Int start);
static void dbg_registers (SANE_Byte * buffer);
#endif

#ifdef STANDALONE

/* implementation */

int DBG_LEVEL = 0;

static void
DBG (int level, const char *msg, ...)
{
  va_list ap;
  va_start (ap, msg);

  if (level <= DBG_LEVEL)
    vfprintf (stderr, msg, ap);

  va_end (ap);
}

#endif

/* debugging functions */

static void
dump_shading (struct st_calibration *myCalib)
{
  if (myCalib != NULL)
    {
      SANE_Int colour, a;
      FILE *shadingfile[3];

      shadingfile[0] = fopen ("RShading.txt", "w");
      shadingfile[1] = fopen ("GShading.txt", "w");
      shadingfile[2] = fopen ("BShading.txt", "w");

      for (colour = 0; colour < 3; colour++)
	{
	  if (shadingfile[colour] != NULL)
	    {
	      for (a = 0; a < myCalib->shadinglength; a++)
		fprintf (shadingfile[colour], "%04i: %04x %04x\n", a,
			 (unsigned int) myCalib->white_shading[colour][a],
			 (unsigned int) myCalib->black_shading[colour][a]);
	      fclose (shadingfile[colour]);
	    }
	}
    }
}

static char *
dbg_scantype (SANE_Int type)
{
  switch (type)
    {
    case ST_NORMAL:
      return "ST_NORMAL";
      break;
    case ST_TA:
      return "ST_TA";
      break;
    case ST_NEG:
      return "ST_NEG";
      break;
    default:
      return "Unknown";
      break;
    }
}

static void
dbg_sensorclock (struct st_cph *cph)
{
  if (cph != NULL)
    {
      DBG (DBG_FNC, " -> cph->p1 = %f\n", cph->p1);
      DBG (DBG_FNC, " -> cph->p2 = %f\n", cph->p2);
      DBG (DBG_FNC, " -> cph->ps = %i\n", cph->ps);
      DBG (DBG_FNC, " -> cph->ge = %i\n", cph->ge);
      DBG (DBG_FNC, " -> cph->go = %i\n", cph->go);
    }
  else
    DBG (DBG_FNC, " -> cph is NULL\n");
}

static void
dbg_timing (struct st_timing *mt)
{
  if (mt != NULL)
    {
      DBG (DBG_FNC, " -> mt->cdss[0]   = %i\n", _B0 (mt->cdss[0]));
      DBG (DBG_FNC, " -> mt->cdsc[0]   = %i\n", _B0 (mt->cdsc[0]));
      DBG (DBG_FNC, " -> mt->cdss[1]   = %i\n", _B0 (mt->cdss[1]));
      DBG (DBG_FNC, " -> mt->cdsc[1]   = %i\n", _B0 (mt->cdsc[1]));
      DBG (DBG_FNC, " -> mt->cnpp      = %i\n", _B0 (mt->cnpp));
      DBG (DBG_FNC, " -> mt->cvtrp0    = %i\n", _B0 (mt->cvtrp[0]));
      DBG (DBG_FNC, " -> mt->cvtrp1    = %i\n", _B0 (mt->cvtrp[1]));
      DBG (DBG_FNC, " -> mt->cvtrp2    = %i\n", _B0 (mt->cvtrp[2]));
      DBG (DBG_FNC, " -> mt->cvtrfpw   = %i\n", _B0 (mt->cvtrfpw));
      DBG (DBG_FNC, " -> mt->cvtrbpw   = %i\n", _B0 (mt->cvtrbpw));
      DBG (DBG_FNC, " -> mt->cvtrw     = %i\n", _B0 (mt->cvtrw));
      DBG (DBG_FNC, " -> mt->clamps    = 0x%08x\n", mt->clamps);
      DBG (DBG_FNC, " -> mt->clampe    = 0x%08x\n", mt->clampe);
      DBG (DBG_FNC, " -> mt->adcclkp0  = %f\n", mt->adcclkp[0]);
      DBG (DBG_FNC, " -> mt->adcclkp1  = %f\n", mt->adcclkp[1]);
      DBG (DBG_FNC, " -> mt->adcclkp2e = %i\n", mt->adcclkp2e);
      DBG (DBG_FNC, " -> mt->cphbp2s   = %i\n", mt->cphbp2s);
      DBG (DBG_FNC, " -> mt->cphbp2e   = %i\n", mt->cphbp2e);
    }
  else
    DBG (DBG_FNC, " -> mt is NULL\n");
}

static void
dbg_sensor (struct st_sensorcfg *sensor)
{
  if (sensor != NULL)
    {
      DBG (DBG_FNC,
	   " -> type, name, res , {chn_color }, {chn_gray}, {rgb_order }, line_dist, evnodd_dist\n");
      DBG (DBG_FNC,
	   " -> ----, ----, --- , {--, --, --}, {--, --  }, {--, --, --}, ---------, -----------\n");
      DBG (DBG_FNC,
	   " -> %4i, %4i, %4i, {%2i, %2i, %2i}, {%2i, %2i  }, {%2i, %2i, %2i}, %9i, %11i\n",
	   sensor->type, sensor->name, sensor->resolution,
	   sensor->channel_color[0], sensor->channel_color[1],
	   sensor->channel_color[2], sensor->channel_gray[0],
	   sensor->channel_gray[1], sensor->rgb_order[0],
	   sensor->rgb_order[1], sensor->rgb_order[2], sensor->line_distance,
	   sensor->evenodd_distance);
    }
  else
    DBG (DBG_FNC, " -> sensor is NULL\n");
}

static void
dbg_buttons (struct st_buttons *buttons)
{
  if (buttons != NULL)
    {
      DBG (DBG_FNC, " -> count, btn1, btn2, btn3, btn4, btn5, btn6\n");
      DBG (DBG_FNC, " -> -----, ----, ----, ----, ----, ----, ----\n");
      DBG (DBG_FNC, " -> %5i, %4i, %4i, %4i, %4i, %4i, %4i\n",
	   buttons->count, buttons->mask[0], buttons->mask[1],
	   buttons->mask[2], buttons->mask[3], buttons->mask[4],
	   buttons->mask[5]);
    }
  else
    DBG (DBG_FNC, " -> buttons is NULL\n");
}

static void
dbg_scanmodes (struct st_device *dev)
{
  if (dev->scanmodes_count > 0)
    {
      SANE_Int a;
      struct st_scanmode *reg;

      DBG (DBG_FNC,
	   " -> ##, ST       , CM        , RES , TM, CV, SR, CLK, CTPC  , BKS , STT, DML, {   Exposure times     }, { Max exposure times   }, MP , MExp16, MExpF, MExp, MRI, MSI, MMTIR, MMTIRH, SK\n");
      DBG (DBG_FNC,
	   " -> --, ---------, ----------, --- , --, --, --, ---, ------, ----, ---, ---, {------  ------  ------}, {------  ------  ------}, ---, ------, -----, ----, ---, ---, -----, ------, --\n");
      for (a = 0; a < dev->scanmodes_count; a++)
	{
	  reg = dev->scanmodes[a];
	  if (reg != NULL)
	    {
	      DBG (DBG_FNC,
		   " -> %2i, %9s, %10s, %4i, %2i, %2i, %2i, %3i, %6i, %4i, %3i, %3i, {%6i, %6i, %6i}, {%6i, %6i, %6i}, %3i, %6i, %5i, %4i, %3i, %3i, %5i, %6i, %2i\n",
		   a, dbg_scantype (reg->scantype),
		   dbg_colour (reg->colormode), reg->resolution, reg->timing,
		   reg->motorcurve, reg->samplerate, reg->systemclock,
		   reg->ctpc, reg->motorbackstep, reg->scanmotorsteptype,
		   reg->dummyline, reg->expt[0], reg->expt[1], reg->expt[2],
		   reg->mexpt[0], reg->mexpt[1], reg->mexpt[2],
		   reg->motorplus, reg->multiexposurefor16bitmode,
		   reg->multiexposureforfullspeed, reg->multiexposure,
		   reg->mri, reg->msi, reg->mmtir, reg->mmtirh,
		   reg->skiplinecount);
	    }
	}
    }
}

static void
dbg_motorcurves (struct st_device *dev)
{
  if (dev->mtrsetting != NULL)
    {
      struct st_motorcurve *mtc;
      SANE_Int a = 0;

      while (a < dev->mtrsetting_count)
	{
	  DBG (DBG_FNC, " -> Motorcurve %2i: ", a);
	  mtc = dev->mtrsetting[a];
	  if (mtc != NULL)
	    {
	      DBG (DBG_FNC, "mri=%i msi=%i skip=%i bckstp=%i\n", mtc->mri,
		   mtc->msi, mtc->skiplinecount, mtc->motorbackstep);
	      if (mtc->curve_count > 0)
		{
		  char *sdata = (char *) malloc (256);
		  if (sdata != NULL)
		    {
		      char *sline = (char *) malloc (256);
		      if (sline != NULL)
			{
			  SANE_Int count;
			  struct st_curve *crv;

			  DBG (DBG_FNC,
			       " ->  ##, dir, type      , count, from, to  , steps\n");
			  DBG (DBG_FNC,
			       " ->  --, ---, ----------, -----, ----, ----, -----\n");

			  count = 0;
			  while (count < mtc->curve_count)
			    {
			      memset (sline, 0, 256);

			      snprintf (sdata, 256, " ->  %02i, ", count);
			      strcat (sline, sdata);

			      crv = mtc->curve[count];
			      if (crv != NULL)
				{
				  if (crv->crv_speed == ACC_CURVE)
				    strcat (sline, "ACC, ");
				  else
				    strcat (sline, "DEC, ");

				  switch (crv->crv_type)
				    {
				    case CRV_NORMALSCAN:
				      strcat (sline, "NORMALSCAN, ");
				      break;
				    case CRV_PARKHOME:
				      strcat (sline, "PARKHOME  , ");
				      break;
				    case CRV_SMEARING:
				      strcat (sline, "SMEARING  , ");
				      break;
				    case CRV_BUFFERFULL:
				      strcat (sline, "BUFFERFULL, ");
				      break;
				    default:
				      snprintf (sdata, 256, "unknown %2i, ",
						crv->crv_type);
				      strcat (sline, sdata);
				      break;
				    }

				  snprintf (sdata, 256, "%5i, ",
					    crv->step_count);
				  strcat (sline, sdata);
				  if (crv->step_count > 0)
				    {
				      SANE_Int stpcount = 0;

				      snprintf (sdata, 256, "%4i, %4i| ",
						crv->step[0],
						crv->step[crv->step_count -
							  1]);
				      strcat (sline, sdata);

				      while (stpcount < crv->step_count)
					{
					  if (stpcount == 10)
					    {
					      strcat (sline, "...");
					      break;
					    }
					  if (stpcount > 0)
					    strcat (sline, ", ");

					  snprintf (sdata, 256, "%4i",
						    crv->step[stpcount]);
					  strcat (sline, sdata);

					  stpcount++;
					}
				      strcat (sline, "\n");
				    }
				  else
				    strcat (sline, "NONE\n");
				}
			      else
				strcat (sline, "NULL ...\n");

			      DBG (DBG_FNC, "%s", sline);

			      count++;
			    }

			  free (sline);
			}
		      free (sdata);
		    }
		}
	    }
	  else
	    DBG (DBG_FNC, "NULL\n");
	  a++;
	}
    }
}

static void
dbg_motormoves (struct st_device *dev)
{
  if (dev->motormove_count > 0)
    {
      SANE_Int a;
      struct st_motormove *reg;

      DBG (DBG_FNC, " -> ##, CLK, CTPC, STT, CV\n");
      DBG (DBG_FNC, " -> --, ---, ----, ---, --\n");
      for (a = 0; a < dev->motormove_count; a++)
	{
	  reg = dev->motormove[a];
	  if (reg != NULL)
	    {
	      DBG (DBG_FNC, " -> %2i, %3i, %4i, %3i, %2i\n",
		   a, reg->systemclock, reg->ctpc,
		   reg->scanmotorsteptype, reg->motorcurve);
	    }
	}
    }
}

static void
dbg_hwdcfg (struct st_hwdconfig *params)
{
  if (params != NULL)
    {
      DBG (DBG_FNC, " -> Low level config:\n");
      DBG (DBG_FNC, " -> startpos              = %i\n", params->startpos);
      DBG (DBG_FNC, " -> arrangeline           = %s\n",
	   (params->arrangeline ==
	    FIX_BY_SOFT) ? "FIX_BY_SOFT" : (params->arrangeline ==
					    FIX_BY_HARD) ? "FIX_BY_HARD" :
	   "FIX_BY_NONE");
      DBG (DBG_FNC, " -> scantype              = %s\n",
	   dbg_scantype (params->scantype));
      DBG (DBG_FNC, " -> compression           = %i\n", params->compression);
      DBG (DBG_FNC, " -> use_gamma_tables      = %i\n",
	   params->use_gamma_tables);
      DBG (DBG_FNC, " -> gamma_tablesize       = %i\n",
	   params->gamma_tablesize);
      DBG (DBG_FNC, " -> white_shading         = %i\n",
	   params->white_shading);
      DBG (DBG_FNC, " -> black_shading         = %i\n",
	   params->black_shading);
      DBG (DBG_FNC, " -> unk3                  = %i\n", params->unk3);
      DBG (DBG_FNC, " -> motorplus             = %i\n", params->motorplus);
      DBG (DBG_FNC, " -> static_head           = %i\n", params->static_head);
      DBG (DBG_FNC, " -> motor_direction       = %s\n",
	   (params->motor_direction == MTR_FORWARD) ? "FORWARD" : "BACKWARD");
      DBG (DBG_FNC, " -> dummy_scan            = %i\n", params->dummy_scan);
      DBG (DBG_FNC, " -> highresolution        = %i\n",
	   params->highresolution);
      DBG (DBG_FNC, " -> sensorevenodddistance = %i\n",
	   params->sensorevenodddistance);
      DBG (DBG_FNC, " -> calibrate             = %i\n", params->calibrate);
    }
}

static void
dbg_ScanParams (struct st_scanparams *params)
{
  if (params != NULL)
    {
      DBG (DBG_FNC, " -> Scan params:\n");
      DBG (DBG_FNC, " -> colormode        = %s\n",
	   dbg_colour (params->colormode));
      DBG (DBG_FNC, " -> depth            = %i\n", params->depth);
      DBG (DBG_FNC, " -> samplerate       = %i\n", params->samplerate);
      DBG (DBG_FNC, " -> timing           = %i\n", params->timing);
      DBG (DBG_FNC, " -> channel          = %i\n", params->channel);
      DBG (DBG_FNC, " -> sensorresolution = %i\n", params->sensorresolution);
      DBG (DBG_FNC, " -> resolution_x     = %i\n", params->resolution_x);
      DBG (DBG_FNC, " -> resolution_y     = %i\n", params->resolution_y);
      DBG (DBG_FNC, " -> left             = %i\n", params->coord.left);
      DBG (DBG_FNC, " -> width            = %i\n", params->coord.width);
      DBG (DBG_FNC, " -> top              = %i\n", params->coord.top);
      DBG (DBG_FNC, " -> height           = %i\n", params->coord.height);
      DBG (DBG_FNC, " -> shadinglength    = %i\n", params->shadinglength);
      DBG (DBG_FNC, " -> v157c            = %i\n", params->v157c);
      DBG (DBG_FNC, " -> bytesperline     = %i\n", params->bytesperline);
      DBG (DBG_FNC, " -> expt             = %i\n", params->expt);
      DBG (DBG_FNC, " *> startpos         = %i\n", params->startpos);
      DBG (DBG_FNC, " *> leftleading      = %i\n", params->leftleading);
      DBG (DBG_FNC, " *> ser              = %i\n", params->ser);
      DBG (DBG_FNC, " *> ler              = %i\n", params->ler);
      DBG (DBG_FNC, " *> scantype         = %s\n",
	   dbg_scantype (params->scantype));
    }
}

static void
dbg_calibtable (struct st_gain_offset *params)
{
  if (params != NULL)
    {
      DBG (DBG_FNC, " -> Calib table:\n");
      DBG (DBG_FNC, " -> type     R     G     B\n");
      DBG (DBG_FNC, " -> -----   ---   ---   ---B\n");
      DBG (DBG_FNC, " -> edcg1 = %3i , %3i , %3i\n", params->edcg1[0],
	   params->edcg1[1], params->edcg1[2]);
      DBG (DBG_FNC, " -> edcg2 = %3i , %3i , %3i\n", params->edcg2[0],
	   params->edcg2[1], params->edcg2[2]);
      DBG (DBG_FNC, " -> odcg1 = %3i , %3i , %3i\n", params->odcg1[0],
	   params->odcg1[1], params->odcg1[2]);
      DBG (DBG_FNC, " -> odcg2 = %3i , %3i , %3i\n", params->odcg2[0],
	   params->odcg2[1], params->odcg2[2]);
      DBG (DBG_FNC, " -> pag   = %3i , %3i , %3i\n", params->pag[0],
	   params->pag[1], params->pag[2]);
      DBG (DBG_FNC, " -> vgag1 = %3i , %3i , %3i\n", params->vgag1[0],
	   params->vgag1[1], params->vgag1[2]);
      DBG (DBG_FNC, " -> vgag2 = %3i , %3i , %3i\n", params->vgag2[0],
	   params->vgag2[1], params->vgag2[2]);
    }
}

static char *
dbg_colour (SANE_Int colour)
{
  switch (colour)
    {
    case CM_COLOR:
      return "CM_COLOR";
      break;
    case CM_GRAY:
      return "CM_GRAY";
      break;
    case CM_LINEART:
      return "CM_LINEART";
      break;
    default:
      return "Unknown";
      break;
    }
}

static void
dbg_motorcfg (struct st_motorcfg *motorcfg)
{
  if (motorcfg != NULL)
    {
      DBG (DBG_FNC,
	   " -> type, res , freq, speed, base, high, park, change\n");
      DBG (DBG_FNC,
	   " -> ----, --- , ----, -----, ----, ----, ----, ------\n");
      DBG (DBG_FNC, " -> %4i, %4i, %4i, %5i, %4i, %4i, %4i, %6i\n",
	   motorcfg->type, motorcfg->resolution, motorcfg->pwmfrequency,
	   motorcfg->basespeedpps, motorcfg->basespeedmotormove,
	   motorcfg->highspeedmotormove, motorcfg->parkhomemotormove,
	   motorcfg->changemotorcurrent);
    }
}

static void
dbg_tiff_save (char *sFile, SANE_Int width, SANE_Int height, SANE_Int depth,
	       SANE_Int colortype, SANE_Int res_x, SANE_Int res_y,
	       SANE_Byte * buffer, SANE_Int size)
{
#ifdef HAVE_TIFFIO_H
  if (buffer != NULL)
    {
      char *path = getenv ("HOME");

      if (path != NULL)
	{
	  char filename[512];
	  TIFF *image;

	  if (snprintf (filename, 512, "%s/%s", path, sFile) > 0)
	    {
	      /* Open the TIFF file */
	      if ((image = TIFFOpen (filename, "w")) != NULL)
		{
		  char desc[256];

		  SANE_Int spp = (colortype == CM_GRAY) ? 1 : 3;
		  SANE_Int ct =
		    (colortype ==
		     CM_GRAY) ? PHOTOMETRIC_MINISBLACK : PHOTOMETRIC_RGB;

		  snprintf (desc, 256, "Created with hp3900 %s",
			    BACKEND_VRSN);

		  /* We need to set some values for basic tags before we can add any data */
		  TIFFSetField (image, TIFFTAG_IMAGEWIDTH, width);
		  TIFFSetField (image, TIFFTAG_IMAGELENGTH, height);
		  TIFFSetField (image, TIFFTAG_BITSPERSAMPLE, depth);
		  TIFFSetField (image, TIFFTAG_SAMPLESPERPIXEL, spp);

		  TIFFSetField (image, TIFFTAG_PHOTOMETRIC, ct);
		  TIFFSetField (image, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB);
		  TIFFSetField (image, TIFFTAG_PLANARCONFIG,
				PLANARCONFIG_CONTIG);

		  TIFFSetField (image, TIFFTAG_XRESOLUTION, (double) res_x);
		  TIFFSetField (image, TIFFTAG_YRESOLUTION, (double) res_y);
		  TIFFSetField (image, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);
		  TIFFSetField (image, TIFFTAG_IMAGEDESCRIPTION, desc);

		  /* Write the information to the file */
		  TIFFWriteRawStrip (image, 0, buffer, size);
		  TIFFClose (image);
		}
	    }
	  else
	    DBG (DBG_ERR, "- dbg_tiff_save: Error generating filename\n");
	}
      else
	DBG (DBG_ERR,
	     "- dbg_tiff_save: Environment HOME variable does not exist\n");
    }
#else
  /* silent gcc */
  sFile = sFile;
  width = width;
  height = height;
  depth = depth;
  colortype = colortype;
  res_x = res_x;
  res_y = res_y;
  buffer = buffer;
  size = size;

  DBG (DBG_ERR, "- dbg_tiff_save: tiffio not supported\n");
#endif
}

static void
dbg_autoref (struct st_scanparams *scancfg, SANE_Byte * pattern,
	     SANE_Int ser1, SANE_Int ser2, SANE_Int ler)
{
  /* this function generates post-autoref.tiff */
  SANE_Byte *img =
    malloc (sizeof (SANE_Byte) *
	    (scancfg->coord.width * scancfg->coord.height * 3));

  if (img != NULL)
    {
      SANE_Int c, value;

      /* generate image from 1 gray channel to 3 color channels */
      for (c = 0; c < (scancfg->coord.width * scancfg->coord.height); c++)
	{
	  value = *(pattern + c);
	  *(img + (3 * c)) = value;
	  *(img + (3 * c) + 1) = value;
	  *(img + (3 * c) + 2) = value;
	}

      for (c = 0; c < scancfg->coord.height; c++)
	{
	  /* line for first SER */
	  if (c < (ler + 5))
	    {
	      *(img + (scancfg->coord.width * c * 3) + (3 * ser1)) = 0;
	      *(img + (scancfg->coord.width * c * 3) + (3 * ser1) + 1) = 255;
	      *(img + (scancfg->coord.width * c * 3) + (3 * ser1) + 2) = 0;
	    }

	  /* line for second SER */
	  if (c > (ler - 5))
	    {
	      *(img + (scancfg->coord.width * c * 3) + (3 * ser2)) = 90;
	      *(img + (scancfg->coord.width * c * 3) + (3 * ser2) + 1) = 90;
	      *(img + (scancfg->coord.width * c * 3) + (3 * ser2) + 2) = 255;
	    }

	  /* vertical lines of the pointer */
	  if ((c > (ler - 5)) && (c < (ler + 5)))
	    {
	      if ((ser2 - 5) >= 0)
		{
		  *(img + (scancfg->coord.width * c * 3) + (3 * (ser2 - 5))) =
		    255;
		  *(img + (scancfg->coord.width * c * 3) + (3 * (ser2 - 5)) +
		    1) = 255;
		  *(img + (scancfg->coord.width * c * 3) + (3 * (ser2 - 5)) +
		    2) = 0;
		}

	      if ((ser2 + 5) < scancfg->coord.width)
		{
		  *(img + (scancfg->coord.width * c * 3) + (3 * (ser2 + 5))) =
		    255;
		  *(img + (scancfg->coord.width * c * 3) + (3 * (ser2 + 5)) +
		    1) = 255;
		  *(img + (scancfg->coord.width * c * 3) + (3 * (ser2 + 5)) +
		    2) = 0;
		}
	    }
	}

      /* line for first LER */
      for (c = 0; c < scancfg->coord.width; c++)
	{
	  if ((c > (ser1 - 5)) && (c < (ser2 + 5)))
	    {
	      if (c != (ser2 - 5))
		{
		  *(img + (scancfg->coord.width * ler * 3) + (3 * c)) = 255;
		  *(img + (scancfg->coord.width * ler * 3) + (3 * c) + 1) =
		    90;
		  *(img + (scancfg->coord.width * ler * 3) + (3 * c) + 2) =
		    90;
		}

	      /* horizontal lines of the pointer */
	      if ((c > (ser2 - 5)) && (c < (ser2 + 5)))
		{
		  if ((ler - 5) >= 0)
		    {
		      *(img + (scancfg->coord.width * (ler - 5) * 3) +
			(3 * c)) = 255;
		      *(img + (scancfg->coord.width * (ler - 5) * 3) +
			(3 * c) + 1) = 255;
		      *(img + (scancfg->coord.width * (ler - 5) * 3) +
			(3 * c) + 2) = 0;
		    }

		  if ((ler + 5) < scancfg->coord.height)
		    {
		      *(img + (scancfg->coord.width * (ler + 5) * 3) +
			(3 * c)) = 255;
		      *(img + (scancfg->coord.width * (ler + 5) * 3) +
			(3 * c) + 1) = 255;
		      *(img + (scancfg->coord.width * (ler + 5) * 3) +
			(3 * c) + 2) = 0;
		    }
		}
	    }
	}

      dbg_tiff_save ("post-autoref.tiff", scancfg->coord.width,
		     scancfg->coord.height, 8, CM_COLOR,
		     scancfg->resolution_x, scancfg->resolution_y, img,
		     scancfg->coord.height * scancfg->coord.width * 3);

      /* free generated image */
      free (img);
    }
}

#ifdef developing

static void
dbg_buffer (SANE_Int level, char *title, SANE_Byte * buffer, SANE_Int size,
	    SANE_Int start)
{
  if (level <= DBG_LEVEL)
    {
      DBG (level, "%s    ", title);
      if ((size > 0) && (buffer != NULL))
	{
	  SANE_Int cont, data, offset = 0;
	  SANE_Int col = 0;
	  char text[9];
	  char *sline = NULL;
	  char *sdata = NULL;

	  sline = (char *) malloc (81);
	  if (sline != NULL)
	    {
	      sdata = (char *) malloc (81);
	      if (sdata != NULL)
		{
		  for (cont = 0; cont < size; cont++)
		    {
		      if (col == 0)
			{
			  if (cont == 0)
			    snprintf (sline, 80, " BF: ");
			  else
			    snprintf (sline, 80, "               ");
			  bzero (&text, sizeof (text));
			}
		      data = _B0 (buffer[cont]);
		      text[col] = (data > 31) ? data : '·';
		      snprintf (sdata, 80, "%02x ", data);
		      sline = strcat (sline, sdata);
		      col++;
		      offset++;
		      if (col == 8)
			{
			  col = 0;
			  snprintf (sdata, 80, " : %s : 0x%04x\n", text,
				    start + offset - 8);
			  sline = strcat (sline, sdata);
			  DBG (level, "%s", sline);
			  bzero (sline, 81);
			}
		    }
		  if (col > 0)
		    {
		      for (cont = col; cont < 8; cont++)
			{
			  snprintf (sdata, 80, "-- ");
			  sline = strcat (sline, sdata);
			  offset++;
			}
		      snprintf (sdata, 80, " : %s : 0x%04x\n", text,
				start + offset - 8);
		      sline = strcat (sline, sdata);
		      DBG (level, "%s", sline);
		      bzero (sline, 81);
		    }
		  free (sdata);
		}
	      free (sline);
	    }
	}
      else
	DBG (level, "           BF: Empty buffer\n");
    }
}

static void
dbg_registers (SANE_Byte * buffer)
{
  /* buffer size must be RT_BUFFER_LEN bytes */
  /*SANE_Int iValue, iValue2;
     double dValue;

     DBG(DBG_FNC, "\n----------------------------------------------------\n");
     DBG(DBG_FNC, """RTS8822 Control Registers Info""\nAddress  Info\n-------  ----\n");
     iValue = data_lsb_get(&buffer[0x000], 1);
     DBG(DBG_FNC, "\n0x0000");
     DBG(DBG_FNC, "   bit[0..3] = systemclock: 0x%02x\n", iValue & 0x0f);
     DBG(DBG_FNC, "         bit[4]    = 0x%02x : MLOCK\n", (iValue >> 4) & 1);
     DBG(DBG_FNC, "         bit[5]    = 0x%02x : Bit to reset scanner\n", (iValue >> 5) & 1);
     DBG(DBG_FNC, "         bit[6]    = 0x%02x : ?\n", (iValue >> 6) & 1);
     DBG(DBG_FNC, "         bit[7]    = 0x%02x : RTS_IsExecuting\n", (iValue >> 7) & 1);

     iValue = data_lsb_get(&buffer[0x001], 1);
     DBG(DBG_FNC, "0x0001   bit[0]    = 0x%02x : ?\n", iValue & 1);
     DBG(DBG_FNC, "         bit[1]    = 0x%02x : (is 1 if has motorcurves)\n", (iValue >> 1) & 1);
     DBG(DBG_FNC, "         bit[2]    = 0x%02x : ?\n", (iValue >> 2) & 1);
     DBG(DBG_FNC, "         bit[3]    = 0x%02x : ?\n", (iValue >> 3) & 1);
     DBG(DBG_FNC, "         bit[4]    = 0x%02x : dummy scan\n", (iValue >> 4) & 1);
     DBG(DBG_FNC, "         bit[5..7] = 0x%02x : ?\n", (iValue >> 5) & 7);

     dbg_buffer(DBG_FNC, "\n0x0002", &buffer[0x002], 0x0e, 0x02);

     iValue = data_lsb_get(&buffer[0x010], 1);
     DBG(DBG_FNC, "\n0x0010   bit[0..4] = 0x%02x : cvrs\n", iValue & 0x1f);
     DBG(DBG_FNC, "         bit[5]    = 0x%02x : Enable CCD\n", ((iValue >> 5) & 1));
     DBG(DBG_FNC, "         bit[6]    = 0x%02x : Enable CCD channel 1\n", ((iValue >> 6) & 1));
     DBG(DBG_FNC, "         bit[7]    = 0x%02x : Enable CCD channel 2\n", ((iValue >> 7) & 1));

     iValue = data_lsb_get(&buffer[0x011], 1);
     DBG(DBG_FNC, "\n0x0011   bit[0..6] = ?: 0x%02x\n", iValue & 0x3f);
     DBG(DBG_FNC, "         bit[7]    = 0x%02x : sensor type (CCD=0|CIS=1)\n", (iValue >> 7) & 1);

     iValue = data_lsb_get(&buffer[0x012], 1);
     DBG(DBG_FNC, "0x0012   bit[0..5] = 0x%02x [0x%02x,0x%02x,0x%02x] rgb channel order\n", (iValue & 0x3f), (iValue >> 4) & 3, (iValue >> 2) & 3, iValue & 3);
     DBG(DBG_FNC, "         bit[6..7] = channels_per_dot : 0x%02x\n", (iValue >> 6) & 3);

     iValue = data_lsb_get(&buffer[0x013], 1);
     DBG(DBG_FNC, "\n0x0013");
     DBG(DBG_FNC, "   bit[0..1] = Pre-Amplifier Gain[RED]   : 0x%02x\n", iValue & 3);
     DBG(DBG_FNC, "         bit[2..3] = Pre-Amplifier Gain[GREEN] : 0x%02x\n", (iValue >> 2) & 3);
     DBG(DBG_FNC, "         bit[4..5] = Pre-Amplifier Gain[BLUE]  : 0x%02x\n", (iValue >> 4) & 3);
     DBG(DBG_FNC, "         bit[6]    = ? : 0x%02x\n", (iValue >> 6) & 1);
     DBG(DBG_FNC, "         bit[7]    = Enable CCD channel 3:  : 0x%02x\n", (iValue >> 7) & 1);

     iValue = data_lsb_get(&buffer[0x014], 1);
     DBG(DBG_FNC, "\n0x0014");
     DBG(DBG_FNC, "   bit[0..4] = Variable Gain Amplifier 1 [RED] : 0x%02x\n", iValue & 0x1f);
     DBG(DBG_FNC, "         bit[5..7] = Top Reference Voltage: 0x%02x\n", (iValue >> 5) & 3);

     iValue = data_lsb_get(&buffer[0x015], 1);
     DBG(DBG_FNC, "0x0015");
     DBG(DBG_FNC, "   bit[0..4] = Variable Gain Amplifier 1 [GREEN] : 0x%02x\n", iValue & 0x1f);
     DBG(DBG_FNC, "         bit[5..7] = Middle Reference Voltage: 0x%02x\n", (iValue >> 5) & 3);

     iValue = data_lsb_get(&buffer[0x016], 1);
     DBG(DBG_FNC, "0x0016");
     DBG(DBG_FNC, "   bit[0..4] = Variable Gain Amplifier 1 [BLUE] : 0x%02x\n", iValue & 0x1f);
     DBG(DBG_FNC, "         bit[5..7] = Bottom Reference Voltage: 0x%02x\n", (iValue >> 5) & 3);

     iValue = data_lsb_get(&buffer[0x017], 1);
     DBG(DBG_FNC, "0x0017");
     DBG(DBG_FNC, "   bit[0..4] = Variable Gain Amplifier 2 [RED] : 0x%02x\n", iValue & 0x1f);
     DBG(DBG_FNC, "         bit[5..7] = Top Reference Voltage: 0x%02x\n", (iValue >> 5) & 3);

     iValue = data_lsb_get(&buffer[0x018], 1);
     DBG(DBG_FNC, "0x0018");
     DBG(DBG_FNC, "   bit[0..4] = Variable Gain Amplifier 2 [GREEN] : 0x%02x\n", iValue & 0x1f);
     DBG(DBG_FNC, "         bit[5..7] = Middle Reference Voltage: 0x%02x\n", (iValue >> 5) & 3);

     iValue = data_lsb_get(&buffer[0x019], 1);
     DBG(DBG_FNC, "0x0019");
     DBG(DBG_FNC, "   bit[0..4] = Variable Gain Amplifier 2 [BLUE] : 0x%02x\n", iValue & 0x1f);
     DBG(DBG_FNC, "         bit[5..7] = Bottom Reference Voltage: 0x%02x\n", (iValue >> 5) & 3);

     iValue = data_lsb_get(&buffer[0x01a], 1);
     iValue2 = data_lsb_get(&buffer[0x01b], 1);
     DBG(DBG_FNC, "\n0x001a-0x001b\n");
     DBG(DBG_FNC, "         Red Even offset 1: 0x%02x\n", ((iValue2 & 0x80) << 1) | iValue);
     DBG(DBG_FNC, "         Red Even offset 2: 0x%02x\n", iValue2 & 0x3f);

     iValue = data_lsb_get(&buffer[0x01c], 1);
     iValue2 = data_lsb_get(&buffer[0x01d], 1);
     DBG(DBG_FNC, "0x001c-0x001d\n");
     DBG(DBG_FNC, "         Red Odd offset 1: 0x%02x\n", ((iValue2 & 0x80) << 1) | iValue);
     DBG(DBG_FNC, "         Red Odd offset 2: 0x%02x\n", iValue2 & 0x3f);

     iValue = data_lsb_get(&buffer[0x01e], 1);
     iValue2 = data_lsb_get(&buffer[0x01f], 1);
     DBG(DBG_FNC, "0x001e-0x001f\n");
     DBG(DBG_FNC, "         Green Even offset 1: 0x%02x\n", ((iValue2 & 0x80) << 1) | iValue);
     DBG(DBG_FNC, "         Green Even offset 2: 0x%02x\n", iValue2 & 0x3f);

     iValue = data_lsb_get(&buffer[0x020], 1);
     iValue2 = data_lsb_get(&buffer[0x021], 1);
     DBG(DBG_FNC, "0x0020-0x0021\n");
     DBG(DBG_FNC, "         Green Odd offset 1: 0x%02x\n", ((iValue2 & 0x80) << 1) | iValue);
     DBG(DBG_FNC, "         Green Odd offset 2: 0x%02x\n", iValue2 & 0x3f);

     iValue = data_lsb_get(&buffer[0x022], 1);
     iValue2 = data_lsb_get(&buffer[0x023], 1);
     DBG(DBG_FNC, "0x0022-0x0023\n");
     DBG(DBG_FNC, "         Blue Even offset 1: 0x%02x\n", ((iValue2 & 0x80) << 1) | iValue);
     DBG(DBG_FNC, "         Blue Even offset 2: 0x%02x\n", iValue2 & 0x3f);

     iValue = data_lsb_get(&buffer[0x024], 1);
     iValue2 = data_lsb_get(&buffer[0x025], 1);
     DBG(DBG_FNC, "0x0024-0x0025\n");
     DBG(DBG_FNC, "         Blue Odd offset 1: 0x%02x\n", ((iValue2 & 0x80) << 1) | iValue);
     DBG(DBG_FNC, "         Blue Odd offset 2: 0x%02x\n", iValue2 & 0x3f);

     dbg_buffer(DBG_FNC, "\n0x0026", &buffer[0x026], 0x03, 0x26);

     iValue = data_lsb_get(&buffer[0x029], 1);
     DBG(DBG_FNC, "\n0x0029");
     DBG(DBG_FNC, "   First connection to scanner? : 0x%02x\n", iValue);

     dbg_buffer(DBG_FNC, "\n0x002a", &buffer[0x02a], 0x06, 0x2a);

     DBG(DBG_FNC, "\nExposure times:\n");
     iValue = data_lsb_get(&buffer[0x030], 3);
     DBG(DBG_FNC, "0x0030   Line exposure time : %i us\n", iValue);

     iValue = data_lsb_get(&buffer[0x033], 3);
     DBG(DBG_FNC, "\n0x0033   mexpts[RED]  : %i us\n", iValue);

     iValue = data_lsb_get(&buffer[0x036], 3);
     DBG(DBG_FNC, "0x0036    expts[RED]  : %i us\n", iValue);

     iValue = data_lsb_get(&buffer[0x039], 3);
     DBG(DBG_FNC, "0x0039   mexpts[GREEN]: %i us\n", iValue);

     iValue = data_lsb_get(&buffer[0x03c], 3);
     DBG(DBG_FNC, "0x003c    expts[GREEN]: %i us\n", iValue);

     iValue = data_lsb_get(&buffer[0x03f], 3);
     DBG(DBG_FNC, "0x003f   mexpts[BLUE] : %i us\n", iValue);

     iValue = data_lsb_get(&buffer[0x042], 3);
     DBG(DBG_FNC, "0x0042    expts[BLUE] : %i us\n", iValue);

     iValue = data_lsb_get(&buffer[0x045], 1);
     DBG(DBG_FNC, "\n0x0045   bit[0..4] = timing.cvtrfpw: 0x%02x\n", iValue & 0x1f);
     DBG(DBG_FNC, "         bit[5] = timing.cvtrp[2]: 0x%02x\n", (iValue >> 5) & 1);
     DBG(DBG_FNC, "         bit[6] = timing.cvtrp[1]: 0x%02x\n", (iValue >> 6) & 1);
     DBG(DBG_FNC, "         bit[7] = timing.cvtrp[0]: 0x%02x\n", (iValue >> 7) & 1);

     iValue = data_lsb_get(&buffer[0x046], 1);
     DBG(DBG_FNC, "0x0046");
     DBG(DBG_FNC, "   bit[0..4] = timing.cvtrbpw: 0x%02x\n", iValue & 0x1f);
     DBG(DBG_FNC, "         bit[5..7] = ?: 0x%02x\n", (iValue >> 5) & 3);

     iValue = data_lsb_get(&buffer[0x047], 1);
     DBG(DBG_FNC, "0x0047");
     DBG(DBG_FNC, "   timing.cvtrw: 0x%02x\n", iValue);

     iValue = data_lsb_get(&buffer[0x04c], 0x01) & 0x0f;
     dValue = iValue * pow(2, 32);
     iValue = data_lsb_get(&buffer[0x04a], 0x02);
     dValue = dValue + (iValue * pow(2, 16)) + data_lsb_get(&buffer[0x048], 0x02);
     DBG(DBG_FNC, "\n0x0048 Linear image sensor clock 1\n");
     DBG(DBG_FNC, "         bit[0..35] = timing.cph0p1: %.0f.\n", dValue);
     iValue = data_lsb_get(&buffer[0x04c], 0x01);
     DBG(DBG_FNC, "         bit[36] = timing.cph0go: 0x%02x\n", (iValue >> 4) & 1);
     DBG(DBG_FNC, "         bit[37] = timing.cph0ge: 0x%02x\n", (iValue >> 5) & 1);
     DBG(DBG_FNC, "         bit[38] = timing.cph0ps: 0x%02x\n", (iValue >> 6) & 1);
     DBG(DBG_FNC, "         bit[39] = ?: 0x%02x\n", (iValue >> 7) & 1);

     iValue = data_lsb_get(&buffer[0x051], 0x01) & 0x0f;
     dValue = iValue * pow(2, 32);
     iValue = data_lsb_get(&buffer[0x04f], 0x02);
     dValue = dValue + (iValue * pow(2, 16)) + data_lsb_get(&buffer[0x04d], 0x02);
     DBG(DBG_FNC, "0x004d");
     DBG(DBG_FNC, "   bit[0..35] = timing.cph0p2: %.0f.\n", dValue);

     iValue = data_lsb_get(&buffer[0x056], 1) & 0x0f;
     dValue = iValue * pow(2, 32);
     iValue = data_lsb_get(&buffer[0x054], 2);
     dValue = dValue + (iValue * pow(2, 16)) + data_lsb_get(&buffer[0x052], 2);
     DBG(DBG_FNC, "\n0x0052 Linear image sensor clock 2\n");
     DBG(DBG_FNC, "         bit[0..35] = timing.cph1p1: %.0f.\n", dValue);
     iValue = data_lsb_get(&buffer[0x056], 1);
     DBG(DBG_FNC, "         bit[36] = timing.cph1go: 0x%02x\n", (iValue >> 4) & 1);
     DBG(DBG_FNC, "         bit[37] = timing.cph1ge: 0x%02x\n", (iValue >> 5) & 1);
     DBG(DBG_FNC, "         bit[38] = timing.cph1ps: 0x%02x\n", (iValue >> 6) & 1);
     DBG(DBG_FNC, "         bit[39] = ?: 0x%02x\n", (iValue >> 7) & 1);


     iValue = data_lsb_get(&buffer[0x05b], 0x01) & 0x0f;
     dValue = iValue * pow(2, 32);
     iValue = data_lsb_get(&buffer[0x059], 0x02);
     dValue = dValue + (iValue * pow(2, 16)) + data_lsb_get(&buffer[0x057], 0x02);
     DBG(DBG_FNC, "0x0057");
     DBG(DBG_FNC, "   bit[0..35] = timing.cph1p2: %.0f.\n", dValue);
     iValue = data_lsb_get(&buffer[0x05b], 0x01);
     DBG(DBG_FNC, "         bits[36..39] = %02x\n", (iValue >> 0x04) & 0x0f);
     DBG(DBG_FNC, "         bit[36] = ?: %02x\n", (iValue >> 0x04) & 0x01);
     DBG(DBG_FNC, "         bit[37] = ?: %02x\n", (iValue >> 0x05) & 0x01);
     DBG(DBG_FNC, "         bit[38] = ?: %02x\n", (iValue >> 0x06) & 0x01);
     DBG(DBG_FNC, "         bit[39] = ?: %02x\n", (iValue >> 0x07) & 0x01);

     iValue = data_lsb_get(&buffer[0x060], 0x01) & 0x0f;
     dValue = iValue * pow(2, 32);
     iValue = data_lsb_get(&buffer[0x05e], 0x02);
     dValue = dValue + (iValue * pow(2, 16)) + data_lsb_get(&buffer[0x05c], 0x02);
     DBG(DBG_FNC, "\n0x005c Linear Image Sensor Clock 3\n");
     DBG(DBG_FNC, "         bit[0..35] = timing.cph2p1: %.0f.\n", dValue);
     iValue = data_lsb_get(&buffer[0x060], 0x01);
     DBG(DBG_FNC, "         bit[36] = timing.cph2go: 0x%02x\n", (iValue >> 0x04) & 0x01);
     DBG(DBG_FNC, "         bit[37] = timing.cph2ge: 0x%02x\n", (iValue >> 0x05) & 0x01);
     DBG(DBG_FNC, "         bit[38] = timing.cph2ps: 0x%02x\n", (iValue >> 0x06) & 0x01);
     DBG(DBG_FNC, "         bit[39] = ?: 0x%02x\n", (iValue >> 0x07) & 0x01);

     iValue = data_lsb_get(&buffer[0x065], 0x01) & 0x0f;
     dValue = iValue * pow(2, 32);
     iValue = data_lsb_get(&buffer[0x063], 0x02);
     dValue = dValue + (iValue * pow(2, 16)) + data_lsb_get(&buffer[0x061], 0x02);
     DBG(DBG_FNC, "0x0061");
     DBG(DBG_FNC, "   bit[0..35] = timing.cph2p2: %.0f.\n", dValue);
     iValue = data_lsb_get(&buffer[0x065], 0x01);
     DBG(DBG_FNC, "         bits[36..39] = 0x%02x\n", (iValue >> 0x04) & 0x0f);
     DBG(DBG_FNC, "         bit[36] = ?: 0x%02x\n", (iValue >> 0x04) & 0x01);
     DBG(DBG_FNC, "         bit[37] = ?: 0x%02x\n", (iValue >> 0x05) & 0x01);
     DBG(DBG_FNC, "         bit[38] = ?: 0x%02x\n", (iValue >> 0x06) & 0x01);
     DBG(DBG_FNC, "         bit[39] = ?: 0x%02x\n", (iValue >> 0x07) & 0x01);

     iValue = data_lsb_get(&buffer[0x06a], 0x01) & 0x0f;
     dValue = iValue * pow(2, 32);
     iValue = data_lsb_get(&buffer[0x068], 0x02);
     dValue = dValue + (iValue * pow(2, 16)) + data_lsb_get(&buffer[0x066], 0x02);
     DBG(DBG_FNC, "\n0x0066 Linear Image Sensor Clock 4\n");
     DBG(DBG_FNC, "         bit[0..35] = timing.cph3p1: %.0f.\n", dValue);
     iValue = data_lsb_get(&buffer[0x06a], 0x01);
     DBG(DBG_FNC, "         bit[36] = timing.cph3go: 0x%02x\n", (iValue >> 0x04) & 0x01);
     DBG(DBG_FNC, "         bit[37] = timing.cph3ge: 0x%02x\n", (iValue >> 0x05) & 0x01);
     DBG(DBG_FNC, "         bit[38] = timing.cph3ps: 0x%02x\n", (iValue >> 0x06) & 0x01);
     DBG(DBG_FNC, "         bit[39] = ?: 0x%02x\n", (iValue >> 0x07) & 0x01);

     iValue = data_lsb_get(&buffer[0x06f], 0x01) & 0x0f;
     dValue = iValue * pow(2, 32);
     iValue = data_lsb_get(&buffer[0x06d], 0x02);
     dValue = dValue + (iValue * pow(2, 16)) + data_lsb_get(&buffer[0x06b], 0x02);
     DBG(DBG_FNC, "0x006b");
     DBG(DBG_FNC, "   bit[0..35] = timing.cph3p2: %.0f.\n", dValue);
     iValue = data_lsb_get(&buffer[0x06f], 0x01);
     DBG(DBG_FNC, "         bits[36..39] = 0x%02x\n", (iValue >> 0x04) & 0x0f);
     DBG(DBG_FNC, "         bit[36] = ?: 0x%02x\n", (iValue >> 0x04) & 0x01);
     DBG(DBG_FNC, "         bit[37] = ?: 0x%02x\n", (iValue >> 0x05) & 0x01);
     DBG(DBG_FNC, "         bit[38] = ?: 0x%02x\n", (iValue >> 0x06) & 0x01);
     DBG(DBG_FNC, "         bit[39] = ?: 0x%02x\n", (iValue >> 0x07) & 0x01);

     iValue = data_lsb_get(&buffer[0x074], 0x01) & 0x0f;
     dValue = iValue * pow(2, 32);
     iValue = data_lsb_get(&buffer[0x072], 0x02);
     dValue = dValue + (iValue * pow(2, 16)) + data_lsb_get(&buffer[0x070], 0x02);
     DBG(DBG_FNC, "\n0x0070 Linear Image Sensor Clock 5\n");
     DBG(DBG_FNC, "         bit[0..35] = timing.cph4p1: %.0f.\n", dValue);
     iValue = data_lsb_get(&buffer[0x074], 0x01);
     DBG(DBG_FNC, "         bit[36] = timing.cph4go: 0x%02x\n", (iValue >> 0x04) & 0x01);
     DBG(DBG_FNC, "         bit[37] = timing.cph4ge: 0x%02x\n", (iValue >> 0x05) & 0x01);
     DBG(DBG_FNC, "         bit[38] = timing.cph4ps: 0x%02x\n", (iValue >> 0x06) & 0x01);
     DBG(DBG_FNC, "         bit[39] = ?: 0x%02x\n", (iValue >> 0x07) & 0x01);

     iValue = data_lsb_get(&buffer[0x079], 0x01) & 0x0f;
     dValue = iValue * pow(2, 32);
     iValue = data_lsb_get(&buffer[0x077], 0x02);
     dValue = dValue + (iValue * pow(2, 16)) + data_lsb_get(&buffer[0x075], 0x02);
     DBG(DBG_FNC, "0x0075");
     DBG(DBG_FNC, "   bit[0..35] = timing.cph4p2: %.0f.\n", dValue);
     iValue = data_lsb_get(&buffer[0x079], 0x01);
     DBG(DBG_FNC, "         bits[36..39] = 0x%02x\n", (iValue >> 0x04) & 0x0f);
     DBG(DBG_FNC, "         bit[36] = ?: 0x%02x\n", (iValue >> 0x04) & 0x01);
     DBG(DBG_FNC, "         bit[37] = ?: 0x%02x\n", (iValue >> 0x05) & 0x01);
     DBG(DBG_FNC, "         bit[38] = ?: 0x%02x\n", (iValue >> 0x06) & 0x01);
     DBG(DBG_FNC, "         bit[39] = ?: 0x%02x\n", (iValue >> 0x07) & 0x01);

     iValue = data_lsb_get(&buffer[0x07e], 0x01) & 0x0f;
     dValue = iValue * pow(2, 32);
     iValue = data_lsb_get(&buffer[0x07c], 0x02);
     dValue = dValue + (iValue * pow(2, 16)) + data_lsb_get(&buffer[0x07a], 0x02);
     DBG(DBG_FNC, "\n0x007a Linear Image Sensor Clock 6\n");
     DBG(DBG_FNC, "         bit[0..35] = timing.cph5p1: %.0f.\n", dValue);
     iValue = data_lsb_get(&buffer[0x07e], 0x01);
     DBG(DBG_FNC, "         bit[36] = timing.cph5go: 0x%02x\n", (iValue >> 0x04) & 0x01);
     DBG(DBG_FNC, "         bit[37] = timing.cph5ge: 0x%02x\n", (iValue >> 0x05) & 0x01);
     DBG(DBG_FNC, "         bit[38] = timing.cph5ps: 0x%02x\n", (iValue >> 0x06) & 0x01);
     DBG(DBG_FNC, "         bit[39] = ?: 0x%02x\n", (iValue >> 0x07) & 0x01);

     iValue = data_lsb_get(&buffer[0x083], 0x01) & 0x0f;
     dValue = iValue * pow(2, 32);
     iValue = data_lsb_get(&buffer[0x081], 0x02);
     dValue = dValue + (iValue * pow(2, 16)) + data_lsb_get(&buffer[0x07f], 0x02);
     DBG(DBG_FNC, "0x007f");
     DBG(DBG_FNC, "   bit[0..35] = timing.cph5p2: %.0f.\n", dValue);
     iValue = data_lsb_get(&buffer[0x083], 0x01);
     DBG(DBG_FNC, "         bits[36..39] = 0x%02x\n", (iValue >> 0x04) & 0x0f);
     DBG(DBG_FNC, "         bit[36] = ?: 0x%02x\n", (iValue >> 0x04) & 0x01);
     DBG(DBG_FNC, "         bit[37] = ?: 0x%02x\n", (iValue >> 0x05) & 0x01);
     DBG(DBG_FNC, "         bit[38] = ?: 0x%02x\n", (iValue >> 0x06) & 0x01);
     DBG(DBG_FNC, "         bit[39] = ?: 0x%02x\n", (iValue >> 0x07) & 0x01);

     iValue = data_lsb_get(&buffer[0x084], 3);
     DBG(DBG_FNC, "\n0x0084");
     DBG(DBG_FNC, "   timing.cphbp2s : 0x%06x\n", iValue);

     iValue = data_lsb_get(&buffer[0x087], 3);
     DBG(DBG_FNC, "0x0087");
     DBG(DBG_FNC, "   timing.cphbp2e : 0x%06x\n", iValue);

     iValue = data_lsb_get(&buffer[0x08a], 3);
     DBG(DBG_FNC, "0x008a");
     DBG(DBG_FNC, "   timing.clamps : 0x%08x\n", iValue);

     iValue = data_lsb_get(&buffer[0x08d], 3);
     DBG(DBG_FNC, "0x008d");
     DBG(DBG_FNC, "   timing.clampe or cphbp2e : 0x%08x\n", iValue);

     iValue = data_lsb_get(&buffer[0x092], 0x01);
     DBG(DBG_FNC, "\n0x0092 Correlated-Double-Sample 1\n");
     DBG(DBG_FNC, "         bit[0..5] = timing.cdss[0]: 0x%02x\n", iValue & 0x3f);
     DBG(DBG_FNC, "         bit[6..7] = ?: 0x%02x\n", (iValue >> 6) & 3);

     iValue = data_lsb_get(&buffer[0x093], 0x01);
     DBG(DBG_FNC, "0x0093");
     DBG(DBG_FNC, "   bit[0..5] = timing.cdsc[0]: 0x%02x\n", iValue & 0x3f);
     DBG(DBG_FNC, "         bit[6..7] = ?: 0x%02x\n", (iValue >> 6) & 3);

     iValue = data_lsb_get(&buffer[0x094], 0x01);
     DBG(DBG_FNC, "\n0x0094 Correlated-Double-Sample 2\n");
     DBG(DBG_FNC, "         bit[0..5] = timing.cdss[1]: 0x%02x\n", iValue & 0x3f);
     DBG(DBG_FNC, "         bit[6..7] = ?: 0x%02x\n", (iValue >> 6) & 3);

     iValue = data_lsb_get(&buffer[0x095], 0x01);
     DBG(DBG_FNC, "0x0095");
     DBG(DBG_FNC, "   bit[0..5] = timing.cdsc[1]: 0x%02x\n", iValue & 0x3f);
     DBG(DBG_FNC, "         bit[6..7] = ?: 0x%02x\n", (iValue >> 6) & 3);

     iValue = data_lsb_get(&buffer[0x096], 0x01);
     DBG(DBG_FNC, "0x0096");
     DBG(DBG_FNC, "   bit[0..5] = timing.cnpp: 0x%02x\n", iValue & 0x3f);
     DBG(DBG_FNC, "         bit[6..7] = ?: 0x%02x\n", (iValue >> 6) & 3);

     iValue = data_lsb_get(&buffer[0x09b], 0x01) & 0x0f;
     dValue = iValue * pow(2, 32);
     iValue = data_lsb_get(&buffer[0x099], 0x02);
     dValue = dValue + (iValue * pow(2, 16)) + data_lsb_get(&buffer[0x097], 0x02);
     DBG(DBG_FNC, "\n0x0097 Analog to Digital Converter clock 1\n");
     DBG(DBG_FNC, "         bit[0..35] = timing.adcclkp[0]: %.0f.\n", dValue);
     iValue = data_lsb_get(&buffer[0x09b], 0x01);
     DBG(DBG_FNC, "         bits[36..39] = 0x%02x\n", (iValue >> 0x04) & 0x0f);
     DBG(DBG_FNC, "         bit[36] = ?: 0x%02x\n", (iValue >> 0x04) & 0x01);
     DBG(DBG_FNC, "         bit[37] = ?: 0x%02x\n", (iValue >> 0x05) & 0x01);
     DBG(DBG_FNC, "         bit[38] = ?: 0x%02x\n", (iValue >> 0x06) & 0x01);
     DBG(DBG_FNC, "         bit[39] = ?: 0x%02x\n", (iValue >> 0x07) & 0x01);

     dbg_buffer(DBG_FNC, "\n0x009c CIS sensor 1", &buffer[0x09c], 0x06, 0x9c);
     dbg_buffer(DBG_FNC, "0x00a2 CIS sensor 2", &buffer[0x0a2], 0x06, 0xa2);
     dbg_buffer(DBG_FNC, "0x00a8 CIS sensor 3", &buffer[0x0a8], 0x06, 0xa8);

     iValue = data_lsb_get(&buffer[0x0ae], 0x01);
     DBG(DBG_FNC, "\n0x00ae");
     DBG(DBG_FNC, "   bit[0..5] = ?: 0x%02x\n", iValue & 0x3f);
     DBG(DBG_FNC, "         bit[6..7] = ?: 0x%02x\n", (iValue >> 6) & 3);

     iValue = data_lsb_get(&buffer[0x0af], 0x01);
     DBG(DBG_FNC, "0x00af");
     DBG(DBG_FNC, "   bit[0..2] = ?: 0x%02x\n", iValue & 7);
     DBG(DBG_FNC, "         bit[3..7] = ?: 0x%02x\n", (iValue >> 3) & 0x1f);

     iValue = data_lsb_get(&buffer[0x0b0], 2);
     DBG(DBG_FNC, "\n0x00b0");
     DBG(DBG_FNC, "   Left : 0x%04x\n", iValue);

     iValue = data_lsb_get(&buffer[0x0b2], 2);
     DBG(DBG_FNC, "0x00b2");
     DBG(DBG_FNC, "   Right: 0x%04x\n", iValue);

     dbg_buffer(DBG_FNC, "\n0x00b4", &buffer[0x0b4], 12, 0xb4);

     iValue = data_lsb_get(&buffer[0x0c0], 0x01);
     DBG(DBG_FNC, "\n0x00c0");
     DBG(DBG_FNC, "   bit[0..4] = resolution ratio: 0x%02x\n", iValue & 0x1f);
     DBG(DBG_FNC, "         bit[5..7] = ?: 0x%02x\n", (iValue >> 5) & 7);

     iValue = data_lsb_get(&buffer[0x0c5], 0x01) & 0x0f;
     dValue = iValue * pow(2, 32);
     iValue = data_lsb_get(&buffer[0x0c3], 0x02);
     dValue = dValue + (iValue * pow(2, 16)) + data_lsb_get(&buffer[0x0c1], 2);
     DBG(DBG_FNC, "\n0x00c1 Analog to Digital Converter clock 2\n");
     DBG(DBG_FNC, "         bit[0..35] = timing.adcclkp[1]: %.0f.\n", dValue);
     iValue = data_lsb_get(&buffer[0x0c5], 0x01);
     DBG(DBG_FNC, "         bits[36..39] = 0x%02x\n", (iValue >> 0x04) & 0x0f);
     DBG(DBG_FNC, "         bit[36] = ?: 0x%02x (equal to bit[32])\n", (iValue >> 0x04) & 0x01);
     DBG(DBG_FNC, "         bit[37] = ?: 0x%02x\n", (iValue >> 0x05) & 0x01);
     DBG(DBG_FNC, "         bit[38] = ?: 0x%02x\n", (iValue >> 0x06) & 0x01);
     DBG(DBG_FNC, "         bit[39] = ?: 0x%02x\n", (iValue >> 0x07) & 0x01);

     dbg_buffer(DBG_FNC, "\n0x00c6", &buffer[0x0c6], 0x0a, 0xc6);

     iValue = ((buffer[0x0d4] & 0x0f) << 0x10) + data_lsb_get(&buffer[0x0d0], 0x02);
     DBG(DBG_FNC, "\n0x00d0");
     DBG(DBG_FNC, "   Top : 0x%04x\n", iValue);

     iValue = ((buffer[0x0d4] & 0xf0) << 0x06)+ data_lsb_get(&buffer[0x0d2], 0x02);
     DBG(DBG_FNC, "x00d2");
     DBG(DBG_FNC, "   Down: 0x%04x\n", iValue);

     iValue = _B0(buffer[0x0d5]);
     DBG(DBG_FNC, "0x00d5");
     DBG(DBG_FNC, "   ?: 0x%04x\n", iValue);

     iValue = data_lsb_get(&buffer[0x0d6], 1);
     DBG(DBG_FNC, "\n0x00d6");
     DBG(DBG_FNC, "   bit[0..3]    = ? : 0x%02x\n", iValue & 0xf);
     DBG(DBG_FNC, "         bit[4..7] = dummyline: 0x%02x\n", (iValue >> 4) & 0xf);

     iValue = data_lsb_get(&buffer[0x0d7], 0x01);
     DBG(DBG_FNC, "\n0x00d7");
     DBG(DBG_FNC, "   bit[0..5] = motor pwm frequency: 0x%02x\n", iValue & 0x3f);
     DBG(DBG_FNC, "         bit[6]    = ?: 0x%02x\n", (iValue >> 6) & 1);
     DBG(DBG_FNC, "         bit[7]    = motor type: 0x%02x ", (iValue >> 7) & 1);
     if (((iValue >> 7) & 1) == MT_OUTPUTSTATE)
     DBG(DBG_FNC, ": Output state machine\n");
     else DBG(DBG_FNC, "On-Chip PWM\n");

     iValue = data_lsb_get(&buffer[0x0d8], 0x01);
     DBG(DBG_FNC, "\n0x00d8");
     DBG(DBG_FNC, "   bit[0..5] = ?: 0x%02x\n", iValue & 0x3f);
     DBG(DBG_FNC, "         bit[6]    = scantype (0=Normal|1=TMA) : 0x%02x\n", (iValue >> 6) & 1);
     DBG(DBG_FNC, "         bit[7]    = enable head movement : 0x%02x :", (iValue >> 7) & 1);

     iValue = data_lsb_get(&buffer[0x0d9], 0x01);
     DBG(DBG_FNC, "\n0x00d9");
     DBG(DBG_FNC, "   bit[0..2] = ?: 0x%02x\n", iValue & 7);
     DBG(DBG_FNC, "         bit[3]    = ?: 0x%02x\n", (iValue >> 3) & 1);
     DBG(DBG_FNC, "         bit[4..6] = motor step type: 0x%02x: ", (iValue >> 4) & 7);
     switch((iValue >> 4) & 7)
     {
     case 0:  DBG(DBG_FNC, "full  (1)\n"); break;
     case 1:  DBG(DBG_FNC, "half  (1/2)\n"); break;
     case 2:  DBG(DBG_FNC, "quart (1/4)\n"); break;
     case 3:  DBG(DBG_FNC, "(1/8)\n"); break;
     default: DBG(DBG_FNC, "unknown\n"); break;
     }
     DBG(DBG_FNC, "         bit[7]    = Motor direction: 0x%02x = ", (iValue >> 7) & 1);
     if (((iValue >> 7) & 1) == 0)
     DBG(DBG_FNC, "Backward\n");
     else DBG(DBG_FNC, "Forward\n");

     iValue = data_lsb_get(&buffer[0x0dd], 0x01);
     DBG(DBG_FNC, "\n0x00da");
     DBG(DBG_FNC, "   msi = 0x%03x\n", ((iValue & 3) << 8) + data_lsb_get(&buffer[0x0da], 1));

     DBG(DBG_FNC, "0x00db");
     DBG(DBG_FNC, "   motorbackstep1 = 0x%03x\n", ((iValue & 0x0c) << 6)  + data_lsb_get(&buffer[0x0db], 1));

     DBG(DBG_FNC, "0x00dc");
     DBG(DBG_FNC, "   motorbackstep2 = 0x%03x\n", ((iValue & 0x30) << 4)  + data_lsb_get(&buffer[0x0dc], 1));

     iValue = data_lsb_get(&buffer[0x0dd], 0x01);
     DBG(DBG_FNC, "0x00dd");
     DBG(DBG_FNC, "         bit[7]    = Motor enabled?: 0x%02x = ", (iValue >> 7) & 1);
     if (((iValue >> 7) & 1) == 0)
     DBG(DBG_FNC, "Yes\n");
     else DBG(DBG_FNC, "No\n");

     iValue = data_lsb_get(&buffer[0x0de], 0x02);
     DBG(DBG_FNC, "\n0x00de");
     DBG(DBG_FNC, "   bit[00..11] = ?: 0x%02x\n", iValue & 0xfff);
     DBG(DBG_FNC, "         bit[12..15] = ?: 0x%02x\n", (iValue >> 12) & 0x0f);

     iValue = data_lsb_get(&buffer[0x0df], 0x01);
     DBG(DBG_FNC, "\n0x00df");
     DBG(DBG_FNC, "   bit[0..3] = ?: 0x%02x\n", iValue & 0x0f);
     DBG(DBG_FNC, "         bit[4]    = has_motorcurves?: 0x%02x\n", (iValue >> 4) & 0x01);
     DBG(DBG_FNC, "         bit[5..7] = ?: 0x%02x\n", (iValue >> 5) & 7);

     iValue = data_lsb_get(&buffer[0x0e0], 1);
     DBG(DBG_FNC, "\n0x00e0   step size - 1 : 0x%02x\n", iValue);

     iValue = data_lsb_get(&buffer[0x0e1], 3);
     DBG(DBG_FNC, "\n0x00e1   0x%06x : last step of accurve.normalscan table\n", iValue);

     iValue = data_lsb_get(&buffer[0x0e4], 3);
     DBG(DBG_FNC, "0x00e4   0x%06x : last step of accurve.smearing table\n", iValue);

     iValue = data_lsb_get(&buffer[0x0e7], 3);
     DBG(DBG_FNC, "0x00e7   0x%06x : last step of accurve.parkhome table\n", iValue);

     iValue = data_lsb_get(&buffer[0x0ea], 3);
     DBG(DBG_FNC, "0x00ea   0x%06x : last step of deccurve.scanbufferfull table\n", iValue);

     iValue = data_lsb_get(&buffer[0x0ed], 3);
     DBG(DBG_FNC, "0x00ed   0x%06x : last step of deccurve.normalscan table\n", iValue);

     iValue = data_lsb_get(&buffer[0x0f0], 3);
     DBG(DBG_FNC, "0x00f0   0x%06x : last step of deccurve.smearing table\n", iValue);

     iValue = data_lsb_get(&buffer[0x0f3], 3);
     DBG(DBG_FNC, "0x00f3   0x%06x : last step of deccurve.parkhome table\n", iValue);

     iValue = data_lsb_get(&buffer[0x0f6], 2);
     DBG(DBG_FNC, "\n0x00f6   bit[00..13] = 0x%04x : ptr to accurve.normalscan step table\n", iValue & 0x3fff);
     DBG(DBG_FNC, "         bit[14..15] = 0x%04x : ?\n",(iValue >> 14) & 3);

     iValue = data_lsb_get(&buffer[0x0f8], 2);
     DBG(DBG_FNC, "0x00f8");
     DBG(DBG_FNC, "   bit[00..13] = 0x%04x : ptr to deccurve.scanbufferfull step table\n", iValue & 0x3fff);
     DBG(DBG_FNC, "         bit[14..15] = 0x%04x : ?\n",(iValue >> 14) & 3);

     iValue = data_lsb_get(&buffer[0x0fa], 2);
     DBG(DBG_FNC, "0x00fa");
     DBG(DBG_FNC, "   bit[00..13] = 0x%04x : ptr to accurve.smearing step table\n", iValue & 0x3fff);
     DBG(DBG_FNC, "         bit[14..15] = 0x%04x : ?\n",(iValue >> 14) & 3);

     iValue = data_lsb_get(&buffer[0x0fc], 2);
     DBG(DBG_FNC, "0x00fc");
     DBG(DBG_FNC, "   bit[00..13] = 0x%04x : ptr to deccurve.smearing step table\n", iValue & 0x3fff);
     DBG(DBG_FNC, "         bit[14..15] = 0x%04x : ?\n",(iValue >> 14) & 3);

     iValue = data_lsb_get(&buffer[0x0fe], 2);
     DBG(DBG_FNC, "0x00fe");
     DBG(DBG_FNC, "   bit[00..13] = 0x%04x : ptr to deccurve.normalscan step table\n", iValue & 0x3fff);
     DBG(DBG_FNC, "         bit[14..15] = 0x%04x : ?\n",(iValue >> 14) & 3);

     iValue = data_lsb_get(&buffer[0x100], 2);
     DBG(DBG_FNC, "0x0100");
     DBG(DBG_FNC, "   bit[00..13] = 0x%04x : ptr to accurve.parkhome step table\n", iValue & 0x3fff);
     DBG(DBG_FNC, "         bit[14..15] = 0x%04x : ?\n",(iValue >> 14) & 3);

     iValue = data_lsb_get(&buffer[0x102], 2);
     DBG(DBG_FNC, "0x0102");
     DBG(DBG_FNC, "   bit[00..13] = 0x%04x : ptr to deccurve.parkhome step table\n", iValue & 0x3fff);
     DBG(DBG_FNC, "         bit[14..15] = 0x%04x : ?\n",(iValue >> 14) & 3);

     dbg_buffer(DBG_FNC, "\n0x0104 Motor resource", &buffer[0x104], 0x20, 0x104);

     dbg_buffer(DBG_FNC, "\n0x0124", &buffer[0x124], 0x22, 0x124);

     iValue = data_lsb_get(&buffer[0x146], 1);
     DBG(DBG_FNC, "\n0x0146");
     DBG(DBG_FNC, "   bit[0..3] = Lamp pulse-width modulation frequency : 0x%02x\n", iValue & 0xf);
     DBG(DBG_FNC, "         bit[4]    = timer enabled? : 0x%02x\n", (iValue >> 4) & 1);
     DBG(DBG_FNC, "         bit[5]    = ? : 0x%02x\n", (iValue >> 5) & 1);
     DBG(DBG_FNC, "         bit[6]    = lamp turned on? : 0x%02x\n", (iValue >> 6) & 1);
     DBG(DBG_FNC, "         bit[7]    = sensor type : 0x%02x ", (iValue >> 7) & 1);
     if (((iValue >> 7) & 1) != 0)
     DBG(DBG_FNC, "CCD\n");
     else DBG(DBG_FNC, "CIS\n");

     iValue = data_lsb_get(&buffer[0x147], 1);
     DBG(DBG_FNC, "\n0x0147");
     DBG(DBG_FNC, "   time to turn off lamp =  0x%02x (minutes * 2.682163611980331)\n", iValue);

     iValue = data_lsb_get(&buffer[0x148], 1);
     DBG(DBG_FNC, "\n0x0148");
     DBG(DBG_FNC, "   bit[0..5] = Lamp pulse-width modulation duty cycle : 0x%02x\n", iValue & 0x3f);
     DBG(DBG_FNC, "         bit[6..7] = ? : 0x%02x\n",(iValue >> 6) & 3);

     iValue = data_lsb_get(&buffer[0x149], 1);
     DBG(DBG_FNC, "\n0x0149");
     DBG(DBG_FNC, "   bit[0..5] = even_odd_distance : 0x%02x\n", iValue & 0x3f);
     DBG(DBG_FNC, "         bit[6..7] = ? : 0x%02x\n",(iValue >> 6) & 3);

     iValue = data_lsb_get(&buffer[0x14a], 1);
     DBG(DBG_FNC, "0x014a");
     DBG(DBG_FNC, "   bit[0..5] = sensor line distance : 0x%02x\n", iValue & 0x3f);
     DBG(DBG_FNC, "         bit[6..7] = ?: 0x%02x\n",(iValue >> 6) & 3);

     iValue = data_lsb_get(&buffer[0x14b], 1);
     DBG(DBG_FNC, "0x014b");
     DBG(DBG_FNC, "   bit[0..5] = sensor line distance + even_odd_distance: 0x%02x\n", iValue & 0x3f);
     DBG(DBG_FNC, "         bit[6..7] = ?: 0x%02x\n",(iValue >> 6) & 3);

     iValue = data_lsb_get(&buffer[0x14c], 1);
     DBG(DBG_FNC, "0x014c");
     DBG(DBG_FNC, "   bit[0..5] = sensor line distance * 2: 0x%02x\n", iValue & 0x3f);
     DBG(DBG_FNC, "         bit[6..7] = ?: 0x%02x\n", (iValue >> 6) & 3);

     iValue = data_lsb_get(&buffer[0x14d], 1);
     DBG(DBG_FNC, "0x014d");
     DBG(DBG_FNC, "   bit[0..5] = (sensor line distance * 2) + even_odd_distance: 0x%02x\n", iValue & 0x3f);
     DBG(DBG_FNC, "         bit[6..7] = ?: 0x%02x\n", (iValue >> 6) & 3);

     iValue = data_lsb_get(&buffer[0x14e], 1);
     DBG(DBG_FNC, "\n0x014e");
     DBG(DBG_FNC, "   bit[0..3] = ?: 0x%02x\n", iValue & 0xf);
     DBG(DBG_FNC, "         bit[4]    = ?: 0x%02x\n", (iValue >> 4) & 1);
     DBG(DBG_FNC, "         bit[5..7] = ?: 0x%02x\n", (iValue >> 5) & 7);

     dbg_buffer(DBG_FNC, "\n0x014f", &buffer[0x14f], 0x05, 0x14f);

     iValue = data_lsb_get(&buffer[0x154], 1);
     DBG(DBG_FNC, "\n0x0154");
     DBG(DBG_FNC, "   bit[0..3] = ?: 0x%02x\n", iValue & 0xf);
     DBG(DBG_FNC, "         bit[4..5] = ?: 0x%02x\n", (iValue >> 4) & 3);
     DBG(DBG_FNC, "         bit[6..7] = ?: 0x%02x\n", (iValue >> 6) & 7);

     iValue = data_lsb_get(&buffer[0x155], 1);
     DBG(DBG_FNC, "\n0x0155");
     DBG(DBG_FNC, "   bit[0..3] = ?: 0x%02x\n", iValue & 0x0f);
     DBG(DBG_FNC, "         bit[4]    = 0x%02x : ", (iValue >> 4) & 1);
     if (((iValue >> 4) & 1) == 0)
     DBG(DBG_FNC, "flb lamp\n");
     else DBG(DBG_FNC, "tma lamp\n");
     DBG(DBG_FNC, "         bit[5..7] = ? : 0x%02x\n", (iValue >> 5) & 7);

     dbg_buffer(DBG_FNC, "\n0x0156", &buffer[0x156], 0x02, 0x156);

     iValue = data_lsb_get(&buffer[0x158], 1);
     DBG(DBG_FNC, "\n0x0158");
     DBG(DBG_FNC, "   bit[0..3] = %02x : Scanner buttons ", iValue & 0x0f);
     if ((iValue & 0x0f) == 0x0f)
     DBG(DBG_FNC, "enabled\n");
     else DBG(DBG_FNC, "dissabled\n");
     DBG(DBG_FNC, "         bit[4..7] = ? : 0x%02x\n", (iValue >> 4) & 0x0f);

     dbg_buffer(DBG_FNC, "\n0x0159", &buffer[0x159], 11, 0x159);

     iValue = data_lsb_get(&buffer[0x164], 1);
     DBG(DBG_FNC, "\n0x0164");
     DBG(DBG_FNC, "   bit[0..6] = ?: 0x%02x\n", iValue & 0x3f);
     DBG(DBG_FNC, "         bit[7]    = ? : 0x%02x\n", (iValue >> 7) & 1);

     dbg_buffer(DBG_FNC, "\n0x0165", &buffer[0x165], 3, 0x165);

     iValue = data_lsb_get(&buffer[0x168], 1);
     DBG(DBG_FNC, "\n0x0168 Buttons status : 0x%02x\n", iValue);
     DBG(DBG_FNC, "         bit[0] = button 1 : 0x%02x\n", iValue & 1);
     DBG(DBG_FNC, "         bit[1] = button 2 : 0x%02x\n", (iValue >> 1) & 1);
     DBG(DBG_FNC, "         bit[2] = button 4 : 0x%02x\n", (iValue >> 2) & 1);
     DBG(DBG_FNC, "         bit[3] = button 3 : 0x%02x\n", (iValue >> 3) & 1);
     DBG(DBG_FNC, "         bit[4] = button ? : 0x%02x\n", (iValue >> 4) & 1);
     DBG(DBG_FNC, "         bit[5] = button ? : 0x%02x\n", (iValue >> 5) & 1);
     DBG(DBG_FNC, "         bit[6] = ? : 0x%02x\n", (iValue >> 6) & 1);
     DBG(DBG_FNC, "         bit[7] = ? : 0x%02x\n", (iValue >> 7) & 1);

     iValue = data_lsb_get(&buffer[0x169], 1);
     DBG(DBG_FNC, "\n0x0169", iValue);
     DBG(DBG_FNC, "   bit[0]    = ? : 0x%02x\n", iValue & 1);
     DBG(DBG_FNC, "         bit[1]    = tma attached? : 0x%02x\n", (iValue >> 1) & 1);
     DBG(DBG_FNC, "         bit[2..7] = ? : 0x%02x\n", (iValue >> 2) & 0x3f);

     iValue = data_lsb_get(&buffer[0x16a], 1);
     DBG(DBG_FNC, "\n0x016a Buttons status 2: 0x%02x\n", iValue);
     DBG(DBG_FNC, "         bit[0] = button 1 : 0x%02x\n", iValue & 1);
     DBG(DBG_FNC, "         bit[1] = button 2 : 0x%02x\n", (iValue >> 1) & 1);
     DBG(DBG_FNC, "         bit[2] = button 4 : 0x%02x\n", (iValue >> 2) & 1);
     DBG(DBG_FNC, "         bit[3] = button 3 : 0x%02x\n", (iValue >> 3) & 1);
     DBG(DBG_FNC, "         bit[4] = button ? : 0x%02x\n", (iValue >> 4) & 1);
     DBG(DBG_FNC, "         bit[5] = button ? : 0x%02x\n", (iValue >> 5) & 1);
     DBG(DBG_FNC, "         bit[6] = ? : 0x%02x\n", (iValue >> 6) & 1);
     DBG(DBG_FNC, "         bit[7] = ? : 0x%02x\n", (iValue >> 7) & 1);

     dbg_buffer(DBG_FNC, "\n0x016b", &buffer[0x16b], 4, 0x16b);

     iValue = data_lsb_get(&buffer[0x16f], 1);
     DBG(DBG_FNC, "\n0x016f");
     DBG(DBG_FNC, "   bit[0..5] = ? : 0x%02x\n", iValue & 0x3f);
     DBG(DBG_FNC, "         bit[6] = is lamp at home? : 0x%02x\n", (iValue >> 6) & 1);
     DBG(DBG_FNC, "         bit[7] = ?: %02x\n", (iValue >> 7) & 1);

     dbg_buffer(DBG_FNC, "\n0x0170", &buffer[0x170], 0x17, 0x170);

     iValue = data_lsb_get(&buffer[0x187], 1);
     DBG(DBG_FNC, "\n0x0187");
     DBG(DBG_FNC, "   bit[0..3] = ? : 0x%02x\n", iValue & 0xf);
     DBG(DBG_FNC, "         bit[4..7] = mclkioc : 0x%02x\n", (iValue >> 4) & 0xf);

     dbg_buffer(DBG_FNC, "\n0x0188", &buffer[0x188], 0x16, 0x188);

     iValue = data_lsb_get(&buffer[0x19e], 2);
     DBG(DBG_FNC, "\n0x019e");
     DBG(DBG_FNC, "   binary threshold low : 0x%04x\n", (iValue >> 8) + ((iValue << 8) & 0xff00));

     iValue = data_lsb_get(&buffer[0x1a0], 2);
     DBG(DBG_FNC, "\n0x01a0");
     DBG(DBG_FNC, "   binary threshold high : 0x%04x\n", (iValue >> 8) + ((iValue << 8) & 0xff00));

     dbg_buffer(DBG_FNC, "\n0x01a2", &buffer[0x1a2], 0x12, 0x1a2);

     iValue = data_lsb_get(&buffer[0x1b4], 2);
     DBG(DBG_FNC, "\n0x01b4");
     DBG(DBG_FNC, "   bit[00..13] = Ptr to red gamma table (table_size * 0) : 0x%04x\n", (iValue & 0x3fff));
     DBG(DBG_FNC, "         bit[14..15] = ? : 0x%02x\n", (iValue >> 14) & 3);

     iValue = data_lsb_get(&buffer[0x1b6], 2);
     DBG(DBG_FNC, "0x01b6");
     DBG(DBG_FNC, "   bit[00..13] = Ptr to green gamma table (table_size * 1) : 0x%04x\n", (iValue & 0x3fff));
     DBG(DBG_FNC, "         bit[14..15] = ? : 0x%02x\n", (iValue >> 14) & 3);

     iValue = data_lsb_get(&buffer[0x1b8], 2);
     DBG(DBG_FNC, "0x01b8");
     DBG(DBG_FNC, "   bit[00..13] = Ptr to blue gamma table (table_size * 2) : 0x%04x\n", (iValue & 0x3fff));
     DBG(DBG_FNC, "         bit[14..15] = ? : 0x%02x\n", (iValue >> 14) & 3);

     iValue = data_lsb_get(&buffer[0x1ba], 1);
     DBG(DBG_FNC, "\n0x01ba");
     DBG(DBG_FNC, "   ? : 0x%02x\n", iValue);

     iValue = data_lsb_get(&buffer[0x1bb], 2);
     DBG(DBG_FNC, "0x01bb");
     DBG(DBG_FNC, "   ? : 0x%04x\n", iValue + ((data_lsb_get(&buffer[0x1bf], 1) & 1) << 16));

     iValue = data_lsb_get(&buffer[0x1bd], 2);
     DBG(DBG_FNC, "0x01bd");
     DBG(DBG_FNC, "   ? : 0x%04x\n", iValue + (((data_lsb_get(&buffer[0x1bf], 1) >> 1) & 3) << 16));

     iValue = data_lsb_get(&buffer[0x1c0], 3);
     DBG(DBG_FNC, "0x01c0");
     DBG(DBG_FNC, "   bit[0..19] = ? : 0x%06x\n", iValue  & 0xfffff);

     iValue = data_lsb_get(&buffer[0x1bf], 2);
     DBG(DBG_FNC, "\n0x01bf");
     DBG(DBG_FNC, "   bit[3..4] = ? : 0x%02x\n", (iValue >> 3) & 3);
     DBG(DBG_FNC, "         bit[5..7] = ? : 0x%02x\n", (iValue >> 5) & 7);

     iValue = data_lsb_get(&buffer[0x1c2], 3);
     DBG(DBG_FNC, "\n0x01c2");
     DBG(DBG_FNC, "   bit[4..23] = ? : 0x%06x\n", ((iValue >> 8) & 0xffff) + (((iValue >> 4) & 0xf) << 16));

     iValue = data_lsb_get(&buffer[0x1c5], 3);
     DBG(DBG_FNC, "0x01c5");
     DBG(DBG_FNC, "   bit[00..19] = ? : 0x%06x\n", iValue  & 0xfffff);
     DBG(DBG_FNC, "         bit[20..23] = ? : 0x%02x\n", (iValue >> 20)  & 0xf);

     dbg_buffer(DBG_FNC, "\n0x01c8", &buffer[0x1c8], 7, 0x1c8);

     iValue = data_lsb_get(&buffer[0x1cf], 3);
     DBG(DBG_FNC, "\n0x01cf");
     DBG(DBG_FNC, "   bit[0] = ? : 0x%02x\n", iValue  & 1);
     DBG(DBG_FNC, "         bit[1]    = shading base (0 = 0x4000|1= 0x2000) : 0x%02x\n", (iValue >> 1) & 1);
     DBG(DBG_FNC, "         bit[2]    = white shading correction : 0x%02x\n", (iValue >> 2) & 1);
     DBG(DBG_FNC, "         bit[3]    = black shading correction : 0x%02x\n", (iValue >> 3) & 1);
     DBG(DBG_FNC, "         bit[4..5] = 0x%02x : ", (iValue >> 4) & 3);
     switch ((iValue >> 4) & 3)
     {
     case 0: DBG(DBG_FNC, "8 bits per channel"); break;
     case 1: DBG(DBG_FNC, "12 bits per channel"); break;
     case 2: DBG(DBG_FNC, "16 bits per channel"); break;
     case 3: DBG(DBG_FNC, "lineart mode"); break;
     }
     DBG(DBG_FNC, "\n");
     DBG(DBG_FNC, "         bit[6]    = samplerate: 0x%02x ", (iValue >> 6) & 1);
     if (((iValue >> 6) & 1) == PIXEL_RATE)
     DBG(DBG_FNC, "PIXEL_RATE\n");
     else DBG(DBG_FNC, "LINE_RATE\n");
     DBG(DBG_FNC, "         bit[7]    = ? : 0x%02x\n", (iValue >> 7) & 1);

     iValue = data_lsb_get(&buffer[0x1d0], 1);
     DBG(DBG_FNC, "\n0x01d0");
     DBG(DBG_FNC, "   bit[0]    = 0x%02x\n", iValue  & 1);
     DBG(DBG_FNC, "         bit[1]    = 0x%02x\n", (iValue >> 1)  & 1);
     DBG(DBG_FNC, "         bit[2..3] = gamma table size : 0x%02x ", (iValue >> 2) & 3);
     switch ((iValue >> 2)  & 3)
     {
     case 0: DBG(DBG_FNC, "bit[0] + 0x100") ;break;
     case 1: DBG(DBG_FNC, "bit[0] + 0x400") ;break;
     case 2: DBG(DBG_FNC, "bit[0] + 0x1000") ;break;
     }
     DBG(DBG_FNC, "\n");
     DBG(DBG_FNC, "         bit[4..5] = ? : 0x%02x\n", (iValue >> 4) & 3);
     DBG(DBG_FNC, "         bit[6]    = use gamma tables? : 0x%02x\n", (iValue >> 6) & 1);
     DBG(DBG_FNC, "         bit[7]    = ? : 0x%02x\n", (iValue >> 7) & 1);

     dbg_buffer(DBG_FNC, "\n0x01d1", &buffer[0x1d1], 0x430, 0x1d1);

     DBG(DBG_FNC, "----------------------------------------------------\n\n");
   */
  /*exit(0); */
}
#endif
