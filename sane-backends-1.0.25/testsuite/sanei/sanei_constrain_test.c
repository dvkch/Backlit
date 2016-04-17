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

static SANE_Option_Descriptor none_opt = {
  SANE_NAME_SCAN_TL_X,
  SANE_TITLE_SCAN_TL_X,
  SANE_DESC_SCAN_TL_X,
  SANE_TYPE_INT,
  SANE_UNIT_NONE,
  sizeof (SANE_Word),
  0,
  SANE_CONSTRAINT_NONE,
  {NULL}
};


static SANE_Option_Descriptor none_bool_opt = {
  SANE_NAME_SCAN_TL_X,
  SANE_TITLE_SCAN_TL_X,
  SANE_DESC_SCAN_TL_X,
  SANE_TYPE_BOOL,
  SANE_UNIT_NONE,
  sizeof (SANE_Word),
  0,
  SANE_CONSTRAINT_NONE,
  {NULL}
};

/* range for int constraint */
static const SANE_Range int_range = {
  3,				/* minimum */
  18,				/* maximum */
  3				/* quantization */
};

/* range for sane fixed constraint */
static const SANE_Range fixed_range = {
  SANE_FIX(1.0),		/* minimum */
  SANE_FIX(431.8),		/* maximum */
  SANE_FIX(0.01)				/* quantization */
};

static SANE_Option_Descriptor int_opt = {
  SANE_NAME_SCAN_TL_X,
  SANE_TITLE_SCAN_TL_X,
  SANE_DESC_SCAN_TL_X,
  SANE_TYPE_FIXED,
  SANE_UNIT_MM,
  sizeof (SANE_Word),
  0,
  SANE_CONSTRAINT_RANGE,
  {NULL}
};

static SANE_Option_Descriptor fixed_opt = {
  SANE_NAME_SCAN_TL_X,
  SANE_TITLE_SCAN_TL_X,
  SANE_DESC_SCAN_TL_X,
  SANE_TYPE_FIXED,
  SANE_UNIT_MM,
  sizeof (SANE_Word),
  0,
  SANE_CONSTRAINT_RANGE,
  {NULL}
};

#define ARRAY_SIZE 7

static SANE_Option_Descriptor array_opt = {
  SANE_NAME_SCAN_TL_X,
  SANE_TITLE_SCAN_TL_X,
  SANE_DESC_SCAN_TL_X,
  SANE_TYPE_FIXED,
  SANE_UNIT_MM,
  sizeof (SANE_Word) * ARRAY_SIZE,
  0,
  SANE_CONSTRAINT_RANGE,
  {NULL}
};

#define WORD_SIZE 9
static const SANE_Int dpi_list[] =
  { WORD_SIZE - 1, 100, 200, 300, 400, 500, 600, 700, 800 };

static SANE_Option_Descriptor word_array_opt = {
  SANE_NAME_SCAN_RESOLUTION,
  SANE_TITLE_SCAN_RESOLUTION,
  SANE_DESC_SCAN_RESOLUTION,
  SANE_TYPE_INT,
  SANE_UNIT_DPI,
  sizeof (SANE_Word) * WORD_SIZE,
  100,
  SANE_CONSTRAINT_WORD_LIST,
  {NULL}
};

static const SANE_String_Const string_list[] = {
  SANE_VALUE_SCAN_MODE_LINEART,
  SANE_VALUE_SCAN_MODE_HALFTONE,
  SANE_VALUE_SCAN_MODE_GRAY,
  "linelength",
  0
};

static SANE_Option_Descriptor string_array_opt = {
  SANE_NAME_SCAN_MODE,
  SANE_TITLE_SCAN_MODE,
  SANE_DESC_SCAN_MODE,
  SANE_TYPE_STRING,
  SANE_UNIT_NONE,
  8,
  0,
  SANE_CONSTRAINT_STRING_LIST,
  {NULL}
};


/******************************/
/* start of tests definitions */
/******************************/

/*
 * constrained int
 */
static void
min_int_value (void)
{
  SANE_Int value = int_range.min;
  SANE_Word info = 0;
  SANE_Status status;

  status = sanei_constrain_value (&int_opt, &value, &info);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (info == 0);
  assert (value == int_range.min);
}

