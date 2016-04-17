/* sane - Scanner Access Now Easy.
   Copyright (C) 1997 Geoffrey T. Dairiki
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

   This file is part of a SANE backend for HP Scanners supporting
   HP Scanner Control Language (SCL).
*/

/*
   $Log$
   Revision 1.13  2005/04/13 12:50:07  ellert-guest
   Add missing SANE_I18N, Regenerate .po files accordingly, Update Swedish translations

   Revision 1.12  2003/10/09 19:32:50  kig-guest
   Bug #300241: fix invers image on 3c/4c/6100C at 10 bit depth


*/

/* pwd.h not available ? */
#if (defined(__IBMC__) || defined(__IBMCPP__))
#ifndef _AIX
#  define SANE_HOME_HP "SANE_HOME_HP"
#endif
#endif

/* To be done: dont reallocate choice accessors */
/* #define HP_ALLOC_CHOICEACC_ONCE 1 */
/*
#define HP_EXPERIMENTAL
*/ /*
#define STUBS
extern int sanei_debug_hp; */
#define DEBUG_DECLARE_ONLY
#include "../include/sane/config.h"
#include "../include/sane/sanei_backend.h"
#include "../include/lalloca.h"

#include <stdio.h>
#include <string.h>
#include "../include/lassert.h"
#ifndef SANE_HOME_HP
#include <pwd.h>
#endif
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei.h"
#include "hp.h"
#include "hp-option.h"
#include "hp-accessor.h"
#include "hp-scsi.h"
#include "hp-scl.h"
#include "hp-device.h"


/* FIXME: descriptors should be static? */

typedef SANE_Option_Descriptor *	_HpSaneOption;
typedef struct hp_option_descriptor_s *	_HpOptionDescriptor;
typedef struct hp_option_s *		_HpOption;

typedef struct hp_data_info_s *	HpDataInfo;
typedef struct hp_data_info_s *		_HpDataInfo;

typedef HpAccessor		HpAccessorOptd;


static hp_bool_t hp_optset_isEnabled (HpOptSet this, HpData data,
                     const char *name, const HpDeviceInfo *info);
static HpOption hp_optset_get	(HpOptSet this, HpOptionDescriptor optd);
static HpOption hp_optset_getByName (HpOptSet this, const char * name);
static SANE_Status hp_download_calib_file (HpScsi scsi);
static SANE_Status hp_probe_parameter_support_table (enum hp_device_compat_e
                     compat, HpScl scl, int value);

#define HP_EOL -9999

/* Dont need requiries for commands that are probed */
#define HP_PROBE_SCL_COMMAND 1

/* Scale factor for vectors (gtk seems not to like vectors/curves
 * in y-range 0.0,...,1.0)
 */
#define HP_VECTOR_SCALE (256.0)
/*
 *
 */
struct hp_option_s
{
    HpOptionDescriptor  descriptor;
    HpAccessorOptd	optd_acsr;
    HpAccessor		data_acsr;
    void *		extra;
};

struct hp_option_descriptor_s
{
    const char *	name;
    const char *	title;
    const char *	desc;
    SANE_Value_Type	type;
    SANE_Unit		unit;
    SANE_Int		cap;

    enum hp_device_compat_e requires;   /* model dependent support flags */

    /* probe for option support */
    SANE_Status	(*probe)    (_HpOption this, HpScsi scsi, HpOptSet optset,
			     HpData data);
    SANE_Status (*program)  (HpOption this, HpScsi scsi, HpOptSet optset,
			     HpData data);
    hp_bool_t	(*enable)   (HpOption this, HpOptSet optset, HpData data,
                             const HpDeviceInfo *info);

    hp_bool_t		has_global_effect;
    hp_bool_t		affects_scan_params;
    hp_bool_t		program_immediate;
    hp_bool_t           suppress_for_scan;
    hp_bool_t		may_change;


    /* This stuff should really be in a subclasses: */
    HpScl		scl_command;
    int                 minval, maxval, startval; /* for simulation */
    HpChoice		choices;
};

struct hp_data_info_s
{
    HpScl		scl;
};

static const struct hp_option_descriptor_s
    NUM_OPTIONS[1], PREVIEW_MODE[1], SCAN_MODE[1], SCAN_RESOLUTION[1],

    CUSTOM_GAMMA[1], GAMMA_VECTOR_8x8[1],
#ifdef ENABLE_7x12_TONEMAPS
    GAMMA_VECTOR_7x12[1],
    RGB_TONEMAP[1], GAMMA_VECTOR_R[1], GAMMA_VECTOR_G[1], GAMMA_VECTOR_B[1],
#endif
    HALFTONE_PATTERN[1], MEDIA[1], OUT8[1], BIT_DEPTH[1], SCAN_SOURCE[1],
#ifdef FAKE_COLORSEP_MATRIXES
    SEPMATRIX[1],
#endif
    MATRIX_TYPE[1];


/* Check if a certain scanner model supports a command with a given parameter
 * value. The function returns SANE_STATUS_GOOD if the command and the
 * value is found in the support table of that scanner.
 * It returns SANE_STATUS_UNSUPPORTED if the command is found in the support
 * table of that scanner, but the value is not included in the table.
 * It returns SANE_STATUS_EOF if there is no information about that command
 * and that scanner in the support table.
 */
static SANE_Status
hp_probe_parameter_support_table (enum hp_device_compat_e compat,
                                  HpScl scl, int value)

{int k, j;
 char *eptr;
 static int photosmart_output_type[] =
   /* HP Photosmart: only b/w, gray, color is supported */
   { HP_COMPAT_PS, SCL_OUTPUT_DATA_TYPE, 0, 4, 5, HP_EOL };

 static int *support_table[] =
 {
   photosmart_output_type
 };

 eptr = getenv ("SANE_HP_CHK_TABLE");
 if ((eptr != NULL) && (*eptr == '0'))
   return SANE_STATUS_EOF;

 for (k = 0; k < (int)(sizeof (support_table)/sizeof (support_table[0])); k++)
 {
   if ((scl == support_table[k][1]) && (support_table[k][0] & compat))
   {
      for (j = 2; support_table[k][j] != HP_EOL; j++)
        if (support_table[k][j] == value) return SANE_STATUS_GOOD;
      return SANE_STATUS_UNSUPPORTED;
   }
 }
 return SANE_STATUS_EOF;
}


/*
 * class HpChoice
 */
typedef struct hp_choice_s * _HpChoice;

static hp_bool_t
hp_choice_isSupported (HpChoice choice, int minval, int maxval)
{
  return ( choice->is_emulated
	   || ( choice->val >= minval && choice->val <= maxval ) );
}

static hp_bool_t
hp_probed_choice_isSupported (HpScsi scsi, HpScl scl,
                              HpChoice choice, int minval, int maxval)
{
  hp_bool_t isSupported;
  SANE_Status status;
  enum hp_device_compat_e compat;

  if ( choice->is_emulated )
  {
     DBG(3, "probed_choice: value %d is emulated\n", choice->val);
     return ( 1 );
  }
  if ( choice->val < minval || choice->val > maxval )
  {
     DBG(3, "probed_choice: value %d out of range (%d,%d)\n", choice->val,
         minval, maxval);
     return ( 0 );
  }

  if (sanei_hp_device_probe (&compat, scsi) != SANE_STATUS_GOOD)
  {
     DBG(1, "probed_choice: Could not get compatibilities for scanner\n");
     return ( 0 );
  }

  status = hp_probe_parameter_support_table (compat, scl, choice->val);
  if (status == SANE_STATUS_GOOD)
  {
    DBG(3, "probed_choice: command/value found in support table\n");
    return ( 1 );
  }
  else if (status == SANE_STATUS_UNSUPPORTED)
  {
    DBG(3, "probed_choice: command found in support table, but value n.s.\n");
    return ( 0 );
  }

  /* Not in the support table. Try to inquire */
  /* Fix me: It seems that the scanner does not raise a parameter error */
  /* after specifiying an unsupported command-value. */

  sanei_hp_scl_clearErrors (scsi);
  sanei_hp_scl_set (scsi, scl, choice->val);

  isSupported = ( sanei_hp_scl_errcheck (scsi) == SANE_STATUS_GOOD );

  DBG(3, "probed_choice: value %d %s\n", choice->val,
       isSupported ? "supported" : "not supported");
  return isSupported;
}

hp_bool_t
sanei_hp_choice_isEnabled (HpChoice this, HpOptSet optset, HpData data,
                           const HpDeviceInfo *info)
{
  if (!this->enable)
      return 1;
  return (*this->enable)(this, optset, data, info);
}

static hp_bool_t
_cenable_incolor (HpChoice UNUSEDARG this, HpOptSet optset, HpData data,
                  const HpDeviceInfo UNUSEDARG *info)
{
  return sanei_hp_optset_scanmode(optset, data) == HP_SCANMODE_COLOR;
}

static hp_bool_t
_cenable_notcolor (HpChoice UNUSEDARG this, HpOptSet optset, HpData data,
                   const HpDeviceInfo UNUSEDARG *info)
{
  return sanei_hp_optset_scanmode(optset, data) != HP_SCANMODE_COLOR;
}

/*
 * class HpAccessorOptd
 */
static HpAccessorOptd
hp_accessor_optd_new (HpData data)
{
  return sanei_hp_accessor_new(data, sizeof(SANE_Option_Descriptor));
}

static _HpSaneOption
hp_accessor_optd_data (HpAccessorOptd this, HpData data)
{
  return sanei__hp_accessor_data(this, data);
}



/*
 * class OptionDescriptor
 */

static SANE_Status
hp_option_descriptor_probe (HpOptionDescriptor desc, HpScsi scsi,
			    HpOptSet optset, HpData data, HpOption * newoptp)
{
  _HpOption 	new;
  SANE_Status	status;
  _HpSaneOption	optd;

  new = sanei_hp_alloc(sizeof(*new));
  new->descriptor  = desc;
  if (!(new->optd_acsr = hp_accessor_optd_new(data)))
      return SANE_STATUS_NO_MEM;
  new->data_acsr = 0;
  optd = hp_accessor_optd_data(new->optd_acsr, data);

  memset(optd, 0, sizeof(*optd));
  optd->name  = desc->name;
  optd->title = desc->title;
  optd->desc  = desc->desc;
  optd->type  = desc->type;
  optd->unit  = desc->unit;
  optd->cap   = desc->cap;

  /*
   * Probe function will set optd->size, optd->constraint_type,
   * and optd->constraint.
   * and also new->accessor
   * and possibly new->extra
   */
  if (desc->probe)
    {
      if (FAILED( status = (*desc->probe)(new, scsi, optset, data) ))
	{
	  /* hp_accessor_optd_destoy(new->optd_acsr) */
	  sanei_hp_free(new);
	  return status;
	}
    }

  *newoptp = new;
  return SANE_STATUS_GOOD;
}


/*
 * class Option
 */
static HpSaneOption
hp_option_saneoption (HpOption this, HpData data)
{
  return hp_accessor_optd_data(this->optd_acsr, data);
}

static _HpSaneOption
_hp_option_saneoption (HpOption this, HpData data)
{
  return hp_accessor_optd_data(this->optd_acsr, data);
}

static SANE_Status
hp_option_download (HpOption this, HpData data, HpOptSet optset, HpScsi scsi)
{
  HpScl scl = this->descriptor->scl_command;
  int value;

  if (IS_SCL_CONTROL(scl))
  {
      value = sanei_hp_accessor_getint(this->data_acsr, data);
      if (   (scl == SCL_DATA_WIDTH)
          && (sanei_hp_optset_scanmode (optset, data) == HP_SCANMODE_COLOR) )
      {
        value *= 3;
      }
      return sanei_hp_scl_set(scsi, scl, value);
  }
  else if (IS_SCL_DATA_TYPE(scl))
      return sanei_hp_scl_download(scsi, scl,
			     sanei_hp_accessor_data(this->data_acsr, data),
			     sanei_hp_accessor_size(this->data_acsr));
  assert(!scl);
  return SANE_STATUS_INVAL;
}

static SANE_Status
hp_option_upload (HpOption this, HpScsi scsi, HpOptSet optset, HpData data)
{
  HpScl scl = this->descriptor->scl_command;
  int	val;

  if (IS_SCL_CONTROL(scl))
    {
      RETURN_IF_FAIL( sanei_hp_scl_inquire(scsi, scl, &val, 0, 0) );
      if (   (scl == SCL_DATA_WIDTH)
          && (sanei_hp_optset_scanmode (optset, data) == HP_SCANMODE_COLOR) )
      {
        val /= 3;
      }
      sanei_hp_accessor_setint(this->data_acsr, data, val);
      return SANE_STATUS_GOOD;
    }
  else if (IS_SCL_DATA_TYPE(scl))
      return sanei_hp_scl_upload(scsi, scl,
			   sanei__hp_accessor_data(this->data_acsr, data),
			   sanei_hp_accessor_size(this->data_acsr));
  assert(!scl);
  return SANE_STATUS_INVAL;
}

static SANE_Status
hp_option_program (HpOption this, HpScsi scsi, HpOptSet optset, HpData data)
{
 const HpDeviceInfo *info;

  DBG(10, "hp_option_program: name=%s, enable=0x%08lx, program=0x%08lx\n",
      this->descriptor->name, (long)this->descriptor->enable,
      (long)this->descriptor->program);

  /* Replaced by flag suppress_for_scan
   * if (this->descriptor->program_immediate)
   * {
   *    DBG(10, "hp_option_program: is program_immediate. Dont program now.\n");
   *    return SANE_STATUS_GOOD;
   * }
   */

  if (!this->descriptor->program)
      return SANE_STATUS_GOOD;

  info = sanei_hp_device_info_get ( sanei_hp_scsi_devicename  (scsi) );
  if (this->descriptor->enable
      && !(*this->descriptor->enable)(this, optset, data, info))
      return SANE_STATUS_GOOD;

  return (*this->descriptor->program)(this, scsi, optset, data);
}

static SANE_Status
hp_option_get (HpOption this, HpData data, void * valp)
{
  if (!this->data_acsr)
      return SANE_STATUS_INVAL;
  return sanei_hp_accessor_get(this->data_acsr, data, valp);
}

static hp_bool_t
_values_are_equal (HpOption this, HpData data,
		   const void * val1, const void * val2)
{
  HpSaneOption optd = hp_option_saneoption(this, data);

  if (optd->type == SANE_TYPE_STRING)
      return strncmp((const char *)val1, (const char *)val2, optd->size) == 0;
  else
      return memcmp(val1, val2, optd->size) == 0;
}

static hp_bool_t
hp_option_isImmediate (HpOption this)
{
  return (   this->descriptor->program_immediate
          && this->descriptor->program );
}

static SANE_Status
hp_option_imm_set (HpOptSet optset, HpOption this, HpData data,
                   void * valp, SANE_Int * info, HpScsi scsi)
{
  HpSaneOption	optd	= hp_option_saneoption(this, data);
  hp_byte_t *	old_val	= alloca(optd->size);
  SANE_Status	status;

  assert (this->descriptor->program_immediate && this->descriptor->program);

  if (!SANE_OPTION_IS_SETTABLE(optd->cap))
      return SANE_STATUS_INVAL;

  DBG(10,"hp_option_imm_set: %s\n", this->descriptor->name);

  if ( this->descriptor->type == SANE_TYPE_BUTTON )
    {
      status = (*this->descriptor->program)(this, scsi, optset, data);
      if ( !FAILED(status) && info )
        {
	  if (this->descriptor->has_global_effect)
	    *info |= SANE_INFO_RELOAD_OPTIONS;
	  if (this->descriptor->affects_scan_params)
	    *info |= SANE_INFO_RELOAD_PARAMS;
        }
      return status;
    }

  if ( !this->data_acsr )
      return SANE_STATUS_INVAL;

  if (!old_val)
      return SANE_STATUS_NO_MEM;

  if (FAILED( status = sanei_constrain_value(optd, valp, info) ))
    {
      DBG(1, "option_imm_set: %s: constrain_value failed :%s\n",
	  this->descriptor->name, sane_strstatus(status));
      return status;
    }

  RETURN_IF_FAIL( sanei_hp_accessor_get(this->data_acsr, data, old_val) );

  if (_values_are_equal(this, data, old_val, valp))
    {
      DBG(3, "option_imm_set: value unchanged\n");
      return SANE_STATUS_GOOD;
    }

  if (info)
      memcpy(old_val, valp, optd->size); /* Save requested value */

  RETURN_IF_FAIL( sanei_hp_accessor_set(this->data_acsr, data, valp) );

  if ( this->descriptor->type == SANE_TYPE_STRING )
     RETURN_IF_FAIL( (*this->descriptor->program)(this, scsi, optset, data) );

  if (info)
    {
      if (!_values_are_equal(this, data, old_val, valp))
	  *info |= SANE_INFO_INEXACT;
      if (this->descriptor->has_global_effect)
	  *info |= SANE_INFO_RELOAD_OPTIONS;
      if (this->descriptor->affects_scan_params)
	  *info |= SANE_INFO_RELOAD_PARAMS;
    }

  return SANE_STATUS_GOOD;
}

static SANE_Status
hp_option_set (HpOption this, HpData data, void * valp, SANE_Int * info)
{
  HpSaneOption	optd	= hp_option_saneoption(this, data);
  hp_byte_t *	old_val	= alloca(optd->size);
  SANE_Status	status;
  char sval[64];


  if (!SANE_OPTION_IS_SETTABLE(optd->cap) || !this->data_acsr)
      return SANE_STATUS_INVAL;
  if (!old_val)
      return SANE_STATUS_NO_MEM;

  sval[0] = '\0';
  if (this->descriptor->type == SANE_TYPE_INT)
    sprintf (sval," value=%d", *(int*)valp);

  DBG(10,"hp_option_set: %s%s\n", this->descriptor->name, sval);

  if (FAILED( status = sanei_constrain_value(optd, valp, info) ))
    {
      DBG(1, "option_set: %s: constrain_value failed :%s\n",
	  this->descriptor->name, sane_strstatus(status));
      return status;
    }

  RETURN_IF_FAIL( sanei_hp_accessor_get(this->data_acsr, data, old_val) );

  if (_values_are_equal(this, data, old_val, valp))
    {
      DBG(3, "option_set: %s: value unchanged\n",this->descriptor->name);
      return SANE_STATUS_GOOD;
    }

  if (info)
      memcpy(old_val, valp, optd->size); /* Save requested value */

  RETURN_IF_FAIL( sanei_hp_accessor_set(this->data_acsr, data, valp) );

  if (info)
    {
      if (!_values_are_equal(this, data, old_val, valp))
	  *info |= SANE_INFO_INEXACT;
      if (this->descriptor->has_global_effect)
	  *info |= SANE_INFO_RELOAD_OPTIONS;
      if (this->descriptor->affects_scan_params)
	  *info |= SANE_INFO_RELOAD_PARAMS;

      DBG(3, "option_set: %s: info=0x%lx\n",this->descriptor->name,
          (long)*info);
    }

  return SANE_STATUS_GOOD;
}

static int
hp_option_getint (HpOption this, HpData data)
{
  return sanei_hp_accessor_getint(this->data_acsr, data);
}

static SANE_Status
hp_option_imm_control (HpOptSet optset, HpOption this, HpData data,
		   SANE_Action action, void * valp, SANE_Int *infop,
		   HpScsi scsi)
{
  HpSaneOption	optd	= hp_option_saneoption(this, data);

  if (!SANE_OPTION_IS_ACTIVE(optd->cap))
      return SANE_STATUS_INVAL;

  switch (action) {
  case SANE_ACTION_GET_VALUE:
      return hp_option_get(this, data, valp);
  case SANE_ACTION_SET_VALUE:
      return hp_option_imm_set(optset, this, data, valp, infop, scsi);
  case SANE_ACTION_SET_AUTO:
  default:
      return SANE_STATUS_INVAL;
  }
}

