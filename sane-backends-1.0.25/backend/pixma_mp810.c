/* SANE - Scanner Access Now Easy.

 Copyright (C) 2011-2015 Rolf Bensch <rolf at bensch hyphen online dot de>
 Copyright (C) 2007-2009 Nicolas Martin, <nicols-guest at alioth dot debian dot org>
 Copyright (C) 2006-2007 Wittawat Yamwong <wittawat@web.de>

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
 */
/* test cases
 1. short USB packet (must be no -ETIMEDOUT)
 2. cancel using button on the printer (look for abort command)
 3. start scan while busy (status 0x1414)
 4. cancel using ctrl-c (must send abort command)
 */

#define TPU_48             /* uncomment to activate TPU scan at 48 bits */
/*#define DEBUG_TPU_48*//* uncomment to debug 48 bits TPU on a non TPU device */
/*#define DEBUG_TPU_24*//* uncomment to debug 24 bits TPU on a non TPU device */

/*#define TPUIR_USE_RGB*/      /* uncomment to use RGB channels and convert them to gray; otherwise use R channel only */

#include "../include/sane/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>		/* localtime(C90) */

#include "pixma_rename.h"
#include "pixma_common.h"
#include "pixma_io.h"

/* Some macro code to enhance readability */
#define RET_IF_ERR(x) do {	\
    if ((error = (x)) < 0)	\
      return error;		\
  } while(0)

#define WAIT_INTERRUPT(x) do {			\
    error = handle_interrupt (s, x);		\
    if (s->cancel)				\
      return PIXMA_ECANCELED;			\
    if (error != PIXMA_ECANCELED && error < 0)	\
      return error;				\
  } while(0)

#ifdef __GNUC__
# define UNUSED(v) (void) v
#else
# define UNUSED(v)
#endif

/* Size of the command buffer should be multiple of wMaxPacketLength and
 greater than 4096+24.
 4096 = size of gamma table. 24 = header + checksum */
#define IMAGE_BLOCK_SIZE (512*1024)
#define CMDBUF_SIZE (4096 + 24)
#define DEFAULT_GAMMA 2.0	/***** Gamma different from 1.0 is potentially impacting color profile generation *****/
#define UNKNOWN_PID 0xffff

#define CANON_VID 0x04a9

/* Generation 2 */
#define MP810_PID 0x171a
#define MP960_PID 0x171b

/* Generation 3 */
/* PIXMA 2007 vintage */
#define MP970_PID 0x1726

/* Flatbed scanner CCD (2007) */
#define CS8800F_PID 0x1901

/* PIXMA 2008 vintage */
#define MP980_PID 0x172d

/* Generation 4 */
#define MP990_PID 0x1740

/* Flatbed scanner CCD (2010) */
#define CS9000F_PID 0x1908

/* 2010 new device (untested) */
#define MG8100_PID 0x174b /* CCD */

/* 2011 new device (untested) */
#define MG8200_PID 0x1756 /* CCD */

/* 2013 new device */
#define CS9000F_MII_PID 0x190d

/* Generation 4 XML messages that encapsulates the Pixma protocol messages */
#define XML_START_1   \
"<?xml version=\"1.0\" encoding=\"utf-8\" ?>\
<cmd xmlns:ivec=\"http://www.canon.com/ns/cmd/2008/07/common/\">\
<ivec:contents><ivec:operation>StartJob</ivec:operation>\
<ivec:param_set servicetype=\"scan\"><ivec:jobID>00000001</ivec:jobID>\
<ivec:bidi>1</ivec:bidi></ivec:param_set></ivec:contents></cmd>"

#define XML_START_2   \
"<?xml version=\"1.0\" encoding=\"utf-8\" ?>\
<cmd xmlns:ivec=\"http://www.canon.com/ns/cmd/2008/07/common/\" xmlns:vcn=\"http://www.canon.com/ns/cmd/2008/07/canon/\">\
<ivec:contents><ivec:operation>VendorCmd</ivec:operation>\
<ivec:param_set servicetype=\"scan\"><ivec:jobID>00000001</ivec:jobID>\
<vcn:ijoperation>ModeShift</vcn:ijoperation><vcn:ijmode>1</vcn:ijmode>\
</ivec:param_set></ivec:contents></cmd>"

#define XML_END   \
"<?xml version=\"1.0\" encoding=\"utf-8\" ?>\
<cmd xmlns:ivec=\"http://www.canon.com/ns/cmd/2008/07/common/\">\
<ivec:contents><ivec:operation>EndJob</ivec:operation>\
<ivec:param_set servicetype=\"scan\"><ivec:jobID>00000001</ivec:jobID>\
</ivec:param_set></ivec:contents></cmd>"

#define XML_OK   "<ivec:response>OK</ivec:response>"

enum mp810_state_t
{
  state_idle,
  state_warmup,
  state_scanning,
  state_transfering,
  state_finished
};

enum mp810_cmd_t
{
  cmd_start_session = 0xdb20,
  cmd_select_source = 0xdd20,
  cmd_gamma = 0xee20,
  cmd_scan_param = 0xde20,
  cmd_status = 0xf320,
  cmd_abort_session = 0xef20,
  cmd_time = 0xeb80,
  cmd_read_image = 0xd420,
  cmd_error_info = 0xff20,

  cmd_start_calibrate_ccd_3 = 0xd520,
  cmd_end_calibrate_ccd_3 = 0xd720,
  cmd_scan_param_3 = 0xd820,
  cmd_scan_start_3 = 0xd920,
  cmd_status_3 = 0xda20,
  cmd_get_tpu_info_3 = 0xf520,
  cmd_set_tpu_info_3 = 0xea20,

  cmd_e920 = 0xe920 /* seen in MP800 */
};

typedef struct mp810_t
{
  enum mp810_state_t state;
  pixma_cmdbuf_t cb;
  uint8_t *imgbuf;
  uint8_t current_status[16];
  unsigned last_block;
  uint8_t generation;
  /* for Generation 3 and CCD shift */
  uint8_t *linebuf;
  uint8_t *data_left_ofs;
  unsigned data_left_len;
  int shift[3];
  unsigned color_shift;
  unsigned stripe_shift;
  unsigned stripe_shift2; /* added for MP810, MP960 at 4800dpi & 9000F at 9600dpi */
  unsigned jumplines; /* added for MP810, MP960 at 4800dpi & 9000F at 9600dpi */
  uint8_t tpu_datalen;
  uint8_t tpu_data[0x40];
} mp810_t;

/*
 STAT:  0x0606 = ok,
 0x1515 = failed (PIXMA_ECANCELED),
 0x1414 = busy (PIXMA_EBUSY)

 Transaction scheme
 1. command_header/data | result_header
 2. command_header      | result_header/data
 3. command_header      | result_header/image_data

 - data has checksum in the last byte.
 - image_data has no checksum.
 - data and image_data begins in the same USB packet as
 command_header or result_header.

 command format #1:
 u16be      cmd
 u8[6]      0
 u8[4]      0
 u32be      PLEN parameter length
 u8[PLEN-1] parameter
 u8         parameter check sum
 result:
 u16be      STAT
 u8         0
 u8         0 or 0x21 if STAT == 0x1414
 u8[4]      0

 command format #2:
 u16be      cmd
 u8[6]      0
 u8[4]      0
 u32be      RLEN result length
 result:
 u16be      STAT
 u8[6]      0
 u8[RLEN-1] result
 u8         result check sum

 command format #3: (only used by read_image_block)
 u16be      0xd420
 u8[6]      0
 u8[4]      0
 u32be      max. block size + 8
 result:
 u16be      STAT
 u8[6]      0
 u8         block info bitfield: 0x8 = end of scan, 0x10 = no more paper, 0x20 = no more data
 u8[3]      0
 u32be      ILEN image data size
 u8[ILEN]   image data
 */

static void mp810_finish_scan (pixma_t * s);

static int is_scanning_from_adf (pixma_t * s)
{
  return (s->param->source == PIXMA_SOURCE_ADF
           || s->param->source == PIXMA_SOURCE_ADFDUP);
}

static int is_scanning_from_adfdup (pixma_t * s)
{
  return (s->param->source == PIXMA_SOURCE_ADFDUP);
}

static int is_scanning_from_tpu (pixma_t * s)
{
  return (s->param->source == PIXMA_SOURCE_TPU);
}

static int send_xml_dialog (pixma_t * s, const char * xml_message)
{
  mp810_t *mp = (mp810_t *) s->subdriver;
  int datalen;

  datalen = pixma_cmd_transaction (s, xml_message, strlen (xml_message),
                                   mp->cb.buf, 1024);
  if (datalen < 0)
    return datalen;

  mp->cb.buf[datalen] = 0;

  PDBG(pixma_dbg (10, "XML message sent to scanner:\n%s\n", xml_message));
  PDBG(pixma_dbg (10, "XML response back from scanner:\n%s\n", mp->cb.buf));

  return (strcasestr ((const char *) mp->cb.buf, XML_OK) != NULL);
}

static void new_cmd_tpu_msg (pixma_t *s, pixma_cmdbuf_t * cb, uint16_t cmd)
{
  pixma_newcmd (cb, cmd, 0, 0);
  cb->buf[3] = (is_scanning_from_tpu (s)) ? 0x01 : 0x00;
}

static int start_session (pixma_t * s)
{
  mp810_t *mp = (mp810_t *) s->subdriver;

  new_cmd_tpu_msg (s, &mp->cb, cmd_start_session);
  return pixma_exec (s, &mp->cb);
}

static int start_scan_3 (pixma_t * s)
{
  mp810_t *mp = (mp810_t *) s->subdriver;

  new_cmd_tpu_msg (s, &mp->cb, cmd_scan_start_3);
  return pixma_exec (s, &mp->cb);
}

static int send_cmd_start_calibrate_ccd_3 (pixma_t * s)
{
  mp810_t *mp = (mp810_t *) s->subdriver;

  pixma_newcmd (&mp->cb, cmd_start_calibrate_ccd_3, 0, 0);
  mp->cb.buf[3] = 0x01;
  return pixma_exec (s, &mp->cb);
}

static int is_calibrated (pixma_t * s)
{
  mp810_t *mp = (mp810_t *) s->subdriver;
  if (mp->generation >= 3)
  {
    return ((mp->current_status[0] & 0x01) == 1);
  }
  if (mp->generation == 1)
  {
    return (mp->current_status[8] == 1);
  }
  else
  {
    return (mp->current_status[9] == 1);
  }
}

static int has_paper (pixma_t * s)
{
  mp810_t *mp = (mp810_t *) s->subdriver;

  if (is_scanning_from_adfdup (s))
    return (mp->current_status[1] == 0 || mp->current_status[2] == 0);
  else
    return (mp->current_status[1] == 0);
}

static void drain_bulk_in (pixma_t * s)
{
  mp810_t *mp = (mp810_t *) s->subdriver;
  while (pixma_read (s->io, mp->imgbuf, IMAGE_BLOCK_SIZE) >= 0)
    ;
}

static int abort_session (pixma_t * s)
{
  mp810_t *mp = (mp810_t *) s->subdriver;
  return pixma_exec_short_cmd (s, &mp->cb, cmd_abort_session);
}

static int send_cmd_e920 (pixma_t * s)
{
  mp810_t *mp = (mp810_t *) s->subdriver;
  return pixma_exec_short_cmd (s, &mp->cb, cmd_e920);
}