static void
max_int_value (void)
{
  SANE_Int value = int_range.max;
  SANE_Word info = 0;
  SANE_Status status;

  status = sanei_constrain_value (&int_opt, &value, &info);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (info == 0);
  assert (value == int_range.max);
}

static void
below_min_int_value (void)
{
  SANE_Int value = int_range.min - 1;
  SANE_Word info = 0;
  SANE_Status status;

  status = sanei_constrain_value (&int_opt, &value, &info);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (info == SANE_INFO_INEXACT);
  assert (value == int_range.min);
}

/* rounded to lower value */
static void
quant1_int_value (void)
{
  SANE_Int value = int_range.min + 1;
  SANE_Word info = 0;
  SANE_Status status;

  status = sanei_constrain_value (&int_opt, &value, &info);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (info == SANE_INFO_INEXACT);
  assert (value == int_range.min);
}

/* rounded to higher value */
static void
quant2_int_value (void)
{
  SANE_Int value = int_range.min + int_range.quant - 1;
  SANE_Word info = 0;
  SANE_Status status;

  status = sanei_constrain_value (&int_opt, &value, &info);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (info == SANE_INFO_INEXACT);
  assert (value == int_range.min + int_range.quant);
}

static void
in_range_int_value (void)
{
  SANE_Int value = int_range.min + int_range.quant;
  SANE_Word info = 0;
  SANE_Status status;

  status = sanei_constrain_value (&int_opt, &value, &info);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (info == 0);
  assert (value == int_range.min + int_range.quant);
}

static void
above_max_int_value (void)
{
  SANE_Int value = int_range.max + 1;
  SANE_Word info = 0;
  SANE_Status status;

  status = sanei_constrain_value (&int_opt, &value, &info);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (info == SANE_INFO_INEXACT);
  assert (value == int_range.max);
}

/*
 * constrained fixed value
 */
static void
min_fixed_value (void)
{
  SANE_Int value = fixed_range.min;
  SANE_Word info = 0;
  SANE_Status status;

  status = sanei_constrain_value (&fixed_opt, &value, &info);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (info == 0);
  assert (value == fixed_range.min);
}

static void
max_fixed_value (void)
{
  SANE_Int value = fixed_range.max;
  SANE_Word info = 0;
  SANE_Status status;

  status = sanei_constrain_value (&fixed_opt, &value, &info);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (info == 0);
  assert (value == fixed_range.max); 
}

static void
below_min_fixed_value (void)
{
  SANE_Int value = fixed_range.min - 1;
  SANE_Word info = 0;
  SANE_Status status;

  status = sanei_constrain_value (&fixed_opt, &value, &info);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (info == SANE_INFO_INEXACT);
  assert (value == fixed_range.min);
}

/* rounded to lower value */
static void
quant1_fixed_value (void)
{
  SANE_Int value = fixed_range.min + fixed_range.quant/3;
  SANE_Word info = 0;
  SANE_Status status;

  status = sanei_constrain_value (&fixed_opt, &value, &info);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (info == SANE_INFO_INEXACT);
  assert (value == fixed_range.min);
}

/* rounded to higher value */
static void
quant2_fixed_value (void)
{
  SANE_Int value = fixed_range.min + fixed_range.quant - fixed_range.quant/3;
  SANE_Word info = 0;
  SANE_Status status;

  status = sanei_constrain_value (&fixed_opt, &value, &info);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (info == SANE_INFO_INEXACT);
  assert (value == fixed_range.min + fixed_range.quant);
}

static void
in_range_fixed_value (void)
{
  SANE_Int value = fixed_range.min + fixed_range.quant;
  SANE_Word info = 0;
  SANE_Status status;

  status = sanei_constrain_value (&fixed_opt, &value, &info);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (info == 0);
  assert (value == fixed_range.min + fixed_range.quant);
}

static void
above_max_fixed_value (void)
{
  SANE_Int value = fixed_range.max + 1;
  SANE_Word info = 0;
  SANE_Status status;

  status = sanei_constrain_value (&fixed_opt, &value, &info);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (info == SANE_INFO_INEXACT);
  assert (value == fixed_range.max);
}


static void
above_max_word (void)
{
  SANE_Word value = 25000;
  SANE_Word info = 0;
  SANE_Status status;

  status = sanei_constrain_value (&word_array_opt, &value, &info);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (info == SANE_INFO_INEXACT);
  assert (value == 800);
}