static SANE_Status
hp_option_control (HpOption this, HpData data,
                   SANE_Action action, void * valp, SANE_Int *infop)
{
  HpSaneOption  optd    = hp_option_saneoption(this, data);

  if (!SANE_OPTION_IS_ACTIVE(optd->cap))
      return SANE_STATUS_INVAL;

  switch (action) {
  case SANE_ACTION_GET_VALUE:
      return hp_option_get(this, data, valp);
  case SANE_ACTION_SET_VALUE:
      return hp_option_set(this, data, valp, infop);
  case SANE_ACTION_SET_AUTO:
  default:
      return SANE_STATUS_INVAL;
  }
}


static void
hp_option_reprogram (HpOption this, HpOptSet optset, HpData data, HpScsi scsi)
{
  if (this->descriptor->may_change)
  {
    DBG(5, "hp_option_reprogram: %s\n", this->descriptor->name);

    hp_option_program (this, scsi, optset, data);
  }
}


static void
hp_option_reprobe (HpOption this, HpOptSet optset, HpData data, HpScsi scsi)
{
  if (this->descriptor->may_change)
  {
    DBG(5, "hp_option_reprobe: %s\n", this->descriptor->name);

    (*this->descriptor->probe)((_HpOption)this, scsi, optset, data);
  }
}

static void
hp_option_updateEnable (HpOption this, HpOptSet optset, HpData data,
                        const HpDeviceInfo *info)
{
  hp_bool_t 	(*f)(HpOption, HpOptSet, HpData, const HpDeviceInfo *)
                    = this->descriptor->enable;
  _HpSaneOption	optd 	= _hp_option_saneoption(this, data);

  if (!f || (*f)(this, optset, data, info))
      optd->cap &= ~SANE_CAP_INACTIVE;
  else
      optd->cap |= SANE_CAP_INACTIVE;
}

static hp_bool_t
hp_option_isInternal (HpOption this)
{
  return this->descriptor->name[0] == '_';
}


/*
 * Option probe functions
 */

static SANE_Status
_set_range (HpOption opt, HpData data,
	    SANE_Word min, SANE_Word quant, SANE_Word max)
{
  _HpSaneOption	optd	= _hp_option_saneoption(opt, data);
  SANE_Range * range	= sanei_hp_alloc(sizeof(*range)); /* FIXME: leak? */

  if (! range)
      return SANE_STATUS_NO_MEM;

  range->min = min;
  range->max = max;
  range->quant = quant;
  optd->constraint.range = range;
  optd->constraint_type = SANE_CONSTRAINT_RANGE;

  return SANE_STATUS_GOOD;
}

static void
_set_size (HpOption opt, HpData data, SANE_Int size)
{
  _hp_option_saneoption(opt, data)->size = size;
}

/* #ifdef HP_EXPERIMENTAL */
static SANE_Status
_probe_int (_HpOption this, HpScsi scsi, HpOptSet UNUSEDARG optset, HpData data)
{
  HpScl		scl	= this->descriptor->scl_command;
  int		minval, maxval;
  int		val	= 0;

  assert(scl);

  RETURN_IF_FAIL( sanei_hp_scl_inquire(scsi, scl, &val,  &minval, &maxval) );

  if (minval >= maxval)
      return SANE_STATUS_UNSUPPORTED;

  /* If we dont have an accessor, get one */
  if (!this->data_acsr)
  {
      if (!(this->data_acsr = sanei_hp_accessor_int_new(data)))
          return SANE_STATUS_NO_MEM;
  }
  sanei_hp_accessor_setint(this->data_acsr, data, val);
  _set_size(this, data, sizeof(SANE_Int));
  return _set_range(this, data, minval, 1, maxval);
}
/* #endif */

static SANE_Status
_probe_int_brightness (_HpOption this, HpScsi scsi, HpOptSet UNUSEDARG optset,
                      HpData data)
{
  HpScl		scl	= this->descriptor->scl_command;
  int		minval, maxval;
  int		val	= 0;
  hp_bool_t     simulate;

  assert(scl);

  simulate = (   sanei_hp_device_support_get (
                    sanei_hp_scsi_devicename  (scsi), scl, 0, 0)
              != SANE_STATUS_GOOD );

  if ( simulate )
  {
    val = this->descriptor->startval;
    minval = this->descriptor->minval;
    maxval = this->descriptor->maxval;
  }
  else
  {
    RETURN_IF_FAIL ( sanei_hp_scl_inquire(scsi,scl,&val,&minval,&maxval) );
  }

  if (minval >= maxval)
      return SANE_STATUS_UNSUPPORTED;

  /* If we dont have an accessor, get one */
  if (!this->data_acsr)
  {
      if (!(this->data_acsr = sanei_hp_accessor_int_new(data)))
          return SANE_STATUS_NO_MEM;
  }

  sanei_hp_accessor_setint(this->data_acsr, data, val);
  _set_size(this, data, sizeof(SANE_Int));
  return _set_range(this, data, minval, 1, maxval);
}

static SANE_Status
_probe_resolution (_HpOption this, HpScsi scsi, HpOptSet UNUSEDARG optset,
                   HpData data)
{
  int		minval, maxval, min2, max2;
  int		val	= 0, val2;
  int           quant   = 1;
  enum hp_device_compat_e compat;

  /* Check for supported resolutions in both directions */
  RETURN_IF_FAIL( sanei_hp_scl_inquire(scsi, SCL_X_RESOLUTION, &val,
                                 &minval, &maxval) );
  RETURN_IF_FAIL( sanei_hp_scl_inquire(scsi, SCL_Y_RESOLUTION, &val2, &min2, &max2));
  if ( min2 > minval ) minval = min2;
  if ( max2 < maxval ) maxval = max2;

  if (minval >= maxval)
      return SANE_STATUS_UNSUPPORTED;

  /* If we dont have an accessor, get one */
  if (!this->data_acsr)
  {
    if (!(this->data_acsr = sanei_hp_accessor_int_new(data)))
      return SANE_STATUS_NO_MEM;
  }
  sanei_hp_accessor_setint(this->data_acsr, data, val);
  _set_size(this, data, sizeof(SANE_Int));

  /* The HP OfficeJet Pro 1150C crashes the scan head when scanning at
   * resolutions less than 42 dpi.  Set a safe minimum resolution.
   * Hopefully 50 dpi is safe enough. */
  if ((sanei_hp_device_probe(&compat,scsi)==SANE_STATUS_GOOD) &&
      ((compat&(HP_COMPAT_OJ_1150C|HP_COMPAT_OJ_1170C))==HP_COMPAT_OJ_1150C)) {
	if (minval<50) minval=50;
  }

  /* HP Photosmart scanner does not allow scanning at arbitrary resolutions */
  /* for slides/negatives. Must be multiple of 300 dpi. Set quantization. */

  if (   (sanei_hp_device_probe (&compat, scsi) == SANE_STATUS_GOOD)
      && (compat & HP_COMPAT_PS) )
  {int val, mi, ma;

    if (   (sanei_hp_scl_inquire(scsi, SCL_MEDIA, &val, &mi, &ma)
              == SANE_STATUS_GOOD)
        && ((val == HP_MEDIA_SLIDE) || (val == HP_MEDIA_NEGATIVE)) )
      quant = 300;
      minval = (minval+quant-1)/quant;
      minval *= quant;
      maxval = (maxval+quant-1)/quant;
      maxval *= quant;
  }
  DBG(5, "_probe_resolution: set range %d..%d, quant=%d\n",minval,maxval,quant);

  return _set_range(this, data, minval, quant, maxval);
}

static SANE_Status
_probe_bool (_HpOption this, HpScsi scsi, HpOptSet UNUSEDARG optset,
             HpData data)
{
  HpScl		scl	= this->descriptor->scl_command;
  int		val	= 0;

  if (scl)
      RETURN_IF_FAIL( sanei_hp_scl_inquire(scsi, scl, &val, 0, 0) );

  /* If we dont have an accessor, get one */
  if (!this->data_acsr)
  {
      if (!(this->data_acsr = sanei_hp_accessor_bool_new(data)))
           return SANE_STATUS_NO_MEM;
  }

  sanei_hp_accessor_setint(this->data_acsr, data, val);
  _set_size(this, data, sizeof(SANE_Bool));
  return SANE_STATUS_GOOD;
}


static SANE_Status
_probe_change_doc (_HpOption this, HpScsi scsi, HpOptSet UNUSEDARG optset,
                   HpData data)

{SANE_Status status;
 int cap = 0;

  DBG(2, "probe_change_doc: inquire ADF capability\n");

  status = sanei_hp_scl_inquire(scsi, SCL_ADF_CAPABILITY, &cap, 0, 0);
  if ( (status != SANE_STATUS_GOOD) || (cap == 0))
    return SANE_STATUS_UNSUPPORTED;

  DBG(2, "probe_change_doc: check if change document is supported\n");

  status = sanei_hp_scl_inquire(scsi, SCL_CHANGE_DOC, &cap, 0, 0);
  if ( status != SANE_STATUS_GOOD )
    return SANE_STATUS_UNSUPPORTED;

  /* If we dont have an accessor, get one */
  if (!this->data_acsr)
  {
      if (!(this->data_acsr = sanei_hp_accessor_bool_new(data)))
          return SANE_STATUS_NO_MEM;
  }

  sanei_hp_accessor_setint(this->data_acsr, data, cap);
  _set_size(this, data, sizeof(SANE_Bool));

  return SANE_STATUS_GOOD;
}

/* The OfficeJets support SCL_UNLOAD even when no ADF is installed, so
 * this function was added to check for SCL_ADF_CAPABILITY, similar to
 * _probe_change_doc(), to hide the unnecessary "Unload" button on
 * non-ADF OfficeJets. */
static SANE_Status
_probe_unload (_HpOption this, HpScsi scsi, HpOptSet UNUSEDARG optset,
               HpData data)

{SANE_Status status;
 int cap = 0;

  DBG(2, "probe_unload: inquire ADF capability\n");

  status = sanei_hp_scl_inquire(scsi, SCL_ADF_CAPABILITY, &cap, 0, 0);
  if ( (status != SANE_STATUS_GOOD) || (cap == 0))
    return SANE_STATUS_UNSUPPORTED;

  DBG(2, "probe_unload: check if unload is supported\n");

  status = sanei_hp_scl_inquire(scsi, SCL_UNLOAD, &cap, 0, 0);
  if ( status != SANE_STATUS_GOOD )
    return SANE_STATUS_UNSUPPORTED;

  /* If we dont have an accessor, get one */
  if (!this->data_acsr)
  {
      if (!(this->data_acsr = sanei_hp_accessor_bool_new(data)))
          return SANE_STATUS_NO_MEM;
  }

  sanei_hp_accessor_setint(this->data_acsr, data, cap);
  _set_size(this, data, sizeof(SANE_Bool));

  return SANE_STATUS_GOOD;
}

static SANE_Status
_probe_calibrate (_HpOption this, HpScsi scsi, HpOptSet UNUSEDARG optset,
                  HpData data)
{
  int val = 0;  /* Always false */
  int minval, maxval;
  int media;
  int download_calib_file = 1;
  enum hp_device_compat_e compat;

  /* The OfficeJets don't seem to support calibration, so we'll
   * remove it from the option list to reduce frontend clutter. */
  if ((sanei_hp_device_probe (&compat, scsi) == SANE_STATUS_GOOD) &&
      (compat & HP_COMPAT_OJ_1150C)) {
	return SANE_STATUS_UNSUPPORTED;
  }

  /* If we have a Photosmart scanner, we only download the calibration file */
  /* when medium is set to prints */
  media = -1;
  if (sanei_hp_scl_inquire(scsi, SCL_MEDIA, &val, &minval, &maxval)
      == SANE_STATUS_GOOD)
    media = val; /* 3: prints, 2: slides, 1: negatives */

  if (   (sanei_hp_device_probe (&compat, scsi) == SANE_STATUS_GOOD)
      && (compat & HP_COMPAT_PS)
      && (media != HP_MEDIA_PRINT))
    download_calib_file = 0;

  /* Recalibrate can not be probed, because it has no inquire ID. */
  /* And the desired ID of 10963 does not work. So we have to trust */
  /* the evaluated HP model number. */

  /* If we dont have an accessor, get one */
  if (!this->data_acsr)
  {
      if (!(this->data_acsr = sanei_hp_accessor_bool_new(data)))
          return SANE_STATUS_NO_MEM;
  }

  sanei_hp_accessor_setint(this->data_acsr, data, val);
  _set_size(this, data, sizeof(SANE_Bool));

  /* Try to download calibration map */
  if (download_calib_file)
    hp_download_calib_file ( scsi );

  return SANE_STATUS_GOOD;
}


static HpChoice
_make_choice_list (HpChoice choice, int minval, int maxval)
{
  static struct hp_choice_s bad = { 0, 0, 0, 0, 0 }; /* FIXME: hack */

  /* FIXME: Another memory leak */

  if (!choice->name)
      return 0;
  else if (hp_choice_isSupported(choice, minval, maxval))
    {
      _HpChoice new = sanei_hp_memdup(choice, sizeof(*new));
      if (!new)
	  return &bad;
      new->next = _make_choice_list(choice + 1, minval, maxval);
      return new;
    }
  else
      return _make_choice_list(choice + 1, minval, maxval);
}

static HpChoice
_make_probed_choice_list (HpScsi scsi, HpScl scl, HpChoice choice,
                          int minval, int maxval)
{
  static struct hp_choice_s bad = { 0, 0, 0, 0, 0 }; /* FIXME: hack */

  /* FIXME: Another memory leak */

  if (!choice->name)
      return 0;
  else if (hp_probed_choice_isSupported(scsi, scl, choice, minval, maxval))
    {
      _HpChoice new = sanei_hp_memdup(choice, sizeof(*new));
      if (!new)
	  return &bad;
      new->next = _make_probed_choice_list(scsi, scl, choice + 1, minval, maxval);
      return new;
    }
  else
      return _make_probed_choice_list(scsi, scl, choice + 1, minval, maxval);
}

static void
_set_stringlist (HpOption this, HpData data,  SANE_String_Const * strlist)
{
  _HpSaneOption optd = _hp_option_saneoption(this, data);
  optd->constraint.string_list = strlist;
  optd->constraint_type = SANE_CONSTRAINT_STRING_LIST;
}

static SANE_Status
_probe_choice (_HpOption this, HpScsi scsi, HpOptSet optset, HpData data)
{
  HpScl		scl	= this->descriptor->scl_command;
  int		minval, maxval, val;
  HpChoice	choices;
  const HpDeviceInfo *info;
  enum hp_device_compat_e compat;

  RETURN_IF_FAIL( sanei_hp_scl_inquire(scsi, scl, &val, &minval, &maxval) );
  DBG(3, "choice_option_probe: '%s': val, min, max = %d, %d, %d\n",
      this->descriptor->name, val, minval, maxval);

  info = sanei_hp_device_info_get ( sanei_hp_scsi_devicename  (scsi) );

  /* Datawidth needs a special handling. The choicelist consists of */
  /* values of bits per sample. But the minval/maxval uses bits per pixel */
  if ( scl == SCL_DATA_WIDTH )
  {
    enum hp_scanmode_e scanmode = sanei_hp_optset_scanmode (optset, data);

    /* The data width inquiries seem not to work properly on PhotoSmart */
    /* Sometimes they report just 24 bits, but support 30 bits too. */
    /* Sometimes they report min/max to be 24/8. Assume they all support */
    /* at least 10 bits per channel for RGB. Grayscale is only supported */
    /* with 8 bits. */
    if (   (sanei_hp_device_probe (&compat, scsi) == SANE_STATUS_GOOD)
        && (compat & HP_COMPAT_PS))
    {
      if (scanmode == HP_SCANMODE_GRAYSCALE)
      {
        minval = 8; if (maxval < 8) maxval = 8;
      }
      else if (scanmode == HP_SCANMODE_COLOR)
      {
        minval = 24; if (maxval < 30) maxval = 30;
      }
      DBG(1, "choice_option_probe: set max. datawidth to %d for photosmart\n",
          maxval);
    }

    if ( scanmode ==  HP_SCANMODE_COLOR )
    {
      minval /= 3; if ( minval <= 0) minval = 1;
      maxval /= 3; if ( maxval <= 0) maxval = 1;
      val /= 3; if (val <= 0) val = 1;
    }

#if 0
    /* The OfficeJets claim to support >8 bits per color, but it may not
     * work on some models.  This code (if not commented out) disables it. */
    if ((sanei_hp_device_probe (&compat, scsi) == SANE_STATUS_GOOD) &&
        (compat & HP_COMPAT_OJ_1150C)) {
          if (maxval>8) maxval=8;
    }
#endif
  }

  choices = _make_choice_list(this->descriptor->choices, minval, maxval);
  if (choices && !choices->name) /* FIXME: hack */
      return SANE_STATUS_NO_MEM;
  if (!choices)
      return SANE_STATUS_UNSUPPORTED;

  /* If no accessor, create one here. */
#ifdef HP_ALLOC_CHOICEACC_ONCE
  if (!(this->data_acsr))
#endif
      this->data_acsr = sanei_hp_accessor_choice_new(data, choices,
                          this->descriptor->may_change);

  if (!(this->data_acsr))
      return SANE_STATUS_NO_MEM;
  sanei_hp_accessor_setint(this->data_acsr, data, val);

  _set_stringlist(this, data,
       sanei_hp_accessor_choice_strlist((HpAccessorChoice)this->data_acsr,
                  0, 0, info));
  _set_size(this, data,
      sanei_hp_accessor_choice_maxsize((HpAccessorChoice)this->data_acsr));
  return SANE_STATUS_GOOD;
}

static SANE_Status
_probe_each_choice (_HpOption this, HpScsi scsi, HpOptSet UNUSEDARG optset,
                    HpData data)
{
  HpScl		scl	= this->descriptor->scl_command;
  int		minval, maxval, val;
  HpChoice	choices;
  const HpDeviceInfo *info;

  RETURN_IF_FAIL( sanei_hp_scl_inquire(scsi, scl, &val, &minval, &maxval) );
  DBG(3, "choice_option_probe_each: '%s': val, min, max = %d, %d, %d\n",
      this->descriptor->name, val, minval, maxval);
  DBG(3, "choice_option_probe_each: test all values for '%s' separately\n",
      this->descriptor->name);

  info = sanei_hp_device_info_get ( sanei_hp_scsi_devicename  (scsi) );
  choices = _make_probed_choice_list(scsi, scl, this->descriptor->choices,
                                     minval, maxval);

  DBG(3, "choice_option_probe_each: restore previous value %d for '%s'\n",
      val, this->descriptor->name);
                                    /* Restore current value */
  RETURN_IF_FAIL( sanei_hp_scl_set(scsi, scl, val) );

  if (choices && !choices->name) /* FIXME: hack */
      return SANE_STATUS_NO_MEM;
  if (!choices)
      return SANE_STATUS_UNSUPPORTED;

  /* If we dont have an accessor, get one */
#ifdef HP_ALLOC_CHOICEACC_ONCE
  if (!this->data_acsr)
#endif
  {
      if (!(this->data_acsr = sanei_hp_accessor_choice_new(data, choices,
                                this->descriptor->may_change )))
          return SANE_STATUS_NO_MEM;
  }

  sanei_hp_accessor_setint(this->data_acsr, data, val);

  _set_stringlist(this, data,
       sanei_hp_accessor_choice_strlist((HpAccessorChoice)this->data_acsr,
                  0, 0, info));
  _set_size(this, data,
       sanei_hp_accessor_choice_maxsize((HpAccessorChoice)this->data_acsr));
  return SANE_STATUS_GOOD;
}

