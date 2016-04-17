/*
   Copyright (C) 2008, Panasonic Russia Ltd.
*/
/* sane - Scanner Access Now Easy.
   Panasonic KV-S1020C / KV-S1025C USB scanners.
*/

#define DEBUG_DECLARE_ONLY

#include "../include/sane/config.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../include/sane/sane.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei.h"
#include "../include/sane/sanei_usb.h"
#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_config.h"
#include "../include/lassert.h"

#include "kvs1025.h"
#include "kvs1025_low.h"

#include "../include/sane/sanei_debug.h"

/* Option lists */

static SANE_String_Const go_scan_mode_list[] = {
  SANE_I18N ("bw"),
  SANE_I18N ("halftone"),
  SANE_I18N ("gray"),
  SANE_I18N ("color"),
  NULL
};

/*
static int go_scan_mode_val[] = {
    0x00,
    0x01,
    0x02,
    0x05
};*/

static const SANE_Word go_resolutions_list[] = {
  11,				/* list size */
  100, 150, 200, 250, 300, 350, 400, 450, 500, 550, 600
};

/* List of scan sources */
static SANE_String_Const go_scan_source_list[] = {
  SANE_I18N ("adf"),
  SANE_I18N ("fb"),
  NULL
};
static const int go_scan_source_val[] = {
  0,
  0x1
};

/* List of feeder modes */
static SANE_String_Const go_feeder_mode_list[] = {
  SANE_I18N ("single"),
  SANE_I18N ("continuous"),
  NULL
};
static const int go_feeder_mode_val[] = {
  0x00,
  0xff
};

/* List of manual feed mode */
static SANE_String_Const go_manual_feed_list[] = {
  SANE_I18N ("off"),
  SANE_I18N ("wait_doc"),
  SANE_I18N ("wait_key"),
  NULL
};
static const int go_manual_feed_val[] = {
  0x00,
  0x01,
  0x02
};

/* List of paper sizes */
static SANE_String_Const go_paper_list[] = {
  SANE_I18N ("user_def"),
  SANE_I18N ("business_card"),
  SANE_I18N ("Check"),
  /*SANE_I18N ("A3"), */
  SANE_I18N ("A4"),
  SANE_I18N ("A5"),
  SANE_I18N ("A6"),
  SANE_I18N ("Letter"),
  /*SANE_I18N ("Double letter 11x17 in"),
     SANE_I18N ("B4"), */
  SANE_I18N ("B5"),
  SANE_I18N ("B6"),
  SANE_I18N ("Legal"),
  NULL
};
static const int go_paper_val[] = {
  0x00,
  0x01,
  0x02,
  /*0x03, *//* A3 : not supported */
  0x04,
  0x05,
  0x06,
  0x07,
  /*0x09,
     0x0C, *//* Dbl letter and B4 : not supported */
  0x0D,
  0x0E,
  0x0F
};

static const KV_PAPER_SIZE go_paper_sizes[] = {
  {210, 297},			/* User defined, default=A4 */
  {54, 90},			/* Business card */
  {80, 170},			/* Check (China business) */
  /*{297, 420}, *//* A3 */
  {210, 297},			/* A4 */
  {148, 210},			/* A5 */
  {105, 148},			/* A6 */
  {216, 280},			/* US Letter 8.5 x 11 in */
  /*{280, 432}, *//* Double Letter 11 x 17 in */
  /*{250, 353}, *//* B4 */
  {176, 250},			/* B5 */
  {125, 176},			/* B6 */
  {216, 356}			/* US Legal */
};

static const int default_paper_size_idx = 3;	/* A4 */
static const int go_paper_max_width = 216;	/* US letter */

/* Lists of supported halftone. They are only valid with
 * for the Black&White mode. */
static SANE_String_Const go_halftone_pattern_list[] = {
  SANE_I18N ("bayer_64"),
  SANE_I18N ("bayer_16"),
  SANE_I18N ("halftone_32"),
  SANE_I18N ("halftone_64"),
  SANE_I18N ("diffusion"),
  NULL
};
static const int go_halftone_pattern_val[] = {
  0x00,
  0x01,
  0x02,
  0x03,
  0x04
};

/* List of automatic threshold options */
static SANE_String_Const go_automatic_threshold_list[] = {
  SANE_I18N ("normal"),
  SANE_I18N ("light"),
  SANE_I18N ("dark"),
  NULL
};
static const int go_automatic_threshold_val[] = {
  0,
  0x11,
  0x1f
};

/* List of white level base. */
static SANE_String_Const go_white_level_list[] = {
  SANE_I18N ("From scanner"),
  SANE_I18N ("From paper"),
  SANE_I18N ("Automatic"),
  NULL
};
static const int go_white_level_val[] = {
  0x00,
  0x80,
  0x81
};

/* List of noise reduction options. */
static SANE_String_Const go_noise_reduction_list[] = {
  SANE_I18N ("default"),
  "1x1",
  "2x2",
  "3x3",
  "4x4",
  "5x5",
  NULL
};
static const int go_noise_reduction_val[] = {
  0x00,
  0x01,
  0x02,
  0x03,
  0x04,
  0x05
};

/* List of image emphasis options, 5 steps */
static SANE_String_Const go_image_emphasis_list[] = {
  SANE_I18N ("smooth"),
  SANE_I18N ("none"),
  SANE_I18N ("low"),
  SANE_I18N ("medium"),		/* default */
  SANE_I18N ("high"),
  NULL
};
static const int go_image_emphasis_val[] = {
  0x14,
  0x00,
  0x11,
  0x12,
  0x13
};

/* List of gamma */
static SANE_String_Const go_gamma_list[] = {
  SANE_I18N ("normal"),
  SANE_I18N ("crt"),
  SANE_I18N ("linier"),
  NULL
};
static const int go_gamma_val[] = {
  0x00,
  0x01,
  0x02
};

/* List of lamp color dropout */
static SANE_String_Const go_lamp_list[] = {
  SANE_I18N ("normal"),
  SANE_I18N ("red"),
  SANE_I18N ("green"),
  SANE_I18N ("blue"),
  NULL
};
static const int go_lamp_val[] = {
  0x00,
  0x01,
  0x02,
  0x03
};

static SANE_Range go_value_range = { 0, 255, 0 };

static SANE_Range go_jpeg_compression_range = { 0, 0x64, 0 };

static SANE_Range go_rotate_range = { 0, 270, 90 };

static SANE_Range go_swdespeck_range = { 0, 9, 1 };

static SANE_Range go_swskip_range = { SANE_FIX(0), SANE_FIX(100), 1 };