static void
below_max_word (void)
{
  SANE_Word value = 1;
  SANE_Word info = 0;
  SANE_Status status;

  status = sanei_constrain_value (&word_array_opt, &value, &info);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (info == SANE_INFO_INEXACT);
  assert (value == 100);
}

static void
closest_200_word (void)
{
  SANE_Word value = 249;
  SANE_Word info = 0;
  SANE_Status status;

  status = sanei_constrain_value (&word_array_opt, &value, &info);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (info == SANE_INFO_INEXACT);
  assert (value == 200);
}


static void
closest_300_word (void)
{
  SANE_Word value = 251;
  SANE_Word info = 0;
  SANE_Status status;

  status = sanei_constrain_value (&word_array_opt, &value, &info);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (info == SANE_INFO_INEXACT);
  assert (value == 300);
}


static void
exact_400_word (void)
{
  SANE_Word value = 400;
  SANE_Word info = 0;
  SANE_Status status;

  status = sanei_constrain_value (&word_array_opt, &value, &info);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (info == 0);
  assert (value == 400);
}

/*
 * constrained int array
 */
static void
min_int_array (void)
{
  SANE_Int value[ARRAY_SIZE];
  SANE_Word info = 0;
  SANE_Status status;
  int i;

  for (i = 0; i < ARRAY_SIZE; i++)
    {
      value[i] = int_range.min;
    }
  status = sanei_constrain_value (&array_opt, value, &info);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (info == 0);
  for (i = 0; i < ARRAY_SIZE; i++)
    {
      assert (value[i] == int_range.min);
    }
}

static void
max_int_array (void)
{
  SANE_Int value[ARRAY_SIZE];
  SANE_Word info = 0;
  SANE_Status status;
  int i;

  for (i = 0; i < ARRAY_SIZE; i++)
    {
      value[i] = int_range.max;
    }

  status = sanei_constrain_value (&array_opt, value, &info);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (info == 0);
  for (i = 0; i < ARRAY_SIZE; i++)
    {
      assert (value[i] == int_range.max);
    }
}

static void
below_min_int_array (void)
{
  SANE_Int value[ARRAY_SIZE];
  SANE_Word info = 0;
  SANE_Status status;
  int i;

  for (i = 0; i < ARRAY_SIZE; i++)
    {
      value[i] = int_range.min - 1;
    }

  status = sanei_constrain_value (&array_opt, &value, &info);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (info == SANE_INFO_INEXACT);
  for (i = 0; i < ARRAY_SIZE; i++)
    {
      assert (value[i] == int_range.min);
    }
}

/* rounded to lower value */
static void
quant1_int_array (void)
{
  SANE_Int value[ARRAY_SIZE];
  SANE_Word info = 0;
  SANE_Status status;
  int i;

  for (i = 0; i < ARRAY_SIZE; i++)
    {
      value[i] = int_range.min + 1;
    }
  status = sanei_constrain_value (&array_opt, &value, &info);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (info == SANE_INFO_INEXACT);
  for (i = 0; i < ARRAY_SIZE; i++)
    {
      assert (value[i] == int_range.min);
    }
}

/* rounded to higher value */
static void
quant2_int_array (void)
{
  SANE_Int value[ARRAY_SIZE];
  SANE_Word info = 0;
  SANE_Status status;
  int i;

  for (i = 0; i < ARRAY_SIZE; i++)
    {
      value[i] = int_range.min + int_range.quant - 1;
    }
  status = sanei_constrain_value (&array_opt, &value, &info);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (info == SANE_INFO_INEXACT);
  for (i = 0; i < ARRAY_SIZE; i++)
    {
      assert (value[i] == int_range.min + int_range.quant);
    }
}

static void
in_range_int_array (void)
{
  SANE_Int value[ARRAY_SIZE];
  SANE_Word info = 0;
  SANE_Status status;
  int i;

  for (i = 0; i < ARRAY_SIZE; i++)
    {
      value[i] = int_range.min + int_range.quant;
    }

  status = sanei_constrain_value (&array_opt, &value, &info);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (info == 0);
  for (i = 0; i < ARRAY_SIZE; i++)
    {
      assert (value[i] == int_range.min + int_range.quant);
    }
}

