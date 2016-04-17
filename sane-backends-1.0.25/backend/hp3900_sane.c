/* HP Scanjet 3900 series - SANE Backend controller
   Copyright (C) 2005-2009 Jonathan Bravo Lopez <jkdsoft@gmail.com>

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

/* Backend Code for SANE*/
#define HP3900_CONFIG_FILE "hp3900.conf"
#define GAMMA_DEFAULT 1.0

#include "../include/sane/config.h"
#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_config.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_usb.h"
#include "../include/sane/sanei_debug.h"

#include "hp3900_rts8822.c"

struct st_convert
{
  SANE_Int colormode;
  SANE_Int depth;
  SANE_Int threshold;
  SANE_Int negative;
  SANE_Int real_depth;
};

/* options enumerator */
typedef enum
{
  opt_begin = 0,

  grp_geometry,
  opt_tlx, opt_tly, opt_brx, opt_bry,
  opt_resolution,

  /* gamma tables */
  opt_gamma_red,
  opt_gamma_green,
  opt_gamma_blue,

  opt_scantype,
  opt_colormode,
  opt_depth,
  opt_threshold,

  /* debugging options */
  grp_debug,
  opt_model,
  opt_negative,
  opt_nogamma,
  opt_nowshading,
  opt_realdepth,
  opt_emulategray,
  opt_nowarmup,
  opt_dbgimages,
  opt_reset,

  /* device information */
  grp_info,
  opt_chipname,
  opt_chipid,
  opt_scancount,
  opt_infoupdate,

  /* supported buttons. RTS8822 supports up to 6 buttons */
  grp_sensors,
  opt_button_0,
  opt_button_1,
  opt_button_2,
  opt_button_3,
  opt_button_4,
  opt_button_5,

  opt_count
} EOptionIndex;

/* linked list of SANE_Device structures */
typedef struct TDevListEntry
{
  struct TDevListEntry *pNext;
  SANE_Device dev;
  char *devname;
} TDevListEntry;

typedef struct
{
  char *pszVendor;
  char *pszName;
} TScannerModel;

typedef union
{
  SANE_Word w;
  SANE_Word *wa;		/* word array */
  SANE_String s;
} TOptionValue;

typedef struct
{
  SANE_Int model;
  SANE_Option_Descriptor aOptions[opt_count];
  TOptionValue aValues[opt_count];
  struct params ScanParams;

  /* lists */
  SANE_String_Const *list_colormodes;
  SANE_Int *list_depths;
  SANE_String_Const *list_models;
  SANE_Int *list_resolutions;
  SANE_String_Const *list_sources;

  SANE_Word *aGammaTable[3];	/* a 16-to-16 bit color lookup table */
  SANE_Range rng_gamma;

  /* reading image */
  SANE_Byte *image;
  SANE_Byte *rest;
  SANE_Int rest_amount;
  SANE_Int mylin;

  /* convertion settings */
  struct st_convert cnv;

  /* ranges */
  SANE_Range rng_threshold;
  SANE_Range rng_horizontal;
  SANE_Range rng_vertical;

  SANE_Int scan_count;
  SANE_Int fScanning;		/* TRUE if actively scanning */
} TScanner;

/* functions to manage backend's options */
static void options_init (TScanner * scanner);
static void options_free (TScanner * scanner);

/* devices listing */
static SANE_Int _ReportDevice (TScannerModel * pModel,
			       const char *pszDeviceName);
static SANE_Status attach_one_device (SANE_String_Const devname);

/* capabilities */
static SANE_Status bknd_colormodes (TScanner * scanner, SANE_Int model);
static void bknd_constrains (TScanner * scanner, SANE_Int source,
			     SANE_Int type);
static SANE_Status bknd_depths (TScanner * scanner, SANE_Int model);
static SANE_Status bknd_info (TScanner * scanner);
static SANE_Status bknd_models (TScanner * scanner);
static SANE_Status bknd_resolutions (TScanner * scanner, SANE_Int model);
static SANE_Status bknd_sources (TScanner * scanner, SANE_Int model);

/* convertions */
static void Color_Negative (SANE_Byte * buffer, SANE_Int size,
			    SANE_Int depth);
static void Color_to_Gray (SANE_Byte * buffer, SANE_Int size, SANE_Int depth);
static void Gray_to_Lineart (SANE_Byte * buffer, SANE_Int size,
			     SANE_Int threshold);
static void Depth_16_to_8 (SANE_Byte * from_buffer, SANE_Int size,
			   SANE_Byte * to_buffer);

/* gamma functions */
static void gamma_apply (TScanner * s, SANE_Byte * buffer, SANE_Int size,
			 SANE_Int depth);
static SANE_Int gamma_create (TScanner * s, double gamma);
static void gamma_free (TScanner * s);

static SANE_Int Get_Colormode (SANE_String colormode);
static SANE_Int Get_Model (SANE_String model);
static SANE_Int Get_Source (SANE_String source);
static SANE_Int GetUSB_device_model (SANE_String_Const name);
static size_t max_string_size (const SANE_String_Const strings[]);

static SANE_Status get_button_status (TScanner * s);

/* reading buffers */
static SANE_Status img_buffers_alloc (TScanner * scanner, SANE_Int size);
static SANE_Status img_buffers_free (TScanner * scanner);

static SANE_Status option_get (TScanner * scanner, SANE_Int optid,
			       void *result);
static SANE_Status option_set (TScanner * scanner, SANE_Int optid,
			       void *value, SANE_Int * pInfo);

static void Set_Coordinates (SANE_Int scantype, SANE_Int resolution,
			     struct st_coords *coords);
static SANE_Int set_ScannerModel (SANE_Int proposed, SANE_Int product,
				  SANE_Int vendor);
static void Silent_Compile (void);
static SANE_Status Translate_coords (struct st_coords *coords);

/* SANE functions */
void sane_cancel (SANE_Handle h);
void sane_close (SANE_Handle h);
SANE_Status sane_control_option (SANE_Handle h, SANE_Int n,
				 SANE_Action Action, void *pVal,
				 SANE_Int * pInfo);
void sane_exit (void);
SANE_Status sane_get_devices (const SANE_Device *** device_list,
			      SANE_Bool local_only);
const SANE_Option_Descriptor *sane_get_option_descriptor (SANE_Handle h,
							  SANE_Int n);
SANE_Status sane_get_parameters (SANE_Handle h, SANE_Parameters * p);
SANE_Status sane_get_select_fd (SANE_Handle handle, SANE_Int * fd);
SANE_Status sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize);
SANE_Status sane_open (SANE_String_Const name, SANE_Handle * h);
SANE_Status sane_read (SANE_Handle h, SANE_Byte * buf, SANE_Int maxlen,
		       SANE_Int * len);
SANE_Status sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking);
SANE_Status sane_start (SANE_Handle h);

/* variables */
static struct st_device *device = NULL;
static TDevListEntry *_pFirstSaneDev = 0;
static SANE_Int iNumSaneDev = 0;
static const SANE_Device **_pSaneDevList = 0;

/* Own functions */

static SANE_Status
bknd_resolutions (TScanner * scanner, SANE_Int model)
{
  SANE_Status rst = SANE_STATUS_INVAL;

  DBG (DBG_FNC, "> bknd_resolutions(*scanner, model=%i)\n", model);

  if (scanner != NULL)
    {
      SANE_Int *res = NULL;

      switch (model)
	{
	case BQ5550:
	case UA4900:
	  {
	    SANE_Int myres[] = { 8, 50, 75, 100, 150, 200, 300, 600, 1200 };

	    res = (SANE_Int *) malloc (sizeof (myres));
	    if (res != NULL)
	      memcpy (res, &myres, sizeof (myres));
	  }
	  break;

	case HPG2710:
	case HP3800:
	  {
	    /* 1200 and 2400 dpi are disabled until problems are solved */
	    SANE_Int myres[] = { 7, 50, 75, 100, 150, 200, 300, 600 };

	    res = (SANE_Int *) malloc (sizeof (myres));
	    if (res != NULL)
	      memcpy (res, &myres, sizeof (myres));
	  }
	  break;

	case HP4370:
	case HPG3010:
	case HPG3110:
	  {
	    SANE_Int myres[] =
	      { 10, 50, 75, 100, 150, 200, 300, 600, 1200, 2400, 4800 };

	    res = (SANE_Int *) malloc (sizeof (myres));
	    if (res != NULL)
	      memcpy (res, &myres, sizeof (myres));
	  }
	  break;

	default:		/* HP3970 & HP4070 & UA4900 */
	  {
	    SANE_Int myres[] =
	      { 9, 50, 75, 100, 150, 200, 300, 600, 1200, 2400 };

	    res = (SANE_Int *) malloc (sizeof (myres));
	    if (res != NULL)
	      memcpy (res, &myres, sizeof (myres));
	  }
	  break;
	}

      if (res != NULL)
	{
	  if (scanner->list_resolutions != NULL)
	    free (scanner->list_resolutions);

	  scanner->list_resolutions = res;
	  rst = SANE_STATUS_GOOD;
	}
    }

  return rst;
}

static SANE_Status
bknd_models (TScanner * scanner)
{
  SANE_Status rst = SANE_STATUS_INVAL;

  DBG (DBG_FNC, "> bknd_models:\n");

  if (scanner != NULL)
    {
      SANE_String_Const *model = NULL;

      /* at this moment all devices use the same list */
      SANE_String_Const mymodel[] =
	{ "HP3800", "HP3970", "HP4070", "HP4370", "UA4900", "HPG3010",
"BQ5550", "HPG2710", "HPG3110", 0 };

      /* allocate space to save list */
      model = (SANE_String_Const *) malloc (sizeof (mymodel));
      if (model != NULL)
	memcpy (model, &mymodel, sizeof (mymodel));

      if (model != NULL)
	{
	  /* free previous list */
	  if (scanner->list_models != NULL)
	    free (scanner->list_models);

	  /* set new list */
	  scanner->list_models = model;
	  rst = SANE_STATUS_GOOD;
	}
    }

  return rst;
}

static SANE_Status
bknd_colormodes (TScanner * scanner, SANE_Int model)
{
  SANE_Status rst = SANE_STATUS_INVAL;

  DBG (DBG_FNC, "> bknd_colormodes(*scanner, model=%i)\n", model);

  if (scanner != NULL)
    {
      SANE_String_Const *colormode = NULL;

      /* at this moment all devices use the same list */
      SANE_String_Const mycolormode[] =
	{ SANE_VALUE_SCAN_MODE_COLOR, SANE_VALUE_SCAN_MODE_GRAY, SANE_VALUE_SCAN_MODE_LINEART, 0 };

      /* silence gcc */
      model = model;

      colormode = (SANE_String_Const *) malloc (sizeof (mycolormode));
      if (colormode != NULL)
	memcpy (colormode, &mycolormode, sizeof (mycolormode));

      if (colormode != NULL)
	{
	  if (scanner->list_colormodes != NULL)
	    free (scanner->list_colormodes);

	  scanner->list_colormodes = colormode;
	  rst = SANE_STATUS_GOOD;
	}
    }

  return rst;
}