static int select_source (pixma_t * s)
{
  mp810_t *mp = (mp810_t *) s->subdriver;
  uint8_t *data;

  data = pixma_newcmd (&mp->cb, cmd_select_source, 12, 0);
  data[5] = ((mp->generation == 2) ? 1 : 0);
  switch (s->param->source)
  {
    case PIXMA_SOURCE_FLATBED:
      data[0] = 1;
      data[1] = 1;
      break;

    case PIXMA_SOURCE_ADF:
      data[0] = 2;
      data[5] = 1;
      data[6] = 1;
      break;

    case PIXMA_SOURCE_ADFDUP:
      data[0] = 2;
      data[5] = 3;
      data[6] = 3;
      break;

    case PIXMA_SOURCE_TPU:
      data[0] = 4;
      data[1] = 2;
      break;
  }
  return pixma_exec (s, &mp->cb);
}

static int send_get_tpu_info_3 (pixma_t * s)
{
  mp810_t *mp = (mp810_t *) s->subdriver;
  uint8_t *data;
  int error;

  data = pixma_newcmd (&mp->cb, cmd_get_tpu_info_3, 0, 0x34);
  RET_IF_ERR(pixma_exec (s, &mp->cb));
  memcpy (mp->tpu_data, data, 0x34);
  return error;
}

static int send_set_tpu_info (pixma_t * s)
{
  mp810_t *mp = (mp810_t *) s->subdriver;
  uint8_t *data;

  if (mp->tpu_datalen == 0)
    return 0;
  data = pixma_newcmd (&mp->cb, cmd_set_tpu_info_3, 0x34, 0);
  memcpy (data, mp->tpu_data, 0x34);
  return pixma_exec (s, &mp->cb);
}

static int send_gamma_table (pixma_t * s)
{
  mp810_t *mp = (mp810_t *) s->subdriver;
  const uint8_t *lut = s->param->gamma_table;
  uint8_t *data;

  if (mp->generation == 1)
  {
    data = pixma_newcmd (&mp->cb, cmd_gamma, 4096 + 8, 0);
    data[0] = (s->param->channels == 3) ? 0x10 : 0x01;
    pixma_set_be16 (0x1004, data + 2);
    if (lut)
      memcpy (data + 4, lut, 4096);
    else
      pixma_fill_gamma_table (DEFAULT_GAMMA, data + 4, 4096);
  }
  else
  {
    /* FIXME: Gamma table for 2nd generation: 1024 * uint16_le */
    data = pixma_newcmd (&mp->cb, cmd_gamma, 2048 + 8, 0);
    data[0] = 0x10;
    pixma_set_be16 (0x0804, data + 2);
    if (lut)
    {
      int i;
      for (i = 0; i < 1024; i++)
      {
        int j = (i << 2) + (i >> 8);
        data[4 + 2 * i + 0] = lut[j];
        data[4 + 2 * i + 1] = lut[j];
      }
    }
    else
    {
      int i;
      pixma_fill_gamma_table (DEFAULT_GAMMA, data + 4, 2048);
      for (i = 0; i < 1024; i++)
      {
        int j = (i << 1) + (i >> 9);
        data[4 + 2 * i + 0] = data[4 + j];
        data[4 + 2 * i + 1] = data[4 + j];
      }
    }
  }
  return pixma_exec (s, &mp->cb);
}

static unsigned calc_raw_width (const mp810_t * mp,
                                const pixma_scan_param_t * param)
{
  unsigned raw_width;
  /* NOTE: Actually, we can send arbitary width to MP810. Lines returned
     are always padded to multiple of 4 or 12 pixels. Is this valid for
     other models, too? */
  if (mp->generation >= 2)
  {
    raw_width = ALIGN_SUP (param->w + param->xs, 32);
    /* PDBG (pixma_dbg (4, "*calc_raw_width***** width %u extended by %u and rounded to %u *****\n", param->w, param->xs, raw_width)); */
  }
  else if (param->channels == 1)
  {
    raw_width = ALIGN_SUP (param->w + param->xs, 12);
  }
  else
  {
    raw_width = ALIGN_SUP (param->w + param->xs, 4);
  }
  return raw_width;
}

static int has_ccd_sensor (pixma_t * s)
{
  return ((s->cfg->cap & PIXMA_CAP_CCD) != 0);
}

#if 0
static int is_color (pixma_t * s)
{
  return (s->param->mode == PIXMA_SCAN_MODE_COLOR);
}

static int is_color_48 (pixma_t * s)
{
  return (s->param->mode == PIXMA_SCAN_MODE_COLOR_48);
}

static int is_color_negative (pixma_t * s)
{
  return (s->param->mode == PIXMA_SCAN_MODE_NEGATIVE_COLOR);
}

static int is_color_all (pixma_t * s)
{
  return (is_color (s) || is_color_48 (s) || is_color_negative (s));
}
#endif

static int is_gray (pixma_t * s)
{
  return (s->param->mode == PIXMA_SCAN_MODE_GRAY);
}

static int is_gray_16 (pixma_t * s)
{
  return (s->param->mode == PIXMA_SCAN_MODE_GRAY_16);
}

static int is_gray_negative (pixma_t * s)
{
  return (s->param->mode == PIXMA_SCAN_MODE_NEGATIVE_GRAY);
}

static int is_gray_all (pixma_t * s)
{
  return (is_gray (s) || is_gray_16 (s) || is_gray_negative (s));
}

static int is_lineart (pixma_t * s)
{
  return (s->param->mode == PIXMA_SCAN_MODE_LINEART);
}

static int is_tpuir (pixma_t * s)
{
  return (s->param->mode == PIXMA_SCAN_MODE_TPUIR);
}

/* CCD sensors don't have neither a Grayscale mode nor a Lineart mode,
 * but use color mode instead */
static unsigned get_cis_ccd_line_size (pixma_t * s)
{
  return ((
      s->param->wx ? s->param->line_size / s->param->w * s->param->wx
                   : s->param->line_size)
      * ((is_tpuir (s) || is_gray_all (s) || is_lineart (s)) ? 3 : 1));
}

static unsigned calc_shifting (pixma_t * s)
{
  mp810_t *mp = (mp810_t *) s->subdriver;

  /* If stripes shift needed (CCD devices), how many pixels shift */
  mp->stripe_shift = 0;
  mp->stripe_shift2 = 0;
  mp->jumplines = 0;

  switch (s->cfg->pid)
  {
    case MP970_PID: /* MP970 at 4800 dpi */
    case CS8800F_PID: /* CanoScan 8800F at 4800 dpi */
      if (s->param->xdpi == 4800)
      {
        if (is_scanning_from_tpu (s))
          mp->stripe_shift = 6;
        else
          mp->stripe_shift = 3;
      }
      break;

    case CS9000F_PID: /* CanoScan 9000F at 4800 dpi */
    case CS9000F_MII_PID:
      if (s->param->xdpi == 4800)
      {
        if (is_scanning_from_tpu (s))
          mp->stripe_shift = 6; /* should work for CS9000F same as manual */
        else
          mp->stripe_shift = 3;
      }
      if (s->param->xdpi == 9600)
      {
        if (is_scanning_from_tpu (s))
        {
          /* need to double up for TPU */
          mp->stripe_shift = 6; /* for 1st set of 4 images */
          /* unfortunately there are 2 stripe shifts */
          mp->stripe_shift2 = 6; /* for 2nd set of 4 images */
          mp->jumplines = 32; /* try 33 or 34 */
        }
        /* there is no 9600dpi support in flatbed mode */
      }
      break;

    case MP960_PID:
      if (s->param->xdpi == 2400)
      {
        if (is_scanning_from_tpu (s))
          mp->stripe_shift = 6;
        else
          mp->stripe_shift = 3;
      }
      if (s->param->xdpi == 4800)
      {
        if (is_scanning_from_tpu (s))
        {
          mp->stripe_shift = 6;
          mp->stripe_shift2 = 6;
        }
        else
        {
          mp->stripe_shift = 3;
          mp->stripe_shift2 = 3;
        }
        mp->jumplines = 33; /* better than 32 or 34 : applies to flatbed & TPU */
      }
      break;

    case MP810_PID:
      if (s->param->xdpi == 2400)
      {
        if (is_scanning_from_tpu (s))
          mp->stripe_shift = 6;
        else
          mp->stripe_shift = 3;
      }
      if (s->param->xdpi == 4800)
      {
        if (is_scanning_from_tpu (s))
        {
          mp->stripe_shift = 6;
          mp->stripe_shift2 = 6;
        }
        else
        {
          mp->stripe_shift = 3;
          mp->stripe_shift2 = 3;
        }
        mp->jumplines = 33; /* better than 32 or 34 : applies to flatbed & TPU */
      }
      break;

    case MP990_PID:
      if (s->param->xdpi == 4800)
      {
        if (is_scanning_from_tpu (s))
        {
          mp->stripe_shift = 6;
          mp->stripe_shift2 = 6;
        }
        else
        {
          mp->stripe_shift = 3;
          mp->stripe_shift2 = 3;
        }
        mp->jumplines = 34; /* better than 32 or 34 : applies to flatbed & TPU */
      }
      break;

    default: /* Default, and all CIS devices */
      break;
  }
  /* If color plane shift (CCD devices), how many pixels shift */
  mp->color_shift = mp->shift[0] = mp->shift[1] = mp->shift[2] = 0;
  if (s->param->ydpi > 75)
  {
    switch (s->cfg->pid)
    {
      case MP970_PID:
      case CS8800F_PID: /* CanoScan 8800F */
        mp->color_shift = s->param->ydpi / 50;
        mp->shift[1] = mp->color_shift * get_cis_ccd_line_size (s);
        mp->shift[0] = 0;
        mp->shift[2] = 2 * mp->shift[1];
        break;

      case CS9000F_PID: /* CanoScan 9000F */
      case CS9000F_MII_PID:
        mp->color_shift = s->param->ydpi / 30;
        mp->shift[1] = mp->color_shift * get_cis_ccd_line_size (s);
        mp->shift[0] = 0;
        mp->shift[2] = 2 * mp->shift[1];
        break;

      case MP980_PID:
      case MP990_PID:
      case MG8200_PID:
        if (s->param->ydpi > 150)
        {
          mp->color_shift = s->param->ydpi / 75;
          mp->shift[1] = mp->color_shift * get_cis_ccd_line_size (s);
          mp->shift[0] = 0;
          mp->shift[2] = 2 * mp->shift[1];
        }
        break;

      case MP810_PID:
      case MP960_PID:
        mp->color_shift = s->param->ydpi / 50;
        if (is_scanning_from_tpu (s))
          mp->color_shift = s->param->ydpi / 50;
        mp->shift[1] = mp->color_shift * get_cis_ccd_line_size (s);
        mp->shift[0] = 2 * mp->shift[1];
        mp->shift[2] = 0;
        break;

      default:
        break;
    }
  }
  /* special settings for 16 bit flatbed mode @ 75 dpi
   * minimum of 150 dpi is used yet */
  /* else if (!is_scanning_from_tpu (s))
    {
      switch (s->cfg->pid)
      {
        case CS9000F_PID:
        case CS9000F_MII_PID:
          if (is_color_48 (s) || is_gray_16 (s))
          {
            mp->color_shift = 5;
            mp->shift[1] = 0;
            mp->shift[0] = 0;
            mp->shift[2] = 0;
          }
          break;
       }
    } */
  /* PDBG (pixma_dbg (4, "*calc_shifing***** color_shift = %u, stripe_shift = %u, jumplines = %u  *****\n",
                   mp->color_shift, mp->stripe_shift, mp->jumplines)); */
  return (2 * mp->color_shift + mp->stripe_shift + mp->jumplines); /* note impact of stripe shift2 later if needed! */
}