/* pseudo probe for exposure times in Photosmart */
static SANE_Status
_probe_ps_exposure_time (_HpOption this, HpScsi scsi, HpOptSet UNUSEDARG optset,
                         HpData data)
{
    int           minval = 0, maxval = 9, val = 0;
    HpChoice      choices;
    const HpDeviceInfo *info;

    choices = _make_choice_list(this->descriptor->choices, minval, maxval);
    if (choices && !choices->name) /* FIXME: hack */
       return SANE_STATUS_NO_MEM;

    info = sanei_hp_device_info_get ( sanei_hp_scsi_devicename  (scsi) );

    /* If we dont have an accessor, get one */
#ifdef HP_ALLOC_CHOICEACC_ONCE
    if (!this->data_acsr)
#endif
    {
      if (!(this->data_acsr = sanei_hp_accessor_choice_new(data, choices,
                                  this->descriptor->may_change )))
         return SANE_STATUS_NO_MEM;
    }

    sanei_hp_accessor_setint(this->data_acsr, data, val);

    _set_stringlist(this, data,
           sanei_hp_accessor_choice_strlist((HpAccessorChoice)this->data_acsr,
               0, 0, info));
    _set_size(this, data,
           sanei_hp_accessor_choice_maxsize((HpAccessorChoice)this->data_acsr));
    return SANE_STATUS_GOOD;
}

/* probe scan type (normal, adf, xpa) */
static SANE_Status
_probe_scan_type (_HpOption this, HpScsi scsi, HpOptSet UNUSEDARG optset,
                  HpData data)
{
  int           val;
  int           numchoices = 0;
  HpChoice      choices;
  SANE_Status   status;
  const HpDeviceInfo *info;
  struct hp_choice_s scan_types[4];
  struct hp_choice_s nch = { 0, 0, 0, 0, 0 };
  enum hp_device_compat_e compat;

  /* We always have normal scan mode */
  scan_types[numchoices++] = this->descriptor->choices[0];

  if ( sanei_hp_device_probe (&compat, scsi) != SANE_STATUS_GOOD )
    compat = 0;

  /* Inquire ADF Capability. PhotoSmart scanner reports ADF capability, */
  /* but it makes no sense. */
  if ((compat & HP_COMPAT_PS) == 0)
  {
    status = sanei_hp_scl_inquire(scsi, SCL_ADF_CAPABILITY, &val, 0, 0);
    if ( (status == SANE_STATUS_GOOD) && (val == 1) )
    {
      scan_types[numchoices++] = this->descriptor->choices[1];
    }
  }

  /* Inquire XPA capability is supported only by IIcx and 6100c/4c/3c. */
  /* But more devices support XPA scan window. So dont inquire XPA cap. */
  if ( compat & (  HP_COMPAT_2CX | HP_COMPAT_4C | HP_COMPAT_4P
                 | HP_COMPAT_5P | HP_COMPAT_5100C | HP_COMPAT_6200C) &&
       !(compat&HP_COMPAT_OJ_1150C) )
  {
    scan_types[numchoices++] = this->descriptor->choices[2];
  }

  /* Only normal scan type available ? No need to display choice */
  if (numchoices <= 1) return SANE_STATUS_UNSUPPORTED;

  scan_types[numchoices] = nch;
  val = 0;

  choices = _make_choice_list(scan_types, 0, numchoices);
  if (choices && !choices->name) /* FIXME: hack */
     return SANE_STATUS_NO_MEM;

  info = sanei_hp_device_info_get ( sanei_hp_scsi_devicename  (scsi) );

  /* If we dont have an accessor, get one */
#ifdef HP_ALLOC_CHOICEACC_ONCE
  if (!this->data_acsr)
#endif
  {
      if (!(this->data_acsr = sanei_hp_accessor_choice_new(data, choices,
                                this->descriptor->may_change )))
         return SANE_STATUS_NO_MEM;
  }

  sanei_hp_accessor_setint(this->data_acsr, data, val);

  _set_stringlist(this, data,
         sanei_hp_accessor_choice_strlist((HpAccessorChoice)this->data_acsr,
             0, 0, info));
  _set_size(this, data,
         sanei_hp_accessor_choice_maxsize((HpAccessorChoice)this->data_acsr));
  return SANE_STATUS_GOOD;
}

static SANE_Status
_probe_mirror_horiz (_HpOption this, HpScsi scsi, HpOptSet UNUSEDARG optset,
                     HpData data)
{
  HpScl		scl	= this->descriptor->scl_command;
  int		minval, maxval, val, sec_dir;
  HpChoice	choices;
  const HpDeviceInfo *info;

  RETURN_IF_FAIL( sanei_hp_scl_inquire(scsi, scl, &val, &minval, &maxval) );
  DBG(3, "probe_mirror_horiz: '%s': val, min, max = %d, %d, %d\n",
      this->descriptor->name, val, minval, maxval);

  /* Look if the device supports the (?) inquire secondary scan-direction */
  if ( sanei_hp_scl_inquire(scsi, SCL_SECONDARY_SCANDIR, &sec_dir, 0, 0)
         == SANE_STATUS_GOOD )
    minval = HP_MIRROR_HORIZ_CONDITIONAL;

  info = sanei_hp_device_info_get ( sanei_hp_scsi_devicename  (scsi) );
  choices = _make_choice_list(this->descriptor->choices, minval, maxval);
  if (choices && !choices->name) /* FIXME: hack */
      return SANE_STATUS_NO_MEM;
  if (!choices)
      return SANE_STATUS_UNSUPPORTED;

  /* If we dont have an accessor, get one */
#ifdef HP_ALLOC_CHOICEACC_ONCE
  if (!this->data_acsr)
#endif
  {
      if (!(this->data_acsr = sanei_hp_accessor_choice_new(data, choices,
                                this->descriptor->may_change )))
          return SANE_STATUS_NO_MEM;
  }

  sanei_hp_accessor_setint(this->data_acsr, data, val);

  _set_stringlist(this, data,
       sanei_hp_accessor_choice_strlist((HpAccessorChoice)this->data_acsr,
                  0, 0, info));
  _set_size(this, data,
       sanei_hp_accessor_choice_maxsize((HpAccessorChoice)this->data_acsr));
  return SANE_STATUS_GOOD;
}

static SANE_Status
_probe_mirror_vert (_HpOption this, HpScsi scsi, HpOptSet UNUSEDARG optset,
                    HpData data)
{
  int		minval = HP_MIRROR_VERT_OFF,
                maxval = HP_MIRROR_VERT_ON,
                val = HP_MIRROR_VERT_OFF;
  int           sec_dir;
  HpChoice	choices;
  const HpDeviceInfo *info;

  info = sanei_hp_device_info_get ( sanei_hp_scsi_devicename  (scsi) );

  /* Look if the device supports the (?) inquire secondary scan-direction */
  if ( sanei_hp_scl_inquire(scsi, SCL_SECONDARY_SCANDIR, &sec_dir, 0, 0)
         == SANE_STATUS_GOOD )
    maxval = HP_MIRROR_VERT_CONDITIONAL;

  choices = _make_choice_list(this->descriptor->choices, minval, maxval);
  if (choices && !choices->name) /* FIXME: hack */
      return SANE_STATUS_NO_MEM;
  if (!choices)
      return SANE_STATUS_UNSUPPORTED;

  /* If we dont have an accessor, get one */
#ifdef HP_ALLOC_CHOICEACC_ONCE
  if (!this->data_acsr)
#endif
  {
      if (!(this->data_acsr = sanei_hp_accessor_choice_new(data, choices,
                                this->descriptor->may_change )))
          return SANE_STATUS_NO_MEM;
  }

  sanei_hp_accessor_setint(this->data_acsr, data, val);

  _set_stringlist(this, data,
       sanei_hp_accessor_choice_strlist((HpAccessorChoice)this->data_acsr,
                  0, 0, info));
  _set_size(this, data,
       sanei_hp_accessor_choice_maxsize((HpAccessorChoice)this->data_acsr));
  return SANE_STATUS_GOOD;
}


static SANE_Status _probe_front_button(_HpOption this, HpScsi scsi,
                                      HpOptSet UNUSEDARG optset, HpData data)
{
  int val = 0;

  if ( sanei_hp_scl_inquire(scsi, SCL_FRONT_BUTTON, &val, 0, 0)
        != SANE_STATUS_GOOD )
    return SANE_STATUS_UNSUPPORTED;

  _set_size(this, data, sizeof(SANE_Bool));

  /* If we dont have an accessor, get one */
  if (!this->data_acsr)
  {
      if ( !(this->data_acsr = sanei_hp_accessor_bool_new(data)) )
          return SANE_STATUS_NO_MEM;
  }

  sanei_hp_accessor_setint(this->data_acsr, data, 0);

  return SANE_STATUS_GOOD;
}


static SANE_Status
_probe_geometry (_HpOption this, HpScsi scsi, HpOptSet optset, HpData data)
{
  HpScl		scl	= this->descriptor->scl_command;
  hp_bool_t	is_tl	= 0;
  hp_bool_t     active_xpa = sanei_hp_is_active_xpa ( scsi );
  int		minval, maxval;
  SANE_Fixed	fval;

  /* There might have been a reason for inquiring the extent */
  /* by using the maxval of the position. But this does not work */
  /* when scanning from ADF. The Y-pos is then inquired with -1..0. */
  /* First try to get the values with SCL_X/Y_POS. If this is not ok, */
  /* use SCL_X/Y_EXTENT */
  if (scl == SCL_X_EXTENT)
  {
    scl = SCL_X_POS;
  }
  else if (scl == SCL_Y_EXTENT)
  {
    scl = SCL_Y_POS;
  }
  else
      is_tl = 1;

  RETURN_IF_FAIL( sanei_hp_scl_inquire(scsi, scl, 0, &minval, &maxval) );
  if (minval >= maxval)
      return SANE_STATUS_INVAL;

  /* Bad maximum value for extent-inquiry ? */
  if ( (!is_tl) && (maxval <= 0) )
  {
    scl = (scl == SCL_X_POS) ? SCL_X_EXTENT : SCL_Y_EXTENT;
    RETURN_IF_FAIL( sanei_hp_scl_inquire(scsi, scl, 0, &minval, &maxval) );
    if (minval >= maxval)
        return SANE_STATUS_INVAL;
  }

  if ((scl == SCL_X_EXTENT) || (scl == SCL_Y_EXTENT))
  {
     /* Max. extent is larger than max. position. Reduce extent */
     maxval--;
     DBG(3, "probe_geometry: Inquiry by extent. Reduced maxval to %lu\n",
         (unsigned long)maxval);
  }

  /* Need a new accessor ? */
  if (!this->data_acsr)
  {
    if (!(this->data_acsr = sanei_hp_accessor_fixed_new(data)))
        return SANE_STATUS_NO_MEM;
  }

  /* The active xpa is only 5x5 inches */
  if (   (!is_tl) && active_xpa
      && (sanei_hp_optset_scan_type (optset, data) == SCL_XPA_SCAN) )
  {
    DBG(3,"Set maxval to 1500 because of active XPA\n");
    maxval = 1500;
  }

  fval = is_tl ? SANE_FIX(0.0) : maxval * SANE_FIX(MM_PER_DEVPIX);
  RETURN_IF_FAIL( sanei_hp_accessor_set(this->data_acsr, data, &fval) );

  _set_size(this, data, sizeof(SANE_Fixed));
  return _set_range(this, data,
		    minval * SANE_FIX(MM_PER_DEVPIX),
		    1,
		    maxval * SANE_FIX(MM_PER_DEVPIX));
}

static SANE_Status
_probe_download_type (HpScl scl, HpScsi scsi)
{
  SANE_Status status;

  sanei_hp_scl_clearErrors (scsi);
  sanei_hp_scl_set (scsi, SCL_DOWNLOAD_TYPE, SCL_INQ_ID(scl));

  status = sanei_hp_scl_errcheck (scsi);

  DBG(3, "probe_download_type: Download type %d %ssupported\n", SCL_INQ_ID(scl),
      (status == SANE_STATUS_GOOD) ? "" : "not ");

  return status;
}

static SANE_Status
_probe_custom_gamma (_HpOption this, HpScsi scsi, HpOptSet UNUSEDARG optset,
                     HpData data)
{
 HpScl       scl = this->descriptor->scl_command;
 HpScl       scl_tonemap = SCL_8x8TONE_MAP;
 SANE_Status status;
 hp_bool_t   simulate;
 int         val = 0, minval, maxval;
 int         id = SCL_INQ_ID(scl_tonemap);

  /* Check if download type supported */
  status = sanei_hp_device_support_get ( sanei_hp_scsi_devicename  (scsi),
                SCL_DOWNLOAD_TYPE, &minval, &maxval);

  simulate = (status != SANE_STATUS_GOOD) || (id < minval) || (id > maxval);

  if (simulate)
  {
    DBG(3, "probe_custom_gamma: Download type 2 not supported. Simulate\n");
  }
  else
  {
    RETURN_IF_FAIL( sanei_hp_scl_inquire(scsi, scl, &val, 0, 0) );
  }

  /* If we dont have an accessor, get one */
  if (!this->data_acsr)
  {
      if (!(this->data_acsr = sanei_hp_accessor_bool_new(data)))
          return SANE_STATUS_NO_MEM;
  }

  sanei_hp_accessor_setint(this->data_acsr, data, val);
  _set_size(this, data, sizeof(SANE_Bool));
  return SANE_STATUS_GOOD;
}

static SANE_Status
_probe_vector (_HpOption this, HpScsi scsi, HpOptSet optset, HpData data)
{
  static struct vector_type_s {
      HpScl	scl;
      unsigned	length, depth;
      HpAccessor (*creator)(HpData data, unsigned length, unsigned depth);
  }	types[] = {
      { SCL_8x8TONE_MAP,    256,      8, sanei_hp_accessor_gamma_vector_new },
#ifdef ENABLE_7x12_TONEMAPS
      { SCL_BW7x12TONE_MAP, 129,     12, sanei_hp_accessor_gamma_vector_new },
      { SCL_7x12TONE_MAP,   3 * 129, 12, sanei_hp_accessor_gamma_vector_new },
#endif
#ifdef ENABLE_16x16_DITHERS
      { SCL_BW16x16DITHER,  256,      8, sanei_hp_accessor_vector_new },
#endif
      { SCL_BW8x8DITHER,    64,       8, sanei_hp_accessor_vector_new },

      { SCL_8x9MATRIX_COEFF,  9,      8, sanei_hp_accessor_matrix_vector_new },
#ifdef ENABLE_10BIT_MATRIXES
      { SCL_10x9MATRIX_COEFF, 9,     10, sanei_hp_accessor_matrix_vector_new },
      { SCL_10x3MATRIX_COEFF, 3,     10, sanei_hp_accessor_matrix_vector_new },
#endif
      { 0, 0, 0, 0 }
  };
  static struct subvector_type_s {
      HpOptionDescriptor	desc;
      unsigned			nchan, chan;
      HpOptionDescriptor	super;
  }	subvec_types[] = {
#ifdef ENABLE_7x12_TONEMAPS
      { GAMMA_VECTOR_R, 3, 0, RGB_TONEMAP },
      { GAMMA_VECTOR_G, 3, 1, RGB_TONEMAP },
      { GAMMA_VECTOR_B, 3, 2, RGB_TONEMAP },
#endif
      { 0, 0, 0, 0 },
  };

  HpScl			scl	= this->descriptor->scl_command;
  HpAccessorVector	vec;

  if (scl)
    {
      struct vector_type_s *type;
      for (type = types; type->scl; type++)
	  if (type->scl == scl)
	      break;
      assert(type->scl);

      RETURN_IF_FAIL ( _probe_download_type (scl, scsi) );
      /* If we dont have an accessor, get one */
#ifdef HP_ALLOC_CHOICEACC_ONCE
      if (!this->data_acsr)
#endif
      {
          this->data_acsr = (*type->creator)(data, type->length, type->depth);
      }
    }
  else
    {
      struct subvector_type_s *type;
      HpOption	super;

      for (type = subvec_types; type->desc; type++)
	  if (type->desc == this->descriptor)
	      break;
      assert(type->desc);

      super = hp_optset_get(optset, type->super);
      assert(super);

      /* If we dont have an accessor, get one */
#ifdef HP_ALLOC_CHOICEACC_ONCE
      if (!this->data_acsr)
#endif
      {
          this->data_acsr = sanei_hp_accessor_subvector_new(
            (HpAccessorVector) super->data_acsr, type->nchan, type->chan);
      }
    }

  if (!this->data_acsr)
      return SANE_STATUS_NO_MEM;

  vec = (HpAccessorVector)this->data_acsr;

  _set_size(this, data, sizeof(SANE_Fixed)
            * sanei_hp_accessor_vector_length(vec));

  return _set_range(this, data,
		    sanei_hp_accessor_vector_minval(vec),
		    1,
		    sanei_hp_accessor_vector_maxval(vec));
}

static SANE_Status
_probe_gamma_vector (_HpOption this, HpScsi scsi, HpOptSet optset, HpData data)
{
  SANE_Fixed *	buf;
  int		i;
  size_t	size, length;

  RETURN_IF_FAIL( _probe_vector(this, scsi, optset, data) );

  /* Initialize to linear map */
  size = hp_option_saneoption(this, data)->size;
  if (!(buf = alloca(size)))
      return SANE_STATUS_NO_MEM;
  length = size / sizeof(SANE_Fixed);
  for (i = 0; i < (int)length; i++)
      buf[i] = (SANE_FIX(HP_VECTOR_SCALE* 1.0) * i + (length-1) / 2) / length;
  return sanei_hp_accessor_set(this->data_acsr, data, buf);
}


static SANE_Status
_probe_horiz_dither (_HpOption this, HpScsi scsi, HpOptSet optset, HpData data)
{
  int		dim	= 8;
  size_t	size;
  int		i, j;
  SANE_Fixed *	buf;

  if (this->descriptor->scl_command == SCL_BW16x16DITHER)
      dim = 16;

  RETURN_IF_FAIL( _probe_vector(this, scsi, optset, data) );

  /* Select vertical dither pattern, and upload it */
  RETURN_IF_FAIL( sanei_hp_scl_set(scsi, SCL_BW_DITHER, HP_DITHER_VERTICAL) );
  RETURN_IF_FAIL( hp_option_upload(this, scsi, optset, data) );

  /* Flip it to get a horizontal dither pattern */
  size = hp_option_saneoption(this, data)->size;
  assert(size == dim * dim * sizeof(SANE_Fixed));
  if (!(buf = alloca(size)))
      return SANE_STATUS_NO_MEM;

#define SWAP_FIXED(x,y) do { SANE_Fixed tmp = x; x = y; y = tmp; } while(0)
  RETURN_IF_FAIL( sanei_hp_accessor_get(this->data_acsr, data, buf) );
  for (i = 0; i < dim; i++) for (j = i + 1; j < dim; j++)
      SWAP_FIXED(buf[i * dim + j], buf[j * dim + i]);
  return sanei_hp_accessor_set(this->data_acsr, data, buf);
}

static SANE_Status
_probe_matrix (_HpOption this, HpScsi scsi, HpOptSet optset, HpData data)
{
  RETURN_IF_FAIL( _probe_vector(this, scsi, optset, data) );

  /* Initial value: select RGB matrix, and upload it. */
  RETURN_IF_FAIL( sanei_hp_scl_set(scsi, SCL_MATRIX, HP_MATRIX_RGB) );
  return hp_option_upload(this, scsi, optset, data);
}

static SANE_Status
_probe_num_options (_HpOption this, HpScsi UNUSEDARG scsi,
                    HpOptSet UNUSEDARG optset, HpData data)
{
  /* If we dont have an accessor, get one */
  if (!this->data_acsr)
  {
      if (!(this->data_acsr = sanei_hp_accessor_int_new(data)))
          return SANE_STATUS_NO_MEM;
  }
  _set_size(this, data, sizeof(SANE_Int));
  return SANE_STATUS_GOOD;
}