static void
above_max_int_array (void)
{
  SANE_Int value[ARRAY_SIZE];
  SANE_Word info = 0;
  SANE_Status status;
  int i;

  for (i = 0; i < ARRAY_SIZE; i++)
    {
      value[i] = int_range.max + 1;
    }
  status = sanei_constrain_value (&array_opt, &value, &info);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (info == SANE_INFO_INEXACT);
  for (i = 0; i < ARRAY_SIZE; i++)
    {
      assert (value[i] == int_range.max);
    }
}

static void
wrong_string_array (void)
{
  SANE_Char value[9] = "wrong";
  SANE_Word info = 0;
  SANE_Status status;

  status = sanei_constrain_value (&string_array_opt, &value, &info);

  /* check results */
  assert (status == SANE_STATUS_INVAL);
  assert (info == 0);
}


static void
none_int (void)
{
  SANE_Int value = 555;
  SANE_Word info = 0;
  SANE_Status status;

  status = sanei_constrain_value (&none_opt, &value, &info);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (info == 0);
}


static void
none_bool_nok (void)
{
  SANE_Bool value = 555;
  SANE_Word info = 0;
  SANE_Status status;

  status = sanei_constrain_value (&none_bool_opt, &value, &info);

  /* check results */
  assert (status == SANE_STATUS_INVAL);
  assert (info == 0);
}


static void
none_bool_ok (void)
{
  SANE_Bool value = SANE_FALSE;
  SANE_Word info = 0;
  SANE_Status status;

  status = sanei_constrain_value (&none_bool_opt, &value, &info);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (info == 0);
}

/**
 * several partial match
 */
static void
string_array_several (void)
{
  SANE_Char value[9] = "Line";
  SANE_Word info = 0;
  SANE_Status status;

  status = sanei_constrain_value (&string_array_opt, &value, &info);

  /* check results */
  assert (status == SANE_STATUS_INVAL);
  assert (info == 0);
}

/**
 * unique partial match
 */
static void
partial_string_array (void)
{
  SANE_Char value[9] = "Linea";
  SANE_Word info = 0;
  SANE_Status status;

  status = sanei_constrain_value (&string_array_opt, &value, &info);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (info == 0);
}

static void
string_array_ignorecase (void)
{
  SANE_Char value[9] = "lineart";
  SANE_Word info = 0;
  SANE_Status status;

  status = sanei_constrain_value (&string_array_opt, &value, &info);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (info == 0);
}

static void
string_array_ok (void)
{
  SANE_Char value[9] = "Lineart";
  SANE_Word info = 0;
  SANE_Status status;

  status = sanei_constrain_value (&string_array_opt, &value, &info);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (info == 0);
}

/**
 * run the test suite for sanei constrain related tests 
 */
static void
sanei_constrain_suite (void)
{
  /* to be compatible with pre-C99 compilers */
  int_opt.constraint.range = &int_range;
  fixed_opt.constraint.range = &fixed_range;
  array_opt.constraint.range = &int_range;
  word_array_opt.constraint.word_list = dpi_list;
  string_array_opt.constraint.string_list = string_list;

  /* tests for constrained int value */
  min_int_value ();
  max_int_value ();
  below_min_int_value ();
  above_max_int_value ();
  quant1_int_value ();
  quant2_int_value ();
  in_range_int_value ();

  /* tests for sane fixed constrained value */
  min_fixed_value ();
  max_fixed_value ();
  below_min_fixed_value ();
  above_max_fixed_value ();
  quant1_fixed_value ();
  quant2_fixed_value ();
  in_range_fixed_value ();

  /* tests for constrained int array */
  min_int_array ();
  max_int_array ();
  below_min_int_array ();
  above_max_int_array ();
  quant1_int_array ();
  quant2_int_array ();
  in_range_int_array ();

  /* tests for word lists */
  above_max_word ();
  below_max_word ();
  closest_200_word ();
  closest_300_word ();
  exact_400_word ();

  /* tests for string lists */
  wrong_string_array ();
  partial_string_array ();
  string_array_ok ();
  string_array_ignorecase ();
  string_array_several ();

  /* constraint none tests  */
  none_int ();
  none_bool_nok ();
  none_bool_ok ();
}

/**
 * main function to run the test suites 
 */
int
main (void)
{
  /* run suites */
  sanei_constrain_suite ();

  return 0;
}

/* vim: set sw=2 cino=>2se-1sn-1s{s^-1st0(0u0 smarttab expandtab: */
