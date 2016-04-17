/* sane - Scanner Access Now Easy.
   Copyright (C) 2000-2003 Jochen Eisinger <jochen.eisinger@gmx.net>
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
   
   This file implements the hardware driver scanners using a 300dpi CCD */

#include "mustek_pp_ccd300.h"

#define MUSTEK_PP_CCD300	4

static void config_ccd_101x (Mustek_pp_Handle * dev);
static void config_ccd (Mustek_pp_Handle * dev);
static void lamp (Mustek_pp_Handle * dev, int lamp_on);

#define CCD300_ASIC1013		0xa8
#define CCD300_ASIC1015		0xa5

#define CCD300_CHANNEL_RED		0
#define CCD300_CHANNEL_GREEN		1
#define CCD300_CHANNEL_BLUE		2
#define CCD300_CHANNEL_GRAY		1

#define CCD300_MAXHSIZE		2600
#define CCD300_MAXVSIZE		3500

/*
 *  Here starts the driver code for the different chipsets
 *
 *  The 1013 & 1015 chipsets share large portions of the code. This
 *  shared functions end with _101x.
 */

static const u_char chan_codes_1013[] = { 0x82, 0x42, 0xC2 };
static const u_char chan_codes_1015[] = { 0x80, 0x40, 0xC0 };
static const u_char fullstep[] = { 0x09, 0x0C, 0x06, 0x03 };
static const u_char halfstep[] = { 0x02, 0x03, 0x01, 0x09,
  0x08, 0x0C, 0x04, 0x06
};
static const u_char voltages[4][3] = { {0x5C, 0x5A, 0x63},
{0xE6, 0xB4, 0xBE},
{0xB4, 0xB4, 0xB4},
{0x64, 0x50, 0x64}
};

/* Forward declarations of 1013/1015 functions */
static void set_ccd_channel_1013 (Mustek_pp_Handle * dev, int channel);
static void motor_backward_1013 (Mustek_pp_Handle * dev);
static void return_home_1013 (Mustek_pp_Handle * dev);
static void motor_forward_1013 (Mustek_pp_Handle * dev);
static void config_ccd_1013 (Mustek_pp_Handle * dev);

static void set_ccd_channel_1015 (Mustek_pp_Handle * dev, int channel);
/* static void motor_backward_1015 (Mustek_pp_Handle * dev); */
static void return_home_1015 (Mustek_pp_Handle * dev, SANE_Bool nowait);
static void motor_forward_1015 (Mustek_pp_Handle * dev);
static void config_ccd_1015 (Mustek_pp_Handle * dev);


/* These functions are common to all 1013/1015 chipsets */

static void
set_led (Mustek_pp_Handle * dev)
{
  mustek_pp_ccd300_priv *priv = dev->priv;

  sanei_pa4s2_writebyte (dev->fd, 6,
			 (priv->motor_step % 5 == 0 ? 0x03 : 0x13));

}

static void
set_sti (Mustek_pp_Handle * dev)
{
  mustek_pp_ccd300_priv *priv = dev->priv;

  sanei_pa4s2_writebyte (dev->fd, 3, 0);
  priv->bank_count++;
  priv->bank_count &= 7;

}

static void
get_bank_count (Mustek_pp_Handle * dev)
{
  u_char val;
  mustek_pp_ccd300_priv *priv = dev->priv;

  sanei_pa4s2_readbegin (dev->fd, 3);
  sanei_pa4s2_readbyte (dev->fd, &val);
  sanei_pa4s2_readend (dev->fd);

  priv->bank_count = (val & 0x07);

}

static void
reset_bank_count (Mustek_pp_Handle * dev)
{
  sanei_pa4s2_writebyte (dev->fd, 6, 7);
}

static void
wait_bank_change (Mustek_pp_Handle * dev, int bankcount, int niceload)
{
  struct timeval start, end;
  unsigned long diff;
  mustek_pp_ccd300_priv *priv = dev->priv;
  int first_time = 1;

  gettimeofday (&start, NULL);

  do
    {
      if ((niceload == 0) && (first_time == 0))
	{
	  usleep (1);		/* could be as well sched_yield */
	  first_time = 0;
	}
      get_bank_count (dev);

      gettimeofday (&end, NULL);
      diff = (end.tv_sec * 1000 + end.tv_usec / 1000) -
	(start.tv_sec * 1000 + start.tv_usec / 1000);

    }
  while ((priv->bank_count != bankcount) && (diff < priv->wait_bank));

}

static void
set_dpi_value (Mustek_pp_Handle * dev)
{
  u_char val = 0;
  mustek_pp_ccd300_priv *priv = dev->priv;

  sanei_pa4s2_writebyte (dev->fd, 6, 0x80);

  switch (priv->hwres)
    {
    case 100:
      val = 0x00;
      break;
    case 200:
      val = 0x10;
      break;
    case 300:
      val = 0x20;
      break;
    }


  if (priv->ccd_type == 1)
    val |= 0x01;

  sanei_pa4s2_writebyte (dev->fd, 5, val);

  sanei_pa4s2_writebyte (dev->fd, 6, 0x00);

  DBG (5, "set_dpi_value: value 0x%02x\n", val);

}

static void
set_line_adjust (Mustek_pp_Handle * dev)
{
  int adjustline;
  mustek_pp_ccd300_priv *priv = dev->priv;

  adjustline = (dev->bottomX - dev->topX) * priv->hwres / 300;
  priv->adjustskip = priv->adjustskip * priv->hwres / 300;

  DBG (5, "set_line_adjust: ppl %u (%u), adjust %u, skip %u\n",
       dev->params.pixels_per_line, (dev->bottomX - dev->topX), adjustline,
       priv->adjustskip);


  sanei_pa4s2_writebyte (dev->fd, 6, 0x11);
  sanei_pa4s2_writebyte (dev->fd, 5, (adjustline + priv->adjustskip) >> 8);
  sanei_pa4s2_writebyte (dev->fd, 6, 0x21);
  sanei_pa4s2_writebyte (dev->fd, 5, (adjustline + priv->adjustskip) & 0xFF);

  sanei_pa4s2_writebyte (dev->fd, 6, 0x01);

}

static void
set_lamp (Mustek_pp_Handle * dev, int lamp_on)
{

  int ctr;
  mustek_pp_ccd300_priv *priv = dev->priv;

  sanei_pa4s2_writebyte (dev->fd, 6, 0xC3);

  for (ctr = 0; ctr < 3; ctr++)
    {
      sanei_pa4s2_writebyte (dev->fd, 6, (lamp_on ? 0x47 : 0x57));
      sanei_pa4s2_writebyte (dev->fd, 6, 0x77);
    }

  priv->motor_step = lamp_on;

  set_led (dev);

}

static void
send_voltages (Mustek_pp_Handle * dev)
{

  int voltage, sel = 8, ctr;
  mustek_pp_ccd300_priv *priv = dev->priv;

  switch (priv->ccd_type)
    {
    case 0:
      voltage = 0;
      break;
    case 1:
      voltage = 1;
      break;
    default:
      voltage = 2;
      break;
    }

  for (ctr = 0; ctr < 3; ctr++)
    {

      sel <<= 1;
      sanei_pa4s2_writebyte (dev->fd, 6, sel);
      sanei_pa4s2_writebyte (dev->fd, 5, voltages[voltage][ctr]);

    }

  sanei_pa4s2_writebyte (dev->fd, 6, 0x00);

}

