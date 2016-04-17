#include "../../include/sane/config.h"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

/* sane includes for the sanei functions called */
#include "../include/sane/sane.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei.h"
#include "../include/sane/sanei_config.h"

#define XSTR(s) STR(s)
#define STR(s) #s
#define CONFIG_PATH XSTR(TESTSUITE_SANEI_SRCDIR)

/*
 * variables and functions used by the tests below
 */


/* range for constraint */
static const SANE_Range model_range = {
  1000,				/* minimum */
  2000,				/* maximum */
  2				/* quantization */
};

/* range for memory buffer size constraint */
static const SANE_Range buffer_range = {
  1024,				/* minimum bytes */
  2048 * 1024,			/* maximum bytes */
  1024				/* quantization */
};

/* range for int value in [0-15] */
static const SANE_Range value16_range = {
  0,				/* minimum */
  15,				/* maximum */
  1				/* quantization */
};

/* range for fixed height value */
static const SANE_Range height_range = {
  SANE_FIX (0),			/* minimum */
  SANE_FIX (29.7),		/* maximum */
  0				/* no quantization : hard to do for float values ... */
};

/* list of astra models */
static const SANE_String_Const astra_models[] =
  { "610", "1220", "1600", "2000", NULL };

/* string list */
static const SANE_String_Const string_list[] =
  { "string1", "string2", "string3", "string4", NULL };

/* last device name used for attach callback */
static char *lastdevname = NULL;

static SANE_Status
check_config_attach (SANEI_Config * config, const char *devname)
{
  /* silence compiler warning for now */
  if (config == NULL)
    {
      return SANE_STATUS_INVAL;
    }

  fprintf (stdout, "attaching with devname '%s'\n", devname);
  if (lastdevname != NULL)
    {
      free (lastdevname);
    }
  lastdevname = strdup (devname);
  return SANE_STATUS_GOOD;
}

/******************************/
/* start of tests definitions */
/******************************/

/*
 * non-existent config file
 */
static void
inexistent_config (void)
{
  SANE_Status status;
  SANEI_Config config;

  config.count = 0;
  config.descriptors = NULL;
  config.values = NULL;
  status = sanei_configure_attach (CONFIG_PATH
                                   "/data/inexistent.conf", &config, NULL);

  /* check results */
  assert (status != SANE_STATUS_GOOD);
}


/*
 * no config struct
 */
static void
null_config (void)
{
  SANE_Status status;

  status =
    sanei_configure_attach (CONFIG_PATH "/data/umax_pp.conf", NULL,
                            check_config_attach);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
}


/*
 * no attach function
 */
static void
null_attach (void)
{
  SANE_Status status;

  status = sanei_configure_attach (CONFIG_PATH
                                   "/data/umax_pp.conf", NULL, NULL);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
}


/*
 * empty config : backend has no configuration option
 */
static void
empty_config (void)
{
  SANE_Status status;
  SANEI_Config config;

  config.count = 0;
  config.descriptors = NULL;
  config.values = NULL;
  status =
    sanei_configure_attach (CONFIG_PATH "/data/empty.conf",
                            &config, check_config_attach);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
}


/*
 * string option
 */
static void
string_option (void)
{
  SANE_Status status;
  SANEI_Config config;
  SANE_Char modelname[128];
  SANE_Char vendor[128];
  SANE_Option_Descriptor *options[2];
  void *values[2];
  int i;

  i = 0;
  options[i] =
    (SANE_Option_Descriptor *) malloc (sizeof (SANE_Option_Descriptor));
  options[i]->name = "modelname";
  options[i]->title = "model name";
  options[i]->desc = "user provided scanner's model name";
  options[i]->type = SANE_TYPE_STRING;
  options[i]->unit = SANE_UNIT_NONE;
  options[i]->size = 128;
  options[i]->cap = SANE_CAP_SOFT_SELECT;
  options[i]->constraint_type = SANE_CONSTRAINT_NONE;
  values[i] = modelname;
  i++;

  options[i] =
    (SANE_Option_Descriptor *) malloc (sizeof (SANE_Option_Descriptor));
  options[i]->name = "vendor";
  options[i]->title = "vendor name";
  options[i]->desc = "user provided scanner's vendor name";
  options[i]->type = SANE_TYPE_STRING;
  options[i]->unit = SANE_UNIT_NONE;
  options[i]->size = 128;
  options[i]->cap = SANE_CAP_SOFT_SELECT;
  options[i]->constraint_type = SANE_CONSTRAINT_NONE;
  values[i] = vendor;
  i++;

  config.count = i;
  config.descriptors = options;
  config.values = values;

  /* configure and attach */
  status =
    sanei_configure_attach (CONFIG_PATH "/data/string.conf",
                            &config, check_config_attach);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (strcmp (modelname, "my model") == 0);
  assert (strcmp (vendor, "my vendor") == 0);
}


