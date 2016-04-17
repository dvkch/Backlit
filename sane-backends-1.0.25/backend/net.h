/* sane - Scanner Access Now Easy.
   Copyright (C) 1997 David Mosberger-Tang
   Copyright (C) 2003 Julien BLACHE <jb@jblache.org>
      AF-independent code + IPv6

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
   If you do not wish that, delete this exception notice.  */
#ifndef net_h
#define net_h

#include <sys/types.h>
#include <sys/socket.h>

#include "../include/sane/sanei_wire.h"
#include "../include/sane/config.h"

typedef struct Net_Device
  {
    struct Net_Device *next;
    const char *name;
#if defined (HAVE_GETADDRINFO) && defined (HAVE_GETNAMEINFO)
    struct addrinfo *addr;
    struct addrinfo *addr_used;
#else
    struct sockaddr addr;
#endif /* HAVE_GETADDRINFO && HAVE_GETNAMEINFO */
    int ctl;			/* socket descriptor (or -1) */
    Wire wire;
    int auth_active;
  }
Net_Device;

typedef struct Net_Scanner
  {
    /* all the state needed to define a scan request: */
    struct Net_Scanner *next;

    int options_valid;			/* are the options current? */
    SANE_Option_Descriptor_Array opt, local_opt;

    SANE_Word handle;		/* remote handle (it's a word, not a ptr!) */

    int data;			/* data socket descriptor */
    int reclen_buf_offset;
    u_char reclen_buf[4];
    size_t bytes_remaining;	/* how many bytes left in this record? */

    /* device (host) info: */
    Net_Device *hw;
  }
Net_Scanner;

#endif /* net_h */
