/***************************************************************************
 * SANE - Scanner Access Now Easy.

   dc240.h

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

   (feedback to:  dc240-devel@fales-lorenz.net)

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

typedef struct DC240_s
{
  SANE_Int fd;			/* file descriptor to talk to it */
  char *tty_name;		/* the tty port name it's on */
  speed_t baud;			/* current tty speed */
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
DC240;

typedef struct dc240_info_s
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
Dc240Info, *Dc240InfoPtr;

static SANE_Int get_info (DC240 *);

#define INIT_PCK	{0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1A}
/*                               ^^^^^^^^^^
 *                               Baud rate: (see pkt_speed structure)
 *                                 0x96 0x00 -> 9600 baud
 *                                 0x19 0x20 -> 19200 baud
 *                                 0x38 0x40 -> 38400 baud
 *                                 0x57 0x60 -> 57600 baud
 *                                 0x11 0x52 -> 115200 baud
 */
#define INFO_PCK	{0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1A}
#define SHOOT_PCK	{0x7C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1A}
#define ERASE_PCK	{0x9D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1A}
#define RES_PCK		{0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1A}
/*                                   ^^^^
 *                                   Resolution: 0x00 = low, 0x01 = high
 */
#define THUMBS_PCK	{0x93, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x1A}
/*  
 *  
 */
#define PICS_PCK	{0x9A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1A}
/*      
 *      
 */
#define PICS_INFO_PCK	{0x91, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1A}
/*                         
 *                         
 */
#define OPEN_CARD_PCK	{0x96, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1A}
#define READ_DIR_PCK	{0x99, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1A}
/*                                   ^^^^
 *				     1=Report number of entries only
 */

struct pkt_speed
{
  speed_t baud;
  SANE_Byte pkt_code[2];
};

#if defined (B57600) && defined (B115200)
# define SPEEDS			{ {   B9600, { 0x96, 0x00 } }, \
				  {  B19200, { 0x19, 0x20 } }, \
				  {  B38400, { 0x38, 0x40 } }, \
				  {  B57600, { 0x57, 0x60 } }, \
				  { B115200, { 0x11, 0x52 } }  }
#else
# define SPEEDS			{ {   B9600, { 0x96, 0x00 } }, \
				  {  B19200, { 0x19, 0x20 } }, \
				  {  B38400, { 0x38, 0x40 } }  }
#endif

#define HIGH_RES		0
#define LOW_RES			1

#define HIGHRES_WIDTH		1280
#define HIGHRES_HEIGHT		960

#define LOWRES_WIDTH		640
#define LOWRES_HEIGHT		480

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

#ifdef OLD

/* This is the layout of the directory in the camera - Unfortunately,
 * this only works in gcc.
 */
struct dir_buf
{
  SANE_Byte entries_msb PACKED;
  SANE_Byte entries_lsb PACKED;
  struct cam_dirent entry[1000] PACKED;
};
#else

/* So, we have to do it the hard way...  */

#define CAMDIRENTRYSIZE 20
#define DIRENTRIES 1000


#define get_name(entry) (SANE_Char*) &dir_buf2[2+CAMDIRENTRYSIZE*(entry)]
#define get_attr(entry) dir_buf2[2+11+CAMDIRENTRYSIZE*(entry)]
#define get_create_time(entry) \
   (  dir_buf2[2+12+CAMDIRENTRYSIZE*(entry)] << 8 \
    + dir_buf2[2+13+CAMDIRENTRYSIZE*(entry)])


#endif

struct cam_dirlist
{
  SANE_Char name[48];
  struct cam_dirlist *next;
};



#include <sys/types.h>

FILE *sanei_config_open (const char *filename);

static SANE_Int init_dc240 (DC240 *);

static void close_dc240 (SANE_Int);

static SANE_Int read_data (SANE_Int fd, SANE_Byte * buf, SANE_Int sz);

static SANE_Int end_of_data (SANE_Int fd);

static PictureInfo *get_pictures_info (void);

static SANE_Int get_picture_info (PictureInfo * pic, SANE_Int p);

static SANE_Status snap_pic (SANE_Int fd);

char *sanei_config_read (char *str, int n, FILE * stream);

static SANE_Int read_dir (SANE_String dir);

static SANE_Int read_info (SANE_String fname);

static SANE_Int dir_insert (struct cam_dirent *entry);

static SANE_Int dir_delete (SANE_String name);

static SANE_Int send_data (SANE_Byte * buf);

static void set_res (SANE_Int lowres);
