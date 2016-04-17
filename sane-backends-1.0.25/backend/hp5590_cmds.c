/* sane - Scanner Access Now Easy.
   Copyright (C) 2007 Ilia Sotnikov <hostcc@gmail.com>
   HP ScanJet 4570c support by Markham Thomas
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

   This file is part of a SANE backend for
   HP ScanJet 4500C/4570C/5500C/5550C/5590/7650 Scanners
*/

#include "../include/sane/config.h"

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif /* HAVE_NETINET_IN_H */
#include <errno.h>
#include <string.h>

#include "../include/sane/sanei_debug.h"
#include "../include/_stdint.h"
#include "hp5590_low.h"
#include "hp5590_cmds.h"

static const struct hp5590_model
hp5590_models[] = {
  {
    SCANNER_HP4570,
    0x03f0, 0x1305, "SILITEKIElwood",
    "4570C/5500C", "Workgroup scanner",
    PF_NONE
  },
  {
    SCANNER_HP5550,
    0x03f0, 0x1205, "SILITEKIPenguin",
    "4500C/5550C", "Workgroup scanner",
    PF_NO_USB_IN_USB_ACK		/* These devices need no
    					 * acknowledgement after USB-in-USB
					 * commands */
  },
  {
    SCANNER_HP5590,
    0x03f0, 0x1705, "SILITEKIPenguin",
    "5590", "Workgroup scanner",
    PF_NONE
  },
  {
    SCANNER_HP7650,
    0x03f0, 0x1805, "SILITEKIArnold",
    "7650", "Document scanner",
    PF_NONE
  }
};

/* Debug levels */
#define	DBG_err		0
#define DBG_proc	10
#define DBG_cmds	40

#define hp5590_cmds_assert(exp) if(!(exp)) { \
  DBG (DBG_err, "Assertion '%s' failed at %s:%u\n", #exp, __FILE__, __LINE__);\
  return SANE_STATUS_INVAL; \
}

#define WAKEUP_TIMEOUT		90

/* First byte of init (0x12 cmd) response */
#define INIT_FLAG_TMA		1 << 0
#define	INIT_FLAG_ADF		1 << 1
#define INIT_FLAG_LCD		1 << 3

/* Power (0x24 cmd) */
#define POWER_FLAG_ON		1 << 1

/* ADF (0x03 cmd) */
#define FLAG_ADF_EMPTY		1 << 1

#define PART_NUMBER_LEN		10

#define REVERSE_MAP_LEN		128 * 1024 / sizeof(uint16_t)
#define FORWARD_MAP_LEN		128 * 1024 / sizeof(uint16_t)

/* Button flags */
/* From left to rigth */
/* 1:   Power
 * 1:   Scan
 * 2:   Collect
 * 3:   File
 * 4:   Email
 * 5:   Copy
 * 6,7: Up/down
 * 8:   Mode
 * 9:   Cancel
 */
#define BUTTON_FLAG_EMAIL	1 << 15
#define BUTTON_FLAG_COPY	1 << 14
#define BUTTON_FLAG_DOWN	1 << 13
#define BUTTON_FLAG_MODE	1 << 12
#define BUTTON_FLAG_UP		1 << 11
#define BUTTON_FLAG_FILE	1 << 9
#define BUTTON_FLAG_POWER	1 << 5
#define BUTTON_FLAG_SCAN	1 << 2
#define	BUTTON_FLAG_COLLECT	1 << 1
#define BUTTON_FLAG_CANCEL	1 << 0

#define CMD_INIT		0x0012
#define CMD_EEPROM_ADDR		0x00f2
#define CMD_EEPROM_READ		0x0bf0
#define CMD_EEPROM_WRITE	0x0bf1
#define CMD_DATA_STATUS		0x0001
#define CMD_STOP_SCAN		0x011b
#define CMD_CONTROL_LAMP	0x00c0
#define CMD_POWER_STATUS	0x0024
#define CMD_SELECT_SOURCE	0x00d6
#define CMD_MISC_STATUS		0x0003
#define CMD_LOCK_UNLOCK		0x0000
#define CMD_SET_BASE_DPI	0x0015
#define CMD_SET_COLOR_MAP	0x0240
#define CMD_SET_SCAN_PARAMS	0x0025
#define CMD_GET_IMAGE_PARAMS	0x0034
#define CMD_START_SCAN		0x051b
#define CMD_BUTTON_STATUS	0x0020

struct init_resp
{
  uint8_t 	flags;			/* bit 0 - TMA, bit 1 - ADF, bit 3 - LCD present */
  uint8_t 	id[15];			/* SILITEKPenguin */
  uint8_t 	pad1[9];		/* 00 00 00 00 00 00 00 00 00 */
  uint8_t 	version[5];		/* 0.0.67 */
  uint16_t 	max_dpi_x;		/* 09 60 = 2400 */
  uint16_t 	max_dpi_y;		/* 09 60 = 2400 */
  uint16_t 	max_pixels_x;		/* 4F B0 = 20400 (20400 / 2400 = 8.5")   */
  uint16_t 	max_pixels_y;		/* 6D E0 = 28128 (28128 / 2400 = 11.72") */
  uint8_t 	pad2[8];		/* 00 00 00 00 00 00 00 00 */
  uint16_t 	motor_param_normal;	/* 00 64 = 100 */
  uint16_t 	motor_param_max;	/* 03 E8 = 1000 */
} __attribute__ ((packed));

struct power_resp
{
  uint8_t flags;
  uint16_t unk1;
} __attribute__ ((packed));

/* 
 * 215.9 mm x 297.2 mm
 * 8.5" x 11.72"
 * 
 *   50 :  425.00 x  586.00
 *   75 :  637.50 x  879.50
 *  100 :  850.00 x 1172.00
 *  150 : 1275.00 x 1758.00 (base DPI)
 *  200 : 1700.00 x 2344.00
 *  300 : 2550.00 x 3516.00 (base DPI)
 *  400 : 3400.00 x 4688.00
 *  600 : 5100.00 x 7032.00 (base DPI)
 */

#define SCAN_PARAMS_SOURCE_TMA_NEGATIVES 	1 << 0
#define SCAN_PARAMS_SOURCE_TMA_SLIDES 		1 << 1
#define SCAN_PARAMS_SOURCE_ADF	 		1 << 2
#define SCAN_PARAMS_SOURCE_FLATBED		1 << 3
#define SCAN_PARAMS_SOURCE_SIMPLEX		1 << 4
#define SCAN_PARAMS_SOURCE_DUPLEX 		1 << 6

struct scan_params
{
  uint8_t source;		/*
				 * TMA Negatives        : 11 = 17
				 * TMA Slides           : 12 = 18
				 * ADF                  : 14 = 20
				 * Flatbed              : 18 = 24 
				 * ADF Duplex           : 54 = 84
				 */

  uint16_t dpi_x;		/*
				 *  50  : 00 64 = 100
				 *  75  : 00 64 = 100
				 * 100  : 00 64 = 100
				 * 150  : 00 c8 = 200
				 * 200  : 00 c8 = 200
				 * 300  : 01 2c = 300
				 * 400  : 02 58 = 600
				 * 600  : 02 58 = 600
				 * 1200 : 04 b0 = 1200
				 */

  uint16_t dpi_y;		/*
				 *  50  : 00 64 = 100
				 *  75  : 00 64 = 100
				 * 100  : 00 64 = 100
				 * 150  : 00 c8 = 200
				 * 200  : 00 c8 = 200
				 * 300  : 01 2c = 300
				 * 400  : 02 58 = 600
				 * 600  : 02 58 = 600
				 * 1200 : 04 b0 = 1200
				 */

  uint16_t top_x;		/*
				 * pixels * (Base DPI / current DPI)
				 * 00 00, 01 6e = 366 (x = 425 - 302 = 123)
				 * 04 b0 = 1200 (x = 425 - 24 = 401)
				 */

  uint16_t top_y;		/*
				 * pixels * (Base DPI / current DPI)
				 * 00 00, 06 99 = 1689 (y = 585 - 21 = 564)
				 */

  uint16_t size_x;		/* X pixels in Base DPI (CMD 15)
				 *  50          : 04f8 =  1272                           ; 150
				 *  75          : 04f8 =  1272                           ; 150
				 * 100          : 04f8 =  1272                           ; 150
				 * 100 TMA      : 00fc =   252                           ; 150
				 * 150          : 09f6 =  2550, 09f0 = 2544, 09f4 = 2548 ; 300
				 * 200          : 09f0 =  2544, 09f6 = 2550, 09f6 = 2550 ; 300
				 * 300          : 09f6 =  2550, 09f0 = 2544, 09f4 = 2548 ; 300
				 * 300 TMA      : 01fc =   508                           ; 300
				 * 400          : 13ec =  5100                           ; 600
				 * 600          : 13e8 =  5096, 13ec = 5100 ,13ec = 5100 ; 600
				 * 1200         : 27a8 = 10152                           ; 1200
				 */

  uint16_t size_y;		/* Y pixels in Base DPI (CMD 15)
				 *  50          : 06db =  1755 ; 150
				 *  75          : 06da =  1754 ; 150
				 * 100          : 06db =  1755 ; 150
				 * 100 TMA      : 0384 =   900 ; 150 
				 * 150          : 0db6 =  3510 ; 300
				 * 200          : 0db6 =  3510 ; 300
				 * 300          : 0db6 =  3510 ; 300
				 * 300 TMA      : 0708 =  1800 ; 300
				 * 400          : 1b6c =  7020 ; 600
				 * 600          : 1b6c =  7020 ; 600
				 * 1200         : 36d8 = 14040 ; 1200
				 */

  uint16_t unk1;		/* 00 80 */