static SANE_Status
_probe_devpix (_HpOption this, HpScsi scsi, HpOptSet UNUSEDARG optset,
               HpData data)
{
  HpScl	scl	= this->descriptor->scl_command;
  int	resolution;

  if (FAILED( sanei_hp_scl_inquire(scsi, scl, &resolution, 0, 0) ))
    {
      DBG(1, "probe_devpix: inquiry failed, assume 300 ppi\n");
      resolution = 300;
    }

  if (!this->data_acsr)
  {
    if (!(this->data_acsr = sanei_hp_accessor_int_new(data)))
        return SANE_STATUS_NO_MEM;
  }

  sanei_hp_accessor_setint(this->data_acsr, data, resolution);
  _set_size(this, data, sizeof(SANE_Int));
  return SANE_STATUS_GOOD;
}


/*
 * Simulate functions
 */
static SANE_Status
_simulate_brightness (HpOption this, HpData data, HpScsi scsi)
{
 int k, val, newval;
 unsigned char *brightness_map;
 HpDeviceInfo *info;

 info = sanei_hp_device_info_get ( sanei_hp_scsi_devicename (scsi) );
 assert (info);

 val = sanei_hp_accessor_getint(this->data_acsr, data);

 DBG(3, "simulate_brightness: value = %d\n", val);

 /* Update brightness map in info structure */
 brightness_map = &(info->simulate.brightness_map[0]);
 val *= 2;  /* A value of 127 should give a totally white image */
 for (k = 0; k < 256; k++)
 {
   newval = k + val;
   if (newval < 0) newval = 0; else if (newval > 255) newval = 255;
   brightness_map[k] = (unsigned char)newval;
 }
 return SANE_STATUS_GOOD;
}

static int
hp_contrast (int x, int g)

{int y = 0;

 if (g < -127) g = -127; else if (g > 127) g = 127;
 if (x < 0) x = 0; else if (x > 255) x = 255;

 if (g == 0)
 {
   y = x;
 }
 else if (g < 0)
 {
   g = -g;
   y = x * (255 - 2*g);
   y = y/255 + g;
 }
 else
 {
   if (x <= g) y = 0;
   else if (x >= 255-g) y = 255;
   else
   {
     y = (x - g)*255;
     y /= (255 - 2*g);
   }
 }

 return y;
}

static SANE_Status
_simulate_contrast (HpOption this, HpData data, HpScsi scsi)
{
 int k, val, newval;
 unsigned char *contrast_map;
 HpDeviceInfo *info;

 info = sanei_hp_device_info_get ( sanei_hp_scsi_devicename (scsi) );
 assert (info);

 val = sanei_hp_accessor_getint(this->data_acsr, data);

 DBG(3, "simulate_contrast: value = %d\n", val);

 /* Update contrast map in info structure */
 contrast_map = &(info->simulate.contrast_map[0]);

 for (k = 0; k < 256; k++)
 {
   newval = hp_contrast (k, val);
   if (newval < 0) newval = 0; else if (newval > 255) newval = 255;
   contrast_map[k] = (unsigned char)newval;
 }
 return SANE_STATUS_GOOD;
}

/*
 * Option download functions
 */
static SANE_Status
_program_generic (HpOption this, HpScsi scsi, HpOptSet optset, HpData data)
{
  return hp_option_download(this, data, optset, scsi);
}

static SANE_Status
_program_geometry (HpOption this, HpScsi scsi, HpOptSet optset, HpData data)
{
/* #define HP_LIMIT_ADF_WINDOW */
#ifndef HP_LIMIT_ADF_WINDOW

 return hp_option_download(this, data, optset, scsi);

#else

 HpScl scl = this->descriptor->scl_command;
 int value;
 SANE_Status Status;

 if (sanei_hp_optset_scan_type (optset, data) != SCL_ADF_SCAN)
   return hp_option_download(this, data, optset, scsi);

 /* ADF may crash when scanning only a window ? */
 if ( (scl == SCL_X_POS) || (scl == SCL_Y_POS) )
 {
   value = 0;
   DBG(3,"program_geometry: set %c-pos to %d\n",
       (scl == SCL_X_POS) ? 'x' : 'y', value);
 }
 else if ( scl == SCL_X_EXTENT )
 {
   value = 2550;
   DBG(3,"program_geometry: set x-extent to %d\n", value);
 }
 else
 {
   value = 4200;
   DBG(3,"program_geometry: set y-extent to %d\n", value);
 }

 Status = sanei_hp_scl_set(scsi, scl, value);
 return Status;

#endif
}

static SANE_Status
_program_data_width (HpOption this, HpScsi scsi, HpOptSet optset, HpData data)
{
  HpScl scl = this->descriptor->scl_command;
  int value = sanei_hp_accessor_getint(this->data_acsr, data);
  SANE_Status status;

  if ( sanei_hp_optset_scanmode (optset, data) == HP_SCANMODE_COLOR )
  {
    value *= 3;
    if (value < 24)
    {
      DBG(3,"program_data_width: map datawith from %d to 24\n", (int)value);
      value = 24;
    }
  }
  status = sanei_hp_scl_set(scsi, scl, value);
  return status;
}

static SANE_Status
_program_generic_simulate (HpOption this, HpScsi scsi, HpOptSet optset,
                           HpData data)
{
 HpScl scl = this->descriptor->scl_command;
 const char *devname = sanei_hp_scsi_devicename (scsi);
 int simulate;

  /* Check if command is supported */
  simulate = (   sanei_hp_device_support_get (devname, scl, 0, 0)
              != SANE_STATUS_GOOD );

  /* Save simulate flag */
  sanei_hp_device_simulate_set (devname, scl, simulate);

  if ( !simulate )  /* Let the device do it */
    return hp_option_download(this, data, optset, scsi);

  DBG(3, "program_generic: %lu not programmed. Will be simulated\n",
      (unsigned long)(SCL_INQ_ID(scl)));

  switch (scl)
  {
    case SCL_BRIGHTNESS:
      _simulate_brightness (this, data, scsi);
      break;

    case SCL_CONTRAST:
      _simulate_contrast (this, data,scsi);
      break;

    default:
      DBG(1, "program_generic: No simulation for %lu\n",
          (unsigned long)(SCL_INQ_ID(scl)));
      break;
  }
  return SANE_STATUS_GOOD;
}

static SANE_Status
_simulate_custom_gamma (HpOption gvector, HpScsi scsi, HpData data)
{
 size_t size = sanei_hp_accessor_size(gvector->data_acsr);
 const unsigned char *vector_data =
     (const unsigned char *)sanei_hp_accessor_data(gvector->data_acsr, data);
 HpDeviceInfo *info;
 int k, newval;

  DBG(3,"program_custom_gamma_simulate: save gamma map\n");
  if (size != 256)
  {
    DBG(1,"program_custom_gamma_simulate: size of vector is %d.\
 Should be 256.\n", (int)size);
    return SANE_STATUS_INVAL;
  }

  RETURN_IF_FAIL (sanei_hp_scl_set(scsi, SCL_TONE_MAP, 0));

  info = sanei_hp_device_info_get ( sanei_hp_scsi_devicename (scsi) );
  info->simulate.gamma_simulate = 1;

  for (k = 0; k < 256; k++)
  {
    newval = 255 - vector_data[255-k];
    if (newval < 0) newval = 0; else if (newval > 255) newval = 255;
    info->simulate.gamma_map[k] = newval;
  }

  return SANE_STATUS_GOOD;
}

static SANE_Status
_program_tonemap (HpOption this, HpScsi scsi, HpOptSet optset, HpData data)
{
  hp_bool_t	use_custom_map	= hp_option_getint(this, data);
  HpOption	gvector = 0;
  int		type = 0;

  if (!use_custom_map)
      return sanei_hp_scl_set(scsi, SCL_TONE_MAP, 0);

#ifdef ENABLE_7x12_TONEMAPS
  /* Try to find the appropriate 5P style tonemap. */
  if (sanei_hp_optset_scanmode(optset, data) == HP_SCANMODE_COLOR)
    {
      type = -1;
      gvector = hp_optset_get(optset, RGB_TONEMAP);
    }
  else
    {
      type = -2;
      gvector = hp_optset_get(optset, GAMMA_VECTOR_7x12);
    }
#endif

  /* If that failed, just use 8x8 tonemap */
  if (!gvector)
  {
    HpScl       scl_tonemap = SCL_8x8TONE_MAP;
    hp_bool_t   simulate;
    int         id = SCL_INQ_ID(scl_tonemap);
    int         minval, maxval;
    SANE_Status status;

      type = -1;
      gvector = hp_optset_get(optset, GAMMA_VECTOR_8x8);

      /* Check if download type supported */
      status = sanei_hp_device_support_get ( sanei_hp_scsi_devicename  (scsi),
                    SCL_DOWNLOAD_TYPE, &minval, &maxval);

      simulate =    (status != SANE_STATUS_GOOD) || (id < minval)
                 || (id > maxval);
      if (simulate)
        return _simulate_custom_gamma (gvector, scsi, data);
  }

  assert(gvector != 0);

  RETURN_IF_FAIL( sanei_hp_scl_set(scsi, SCL_TONE_MAP, type) );
  return hp_option_download(gvector, data, optset, scsi);
}


static SANE_Status
_program_dither (HpOption this, HpScsi scsi, HpOptSet optset, HpData data)
{
  enum hp_dither_type_e type	= hp_option_getint(this, data);
  HpOption	dither;

  switch (type) {
  case HP_DITHER_CUSTOM:
      dither = hp_optset_getByName(optset, SANE_NAME_HALFTONE_PATTERN);
      assert(dither != 0);
      break;
  case HP_DITHER_HORIZONTAL:
      dither = hp_optset_getByName(optset, HP_NAME_HORIZONTAL_DITHER);
      type = HP_DITHER_CUSTOM;
      assert(dither != 0);
      break;
  default:
      dither = 0;
      break;
  }

  RETURN_IF_FAIL( sanei_hp_scl_set(scsi, SCL_BW_DITHER, type) );
  if (!dither)
      return SANE_STATUS_GOOD;
  return hp_option_download(dither, data, optset, scsi);
}

#ifdef FAKE_COLORSEP_MATRIXES
static HpOption
_get_sepmatrix (HpOptSet optset, HpData data, enum hp_matrix_type_e type)
{
  SANE_Fixed	buf[9];
  HpOption	matrix	= hp_optset_get(optset, SEPMATRIX);

  memset(buf, 0, sizeof(buf));
  if (type == HP_MATRIX_RED)
      buf[1] = SANE_FIX(1.0);
  else if (type == HP_MATRIX_GREEN)
      buf[4] = SANE_FIX(1.0);
  else if (type == HP_MATRIX_BLUE)
      buf[7] = SANE_FIX(1.0);
  else
    {
      assert(!"Bad colorsep type");
      return 0;
    }
  sanei_hp_accessor_set(matrix->data_acsr, data, buf);
  return matrix;
}
#endif

static SANE_Status
_program_matrix (HpOption this, HpScsi scsi, HpOptSet optset, HpData data)
{
  enum hp_matrix_type_e type	= hp_option_getint(this, data);
  HpOption		matrix	= 0;

  if (type == HP_MATRIX_AUTO)
      return SANE_STATUS_GOOD;	/* Default to matrix set by mode */

  /* Download custom matrix, if we need it. */
  if (type == HP_MATRIX_CUSTOM)
    {
      matrix = hp_optset_getByName(optset, SANE_NAME_MATRIX_RGB);
      assert(matrix);
    }
#ifdef FAKE_COLORSEP_MATRIXES
  else if (type == HP_MATRIX_RED
	   || type == HP_MATRIX_BLUE
	   || type == HP_MATRIX_GREEN)
    {
      matrix = _get_sepmatrix(optset, data, type);
      type = HP_MATRIX_CUSTOM;
      assert(matrix);
    }
#else
  else if (type == HP_MATRIX_GREEN)
      type = HP_MATRIX_PASS;
#endif


  RETURN_IF_FAIL( sanei_hp_scl_set(scsi, SCL_MATRIX, type) );
  if (matrix)
      RETURN_IF_FAIL( hp_option_download(matrix, data, optset, scsi) );

  return SANE_STATUS_GOOD;
}

static SANE_Status
_program_resolution (HpOption this, HpScsi scsi, HpOptSet UNUSEDARG optset,
                     HpData data)
{

  int xresolution = hp_option_getint(this, data);
  int yresolution = xresolution;
  int xscale = 100, yscale = 100;

#ifdef FIX_PHOTOSMART
  int minval, maxval, media;
  enum hp_device_compat_e compat;

  /* HP Photosmart scanner has problems with scanning slides/negatives */
  /* at arbitrary resolutions. The following tests did not work: */
  /* xres = yres = next lower multiple of 300, xscale = yscale > 100: */
  /* xres = yres = next higher multiple of 300, xscale = yscale < 100: */
  /* xres = next lower multiple of 300, xscale > 100 */
  /* xres = next higher multiple of 300, xscale < 100 */
  /* yres = next lower multiple of 300, yscale > 100 */
  /* yres = next higher multiple of 300, yscale < 100 */
  /* The image extent was ok, but the content was streched in y-direction */

  if (xresolution > 300)
  {
    if (   (sanei_hp_device_probe (&compat, scsi) == SANE_STATUS_GOOD)
        && (compat & HP_COMPAT_PS)
        && (sanei_hp_scl_inquire(scsi, SCL_MEDIA, &media, &minval, &maxval)
              == SANE_STATUS_GOOD)
        && ((media == HP_MEDIA_SLIDE) || (media == HP_MEDIA_NEGATIVE)))
    {int next_resolution;
      next_resolution = (xresolution % 300) * 300;
      if (next_resolution < 300) next_resolution = 300;
      yresolution = next_resolution;
      yscale = (int)(100.0 * xresolution / yresolution);
    }
  }
#endif

  RETURN_IF_FAIL( sanei_hp_scl_set(scsi, SCL_X_SCALE, xscale) );
  RETURN_IF_FAIL( sanei_hp_scl_set(scsi, SCL_Y_SCALE, yscale) );
  RETURN_IF_FAIL( sanei_hp_scl_set(scsi, SCL_X_RESOLUTION, xresolution) );
  RETURN_IF_FAIL( sanei_hp_scl_set(scsi, SCL_Y_RESOLUTION, yresolution) );

  return SANE_STATUS_GOOD;
}

static char *
get_home_dir (void)
{
#ifdef SANE_HOME_HP

 return getenv (SANE_HOME_HP);

#else

  struct passwd *pw;

  pw = getpwuid (getuid ());  /* Look if we can find our home directory */
  return pw ? pw->pw_dir : NULL;

#endif
}

static char *
get_calib_filename (HpScsi scsi)
{
  char *homedir;
  char *calib_filename, *cf;
  const char *devname = sanei_hp_scsi_devicename (scsi);
  int name_len;

  homedir = get_home_dir (); /* Look if we can find our home directory */
  if (!homedir) return NULL;

  name_len = strlen (homedir) + 33;
  if ( devname ) name_len += strlen (devname);
  calib_filename = sanei_hp_allocz (name_len);
  if (!calib_filename) return NULL;

  strcpy (calib_filename, homedir);
  strcat (calib_filename, "/.sane/calib-hp");
  if ( devname && devname[0] ) /* Replace '/' by "+-" */
    {
      cf = calib_filename + strlen (calib_filename);
      *(cf++) = ':';
      while (*devname)
      {
        if (*devname == '/') *(cf++) = '+', *(cf++) = '-';
        else *(cf++) = *devname;
        devname++;
      }
    }
  strcat (calib_filename, ".dat");

  return calib_filename;
}

static SANE_Status
read_calib_file (int *nbytes, char **calib_data, HpScsi scsi)
{
  SANE_Status status = SANE_STATUS_GOOD;
  char *calib_filename;
  FILE *calib_file;
  int err, c1, c2, c3, c4;

  *nbytes = 0;
  *calib_data = NULL;

  calib_filename = get_calib_filename ( scsi );
  if (!calib_filename) return SANE_STATUS_NO_MEM;

  calib_file = fopen (calib_filename, "rb");
  if ( calib_file )
    {
      err = ((c1 = getc (calib_file)) == EOF);
      err |= ((c2 = getc (calib_file)) == EOF);
      err |= ((c3 = getc (calib_file)) == EOF);
      err |= ((c4 = getc (calib_file)) == EOF);
      *nbytes = (c1 << 24) | (c2 << 16) | (c3 << 8) | c4;
      if ( err )
        {
          DBG(1, "read_calib_file: Error reading calibration data size\n");
          status = SANE_STATUS_EOF;
        }
      else
        {
          *calib_data = sanei_hp_alloc ( *nbytes );
          if ( !*calib_data )
            {
              status = SANE_STATUS_NO_MEM;
            }
          else
            {
              err |= ((int)fread (*calib_data,1,*nbytes,calib_file) != *nbytes);
              if ( err )
                {
                  DBG(1, "read_calib_file: Error reading calibration data\n");
                  sanei_hp_free ( *calib_data );
                  status = SANE_STATUS_EOF;
                }
            }
        }
      fclose ( calib_file );
    }
  else
    {
      DBG(1, "read_calib_file: Error opening calibration file %s\
 for reading\n", calib_filename);
      status = SANE_STATUS_EOF;
    }

  sanei_hp_free (calib_filename);

  return ( status );
}

static SANE_Status
write_calib_file (int nbytes, char *data, HpScsi scsi)
{
  SANE_Status status = SANE_STATUS_GOOD;
  char *calib_filename;
  int err;
  FILE *calib_file;

  calib_filename = get_calib_filename ( scsi );
  if (!calib_filename) return SANE_STATUS_NO_MEM;

  calib_file = fopen (calib_filename, "wb");
  if ( calib_file )
    {
      err = (putc ((nbytes >> 24) & 0xff, calib_file) == EOF);
      err |= (putc ((nbytes >> 16) & 0xff, calib_file) == EOF);
      err |= (putc ((nbytes >> 8) & 0xff, calib_file) == EOF);
      err |= (putc (nbytes & 0xff, calib_file) == EOF);
      err |= ((int)fwrite (data, 1, nbytes, calib_file) != nbytes);
      fclose (calib_file);
      if ( err )
        {
          DBG(1, "write_calib_file: Error writing calibration data\n");
          unlink (calib_filename);
          status = SANE_STATUS_EOF;
        }
    }
  else
    {
      DBG(1, "write_calib_file: Error opening calibration file %s\
 for writing\n", calib_filename);
      status = SANE_STATUS_EOF;
    }

  sanei_hp_free (calib_filename);
  return (status);
}

static SANE_Status
_program_media (HpOption this, HpScsi scsi, HpOptSet optset, HpData data)
{
  int      req_media, minval, maxval, current_media;
  HpScl scl = this->descriptor->scl_command;

  req_media = sanei_hp_accessor_getint(this->data_acsr, data);

  RETURN_IF_FAIL( sanei_hp_scl_inquire(scsi, scl, &current_media,
                                       &minval, &maxval) );
  if (current_media == req_media)
    return SANE_STATUS_GOOD;

  /* Unload scanner */
  RETURN_IF_FAIL( sanei_hp_scl_set(scsi, SCL_UNLOAD, 0) );

  /* Select new media */
  RETURN_IF_FAIL( hp_option_download(this, data, optset, scsi));

  /* Update support list */
  sanei_hp_device_support_probe (scsi);

  if (req_media == HP_MEDIA_PRINT)
    hp_download_calib_file (scsi);

  return SANE_STATUS_GOOD;
}

