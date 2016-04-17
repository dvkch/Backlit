/*
   Copyright (C) 2009, Panasonic Russia Ltd.
   Copyright (C) 2010,2011, m. allan noah
*/
/*
   Panasonic KV-S40xx USB-SCSI scanner driver.
*/

#include "../include/sane/config.h"

#include <string.h>
#define DEBUG_DECLARE_ONLY
#define BACKEND_NAME kvs40xx

#include "../include/sane/sanei_backend.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei.h"
#include "../include/sane/sanei_config.h"
#include "lassert.h"

#include "kvs40xx.h"

#include "../include/sane/sanei_debug.h"


static inline unsigned
mm2scanner_units (unsigned mm)
{
  return (mm * 12000 / 254.0 + .5);
}
static inline unsigned
scanner_units2mm (unsigned u)
{
  return (u * 254.0 / 12000 + .5);
}
struct restriction
{
  unsigned ux, uy, ux_pix, uy_pix;
};

static struct restriction flatbad = { 14064, 20400, 7031, 63999 };
static struct restriction cw = { 14268, 128000, 7133, 63999 };
static struct restriction cl = { 10724, 128000, 5361, 63999 };

static inline int
check_area (struct scanner *s, unsigned ux,
	    unsigned uy, unsigned bx, unsigned by)
{
  int fb = !strcmp (s->val[SOURCE].s, SANE_I18N ("fb"));
  struct restriction *r = fb ? &flatbad
    : (s->id == KV_S4085CL || s->id == KV_S4065CL) ? &cl : &cw;
  unsigned res = s->val[RESOLUTION].w;
  unsigned w = bx - ux;
  unsigned h = by - uy;
  unsigned c1 = mm2scanner_units (ux + w);
  unsigned c2 = mm2scanner_units (uy + h);
  int c = c1 <= r->ux && c1 >= 16 && c2 >= 1 && c2 <= r->uy ? 0 : -1;
  if (c)
    return c;
  if (mm2scanner_units (ux) > r->ux)
    return -1;
  if (res * mm2scanner_units (ux) / 1200 > r->ux_pix)
    return -1;

  if (res * mm2scanner_units (uy) / 1200 > r->uy_pix)
    return -1;
  return 0;
}

static size_t
max_string_size (const SANE_String_Const strings[])
{
  size_t size, max_size = 0;
  SANE_Int i;

  for (i = 0; strings[i]; ++i)
    {
      size = strlen (strings[i]) + 1;
      if (size > max_size)
	max_size = size;
    }
  return max_size;
}

static SANE_String_Const mode_list[] = {
  SANE_VALUE_SCAN_MODE_LINEART,
  SANE_VALUE_SCAN_MODE_GRAY,
  SANE_VALUE_SCAN_MODE_COLOR,
  NULL
};
static const unsigned mode_val[] = { 0, 2, 5 };
static const unsigned bps_val[] = { 1, 8, 24 };

static const SANE_Range resolutions_range = {
  100,600,1
};

/* List of feeder modes */
static SANE_String_Const feeder_mode_list[] = {
  SANE_I18N ("single"),
  SANE_I18N ("continuous"),
  NULL
};

/* List of scan sources */
static SANE_String_Const source_list[] = {
  SANE_I18N ("adf"),
  SANE_I18N ("fb"),
  NULL
};

/* List of manual feed mode */
static SANE_String_Const manual_feed_list[] = {
  SANE_I18N ("off"),
  SANE_I18N ("wait_doc"),
  SANE_I18N ("wait_doc_hopper_up"),
  SANE_I18N ("wait_key"),
  NULL
};

/* List of paper sizes */
static SANE_String_Const paper_list[] = {
  SANE_I18N ("user_def"),
  SANE_I18N ("business_card"),
  SANE_I18N ("Check"),
  SANE_I18N ("A3"),
  SANE_I18N ("A4"),
  SANE_I18N ("A5"),
  SANE_I18N ("A6"),
  SANE_I18N ("Letter"),
  SANE_I18N ("Double letter 11x17 in"),
  SANE_I18N ("B4"),
  SANE_I18N ("B5"),
  SANE_I18N ("B6"),
  SANE_I18N ("Legal"),
  NULL
};

static SANE_String_Const paper_list_woA3[] = {
  SANE_I18N ("user_def"),
  SANE_I18N ("business_card"),
  SANE_I18N ("Check"),
  /*SANE_I18N ("A3"), */
  SANE_I18N ("A4"),
  SANE_I18N ("A5"),
  SANE_I18N ("A6"),
  SANE_I18N ("Letter"),
  /*SANE_I18N ("Double letter 11x17 in"), */
  /*SANE_I18N ("B4"), */
  SANE_I18N ("B5"),
  SANE_I18N ("B6"),
  SANE_I18N ("Legal"),
  NULL
};

static const unsigned paper_val[] = { 0, 1, 2, 3, 4, 5, 6, 7,
  9, 12, 13, 14, 15
};

struct paper_size
{
  int width;
  int height;
};
static const struct paper_size paper_sizes[] = {
  {210, 297},			/* User defined, default=A4 */
  {54, 90},			/* Business card */
  {80, 170},			/* Check (China business) */
  {297, 420},			/* A3 */
  {210, 297},			/* A4 */
  {148, 210},			/* A5 */
  {105, 148},			/* A6 */
  {215, 280},			/* US Letter 8.5 x 11 in */
  {280, 432},			/* Double Letter 11 x 17 in */
  {250, 353},			/* B4 */
  {176, 250},			/* B5 */
  {125, 176},			/* B6 */
  {215, 355}			/* US Legal */
};

#define MIN_WIDTH	48
#define MIN_LENGTH	70
#define MAX_WIDTH	297
#define MAX_LENGTH	432

#define MAX_WIDTH_A4	227
#define MAX_LENGTH_A4	432

static SANE_Range tl_x_range = { 0, MAX_WIDTH - MIN_WIDTH, 0 };
static SANE_Range tl_y_range = { 0, MAX_LENGTH - MIN_LENGTH, 0 };
static SANE_Range br_x_range = { MIN_WIDTH, MAX_WIDTH, 0 };
static SANE_Range br_y_range = { MIN_LENGTH, MAX_LENGTH, 0 };

static SANE_Range tl_x_range_A4 = { 0, MAX_WIDTH_A4 - MIN_WIDTH, 0 };
static SANE_Range tl_y_range_A4 = { 0, MAX_LENGTH_A4 - MIN_LENGTH, 0 };
static SANE_Range br_x_range_A4 = { MIN_WIDTH, MAX_WIDTH_A4, 0 };
static SANE_Range br_y_range_A4 = { MIN_LENGTH, MAX_LENGTH_A4, 0 };

static SANE_Range byte_value_range = { 0, 255, 0 };
static SANE_Range compression_value_range = { 1, 0x64, 0 };

/* List of image emphasis options, 5 steps */
static SANE_String_Const image_emphasis_list[] = {
  SANE_I18N ("none"),
  SANE_I18N ("low"),
  SANE_I18N ("medium"),
  SANE_I18N ("high"),
  SANE_I18N ("smooth"),
  NULL
};