/*
 * int option
 */
static void
int_option (void)
{
  SANE_Status status;
  SANEI_Config config;
  SANE_Word modelnumber;
  SANE_Option_Descriptor *options[1];
  void *values[1];
  int i;

  i = 0;
  options[i] =
    (SANE_Option_Descriptor *) malloc (sizeof (SANE_Option_Descriptor));
  options[i]->name = "modelnumber";
  options[i]->title = "model number";
  options[i]->desc = "user provided scanner's model number";
  options[i]->type = SANE_TYPE_INT;
  options[i]->unit = SANE_UNIT_NONE;
  options[i]->size = sizeof (SANE_Word);
  options[i]->cap = SANE_CAP_SOFT_SELECT;
  options[i]->constraint_type = SANE_CONSTRAINT_RANGE;
  options[i]->constraint.range = &model_range;
  values[i] = &modelnumber;
  i++;

  config.descriptors = options;
  config.values = values;
  config.count = i;

  /* configure and attach */
  status =
    sanei_configure_attach (CONFIG_PATH "/data/int.conf", &config,
                            check_config_attach);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (modelnumber == 1234);
}


/*
 * int option out of range
 */
static void
wrong_range_int_option (void)
{
  SANE_Status status;
  SANEI_Config config;
  SANE_Word modelnumber = -1;
  SANE_Option_Descriptor *options[1];
  void *values[1];
  int i;

  i = 0;
  options[i] =
    (SANE_Option_Descriptor *) malloc (sizeof (SANE_Option_Descriptor));
  options[i]->name = "modelnumber";
  options[i]->title = "model number";
  options[i]->desc = "user provided scanner's model number";
  options[i]->type = SANE_TYPE_INT;
  options[i]->unit = SANE_UNIT_NONE;
  options[i]->size = sizeof (SANE_Word);
  options[i]->cap = SANE_CAP_SOFT_SELECT;
  options[i]->constraint_type = SANE_CONSTRAINT_RANGE;
  options[i]->constraint.range = &model_range;
  values[i] = &modelnumber;
  i++;

  config.descriptors = options;
  config.values = values;
  config.count = i;

  /* configure and attach */
  status =
    sanei_configure_attach (CONFIG_PATH "/data/wrong-range.conf",
                            &config, check_config_attach);

  /* check results */
  assert (status == SANE_STATUS_INVAL);
  assert (modelnumber == -1);
}


/*
 * word array
 */
static void
word_array_option (void)
{
  SANE_Status status;
  SANEI_Config config;
  SANE_Word numbers[7];
  SANE_Option_Descriptor *options[1];
  void *values[1];
  int i;

  i = 0;
  options[i] =
    (SANE_Option_Descriptor *) malloc (sizeof (SANE_Option_Descriptor));
  options[i]->name = "numbers";
  options[i]->title = "some numbers";
  options[i]->desc = "an array of numbers";
  options[i]->type = SANE_TYPE_INT;
  options[i]->unit = SANE_UNIT_NONE;
  options[i]->size = sizeof (SANE_Word) * 7;
  options[i]->cap = SANE_CAP_SOFT_SELECT;
  options[i]->constraint_type = SANE_CONSTRAINT_RANGE;
  options[i]->constraint.range = &model_range;
  values[i] = numbers;
  i++;

  config.descriptors = options;
  config.values = values;
  config.count = i;

  /* configure and attach */
  status =
    sanei_configure_attach (CONFIG_PATH "/data/word-array.conf",
                            &config, check_config_attach);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  for (i = 0; i < 7; i++)
    {
      assert (numbers[i] == 1000 + 100 * i);
    }
}


/*
 * string option with string list constraint
 */