static SANE_Status
_program_unload_after_scan (HpOption this, HpScsi scsi,
                            HpOptSet UNUSEDARG optset, HpData data)
{ HpDeviceInfo *info;

  info = sanei_hp_device_info_get ( sanei_hp_scsi_devicename  (scsi) );
  assert (info);
  info->unload_after_scan = sanei_hp_accessor_getint(this->data_acsr, data);

  DBG(3,"program_unload_after_scan: flag = %lu\n",
      (unsigned long)info->unload_after_scan);

  return SANE_STATUS_GOOD;
}

static SANE_Status
_program_lamp_off (HpOption UNUSEDARG this, HpScsi scsi,
                   HpOptSet UNUSEDARG optset, HpData UNUSEDARG data)
{
  DBG(3,"program_lamp_off: shut off lamp\n");

  return sanei_hp_scl_set(scsi, SCL_LAMPTEST, 0);
}

static SANE_Status
_program_scan_type (HpOption this, HpScsi scsi, HpOptSet optset, HpData data)

{ int      req_scan_type;

  req_scan_type = sanei_hp_accessor_getint(this->data_acsr, data);

  if ( req_scan_type == HP_SCANTYPE_XPA )
  {
   enum hp_scanmode_e scan_mode = sanei_hp_optset_scanmode(optset, data);
   static unsigned char xpa_matrix_coeff[] = {
0x00,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x80
   };
   static unsigned char xpa_tone_map[] = {
0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0xfe,0x0f,
0xe1,0x0f,0xd3,0x0f,0xc4,0x0f,0xb5,0x0f,0xa6,0x0f,0x97,0x0f,0x88,0x0f,0x79,0x0f,
0x6a,0x0f,0x5b,0x0f,0x4b,0x0f,0x3c,0x0f,0x2c,0x0f,0x1d,0x0f,0x0d,0x0e,0xfe,0x0e,
0xee,0x0e,0xde,0x0e,0xce,0x0e,0xbe,0x0e,0xae,0x0e,0x9e,0x0e,0x8e,0x0e,0x7d,0x0e,
0x6d,0x0e,0x5c,0x0e,0x4c,0x0e,0x3b,0x0e,0x2a,0x0e,0x19,0x0e,0x08,0x0d,0xf7,0x0d,
0xe6,0x0d,0xd5,0x0d,0xc4,0x0d,0xb2,0x0d,0xa1,0x0d,0x8f,0x0d,0x7d,0x0d,0x6b,0x0d,
0x59,0x0d,0x47,0x0d,0x35,0x0d,0x22,0x0d,0x10,0x0c,0xfd,0x0c,0xeb,0x0c,0xd8,0x0c,
0xc5,0x0c,0xb2,0x0c,0x9e,0x0c,0x8b,0x0c,0x77,0x0c,0x64,0x0c,0x50,0x0c,0x3c,0x0c,
0x28,0x0c,0x14,0x0b,0xff,0x0b,0xeb,0x0b,0xd6,0x0b,0xc1,0x0b,0xac,0x0b,0x96,0x0b,
0x81,0x0b,0x6b,0x0b,0x55,0x0b,0x3f,0x0b,0x29,0x0b,0x12,0x0a,0xfc,0x0a,0xe5,0x0a,
0xce,0x0a,0xb6,0x0a,0x9e,0x0a,0x87,0x0a,0x6e,0x0a,0x56,0x0a,0x3d,0x0a,0x24,0x0a,
0x0b,0x09,0xf1,0x09,0xd8,0x09,0xbd,0x09,0xa3,0x09,0x88,0x09,0x6d,0x09,0x51,0x09,
0x35,0x09,0x19,0x08,0xfc,0x08,0xdf,0x08,0xc1,0x08,0xa3,0x08,0x84,0x08,0x65,0x08,
0x45,0x08,0x24,0x08,0x03,0x07,0xe1,0x07,0xbe,0x07,0x9b,0x07,0x78,0x07,0x53,0x07,
0x2d,0x07,0x07,0x06,0xdf,0x06,0xb7,0x06,0x8d,0x06,0x62,0x06,0x36,0x06,0x07,0x05,
0xd8,0x05,0xa6,0x05,0x72,0x05,0x3c,0x04,0xfc,0x04,0x7c,0x03,0xfc,0x03,0x7c,0x02,
0xfc,0x02,0x7c,0x01,0xfc,0x01,0x7c,0x00,0xfc,0x00,0x7c,0x00,0x00,0x0f,0xfe,0x0f,
0xe1,0x0f,0xd3,0x0f,0xc4,0x0f,0xb5,0x0f,0xa6,0x0f,0x97,0x0f,0x88,0x0f,0x79,0x0f,
0x6a,0x0f,0x5b,0x0f,0x4b,0x0f,0x3c,0x0f,0x2c,0x0f,0x1d,0x0f,0x0d,0x0e,0xfe,0x0e,
0xee,0x0e,0xde,0x0e,0xce,0x0e,0xbe,0x0e,0xae,0x0e,0x9e,0x0e,0x8e,0x0e,0x7d,0x0e,
0x6d,0x0e,0x5c,0x0e,0x4c,0x0e,0x3b,0x0e,0x2a,0x0e,0x19,0x0e,0x08,0x0d,0xf7,0x0d,
0xe6,0x0d,0xd5,0x0d,0xc4,0x0d,0xb2,0x0d,0xa1,0x0d,0x8f,0x0d,0x7d,0x0d,0x6b,0x0d,
0x59,0x0d,0x47,0x0d,0x35,0x0d,0x22,0x0d,0x10,0x0c,0xfd,0x0c,0xeb,0x0c,0xd8,0x0c,
0xc5,0x0c,0xb2,0x0c,0x9e,0x0c,0x8b,0x0c,0x77,0x0c,0x64,0x0c,0x50,0x0c,0x3c,0x0c,
0x28,0x0c,0x14,0x0b,0xff,0x0b,0xeb,0x0b,0xd6,0x0b,0xc1,0x0b,0xac,0x0b,0x96,0x0b,
0x81,0x0b,0x6b,0x0b,0x55,0x0b,0x3f,0x0b,0x29,0x0b,0x12,0x0a,0xfc,0x0a,0xe5,0x0a,
0xce,0x0a,0xb6,0x0a,0x9e,0x0a,0x87,0x0a,0x6e,0x0a,0x56,0x0a,0x3d,0x0a,0x24,0x0a,
0x0b,0x09,0xf1,0x09,0xd8,0x09,0xbd,0x09,0xa3,0x09,0x88,0x09,0x6d,0x09,0x51,0x09,
0x35,0x09,0x19,0x08,0xfc,0x08,0xdf,0x08,0xc1,0x08,0xa3,0x08,0x84,0x08,0x65,0x08,
0x45,0x08,0x24,0x08,0x03,0x07,0xe1,0x07,0xbe,0x07,0x9b,0x07,0x78,0x07,0x53,0x07,
0x2d,0x07,0x07,0x06,0xdf,0x06,0xb7,0x06,0x8d,0x06,0x62,0x06,0x36,0x06,0x07,0x05,
0xd8,0x05,0xa6,0x05,0x72,0x05,0x3c,0x04,0xfc,0x04,0x7c,0x03,0xfc,0x03,0x7c,0x02,
0xfc,0x02,0x7c,0x01,0xfc,0x01,0x7c,0x00,0xfc,0x00,0x7c,0x00,0x00,0x0f,0xfe,0x0f,
0xe1,0x0f,0xd3,0x0f,0xc4,0x0f,0xb5,0x0f,0xa6,0x0f,0x97,0x0f,0x88,0x0f,0x79,0x0f,
0x6a,0x0f,0x5b,0x0f,0x4b,0x0f,0x3c,0x0f,0x2c,0x0f,0x1d,0x0f,0x0d,0x0e,0xfe,0x0e,
0xee,0x0e,0xde,0x0e,0xce,0x0e,0xbe,0x0e,0xae,0x0e,0x9e,0x0e,0x8e,0x0e,0x7d,0x0e,
0x6d,0x0e,0x5c,0x0e,0x4c,0x0e,0x3b,0x0e,0x2a,0x0e,0x19,0x0e,0x08,0x0d,0xf7,0x0d,
0xe6,0x0d,0xd5,0x0d,0xc4,0x0d,0xb2,0x0d,0xa1,0x0d,0x8f,0x0d,0x7d,0x0d,0x6b,0x0d,
0x59,0x0d,0x47,0x0d,0x35,0x0d,0x22,0x0d,0x10,0x0c,0xfd,0x0c,0xeb,0x0c,0xd8,0x0c,
0xc5,0x0c,0xb2,0x0c,0x9e,0x0c,0x8b,0x0c,0x77,0x0c,0x64,0x0c,0x50,0x0c,0x3c,0x0c,
0x28,0x0c,0x14,0x0b,0xff,0x0b,0xeb,0x0b,0xd6,0x0b,0xc1,0x0b,0xac,0x0b,0x96,0x0b,
0x81,0x0b,0x6b,0x0b,0x55,0x0b,0x3f,0x0b,0x29,0x0b,0x12,0x0a,0xfc,0x0a,0xe5,0x0a,
0xce,0x0a,0xb6,0x0a,0x9e,0x0a,0x87,0x0a,0x6e,0x0a,0x56,0x0a,0x3d,0x0a,0x24,0x0a,
0x0b,0x09,0xf1,0x09,0xd8,0x09,0xbd,0x09,0xa3,0x09,0x88,0x09,0x6d,0x09,0x51,0x09,
0x35,0x09,0x19,0x08,0xfc,0x08,0xdf,0x08,0xc1,0x08,0xa3,0x08,0x84,0x08,0x65,0x08,
0x45,0x08,0x24,0x08,0x03,0x07,0xe1,0x07,0xbe,0x07,0x9b,0x07,0x78,0x07,0x53,0x07,
0x2d,0x07,0x07,0x06,0xdf,0x06,0xb7,0x06,0x8d,0x06,0x62,0x06,0x36,0x06,0x07,0x05,
0xd8,0x05,0xa6,0x05,0x72,0x05,0x3c,0x04,0xfc,0x04,0x7c,0x03,0xfc,0x03,0x7c,0x02,
0xfc,0x02,0x7c,0x01,0xfc,0x01,0x7c,0x00,0xfc,0x00,0x7c,0x00,0x00
     };

    sanei_hp_scl_set(scsi, SCL_RESERVED1, 0); /* dont know */
    sanei_hp_scl_set(scsi, SCL_10952, 0);     /* Calibration mode */

    if (   sanei_hp_is_active_xpa (scsi)
        && (   (scan_mode==HP_SCANMODE_COLOR)
            || (scan_mode==HP_SCANMODE_GRAYSCALE)) )
    {
      DBG (3,"program_scan_type: set tone map for active XPA\n");
      sanei_hp_scl_download (scsi, SCL_10x9MATRIX_COEFF, xpa_matrix_coeff,
                             sizeof (xpa_matrix_coeff));
      sanei_hp_scl_set(scsi, SCL_MATRIX, -1);     /* Set matrix coefficient */

      sanei_hp_scl_download (scsi, SCL_7x12TONE_MAP, xpa_tone_map,
                             sizeof (xpa_tone_map));
      sanei_hp_scl_set(scsi, SCL_TONE_MAP, -1); /* Select tone map */
    }
  }
  
  return SANE_STATUS_GOOD;
}

static SANE_Status
_program_change_doc (HpOption UNUSEDARG this, HpScsi scsi,
                     HpOptSet UNUSEDARG optset, HpData UNUSEDARG data)
{
  int istat;

  DBG(2, "program_change_doc: inquire ADF ready\n");

  RETURN_IF_FAIL( sanei_hp_scl_inquire(scsi, SCL_ADF_READY, &istat, 0, 0) );
  if ( istat != 1 )    /* ADF not ready */
  {
    DBG(2, "program_change_doc: ADF not ready\n");
    return SANE_STATUS_INVAL;
  }

  DBG(2, "program_change_doc: inquire paper in ADF\n");

  RETURN_IF_FAIL( sanei_hp_scl_inquire(scsi, SCL_ADF_BIN, &istat, 0, 0) );
  if ( istat == 0 )    /* Nothing in ADF BIN */
  {
    DBG(2, "program_change_doc: nothing in ADF BIN. Just Unload.\n");
    return sanei_hp_scl_set(scsi, SCL_UNLOAD, 0);
  }

  DBG(2, "program_change_doc: Clear errors and change document.\n");

  RETURN_IF_FAIL( sanei_hp_scl_clearErrors (scsi) );

  RETURN_IF_FAIL( sanei_hp_scl_set(scsi, SCL_CHANGE_DOC, 0) );

  return sanei_hp_scl_errcheck (scsi);
}

static SANE_Status
_program_unload (HpOption this, HpScsi scsi, HpOptSet optset, HpData data)
{
  hp_bool_t  adfscan = ( sanei_hp_optset_scan_type (optset, data)
                           == SCL_ADF_SCAN );

  /* If we have an ADF, try to see if it is ready to unload */
  if (adfscan)
  {int val;

   if ( sanei_hp_scl_inquire(scsi, SCL_ADF_RDY_UNLOAD, &val, 0, 0)
       == SANE_STATUS_GOOD )
   {
     DBG(3, "program_unload: ADF is%sready to unload\n", val ? " " : " not ");
   }
   else
   {
     DBG(3, "program_unload: Command 'Ready to unload' not supported\n");
   }
  }
  return hp_option_download(this, data, optset, scsi);
}

static SANE_Status
_program_calibrate (HpOption UNUSEDARG this, HpScsi scsi,
                    HpOptSet UNUSEDARG optset, HpData UNUSEDARG data)
{
  struct passwd *pw;
  SANE_Status status = SANE_STATUS_GOOD;
  size_t calib_size;
  char *calib_buf;

  RETURN_IF_FAIL ( sanei_hp_scl_calibrate(scsi) );   /* Start calibration */

  pw = getpwuid (getuid ());  /* Look if we can find our home directory */
  if (!pw) return SANE_STATUS_GOOD;

  DBG(3, "_program_calibrate: Read calibration data\n");

  RETURN_IF_FAIL ( sanei_hp_scl_upload_binary (scsi, SCL_CALIB_MAP,
                                               &calib_size, &calib_buf) );

  DBG(3, "_program_calibrate: Got %lu bytes of calibration data\n",
      (unsigned long) calib_size);

  write_calib_file (calib_size, calib_buf, scsi);

  sanei_hp_free (calib_buf);

  return (status);
}

/* The exposure time of the HP Photosmart can be changed by overwriting
 * some headers of the calibration data. The scanner uses a slower stepping
 * speed for higher exposure times */
static SANE_Status
_program_ps_exposure_time (HpOption this, HpScsi scsi,
                           HpOptSet UNUSEDARG optset, HpData data)
{
    SANE_Status status = SANE_STATUS_GOOD;
    size_t calib_size = 0;
    char *calib_buf = NULL;
    int i;
    int option =  hp_option_getint(this, data);
    static char *exposure[] =
                      {"\x00\x64\x00\x64\x00\x64",  /* 100% */
                       "\x00\x7d\x00\x7d\x00\x7d",  /* 125% */
                       "\x00\x96\x00\x96\x00\x96",  /* 150% */
                       "\x00\xaf\x00\xaf\x00\xaf",  /* 175% */
                       "\x00\xc0\x00\xc0\x00\xc0",  /* 200% */
                       "\x00\xe1\x00\xe1\x00\xe1",  /* 225% */
                       "\x00\xfa\x00\xfa\x00\xfa",  /* 250% */
                       "\x01\x13\x01\x13\x01\x13",  /* 275% */
                       "\x01\x24\x01\x24\x01\x24",  /* 300% */
                       "\x00\x64\x00\xc0\x01\x24"}; /* Negatives */
    /* Negatives get some extra blue to penetrate the orange mask and less
       red to not saturate the red channel; R:G:B = 100:200:300 */

    /* We dont use the 100% case. It may cause mechanical problems */
    if ((option < 1) || (option > 9)) return 0;
    RETURN_IF_FAIL ( sanei_hp_scl_upload_binary (scsi, SCL_CALIB_MAP,
               &calib_size, &calib_buf) );

    DBG(3, "_program_ps_exposure_time: Got %lu bytes of calibration data\n",
           (unsigned long) calib_size);

    for (i = 0; i < 6; i++)
       calib_buf[24 + i] = exposure[option][i];

    status = sanei_hp_scl_download ( scsi, SCL_CALIB_MAP, calib_buf,
           (size_t)calib_size);

    /* see what the scanner did to our alterations */
    /*
     * RETURN_IF_FAIL ( sanei_hp_scl_upload_binary (scsi, SCL_CALIB_MAP,
     *           &calib_size, &calib_buf) );
     *
     * for (i = 0; i < 9; i++)
     *    DBG(1, ">%x ", (unsigned char) calib_buf[24 + i]);
     */

    sanei_hp_free (calib_buf);

    return (status);
}

static SANE_Status
_program_scanmode (HpOption this, HpScsi scsi, HpOptSet optset, HpData data)
{
  enum hp_scanmode_e	new_mode  = hp_option_getint(this, data);
  int	invert	 = 0;
  int   fw_invert = 0;  /* Flag: does firmware do inversion ? */
  int   is_model_4c = 0;
  enum hp_device_compat_e compat;
  hp_bool_t  disable_xpa = ( sanei_hp_optset_scan_type (optset, data)
                               != SCL_XPA_SCAN );

  /* Seems that models 3c/4c/6100C invert image data at 10 bit by themself. */
  /* So we must not invert it by the invert command. */
  if (   (sanei_hp_device_probe (&compat,scsi) == SANE_STATUS_GOOD)
      && (compat & HP_COMPAT_4C) )
  {
    is_model_4c = 1;
    DBG(3, "program_scanmode: model 3c/4c/6100C recognized\n");
  }

  if (is_model_4c)
  {
    const HpDeviceInfo *info;
    int data_width;
    HpOption option;
    int is_preview = 0;

    /* Preview uses maximum 8 bit. So we don't need to check data width */
    option = hp_optset_getByName (optset, SANE_NAME_PREVIEW);
    if ( option )
      is_preview = hp_option_getint (option, data);

    info = sanei_hp_device_info_get ( sanei_hp_scsi_devicename  (scsi) );

    if (   (!is_preview)
        && hp_optset_isEnabled (optset, data, SANE_NAME_BIT_DEPTH, info))
    {
      data_width = sanei_hp_optset_data_width (optset, data);
      if ((data_width == 10) || (data_width == 30))
      {
        fw_invert = 1;
        DBG(3, "program_scanmode: firmware is doing inversion\n");
      }
    }
  }

  /* Disabling XPA resets some settings in the scanner. */
  /* Scanmode is the first we program. So set XPA prior to scanmode */
  DBG(3, "program_scanmode: disable XPA = %d\n", (int)disable_xpa);
  sanei_hp_scl_set(scsi, SCL_XPA_DISABLE, disable_xpa);

  RETURN_IF_FAIL( hp_option_download(this, data, optset, scsi) );

  switch (new_mode) {
  case HP_SCANMODE_GRAYSCALE:
      /* Make sure that it is not b/w. Correct data width will be set later */
      RETURN_IF_FAIL( sanei_hp_scl_set(scsi, SCL_DATA_WIDTH, 8) );
      invert = 1;
      if (fw_invert) invert = 0;
      /* For active XPA we use a tone map. Dont invert */
      if ( (!disable_xpa) && sanei_hp_is_active_xpa (scsi) ) invert = 0;
      break;
  case HP_SCANMODE_COLOR:
      invert = 1;
      if (fw_invert) invert = 0;
      /* For active XPA we use a tone map. Dont invert */
      if ( (!disable_xpa) && sanei_hp_is_active_xpa (scsi) ) invert = 0;
      break;
  default:
      break;
  }

  return sanei_hp_scl_set(scsi, SCL_INVERSE_IMAGE, invert);
}

