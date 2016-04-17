/* sane - Scanner Access Now Easy.

   This file (C) 1997 Ingo Schneider

   This file is part of the SANE package.

   SANE is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2 of the License, or (at your
   option) any later version.

   SANE is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License
   along with sane; see the file COPYING.  If not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */
#ifndef s9036_h
#define s9036_h

enum S9036_Option
  {
    OPT_NUM_OPTS = 0,

    OPT_MODE_GROUP,
    OPT_DEPTH,
    OPT_RESOLUTION,

    OPT_GEOMETRY_GROUP,
    OPT_TL_X,			/* top-left x */
    OPT_TL_Y,			/* top-left y */
    OPT_BR_X,			/* bottom-right x */
    OPT_BR_Y,			/* bottom-right y */

    OPT_ENHANCEMENT_GROUP,
    OPT_BRIGHTNESS,
    OPT_CONTRAST,
    OPT_BRIGHT_ADJUST,
    OPT_CONTR_ADJUST,

    /* must come last: */
    NUM_OPTIONS
  };

typedef struct S9036_Device
  {
    struct S9036_Device *next;
    SANE_Device sane;
    SANE_Handle handle;
  }
S9036_Device;

typedef struct S9036_Scanner
  {
    /* all the state needed to define a scan request: */

    SANE_Option_Descriptor opt[NUM_OPTIONS];
    SANE_Word val[NUM_OPTIONS];

    /* Parsed option values and variables that are valid only during
       actual scanning: */
    SANE_Bool scanning;
    SANE_Parameters params;

    size_t bufsize;		/* about SCSI_MAX_REQUEST_SIZE */
    SANE_Byte *buffer;		/* buffer of size 'bufsize' */
    SANE_Byte *bufstart;	/* Start of data for next read */
    size_t in_buffer;		/* bytes already in buffer */

    int lines_in_scanner;	/* Lines in scanner memory */
    int lines_read;		/* Total lines read for now */

    int fd;			/* SCSI filedescriptor */

    /* scanner dependent/low-level state: */
    S9036_Device *hw;

  }
S9036_Scanner;

#endif /* s9036_h */