/* List of gamma */
static SANE_String_Const gamma_list[] = {
  SANE_I18N ("normal"),
  SANE_I18N ("crt"),
  NULL
};
static unsigned gamma_val[] = { 0, 1 };

/* List of lamp color dropout */
static SANE_String_Const lamp_list[] = {
  SANE_I18N ("normal"),
  SANE_I18N ("red"),
  SANE_I18N ("green"),
  SANE_I18N ("blue"),
  NULL
};
static SANE_String_Const dfeed_sence_list[] = {
  SANE_I18N ("Normal"),
  SANE_I18N ("High sensivity"),
  SANE_I18N ("Low sensivity"),
  NULL
};

/* Lists of supported halftone. They are only valid with
 * for the Black&White mode. */
static SANE_String_Const halftone_pattern[] = {
  SANE_I18N ("bayer_64"),
  SANE_I18N ("bayer_16"),
  SANE_I18N ("halftone_32"),
  SANE_I18N ("halftone_64"),
  SANE_I18N ("err_diffusion"),
  NULL
};

/*  Stapled document */
static SANE_String_Const stapeled_list[] = {
  SANE_I18N ("No detection"),
  SANE_I18N ("Normal mode"),
  SANE_I18N ("Enhanced mode"),
  NULL
};


/* List of automatic threshold options */
static SANE_String_Const automatic_threshold_list[] = {
  SANE_I18N ("normal"),
  SANE_I18N ("light"),
  SANE_I18N ("dark"),
  NULL
};
static const int automatic_threshold_val[] = {
  0,
  0x11,
  0x1f
};

/* List of white level base. */
static SANE_String_Const white_level_list[] = {
  SANE_I18N ("From scanner"),
  SANE_I18N ("From paper"),
  SANE_I18N ("Automatic"),
  NULL
};
static const int white_level_val[] = {
  0x00,
  0x80,
  0x81
};

/* List of noise reduction options. */
static SANE_String_Const noise_reduction_list[] = {
  SANE_I18N ("default"),
  "1x1",
  "2x2",
  "3x3",
  "4x4",
  "5x5",
  NULL
};

