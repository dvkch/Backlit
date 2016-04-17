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

#ifndef HP_DEVICE_INCLUDED
#define HP_DEVICE_INCLUDED
#include "hp.h"

enum hp_device_compat_e {
    HP_COMPAT_PLUS 	= 1 << 0, /* HP ScanJet Plus */
    HP_COMPAT_2C	= 1 << 1,
    HP_COMPAT_2P	= 1 << 2,
    HP_COMPAT_2CX	= 1 << 3,
    HP_COMPAT_4C	= 1 << 4, /* also 3c, 6100C */
    HP_COMPAT_3P	= 1 << 5,
    HP_COMPAT_4P	= 1 << 6,
    HP_COMPAT_5P	= 1 << 7, /* also 4100C, 5100C */
    HP_COMPAT_5100C	= 1 << 8, /* redundant with 5p */
    HP_COMPAT_PS	= 1 << 9, /* HP PhotoSmart Photo Scanner */
    HP_COMPAT_OJ_1150C	= 1 << 10,
    HP_COMPAT_OJ_1170C	= 1 << 11, /* also later OfficeJets */
    HP_COMPAT_6200C	= 1 << 12,
    HP_COMPAT_5200C	= 1 << 13,
    HP_COMPAT_6300C	= 1 << 14
};

struct hp_device_s
{
    HpData	data;
    HpOptSet	options;
    SANE_Device	sanedev;
    enum hp_device_compat_e compat;
};

SANE_Status	sanei_hp_device_new (HpDevice * new, const char * devname);

const SANE_Device * sanei_hp_device_sanedevice (HpDevice this);

void            sanei_hp_device_simulate_clear (const char *devname);
SANE_Status     sanei_hp_device_simulate_set (const char *devname, HpScl scl,
                                              int flag);
SANE_Status     sanei_hp_device_support_get (const char *devname, HpScl scl,
                                             int *minval, int *maxval);
SANE_Status     sanei_hp_device_support_probe (HpScsi scsi);
SANE_Status     sanei_hp_device_probe_model (enum hp_device_compat_e *compat,
                                             HpScsi scsi, int *model_num,
                                             const char **model_name);
SANE_Status     sanei_hp_device_probe (enum hp_device_compat_e *compat,
                                       HpScsi scsi);
hp_bool_t	sanei_hp_device_compat (HpDevice this,
                                        enum hp_device_compat_e c);


#endif /*  HP_DEVICE_INCLUDED */