static SANE_Status
bknd_sources (TScanner * scanner, SANE_Int model)
{
  SANE_Status rst = SANE_STATUS_INVAL;

  DBG (DBG_FNC, "> bknd_sources(*scanner, model=%i)\n", model);

  if (scanner != NULL)
    {
      SANE_String_Const *source = NULL;

      switch (model)
	{
	case UA4900:
	  {
	    SANE_String_Const mysource[] = { SANE_I18N ("Flatbed"), 0 };
	    source = (SANE_String_Const *) malloc (sizeof (mysource));
	    if (source != NULL)
	      memcpy (source, &mysource, sizeof (mysource));
	  }
	  break;
	default:		/* hp3970, hp4070, hp4370 and others */
	  {
	    SANE_String_Const mysource[] =
	      { SANE_I18N ("Flatbed"), SANE_I18N ("Slide"),
SANE_I18N ("Negative"), 0 };
	    source = (SANE_String_Const *) malloc (sizeof (mysource));
	    if (source != NULL)
	      memcpy (source, &mysource, sizeof (mysource));
	  }
	  break;
	}

      if (source != NULL)
	{
	  if (scanner->list_sources != NULL)
	    free (scanner->list_sources);

	  scanner->list_sources = source;
	  rst = SANE_STATUS_GOOD;
	}
    }

  return rst;
}

static SANE_Status
bknd_depths (TScanner * scanner, SANE_Int model)
{
  SANE_Status rst = SANE_STATUS_INVAL;

  DBG (DBG_FNC, "> bknd_depths(*scanner, model=%i\n", model);

  if (scanner != NULL)
    {
      SANE_Int *depth = NULL;

      /* at this moment all devices use the same list */
      SANE_Int mydepth[] = { 2, 8, 16 };	/*{3, 8, 12, 16}; */

      /* silence gcc */
      model = model;

      depth = (SANE_Int *) malloc (sizeof (mydepth));
      if (depth != NULL)
	memcpy (depth, &mydepth, sizeof (mydepth));

      if (depth != NULL)
	{
	  if (scanner->list_depths != NULL)
	    free (scanner->list_depths);

	  scanner->list_depths = depth;
	  rst = SANE_STATUS_GOOD;
	}
    }

  return rst;
}

static SANE_Status
bknd_info (TScanner * scanner)
{
  SANE_Status rst = SANE_STATUS_INVAL;

  DBG (DBG_FNC, "> bknd_info(*scanner)");

  if (scanner != NULL)
    {
      char data[256];

      /* update chipset name */
      Chipset_Name (device, data, 255);
      if (scanner->aValues[opt_chipname].s != NULL)
	{
	  free (scanner->aValues[opt_chipname].s);
	  scanner->aValues[opt_chipname].s = NULL;
	}

      scanner->aValues[opt_chipname].s = strdup (data);
      scanner->aOptions[opt_chipname].size = strlen (data) + 1;

      /* update chipset id */
      scanner->aValues[opt_chipid].w = Chipset_ID (device);

      /* update scans counter */
      scanner->aValues[opt_scancount].w = RTS_ScanCounter_Get (device);

      rst = SANE_STATUS_GOOD;
    }

  return rst;
}

static SANE_Int
GetUSB_device_model (SANE_String_Const name)
{
  SANE_Int usbid, model;

  /* default model is unknown */
  model = -1;

  /* open usb device */
  if (sanei_usb_open (name, &usbid) == SANE_STATUS_GOOD)
    {
      SANE_Int vendor, product;

      if (sanei_usb_get_vendor_product (usbid, &vendor, &product) ==
	  SANE_STATUS_GOOD)
	model = Device_get (product, vendor);

      sanei_usb_close (usbid);
    }

  return model;
}

static void
Silent_Compile (void)
{
  /*
     There are some functions in hp3900_rts8822.c that aren't used yet.
     To avoid compilation warnings we will use them here
   */

  SANE_Byte a = 1;

  if (a == 0)
    {
      Buttons_Status (device);
      Calib_WriteTable (device, NULL, 0, 0);
      Gamma_GetTables (device, NULL);
    }
}

static void
bknd_constrains (TScanner * scanner, SANE_Int source, SANE_Int type)
{
  struct st_coords *coords = Constrains_Get (device, source);

  if ((coords != NULL) && (scanner != NULL))
    {
      switch (type)
	{
	case 1:		/* Y */
	  scanner->rng_vertical.max = coords->height;
	  break;
	default:		/* X */
	  scanner->rng_horizontal.max = coords->width;
	  break;
	}
    }
}

static SANE_Status
img_buffers_free (TScanner * scanner)
{
  if (scanner != NULL)
    {
      if (scanner->image != NULL)
	{
	  free (scanner->image);
	  scanner->image = NULL;
	}

      if (scanner->rest != NULL)
	{
	  free (scanner->rest);
	  scanner->rest = NULL;
	}

      scanner->rest_amount = 0;
    }

  return SANE_STATUS_GOOD;
}

static SANE_Status
img_buffers_alloc (TScanner * scanner, SANE_Int size)
{
  SANE_Status rst;

  /* default result at this point */
  rst = SANE_STATUS_INVAL;

  if (scanner != NULL)
    {
      /* default result at this point */
      rst = SANE_STATUS_NO_MEM;

      /* free previous allocs */
      img_buffers_free (scanner);

      scanner->image = (SANE_Byte *) malloc (size * sizeof (SANE_Byte));
      if (scanner->image != NULL)
	{
	  scanner->rest = (SANE_Byte *) malloc (size * sizeof (SANE_Byte));
	  if (scanner->rest != NULL)
	    rst = SANE_STATUS_GOOD;	/* ok !! */
	}

      if (rst != SANE_STATUS_GOOD)
	img_buffers_free (scanner);
    }

  return rst;
}

static SANE_Int
set_ScannerModel (SANE_Int proposed, SANE_Int product, SANE_Int vendor)
{
  /* This function will set the device behaviour */

  SANE_Int current = Device_get (product, vendor);
  char *sdevname[10] =
    { "Unknown", "HP3970", "HP4070", "HP4370", "UA4900", "HP3800", "HPG3010",
"BQ5550", "HPG2710", "HPG3110" };

  DBG (DBG_FNC,
       "> set_ScannerModel(proposed=%i, product=%04x, vendor=%04x)\n",
       proposed, product, vendor);

  if (proposed < 0)
    {
      if ((current < 0) || (current >= DEVSCOUNT))
	{
	  DBG (DBG_VRB, " -> Unknown device. Defaulting to HP3970...\n");
	  RTS_Debug->dev_model = HP3970;
	}
      else
	{
	  RTS_Debug->dev_model = current;
	  DBG (DBG_VRB, " -> Device model is %s\n", sdevname[current + 1]);
	}
    }
  else
    {
      if (proposed < DEVSCOUNT)
	{
	  RTS_Debug->dev_model = proposed;
	  DBG (DBG_VRB, " -> Device %s ,  treating as %s ...\n",
	       sdevname[current + 1], sdevname[proposed + 1]);
	}
      else
	{
	  if ((current >= 0) && (current < DEVSCOUNT))
	    {
	      RTS_Debug->dev_model = current;
	      DBG (DBG_VRB,
		   " -> Device not supported. Defaulting to %s ...\n",
		   sdevname[current + 1]);
	    }
	  else
	    {
	      RTS_Debug->dev_model = HP3970;
	      DBG (DBG_VRB,
		   "-> Device not supported. Defaulting to HP3970...\n");
	    }
	}
    }

  return OK;
}

static void
Set_Coordinates (SANE_Int scantype, SANE_Int resolution,
		 struct st_coords *coords)
{
  struct st_coords *limits = Constrains_Get (device, scantype);

  DBG (DBG_FNC, "> Set_Coordinates(res=%i, *coords):\n", resolution);

  if (coords->left == -1)
    coords->left = 0;

  if (coords->width == -1)
    coords->width = limits->width;

  if (coords->top == -1)
    coords->top = 0;

  if (coords->height == -1)
    coords->height = limits->height;

  DBG (DBG_FNC, " -> Coords [MM] : xy(%i, %i) wh(%i, %i)\n", coords->left,
       coords->top, coords->width, coords->height);

  coords->left = MM_TO_PIXEL (coords->left, resolution);
  coords->width = MM_TO_PIXEL (coords->width, resolution);
  coords->top = MM_TO_PIXEL (coords->top, resolution);
  coords->height = MM_TO_PIXEL (coords->height, resolution);

  DBG (DBG_FNC, " -> Coords [px] : xy(%i, %i) wh(%i, %i)\n", coords->left,
       coords->top, coords->width, coords->height);

  Constrains_Check (device, resolution, scantype, coords);

  DBG (DBG_FNC, " -> Coords [check]: xy(%i, %i) wh(%i, %i)\n", coords->left,
       coords->top, coords->width, coords->height);
}

static void
Color_Negative (SANE_Byte * buffer, SANE_Int size, SANE_Int depth)
{
  if (buffer != NULL)
    {
      SANE_Int a;
      SANE_Int max_value = (1 << depth) - 1;

      if (depth > 8)
	{
	  USHORT *sColor = (void *) buffer;
	  for (a = 0; a < size / 2; a++)
	    {
	      *sColor = max_value - *sColor;
	      sColor++;
	    }
	}
      else
	{
	  for (a = 0; a < size; a++)
	    *(buffer + a) = max_value - *(buffer + a);
	}
    }
}

static SANE_Status
get_button_status (TScanner * s)
{
  if (s != NULL)
    {
      SANE_Int a, b, status, btn;

      b = 1;
      status = Buttons_Released (device) & 63;
      for (a = 0; a < 6; a++)
	{
	  if ((status & b) != 0)
	    {
	      btn = Buttons_Order (device, b);
	      if (btn != -1)
		s->aValues[opt_button_0 + btn].w = SANE_TRUE;
	    }

	  b <<= 1;
	}
    }

  return SANE_STATUS_GOOD;
}

static void
Depth_16_to_8 (SANE_Byte * from_buffer, SANE_Int size, SANE_Byte * to_buffer)
{
  if ((from_buffer != NULL) && (to_buffer != NULL))
    {
      SANE_Int a, b;

      a = 1;
      b = 0;

      while (a < size)
	{
	  *(to_buffer + b) = *(from_buffer + a);
	  a += 2;
	  b++;
	}
    }
}