static int send_scan_param (pixma_t * s)
{
  mp810_t *mp = (mp810_t *) s->subdriver;
  uint8_t *data;
  unsigned raw_width = calc_raw_width (mp, s->param);
  unsigned h, h1, h2, shifting;

  /* TPU scan does not support lineart */
  if (is_scanning_from_tpu (s) && is_lineart (s))
  {
    return PIXMA_ENOTSUP;
  }

  shifting = calc_shifting (s);
  h1 = s->param->h + shifting; /* add lines for color shifting */
  /* PDBG (pixma_dbg (4, "* send_scan_param: height calc (choose lesser) 1 %u \n", h1 )); */
  if (mp->generation >= 4) /* use most global condition */
  {
    /* tested for MP960, 9000F */
    /* add lines for color shifting */
    /* otherwise you cannot scan all lines defined for flatbed mode */
    /* this shouldn't affect TPU mode */
    h2 = s->cfg->height * s->param->ydpi / 75 + shifting;
    /* PDBG (pixma_dbg (4, "* send_scan_param: height calc (choose lesser) 2 %u = %u max. lines for scanner + %u lines for color shifting \n", h2, s->cfg->height * s->param->ydpi / 75, shifting )); */
  }
  else
  {
    /* TODO: Check for other scanners. */
    h2 = s->cfg->height * s->param->ydpi / 75; /* this might be causing problems for generation 1 devices */
    /* PDBG (pixma_dbg (4, "* send_scan_param: height calc (choose lesser) 2 %u \n", h2 )); */
  }
  h = MIN (h1, h2);

  if (mp->generation <= 2)
  {
    data = pixma_newcmd (&mp->cb, cmd_scan_param, 0x30, 0);
    pixma_set_be16 (s->param->xdpi | 0x8000, data + 0x04);
    pixma_set_be16 (s->param->ydpi | 0x8000, data + 0x06);
    pixma_set_be32 (s->param->x, data + 0x08);
    if (mp->generation == 2)
      pixma_set_be32 (s->param->x - s->param->xs, data + 0x08);
    pixma_set_be32 (s->param->y, data + 0x0c);
    pixma_set_be32 (raw_width, data + 0x10);
    pixma_set_be32 (h, data + 0x14);
    data[0x18] =
        ((s->param->channels != 1) || is_gray_all (s) || is_lineart (s)) ?
            0x08 : 0x04;
    data[0x19] = ((s->param->software_lineart) ? 8 : s->param->depth)
                  * ((is_gray_all (s) || is_lineart (s)) ? 3 : s->param->channels); /* bits per pixel */
    data[0x1a] = (is_scanning_from_tpu (s) ? 1 : 0);
    data[0x20] = 0xff;
    data[0x23] = 0x81;
    data[0x26] = 0x02;
    data[0x27] = 0x01;
  }
  else
  {
    /* scan parameters:
     * ================
     *
     * byte | # of  |  mode   | value / description
     *      | bytes |         |
     * -----+-------+---------+---------------------------
     * 0x00 |   1   | default | 0x01
     *      |       |   adf   | 0x02
     *      |       |   tpu   | 0x04
     *      |       |  tpuir  | cs9000f: 0x03
     * -----+-------+---------+---------------------------
     * 0x01 |   1   | default | 0x00
     *      |       |   tpu   | 0x02
     * -----+-------+---------+---------------------------
     * 0x02 |   1   | default | 0x01
     *      |       | adfdup  | 0x03
     * -----+-------+---------+---------------------------
     * 0x03 |   1   | default | 0x00
     *      |       | adfdup  | 0x03
     * -----+-------+---------+---------------------------
     * 0x04 |   1   |   all   | 0x00
     * -----+-------+---------+---------------------------
     * 0x05 |   1   |   all   | 0x01: This one also seen at 0. Don't know yet what's used for.
     * -----+-------+---------+---------------------------
     *  ... |   1   |   all   | 0x00
     * -----+-------+---------+---------------------------
     * 0x08 |   2   |   all   | xdpi | 0x8000
     * -----+-------+---------+---------------------------
     * 0x0a |   2   |   all   | ydpi | 0x8000: Must be the same as xdpi.
     * -----+-------+---------+---------------------------
     * 0x0c |   4   |   all   | x position of start pixel
     * -----+-------+---------+---------------------------
     * 0x10 |   4   |   all   | y position of start pixel
     * -----+-------+---------+---------------------------
     * 0x14 |   4   |   all   | # of pixels in 1 line
     * -----+-------+---------+---------------------------
     * 0x18 |   4   |   all   | # of scan lines
     * -----+-------+---------+---------------------------
     * 0x1c |   1   |   all   | 0x08
     *      |       |         | 0x04 = relict from cis scanners?
     * -----+-------+---------+---------------------------
     * 0x1d |   1   |   all   | # of bits per pixel
     * -----+-------+---------+---------------------------
     * 0x1e |   1   | default | 0x00: paper
     *      |       |   tpu   | 0x01: positives
     *      |       |   tpu   | 0x02: negatives
     *      |       |  tpuir  | 0x01: positives
     * -----+-------+---------+---------------------------
     * 0x1f |   1   |   all   | 0x01
     *      |       |         | cs9000f: 0x00: Not sure if that is because of positives.
     * -----+-------+---------+---------------------------
     * 0x20 |   1   |   all   | 0xff
     * -----+-------+---------+---------------------------
     * 0x21 |   1   |   all   | 0x81
     * -----+-------+---------+---------------------------
     * 0x22 |   1   |   all   | 0x00
     * -----+-------+---------+---------------------------
     * 0x23 |   1   |   all   | 0x02
     * -----+-------+---------+---------------------------
     * 0x24 |   1   |   all   | 0x01
     * -----+-------+---------+---------------------------
     * 0x25 |   1   | default | 0x00; cs8800f: 0x01
     *      |       |   tpu   | 0x00; cs9000f, mg8200, mp990: 0x01
     *      |       |  tpuir  | cs9000f: 0x00
     * -----+-------+---------+---------------------------
     *  ... |   1   |   all   | 0x00
     * -----+-------+---------+---------------------------
     * 0x30 |   1   |   all   | 0x01
     *
     */

    data = pixma_newcmd (&mp->cb, cmd_scan_param_3, 0x38, 0);
    data[0x00] = is_scanning_from_adf (s) ? 0x02 : 0x01;
    data[0x01] = 0x01;
    if (is_scanning_from_tpu (s))
    {
      data[0x00] = is_tpuir (s) ? 0x03 : 0x04;
      data[0x01] = 0x02;
      data[0x1e] = 0x02; /* NB: CanoScan 8800F: 0x02->negatives, 0x01->positives, paper->0x00 */
    }
    data[0x02] = 0x01;
    if (is_scanning_from_adfdup (s))
    {
      data[0x02] = 0x03;
      data[0x03] = 0x03;
    }
    if (s->cfg->pid != MG8200_PID)
      data[0x05] = 0x01; /* This one also seen at 0. Don't know yet what's used for */
    /* the scanner controls the scan */
    /* no software control needed */
    pixma_set_be16 (s->param->xdpi | 0x8000, data + 0x08);
    pixma_set_be16 (s->param->ydpi | 0x8000, data + 0x0a);
    /*PDBG (pixma_dbg (4, "*send_scan_param***** Setting: xdpi=%hi ydpi=%hi  x=%u y=%u  w=%u h=%u ***** \n",
     s->param->xdpi,s->param->ydpi,(s->param->x)-(s->param->xs),s->param->y,raw_width,h));*/
    pixma_set_be32 (s->param->x - s->param->xs, data + 0x0c);
    pixma_set_be32 (s->param->y, data + 0x10);
    pixma_set_be32 (raw_width, data + 0x14);
    pixma_set_be32 (h, data + 0x18);
    data[0x1c] = ((s->param->channels != 1) || is_tpuir (s) || is_gray_all (s) || is_lineart (s)) ? 0x08 : 0x04;

#ifdef DEBUG_TPU_48
    data[0x1d] = 24;
#else
    data[0x1d] = (is_scanning_from_tpu (s)) ? 48
                                            : (((s->param->software_lineart) ? 8 : s->param->depth)
                                               * ((is_tpuir (s) || is_gray_all (s) || is_lineart (s)) ? 3 : s->param->channels)); /* bits per pixel */
#endif

    data[0x1f] = 0x01; /* for 9000F this appears to be 0x00, not sure if that is because of positives */

    if (s->cfg->pid == CS9000F_PID || s->cfg->pid == CS9000F_MII_PID || s->cfg->pid == MG8200_PID)
    {
      data[0x1f] = 0x00;
    }

    data[0x20] = 0xff;
    data[0x21] = 0x81;
    data[0x23] = 0x02;
    data[0x24] = 0x01;

    /* MG8200 & MP990 addition */
    if (s->cfg->pid == MG8200_PID || s->cfg->pid == MP990_PID)
    {
      if (is_scanning_from_tpu (s))
      {
        data[0x25] = 0x01;
      }
    }

    /* CS8800F & CS9000F addition */
    if (s->cfg->pid == CS8800F_PID || s->cfg->pid == CS9000F_PID || s->cfg->pid == CS9000F_MII_PID)
    {
      if (is_scanning_from_tpu (s))
      { /* TPU */
        /* 0x02->negatives, 0x01->positives, paper->0x00
         * no paper in TPU mode */
        if (s->param->mode == PIXMA_SCAN_MODE_NEGATIVE_COLOR
            || s->param->mode == PIXMA_SCAN_MODE_NEGATIVE_GRAY)
        {
          PDBG(
              pixma_dbg (4, "*send_scan_param***** TPU scan negatives *****\n"));
          data[0x1e] = 0x02;
        }
        else
        {
          PDBG(
              pixma_dbg (4, "*send_scan_param***** TPU scan positives *****\n"));
          data[0x1e] = 0x01;
        }
        /* CS8800F: 0x00 for TPU color management */
        if (s->cfg->pid == CS8800F_PID)
          data[0x25] = 0x00;
        /* CS9000F: 0x01 for TPU */
        if (s->cfg->pid == CS9000F_PID || s->cfg->pid == CS9000F_MII_PID)
          data[0x25] = 0x01;
        if (s->param->mode == PIXMA_SCAN_MODE_TPUIR)
          data[0x25] = 0x00;
      }
      else
      { /* flatbed and ADF */
        /* paper->0x00 */
        data[0x1e] = 0x00;
        /* CS8800F: 0x01 normally */
        if (s->cfg->pid == CS8800F_PID)
          data[0x25] = 0x01;
        /* CS9000F: 0x00 normally */
        if (s->cfg->pid == CS9000F_PID || s->cfg->pid == CS9000F_MII_PID)
          data[0x25] = 0x00;
      }
    }

    data[0x30] = 0x01;
  }
  return pixma_exec (s, &mp->cb);
}

static int query_status_3 (pixma_t * s)
{
  mp810_t *mp = (mp810_t *) s->subdriver;
  uint8_t *data;
  int error, status_len;

  status_len = 8;
  data = pixma_newcmd (&mp->cb, cmd_status_3, 0, status_len);
  RET_IF_ERR(pixma_exec (s, &mp->cb));
  memcpy (mp->current_status, data, status_len);
  return error;
}

static int query_status (pixma_t * s)
{
  mp810_t *mp = (mp810_t *) s->subdriver;
  uint8_t *data;
  int error, status_len;

  status_len = (mp->generation == 1) ? 12 : 16;
  data = pixma_newcmd (&mp->cb, cmd_status, 0, status_len);
  RET_IF_ERR(pixma_exec (s, &mp->cb));
  memcpy (mp->current_status, data, status_len);
  PDBG(
      pixma_dbg (3, "Current status: paper=%u cal=%u lamp=%u busy=%u\n", data[1], data[8], data[7], data[9]));
  return error;
}