/* Reset the options for that scanner. */
void
kvs40xx_init_options (struct scanner *s)
{
  int i;
  SANE_Option_Descriptor *o;
  /* Pre-initialize the options. */
  memset (s->opt, 0, sizeof (s->opt));
  memset (s->val, 0, sizeof (s->val));

  for (i = 0; i < NUM_OPTIONS; i++)
    {
      s->opt[i].size = sizeof (SANE_Word);
      s->opt[i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }

  /* Number of options. */
  o = &s->opt[NUM_OPTS];
  o->name = "";
  o->title = SANE_TITLE_NUM_OPTIONS;
  o->desc = SANE_DESC_NUM_OPTIONS;
  o->type = SANE_TYPE_INT;
  o->cap = SANE_CAP_SOFT_DETECT;
  s->val[NUM_OPTS].w = NUM_OPTIONS;

  /* Mode group */
  o = &s->opt[MODE_GROUP];
  o->title = SANE_I18N ("Scan Mode");
  o->desc = "";			/* not valid for a group */
  o->type = SANE_TYPE_GROUP;
  o->cap = 0;
  o->size = 0;
  o->constraint_type = SANE_CONSTRAINT_NONE;

  /* Scanner supported modes */
  o = &s->opt[MODE];
  o->name = SANE_NAME_SCAN_MODE;
  o->title = SANE_TITLE_SCAN_MODE;
  o->desc = SANE_DESC_SCAN_MODE;
  o->type = SANE_TYPE_STRING;
  o->size = max_string_size (mode_list);
  o->constraint_type = SANE_CONSTRAINT_STRING_LIST;
  o->constraint.string_list = mode_list;
  s->val[MODE].s = malloc (o->size);
  strcpy (s->val[MODE].s, mode_list[2]);

  /* X and Y resolution */
  o = &s->opt[RESOLUTION];
  o->name = SANE_NAME_SCAN_RESOLUTION;
  o->title = SANE_TITLE_SCAN_RESOLUTION;
  o->desc = SANE_DESC_SCAN_RESOLUTION;
  o->type = SANE_TYPE_INT;
  o->unit = SANE_UNIT_DPI;
  o->constraint_type = SANE_CONSTRAINT_RANGE;
  o->constraint.range = &resolutions_range;
  s->val[RESOLUTION].w = 100;

  /* Duplex */
  o = &s->opt[DUPLEX];
  o->name = "duplex";
  o->title = SANE_I18N ("Duplex");
  o->desc = SANE_I18N ("Enable Duplex (Dual-Sided) Scanning");
  o->type = SANE_TYPE_BOOL;
  o->unit = SANE_UNIT_NONE;
  s->val[DUPLEX].w = SANE_FALSE;

  /*FIXME 
     if (!s->support_info.support_duplex)
     o->cap |= SANE_CAP_INACTIVE;
   */

  /* Feeder mode */
  o = &s->opt[FEEDER_MODE];
  o->name = "feeder-mode";
  o->title = SANE_I18N ("Feeder mode");
  o->desc = SANE_I18N ("Sets the feeding mode");
  o->type = SANE_TYPE_STRING;
  o->size = max_string_size (feeder_mode_list);
  o->constraint_type = SANE_CONSTRAINT_STRING_LIST;
  o->constraint.string_list = feeder_mode_list;
  s->val[FEEDER_MODE].s = malloc (o->size);
  strcpy (s->val[FEEDER_MODE].s, feeder_mode_list[0]);

  /* Scan source */
  o = &s->opt[SOURCE];
  o->name = SANE_NAME_SCAN_SOURCE;
  o->title = SANE_TITLE_SCAN_SOURCE;
  o->desc = SANE_DESC_SCAN_SOURCE;
  o->type = SANE_TYPE_STRING;
  o->size = max_string_size (source_list);
  o->constraint_type = SANE_CONSTRAINT_STRING_LIST;
  o->constraint.string_list = source_list;
  s->val[SOURCE].s = malloc (o->size);
  strcpy (s->val[SOURCE].s, source_list[0]);
  if (s->id != KV_S7075C)
    o->cap |= SANE_CAP_INACTIVE;

  /* Length control */
  o = &s->opt[LENGTHCTL];
  o->name = "length-control";
  o->title = SANE_I18N ("Length control mode");
  o->desc =
    SANE_I18N
    ("Length Control Mode is a mode that the scanner reads up to the shorter length of actual"
     " paper or logical document length.");
  o->type = SANE_TYPE_BOOL;
  o->unit = SANE_UNIT_NONE;
  s->val[LENGTHCTL].w = SANE_FALSE;

  o = &s->opt[LONG_PAPER];
  o->name = "long-paper";
  o->title = SANE_I18N ("Long paper mode");
  o->desc = SANE_I18N ("Long Paper Mode is a mode that the scanner "
		       "reads the image after it divides long paper "
		       "by the length which is set in Document Size option.");
  o->type = SANE_TYPE_BOOL;
  o->unit = SANE_UNIT_NONE;
  s->val[LONG_PAPER].w = SANE_FALSE;
  o->cap |= SANE_CAP_INACTIVE;

  /* Manual feed */
  o = &s->opt[MANUALFEED];
  o->name = "manual-feed";
  o->title = SANE_I18N ("Manual feed mode");
  o->desc = SANE_I18N ("Sets the manual feed mode");
  o->type = SANE_TYPE_STRING;
  o->size = max_string_size (manual_feed_list);
  o->constraint_type = SANE_CONSTRAINT_STRING_LIST;
  o->constraint.string_list = manual_feed_list;
  s->val[MANUALFEED].s = malloc (o->size);
  strcpy (s->val[MANUALFEED].s, manual_feed_list[0]);

  /*Manual feed timeout */
  o = &s->opt[FEED_TIMEOUT];
  o->name = "feed-timeout";
  o->title = SANE_I18N ("Manual feed timeout");
  o->desc = SANE_I18N ("Sets the manual feed timeout in seconds");
  o->type = SANE_TYPE_INT;
  o->unit = SANE_UNIT_NONE;
  o->size = sizeof (SANE_Int);
  o->constraint_type = SANE_CONSTRAINT_RANGE;
  o->constraint.range = &(byte_value_range);
  o->cap |= SANE_CAP_INACTIVE;
  s->val[FEED_TIMEOUT].w = 30;

  /* Double feed */
  o = &s->opt[DBLFEED];
  o->name = "dfeed";
  o->title = SANE_I18N ("Double feed detection");
  o->desc = SANE_I18N ("Enable/Disable double feed detection");
  o->type = SANE_TYPE_BOOL;
  o->unit = SANE_UNIT_NONE;
  s->val[DBLFEED].w = SANE_FALSE;

  o = &s->opt[DFEED_SENCE];
  o->name = "dfeed-sense";
  o->title = SANE_I18N ("Double feed detector sensitivity");
  o->desc = SANE_I18N ("Set the double feed detector sensitivity");
  o->type = SANE_TYPE_STRING;
  o->size = max_string_size (dfeed_sence_list);
  o->constraint_type = SANE_CONSTRAINT_STRING_LIST;
  o->constraint.string_list = dfeed_sence_list;
  s->val[DFEED_SENCE].s = malloc (o->size);
  strcpy (s->val[DFEED_SENCE].s, dfeed_sence_list[0]);
  o->cap |= SANE_CAP_INACTIVE;

  o = &s->opt[DFSTOP];
  o->name = "dfstop";
  o->title = SANE_I18N ("Do not stop after double feed detection");
  o->desc = SANE_I18N ("Do not stop after double feed detection");
  o->type = SANE_TYPE_BOOL;
  o->unit = SANE_UNIT_NONE;
  s->val[DFSTOP].w = SANE_FALSE;
  o->cap |= SANE_CAP_INACTIVE;

  o = &s->opt[DFEED_L];
  o->name = "dfeed_l";
  o->title = SANE_I18N ("Ignore left double feed sensor");
  o->desc = SANE_I18N ("Ignore left double feed sensor");
  o->type = SANE_TYPE_BOOL;
  o->unit = SANE_UNIT_NONE;
  s->val[DFEED_L].w = SANE_FALSE;
  o->cap |= SANE_CAP_INACTIVE;

  o = &s->opt[DFEED_C];
  o->name = "dfeed_c";
  o->title = SANE_I18N ("Ignore center double feed sensor");
  o->desc = SANE_I18N ("Ignore center double feed sensor");
  o->type = SANE_TYPE_BOOL;
  o->unit = SANE_UNIT_NONE;
  s->val[DFEED_C].w = SANE_FALSE;
  o->cap |= SANE_CAP_INACTIVE;

  o = &s->opt[DFEED_R];
  o->name = "dfeed_r";
  o->title = SANE_I18N ("Ignore right double feed sensor");
  o->desc = SANE_I18N ("Ignore right double feed sensor");
  o->type = SANE_TYPE_BOOL;
  o->unit = SANE_UNIT_NONE;
  s->val[DFEED_R].w = SANE_FALSE;
  o->cap |= SANE_CAP_INACTIVE;

  /* Fit to page */
  o = &s->opt[FIT_TO_PAGE];
  o->name = SANE_I18N ("fit-to-page");
  o->title = SANE_I18N ("Fit to page");
  o->desc = SANE_I18N ("Scanner shrinks image to fit scanned page");
  o->type = SANE_TYPE_BOOL;
  o->unit = SANE_UNIT_NONE;
  s->val[FIT_TO_PAGE].w = SANE_FALSE;

  /* Geometry group */
  o = &s->opt[GEOMETRY_GROUP];
  o->title = SANE_I18N ("Geometry");
  o->desc = "";			/* not valid for a group */
  o->type = SANE_TYPE_GROUP;
  o->cap = 0;
  o->size = 0;
  o->constraint_type = SANE_CONSTRAINT_NONE;

  /* Paper sizes list */
  o = &s->opt[PAPER_SIZE];
  o->name = "paper-size";
  o->title = SANE_I18N ("Paper size");
  o->desc = SANE_I18N ("Physical size of the paper in the ADF");
  o->type = SANE_TYPE_STRING;
  o->constraint.string_list =
    s->id == KV_S4085CL || s->id == KV_S4065CL ? paper_list_woA3 : paper_list;


  o->size = max_string_size (o->constraint.string_list);
  o->constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->val[PAPER_SIZE].s = malloc (o->size);
  strcpy (s->val[PAPER_SIZE].s, SANE_I18N ("A4"));

  /* Landscape */
  o = &s->opt[LANDSCAPE];
  o->name = "landscape";
  o->title = SANE_I18N ("Landscape");
  o->desc =
    SANE_I18N ("Set paper position : "
	       "true for landscape, false for portrait");
  o->type = SANE_TYPE_BOOL;
  o->unit = SANE_UNIT_NONE;
  s->val[LANDSCAPE].w = SANE_FALSE;

  /* Upper left X */
  o = &s->opt[TL_X];
  o->name = SANE_NAME_SCAN_TL_X;
  o->title = SANE_TITLE_SCAN_TL_X;
  o->desc = SANE_DESC_SCAN_TL_X;
  o->type = SANE_TYPE_INT;
  o->unit = SANE_UNIT_MM;
  o->constraint_type = SANE_CONSTRAINT_RANGE;
  o->constraint.range =
    (s->id == KV_S4085CL || s->id == KV_S4065CL)
    ? &tl_x_range_A4 : &tl_x_range;
  o->cap |= SANE_CAP_INACTIVE;
  s->val[TL_X].w = 0;

  /* Upper left Y */
  o = &s->opt[TL_Y];
  o->name = SANE_NAME_SCAN_TL_Y;
  o->title = SANE_TITLE_SCAN_TL_Y;
  o->desc = SANE_DESC_SCAN_TL_Y;
  o->type = SANE_TYPE_INT;
  o->unit = SANE_UNIT_MM;
  o->constraint_type = SANE_CONSTRAINT_RANGE;
  o->constraint.range =
    (s->id == KV_S4085CL || s->id == KV_S4065CL)
    ? &tl_y_range_A4 : &tl_y_range;
  o->cap |= SANE_CAP_INACTIVE;
  s->val[TL_Y].w = 0;

  /* Bottom-right x */
  o = &s->opt[BR_X];
  o->name = SANE_NAME_SCAN_BR_X;
  o->title = SANE_TITLE_SCAN_BR_X;
  o->desc = SANE_DESC_SCAN_BR_X;
  o->type = SANE_TYPE_INT;
  o->unit = SANE_UNIT_MM;
  o->constraint_type = SANE_CONSTRAINT_RANGE;
  o->constraint.range =
    (s->id == KV_S4085CL || s->id == KV_S4065CL)
    ? &br_x_range_A4 : &br_x_range;
  o->cap |= SANE_CAP_INACTIVE;
  s->val[BR_X].w = 210;

  /* Bottom-right y */
  o = &s->opt[BR_Y];
  o->name = SANE_NAME_SCAN_BR_Y;
  o->title = SANE_TITLE_SCAN_BR_Y;
  o->desc = SANE_DESC_SCAN_BR_Y;
  o->type = SANE_TYPE_INT;
  o->unit = SANE_UNIT_MM;
  o->constraint_type = SANE_CONSTRAINT_RANGE;
  o->constraint.range =
    (s->id == KV_S4085CL || s->id == KV_S4065CL)
    ? &br_y_range_A4 : &br_y_range;
  o->cap |= SANE_CAP_INACTIVE;
  s->val[BR_Y].w = 297;

  /* Enhancement group */
  o = &s->opt[ADVANCED_GROUP];
  o->title = SANE_I18N ("Advanced");
  o->desc = "";			/* not valid for a group */
  o->type = SANE_TYPE_GROUP;
  o->cap = SANE_CAP_ADVANCED;
  o->size = 0;
  o->constraint_type = SANE_CONSTRAINT_NONE;

  /* Brightness */
  o = &s->opt[BRIGHTNESS];
  o->name = SANE_NAME_BRIGHTNESS;
  o->title = SANE_TITLE_BRIGHTNESS;
  o->desc = SANE_DESC_BRIGHTNESS;
  o->type = SANE_TYPE_INT;
  o->unit = SANE_UNIT_NONE;
  o->size = sizeof (SANE_Int);
  o->constraint_type = SANE_CONSTRAINT_RANGE;
  o->constraint.range = &(byte_value_range);
  s->val[BRIGHTNESS].w = 128;

  /* Contrast */
  o = &s->opt[CONTRAST];
  o->name = SANE_NAME_CONTRAST;
  o->title = SANE_TITLE_CONTRAST;
  o->desc = SANE_DESC_CONTRAST;
  o->type = SANE_TYPE_INT;
  o->unit = SANE_UNIT_NONE;
  o->size = sizeof (SANE_Int);
  o->constraint_type = SANE_CONSTRAINT_RANGE;
  o->constraint.range = &(byte_value_range);
  s->val[CONTRAST].w = 128;

  /* threshold */
  o = &s->opt[THRESHOLD];
  o->name = SANE_NAME_THRESHOLD;
  o->title = SANE_TITLE_THRESHOLD;
  o->desc = SANE_DESC_THRESHOLD;
  o->type = SANE_TYPE_INT;
  o->size = sizeof (SANE_Int);
  o->constraint_type = SANE_CONSTRAINT_RANGE;
  o->constraint.range = &(byte_value_range);
  s->val[THRESHOLD].w = 128;
  o->cap |= SANE_CAP_INACTIVE;

  o = &s->opt[AUTOMATIC_THRESHOLD];
  o->name = "athreshold";
  o->title = SANE_I18N ("Automatic threshold mode");
  o->desc = SANE_I18N ("Sets the automatic threshold mode");
  o->type = SANE_TYPE_STRING;
  o->size = max_string_size (automatic_threshold_list);
  o->constraint_type = SANE_CONSTRAINT_STRING_LIST;
  o->constraint.string_list = automatic_threshold_list;
  s->val[AUTOMATIC_THRESHOLD].s = malloc (o->size);
  strcpy (s->val[AUTOMATIC_THRESHOLD].s, automatic_threshold_list[0]);
  o->cap |= SANE_CAP_INACTIVE;

  /* Image emphasis */
  o = &s->opt[IMAGE_EMPHASIS];
  o->name = "image-emphasis";
  o->title = SANE_I18N ("Image emphasis");
  o->desc = SANE_I18N ("Sets the image emphasis");
  o->type = SANE_TYPE_STRING;
  o->size = max_string_size (image_emphasis_list);
  o->constraint_type = SANE_CONSTRAINT_STRING_LIST;
  o->constraint.string_list = image_emphasis_list;
  s->val[IMAGE_EMPHASIS].s = malloc (o->size);
  strcpy (s->val[IMAGE_EMPHASIS].s, image_emphasis_list[0]);;
  o->cap |= SANE_CAP_INACTIVE;

  /* Gamma */
  o = &s->opt[GAMMA_CORRECTION];
  o->name = "gamma-cor";
  o->title = SANE_I18N ("Gamma correction");
  o->desc = SANE_I18N ("Gamma correction");
  o->type = SANE_TYPE_STRING;
  o->size = max_string_size (gamma_list);
  o->constraint_type = SANE_CONSTRAINT_STRING_LIST;
  o->constraint.string_list = gamma_list;
  s->val[GAMMA_CORRECTION].s = malloc (o->size);
  strcpy (s->val[GAMMA_CORRECTION].s, gamma_list[0]);
  o->cap |= SANE_CAP_INACTIVE;

  /* Lamp color dropout */
  o = &s->opt[LAMP];
  o->name = "lamp-color";
  o->title = SANE_I18N ("Lamp color");
  o->desc = SANE_I18N ("Sets the lamp color (color dropout)");
  o->type = SANE_TYPE_STRING;
  o->size = max_string_size (lamp_list);
  o->constraint_type = SANE_CONSTRAINT_STRING_LIST;
  o->constraint.string_list = lamp_list;
  s->val[LAMP].s = malloc (o->size);
  strcpy (s->val[LAMP].s, lamp_list[0]);

  /* Inverse image */
  o = &s->opt[INVERSE];
  o->name = "inverse";
  o->title = SANE_I18N ("Inverse Image");
  o->desc = SANE_I18N ("Inverse image in B/W mode");
  o->type = SANE_TYPE_BOOL;
  o->unit = SANE_UNIT_NONE;
  o->cap |= SANE_CAP_INACTIVE;

  /* Halftone pattern */
  o = &s->opt[HALFTONE_PATTERN];
  o->name = SANE_NAME_HALFTONE_PATTERN;
  o->title = SANE_TITLE_HALFTONE_PATTERN;
  o->desc = SANE_DESC_HALFTONE_PATTERN;
  o->type = SANE_TYPE_STRING;
  o->size = max_string_size (halftone_pattern);
  o->constraint_type = SANE_CONSTRAINT_STRING_LIST;
  o->constraint.string_list = halftone_pattern;
  s->val[HALFTONE_PATTERN].s = malloc (o->size);
  strcpy (s->val[HALFTONE_PATTERN].s, halftone_pattern[0]);
  o->cap |= SANE_CAP_INACTIVE;

  /* JPEG Compression */
  o = &s->opt[COMPRESSION];
  o->name = "jpeg";
  o->title = SANE_I18N ("JPEG compression");
  o->desc =
    SANE_I18N
    ("JPEG compression (yours application must be able to uncompress)");
  o->type = SANE_TYPE_BOOL;
  o->unit = SANE_UNIT_NONE;

  /* Compression parameter */
  o = &s->opt[COMPRESSION_PAR];
  o->name = "comp_arg";
  o->title = "Compression Argument";
  o->desc = "Compression Argument (Q parameter for JPEG)";
  o->type = SANE_TYPE_INT;
  o->size = sizeof (SANE_Int);
  o->constraint_type = SANE_CONSTRAINT_RANGE;
  o->constraint.range = &(compression_value_range);
  s->val[COMPRESSION_PAR].w = 0x4b;
  o->cap |= SANE_CAP_INACTIVE;

  /*  Stapled document */
  o = &s->opt[STAPELED_DOC];
  o->name = "stapeled_doc";
  o->title = SANE_I18N ("Detect stapled document");
  o->desc = SANE_I18N ("Detect stapled document");
  o->type = SANE_TYPE_STRING;
  o->size = max_string_size (stapeled_list);
  o->constraint_type = SANE_CONSTRAINT_STRING_LIST;
  o->constraint.string_list = stapeled_list;
  s->val[STAPELED_DOC].s = malloc (o->size);
  strcpy (s->val[STAPELED_DOC].s, stapeled_list[0]);
  if (s->id == KV_S7075C)
    o->cap |= SANE_CAP_INACTIVE;

  /* White level base */
  o = &s->opt[WHITE_LEVEL];
  o->name = SANE_NAME_WHITE_LEVEL;
  o->title = SANE_TITLE_WHITE_LEVEL;
  o->desc = SANE_DESC_WHITE_LEVEL;
  o->type = SANE_TYPE_STRING;
  o->size = max_string_size (white_level_list);
  o->constraint_type = SANE_CONSTRAINT_STRING_LIST;
  o->constraint.string_list = white_level_list;
  s->val[WHITE_LEVEL].s = malloc (o->size);
  strcpy (s->val[WHITE_LEVEL].s, white_level_list[0]);
  o->cap |= SANE_CAP_INACTIVE;

  /* Noise reduction */
  o = &s->opt[NOISE_REDUCTION];
  o->name = "noise-reduction";
  o->title = SANE_I18N ("Noise reduction");
  o->desc = SANE_I18N ("Reduce the isolated dot noise");
  o->type = SANE_TYPE_STRING;
  o->size = max_string_size (noise_reduction_list);
  o->constraint_type = SANE_CONSTRAINT_STRING_LIST;
  o->constraint.string_list = noise_reduction_list;
  s->val[NOISE_REDUCTION].s = malloc (o->size);
  strcpy (s->val[NOISE_REDUCTION].s, noise_reduction_list[0]);
  o->cap |= SANE_CAP_INACTIVE;

  o = &s->opt[RED_CHROMA];
  o->name = "red-chroma";
  o->title = SANE_I18N ("chroma of red");
  o->desc = SANE_I18N ("Set chroma of red");
  o->type = SANE_TYPE_INT;
  o->unit = SANE_UNIT_NONE;
  o->size = sizeof (SANE_Int);
  o->constraint_type = SANE_CONSTRAINT_RANGE;
  o->constraint.range = &(byte_value_range);
  s->val[RED_CHROMA].w = 0;

  o = &s->opt[BLUE_CHROMA];
  o->name = "blue chroma";
  o->title = SANE_I18N ("chroma of blue");
  o->desc = SANE_I18N ("Set chroma of blue");
  o->type = SANE_TYPE_INT;
  o->unit = SANE_UNIT_NONE;
  o->size = sizeof (SANE_Int);
  o->constraint_type = SANE_CONSTRAINT_RANGE;
  o->constraint.range = &(byte_value_range);
  s->val[BLUE_CHROMA].w = 0;

  o = &s->opt[DESKEW];
  o->name = "deskew";
  o->title = SANE_I18N ("Skew adjustment");
  o->desc = SANE_I18N ("Skew adjustment");
  o->type = SANE_TYPE_BOOL;
  o->unit = SANE_UNIT_NONE;
  s->val[DESKEW].w = SANE_FALSE;
  if (s->id != KV_S4085CL && s->id != KV_S4085CW)
    o->cap |= SANE_CAP_INACTIVE;

  o = &s->opt[STOP_SKEW];
  o->name = "stop-skew";
  o->title = SANE_I18N ("Stop scanner when a paper have been skewed");
  o->desc = SANE_I18N ("Scanner will be stop  when a paper have been skewed");
  o->type = SANE_TYPE_BOOL;
  o->unit = SANE_UNIT_NONE;
  s->val[STOP_SKEW].w = SANE_FALSE;

  o = &s->opt[CROP];
  o->name = "crop";
  o->title = SANE_I18N ("Crop actual image area");
  o->desc = SANE_I18N ("Scanner automatically detect image area and crop it");
  o->type = SANE_TYPE_BOOL;
  o->unit = SANE_UNIT_NONE;
  s->val[CROP].w = SANE_FALSE;
  if (s->id != KV_S4085CL && s->id != KV_S4085CW)
    o->cap |= SANE_CAP_INACTIVE;

  o = &s->opt[MIRROR];
  o->name = "mirror";
  o->title = SANE_I18N ("Mirror image");
  o->desc = SANE_I18N ("It is right and left reversing");
  o->type = SANE_TYPE_BOOL;
  o->unit = SANE_UNIT_NONE;
  s->val[MIRROR].w = SANE_FALSE;

  o = &s->opt[TOPPOS];
  o->name = "toppos";
  o->title = SANE_I18N ("Addition of space in top position");
  o->desc = SANE_I18N ("Addition of space in top position");
  o->type = SANE_TYPE_BOOL;
  o->unit = SANE_UNIT_NONE;
  s->val[TOPPOS].w = SANE_FALSE;

  o = &s->opt[BTMPOS];
  o->name = "btmpos";
  o->title = SANE_I18N ("Addition of space in bottom position");
  o->desc = SANE_I18N ("Addition of space in bottom position");
  o->type = SANE_TYPE_BOOL;
  o->unit = SANE_UNIT_NONE;
  s->val[BTMPOS].w = SANE_FALSE;
}


/* Lookup a string list from one array and return its index. */
static int
str_index (const SANE_String_Const * list, SANE_String_Const name)
{
  int index;
  index = 0;
  while (list[index])
    {
      if (!strcmp (list[index], name))
	return (index);
      index++;
    }
  return (-1);			/* not found */
}

/* Control option */
SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *val, SANE_Int * info)
{
  int i;
  SANE_Status status;
  SANE_Word cap;
  struct scanner *s = (struct scanner *) handle;

  if (info)
    *info = 0;

  if (option < 0 || option >= NUM_OPTIONS)
    return SANE_STATUS_UNSUPPORTED;

  cap = s->opt[option].cap;
  if (!SANE_OPTION_IS_ACTIVE (cap))
    return SANE_STATUS_UNSUPPORTED;

  if (action == SANE_ACTION_GET_VALUE)
    {
      if (s->opt[option].type == SANE_TYPE_STRING)
	{
	  DBG (DBG_INFO,
	       "sane_control_option: reading opt[%d] =  %s\n",
	       option, s->val[option].s);
	  strcpy (val, s->val[option].s);
	}
      else
	{
	  *(SANE_Word *) val = s->val[option].w;
	  DBG (DBG_INFO,
	       "sane_control_option: reading opt[%d] =  %d\n",
	       option, s->val[option].w);
	}
      return SANE_STATUS_GOOD;

    }
  else if (action == SANE_ACTION_SET_VALUE)
    {
      if (!SANE_OPTION_IS_SETTABLE (cap))
	return SANE_STATUS_INVAL;

      status = sanei_constrain_value (s->opt + option, val, info);
      if (status != SANE_STATUS_GOOD)
	return status;

      if (s->opt[option].type == SANE_TYPE_STRING)
	{
	  if (!strcmp (val, s->val[option].s))
	    return SANE_STATUS_GOOD;
	  DBG (DBG_INFO,
	       "sane_control_option: writing opt[%d] =  %s\n",
	       option, (SANE_String_Const) val);
	}
      else
	{
	  if (*(SANE_Word *) val == s->val[option].w)
	    return SANE_STATUS_GOOD;
	  DBG (DBG_INFO,
	       "sane_control_option: writing opt[%d] =  %d\n",
	       option, *(SANE_Word *) val);
	}

      switch (option)
	{
	  /* Side-effect options */
	case RESOLUTION:
	  s->val[option].w = *(SANE_Word *) val;
	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;

	case TL_Y:
	  if ((*(SANE_Word *) val) + MIN_LENGTH <=
	      s->val[BR_Y].w &&
	      !check_area (s, s->val[TL_X].w, *(SANE_Word *) val,
			   s->val[BR_X].w, s->val[BR_Y].w))
	    {
	      s->val[option].w = *(SANE_Word *) val;
	      if (info)
		*info |= SANE_INFO_RELOAD_PARAMS;
	    }
	  else if (info)
	    *info |= SANE_INFO_INEXACT |
	      SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;
	case BR_Y:
	  if ((*(SANE_Word *) val) >=
	      s->val[TL_Y].w + MIN_LENGTH
	      && !check_area (s, s->val[TL_X].w, s->val[TL_Y].w,
			      s->val[BR_X].w, *(SANE_Word *) val))
	    {
	      s->val[option].w = *(SANE_Word *) val;
	      if (info)
		*info |= SANE_INFO_RELOAD_PARAMS;
	    }
	  else if (info)
	    *info |= SANE_INFO_INEXACT |
	      SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;

	case TL_X:
	  if ((*(SANE_Word *) val) + MIN_WIDTH <=
	      s->val[BR_X].w &&
	      !check_area (s, *(SANE_Word *) val, s->val[TL_Y].w,
			   s->val[BR_X].w, s->val[BR_Y].w))
	    {
	      s->val[option].w = *(SANE_Word *) val;
	      if (info)
		*info |= SANE_INFO_RELOAD_PARAMS;
	    }
	  else if (info)
	    *info |= SANE_INFO_INEXACT |
	      SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;

	case BR_X:
	  if (*(SANE_Word *) val >=
	      s->val[TL_X].w + MIN_WIDTH
	      && !check_area (s, s->val[TL_X].w, s->val[TL_Y].w,
			      *(SANE_Word *) val, s->val[BR_Y].w))
	    {
	      s->val[option].w = *(SANE_Word *) val;
	      if (info)
		*info |= SANE_INFO_RELOAD_PARAMS;
	    }
	  else if (info)
	    *info |= SANE_INFO_INEXACT |
	      SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;

	case LANDSCAPE:
	  s->val[option].w = *(SANE_Word *) val;
	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  return SANE_STATUS_GOOD;

	  /* Side-effect free options */
	case CONTRAST:
	case BRIGHTNESS:
	case DUPLEX:
	case LENGTHCTL:
	case LONG_PAPER:
	case FIT_TO_PAGE:
	case THRESHOLD:
	case INVERSE:
	case COMPRESSION_PAR:
	case DFSTOP:
	case DFEED_L:
	case DFEED_C:
	case DFEED_R:
	case STOP_SKEW:
	case DESKEW:
	case MIRROR:
	case CROP:
	case TOPPOS:
	case BTMPOS:
	case RED_CHROMA:
	case BLUE_CHROMA:
	  s->val[option].w = *(SANE_Word *) val;
	  return SANE_STATUS_GOOD;

	case FEED_TIMEOUT:
	  s->val[option].w = *(SANE_Word *) val;
	  return kvs40xx_set_timeout (s, s->val[option].w);

	  /* String mode */
	case IMAGE_EMPHASIS:
	case GAMMA_CORRECTION:
	case LAMP:
	case HALFTONE_PATTERN:
	case DFEED_SENCE:
	case AUTOMATIC_THRESHOLD:
	case WHITE_LEVEL:
	case NOISE_REDUCTION:
	  strcpy (s->val[option].s, val);
	  return SANE_STATUS_GOOD;

	case SOURCE:
	  strcpy (s->val[option].s, val);
	  if (strcmp (s->val[option].s, SANE_I18N ("adf")))
	    {
	      strcpy (s->val[FEEDER_MODE].s, feeder_mode_list[0]);
	      strcpy (s->val[MANUALFEED].s, manual_feed_list[0]);
	      s->val[DUPLEX].w = SANE_FALSE;
	      s->val[DBLFEED].w = SANE_FALSE;
	      s->val[BTMPOS].w = SANE_FALSE;
	      s->val[TOPPOS].w = SANE_FALSE;
	      s->val[STOP_SKEW].w = SANE_FALSE;
	      s->val[LENGTHCTL].w = SANE_FALSE;
	      s->val[LONG_PAPER].w = SANE_FALSE;
	      s->opt[FEEDER_MODE].cap |= SANE_CAP_INACTIVE;
	      s->opt[MANUALFEED].cap |= SANE_CAP_INACTIVE;
	      s->opt[DUPLEX].cap |= SANE_CAP_INACTIVE;
	      s->opt[DBLFEED].cap |= SANE_CAP_INACTIVE;
	      s->opt[BTMPOS].cap |= SANE_CAP_INACTIVE;
	      s->opt[TOPPOS].cap |= SANE_CAP_INACTIVE;
	      s->opt[STOP_SKEW].cap |= SANE_CAP_INACTIVE;
	      s->opt[LENGTHCTL].cap |= SANE_CAP_INACTIVE;
	      s->opt[LONG_PAPER].cap |= SANE_CAP_INACTIVE;
	    }
	  else
	    {
	      s->opt[FEEDER_MODE].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[MANUALFEED].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[DUPLEX].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[DBLFEED].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[BTMPOS].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[TOPPOS].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[STOP_SKEW].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[LENGTHCTL].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[LONG_PAPER].cap &= ~SANE_CAP_INACTIVE;
	    }
	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS;

	  return SANE_STATUS_GOOD;

	case FEEDER_MODE:
	  strcpy (s->val[option].s, val);
	  if (strcmp (s->val[option].s, SANE_I18N ("continuous")))
	    {
	      s->opt[LONG_PAPER].cap |= SANE_CAP_INACTIVE;
	    }
	  else
	    {
	      s->opt[LONG_PAPER].cap &= ~SANE_CAP_INACTIVE;
	    }
	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS;

	  return SANE_STATUS_GOOD;

	case MODE:
	  strcpy (s->val[option].s, val);
	  if (!strcmp (s->val[option].s, SANE_VALUE_SCAN_MODE_LINEART))
	    {
	      s->opt[GAMMA_CORRECTION].cap |= SANE_CAP_INACTIVE;
	      s->opt[COMPRESSION].cap |= SANE_CAP_INACTIVE;
	      s->opt[COMPRESSION_PAR].cap |= SANE_CAP_INACTIVE;
	      s->opt[THRESHOLD].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[HALFTONE_PATTERN].cap &= ~SANE_CAP_INACTIVE;

	      s->opt[AUTOMATIC_THRESHOLD].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[WHITE_LEVEL].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[NOISE_REDUCTION].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[INVERSE].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[RED_CHROMA].cap |= SANE_CAP_INACTIVE;
	      s->opt[BLUE_CHROMA].cap |= SANE_CAP_INACTIVE;
	    }
	  else
	    {
	      s->opt[COMPRESSION].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[COMPRESSION_PAR].cap &= ~SANE_CAP_INACTIVE;

	      s->opt[THRESHOLD].cap |= SANE_CAP_INACTIVE;
	      s->opt[INVERSE].cap |= SANE_CAP_INACTIVE;
	      s->opt[HALFTONE_PATTERN].cap |= SANE_CAP_INACTIVE;

	      s->opt[AUTOMATIC_THRESHOLD].cap |= SANE_CAP_INACTIVE;
	      s->opt[WHITE_LEVEL].cap |= SANE_CAP_INACTIVE;
	      s->opt[NOISE_REDUCTION].cap |= SANE_CAP_INACTIVE;
	      s->opt[RED_CHROMA].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[BLUE_CHROMA].cap &= ~SANE_CAP_INACTIVE;
	    }

	  if (!strcmp (s->val[option].s, SANE_VALUE_SCAN_MODE_GRAY))
	    {
	      s->opt[INVERSE].cap &= ~SANE_CAP_INACTIVE;

	      s->opt[GAMMA_CORRECTION].cap &= ~SANE_CAP_INACTIVE;
	    }
	  else
	    {
	      s->opt[GAMMA_CORRECTION].cap |= SANE_CAP_INACTIVE;
	    }

	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;

	  return SANE_STATUS_GOOD;

	case MANUALFEED:
	  strcpy (s->val[option].s, val);
	  if (strcmp (s->val[option].s, manual_feed_list[0]) == 0)	/* off */
	    s->opt[FEED_TIMEOUT].cap |= SANE_CAP_INACTIVE;
	  else
	    s->opt[FEED_TIMEOUT].cap &= ~SANE_CAP_INACTIVE;
	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS;

	  return SANE_STATUS_GOOD;

	case STAPELED_DOC:
	  strcpy (s->val[option].s, val);
	  if (strcmp (s->val[option].s, stapeled_list[0]) == 0)
	    {
	      s->opt[DBLFEED].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[DFSTOP].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[DFEED_L].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[DFEED_C].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[DFEED_C].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[DFEED_R].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[DFEED_SENCE].cap &= ~SANE_CAP_INACTIVE;
	    }
	  else
	    {
	      s->opt[DBLFEED].cap |= SANE_CAP_INACTIVE;
	      s->opt[DFSTOP].cap |= SANE_CAP_INACTIVE;
	      s->opt[DFEED_L].cap |= SANE_CAP_INACTIVE;
	      s->opt[DFEED_C].cap |= SANE_CAP_INACTIVE;
	      s->opt[DFEED_R].cap |= SANE_CAP_INACTIVE;
	      s->opt[DFEED_SENCE].cap |= SANE_CAP_INACTIVE;
	    }
	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS;

	  return SANE_STATUS_GOOD;

	case DBLFEED:
	  s->val[option].w = *(SANE_Word *) val;
	  if (!s->val[option].b)
	    {
	      s->opt[DFSTOP].cap |= SANE_CAP_INACTIVE;
	      s->opt[DFEED_L].cap |= SANE_CAP_INACTIVE;
	      s->opt[DFEED_C].cap |= SANE_CAP_INACTIVE;
	      s->opt[DFEED_R].cap |= SANE_CAP_INACTIVE;
	      s->opt[DFEED_SENCE].cap |= SANE_CAP_INACTIVE;
	    }
	  else
	    {
	      s->opt[DFSTOP].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[DFEED_L].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[DFEED_C].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[DFEED_C].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[DFEED_R].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[DFEED_SENCE].cap &= ~SANE_CAP_INACTIVE;
	    }
	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS;

	  return SANE_STATUS_GOOD;

	case PAPER_SIZE:
	  strcpy (s->val[option].s, val);
	  i = str_index (paper_list, s->val[option].s);
	  if (i == 0)
	    {			/*user def */
	      s->opt[TL_X].cap &=
		s->opt[TL_Y].cap &=
		s->opt[BR_X].cap &= s->opt[BR_Y].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[LANDSCAPE].cap |= SANE_CAP_INACTIVE;
	      s->val[LANDSCAPE].w = 0;
	    }
	  else
	    {
	      s->opt[TL_X].cap |=
		s->opt[TL_Y].cap |=
		s->opt[BR_X].cap |= s->opt[BR_Y].cap |= SANE_CAP_INACTIVE;
	      if ( /*i == 4 || */ i == 5 || i == 6 /*XXX*/
		  || i == 10 || i == 11)
		{		/*A4, A5, A6, B5, B6 */
		  if ((s->id == KV_S4085CL || s->id == KV_S4065CL)
		      && i == 4 && i == 10)
		    {		/*A4, B5 */
		      s->opt[LANDSCAPE].cap |= SANE_CAP_INACTIVE;
		      s->val[LANDSCAPE].w = 0;
		    }
		  else
		    s->opt[LANDSCAPE].cap &= ~SANE_CAP_INACTIVE;
		}
	      else
		{
		  s->opt[LANDSCAPE].cap |= SANE_CAP_INACTIVE;
		  s->val[LANDSCAPE].w = 0;
		}
	    }
	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;

	  return SANE_STATUS_GOOD;

	case COMPRESSION:
	  s->val[option].w = *(SANE_Word *) val;
	  if (!s->val[option].b)
	    {
	      s->opt[COMPRESSION_PAR].cap |= SANE_CAP_INACTIVE;
	    }
	  else
	    {
	      s->opt[COMPRESSION_PAR].cap &= ~SANE_CAP_INACTIVE;
	    }
	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS;

	  return SANE_STATUS_GOOD;
	}
    }


  return SANE_STATUS_UNSUPPORTED;
}

