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

/* #define STUBS
extern int sanei_debug_hp; */
#define DEBUG_DECLARE_ONLY

#include "../include/sane/config.h"
#include "../include/sane/sanei_backend.h"

#include "../include/lassert.h"
#include <string.h>

#include <sys/types.h>

#include "hp.h"
#include "hp-option.h"
#include "hp-accessor.h"
#include "hp-device.h"

#define DATA_SIZE_INCREMENT	(1024)

/*
 * class HpData
 */
struct hp_data_s
{
    hp_byte_t *	buf;
    size_t	bufsiz;
    size_t	length;
    hp_bool_t	frozen;
};


static void
hp_data_resize (HpData this, size_t newsize)
{

  if (this->bufsiz != newsize)
    {
      assert(!this->frozen);
      this->buf = sanei_hp_realloc(this->buf, newsize);
      assert(this->buf);
      this->bufsiz = newsize;
    }
}

static void
hp_data_freeze (HpData this)
{
  hp_data_resize(this, this->length);
  this->frozen = 1;
}

static size_t
hp_data_alloc (HpData this, size_t sz)
{
  size_t	newsize	= this->bufsiz;
  size_t	offset	= this->length;

 /*
  * mike@easysw.com:
  *
  * The following code is REQUIRED so that pointers, etc. aren't
  * misaligned.  This causes MAJOR problems on all SPARC, ALPHA,
  * and MIPS processors, and possibly others.
  *
  * The workaround is to ensure that all allocations are in multiples
  * of 8 bytes.
  */
  sz = (sz + sizeof (long) - 1) & ~(sizeof (long) - 1);

  while (newsize < this->length + sz)
      newsize += DATA_SIZE_INCREMENT;
  hp_data_resize(this, newsize);

  this->length += sz;
  return offset;
}

static void *
hp_data_data (HpData this, size_t offset)
{
  assert(offset < this->length);
  return (char *)this->buf + offset;
}

HpData
sanei_hp_data_new (void)
{
  return sanei_hp_allocz(sizeof(struct hp_data_s));
}

HpData
sanei_hp_data_dup (HpData orig)
{
  HpData new;

  hp_data_freeze(orig);
  if (!( new = sanei_hp_memdup(orig, sizeof(*orig)) ))
      return 0;
  if (!(new->buf = sanei_hp_memdup(orig->buf, orig->bufsiz)))
      {
	sanei_hp_free(new);
	return 0;
      }
  return new;
}

void
sanei_hp_data_destroy (HpData this)
{
  sanei_hp_free(this->buf);
  sanei_hp_free(this);
}


/*
 * class HpAccessor
 */

typedef const struct hp_accessor_type_s *	HpAccessorType;
typedef struct hp_accessor_s *			_HpAccessor;

struct hp_accessor_s
{
    HpAccessorType	type;
    size_t		data_offset;
    size_t		data_size;
};

struct hp_accessor_type_s
{
    SANE_Status (*get)(HpAccessor this, HpData data, void * valp);
    SANE_Status (*set)(HpAccessor this,  HpData data, void * valp);
    int	 (*getint)(HpAccessor this, HpData data);
    void (*setint)(HpAccessor this, HpData data, int val);
};

SANE_Status
sanei_hp_accessor_get (HpAccessor this, HpData data, void * valp)
{
  if (!this->type->get)
      return SANE_STATUS_INVAL;
  return (*this->type->get)(this, data, valp);
}

SANE_Status
sanei_hp_accessor_set (HpAccessor this,  HpData data, void * valp)
{
  if (!this->type->set)
      return SANE_STATUS_INVAL;
  return (*this->type->set)(this, data, valp);
}

int
sanei_hp_accessor_getint (HpAccessor this, HpData data)
{
  assert (this->type->getint);
  return (*this->type->getint)(this, data);
}

void
sanei_hp_accessor_setint (HpAccessor this, HpData data, int val)
{
  assert (this->type->setint);
  (*this->type->setint)(this, data, val);
}