  uint16_t bw_gray_flag;	/*
				 * 00 40 - bw (ntsc gray)/gray,
				 * 00 20 - bw (by green band),
				 * 00 10 - bw (by red band),
				 * 00 30 - bw (by blue band),
				 * 00 00 - color
				 */

  uint8_t pixel_bits;		/*
				 * bw 50/75/150/400                             : 08 =  8
				 * bw 100/200/300/600/1200                      : 01 =  1
				 * gray 50/75/100/150/200/400/600      	        : 08 =  8
				 * color 24 bit 50/75/100/150/200/400/600	: 18 = 24
				 * color 48 bit 100/200				: 30 = 48
				 */

  uint16_t flags;		/*
				 * 50/75/100/150/200/300        	: e8 40 = 59456
				 * 400/600/1200                         : c8 40 = 51264
				 */

  uint16_t motor_param1;	/*
				 * 00 64 = 100
				 */
  uint16_t motor_param2;	/*
				 * 00 64 = 100 - ADF, Flatbed, TMA slides
				 * 00 c8 = 200 - TMA Negatives
				 */
  uint16_t motor_param3;	/*
				 * 00 64 = 100 - ADF, Flatbed, TMA slides
				 * 01 90 = 400 - TMA negatives
				 */
  uint32_t pad1;		/* 00 00 00 00 */
  uint16_t pad2;		/* 00 00 */
  uint8_t mode;		/* 00 - normal scan, 04 - preview scan */
  uint16_t pad3;		/* 00 00 */

  uint16_t line_width;		/* Based on current .dpi_x
				 * bw 50                                : 03 50 =   848
				 * gray 50                              : 03 50 =   848
				 * color 50                             : 09 f0 =  2544 (3 * gray)
				 *
				 * bw 75                                : 03 50 =   848
				 * gray 75                              : 03 50 =   848
				 * color 75                             : 09 f0 =  2544 (3 * gray)
				 *
				 * bw 100                               : 00 6a =   106 
				 * gray 100                             : 03 50 =   848 (8 * bw) 
				 * color 100(24)			: 09 f0 =  2544 (3 * gray)
				 * color 100(48)			: 13 e0 =  5088 (2 * color 24)
				 * color 100(48) TMA			: 03 f0 = 1008
				 *
				 * bw 150                               : 06 a4 =  1700
				 * gray 150                             : 06 a4 =  1700
				 * color 150				: 13 ec =  5100 (3 * gray)
				 *
				 * bw 200                               : 00 d4 =   212
				 * gray 200                             : 06 a4 =  1700 (8 * bw)
				 * color 200(24)                	: 13 ec =  5100 (3 * gray)
				 * color 200(48)                	: 27 a8 = 10152
				 *
				 * bw 300                               : 01 3e =   318
				 * gray 300                             : 09 f4 =  2548 (8 * bw)
				 * color 300                    	: 1d dc =  7644 (3 * gray)
				 * color 300(48) TMA    		: 0b e8 = 3048
				 *
				 * bw 400                               : 13 ec =  5100
				 * gray 400                             : 13 ec =  5100
				 * color 400  		             	: 3b c4 = 15300 (3 * gray)
				 *
				 * bw 600                               : 02 7d =   637
				 * gray 600                             : 13 ec =  5100 (8 * bw)
				 * color 600                    	: 3b c4 = 15300 (3 * gray)
				 *
				 * bw 1200                              : 04 f5 =  1269
				 */
} __attribute__ ((packed));

struct image_params
{
  uint8_t signature;		/* c0 */
  uint8_t pad1;		/* 00 */
  uint32_t image_size;		/*
				 * bw 50                : 00 0f 23 a0 =    992 160
				 * gray 50              : 00 0f 23 a0 =    992 160
				 * color 50             : 00 2d 6a e0 =  2 976 480
				 *
				 * bw 75                : 00 0f 20 50 =    991 312
				 * gray 75              : 00 0f 20 50 =    991 312
				 * color 75             : 00 2d 60 f0 =  2 973 936
				 * color 75(48) 	: 00 5a 86 40 =  5 932 608
				 *
				 * bw 100               : 00 01 e4 74 =    124 020
				 * gray 100             : 00 0f 23 a0 =    992 160
				 * color 100    	: 00 2d 6a e0 =  2 976 480
				 * color 100(48)	: 00 5a 68 10 =  5 924 880
				 * color 100(48), preview: 00 5a d5 c0 = 5 952 960
				 *
				 * bw 150               : 00 3c b3 10 =   3 978 000
				 * gray 150             : 00 3c b3 10 =   3 978 000
				 * color 150    	: 00 b6 19 30 =  11 934 000
				 * color 150(48)	: 01 6a 7b a0 =  23 755 680
				 *
				 * bw 200               : 00 07 91 d0 =     496 080
				 * gray 200             : 00 3c b3 10 =   3 978 000
				 * color 200    	: 00 b6 19 30 =  11 934 000
				 * color 200(48)	: 01 6a f3 a0 =  23 786 400
				 *
				 * bw 300               : 00 11 08 14 =   1 116 180
				 * gray 300             : 00 88 77 78 =   8 943 480
				 * color 300    	: 01 99 66 68 =  26 830 440
				 *
				 * bw 400               : 02 22 4b 90 =  35 802 000
				 * gray 400             : 02 22 4b 90 =  35 802 000
				 * color 400    	: 06 66 e2 b0 = 107 406 000
				 *
				 * bw 600               : 00 44 3b bc =   4 471 740
				 * gray 600             : 02 22 4b 90 =  35 802 000
				 * color 600    	: 06 66 e2 b0 = 107 406 000
				 */
  uint16_t pad2;		/* 00 00 */
  uint16_t line_width;
  uint16_t real_size_y;
  uint32_t pad3;		/* 00 00 00 00 */
} __attribute__ ((packed));

struct lamp_state
{
  uint8_t	unk1;		/* 02 */
  uint8_t 	flag;		/* 01 on start, 02 - TMA, 03 - all other */
  uint16_t 	turnoff_time;	/* 0a 0a, 03 36, 0f 36 */
} __attribute__ ((packed));

struct color_map
{
  uint8_t 	color1[6];	/* 00 00 00 00 01 00 */
  uint8_t 	color2[6];	/* 00 00 00 00 01 00 */
  uint8_t 	color3[6];	/* 00 00 00 00 01 00 */
} __attribute__ ((packed));

struct reg_03
{
  uint8_t 	unk1;		/* 0x0b - ADF ready, 0x03 - not */
  uint8_t 	unk2;		/* 0x80 */
  uint8_t 	adf_flags;	/* 0x01 - ADF ready when selected, 0x02 - not */
} __attribute__ ((packed));

/******************************************************************************/
static SANE_Status
hp5590_model_def (enum hp_scanner_types scanner_type,
		  const struct hp5590_model ** model)
{
  unsigned int i;
   
  hp5590_cmds_assert (model != NULL);

  for (i = 0; i < sizeof (hp5590_models) / sizeof (struct hp5590_model); i++)
    {
      if (hp5590_models[i].scanner_type == scanner_type)
	{
	  *model = &hp5590_models[i];
	  return SANE_STATUS_GOOD;
	}
    }
	
  return SANE_STATUS_INVAL;
}

/******************************************************************************/
static SANE_Status
hp5590_vendor_product_id (enum hp_scanner_types scanner_type,
			  SANE_Word * vendor_id,
			  SANE_Word * product_id)
{
  const struct hp5590_model	*model;
  SANE_Status			ret;

  hp5590_cmds_assert (vendor_id != NULL);
  hp5590_cmds_assert (product_id != NULL);

  ret = hp5590_model_def (scanner_type, &model);
  if (ret != SANE_STATUS_GOOD)
    return ret;
  *vendor_id = model->usb_vendor_id;
  *product_id = model->usb_product_id;

  return SANE_STATUS_GOOD;
}

/******************************************************************************/
static SANE_Status
hp5590_init_scanner (SANE_Int dn,
		     enum proto_flags proto_flags,
		     struct scanner_info ** info,
		     enum hp_scanner_types scanner_type)
{
  struct init_resp 		init_resp;
  char 				id_buf[sizeof (init_resp.id) + 1];
  char 				ver_buf[sizeof (init_resp.version) + 1];
  SANE_Status 			ret;
  const struct hp5590_model 	*scanner_model;

  /*
   * 0A 53 49 4C 49 54 45 4B 49 50 65 6E 67 75 69 6E  .SILITEKIPenguin
   * 00 00 00 00 00 00 00 00 00 30 2E 30 36 37 09 60  .........0.067..
   * 09 60 4F B0 6D E0 00 00 00 00 00 00 00 00 00 64  ..O.m..........d
   * 03 E8                                            ..
   */
  DBG (DBG_proc, "%s\n", __FUNCTION__);

  hp5590_cmds_assert (sizeof (init_resp) == 50);

  /* Init scanner */
  ret = hp5590_cmd (dn,
  		    proto_flags,
  		    CMD_IN | CMD_VERIFY,
		    CMD_INIT,
		    (unsigned char *) &init_resp,
		    sizeof (init_resp), CORE_NONE);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  memset (id_buf, 0, sizeof (id_buf));
  memcpy (id_buf, init_resp.id, sizeof (id_buf) - 1);
  scanner_model = NULL;
  if (scanner_type != SCANNER_NONE)
    {
      unsigned int i;
      for (i = 0; i < sizeof (hp5590_models) / sizeof (struct hp5590_model);
	   i++)
	{
	  if (hp5590_models[i].scanner_type == scanner_type)
	    {
	      if (strcmp (id_buf, hp5590_models[i].vendor_id) != 0)
		{
		  DBG (DBG_err, "%s: Vendor id mismatch for scanner HP%s - "
		       "required  '%s', got '%s'\n",
		       __FUNCTION__,
		       hp5590_models[i].model,
		       hp5590_models[i].vendor_id, id_buf);
		  return SANE_STATUS_INVAL;
		}
	      scanner_model = &hp5590_models[i];
	      break;
	    }
	}
      hp5590_cmds_assert (scanner_model != NULL);
    }