void
kvs40xx_init_window (struct scanner *s, struct window *wnd, int wnd_id)
{
  int paper = str_index (paper_list, s->val[PAPER_SIZE].s), i;
  memset (wnd, 0, sizeof (struct window));
  *(u16 *) wnd->window_descriptor_block_length = cpu2be16 (66);

  wnd->window_identifier = wnd_id;
  *(u16 *) wnd->x_resolution = cpu2be16 (s->val[RESOLUTION].w);
  *(u16 *) wnd->y_resolution = cpu2be16 (s->val[RESOLUTION].w);
  if (!paper)
    {
      *(u32 *) wnd->upper_left_x =
	cpu2be32 (mm2scanner_units (s->val[TL_X].w));
      *(u32 *) wnd->upper_left_y =
	cpu2be32 (mm2scanner_units (s->val[TL_Y].w));
      *(u32 *) wnd->document_width =
	cpu2be32 (mm2scanner_units (s->val[BR_X].w));
      *(u32 *) wnd->width =
	cpu2be32 (mm2scanner_units (s->val[BR_X].w - s->val[TL_X].w));
      *(u32 *) wnd->document_length = cpu2be32 (mm2scanner_units
						(s->val[BR_Y].w));
      *(u32 *) wnd->length =
	cpu2be32 (mm2scanner_units (s->val[BR_Y].w - s->val[TL_Y].w));
    }
  else
    {
      u32 w = cpu2be32 (mm2scanner_units (paper_sizes[paper].width));
      u32 h = cpu2be32 (mm2scanner_units (paper_sizes[paper].height));
      *(u32 *) wnd->upper_left_x = cpu2be32 (mm2scanner_units (0));
      *(u32 *) wnd->upper_left_y = cpu2be32 (mm2scanner_units (0));
      if (!s->val[LANDSCAPE].b)
	{
	  *(u32 *) wnd->document_width = *(u32 *) wnd->width = w;
	  *(u32 *) wnd->document_length = *(u32 *) wnd->length = h;
	}
      else
	{
	  *(u32 *) wnd->document_width = *(u32 *) wnd->width = h;
	  *(u32 *) wnd->document_length = *(u32 *) wnd->length = w;
	}
    }
  wnd->brightness = s->val[BRIGHTNESS].w;
  wnd->threshold = s->val[THRESHOLD].w;
  wnd->contrast = s->val[CONTRAST].w;
  wnd->image_composition = mode_val[str_index (mode_list, s->val[MODE].s)];
  wnd->bit_per_pixel = bps_val[str_index (mode_list, s->val[MODE].s)];

  *(u16 *) wnd->halftone_pattern =
    cpu2be16 (str_index (halftone_pattern, s->val[HALFTONE_PATTERN].s));

  wnd->rif_padding = s->val[INVERSE].b << 7;
  *(u16 *) wnd->bit_ordering = cpu2be16 (BIT_ORDERING);
  wnd->compression_type = s->val[COMPRESSION].b ? 0x81 : 0;
  wnd->compression_argument = s->val[COMPRESSION_PAR].w;

  wnd->vendor_unique_identifier = 0;
  wnd->nobuf_fstspeed_dfstop = str_index (source_list,
					  s->val[SOURCE].s) << 7 |
    str_index (stapeled_list,
	       s->val[STAPELED_DOC].s) << 5 |
    s->val[STOP_SKEW].b << 4 | s->val[CROP].b << 3 | s->val[DFSTOP].b << 0;

  wnd->mirror_image = s->val[MIRROR].b << 7 |
    s->val[DFEED_L].b << 2 | s->val[DFEED_C].b << 1 | s->val[DFEED_R].b << 0;
  wnd->image_emphasis = str_index (image_emphasis_list,
				   s->val[IMAGE_EMPHASIS].s);
  wnd->gamma_correction = gamma_val[str_index (gamma_list,
					       s->val[GAMMA_CORRECTION].s)];
  wnd->mcd_lamp_dfeed_sens =
    str_index (lamp_list, s->val[LAMP].s) << 4 |
    str_index (dfeed_sence_list, s->val[DFEED_SENCE].s);

  wnd->document_size = (paper != 0) << 7
    | s->val[LENGTHCTL].b << 6
    | s->val[LONG_PAPER].b << 5 | s->val[LANDSCAPE].b << 4 | paper_val[paper];

  wnd->ahead_deskew_dfeed_scan_area_fspeed_rshad =
    (s->val[DESKEW].b || s->val[CROP].b ? 2 : 0) << 5 | /*XXX*/
    s->val[DBLFEED].b << 4 | s->val[FIT_TO_PAGE].b << 2;
  wnd->continuous_scanning_pages =
    str_index (feeder_mode_list, s->val[FEEDER_MODE].s) ? 0xff : 0;
  wnd->automatic_threshold_mode = automatic_threshold_val
    [str_index (automatic_threshold_list, s->val[AUTOMATIC_THRESHOLD].s)];
  wnd->automatic_separation_mode = 0;	/*Does not supported */
  wnd->standard_white_level_mode =
    white_level_val[str_index (white_level_list, s->val[WHITE_LEVEL].s)];
  wnd->b_wnr_noise_reduction =
    str_index (noise_reduction_list, s->val[NOISE_REDUCTION].s);

  i = str_index (manual_feed_list, s->val[MANUALFEED].s);
  wnd->mfeed_toppos_btmpos_dsepa_hsepa_dcont_rstkr = i << 6 |
    s->val[TOPPOS].b << 5 | s->val[BTMPOS].b << 4;
  wnd->stop_mode = 1;
  wnd->red_chroma = s->val[RED_CHROMA].w;
  wnd->blue_chroma = s->val[BLUE_CHROMA].w;
}