const void *
sanei_hp_accessor_data (HpAccessor this, HpData data)
{
  return hp_data_data(data, this->data_offset);
}

void *
sanei__hp_accessor_data (HpAccessor this, HpData data)
{
  return hp_data_data(data, this->data_offset);
}

size_t
sanei_hp_accessor_size (HpAccessor this)
{
  return  this->data_size;
}

HpAccessor
sanei_hp_accessor_new (HpData data, size_t sz)
{
  static const struct hp_accessor_type_s type = {
      0, 0, 0, 0
  };
  _HpAccessor	 new = sanei_hp_alloc(sizeof(*new));
  new->type = &type;
  new->data_offset = hp_data_alloc(data, new->data_size = sz);
  return new;
}


/*
 * class HpAccessorInt
 */

#define hp_accessor_int_s	hp_accessor_s

typedef const struct hp_accessor_int_s *	HpAccessorInt;
typedef struct hp_accessor_int_s *		_HpAccessorInt;

static SANE_Status
hp_accessor_int_get (HpAccessor this, HpData data, void * valp)
{
  *(SANE_Int*)valp = *(int *)hp_data_data(data, this->data_offset);
  return SANE_STATUS_GOOD;
}

static SANE_Status
hp_accessor_int_set (HpAccessor this, HpData data, void * valp)
{
  *(int *)hp_data_data(data, this->data_offset) = *(SANE_Int*)valp;
  return SANE_STATUS_GOOD;
}

static int
hp_accessor_int_getint (HpAccessor this, HpData data)
{
  return *(int *)hp_data_data(data, this->data_offset);
}

static void
hp_accessor_int_setint (HpAccessor this, HpData data, int val)
{
  *(int *)hp_data_data(data, this->data_offset) = val;
}

HpAccessor
sanei_hp_accessor_int_new (HpData data)
{
  static const struct hp_accessor_type_s type = {
      hp_accessor_int_get, hp_accessor_int_set,
      hp_accessor_int_getint, hp_accessor_int_setint
  };
  _HpAccessorInt	 new = sanei_hp_alloc(sizeof(*new));
  new->type = &type;
  new->data_offset = hp_data_alloc(data, new->data_size = sizeof(int));
  return (HpAccessor)new;
}


/*
 * class HpAccessorBool
 */

#define hp_accessor_bool_s	hp_accessor_s

typedef const struct hp_accessor_bool_s *	HpAccessorBool;
typedef struct hp_accessor_bool_s *		_HpAccessorBool;

static SANE_Status
hp_accessor_bool_get (HpAccessor this, HpData data, void * valp)
{
  int val = *(int *)hp_data_data(data, this->data_offset);
  *(SANE_Bool*)valp = val ? SANE_TRUE : SANE_FALSE;
  return SANE_STATUS_GOOD;
}

static SANE_Status
hp_accessor_bool_set (HpAccessor this, HpData data, void * valp)
{
  int * datap = hp_data_data(data, this->data_offset);
  *datap = *(SANE_Bool*)valp == SANE_FALSE ? 0 : 1;
  return SANE_STATUS_GOOD;
}

HpAccessor
sanei_hp_accessor_bool_new (HpData data)
{
  static const struct hp_accessor_type_s type = {
      hp_accessor_bool_get, hp_accessor_bool_set,
      hp_accessor_int_getint, hp_accessor_int_setint
  };
  _HpAccessorBool	 new = sanei_hp_alloc(sizeof(*new));
  new->type = &type;
  new->data_offset = hp_data_alloc(data, new->data_size = sizeof(int));
  return (HpAccessor)new;
}


/*
 * class HpAccessorFixed
 */

#define hp_accessor_fixed_s	hp_accessor_s

typedef const struct hp_accessor_fixed_s *	HpAccessorFixed;
typedef struct hp_accessor_fixed_s *		_HpAccessorFixed;

static SANE_Status
hp_accessor_fixed_get (HpAccessor this, HpData data, void * valp)
{
  *(SANE_Fixed*)valp = *(SANE_Fixed *)hp_data_data(data, this->data_offset);
  return SANE_STATUS_GOOD;
}