static SANE_Status
_program_mirror_horiz (HpOption this, HpScsi scsi, HpOptSet UNUSEDARG optset,
                       HpData data)
{
  int sec_dir, mirror = hp_option_getint(this, data);

  if ( mirror == HP_MIRROR_HORIZ_CONDITIONAL )
  {
     RETURN_IF_FAIL( sanei_hp_scl_inquire(scsi, SCL_SECONDARY_SCANDIR,
                       &sec_dir, 0, 0) );
     mirror = (sec_dir == 1);
  }

  return sanei_hp_scl_set(scsi, SCL_MIRROR_IMAGE, mirror);
}

/*
 * Option enable predicates
 */
static hp_bool_t
_enable_choice (HpOption this, HpOptSet optset, HpData data,
                const HpDeviceInfo *info)
{
  SANE_String_Const * strlist
      = sanei_hp_accessor_choice_strlist((HpAccessorChoice) this->data_acsr,
				   optset, data, info);

  _set_stringlist(this, data, strlist);

  assert(strlist[0]);
  return strlist[0] != 0;
}

#ifdef ENABLE_7x12_TONEMAPS
static hp_bool_t
_enable_rgb_maps (HpOption this, HpOptSet optset, HpData data,
                  const HpDeviceInfo *info)
{
  HpOption 	cgam	= hp_optset_get(optset, CUSTOM_GAMMA);

  return (cgam && hp_option_getint(cgam, data)
	  && sanei_hp_optset_scanmode(optset, data) == HP_SCANMODE_COLOR);
}
#endif

static hp_bool_t
_enable_mono_map (HpOption UNUSEDARG this, HpOptSet optset, HpData data,
                  const HpDeviceInfo UNUSEDARG *info)
{
  HpOption 	cgam	= hp_optset_get(optset, CUSTOM_GAMMA);

  return (cgam && hp_option_getint(cgam, data)
	  && ( sanei_hp_optset_scanmode(optset, data) != HP_SCANMODE_COLOR
	       || ! hp_optset_getByName(optset, SANE_NAME_GAMMA_VECTOR_R) ));
}

static hp_bool_t
_enable_rgb_matrix (HpOption UNUSEDARG this, HpOptSet optset, HpData data,
                    const HpDeviceInfo UNUSEDARG *info)
{
  HpOption 	type = hp_optset_get(optset, MATRIX_TYPE);

  return type && hp_option_getint(type, data) == HP_MATRIX_CUSTOM;
}

static hp_bool_t
_enable_brightness (HpOption this, HpOptSet optset, HpData data,
                    const HpDeviceInfo *info)
{
  HpOption 	cgam = hp_optset_get(optset, CUSTOM_GAMMA);
  HpScl         scl = this->descriptor->scl_command;
  int           simulate;

  simulate = (   sanei_hp_device_support_get ( info->devname, scl, 0, 0 )
              != SANE_STATUS_GOOD );
  /* If brightness is simulated, we only do it for gray/color */
  if ( simulate )
  {HpOption  mode = hp_optset_get(optset, SCAN_MODE);
   int       val = hp_option_getint (mode, data);
   int       disable;

    disable = (val != HP_SCANMODE_GRAYSCALE) && (val != HP_SCANMODE_COLOR);
    if (disable)
    {
       if ( cgam )   /* Disable custom gamma. */
       {
         val = 0;
         hp_option_set (cgam, data, &val, 0);
       }
       return 0;
    }
  }

  return !cgam || !hp_option_getint(cgam, data);
}

static hp_bool_t
_enable_autoback (HpOption UNUSEDARG this, HpOptSet optset, HpData data,
                  const HpDeviceInfo UNUSEDARG *info)
{
  return sanei_hp_optset_scanmode(optset, data) == HP_SCANMODE_LINEART;
}

static hp_bool_t
_enable_custom_gamma (HpOption this, HpOptSet optset, HpData data,
                      const HpDeviceInfo *info)
{
  HpScl     scl_tonemap = SCL_8x8TONE_MAP;
  int       id = SCL_INQ_ID(scl_tonemap);
  int       simulate, minval, maxval;
  SANE_Status status;

  /* Check if download type supported */
  status = sanei_hp_device_support_get ( info->devname,
                SCL_DOWNLOAD_TYPE, &minval, &maxval);

  simulate = (status != SANE_STATUS_GOOD) || (id < minval) || (id > maxval);

  /* If custom gamma is simulated, we only do it for gray/color */
  if ( simulate )
  {HpOption  mode = hp_optset_get(optset, SCAN_MODE);
   int       val;

    if ( mode )
    {
      val = hp_option_getint (mode, data);
      if ((val != HP_SCANMODE_GRAYSCALE) && (val != HP_SCANMODE_COLOR))
      {
        val = 0;
        hp_option_set (this, data, &val, 0);
        return 0;
      }
    }
  }

  return 1;
}

static hp_bool_t
_enable_halftone (HpOption UNUSEDARG this, HpOptSet optset, HpData data,
                  const HpDeviceInfo UNUSEDARG *info)
{
  return sanei_hp_optset_scanmode(optset, data) == HP_SCANMODE_HALFTONE;
}

static hp_bool_t
_enable_halftonevec (HpOption UNUSEDARG this, HpOptSet optset, HpData data,
                     const HpDeviceInfo UNUSEDARG *info)
{
  if (sanei_hp_optset_scanmode(optset, data) == HP_SCANMODE_HALFTONE)
    {
      HpOption 		dither	= hp_optset_get(optset, HALFTONE_PATTERN);

      return dither && hp_option_getint(dither, data) == HP_DITHER_CUSTOM;
    }
  return 0;
}

static hp_bool_t
_enable_data_width (HpOption UNUSEDARG this, HpOptSet optset, HpData data,
                   const HpDeviceInfo UNUSEDARG *info)
{enum hp_scanmode_e mode;

 mode = sanei_hp_optset_scanmode (optset, data);
 return ( (mode == HP_SCANMODE_GRAYSCALE) || (mode == HP_SCANMODE_COLOR) );
}

static hp_bool_t
_enable_out8 (HpOption UNUSEDARG this, HpOptSet optset, HpData data,
              const HpDeviceInfo *info)
{
  if (hp_optset_isEnabled (optset, data, SANE_NAME_BIT_DEPTH, info))
  {
    int data_width = sanei_hp_optset_data_width (optset, data);
    return (((data_width > 8) && (data_width <= 16)) || (data_width > 24));
  }
  return 0;
}

static hp_bool_t
_enable_calibrate (HpOption UNUSEDARG this, HpOptSet optset, HpData data,
                   const HpDeviceInfo UNUSEDARG *info)
{
  HpOption 	media	= hp_optset_get(optset, MEDIA);

  /* If we dont have the media button, we should have calibrate */
  if ( !media ) return 1;

  return hp_option_getint(media, data) == HP_MEDIA_PRINT;
}

static SANE_Status
hp_download_calib_file (HpScsi scsi)
{
  int nbytes;
  char *calib_data;
  SANE_Status status;

  RETURN_IF_FAIL ( read_calib_file ( &nbytes, &calib_data, scsi ) );

  DBG(3, "hp_download_calib_file: Got %d bytes calibration data\n", nbytes);

  status = sanei_hp_scl_download ( scsi, SCL_CALIB_MAP, calib_data,
                                   (size_t) nbytes);
  sanei_hp_free ( calib_data );

  DBG(3, "hp_download_calib_file: download %s\n", (status == SANE_STATUS_GOOD) ?
      "successful" : "failed");

  return status;
}


/*
 * The actual option descriptors.
 */

#if (defined(__IBMC__) || defined(__IBMCPP__))
#ifndef _AIX
  #define INT INT
#endif
#endif

#define SCANNER_OPTION(name,type,unit)					\
   PASTE(SANE_NAME_,name),						\
   PASTE(SANE_TITLE_,name),						\
   PASTE(SANE_DESC_,name),						\
   PASTE(SANE_TYPE_,type),						\
   PASTE(SANE_UNIT_,unit),						\
   SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT

#define CONSTANT_OPTION(name,type,unit)					\
   PASTE(SANE_NAME_,name),						\
   PASTE(SANE_TITLE_,name),						\
   PASTE(SANE_DESC_,name),						\
   PASTE(SANE_TYPE_,type),						\
   PASTE(SANE_UNIT_,unit),						\
   SANE_CAP_SOFT_DETECT

#define INTERNAL_OPTION(name,type,unit)					\
   PASTE(HP_NAME_,name), "", "",					\
   PASTE(SANE_TYPE_,type),						\
   PASTE(SANE_UNIT_,unit),						\
   SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT

#define OPTION_GROUP(name)   "", name, "", SANE_TYPE_GROUP, SANE_UNIT_NONE, 0

#define ADVANCED_GROUP(name)						\
   "", name, "",  SANE_TYPE_GROUP, SANE_UNIT_NONE, SANE_CAP_ADVANCED


#define REQUIRES(req)  req
#define NO_REQUIRES    REQUIRES(0)

static const struct hp_option_descriptor_s NUM_OPTIONS[1] = {{
    CONSTANT_OPTION(NUM_OPTIONS, INT, NONE),
    NO_REQUIRES,
    _probe_num_options,
    0,0,0,0,0,0,0,0,0,0,0,0 /* for gcc-s sake */
}};

static const struct hp_option_descriptor_s SCAN_MODE_GROUP[1] = {{
    OPTION_GROUP(SANE_I18N("Scan Mode")),
    0,0,0,0,0,0,0,0,0,0,0,0,0,0 /* for gcc-s sake */
}};

/* Preview stuff */
static const struct hp_option_descriptor_s PREVIEW_MODE[1] = {{
    SCANNER_OPTION(PREVIEW, BOOL, NONE),
    NO_REQUIRES,
    _probe_bool,
    0,0,0,0,0,0,0,0,0,0,0,0 /* for gcc-s sake */
}};

static const struct hp_choice_s _scanmode_choices[] = {
    { HP_SCANMODE_LINEART, SANE_VALUE_SCAN_MODE_LINEART, 0, 0, 0 },
    { HP_SCANMODE_HALFTONE, SANE_VALUE_SCAN_MODE_HALFTONE, 0, 0, 0 },
    { HP_SCANMODE_GRAYSCALE, SANE_VALUE_SCAN_MODE_GRAY, 0, 0, 0 },
    { HP_SCANMODE_COLOR, SANE_VALUE_SCAN_MODE_COLOR, 0, 0, 0 },
    { 0, 0, 0, 0, 0 }
};
static const struct hp_option_descriptor_s SCAN_MODE[1] = {{
    SCANNER_OPTION(SCAN_MODE, STRING, NONE),
    NO_REQUIRES,
    _probe_each_choice, _program_scanmode, 0,
    1, 1, 1, 0, 0, SCL_OUTPUT_DATA_TYPE, 0, 0, 0, _scanmode_choices
}};
static const struct hp_option_descriptor_s SCAN_RESOLUTION[1] = {{
    SCANNER_OPTION(SCAN_RESOLUTION, INT, DPI),
    NO_REQUIRES,
    _probe_resolution, _program_resolution, 0,
    0, 1, 0, 0, 1, SCL_X_RESOLUTION, 0, 0, 0, 0
}};
static const struct hp_option_descriptor_s DEVPIX_RESOLUTION[1] = {{
    INTERNAL_OPTION(DEVPIX_RESOLUTION, INT, DPI),
    NO_REQUIRES,
    _probe_devpix, 0, 0,
    0, 0, 0, 0, 1, SCL_DEVPIX_RESOLUTION, 0, 0, 0, 0
}};

static const struct hp_option_descriptor_s ENHANCEMENT_GROUP[1] = {{
    OPTION_GROUP(SANE_I18N("Enhancement")),
    0,0,0,0,0,0,0,0,0,0,0,0,0,0 /* for gcc-s sake */
}};
static const struct hp_option_descriptor_s BRIGHTNESS[1] = {{
    SCANNER_OPTION(BRIGHTNESS, INT, NONE),
    NO_REQUIRES,
    _probe_int_brightness, _program_generic_simulate, _enable_brightness,
    0, 0, 0, 0, 0, SCL_BRIGHTNESS, -127, 127, 0, 0
}};
static const struct hp_option_descriptor_s CONTRAST[1] = {{
    SCANNER_OPTION(CONTRAST, INT, NONE),
    NO_REQUIRES,
    _probe_int_brightness, _program_generic_simulate, _enable_brightness,
    0, 0, 0, 0, 0, SCL_CONTRAST, -127, 127, 0, 0
}};
#ifdef SCL_SHARPENING
static const struct hp_option_descriptor_s SHARPENING[1] = {{
    SCANNER_OPTION(SHARPENING, INT, NONE),
    NO_REQUIRES,
    _probe_int, _program_generic, 0,
    0, 0, 0, 0, 0, SCL_SHARPENING, -127, 127, 0, 0
}};
#endif
static const struct hp_option_descriptor_s AUTO_THRESHOLD[1] = {{
    SCANNER_OPTION(AUTO_THRESHOLD, BOOL, NONE),
    NO_REQUIRES,
    _probe_bool, _program_generic, _enable_autoback,
    0, 0, 0, 0, 0, SCL_AUTO_BKGRND, 0, 0, 0, 0
}};

static const struct hp_option_descriptor_s ADVANCED_GROUP[1] = {{
    ADVANCED_GROUP(SANE_I18N("Advanced Options")),
    0,0,0,0,0,0,0,0,0,0,0,0,0,0 /* for gcc-s sake */
}};
/* FIXME: make this a choice? (BW or RGB custom) */
static const struct hp_option_descriptor_s CUSTOM_GAMMA[1] = {{
    SCANNER_OPTION(CUSTOM_GAMMA, BOOL, NONE),
    NO_REQUIRES,
    _probe_custom_gamma, _program_tonemap, _enable_custom_gamma,
    1, 0, 0, 0, 0, SCL_TONE_MAP, 0, 0, 0, 0
}};
static const struct hp_option_descriptor_s GAMMA_VECTOR_8x8[1] = {{
    SCANNER_OPTION(GAMMA_VECTOR, FIXED, NONE),
    NO_REQUIRES,
    _probe_gamma_vector, 0, _enable_mono_map,
    0, 0, 0, 0, 0, SCL_8x8TONE_MAP, 0, 0, 0, 0
}};

#ifdef ENABLE_7x12_TONEMAPS
static const struct hp_option_descriptor_s GAMMA_VECTOR_7x12[1] = {{
    SCANNER_OPTION(GAMMA_VECTOR, FIXED, NONE),
    REQUIRES(HP_COMPAT_5P | HP_COMPAT_5100C | HP_COMPAT_PS | HP_COMPAT_6200C
             |HP_COMPAT_5200C|HP_COMPAT_6300C),
    _probe_gamma_vector, 0, _enable_mono_map,
    0, 0, 0, 0, 0, SCL_BW7x12TONE_MAP, 0, 0, 0, 0
}};

static const struct hp_option_descriptor_s RGB_TONEMAP[1] = {{
    INTERNAL_OPTION(RGB_TONEMAP, FIXED, NONE),
    REQUIRES(HP_COMPAT_5P | HP_COMPAT_5100C | HP_COMPAT_PS | HP_COMPAT_6200C
             |HP_COMPAT_5200C|HP_COMPAT_6300C),
    _probe_gamma_vector, 0, 0,
    0, 0, 0, 0, 0, SCL_7x12TONE_MAP, 0, 0, 0, 0
}};
static const struct hp_option_descriptor_s GAMMA_VECTOR_R[1] = {{
    SCANNER_OPTION(GAMMA_VECTOR_R, FIXED, NONE),
    REQUIRES(HP_COMPAT_5P | HP_COMPAT_5100C | HP_COMPAT_PS | HP_COMPAT_6200C
             |HP_COMPAT_5200C|HP_COMPAT_6300C),
    _probe_gamma_vector, 0, _enable_rgb_maps,
    0,0,0,0,0,0,0,0,0,0
}};
static const struct hp_option_descriptor_s GAMMA_VECTOR_G[1] = {{
    SCANNER_OPTION(GAMMA_VECTOR_G, FIXED, NONE),
    REQUIRES(HP_COMPAT_5P | HP_COMPAT_5100C | HP_COMPAT_PS | HP_COMPAT_6200C
             |HP_COMPAT_5200C|HP_COMPAT_6300C),
    _probe_gamma_vector, 0, _enable_rgb_maps,
    0,0,0,0,0,0,0,0,0,0
}};
static const struct hp_option_descriptor_s GAMMA_VECTOR_B[1] = {{
    SCANNER_OPTION(GAMMA_VECTOR_B, FIXED, NONE),
    REQUIRES(HP_COMPAT_5P | HP_COMPAT_5100C | HP_COMPAT_PS | HP_COMPAT_6200C
             |HP_COMPAT_5200C|HP_COMPAT_6300C),
    _probe_gamma_vector, 0, _enable_rgb_maps,
    0,0,0,0,0,0,0,0,0,0
}};
#endif

static const struct hp_choice_s _halftone_choices[] = {
    { HP_DITHER_COARSE,		SANE_I18N("Coarse"), 0, 0, 0 },
    { HP_DITHER_FINE,		SANE_I18N("Fine"), 0, 0, 0 },
    { HP_DITHER_BAYER,		SANE_I18N("Bayer"), 0, 0, 0 },
    { HP_DITHER_VERTICAL,	SANE_I18N("Vertical"), 0, 0, 0 },
    { HP_DITHER_HORIZONTAL,	SANE_I18N("Horizontal"), 0, 1, 0 },
    { HP_DITHER_CUSTOM, 	SANE_I18N("Custom"), 0, 0, 0 },
    { 0, 0, 0, 0, 0 }
};
static const struct hp_option_descriptor_s HALFTONE_PATTERN[1] = {{
    SCANNER_OPTION(HALFTONE_PATTERN, STRING, NONE),
    NO_REQUIRES,
    _probe_each_choice, _program_dither, _enable_halftone,
    1, 0, 0, 0, 0, SCL_BW_DITHER, 0, 0, 0, _halftone_choices
}};
/* FIXME: Halftone dimension? */

#ifdef ENABLE_16X16_DITHERS
static const struct hp_option_descriptor_s HALFTONE_PATTERN_16x16[1] = {{
    SCANNER_OPTION(HALFTONE_PATTERN, FIXED, NONE),
    REQUIRES(HP_COMPAT_5P | HP_COMPAT_4P | HP_COMPAT_4C | HP_COMPAT_5100C
              | HP_COMPAT_6200C | HP_COMPAT_5200C | HP_COMPAT_6300C),
    _probe_horiz_dither, 0, _enable_halftonevec,
    0, 0, 0, 0, 0, SCL_BW16x16DITHER
}};
static const struct hp_option_descriptor_s HORIZONTAL_DITHER_16x16[1] = {{
    INTERNAL_OPTION(HORIZONTAL_DITHER, FIXED, NONE),
    REQUIRES(HP_COMPAT_5P | HP_COMPAT_4P | HP_COMPAT_4C | HP_COMPAT_5100C
              | HP_COMPAT_6200C | HP_COMPAT_5200C | HP_COMPAT_6300C),
    _probe_horiz_dither, 0, 0,
    0, 0, 0, 0, 0, SCL_BW16x16DITHER
}};
#endif
static const struct hp_option_descriptor_s HALFTONE_PATTERN_8x8[1] = {{
    SCANNER_OPTION(HALFTONE_PATTERN, FIXED, NONE),
    NO_REQUIRES,
    _probe_horiz_dither, 0, _enable_halftonevec,
    0, 0, 0, 0, 0, SCL_BW8x8DITHER, 0, 0, 0, 0
}};
static const struct hp_option_descriptor_s HORIZONTAL_DITHER_8x8[1] = {{
    INTERNAL_OPTION(HORIZONTAL_DITHER, FIXED, NONE),
    NO_REQUIRES,
    _probe_horiz_dither, 0, 0,
    0, 0, 0, 0, 0, SCL_BW8x8DITHER, 0, 0, 0, 0
}};