static int
compar (const void *a, const void *b)
{
  return (signed int) (*(const SANE_Byte *) a) -
    (signed int) (*(const SANE_Byte *) b);
}

static void
set_ccd_channel_101x (Mustek_pp_Handle * dev, int channel)
{
  mustek_pp_ccd300_priv *priv = dev->priv;
  switch (priv->asic)
    {
    case CCD300_ASIC1013:
      set_ccd_channel_1013 (dev, channel);
      break;

    case CCD300_ASIC1015:
      set_ccd_channel_1015 (dev, channel);
      break;
    }
}

static void
motor_forward_101x (Mustek_pp_Handle * dev)
{
  mustek_pp_ccd300_priv *priv = dev->priv;
  switch (priv->asic)
    {
    case CCD300_ASIC1013:
      motor_forward_1013 (dev);
      break;

    case CCD300_ASIC1015:
      motor_forward_1015 (dev);
      break;
    }
}

static void
motor_backward_101x (Mustek_pp_Handle * dev)
{
  mustek_pp_ccd300_priv *priv = dev->priv;
  switch (priv->asic)
    {
    case CCD300_ASIC1013:
      motor_backward_1013 (dev);
      break;

    case CCD300_ASIC1015:
/*      motor_backward_1015 (dev); */
      break;
    }
}

static void
move_motor_101x (Mustek_pp_Handle * dev, int forward)
{
  mustek_pp_ccd300_priv *priv = dev->priv;
  if (forward == SANE_TRUE)
    motor_forward_101x (dev);
  else
    motor_backward_101x (dev);

  wait_bank_change (dev, priv->bank_count, 1);
  reset_bank_count (dev);
}


static void
config_ccd_101x (Mustek_pp_Handle * dev)
{
  mustek_pp_ccd300_priv *priv = dev->priv;
  switch (priv->asic)
    {
    case CCD300_ASIC1013:
      config_ccd_1013 (dev);
      break;

    case CCD300_ASIC1015:
      config_ccd_1015 (dev);
      break;
    }
}



static void
read_line_101x (Mustek_pp_Handle * dev, SANE_Byte * buf, SANE_Int pixel,
		SANE_Int RefBlack, SANE_Byte * calib, SANE_Int * gamma)
{

  SANE_Byte *cal = calib;
  u_char color;
  mustek_pp_ccd300_priv *priv = dev->priv;
  int ctr, skips = priv->adjustskip + 1, cval;

  if (pixel <= 0)
    return;

  sanei_pa4s2_readbegin (dev->fd, 1);


  if (priv->hwres == dev->res)
    {

      while (skips--)
	sanei_pa4s2_readbyte (dev->fd, &color);

      for (ctr = 0; ctr < pixel; ctr++)
	{

	  sanei_pa4s2_readbyte (dev->fd, &color);

	  cval = color;

	  if (cval < RefBlack)
	    cval = 0;
	  else
	    cval -= RefBlack;

	  if (cal)
	    {
	      if (cval >= cal[ctr])
		cval = 0xFF;
	      else
		{
		  cval <<= 8;
		  cval /= (int) cal[ctr];
		}
	    }

	  if (gamma)
	    cval = gamma[cval];

	  buf[ctr] = cval;

	}

    }
  else
    {

      int pos = 0, bpos = 0;

      while (skips--)
	sanei_pa4s2_readbyte (dev->fd, &color);

      ctr = 0;

      do
	{

	  sanei_pa4s2_readbyte (dev->fd, &color);

	  cval = color;

	  if (ctr < (pos >> SANE_FIXED_SCALE_SHIFT))
	    {
	      ctr++;
	      continue;
	    }

	  ctr++;
	  pos += priv->res_step;


	  if (cval < RefBlack)
	    cval = 0;
	  else
	    cval -= RefBlack;

	  if (cal)
	    {
	      if (cval >= cal[bpos])
		cval = 0xFF;
	      else
		{
		  cval <<= 8;
		  cval /= (int) cal[bpos];
		}
	    }

	  if (gamma)
	    cval = gamma[cval];

	  buf[bpos++] = cval;

	}
      while (bpos < pixel);

    }

  sanei_pa4s2_readend (dev->fd);

}

static void
read_average_line_101x (Mustek_pp_Handle * dev, SANE_Byte * buf, int pixel,
			int RefBlack)
{

  SANE_Byte lbuf[4][CCD300_MAXHSIZE * 2];
  int ctr, sum;
  mustek_pp_ccd300_priv *priv = dev->priv;

  for (ctr = 0; ctr < 4; ctr++)
    {

      wait_bank_change (dev, priv->bank_count, 1);
      read_line_101x (dev, lbuf[ctr], pixel, RefBlack, NULL, NULL);
      reset_bank_count (dev);
      if (ctr < 3)
	set_sti (dev);

    }

  for (ctr = 0; ctr < pixel; ctr++)
    {

      sum = lbuf[0][ctr] + lbuf[1][ctr] + lbuf[2][ctr] + lbuf[3][ctr];

      buf[ctr] = (sum / 4);

    }

}

static void
find_black_side_edge_101x (Mustek_pp_Handle * dev)
{
  SANE_Byte buf[CCD300_MAXHSIZE * 2];
  SANE_Byte blackposition[5];
  int pos = 0, ctr, blackpos;
  mustek_pp_ccd300_priv *priv = dev->priv;


  for (ctr = 0; ctr < 20; ctr++)
    {

      motor_forward_101x (dev);
      wait_bank_change (dev, priv->bank_count, 1);
      read_line_101x (dev, buf, CCD300_MAXHSIZE, 0, NULL, NULL);
      reset_bank_count (dev);

      priv->ref_black = priv->ref_red = priv->ref_green = priv->ref_blue =
	buf[0];

      blackpos = CCD300_MAXHSIZE / 4;

      while ((abs (buf[blackpos] - buf[0]) >= 15) && (blackpos > 0))
	blackpos--;

      if (blackpos > 1)
	blackposition[pos++] = blackpos;

      if (pos == 5)
	break;

    }

  blackpos = 0;

  for (ctr = 0; ctr < pos; ctr++)
    if (blackposition[ctr] > blackpos)
      blackpos = blackposition[ctr];

  if (blackpos < 0x66)
    blackpos = 0x6A;

  priv->blackpos = blackpos;
  priv->saved_skipcount = (blackpos + 12) & 0xFF;

}