static SANE_Status
hp_accessor_fixed_set (HpAccessor this, HpData data, void * valp)
{
  *(SANE_Fixed *)hp_data_data(data, this->data_offset) = *(SANE_Fixed*)valp;
  return SANE_STATUS_GOOD;
}

HpAccessor
sanei_hp_accessor_fixed_new (HpData data)
{
  static const struct hp_accessor_type_s type = {
      hp_accessor_fixed_get, hp_accessor_fixed_set, 0, 0
  };
  _HpAccessorFixed	 new = sanei_hp_alloc(sizeof(*new));
  new->type = &type;
  new->data_offset = hp_data_alloc(data, new->data_size = sizeof(SANE_Fixed));
  return (HpAccessor)new;
}


/*
 * class HpAccessorChoice
 */

typedef struct hp_accessor_choice_s *		_HpAccessorChoice;

struct hp_accessor_choice_s
{
    HpAccessorType	type;
    size_t		data_offset;
    size_t		data_size;

    HpChoice		choices;
    SANE_String_Const * strlist;
};

static SANE_Status
hp_accessor_choice_get (HpAccessor this, HpData data, void * valp)
{
  HpChoice choice = *(HpChoice *)hp_data_data(data, this->data_offset);
  strcpy(valp, choice->name);
  return SANE_STATUS_GOOD;
}

static SANE_Status
hp_accessor_choice_set (HpAccessor _this, HpData data, void * valp)
{
  HpAccessorChoice	this	= (HpAccessorChoice)_this;
  HpChoice 		choice;
  SANE_String_Const *	strlist = this->strlist;

  for (choice = this->choices; choice; choice = choice->next)
    {
      /* Skip choices which aren't in strlist. */
      if (!*strlist || strcmp(*strlist, choice->name) != 0)
	  continue;
      strlist++;

      if (strcmp((const char *)valp, choice->name) == 0)
	{
	  *(HpChoice *)hp_data_data(data, this->data_offset) = choice;
	  return SANE_STATUS_GOOD;
	}
    }

  return SANE_STATUS_INVAL;
}

static int
hp_accessor_choice_getint (HpAccessor this, HpData data)
{
  HpChoice choice = *(HpChoice *)hp_data_data(data, this->data_offset);
  return choice->val;
}

static void
hp_accessor_choice_setint (HpAccessor _this,  HpData data, int val)
{
  HpAccessorChoice	this	= (HpAccessorChoice)_this;
  HpChoice 		choice;
  HpChoice 		first_choice = 0;
  SANE_String_Const *	strlist = this->strlist;

  for (choice = this->choices; choice; choice = choice->next)
    {
      /* Skip choices which aren't in strlist. */
      if (!*strlist || strcmp(*strlist, choice->name) != 0)
	  continue;
      strlist++;

      if (!first_choice)
	  first_choice = choice; /* First enabled choice */

      if (choice->val == val)
	{
	  *(HpChoice *)hp_data_data(data, this->data_offset) = choice;
	  return;
	}
    }

  if (first_choice)
      *(HpChoice *)hp_data_data(data, this->data_offset) = first_choice;
  else
      assert(!"No choices to choose from?");
}

SANE_Int
sanei_hp_accessor_choice_maxsize (HpAccessorChoice this)
{
  HpChoice	choice;
  SANE_Int	size	= 0;

  for (choice = this->choices; choice; choice = choice->next)
      if ((SANE_Int)strlen(choice->name) >= size)
	  size = strlen(choice->name) + 1;
  return size;
}

SANE_String_Const *
sanei_hp_accessor_choice_strlist (HpAccessorChoice this,
			    HpOptSet optset, HpData data,
                            const HpDeviceInfo *info)
{
  if (optset)
    {
      int	old_val = hp_accessor_choice_getint((HpAccessor)this, data);
      HpChoice	choice;
      size_t	count	= 0;

      for (choice = this->choices; choice; choice = choice->next)
	  if (sanei_hp_choice_isEnabled(choice, optset, data, info))
	      this->strlist[count++] = choice->name;
      this->strlist[count] = 0;

      hp_accessor_choice_setint((HpAccessor)this, data, old_val);
    }

  return this->strlist;
}