static void
Gray_to_Lineart (SANE_Byte * buffer, SANE_Int size, SANE_Int threshold)
{
  /* code provided by tobias leutwein */

  if (buffer != NULL)
    {
      SANE_Byte toBufferByte;
      SANE_Int fromBufferPos_i = 0;
      SANE_Int toBufferPos_i = 0;
      SANE_Int bitPos_i;

      while (fromBufferPos_i < size)
	{
	  toBufferByte = 0;

	  for (bitPos_i = 7; bitPos_i != (-1); bitPos_i--)
	    {
	      if ((fromBufferPos_i < size)
		  && (buffer[fromBufferPos_i] < threshold))
		toBufferByte |= (1u << bitPos_i);

	      fromBufferPos_i++;
	    }

	  buffer[toBufferPos_i] = toBufferByte;
	  toBufferPos_i++;
	}
    }
}

static void
Color_to_Gray (SANE_Byte * buffer, SANE_Int size, SANE_Int depth)
{
  /* converts 3 color channel into 1 gray channel of specified bit depth */

  if (buffer != NULL)
    {
      SANE_Int c, chn, chn_size;
      SANE_Byte *ptr_src = NULL;
      SANE_Byte *ptr_dst = NULL;
      float data, chn_data;
      float coef[3] = { 0.299, 0.587, 0.114 };	/* coefficients per channel */

      chn_size = (depth > 8) ? 2 : 1;
      ptr_src = (void *) buffer;
      ptr_dst = (void *) buffer;

      for (c = 0; c < size / (3 * chn_size); c++)
	{
	  data = 0.;

	  /* get, apply coeffs and sum channels */
	  for (chn = 0; chn < 3; chn++)
	    {
	      chn_data = data_lsb_get (ptr_src + (chn * chn_size), chn_size);
	      data += (chn_data * coef[chn]);
	    }

	  /* save result */
	  data_lsb_set (ptr_dst, (SANE_Int) data, chn_size);

	  ptr_src += 3 * chn_size;	/* next triplet */
	  ptr_dst += chn_size;
	}
    }
}

static void
gamma_free (TScanner * s)
{
  DBG (DBG_FNC, "> gamma_free()\n");

  if (s != NULL)
    {
      /* Destroy gamma tables */
      SANE_Int a;

      for (a = CL_RED; a <= CL_BLUE; a++)
	{
	  if (s->aGammaTable[a] != NULL)
	    {
	      free (s->aGammaTable[a]);
	      s->aGammaTable[a] = NULL;
	    }
	}
    }
}

static SANE_Int
gamma_create (TScanner * s, double gamma)
{
  SANE_Int rst = ERROR;		/* by default */

  DBG (DBG_FNC, "> gamma_create(*s)\n");

  if (s != NULL)
    {
      SANE_Int a;
      double value, c;

      /* default result */
      rst = OK;

      /* destroy previus gamma tables */
      gamma_free (s);

      /* check gamma value */
      if (gamma < 0)
	gamma = GAMMA_DEFAULT;

      /* allocate space for 16 bit gamma tables */
      for (a = CL_RED; a <= CL_BLUE; a++)
	{
	  s->aGammaTable[a] = malloc (65536 * sizeof (SANE_Word));
	  if (s->aGammaTable[a] == NULL)
	    {
	      rst = ERROR;
	      break;
	    }
	}

      if (rst == OK)
	{
	  /* fill tables */
	  for (a = 0; a < 65536; a++)
	    {
	      value = (a / (65536. - 1));
	      value = pow (value, (1. / gamma));
	      value = value * (65536. - 1);

	      c = (SANE_Int) value;
	      if (c > (65536. - 1))
		c = (65536. - 1);
	      else if (c < 0)
		c = 0;

	      s->aGammaTable[CL_RED][a] = c;
	      s->aGammaTable[CL_GREEN][a] = c;
	      s->aGammaTable[CL_BLUE][a] = c;
	    }
	}
      else
	gamma_free (s);
    }

  return rst;
}

static void
gamma_apply (TScanner * s, SANE_Byte * buffer, SANE_Int size, SANE_Int depth)
{
  if ((s != NULL) && (buffer != NULL))
    {
      SANE_Int c;
      SANE_Int dot_size = 3 * ((depth > 8) ? 2 : 1);
      SANE_Byte *pColor = buffer;
      USHORT *sColor = (void *) buffer;

      if ((s->aGammaTable[CL_RED] != NULL)
	  && (s->aGammaTable[CL_GREEN] != NULL)
	  && (s->aGammaTable[CL_BLUE] != NULL))
	{
	  for (c = 0; c < size / dot_size; c++)
	    {
	      if (depth > 8)
		{
		  *sColor = s->aGammaTable[CL_RED][*sColor];
		  *(sColor + 1) = s->aGammaTable[CL_GREEN][*(sColor + 1)];
		  *(sColor + 2) = s->aGammaTable[CL_BLUE][*(sColor + 2)];
		  sColor += 3;
		}
	      else
		{
		  /* 8 bits gamma */
		  *pColor =
		    (s->aGammaTable[CL_RED][*pColor * 256] >> 8) & 0xff;
		  *(pColor + 1) =
		    (s->
		     aGammaTable[CL_GREEN][*(pColor + 1) * 256] >> 8) & 0xff;
		  *(pColor + 2) =
		    (s->
		     aGammaTable[CL_BLUE][*(pColor + 2) * 256] >> 8) & 0xff;
		  pColor += 3;
		}
	    }
	}
    }
}

static SANE_Int
Get_Model (SANE_String model)
{
  SANE_Int rst;

  if (strcmp (model, "HP3800") == 0)
    rst = HP3800;
  else if (strcmp (model, "HPG2710") == 0)
    rst = HPG2710;
  else if (strcmp (model, "HP3970") == 0)
    rst = HP3970;
  else if (strcmp (model, "HP4070") == 0)
    rst = HP4070;
  else if (strcmp (model, "HP4370") == 0)
    rst = HP4370;
  else if (strcmp (model, "HPG3010") == 0)
    rst = HPG3010;
  else if (strcmp (model, "HPG3110") == 0)
    rst = HPG3110;
  else if (strcmp (model, "UA4900") == 0)
    rst = UA4900;
  else if (strcmp (model, "BQ5550") == 0)
    rst = BQ5550;
  else
    rst = HP3970;		/* default */

  return rst;
}

static SANE_Int
Get_Source (SANE_String source)
{
  SANE_Int rst;

  if (strcmp (source, SANE_I18N ("Flatbed")) == 0)
    rst = ST_NORMAL;
  else if (strcmp (source, SANE_I18N ("Slide")) == 0)
    rst = ST_TA;
  else if (strcmp (source, SANE_I18N ("Negative")) == 0)
    rst = ST_NEG;
  else
    rst = ST_NORMAL;		/* default */

  return rst;
}

static SANE_Int
Get_Colormode (SANE_String colormode)
{
  SANE_Int rst;

  if (strcmp (colormode, SANE_VALUE_SCAN_MODE_COLOR) == 0)
    rst = CM_COLOR;
  else if (strcmp (colormode, SANE_VALUE_SCAN_MODE_GRAY) == 0)
    rst = CM_GRAY;
  else if (strcmp (colormode, SANE_VALUE_SCAN_MODE_LINEART) == 0)
    rst = CM_LINEART;
  else
    rst = CM_COLOR;		/* default */

  return rst;
}

static SANE_Status
Translate_coords (struct st_coords *coords)
{
  SANE_Int data;

  DBG (DBG_FNC, "> Translate_coords(*coords)\n");

  if ((coords->left < 0) || (coords->top < 0) ||
      (coords->width < 0) || (coords->height < 0))
    return SANE_STATUS_INVAL;

  if (coords->width < coords->left)
    {
      data = coords->left;
      coords->left = coords->width;
      coords->width = data;
    }

  if (coords->height < coords->top)
    {
      data = coords->top;
      coords->top = coords->height;
      coords->height = data;
    }

  coords->width -= coords->left;
  coords->height -= coords->top;

  if (coords->width == 0)
    coords->width++;

  if (coords->height == 0)
    coords->height++;

  return SANE_STATUS_GOOD;
}

static size_t
max_string_size (const SANE_String_Const strings[])
{
  size_t size, max_size = 0;
  SANE_Int i;

  DBG (DBG_FNC, "> max_string_size:\n");

  for (i = 0; strings[i]; ++i)
    {
      size = strlen (strings[i]) + 1;
      if (size > max_size)
	max_size = size;
    }

  return max_size;
}

static void
options_free (TScanner * scanner)
{
  /* frees all information contained in controls */

  DBG (DBG_FNC, "> options_free\n");

  if (scanner != NULL)
    {
      SANE_Int i;
      SANE_Option_Descriptor *pDesc;
      TOptionValue *pVal;

      /* free gamma tables */
      gamma_free (scanner);

      /* free lists */
      if (scanner->list_resolutions != NULL)
	free (scanner->list_resolutions);

      if (scanner->list_depths != NULL)
	free (scanner->list_depths);

      if (scanner->list_sources != NULL)
	free (scanner->list_sources);

      if (scanner->list_colormodes != NULL)
	free (scanner->list_colormodes);

      if (scanner->list_models != NULL)
	free (scanner->list_models);

      /* free values in certain controls */
      for (i = opt_begin; i < opt_count; i++)
	{
	  pDesc = &scanner->aOptions[i];
	  pVal = &scanner->aValues[i];

	  if (pDesc->type == SANE_TYPE_STRING)
	    {
	      if (pVal->s != NULL)
		free (pVal->s);
	    }
	}
    }
}