static void
string_list_option (void)
{
  SANE_Status status;
  SANEI_Config config;
  SANE_Char choice[128];
  SANE_Option_Descriptor *options[1];
  void *values[1];
  int i;

  i = 0;
  options[i] =
    (SANE_Option_Descriptor *) malloc (sizeof (SANE_Option_Descriptor));
  options[i]->name = "string-choice";
  options[i]->title = "string choice";
  options[i]->desc = "one string among a fixed list";
  options[i]->type = SANE_TYPE_STRING;
  options[i]->unit = SANE_UNIT_NONE;
  options[i]->size = 128;
  options[i]->cap = SANE_CAP_SOFT_SELECT;
  options[i]->constraint_type = SANE_CONSTRAINT_STRING_LIST;
  options[i]->constraint.string_list = string_list;
  values[i] = choice;
  i++;

  config.descriptors = options;
  config.values = values;
  config.count = i;

  /* configure and attach */
  status =
    sanei_configure_attach (CONFIG_PATH "/data/string-list.conf",
                            &config, check_config_attach);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (strcmp (choice, "string3") == 0);
}


/*
 * string option with string list constraint
 */
static void
wrong_string_list_option (void)
{
  SANE_Status status;
  SANEI_Config config;
  SANE_Char choice[128];
  SANE_Option_Descriptor *options[1];
  void *values[1];
  int i;

  i = 0;
  options[i] =
    (SANE_Option_Descriptor *) malloc (sizeof (SANE_Option_Descriptor));
  options[i]->name = "string-choice";
  options[i]->title = "string choice";
  options[i]->desc = "one string among a fixed list";
  options[i]->type = SANE_TYPE_STRING;
  options[i]->unit = SANE_UNIT_NONE;
  options[i]->size = 128;
  options[i]->cap = SANE_CAP_SOFT_SELECT;
  options[i]->constraint_type = SANE_CONSTRAINT_STRING_LIST;
  options[i]->constraint.string_list = string_list;
  values[i] = choice;
  i++;

  config.descriptors = options;
  config.values = values;
  config.count = i;

  choice[0] = 0;

  /* configure and attach */
  status =
    sanei_configure_attach (CONFIG_PATH
                            "/data/wrong-string-list.conf", &config,
			    check_config_attach);

  /* check results */
  assert (status == SANE_STATUS_INVAL);
  assert (strcmp (choice, "") == 0);
}


/*
 * real umax_pp confiugration file parsing
 */