static const char *go_option_name[] = {
  "OPT_NUM_OPTS",

  /* General options */
  "OPT_MODE_GROUP",
  "OPT_MODE",			/* scanner modes */
  "OPT_RESOLUTION",		/* X and Y resolution */
  "OPT_DUPLEX",			/* Duplex mode */
  "OPT_SCAN_SOURCE",		/* Scan source, fixed to ADF */
  "OPT_FEEDER_MODE",		/* Feeder mode, fixed to Continous */
  "OPT_LONGPAPER",		/* Long paper mode */
  "OPT_LENGTHCTL",		/* Length control mode */
  "OPT_MANUALFEED",		/* Manual feed mode */
  "OPT_FEED_TIMEOUT",		/* Feed timeout */
  "OPT_DBLFEED",		/* Double feed detection mode */
  "OPT_FIT_TO_PAGE",		/* Scanner shrinks image to fit scanned page */

  /* Geometry group */
  "OPT_GEOMETRY_GROUP",
  "OPT_PAPER_SIZE",		/* Paper size */
  "OPT_LANDSCAPE",		/* true if landscape */
  "OPT_TL_X",			/* upper left X */
  "OPT_TL_Y",			/* upper left Y */
  "OPT_BR_X",			/* bottom right X */
  "OPT_BR_Y",			/* bottom right Y */

  "OPT_ENHANCEMENT_GROUP",
  "OPT_BRIGHTNESS",		/* Brightness */
  "OPT_CONTRAST",		/* Contrast */
  "OPT_AUTOMATIC_THRESHOLD",	/* Binary threshold */
  "OPT_HALFTONE_PATTERN",	/* Halftone pattern */
  "OPT_AUTOMATIC_SEPARATION",	/* Automatic separation */
  "OPT_WHITE_LEVEL",		/* White level */
  "OPT_NOISE_REDUCTION",	/* Noise reduction */
  "OPT_IMAGE_EMPHASIS",		/* Image emphasis */
  "OPT_GAMMA",			/* Gamma */
  "OPT_LAMP",			/* Lamp -- color drop out */
  "OPT_INVERSE",		/* Inverse image */
  "OPT_MIRROR",			/* Mirror image */
  "OPT_JPEG",			/* JPEG Compression */
  "OPT_ROTATE",         	/* Rotate image */

  "OPT_SWDESKEW",               /* Software deskew */
  "OPT_SWDESPECK",              /* Software despeckle */
  "OPT_SWDEROTATE",             /* Software detect/correct 90 deg. rotation */
  "OPT_SWCROP",                 /* Software autocrop */
  "OPT_SWSKIP",                 /* Software blank page skip */

  /* must come last: */
  "OPT_NUM_OPTIONS"
};


/* Round to boundry, return 1 if value modified */
static int
round_to_boundry (SANE_Word * pval, SANE_Word boundry,
		  SANE_Word minv, SANE_Word maxv)
{
  SANE_Word lower, upper, k, v;

  v = *pval;
  k = v / boundry;
  lower = k * boundry;
  upper = (k + 1) * boundry;

  if (v - lower <= upper - v)
    {
      *pval = lower;
    }
  else
    {
      *pval = upper;
    }

  if ((*pval) < minv)
    *pval = minv;
  if ((*pval) > maxv)
    *pval = maxv;

  return ((*pval) != v);
}

/* Returns the length of the longest string, including the terminating
 * character. */
static size_t
max_string_size (SANE_String_Const * strings)
{
  size_t size, max_size = 0;
  int i;

  for (i = 0; strings[i]; ++i)
    {
      size = strlen (strings[i]) + 1;
      if (size > max_size)
	{
	  max_size = size;
	}
    }

  return max_size;
}

/* Lookup a string list from one array and return its index. */
static int
get_string_list_index (const SANE_String_Const * list, SANE_String_Const name)
{
  int index;

  index = 0;
  while (list[index] != NULL)
    {
      if (strcmp (list[index], name) == 0)
	{
	  return (index);
	}
      index++;
    }

  DBG (DBG_error, "System bug: option %s not found in list\n", name);

  return (-1);			/* not found */
}


/* Lookup a string list from one array and return the correnpond value. */
int
get_optval_list (const PKV_DEV dev, int idx,
		 const SANE_String_Const * str_list, const int *val_list)
{
  int index;

  index = get_string_list_index (str_list, dev->val[idx].s);

  if (index < 0)
    index = 0;

  return val_list[index];
}


/* Get device mode from device options */
KV_SCAN_MODE
kv_get_mode (const PKV_DEV dev)
{
  int i;

  i = get_string_list_index (go_scan_mode_list, dev->val[OPT_MODE].s);

  switch (i)
    {
    case 0:
      return SM_BINARY;
    case 1:
      return SM_DITHER;
    case 2:
      return SM_GRAYSCALE;
    case 3:
      return SM_COLOR;
    default:
      assert (0 == 1);
      return 0;
    }
}

void
kv_calc_paper_size (const PKV_DEV dev, int *w, int *h)
{
  int i = get_string_list_index (go_paper_list,
				 dev->val[OPT_PAPER_SIZE].s);
  if (i == 0)
    {				/* Non-standard document */
      int x_tl = mmToIlu (SANE_UNFIX (dev->val[OPT_TL_X].w));
      int y_tl = mmToIlu (SANE_UNFIX (dev->val[OPT_TL_Y].w));
      int x_br = mmToIlu (SANE_UNFIX (dev->val[OPT_BR_X].w));
      int y_br = mmToIlu (SANE_UNFIX (dev->val[OPT_BR_Y].w));
      *w = x_br - x_tl;
      *h = y_br - y_tl;
    }
  else
    {
      if (dev->val[OPT_LANDSCAPE].s)
	{
	  *h = mmToIlu (go_paper_sizes[i].width);
	  *w = mmToIlu (go_paper_sizes[i].height);
	}
      else
	{
	  *w = mmToIlu (go_paper_sizes[i].width);
	  *h = mmToIlu (go_paper_sizes[i].height);
	}
    }
}

/* Get bit depth from scan mode */
int
kv_get_depth (KV_SCAN_MODE mode)
{
  switch (mode)
    {
    case SM_BINARY:
    case SM_DITHER:
      return 1;
    case SM_GRAYSCALE:
      return 8;
    case SM_COLOR:
      return 24;
    default:
      assert (0 == 1);
      return 0;
    }
}

const SANE_Option_Descriptor *
kv_get_option_descriptor (PKV_DEV dev, SANE_Int option)
{
  DBG (DBG_proc, "sane_get_option_descriptor: enter, option %s\n",
       go_option_name[option]);

  if ((unsigned) option >= OPT_NUM_OPTIONS)
    {
      return NULL;
    }

  DBG (DBG_proc, "sane_get_option_descriptor: exit\n");

  return dev->opt + option;
}

