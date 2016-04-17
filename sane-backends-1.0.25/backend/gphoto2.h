/***************************************************************************
 * SANE - Scanner Access Now Easy.

   gphoto2.h

   03/12/01 - Peter Fales

   Based on the dc210 driver, (C) 1998 Brian J. Murrell (which is
	based on dc25 driver (C) 1998 by Peter Fales)

   This file (C) 2001 by Peter Fales

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

 ***************************************************************************

   This file implements a SANE backend for the Kodak DC-240
   digital camera.  THIS IS EXTREMELY ALPHA CODE!  USE AT YOUR OWN RISK!! 

   (feedback to:  gphoto2-devel@fales-lorenz.net)

   This backend is based somewhat on the dc25 backend included in this
   package by Peter Fales, and the dc210 backend by Brian J. Murrell

 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>

#ifndef TRUE
#define TRUE	(1==1)
#endif

#ifndef FALSE
#define FALSE	(!TRUE)
#endif

#ifndef NULL
#define NULL	0L
#endif

typedef struct picture_info
{
  int low_res;
  int size;
}
PictureInfo;

typedef struct GPHOTO2_s
{
  SANE_String port;		/* the port name it's on */
  SANE_Int speed;		/* current port speed */
  SANE_String camera_name;
  SANE_Bool scanning;		/* currently scanning an image? */
  SANE_Byte model;
  SANE_Byte ver_major;
  SANE_Byte ver_minor;
  SANE_Int pic_taken;
  SANE_Int pic_left;
  struct
  {
    unsigned int low_res:1;
    unsigned int low_batt:1;
  }
  flags;
  PictureInfo *Pictures;	/* array of pictures */
  SANE_Int current_picture_number;	/* picture being operated on */
}
GPHOTO2;

typedef struct gphoto2_info_s
{
  SANE_Byte model;
  SANE_Byte ver_major;
  SANE_Byte ver_minor;
  SANE_Int pic_taken;
  SANE_Int pic_left;
  struct
  {
    SANE_Int low_res:1;
    SANE_Int low_batt:1;
  }
  flags;
}
Gphoto2Info, *Gphoto2InfoPtr;

static SANE_Int get_info (void);

#define HIGH_RES		0
#define LOW_RES			1

#define HIGHRES_WIDTH		highres_width
#define HIGHRES_HEIGHT		highres_height

#define LOWRES_WIDTH		lowres_width
#define LOWRES_HEIGHT		lowres_height

#define THUMB_WIDTH		thumb_width
#define THUMB_HEIGHT		thumb_height

/*
 *    External definitions
 */

extern char *__progname;	/* Defined in /usr/lib/crt0.o */


struct cam_dirent
{
  SANE_Char name[11];
  SANE_Byte attr;
  SANE_Byte create_time[2];
  SANE_Byte creat_date[2];
  long size;
};

#ifdef __GNUC__
#define UNUSEDARG __attribute__ ((unused))
#else
#define UNUSEDARG
#endif

struct cam_dirlist
{
  SANE_Char name[48];
  struct cam_dirlist *next;
};



#include <sys/types.h>

FILE *sanei_config_open (const char *filename);

static SANE_Int init_gphoto2 (void);

static void close_gphoto2 (void);

static PictureInfo *get_pictures_info (void);

static SANE_Int get_picture_info (PictureInfo * pic, SANE_Int p);

static SANE_Status snap_pic (void);

char *sanei_config_read (char *str, int n, FILE * stream);

static SANE_Int read_dir (SANE_String dir, SANE_Bool read_files);

static void set_res (SANE_Int lowres);

static SANE_Int read_info (SANE_String_Const fname);

static SANE_Status converter_do_scan_complete_cleanup (void);

static SANE_Int converter_fill_buffer (void);

static SANE_Bool converter_scan_complete (void);

static SANE_Status converter_init (SANE_Handle handle);