HpAccessor
sanei_hp_accessor_choice_new (HpData data, HpChoice choices,
                              hp_bool_t may_change)
{
  static const struct hp_accessor_type_s type = {
      hp_accessor_choice_get, hp_accessor_choice_set,
      hp_accessor_choice_getint, hp_accessor_choice_setint
  };
  HpChoice	choice;
  size_t	count	= 0;
  _HpAccessorChoice this;

  if ( may_change ) data->frozen = 0;

  for (choice = choices; choice; choice = choice->next)
      count++;
  this = sanei_hp_alloc(sizeof(*this) + (count+1) * sizeof(*this->strlist));
  if (!this)
      return 0;

  this->type = &type;
  this->data_offset = hp_data_alloc(data, this->data_size = sizeof(HpChoice));
  this->choices = choices;
  this->strlist = (SANE_String_Const *)(this + 1);

  count = 0;
  for (choice = this->choices; choice; choice = choice->next)
      this->strlist[count++] = choice->name;
  this->strlist[count] = 0;

  return (HpAccessor)this;
}

/*
 * class HpAccessorVector
 */

typedef struct hp_accessor_vector_s *		_HpAccessorVector;

struct hp_accessor_vector_s
{
    HpAccessorType	type;
    size_t		data_offset;
    size_t		data_size;

    unsigned short	mask;
    unsigned short	length;
    unsigned short	offset;
    short		stride;

    unsigned short	(*unscale)(HpAccessorVector this, SANE_Fixed fval);
    SANE_Fixed		(*scale)(HpAccessorVector this, unsigned short val);

    SANE_Fixed		fmin;
    SANE_Fixed		fmax;

};

unsigned
sanei_hp_accessor_vector_length (HpAccessorVector this)
{
  return this->length;
}

SANE_Fixed
sanei_hp_accessor_vector_minval (HpAccessorVector this)
{
  return this->fmin;
}

SANE_Fixed
sanei_hp_accessor_vector_maxval (HpAccessorVector this)
{
  return this->fmax;
}

static unsigned short
_v_get (HpAccessorVector this, const unsigned char * data)
{
  unsigned short val;

  if (this->mask <= 255)
      val = data[0];
  else
#ifndef NotOrig
      val = (data[0] << 8) + data[1];
#else
      val = (data[1] << 8) + data[0];
#endif

  return val & this->mask;
}

static void
_v_set (HpAccessorVector this, unsigned char * data, unsigned short val)
{
  val &= this->mask;

  if (this->mask <= 255)
    {
      data[0] = (unsigned char)val;
    }
  else
    {
#ifndef NotOrig
      data[1] = (unsigned char)val;
      data[0] = (unsigned char)(val >> 8);
#else
      data[0] = (unsigned char)val;
      data[1] = (unsigned char)(val >> 8);
#endif
    }
}

static SANE_Status
hp_accessor_vector_get (HpAccessor _this, HpData d, void * valp)
{
  HpAccessorVector	this	= (HpAccessorVector)_this;
  SANE_Fixed *		ptr	= valp;
  const SANE_Fixed *	end	= ptr + this->length;
  const unsigned char *	data	= hp_data_data(d, this->data_offset);

  data += this->offset;

  while (ptr < end)
    {
      *ptr++ = (*this->scale)(this, _v_get(this, data));
      data += this->stride;
    }
  return SANE_STATUS_GOOD;
}

static SANE_Status
hp_accessor_vector_set (HpAccessor _this, HpData d, void * valp)
{
  HpAccessorVector	this	= (HpAccessorVector)_this;
  SANE_Fixed *		ptr	= valp;
  const SANE_Fixed *	end	= ptr + this->length;
  unsigned char *	data	= hp_data_data(d, this->data_offset);

  data += this->offset;

  while (ptr < end)
    {
      if (*ptr < this->fmin)
	  *ptr = this->fmin;
      if (*ptr > this->fmax)
	  *ptr = this->fmax;

      _v_set(this, data, (*this->unscale)(this, *ptr++));

      data += this->stride;
    }
  return SANE_STATUS_GOOD;
}

