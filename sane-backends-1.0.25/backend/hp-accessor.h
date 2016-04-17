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

#ifndef HP_ACCESSOR_H_INCLUDED
#define HP_ACCESSOR_H_INCLUDED
#include "hp.h"

HpData sanei_hp_data_new (void);
HpData sanei_hp_data_dup (HpData orig);
void   sanei_hp_data_destroy (HpData this);

HpAccessor sanei_hp_accessor_new       (HpData data, size_t size);
HpAccessor sanei_hp_accessor_int_new   (HpData data);
HpAccessor sanei_hp_accessor_bool_new  (HpData data);
HpAccessor sanei_hp_accessor_fixed_new (HpData data);
HpAccessor sanei_hp_accessor_choice_new(HpData data, HpChoice choices,
                                        hp_bool_t may_change);
HpAccessor sanei_hp_accessor_vector_new(HpData data,
                                        unsigned length, unsigned depth);
HpAccessor sanei_hp_accessor_gamma_vector_new(HpData data,
                                        unsigned length, unsigned depth);
HpAccessor sanei_hp_accessor_matrix_vector_new(HpData data,
                                        unsigned length, unsigned depth);
HpAccessor sanei_hp_accessor_subvector_new(HpAccessorVector super,
                                        unsigned nchan, unsigned chan);

HpAccessor sanei_hp_accessor_geometry_new (HpAccessor val, HpAccessor lim,
                                        hp_bool_t is_br, HpAccessor res);

SANE_Status sanei_hp_accessor_get   (HpAccessor this, HpData data, void * valp);
SANE_Status sanei_hp_accessor_set   (HpAccessor this, HpData data, void * valp);
int	    sanei_hp_accessor_getint(HpAccessor this, HpData data);
void	    sanei_hp_accessor_setint(HpAccessor this, HpData data, int v);
const void *sanei_hp_accessor_data  (HpAccessor this, HpData data);
void *	    sanei__hp_accessor_data (HpAccessor this, HpData data);
size_t      sanei_hp_accessor_size  (HpAccessor this);

unsigned    sanei_hp_accessor_vector_length (HpAccessorVector this);
SANE_Fixed  sanei_hp_accessor_vector_minval (HpAccessorVector this);
SANE_Fixed  sanei_hp_accessor_vector_maxval (HpAccessorVector this);

SANE_Int    sanei_hp_accessor_choice_maxsize (HpAccessorChoice this);
SANE_String_Const *
  sanei_hp_accessor_choice_strlist (HpAccessorChoice this, HpOptSet optset,
                                    HpData data, const HpDeviceInfo *info);

#endif /* HP_ACCESSOR_H_INCLUDED */
