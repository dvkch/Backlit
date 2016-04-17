/* --------------------------------------------------------------------- */

/* coolscan.h - headerfile for SANE-backend for coolscan scanners

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

 */

/* --------------------------------------------------------------------- */

#ifndef coolscan_h
#define coolscan_h

#include "sys/types.h"

enum Coolscan_Option
  {
    OPT_NUM_OPTS = 0,

    OPT_MODE_GROUP,
    OPT_MODE,
    OPT_SOURCE,
    OPT_RESOLUTION,
    OPT_PREVIEW_RESOLUTION,
    OPT_TYPE,
    OPT_BIT_DEPTH,
    OPT_PRESCAN,
    OPT_PRESCAN_NOW,

    OPT_GEOMETRY_GROUP,
    OPT_TL_X,			/* top-left x */
    OPT_TL_Y,			/* top-left y */
    OPT_BR_X,			/* bottom-right x */
    OPT_BR_Y,			/* bottom-right y */

    OPT_ENHANCEMENT_GROUP,
    OPT_GAMMA_BIND,
    OPT_ANALOG_GAMMA,
    OPT_AVERAGING,
    OPT_RGB_CONTROL,

    OPT_BRIGHTNESS,
    OPT_R_BRIGHTNESS,
    OPT_G_BRIGHTNESS,
    OPT_B_BRIGHTNESS,

    OPT_CONTRAST,
    OPT_R_CONTRAST,
    OPT_G_CONTRAST,
    OPT_B_CONTRAST,

    OPT_EXPOSURE,
    OPT_R_EXPOSURE,
    OPT_G_EXPOSURE,
    OPT_B_EXPOSURE,

    OPT_R_SHIFT,
    OPT_G_SHIFT,
    OPT_B_SHIFT,

    OPT_ADVANCED_GROUP,
    OPT_PREVIEW,		/* preview */
    OPT_AUTOFOCUS,		/* autofocus */
    OPT_IRED_RED,
    OPT_GAMMA_VECTOR,
    OPT_GAMMA_VECTOR_R,
    OPT_GAMMA_VECTOR_G,
    OPT_GAMMA_VECTOR_B,

    /* must come last: */
    NUM_OPTIONS
  };


typedef struct Image_Pos
{ int start;           /* start position of image on film strip */
  int end;             /* end position of image on film strip */
  int offset           /* always 0 */;
  int height;          /* image height always 2591 */  
} Image_Pos_t;