static unsigned short
_vector_unscale (HpAccessorVector this, SANE_Fixed fval)
{
  unsigned short max_val = this->mask;
  return (fval * max_val + SANE_FIX(0.5)) / SANE_FIX(1.0);
}

static SANE_Fixed
_vector_scale (HpAccessorVector this, unsigned short val)
{
  unsigned short max_val = this->mask;
  return (SANE_FIX(1.0) * val + max_val / 2) / max_val;
}

HpAccessor
sanei_hp_accessor_vector_new (HpData data, unsigned length, unsigned depth)
{
  static const struct hp_accessor_type_s type = {
      hp_accessor_vector_get, hp_accessor_vector_set, 0, 0
  };
  unsigned		width	= depth > 8 ? 2 : 1;
  _HpAccessorVector	new	= sanei_hp_alloc(sizeof(*new));

  if (!new)
      return 0;

  assert(depth > 0 && depth <= 16);
  assert(length > 0);

  new->type = &type;
  new->data_size   = length * width;
  new->data_offset = hp_data_alloc(data, new->data_size);

  new->mask  = ((unsigned)1 << depth) - 1;
  new->length = length;
  new->offset = 0;
  new->stride = width;

  new->scale   = _vector_scale;
  new->unscale = _vector_unscale;

  new->fmin = SANE_FIX(0.0);
  new->fmax = SANE_FIX(1.0);

  return (HpAccessor)new;
}

static unsigned short
_gamma_vector_unscale (HpAccessorVector UNUSEDARG this, SANE_Fixed fval)
{
  unsigned short unscaled = fval / SANE_FIX(1.0);
  if (unscaled > 255) unscaled = 255;
  unscaled = 255 - unscaled;  /* Dont know why. But this is how it works */

  return unscaled;
}

static SANE_Fixed
_gamma_vector_scale (HpAccessorVector UNUSEDARG this, unsigned short val)
{
  SANE_Fixed scaled;
  val = 255-val;     /* Dont know why. But this is how it works */
  scaled = val * SANE_FIX(1.0);

  return scaled;
}

HpAccessor
sanei_hp_accessor_gamma_vector_new (HpData data, unsigned length,
                                    unsigned depth)
{
  _HpAccessorVector	this	=
      ( (_HpAccessorVector) sanei_hp_accessor_vector_new(data, length, depth) );


  if (!this)
      return 0;

  this->offset += this->stride * (this->length - 1);
  this->stride = -this->stride;

  this->scale   = _gamma_vector_scale;
  this->unscale = _gamma_vector_unscale;

  this->fmin = SANE_FIX(0.0);
  this->fmax = SANE_FIX(255.0);

  return (HpAccessor)this;
}

static unsigned short
_matrix_vector_unscale (HpAccessorVector this, SANE_Fixed fval)
{
  unsigned short max_val  = this->mask >> 1;
  unsigned short sign_bit = this->mask & ~max_val;
  unsigned short sign	  = 0;

  if (fval == SANE_FIX(1.0))
      return sign_bit;

  if (fval < 0)
    {
      sign = sign_bit;
      fval = -fval;
    }
  return sign | ((fval * max_val + this->fmax / 2) / this->fmax);
}

static SANE_Fixed
_matrix_vector_scale (HpAccessorVector this, unsigned short val)
{
  unsigned short max_val  = this->mask >> 1;
  unsigned short sign_bit = this->mask & ~max_val;
  SANE_Fixed	 fval;

  if (val == sign_bit)
      return SANE_FIX(1.0);

  fval = (this->fmax * (val & max_val) + max_val / 2) / max_val;

  if ((val & sign_bit) != 0)
      fval = -fval;

  return fval;
}

