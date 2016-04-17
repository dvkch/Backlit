/* sane - Scanner Access Now Easy.

   Copyright (C) 2004 -2006 Gerard Klaver (gerard at gkall dot hobby dot nl)

   The teco2 and gl646 backend (Frank Zago) are used as a template for 
   this backend.
      
   For the usb commands and bayer decoding parts of the following 
   program are used:
   The pencam2 program  (GNU GPL license 2)

   For the usb commands parts of the following programs are used:
   The libgphoto2 (camlib stv0680)   (GNU GPL license 2)
   The stv680.c/.h kernel module   (GNU GPL license 2)

   For the stv680_add_text routine the add_text routine and font_6x11.h file 
   are taken from the webcam.c file, part of xawtv program,
   (c) 1998-2002 Gerd Knorr (GNU GPL license 2).

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
   ---------------------------------------------------------------------
*/

/* 
	$Id$
        update 20-04-2006*/

/* Commands supported by the vidcam. */

/*--------------------------------------------------------------------------*/

static inline int
getbitfield (unsigned char *pageaddr, int mask, int shift)
{
  return ((*pageaddr >> shift) & mask);
}

/*--------------------------------------------------------------------------*/

#include <stdio.h>

#define LIBUSB_TIMEOUT					1000	/* ms */

typedef unsigned char byte;

/*--------------------------------------------------------------------------*/

/* Black magic for color adjustment. */
struct dpi_color_adjust
{
  int resolution_x;		/* x-resolution  */
  int resolution_y;		/* y-resolution  */

  int z1_color_0;		/* 0, 1 or 2 */
  int z1_color_1;		/* idem */
  int z1_color_2;		/* idem */
};

/*--------------------------------------------------------------------------*/

enum Stv680_Option
{
  /* Must come first */
  OPT_NUM_OPTS = 0,

  OPT_MODE_GROUP,
  OPT_MODE,			/* vidcam modes */
  OPT_RESOLUTION,		/* X and Y resolution */
  OPT_BRIGHTNESS,		/* brightness   */

  OPT_ENHANCEMENT_GROUP,
  OPT_WHITE_LEVEL_R,		/*white level red correction */
  OPT_WHITE_LEVEL_G,		/*white level green correction */
  OPT_WHITE_LEVEL_B,		/*white level blue correction */

  /* must come last: */
  OPT_NUM_OPTIONS
};

/*--------------------------------------------------------------------------*/

/* 
 * Video Camera supported by this backend. 
 */
struct vidcam_hardware
{
  /* USB stuff */
  SANE_Word vendor;
  SANE_Word product;
  SANE_Word class;

  /* Readable names */
  const char *vendor_name;	/* brand on the box */
  const char *product_name;	/* name on the box */

  /* Resolutions supported in color mode. */
  const struct dpi_color_adjust *color_adjust;
};

#define COLOR_RAW_STR			SANE_I18N("Color RAW")
#define COLOR_RGB_STR			SANE_I18N("Color RGB")
#define COLOR_RGB_TEXT_STR              SANE_I18N("Color RGB TEXT")
/*--------------------------------------------------------------------------*/