  if (scanner_model)
    {
      DBG (DBG_cmds, "HP%s flags (0x%02x)\n", scanner_model->model, init_resp.flags);

      DBG (DBG_cmds, "HP%s flags: ADF %s, TMA %s, LCD %s\n",
	   scanner_model->model,
	   init_resp.flags & INIT_FLAG_ADF ? "yes" : "no",
	   init_resp.flags & INIT_FLAG_TMA ? "yes" : "no",
	   init_resp.flags & INIT_FLAG_LCD ? "yes" : "no");


      memset (ver_buf, 0, sizeof (ver_buf));
      memcpy (ver_buf, init_resp.version, sizeof (ver_buf) - 1);
      DBG (DBG_cmds, "HP%s firmware version: %s\n", scanner_model->model, ver_buf);

      DBG (DBG_cmds, "HP%s max resolution X: %u DPI\n",
	   scanner_model->model, ntohs (init_resp.max_dpi_x));
      DBG (DBG_cmds, "HP%s max resolution Y: %u DPI\n",
	   scanner_model->model, ntohs (init_resp.max_dpi_y));
      DBG (DBG_cmds, "HP%s max pixels X: %u\n",
	   scanner_model->model, ntohs (init_resp.max_pixels_x));
      DBG (DBG_cmds, "HP%s max pixels Y: %u\n",
	   scanner_model->model, ntohs (init_resp.max_pixels_y));
      DBG (DBG_cmds, "HP%s max size X: %.3f inches\n",
	   scanner_model->model,
	   ntohs (init_resp.max_pixels_x) * 1.0 /
	   ntohs (init_resp.max_dpi_x));
      DBG (DBG_cmds, "HP%s max size Y: %.3f inches\n", scanner_model->model,
	   ntohs (init_resp.max_pixels_y) * 1.0 /
	   ntohs (init_resp.max_dpi_y));
      DBG (DBG_cmds, "HP%s normal motor param: %u, max motor param: %u\n",
	   scanner_model->model, ntohs (init_resp.motor_param_normal),
	   ntohs (init_resp.motor_param_max));
    }

  if (info)
    {
      *info = malloc (sizeof (struct scanner_info));
      if (!*info)
	{
	  DBG (DBG_err, "Memory allocation failed\n");
	  return SANE_STATUS_NO_MEM;
	}
      memset (*info, 0, sizeof (struct scanner_info));

      (*info)->max_dpi_x = ntohs (init_resp.max_dpi_x);
      (*info)->max_dpi_y = ntohs (init_resp.max_dpi_y);
      (*info)->max_pixels_x = ntohs (init_resp.max_pixels_x) - 1;
      (*info)->max_pixels_y = ntohs (init_resp.max_pixels_y) + 1;
      (*info)->max_size_x = (*info)->max_pixels_x * 1.0 / (*info)->max_dpi_x;
      (*info)->max_size_y = (*info)->max_pixels_y * 1.0 / (*info)->max_dpi_y;
      (*info)->features = FEATURE_NONE;
      if (init_resp.flags & INIT_FLAG_LCD)
	(*info)->features |= FEATURE_LCD;
      if (init_resp.flags & INIT_FLAG_ADF)
	(*info)->features |= FEATURE_ADF;
      if (init_resp.flags & INIT_FLAG_TMA)
	(*info)->features |= FEATURE_TMA;
      if (scanner_model)
	{
	  (*info)->model = scanner_model->model;
	  (*info)->kind = scanner_model->kind;
	}
    }

  ret = hp5590_get_status (dn, proto_flags);
  if (ret != SANE_STATUS_GOOD)
    {
      DBG (DBG_err, "%s: scanner reports non-zero status: %s\n",
	   __FUNCTION__, sane_strstatus (ret));
      return ret;
    }
  DBG (DBG_cmds, "%s: scanner status OK\n", __FUNCTION__);

  return SANE_STATUS_GOOD;
}

/******************************************************************************/
static SANE_Status
hp5590_read_eeprom (SANE_Int dn,
		    enum proto_flags proto_flags,
		    unsigned int addr,
		    unsigned char *data, unsigned int size)
{
  uint8_t eeprom_addr = addr;
  SANE_Status ret;

  hp5590_cmds_assert (data != NULL);
  hp5590_cmds_assert (sizeof (eeprom_addr) == 1);

  DBG (DBG_proc, "%s\n", __FUNCTION__);
  DBG (DBG_proc, "Reading EEPROM: addr %04x, size %u\n", addr, size);

  ret = hp5590_cmd (dn,
  		    proto_flags,
  		    CMD_VERIFY,
		    CMD_EEPROM_ADDR,
		    (unsigned char *) &eeprom_addr,
		    sizeof (eeprom_addr), CORE_NONE);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  ret = hp5590_cmd (dn,
  		    proto_flags,
		    CMD_IN | CMD_VERIFY,
		    CMD_EEPROM_READ, data, size, CORE_NONE);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  return SANE_STATUS_GOOD;
}

/******************************************************************************/
static SANE_Status
hp5590_write_eeprom (SANE_Int dn,
		     enum proto_flags proto_flags,
		     unsigned int addr,
		     unsigned char *data, unsigned int size)
{
  uint8_t eeprom_addr = addr;
  SANE_Status ret;

  hp5590_cmds_assert (data != NULL);
  hp5590_cmds_assert (sizeof (eeprom_addr) == 1);

  DBG (DBG_proc, "%s\n", __FUNCTION__);
  DBG (DBG_proc, "Writing EEPROM: addr %04x, size: %u\n", addr, size);

  ret = hp5590_cmd (dn,
  		    proto_flags,
  		    CMD_VERIFY,
		    CMD_EEPROM_ADDR,
		    (unsigned char *) &eeprom_addr,
		    sizeof (eeprom_addr), CORE_NONE);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  ret = hp5590_cmd (dn,
  		    proto_flags,
		    CMD_VERIFY,
		    CMD_EEPROM_WRITE, data, size, CORE_DATA);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  return SANE_STATUS_GOOD;
}

/******************************************************************************/
static SANE_Status
hp5590_read_scan_count (SANE_Int dn,
			enum proto_flags proto_flags,
			unsigned int *count)
{
  uint32_t scan_count;
  SANE_Status ret;

  hp5590_cmds_assert (count != NULL);
  hp5590_cmds_assert (sizeof (scan_count) == 4);

  DBG (DBG_proc, "%s\n", __FUNCTION__);
  DBG (DBG_proc, "Reading scan count\n");

  ret = hp5590_read_eeprom (dn,
  			    proto_flags,
  			    0x00,
			    (unsigned char *) &scan_count,
			    sizeof (scan_count));

  if (ret != SANE_STATUS_GOOD)
    return ret;

  /* Host order */
  *count = scan_count;

  DBG (DBG_proc, "Scan count %u\n", *count);

  return SANE_STATUS_GOOD;
}

/******************************************************************************/
static SANE_Status
hp5590_inc_scan_count (SANE_Int dn,
		       enum proto_flags proto_flags)
{
  uint32_t scan_count;
  unsigned int count;
  unsigned int new_count;
  SANE_Status ret;

  DBG (DBG_proc, "%s\n", __FUNCTION__);
  hp5590_cmds_assert (sizeof (scan_count) == 4);

  ret = hp5590_read_scan_count (dn, proto_flags, &count);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  scan_count = ++count;

  ret = hp5590_write_eeprom (dn,
  			     proto_flags,
  			     0x00,
			     (unsigned char *) &scan_count,
			     sizeof (scan_count));
  if (ret != SANE_STATUS_GOOD)
    return ret;

  /* Verify its setting */
  ret = hp5590_read_scan_count (dn, proto_flags, &new_count);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  if (count != new_count)
    {
      DBG (DBG_err, "Scan count wasn't set\n");
      return SANE_STATUS_IO_ERROR;
    }

  return SANE_STATUS_GOOD;
}

/******************************************************************************/
static SANE_Status
hp5590_read_max_scan_count (SANE_Int dn,
			    enum proto_flags proto_flags,
			    unsigned int *max_count)
{
  uint8_t max_scan_count[3];
  SANE_Status ret;

  hp5590_cmds_assert (max_count != NULL);
  hp5590_cmds_assert (sizeof (max_scan_count) == 3);

  DBG (DBG_proc, "%s\n", __FUNCTION__);
  DBG (DBG_proc, "Reading max scan count\n");

  ret = hp5590_read_eeprom (dn,
  			    proto_flags,
			    0x10,
			    (unsigned char *) max_scan_count,
			    sizeof (max_scan_count));
  if (ret != SANE_STATUS_GOOD)
    return ret;

  /* Host order */
  *max_count = 0;
  memcpy (max_count, max_scan_count, sizeof (max_scan_count));

  DBG (DBG_proc, "Max scan count %u\n", *max_count);

  return SANE_STATUS_GOOD;
}