static void
options_init (TScanner * scanner)
{
  /* initializes all controls */

  DBG (DBG_FNC, "> options_init\n");

  if (scanner != NULL)
    {
      SANE_Int i;
      SANE_Option_Descriptor *pDesc;
      TOptionValue *pVal;

      /* set gamma */
      gamma_create (scanner, 2.2);

      /* color convertion */
      scanner->cnv.colormode = -1;
      scanner->cnv.negative = FALSE;
      scanner->cnv.threshold = 40;
      scanner->cnv.real_depth = FALSE;
      scanner->cnv.depth = -1;

      /* setting threshold */
      scanner->rng_threshold.min = 0;
      scanner->rng_threshold.max = 255;
      scanner->rng_threshold.quant = 0;

      /* setting gamma range (16 bits depth) */
      scanner->rng_gamma.min = 0;
      scanner->rng_gamma.max = 65535;
      scanner->rng_gamma.quant = 0;

      /* setting default horizontal constrain in milimeters */
      scanner->rng_horizontal.min = 0;
      scanner->rng_horizontal.max = 220;
      scanner->rng_horizontal.quant = 1;

      /* setting default vertical constrain in milimeters */
      scanner->rng_vertical.min = 0;
      scanner->rng_vertical.max = 300;
      scanner->rng_vertical.quant = 1;

      /* allocate option lists */
      bknd_info (scanner);
      bknd_colormodes (scanner, RTS_Debug->dev_model);
      bknd_depths (scanner, RTS_Debug->dev_model);
      bknd_models (scanner);
      bknd_resolutions (scanner, RTS_Debug->dev_model);
      bknd_sources (scanner, RTS_Debug->dev_model);

      /* By default preview scan */
      scanner->ScanParams.scantype = ST_NORMAL;
      scanner->ScanParams.colormode = CM_COLOR;
      scanner->ScanParams.resolution_x = 75;
      scanner->ScanParams.resolution_y = 75;
      scanner->ScanParams.coords.left = 0;
      scanner->ScanParams.coords.top = 0;
      scanner->ScanParams.coords.width = 220;
      scanner->ScanParams.coords.height = 300;
      scanner->ScanParams.depth = 8;
      scanner->ScanParams.channel = 0;

      for (i = opt_begin; i < opt_count; i++)
	{
	  pDesc = &scanner->aOptions[i];
	  pVal = &scanner->aValues[i];

	  /* defaults */
	  pDesc->name = "";
	  pDesc->title = "";
	  pDesc->desc = "";
	  pDesc->type = SANE_TYPE_INT;
	  pDesc->unit = SANE_UNIT_NONE;
	  pDesc->size = sizeof (SANE_Word);
	  pDesc->constraint_type = SANE_CONSTRAINT_NONE;
	  pDesc->cap = 0;

	  switch (i)
	    {
	    case opt_begin:
	      pDesc->title = SANE_TITLE_NUM_OPTIONS;
	      pDesc->desc = SANE_DESC_NUM_OPTIONS;
	      pDesc->cap = SANE_CAP_SOFT_DETECT;
	      pVal->w = (SANE_Word) opt_count;
	      break;

	    case grp_geometry:
	      pDesc->name = SANE_NAME_GEOMETRY;
	      pDesc->title = SANE_TITLE_GEOMETRY;
	      pDesc->desc = SANE_DESC_GEOMETRY;
	      pDesc->type = SANE_TYPE_GROUP;
	      pDesc->unit = SANE_UNIT_NONE;
	      pDesc->size = 0;
	      pDesc->cap = 0;
	      pDesc->constraint_type = SANE_CONSTRAINT_NONE;
	      pDesc->constraint.range = 0;
	      pVal->w = 0;
	      break;

	    case opt_tlx:
	      pDesc->name = SANE_NAME_SCAN_TL_X;
	      pDesc->title = SANE_TITLE_SCAN_TL_X;
	      pDesc->desc = SANE_DESC_SCAN_TL_X;
	      pDesc->unit = SANE_UNIT_MM;
	      pDesc->constraint_type = SANE_CONSTRAINT_RANGE;
	      pDesc->constraint.range = &scanner->rng_horizontal;
	      pDesc->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	      pVal->w = 0;
	      break;

	    case opt_tly:
	      pDesc->name = SANE_NAME_SCAN_TL_Y;
	      pDesc->title = SANE_TITLE_SCAN_TL_Y;
	      pDesc->desc = SANE_DESC_SCAN_TL_Y;
	      pDesc->unit = SANE_UNIT_MM;
	      pDesc->constraint_type = SANE_CONSTRAINT_RANGE;
	      pDesc->constraint.range = &scanner->rng_vertical;
	      pDesc->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	      pVal->w = 0;
	      break;

	    case opt_brx:
	      pDesc->name = SANE_NAME_SCAN_BR_X;
	      pDesc->title = SANE_TITLE_SCAN_BR_X;
	      pDesc->desc = SANE_DESC_SCAN_BR_X;
	      pDesc->unit = SANE_UNIT_MM;
	      pDesc->constraint_type = SANE_CONSTRAINT_RANGE;
	      pDesc->constraint.range = &scanner->rng_horizontal;
	      pDesc->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	      pVal->w = scanner->rng_horizontal.max;
	      break;

	    case opt_bry:
	      pDesc->name = SANE_NAME_SCAN_BR_Y;
	      pDesc->title = SANE_TITLE_SCAN_BR_Y;
	      pDesc->desc = SANE_DESC_SCAN_BR_Y;
	      pDesc->unit = SANE_UNIT_MM;
	      pDesc->constraint_type = SANE_CONSTRAINT_RANGE;
	      pDesc->constraint.range = &scanner->rng_vertical;
	      pDesc->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	      pVal->w = scanner->rng_vertical.max;
	      break;

	    case opt_resolution:
	      pDesc->name = SANE_NAME_SCAN_RESOLUTION;
	      pDesc->title = SANE_TITLE_SCAN_RESOLUTION;
	      pDesc->desc = SANE_DESC_SCAN_RESOLUTION;
	      pDesc->unit = SANE_UNIT_DPI;
	      pDesc->constraint_type = SANE_CONSTRAINT_WORD_LIST;
	      pDesc->constraint.word_list = scanner->list_resolutions;
	      pDesc->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	      pVal->w = scanner->list_resolutions[1];
	      break;

	    case opt_gamma_red:
	      pDesc->name = SANE_NAME_GAMMA_VECTOR_R;
	      pDesc->title = SANE_TITLE_GAMMA_VECTOR_R;
	      pDesc->desc = SANE_DESC_GAMMA_VECTOR_R;
	      pDesc->size = scanner->rng_gamma.max * sizeof (SANE_Word);
	      pDesc->unit = SANE_UNIT_NONE;
	      pDesc->constraint_type = SANE_CONSTRAINT_RANGE;
	      pDesc->constraint.range = &scanner->rng_gamma;
	      pDesc->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	      pVal->wa = scanner->aGammaTable[CL_RED];
	      break;

	    case opt_gamma_green:
	      pDesc->name = SANE_NAME_GAMMA_VECTOR_G;
	      pDesc->title = SANE_TITLE_GAMMA_VECTOR_G;
	      pDesc->desc = SANE_DESC_GAMMA_VECTOR_G;
	      pDesc->size = scanner->rng_gamma.max * sizeof (SANE_Word);
	      pDesc->unit = SANE_UNIT_NONE;
	      pDesc->constraint_type = SANE_CONSTRAINT_RANGE;
	      pDesc->constraint.range = &scanner->rng_gamma;
	      pDesc->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	      pVal->wa = scanner->aGammaTable[CL_GREEN];
	      break;

	    case opt_gamma_blue:
	      pDesc->name = SANE_NAME_GAMMA_VECTOR_B;
	      pDesc->title = SANE_TITLE_GAMMA_VECTOR_B;
	      pDesc->desc = SANE_DESC_GAMMA_VECTOR_B;
	      pDesc->size = scanner->rng_gamma.max * sizeof (SANE_Word);
	      pDesc->unit = SANE_UNIT_NONE;
	      pDesc->constraint_type = SANE_CONSTRAINT_RANGE;
	      pDesc->constraint.range = &scanner->rng_gamma;
	      pDesc->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	      pVal->wa = scanner->aGammaTable[CL_BLUE];
	      break;

	    case opt_scantype:
	      pDesc->name = SANE_NAME_SCAN_SOURCE;
	      pDesc->title = SANE_TITLE_SCAN_SOURCE;
	      pDesc->desc = SANE_DESC_SCAN_SOURCE;
	      pDesc->type = SANE_TYPE_STRING;
	      pDesc->size = max_string_size (scanner->list_sources);
	      pDesc->constraint_type = SANE_CONSTRAINT_STRING_LIST;
	      pDesc->constraint.string_list = scanner->list_sources;
	      pDesc->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	      pVal->s = strdup (scanner->list_sources[0]);
	      break;

	    case opt_colormode:
	      pDesc->name = SANE_NAME_SCAN_MODE;
	      pDesc->title = SANE_TITLE_SCAN_MODE;
	      pDesc->desc = SANE_DESC_SCAN_MODE;
	      pDesc->type = SANE_TYPE_STRING;
	      pDesc->size = max_string_size (scanner->list_colormodes);
	      pDesc->constraint_type = SANE_CONSTRAINT_STRING_LIST;
	      pDesc->constraint.string_list = scanner->list_colormodes;
	      pDesc->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	      pVal->s = strdup (scanner->list_colormodes[0]);
	      break;

	    case opt_depth:
	      pDesc->name = SANE_NAME_BIT_DEPTH;
	      pDesc->title = SANE_TITLE_BIT_DEPTH;
	      pDesc->desc = SANE_DESC_BIT_DEPTH;
	      pDesc->type = SANE_TYPE_INT;
	      pDesc->unit = SANE_UNIT_BIT;
	      pDesc->constraint_type = SANE_CONSTRAINT_WORD_LIST;
	      pDesc->constraint.word_list = scanner->list_depths;
	      pDesc->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	      pVal->w = scanner->list_depths[1];
	      break;

	    case opt_threshold:
	      pDesc->name = SANE_NAME_THRESHOLD;
	      pDesc->title = SANE_TITLE_THRESHOLD;
	      pDesc->desc = SANE_DESC_THRESHOLD;
	      pDesc->type = SANE_TYPE_INT;
	      pDesc->unit = SANE_UNIT_NONE;
	      pDesc->constraint_type = SANE_CONSTRAINT_RANGE;
	      pDesc->constraint.range = &scanner->rng_threshold;
	      pDesc->cap |=
		SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT |
		SANE_CAP_INACTIVE;
	      pVal->w = 0x80;
	      break;

	      /* debugging options */
	    case grp_debug:
	      pDesc->name = "grp_debug";
	      pDesc->title = SANE_I18N ("Debugging Options");
	      pDesc->desc = "";
	      pDesc->type = SANE_TYPE_GROUP;
	      pDesc->unit = SANE_UNIT_NONE;
	      pDesc->size = 0;
	      pDesc->cap = SANE_CAP_ADVANCED;
	      pDesc->constraint_type = SANE_CONSTRAINT_NONE;
	      pDesc->constraint.range = 0;
	      pVal->w = 0;
	      break;

	    case opt_model:
	      pDesc->name = "opt_model";
	      pDesc->title = SANE_I18N ("Scanner model");
	      pDesc->desc =
		SANE_I18N
		("Allows one to test device behaviour with other supported models");
	      pDesc->type = SANE_TYPE_STRING;
	      pDesc->size = max_string_size (scanner->list_models);
	      pDesc->constraint_type = SANE_CONSTRAINT_STRING_LIST;
	      pDesc->constraint.string_list = scanner->list_models;
	      pDesc->cap =
		SANE_CAP_ADVANCED | SANE_CAP_SOFT_SELECT |
		SANE_CAP_SOFT_DETECT;
	      pVal->s = strdup (scanner->list_models[0]);
	      break;

	    case opt_negative:
	      pDesc->name = "opt_negative";
	      pDesc->title = SANE_I18N ("Negative");
	      pDesc->desc = SANE_I18N ("Image colours will be inverted");
	      pDesc->type = SANE_TYPE_BOOL;
	      pDesc->unit = SANE_UNIT_NONE;
	      pDesc->size = sizeof (SANE_Word);
	      pDesc->constraint_type = SANE_CONSTRAINT_NONE;
	      pDesc->constraint.range = 0;
	      pDesc->cap =
		SANE_CAP_ADVANCED | SANE_CAP_SOFT_DETECT |
		SANE_CAP_SOFT_SELECT;
	      pVal->w = SANE_FALSE;
	      break;

	    case opt_nogamma:
	      pDesc->name = "opt_nogamma";
	      pDesc->title = SANE_I18N ("Disable gamma correction");
	      pDesc->desc = SANE_I18N ("Gamma correction will be disabled");
	      pDesc->type = SANE_TYPE_BOOL;
	      pDesc->unit = SANE_UNIT_NONE;
	      pDesc->size = sizeof (SANE_Word);
	      pDesc->constraint_type = SANE_CONSTRAINT_NONE;
	      pDesc->constraint.range = 0;
	      pDesc->cap =
		SANE_CAP_ADVANCED | SANE_CAP_SOFT_DETECT |
		SANE_CAP_SOFT_SELECT;
	      pVal->w = SANE_FALSE;
	      break;

	    case opt_nowshading:
	      pDesc->name = "opt_nowshading";
	      pDesc->title = SANE_I18N ("Disable white shading correction");
	      pDesc->desc =
		SANE_I18N ("White shading correction will be disabled");
	      pDesc->type = SANE_TYPE_BOOL;
	      pDesc->unit = SANE_UNIT_NONE;
	      pDesc->size = sizeof (SANE_Word);
	      pDesc->constraint_type = SANE_CONSTRAINT_NONE;
	      pDesc->constraint.range = 0;
	      pDesc->cap =
		SANE_CAP_ADVANCED | SANE_CAP_SOFT_DETECT |
		SANE_CAP_SOFT_SELECT;
	      pVal->w = SANE_FALSE;
	      break;

	    case opt_nowarmup:
	      pDesc->name = "opt_nowarmup";
	      pDesc->title = SANE_I18N ("Skip warmup process");
	      pDesc->desc = SANE_I18N ("Warmup process will be disabled");
	      pDesc->type = SANE_TYPE_BOOL;
	      pDesc->unit = SANE_UNIT_NONE;
	      pDesc->size = sizeof (SANE_Word);
	      pDesc->constraint_type = SANE_CONSTRAINT_NONE;
	      pDesc->constraint.range = 0;
	      pDesc->cap =
		SANE_CAP_ADVANCED | SANE_CAP_SOFT_DETECT |
		SANE_CAP_SOFT_SELECT;
	      pVal->w = SANE_FALSE;
	      break;

	    case opt_realdepth:
	      pDesc->name = "opt_realdepth";
	      pDesc->title = SANE_I18N ("Force real depth");
	      pDesc->desc =
		SANE_I18N
		("If gamma is enabled, scans are always made in 16 bits depth to improve image quality and then converted to the selected depth. This option avoids depth emulation.");
	      pDesc->type = SANE_TYPE_BOOL;
	      pDesc->unit = SANE_UNIT_NONE;
	      pDesc->size = sizeof (SANE_Word);
	      pDesc->constraint_type = SANE_CONSTRAINT_NONE;
	      pDesc->constraint.range = 0;
	      pDesc->cap =
		SANE_CAP_ADVANCED | SANE_CAP_SOFT_DETECT |
		SANE_CAP_SOFT_SELECT;
	      pVal->w = SANE_FALSE;
	      break;

	    case opt_emulategray:
	      pDesc->name = "opt_emulategray";
	      pDesc->title = SANE_I18N ("Emulate Grayscale");
	      pDesc->desc =
		SANE_I18N
		("If enabled, image will be scanned in color mode and then converted to grayscale by software. This may improve image quality in some circumstances.");
	      pDesc->type = SANE_TYPE_BOOL;
	      pDesc->unit = SANE_UNIT_NONE;
	      pDesc->size = sizeof (SANE_Word);
	      pDesc->constraint_type = SANE_CONSTRAINT_NONE;
	      pDesc->constraint.range = 0;
	      pDesc->cap =
		SANE_CAP_ADVANCED | SANE_CAP_SOFT_DETECT |
		SANE_CAP_SOFT_SELECT;
	      pVal->w = SANE_FALSE;
	      break;

	    case opt_dbgimages:
	      pDesc->name = "opt_dbgimages";
	      pDesc->title = SANE_I18N ("Save debugging images");
	      pDesc->desc =
		SANE_I18N
		("If enabled, some images involved in scanner processing are saved to analyze them.");
	      pDesc->type = SANE_TYPE_BOOL;
	      pDesc->unit = SANE_UNIT_NONE;
	      pDesc->size = sizeof (SANE_Word);
	      pDesc->constraint_type = SANE_CONSTRAINT_NONE;
	      pDesc->constraint.range = 0;
	      pDesc->cap =
		SANE_CAP_ADVANCED | SANE_CAP_SOFT_DETECT |
		SANE_CAP_SOFT_SELECT;
	      pVal->w = SANE_FALSE;
	      break;

	    case opt_reset:
	      pDesc->name = "opt_reset";
	      pDesc->title = SANE_I18N ("Reset chipset");
	      pDesc->desc = SANE_I18N ("Resets chipset data");
	      pDesc->type = SANE_TYPE_BUTTON;
	      pDesc->unit = SANE_UNIT_NONE;
	      pDesc->size = 0;
	      pDesc->constraint_type = SANE_CONSTRAINT_NONE;
	      pDesc->constraint.string_list = 0;
	      pDesc->cap = SANE_CAP_ADVANCED | SANE_CAP_SOFT_SELECT;
	      pVal->w = 0;
	      break;

	      /* device information */
	    case grp_info:
	      pDesc->name = "grp_info";
	      pDesc->title = SANE_I18N ("Information");
	      pDesc->desc = "";
	      pDesc->type = SANE_TYPE_GROUP;
	      pDesc->unit = SANE_UNIT_NONE;
	      pDesc->size = 0;
	      pDesc->cap = 0;
	      pDesc->constraint_type = SANE_CONSTRAINT_NONE;
	      pDesc->constraint.range = 0;
	      pVal->w = 0;
	      break;

	    case opt_chipname:
	      pDesc->name = "opt_chipname";
	      pDesc->title = SANE_I18N ("Chipset name");
	      pDesc->desc = SANE_I18N ("Shows chipset name used in device.");
	      pDesc->type = SANE_TYPE_STRING;
	      pDesc->constraint_type = SANE_CONSTRAINT_NONE;
	      pDesc->cap = SANE_CAP_ADVANCED | SANE_CAP_SOFT_DETECT;
	      pVal->s = strdup (SANE_I18N ("Unknown"));
	      pDesc->size = strlen(pVal->s) + 1;
	      break;

	    case opt_chipid:
	      pDesc->name = "opt_chipid";
	      pDesc->title = SANE_I18N ("Chipset ID");
	      pDesc->desc = SANE_I18N ("Shows the chipset ID");
	      pDesc->type = SANE_TYPE_INT;
	      pDesc->unit = SANE_UNIT_NONE;
	      pDesc->constraint_type = SANE_CONSTRAINT_NONE;
	      pDesc->cap = SANE_CAP_ADVANCED | SANE_CAP_SOFT_DETECT;
	      pVal->w = -1;
	      break;

	    case opt_scancount:
	      pDesc->name = "opt_scancount";
	      pDesc->title = SANE_I18N ("Scan counter");
	      pDesc->desc =
		SANE_I18N ("Shows the number of scans made by scanner");
	      pDesc->type = SANE_TYPE_INT;
	      pDesc->unit = SANE_UNIT_NONE;
	      pDesc->constraint_type = SANE_CONSTRAINT_NONE;
	      pDesc->cap = SANE_CAP_ADVANCED | SANE_CAP_SOFT_DETECT;
	      pVal->w = -1;
	      break;

	    case opt_infoupdate:
	      pDesc->name = "opt_infoupdate";
	      pDesc->title = SANE_I18N ("Update information");
	      pDesc->desc = SANE_I18N ("Updates information about device");
	      pDesc->type = SANE_TYPE_BUTTON;
	      pDesc->unit = SANE_UNIT_NONE;
	      pDesc->size = 0;
	      pDesc->constraint_type = SANE_CONSTRAINT_NONE;
	      pDesc->constraint.string_list = 0;
	      pDesc->cap = SANE_CAP_ADVANCED | SANE_CAP_SOFT_SELECT;
	      pVal->w = 0;
	      break;

	      /* buttons support */
	    case grp_sensors:
	      pDesc->name = SANE_NAME_SENSORS;
	      pDesc->title = SANE_TITLE_SENSORS;
	      pDesc->desc = SANE_DESC_SENSORS;
	      pDesc->type = SANE_TYPE_GROUP;
	      pDesc->unit = SANE_UNIT_NONE;
	      pDesc->size = 0;
	      pDesc->cap = 0;
	      pDesc->constraint_type = SANE_CONSTRAINT_NONE;
	      pDesc->constraint.range = 0;
	      pVal->w = 0;
	      break;

	    case opt_button_0:
	    case opt_button_1:
	    case opt_button_2:
	    case opt_button_3:
	    case opt_button_4:
	    case opt_button_5:
	      {
		char name[12];
		char title[128];

		sprintf (name, "button %d", i - opt_button_0);
		sprintf (title, "Scanner button %d", i - opt_button_0);
		pDesc->name = strdup (name);
		pDesc->title = strdup (title);
		pDesc->desc =
		  SANE_I18N
		  ("This option reflects a front panel scanner button");
		pDesc->type = SANE_TYPE_BOOL;
		pDesc->cap = SANE_CAP_SOFT_DETECT | SANE_CAP_ADVANCED;

		if (i - opt_button_0 >= Buttons_Count (device))
		  pDesc->cap |= SANE_CAP_INACTIVE;

		pDesc->unit = SANE_UNIT_NONE;
		pDesc->size = sizeof (SANE_Word);
		pDesc->constraint_type = SANE_CONSTRAINT_NONE;
		pVal->w = SANE_FALSE;
	      }
	      break;
	    }
	}
    }
}

