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

#ifndef GT68XX_SHM_CHANNEL_H
#define GT68XX_SHM_CHANNEL_H

/** @file
 * @brief Shared memory channel support.
 */

#include "../include/sane/sane.h"

typedef struct Shm_Channel Shm_Channel;

static SANE_Status
shm_channel_new (SANE_Int buf_size,
		 SANE_Int buf_count, Shm_Channel ** shm_channel_return);

static SANE_Status shm_channel_free (Shm_Channel * shm_channel);


static SANE_Status shm_channel_writer_init (Shm_Channel * shm_channel);

static SANE_Status
shm_channel_writer_get_buffer (Shm_Channel * shm_channel,
			       SANE_Int * buffer_id_return,
			       SANE_Byte ** buffer_addr_return);

static SANE_Status
shm_channel_writer_put_buffer (Shm_Channel * shm_channel,
			       SANE_Int buffer_id, SANE_Int buffer_bytes);

static SANE_Status shm_channel_writer_close (Shm_Channel * shm_channel);


static SANE_Status shm_channel_reader_init (Shm_Channel * shm_channel);

#if 0
static SANE_Status
shm_channel_reader_set_io_mode (Shm_Channel * shm_channel,
				SANE_Bool non_blocking);

static SANE_Status
shm_channel_reader_get_select_fd (Shm_Channel * shm_channel,
				  SANE_Int * fd_return);

#endif

static SANE_Status shm_channel_reader_start (Shm_Channel * shm_channel);

static SANE_Status
shm_channel_reader_get_buffer (Shm_Channel * shm_channel,
			       SANE_Int * buffer_id_return,
			       SANE_Byte ** buffer_addr_return,
			       SANE_Int * buffer_bytes_return);

static SANE_Status
shm_channel_reader_put_buffer (Shm_Channel * shm_channel, SANE_Int buffer_id);

#if 0
static SANE_Status shm_channel_reader_close (Shm_Channel * shm_channel);
#endif

#endif /* not GT68XX_SHM_CHANNEL_H */

/* vim: set sw=2 cino=>2se-1sn-1s{s^-1st0(0u0 smarttab expandtab: */