/***************************************************************************
 * 
 * EEPROM contents:
 * 
 * 0000: 6A 11 00 00 FF FF FF FF FF FF FF FF 09 0E 0F 00  j...............
 * 0010: 0C 13 0F 00 00 3A 00 FF FF FF 4E 35 39 45 54 52  ..........N59ETR
 * 0020: 31 52 4D 00 FF FF 00 16 00 0A 00 0D 00 11 00 10  1RM.............
 * 0030: FF FF FF FF FF FF FF FF FF 00 FF FF FF FF FF FF  ................
 * 0040: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
 * 0050: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
 * 0060: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
 * 0070: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
 * 0080: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
 * 0090: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
 * 00A0: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
 * 00B0: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
 * 00C0: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
 * 00D0: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
 * 00E0: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF  ................
 * 00F0: FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF     ...............
 *
 * Addr 0x00, len: 0x04 - scan count (little-endian)
 * Addr 0x1A, len: 0x0A - part number (w/o first 'CN' letters)
 * Addr 0x10, len: 0x03 - max scan count (little-endian) (0C 13 0F)
 *
 */

/******************************************************************************/
static __sane_unused__ SANE_Status
hp5590_read_eeprom_all_cmd (SANE_Int dn,
			    enum proto_flags proto_flags)
{
  uint8_t eeprom[255];
  SANE_Status ret;

  DBG (DBG_proc, "%s\n", __FUNCTION__);

  ret = hp5590_read_eeprom (dn,
  			    proto_flags,
  			    0x00,
			    (unsigned char *) eeprom,
			    sizeof (eeprom));
  if (ret != SANE_STATUS_GOOD)
    return ret;

  /* FIXME: Debug output */

  return SANE_STATUS_GOOD;
}

/******************************************************************************/
static SANE_Status
hp5590_read_part_number (SANE_Int dn,
			 enum proto_flags proto_flags)
{
  unsigned int part_number_len = PART_NUMBER_LEN;
  unsigned char part_number[PART_NUMBER_LEN + 1];
  SANE_Status ret;

  DBG (DBG_proc, "%s\n", __FUNCTION__);

  memset (part_number, 0, sizeof (part_number));
  ret = hp5590_read_eeprom (dn,
  			    proto_flags,
			    0x1a,
			    part_number, part_number_len);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  DBG (DBG_cmds, "Part number: '%s'\n", part_number);

  return SANE_STATUS_GOOD;
}

/******************************************************************************/
static SANE_Status
hp5590_is_data_available (SANE_Int dn,
			  enum proto_flags proto_flags)
{
  uint8_t data_status;
  SANE_Status ret;
  SANE_Bool data_available;

  DBG (DBG_proc, "%s\n", __FUNCTION__);

  hp5590_cmds_assert (sizeof (data_status) == 1);
  data_available = SANE_FALSE;

  ret = hp5590_cmd (dn,
  		    proto_flags,
  		    CMD_IN | CMD_VERIFY,
		    CMD_DATA_STATUS,
		    (unsigned char *) &data_status,
		    sizeof (data_status), CORE_DATA);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  DBG (DBG_cmds, "%s: Data status: %02x\n", __FUNCTION__, data_status);

  if (data_status == 0x40)
    data_available = SANE_TRUE;

  DBG (DBG_cmds, "%s: Data is %s\n",
       __FUNCTION__,
       data_available == SANE_TRUE ? "available" : "not available");

  return data_available == SANE_TRUE ? SANE_STATUS_GOOD : SANE_STATUS_NO_DOCS;
}

/******************************************************************************/
static SANE_Status
hp5590_stop_scan (SANE_Int dn,
		  enum proto_flags proto_flags)
{
  uint8_t reg_011b = 0x40;
  SANE_Status ret;

  DBG (DBG_proc, "%s\n", __FUNCTION__);

  hp5590_cmds_assert (sizeof (reg_011b) == 1);

  ret = hp5590_cmd (dn,
  		    proto_flags,
  		    CMD_VERIFY,
		    CMD_STOP_SCAN,
		    (unsigned char *) &reg_011b,
		    sizeof (reg_011b), CORE_NONE);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  /* FIXME */

  return SANE_STATUS_GOOD;
}

/******************************************************************************/
static SANE_Status
hp5590_turnon_lamp (SANE_Int dn,
		    enum proto_flags proto_flags,
		    enum hp5590_lamp_state state)
{
  struct lamp_state lamp_state;
  SANE_Status ret;

  DBG (DBG_proc, "%s\n", __FUNCTION__);

  hp5590_cmds_assert (sizeof (lamp_state) == 4);

  if (state == LAMP_STATE_TURNON)
    {
      /* Turn on lamp */
      lamp_state.unk1 = 0x02;
      lamp_state.flag = 0x01;
      lamp_state.turnoff_time = htons (0x0a0a);
      DBG (DBG_cmds, "%s: turning lamp on\n", __FUNCTION__);
    }

  if (state == LAMP_STATE_TURNOFF)
    {
      /* Turn off lamp */
      lamp_state.unk1 = 0x02;
      lamp_state.flag = 0x02;
      lamp_state.turnoff_time = htons (0x0a0a);
      DBG (DBG_cmds, "%s: turning lamp off\n", __FUNCTION__);
    }

  if (state == LAMP_STATE_SET_TURNOFF_TIME)
    {
      /* Turn on lamp */
      lamp_state.unk1 = 0x02;
      lamp_state.flag = 0x03;
      lamp_state.turnoff_time = htons (0x0336);
      DBG (DBG_cmds, "%s: setting turnoff time\n", __FUNCTION__);
    }

  if (state == LAMP_STATE_SET_TURNOFF_TIME_LONG)
    {
      /* Turn on lamp */
      lamp_state.unk1 = 0x02;
      lamp_state.flag = 0x03;
      lamp_state.turnoff_time = htons (0x0f36);
      DBG (DBG_cmds, "%s: setting long turnoff time\n", __FUNCTION__);
    }

  ret = hp5590_cmd (dn,
  		    proto_flags,
  		    CMD_VERIFY,
		    CMD_CONTROL_LAMP,
		    (unsigned char *) &lamp_state,
		    sizeof (lamp_state), CORE_DATA);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  if (state == LAMP_STATE_TURNON)
    {
      ret = hp5590_init_scanner (dn, proto_flags, NULL, SCANNER_NONE);
      if (ret != SANE_STATUS_GOOD)
	return ret;
    }

  return SANE_STATUS_GOOD;
}

/******************************************************************************/
static SANE_Status
hp5590_power_status (SANE_Int dn,
		     enum proto_flags proto_flags)
{
  struct power_resp power_resp;
  SANE_Status ret;

  DBG (DBG_proc, "%s\n", __FUNCTION__);

  hp5590_cmds_assert (sizeof (power_resp) == 3);

  ret = hp5590_cmd (dn,
  		    proto_flags,
  		    CMD_IN | CMD_VERIFY,
		    CMD_POWER_STATUS,
		    (unsigned char *) &power_resp,
		    sizeof (power_resp), CORE_NONE);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  DBG (DBG_cmds, "Power status: %s (%02x)\n",
       power_resp.flags & POWER_FLAG_ON ? "on" : "off", power_resp.flags);

  if (!(power_resp.flags & POWER_FLAG_ON))
    {
      DBG (DBG_cmds, "Turning lamp on\n");
      ret = hp5590_turnon_lamp (dn, proto_flags, LAMP_STATE_TURNON);
      if (ret != SANE_STATUS_GOOD)
	return ret;
    }

  return SANE_STATUS_GOOD;
}

/******************************************************************************/
static SANE_Status
hp5590_read_error_code (SANE_Int dn,
			enum proto_flags proto_flags,
			unsigned int *adf_flags)
{
  struct reg_03 reg_03;
  SANE_Status 	ret;

  DBG (DBG_proc, "%s\n", __FUNCTION__);
  
  hp5590_cmds_assert (sizeof (reg_03) == 3);
  hp5590_cmds_assert (adf_flags != NULL);

  memset (&reg_03, 0, sizeof (reg_03));
  *adf_flags = 0;
  ret = hp5590_cmd (dn,
  		    proto_flags,
  		    CMD_IN,
		    CMD_MISC_STATUS,
		    (unsigned char *) &reg_03,
		    sizeof (reg_03), CORE_NONE);

  if (ret != SANE_STATUS_GOOD)
    return ret;

  DBG (DBG_cmds, "%s: adf_flags: %04x\n", __FUNCTION__, reg_03.adf_flags);
  DBG (DBG_cmds, "%s: unk1     : %04x\n", __FUNCTION__, reg_03.unk1);
  DBG (DBG_cmds, "%s: unk2     : %04x\n", __FUNCTION__, reg_03.unk2);

  *adf_flags = reg_03.adf_flags;

  return SANE_STATUS_GOOD;
}

/******************************************************************************/
static SANE_Status
hp5590_reset_scan_head (SANE_Int dn,
			enum proto_flags proto_flags)
{
  SANE_Status	ret;

  DBG (DBG_proc, "%s\n", __FUNCTION__);
  
  ret = hp5590_turnon_lamp (dn, proto_flags, LAMP_STATE_TURNOFF);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  usleep (100 * 1000);

  ret = hp5590_turnon_lamp (dn, proto_flags, LAMP_STATE_TURNON);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  return SANE_STATUS_GOOD;
}

