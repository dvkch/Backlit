#include "../../include/sane/config.h"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

/* sane includes for the sanei functions called */
#include "../../include/sane/sane.h"
#include "../../include/sane/saneopts.h"
#include "../../include/sane/sanei.h"

/* range for constraint */
static const SANE_Range int_range = {
  3,				/* minimum */
  18,				/* maximum */
  3				/* quantization */
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

static SANE_Option_Descriptor bool_opt = {
  SANE_NAME_SCAN_TL_X,
  SANE_TITLE_SCAN_TL_X,
  SANE_DESC_SCAN_TL_X,
  SANE_TYPE_BOOL,
  SANE_UNIT_MM,
  sizeof (SANE_Bool),
  0,
  SANE_CONSTRAINT_NONE,
  {NULL}
};

static SANE_Option_Descriptor bool_array_opt = {
  SANE_NAME_SCAN_TL_X,
  SANE_TITLE_SCAN_TL_X,
  SANE_DESC_SCAN_TL_X,
  SANE_TYPE_BOOL,
  SANE_UNIT_MM,
  sizeof (SANE_Bool) * ARRAY_SIZE,
  0,
  SANE_CONSTRAINT_NONE,
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
  SANE_Status status;

  status = sanei_check_value (&int_opt, &value);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (value == int_range.min);
}


static void
max_int_value (void)
{
  SANE_Int value = int_range.max;
  SANE_Status status;

  status = sanei_check_value (&int_opt, &value);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (value == int_range.max);
}


static void
below_min_int_value (void)
{
  SANE_Int value = int_range.min - 1;
  SANE_Status status;

  status = sanei_check_value (&int_opt, &value);

  /* check results */
  assert (status == SANE_STATUS_INVAL);
}


/* rounded to lower value */
static void
quant1_int_value (void)
{
  SANE_Int value = int_range.min + 1;
  SANE_Status status;

  status = sanei_check_value (&int_opt, &value);

  /* check results */
  assert (status == SANE_STATUS_INVAL);
}


/* close to higher value */
static void
quant2_int_value (void)
{
  SANE_Int value = int_range.min + int_range.quant - 1;
  SANE_Status status;

  status = sanei_check_value (&int_opt, &value);

  /* check results */
  assert (status == SANE_STATUS_INVAL);
}


static void
in_range_int_value (void)
{
  SANE_Int value = int_range.min + int_range.quant;
  SANE_Status status;

  status = sanei_check_value (&int_opt, &value);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  assert (value == int_range.min + int_range.quant);
}


static void
above_max_int_value (void)
{
  SANE_Int value = int_range.max + 1;
  SANE_Status status;

  status = sanei_check_value (&int_opt, &value);

  /* check results */
  assert (status == SANE_STATUS_INVAL);
}


/*
 * constrained int array
 */
static void
min_int_array (void)
{
  SANE_Int value[ARRAY_SIZE];
  SANE_Status status;
  int i;

  for (i = 0; i < ARRAY_SIZE; i++)
    {
      value[i] = int_range.min;
    }
  status = sanei_check_value (&array_opt, value);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  for (i = 0; i < ARRAY_SIZE; i++)
    {
      assert (value[i] == int_range.min);
    }
}


static void
max_int_array (void)
{
  SANE_Int value[ARRAY_SIZE];
  SANE_Status status;
  int i;

  for (i = 0; i < ARRAY_SIZE; i++)
    {
      value[i] = int_range.max;
    }

  status = sanei_check_value (&array_opt, value);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  for (i = 0; i < ARRAY_SIZE; i++)
    {
      assert (value[i] == int_range.max);
    }
}


static void
below_min_int_array (void)
{
  SANE_Int value[ARRAY_SIZE];
  SANE_Status status;
  int i;

  for (i = 0; i < ARRAY_SIZE; i++)
    {
      value[i] = int_range.min - 1;
    }

  status = sanei_check_value (&array_opt, &value);

  /* check results */
  assert (status == SANE_STATUS_INVAL);
}


/* rounded to lower value */
static void
quant1_int_array (void)
{
  SANE_Int value[ARRAY_SIZE];
  SANE_Status status;
  int i;

  for (i = 0; i < ARRAY_SIZE; i++)
    {
      value[i] = int_range.min + 1;
    }
  status = sanei_check_value (&array_opt, &value);

  /* check results */
  assert (status == SANE_STATUS_INVAL);
}


/* rounded to higher value */
static void
quant2_int_array (void)
{
  SANE_Int value[ARRAY_SIZE];
  SANE_Status status;
  int i;

  for (i = 0; i < ARRAY_SIZE; i++)
    {
      value[i] = int_range.min + int_range.quant - 1;
    }
  status = sanei_check_value (&array_opt, &value);

  /* check results */
  assert (status == SANE_STATUS_INVAL);
}


static void
in_range_int_array (void)
{
  SANE_Int value[ARRAY_SIZE];
  SANE_Status status;
  int i;

  for (i = 0; i < ARRAY_SIZE; i++)
    {
      value[i] = int_range.min + int_range.quant;
    }

  status = sanei_check_value (&array_opt, &value);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
  for (i = 0; i < ARRAY_SIZE; i++)
    {
      assert (value[i] == int_range.min + int_range.quant);
    }
}


static void
above_max_int_array (void)
{
  SANE_Int value[ARRAY_SIZE];
  SANE_Status status;
  int i;

  for (i = 0; i < ARRAY_SIZE; i++)
    {
      value[i] = int_range.max + 1;
    }
  status = sanei_check_value (&array_opt, &value);

  /* check results */
  assert (status == SANE_STATUS_INVAL);
}


static void
bool_true (void)
{
  SANE_Bool value = SANE_TRUE;
  SANE_Status status;
  status = sanei_check_value (&bool_opt, &value);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
}


static void
bool_false (void)
{
  SANE_Bool value = SANE_FALSE;
  SANE_Status status;
  status = sanei_check_value (&bool_opt, &value);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
}


static void
wrong_bool (void)
{
  SANE_Bool value = 2;
  SANE_Status status;
  status = sanei_check_value (&bool_opt, &value);

  /* check results */
  assert (status == SANE_STATUS_INVAL);
}


static void
bool_array (void)
{
  SANE_Bool value[ARRAY_SIZE];
  SANE_Status status;
  int i;
  for (i = 0; i < ARRAY_SIZE; i++)
    value[i] = i % 2;
  status = sanei_check_value (&bool_array_opt, &value);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
}


static void
word_array_ok (void)
{
  SANE_Word value = 400;
  SANE_Status status;
  status = sanei_check_value (&word_array_opt, &value);

  /* check results */
  assert (status == SANE_STATUS_GOOD);
}


static void
word_array_nok (void)
{
  SANE_Word value = 444;
  SANE_Status status;
  status = sanei_check_value (&word_array_opt, &value);

  /* check results */
  assert (status == SANE_STATUS_INVAL);
}

static void
wrong_bool_array (void)
{
  SANE_Bool value[ARRAY_SIZE];
  SANE_Status status;
  int i;
  for (i = 0; i < ARRAY_SIZE; i++)
    value[i] = i % 2;
  value[3] = 4;
  status = sanei_check_value (&bool_array_opt, &value);

  /* check results */
  assert (status == SANE_STATUS_INVAL);
}


/**
 * run the test suite for sanei_check_value related tests 
 */
static void
sanei_check_suite (void)
{
  /* to be compatible with pre-C99 compilers */
  int_opt.constraint.range = &int_range;
  array_opt.constraint.range = &int_range;
  word_array_opt.constraint.word_list = dpi_list;

  /* tests for constrained int value */
  min_int_value ();
  max_int_value ();
  below_min_int_value ();
  above_max_int_value ();
  quant1_int_value ();
  quant2_int_value ();
  in_range_int_value ();

  /* tests for constrained int array */
  min_int_array ();
  max_int_array ();
  below_min_int_array ();
  above_max_int_array ();
  quant1_int_array ();
  quant2_int_array ();
  in_range_int_array ();

  /* tests for boolean value */
  bool_true ();
  bool_false ();
  wrong_bool ();
  bool_array ();
  wrong_bool_array ();

  /* word array test */
  word_array_ok ();
  word_array_nok ();
}


int
main (void)
{
  sanei_check_suite ();
  return 0;
}

/* vim: set sw=2 cino=>2se-1sn-1s{s^-1st0(0u0 smarttab expandtab: */
