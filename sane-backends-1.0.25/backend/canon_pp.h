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

   canon_pp.h: $Revision$

   This file is part of the canon_pp backend, supporting Canon FBX30P 
   and NX40P scanners

*/


#ifndef CANON_PARALLEL_H

#define CANON_PARALLEL_H

#ifdef BACKEND_NAME
#undef BACKEND_NAME
#define BACKEND_NAME canon_pp
#endif

#define DEBUG_NOT_STATIC
#include "../include/sane/sanei_debug.h"

#ifndef PACKAGE
#define PACKAGE "Canon Parallel SANE Backend"
#endif

#define CMODE_COLOUR "Colour"
#define CMODE_MONO "Mono"
#define CANONP_CONFIG_FILE "canon_pp.conf"
/* options: num,res,colour,depth,tl-x,tl-y,br-x,br-y,cal */
/* preview option disabled */
#define NUM_OPTIONS 9  
#define BUF_MAX 64000

/* Indexes into options array */
#define OPT_NUM_OPTIONS 0
#define OPT_RESOLUTION 1
#define OPT_COLOUR_MODE 2
#define OPT_DEPTH 3
#define OPT_TL_X 4
#define OPT_TL_Y 5
#define OPT_BR_X 6
#define OPT_BR_Y 7
#define OPT_CAL 8
#define OPT_PREVIEW 9
#if 0
#define OPT_GAMMA_R 10
#define OPT_GAMMA_G 11
#define OPT_GAMMA_B 12
#endif
/*#define OPT_GAMMA 13*/

typedef struct CANONP_Scanner_Struct CANONP_Scanner;

struct CANONP_Scanner_Struct
{
	CANONP_Scanner *next;
	SANE_Device hw;
	SANE_Option_Descriptor opt[NUM_OPTIONS];
	SANE_Int vals[NUM_OPTIONS];
	SANE_Bool opened;
	SANE_Bool scanning;
	SANE_Bool sent_eof;
	SANE_Bool cancelled;
	SANE_Bool setup;
	SANE_Int lines_scanned;
	SANE_Int bytes_sent;

	char *weights_file;
	SANE_Bool cal_readonly;
	SANE_Bool cal_valid;

	scanner_parameters params;
	scan_parameters scan;

	int ieee1284_mode;
	int init_mode;

	SANE_Bool scanner_present;

};


#endif