static int send_time (pixma_t * s)
{
  /* Why does a scanner need a time? */
  time_t now;
  struct tm *t;
  uint8_t *data;
  mp810_t *mp = (mp810_t *) s->subdriver;

  data = pixma_newcmd (&mp->cb, cmd_time, 20, 0);
  pixma_get_time (&now, NULL);
  t = localtime (&now);
  snprintf ((char *) data, 16, "%02d/%02d/%02d %02d:%02d", t->tm_year % 100,
            t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min);
  PDBG(pixma_dbg (3, "Sending time: '%s'\n", (char *) data));
  return pixma_exec (s, &mp->cb);
}

/* TODO: Simplify this function. Read the whole data packet in one shot. */
static int read_image_block (pixma_t * s, uint8_t * header, uint8_t * data)
{
  uint8_t cmd[16];
  mp810_t *mp = (mp810_t *) s->subdriver;
  const int hlen = 8 + 8;
  int error, datalen;

  memset (cmd, 0, sizeof(cmd));
  /* PDBG (pixma_dbg (4, "* read_image_block: last_block\n", mp->last_block)); */
  pixma_set_be16 (cmd_read_image, cmd);
  if ((mp->last_block & 0x20) == 0)
    pixma_set_be32 ((IMAGE_BLOCK_SIZE / 65536) * 65536 + 8, cmd + 0xc);
  else
    pixma_set_be32 (32 + 8, cmd + 0xc);

  mp->state = state_transfering;
  mp->cb.reslen = pixma_cmd_transaction (s, cmd, sizeof(cmd), mp->cb.buf, 512); /* read 1st 512 bytes of image block */
  datalen = mp->cb.reslen;
  if (datalen < 0)
    return datalen;

  memcpy (header, mp->cb.buf, hlen);
  /* PDBG (pixma_dbg (4, "* read_image_block: datalen %i\n", datalen)); */
  /* PDBG (pixma_dbg (4, "* read_image_block: hlen %i\n", hlen)); */
  if (datalen >= hlen)
  {
    datalen -= hlen;
    memcpy (data, mp->cb.buf + hlen, datalen);
    data += datalen;
    if (mp->cb.reslen == 512)
    { /* read the rest of the image block */
      error = pixma_read (s->io, data, IMAGE_BLOCK_SIZE - 512 + hlen);
      RET_IF_ERR(error);
      datalen += error;
    }
  }

  mp->state = state_scanning;
  mp->cb.expected_reslen = 0;
  RET_IF_ERR(pixma_check_result (&mp->cb));
  if (mp->cb.reslen < hlen)
    return PIXMA_EPROTO;
  return datalen;
}

static int read_error_info (pixma_t * s, void *buf, unsigned size)
{
  unsigned len = 16;
  mp810_t *mp = (mp810_t *) s->subdriver;
  uint8_t *data;
  int error;

  data = pixma_newcmd (&mp->cb, cmd_error_info, 0, len);
  RET_IF_ERR(pixma_exec (s, &mp->cb));
  if (buf && len < size)
  {
    size = len;
    /* NOTE: I've absolutely no idea what the returned data mean. */
    memcpy (buf, data, size);
    error = len;
  }
  return error;
}

/*
 handle_interrupt() waits until it receives an interrupt packet or times out.
 It calls send_time() and query_status() if necessary. Therefore, make sure
 that handle_interrupt() is only called from a safe context for send_time()
 and query_status().

 Returns:
 0     timed out
 1     an interrupt packet received
 PIXMA_ECANCELED interrupted by signal
 <0    error
 */
static int handle_interrupt (pixma_t * s, int timeout)
{
  uint8_t buf[64];      /* check max. packet size with 'lsusb -v' for "EP 9 IN" */
  int len;

  len = pixma_wait_interrupt (s->io, buf, sizeof(buf), timeout);
  if (len == PIXMA_ETIMEDOUT)
    return 0;
  if (len < 0)
    return len;
  if (len%16)           /* len must be a multiple of 16 bytes */
  {
    PDBG(pixma_dbg (1, "WARNING:unexpected interrupt packet length %d\n", len));
    return PIXMA_EPROTO;
  }

  /* s->event = 0x0brroott
   * b:  button
   * oo: original
   * tt: target
   * rr: scan resolution
   * poll event with 'scanimage -A' */
  if (s->cfg->pid == MG8200_PID)
  /* button no. in buf[7]
   * size in buf[10] 01=A4; 02=Letter; 08=10x15; 09=13x18; 0b=auto
   * format in buf[11] 01=JPEG; 02=TIFF; 03=PDF; 04=Kompakt-PDF
   * dpi in buf[12] 01=75; 02=150; 03=300; 04=600
   * target = format; original = size; scan-resolution = dpi */
  {
    if (buf[7] & 1)
      s->events = PIXMA_EV_BUTTON1 | buf[11] | buf[10]<<8 | buf[12]<<16;    /* color scan */
    if (buf[7] & 2)
      s->events = PIXMA_EV_BUTTON2 | buf[11] | buf[10]<<8 | buf[12]<<16;    /* b/w scan */
  }
  else if (s->cfg->pid == CS8800F_PID
            || s->cfg->pid == CS9000F_PID
            || s->cfg->pid == CS9000F_MII_PID)
  /* button no. in buf[1]
   * target = button no.
   * "Finish PDF" is Button-2, all others are Button-1 */
  {
    if ((s->cfg->pid == CS8800F_PID && buf[1] == 0x70)
        || (s->cfg->pid != CS8800F_PID && buf[1] == 0x50))
      s->events = PIXMA_EV_BUTTON2 | buf[1] >> 4;  /* button 2 = cancel / end scan */
    else
      s->events = PIXMA_EV_BUTTON1 | buf[1] >> 4;  /* button 1 = start scan */
  }
  else
  /* button no. in buf[0]
   * original in buf[0]
   * target in buf[1] */
  {
    /* More than one event can be reported at the same time. */
    if (buf[3] & 1)
      send_time (s);    /* FIXME: some scanners hang here */
    if (buf[9] & 2)
      query_status (s);

    if (buf[0] & 2)
      s->events = PIXMA_EV_BUTTON2 | buf[1] | ((buf[0] & 0xf0) << 4); /* b/w scan */
    if (buf[0] & 1)
      s->events = PIXMA_EV_BUTTON1 | buf[1] | ((buf[0] & 0xf0) << 4); /* color scan */
  }
  return 1;
}

static int init_ccd_lamp_3 (pixma_t * s)
{
  mp810_t *mp = (mp810_t *) s->subdriver;
  uint8_t *data;
  int error, status_len, tmo;

  status_len = 8;
  RET_IF_ERR(query_status (s));
  RET_IF_ERR(query_status (s));
  RET_IF_ERR(send_cmd_start_calibrate_ccd_3 (s));
  RET_IF_ERR(query_status (s));
  tmo = 20; /* like Windows driver, CCD lamp adjustment */
  while (--tmo >= 0)
  {
    data = pixma_newcmd (&mp->cb, cmd_end_calibrate_ccd_3, 0, status_len);
    RET_IF_ERR(pixma_exec (s, &mp->cb));
    memcpy (mp->current_status, data, status_len);
    PDBG(pixma_dbg (3, "Lamp status: %u , timeout in: %u\n", data[0], tmo));
    if (mp->current_status[0] == 3 || !is_scanning_from_tpu (s))
      break;
    WAIT_INTERRUPT(1000);
  }
  return error;
}

static int wait_until_ready (pixma_t * s)
{
  mp810_t *mp = (mp810_t *) s->subdriver;
  int error, tmo = 60;

  RET_IF_ERR((mp->generation >= 3) ? query_status_3 (s) : query_status (s));
  while (!is_calibrated (s))
  {
    WAIT_INTERRUPT(1000);
    if (mp->generation >= 3)
      RET_IF_ERR(query_status_3 (s));
    if (--tmo == 0)
    {
      PDBG(pixma_dbg (1, "WARNING: Timed out in wait_until_ready()\n"));
      PDBG(query_status (s));
      return PIXMA_ETIMEDOUT;
    }
  }
  return 0;
}

/* the RGB images are shifted by # of lines              */
/* the R image is shifted by colshift[0]                 */
/* the G image is shifted by colshift[1]                 */
/* the B image is shifted by colshift[2]                 */
/* usually one of the RGB images must not be shifted     */
/* which one depends on the scanner                      */
/* some scanners have an additional stripe shift         */
/* e.g. colshift[0]=0, colshift[1]=1, colshift[2]=2      */
/* R image is OK: RGBRGBRGB...                           */
/*                 ^^ ^^ ^^                              */
/*                 || || ||                              */
/* shift G image: RG|RG|RG|...                           */
/*                  |  |  |                              */
/* shift B image: RGBRGBRGB...                           */
/* this doesn't affect the G and B images                */
/* G image will become the R image in the next run       */
/* B image will become the G image in the next run       */
/* the next line will become the B image in the next run */
static uint8_t *
shift_colors (uint8_t * dptr, uint8_t * sptr, unsigned w, unsigned dpi,
              unsigned pid, unsigned c, int * colshft, unsigned strshft)
{
  unsigned i, sr, sg, sb, st;
  UNUSED(dpi);
  UNUSED(pid);
  sr = colshft[0];
  sg = colshft[1];
  sb = colshft[2];

  /* PDBG (pixma_dbg (4, "*shift_colors***** c=%u, w=%i, sr=%u, sg=%u, sb=%u, strshft=%u ***** \n",
        c, w, sr, sg, sb, strshft)); */

  for (i = 0; i < w; i++)
  {
    /* stripes shift for MP970 at 4800 dpi, MP810 at 2400 dpi */
    st = (i % 2 == 0) ? strshft : 0;

    *sptr++ = *(dptr++ + sr + st);
    if (c == 6)
      *sptr++ = *(dptr++ + sr + st);
    *sptr++ = *(dptr++ + sg + st);
    if (c == 6)
      *sptr++ = *(dptr++ + sg + st);
    *sptr++ = *(dptr++ + sb + st);
    if (c == 6)
      *sptr++ = *(dptr++ + sb + st);
  }

  return dptr;
}

static uint8_t *
shift_colorsCS9000 (uint8_t * dptr, uint8_t * sptr, unsigned w, unsigned dpi,
                    unsigned pid, unsigned c, int * colshft, unsigned strshft,
                    unsigned strshft2, unsigned jump)

{
  unsigned i, sr, sg, sb, st, st2;
  UNUSED(dpi);
  UNUSED(pid);
  sr = colshft[0];
  sg = colshft[1];
  sb = colshft[2];

  for (i = 0; i < w; i++)
  {
    if (i < (w / 2))
    {
      /* stripes shift for 1st 4 images for Canoscan 9000F at 9600dpi */
      st = (i % 2 == 0) ? strshft : 0;
      *sptr++ = *(dptr++ + sr + st);
      if (c == 6)
        *sptr++ = *(dptr++ + sr + st);
      *sptr++ = *(dptr++ + sg + st);
      if (c == 6)
        *sptr++ = *(dptr++ + sg + st);
      *sptr++ = *(dptr++ + sb + st);
      if (c == 6)
        *sptr++ = *(dptr++ + sb + st);
    }
    if (i >= (w / 2))
    {
      /* stripes shift for 2nd 4 images for Canoscan 9000F at 9600dpi */
      st2 = (i % 2 == 0) ? strshft2 : 0;
      *sptr++ = *(dptr++ + sr + jump + st2);
      if (c == 6)
        *sptr++ = *(dptr++ + sr + jump + st2);
      *sptr++ = *(dptr++ + sg + jump + st2);
      if (c == 6)
        *sptr++ = *(dptr++ + sg + jump + st2);
      *sptr++ = *(dptr++ + sb + jump + st2);
      if (c == 6)
        *sptr++ = *(dptr++ + sb + jump + st2);
    }
  }
  return dptr;
}