HpAccessor
sanei_hp_accessor_matrix_vector_new (HpData data, unsigned length,
                                     unsigned depth)
{
  _HpAccessorVector	this	=
      ( (_HpAccessorVector) sanei_hp_accessor_vector_new(data, length, depth) );

  if (!this)
      return 0;

  this->scale   = _matrix_vector_scale;
  this->unscale = _matrix_vector_unscale;

  this->fmax = depth == 10 ? SANE_FIX(4.0) : SANE_FIX(2.0);
  this->fmax *= (this->mask >> 1);
  this->fmax >>= (depth - 1);
  this->fmin = - this->fmax;

  return (HpAccessor)this;
}

HpAccessor
sanei_hp_accessor_subvector_new (HpAccessorVector super,
			   unsigned nchan, unsigned chan)
{
  _HpAccessorVector	this	= sanei_hp_memdup(super, sizeof(*this));

  if (!this)
      return 0;

  assert(chan < nchan);
  assert(this->length % nchan == 0);

  this->length /= nchan;

  if (this->stride < 0)
      this->offset += (nchan - chan - 1) * this->stride;
  else
      this->offset += chan * this->stride;

  this->stride *= nchan;

  return (HpAccessor)this;
}

/*
 * class HpAccessorGeometry
 */

typedef const struct hp_accessor_geometry_s *	HpAccessorGeometry;
typedef struct hp_accessor_geometry_s *		_HpAccessorGeometry;

struct hp_accessor_geometry_s
{
    HpAccessorType	type;
    size_t		data_offset;
    size_t		data_size;

    HpAccessor		this;
    HpAccessor		other;
    hp_bool_t		is_br;
    HpAccessor		resolution;
};


static SANE_Status
hp_accessor_geometry_set (HpAccessor _this, HpData data, void * _valp)
{
  HpAccessorGeometry	this	= (HpAccessorGeometry)_this;
  SANE_Fixed *		valp	= _valp;
  SANE_Fixed		limit;

  sanei_hp_accessor_get(this->other, data, &limit);
  if (this->is_br ? *valp < limit : *valp > limit)
      *valp = limit;
  return sanei_hp_accessor_set(this->this, data, valp);
}

static int
_to_devpixels (SANE_Fixed val_mm, SANE_Fixed mm_per_pix)
{
  assert(val_mm >= 0);
  return (val_mm + mm_per_pix / 2) / mm_per_pix;
}

static int
hp_accessor_geometry_getint (HpAccessor _this, HpData data)
{
  HpAccessorGeometry	this	= (HpAccessorGeometry)_this;
  SANE_Fixed		this_val, other_val;
  int			res	= sanei_hp_accessor_getint(this->resolution,
                                                           data);
  SANE_Fixed		mm_per_pix = (SANE_FIX(MM_PER_INCH) + res / 2) / res;

  assert(res > 0);
  sanei_hp_accessor_get(this->this, data, &this_val);

  if (this->is_br)
    {
      /* Convert to extent. */
      sanei_hp_accessor_get(this->other, data, &other_val);
      assert(this_val >= other_val && other_val >= 0);
      return (_to_devpixels(this_val, mm_per_pix)
	      - _to_devpixels(other_val, mm_per_pix) + 1);
    }
  return _to_devpixels(this_val, mm_per_pix);
}

/*
 * we should implement hp_accessor_geometry_setint, but we don't
 * need it yet...
 */


HpAccessor
sanei_hp_accessor_geometry_new (HpAccessor val, HpAccessor lim, hp_bool_t is_br,
			  HpAccessor resolution)
{
  static const struct hp_accessor_type_s type = {
      hp_accessor_fixed_get, hp_accessor_geometry_set,
      hp_accessor_geometry_getint, 0
  };
  _HpAccessorGeometry new = sanei_hp_alloc(sizeof(*new));

  new->type = &type;
  new->data_offset = val->data_offset;
  new->data_size   = val->data_size;

  new->this  = val;
  new->other = lim;
  new->is_br = is_br;
  new->resolution = resolution;

  return (HpAccessor)new;
}