static void
min_color_levels_101x (Mustek_pp_Handle * dev)
{

  SANE_Byte buf[CCD300_MAXHSIZE * 2];
  int ctr, sum = 0;
  mustek_pp_ccd300_priv *priv = dev->priv;

  for (ctr = 0; ctr < 8; ctr++)
    {

      set_ccd_channel_101x (dev, CCD300_CHANNEL_RED);
      set_sti (dev);
      wait_bank_change (dev, priv->bank_count, 1);

      read_line_101x (dev, buf, CCD300_MAXHSIZE, 0, NULL, NULL);

      reset_bank_count (dev);

      sum += buf[3];

    }

  priv->ref_red = sum / 8;

  sum = 0;

  for (ctr = 0; ctr < 8; ctr++)
    {

      set_ccd_channel_101x (dev, CCD300_CHANNEL_GREEN);
      set_sti (dev);
      wait_bank_change (dev, priv->bank_count, 1);

      read_line_101x (dev, buf, CCD300_MAXHSIZE, 0, NULL, NULL);

      reset_bank_count (dev);

      sum += buf[3];

    }

  priv->ref_green = sum / 8;

  sum = 0;

  for (ctr = 0; ctr < 8; ctr++)
    {

      set_ccd_channel_101x (dev, CCD300_CHANNEL_BLUE);
      set_sti (dev);
      wait_bank_change (dev, priv->bank_count, 1);

      read_line_101x (dev, buf, CCD300_MAXHSIZE, 0, NULL, NULL);

      reset_bank_count (dev);

      sum += buf[3];

    }

  priv->ref_blue = sum / 8;

}


static void
max_color_levels_101x (Mustek_pp_Handle * dev)
{

  int ctr, line, sum;
  SANE_Byte rbuf[32][CCD300_MAXHSIZE * 2];
  SANE_Byte gbuf[32][CCD300_MAXHSIZE * 2];
  SANE_Byte bbuf[32][CCD300_MAXHSIZE * 2];
  mustek_pp_ccd300_priv *priv = dev->priv;

  SANE_Byte maxbuf[32];

  for (ctr = 0; ctr < 32; ctr++)
    {

      if (dev->mode == MODE_COLOR)
	{

	  set_ccd_channel_101x (dev, CCD300_CHANNEL_RED);
	  motor_forward_101x (dev);

	  read_average_line_101x (dev, rbuf[ctr], dev->params.pixels_per_line,
				  priv->ref_red);

	  set_ccd_channel_101x (dev, CCD300_CHANNEL_GREEN);
	  set_sti (dev);

	  read_average_line_101x (dev, gbuf[ctr], dev->params.pixels_per_line,
				  priv->ref_green);

	  set_ccd_channel_101x (dev, CCD300_CHANNEL_BLUE);
	  set_sti (dev);

	  read_average_line_101x (dev, bbuf[ctr], dev->params.pixels_per_line,
				  priv->ref_blue);

	}
      else
	{

	  priv->channel = CCD300_CHANNEL_GRAY;

	  motor_forward_101x (dev);

	  read_average_line_101x (dev, gbuf[ctr], dev->params.pixels_per_line,
				  priv->ref_black);

	}

    }


  for (ctr = 0; ctr < dev->params.pixels_per_line; ctr++)
    {
      for (line = 0; line < 32; line++)
	maxbuf[line] = gbuf[line][ctr];

      qsort (maxbuf, 32, sizeof (maxbuf[0]), compar);

      sum = maxbuf[4] + maxbuf[5] + maxbuf[6] + maxbuf[7];

      priv->calib_g[ctr] = sum / 4;

    }

  if (dev->mode == MODE_COLOR)
    {

      for (ctr = 0; ctr < dev->params.pixels_per_line; ctr++)
	{
	  for (line = 0; line < 32; line++)
	    maxbuf[line] = rbuf[line][ctr];

	  qsort (maxbuf, 32, sizeof (maxbuf[0]), compar);

	  sum = maxbuf[4] + maxbuf[5] + maxbuf[6] + maxbuf[7];

	  priv->calib_r[ctr] = sum / 4;

	}

      for (ctr = 0; ctr < dev->params.pixels_per_line; ctr++)
	{
	  for (line = 0; line < 32; line++)
	    maxbuf[line] = bbuf[line][ctr];

	  qsort (maxbuf, 32, sizeof (maxbuf[0]), compar);

	  sum = maxbuf[4] + maxbuf[5] + maxbuf[6] + maxbuf[7];

	  priv->calib_b[ctr] = sum / 4;

	}

    }

}

static void
find_black_top_edge_101x (Mustek_pp_Handle * dev)
{

  int lines = 0, ctr, pos;
  SANE_Byte buf[CCD300_MAXHSIZE * 2];
  mustek_pp_ccd300_priv *priv = dev->priv;

  priv->channel = CCD300_CHANNEL_GRAY;

  do
    {

      motor_forward_101x (dev);
      wait_bank_change (dev, priv->bank_count, 1);

      read_line_101x (dev, buf, CCD300_MAXHSIZE, priv->ref_black, NULL, NULL);

      reset_bank_count (dev);

      pos = 0;

      for (ctr = priv->blackpos; ctr > priv->blackpos - 10; ctr--)
	if (buf[ctr] <= 15)
	  pos++;

    }
  while ((pos >= 8) && (lines++ < 67));

}

static void
calibrate_device_101x (Mustek_pp_Handle * dev)
{

  int saved_ppl = dev->params.pixels_per_line, ctr;
  mustek_pp_ccd300_priv *priv = dev->priv;

  priv->saved_mode = dev->mode;
  priv->saved_invert = dev->invert;
  priv->saved_skipcount = priv->skipcount;
  priv->saved_skipimagebyte = priv->skipimagebytes;
  priv->saved_adjustskip = priv->adjustskip;
  priv->saved_res = dev->res;
  priv->saved_hwres = priv->hwres;
  priv->saved_res_step = priv->res_step;
  priv->saved_line_step = priv->line_step;
  priv->saved_channel = priv->channel;

  dev->params.pixels_per_line = CCD300_MAXHSIZE;
  priv->hwres = dev->res = 300;
  dev->mode = MODE_GRAYSCALE;
  priv->skipcount = priv->skipimagebytes = 0;
  dev->invert = SANE_FALSE;
  priv->channel = CCD300_CHANNEL_GRAY;

  config_ccd_101x (dev);
  get_bank_count (dev);

  find_black_side_edge_101x (dev);

  for (ctr = 0; ctr < 4; ctr++)
    move_motor_101x (dev, SANE_TRUE);

  dev->mode = priv->saved_mode;
  dev->invert = priv->saved_invert;
  priv->skipcount = priv->saved_skipcount;
  priv->skipimagebytes = priv->saved_skipimagebyte;
  priv->adjustskip = priv->saved_adjustskip;
  dev->res = priv->saved_res;
  priv->hwres = priv->saved_hwres;
  priv->res_step = priv->saved_res_step;
  priv->line_step = priv->saved_line_step;
  priv->channel = priv->saved_channel;

  priv->hwres = dev->res = 300;
  priv->skipcount = priv->skipimagebytes = 0;
  dev->invert = SANE_FALSE;

  config_ccd_101x (dev);
  get_bank_count (dev);

  if ((dev->mode == MODE_COLOR) && (priv->ccd_type != 0))
    min_color_levels_101x (dev);

  dev->mode = priv->saved_mode;
  dev->invert = priv->saved_invert;
  priv->skipcount = priv->saved_skipcount;
  priv->skipimagebytes = priv->saved_skipimagebyte;
  priv->adjustskip = priv->saved_adjustskip;
  dev->res = priv->saved_res;
  priv->hwres = priv->saved_hwres;
  priv->res_step = priv->saved_res_step;
  priv->line_step = priv->saved_line_step;
  priv->channel = priv->saved_channel;

  dev->params.pixels_per_line = saved_ppl;
  dev->invert = SANE_FALSE;

  config_ccd_101x (dev);
  get_bank_count (dev);

  max_color_levels_101x (dev);

  dev->params.pixels_per_line = CCD300_MAXHSIZE;
  dev->mode = MODE_GRAYSCALE;
  priv->hwres = dev->res = 300;
  priv->skipcount = priv->skipimagebytes = 0;
  dev->invert = SANE_FALSE;

  config_ccd_101x (dev);
  get_bank_count (dev);

  find_black_top_edge_101x (dev);

  dev->mode = priv->saved_mode;
  dev->invert = priv->saved_invert;
  priv->skipcount = priv->saved_skipcount;
  priv->skipimagebytes = priv->saved_skipimagebyte;
  priv->adjustskip = priv->saved_adjustskip;
  dev->res = priv->saved_res;
  priv->hwres = priv->saved_hwres;
  priv->res_step = priv->saved_res_step;
  priv->line_step = priv->saved_line_step;
  priv->channel = priv->saved_channel;

  dev->params.pixels_per_line = saved_ppl;

  config_ccd_101x (dev);
  get_bank_count (dev);

}