/* Get scan parameters */
SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  struct scanner *s = (struct scanner *) handle;
  SANE_Parameters *p = &s->params;

  if (!s->scanning)
    {
      unsigned w, h, res = s->val[RESOLUTION].w;
      unsigned i = str_index (paper_list,
			      s->val[PAPER_SIZE].s);
      if (i)
	{
	  if (s->val[LANDSCAPE].b)
	    {
	      w = paper_sizes[i].height;
	      h = paper_sizes[i].width;
	    }
	  else
	    {
	      w = paper_sizes[i].width;
	      h = paper_sizes[i].height;
	    }
	}
      else
	{
	  w = s->val[BR_X].w - s->val[TL_X].w;
	  h = s->val[BR_Y].w - s->val[TL_Y].w;
	}
      p->pixels_per_line = w * res / 25.4 + .5;
      p->lines = h * res / 25.4 + .5;
    }

  p->format = !strcmp (s->val[MODE].s,
		       SANE_VALUE_SCAN_MODE_COLOR) ? SANE_FRAME_RGB :
    SANE_FRAME_GRAY;
  p->last_frame = SANE_TRUE;
  p->depth = bps_val[str_index (mode_list, s->val[MODE].s)];
  p->bytes_per_line = p->depth * p->pixels_per_line / 8;
  if (p->depth > 8)
    p->depth = 8;
  if (params)
    memcpy (params, p, sizeof (SANE_Parameters));
  s->side_size = p->bytes_per_line * p->lines;

  return SANE_STATUS_GOOD;
}