static SANE_Int
_ReportDevice (TScannerModel * pModel, const char *pszDeviceName)
{
  SANE_Int rst = ERROR;
  TDevListEntry *pNew, *pDev;

  DBG (DBG_FNC, "> _ReportDevice:\n");

  pNew = malloc (sizeof (TDevListEntry));
  if (pNew != NULL)
    {
      rst = OK;

      /* add new element to the end of the list */
      if (_pFirstSaneDev != NULL)
	{
	  /* Add at the end of existing list */
	  for (pDev = _pFirstSaneDev; pDev->pNext; pDev = pDev->pNext);

	  pDev->pNext = pNew;
	}
      else
	_pFirstSaneDev = pNew;

      /* fill in new element */
      pNew->pNext = NULL;
      pNew->devname = (char *) strdup (pszDeviceName);
      pNew->dev.name = pNew->devname;
      pNew->dev.vendor = pModel->pszVendor;
      pNew->dev.model = pModel->pszName;
      pNew->dev.type = SANE_I18N ("flatbed scanner");

      iNumSaneDev++;
    }

  return rst;
}

static SANE_Status
attach_one_device (SANE_String_Const devname)
{
  static TScannerModel sModel;

  DBG (DBG_FNC, "> attach_one_device(devname=%s)\n", devname);

  switch (GetUSB_device_model (devname))
    {
    case HP3800:
      sModel.pszVendor = (char *) strdup ("Hewlett-Packard");
      sModel.pszName = (char *) strdup ("Scanjet 3800");
      break;
    case HPG2710:
      sModel.pszVendor = (char *) strdup ("Hewlett-Packard");
      sModel.pszName = (char *) strdup ("Scanjet G2710");
      break;
    case HP3970:
      sModel.pszVendor = (char *) strdup ("Hewlett-Packard");
      sModel.pszName = (char *) strdup ("Scanjet 3970");
      break;
    case HP4070:
      sModel.pszVendor = (char *) strdup ("Hewlett-Packard");
      sModel.pszName = (char *) strdup ("Scanjet 4070 Photosmart");
      break;
    case HP4370:
      sModel.pszVendor = (char *) strdup ("Hewlett-Packard");
      sModel.pszName = (char *) strdup ("Scanjet 4370");
      break;
    case HPG3010:
      sModel.pszVendor = (char *) strdup ("Hewlett-Packard");
      sModel.pszName = (char *) strdup ("Scanjet G3010");
      break;
    case HPG3110:
      sModel.pszVendor = (char *) strdup ("Hewlett-Packard");
      sModel.pszName = (char *) strdup ("Scanjet G3110");
      break;
    case UA4900:
      sModel.pszVendor = (char *) strdup ("UMAX");
      sModel.pszName = (char *) strdup ("Astra 4900");
      break;
    case BQ5550:
      sModel.pszVendor = (char *) strdup ("BenQ");
      sModel.pszName = (char *) strdup ("5550");
      break;
    default:
      sModel.pszVendor = (char *) strdup ("Unknown");
      sModel.pszName = (char *) strdup ("RTS8822 chipset based");
      break;
    }

  _ReportDevice (&sModel, devname);

  return SANE_STATUS_GOOD;
}