static uint8_t *
shift_colorsCS9000_4800 (uint8_t * dptr, uint8_t * sptr, unsigned w,
                         unsigned dpi, unsigned pid, unsigned c, int * colshft,
                         unsigned strshft, unsigned strshft2, unsigned jump)

{
  unsigned i, sr, sg, sb, st2;
  UNUSED(dpi);
  UNUSED(pid);
  UNUSED(strshft);
  sr = colshft[0];
  sg = colshft[1];
  sb = colshft[2];

  for (i = 0; i < w; i++)
  {
    /* stripes shift for 2nd 4 images
     * for Canoscan 9000F with 16 bit flatbed scans at 4800dpi */
    st2 = (i % 2 == 0) ? strshft2 : 0;
    *sptr++ = *(dptr++ + sr + jump + st2);
    if (c == 6)
      *sptr++ = *(dptr++ + sr + jump + st2);
    *sptr++ = *(dptr++ + sg + jump + st2);
    if (c == 6)
      *sptr++ = *(dptr++ + sg + jump + st2);
    *sptr++ = *(dptr++ + sb + jump + st2);
    if (c == 6)
      *sptr++ = *(dptr++ + sb + jump + st2);
  }
  return dptr;
}

/* under some conditions some scanners have sub images in one line */
/* e.g. doubled image, line size = 8				   */
/* line before reordering: px1 px3 px5 px7 px2 px4 px6 px8         */
/* line after reordering:  px1 px2 px3 px4 px5 px6 px7 px8         */
static void reorder_pixels (uint8_t * linebuf, uint8_t * sptr, unsigned c,
                            unsigned n, unsigned m, unsigned w,
                            unsigned line_size)
{
  unsigned i;

  for (i = 0; i < w; i++)
  { /* process complete line */
    memcpy (linebuf + c * (n * (i % m) + i / m), sptr + c * i, c);
  }
  memcpy (sptr, linebuf, line_size);
}

/* special reorder matrix for mp960 */
static void mp960_reorder_pixels (uint8_t * linebuf, uint8_t * sptr, unsigned c,
                                      unsigned n, unsigned m, unsigned w,
                                      unsigned line_size)
{
  unsigned i, i2;

  /* try and copy 2 px at once */
  for (i = 0; i < w; i++)
  { /* process complete line */
    i2 = i % 2;
    if (i < w / 2)
    {
      if (i2 == 0)
        memcpy (linebuf + c * (n * ((i) % m) + ((i) / m)), sptr + c * i, c);
      else
        memcpy (linebuf + c * (n * ((i - 1) % m) + 1 + ((i) / m)), sptr + c * i, c);
    }
    else
    {
      if (i2 == 0)
        memcpy (linebuf + c * (n * ((i) % m) + ((i) / m) + 1), sptr + c * i, c);
      else
        memcpy (linebuf + c * (n * ((i - 1) % m) + 1 + ((i) / m) + 1), sptr + c * i, c);
    }
  }

  memcpy (sptr, linebuf, line_size);
}

/* special reorder matrix for mp970 */
static void mp970_reorder_pixels (uint8_t * linebuf, uint8_t * sptr, unsigned c,
                                      unsigned w, unsigned line_size)
{
  unsigned i, i8;

  for (i = 0; i < w; i++)
  { /* process complete line */
    i8 = i % 8;
    memcpy (linebuf + c * (i + i8 - ((i8 > 3) ? 7 : 0)), sptr + c * i, c);
  }
  memcpy (sptr, linebuf, line_size);
}

/* special reorder matrix for CS9000F */
static void cs9000f_initial_reorder_pixels (uint8_t * linebuf, uint8_t * sptr,
                                                 unsigned c, unsigned n, unsigned m,
                                                 unsigned w, unsigned line_size)
{
  unsigned i, i2;

  /* try and copy 2 px at once */
  for (i = 0; i < w; i++)
  { /* process complete line */
    i2 = i % 2;
    if (i < w / 8)
    {
      if (i2 == 0)
        memcpy (linebuf + c * (n * ((i) % m) + ((i) / m)), sptr + c * i, c);
      else
        memcpy (linebuf + c * (n * ((i - 1) % m) + 1 + ((i) / m)), sptr + c * i, c);
    }
    else if (i >= w / 8 && i < w / 4)
    {
      if (i2 == 0)
        memcpy (linebuf + c * (n * ((i) % m) + ((i) / m) + 1), sptr + c * i, c);
      else
        memcpy (linebuf + c * (n * ((i - 1) % m) + 1 + ((i) / m) + 1), sptr + c * i, c);
    }
    else if (i >= w / 4 && i < 3 * w / 8)
    {
      if (i2 == 0)
        memcpy (linebuf + c * (n * ((i) % m) + ((i) / m) + 2), sptr + c * i, c);
      else
        memcpy (linebuf + c * (n * ((i - 1) % m) + 1 + ((i) / m) + 2), sptr + c * i, c);
    }
    else if (i >= 3 * w / 8 && i < w / 2)
    {
      if (i2 == 0)
        memcpy (linebuf + c * (n * ((i) % m) + ((i) / m) + 3), sptr + c * i, c);
      else
        memcpy (linebuf + c * (n * ((i - 1) % m) + 1 + ((i) / m) + 3), sptr + c * i, c);
    }
    else if (i >= w / 2 && i < 5 * w / 8)
    {
      if (i2 == 0)
        memcpy (linebuf + c * (n * ((i) % m) + ((i) / m) + 4), sptr + c * i, c);
      else
        memcpy (linebuf + c * (n * ((i - 1) % m) + 1 + ((i) / m) + 4), sptr + c * i, c);
    }
    else if (i >= 5 * w / 8 && i < 3 * w / 4)
    {
      if (i2 == 0)
        memcpy (linebuf + c * (n * ((i) % m) + ((i) / m) + 5), sptr + c * i, c);
      else
        memcpy (linebuf + c * (n * ((i - 1) % m) + 1 + ((i) / m) + 5), sptr + c * i, c);
    }
    else if (i >= 3 * w / 4 && i < 7 * w / 8)
    {
      if (i2 == 0)
        memcpy (linebuf + c * (n * ((i) % m) + ((i) / m) + 6), sptr + c * i, c);
      else
        memcpy (linebuf + c * (n * ((i - 1) % m) + 1 + ((i) / m) + 6), sptr + c * i, c);
    }
    else
    {
      if (i2 == 0)
        memcpy (linebuf + c * (n * ((i) % m) + ((i) / m) + 7), sptr + c * i, c);
      else
        memcpy (linebuf + c * (n * ((i - 1) % m) + 1 + ((i) / m) + 7), sptr + c * i, c);
    }
  }

  memcpy (sptr, linebuf, line_size);
}

/* CS9000F 9600dpi reorder: actually 4800dpi since each pixel is doubled      */
/* need to rearrange each sequence of 16 pairs of pixels as follows:          */
/* start px : 0   2   4   6   8   10  12  14  16  18  20  22  24  26  28  30  */
/* before   : p1a p1b p1c p1d p2a p2b p2c p2d p3a p3b p3c p3d p4a p4b p4c p4d */
/* after    : p1a p3a p1b p3b p1c p3c p1d p3d p2a p4a p2b p4b p2c p4c p2d p4d */
/* start px : 0   16  2   18  4   20  6   22  8   24  10  26  12  28  14  30  */
/* change   :      *   *   *   *   *   *   *   *   *   *   *   *   *   *      */
/* no change:  *                                                           *  */
/* so the 1st and the 3rd set are interleaved, followed by the 2nd and 4th sets interleaved */
static void cs9000f_second_reorder_pixels (uint8_t * linebuf, uint8_t * sptr,
                                                unsigned c, unsigned w,
                                                unsigned line_size)
{
  unsigned i, i8;
  static const int shifts[8] =
  { 2, 4, 6, 8, -8, -6, -4, -2 };

  for (i = 0; i < w; i += 2)
  { /* process complete line */
    i8 = (i >> 1) & 0x7;
    /* Copy 2 pixels at once */
    memcpy (linebuf + c * (i + shifts[i8]), sptr + c * i, c * 2);
  }

  memcpy (sptr, linebuf, line_size);
}

#ifndef TPU_48
static unsigned
pack_48_24_bpc (uint8_t * sptr, unsigned n)
{
  unsigned i;
  uint8_t *cptr, lsb;
  static uint8_t offset = 0;

  cptr = sptr;
  if (n % 2 != 0)
  PDBG (pixma_dbg (3, "WARNING: misaligned image.\n"));
  for (i = 0; i < n; i += 2)
  {
    /* offset = 1 + (offset % 3); */
    lsb = *sptr++;
    *cptr++ = ((*sptr++) << offset) | lsb >> (8 - offset);
  }
  return (n / 2);
}
#endif

/* This function deals both with PIXMA CCD sensors producing shifted color 
 * planes images, Grayscale CCD scan and Generation >= 3 high dpi images.
 * Each complete line in mp->imgbuf is processed for shifting CCD sensor 
 * color planes, reordering pixels above 600 dpi for Generation >= 3, and
 * converting to Grayscale for CCD sensors. */
