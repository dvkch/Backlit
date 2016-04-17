/* sane - Scanner Access Now Easy.

   Copyright (C) 2002 Sergey Vlasov <vsu@altlinux.ru>
   
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
*/

#ifndef GT68XX_GENERIC_H
#define GT68XX_GENERIC_H

#include "gt68xx_low.h"

static SANE_Status
gt68xx_generic_move_relative (GT68xx_Device * dev, SANE_Int distance);

static SANE_Status gt68xx_generic_start_scan (GT68xx_Device * dev);

static SANE_Status
gt68xx_generic_read_scanned_data (GT68xx_Device * dev, SANE_Bool * ready);

static SANE_Status
gt68xx_generic_set_afe (GT68xx_Device * dev, GT68xx_AFE_Parameters * params);

static SANE_Status
gt68xx_generic_set_exposure_time (GT68xx_Device * dev,
				  GT68xx_Exposure_Parameters * params);
static SANE_Status gt68xx_generic_get_id (GT68xx_Device * dev);
static SANE_Status gt68xx_generic_paperfeed (GT68xx_Device * dev);

static SANE_Status
gt68xx_generic_setup_scan (GT68xx_Device * dev,
			   GT68xx_Scan_Request * request,
			   GT68xx_Scan_Action action,
			   GT68xx_Scan_Parameters * params);
static SANE_Status
gt68xx_generic_move_paper (GT68xx_Device * dev,
			   GT68xx_Scan_Request * request);

#endif /* not GT68XX_GENERIC_H */