/* Sane default functions */

SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  FILE *conf_fp;		/* Config file stream  */
  SANE_Char line[PATH_MAX];
  SANE_Char *str = NULL;
  SANE_String_Const proper_str;
  SANE_Int nline = 0;

  /* Initialize debug */
  DBG_INIT ();

  DBG (DBG_FNC, "> sane_init\n");

  /* silence gcc */
  authorize = authorize;

  /* Initialize usb */
  sanei_usb_init ();

  /* Parse config file */
  conf_fp = sanei_config_open (HP3900_CONFIG_FILE);
  if (conf_fp)
    {
      while (sanei_config_read (line, sizeof (line), conf_fp))
	{
	  nline++;
	  if (str)
	    free (str);

	  proper_str = sanei_config_get_string (line, &str);

	  /* Discards white lines and comments */
	  if ((str != NULL) && (proper_str != line) && (str[0] != '#'))
	    {
	      /* If line's not blank or a comment, then it's the device
	       * filename or a usb directive. */
	      sanei_usb_attach_matching_devices (line, attach_one_device);
	    }
	}
      fclose (conf_fp);
    }
  else
    {
      /* default */
      DBG (DBG_VRB, "- %s not found. Looking for hardcoded usb ids ...\n",
	   HP3900_CONFIG_FILE);

      sanei_usb_attach_matching_devices ("usb 0x03f0 0x2605", attach_one_device);	/* HP3800  */
      sanei_usb_attach_matching_devices ("usb 0x03f0 0x2805", attach_one_device);	/* HPG2710 */
      sanei_usb_attach_matching_devices ("usb 0x03f0 0x2305", attach_one_device);	/* HP3970  */
      sanei_usb_attach_matching_devices ("usb 0x03f0 0x2405", attach_one_device);	/* HP4070  */
      sanei_usb_attach_matching_devices ("usb 0x03f0 0x4105", attach_one_device);	/* HP4370  */
      sanei_usb_attach_matching_devices ("usb 0x03f0 0x4205", attach_one_device);	/* HPG3010 */
      sanei_usb_attach_matching_devices ("usb 0x03f0 0x4305", attach_one_device);	/* HPG3110 */
      sanei_usb_attach_matching_devices ("usb 0x06dc 0x0020", attach_one_device);	/* UA4900  */
      sanei_usb_attach_matching_devices ("usb 0x04a5 0x2211", attach_one_device);	/* BQ5550  */
    }

  /* Return backend version */
  if (version_code != NULL)
    *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, 0);

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  SANE_Status rst = SANE_STATUS_GOOD;

  local_only = local_only;

  if (_pSaneDevList)
    free (_pSaneDevList);

  _pSaneDevList = malloc (sizeof (*_pSaneDevList) * (iNumSaneDev + 1));
  if (_pSaneDevList != NULL)
    {
      TDevListEntry *pDev;
      SANE_Int i = 0;

      for (pDev = _pFirstSaneDev; pDev; pDev = pDev->pNext)
	_pSaneDevList[i++] = &pDev->dev;

      _pSaneDevList[i++] = 0;	/* last entry is 0 */
      *device_list = _pSaneDevList;
    }
  else
    rst = SANE_STATUS_NO_MEM;

  DBG (DBG_FNC, "> sane_get_devices: %i\n", rst);

  return rst;
}

SANE_Status
sane_open (SANE_String_Const name, SANE_Handle * h)
{
  TScanner *s;
  SANE_Status rst;

  /* check the name */
  if (strlen (name) == 0)
    /* default to first available device */
    name = _pFirstSaneDev->dev.name;

  /* allocate space for RTS environment */
  device = RTS_Alloc ();
  if (device != NULL)
    {
      /* Open device */
      rst = sanei_usb_open (name, &device->usb_handle);
      if (rst == SANE_STATUS_GOOD)
	{
	  /* Allocating memory for device */
	  s = malloc (sizeof (TScanner));
	  if (s != NULL)
	    {
	      memset (s, 0, sizeof (TScanner));

	      /* Initializing RTS */
	      if (Init_Vars () == OK)
		{
		  SANE_Int vendor, product;

		  /* Setting device model */
		  if (sanei_usb_get_vendor_product
		      (device->usb_handle, &vendor,
		       &product) == SANE_STATUS_GOOD)
		    s->model = Device_get (product, vendor);
		  else
		    s->model = HP3970;

		  set_ScannerModel (s->model, product, vendor);

		  /* Initialize device */
		  if (RTS_Scanner_Init (device) == OK)
		    {
		      /* silencing unused functions */
		      Silent_Compile ();

		      /* initialize backend options */
		      options_init (s);
		      *h = s;

		      /* everything went ok */
		      rst = SANE_STATUS_GOOD;
		    }
		  else
		    {
		      free ((void *) s);
		      rst = SANE_STATUS_INVAL;
		    }
		}
	      else
		rst = SANE_STATUS_NO_MEM;
	    }
	  else
	    rst = SANE_STATUS_NO_MEM;
	}
    }
  else
    rst = SANE_STATUS_NO_MEM;

  DBG (DBG_FNC, "> sane_open(name=%s): %i\n", name, rst);

  return rst;
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle h, SANE_Int n)
{
  SANE_Option_Descriptor *rst = NULL;

  if ((n >= opt_begin) && (n < opt_count))
    {
      TScanner *s = (TScanner *) h;
      rst = &s->aOptions[n];
    }

  DBG (DBG_FNC, "> SANE_Option_Descriptor(handle, n=%i): %i\n", n,
       (rst == NULL) ? -1 : 0);

  return rst;
}

static SANE_Status
option_get (TScanner * scanner, SANE_Int optid, void *result)
{
  /* This function returns value contained in selected option */

  DBG (DBG_FNC, "> option_get(optid=%i)\n", optid);

  if ((scanner != NULL) && (result != NULL))
    {
      switch (optid)
	{
	  /* SANE_Word */
	case opt_begin:	/* null */
	case opt_reset:	/* null */
	case opt_negative:
	case opt_nogamma:
	case opt_nowshading:
	case opt_emulategray:
	case opt_dbgimages:
	case opt_nowarmup:
	case opt_realdepth:
	case opt_depth:
	case opt_resolution:
	case opt_threshold:
	case opt_brx:
	case opt_tlx:
	case opt_bry:
	case opt_tly:
	  *(SANE_Word *) result = scanner->aValues[optid].w;
	  break;

	  /* SANE_Int */
	case opt_chipid:
	case opt_scancount:
	  *(SANE_Int *) result = scanner->aValues[optid].w;
	  break;

	  /* SANE_Word array */
	case opt_gamma_red:
	case opt_gamma_green:
	case opt_gamma_blue:
	  memcpy (result, scanner->aValues[optid].wa,
		  scanner->aOptions[optid].size);
	  break;

	  /* String */
	case opt_colormode:
	case opt_scantype:
	case opt_model:
	case opt_chipname:
	  strncpy (result, scanner->aValues[optid].s, scanner->aOptions[optid].size);
	  ((char*)result)[scanner->aOptions[optid].size-1] = '\0';

	  break;

	  /* scanner buttons */
	case opt_button_0:
	  get_button_status (scanner);
	case opt_button_1:
	case opt_button_2:
	case opt_button_3:
	case opt_button_4:
	case opt_button_5:
	  /* copy the button state */
	  *(SANE_Word *) result = scanner->aValues[optid].w;
	  /* clear the button state */
	  scanner->aValues[optid].w = SANE_FALSE;
	  break;
	}
    }

  return SANE_STATUS_GOOD;
}