/* Define a vidcam occurence. */
typedef struct Stv680_Vidcam
{
  struct Stv680_Vidcam *next;
  SANE_Device sane;

  char *devicename;
  SANE_Int fd;			/* device handle */

  /* USB handling */
  size_t buffer_size;		/* size of the buffer */
  SANE_Byte *buffer;		/* for USB transfer. */

  /* Bayer handling */
  size_t output_size;		/* size of the output */
  SANE_Byte *output;		/* for bayer conversion */

  size_t image_size;		/* allocated size of image */
  size_t image_begin;		/* first significant byte in image */
  size_t image_end;		/* first free byte in image */
  SANE_Byte *image;		/* keep the raw image here */

  /* USB control messages handling */
  size_t windoww_size;		/* size of window write */
  size_t windowr_size;		/* size of window read  */
  SANE_Byte *windoww;		/* for window write     */
  SANE_Byte *windowr;		/* for window read      */

  /* Scanner infos. */
  const struct vidcam_hardware *hw;	/* default options for that vidcam */

  SANE_Word *resolutions_list;
  SANE_Word *color_sequence_list;

  /* Scanning handling. */
  SANE_Bool scanning;		/* TRUE if a scan is running. */
  SANE_Bool deliver_eof;
  int x_resolution;		/* X resolution */
  int y_resolution;		/* Y resolution */
  int depth;			/* depth per color */
  unsigned int colour;
  int red_s;
  int green_s;
  int blue_s;

  SANE_Parameters s_params;
  enum
  {
    STV680_COLOR_RGB,
    STV680_COLOR_RGB_TEXT,
    STV680_COLOR,
    STV680_COLOR_RAW
  }
  scan_mode;

  size_t bytes_left;		/* number of bytes left to give to the backend */
  size_t real_bytes_left;	/* number of bytes left the vidcam will return. */
  int bytes_pixel;

  const struct dpi_color_adjust *color_adjust;

  SANE_Parameters params;

  /* Options */
  SANE_Option_Descriptor opt[OPT_NUM_OPTIONS];
  Option_Value val[OPT_NUM_OPTIONS];

  unsigned int video_mode;	/* 0x0100 = VGA, 0x0000 = CIF,
				 * 0x0300 = QVGA, 0x0200 = QCIF*/
  unsigned int video_status;	/* 0x01=start, 0x02=video, 0x04=busy, 0x08=idle */

  int SupportedModes;
  int HardwareConfig;
  int QSIF;
  int CIF;
  int VGA;
  int QVGA;
  int QCIF;
  int cwidth;			/* camera width */
  int cheight;
  int subsample;

  int framecount;
  char picmsg_ps[50];

}
Stv680_Vidcam;

/*--------------------------------------------------------------------------*/

/* Debug levels. 
 * Should be common to all backends. */

#define DBG_error0  0
#define DBG_error   1
#define DBG_sense   2
#define DBG_warning 3
#define DBG_inquiry 4
#define DBG_info    5
#define DBG_info2   6
#define DBG_proc    7
#define DBG_read    8
#define DBG_sane_init   10
#define DBG_sane_proc   11
#define DBG_sane_info   12
#define DBG_sane_option 13

/*--------------------------------------------------------*/

static SANE_Byte red_g[256] = {
  0, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
  18, 18, 18, 18, 18, 18, 18, 25, 30, 35, 38, 42,
  44, 47, 50, 53, 54, 57, 59, 61, 63, 65, 67, 69,
  71, 71, 73, 75, 77, 78, 80, 81, 82, 84, 85, 87,
  88, 89, 90, 91, 93, 94, 95, 97, 98, 98, 99, 101,
  102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113,
  114, 115, 116, 116, 117, 118, 119, 120, 121, 122, 123, 124,
  125, 125, 126, 127, 128, 129, 129, 130, 131, 132, 133, 134,
  134, 135, 135, 136, 137, 138, 139, 140, 140, 141, 142, 143,
  143, 143, 144, 145, 146, 147, 147, 148, 149, 150, 150, 151,
  152, 152, 152, 153, 154, 154, 155, 156, 157, 157, 158, 159,
  159, 160, 161, 161, 161, 162, 163, 163, 164, 165, 165, 166,
  167, 167, 168, 168, 169, 170, 170, 170, 171, 171, 172, 173,
  173, 174, 174, 175, 176, 176, 177, 178, 178, 179, 179, 179,
  180, 180, 181, 181, 182, 183, 183, 184, 184, 185, 185, 186,
  187, 187, 188, 188, 188, 188, 189, 190, 190, 191, 191, 192,
  192, 193, 193, 194, 195, 195, 196, 196, 197, 197, 197, 197,
  198, 198, 199, 199, 200, 201, 201, 202, 202, 203, 203, 204,
  204, 205, 205, 206, 206, 206, 206, 207, 207, 208, 208, 209,
  209, 210, 210, 211, 211, 212, 212, 213, 213, 214, 214, 215,
  215, 215, 215, 216, 216, 217, 217, 218, 218, 218, 219, 219,
  220, 220, 221, 221
};