static void
get_grayscale_line_101x (Mustek_pp_Handle * dev, SANE_Byte * buf)
{

  int skips;
  mustek_pp_ccd300_priv *priv = dev->priv;

  priv->line_diff += SANE_FIX (300.0 / (float) dev->res);

  skips = (priv->line_diff >> SANE_FIXED_SCALE_SHIFT);

  while (--skips)
    {
      motor_forward_101x (dev);
      wait_bank_change (dev, priv->bank_count, 1);
      reset_bank_count (dev);
    }

  priv->line_diff &= 0xFFFF;

  motor_forward_101x (dev);
  wait_bank_change (dev, priv->bank_count, 1);

  read_line_101x (dev, buf, dev->params.pixels_per_line, priv->ref_black,
		  priv->calib_g, NULL);

  reset_bank_count (dev);

}

static void
get_lineart_line_101x (Mustek_pp_Handle * dev, SANE_Byte * buf)
{

  int ctr;
  SANE_Byte gbuf[CCD300_MAXHSIZE * 2];
  mustek_pp_ccd300_priv *priv = dev->priv;

  get_grayscale_line_101x (dev, gbuf);

  memset (buf, 0xFF, dev->params.bytes_per_line);

  for (ctr = 0; ctr < dev->params.pixels_per_line; ctr++)
    buf[ctr >> 3] ^= ((gbuf[ctr] > priv->bw) ? (1 << (7 - ctr % 8)) : 0);

}

static void
get_color_line_101x (Mustek_pp_Handle * dev, SANE_Byte * buf)
{

  SANE_Byte *red, *blue, *src, *dest;
  int gotline = 0, ctr;
  int gored, goblue, gogreen;
  mustek_pp_ccd300_priv *priv = dev->priv;
  int step = priv->line_step;

  do
    {

      red = priv->red[priv->redline];
      blue = priv->blue[priv->blueline];

      priv->ccd_line++;

      if ((priv->rdiff >> SANE_FIXED_SCALE_SHIFT) == priv->ccd_line)
	{
	  gored = 1;
	  priv->rdiff += step;
	}
      else
	gored = 0;

      if ((priv->bdiff >> SANE_FIXED_SCALE_SHIFT) == priv->ccd_line)
	{
	  goblue = 1;
	  priv->bdiff += step;
	}
      else
	goblue = 0;

      if ((priv->gdiff >> SANE_FIXED_SCALE_SHIFT) == priv->ccd_line)
	{
	  gogreen = 1;
	  priv->gdiff += step;
	}
      else
	gogreen = 0;

      if (!gored && !goblue && !gogreen)
	{
	  motor_forward_101x (dev);
	  wait_bank_change (dev, priv->bank_count, 1);
	  reset_bank_count (dev);
	  if (priv->ccd_line >= (priv->line_step >> SANE_FIXED_SCALE_SHIFT))
	    priv->redline = (priv->redline + 1) % priv->green_offs;
	  if (priv->ccd_line >=
	      priv->blue_offs + (priv->line_step >> SANE_FIXED_SCALE_SHIFT))
	    priv->blueline = (priv->blueline + 1) % priv->blue_offs;
	  continue;
	}

      if (gored)
	priv->channel = CCD300_CHANNEL_RED;
      else if (goblue)
	priv->channel = CCD300_CHANNEL_BLUE;
      else
	priv->channel = CCD300_CHANNEL_GREEN;

      motor_forward_101x (dev);
      wait_bank_change (dev, priv->bank_count, 1);

      if (priv->ccd_line >= priv->green_offs && gogreen)
	{
	  src = red;
	  dest = buf;

	  for (ctr = 0; ctr < dev->params.pixels_per_line; ctr++)
	    {
	      *dest = *src++;
	      dest += 3;
	    }
	}

      if (gored)
	{

	  read_line_101x (dev, red, dev->params.pixels_per_line,
			  priv->ref_red, priv->calib_r, NULL);

	  reset_bank_count (dev);

	}

      priv->redline = (priv->redline + 1) % priv->green_offs;

      if (priv->ccd_line >= priv->green_offs && gogreen)
	{
	  src = blue;
	  dest = buf + 2;


	  for (ctr = 0; ctr < dev->params.pixels_per_line; ctr++)
	    {
	      *dest = *src++;
	      dest += 3;
	    }

	}

      if (goblue)
	{
	  if (gored)
	    {
	      set_ccd_channel_101x (dev, CCD300_CHANNEL_BLUE);
	      set_sti (dev);
	      wait_bank_change (dev, priv->bank_count, 1);
	    }

	  read_line_101x (dev, blue, dev->params.pixels_per_line,
			  priv->ref_blue, priv->calib_b, NULL);

	  reset_bank_count (dev);

	}

      if (priv->ccd_line >=
	  priv->blue_offs + (priv->line_step >> SANE_FIXED_SCALE_SHIFT))
	priv->blueline = (priv->blueline + 1) % priv->blue_offs;

      if (gogreen)
	{

	  if (gored || goblue)
	    {
	      set_ccd_channel_101x (dev, CCD300_CHANNEL_GREEN);
	      set_sti (dev);
	      wait_bank_change (dev, priv->bank_count, 1);
	    }

	  read_line_101x (dev, priv->green, dev->params.pixels_per_line,
			  priv->ref_green, priv->calib_g, NULL);

	  reset_bank_count (dev);

	  src = priv->green;
	  dest = buf + 1;

	  for (ctr = 0; ctr < dev->params.pixels_per_line; ctr++)
	    {
	      *dest = *src++;
	      dest += 3;
	    }

	  gotline = 1;
	}

    }
  while (!gotline);

}



/* these functions are for the 1013 chipset */