static SANE_Status
option_set (TScanner * scanner, SANE_Int optid, void *value, SANE_Int * pInfo)
{
  SANE_Status rst;

  DBG (DBG_FNC, "> option_set(optid=%i)\n", optid);

  rst = SANE_STATUS_INVAL;

  if (scanner != NULL)
    {
      if (scanner->fScanning == FALSE)
	{
	  SANE_Int info = 0;

	  rst = SANE_STATUS_GOOD;

	  switch (optid)
	    {
	    case opt_brx:
	    case opt_tlx:
	    case opt_bry:
	    case opt_tly:
	    case opt_depth:
	    case opt_nogamma:
	    case opt_nowshading:
	    case opt_nowarmup:
	    case opt_negative:
	    case opt_emulategray:
	    case opt_dbgimages:
	    case opt_threshold:
	    case opt_resolution:
	      info |= SANE_INFO_RELOAD_PARAMS;
	      scanner->aValues[optid].w = *(SANE_Word *) value;
	      break;

	    case opt_gamma_red:
	    case opt_gamma_green:
	    case opt_gamma_blue:
	      memcpy (scanner->aValues[optid].wa, value,
		      scanner->aOptions[optid].size);
	      break;

	    case opt_scantype:
	      if (strcmp (scanner->aValues[optid].s, value) != 0)
		{
		  struct st_coords *coords;
		  SANE_Int source;

		  if (scanner->aValues[optid].s)
		    free (scanner->aValues[optid].s);

		  scanner->aValues[optid].s = strdup (value);

		  source = Get_Source (scanner->aValues[opt_scantype].s);
		  coords = Constrains_Get (device, source);
		  if (coords != NULL)
		    {
		      bknd_constrains (scanner, source, 0);
		      bknd_constrains (scanner, source, 1);
		      scanner->aValues[opt_tlx].w = 0;
		      scanner->aValues[opt_tly].w = 0;
		      scanner->aValues[opt_brx].w = coords->width;
		      scanner->aValues[opt_bry].w = coords->height;
		    }

		  info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
		}
	      break;

	    case opt_colormode:
	      if (strcmp (scanner->aValues[optid].s, value) != 0)
		{
		  if (scanner->aValues[optid].s)
		    free (scanner->aValues[optid].s);
		  scanner->aValues[optid].s = strdup (value);
		  if (Get_Colormode (scanner->aValues[optid].s) == CM_LINEART)
		    scanner->aOptions[opt_threshold].cap &=
		      ~SANE_CAP_INACTIVE;
		  else
		    scanner->aOptions[opt_threshold].cap |= SANE_CAP_INACTIVE;
		  info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
		}
	      break;

	    case opt_model:
	      if (strcmp (scanner->aValues[optid].s, value) != 0)
		{
		  SANE_Int model;

		  if (scanner->aValues[optid].s)
		    free (scanner->aValues[optid].s);
		  scanner->aValues[optid].s = strdup (value);

		  model = Get_Model (scanner->aValues[optid].s);
		  if (model != RTS_Debug->dev_model)
		    {
		      SANE_Int source;
		      struct st_coords *coords;

		      /* free configuration of last model */
		      Free_Config (device);

		      /* set new model */
		      RTS_Debug->dev_model = model;

		      /* and load configuration of current model */
		      Load_Config (device);

		      /* update options according to selected device */
		      bknd_info (scanner);
		      bknd_colormodes (scanner, model);
		      bknd_depths (scanner, model);
		      bknd_resolutions (scanner, model);
		      bknd_sources (scanner, model);

		      /* updating lists */
		      scanner->aOptions[opt_colormode].size =
			max_string_size (scanner->list_colormodes);
		      scanner->aOptions[opt_colormode].constraint.
			string_list = scanner->list_colormodes;
		      scanner->aOptions[opt_depth].constraint.word_list =
			scanner->list_depths;
		      scanner->aOptions[opt_resolution].constraint.word_list =
			scanner->list_resolutions;
		      scanner->aOptions[opt_scantype].size =
			max_string_size (scanner->list_sources);
		      scanner->aOptions[opt_scantype].constraint.string_list =
			scanner->list_sources;

		      /* default values */
		      if (scanner->aValues[opt_colormode].s != NULL)
			free (scanner->aValues[opt_colormode].s);

		      if (scanner->aValues[opt_scantype].s != NULL)
			free (scanner->aValues[opt_scantype].s);

		      scanner->aValues[opt_colormode].s =
			strdup (scanner->list_colormodes[0]);
		      scanner->aValues[opt_scantype].s =
			strdup (scanner->list_sources[0]);
		      scanner->aValues[opt_resolution].w =
			scanner->list_resolutions[1];
		      scanner->aValues[opt_depth].w = scanner->list_depths[1];

		      source = Get_Source (scanner->aValues[opt_scantype].s);
		      coords = Constrains_Get (device, source);
		      if (coords != NULL)
			{
			  bknd_constrains (scanner, source, 0);
			  bknd_constrains (scanner, source, 1);
			  scanner->aValues[opt_tlx].w = 0;
			  scanner->aValues[opt_tly].w = 0;
			  scanner->aValues[opt_brx].w = coords->width;
			  scanner->aValues[opt_bry].w = coords->height;
			}
		    }

		  info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
		}
	      break;

	    case opt_reset:
	      Chipset_Reset (device);
	      break;

	    case opt_realdepth:
	      scanner->aValues[optid].w =
		(scanner->cnv.real_depth == TRUE) ? SANE_TRUE : SANE_FALSE;
	      break;

	    case opt_infoupdate:
	      if (bknd_info (scanner) == SANE_STATUS_GOOD)
		info |= SANE_INFO_RELOAD_OPTIONS;
	      break;

	    default:
	      rst = SANE_STATUS_INVAL;
	      break;
	    }

	  if (pInfo != NULL)
	    *pInfo = info;
	}
    }

  return rst;
}

SANE_Status
sane_control_option (SANE_Handle h, SANE_Int n, SANE_Action Action,
		     void *pVal, SANE_Int * pInfo)
{
  TScanner *scanner;
  SANE_Status rst;

  DBG (DBG_FNC, "> sane_control_option\n");

  scanner = (TScanner *) h;

  switch (Action)
    {
    case SANE_ACTION_GET_VALUE:
      rst = option_get (scanner, n, pVal);
      break;

    case SANE_ACTION_SET_VALUE:
      rst = option_set (scanner, n, pVal, pInfo);
      break;

    case SANE_ACTION_SET_AUTO:
      rst = SANE_STATUS_UNSUPPORTED;
      break;

    default:
      rst = SANE_STATUS_INVAL;
      break;
    }

  return rst;
}

SANE_Status
sane_get_parameters (SANE_Handle h, SANE_Parameters * p)
{
  SANE_Status rst = SANE_STATUS_INVAL;
  TScanner *s = (TScanner *) h;

  DBG (DBG_FNC, "+ sane_get_parameters:");

  if (s != NULL)
    {
      struct st_coords coords;
      SANE_Int res, source, depth, colormode, frameformat, bpl;

      /* first do some checks */

      /* colormode */
      colormode = Get_Colormode (s->aValues[opt_colormode].s);

      /* frameformat */
      frameformat =
	(colormode == CM_COLOR) ? SANE_FRAME_RGB : SANE_FRAME_GRAY;

      /* depth */
      depth = (colormode == CM_LINEART) ? 1 : s->aValues[opt_depth].w;

      /* scan type */
      source = Get_Source (s->aValues[opt_scantype].s);

      /* resolution */
      res = s->aValues[opt_resolution].w;

      /* image coordinates in milimeters */
      coords.left = s->aValues[opt_tlx].w;
      coords.top = s->aValues[opt_tly].w;
      coords.width = s->aValues[opt_brx].w;
      coords.height = s->aValues[opt_bry].w;

      /* validate coords */
      if (Translate_coords (&coords) == SANE_STATUS_GOOD)
	{
	  Set_Coordinates (source, res, &coords);

	  if (colormode != CM_LINEART)
	    {
	      bpl = coords.width * ((depth > 8) ? 2 : 1);
	      if (colormode == CM_COLOR)
		bpl *= 3;	/* three channels */
	    }
	  else
	    bpl = (coords.width + 7) / 8;

	  /* return the data */
	  p->format = frameformat;
	  p->last_frame = SANE_TRUE;
	  p->depth = depth;
	  p->lines = coords.height;
	  p->pixels_per_line = coords.width;
	  p->bytes_per_line = bpl;

	  DBG (DBG_FNC, " -> Depth : %i\n", depth);
	  DBG (DBG_FNC, " -> Height: %i\n", coords.height);
	  DBG (DBG_FNC, " -> Width : %i\n", coords.width);
	  DBG (DBG_FNC, " -> BPL   : %i\n", bpl);

	  rst = SANE_STATUS_GOOD;
	}
    }

  DBG (DBG_FNC, "- sane_get_parameters: %i\n", rst);

  return rst;
}