/******************************************************************************/
static SANE_Status
hp5590_select_source_and_wakeup (SANE_Int dn,
				 enum proto_flags proto_flags,
				 enum scan_sources source,
				 SANE_Bool extend_lamp_timeout)
{
  uint8_t 	reg_d6 = 0x04;
  SANE_Status 	ret;
  unsigned int	adf_flags;

  DBG (DBG_proc, "%s\n", __FUNCTION__);

  hp5590_cmds_assert (sizeof (reg_d6) == 1);

  if (source == SOURCE_TMA_SLIDES || source == SOURCE_TMA_NEGATIVES)
    {
      ret = hp5590_turnon_lamp (dn, proto_flags, LAMP_STATE_TURNOFF);
      if (ret != SANE_STATUS_GOOD)
	return ret;
    }
  else
    {
      ret = hp5590_turnon_lamp (dn,
				proto_flags,
				extend_lamp_timeout == SANE_TRUE ?
				LAMP_STATE_SET_TURNOFF_TIME_LONG :
				LAMP_STATE_SET_TURNOFF_TIME);
      if (ret != SANE_STATUS_GOOD)
	return ret;
    }

  switch (source)
    {
      case SOURCE_ADF:
      case SOURCE_ADF_DUPLEX:
	reg_d6 = 0x03;
	break;
      case SOURCE_FLATBED:
	reg_d6 = 0x04;
	break;
      case SOURCE_TMA_SLIDES:
	reg_d6 = 0x02;
	break;
      case SOURCE_TMA_NEGATIVES:
	reg_d6 = 0x01;
	break;
      case SOURCE_NONE:
	DBG (DBG_err, "Scan source not selected\n");
	return SANE_STATUS_INVAL;
      default:
	DBG (DBG_err, "Unknown scan source: %u\n", source);
	return SANE_STATUS_INVAL;
    }

  DBG (DBG_cmds, "Scan source: %u\n", reg_d6);

  ret = hp5590_cmd (dn,
  		    proto_flags,
  		    CMD_VERIFY,
		    CMD_SELECT_SOURCE,
		    (unsigned char *) &reg_d6,
		    sizeof (reg_d6), CORE_NONE);

  if (ret != SANE_STATUS_GOOD && ret != SANE_STATUS_DEVICE_BUSY)
    return ret;

  ret = hp5590_read_error_code (dn, proto_flags, &adf_flags);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  if (adf_flags & FLAG_ADF_EMPTY)
    {
      DBG (DBG_cmds, "ADF empty\n");
      return SANE_STATUS_NO_DOCS;
    }

  return SANE_STATUS_GOOD;
}

/******************************************************************************/
static SANE_Status
hp5590_lock_unlock_scanner (SANE_Int dn,
			    enum proto_flags proto_flags)
{
  uint8_t 	reg_00 = 0x01;
  SANE_Status 	ret;
  unsigned int	adf_flags;
  unsigned int	waiting;

  DBG (DBG_proc, "%s\n", __FUNCTION__);
  hp5590_cmds_assert (sizeof (reg_00) == 1);

  for (waiting = 0; waiting < WAKEUP_TIMEOUT; waiting++, sleep (1))
    {
      ret = hp5590_cmd (dn,
      			proto_flags,
      			CMD_VERIFY,
			CMD_LOCK_UNLOCK,
			(unsigned char *) &reg_00,
			sizeof (reg_00), CORE_NONE);
      if (ret == SANE_STATUS_GOOD)
	break;

      if (ret != SANE_STATUS_DEVICE_BUSY)
	return ret;

      DBG (DBG_cmds, "Waiting for scanner...\n");
      ret = hp5590_read_error_code (dn, proto_flags, &adf_flags);
      if (ret != SANE_STATUS_GOOD)
	return ret;

      if (adf_flags & FLAG_ADF_EMPTY)
	{
	  DBG (DBG_cmds, "ADF empty\n");
	  return SANE_STATUS_NO_DOCS;
	}
    }

  if (waiting == WAKEUP_TIMEOUT)
    return SANE_STATUS_DEVICE_BUSY;

  return SANE_STATUS_GOOD;
}

/******************************************************************************/
static SANE_Status
hp5590_set_base_dpi (SANE_Int dn,
		     enum proto_flags proto_flags,
		     struct scanner_info *scanner_info,
		     unsigned int base_dpi)
{
  uint16_t _base_dpi;
  SANE_Status ret;

  DBG (DBG_proc, "%s\n", __FUNCTION__);

  hp5590_cmds_assert (scanner_info != NULL);
  hp5590_cmds_assert (base_dpi != 0);
  hp5590_cmds_assert (sizeof (_base_dpi) == 2);

  if (base_dpi > scanner_info->max_dpi_x
      || base_dpi > scanner_info->max_dpi_y)
    {
      DBG (DBG_err, "Base DPI too large "
	   "(given: %u, max X DPI: %u, max Y DPI: %u)\n",
	   base_dpi,
	   scanner_info->max_dpi_x,
	   scanner_info->max_dpi_y);
      return SANE_STATUS_INVAL;
    }

  _base_dpi = htons (base_dpi);

  ret = hp5590_cmd (dn,
  		    proto_flags,
  		    CMD_VERIFY,
		    CMD_SET_BASE_DPI,
		    (unsigned char *) &_base_dpi,
		    sizeof (_base_dpi), CORE_DATA);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  return SANE_STATUS_GOOD;
}

/******************************************************************************/
static SANE_Status
hp5590_set_color_map (SANE_Int dn,
		      enum proto_flags proto_flags,
		      unsigned int base_dpi)
{
  struct color_map color_map;
  SANE_Status ret;

  DBG (DBG_proc, "%s\n", __FUNCTION__);

  hp5590_cmds_assert (sizeof (color_map) == 18);
  hp5590_cmds_assert (base_dpi != 0);

  memset (&color_map, 0, sizeof (color_map));
  if (base_dpi < 2400)
    {
      color_map.color1[4] = 0x01;
      color_map.color2[4] = 0x01;
      color_map.color3[4] = 0x01;
    }
  else
    {
      color_map.color1[2] = 0xff;
      color_map.color1[3] = 0x01;
      color_map.color1[4] = 0x04;
      color_map.color1[5] = 0x02;

      color_map.color2[2] = 0xff;
      color_map.color2[3] = 0x01;
      color_map.color2[4] = 0x04;
      color_map.color2[5] = 0x02;

      color_map.color3[2] = 0xff;
      color_map.color3[3] = 0x01;
      color_map.color3[4] = 0x04;
      color_map.color3[5] = 0x02;
    }

  ret = hp5590_cmd (dn,
  		    proto_flags,
  		    CMD_VERIFY,
		    CMD_SET_COLOR_MAP,
		    (unsigned char *) &color_map,
		    sizeof (color_map), CORE_DATA);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  return SANE_STATUS_GOOD;
}

/******************************************************************************
 * Calculate base DPI
 * Base DPI is what image dimensions are calculated on (top X,Y; bottom X,Y)
 * Calculated according the following rules:
 * Base DPI is 150 when   0 <  DPI < 150;
 * Base DPI is 300 when 150 <= DPI <= 300;
 * Base DPI is 600 when 300 < DPI <= 600;
 * Base DPI is 1200 when 600 < DPI;
 */
static SANE_Status
calc_base_dpi (unsigned int dpi, unsigned int *base_dpi)
{

  DBG (DBG_proc, "%s\n", __FUNCTION__);

  hp5590_cmds_assert (base_dpi != NULL);
  hp5590_cmds_assert (dpi != 0);

  *base_dpi = 0;

  if (dpi < 150)
    {
      *base_dpi = 150;
      return SANE_STATUS_GOOD;
    }

  if (dpi >= 150 && dpi <= 300)
    {
      *base_dpi = 300;
      return SANE_STATUS_GOOD;
    }

  if (dpi > 300 && dpi <= 600)
    {
      *base_dpi = 600;
      return SANE_STATUS_GOOD;
    }

  if (dpi > 600 && dpi <= 1200)
    {
      *base_dpi = 1200;
      return SANE_STATUS_GOOD;
    }

  if (dpi > 1200 && dpi <= 2400)
    {
      *base_dpi = 2400;
      return SANE_STATUS_GOOD;
    }

  DBG (DBG_err, "Error calculating base DPI (given DPI: %u)\n", dpi);
  return SANE_STATUS_INVAL;
}

/******************************************************************************/
static SANE_Status
calc_scanner_dpi (unsigned int dpi, unsigned int *scanner_dpi)
{
  DBG (DBG_proc, "%s\n", __FUNCTION__);

  hp5590_cmds_assert (scanner_dpi != NULL);
  hp5590_cmds_assert (dpi != 0);

  if (dpi <= 100)
    {
      *scanner_dpi = 100;
      return SANE_STATUS_GOOD;
    }

  if (dpi > 100 && dpi <= 200)
    {
      *scanner_dpi = 200;
      return SANE_STATUS_GOOD;
    }

  if (dpi == 300)
    {
      *scanner_dpi = 300;
      return SANE_STATUS_GOOD;
    }

  if (dpi > 300 && dpi <= 600)
    {
      *scanner_dpi = 600;
      return SANE_STATUS_GOOD;
    }

  if (dpi > 600 && dpi <= 1200)
    {
      *scanner_dpi = 1200;
      return SANE_STATUS_GOOD;
    }

  if (dpi > 1200 && dpi <= 2400)
    {
      *scanner_dpi = 2400;
      return SANE_STATUS_GOOD;
    }

  DBG (DBG_err, "Error calculating scanner DPI (given DPI: %u)\n", dpi);
  return SANE_STATUS_INVAL;
}

/******************************************************************************/
static SANE_Status
hp5590_calc_pixel_bits (unsigned int dpi, enum color_depths color_depth,
			unsigned int *pixel_bits)
{
  unsigned int scanner_dpi;
  SANE_Status ret;

  DBG (DBG_proc, "%s\n", __FUNCTION__);

  hp5590_cmds_assert (pixel_bits != NULL);
  hp5590_cmds_assert (dpi != 0);

  ret = calc_scanner_dpi (dpi, &scanner_dpi);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  if (color_depth == DEPTH_COLOR_48)
    {
      *pixel_bits = 48;
      return SANE_STATUS_GOOD;
    }

  if (color_depth == DEPTH_COLOR_24)
    {
      *pixel_bits = 24;
      return SANE_STATUS_GOOD;
    }

  if (color_depth == DEPTH_GRAY)
    {
      *pixel_bits = 8;
      return SANE_STATUS_GOOD;
    }

  if (color_depth == DEPTH_BW)
    {
      if (dpi == scanner_dpi)
	*pixel_bits = 1;
      else
	*pixel_bits = 8;
      return SANE_STATUS_GOOD;
    }

  DBG (DBG_err, "Error calculating pixel bits (given DPI: %u)\n", dpi);
  return SANE_STATUS_INVAL;
}

