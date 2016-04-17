/* sane - Scanner Access Now Easy.
   Artec AS6E backend.
   Copyright (C) 2000 Eugene S. Weiss
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

   This file implements a backend for the Artec AS6E by making a bridge
   to the as6edriver program.  The as6edriver program can be found at
   http://as6edriver.sourceforge.net .  */

#ifndef as6e_h
#define as6e_h

#include <sys/stat.h>
#include <sys/types.h>
#include "../include/sane/sane.h"


typedef enum
{
	OPT_NUM_OPTS = 0,
	OPT_MODE,
	OPT_RESOLUTION,

	OPT_TL_X,			/* top-left x */
	OPT_TL_Y,			/* top-left y */
	OPT_BR_X,			/* bottom-right x */
	OPT_BR_Y,			/* bottom-right y */

	OPT_BRIGHTNESS,
	OPT_CONTRAST,

	/* must come last */
	NUM_OPTIONS
  } AS6E_Option;

typedef struct
{
	int color;
	int resolution;
	int startpos;
	int stoppos;
	int startline;
	int stopline;
	int ctloutpipe;
	int ctlinpipe;
	int datapipe;
} AS6E_Params;


typedef struct AS6E_Device
{
	struct AS6E_Device *next;
	SANE_Device sane;
} AS6E_Device;



typedef struct AS6E_Scan
{
	struct AS6E_Scan *next;
	SANE_Option_Descriptor options_list[NUM_OPTIONS];
	Option_Value value[NUM_OPTIONS];
	SANE_Bool scanning;
	SANE_Bool cancelled;
	SANE_Parameters sane_params;
	AS6E_Params	as6e_params;
	pid_t child_pid;
	size_t bytes_to_read;
	SANE_Byte *scan_buffer;
	SANE_Byte *line_buffer;
	SANE_Word scan_buffer_count;
	SANE_Word image_counter;
} AS6E_Scan;


#ifndef PATH_MAX
#define PATH_MAX	1024
#endif

#define AS6E_CONFIG_FILE "as6e.conf"

#define READPIPE 0
#define WRITEPIPE 1


#define SCAN_BUF_SIZE 32768

#endif /* as6e_h */
