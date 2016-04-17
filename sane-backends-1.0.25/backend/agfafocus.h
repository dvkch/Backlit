/* sane - Scanner Access Now Easy.

   This file (C) 1997 Ingo Schneider
             (C) 1998 Karl Anders Øygard

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

#ifndef agfafocus_h
#define agfafocus_h

enum AgfaFocus_Scanner_Type
  {
    AGFAGRAY64,
    AGFALINEART,
    AGFAGRAY256,
    AGFACOLOR 
  };

typedef enum
  {
    LINEART,
    GRAY6BIT,
    GRAY8BIT,
    COLOR18BIT,
    COLOR24BIT
  }
AgfaFocus_Scanner_Mode;

enum AgfaFocus_Option
  {
    OPT_NUM_OPTS = 0,

    OPT_MODE_GROUP,
    OPT_MODE,
    OPT_HALFTONE_PATTERN,	/* halftone matrix */
    OPT_RESOLUTION,
    OPT_SOURCE,
    OPT_QUALITY,                /* quality calibration */

    OPT_GEOMETRY_GROUP,
    OPT_TL_X,			/* top-left x */
    OPT_TL_Y,			/* top-left y */
    OPT_BR_X,			/* bottom-right x */
    OPT_BR_Y,			/* bottom-right y */

    OPT_ENHANCEMENT_GROUP,
    OPT_EXPOSURE,
    OPT_BRIGHTNESS,
    OPT_CONTRAST,
    OPT_AUTO_BRIGHTNESS,
    OPT_AUTO_CONTRAST,
    OPT_ATTENUATION_RED,
    OPT_ATTENUATION_BLUE,
    OPT_ATTENUATION_GREEN,
    OPT_SHARPEN,		/* sharpening */

    /* must come last: */
    NUM_OPTIONS
  };


typedef struct AgfaFocus_Device
  {
    struct AgfaFocus_Device *next;
    SANE_Device sane;
    SANE_Handle handle;

    SANE_Word type;
    SANE_Bool transparent;
    SANE_Bool analoglog;
    SANE_Bool tos5;
    SANE_Bool quality;
    SANE_Bool disconnect;
    SANE_Bool upload_user_defines;
  }
AgfaFocus_Device;

typedef struct AgfaFocus_Scanner
  {
    /* all the state needed to define a scan request: */

    SANE_Option_Descriptor opt[NUM_OPTIONS];
    Option_Value val[NUM_OPTIONS];

    /* Parsed option values and variables that are valid only during
       actual scanning: */
    SANE_Bool scanning;
    SANE_Int pass;
    SANE_Parameters params;

    AgfaFocus_Scanner_Mode mode;

    SANE_Int image_composition;
    SANE_Int bpp;
    SANE_Int halftone;
    SANE_Int original;
    SANE_Int exposure;
    SANE_Int r_att;
    SANE_Int g_att;
    SANE_Int b_att;
    SANE_Int tonecurve;
    SANE_Int quality;
    SANE_Bool edge;
    SANE_Bool lin_log;

    int lines_available;	/* Lines in scanner memory */

    int fd;			/* SCSI filedescriptor */
    SANE_Pid reader_pid;		/* process id of reader */
    int pipe;			/* pipe to reader process */
    int reader_pipe;		/* pipe from reader process */

    /* scanner dependent/low-level state: */
    AgfaFocus_Device *hw;
  }
AgfaFocus_Scanner;

#endif /* agfafocus_h */