/******************************************************************************/
static SANE_Status
hp5590_set_scan_area (SANE_Int dn,
		      enum proto_flags proto_flags,
		      struct scanner_info *scanner_info,
		      unsigned int top_x, unsigned int top_y,
		      unsigned int width, unsigned int height,
		      unsigned int dpi, enum color_depths color_depth,
		      enum scan_modes scan_mode,
		      enum scan_sources scan_source)
{
  struct scan_params	scan_params;
  unsigned int 		scanner_top_x;
  unsigned int 		scanner_top_y;
  unsigned int 		scanner_pixels_x;
  unsigned int		scanner_pixels_y;
  unsigned int 		base_dpi;
  unsigned int 		scanner_dpi;
  unsigned int 		pixel_bits;
  unsigned int 		scanner_line_width;
  unsigned int 		max_pixels_x_current_dpi;
  unsigned int		max_pixels_y_current_dpi;
  unsigned int 		pixels_x;
  unsigned int 		pixels_y;
  SANE_Status 		ret;

  DBG (DBG_proc, "%s\n", __FUNCTION__);

  hp5590_cmds_assert (sizeof (scan_params) == 37);
  hp5590_cmds_assert (dpi != 0);
  hp5590_cmds_assert (scanner_info != NULL);

  memset (&scan_params, 0, sizeof (scan_params));

  scan_params.source = SCAN_PARAMS_SOURCE_SIMPLEX;
  if (scan_source == SOURCE_ADF)
    scan_params.source |= SCAN_PARAMS_SOURCE_ADF;
  if (scan_source == SOURCE_ADF_DUPLEX)
    scan_params.source |= SCAN_PARAMS_SOURCE_ADF | SCAN_PARAMS_SOURCE_DUPLEX;
  if (scan_source == SOURCE_FLATBED)
    scan_params.source |= SCAN_PARAMS_SOURCE_FLATBED;
  if (scan_source == SOURCE_TMA_SLIDES)
    scan_params.source |= SCAN_PARAMS_SOURCE_TMA_SLIDES;
  if (scan_source == SOURCE_TMA_NEGATIVES)
    scan_params.source |= SCAN_PARAMS_SOURCE_TMA_NEGATIVES;

  DBG (DBG_cmds, "Scan params. source : 0x%04x\n", scan_params.source);

  DBG (DBG_cmds, "DPI: %u\n", dpi);
  if (dpi > scanner_info->max_dpi_x || dpi > scanner_info->max_dpi_y)
    {
      DBG (DBG_err, "DPI too large "
	   "(given: %u, max X DPI: %u, max Y DPI: %u)\n",
	   dpi, scanner_info->max_dpi_x, scanner_info->max_dpi_y);
      return SANE_STATUS_INVAL;
    }

  ret = calc_base_dpi (dpi, &base_dpi);
  if (ret != SANE_STATUS_GOOD)
    return ret;
  DBG (DBG_cmds, "Base DPI: %u\n", base_dpi);

  ret = calc_scanner_dpi (dpi, &scanner_dpi);
  if (ret != SANE_STATUS_GOOD)
    return ret;
  DBG (DBG_cmds, "Scanner DPI: %u\n", scanner_dpi);

  scan_params.dpi_x = htons (scanner_dpi);
  scan_params.dpi_y = htons (scanner_dpi);

  DBG (DBG_cmds, "DPI X: 0x%04x\n", scanner_dpi);
  DBG (DBG_cmds, "DPI Y: 0x%04x\n", scanner_dpi);

  ret = hp5590_calc_pixel_bits (dpi, color_depth, &pixel_bits);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  DBG (DBG_cmds, "Pixel bits: %u\n", pixel_bits);

  scan_params.pixel_bits = pixel_bits;

  scan_params.bw_gray_flag = 0;
  if (color_depth == DEPTH_BW || color_depth == DEPTH_GRAY)
    scan_params.bw_gray_flag = htons (0x40);

  scan_params.flags = htons (0xe840);
  if (dpi > 300 && dpi <= 1200)
    scan_params.flags = htons (0xc840);
  if (dpi > 1200)
    scan_params.flags = htons (0xc040);

  scan_params.motor_param1 = htons (100);
  scan_params.motor_param2 = htons (100);
  scan_params.motor_param3 = htons (100);
  if (scan_source == SOURCE_TMA_NEGATIVES)
    {
      scan_params.motor_param2 = htons (200);
      scan_params.motor_param3 = htons (400);
    }

  scan_params.unk1 = htons (0x80);

  scan_params.mode = 0;
  if (scan_mode == MODE_PREVIEW)
    scan_params.mode = 0x04;

  max_pixels_x_current_dpi = (float) scanner_info->max_size_x * dpi;
  max_pixels_y_current_dpi = (float) scanner_info->max_size_y * dpi;
  if (   scan_source == SOURCE_TMA_NEGATIVES
      || scan_source == SOURCE_TMA_SLIDES)
    {
      max_pixels_x_current_dpi = (float) (TMA_MAX_X_INCHES * dpi);
      max_pixels_y_current_dpi = (float) (TMA_MAX_Y_INCHES * dpi);
    }
  /* In ADF mode the device can scan up to ADF_MAX_Y_INCHES, which is usually
   * bigger than what scanner reports back during initialization
   */
  if ( scan_source == SOURCE_ADF )
    max_pixels_y_current_dpi = (float) (ADF_MAX_Y_INCHES * dpi);

  /* Allow two times of max pixels for ADF Duplex mode */
  if (scan_source == SOURCE_ADF_DUPLEX)
    max_pixels_y_current_dpi *= 2;

  pixels_x = width;
  pixels_y = height;

  scanner_top_x = (float) (top_x * (1.0 * base_dpi / dpi));
  scanner_top_y = (float) (top_y * (1.0 * base_dpi / dpi));
  scanner_pixels_x = (float) (pixels_x * (1.0 * base_dpi / dpi));
  scanner_pixels_y = (float) (pixels_y * (1.0 * base_dpi / dpi));

  DBG (DBG_cmds, "Top X: %u, top Y: %u, size X: %u, size Y: %u\n",
       top_x, top_y, pixels_x, pixels_y);
  DBG (DBG_cmds, "Scanner top X: %u, top Y: %u, size X: %u, size Y: %u\n",
       scanner_top_x, scanner_top_y,
       scanner_pixels_x, scanner_pixels_y);

  if (top_x + pixels_x > max_pixels_x_current_dpi)
    {
      DBG (DBG_err, "Top X (%u) + pixels X (%u) exceedes max X %u\n",
	   top_x, pixels_x, max_pixels_x_current_dpi);
      return SANE_STATUS_INVAL;
    }

  if (top_y + pixels_y > max_pixels_y_current_dpi)
    {
      DBG (DBG_err, "Top Y (%u) + pixels Y (%u) exceedes max Y %u\n",
	   top_y, pixels_y, max_pixels_y_current_dpi);
      return SANE_STATUS_INVAL;
    }

  scan_params.top_x = htons (scanner_top_x);
  scan_params.top_y = htons (scanner_top_y);
  scan_params.size_x = htons (scanner_pixels_x);
  scan_params.size_y = htons (scanner_pixels_y);

  scanner_line_width = (float) (pixels_x
				* (1.0 * scanner_dpi / dpi) / 8 * pixel_bits);

  /* Scanner hangs at scan command if line width less than 18 */
  if (scanner_line_width < 18)
    {
      DBG (DBG_err, "Line width too smal, extending to minimum\n");
      scanner_line_width = 18;
    }
  scan_params.line_width = htons (scanner_line_width);
  DBG (DBG_cmds, "Line width: %u\n", scanner_line_width);

  ret = hp5590_cmd (dn,
  		    proto_flags,
  		    CMD_VERIFY,
		    CMD_SET_SCAN_PARAMS,
		    (unsigned char *) &scan_params,
		    sizeof (scan_params), CORE_DATA);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  return SANE_STATUS_GOOD;
}

/******************************************************************************/
static SANE_Status
hp5590_read_image_params (SANE_Int dn,
			  enum proto_flags proto_flags)
{
  struct image_params image_params;
  SANE_Status ret;

  DBG (DBG_proc, "%s\n", __FUNCTION__);

  hp5590_cmds_assert (sizeof (image_params) == 16);

  memset (&image_params, 0, sizeof (image_params));

  ret = hp5590_cmd (dn,
  		    proto_flags,
  		    CMD_IN | CMD_VERIFY,
		    CMD_GET_IMAGE_PARAMS,
		    (unsigned char *) &image_params,
		    sizeof (image_params), CORE_NONE);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  if (image_params.signature != 0xc0)
    {
      DBG (DBG_err, "Wrong signature for image parameters structure "
	   "received (needed 0xc0, got %02x)\n", image_params.signature);
      return SANE_STATUS_IO_ERROR;
    }
  DBG (DBG_cmds, "Received image params:\n");
  DBG (DBG_cmds, "Signature %02x\n", image_params.signature);
  DBG (DBG_cmds, "Image size %lu (%04lx)\n",
       (unsigned long) ntohl (image_params.image_size),
       (unsigned long) ntohl (image_params.image_size));
  DBG (DBG_cmds, "Line width: %u (%02x)\n", ntohs (image_params.line_width),
       ntohs (image_params.line_width));
  DBG (DBG_cmds, "Actual size Y: %u (%02x)\n",
       ntohs (image_params.real_size_y), ntohs (image_params.real_size_y));