static void
umax_pp (void)
{
  SANE_Status status;
  SANEI_Config config;
  SANE_Option_Descriptor *options[9];
  void *values[9];
  int i = 0;
  /* placeholders for options */
  SANE_Word buffersize = -1;
  SANE_Word redgain = -1;
  SANE_Word greengain = -1;
  SANE_Word bluegain = -1;
  SANE_Word redoffset = -1;
  SANE_Word greenoffset = -1;
  SANE_Word blueoffset = -1;
  SANE_Char model[128];

  i = 0;
  options[i] =
    (SANE_Option_Descriptor *) malloc (sizeof (SANE_Option_Descriptor));
  options[i]->name = "buffer";
  options[i]->type = SANE_TYPE_INT;
  options[i]->unit = SANE_UNIT_NONE;
  options[i]->size = sizeof (SANE_Word);
  options[i]->cap = SANE_CAP_SOFT_SELECT;
  options[i]->constraint_type = SANE_CONSTRAINT_RANGE;
  options[i]->constraint.range = &buffer_range;
  values[i] = &buffersize;
  i++;

  options[i] =
    (SANE_Option_Descriptor *) malloc (sizeof (SANE_Option_Descriptor));
  options[i]->name = "red-gain";
  options[i]->type = SANE_TYPE_INT;
  options[i]->unit = SANE_UNIT_NONE;
  options[i]->size = sizeof (SANE_Word);
  options[i]->cap = SANE_CAP_SOFT_SELECT;
  options[i]->constraint_type = SANE_CONSTRAINT_RANGE;
  options[i]->constraint.range = &value16_range;
  values[i] = &redgain;
  i++;

  options[i] =
    (SANE_Option_Descriptor *) malloc (sizeof (SANE_Option_Descriptor));
  options[i]->name = "green-gain";
  options[i]->type = SANE_TYPE_INT;
  options[i]->unit = SANE_UNIT_NONE;
  options[i]->size = sizeof (SANE_Word);
  options[i]->cap = SANE_CAP_SOFT_SELECT;
  options[i]->constraint_type = SANE_CONSTRAINT_RANGE;
  options[i]->constraint.range = &value16_range;
  values[i] = &greengain;
  i++;

  options[i] =
    (SANE_Option_Descriptor *) malloc (sizeof (SANE_Option_Descriptor));
  options[i]->name = "blue-gain";
  options[i]->type = SANE_TYPE_INT;
  options[i]->unit = SANE_UNIT_NONE;
  options[i]->size = sizeof (SANE_Word);
  options[i]->cap = SANE_CAP_SOFT_SELECT;
  options[i]->constraint_type = SANE_CONSTRAINT_RANGE;
  options[i]->constraint.range = &value16_range;
  values[i] = &bluegain;
  i++;

  options[i] =
    (SANE_Option_Descriptor *) malloc (sizeof (SANE_Option_Descriptor));
  options[i]->name = "red-offset";
  options[i]->type = SANE_TYPE_INT;
  options[i]->unit = SANE_UNIT_NONE;
  options[i]->size = sizeof (SANE_Word);
  options[i]->cap = SANE_CAP_SOFT_SELECT;
  options[i]->constraint_type = SANE_CONSTRAINT_RANGE;
  options[i]->constraint.range = &value16_range;
  values[i] = &redoffset;
  i++;

  options[i] =
    (SANE_Option_Descriptor *) malloc (sizeof (SANE_Option_Descriptor));
  options[i]->name = "green-offset";
  options[i]->type = SANE_TYPE_INT;
  options[i]->unit = SANE_UNIT_NONE;
  options[i]->size = sizeof (SANE_Word);
  options[i]->cap = SANE_CAP_SOFT_SELECT;
  options[i]->constraint_type = SANE_CONSTRAINT_RANGE;
  options[i]->constraint.range = &value16_range;
  values[i] = &greenoffset;
  i++;

  options[i] =
    (SANE_Option_Descriptor *) malloc (sizeof (SANE_Option_Descriptor));
  options[i]->name = "blue-offset";
  options[i]->type = SANE_TYPE_INT;
  options[i]->unit = SANE_UNIT_NONE;
  options[i]->size = sizeof (SANE_Word);
  options[i]->cap = SANE_CAP_SOFT_SELECT;
  options[i]->constraint_type = SANE_CONSTRAINT_RANGE;
  options[i]->constraint.range = &value16_range;
  values[i] = &blueoffset;
  i++;

  options[i] =
    (SANE_Option_Descriptor *) malloc (sizeof (SANE_Option_Descriptor));
  options[i]->name = "astra";
  options[i]->type = SANE_TYPE_STRING;
  options[i]->unit = SANE_UNIT_NONE;
  options[i]->size = 128;
  options[i]->cap = SANE_CAP_SOFT_SELECT;
  options[i]->constraint_type = SANE_CONSTRAINT_STRING_LIST;
  options[i]->constraint.string_list = astra_models;
  values[i] = &model;
  i++;

  config.descriptors = options;
  config.values = values;
  config.count = i;

  model[0] = 0;

  /* configure and attach */
  status =
    sanei_configure_attach (CONFIG_PATH "/data/umax_pp.conf",
                            &config, check_config_attach);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (buffersize == 1048576);
  assert (redgain == 1);
  assert (greengain == 2);
  assert (bluegain == 3);
  assert (redoffset == 4);
  assert (greenoffset == 5);
  assert (blueoffset == 6);
  assert (strcmp (model, "1600") == 0);
  assert (strcmp (lastdevname, "port safe-auto") == 0);

  /* free memory */
  while (i > 0)
    {
      i--;
      free (options[i]);
    }
}


/*
 * boolean option
 */
static void
wrong_bool_option (void)
{
  SANE_Status status;
  SANEI_Config config;
  SANE_Option_Descriptor *options[2];
  void *values[2];
  SANE_Bool booltrue, boolfalse;
  int i;

  i = 0;
  options[i] =
    (SANE_Option_Descriptor *) malloc (sizeof (SANE_Option_Descriptor));
  options[i]->name = "booltrue";
  options[i]->title = "boolean true";
  options[i]->type = SANE_TYPE_BOOL;
  options[i]->unit = SANE_UNIT_NONE;
  options[i]->size = sizeof (SANE_Bool);
  options[i]->cap = SANE_CAP_SOFT_SELECT;
  options[i]->constraint_type = SANE_CONSTRAINT_NONE;
  values[i] = &booltrue;
  i++;

  options[i] =
    (SANE_Option_Descriptor *) malloc (sizeof (SANE_Option_Descriptor));
  options[i]->name = "boolfalse";
  options[i]->title = "boolean false";
  options[i]->type = SANE_TYPE_BOOL;
  options[i]->unit = SANE_UNIT_NONE;
  options[i]->size = sizeof (SANE_Bool);
  options[i]->cap = SANE_CAP_SOFT_SELECT;
  options[i]->constraint_type = SANE_CONSTRAINT_NONE;
  values[i] = &boolfalse;
  i++;

  config.descriptors = options;
  config.values = values;
  config.count = i;

  /* configure and attach */
  status =
    sanei_configure_attach (CONFIG_PATH "/data/wrong-boolean.conf",
                            &config, check_config_attach);

  /* check results */
  assert (status == SANE_STATUS_INVAL);
  assert (booltrue == SANE_TRUE);
}