static void
set_ccd_channel_1013 (Mustek_pp_Handle * dev, int channel)
{
  mustek_pp_ccd300_priv *priv = dev->priv;
  priv->channel = channel;
  sanei_pa4s2_writebyte (dev->fd, 6, chan_codes_1013[channel]);
}

static void
motor_backward_1013 (Mustek_pp_Handle * dev)
{
  mustek_pp_ccd300_priv *priv = dev->priv;

  priv->motor_step++;
  set_led (dev);

  if (priv->motor_phase > 3)
    priv->motor_phase = 3;

  sanei_pa4s2_writebyte (dev->fd, 6, 0x62);
  sanei_pa4s2_writebyte (dev->fd, 5, fullstep[priv->motor_phase]);

  priv->motor_phase = (priv->motor_phase == 0 ? 3 : priv->motor_phase - 1);

  set_ccd_channel_1013 (dev, priv->channel);
  set_sti (dev);

}

static void
return_home_1013 (Mustek_pp_Handle * dev)
{
  u_char ishome;
  int ctr;
  mustek_pp_ccd300_priv *priv = dev->priv;

  /* 1013 can't return home all alone, nowait ignored */

  for (ctr = 0; ctr < 4500; ctr++)
    {

      /* check_is_home_1013 */
      sanei_pa4s2_readbegin (dev->fd, 2);
      sanei_pa4s2_readbyte (dev->fd, &ishome);
      sanei_pa4s2_readend (dev->fd);

      /* yes, it should be is_not_home */
      if ((ishome & 1) == 0)
	break;

      motor_backward_1013 (dev);
      wait_bank_change (dev, priv->bank_count, 0);
      reset_bank_count (dev);

    }

}

static void
motor_forward_1013 (Mustek_pp_Handle * dev)
{

  int ctr;
  mustek_pp_ccd300_priv *priv = dev->priv;

  priv->motor_step++;
  set_led (dev);

  for (ctr = 0; ctr < 2; ctr++)
    {

      sanei_pa4s2_writebyte (dev->fd, 6, 0x62);
      sanei_pa4s2_writebyte (dev->fd, 5, halfstep[priv->motor_phase]);

      priv->motor_phase =
	(priv->motor_phase == 7 ? 0 : priv->motor_phase + 1);

    }

  set_ccd_channel_1013 (dev, priv->channel);
  set_sti (dev);
}



static void
config_ccd_1013 (Mustek_pp_Handle * dev)
{
  mustek_pp_ccd300_priv *priv = dev->priv;

  if (dev->res != 0)
    priv->res_step = SANE_FIX ((float) priv->hwres / (float) dev->res);

  set_dpi_value (dev);

  /* set_start_channel_1013 (dev); */

  sanei_pa4s2_writebyte (dev->fd, 6, 0x05);

  switch (dev->mode)
    {
    case MODE_BW:
    case MODE_GRAYSCALE:
      priv->channel = CCD300_CHANNEL_GRAY;
      break;

    case MODE_COLOR:
      priv->channel = CCD300_CHANNEL_RED;
      break;

    }

  set_ccd_channel_1013 (dev, priv->channel);

  /* set_invert_1013 (dev); */

  sanei_pa4s2_writebyte (dev->fd, 6,
			 (dev->invert == SANE_TRUE ? 0x04 : 0x14));

  sanei_pa4s2_writebyte (dev->fd, 6, 0x37);
  reset_bank_count (dev);

  sanei_pa4s2_writebyte (dev->fd, 6, 0x27);
  sanei_pa4s2_writebyte (dev->fd, 6, 0x67);
  sanei_pa4s2_writebyte (dev->fd, 6, 0x17);
  sanei_pa4s2_writebyte (dev->fd, 6, 0x77);

  /* set_initial_skip_1013 (dev); */

  sanei_pa4s2_writebyte (dev->fd, 6, 0x41);

  priv->adjustskip = priv->skipcount + priv->skipimagebytes;

  DBG (5, "config_ccd_1013: adjustskip %u\n", priv->adjustskip);

  sanei_pa4s2_writebyte (dev->fd, 5, priv->adjustskip / 16 + 2);

  priv->adjustskip %= 16;

  sanei_pa4s2_writebyte (dev->fd, 6, 0x81);
  sanei_pa4s2_writebyte (dev->fd, 5, 0x70);
  sanei_pa4s2_writebyte (dev->fd, 6, 0x01);


  set_line_adjust (dev);

  get_bank_count (dev);

}

/* these functions are for the 1015 chipset */


static void
motor_control_1015 (Mustek_pp_Handle * dev, u_char control)
{
  u_char val;

  DBG (5, "motor_controll_1015: control code 0x%02x\n",
       (unsigned int) control);

  sanei_pa4s2_writebyte (dev->fd, 6, 0xF6);
  sanei_pa4s2_writebyte (dev->fd, 6, 0x22);
  sanei_pa4s2_writebyte (dev->fd, 5, control);
  sanei_pa4s2_writebyte (dev->fd, 6, 0x02);

  do
    {

      sanei_pa4s2_readbegin (dev->fd, 2);
      sanei_pa4s2_readbyte (dev->fd, &val);
      sanei_pa4s2_readend (dev->fd);

    }
  while ((val & 0x08) != 0);

}

static void
return_home_1015 (Mustek_pp_Handle * dev, SANE_Bool nowait)
{

  u_char ishome, control = 0xC3;

  motor_control_1015 (dev, control);

  do
    {

      /* check_is_home_1015 */
      sanei_pa4s2_readbegin (dev->fd, 2);
      sanei_pa4s2_readbyte (dev->fd, &ishome);
      sanei_pa4s2_readend (dev->fd);

      if (nowait)
	break;

      usleep (1000);		/* much nicer load */

    }
  while ((ishome & 2) == 0);

}

static void
motor_forward_1015 (Mustek_pp_Handle * dev)
{
  u_char control = 0x1B;
  mustek_pp_ccd300_priv *priv = dev->priv;

  priv->motor_step++;
  set_led (dev);


  motor_control_1015 (dev, control);

  set_ccd_channel_1015 (dev, priv->channel);
  set_sti (dev);

}

/*
static void
motor_backward_1015 (Mustek_pp_Handle * dev)
{
  u_char control = 0x43;
  mustek_pp_ccd300_priv *priv = dev->priv;

  priv->motor_step++;

  set_led (dev);

  switch (priv->ccd_type)
    {
    case 1:
      control = 0x1B;
      break;

    default:
      control = 0x43;
      break;
    }

  motor_control_1015 (dev, control);

  set_ccd_channel_1015 (dev, priv->channel);
  set_sti (dev);

}
*/


static void
set_ccd_channel_1015 (Mustek_pp_Handle * dev, int channel)
{

  u_char chancode = chan_codes_1015[channel];
  mustek_pp_ccd300_priv *priv = dev->priv;

  priv->channel = channel;

  priv->image_control &= 0x34;
  chancode |= priv->image_control;


  priv->image_control = chancode;

  sanei_pa4s2_writebyte (dev->fd, 6, chancode);

}