static unsigned post_process_image_data (pixma_t * s, pixma_imagebuf_t * ib)
{
  mp810_t *mp = (mp810_t *) s->subdriver;
  unsigned c, lines, line_size, n, m, cw, cx, reducelines;
  uint8_t *sptr, *dptr, *gptr, *cptr;
  unsigned /*color_shift, stripe_shift, stripe_shift2,*/ jumplines /*, height*/;
  int test;

  /* For testers: */
  /* set this to 1 in order to get unprocessed images next to one another at 9600dpi */
  /* other resolutions should not be affected */
  /* set this to 2 if you want to see the same with jumplines=0 */
  test = 0;
  jumplines = 0;

  c = ((is_tpuir (s) || is_gray_all (s) || is_lineart (s)) ? 3 : s->param->channels)
      * ((s->param->software_lineart) ? 8 : s->param->depth) / 8;
  cw = c * s->param->w;
  cx = c * s->param->xs;

  /* PDBG (pixma_dbg (4, "*post_process_image_data***** c = %u, cw = %u, cx = %u  *****\n", c, cw, cx)); */

  if (mp->generation >= 3)
    n = s->param->xdpi / 600;
  else
    /* FIXME: maybe need different values for CIS and CCD sensors */
    n = s->param->xdpi / 2400;

  /* Some exceptions to global rules here */
  if (s->cfg->pid == MP970_PID || s->cfg->pid == MP990_PID || s->cfg->pid == MG8200_PID
      || s->cfg->pid == CS8800F_PID || s->cfg->pid == CS9000F_PID || s->cfg->pid == CS9000F_MII_PID)
    n = MIN (n, 4);

  /* exception for 9600dpi on Canoscan 9000F */
  if ((s->cfg->pid == CS9000F_PID || s->cfg->pid == CS9000F_MII_PID) && (s->param->xdpi == 9600))
  {
    n = 8;
    if (test > 0)
      n = 1; /* test if 8 images are next to one another */
  }

  /* test if 2 images are next to one another */
  if ((s->cfg->pid == MP960_PID) && (s->param->xdpi == 4800) && (test > 0))
  {
    n = 1;
  }

  m = (n > 0) ? s->param->wx / n : 1;

  sptr = dptr = gptr = cptr = mp->imgbuf;
  line_size = get_cis_ccd_line_size (s);
  /* PDBG (pixma_dbg (4, "*post_process_image_data***** ----- Set n=%u, m=%u, line_size=%u ----- ***** \n", n, m, line_size)); */
  /* PDBG (pixma_dbg (4, "*post_process_image_data***** ----- spr=dpr=%u, linebuf=%u ----- ***** \n", sptr, mp->linebuf)); */

  lines = (mp->data_left_ofs - mp->imgbuf) / line_size;
  /* PDBG (pixma_dbg (4, "*post_process_image_data***** lines = %i > 2 * mp->color_shift + mp->stripe_shift = %i ***** \n",
               lines, 2 * mp->color_shift + mp->stripe_shift)); */
  /* PDBG (pixma_dbg (4, "*post_process_image_data***** mp->color_shift = %u, mp->stripe_shift = %u, , mp->stripe_shift2 = %u ***** \n",
               mp->color_shift, mp->stripe_shift, mp->stripe_shift2)); */

  /*color_shift = mp->color_shift;*/
  /*stripe_shift = mp->stripe_shift;*/
  /*stripe_shift2 = mp->stripe_shift2;*/
  jumplines = mp->jumplines;

  /* height not needed here! */
  /* removed to avoid confusion */
  /* height = MIN (s->param->h + calc_shifting (s),
                    s->cfg->height * s->param->ydpi / 75); */

  /* have to test if rounding down is OK or not -- currently 0.5 lines is rounded down */
  /* note stripe shifts doubled already in definitions */
  if ((s->cfg->pid == CS9000F_PID || s->cfg->pid == CS9000F_MII_PID) && (s->param->xdpi == 9600) && (test > 0))
  {
    /* using test==2 you can check in GIMP the required offset, and
       use the below line (uncommented) and replace XXX with that
       number, and then compile again with test set to 1. */

    jumplines = 32;
    if (test == 2)
      jumplines = 0;
  }

  /* mp960 test */
  if ((s->cfg->pid == MP960_PID) && (s->param->xdpi == 4800) && (test > 0))
  {
    jumplines = 32;
    if (test == 2)
      jumplines = 0;
  }

  reducelines = ((2 * mp->color_shift + mp->stripe_shift) + jumplines);
  /* PDBG (pixma_dbg (4, "*post_process_image_data: lines %u, reducelines %u \n", lines, reducelines)); */
  if (lines > reducelines)
  { /* (line - reducelines) of image lines can be converted */
    unsigned i;

    lines -= reducelines;

    for (i = 0; i < lines; i++, sptr += line_size)
    { /* convert only full image lines */
      /* Color plane and stripes shift needed by e.g. CCD */
      /* PDBG (pixma_dbg (4, "*post_process_image_data***** Processing with c=%u, n=%u, m=%u, w=%i, line_size=%u ***** \n",
       c, n, m, s->param->wx, line_size)); */
      if (c >= 3)
      {
        if (((s->cfg->pid == CS9000F_PID || s->cfg->pid == CS9000F_MII_PID) && (s->param->xdpi == 9600))
            || ((s->cfg->pid == MP960_PID) && (s->param->xdpi == 4800))
            || ((s->cfg->pid == MP810_PID) && (s->param->xdpi == 4800)))
        {
          dptr = shift_colorsCS9000 (dptr, sptr, s->param->wx, s->param->xdpi,
                                     s->cfg->pid, c, mp->shift,
                                     mp->stripe_shift, mp->stripe_shift2,
                                     jumplines * line_size);
        }

        else if ((s->cfg->pid == CS9000F_PID || s->cfg->pid == CS9000F_MII_PID) /* 9000F: 16 bit flatbed scan at 4800dpi */
                  && ((s->param->mode == PIXMA_SCAN_MODE_COLOR_48)
                      || (s->param->mode == PIXMA_SCAN_MODE_GRAY_16))
                  && (s->param->xdpi == 4800)
                  && (s->param->source == PIXMA_SOURCE_FLATBED))
          dptr = shift_colorsCS9000_4800 (dptr, sptr, s->param->wx,
                                          s->param->xdpi, s->cfg->pid, c,
                                          mp->shift, mp->stripe_shift,
                                          mp->stripe_shift2,
                                          jumplines * line_size);

        else
          /* all except 9000F at 9600dpi */
          dptr = shift_colors (dptr, sptr, s->param->wx, s->param->xdpi,
                               s->cfg->pid, c, mp->shift, mp->stripe_shift);
      }

      /*PDBG (pixma_dbg (4, "*post_process_image_data***** test = %i  *****\n", test)); */

      /*--comment out all between this line and the one below for 9000F tests at 9600dpi or MP960 at 4800dpi ------*/
      /* if ( 0 ) */
      if ((((s->cfg->pid != CS9000F_PID && s->cfg->pid != CS9000F_MII_PID) || (s->param->xdpi < 9600))
          && ((s->cfg->pid != MP960_PID) || (s->param->xdpi < 4800))
          && ((s->cfg->pid != MP810_PID) || (s->param->xdpi < 4800)))
          || (test == 0))
      {
        /* PDBG (pixma_dbg (4, "*post_process_image_data***** MUST GET HERE WHEN TEST == 0  *****\n")); */

        if (!((s->cfg->pid == MP810_PID) && (s->param->xdpi == 4800))
            && !((s->cfg->pid == MP960_PID) && (s->param->xdpi == 4800))
            && !((s->cfg->pid == CS9000F_PID || s->cfg->pid == CS9000F_MII_PID) && (s->param->xdpi == 9600)))
        { /* for both flatbed & TPU */
          /* PDBG (pixma_dbg (4, "*post_process_image_data***** reordering pixels normal n = %i  *****\n", n)); */
          reorder_pixels (mp->linebuf, sptr, c, n, m, s->param->wx, line_size);
        }

        if ((s->cfg->pid == CS9000F_PID || s->cfg->pid == CS9000F_MII_PID) && (s->param->xdpi == 9600))
        {
          /* PDBG (pixma_dbg (4, "*post_process_image_data***** cs900f_initial_reorder_pixels n = %i  *****\n", n)); */
          /* this combines pixels from 8 images 2px at a time from left to right: 1122334455667788... */
          cs9000f_initial_reorder_pixels (mp->linebuf, sptr, c, n, m,
                                          s->param->wx, line_size);
          /* final interleaving */
          cs9000f_second_reorder_pixels (mp->linebuf, sptr, c, s->param->wx,
                                         line_size);
        }

        /* comment: special image format for MP960 in flatbed mode
         at 4800dpi. It is actually 2400dpi, with each pixel
         doubled. The TPU mode has proper pixel ordering */
        if ((((s->cfg->pid == MP960_PID) || (s->cfg->pid == MP810_PID)) && (s->param->xdpi == 4800))
            && (n > 0))
        {
          /* for both flatbed & TPU */
          /* PDBG (pixma_dbg (4, "*post_process_image_data***** flatbed mp960_reordering pixels n = %i  *****\n", n)); */
          mp960_reorder_pixels (mp->linebuf, sptr, c, n, m, s->param->wx,
                                line_size);
        }

        /* comment: MP970, CS8800F, CS9000F specific reordering for 4800 dpi */
        if ((s->cfg->pid == MP970_PID || s->cfg->pid == CS8800F_PID
            || s->cfg->pid == CS9000F_PID || s->cfg->pid == CS9000F_MII_PID
            || s->cfg->pid == MP990_PID) && (s->param->xdpi == 4800))
        {
          /*PDBG (pixma_dbg (4, "*post_process_image_data***** mp970_reordering pixels n = %i  *****\n", n)); */
          mp970_reorder_pixels (mp->linebuf, sptr, c, s->param->wx, line_size);
        }

      }
      /*-------------------------------------------------------*/

      /* PDBG (pixma_dbg (4, "*post_process_image_data: sptr=%u, dptr=%u \n", sptr, dptr)); */

      /* Crop line to selected borders */
      memmove (cptr, sptr + cx, cw);
      /* PDBG (pixma_dbg (4, "*post_process_image_data***** crop line: cx=%u, cw=%u ***** \n", cx, cw)); */

      /* Color to Lineart convert for CCD sensor */
      if (is_lineart (s))
        cptr = gptr = pixma_binarize_line (s->param, gptr, cptr, s->param->w, c);
#ifndef TPUIR_USE_RGB
      /* save IR only for CCD sensor */
      else if (is_tpuir (s))
        cptr = gptr = pixma_r_to_ir (gptr, cptr, s->param->w, c);
      /* Color to Grayscale convert for CCD sensor */
      else if (is_gray_all (s))
#else
      /* IR *and* Color to Grayscale convert for CCD sensor */
      else if (is_tpuir (s) || is_gray_all (s))
#endif
        cptr = gptr = pixma_rgb_to_gray (gptr, cptr, s->param->w, c);
      else
        cptr += cw;
    }
    /* PDBG (pixma_dbg (4, "*post_process_image_data: sptr=%u, dptr=%u \n", sptr, dptr)); */
  }
  ib->rptr = mp->imgbuf;
  ib->rend = cptr;
  return mp->data_left_ofs - sptr; /* # of non processed bytes */
  /* contains shift color data for new lines */
  /* and already received data for the next line */
}

static int mp810_open (pixma_t * s)
{
  mp810_t *mp;
  uint8_t *buf;

  mp = (mp810_t *) calloc (1, sizeof(*mp));
  if (!mp)
    return PIXMA_ENOMEM;

  buf = (uint8_t *) malloc (CMDBUF_SIZE + IMAGE_BLOCK_SIZE);
  if (!buf)
  {
    free (mp);
    return PIXMA_ENOMEM;
  }

  s->subdriver = mp;
  mp->state = state_idle;

  mp->cb.buf = buf;
  mp->cb.size = CMDBUF_SIZE;
  mp->cb.res_header_len = 8;
  mp->cb.cmd_header_len = 16;
  mp->cb.cmd_len_field_ofs = 14;

  mp->imgbuf = buf + CMDBUF_SIZE;

  /* General rules for setting Pixma protocol generation # */
  mp->generation = (s->cfg->pid >= MP810_PID) ? 2 : 1; /* no generation 1 devices anyway, but keep similar to pixma_mp150.c file */

  if (s->cfg->pid >= MP970_PID)
    mp->generation = 3;

  if (s->cfg->pid >= MP990_PID)
    mp->generation = 4;

  /* And exceptions to be added here */
  if (s->cfg->pid == CS8800F_PID)
    mp->generation = 3;

  if (s->cfg->pid == CS9000F_PID || s->cfg->pid == CS9000F_MII_PID)
    mp->generation = 4;

  /* TPU info data setup */
  mp->tpu_datalen = 0;

  if (mp->generation < 4)
  {
    /* Canoscan 8800F ignores commands if not initialized */
    if (s->cfg->pid == CS8800F_PID)
      abort_session (s);
    else
    {
      query_status (s);
      handle_interrupt (s, 200);
      if (mp->generation == 3 && has_ccd_sensor (s))
        send_cmd_start_calibrate_ccd_3 (s);
    }
  }
  return 0;
}

static void mp810_close (pixma_t * s)
{
  mp810_t *mp = (mp810_t *) s->subdriver;

  mp810_finish_scan (s);
  free (mp->cb.buf);
  free (mp);
  s->subdriver = NULL;
}