/*
 * boolean option
 */
static void
bool_option (void)
{
  SANE_Status status;
  SANEI_Config config;
  SANE_Option_Descriptor *options[3];
  void *values[3];
  SANE_Bool booltrue, boolfalse, boolarray[3];
  int i;

  i = 0;
  options[i] =
    (SANE_Option_Descriptor *) malloc (sizeof (SANE_Option_Descriptor));
  options[i]->name = "booltrue";
  options[i]->title = "boolean true";
  options[i]->type = SANE_TYPE_BOOL;
  options[i]->unit = SANE_UNIT_NONE;
  options[i]->size = sizeof (SANE_Bool);
  options[i]->cap = SANE_CAP_SOFT_SELECT;
  options[i]->constraint_type = SANE_CONSTRAINT_NONE;
  values[i] = &booltrue;
  i++;

  options[i] =
    (SANE_Option_Descriptor *) malloc (sizeof (SANE_Option_Descriptor));
  options[i]->name = "boolfalse";
  options[i]->title = "boolean false";
  options[i]->type = SANE_TYPE_BOOL;
  options[i]->unit = SANE_UNIT_NONE;
  options[i]->size = sizeof (SANE_Bool);
  options[i]->cap = SANE_CAP_SOFT_SELECT;
  options[i]->constraint_type = SANE_CONSTRAINT_NONE;
  values[i] = &boolfalse;
  i++;

  options[i] =
    (SANE_Option_Descriptor *) malloc (sizeof (SANE_Option_Descriptor));
  options[i]->name = "boolarray";
  options[i]->title = "boolean array";
  options[i]->type = SANE_TYPE_BOOL;
  options[i]->unit = SANE_UNIT_NONE;
  options[i]->size = sizeof (boolarray);
  options[i]->cap = SANE_CAP_SOFT_SELECT;
  options[i]->constraint_type = SANE_CONSTRAINT_NONE;
  values[i] = boolarray;
  i++;

  config.descriptors = options;
  config.values = values;
  config.count = i;

  /* configure and attach */
  status =
    sanei_configure_attach (CONFIG_PATH "/data/boolean.conf",
                            &config, check_config_attach);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (booltrue == SANE_TRUE);
  assert (boolfalse == SANE_FALSE);
  for (i = 0; i < 3; i++)
    {
      assert (boolarray[i] == (SANE_Bool) i % 2);
    }
}


/*
 * fixed option
 */
static void
fixed_option (void)
{
  SANE_Status status;
  SANEI_Config config;
  SANE_Word width, height, fixedarray[7];
  SANE_Option_Descriptor *options[3];
  void *values[3];
  int i;

  i = 0;
  options[i] =
    (SANE_Option_Descriptor *) malloc (sizeof (SANE_Option_Descriptor));
  options[i]->name = "width";
  options[i]->title = "width";
  options[i]->type = SANE_TYPE_FIXED;
  options[i]->unit = SANE_UNIT_NONE;
  options[i]->size = sizeof (SANE_Word);
  options[i]->cap = SANE_CAP_SOFT_SELECT;
  options[i]->constraint_type = SANE_CONSTRAINT_NONE;
  values[i] = &width;
  i++;

  options[i] =
    (SANE_Option_Descriptor *) malloc (sizeof (SANE_Option_Descriptor));
  options[i]->name = "height";
  options[i]->title = "height";
  options[i]->type = SANE_TYPE_FIXED;
  options[i]->unit = SANE_UNIT_NONE;
  options[i]->size = sizeof (SANE_Word);
  options[i]->cap = SANE_CAP_SOFT_SELECT;
  options[i]->constraint_type = SANE_CONSTRAINT_NONE;
  values[i] = &height;
  i++;

  options[i] =
    (SANE_Option_Descriptor *) malloc (sizeof (SANE_Option_Descriptor));
  options[i]->name = "array-of-fixed";
  options[i]->title = "array of fixed";
  options[i]->type = SANE_TYPE_FIXED;
  options[i]->unit = SANE_UNIT_NONE;
  options[i]->size = sizeof (fixedarray);
  options[i]->cap = SANE_CAP_SOFT_SELECT;
  options[i]->constraint_type = SANE_CONSTRAINT_RANGE;
  options[i]->constraint.range = &height_range;
  values[i] = fixedarray;
  i++;

  config.descriptors = options;
  config.values = values;
  config.count = i;

  /* configure and attach */
  status =
    sanei_configure_attach (CONFIG_PATH "/data/fixed.conf",
                            &config, check_config_attach);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (width == SANE_FIX (21.0));
  assert (height == SANE_FIX (29.7));
  for (i = 0; i < 7; i++)
    {
      assert (fixedarray[i] == SANE_FIX (2.0 + 0.1 * ((float) i)));
    }
}


