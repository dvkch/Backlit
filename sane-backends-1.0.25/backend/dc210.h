/***************************************************************************
 * SANE - Scanner Access Now Easy.

   dc210.c 

   11/11/98

   This file (C) 1998 Brian J. Murrell

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

   This file implements a SANE backend for the Kodak DC-210
   digital camera.  THIS IS EXTREMELY ALPHA CODE!  USE AT YOUR OWN RISK!! 

   (feedback to:  sane-dc210@interlinx.bc.ca

   This backend is based somewhat on the dc25 backend included in this
   package by Peter Fales 

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
    unsigned int low_res;
    unsigned int size;
  }
PictureInfo;

typedef struct DC210_s
  {
    int fd;			/* file descriptor to talk to it */
    char *tty_name;		/* the tty port name it's on */
    speed_t baud;		/* current tty speed */
    SANE_Bool scanning;		/* currently scanning an image? */
    unsigned char model;
    unsigned char ver_major;
    unsigned char ver_minor;
    int pic_taken;
    int pic_left;
    struct
      {
	unsigned int low_res:1;
	unsigned int low_batt:1;
      }
    flags;
    PictureInfo *Pictures;	/* array of pictures */
    unsigned int current_picture_number;	/* picture being operated on */
  }
DC210;

typedef struct dc210_info_s
  {
    unsigned char model;
    unsigned char ver_major;
    unsigned char ver_minor;
    int pic_taken;
    int pic_left;
    struct
      {
	unsigned int low_res:1;
	unsigned int low_batt:1;
      }
    flags;
  }
Dc210Info, *Dc210InfoPtr;

static int get_info (DC210 *);

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
#define ERASE_PCK	{0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1A}
#define RES_PCK		{0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1A}
/*                               ^^^^
 *                               Resolution: 0x00 = high, 0x01 = low
 */
#define THUMBS_PCK	{0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1A}
/*                                     ^^^^
 *                                     Thumbnail number
 */
#define PICS_PCK	{0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1A}
/*                                     ^^^^
 *                                     Picture number
 */
#define PICS_INFO_PCK	{0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1A}
/*                                     ^^^^
 *                                     Picture number
 */

struct pkt_speed
  {
    speed_t baud;
    unsigned char pkt_code[2];
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

/*
 *    Image parameters
 */

#define LOW_CAMERA_HEADER	256
#define HIGH_CAMERA_HEADER	512
#define CAMERA_HEADER(r)	( (r) ? LOW_CAMERA_HEADER : HIGH_CAMERA_HEADER )

#define LOW_WIDTH			256
#define HIGH_WIDTH			512
#define WIDTH(r)			( (r) ? LOW_WIDTH : HIGH_WIDTH )

#define HEIGHT				243

#define LEFT_MARGIN			1

#define LOW_RIGHT_MARGIN	5
#define HIGH_RIGHT_MARGIN	10
#define RIGHT_MARGIN(r)		( (r) ? LOW_RIGHT_MARGIN : HIGH_RIGHT_MARGIN )

#define TOP_MARGIN			1

#define BOTTOM_MARGIN		1

#define BLOCK_SIZE			1024

#define LOW_BLOCKS			61
#define HIGH_BLOCKS			122
#define BLOCKS(r)			( (r) ? LOW_BLOCKS : HIGH_BLOCKS )

#define	LOW_IMAGE_SIZE		( LOW_BLOCKS * BLOCK_SIZE )
#define HIGH_IMAGE_SIZE		( HIGH_BLOCKS * BLOCK_SIZE )
#define IMAGE_SIZE(r)		( (r) ? LOW_IMAGE_SIZE : HIGH_IMAGE_SIZE )
#define MAX_IMAGE_SIZE		( HIGH_IMAGE_SIZE )

/*
 *    Comet file
 */

#define COMET_MAGIC			"COMET"
#define COMET_HEADER_SIZE	128
#define COMET_EXT			"cmt"

/*
 *    Pixmap structure
 */

struct pixmap
  {
    int width;
    int height;
    int components;
    unsigned char *planes;
  };

/*
 *    Rotations
 */

#define ROT_STRAIGHT	0x00
#define ROT_LEFT		0x01
#define ROT_RIGHT		0x02
#define ROT_HEADDOWN	0x03

#define ROT_MASK		0x03

/*
 *    File formats
 */

#define SAVE_RAW		0x01
#define SAVE_GREYSCALE		0x02
#define SAVE_24BITS		0x04
#define SAVE_FILES		0x07
#define SAVE_FORMATS		0x38
#define SAVE_ADJASPECT		0x80

/*
 *    External definitions
 */

extern char *__progname;	/* Defined in /usr/lib/crt0.o */



#include <sys/types.h>

FILE *sanei_config_open (const char *filename);

static int init_dc210 (DC210 *);

static void close_dc210 (int);

static int read_data (int fd, unsigned char *buf, int sz);

static int end_of_data (int fd);

static PictureInfo *get_pictures_info (void);

static int get_picture_info (PictureInfo * pic, int p);

static SANE_Status snap_pic (int);

char *sanei_config_read (char *str, int n, FILE * stream);