static void
config_ccd_1015 (Mustek_pp_Handle * dev)
{

  u_char val;
  mustek_pp_ccd300_priv *priv = dev->priv;

  if (dev->res != 0)
    priv->res_step = SANE_FIX ((float) priv->hwres / (float) dev->res);


  set_dpi_value (dev);

  priv->image_control = 4;

  /* set_start_channel_1015 (dev); */

  switch (dev->mode)
    {
    case MODE_BW:
    case MODE_GRAYSCALE:
      priv->channel = CCD300_CHANNEL_GRAY;
      break;

    case MODE_COLOR:
      priv->channel = CCD300_CHANNEL_RED;
      break;

    }

  set_ccd_channel_1015 (dev, priv->channel);


  /* set_invert_1015 (dev); */

  priv->image_control &= 0xE4;

  if (dev->invert == SANE_FALSE)
    priv->image_control |= 0x10;


  sanei_pa4s2_writebyte (dev->fd, 6, priv->image_control);

  sanei_pa4s2_writebyte (dev->fd, 6, 0x23);
  sanei_pa4s2_writebyte (dev->fd, 5, 0x00);

  sanei_pa4s2_writebyte (dev->fd, 6, 0x43);

  switch (priv->ccd_type)
    {
    case 1:
      val = 0x6B;
      break;
    case 4:
      val = 0x9F;
      break;
    default:
      val = 0x92;
      break;
    }

  sanei_pa4s2_writebyte (dev->fd, 5, val);
  sanei_pa4s2_writebyte (dev->fd, 6, 0x03);

  sanei_pa4s2_writebyte (dev->fd, 6, 0x37);
  reset_bank_count (dev);

  sanei_pa4s2_writebyte (dev->fd, 6, 0x27);
  sanei_pa4s2_writebyte (dev->fd, 6, 0x67);
  sanei_pa4s2_writebyte (dev->fd, 6, 0x17);
  sanei_pa4s2_writebyte (dev->fd, 6, 0x77);

  /* set_initial_skip_1015 (dev); */

  sanei_pa4s2_writebyte (dev->fd, 6, 0x41);

  priv->adjustskip = priv->skipcount + priv->skipimagebytes;

  /* if (dev->CCD.mode == MODE_COLOR)
     dev->CCD.adjustskip <<= 3; */


  sanei_pa4s2_writebyte (dev->fd, 5, priv->adjustskip / 32 + 1);

  priv->adjustskip %= 32;


  sanei_pa4s2_writebyte (dev->fd, 6, 0x81);

  /* expose time */
  switch (priv->ccd_type)
    {
    case 1:

      val = 0xA8;
      break;
    case 0:
      val = 0x8A;
      break;
    default:
      val = 0xA8;
      break;
    }

  sanei_pa4s2_writebyte (dev->fd, 5, val);

  sanei_pa4s2_writebyte (dev->fd, 6, 0x01);


  set_line_adjust (dev);

  get_bank_count (dev);

}


/* these functions are interfaces only */
static void
config_ccd (Mustek_pp_Handle * dev)
{
  mustek_pp_ccd300_priv *priv = dev->priv;

  DBG (5, "config_ccd: %d dpi, mode %d, invert %d, size %d\n",
       priv->hwres, dev->mode, dev->invert, dev->params.pixels_per_line);

  switch (priv->asic)
    {
    case CCD300_ASIC1013:
      config_ccd_1013 (dev);
      break;

    case CCD300_ASIC1015:
      config_ccd_1015 (dev);
      break;
    }

}

static void
return_home (Mustek_pp_Handle * dev, SANE_Bool nowait)
{
  mustek_pp_ccd300_priv *priv = dev->priv;

  priv->saved_mode = dev->mode;
  priv->saved_invert = dev->invert;
  priv->saved_skipcount = priv->skipcount;
  priv->saved_skipimagebyte = priv->skipimagebytes;
  priv->saved_adjustskip = priv->adjustskip;
  priv->saved_res = dev->res;
  priv->saved_hwres = priv->hwres;
  priv->saved_res_step = priv->res_step;
  priv->saved_line_step = priv->line_step;
  priv->saved_channel = priv->channel;


  priv->hwres = dev->res = 100;
  dev->mode = MODE_GRAYSCALE;

  priv->skipcount = priv->skipimagebytes = 0;

  config_ccd (dev);

  switch (priv->asic)
    {
    case CCD300_ASIC1013:
      return_home_1013 (dev);
      break;

    case CCD300_ASIC1015:
      return_home_1015 (dev, nowait);
      break;
    }


  dev->mode = priv->saved_mode;
  dev->invert = priv->saved_invert;
  priv->skipcount = priv->saved_skipcount;
  priv->skipimagebytes = priv->saved_skipimagebyte;
  priv->adjustskip = priv->saved_adjustskip;
  dev->res = priv->saved_res;
  priv->hwres = priv->saved_hwres;
  priv->res_step = priv->saved_res_step;
  priv->line_step = priv->saved_line_step;
  priv->channel = priv->saved_channel;
  priv->motor_step = 0;

  config_ccd (dev);
}

static void
lamp (Mustek_pp_Handle * dev, int lamp_on)
{

  set_lamp (dev, lamp_on);

}

static void
set_voltages (Mustek_pp_Handle * dev)
{
  send_voltages (dev);
}

static void
move_motor (Mustek_pp_Handle * dev, int count, int forward)
{

  int ctr;

  DBG (5, "move_motor: %u steps (%s)\n", count,
       (forward == SANE_TRUE ? "forward" : "backward"));


  for (ctr = 0; ctr < count; ctr++)
    {

      move_motor_101x (dev, forward);

    }


}

static void
calibrate (Mustek_pp_Handle * dev)
{
  mustek_pp_ccd300_priv *priv = dev->priv;

  DBG (5, "calibrate entered (asic = 0x%02x)\n", priv->asic);

  calibrate_device_101x (dev);

  DBG (5, "calibrate: ref_black %d, blackpos %d\n",
       priv->ref_black, priv->blackpos);

}


static void
get_lineart_line (Mustek_pp_Handle * dev, SANE_Byte * buf)
{
  get_lineart_line_101x (dev, buf);
}

static void
get_grayscale_line (Mustek_pp_Handle * dev, SANE_Byte * buf)
{

  get_grayscale_line_101x (dev, buf);
}

static void
get_color_line (Mustek_pp_Handle * dev, SANE_Byte * buf)
{

  get_color_line_101x (dev, buf);

}


