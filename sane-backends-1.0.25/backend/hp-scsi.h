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

#ifndef HP_SCSI_INCLUDED
#define HP_SCSI_INCLUDED

#define HP_SCSI_MAX_WRITE	(2048)
SANE_Status sanei_hp_nonscsi_new (HpScsi * newp, const char * devname,
                                  HpConnect connect);
SANE_Status sanei_hp_scsi_new	(HpScsi * newp, const char * devname);
void	    sanei_hp_scsi_destroy	(HpScsi this,int completely);

hp_byte_t * sanei_hp_scsi_inq        (HpScsi this);
const char *sanei_hp_scsi_model	     (HpScsi this);
const char *sanei_hp_scsi_vendor     (HpScsi this);
const char *sanei_hp_scsi_devicename (HpScsi this);

SANE_Status sanei_hp_scsi_pipeout    (HpScsi this, int outfd,
                                      HpProcessData *pdescr);

SANE_Status sanei_hp_scl_calibrate   (HpScsi scsi);
SANE_Status sanei_hp_scl_startScan   (HpScsi scsi, HpScl scl);
SANE_Status sanei_hp_scl_reset       (HpScsi scsi);
SANE_Status sanei_hp_scl_clearErrors (HpScsi scsi);
SANE_Status sanei_hp_scl_errcheck    (HpScsi scsi);

SANE_Status sanei_hp_scl_upload_binary (HpScsi scsi, HpScl scl,
                                        size_t *lengthhp, char **bufhp);
SANE_Status sanei_hp_scl_set    (HpScsi scsi, HpScl scl, int val);
SANE_Status sanei_hp_scl_inquire(HpScsi scsi, HpScl scl,
                                 int * valp, int * minp, int * maxp);
SANE_Status sanei_hp_scl_upload (HpScsi scsi, HpScl scl,
                                 void * buf, size_t sz);
SANE_Status sanei_hp_scl_download (HpScsi scsi, HpScl scl,
                                   const void * buf, size_t sz);

#endif /* HP_SCSI_INCLUDED */
