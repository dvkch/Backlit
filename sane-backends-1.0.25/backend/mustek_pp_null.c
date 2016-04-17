/* sane - Scanner Access Now Easy.
   Copyright (C) 2000-2003 Jochen Eisinger <jochen.eisinger@gmx.net>
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

   This file implements a SANE backend for Mustek PP flatbed scanners.  */
   
#include "../include/sane/config.h"

#if defined(HAVE_STDLIB_H)
# include <stdlib.h>
#endif
#include <ctype.h>
#include <stdio.h>
#if defined(HAVE_STRING_H)
# include <string.h>
#elif defined(HAVE_STRINGS_H)
# include <strings.h>
#endif

#define DEBUG_DECLARE_ONLY

#include "mustek_pp.h"
#include "mustek_pp_decl.h"
#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"

#define MUSTEK_PP_NULL_DRIVER	0

static SANE_Status 
debug_drv_init(SANE_Int options, SANE_String_Const port,
		SANE_String_Const name, SANE_Attach_Callback attach)
{

	if (options != CAP_NOTHING)
		return SANE_STATUS_INVAL;

	return attach(port, name, MUSTEK_PP_NULL_DRIVER, 0);

}

/*ARGSUSED*/
static void 
debug_drv_capabilities(SANE_Int info __UNUSED__, SANE_String *model,
                            SANE_String *vendor, SANE_String *type,
                            SANE_Int *maxres, SANE_Int *minres,
                            SANE_Int *maxhsize, SANE_Int *maxvsize,
                            SANE_Int *caps)
{

	*model = strdup("debugger");
	*vendor = strdup("mustek_pp");
	*type = strdup("software emulated");
	*maxres = 300;
	*minres = 50;
	*maxhsize = 1000;
	*maxvsize = 3000;
	*caps = CAP_NOTHING;

}

/*ARGSUSED*/
static SANE_Status
debug_drv_open (SANE_String port __UNUSED__, 
			    SANE_Int caps __UNUSED__, SANE_Int *fd)
{
	*fd = 1;
	return SANE_STATUS_GOOD;
}

static void 
debug_drv_setup (SANE_Handle hndl)
{

	Mustek_pp_Handle *dev = hndl;

	dev->lamp_on = 0;
	dev->priv = NULL;
}

/*ARGSUSED*/
static SANE_Status
debug_drv_config(SANE_Handle hndl __UNUSED__, 
			     SANE_String_Const optname,
			     SANE_String_Const optval)
{
	DBG (3, "debug_drv cfg option: %s=%s\n", optname, optval ? optval : "");
	return SANE_STATUS_GOOD;
}

/*ARGSUSED*/
static void
debug_drv_close (SANE_Handle hndl __UNUSED__)
{
}

/*ARGSUSED*/
static SANE_Status
debug_drv_start (SANE_Handle hndl __UNUSED__)
{
	return SANE_STATUS_GOOD;
}

static void 
debug_drv_read (SANE_Handle hndl, SANE_Byte *buffer)
{

	Mustek_pp_Handle *dev = hndl;

	memset (buffer, 0, dev->params.bytes_per_line);
}

/*ARGSUSED*/
static void
debug_drv_stop (SANE_Handle hndl __UNUSED__)
{

}