/* Reset the options for that scanner. */
void
kv_init_options (PKV_DEV dev)
{
  int i;

  if (dev->option_set)
    return;

  DBG (DBG_proc, "kv_init_options: enter\n");

  /* Pre-initialize the options. */
  memset (dev->opt, 0, sizeof (dev->opt));
  memset (dev->val, 0, sizeof (dev->val));

  for (i = 0; i < OPT_NUM_OPTIONS; ++i)
    {
      dev->opt[i].size = sizeof (SANE_Word);
      dev->opt[i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }

  /* Number of options. */
  dev->opt[OPT_NUM_OPTS].name = "";
  dev->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  dev->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
  dev->opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
  dev->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
  dev->val[OPT_NUM_OPTS].w = OPT_NUM_OPTIONS;

  /* Mode group */
  dev->opt[OPT_MODE_GROUP].title = SANE_I18N ("Scan Mode");
  dev->opt[OPT_MODE_GROUP].desc = "";	/* not valid for a group */
  dev->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
  dev->opt[OPT_MODE_GROUP].cap = 0;
  dev->opt[OPT_MODE_GROUP].size = 0;
  dev->opt[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* Scanner supported modes */
  dev->opt[OPT_MODE].name = SANE_NAME_SCAN_MODE;
  dev->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  dev->opt[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
  dev->opt[OPT_MODE].type = SANE_TYPE_STRING;
  dev->opt[OPT_MODE].size = max_string_size (go_scan_mode_list);
  dev->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  dev->opt[OPT_MODE].constraint.string_list = go_scan_mode_list;
  dev->val[OPT_MODE].s = strdup ("");	/* will be set later */

  /* X and Y resolution */
  dev->opt[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  dev->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  dev->opt[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  dev->opt[OPT_RESOLUTION].type = SANE_TYPE_INT;
  dev->opt[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
  dev->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  dev->opt[OPT_RESOLUTION].constraint.word_list = go_resolutions_list;
  dev->val[OPT_RESOLUTION].w = go_resolutions_list[3];

  /* Duplex */
  dev->opt[OPT_DUPLEX].name = SANE_NAME_DUPLEX;
  dev->opt[OPT_DUPLEX].title = SANE_TITLE_DUPLEX;
  dev->opt[OPT_DUPLEX].desc = SANE_DESC_DUPLEX;
  dev->opt[OPT_DUPLEX].type = SANE_TYPE_BOOL;
  dev->opt[OPT_DUPLEX].unit = SANE_UNIT_NONE;
  dev->val[OPT_DUPLEX].w = SANE_FALSE;
  if (!dev->support_info.support_duplex)
    dev->opt[OPT_DUPLEX].cap |= SANE_CAP_INACTIVE;

  /* Scan source */
  dev->opt[OPT_SCAN_SOURCE].name = SANE_NAME_SCAN_SOURCE;
  dev->opt[OPT_SCAN_SOURCE].title = SANE_TITLE_SCAN_SOURCE;
  dev->opt[OPT_SCAN_SOURCE].desc = SANE_I18N ("Sets the scan source");
  dev->opt[OPT_SCAN_SOURCE].type = SANE_TYPE_STRING;
  dev->opt[OPT_SCAN_SOURCE].size = max_string_size (go_scan_source_list);
  dev->opt[OPT_SCAN_SOURCE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  dev->opt[OPT_SCAN_SOURCE].constraint.string_list = go_scan_source_list;
  dev->val[OPT_SCAN_SOURCE].s = strdup (go_scan_source_list[0]);
  dev->opt[OPT_SCAN_SOURCE].cap &= ~SANE_CAP_SOFT_SELECT;
  /* for KV-S1020C / KV-S1025C, scan source is fixed to ADF */

  /* Feeder mode */
  dev->opt[OPT_FEEDER_MODE].name = "feeder-mode";
  dev->opt[OPT_FEEDER_MODE].title = SANE_I18N ("Feeder mode");
  dev->opt[OPT_FEEDER_MODE].desc = SANE_I18N ("Sets the feeding mode");
  dev->opt[OPT_FEEDER_MODE].type = SANE_TYPE_STRING;
  dev->opt[OPT_FEEDER_MODE].size = max_string_size (go_feeder_mode_list);
  dev->opt[OPT_FEEDER_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  dev->opt[OPT_FEEDER_MODE].constraint.string_list = go_feeder_mode_list;
  dev->val[OPT_FEEDER_MODE].s = strdup (go_feeder_mode_list[1]);

  /* Long paper */
  dev->opt[OPT_LONGPAPER].name = SANE_NAME_LONGPAPER;
  dev->opt[OPT_LONGPAPER].title = SANE_TITLE_LONGPAPER;
  dev->opt[OPT_LONGPAPER].desc = SANE_I18N ("Enable/Disable long paper mode");
  dev->opt[OPT_LONGPAPER].type = SANE_TYPE_BOOL;
  dev->opt[OPT_LONGPAPER].unit = SANE_UNIT_NONE;
  dev->val[OPT_LONGPAPER].w = SANE_FALSE;

  /* Length control */
  dev->opt[OPT_LENGTHCTL].name = SANE_NAME_LENGTHCTL;
  dev->opt[OPT_LENGTHCTL].title = SANE_TITLE_LENGTHCTL;
  dev->opt[OPT_LENGTHCTL].desc =
    SANE_I18N ("Enable/Disable length control mode");
  dev->opt[OPT_LENGTHCTL].type = SANE_TYPE_BOOL;
  dev->opt[OPT_LENGTHCTL].unit = SANE_UNIT_NONE;
  dev->val[OPT_LENGTHCTL].w = SANE_TRUE;

  /* Manual feed */
  dev->opt[OPT_MANUALFEED].name = SANE_NAME_MANUALFEED;
  dev->opt[OPT_MANUALFEED].title = SANE_TITLE_MANUALFEED;
  dev->opt[OPT_MANUALFEED].desc = SANE_I18N ("Sets the manual feed mode");
  dev->opt[OPT_MANUALFEED].type = SANE_TYPE_STRING;
  dev->opt[OPT_MANUALFEED].size = max_string_size (go_manual_feed_list);
  dev->opt[OPT_MANUALFEED].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  dev->opt[OPT_MANUALFEED].constraint.string_list = go_manual_feed_list;
  dev->val[OPT_MANUALFEED].s = strdup (go_manual_feed_list[0]);

  /*Manual feed timeout */
  dev->opt[OPT_FEED_TIMEOUT].name = SANE_NAME_FEED_TIMEOUT;
  dev->opt[OPT_FEED_TIMEOUT].title = SANE_TITLE_FEED_TIMEOUT;
  dev->opt[OPT_FEED_TIMEOUT].desc =
    SANE_I18N ("Sets the manual feed timeout in seconds");
  dev->opt[OPT_FEED_TIMEOUT].type = SANE_TYPE_INT;
  dev->opt[OPT_FEED_TIMEOUT].unit = SANE_UNIT_NONE;
  dev->opt[OPT_FEED_TIMEOUT].size = sizeof (SANE_Int);
  dev->opt[OPT_FEED_TIMEOUT].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_FEED_TIMEOUT].constraint.range = &(go_value_range);
  dev->opt[OPT_FEED_TIMEOUT].cap |= SANE_CAP_INACTIVE;
  dev->val[OPT_FEED_TIMEOUT].w = 30;

  /* Double feed */
  dev->opt[OPT_DBLFEED].name = SANE_NAME_DBLFEED;
  dev->opt[OPT_DBLFEED].title = SANE_TITLE_DBLFEED;
  dev->opt[OPT_DBLFEED].desc =
    SANE_I18N ("Enable/Disable double feed detection");
  dev->opt[OPT_DBLFEED].type = SANE_TYPE_BOOL;
  dev->opt[OPT_DBLFEED].unit = SANE_UNIT_NONE;
  dev->val[OPT_DBLFEED].w = SANE_FALSE;

  /* Fit to page */
  dev->opt[OPT_FIT_TO_PAGE].name = SANE_I18N ("fit-to-page");
  dev->opt[OPT_FIT_TO_PAGE].title = SANE_I18N ("Fit to page");
  dev->opt[OPT_FIT_TO_PAGE].desc =
    SANE_I18N ("Scanner shrinks image to fit scanned page");
  dev->opt[OPT_FIT_TO_PAGE].type = SANE_TYPE_BOOL;
  dev->opt[OPT_FIT_TO_PAGE].unit = SANE_UNIT_NONE;
  dev->val[OPT_FIT_TO_PAGE].w = SANE_FALSE;

  /* Geometry group */
  dev->opt[OPT_GEOMETRY_GROUP].title = SANE_I18N ("Geometry");
  dev->opt[OPT_GEOMETRY_GROUP].desc = "";	/* not valid for a group */
  dev->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
  dev->opt[OPT_GEOMETRY_GROUP].cap = 0;
  dev->opt[OPT_GEOMETRY_GROUP].size = 0;
  dev->opt[OPT_GEOMETRY_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* Paper sizes list */
  dev->opt[OPT_PAPER_SIZE].name = SANE_NAME_PAPER_SIZE;
  dev->opt[OPT_PAPER_SIZE].title = SANE_TITLE_PAPER_SIZE;
  dev->opt[OPT_PAPER_SIZE].desc = SANE_DESC_PAPER_SIZE;
  dev->opt[OPT_PAPER_SIZE].type = SANE_TYPE_STRING;
  dev->opt[OPT_PAPER_SIZE].size = max_string_size (go_paper_list);
  dev->opt[OPT_PAPER_SIZE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  dev->opt[OPT_PAPER_SIZE].constraint.string_list = go_paper_list;
  dev->val[OPT_PAPER_SIZE].s = strdup ("");	/* will be set later */

  /* Landscape */
  dev->opt[OPT_LANDSCAPE].name = SANE_NAME_LANDSCAPE;
  dev->opt[OPT_LANDSCAPE].title = SANE_TITLE_LANDSCAPE;
  dev->opt[OPT_LANDSCAPE].desc =
    SANE_I18N ("Set paper position : "
	       "true for landscape, false for portrait");
  dev->opt[OPT_LANDSCAPE].type = SANE_TYPE_BOOL;
  dev->opt[OPT_LANDSCAPE].unit = SANE_UNIT_NONE;
  dev->val[OPT_LANDSCAPE].w = SANE_FALSE;

  /* Upper left X */
  dev->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  dev->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  dev->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  dev->opt[OPT_TL_X].type = SANE_TYPE_FIXED;
  dev->opt[OPT_TL_X].unit = SANE_UNIT_MM;
  dev->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_TL_X].constraint.range = &(dev->x_range);

  /* Upper left Y */
  dev->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  dev->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  dev->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  dev->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
  dev->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
  dev->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_TL_Y].constraint.range = &(dev->y_range);

  /* Bottom-right x */
  dev->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  dev->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  dev->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  dev->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
  dev->opt[OPT_BR_X].unit = SANE_UNIT_MM;
  dev->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_BR_X].constraint.range = &(dev->x_range);

  /* Bottom-right y */
  dev->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  dev->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  dev->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  dev->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
  dev->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
  dev->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_BR_Y].constraint.range = &(dev->y_range);

  /* Enhancement group */
  dev->opt[OPT_ENHANCEMENT_GROUP].title = SANE_I18N ("Enhancement");
  dev->opt[OPT_ENHANCEMENT_GROUP].desc = "";	/* not valid for a group */
  dev->opt[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
  dev->opt[OPT_ENHANCEMENT_GROUP].cap = SANE_CAP_ADVANCED;
  dev->opt[OPT_ENHANCEMENT_GROUP].size = 0;
  dev->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* Brightness */
  dev->opt[OPT_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
  dev->opt[OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
  dev->opt[OPT_BRIGHTNESS].desc = SANE_DESC_BRIGHTNESS;
  dev->opt[OPT_BRIGHTNESS].type = SANE_TYPE_INT;
  dev->opt[OPT_BRIGHTNESS].unit = SANE_UNIT_NONE;
  dev->opt[OPT_BRIGHTNESS].size = sizeof (SANE_Int);
  dev->opt[OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_BRIGHTNESS].constraint.range = &(go_value_range);
  dev->val[OPT_BRIGHTNESS].w = 128;

  /* Contrast */
  dev->opt[OPT_CONTRAST].name = SANE_NAME_CONTRAST;
  dev->opt[OPT_CONTRAST].title = SANE_TITLE_CONTRAST;
  dev->opt[OPT_CONTRAST].desc = SANE_DESC_CONTRAST;
  dev->opt[OPT_CONTRAST].type = SANE_TYPE_INT;
  dev->opt[OPT_CONTRAST].unit = SANE_UNIT_NONE;
  dev->opt[OPT_CONTRAST].size = sizeof (SANE_Int);
  dev->opt[OPT_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_CONTRAST].constraint.range = &(go_value_range);
  dev->val[OPT_CONTRAST].w = 128;

  /* Automatic threshold */
  dev->opt[OPT_AUTOMATIC_THRESHOLD].name = "automatic-threshold";
  dev->opt[OPT_AUTOMATIC_THRESHOLD].title = SANE_I18N ("Automatic threshold");
  dev->opt[OPT_AUTOMATIC_THRESHOLD].desc =
    SANE_I18N
    ("Automatically sets brightness, contrast, white level, "
     "gamma, noise reduction and image emphasis");
  dev->opt[OPT_AUTOMATIC_THRESHOLD].type = SANE_TYPE_STRING;
  dev->opt[OPT_AUTOMATIC_THRESHOLD].size =
    max_string_size (go_automatic_threshold_list);
  dev->opt[OPT_AUTOMATIC_THRESHOLD].constraint_type =
    SANE_CONSTRAINT_STRING_LIST;
  dev->opt[OPT_AUTOMATIC_THRESHOLD].constraint.string_list =
    go_automatic_threshold_list;
  dev->val[OPT_AUTOMATIC_THRESHOLD].s =
    strdup (go_automatic_threshold_list[0]);

  /* Halftone pattern */
  dev->opt[OPT_HALFTONE_PATTERN].name = SANE_NAME_HALFTONE_PATTERN;
  dev->opt[OPT_HALFTONE_PATTERN].title = SANE_TITLE_HALFTONE_PATTERN;
  dev->opt[OPT_HALFTONE_PATTERN].desc = SANE_DESC_HALFTONE_PATTERN;
  dev->opt[OPT_HALFTONE_PATTERN].type = SANE_TYPE_STRING;
  dev->opt[OPT_HALFTONE_PATTERN].size =
    max_string_size (go_halftone_pattern_list);
  dev->opt[OPT_HALFTONE_PATTERN].constraint_type =
    SANE_CONSTRAINT_STRING_LIST;
  dev->opt[OPT_HALFTONE_PATTERN].constraint.string_list =
    go_halftone_pattern_list;
  dev->val[OPT_HALFTONE_PATTERN].s = strdup (go_halftone_pattern_list[0]);

  /* Automatic separation */
  dev->opt[OPT_AUTOMATIC_SEPARATION].name = SANE_NAME_AUTOSEP;
  dev->opt[OPT_AUTOMATIC_SEPARATION].title = SANE_TITLE_AUTOSEP;
  dev->opt[OPT_AUTOMATIC_SEPARATION].desc = SANE_DESC_AUTOSEP;
  dev->opt[OPT_AUTOMATIC_SEPARATION].type = SANE_TYPE_BOOL;
  dev->opt[OPT_AUTOMATIC_SEPARATION].unit = SANE_UNIT_NONE;
  dev->val[OPT_AUTOMATIC_SEPARATION].w = SANE_FALSE;

  /* White level base */
  dev->opt[OPT_WHITE_LEVEL].name = SANE_NAME_WHITE_LEVEL;
  dev->opt[OPT_WHITE_LEVEL].title = SANE_TITLE_WHITE_LEVEL;
  dev->opt[OPT_WHITE_LEVEL].desc = SANE_DESC_WHITE_LEVEL;
  dev->opt[OPT_WHITE_LEVEL].type = SANE_TYPE_STRING;
  dev->opt[OPT_WHITE_LEVEL].size = max_string_size (go_white_level_list);
  dev->opt[OPT_WHITE_LEVEL].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  dev->opt[OPT_WHITE_LEVEL].constraint.string_list = go_white_level_list;
  dev->val[OPT_WHITE_LEVEL].s = strdup (go_white_level_list[0]);

  /* Noise reduction */
  dev->opt[OPT_NOISE_REDUCTION].name = "noise-reduction";
  dev->opt[OPT_NOISE_REDUCTION].title = SANE_I18N ("Noise reduction");
  dev->opt[OPT_NOISE_REDUCTION].desc =
    SANE_I18N ("Reduce the isolated dot noise");
  dev->opt[OPT_NOISE_REDUCTION].type = SANE_TYPE_STRING;
  dev->opt[OPT_NOISE_REDUCTION].size =
    max_string_size (go_noise_reduction_list);
  dev->opt[OPT_NOISE_REDUCTION].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  dev->opt[OPT_NOISE_REDUCTION].constraint.string_list =
    go_noise_reduction_list;
  dev->val[OPT_NOISE_REDUCTION].s = strdup (go_noise_reduction_list[0]);

  /* Image emphasis */
  dev->opt[OPT_IMAGE_EMPHASIS].name = "image-emphasis";
  dev->opt[OPT_IMAGE_EMPHASIS].title = SANE_I18N ("Image emphasis");
  dev->opt[OPT_IMAGE_EMPHASIS].desc = SANE_I18N ("Sets the image emphasis");
  dev->opt[OPT_IMAGE_EMPHASIS].type = SANE_TYPE_STRING;
  dev->opt[OPT_IMAGE_EMPHASIS].size =
    max_string_size (go_image_emphasis_list);
  dev->opt[OPT_IMAGE_EMPHASIS].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  dev->opt[OPT_IMAGE_EMPHASIS].constraint.string_list =
    go_image_emphasis_list;
  dev->val[OPT_IMAGE_EMPHASIS].s = strdup (SANE_I18N ("medium"));

  /* Gamma */
  dev->opt[OPT_GAMMA].name = "gamma";
  dev->opt[OPT_GAMMA].title = SANE_I18N ("Gamma");
  dev->opt[OPT_GAMMA].desc = SANE_I18N ("Gamma");
  dev->opt[OPT_GAMMA].type = SANE_TYPE_STRING;
  dev->opt[OPT_GAMMA].size = max_string_size (go_gamma_list);
  dev->opt[OPT_GAMMA].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  dev->opt[OPT_GAMMA].constraint.string_list = go_gamma_list;
  dev->val[OPT_GAMMA].s = strdup (go_gamma_list[0]);

  /* Lamp color dropout */
  dev->opt[OPT_LAMP].name = "lamp-color";
  dev->opt[OPT_LAMP].title = SANE_I18N ("Lamp color");
  dev->opt[OPT_LAMP].desc = SANE_I18N ("Sets the lamp color (color dropout)");
  dev->opt[OPT_LAMP].type = SANE_TYPE_STRING;
  dev->opt[OPT_LAMP].size = max_string_size (go_lamp_list);
  dev->opt[OPT_LAMP].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  dev->opt[OPT_LAMP].constraint.string_list = go_lamp_list;
  dev->val[OPT_LAMP].s = strdup (go_lamp_list[0]);
  if (!dev->support_info.support_lamp)
    dev->opt[OPT_LAMP].cap |= SANE_CAP_INACTIVE;

  /* Inverse image */
  dev->opt[OPT_INVERSE].name = SANE_NAME_INVERSE;
  dev->opt[OPT_INVERSE].title = SANE_TITLE_INVERSE;
  dev->opt[OPT_INVERSE].desc =
    SANE_I18N ("Inverse image in B/W or halftone mode");
  dev->opt[OPT_INVERSE].type = SANE_TYPE_BOOL;
  dev->opt[OPT_INVERSE].unit = SANE_UNIT_NONE;
  dev->val[OPT_INVERSE].w = SANE_FALSE;

  /* Mirror image (left/right flip) */
  dev->opt[OPT_MIRROR].name = SANE_NAME_MIRROR;
  dev->opt[OPT_MIRROR].title = SANE_TITLE_MIRROR;
  dev->opt[OPT_MIRROR].desc = SANE_I18N ("Mirror image (left/right flip)");
  dev->opt[OPT_MIRROR].type = SANE_TYPE_BOOL;
  dev->opt[OPT_MIRROR].unit = SANE_UNIT_NONE;
  dev->val[OPT_MIRROR].w = SANE_FALSE;

  /* JPEG Image Compression */
  dev->opt[OPT_JPEG].name = "jpeg";
  dev->opt[OPT_JPEG].title = SANE_I18N ("jpeg compression");
  dev->opt[OPT_JPEG].desc =
    SANE_I18N
    ("JPEG Image Compression with Q parameter, '0' - no compression");
  dev->opt[OPT_JPEG].type = SANE_TYPE_INT;
  dev->opt[OPT_JPEG].unit = SANE_UNIT_NONE;
  dev->opt[OPT_JPEG].size = sizeof (SANE_Int);
  dev->opt[OPT_JPEG].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_JPEG].constraint.range = &(go_jpeg_compression_range);
  dev->val[OPT_JPEG].w = 0;

  /* Image Rotation */
  dev->opt[OPT_ROTATE].name = "rotate";
  dev->opt[OPT_ROTATE].title = SANE_I18N ("Rotate image clockwise");
  dev->opt[OPT_ROTATE].desc =
    SANE_I18N("Request driver to rotate pages by a fixed amount");
  dev->opt[OPT_ROTATE].type = SANE_TYPE_INT;
  dev->opt[OPT_ROTATE].unit = SANE_UNIT_NONE;
  dev->opt[OPT_ROTATE].size = sizeof (SANE_Int);
  dev->opt[OPT_ROTATE].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_ROTATE].constraint.range = &(go_rotate_range);
  dev->val[OPT_ROTATE].w = 0;

  /* Software Deskew */
  dev->opt[OPT_SWDESKEW].name = "swdeskew";
  dev->opt[OPT_SWDESKEW].title = SANE_I18N ("Software deskew");
  dev->opt[OPT_SWDESKEW].desc =
    SANE_I18N("Request driver to rotate skewed pages digitally");
  dev->opt[OPT_SWDESKEW].type = SANE_TYPE_BOOL;
  dev->opt[OPT_SWDESKEW].unit = SANE_UNIT_NONE;
  dev->val[OPT_SWDESKEW].w = SANE_FALSE;

  /* Software Despeckle */
  dev->opt[OPT_SWDESPECK].name = "swdespeck";
  dev->opt[OPT_SWDESPECK].title = SANE_I18N ("Software despeckle diameter");
  dev->opt[OPT_SWDESPECK].desc =
    SANE_I18N("Maximum diameter of lone dots to remove from scan");
  dev->opt[OPT_SWDESPECK].type = SANE_TYPE_INT;
  dev->opt[OPT_SWDESPECK].unit = SANE_UNIT_NONE;
  dev->opt[OPT_SWDESPECK].size = sizeof (SANE_Int);
  dev->opt[OPT_SWDESPECK].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_SWDESPECK].constraint.range = &(go_swdespeck_range);
  dev->val[OPT_SWDESPECK].w = 0;

  /* Software Derotate */
  dev->opt[OPT_SWDEROTATE].name = "swderotate";
  dev->opt[OPT_SWDEROTATE].title = SANE_I18N ("Software derotate");
  dev->opt[OPT_SWDEROTATE].desc =
    SANE_I18N("Request driver to detect and correct 90 degree image rotation");
  dev->opt[OPT_SWDEROTATE].type = SANE_TYPE_BOOL;
  dev->opt[OPT_SWDEROTATE].unit = SANE_UNIT_NONE;
  dev->val[OPT_SWDEROTATE].w = SANE_FALSE;

  /* Software Autocrop*/
  dev->opt[OPT_SWCROP].name = "swcrop";
  dev->opt[OPT_SWCROP].title = SANE_I18N ("Software automatic cropping");
  dev->opt[OPT_SWCROP].desc =
    SANE_I18N("Request driver to remove border from pages digitally");
  dev->opt[OPT_SWCROP].type = SANE_TYPE_BOOL;
  dev->opt[OPT_SWCROP].unit = SANE_UNIT_NONE;
  dev->val[OPT_SWCROP].w = SANE_FALSE;

  /* Software blank page skip */
  dev->opt[OPT_SWSKIP].name = "swskip";
  dev->opt[OPT_SWSKIP].title = SANE_I18N ("Software blank skip percentage");
  dev->opt[OPT_SWSKIP].desc
   = SANE_I18N("Request driver to discard pages with low numbers of dark pixels");
  dev->opt[OPT_SWSKIP].type = SANE_TYPE_FIXED;
  dev->opt[OPT_SWSKIP].unit = SANE_UNIT_PERCENT;
  dev->opt[OPT_SWSKIP].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_SWSKIP].constraint.range = &(go_swskip_range);

  /* Lastly, set the default scan mode. This might change some
   * values previously set here. */
  sane_control_option (dev, OPT_PAPER_SIZE, SANE_ACTION_SET_VALUE,
		       (void *) go_paper_list[default_paper_size_idx], NULL);
  sane_control_option (dev, OPT_MODE, SANE_ACTION_SET_VALUE,
		       (void *) go_scan_mode_list[0], NULL);

  DBG (DBG_proc, "kv_init_options: exit\n");

  dev->option_set = 1;
}


SANE_Status
kv_control_option (PKV_DEV dev, SANE_Int option,
		   SANE_Action action, void *val, SANE_Int * info)
{
  SANE_Status status;
  SANE_Word cap;
  SANE_String_Const name;
  int i;
  SANE_Word value;

  DBG (DBG_proc, "sane_control_option: enter, option %s, action %s\n",
       go_option_name[option], action == SANE_ACTION_GET_VALUE ? "R" : "W");

  if (info)
    {
      *info = 0;
    }

  if (dev->scanning)
    {
      return SANE_STATUS_DEVICE_BUSY;
    }

  if (option < 0 || option >= OPT_NUM_OPTIONS)
    {
      return SANE_STATUS_UNSUPPORTED;
    }

  cap = dev->opt[option].cap;
  if (!SANE_OPTION_IS_ACTIVE (cap))
    {
      return SANE_STATUS_UNSUPPORTED;
    }

  name = dev->opt[option].name;
  if (!name)
    {
      name = "(no name)";
    }
  if (action == SANE_ACTION_GET_VALUE)
    {
      switch (option)
	{
	  /* word options */
	case OPT_NUM_OPTS:
	case OPT_LONGPAPER:
	case OPT_LENGTHCTL:
	case OPT_DBLFEED:
	case OPT_RESOLUTION:
	case OPT_TL_Y:
	case OPT_BR_Y:
	case OPT_TL_X:
	case OPT_BR_X:
	case OPT_BRIGHTNESS:
	case OPT_CONTRAST:
	case OPT_DUPLEX:
	case OPT_LANDSCAPE:
	case OPT_AUTOMATIC_SEPARATION:
	case OPT_INVERSE:
	case OPT_MIRROR:
	case OPT_FEED_TIMEOUT:
	case OPT_JPEG:
	case OPT_ROTATE:
	case OPT_SWDESKEW:
	case OPT_SWDESPECK:
	case OPT_SWDEROTATE:
	case OPT_SWCROP:
	case OPT_SWSKIP:
	case OPT_FIT_TO_PAGE:
	  *(SANE_Word *) val = dev->val[option].w;
	  DBG (DBG_error, "opt value = %d\n", *(SANE_Word *) val);
	  return SANE_STATUS_GOOD;

	  /* string options */
	case OPT_MODE:
	case OPT_FEEDER_MODE:
	case OPT_SCAN_SOURCE:
	case OPT_MANUALFEED:
	case OPT_HALFTONE_PATTERN:
	case OPT_PAPER_SIZE:
	case OPT_AUTOMATIC_THRESHOLD:
	case OPT_WHITE_LEVEL:
	case OPT_NOISE_REDUCTION:
	case OPT_IMAGE_EMPHASIS:
	case OPT_GAMMA:
	case OPT_LAMP:

	  strcpy (val, dev->val[option].s);
	  DBG (DBG_error, "opt value = %s\n", (char *) val);
	  return SANE_STATUS_GOOD;

	default:
	  return SANE_STATUS_UNSUPPORTED;
	}
    }
  else if (action == SANE_ACTION_SET_VALUE)
    {
      if (!SANE_OPTION_IS_SETTABLE (cap))
	{
	  DBG (DBG_error,
	       "could not set option %s, not settable\n",
	       go_option_name[option]);
	  return SANE_STATUS_INVAL;
	}

      status = sanei_constrain_value (dev->opt + option, val, info);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (DBG_error, "could not set option, invalid value\n");
	  return status;
	}

      switch (option)
	{
	  /* Side-effect options */
	case OPT_TL_Y:
	case OPT_BR_Y:
	case OPT_RESOLUTION:
	  if (info)
	    {
	      *info |= SANE_INFO_RELOAD_PARAMS;
	    }

	  dev->val[option].w = *(SANE_Word *) val;

	  if (option == OPT_RESOLUTION)
	    {
	      if (round_to_boundry (&(dev->val[option].w),
				    dev->support_info.
				    step_resolution, 100, 600))
		{
		  if (info)
		    {
		      *info |= SANE_INFO_INEXACT;
		    }
		}
	    }
	  else if (option == OPT_TL_Y)
	    {
	      if (dev->val[option].w > dev->val[OPT_BR_Y].w)
		{
		  dev->val[option].w = dev->val[OPT_BR_Y].w;
		  if (info)
		    {
		      *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_INEXACT;
		    }
		}
	    }
	  else
	    {
	      if (dev->val[option].w < dev->val[OPT_TL_Y].w)
		{
		  dev->val[option].w = dev->val[OPT_TL_Y].w;
		  if (info)
		    {
		      *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_INEXACT;
		    }
		}
	    }

	  DBG (DBG_error,
	       "option %s, input = %d, value = %d\n",
	       go_option_name[option], (*(SANE_Word *) val),
	       dev->val[option].w);

	  return SANE_STATUS_GOOD;

	  /* The length of X must be rounded (up). */
	case OPT_TL_X:
	case OPT_BR_X:
	  {
	    SANE_Word xr = dev->val[OPT_RESOLUTION].w;
	    SANE_Word tl_x = mmToIlu (SANE_UNFIX (dev->val[OPT_TL_X].w)) * xr;
	    SANE_Word br_x = mmToIlu (SANE_UNFIX (dev->val[OPT_BR_X].w)) * xr;
	    value = mmToIlu (SANE_UNFIX (*(SANE_Word *) val)) * xr;	/* XR * W */

	    if (option == OPT_TL_X)
	      {
		SANE_Word max = KV_PIXEL_MAX * xr - KV_PIXEL_ROUND;
		if (br_x < max)
		  max = br_x;
		if (round_to_boundry (&value, KV_PIXEL_ROUND, 0, max))
		  {
		    if (info)
		      {
			*info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_INEXACT;
		      }
		  }
	      }
	    else
	      {
		if (round_to_boundry
		    (&value, KV_PIXEL_ROUND, tl_x, KV_PIXEL_MAX * xr))
		  {
		    if (info)
		      {
			*info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_INEXACT;
		      }
		  }
	      }

	    dev->val[option].w = SANE_FIX (iluToMm ((double) value / xr));

	    if (info)
	      {
		*info |= SANE_INFO_RELOAD_PARAMS;
	      }

	    DBG (DBG_error,
		 "option %s, input = %d, value = %d\n",
		 go_option_name[option], (*(SANE_Word *) val),
		 dev->val[option].w);
	    return SANE_STATUS_GOOD;
	  }
	case OPT_LANDSCAPE:
	  dev->val[option].w = *(SANE_Word *) val;
	  if (info)
	    {
	      *info |= SANE_INFO_RELOAD_PARAMS;
	    }
	  return SANE_STATUS_GOOD;

	  /* Side-effect free options */
	case OPT_CONTRAST:
	case OPT_BRIGHTNESS:
	case OPT_DUPLEX:
	case OPT_LONGPAPER:
	case OPT_LENGTHCTL:
	case OPT_DBLFEED:
	case OPT_INVERSE:
	case OPT_MIRROR:
	case OPT_AUTOMATIC_SEPARATION:
	case OPT_JPEG:
	case OPT_ROTATE:
	case OPT_SWDESKEW:
	case OPT_SWDESPECK:
	case OPT_SWDEROTATE:
	case OPT_SWCROP:
	case OPT_SWSKIP:
	case OPT_FIT_TO_PAGE:
	  dev->val[option].w = *(SANE_Word *) val;
	  return SANE_STATUS_GOOD;

	case OPT_FEED_TIMEOUT:
	  dev->val[option].w = *(SANE_Word *) val;
	  return CMD_set_timeout (dev, *(SANE_Word *) val);

	  /* String mode */
	case OPT_SCAN_SOURCE:
	case OPT_WHITE_LEVEL:
	case OPT_NOISE_REDUCTION:
	case OPT_IMAGE_EMPHASIS:
	case OPT_GAMMA:
	case OPT_LAMP:
	case OPT_HALFTONE_PATTERN:
	case OPT_FEEDER_MODE:
	  if (strcmp (dev->val[option].s, val) == 0)
	    return SANE_STATUS_GOOD;
	  free (dev->val[option].s);
	  dev->val[option].s = (SANE_String) strdup (val);

	  if (option == OPT_FEEDER_MODE &&
	      get_string_list_index (go_feeder_mode_list,
				     dev->val[option].s) == 1)
	    /* continuous mode */
	    {
	      free (dev->val[OPT_SCAN_SOURCE].s);
	      dev->val[OPT_SCAN_SOURCE].s = strdup (go_scan_source_list[0]);
	      dev->opt[OPT_LONGPAPER].cap &= ~SANE_CAP_INACTIVE;
	      if (info)
		*info |= SANE_INFO_RELOAD_OPTIONS;
	    }
	  else
	    {
	      dev->opt[OPT_LONGPAPER].cap |= SANE_CAP_INACTIVE;
	      if (info)
		*info |= SANE_INFO_RELOAD_OPTIONS;
	    }

	  if (option == OPT_SCAN_SOURCE &&
	      get_string_list_index (go_scan_source_list,
				     dev->val[option].s) == 1)
	    /* flatbed */
	    {
	      free (dev->val[OPT_FEEDER_MODE].s);
	      dev->val[OPT_FEEDER_MODE].s = strdup (go_feeder_mode_list[0]);
	    }

	  return SANE_STATUS_GOOD;

	case OPT_MODE:
	  if (strcmp (dev->val[option].s, val) == 0)
	    return SANE_STATUS_GOOD;
	  free (dev->val[OPT_MODE].s);
	  dev->val[OPT_MODE].s = (SANE_String) strdup (val);

	  /* Set default options for the scan modes. */
	  dev->opt[OPT_HALFTONE_PATTERN].cap |= SANE_CAP_INACTIVE;
	  dev->opt[OPT_AUTOMATIC_THRESHOLD].cap |= SANE_CAP_INACTIVE;
	  dev->opt[OPT_AUTOMATIC_SEPARATION].cap |= SANE_CAP_INACTIVE;
	  dev->opt[OPT_GAMMA].cap |= SANE_CAP_INACTIVE;
	  dev->opt[OPT_INVERSE].cap |= SANE_CAP_INACTIVE;
	  dev->opt[OPT_JPEG].cap &= ~SANE_CAP_INACTIVE;

	  if (strcmp (dev->val[OPT_MODE].s, go_scan_mode_list[0]) == 0)
	    /* binary */
	    {
	      dev->opt[OPT_AUTOMATIC_THRESHOLD].cap &= ~SANE_CAP_INACTIVE;
	      dev->opt[OPT_INVERSE].cap &= ~SANE_CAP_INACTIVE;
	      dev->opt[OPT_JPEG].cap |= SANE_CAP_INACTIVE;
	    }
	  else if (strcmp (dev->val[OPT_MODE].s, go_scan_mode_list[1]) == 0)
	    /* halftone */
	    {
	      dev->opt[OPT_HALFTONE_PATTERN].cap &= ~SANE_CAP_INACTIVE;
	      dev->opt[OPT_AUTOMATIC_SEPARATION].cap &= ~SANE_CAP_INACTIVE;
	      dev->opt[OPT_GAMMA].cap &= ~SANE_CAP_INACTIVE;
	      dev->opt[OPT_INVERSE].cap &= ~SANE_CAP_INACTIVE;
	      dev->opt[OPT_JPEG].cap |= SANE_CAP_INACTIVE;
	    }
	  else if (strcmp (dev->val[OPT_MODE].s, go_scan_mode_list[2]) == 0)
	    /* grayscale */
	    {
	      dev->opt[OPT_GAMMA].cap &= ~SANE_CAP_INACTIVE;
	    }

	  if (info)
	    {
	      *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	    }

	  return SANE_STATUS_GOOD;

	case OPT_MANUALFEED:
	  if (strcmp (dev->val[option].s, val) == 0)
	    return SANE_STATUS_GOOD;
	  free (dev->val[option].s);
	  dev->val[option].s = (SANE_String) strdup (val);

	  if (strcmp (dev->val[option].s, go_manual_feed_list[0]) == 0)	/* off */
	    dev->opt[OPT_FEED_TIMEOUT].cap |= SANE_CAP_INACTIVE;
	  else
	    dev->opt[OPT_FEED_TIMEOUT].cap &= ~SANE_CAP_INACTIVE;
	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS;

	  return SANE_STATUS_GOOD;

	case OPT_PAPER_SIZE:
	  if (strcmp (dev->val[option].s, val) == 0)
	    return SANE_STATUS_GOOD;

	  free (dev->val[OPT_PAPER_SIZE].s);
	  dev->val[OPT_PAPER_SIZE].s = (SANE_Char *) strdup (val);

	  i = get_string_list_index (go_paper_list,
				     dev->val[OPT_PAPER_SIZE].s);
	  if (i == 0)
	    {			/*user def */
	      dev->opt[OPT_TL_X].cap &=
		dev->opt[OPT_TL_Y].cap &=
		dev->opt[OPT_BR_X].cap &=
		dev->opt[OPT_BR_Y].cap &= ~SANE_CAP_INACTIVE;
	      dev->opt[OPT_LANDSCAPE].cap |= SANE_CAP_INACTIVE;
	      dev->val[OPT_LANDSCAPE].w = 0;
	    }
	  else
	    {
	      dev->opt[OPT_TL_X].cap |=
		dev->opt[OPT_TL_Y].cap |=
		dev->opt[OPT_BR_X].cap |=
		dev->opt[OPT_BR_Y].cap |= SANE_CAP_INACTIVE;
	      if (i == 4 || i == 5 || i == 7)
		{		/*A5, A6 or B6 */
		  dev->opt[OPT_LANDSCAPE].cap &= ~SANE_CAP_INACTIVE;
		}
	      else
		{
		  dev->opt[OPT_LANDSCAPE].cap |= SANE_CAP_INACTIVE;
		  dev->val[OPT_LANDSCAPE].w = 0;
		}
	    }

	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;

	  return SANE_STATUS_GOOD;


	case OPT_AUTOMATIC_THRESHOLD:
	  if (strcmp (dev->val[option].s, val) == 0)
	    return SANE_STATUS_GOOD;

	  free (dev->val[option].s);
	  dev->val[option].s = (SANE_Char *) strdup (val);

	  /* If the threshold is not set to none, some option must
	   * disappear. */

	  dev->opt[OPT_WHITE_LEVEL].cap |= SANE_CAP_INACTIVE;
	  dev->opt[OPT_NOISE_REDUCTION].cap |= SANE_CAP_INACTIVE;
	  dev->opt[OPT_IMAGE_EMPHASIS].cap |= SANE_CAP_INACTIVE;
	  dev->opt[OPT_AUTOMATIC_SEPARATION].cap |= SANE_CAP_INACTIVE;
	  dev->opt[OPT_HALFTONE_PATTERN].cap |= SANE_CAP_INACTIVE;

	  if (strcmp (val, go_automatic_threshold_list[0]) == 0)
	    {
	      dev->opt[OPT_WHITE_LEVEL].cap &= ~SANE_CAP_INACTIVE;
	      dev->opt[OPT_NOISE_REDUCTION].cap &= ~SANE_CAP_INACTIVE;
	      dev->opt[OPT_IMAGE_EMPHASIS].cap &= ~SANE_CAP_INACTIVE;
	      dev->opt[OPT_AUTOMATIC_SEPARATION].cap &= ~SANE_CAP_INACTIVE;
	      if (strcmp (dev->val[OPT_MODE].s, go_scan_mode_list[1]) == 0)
		{
		  dev->opt[OPT_HALFTONE_PATTERN].cap &= ~SANE_CAP_INACTIVE;
		}
	    }

	  if (info)
	    {
	      *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	    }
	  return SANE_STATUS_GOOD;

	default:
	  return SANE_STATUS_INVAL;
	}
    }

  DBG (DBG_proc, "sane_control_option: exit, bad\n");

  return SANE_STATUS_UNSUPPORTED;
}

/* Display a buffer in the log. */
void
hexdump (int level, const char *comment, unsigned char *p, int l)
{
  int i;
  char line[128];
  char *ptr;

  DBG (level, "%s\n", comment);
  ptr = line;
  for (i = 0; i < l; i++, p++)
    {
      if ((i % 16) == 0)
	{
	  if (ptr != line)
	    {
	      *ptr = '\0';
	      DBG (level, "%s\n", line);
	      ptr = line;
	    }
	  sprintf (ptr, "%3.3d:", i);
	  ptr += 4;
	}
      sprintf (ptr, " %2.2x", *p);
      ptr += 3;
    }
  *ptr = '\0';
  DBG (level, "%s\n", line);
}

/* Set window data */
void
kv_set_window_data (PKV_DEV dev,
		    KV_SCAN_MODE scan_mode,
		    int side, unsigned char *windowdata)
{
  int paper = go_paper_val[get_string_list_index (go_paper_list,
						  dev->val[OPT_PAPER_SIZE].
						  s)];

  /* Page side */
  windowdata[0] = side;

  /* X and Y resolution */
  Ito16 (dev->val[OPT_RESOLUTION].w, &windowdata[2]);
  Ito16 (dev->val[OPT_RESOLUTION].w, &windowdata[4]);

  /* Width and length */
  if (paper == 0)
    {				/* Non-standard document */
      int x_tl = mmToIlu (SANE_UNFIX (dev->val[OPT_TL_X].w));
      int y_tl = mmToIlu (SANE_UNFIX (dev->val[OPT_TL_Y].w));
      int x_br = mmToIlu (SANE_UNFIX (dev->val[OPT_BR_X].w));
      int y_br = mmToIlu (SANE_UNFIX (dev->val[OPT_BR_Y].w));
      int width = x_br - x_tl;
      int length = y_br - y_tl;
      /* Upper Left (X,Y) */
      Ito32 (x_tl, &windowdata[6]);
      Ito32 (y_tl, &windowdata[10]);

      Ito32 (width, &windowdata[14]);
      Ito32 (length, &windowdata[18]);
      Ito32 (width, &windowdata[48]);	/* device specific */
      Ito32 (length, &windowdata[52]);	/* device specific */
    }

  /* Brightness */
  windowdata[22] = 255 - GET_OPT_VAL_W (dev, OPT_BRIGHTNESS);
  windowdata[23] = windowdata[22];	/* threshold, same as brightness. */

  /* Contrast */
  windowdata[24] = GET_OPT_VAL_W (dev, OPT_CONTRAST);

  /* Image Composition */
  windowdata[25] = (unsigned char) scan_mode;

  /* Depth */
  windowdata[26] = kv_get_depth (scan_mode);

  /* Halftone pattern. */
  if (scan_mode == SM_DITHER)
    {
      windowdata[28] = GET_OPT_VAL_L (dev, OPT_HALFTONE_PATTERN,
				      halftone_pattern);
    }

  /* Inverse */
  if (scan_mode == SM_BINARY || scan_mode == SM_DITHER)
    {
      windowdata[29] = GET_OPT_VAL_W (dev, OPT_INVERSE);
    }

  /* Bit ordering */
  windowdata[31] = 1;

  /*Compression Type */
  if (!(dev->opt[OPT_JPEG].cap & SANE_CAP_INACTIVE)
      && GET_OPT_VAL_W (dev, OPT_JPEG))
    {
      windowdata[32] = 0x81;	/*jpeg */
      /*Compression Argument */
      windowdata[33] = GET_OPT_VAL_W (dev, OPT_JPEG);
    }

  /* Gamma */
  if (scan_mode == SM_DITHER || scan_mode == SM_GRAYSCALE)
    {
      windowdata[44] = GET_OPT_VAL_L (dev, OPT_GAMMA, gamma);
    }

  /* Feeder mode */
  windowdata[57] = GET_OPT_VAL_L (dev, OPT_FEEDER_MODE, feeder_mode);

  /* Stop skew -- disabled */
  windowdata[41] = 0;

  /* Scan source */
  if (GET_OPT_VAL_L (dev, OPT_SCAN_SOURCE, scan_source))
    {				/* flatbed */
      windowdata[41] |= 0x80;
    }
  else
    {
      windowdata[41] &= 0x7f;
    }

  /* Paper size */
  windowdata[47] = paper;

  if (paper)			/* Standard Document */
    windowdata[47] |= 1 << 7;

  /* Long paper */
  if (GET_OPT_VAL_W (dev, OPT_LONGPAPER))
    {
      windowdata[47] |= 0x20;
    }

  /* Length control */
  if (GET_OPT_VAL_W (dev, OPT_LENGTHCTL))
    {
      windowdata[47] |= 0x40;
    }

  /* Landscape */
  if (GET_OPT_VAL_W (dev, OPT_LANDSCAPE))
    {
      windowdata[47] |= 1 << 4;
    }
  /* Double feed */
  if (GET_OPT_VAL_W (dev, OPT_DBLFEED))
    {
      windowdata[56] = 0x10;
    }

  /* Fit to page */
  if (GET_OPT_VAL_W (dev, OPT_FIT_TO_PAGE))
    {
      windowdata[56] |= 1 << 2;
    }

  /* Manual feed */
  windowdata[62] = GET_OPT_VAL_L (dev, OPT_MANUALFEED, manual_feed) << 6;

  /* Mirror image */
  if (GET_OPT_VAL_W (dev, OPT_MIRROR))
    {
      windowdata[42] = 0x80;
    }

  /* Image emphasis */
  windowdata[43] = GET_OPT_VAL_L (dev, OPT_IMAGE_EMPHASIS, image_emphasis);

  /* White level */
  windowdata[60] = GET_OPT_VAL_L (dev, OPT_WHITE_LEVEL, white_level);

  if (scan_mode == SM_BINARY || scan_mode == SM_DITHER)
    {
      /* Noise reduction */
      windowdata[61] = GET_OPT_VAL_L (dev, OPT_NOISE_REDUCTION,
				      noise_reduction);

      /* Automatic separation */
      if (scan_mode == SM_DITHER && GET_OPT_VAL_W (dev,
						   OPT_AUTOMATIC_SEPARATION))
	{
	  windowdata[59] = 0x80;
	}
    }

  /* Automatic threshold. Must be last because it may override
   * some previous options. */
  if (scan_mode == SM_BINARY)
    {
      windowdata[58] =
	GET_OPT_VAL_L (dev, OPT_AUTOMATIC_THRESHOLD, automatic_threshold);
    }

  if (windowdata[58] != 0)
    {
      /* Automatic threshold is enabled. */
      windowdata[22] = 0;	/* brightness. */
      windowdata[23] = 0;	/* threshold, same as brightness. */
      windowdata[24] = 0;	/* contrast */
      windowdata[27] = windowdata[28] = 0;	/* Halftone pattern. */
      windowdata[43] = 0;	/* Image emphasis */
      windowdata[59] = 0;	/* Automatic separation */
      windowdata[60] = 0;	/* White level */
      windowdata[61] = 0;	/* Noise reduction */
    }

  /* lamp -- color dropout */
  windowdata[45] = GET_OPT_VAL_L (dev, OPT_LAMP, lamp) << 4;

  /*Stop Mode:    After 1 page */
  windowdata[63] = 1;
}