static int mp810_check_param (pixma_t * s, pixma_scan_param_t * sp)
{
  mp810_t *mp = (mp810_t *) s->subdriver;
  unsigned w_max;

  /* PDBG (pixma_dbg (4, "*mp810_check_param***** Initially: channels=%u, depth=%u, x=%u, y=%u, w=%u, h=%u, xs=%u, wx=%u *****\n",
                   sp->channels, sp->depth, sp->x, sp->y, sp->w, sp->h, sp->xs, sp->wx)); */

  sp->channels = 3;
  sp->software_lineart = 0;
  switch (sp->mode)
  {
    /* standard scan modes
     * 8 bit per channel in color and grayscale mode
     * 16 bit per channel with TPU */
    case PIXMA_SCAN_MODE_GRAY:
    case PIXMA_SCAN_MODE_NEGATIVE_GRAY:
    case PIXMA_SCAN_MODE_TPUIR:
      sp->channels = 1;
      /* fall through */
    case PIXMA_SCAN_MODE_COLOR:
    case PIXMA_SCAN_MODE_NEGATIVE_COLOR:
      sp->depth = 8;
#ifdef TPU_48
#ifndef DEBUG_TPU_48
      if (sp->source == PIXMA_SOURCE_TPU)
#endif
        sp->depth = 16; /* TPU in 16 bits mode */
#endif
      break;
      /* extended scan modes for 48 bit flatbed scanners
       * 16 bit per channel in color and grayscale mode */
    case PIXMA_SCAN_MODE_GRAY_16:
      sp->channels = 1;
      sp->depth = 16;
      break;
    case PIXMA_SCAN_MODE_COLOR_48:
      sp->channels = 3;
      sp->depth = 16;
      break;
      /* software lineart
       * 1 bit per channel */
    case PIXMA_SCAN_MODE_LINEART:
      sp->software_lineart = 1;
      sp->channels = 1;
      sp->depth = 1;
      break;
  }

  /* for software lineart w must be a multiple of 8
   * I don't know why is_lineart(s) doesn't work here */
  if (sp->software_lineart == 1 && sp->w % 8)
  {
    sp->w += 8 - (sp->w % 8);

    /* do not exceed the scanner capability */
    w_max = s->cfg->width * s->cfg->xdpi / 75;
    w_max -= w_max % 8;
    if (sp->w > w_max)
      sp->w = w_max;
  }

  if (sp->source == PIXMA_SOURCE_TPU && !sp->tpu_offset_added)
  {
    unsigned fixed_offset_y; /* TPU offsets for CanoScan 8800F, or other CCD at 300dpi. */
    unsigned max_y; /* max TPU height for CS9000F at 75 dpi */

    /* CanoScan 8800F and others adding an offset depending on resolution */
    /* CS9000F and others maximum TPU height */
    switch (s->cfg->pid)
    {
      case CS8800F_PID:
        fixed_offset_y = 140;
        max_y = MIN (740, s->cfg->height);
        break;
      case CS9000F_PID:
      case CS9000F_MII_PID:
        fixed_offset_y = 146;
        max_y = MIN (740, s->cfg->height);
        break;
      default:
        fixed_offset_y = 0;
        max_y = s->cfg->height;
        break;
    }

    /* cropping y and h to scanable area */
    max_y *= (sp->ydpi) / 75;
    sp->y = MIN(sp->y, max_y);
    sp->h = MIN(sp->h, max_y - sp->y);
    /* PDBG (pixma_dbg (4, "*mp810_check_param***** Cropping: y=%u, h=%u *****\n",
     sp->y, sp->h)); */
    if (!sp->h)
      return SANE_STATUS_INVAL; /* no lines */

    /* Convert the offsets from 300dpi to actual resolution */
    fixed_offset_y = fixed_offset_y * (sp->xdpi) / 300;

    /* In TPU mode, the CS9000F appears to always subtract 146 from the
     vertical starting position, but clamps its at 0. Therefore vertical
     offsets 0 through 146 (@300 dpi) get all mapped onto the same
     physical starting position: line 0. Then, starting from 147, the
     offsets get mapped onto successive physical lines:
     y    line
     0 ->  0
     1 ->  0
     2 ->  0
     ...
     146 ->  0
     147 ->  1
     148 ->  2
     ...
     Since a preview scan is typically made starting from y = 0, but
     partial image scans usually start at y >> 147, this results in a
     discontinuity in the y to line mapping, resulting in wrong offsets.
     To prevent this, we must always add (at least) 146 to the y
     offset before it is sent to the scanner. The scanner will then
     map y = 0 (146) to the first line, y = 1 (147) to the second line,
     and so on. Any distance that is then measured on the preview scan,
     can be translated without any discontinuity.

     However, there is one complication: during a preview scan, which
     normally covers the whole scan area of the scanner, we should _not_
     add the offset because it will result in a reduced number of lines
     being returned (the scan height is clamped in
     pixma_check_scan_param()). Since the frontend has no way of telling
     that the scan area has been reduced, it would derive an incorrect
     effective scan resolution, and any position calculations based on
     this would therefore be inaccurate.

     To prevent this, we don't add the offset in case y = 0, which is
     typically the case during a preview scan (the scanner effectively
     adds the offset for us, see above). In that way we keep the
     linearity and we don't affect the scan area during previews.
     */

    if (sp->y > 0)
      sp->y += fixed_offset_y;

    /* Prevent repeated corrections as check_param may be called multiple times */
    sp->tpu_offset_added = 1;
  }

  if (mp->generation >= 2)
  {
    /* mod 32 and expansion of the X scan limits */
    /* PDBG (pixma_dbg (4, "*mp810_check_param***** (gen>=2) xs=mod32 ----- Initially: x=%u, y=%u, w=%u, h=%u *****\n", sp->x, sp->y, sp->w, sp->h)); */
    sp->xs = (sp->x) % 32;
  }
  else
  {
    sp->xs = 0;
    /* PDBG (pixma_dbg (4, "*mp810_check_param***** (else) xs=0 Selected origin, origin shift: %u, %u *****\n", sp->x, sp->xs)); */
  }
  sp->wx = calc_raw_width (mp, sp);
  sp->line_size = sp->w * sp->channels * (((sp->software_lineart) ? 8 : sp->depth) / 8); /* bytes per line per color after cropping */
  /* PDBG (pixma_dbg (4, "*mp810_check_param***** (else) Final scan width and line-size: %u, %"PRIu64" *****\n", sp->wx, sp->line_size)); */

  /* highest res is 600, 2400, 4800 or 9600 dpi */
  {
    uint8_t k;

    if ((sp->source == PIXMA_SOURCE_ADF || sp->source == PIXMA_SOURCE_ADFDUP)
        && mp->generation >= 4)
      /* ADF/ADF duplex mode: max scan res is 600 dpi, at least for generation 4 */
      k = sp->xdpi / MIN (sp->xdpi, 600);
    else if (sp->source == PIXMA_SOURCE_TPU && sp->mode == PIXMA_SCAN_MODE_TPUIR)
      /* TPUIR mode: max scan res is 2400 dpi */
      k = sp->xdpi / MIN (sp->xdpi, 2400);
    else if (sp->source == PIXMA_SOURCE_TPU && (s->cfg->pid == CS9000F_PID || s->cfg->pid == CS9000F_MII_PID))
      /* CS9000F in TPU mode */
      k = sp->xdpi / MIN (sp->xdpi, 9600);
    else
      /* default */
      k = sp->xdpi / MIN (sp->xdpi, 4800);

    sp->x /= k;
    sp->xs /= k;
    sp->y /= k;
    sp->w /= k;
    sp->wx /= k;
    sp->h /= k;
    sp->xdpi /= k;
    sp->ydpi = sp->xdpi;
  }

  /* lowest res is 75, 150, 300 or 600 dpi */
  {
    uint8_t k;

    if (sp->source == PIXMA_SOURCE_TPU && sp->mode == PIXMA_SCAN_MODE_TPUIR)
      /* TPUIR mode */
      k = MAX (sp->xdpi, 600) / sp->xdpi;
    else if (sp->source == PIXMA_SOURCE_TPU
              && ((mp->generation >= 3) || (s->cfg->pid == MP810_PID) || (s->cfg->pid == MP960_PID)))
      /* TPU mode for generation 3+ scanners
       * MP810, MP960 appear to have a 200dpi mode for low-res scans, not 150 dpi */
      k = MAX (sp->xdpi, 300) / sp->xdpi;
    else if (sp->source == PIXMA_SOURCE_TPU
              || sp->mode == PIXMA_SCAN_MODE_COLOR_48 || sp->mode == PIXMA_SCAN_MODE_GRAY_16)
      /* TPU mode and 16 bit flatbed scans
       * TODO: either the frontend (xsane) cannot handle 48 bit flatbed scans @ 75 dpi (prescan)
       *       or there is a bug in this subdriver */
      k = MAX (sp->xdpi, 150) / sp->xdpi;
    else
      /* default */
      k = MAX (sp->xdpi, 75) / sp->xdpi;

    sp->x *= k;
    sp->xs *= k;
    sp->y *= k;
    sp->w *= k;
    sp->wx *= k;
    sp->h *= k;
    sp->xdpi *= k;
    sp->ydpi = sp->xdpi;
  }

  /* PDBG (pixma_dbg (4, "*mp810_check_param***** Finally: channels=%u, depth=%u, x=%u, y=%u, w=%u, h=%u, xs=%u, wx=%u *****\n",
                   sp->channels, sp->depth, sp->x, sp->y, sp->w, sp->h, sp->xs, sp->wx)); */

  return 0;
}

static int mp810_scan (pixma_t * s)
{
  int error = 0, tmo;
  mp810_t *mp = (mp810_t *) s->subdriver;

  if (mp->state != state_idle)
    return PIXMA_EBUSY;

  /* Generation 4: send XML dialog */
  if (mp->generation == 4 && s->param->adf_pageid == 0)
  {
    if (!send_xml_dialog (s, XML_START_1))
      return PIXMA_EPROTO;
    if (!send_xml_dialog (s, XML_START_2))
      return PIXMA_EPROTO;
  }

  /* clear interrupt packets buffer */
  while (handle_interrupt (s, 0) > 0)
  {
  }

  /* FIXME: Duplex ADF: check paper status only before odd pages (1,3,5,...). */
  if (is_scanning_from_adf (s))
  {
    if ((error = query_status (s)) < 0)
      return error;
    tmo = 10;
    while (!has_paper (s) && --tmo >= 0)
    {
      WAIT_INTERRUPT(1000);
      PDBG(pixma_dbg (2, "No paper in ADF. Timed out in %d sec.\n", tmo));
    }
    if (!has_paper (s))
      return PIXMA_ENO_PAPER;
  }

  if (has_ccd_sensor (s) && (mp->generation <= 2))
  {
    error = send_cmd_e920 (s);
    switch (error)
    {
      case PIXMA_ECANCELED:
      case PIXMA_EBUSY:
        PDBG(pixma_dbg (2, "cmd e920 or d520 returned %s\n", pixma_strerror (error)));
        /* fall through */
      case 0:
        query_status (s);
        break;
      default:
        PDBG(pixma_dbg (1, "WARNING: cmd e920 or d520 failed %s\n", pixma_strerror (error)));
        return error;
    }
    tmo = 3; /* like Windows driver, CCD calibration ? */
    while (--tmo >= 0)
    {
      WAIT_INTERRUPT(1000);
      PDBG(pixma_dbg (2, "CCD Calibration ends in %d sec.\n", tmo));
    }
    /* pixma_sleep(2000000); */
  }

  tmo = 10;
  if (s->param->adf_pageid == 0 || mp->generation <= 2)
  {
    error = start_session (s);
    while (error == PIXMA_EBUSY && --tmo >= 0)
    {
      if (s->cancel)
      {
        error = PIXMA_ECANCELED;
        break;
      }
      PDBG(pixma_dbg (2, "Scanner is busy. Timed out in %d sec.\n", tmo + 1));
      pixma_sleep (1000000);
      error = start_session (s);
    }
    if (error == PIXMA_EBUSY || error == PIXMA_ETIMEDOUT)
    {
      /* The scanner maybe hangs. We try to empty output buffer of the
       * scanner and issue the cancel command. */
      PDBG(pixma_dbg (2, "Scanner hangs? Sending abort_session command.\n"));
      drain_bulk_in (s);
      abort_session (s);
      pixma_sleep (500000);
      error = start_session (s);
    }
    if ((error >= 0) || (mp->generation >= 3))
      mp->state = state_warmup;
    if ((error >= 0) && (mp->generation <= 2))
      error = select_source (s);
    if ((error >= 0) && (mp->generation >= 3) && has_ccd_sensor (s))
      error = init_ccd_lamp_3 (s);
    if ((error >= 0) && !is_scanning_from_tpu (s))
    {
      int i;
      /* FIXME: 48 bit flatbed scans don't need gamma tables
       *        the code below doesn't run */
      /*if (is_color_48 (s) || is_gray_16 (s))
       error = 0;
       else*/
      for (i = (mp->generation >= 3) ? 3 : 1; i > 0 && error >= 0; i--)
        error = send_gamma_table (s);
    }
    else if (error >= 0) /* in TPU mode, for gen 1, 2, and 3 */
      error = send_set_tpu_info (s);
  }
  else
    /* ADF pageid != 0 and gen3 or above */
    pixma_sleep (1000000);

  if ((error >= 0) || (mp->generation >= 3))
    mp->state = state_warmup;
  if (error >= 0)
    error = send_scan_param (s);
  if ((error >= 0) && (mp->generation >= 3))
    error = start_scan_3 (s);
  if (error < 0)
  {
    mp->last_block = 0x38; /* Force abort session if ADF scan */
    mp810_finish_scan (s);
    return error;
  }
  return 0;
}