SANE_Status
sane_start (SANE_Handle h)
{
  SANE_Status rst = SANE_STATUS_INVAL;
  TScanner *s;

  DBG (DBG_FNC, "+ sane_start\n");

  s = (TScanner *) h;
  if (s != NULL)
    {
      struct st_coords coords;
      SANE_Int res, source, colormode, depth, channel;

      /* first do some checks */
      /* Get Scan type */
      source = Get_Source (s->aValues[opt_scantype].s);

      /* Check if scanner supports slides and negatives in case selected source is tma */
      if (!((source != ST_NORMAL) && (RTS_isTmaAttached (device) == FALSE)))
	{
	  /* Get depth */
	  depth = s->aValues[opt_depth].w;

	  /* Get color mode */
	  colormode = Get_Colormode (s->aValues[opt_colormode].s);

	  /* Emulating certain color modes */
	  if (colormode == CM_LINEART)
	    {
	      /* emulate lineart */
	      s->cnv.colormode = CM_LINEART;
	      colormode = CM_GRAY;
	      depth = 8;
	    }
	  else if ((colormode == CM_GRAY)
		   && (s->aValues[opt_emulategray].w == SANE_TRUE))
	    {
	      /* emulate grayscale */
	      s->cnv.colormode = CM_GRAY;
	      colormode = CM_COLOR;
	    }
	  else
	    s->cnv.colormode = -1;

	  /* setting channel for colormodes different than CM_COLOR */
	  channel = (colormode != CM_COLOR) ? 1 : 0;

	  /* negative colors */
	  s->cnv.negative =
	    (s->aValues[opt_negative].w == SANE_TRUE) ? TRUE : FALSE;

	  /* Get threshold */
	  s->cnv.threshold = s->aValues[opt_threshold].w;

	  /* Get resolution */
	  res = s->aValues[opt_resolution].w;

	  /* set depth emulation */
	  if (s->cnv.colormode == CM_LINEART)
	    s->cnv.real_depth = TRUE;
	  else
	    s->cnv.real_depth =
	      (s->aValues[opt_realdepth].w == SANE_TRUE) ? TRUE : FALSE;

	  /* use gamma? */
	  RTS_Debug->EnableGamma =
	    (s->aValues[opt_nogamma].w == SANE_TRUE) ? FALSE : TRUE;

	  /* disable white shading correction? */
	  RTS_Debug->wshading =
	    (s->aValues[opt_nowshading].w == SANE_TRUE) ? FALSE : TRUE;

	  /* skip warmup process? */
	  RTS_Debug->warmup =
	    (s->aValues[opt_nowarmup].w == SANE_TRUE) ? FALSE : TRUE;

	  /* save debugging images? */
	  RTS_Debug->SaveCalibFile =
	    (s->aValues[opt_dbgimages].w == SANE_TRUE) ? TRUE : FALSE;

	  /* Get image coordinates in milimeters */
	  coords.left = s->aValues[opt_tlx].w;
	  coords.top = s->aValues[opt_tly].w;
	  coords.width = s->aValues[opt_brx].w;
	  coords.height = s->aValues[opt_bry].w;

	  /* Validate coords */
	  if (Translate_coords (&coords) == SANE_STATUS_GOOD)
	    {

	      /* Stop previusly started scan */
	      RTS_Scanner_StopScan (device, TRUE);

	      s->ScanParams.scantype = source;
	      s->ScanParams.colormode = colormode;
	      s->ScanParams.resolution_x = res;
	      s->ScanParams.resolution_y = res;
	      s->ScanParams.channel = channel;

	      memcpy (&s->ScanParams.coords, &coords,
		      sizeof (struct st_coords));
	      Set_Coordinates (source, res, &s->ScanParams.coords);

	      /* emulating depth? */
	      if ((s->cnv.real_depth == FALSE) && (depth < 16)
		  && (RTS_Debug->EnableGamma == TRUE))
		{
		  /* In order to improve image quality, we will scan at 16bits if
		     we are using gamma correction */
		  s->cnv.depth = depth;
		  s->ScanParams.depth = 16;
		}
	      else
		{
		  s->ScanParams.depth = depth;
		  s->cnv.depth = -1;
		}

	      /* set scanning parameters */
	      if (RTS_Scanner_SetParams (device, &s->ScanParams) == OK)
		{
		  /* Start scanning process */
		  if (RTS_Scanner_StartScan (device) == OK)
		    {
		      /* Allocate buffer to read one line */
		      s->mylin = 0;
		      rst = img_buffers_alloc (s, bytesperline);
		    }
		}
	    }
	}
      else
	rst = SANE_STATUS_COVER_OPEN;
    }

  DBG (DBG_FNC, "- sane_start: %i\n", rst);

  return rst;
}

SANE_Status
sane_read (SANE_Handle h, SANE_Byte * buf, SANE_Int maxlen, SANE_Int * len)
{
  SANE_Status rst = SANE_STATUS_GOOD;
  TScanner *s = (TScanner *) h;

  DBG (DBG_FNC, "+ sane_read\n");

  if ((s != NULL) && (buf != NULL) && (len != NULL))
    {
      /* nothing has been read at the moment */
      *len = 0;

      /* if we read all the lines return EOF */
      if ((s->mylin == s->ScanParams.coords.height)
	  || (device->status->cancel == TRUE))
	{
	  rst =
	    (device->status->cancel ==
	     TRUE) ? SANE_STATUS_CANCELLED : SANE_STATUS_EOF;

	  RTS_Scanner_StopScan (device, FALSE);
	  img_buffers_free (s);
	}
      else
	{
	  SANE_Int emul_len, emul_maxlen;
	  SANE_Int thwidth, transferred, bufflength;
	  SANE_Byte *buffer, *pbuffer;

	  emul_len = 0;
	  if (s->cnv.depth != -1)
	    emul_maxlen = maxlen * (s->ScanParams.depth / s->cnv.depth);
	  else
	    emul_maxlen = maxlen;

	  /* if grayscale emulation is enabled check that retrieved data is multiple of three */
	  if (s->cnv.colormode == CM_GRAY)
	    {
	      SANE_Int chn_size, rest;

	      chn_size = (s->ScanParams.depth > 8) ? 2 : 1;
	      rest = emul_maxlen % (3 * chn_size);

	      if (rest != 0)
		emul_maxlen -= rest;
	    }

	  /* this is important to keep lines alignment in lineart mode */
	  if (s->cnv.colormode == CM_LINEART)
	    emul_maxlen = s->ScanParams.coords.width;

	  /* if we are emulating depth, we scan at 16bit when frontend waits
	     for 8bit data. Next buffer will be used to retrieve data from
	     scanner prior to convert to 8 bits depth */
	  buffer = (SANE_Byte *) malloc (emul_maxlen * sizeof (SANE_Byte));

	  if (buffer != NULL)
	    {
	      pbuffer = buffer;

	      /* get bytes per line */
	      if (s->ScanParams.colormode != CM_LINEART)
		{
		  thwidth =
		    s->ScanParams.coords.width *
		    ((s->ScanParams.depth > 8) ? 2 : 1);

		  if (s->ScanParams.colormode == CM_COLOR)
		    thwidth *= 3;	/* three channels */
		}
	      else
		thwidth = (s->ScanParams.coords.width + 7) / 8;

	      /* read as many lines the buffer may contain and while there are lines to be read */
	      while ((emul_len < emul_maxlen)
		     && (s->mylin < s->ScanParams.coords.height))
		{
		  /* Is there any data waiting for being passed ? */
		  if (s->rest_amount != 0)
		    {
		      /* copy to buffer as many bytes as we can */
		      bufflength =
			min (emul_maxlen - emul_len, s->rest_amount);
		      memcpy (pbuffer, s->rest, bufflength);
		      emul_len += bufflength;
		      pbuffer += bufflength;
		      s->rest_amount -= bufflength;
		      if (s->rest_amount == 0)
			s->mylin++;
		    }
		  else
		    {
		      /* read from scanner up to one line */
		      if (Read_Image
			  (device, bytesperline, s->image,
			   &transferred) != OK)
			{
			  /* error, exit function */
			  rst = SANE_STATUS_EOF;
			  break;
			}

		      /* is there any data? */
		      if (transferred != 0)
			{
			  /* copy to buffer as many bytes as we can */
			  bufflength = min (emul_maxlen - emul_len, thwidth);

			  memcpy (pbuffer, s->image, bufflength);
			  emul_len += bufflength;
			  pbuffer += bufflength;

			  /* the rest will be copied to s->rest buffer */
			  if (bufflength < thwidth)
			    {
			      s->rest_amount = thwidth - bufflength;
			      memcpy (s->rest, s->image + bufflength,
				      s->rest_amount);
			    }
			  else
			    s->mylin++;
			}
		      else
			break;
		    }
		}		/* while */

	      /* process buffer before sending to frontend */
	      if ((emul_len > 0) && (rst != SANE_STATUS_EOF))
		{
		  /* at this point ...
		     buffer  : contains retrieved image
		     emul_len: contains size in bytes of retrieved image

		     after this code ...
		     buf : will contain postprocessed image
		     len : will contain size in bytes of postprocessed image */

		  /* apply gamma if neccesary */
		  if (RTS_Debug->EnableGamma == TRUE)
		    gamma_apply (s, buffer, emul_len, s->ScanParams.depth);

		  /* if we are scanning negatives, let's invert colors */
		  if (s->ScanParams.scantype == ST_NEG)
		    {
		      if (s->cnv.negative == FALSE)
			Color_Negative (buffer, emul_len,
					s->ScanParams.depth);
		    }
		  else if (s->cnv.negative != FALSE)
		    Color_Negative (buffer, emul_len, s->ScanParams.depth);

		  /* emulating grayscale ? */
		  if (s->cnv.colormode == CM_GRAY)
		    {
		      Color_to_Gray (buffer, emul_len, s->ScanParams.depth);
		      emul_len /= 3;
		    }

		  /* emulating depth */
		  if (s->cnv.depth != -1)
		    {
		      switch (s->cnv.depth)
			{
			  /* case 1: treated separately as lineart */
			  /*case 12: in the future */
			case 8:
			  Depth_16_to_8 (buffer, emul_len, buffer);
			  emul_len /= 2;
			  break;
			}
		    }

		  /* lineart mode ? */
		  if (s->cnv.colormode == CM_LINEART)
		    {
		      /* I didn't see any scanner supporting lineart mode.
		         Windows drivers scan in grayscale and then convert image to lineart
		         so let's perform convertion */
		      SANE_Int rest = emul_len % 8;

		      Gray_to_Lineart (buffer, emul_len, s->cnv.threshold);
		      emul_len /= 8;
		      if (rest > 0)
			emul_len++;
		    }

		  /* copy postprocessed image */
		  *len = emul_len;
		  memcpy (buf, buffer, *len);
		}

	      free (buffer);
	    }
	}
    }
  else
    rst = SANE_STATUS_EOF;

  DBG (DBG_FNC, "- sane_read: %s\n", sane_strstatus (rst));

  return rst;
}

void
sane_cancel (SANE_Handle h)
{
  DBG (DBG_FNC, "> sane_cancel\n");

  /* silence gcc */
  h = h;

  device->status->cancel = TRUE;
}

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  DBG (DBG_FNC, "> sane_set_io_mode\n");

  /* silence gcc */
  handle = handle;
  non_blocking = non_blocking;

  return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int * fd)
{
  DBG (DBG_FNC, "> sane_get_select_fd\n");

  /* silence gcc */
  handle = handle;
  fd = fd;

  return SANE_STATUS_UNSUPPORTED;
}

void
sane_close (SANE_Handle h)
{
  TScanner *scanner = (TScanner *) h;

  DBG (DBG_FNC, "- sane_close...\n");

  /* stop previus scans */
  RTS_Scanner_StopScan (device, TRUE);

  /* close usb */
  sanei_usb_close (device->usb_handle);

  /* free scanner internal variables */
  RTS_Scanner_End (device);

  /* free RTS enviroment */
  RTS_Free (device);

  /* free backend variables */
  if (scanner != NULL)
    {
      options_free (scanner);

      img_buffers_free (scanner);
    }
}

void
sane_exit (void)
{
  /* free device list memory */
  if (_pSaneDevList)
    {
      TDevListEntry *pDev, *pNext;

      for (pDev = _pFirstSaneDev; pDev; pDev = pNext)
	{
	  pNext = pDev->pNext;
	  /* pDev->dev.name is the same pointer that pDev->devname */
	  free (pDev->devname);
	  free (pDev);
	}

      _pFirstSaneDev = NULL;
      free (_pSaneDevList);
      _pSaneDevList = NULL;
    }
}
