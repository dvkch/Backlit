/* sane - Scanner Access Now Easy.
   Copyright (C) 2001-2002 Matthew C. Duggan and Simon Krix
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

   -----

   canon_pp-io.h: $Revision$

   This file is part of the canon_pp backend, supporting Canon FBX30P 
   and NX40P scanners and also distributed as part of the stand-alone
   driver.

   Low-level scanner interface
   */

#ifndef CANON_PP_IO_H
#define CANON_PP_IO_H	

/* Actual Interface */
void sanei_canon_pp_set_ieee1284_mode(int m);
int sanei_canon_pp_wake_scanner(struct parport *port, int mode);
int sanei_canon_pp_write(struct parport *port, int length, 
		unsigned char *data);
int sanei_canon_pp_read(struct parport *port, int length, 
		unsigned char *data);
int sanei_canon_pp_check_status(struct parport *port);
int sanei_canon_pp_scanner_init(struct parport *port);

#endif