static int mp810_fill_buffer (pixma_t * s, pixma_imagebuf_t * ib)
{
  int error;
  mp810_t *mp = (mp810_t *) s->subdriver;
  unsigned block_size, bytes_received, proc_buf_size, line_size;
  uint8_t header[16];

  if (mp->state == state_warmup)
  { /* prepare read image data */
    /* PDBG (pixma_dbg (4, "**mp810_fill_buffer***** warmup *****\n")); */

    RET_IF_ERR(wait_until_ready (s));
    pixma_sleep (1000000); /* No need to sleep, actually, but Window's driver
                            * sleep 1.5 sec. */
    mp->state = state_scanning;
    mp->last_block = 0;

    line_size = get_cis_ccd_line_size (s);
    proc_buf_size = (2 * calc_shifting (s) + 2) * line_size;
    mp->cb.buf = realloc (mp->cb.buf, CMDBUF_SIZE + IMAGE_BLOCK_SIZE + proc_buf_size);
    if (!mp->cb.buf)
      return PIXMA_ENOMEM;
    mp->linebuf = mp->cb.buf + CMDBUF_SIZE;
    mp->imgbuf = mp->data_left_ofs = mp->linebuf + line_size;
    mp->data_left_len = 0;
  }

  do
  { /* read complete image data from the scanner */
    if (s->cancel)
      return PIXMA_ECANCELED;
    if ((mp->last_block & 0x28) == 0x28)
    { /* end of image */
      mp->state = state_finished;
      /* PDBG (pixma_dbg (4, "**mp810_fill_buffer***** end of image *****\n")); */
      return 0;
    }
    /* PDBG (pixma_dbg (4, "*mp810_fill_buffer***** moving %u bytes into buffer *****\n", mp->data_left_len)); */
    memmove (mp->imgbuf, mp->data_left_ofs, mp->data_left_len);
    error = read_image_block (s, header, mp->imgbuf + mp->data_left_len);
    if (error < 0)
    {
      if (error == PIXMA_ECANCELED)
      {
        /* NOTE: I see this in traffic logs but I don't know its meaning. */
        read_error_info (s, NULL, 0);
      }
      return error;
    }

    bytes_received = error;
    /*PDBG (pixma_dbg (4, "*mp810_fill_buffer***** %u bytes received by read_image_block *****\n", bytes_received));*/
    block_size = pixma_get_be32 (header + 12);
    mp->last_block = header[8] & 0x38;
    if ((header[8] & ~0x38) != 0)
    {
      PDBG(pixma_dbg (1, "WARNING: Unexpected result header\n"));
      PDBG(pixma_hexdump (1, header, 16));
    }
    PASSERT(bytes_received == block_size);

    if (block_size == 0)
    { /* no image data at this moment. */
      pixma_sleep (10000);
    }
    /* For TPU at 48 bits/pixel to output at 24 bits/pixel */
#ifndef DEBUG_TPU_48
#ifndef TPU_48
    PDBG (pixma_dbg (1, "WARNING: 9000F using 24 instead of 48 bit processing \n"));
#ifndef DEBUG_TPU_24
    if (is_scanning_from_tpu (s))
#endif
    bytes_received = pack_48_24_bpc (mp->imgbuf + mp->data_left_len, bytes_received);
#endif
#endif        
    /* Post-process the image data */
    mp->data_left_ofs = mp->imgbuf + mp->data_left_len + bytes_received;
    mp->data_left_len = post_process_image_data (s, ib);
    mp->data_left_ofs -= mp->data_left_len;
    /* PDBG (pixma_dbg (4, "* mp810_fill_buffer: data_left_len %u \n", mp->data_left_len)); */
    /* PDBG (pixma_dbg (4, "* mp810_fill_buffer: data_left_ofs %u \n", mp->data_left_ofs)); */
  }
  while (ib->rend == ib->rptr);

  return ib->rend - ib->rptr;
}

static void mp810_finish_scan (pixma_t * s)
{
  int error;
  mp810_t *mp = (mp810_t *) s->subdriver;

  switch (mp->state)
  {
    case state_transfering:
      drain_bulk_in (s);
      /* fall through */
    case state_scanning:
    case state_warmup:
    case state_finished:
      /* Send the get TPU info message */
      if (is_scanning_from_tpu (s) && mp->tpu_datalen == 0)
        send_get_tpu_info_3 (s);
      /* FIXME: to process several pages ADF scan, must not send 
       * abort_session and start_session between pages (last_block=0x28) */
      if (mp->generation <= 2 || !is_scanning_from_adf (s)
          || mp->last_block == 0x38)
      {
        error = abort_session (s); /* FIXME: it probably doesn't work in duplex mode! */
        if (error < 0)
          PDBG(pixma_dbg (1, "WARNING:abort_session() failed %d\n", error));

        /* Generation 4: send XML end of scan dialog */
        if (mp->generation == 4)
        {
          if (!send_xml_dialog (s, XML_END))
            PDBG(pixma_dbg (1, "WARNING:XML_END dialog failed \n"));
        }
      }
      mp->state = state_idle;
      /* fall through */
    case state_idle:
      break;
  }
}

static void mp810_wait_event (pixma_t * s, int timeout)
{
  /* FIXME: timeout is not correct. See usbGetCompleteUrbNoIntr() for
   * instance. */
  while (s->events == 0 && handle_interrupt (s, timeout) > 0)
  {
  }
}

static int mp810_get_status (pixma_t * s, pixma_device_status_t * status)
{
  int error;

  RET_IF_ERR(query_status (s));
  status->hardware = PIXMA_HARDWARE_OK;
  status->adf = (has_paper (s)) ? PIXMA_ADF_OK : PIXMA_ADF_NO_PAPER;
  status->cal =
      (is_calibrated (s)) ? PIXMA_CALIBRATION_OK : PIXMA_CALIBRATION_OFF;
  return 0;
}

static const pixma_scan_ops_t pixma_mp810_ops =
{
  mp810_open,
  mp810_close,
  mp810_scan,
  mp810_fill_buffer,
  mp810_finish_scan,
  mp810_wait_event,
  mp810_check_param,
  mp810_get_status
};

#define DEVICE(name, model, pid, dpi, adftpu_min_dpi, adftpu_max_dpi, tpuir_min_dpi, tpuir_max_dpi, w, h, cap) { \
        name,              /* name */               \
        model,             /* model */              \
        CANON_VID, pid,    /* vid pid */            \
        0,                 /* iface */              \
        &pixma_mp810_ops,  /* ops */                \
        dpi, 2*(dpi),      /* xdpi, ydpi */         \
        adftpu_min_dpi, adftpu_max_dpi,  /* adftpu_min_dpi, adftpu_max_dpi */ \
        tpuir_min_dpi, tpuir_max_dpi,    /* tpuir_min_dpi, tpuir_max_dpi */   \
        w, h,              /* width, height */      \
        PIXMA_CAP_EASY_RGB|                         \
        PIXMA_CAP_GRAY|    /* all scanners with software grayscale */ \
        PIXMA_CAP_LINEART| /* all scanners with software lineart */ \
        PIXMA_CAP_GAMMA_TABLE|PIXMA_CAP_EVENTS|cap  \
}

#define END_OF_DEVICE_LIST DEVICE(NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0)

const pixma_config_t pixma_mp810_devices[] =
{
  /* Generation 2: CCD */
  DEVICE ("Canon PIXMA MP810", "MP810", MP810_PID, 4800, 300, 0, 0, 0, 638, 877, PIXMA_CAP_CCD | PIXMA_CAP_TPU),
  DEVICE ("Canon PIXMA MP960", "MP960", MP960_PID, 4800, 300, 0, 0, 0, 638, 877, PIXMA_CAP_CCD | PIXMA_CAP_TPU),

  /* Generation 3 CCD not managed as Generation 2 */
  DEVICE ("Canon Pixma MP970", "MP970", MP970_PID, 4800, 300, 0, 0, 0, 638, 877, PIXMA_CAP_CCD | PIXMA_CAP_TPU),

  /* Flatbed scanner CCD (2007) */
  DEVICE ("Canoscan 8800F", "8800F", CS8800F_PID, 4800, 300, 0, 0, 0, 638, 877, PIXMA_CAP_CCD | PIXMA_CAP_TPU /*| PIXMA_CAP_NEGATIVE*/ | PIXMA_CAP_48BIT),

  /* PIXMA 2008 vintage CCD */
  DEVICE ("Canon MP980 series", "MP980", MP980_PID, 4800, 300, 0, 0, 0, 638, 877, PIXMA_CAP_CCD | PIXMA_CAP_TPU),

  /* Generation 4 CCD */
  DEVICE ("Canon MP990 series", "MP990", MP990_PID, 4800, 300, 0, 0, 0, 638, 877, PIXMA_CAP_CCD | PIXMA_CAP_TPU),

  /* Flatbed scanner (2010) */
  DEVICE ("Canoscan 9000F", "9000F", CS9000F_PID, 4800, 300, 9600, 600, 2400, 638, 877, PIXMA_CAP_CCD | PIXMA_CAP_TPUIR /*| PIXMA_CAP_NEGATIVE*/ | PIXMA_CAP_48BIT),

  /* Latest devices (2010) Generation 4 CCD untested */
  DEVICE ("Canon PIXMA MG8100", "MG8100", MG8100_PID, 4800, 300, 0, 0, 0, 638, 877, PIXMA_CAP_CCD | PIXMA_CAP_TPU),

  /* Latest devices (2011) Generation 4 CCD untested */
  DEVICE ("Canon PIXMA MG8200", "MG8200", MG8200_PID, 4800, 300, 0, 0, 0, 638, 877, PIXMA_CAP_CCD | PIXMA_CAP_TPU),

  /* Flatbed scanner (2013) */
  DEVICE ("Canoscan 9000F Mark II", "9000FMarkII", CS9000F_MII_PID, 4800, 300, 9600, 600, 2400, 638, 877, PIXMA_CAP_CCD | PIXMA_CAP_TPUIR | PIXMA_CAP_48BIT),

  END_OF_DEVICE_LIST
};