static SANE_Status
ccd300_init (SANE_Int options, SANE_String_Const port,
	     SANE_String_Const name, SANE_Attach_Callback attach)
{
  SANE_Status status;
  unsigned char asic, ccd;
  int fd;

  if (options != CAP_NOTHING)
    {
      DBG (1, "ccd300_init: called with unknown options (%#02x)\n", options);
      return SANE_STATUS_INVAL;
    }

  /* try to attach to he supplied port */
  status = sanei_pa4s2_open (port, &fd);

  if (status != SANE_STATUS_GOOD)
    {
      DBG (2, "ccd300_init: couldn't attach to port ``%s'' (%s)\n",
	   port, sane_strstatus (status));
      return status;
    }

  sanei_pa4s2_enable (fd, SANE_TRUE);
  sanei_pa4s2_readbegin (fd, 0);
  sanei_pa4s2_readbyte (fd, &asic);
  sanei_pa4s2_readend (fd);
  sanei_pa4s2_readbegin (fd, 2);
  sanei_pa4s2_readbyte (fd, &ccd);
  sanei_pa4s2_readend (fd);
  sanei_pa4s2_enable (fd, SANE_FALSE);
  sanei_pa4s2_close (fd);

  if (asic != CCD300_ASIC1013 && asic != CCD300_ASIC1015)
    {
      DBG (2, "ccd300_init: scanner not recognized (unknown ASIC id %#02x)\n",
	   asic);
      return SANE_STATUS_INVAL;
    }

  ccd &= (asic == CCD300_ASIC1013 ? 0x04 : 0x05);

  DBG (3, "ccd_init: found scanner on port ``%s'' (ASIC id %#02x, CCD %d)\n",
       port, asic, ccd);

  return attach (port, name, MUSTEK_PP_CCD300, options);

}

static void
ccd300_capabilities (SANE_Int info, SANE_String * model,
		     SANE_String * vendor, SANE_String * type,
		     SANE_Int * maxres, SANE_Int * minres,
		     SANE_Int * maxhsize, SANE_Int * maxvsize,
		     SANE_Int * caps)
{
  *model = strdup ("600 III EP Plus");
  *vendor = strdup ("Mustek");
  *type = strdup ("flatbed (CCD 300 dpi)");
  DBG (3,
       "ccd300_capabilities: 600 III EP Plus flatbed CCD (300 dpi) scanner\n");

  *maxres = 300;
  *minres = 50;
  *maxhsize = CCD300_MAXHSIZE;
  *maxvsize = CCD300_MAXVSIZE;
  *caps = info | CAP_INVERT | CAP_LAMP_OFF;
}

static SANE_Status
ccd300_open (SANE_String port, SANE_Int caps, SANE_Int * fd)
{
  SANE_Status status;

  if (caps & ~(CAP_NOTHING | CAP_INVERT | CAP_LAMP_OFF))
    {
      DBG (1, "ccd300_open: called with unknown capabilities (%#02x)\n",
	   caps);
      return SANE_STATUS_INVAL;
    }

  DBG (3, "ccd300_open: called for port ``%s''\n", port);

  status = sanei_pa4s2_open (port, fd);

  if (status != SANE_STATUS_GOOD)
    DBG (2, "ccd300_open: open failed (%s)\n", sane_strstatus (status));

  return status;
}

static void
ccd300_setup (SANE_Handle handle)
{
  Mustek_pp_Handle *dev = handle;
  mustek_pp_ccd300_priv *priv;
  unsigned char asic, ccd;

  DBG (3, "ccd300_setup: called for port ``%s''\n", dev->dev->port);

  if ((priv = malloc (sizeof (mustek_pp_ccd300_priv))) == NULL)
    {
      DBG (1, "ccd300_setup: not enough memory\n");
      return;			/* can you here the shit hitting the fan? */
    }

  dev->priv = priv;
  memset (priv, 0, sizeof (mustek_pp_ccd300_priv));

  priv->bw = 128;
  priv->wait_bank = 700;
  priv->top = 47;

  sanei_pa4s2_enable (dev->fd, SANE_TRUE);

  sanei_pa4s2_readbegin (dev->fd, 0);
  sanei_pa4s2_readbyte (dev->fd, &asic);
  sanei_pa4s2_readend (dev->fd);
  sanei_pa4s2_readbegin (dev->fd, 2);
  sanei_pa4s2_readbyte (dev->fd, &ccd);
  sanei_pa4s2_readend (dev->fd);
  ccd &= (asic == CCD300_ASIC1013 ? 0x04 : 0x05);
  priv->asic = asic;
  priv->ccd_type = ccd;

  return_home (dev, SANE_TRUE);
  lamp (dev, SANE_TRUE);
  sanei_pa4s2_enable (dev->fd, SANE_FALSE);
  dev->lamp_on = time (NULL);
  dev->res = priv->hwres = 300;
  dev->mode = MODE_COLOR;
}

static void
ccd300_close (SANE_Handle handle)
{

  Mustek_pp_Handle *dev = handle;
  mustek_pp_ccd300_priv *priv = dev->priv;

  DBG (3, "ccd300_close: called for port ``%s''\n", dev->dev->port);

  sanei_pa4s2_enable (dev->fd, SANE_TRUE);
  lamp (dev, SANE_FALSE);
  return_home (dev, SANE_FALSE);
  sanei_pa4s2_enable (dev->fd, SANE_FALSE);

  sanei_pa4s2_close (dev->fd);
  free (priv);

  DBG (3, "ccd300_close: device shut down and all buffers freed\n");
}

static SANE_Status
ccd300_config (SANE_Handle handle, SANE_String_Const optname,
	       SANE_String_Const optval)
{
  Mustek_pp_Handle *dev = handle;
  mustek_pp_ccd300_priv *priv = dev->priv;
  int value = -1;

  DBG (3, "ccd300_config: called for port ``%s'' (%s%s%s)\n",
       dev->dev->port,
       optname, (optval ? " = " : ""), (optval ? optval : ""));

  if (!strcmp (optname, "bw"))
    {

      if (!optval)
	{
	  DBG (1, "ccd300_config: missing value for option ``bw''\n");
	  return SANE_STATUS_INVAL;
	}

      /* ok, ok, should be strtol... know what? send me a patch. */
      value = atoi (optval);

      if ((value < 0) || (value > 255))
	{
	  DBG (1,
	       "ccd300_config: value ``%s'' for option ``bw'' is out of range (0 <= bw <= 255)\n",
	       optval);
	  return SANE_STATUS_INVAL;
	}

      priv->bw = value;

    }
  else if (!strcmp (optname, "waitbank"))
    {

      if (!optval)
	{
	  DBG (1, "ccd300_config: missing value for option ``waitbank''\n");
	  return SANE_STATUS_INVAL;
	}

      value = atoi (optval);

      if (value < 0)
	{
	  DBG (1,
	       "ccd300_config: value ``%s'' for option ``waitbank'' is out of range (>= 0)\n",
	       optval);
	  return SANE_STATUS_INVAL;
	}

      priv->wait_bank = value;
    }
  else if (!strcmp (optname, "top"))
    {

      if (!optval)
	{
	  DBG (1, "ccd300_config: missing value for option ``top''\n");
	  return SANE_STATUS_INVAL;
	}

      value = atoi (optval);

      if (value < 0)
	{
	  DBG (1,
	       "ccd300_config: value ``%s'' for option ``top'' is out of range (>= 0)\n",
	       optval);
	  return SANE_STATUS_INVAL;
	}

      priv->top = value;
    }
  else
    {
      DBG (1, "ccd300_config: unknown option ``%s''", optname);
      return SANE_STATUS_INVAL;
    }

  return SANE_STATUS_GOOD;

}

