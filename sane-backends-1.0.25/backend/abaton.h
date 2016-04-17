/* sane - Scanner Access Now Easy.

   Copyright (C) 1998 David Huggins-Daines, heavily based on the Apple
   scanner driver (since Abaton scanners are very similar to old Apple
   scanners), which is (C) 1998 Milon Firikis, which is, in turn, based
   on the Mustek driver, (C) 1996-7 David Mosberger-Tang.

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

#ifndef abaton_h
#define abaton_h

#include <sys/types.h>

enum Abaton_Modes
  {
    ABATON_MODE_LINEART=0,
    ABATON_MODE_HALFTONE,
    ABATON_MODE_GRAY
  };
  
enum Abaton_Option
  {
    OPT_NUM_OPTS = 0,

    OPT_MODE_GROUP,
    OPT_MODE,
    OPT_X_RESOLUTION,
    OPT_Y_RESOLUTION,
    OPT_RESOLUTION_BIND,
    OPT_PREVIEW,
    OPT_HALFTONE_PATTERN,
    
    OPT_GEOMETRY_GROUP,
    OPT_TL_X,			/* top-left x */
    OPT_TL_Y,			/* top-left y */
    OPT_BR_X,			/* bottom-right x */
    OPT_BR_Y,			/* bottom-right y */

    OPT_ENHANCEMENT_GROUP,
    OPT_BRIGHTNESS,
    OPT_CONTRAST,
    OPT_THRESHOLD,
    OPT_NEGATIVE,
    OPT_MIRROR,
    
    /* must come last: */
    NUM_OPTIONS
  };

enum ScannerModels
{
  ABATON_300GS,
  ABATON_300S
};

typedef struct Abaton_Device
  {
    struct Abaton_Device *next;
    SANE_Int ScannerModel;
    SANE_Device sane;
    SANE_Range dpi_range;
    unsigned flags;
  }
Abaton_Device;

typedef struct Abaton_Scanner
  {
    /* all the state needed to define a scan request: */
    struct Abaton_Scanner *next;

    SANE_Option_Descriptor opt[NUM_OPTIONS];
    Option_Value val[NUM_OPTIONS];

    SANE_Bool scanning;
    SANE_Bool AbortedByUser;
 
    SANE_Parameters params;

    /* The actual bpp, before "Pseudo-8-bit" fiddling */
    SANE_Int bpp;

    /* window, in pixels */
    SANE_Int ULx;
    SANE_Int ULy;
    SANE_Int Width;
    SANE_Int Height;

    int fd;			/* SCSI filedescriptor */

    /* scanner dependent/low-level state: */
    Abaton_Device *hw;

  }
Abaton_Scanner;

#endif /* abaton_h */