static SANE_Byte green_g[256] = {
  0, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
  21, 21, 21, 21, 21, 21, 21, 28, 34, 39, 43, 47,
  50, 53, 56, 59, 61, 64, 66, 68, 71, 73, 75, 77,
  79, 80, 82, 84, 86, 87, 89, 91, 92, 94, 95, 97,
  98, 100, 101, 102, 104, 105, 106, 108, 109, 110, 111, 113,
  114, 115, 116, 117, 118, 120, 121, 122, 123, 124, 125, 126,
  127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138,
  139, 140, 141, 142, 143, 144, 144, 145, 146, 147, 148, 149,
  150, 151, 151, 152, 153, 154, 155, 156, 156, 157, 158, 159,
  160, 160, 161, 162, 163, 164, 164, 165, 166, 167, 167, 168,
  169, 170, 170, 171, 172, 172, 173, 174, 175, 175, 176, 177,
  177, 178, 179, 179, 180, 181, 182, 182, 183, 184, 184, 185,
  186, 186, 187, 187, 188, 189, 189, 190, 191, 191, 192, 193,
  193, 194, 194, 195, 196, 196, 197, 198, 198, 199, 199, 200,
  201, 201, 202, 202, 203, 204, 204, 205, 205, 206, 206, 207,
  208, 208, 209, 209, 210, 210, 211, 212, 212, 213, 213, 214,
  214, 215, 215, 216, 217, 217, 218, 218, 219, 219, 220, 220,
  221, 221, 222, 222, 223, 224, 224, 225, 225, 226, 226, 227,
  227, 228, 228, 229, 229, 230, 230, 231, 231, 232, 232, 233,
  233, 234, 234, 235, 235, 236, 236, 237, 237, 238, 238, 239,
  239, 240, 240, 241, 241, 242, 242, 243, 243, 243, 244, 244,
  245, 245, 246, 246
};

static SANE_Byte blue_g[256] = {
  0, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
  23, 23, 23, 23, 23, 23, 23, 30, 37, 42, 47, 51,
  55, 58, 61, 64, 67, 70, 72, 74, 78, 80, 82, 84,
  86, 88, 90, 92, 94, 95, 97, 100, 101, 103, 104, 106,
  107, 110, 111, 112, 114, 115, 116, 118, 119, 121, 122, 124,
  125, 126, 127, 128, 129, 132, 133, 134, 135, 136, 137, 138,
  139, 140, 141, 143, 144, 145, 146, 147, 148, 149, 150, 151,
  152, 154, 155, 156, 157, 158, 158, 159, 160, 161, 162, 163,
  165, 166, 166, 167, 168, 169, 170, 171, 171, 172, 173, 174,
  176, 176, 177, 178, 179, 180, 180, 181, 182, 183, 183, 184,
  185, 187, 187, 188, 189, 189, 190, 191, 192, 192, 193, 194,
  194, 195, 196, 196, 198, 199, 200, 200, 201, 202, 202, 203,
  204, 204, 205, 205, 206, 207, 207, 209, 210, 210, 211, 212,
  212, 213, 213, 214, 215, 215, 216, 217, 217, 218, 218, 220,
  221, 221, 222, 222, 223, 224, 224, 225, 225, 226, 226, 227,
  228, 228, 229, 229, 231, 231, 232, 233, 233, 234, 234, 235,
  235, 236, 236, 237, 238, 238, 239, 239, 240, 240, 242, 242,
  243, 243, 244, 244, 245, 246, 246, 247, 247, 248, 248, 249,
  249, 250, 250, 251, 251, 253, 253, 254, 254, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255
};
