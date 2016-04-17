/* sane - Scanner Access Now Easy.
   Copyright (C) 1996, 1997 David Mosberger-Tang
   Copyright (C) 1997 R.E.Wolff@BitWizard.nl

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

   */

#ifndef tamarack_h
#define tamarack_h

#include <sys/types.h>

#define TAMARACK_FLAG_TA	(1 << 3)	/* transparency adapter */


enum Tamarack_Option
  {
    OPT_NUM_OPTS = 0,

    OPT_MODE_GROUP,
    OPT_MODE,
#define OPT_MODE_DEFAULT 2
    OPT_RESOLUTION,
#define OPT_RESOLUTION_DEFAULT 100
#if 0
    OPT_SPEED,
    OPT_SOURCE,
    OPT_BACKTRACK,
#endif
    OPT_PREVIEW,
    OPT_GRAY_PREVIEW,

    OPT_GEOMETRY_GROUP,
    OPT_TL_X,			/* top-left x */
    OPT_TL_Y,			/* top-left y */
    OPT_BR_X,			/* bottom-right x */
    OPT_BR_Y,			/* bottom-right y */

    OPT_ENHANCEMENT_GROUP,
    OPT_TRANS,
    OPT_BRIGHTNESS,
    OPT_CONTRAST,
    OPT_THRESHOLD,
#if 0
    OPT_CUSTOM_GAMMA,		/* use custom gamma tables? */
    /* The gamma vectors MUST appear in the order gray, red, green,
       blue.  */
    OPT_GAMMA_VECTOR,
    OPT_GAMMA_VECTOR_R,
    OPT_GAMMA_VECTOR_G,
    OPT_GAMMA_VECTOR_B,

    OPT_HALFTONE_DIMENSION,
    OPT_HALFTONE_PATTERN,
#endif

    /* must come last: */
    NUM_OPTIONS
  };



typedef struct Tamarack_Device
  {
    struct Tamarack_Device *next;
    SANE_Device sane;
    SANE_Range dpi_range;
    SANE_Range x_range;
    SANE_Range y_range;
    unsigned flags;
  }

Tamarack_Device;

typedef struct Tamarack_Scanner
  {
    /* all the state needed to define a scan request: */
    struct Tamarack_Scanner *next;

    SANE_Option_Descriptor opt[NUM_OPTIONS];
    Option_Value val[NUM_OPTIONS];
    SANE_Int gamma_table[4][256];
#if 0
    SANE_Int halftone_pattern[64];
#endif

    int scanning;
    int pass;			/* pass number */
    int line;			/* current line number */
    SANE_Parameters params;

    /* Parsed option values and variables that are valid only during
       actual scanning: */
    int mode;
#if 0
    int one_pass_color_scan;
    int resolution_code;
#endif
    int fd;			/* SCSI filedescriptor */
    SANE_Pid reader_pid;	/* process id of reader */
    int pipe;			/* pipe to reader process */
    int reader_pipe;		/* pipe from reader process */

    /* scanner dependent/low-level state: */
    Tamarack_Device *hw;

#if 0
    /* line-distance correction related state: */
    struct
      {
	int max_value;
	int peak_res;
	struct
	  {
	    int dist;		/* line distance */
	    int Qk;
	  }
	c[3];
	/* these are used in the MLD_MFS mode only: */
	char *red_buf;
	char *green_buf;
      }
    ld;
#endif
  }
Tamarack_Scanner;



#define TAM_ADF_ON    0x80
#define TAM_DOUBLE_ON 0x40
#define TAM_TRANS_ON  0x20

#define TAM_INVERSE_ON 0x20




#define THRESHOLDED 0
#define DITHERED 1
#define GREYSCALE 2
#define TRUECOLOR 3



/* Some Tamarack driver internal defines */
#define WINID 0


/* SCSI commands that the Tamarack scanners understand: */

#define TAMARACK_SCSI_TEST_UNIT_READY		0x00
#define TAMARACK_SCSI_INQUIRY			0x12
#define TAMARACK_SCSI_MODE_SELECT		0x15 
#define TAMARACK_SCSI_START_STOP		0x1b
#define TAMARACK_SCSI_AREA_AND_WINDOWS		0x24
#define TAMARACK_SCSI_READ_SCANNED_DATA		0x28
#define TAMARACK_SCSI_GET_DATA_STATUS		0x34 


/* The structures that you have to send to the tamarack to get it to
   do various stuff... */

struct win_desc_header {
  unsigned char pad0[6];
  unsigned char wpll[2];
};


struct win_desc_block {
  unsigned char winid;
  unsigned char pad0;
  unsigned char xres[2];
  unsigned char yres[2];
  unsigned char ulx[4];
  unsigned char uly[4];
  unsigned char width[4];  
  unsigned char length[4];
  unsigned char brightness;
  unsigned char thresh;
  unsigned char contrast;
  unsigned char image_comp;
  unsigned char bpp;
  unsigned char halftone[2];
  unsigned char pad_type;
  unsigned char exposure;
  unsigned char pad3;
  unsigned char compr_type;
  unsigned char pad4[5];
};



struct command_header {
  unsigned char opc;
  unsigned char pad0[3];  
  unsigned char len;
  unsigned char pad1;
};


struct command_header_10 {
  unsigned char opc;
  unsigned char pad0[5];  
  unsigned char len[3];
  unsigned char pad1;
};




struct def_win_par {
  struct command_header_10 dwph;
  struct win_desc_header wdh;
  struct win_desc_block wdb;
};


struct page_header{
  char pad0[4];
  char code;
  char length;
};



struct tamarack_page {
  char gamma;
  unsigned char thresh;
  unsigned char masks;
  char delay;
  char features;
  char pad0;
};


/* set SCSI highended variables. Declare them as an array of chars */
/* endianness-safe, int-size safe... */
#define set_double(var,val) var[0] = ((val) >> 8) & 0xff;  \
                            var[1] = ((val)     ) & 0xff;

#define set_triple(var,val) var[0] = ((val) >> 16) & 0xff; \
                            var[1] = ((val) >> 8 ) & 0xff; \
                            var[2] = ((val)      ) & 0xff;

#define set_quad(var,val)   var[0] = ((val) >> 24) & 0xff; \
                            var[1] = ((val) >> 16) & 0xff; \
                            var[2] = ((val) >> 8 ) & 0xff; \
                            var[3] = ((val)      ) & 0xff;

#endif /* tamarack_h */