/*
 * fixed option with value out of range
 */
static void
wrong_fixed_option (void)
{
  SANE_Status status;
  SANEI_Config config;
  SANE_Word height;
  SANE_Option_Descriptor *options[1];
  void *values[1];
  int i;

  i = 0;
  options[i] =
    (SANE_Option_Descriptor *) malloc (sizeof (SANE_Option_Descriptor));
  options[i]->name = "height";
  options[i]->title = "height";
  options[i]->type = SANE_TYPE_FIXED;
  options[i]->unit = SANE_UNIT_NONE;
  options[i]->size = sizeof (SANE_Word);
  options[i]->cap = SANE_CAP_SOFT_SELECT;
  options[i]->constraint_type = SANE_CONSTRAINT_RANGE;
  options[i]->constraint.range = &height_range;
  values[i] = &height;
  i++;

  config.descriptors = options;
  config.values = values;
  config.count = i;

  /* configure and attach */
  status =
    sanei_configure_attach (CONFIG_PATH "/data/wrong-fixed.conf",
                            &config, check_config_attach);

  /* check results */
  assert (status == SANE_STATUS_INVAL);
}


static void
snapscan (void)
{
  SANE_Status status;
  SANEI_Config config;
  SANE_Char firmware[128];
  SANE_Option_Descriptor *options[1];
  void *values[1];
  int i;

  i = 0;
  options[i] =
    (SANE_Option_Descriptor *) malloc (sizeof (SANE_Option_Descriptor));
  options[i]->name = "firmware";
  options[i]->title = "scanner's firmware path";
  options[i]->desc = "user provided scanner's full path";
  options[i]->type = SANE_TYPE_STRING;
  options[i]->unit = SANE_UNIT_NONE;
  options[i]->size = sizeof (firmware);
  options[i]->cap = SANE_CAP_SOFT_SELECT;
  options[i]->constraint_type = SANE_CONSTRAINT_NONE;
  values[i] = firmware;
  i++;

  config.descriptors = options;
  config.values = values;
  config.count = i;

  /* configure and attach */
  status =
    sanei_configure_attach (CONFIG_PATH "/data/snapscan.conf",
                            &config, check_config_attach);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (strcmp (firmware, "/usr/share/sane/snapscan/your-firmwarefile.bin")
	  == 0);
  /* TODO must test attach() done */
}


/**
 * create the test suite for sanei config related tests 
 */
static void
sanei_config_suite (void)
{
  /* tests */
  inexistent_config ();
  empty_config ();
  null_config ();
  null_attach ();
  string_option ();
  int_option ();
  string_list_option ();
  word_array_option ();
  bool_option ();
  fixed_option ();
  wrong_range_int_option ();
  wrong_string_list_option ();
  wrong_bool_option ();
  wrong_fixed_option ();

  /* backend real conf inspired cases */
  umax_pp ();
  snapscan ();
}

/**
 * main function to run the test suites 
 */
int
main (void)
{
  /* set up config dir for local conf files */
  putenv("SANE_CONFIG_DIR=.:/");

  /* run suites */
  sanei_config_suite ();

  return 0;
}

/* vim: set sw=2 cino=>2se-1sn-1s{s^-1st0(0u0 smarttab expandtab: */