static const struct hp_choice_s _matrix_choices[] = {
    { HP_MATRIX_AUTO, SANE_I18N("Auto"), 0, 1, 0 },
    { HP_MATRIX_RGB, SANE_I18N("NTSC RGB"), _cenable_incolor, 0, 0 },
    { HP_MATRIX_XPA_RGB, SANE_I18N("XPA RGB"), _cenable_incolor, 0, 0 },
    { HP_MATRIX_PASS, SANE_I18N("Pass-through"), _cenable_incolor, 0, 0 },
    { HP_MATRIX_BW, SANE_I18N("NTSC Gray"), _cenable_notcolor, 0, 0 },
    { HP_MATRIX_XPA_BW, SANE_I18N("XPA Gray"), _cenable_notcolor, 0, 0 },
    { HP_MATRIX_RED, SANE_I18N("Red"), _cenable_notcolor, 0, 0 },
    { HP_MATRIX_GREEN, SANE_I18N("Green"), _cenable_notcolor, 1, 0 },
    { HP_MATRIX_BLUE, SANE_I18N("Blue"), _cenable_notcolor, 0, 0 },
#ifdef ENABLE_CUSTOM_MATRIX
    { HP_MATRIX_CUSTOM, SANE_I18N("Custom"), 0, 0, 0 },
#endif
    { 0, 0, 0, 0, 0 }
};

static const struct hp_option_descriptor_s MATRIX_TYPE[1] = {{
    SCANNER_OPTION(MATRIX_TYPE, STRING, NONE),
    NO_REQUIRES,
    _probe_each_choice, _program_matrix, _enable_choice,
    1, 0, 0, 0, 0, SCL_MATRIX, 0, 0, 0, _matrix_choices
}};

static const struct hp_option_descriptor_s MATRIX_RGB[1] = {{
    SCANNER_OPTION(MATRIX_RGB, FIXED, NONE),
    NO_REQUIRES,
    _probe_matrix, 0, _enable_rgb_matrix,
    0, 0, 0, 0, 0, SCL_8x9MATRIX_COEFF, 0, 0, 0, 0
}};
#ifdef FAKE_COLORSEP_MATRIXES
static const struct hp_option_descriptor_s SEPMATRIX[1] = {{
    INTERNAL_OPTION(SEPMATRIX, FIXED, NONE),
    NO_REQUIRES,
    _probe_vector, 0, 0,
    0, 0, 0, 0, 0, SCL_8x9MATRIX_COEFF, 0, 0, 0, 0
}};
#endif
#ifdef ENABLE_10BIT_MATRIXES
static const struct hp_option_descriptor_s MATRIX_RGB10[1] = {{
    SCANNER_OPTION(MATRIX_RGB, FIXED, NONE),
    REQUIRES(HP_COMPAT_5P | HP_COMPAT_5100C | HP_COMPAT_6200C
             | HP_COMPAT_5200C | HP_COMPAT_6300C),
    _probe_matrix, 0, _enable_rgb_matrix,
    0, 0, 0, 0, 0, SCL_10x9MATRIX_COEFF, 0, 0, 0, 0
}};
#endif
#ifdef NotYetSupported
static const struct hp_option_descriptor_s BWMATRIX_GRAY10[1] = {{
    SCANNER_OPTION(MATRIX_GRAY, FIXED, NONE),
    REQUIRES(HP_COMPAT_5P | HP_COMPAT_5100C | HP_COMPAT_6200C
             | HP_COMPAT_5200C | HP_COMPAT_6300C),
    _probe_matrix, 0, _enable_gray_matrix,
    0, 0, 0, 0, 0, SCL_10x3MATRIX_COEFF, 0, 0, 0, 0
}};
#endif

static const struct hp_choice_s _scan_speed_choices[] = {
    { 0, SANE_I18N("Auto"), 0, 0, 0 },
    { 1, SANE_I18N("Slow"), 0, 0, 0 },
    { 2, SANE_I18N("Normal"), 0, 0, 0 },
    { 3, SANE_I18N("Fast"), 0, 0, 0 },
    { 4, SANE_I18N("Extra Fast"), 0, 0, 0 },
    { 0, 0, 0, 0, 0 }
};
static const struct hp_option_descriptor_s SCAN_SPEED[1] = {{
    SCANNER_OPTION(SCAN_SPEED, STRING, NONE),
    NO_REQUIRES,
    _probe_each_choice, _program_generic, 0,
    0, 0, 0, 0, 1, SCL_SPEED, 0, 0, 0, _scan_speed_choices
}};

static const struct hp_choice_s _smoothing_choices[] = {
    { 0, SANE_I18N("Auto"), 0, 0, 0 },
    { 3, SANE_I18N("Off"), 0, 0, 0 },
    { 1, SANE_I18N("2-pixel"), 0, 0, 0 },
    { 2, SANE_I18N("4-pixel"), 0, 0, 0 },
    { 4, SANE_I18N("8-pixel"), 0, 0, 0 },
    { 0, 0, 0, 0, 0 }
};
static const struct hp_option_descriptor_s SMOOTHING[1] = {{
    SCANNER_OPTION(SMOOTHING, STRING, NONE),
    NO_REQUIRES,
    _probe_each_choice, _program_generic, 0,
    0, 0, 0, 0, 0, SCL_FILTER, 0, 0, 0, _smoothing_choices
}};

static const struct hp_choice_s _media_choices[] = {
    { HP_MEDIA_PRINT, SANE_I18N("Print"), 0, 0, 0 },
    { HP_MEDIA_SLIDE, SANE_I18N("Slide"), 0, 0, 0 },
    { HP_MEDIA_NEGATIVE, SANE_I18N("Film-strip"), 0, 0, 0 },
    { 0, 0, 0, 0, 0 }
};
static const struct hp_option_descriptor_s MEDIA[1] = {{
    SCANNER_OPTION(MEDIA, STRING, NONE),
    NO_REQUIRES,
    _probe_choice, _program_media, 0,
    1, 1, 1, 1, 0, SCL_MEDIA, 0, 0, 0, _media_choices
}};

static const struct hp_choice_s _data_widths[] = {
    {1, "1", 0, 0, 0},
    {8, "8", 0, 0, 0},
    {10, "10", 0, 0, 0},
    {12, "12", 0, 0, 0},
    {14, "14", 0, 0, 0},
    {16, "16", 0, 0, 0},
    {0, 0, 0, 0, 0}
};

static const struct hp_option_descriptor_s BIT_DEPTH[1] = {{
    SCANNER_OPTION(BIT_DEPTH, STRING, NONE),
    NO_REQUIRES,
    _probe_choice, _program_data_width, _enable_data_width,
    1, 1, 1, 0, 1, SCL_DATA_WIDTH, 0, 0, 0, _data_widths
}};

static const struct hp_option_descriptor_s OUT8[1] =
{
  {
    SCANNER_OPTION(OUTPUT_8BIT, BOOL, NONE),
    NO_REQUIRES,            /* enum hp_device_compat_e requires */
    _probe_bool,            /* SANE_Status (*probe)() */
    0,                      /* SANE_Status (*program)() */
    _enable_out8,           /* hp_bool_t (*enable)() */
    0,                      /* hp_bool_t has_global_effect */
    0,                      /* hp_bool_t affects_scan_params */
    0,                      /* hp_bool_t program_immediate */
    0,                      /* hp_bool_t suppress_for_scan */
    0,                      /* hp_bool_t may_change */
    0,                      /* HpScl scl_command */
    0,                      /* int minval */
    0,                      /* int maxval */
    0,                      /* int startval */
    0                       /* HpChoice choices */
  }
};

/* The 100% setting may cause problems within the scanner */
static const struct hp_choice_s _ps_exposure_times[] = {
    /* {0, "100%", 0, 0, 0}, */
    { 0, SANE_I18N("Default"), 0, 0, 0 },
    {1, "125%", 0, 0, 0},
    {2, "150%", 0, 0, 0},
    {3, "175%", 0, 0, 0},
    {4, "200%", 0, 0, 0},
    {5, "225%", 0, 0, 0},
    {6, "250%", 0, 0, 0},
    {7, "275%", 0, 0, 0},
    {8, "300%", 0, 0, 0},
    {9, SANE_I18N("Negative"), 0, 0, 0},
    {0, 0, 0, 0, 0}
};

/* Photosmart exposure time */
static const struct hp_option_descriptor_s PS_EXPOSURE_TIME[1] = {{
    SCANNER_OPTION(PS_EXPOSURE_TIME, STRING, NONE),
    REQUIRES( HP_COMPAT_PS ),
    _probe_ps_exposure_time, _program_ps_exposure_time, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, _ps_exposure_times
}};

/* Normal, ADF or XPA scanning. Because scanning from ADF can change */
/* the extent of the scanning area, this option is marked to change  */
/* global settings. The user should switch to ADF scanning after     */
/* placing paper in the ADF. */
static const struct hp_choice_s _scan_types[] = {
    { HP_SCANTYPE_NORMAL, SANE_I18N("Normal"), 0, 0, 0 },
    { HP_SCANTYPE_ADF, SANE_I18N("ADF"), 0, 0, 0 },
    { HP_SCANTYPE_XPA, SANE_I18N("XPA"), 0, 0, 0 },
    {0, 0, 0, 0, 0 }
};

static const struct hp_option_descriptor_s SCAN_SOURCE[1] = {{
    SCANNER_OPTION(SCAN_SOURCE, STRING, NONE),
    NO_REQUIRES,
    _probe_scan_type, _program_scan_type, 0,
    1, 1, 1, 0, 0, SCL_START_SCAN, 0, 0, 0, _scan_types
}};

/* Unload after is only necessary for PhotoScanner */
static const struct hp_option_descriptor_s UNLOAD_AFTER_SCAN[1] = {{
    SCANNER_OPTION(UNLOAD_AFTER_SCAN, BOOL, NONE),
    REQUIRES(HP_COMPAT_PS),
    _probe_bool, _program_unload_after_scan, 0,
    0, 0, 0, 1, 0, SCL_UNLOAD, 0, 0, 0, 0
}};

static const struct hp_option_descriptor_s CHANGE_DOC[1] = {{
    SCANNER_OPTION(CHANGE_DOC, BUTTON, NONE),
    NO_REQUIRES,
    _probe_change_doc, _program_change_doc, 0,
    1, 1, 1, 1, 0, SCL_CHANGE_DOC, 0, 0, 0, 0
}};

static const struct hp_option_descriptor_s UNLOAD[1] = {{
    SCANNER_OPTION(UNLOAD, BUTTON, NONE),
    NO_REQUIRES,
    _probe_unload, _program_unload, 0,
    0, 0, 1, 1, 0, SCL_UNLOAD, 0, 0, 0, 0
}};

/* There is no inquire ID-for the calibrate command. */
/* So here we need the requiries. */
static const struct hp_option_descriptor_s CALIBRATE[1] = {{
    SCANNER_OPTION(CALIBRATE, BUTTON, NONE),
    REQUIRES(HP_COMPAT_PS),
    _probe_calibrate, _program_calibrate, _enable_calibrate,
    0, 0, 1, 1, 0, SCL_CALIBRATE, 0, 0, 0, 0
}};

static const struct hp_option_descriptor_s GEOMETRY_GROUP[1] = {{
    ADVANCED_GROUP(SANE_I18N("Geometry")),
    0,0,0,0,0,0,0,0,0,0,0,0,0,0 /* for gcc-s sake */
}};
static const struct hp_option_descriptor_s SCAN_TL_X[1] = {{
    SCANNER_OPTION(SCAN_TL_X, FIXED, MM),
    NO_REQUIRES,
    _probe_geometry, _program_geometry, 0,
    0, 1, 0, 0, 1, SCL_X_POS, 0, 0, 0, 0
}};
static const struct hp_option_descriptor_s SCAN_TL_Y[1] = {{
    SCANNER_OPTION(SCAN_TL_Y, FIXED, MM),
    NO_REQUIRES,
    _probe_geometry, _program_geometry, 0,
    0, 1, 0, 0, 1, SCL_Y_POS, 0, 0, 0, 0
}};
static const struct hp_option_descriptor_s SCAN_BR_X[1] = {{
    SCANNER_OPTION(SCAN_BR_X, FIXED, MM),
    NO_REQUIRES,
    _probe_geometry, _program_geometry, 0,
    0, 1, 0, 0, 1, SCL_X_EXTENT, 0, 0, 0, 0
}};
static const struct hp_option_descriptor_s SCAN_BR_Y[1] = {{
    SCANNER_OPTION(SCAN_BR_Y, FIXED, MM),
    NO_REQUIRES,
    _probe_geometry, _program_geometry, 0,
    0, 1, 0, 0, 1, SCL_Y_EXTENT, 0, 0, 0, 0
}};

static const struct hp_choice_s _mirror_horiz_choices[] = {
    { HP_MIRROR_HORIZ_OFF, SANE_I18N("Off"), 0, 0, 0 },
    { HP_MIRROR_HORIZ_ON, SANE_I18N("On"), 0, 0, 0 },
    { HP_MIRROR_HORIZ_CONDITIONAL, SANE_I18N("Conditional"), 0, 0, 0 },
    { 0, 0, 0, 0, 0 }
};
static const struct hp_option_descriptor_s MIRROR_HORIZ[1] = {{
    SCANNER_OPTION(MIRROR_HORIZ, STRING, NONE),
    NO_REQUIRES,
    _probe_mirror_horiz, _program_mirror_horiz, 0,
    0, 0, 0, 0, 0, SCL_MIRROR_IMAGE, 0, 0, 0, _mirror_horiz_choices
}};

static const struct hp_choice_s _mirror_vert_choices[] = {
    { HP_MIRROR_VERT_OFF, SANE_I18N("Off"), 0, 0, 0 },
    { HP_MIRROR_VERT_ON, SANE_I18N("On"), 0, 0, 0 },
    { HP_MIRROR_VERT_CONDITIONAL, SANE_I18N("Conditional"), 0, 0, 0 },
    { 0, 0, 0, 0, 0 }
};
static const struct hp_option_descriptor_s MIRROR_VERT[1] = {{
    SCANNER_OPTION(MIRROR_VERT, STRING, NONE),
    NO_REQUIRES,
    _probe_mirror_vert, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, _mirror_vert_choices
}};

static const struct hp_option_descriptor_s BUTTON_WAIT[1] =
{
  {
    SCANNER_OPTION(BUTTON_WAIT, BOOL, NONE),
    NO_REQUIRES,            /* enum hp_device_compat_e requires */
    _probe_front_button,    /* SANE_Status (*probe)() */
    0,                      /* SANE_Status (*program)() */
    0,                      /* hp_bool_t (*enable)() */
    0,                      /* hp_bool_t has_global_effect */
    0,                      /* hp_bool_t affects_scan_params */
    0,                      /* hp_bool_t program_immediate */
    0,                      /* hp_bool_t suppress_for_scan */
    0,                      /* hp_bool_t may_change */
    0,                      /* HpScl scl_command */
    0,                      /* int minval */
    0,                      /* int maxval */
    0,                      /* int startval */
    0                       /* HpChoice choices */
  }
};


static const struct hp_option_descriptor_s LAMP_OFF[1] =
{
  {
    SCANNER_OPTION(LAMP_OFF, BUTTON, NONE),
    /* Lamp off instruction not supported by Photosmart */
    REQUIRES(  HP_COMPAT_PLUS | HP_COMPAT_2C | HP_COMPAT_2P | HP_COMPAT_2CX
             | HP_COMPAT_4C |  HP_COMPAT_3P | HP_COMPAT_4P | HP_COMPAT_5P
             | HP_COMPAT_5100C | HP_COMPAT_6200C | HP_COMPAT_5200C
             | HP_COMPAT_6300C),   /* enum hp_device_compat_e requires */
    _probe_bool,            /* SANE_Status (*probe)() */
    _program_lamp_off,      /* SANE_Status (*program)() */
    0,                      /* hp_bool_t (*enable)() */
    0,                      /* hp_bool_t has_global_effect */
    0,                      /* hp_bool_t affects_scan_params */
    1,                      /* hp_bool_t program_immediate */
    1,                      /* hp_bool_t suppress_for_scan */
    0,                      /* hp_bool_t may_change */
    SCL_LAMPTEST,           /* HpScl scl_command */
    0,                      /* int minval */
    0,                      /* int maxval */
    0,                      /* int startval */
    0                       /* HpChoice choices */
  }
};

#ifdef HP_EXPERIMENTAL

static const struct hp_choice_s _range_choices[] = {
    { 0, "0", 0, 0, 0 },
    { 1, "1", 0, 0, 0 },
    { 2, "2", 0, 0, 0 },
    { 3, "3", 0, 0, 0 },
    { 4, "4", 0, 0, 0 },
    { 5, "5", 0, 0, 0 },
    { 6, "6", 0, 0, 0 },
    { 7, "7", 0, 0, 0 },
    { 8, "8", 0, 0, 0 },
    { 9, "9", 0, 0, 0 },
    { 0, 0, 0, 0, 0 }
};
static const struct hp_option_descriptor_s EXPERIMENT_GROUP[1] = {{
    ADVANCED_GROUP(SANE_I18N("Experiment"))
    0,0,0,0,0,0,0,0,0,0,0,0,0,0 /* for gcc-s sake */
}};
static const struct hp_option_descriptor_s PROBE_10470[1] = {{
    SCANNER_OPTION(10470, STRING, NONE),
    NO_REQUIRES,
    _probe_each_choice, _program_generic, 0,
    0, 0, 0, 0, 0, SCL_10470, 0, 0, 0, _range_choices
}};
static const struct hp_option_descriptor_s PROBE_10485[1] = {{
    SCANNER_OPTION(10485, STRING, NONE),
    NO_REQUIRES,
    _probe_each_choice, _program_generic, 0,
    0, 0, 0, 0, 0, SCL_10485, 0, 0, 0, _range_choices
}};
static const struct hp_option_descriptor_s PROBE_10952[1] = {{
    SCANNER_OPTION(10952, STRING, NONE),
    NO_REQUIRES,
    _probe_each_choice, _program_generic, 0,
    0, 0, 0, 0, 0, SCL_10952, 0, 0, 0, _range_choices
}};
static const struct hp_option_descriptor_s PROBE_10967[1] = {{
    SCANNER_OPTION(10967, INT, NONE),
    NO_REQUIRES,
    _probe_int, _program_generic, 0,
    0, 0, 0, 0, 0, SCL_10967, 0, 0, 0, 0
}};

#endif

static HpOptionDescriptor hp_options[] = {
    NUM_OPTIONS,

    SCAN_MODE_GROUP,
    PREVIEW_MODE,
    SCAN_MODE, SCAN_RESOLUTION, DEVPIX_RESOLUTION,

    ENHANCEMENT_GROUP,
    BRIGHTNESS, CONTRAST,
#ifdef SCL_SHARPENING
    SHARPENING,
#endif
    AUTO_THRESHOLD,

    ADVANCED_GROUP,
    CUSTOM_GAMMA,
#ifdef ENABLE_7x12_TONEMAPS
    GAMMA_VECTOR_7x12,
    RGB_TONEMAP, GAMMA_VECTOR_R, GAMMA_VECTOR_G, GAMMA_VECTOR_B,
#endif
    GAMMA_VECTOR_8x8,

    MATRIX_TYPE,
#ifdef FAKE_COLORSEP_MATRIXES
    SEPMATRIX,
#endif
#ifdef ENABLE_10BIT_MATRIXES
    MATRIX_RGB10, /* FIXME: unsupported: MATRIX_GRAY10, */
#endif
    MATRIX_RGB,

    HALFTONE_PATTERN,
#ifdef ENABLE_16X16_DITHERS
    HALFTONE_PATTERN_16x16, HORIZONTAL_DITHER_16x16,
#endif
    HALFTONE_PATTERN_8x8, HORIZONTAL_DITHER_8x8,

    SCAN_SPEED, SMOOTHING, MEDIA, PS_EXPOSURE_TIME, BIT_DEPTH, OUT8,
    SCAN_SOURCE, BUTTON_WAIT, LAMP_OFF, UNLOAD_AFTER_SCAN,
    CHANGE_DOC, UNLOAD, CALIBRATE,

    GEOMETRY_GROUP,
    SCAN_TL_X, SCAN_TL_Y, SCAN_BR_X, SCAN_BR_Y,
    MIRROR_HORIZ, MIRROR_VERT,

#ifdef HP_EXPERIMENTAL

    EXPERIMENT_GROUP,
    PROBE_10470,
    PROBE_10485,
    PROBE_10952,
    PROBE_10967,

#endif

    0
};