  return SANE_STATUS_GOOD;
}

/******************************************************************************/
static SANE_Status
hp5590_set_scan_params (SANE_Int dn,
			enum proto_flags proto_flags,
			struct scanner_info * scanner_info,
			unsigned int top_x, unsigned int top_y,
			unsigned int width, unsigned int height,
			unsigned int dpi, enum color_depths color_depth,
			enum scan_modes scan_mode,
			enum scan_sources scan_source)
{
  unsigned int base_dpi;
  SANE_Status ret;

  DBG (DBG_proc, "%s\n", __FUNCTION__);

  hp5590_cmds_assert (scanner_info != NULL);
  hp5590_cmds_assert (dpi != 0);

  /* Lock scanner */
  ret = hp5590_lock_unlock_scanner (dn, proto_flags);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  /* Set base DPI */
  ret = calc_base_dpi (dpi, &base_dpi);
  if (ret != SANE_STATUS_GOOD)
    {
      /* Unlock scanner */
      hp5590_lock_unlock_scanner (dn, proto_flags);
      return ret;
    }

  DBG (DBG_cmds, "Set base DPI: %u\n", base_dpi);
  ret = hp5590_set_base_dpi (dn, proto_flags, scanner_info, base_dpi);
  if (ret != SANE_STATUS_GOOD)
    {
      /* Unlock scanner */
      hp5590_lock_unlock_scanner (dn, proto_flags);
      return ret;
    }

  /* Set color map */
  ret = hp5590_set_color_map (dn, proto_flags, base_dpi);
  if (ret != SANE_STATUS_GOOD)
    {
      /* Unlock scanner */
      hp5590_lock_unlock_scanner (dn, proto_flags);
      return ret;
    }

  ret = hp5590_set_scan_area (dn,
  			      proto_flags,
  			      scanner_info,
			      top_x, top_y,
			      width, height,
			      dpi, color_depth, scan_mode, scan_source);
  if (ret != SANE_STATUS_GOOD)
    {
      /* Unlock scanner */
      hp5590_lock_unlock_scanner (dn, proto_flags);
      return ret;
    }

  ret = hp5590_read_image_params (dn, proto_flags);
  if (ret != SANE_STATUS_GOOD)
    {
      /* Unlock scanner */
      hp5590_lock_unlock_scanner (dn, proto_flags);
      return ret;
    }

  /* Unlock scanner */
  ret = hp5590_lock_unlock_scanner (dn, proto_flags);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  return SANE_STATUS_GOOD;
}

/******************************************************************************/
static SANE_Status
hp5590_send_reverse_calibration_map (SANE_Int dn,
				     enum proto_flags proto_flags)
{
  unsigned int reverse_map_size = REVERSE_MAP_LEN;
  uint16_t reverse_map[REVERSE_MAP_LEN];
  unsigned int i;
  uint16_t val;
  unsigned int len;
  SANE_Status ret;

  DBG (DBG_proc, "%s\n", __FUNCTION__);
  DBG (DBG_proc, "Preparing reverse calibration map\n");
  val = 0xffff;
  len = reverse_map_size / 4;
  for (i = 0; i < len; i++)
    {
      reverse_map[i] = htons (val);
      val -= 1;
    }

  for (i = len; i < len * 2; i++)
    {
      reverse_map[i] = htons (val);
      val -= 1;
    }

  for (i = len * 2; i < len * 3; i++)
    {
      reverse_map[i] = htons (val);
      val -= 1;
    }

  for (i = len * 3; i < len * 4; i++)
    {
      reverse_map[i] = htons (0xffff);
    }

  DBG (DBG_proc, "Done preparing reverse calibration map\n");

  ret = hp5590_bulk_write (dn,
  			   proto_flags,
  			   0x2b,
			   (unsigned char *) reverse_map,
			   reverse_map_size * sizeof (uint16_t));
  if (ret != SANE_STATUS_GOOD)
    return ret;

  return SANE_STATUS_GOOD;
}

/******************************************************************************/
static SANE_Status
hp5590_send_forward_calibration_maps (SANE_Int dn,
				      enum proto_flags proto_flags)
{
  unsigned int forward_map_size = FORWARD_MAP_LEN;
  uint16_t forward_map[FORWARD_MAP_LEN];
  SANE_Status ret;
  unsigned int i;
  uint16_t val;

  DBG (DBG_proc, "%s\n", __FUNCTION__);
  DBG (DBG_proc, "Preparing forward calibration map\n");
  val = 0x0000;
  for (i = 0; i < forward_map_size; i++)
    {
      forward_map[i] = htons (val);
      if (val < 0xffff)
	val += 1;
    }
  DBG (DBG_proc, "Done preparing forward calibration map\n");

  ret = hp5590_bulk_write (dn,
  			   proto_flags,
  			   0x012a,
			   (unsigned char *) forward_map,
			   forward_map_size * sizeof (uint16_t));
  if (ret != SANE_STATUS_GOOD)
    return ret;

  ret = hp5590_bulk_write (dn,
  			   proto_flags,
  			   0x022a,
			   (unsigned char *) forward_map,
			   forward_map_size * sizeof (uint16_t));
  if (ret != SANE_STATUS_GOOD)
    return ret;

  ret = hp5590_bulk_write (dn,
  			   proto_flags,
  			   0x032a,
			   (unsigned char *) forward_map,
			   forward_map_size * sizeof (uint16_t));
  if (ret != SANE_STATUS_GOOD)
    return ret;

  return SANE_STATUS_GOOD;
}

/******************************************************************************/
static SANE_Status
hp5590_read (SANE_Int dn,
	     enum proto_flags proto_flags,
	     unsigned char *bytes, unsigned int size,
	     void *state)
{
  SANE_Status ret;

  DBG (DBG_proc, "%s\n", __FUNCTION__);

  hp5590_cmds_assert (bytes != NULL);
  hp5590_cmds_assert (state != NULL);

  ret = hp5590_bulk_read (dn, proto_flags, bytes, size, state);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  return SANE_STATUS_GOOD;
}

/******************************************************************************/
static SANE_Status
hp5590_start_scan (SANE_Int dn,
		   enum proto_flags proto_flags)
{
  uint8_t reg_051b = 0x40;
  SANE_Status ret;

  DBG (DBG_proc, "%s\n", __FUNCTION__);

  hp5590_cmds_assert (sizeof (reg_051b) == 1);

  ret = hp5590_cmd (dn,
  		    proto_flags,
  		    CMD_VERIFY,
		    CMD_START_SCAN,
		    (unsigned char *) &reg_051b,
		    sizeof (reg_051b), CORE_NONE);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  return SANE_STATUS_GOOD;
}

/******************************************************************************/
static SANE_Status
hp5590_read_buttons (SANE_Int dn,
		     enum proto_flags proto_flags,
		     enum button_status * status)
{
  uint16_t button_status;
  SANE_Status ret;

  DBG (DBG_proc, "%s\n", __FUNCTION__);

  hp5590_cmds_assert (status != NULL);
  hp5590_cmds_assert (sizeof (button_status) == 2);

  ret = hp5590_cmd (dn,
  		    proto_flags,
  		    CMD_IN | CMD_VERIFY,
		    CMD_BUTTON_STATUS,
		    (unsigned char *) &button_status,
		    sizeof (button_status), CORE_NONE);
  if (ret != SANE_STATUS_GOOD)
    return ret;

  *status = BUTTON_NONE;

  /* Network order */
  button_status = ntohs (button_status);
  DBG (DBG_cmds, "Button status: %04x\n", button_status);
  DBG (DBG_cmds, "Power: %s, Scan: %s, Collect: %s, File: %s, Email: %s, Copy: %s, "
       "Up: %s, Down: %s, Mode: %s, Cancel: %s\n",
       button_status & BUTTON_FLAG_POWER ? " on" : "off",
       button_status & BUTTON_FLAG_SCAN ? " on" : "off",
       button_status & BUTTON_FLAG_COLLECT ? " on" : "off",
       button_status & BUTTON_FLAG_FILE ? " on" : "off",
       button_status & BUTTON_FLAG_EMAIL ? " on" : "off",
       button_status & BUTTON_FLAG_COPY ? " on" : "off",
       button_status & BUTTON_FLAG_UP ? " on" : "off",
       button_status & BUTTON_FLAG_DOWN ? " on" : "off",
       button_status & BUTTON_FLAG_MODE ? " on" : "off",
       button_status & BUTTON_FLAG_CANCEL ? " on" : "off");

  if (button_status & BUTTON_FLAG_POWER)
    *status = BUTTON_POWER;

  if (button_status & BUTTON_FLAG_SCAN)
    *status = BUTTON_SCAN;

  if (button_status & BUTTON_FLAG_COLLECT)
    *status = BUTTON_COLLECT;

  if (button_status & BUTTON_FLAG_FILE)
    *status = BUTTON_FILE;

  if (button_status & BUTTON_FLAG_EMAIL)
    *status = BUTTON_EMAIL;

  if (button_status & BUTTON_FLAG_COPY)
    *status = BUTTON_COPY;

  if (button_status & BUTTON_FLAG_UP)
    *status = BUTTON_UP;

  if (button_status & BUTTON_FLAG_DOWN)
    *status = BUTTON_DOWN;

  if (button_status & BUTTON_FLAG_MODE)
    *status = BUTTON_MODE;

  if (button_status & BUTTON_FLAG_CANCEL)
    *status = BUTTON_CANCEL;

  return SANE_STATUS_GOOD;
}