static void
ccd300_stop (SANE_Handle handle)
{
  Mustek_pp_Handle *dev = handle;
  mustek_pp_ccd300_priv *priv = dev->priv;
  int cnt;

  DBG (3, "ccd300_stop: stopping scan operating on port ``%s''\n",
       dev->dev->port);

  sanei_pa4s2_enable (dev->fd, SANE_TRUE);
  return_home (dev, SANE_TRUE);
  sanei_pa4s2_enable (dev->fd, SANE_FALSE);

  free (priv->calib_r);
  free (priv->calib_g);
  free (priv->calib_b);

  if (priv->red)
    {
      for (cnt = 0; cnt < priv->green_offs; cnt++)
	free (priv->red[cnt]);
      free (priv->red);
    }
  if (priv->blue)
    {
      for (cnt = 0; cnt < priv->blue_offs; cnt++)
	free (priv->blue[cnt]);
      free (priv->blue);
    }
  free (priv->green);

  priv->calib_r = priv->calib_g = priv->calib_b = NULL;
  priv->red = priv->blue = NULL;
  priv->green = NULL;

}

static SANE_Status
ccd300_start (SANE_Handle handle)
{
  Mustek_pp_Handle *dev = handle;
  mustek_pp_ccd300_priv *priv = dev->priv;

  DBG (3, "ccd300_start: called for port ``%s''\n", dev->dev->port);

  if (dev->res <= 100)
    priv->hwres = 100;
  else if (dev->res <= 200)
    priv->hwres = 200;
  else if (dev->res <= 300)
    priv->hwres = 300;

  DBG (4, "ccd300_start: setting hardware resolution to %d dpi\n",
       priv->hwres);

  priv->skipimagebytes = dev->topX;
  
  sanei_pa4s2_enable (dev->fd, SANE_TRUE);
  config_ccd (dev);
  set_voltages (dev);
  get_bank_count (dev);

  if (priv->bank_count != 0)
    {
      DBG (2, "ccd300_start: bank count is not zero...\n");
    }

  return_home (dev, SANE_FALSE);

  priv->motor_step = 0;

  /* allocate memory for calibration */
  if ((priv->calib_g = malloc (dev->params.pixels_per_line)) == NULL)
    {
      sanei_pa4s2_enable (dev->fd, SANE_FALSE);
      DBG (1, "ccd300_start: not enough memory\n");
      return SANE_STATUS_NO_MEM;
    }

  if (dev->mode == MODE_COLOR)
    {
      priv->calib_r = malloc (dev->params.pixels_per_line);
      priv->calib_b = malloc (dev->params.pixels_per_line);

      if ((priv->calib_r == NULL) || (priv->calib_b == NULL))
	{
	  free (priv->calib_g);
	  free (priv->calib_r);
	  free (priv->calib_b);
	  priv->calib_r = priv->calib_g = priv->calib_b = NULL;

	  sanei_pa4s2_enable (dev->fd, SANE_FALSE);
	  DBG (1, "ccd300_start: not enough memory\n");
	  return SANE_STATUS_NO_MEM;
	}
    }

  calibrate (dev);

  if (priv->ccd_type == 1)
    {
      priv->blue_offs = 4;
      priv->green_offs = 8;
    }
  else
    {
      priv->blue_offs = 8;
      priv->green_offs = 16;
    }

  move_motor (dev, priv->top + dev->topY -
	      (dev->mode == MODE_COLOR ? priv->green_offs : 0), SANE_TRUE);

  if (priv->ccd_type == 1)
    sanei_pa4s2_writebyte (dev->fd, 6, 0x15);

  sanei_pa4s2_enable (dev->fd, SANE_FALSE);

  if (dev->mode == MODE_COLOR)
    {
      int failed = SANE_FALSE, cnt;

      priv->line_step = SANE_FIX (300.0 / (float) dev->res);
      priv->rdiff = priv->line_step;
      priv->bdiff = priv->rdiff + (priv->blue_offs << SANE_FIXED_SCALE_SHIFT);
      priv->gdiff =
	priv->rdiff + (priv->green_offs << SANE_FIXED_SCALE_SHIFT);

      priv->red = malloc (sizeof (SANE_Byte *) * priv->green_offs);
      priv->blue = malloc (sizeof (SANE_Byte *) * priv->blue_offs);
      priv->green = malloc (dev->params.pixels_per_line);

      if ((priv->red == NULL) || (priv->blue == NULL)
	  || (priv->green == NULL))
	{
	  free (priv->calib_r);
	  free (priv->calib_g);
	  free (priv->calib_b);
	  priv->calib_r = priv->calib_g = priv->calib_b = NULL;

	  free (priv->red);
	  free (priv->green);
	  free (priv->blue);
	  priv->red = priv->blue = NULL;
	  priv->green = NULL;

	  DBG (1, "ccd300_start: not enough memory for ld buffers\n");
	  return SANE_STATUS_NO_MEM;
	}

      /* note to myself: better allocate one huge chunk of memory and set
         pointers */
      for (cnt = 0; cnt < priv->green_offs; cnt++)
	if ((priv->red[cnt] = malloc (dev->params.pixels_per_line)) == NULL)
	  failed = SANE_TRUE;

      for (cnt = 0; cnt < priv->blue_offs; cnt++)
	if ((priv->blue[cnt] = malloc (dev->params.pixels_per_line)) == NULL)
	  failed = SANE_TRUE;

      if (failed == SANE_TRUE)
	{
	  free (priv->calib_r);
	  free (priv->calib_g);
	  free (priv->calib_b);
	  priv->calib_r = priv->calib_g = priv->calib_b = NULL;

	  for (cnt = 0; cnt < priv->green_offs; cnt++)
	    free (priv->red[cnt]);
	  for (cnt = 0; cnt < priv->blue_offs; cnt++)
	    free (priv->blue[cnt]);

	  free (priv->red);
	  free (priv->green);
	  free (priv->blue);
	  priv->red = priv->blue = NULL;
	  priv->green = NULL;

	  DBG (1, "ccd300_start: not enough memory for ld buffers\n");
	  return SANE_STATUS_NO_MEM;
	}

      priv->redline = priv->blueline = priv->ccd_line = 0;
    }

  priv->lines = 0;
  priv->lines_left = dev->params.lines;

  DBG (3, "ccd300_start: device ready for scanning\n");

  return SANE_STATUS_GOOD;
}

static void
ccd300_read (SANE_Handle handle, SANE_Byte * buffer)
{
  Mustek_pp_Handle *dev = handle;
  mustek_pp_ccd300_priv *priv = dev->priv;

  DBG (3, "ccd300_read: receiving one line from port ``%s''\n",
       dev->dev->port);

  sanei_pa4s2_enable (dev->fd, SANE_TRUE);

  switch (dev->mode)
    {
    case MODE_BW:
      get_lineart_line (dev, buffer);
      break;

    case MODE_GRAYSCALE:
      get_grayscale_line (dev, buffer);
      break;

    case MODE_COLOR:
      get_color_line (dev, buffer);
      break;
    }

  priv->lines_left--;
  priv->lines++;

  DBG (4, "ccd300_read: %d lines read (%d to go)\n", priv->lines,
       priv->lines_left);

  if (priv->lines_left == 0)
    {
      DBG (3, "ccd300_read: scan finished\n");
      return_home (dev, SANE_TRUE);
    }

  sanei_pa4s2_enable (dev->fd, SANE_FALSE);

}