/*
 * class HpOptSet
 */

struct hp_optset_s
{
#define OPTION_LIST_MAX	sizeof(hp_options)/sizeof(hp_options[0])
    HpOption	options[OPTION_LIST_MAX];
    size_t	num_sane_opts;
    size_t	num_opts;

    /* Magic accessors to get coord in actual scan pixels: */
    HpAccessor	tl_x, tl_y, br_x, br_y;
};

static HpOption
hp_optset_get (HpOptSet this, HpOptionDescriptor optd)
{
  HpOption *	optp	= this->options;
  int		i	= this->num_opts;

  while (i--)
    {
      if ((*optp)->descriptor == optd)
	  return *optp;
      optp++;
    }
  return 0;
}

static HpOption
hp_optset_getByIndex (HpOptSet this, int optnum)
{
  if ((optnum < 0) || (optnum >= (int)this->num_sane_opts))
      return 0;
  return this->options[optnum];
}

static HpOption
hp_optset_getByName (HpOptSet this, const char * name)
{
  HpOption *	optp	= this->options;
  int		i	= this->num_opts;

  while (i--)
    {
      if (strcmp((*optp)->descriptor->name, name) == 0)
	  return *optp;
      optp++;
    }
  return 0;
}

static _HpOption
_hp_optset_get (HpOptSet this, HpOptionDescriptor opt)
{
  /* Cast away const-ness */
  return (_HpOption) hp_optset_get(this, opt);
}

enum hp_scanmode_e
sanei_hp_optset_scanmode (HpOptSet this, HpData data)
{
  HpOption mode = hp_optset_get(this, SCAN_MODE);
  assert(mode);
  return hp_option_getint(mode, data);
}


hp_bool_t
sanei_hp_optset_output_8bit (HpOptSet this, HpData data)
{
  HpOption option_out8;
  int out8;

  option_out8 = hp_optset_get(this, OUT8);
  if (option_out8)
  {
    out8 = hp_option_getint(option_out8, data);
    return out8;
  }
  return 0;
}


/* Returns the data width that is send to the scanner, depending */
/* on the scanmode. (b/w: 1, gray: 8..12, color: 24..36 */
int
sanei_hp_optset_data_width (HpOptSet this, HpData data)
{
 enum hp_scanmode_e mode = sanei_hp_optset_scanmode (this, data);
 int datawidth = 0;
 HpOption opt_dwidth;

 switch (mode)
 {
   case HP_SCANMODE_LINEART:
   case HP_SCANMODE_HALFTONE:
          datawidth = 1;
          break;

   case HP_SCANMODE_GRAYSCALE:
          opt_dwidth = hp_optset_get(this, BIT_DEPTH);
          if (opt_dwidth)
            datawidth = hp_option_getint (opt_dwidth, data);
          else
            datawidth = 8;
          break;

   case HP_SCANMODE_COLOR:
          opt_dwidth = hp_optset_get(this, BIT_DEPTH);
          if (opt_dwidth)
            datawidth = 3 * hp_option_getint (opt_dwidth, data);
          else
            datawidth = 24;
          break;
 }
 return datawidth;
}

hp_bool_t
sanei_hp_optset_mirror_vert (HpOptSet this, HpData data, HpScsi scsi)
{
  HpOption mode;
  int      mirror, sec_dir;

  mode = hp_optset_get(this, MIRROR_VERT);
  assert(mode);
  mirror = hp_option_getint(mode, data);

  if (mirror == HP_MIRROR_VERT_CONDITIONAL)
  {
    mirror = HP_MIRROR_VERT_OFF;
    if ( ( sanei_hp_scl_inquire(scsi, SCL_SECONDARY_SCANDIR, &sec_dir, 0, 0)
           == SANE_STATUS_GOOD ) && ( sec_dir == 1 ) )
      mirror = HP_MIRROR_VERT_ON;
  }
  return mirror == HP_MIRROR_VERT_ON;
}

hp_bool_t sanei_hp_optset_start_wait(HpOptSet this, HpData data)
{
  HpOption mode;
  int wait;

  if ((mode = hp_optset_get(this, BUTTON_WAIT)) == 0)
    return(0);

  wait = hp_option_getint(mode, data);

  return(wait);
}

HpScl
sanei_hp_optset_scan_type (HpOptSet this, HpData data)
{
  HpOption mode;
  HpScl    scl = SCL_START_SCAN;
  int      scantype;

  mode = hp_optset_get(this, SCAN_SOURCE);
  if (mode)
  {
    scantype = hp_option_getint(mode, data);
    DBG(5, "sanei_hp_optset_scan_type: scantype=%d\n", scantype);

    switch (scantype)
    {
      case 1: scl = SCL_ADF_SCAN; break;
      case 2: scl = SCL_XPA_SCAN; break;
      default: scl = SCL_START_SCAN; break;
    }
  }
  return scl;
}

static void
hp_optset_add (HpOptSet this, HpOption opt)
{
  assert(this->num_opts < OPTION_LIST_MAX);

  /*
   * Keep internal options at the end of the list.
   */
  if (hp_option_isInternal(opt))
      this->options[this->num_opts] = opt;
  else
    {
      if (this->num_opts != this->num_sane_opts)
	  memmove(&this->options[this->num_sane_opts + 1],
		  &this->options[this->num_sane_opts],
		  ( (this->num_opts - this->num_sane_opts)
		    * sizeof(*this->options) ));
      this->options[this->num_sane_opts++] = opt;
    }
  this->num_opts++;
}

static SANE_Status
hp_optset_fix_geometry_options (HpOptSet this)
{
  _HpOption	tl_x	= _hp_optset_get(this, SCAN_TL_X);
  _HpOption	tl_y	= _hp_optset_get(this, SCAN_TL_Y);
  _HpOption	br_x	= _hp_optset_get(this, SCAN_BR_X);
  _HpOption	br_y	= _hp_optset_get(this, SCAN_BR_Y);
  HpOption	scanres	= hp_optset_get(this, SCAN_RESOLUTION);
  HpOption	devpix	= hp_optset_get(this, DEVPIX_RESOLUTION);

  HpAccessor	tl_xa, tl_ya, br_xa, br_ya;

  assert(tl_x && tl_y && br_x && br_y); /* Geometry options missing */

  tl_xa = tl_x->data_acsr;
  tl_ya = tl_y->data_acsr;
  br_xa = br_x->data_acsr;
  br_ya = br_y->data_acsr;

  assert(tl_xa && tl_ya && br_xa && br_ya);
  assert(scanres->data_acsr && devpix->data_acsr);

  /* Magic accessors that will read out in device pixels */
  tl_x->data_acsr = sanei_hp_accessor_geometry_new(tl_xa, br_xa, 0,
					     devpix->data_acsr);
  tl_y->data_acsr = sanei_hp_accessor_geometry_new(tl_ya, br_ya, 0,
					     devpix->data_acsr);
  br_x->data_acsr = sanei_hp_accessor_geometry_new(br_xa, tl_xa, 1,
					     devpix->data_acsr);
  br_y->data_acsr = sanei_hp_accessor_geometry_new(br_ya, tl_ya, 1,
					     devpix->data_acsr);

  if (!tl_x->data_acsr || !tl_y->data_acsr
      || !br_x->data_acsr || !br_y->data_acsr)
      return SANE_STATUS_NO_MEM;

  /* Magic accessors that will read out in scan pixels */
  this->tl_x = sanei_hp_accessor_geometry_new(tl_xa, br_xa, 0,
                                              scanres->data_acsr);
  this->tl_y = sanei_hp_accessor_geometry_new(tl_ya, br_ya, 0,
                                              scanres->data_acsr);
  this->br_x = sanei_hp_accessor_geometry_new(br_xa, tl_xa, 1,
                                              scanres->data_acsr);
  this->br_y = sanei_hp_accessor_geometry_new(br_ya, tl_ya, 1,
                                              scanres->data_acsr);
  if (!this->tl_x || !this->tl_y || !this->br_x || !this->br_y)
      return SANE_STATUS_NO_MEM;

  return SANE_STATUS_GOOD;
}

static void
hp_optset_reprogram (HpOptSet this, HpData data, HpScsi scsi)
{
  int i;

  DBG(5, "hp_optset_reprogram: %lu options\n",
      (unsigned long) this->num_opts);

  for (i = 0; i < (int)this->num_opts; i++)
      hp_option_reprogram(this->options[i], this, data, scsi);

  DBG(5, "hp_optset_reprogram: finished\n");
}

static void
hp_optset_reprobe (HpOptSet this, HpData data, HpScsi scsi)
{
  int i;

  DBG(5, "hp_optset_reprobe: %lu options\n",
      (unsigned long) this->num_opts);

  for (i = 0; i < (int)this->num_opts; i++)
      hp_option_reprobe(this->options[i], this, data, scsi);

  DBG(5, "hp_optset_reprobe: finished\n");
}

static void
hp_optset_updateEnables (HpOptSet this, HpData data, const HpDeviceInfo *info)
{
  int i;

  DBG(5, "hp_optset_updateEnables: %lu options\n",
      (unsigned long) this->num_opts);

  for (i = 0; i < (int)this->num_opts; i++)
      hp_option_updateEnable(this->options[i], this, data, info);
}

static hp_bool_t
hp_optset_isEnabled (HpOptSet this, HpData data, const char *name,
                     const HpDeviceInfo *info)
{
  HpOption optpt;

  optpt = hp_optset_getByName (this, name);

  if (!optpt)  /* Not found ? Not enabled */
    return 0;

  if (!(optpt->descriptor->enable)) /* No enable necessary ? Enabled */
    return 1;

  return (*optpt->descriptor->enable)(optpt, this, data, info);
}

/* This function is only called from sanei_hp_handle_startScan() */
SANE_Status
sanei_hp_optset_download (HpOptSet this, HpData data, HpScsi scsi)
{
  int i, errcount = 0;

  DBG(3, "Start downloading parameters to scanner\n");

  /* Reset scanner to wake it up */

  /* Reset would switch off XPA lamp and switch on scanner lamp. */
  /* Only do a reset if not in active XPA mode */
  if (  (sanei_hp_optset_scan_type (this, data) != SCL_XPA_SCAN)
      || (!sanei_hp_is_active_xpa (scsi)) )
  {
    RETURN_IF_FAIL(sanei_hp_scl_reset (scsi));
  }
  RETURN_IF_FAIL(sanei_hp_scl_clearErrors (scsi));

  sanei_hp_device_simulate_clear ( sanei_hp_scsi_devicename (scsi) );

  for (i = 0; i < (int)this->num_opts; i++)
  {
      if ( (this->options[i])->descriptor->suppress_for_scan )
      {
        DBG(3,"sanei_hp_optset_download: %s suppressed for scan\n",
            (this->options[i])->descriptor->name);
      }
      else
      {
        RETURN_IF_FAIL( hp_option_program(this->options[i], scsi, this, data) );

        if ( sanei_hp_scl_errcheck (scsi) != SANE_STATUS_GOOD )
        {
          errcount++;
          DBG(3, "Option %s generated scanner error\n",
              this->options[i]->descriptor->name);

          RETURN_IF_FAIL(sanei_hp_scl_clearErrors (scsi));
        }
      }
  }
  DBG(3, "Downloading parameters finished.\n");

  /* Check preview */
  {HpOption option;
   int is_preview, data_width;
   const HpDeviceInfo *info;

   option = hp_optset_getByName (this, SANE_NAME_PREVIEW);
   if ( option )
   {
     is_preview = hp_option_getint (option, data);
     if ( is_preview )
     {
       /* For preview we only use 8 bit per channel */

       DBG(3, "sanei_hp_optset_download: Set up preview options\n");

       info = sanei_hp_device_info_get ( sanei_hp_scsi_devicename  (scsi) );

       if (hp_optset_isEnabled (this, data, SANE_NAME_BIT_DEPTH, info))
       {
         data_width = sanei_hp_optset_data_width (this, data);
         if (data_width > 24)
         {
           sanei_hp_scl_set(scsi, SCL_DATA_WIDTH, 24);
         }
         else if ((data_width > 8) && (data_width <= 16))
         {
           sanei_hp_scl_set(scsi, SCL_DATA_WIDTH, 8);
         }
       }
     }
   }
  }

  return SANE_STATUS_GOOD;
}

SANE_Status
sanei_hp_optset_new(HpOptSet * newp, HpScsi scsi, HpDevice dev)
{
  HpOptionDescriptor *	ptr;
  HpOptSet		this = sanei_hp_allocz(sizeof(*this));
  SANE_Status		status;
  HpOption 		option;
  const HpDeviceInfo   *info;

  if (!this)
      return SANE_STATUS_NO_MEM;

  /* FIXME: more DBG's */
  for (ptr = hp_options; *ptr; ptr++)
    {
      HpOptionDescriptor desc = *ptr;

      DBG(8, "sanei_hp_optset_new: %s\n", desc->name);
      if (desc->requires && !sanei_hp_device_compat(dev, desc->requires))
	  continue;
      if (desc->type != SANE_TYPE_GROUP
	  && hp_optset_getByName(this, desc->name))
	  continue;

      status = hp_option_descriptor_probe(desc, scsi, this,
					  dev->data, &option);
      if (UNSUPPORTED(status))
	  continue;
      if (FAILED(status))
	{
	  DBG(1, "Option '%s': probe failed: %s\n", desc->name,
	      sane_strstatus(status));
	  sanei_hp_free(this);
	  return status;
	}
      hp_optset_add(this, option);
    }

  /* Set NUM_OPTIONS */
  assert(this->options[0]->descriptor == NUM_OPTIONS);
  sanei_hp_accessor_setint(this->options[0]->data_acsr, dev->data,
		     this->num_sane_opts);

  /* Now for some kludges */
  status = hp_optset_fix_geometry_options(this);
  if (FAILED(status))
    {
      sanei_hp_free(this);
      return status;
    }

  info = sanei_hp_device_info_get ( sanei_hp_scsi_devicename  (scsi) );
  hp_optset_updateEnables(this, dev->data, info);

  *newp = this;
  return SANE_STATUS_GOOD;
}

hp_bool_t
sanei_hp_optset_isImmediate (HpOptSet this, int optnum)
{
  HpOption	opt  = hp_optset_getByIndex(this, optnum);

  if (!opt)
      return 0;

  return hp_option_isImmediate (opt);
}

SANE_Status
sanei_hp_optset_control (HpOptSet this, HpData data,
                         int optnum, SANE_Action action,
                         void * valp, SANE_Int *infop, HpScsi scsi,
                         hp_bool_t immediate)
{
  HpOption	opt  = hp_optset_getByIndex(this, optnum);
  SANE_Int	my_info = 0, my_val = 0;

  DBG(3,"sanei_hp_optset_control: %s\n", opt ? opt->descriptor->name : "");

  if (infop)
      *infop = 0;
  else
      infop = &my_info;

  if (!opt)
      return SANE_STATUS_INVAL;

  /* There are problems with SANE_ACTION_GET_VALUE and valp == 0. */
  /* Check if we really need valp. */
  if ((action == SANE_ACTION_GET_VALUE) && (!valp))
  {
    /* Options without a value ? */
    if (   (opt->descriptor->type == SANE_TYPE_BUTTON)
        || (opt->descriptor->type == SANE_TYPE_GROUP))
    {
      valp = &my_val; /* Just simulate a return value locally. */
    }
    else /* Others must return a value. So this is invalid */
    {
      DBG(1, "sanei_hp_optset_control: get value, but valp == 0\n");
      return SANE_STATUS_INVAL;
    }
  }

  if (immediate)
    RETURN_IF_FAIL( hp_option_imm_control(this, opt, data, action, valp, infop,
                                          scsi) );
  else
    RETURN_IF_FAIL( hp_option_control(opt, data, action, valp, infop ) );

  if ((*infop & SANE_INFO_RELOAD_OPTIONS) != 0)
  {const HpDeviceInfo *info;

      DBG(3,"sanei_hp_optset_control: reprobe\n");

      /* At first we try to reprogram the parameters that may have changed */
      /* by an option that had a global effect.  This is necessary to */
      /* specify options in an arbitrary order. Example: */
      /* Changing scan mode resets scan resolution in the scanner. */
      /* If resolution is set from the API before scan mode, we must */
      /* reprogram the resolution afterwards. */
      hp_optset_reprogram(this, data, scsi);
      hp_optset_reprobe(this, data, scsi);
      info = sanei_hp_device_info_get ( sanei_hp_scsi_devicename  (scsi) );
      hp_optset_updateEnables(this, data, info);
  }

  return SANE_STATUS_GOOD;
}

SANE_Status
sanei_hp_optset_guessParameters (HpOptSet this, HpData data,
                                 SANE_Parameters * p)
{
  /* These are magic accessors which actually get the extent, not the
   * absolute position... */
  int xextent = sanei_hp_accessor_getint(this->br_x, data);
  int yextent = sanei_hp_accessor_getint(this->br_y, data);
  int data_width;

  assert(xextent > 0 && yextent > 0);
  p->last_frame = SANE_TRUE;
  p->pixels_per_line = xextent;
  p->lines           = yextent;

  switch (sanei_hp_optset_scanmode(this, data)) {
  case HP_SCANMODE_LINEART: /* Lineart */
  case HP_SCANMODE_HALFTONE: /* Halftone */
      p->format = SANE_FRAME_GRAY;
      p->depth  = 1;
      p->bytes_per_line  = (p->pixels_per_line + 7) / 8;
      break;
  case HP_SCANMODE_GRAYSCALE: /* Grayscale */
      p->format = SANE_FRAME_GRAY;
      p->depth  = 8;
      p->bytes_per_line  = p->pixels_per_line;
      if ( !sanei_hp_optset_output_8bit (this, data) )
      {
        data_width = sanei_hp_optset_data_width (this, data);
        if ( data_width > 8 )
        {
          p->depth *= 2;
          p->bytes_per_line *= 2;
        }
      }
      break;
  case HP_SCANMODE_COLOR: /* RGB */
      p->format = SANE_FRAME_RGB;
      p->depth = 8;
      p->bytes_per_line  = 3 * p->pixels_per_line;
      if ( !sanei_hp_optset_output_8bit (this, data) )
      {
        data_width = sanei_hp_optset_data_width (this, data);
        if ( data_width > 24 )
        {
          p->depth *= 2;
          p->bytes_per_line *= 2;
        }
      }
      break;
  default:
      assert(!"Bad scan mode?");
      return SANE_STATUS_INVAL;
  }

  return SANE_STATUS_GOOD;
}

const SANE_Option_Descriptor *
sanei_hp_optset_saneoption (HpOptSet this, HpData data, int optnum)
{
  HpOption opt = hp_optset_getByIndex(this, optnum);

  if (!opt)
      return 0;
  return hp_option_saneoption(opt, data);
}