/* SET SCAN PARAMETERS
==================== 50 =======================
BW 50 (425 x 585)
18 00 64 00 64 00 00 00 00 04 f8 06 db 00 80 00
40 08 e8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 03 50

BW 50 reduced top left (261 x 469)
18 00 64 00 64 00 00 00 00 03 0c 05 7f 00 80 00
40 08 e8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 02 08

BW 50 reduced top right (302 x 498)
18 00 64 00 64 01 6e 00 00 03 8a 05 d6 00 80 00
40 08 e8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 02 5c

BW 50 X 0, small width (16 x 585)
18 00 64 00 64 00 00 00 00 00 30 06 db 00 80 00
40 08 e8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 00 20

BW 50 X large, small width (24 x 585)
18 00 64 00 64 04 b0 00 00 00 48 06 db 00 80 00
40 08 e8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 00 30

BW 50 Y 0, small height (425 x 21)
18 00 64 00 64 00 00 00 00 04 f8 00 3f 00 80 00
40 08 e8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 03 50

BW 50 Y large, small height (425 x 21)
18 00 64 00 64 00 00 06 99 04 f8 00 3f 00 80 00
40 08 e8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 03 50

GRAY 50
18 00 64 00 64 00 00 00 00 04 f8 06 db 00 80 00
40 08 e8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 03 50

COLOR 50
18 00 64 00 64 00 00 00 00 04 f8 06 db 00 80 00
00 18 e8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 09 f0

==================== 75 =======================
BW 75
18 00 64 00 64 00 00 00 00 04 f8 06 da 00 80 00
40 08 e8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 03 50

GRAY 75
18 00 64 00 64 00 00 00 00 04 f8 06 da 00 80 00
40 08 e8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 03 50

COLOR 75
18 00 64 00 64 00 00 00 00 04 f8 06 da 00 80 00
00 18 e8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 09 f0

COLOR 75 48 bit
18 00 64 00 64 00 00 00 00 04 f8 06 db 00 80 00
00 30 e8 40 00 64 00 64 00 64 00 00 00 00 00 00
04 00 00 13 e0

=================== 100 =======================
BW 100
18 00 64 00 64 00 00 00 00 04 f8 06 db 00 80 00
40 01 e8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 00 6a

GRAY 100
18 00 64 00 64 00 00 00 00 04 f8 06 db 00 80 00
40 08 e8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 03 50
	
COLOR 100
18 00 64 00 64 00 00 00 00 04 f8 06 db 00 80 00
00 18 e8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 09 f0

COLOR 100 48bit, preview 
18 00 64 00 64 00 00 00 00 04 f8 06 db 00 80 00
00 30 e8 40 00 64 00 64 00 64 00 00 00 00 00 00
04 00 00 13 e0

COLOR 100 48bit
18 00 64 00 64 00 00 00 00 04 f2 06 db 00 80 00
00 30 e8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 13 c8

COLOR 100 48bit, TMA negatives
11 00 64 00 64 00 00 00 00 00 fc 03 84 00 80 00
00 30 e8 40 00 64 00 c8 01 90 00 00 00 00 00 00
00 00 00 03 f0

COLOR 100 48bit, TMA slides
12 00 64 00 64 00 00 00 00 00 fc 03 84 00 80 00
00 30 e8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 03 f0

=================== 150 =======================
BW 150
18 00 c8 00 c8 00 00 00 00 09 f6 0d b6 00 80 00
40 08 e8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 06 a4

GRAY 150
18 00 c8 00 c8 00 00 00 00 09 f6 0d b6 00 80 00
40 08 e8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 06 a4

COLOR 150
18 00 c8 00 c8 00 00 00 00 09 f6 0d b6 00 80 00
00 18 e8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 13 ec

COLOR 150 48 bit
18 00 c8 00 c8 00 00 00 00 09 ea 0d b6 00 80 00
00 30 e8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 27 a8

=================== 200 =======================
BW 200
18 00 c8 00 c8 00 00 00 00 09 f0 0d b6 00 80 00
40 01 e8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 00 d4

GRAY 200
18 00 c8 00 c8 00 00 00 00 09 f6 0d b6 00 80 00
40 08 e8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 06 a4

COLOR 200
18 00 c8 00 c8 00 00 00 00 09 f6 0d b6 00 80 00
00 18 e8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 13 ec

COLOR 200 48 bit
18 00 c8 00 c8 00 00 00 00 09 f6 0d aa 00 80 00
00 30 e8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 27 d8

=================== 300 =======================
BW 300
18 01 2c 01 2c 00 00 00 00 09 f0 0d b6 00 80 00
40 01 e8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 01 3e

GRAY 300
18 01 2c 01 2c 00 00 00 00 09 f4 0d b6 00 80 00
40 08 e8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 09 f4

COLOR 300
18 01 2c 01 2c 00 00 00 00 09 f4 0d b6 00 80 00
00 18 e8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 1d dc

COLOR 300 48bit, TMA negatives
11 01 2c 01 2c 00 00 00 06 01 fc 07 02 00 80 00
00 30 e8 40 00 64 00 c8 01 90 00 00 00 00 00 00
00 00 00 0b e8

COLOR 300 48bit, TMA slides
12 01 2c 01 2c 00 00 00 00 01 fc 07 08 00 80 00
00 30 e8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 0b e8

==================== 400 ======================
BW 400
18 02 58 02 58 00 00 00 00 13 ec 1b 6c 00 80 00
40 08 c8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 13 ec

GRAY 400
18 02 58 02 58 00 00 00 00 13 ec 1b 6c 00 80 00
40 08 c8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 13 ec
	
COLOR 400
18 02 58 02 58 00 00 00 00 13 ec 1b 6c 00 80 00
00 18 c8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 3b c4

==================== 600 ======================
BW 600
18 02 58 02 58 00 00 00 00 13 e8 1b 6c 00 80 00
40 01 c8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 02 7d

GRAY 600
18 02 58 02 58 00 00 00 00 13 ec 1b 6c 00 80 00
40 08 c8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 13 ec

COLOR 600
18 02 58 02 58 00 00 00 00 13 ec 1b 6c 00 80 00
00 18 c8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 3b c4

==================== 1200 =====================
BW 1200

18 04 b0 04 b0 00 00 00 00 27 a8 36 d8 00 80 00
40 01 c8 40 00 64 00 64 00 64 00 00 00 00 00 00
00 00 00 04 f5
*/

/* READ SCAN PARAMETERS
====================== 50 =====================
BW 50
c0 00 00 0f 23 a0 00 00 03 50 04 92 00 00 00 00

GRAY 50
c0 00 00 0f 23 a0 00 00 03 50 04 92 00 00 00 00

COLOR 50
c0 00 00 2d 6a e0 00 00 09 f0 04 92 00 00 00 00

====================== 75 =====================
BW 75
c0 00 00 0f 20 50 00 00 03 50 04 91 00 00 00 00

GRAY 75
c0 00 00 0f 20 50 00 00 03 50 04 91 00 00 00 00

COLOR 75
c0 00 00 2d 60 f0 00 00 09 f0 04 91 00 00 00 00

COLOR 75 48 bit
c0 00 00 5a 86 40 00 00 13 e0 04 8e 00 00 00 00

===================== 100 =====================
BW 100
c0 00 00 01 e4 74 00 00 00 6a 04 92 00 00 00 00

GRAY 100
c0 00 00 0f 23 a0 00 00 03 50 04 92 00 00 00 00

COLOR 100
c0 00 00 2d 6a e0 00 00 09 f0 04 92 00 00 00 00

COLOR 100, 48 bit preview
c0 00 00 5a d5 c0 00 00 13 e0 04 92 00 00 00 00

COLOR 100, 48 bit
c0 00 00 5a 68 10 00 00 13 c8 04 92 00 00 00 00

===================== 150 =====================
BW 150
c0 00 00 3c b3 10 00 00 06 a4 09 24 00 00 00 00

GRAY 150
c0 00 00 3c b3 10 00 00 06 a4 09 24 00 00 00 00

COLOR 150
c0 00 00 b6 19 30 00 00 13 ec 09 24 00 00 00 00

COLOR 150 48bit
c0 00 01 6a 7b a0 00 00 27 a8 09 24 00 00 00 00

===================== 200 =====================
BW 200
c0 00 00 07 91 d0 00 00 00 d4 09 24 00 00 00 00

GRAY 200
c0 00 00 3c b3 10 00 00 06 a4 09 24 00 00 00 00

COLOR 200
c0 00 00 b6 19 30 00 00 13 ec 09 24 00 00 00 00

COLOR 200 48 bit
c0 00 01 6a f3 a0 00 00 27 d8 09 1c 00 00 00 00

===================== 300 =====================
BW 300
c0 00 00 11 08 14 00 00 01 3e 0d b6 00 00 00 00

GRAY 300
c0 00 00 88 77 78 00 00 09 f4 0d b6 00 00 00 00

COLOR 300
c0 00 01 99 66 68 00 00 1d dc 0d b6 00 00 00 00

===================== 400 =====================
BW 400
c0 00 02 22 4b 90 00 00 13 ec 1b 6c 00 00 00 00

GRAY 400
c0 00 02 22 4b 90 00 00 13 ec 1b 6c 00 00 00 00

COLOR 400
c0 00 06 66 e2 b0 00 00 3b c4 1b 6c 00 00 00 00

===================== 600 =====================
BW 600
c0 00 00 44 3b bc 00 00 02 7d 1b 6c 00 00 00 00

GRAY 600
c0 00 02 22 4b 90 00 00 13 ec 1b 6c 00 00 00 00

COLOR 600
c0 00 06 66 e2 b0 00 00 3b c4 1b 6c 00 00 00 00
*/
/* vim: sw=2 ts=8
 */