typedef struct Coolscan
  {
    struct Coolscan *next;

    SANE_Option_Descriptor opt[NUM_OPTIONS];

    SANE_Pid reader_pid;
    int reader_fds;
    int pipe;
    int scanning;
/*--------------------------*/
    SANE_Device sane;
    SANE_Range dpi_range;
    SANE_Range x_range;
    SANE_Range y_range;

/*--------------------------*/
    /* buffer used for scsi-transfer and writing*/
    unsigned char *buffer;
    unsigned char *obuffer;
    unsigned int row_bufsize;

    char *devicename;		/* name of the scanner device */
    int sfd;			/* output file descriptor, scanner device */

    char vendor[9];		/* will be Nikon */
    char product[17];		/* e.g. "LS-1000 " or so */
    char version[5];		/* e.g. V1.6 */

    int LS;			/* index in scanner_str  */
    int cont;			/* continue although scanner is unknown */
    int verbose;		/* 1,2=output informations */
    int asf;			/* Automatic Slide Feeder enabled? */

    int MUD;			/* Measurement Unit Divisor (1200 or 2700) */

    int inquiry_len;		/* length of inquiry return block [36] */
    int inquiry_wdb_len;	/* length of window descriptor block [117] */

    int wdb_len;		/* use this length of WDB */

    double width;		/* use this width of scan-area */
    double length;		/* use this length of scan-area */

    int x_nres;
    int y_nres;

    int x_p_nres;		/* same as above, but apply to preview */
    int y_p_nres;


    int tlx;			/* Left edge in 'Internal Length Units'. */
    int tly;			/* Top edge in ILU */
    int brx;			/* Right edge in ILU. */
    int bry;			/* Bottom edge in ILU. */

    int bits_per_color;		/* bits per color (8/10/12) */
    int bits_per_pixel;		/* bits per pixel (24/30/40) */
    int negative;		/* Negative/positive object */
    int dropoutcolor;		/* Which color to scan when gray */
    int transfermode;		/**/
    int	gammaselection;		/* Linear/Monitor*/
    int shading;
    int averaging;
    int brightness_R;
    int brightness_G;
    int brightness_B;
    int contrast_R;
    int contrast_G;
    int contrast_B;
    int exposure_R;
    int exposure_G;
    int exposure_B;
    int shift_R;
    int shift_G;
    int shift_B;
    int set_auto;		/* 0 or 1, don't know what it is */
    int preview;		/* 1 if preview */
    int autofocus;		/* when to do autofocus */
#define AF_NEVER            0x00
#define AF_PREVIEW          0x01
#define AF_SCAN             0x02
#define AF_PREANDSCAN       0x03

    int colormode;		/* GREYSCALE or RGB  */
    int colormode_p;		/* GREYSCALE or RGB for preview */
#define GREYSCALE           0x01
#define RGB                 0x07
#define IRED                0x08
#define RGBI                0x0f

    int low_byte_first;         /* 1 if little-endian - 0 if big-endian */

    /* Internal information */
    int adbits;			/* Number of A/D bits [8 or 12] */
    int outputbits;		/* Number of output image data bits [8] */
    int maxres;			/* Maximum resolution [2700] (dpi) */
    int xmax;			/* X-axis coordinate maximum value 
				   (basic measurement unit when measurement 
				   unit divisor = 1200) [1151] */
    int ymax;			/* Y-axis coordinate maximum value 
				   (basic measurement unit when measurement 
				   unit divisor = 1200) [1727] */
    int xmaxpix;		/* X-axis coordinate maximum value (pixel 
				   address value) [2591] */
    int ymaxpix;		/* Y-axis coordinate maximum value (pixel 
				   address value) [3887] */
    int ycurrent;		/* Current stage position (Y-axis direction 
				   pixel address) [0-7652] */
    int currentfocus;		/* Current focus position (focus direction 
				   address) [0-200] */
    int currentscanpitch;	/* Current scan pitch [1-25] */
    int autofeeder;		/* Provision of auto feeder [Yes: 1, No: 0] */
    int analoggamma;		/* Analog gamma support [Yes: 1, No: 0] */
    int derr[8];		/* Device error code (0 is latest, 7 oldest) */
    int wbetr_r;		/* White balance exposure time variable (R) */
    int webtr_g;		/* White balance exposure time variable (G) */
    int webtr_b;		/* White balance exposure time variable (B) */
    int pretv_r;		/* Prescan result exposure time variable (R) */
    int pretv_g;		/* Prescan result exposure time variable (G) */
    int pretv_b;		/* Prescan result exposure time variable (B) */
    int cetv_r;			/* Current exposure time variable (R) */
    int cetv_g;			/* Current exposure time variable (G) */
    int cetv_b;			/* Current exposure time variable (B) */
    int ietu_r;			/* Internal exposure time unit (R) */
    int ietu_g;			/* Internal exposure time unit (G) */
    int ietu_b;			/* Internal exposure time unit (B) */
    int limitcondition;		/* Condition of each limit SW, DIP SW, etc. */
    int offsetdata_r;		/* Offset data (R) */
    int offsetdata_g;		/* Offset data (G) */
    int offsetdata_b;		/* Offset data (B) */
    char power_on_errors[8];	/* Records of error code at power on */
    /* End of internal information */

    int brightness;		/* (128) cbhs_range 0-255, halftone mode */
    int contrast;		/* (128) cbhs_range 0-255, halftone-mode */

    int prescan;		/* */
    int rgb_control;		/* */
    int gamma_bind;		/* TRUE -> RGB */
    int lutlength;              /* length of gamma table */
    int max_lut_val;            /* maximum value in lut */
    SANE_Word gamma[4096];	/* gamma value for RGB */
    SANE_Word gamma_r[4096];	/* gamma value for red */
    SANE_Word gamma_g[4096];	/* gamma value for green */
    SANE_Word gamma_b[4096];	/* gamma value for blue */

    int luti[4096];	        /* lut value for infrared */
    int lutr[4096];	        /* lut value for red */
    int lutg[4096];	        /* lut value for green */
    int lutb[4096];	        /* lut value for blue */

    char *gamma_file_r;		/* file for gamma download */
    char *gamma_file_g;		/* file for gamma download */
    char *gamma_file_b;		/* file for gamma download */

    int analog_gamma_r;		/* analog gamma red and grey */
    int analog_gamma_g;		/* analog gamma green */
    int analog_gamma_b;		/* analog gamma blue */

    /* Infrared correction values */    
    int ired_red;
    int ired_green;
    int ired_blue;

    int feeder;                 /* type of feeder used */
    int numima;                 /* number of images on film strip */
    int posima;                 /* current image */
    Image_Pos_t ipos[6];        /* positions for 6 images */
#define  STRIP_FEEDER 1
#define  MOUNT_FEEDER 2
  }
Coolscan_t;




typedef struct
  {
    char *scanner;
    char *inquiry;
    int inquiry_len;
  }
inquiry_blk;


/* ==================================================================== */

/* names of scanners that are supported because */
/* the inquiry_return_block is ok and driver is tested */

static char *scanner_str[] =
{
  "COOLSCAN II ",
  "LS-1000 ",
  "COOLSCANIII ",
  "LS-2000 ",
};

#define known_scanners 4

/* Comment this line if you havn't patched sane.h to include
  SANE_FRAME_RGBA */
/* #define HAS_IRED 1 */

#endif /* coolscan-sane_h */
