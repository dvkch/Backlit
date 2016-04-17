/* sane - Scanner Access Now Easy.

   Copyright (C) 2000 Mustek.
   Originally maintained by Tom Wang <tom.wang@mustek.com.tw>

   Copyright (C) 2001, 2002 by Henning Meier-Geinitz.

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

   This file implements a SANE backend for Mustek 1200UB and similar 
   USB flatbed scanners.  */

#include "mustek_usb_mid.h"
#include "mustek_usb_low.c"

/* ------------------ sensor NEC 3797 600 CIS functions ------------------- */

static SANE_Word usb_mid_n600_optical_x_dpi[] =
  { 600, 400, 300, 200, 100, 50, 0 };

SANE_Status
usb_mid_n600_prepare_rgb_600_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_n600_prepare_rgb_600_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_TRUE, SW_P6P6));
  RIE (usb_low_set_soft_resample (chip, 1));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_n600_prepare_rgb_600_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_n600_prepare_rgb_400_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_n600_prepare_rgb_400_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_TRUE, SW_P4P6));
  RIE (usb_low_set_soft_resample (chip, 1));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_n600_prepare_rgb_400_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_n600_prepare_rgb_300_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_n600_prepare_rgb_300_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_TRUE, SW_P3P6));
  RIE (usb_low_set_soft_resample (chip, 1));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_n600_prepare_rgb_300_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_n600_prepare_rgb_200_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_n600_prepare_rgb_200_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_TRUE, SW_P2P6));
  RIE (usb_low_set_soft_resample (chip, 1));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_n600_prepare_rgb_200_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_n600_prepare_rgb_100_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_n600_prepare_rgb_100_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_TRUE, SW_P1P6));
  RIE (usb_low_set_soft_resample (chip, 1));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_n600_prepare_rgb_100_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_n600_prepare_rgb_50_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_n600_prepare_rgb_50_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_TRUE, SW_P1P6));
  RIE (usb_low_set_soft_resample (chip, 2));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_n600_prepare_rgb_50_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_n600_prepare_mono_600_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_n600_prepare_mono_600_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_TRUE, SW_P6P6));
  RIE (usb_low_set_soft_resample (chip, 1));
  RIE (usb_low_set_led_light_all (chip, SANE_TRUE));
  DBG (6, "usb_mid_n600_prepare_mono_600_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_n600_prepare_mono_400_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_n600_prepare_mono_400_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_TRUE, SW_P4P6));
  RIE (usb_low_set_soft_resample (chip, 1));
  RIE (usb_low_set_led_light_all (chip, SANE_TRUE));
  DBG (6, "usb_mid_n600_prepare_mono_400_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_n600_prepare_mono_300_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_n600_prepare_mono_300_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_TRUE, SW_P3P6));
  RIE (usb_low_set_soft_resample (chip, 1));
  RIE (usb_low_set_led_light_all (chip, SANE_TRUE));
  DBG (6, "usb_mid_n600_prepare_mono_300_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_n600_prepare_mono_200_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_n600_prepare_mono_200_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_TRUE, SW_P2P6));
  RIE (usb_low_set_soft_resample (chip, 1));
  RIE (usb_low_set_led_light_all (chip, SANE_TRUE));
  DBG (6, "usb_mid_n600_prepare_mono_200_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_n600_prepare_mono_100_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_n600_prepare_mono_100_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_TRUE, SW_P1P6));
  RIE (usb_low_set_soft_resample (chip, 1));
  RIE (usb_low_set_led_light_all (chip, SANE_TRUE));
  DBG (6, "usb_mid_n600_prepare_mono_100_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_n600_prepare_mono_50_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_n600_prepare_mono_50_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_TRUE, SW_P1P6));
  RIE (usb_low_set_soft_resample (chip, 2));
  RIE (usb_low_set_led_light_all (chip, SANE_TRUE));
  DBG (6, "usb_mid_n600_prepare_mono_50_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_n600_prepare_rgb (ma1017 * chip, SANE_Word dpi)
{
  DBG (6, "usb_mid_n600_prepare_rgb: start\n");
  switch (dpi)
    {
    case 50:
      return usb_mid_n600_prepare_rgb_50_dpi (chip);
      break;
    case 100:
      return usb_mid_n600_prepare_rgb_100_dpi (chip);
      break;
    case 200:
      return usb_mid_n600_prepare_rgb_200_dpi (chip);
      break;
    case 300:
      return usb_mid_n600_prepare_rgb_300_dpi (chip);
      break;
    case 400:
      return usb_mid_n600_prepare_rgb_400_dpi (chip);
      break;
    case 600:
      return usb_mid_n600_prepare_rgb_600_dpi (chip);
      break;
    default:
      DBG (3, "usb_mid_n600_prepare_rgb: unmatched dpi: %d\n", dpi);
      return SANE_STATUS_INVAL;
      break;
    }
  DBG (6, "usb_mid_n600_prepare_rgb: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_n600_prepare_mono (ma1017 * chip, SANE_Word dpi)
{
  DBG (6, "usb_mid_n600_prepare_mono: start\n");
  switch (dpi)
    {
    case 50:
      return usb_mid_n600_prepare_mono_50_dpi (chip);
      break;
    case 100:
      return usb_mid_n600_prepare_mono_100_dpi (chip);
      break;
    case 200:
      return usb_mid_n600_prepare_mono_200_dpi (chip);
      break;
    case 300:
      return usb_mid_n600_prepare_mono_300_dpi (chip);
      break;
    case 400:
      return usb_mid_n600_prepare_mono_400_dpi (chip);
      break;
    case 600:
      return usb_mid_n600_prepare_mono_600_dpi (chip);
      break;
    default:
      DBG (6, "usb_mid_n600_prepare_mono: unmatched dpi: %d\n", dpi);
      return SANE_STATUS_INVAL;
      break;
    }
  return SANE_STATUS_GOOD;
}

/* ---------------------- sensor 600 CIS functions ----------------------- */

static SANE_Word usb_mid_c600_optical_x_dpi[] =
  { 600, 400, 300, 200, 150, 100, 50, 0 };

SANE_Status
usb_mid_c600_prepare_rgb_600_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c600_prepare_rgb_600_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_FALSE, SW_P6P6));
  RIE (usb_low_set_soft_resample (chip, 1));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_c600_prepare_rgb_600_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c600_prepare_rgb_400_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c600_prepare_rgb_400_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_FALSE, SW_P4P6));
  RIE (usb_low_set_soft_resample (chip, 1));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_c600_prepare_rgb_400_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c600_prepare_rgb_300_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c600_prepare_rgb_300_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_FALSE, SW_P3P6));
  RIE (usb_low_set_soft_resample (chip, 1));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_c600_prepare_rgb_300_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c600_prepare_rgb_200_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c600_prepare_rgb_200_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_FALSE, SW_P2P6));
  RIE (usb_low_set_soft_resample (chip, 1));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_c600_prepare_rgb_200_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c600_prepare_rgb_150_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c600_prepare_rgb_150_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_FALSE, SW_P3P6));
  RIE (usb_low_set_soft_resample (chip, 2));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_c600_prepare_rgb_150_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c600_prepare_rgb_100_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c600_prepare_rgb_100_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_FALSE, SW_P1P6));
  RIE (usb_low_set_soft_resample (chip, 1));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_c600_prepare_rgb_100_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c600_prepare_rgb_50_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c600_prepare_rgb_50_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_FALSE, SW_P1P6));
  RIE (usb_low_set_soft_resample (chip, 2));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_c600_prepare_rgb_50_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c600_prepare_mono_600_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c600_prepare_mono_600_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_FALSE, SW_P6P6));
  RIE (usb_low_set_soft_resample (chip, 1));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_c600_prepare_mono_600_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c600_prepare_mono_400_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c600_prepare_mono_400_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_FALSE, SW_P4P6));
  RIE (usb_low_set_soft_resample (chip, 1));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_c600_prepare_mono_400_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c600_prepare_mono_300_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c600_prepare_mono_300_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_FALSE, SW_P3P6));
  RIE (usb_low_set_soft_resample (chip, 1));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_c600_prepare_mono_300_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c600_prepare_mono_200_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c600_prepare_mono_200_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_FALSE, SW_P2P6));
  RIE (usb_low_set_soft_resample (chip, 1));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_c600_prepare_mono_200_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c600_prepare_mono_150_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c600_prepare_mono_150_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_FALSE, SW_P3P6));
  RIE (usb_low_set_soft_resample (chip, 2));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_c600_prepare_mono_150_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c600_prepare_mono_100_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c600_prepare_mono_100_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_FALSE, SW_P1P6));
  RIE (usb_low_set_soft_resample (chip, 1));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_c600_prepare_mono_100_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c600_prepare_mono_50_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c600_prepare_mono_50_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_FALSE, SW_P1P6));
  RIE (usb_low_set_soft_resample (chip, 2));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_c600_prepare_mono_50_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c600_prepare_rgb (ma1017 * chip, SANE_Word dpi)
{
  DBG (6, "usb_mid_c600_prepare_rgb: start\n");
  switch (dpi)
    {
    case 50:
      return usb_mid_c600_prepare_rgb_50_dpi (chip);
      break;
    case 100:
      return usb_mid_c600_prepare_rgb_100_dpi (chip);
      break;
    case 150:
      return usb_mid_c600_prepare_rgb_150_dpi (chip);
      break;
    case 200:
      return usb_mid_c600_prepare_rgb_200_dpi (chip);
      break;
    case 300:
      return usb_mid_c600_prepare_rgb_300_dpi (chip);
      break;
    case 400:
      return usb_mid_c600_prepare_rgb_400_dpi (chip);
      break;
    case 600:
      return usb_mid_c600_prepare_rgb_600_dpi (chip);
      break;
    default:
      DBG (3, "usb_mid_c600_prepare_rgb: unmatched dpi: %d\n", dpi);
      return SANE_STATUS_INVAL;
      break;
    }
  DBG (6, "usb_mid_c600_prepare_rgb: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c600_prepare_mono (ma1017 * chip, SANE_Word dpi)
{
  DBG (6, "usb_mid_c600_prepare_mono: start\n");
  switch (dpi)
    {
    case 50:
      return usb_mid_c600_prepare_mono_50_dpi (chip);
      break;
    case 100:
      return usb_mid_c600_prepare_mono_100_dpi (chip);
      break;
    case 150:
      return usb_mid_c600_prepare_mono_150_dpi (chip);
      break;
    case 200:
      return usb_mid_c600_prepare_mono_200_dpi (chip);
      break;
    case 300:
      return usb_mid_c600_prepare_mono_300_dpi (chip);
      break;
    case 400:
      return usb_mid_c600_prepare_mono_400_dpi (chip);
      break;
    case 600:
      return usb_mid_c600_prepare_mono_600_dpi (chip);
      break;
    default:
      DBG (6, "usb_mid_c600_prepare_mono: unmatched dpi: %d\n", dpi);
      return SANE_STATUS_INVAL;
      break;
    }
  return SANE_STATUS_GOOD;
}



/* ------------------- sensor 300/600 CIS functions ----------------------- */


static SANE_Word usb_mid_c300600_optical_x_dpi[] =
  { 600, 400, 300, 200, 150, 100, 50, 0 };

SANE_Status
usb_mid_c300600_prepare_rgb_600_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c300600_prepare_rgb_600_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_TRUE, SW_P6P6));
  RIE (usb_low_set_soft_resample (chip, 1));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_c300600_prepare_rgb_600_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c300600_prepare_rgb_400_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c300600_prepare_rgb_400_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_TRUE, SW_P4P6));
  RIE (usb_low_set_soft_resample (chip, 1));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_c300600_prepare_rgb_400_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c300600_prepare_rgb_300_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c300600_prepare_rgb_300_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_FALSE, SW_P6P6));
  RIE (usb_low_set_soft_resample (chip, 1));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_c300600_prepare_rgb_300_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c300600_prepare_rgb_200_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c300600_prepare_rgb_200_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_TRUE, SW_P2P6));
  RIE (usb_low_set_soft_resample (chip, 1));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_c300600_prepare_rgb_200_dpi: exit\n");
  return SANE_STATUS_GOOD;
}


SANE_Status
usb_mid_c300600_prepare_rgb_150_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c300600_prepare_rgb_150_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_FALSE, SW_P3P6));
  RIE (usb_low_set_soft_resample (chip, 1));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_c300600_prepare_rgb_150_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c300600_prepare_rgb_100_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c300600_prepare_rgb_100_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_FALSE, SW_P2P6));
  RIE (usb_low_set_soft_resample (chip, 1));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_c300600_prepare_rgb_100_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c300600_prepare_rgb_50_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c300600_prepare_rgb_50_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_FALSE, SW_P1P6));
  RIE (usb_low_set_soft_resample (chip, 1));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_c300600_prepare_rgb_50_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c300600_prepare_mono_600_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c300600_prepare_mono_600_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_TRUE, SW_P6P6));
  RIE (usb_low_set_soft_resample (chip, 1));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_c300600_prepare_mono_600_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c300600_prepare_mono_400_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c300600_prepare_mono_400_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_TRUE, SW_P4P6));
  RIE (usb_low_set_soft_resample (chip, 1));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_c300600_prepare_mono_400_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c300600_prepare_mono_300_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c300600_prepare_mono_300_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_FALSE, SW_P6P6));
  RIE (usb_low_set_soft_resample (chip, 1));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_c300600_prepare_mono_300_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c300600_prepare_mono_200_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c300600_prepare_mono_200_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_TRUE, SW_P2P6));
  RIE (usb_low_set_soft_resample (chip, 1));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_c300600_prepare_mono_200_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c300600_prepare_mono_150_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c300600_prepare_mono_150_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_FALSE, SW_P3P6));
  RIE (usb_low_set_soft_resample (chip, 1));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_c300600_prepare_mono_150_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c300600_prepare_mono_100_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c300600_prepare_mono_100_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_FALSE, SW_P2P6));
  RIE (usb_low_set_soft_resample (chip, 1));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_c300600_prepare_mono_100_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c300600_prepare_mono_50_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c300600_prepare_mono_50_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_FALSE, SW_P1P6));
  RIE (usb_low_set_soft_resample (chip, 1));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_c300600_prepare_mono_50_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c300600_prepare_rgb (ma1017 * chip, SANE_Word dpi)
{
  DBG (6, "usb_mid_c300600_prepare_rgb: start\n");
  switch (dpi)
    {
    case 50:
      return usb_mid_c300600_prepare_rgb_50_dpi (chip);
      break;
    case 100:
      return usb_mid_c300600_prepare_rgb_100_dpi (chip);
      break;
    case 150:
      return usb_mid_c300600_prepare_rgb_150_dpi (chip);
      break;
    case 200:
      return usb_mid_c300600_prepare_rgb_200_dpi (chip);
      break;
    case 300:
      return usb_mid_c300600_prepare_rgb_300_dpi (chip);
      break;
    case 400:
      return usb_mid_c300600_prepare_rgb_400_dpi (chip);
      break;
    case 600:
      return usb_mid_c300600_prepare_rgb_600_dpi (chip);
      break;
    default:
      DBG (3, "usb_mid_c300600_prepare_rgb: unmatched dpi: %d\n", dpi);
      return SANE_STATUS_INVAL;
      break;
    }
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c300600_prepare_mono (ma1017 * chip, SANE_Word dpi)
{
  switch (dpi)
    {
    case 50:
      return usb_mid_c300600_prepare_mono_50_dpi (chip);
      break;
    case 100:
      return usb_mid_c300600_prepare_mono_100_dpi (chip);
      break;
    case 150:
      return usb_mid_c300600_prepare_mono_150_dpi (chip);
      break;
    case 200:
      return usb_mid_c300600_prepare_mono_200_dpi (chip);
      break;
    case 300:
      return usb_mid_c300600_prepare_mono_300_dpi (chip);
      break;
    case 400:
      return usb_mid_c300600_prepare_mono_400_dpi (chip);
      break;
    case 600:
      return usb_mid_c300600_prepare_mono_600_dpi (chip);
      break;
    default:
      DBG (3, "usb_mid_c300600_prepare_mono: unmatched dpi: %d\n", dpi);
      return SANE_STATUS_INVAL;
      break;
    }
  return SANE_STATUS_GOOD;
}

/* ---------------------- sensor 300 CIS functions ----------------------- */

static SANE_Word usb_mid_c300_optical_x_dpi[] = { 300, 200, 150, 100, 50, 0 };

SANE_Status
usb_mid_c300_prepare_rgb_300_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c300_prepare_rgb_300_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_TRUE, SW_P6P6));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_c300_prepare_rgb_300_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c300_prepare_rgb_200_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c300_prepare_rgb_200_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_TRUE, SW_P4P6));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_c300_prepare_rgb_200_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c300_prepare_rgb_150_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c300_prepare_rgb_150_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_TRUE, SW_P3P6));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_c300_prepare_rgb_150_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c300_prepare_rgb_100_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c300_prepare_rgb_100_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_TRUE, SW_P2P6));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_c300_prepare_rgb_100_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c300_prepare_rgb_50_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c300_prepare_rgb_50_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_TRUE, SW_P1P6));
  RIE (usb_low_set_led_light_all (chip, SANE_FALSE));
  DBG (6, "usb_mid_c300_prepare_rgb_50_dpi: start\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c300_prepare_mono_300_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c300_prepare_mono_300_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_TRUE, SW_P6P6));
  RIE (usb_low_set_led_light_all (chip, SANE_TRUE));
  DBG (6, "usb_mid_c300_prepare_mono_300_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c300_prepare_mono_200_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c300_prepare_mono_200_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_TRUE, SW_P4P6));
  RIE (usb_low_set_led_light_all (chip, SANE_TRUE));
  DBG (6, "usb_mid_c300_prepare_mono_200_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c300_prepare_mono_150_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c300_prepare_mono_150_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_TRUE, SW_P3P6));
  RIE (usb_low_set_led_light_all (chip, SANE_TRUE));
  DBG (6, "usb_mid_c300_prepare_mono_150_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c300_prepare_mono_100_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c300_prepare_mono_100_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_TRUE, SW_P2P6));
  RIE (usb_low_set_led_light_all (chip, SANE_TRUE));
  DBG (6, "usb_mid_c300_prepare_mono_100_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c300_prepare_mono_50_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_c300_prepare_mono_50_dpi: start\n");
  RIE (usb_low_set_image_dpi (chip, SANE_TRUE, SW_P1P6));
  RIE (usb_low_set_led_light_all (chip, SANE_TRUE));
  DBG (6, "usb_mid_c300_prepare_mono_50_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c300_prepare_rgb (ma1017 * chip, SANE_Word dpi)
{
  DBG (6, "usb_mid_c300_prepare_rgb: start\n");
  switch (dpi)
    {
    case 50:
      return usb_mid_c300_prepare_rgb_50_dpi (chip);
      break;
    case 100:
      return usb_mid_c300_prepare_rgb_100_dpi (chip);
      break;
    case 150:
      return usb_mid_c300_prepare_rgb_150_dpi (chip);
      break;
    case 200:
      return usb_mid_c300_prepare_rgb_200_dpi (chip);
      break;
    case 300:
      return usb_mid_c300_prepare_rgb_300_dpi (chip);
      break;
    default:
      DBG (3, "usb_mid_c300_prepare_rgb: unmatched dpi: %d\n", dpi);
      return SANE_STATUS_INVAL;
      break;
    }
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_c300_prepare_mono (ma1017 * chip, SANE_Word dpi)
{
  DBG (6, "usb_mid_c300_prepare_mono: start\n");
  switch (dpi)
    {
    case 50:
      return usb_mid_c300_prepare_mono_50_dpi (chip);
      break;
    case 100:
      return usb_mid_c300_prepare_mono_100_dpi (chip);
      break;
    case 150:
      return usb_mid_c300_prepare_mono_150_dpi (chip);
      break;
    case 200:
      return usb_mid_c300_prepare_mono_200_dpi (chip);
      break;
    case 300:
      return usb_mid_c300_prepare_mono_300_dpi (chip);
      break;
    default:
      DBG (3, "usb_mid_c300_prepare_mono: unmatched dpi: %d\n", dpi);
      return SANE_STATUS_INVAL;
      break;
    }
  return SANE_STATUS_GOOD;
}

/* -------------------------- sensor functions ---------------------------- */

SANE_Bool
usb_mid_sensor_is600_mode (ma1017 * chip, SANE_Word dpi)
{
  if (chip->sensor == ST_CANON300)
    {
      DBG (6, "usb_mid_sensor_is600_mode: chip=%p, dpi=%d, FALSE\n",
	   (void *) chip, dpi);
      return SANE_FALSE;
    }
  else if ((chip->sensor == ST_CANON600) || (chip->sensor == ST_NEC600))
    {
      DBG (6, "usb_mid_sensor_is600_mode: chip=%p, dpi=%d, TRUE\n",
	   (void *) chip, dpi);
      return SANE_TRUE;
    }
  else
    {
      switch (dpi)
	{
	case 300:
	case 150:
	case 100:
	case 50:
	  DBG (6, "usb_mid_sensor_is600_mode: chip=%p, dpi=%d, FALSE\n",
	       (void *) chip, dpi);
	  return SANE_FALSE;
	case 600:
	case 400:
	case 200:
	  DBG (6, "usb_mid_sensor_is600_mode: chip=%p, dpi=%d, TRUE\n",
	       (void *) chip, dpi);
	  return SANE_TRUE;
	default:
	  DBG (3, "usb_mid_sensor_is600_mode: unmatched dpi: %d\n", dpi);
	  return SANE_FALSE;
	  break;
	}

    }

  return SANE_FALSE;
}

static SANE_Status
usb_mid_sensor_prepare_rgb (ma1017 * chip, SANE_Word dpi)
{
  if (chip->sensor == ST_CANON300)
    return usb_mid_c300_prepare_rgb (chip, dpi);
  else if (chip->sensor == ST_CANON600)
    return usb_mid_c600_prepare_rgb (chip, dpi);
  else if (chip->sensor == ST_NEC600)
    return usb_mid_n600_prepare_rgb (chip, dpi);
  else
    return usb_mid_c300600_prepare_rgb (chip, dpi);

  return SANE_STATUS_INVAL;
}

static SANE_Status
usb_mid_sensor_prepare_mono (ma1017 * chip, SANE_Word dpi)
{
  if (chip->sensor == ST_CANON300)
    return usb_mid_c300_prepare_mono (chip, dpi);
  else if (chip->sensor == ST_CANON600)
    return usb_mid_c600_prepare_mono (chip, dpi);
  else if (chip->sensor == ST_NEC600)
    return usb_mid_n600_prepare_mono (chip, dpi);
  else
    return usb_mid_c300600_prepare_mono (chip, dpi);

  return SANE_STATUS_INVAL;
}

static SANE_Status
usb_mid_sensor_get_dpi (ma1017 * chip, SANE_Word wanted_dpi, SANE_Word * dpi)
{
  SANE_Word *dpi_list;
  SANE_Word i;

  if (!dpi)
    return SANE_STATUS_INVAL;

  DBG (5, "usb_mid_sensor_get_dpi: chip->sensor=%d\n", chip->sensor);

  if (chip->sensor == ST_CANON300)
    dpi_list = usb_mid_c300_optical_x_dpi;
  else if (chip->sensor == ST_CANON300600)
    dpi_list = usb_mid_c300600_optical_x_dpi;
  else if (chip->sensor == ST_CANON600)
    dpi_list = usb_mid_c600_optical_x_dpi;
  else if (chip->sensor == ST_NEC600)
    dpi_list = usb_mid_n600_optical_x_dpi;
  else
    return SANE_STATUS_INVAL;

  for (i = 0; dpi_list[i] != 0; i++)
    {
      if (wanted_dpi > dpi_list[i])
	break;
    }
  if (i)
    i--;
  *dpi = dpi_list[i];
  DBG (5, "usb_mid_sensor_get_dpi: wanted %d dpi, got %d dpi\n", wanted_dpi,
       *dpi);
  return SANE_STATUS_GOOD;
}


/* ---------------1200 dpi motor function declarations --------------------- */

static SANE_Word usb_mid_motor1200_optical_dpi[] =
  { 1200, 600, 400, 300, 200, 150, 100, 50, 0 };

SANE_Status
usb_mid_motor1200_prepare_rgb_1200_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor1200_prepare_rgb_1200_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_BLUE, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_RED, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 3, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 4, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 4));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_FALSE, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_io_3 (chip, SANE_FALSE));
  DBG (6, "usb_mid_motor1200_prepare_rgb_1200_dpi: exit\n");
  return SANE_STATUS_GOOD;
}


SANE_Status
usb_mid_motor1200_prepare_rgb_600_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor1200_prepare_rgb_600_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_BLUE, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_RED, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 3, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 4, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 4));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_TRUE, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_io_3 (chip, SANE_FALSE));
  DBG (6, "usb_mid_motor1200_prepare_rgb_600_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor1200_prepare_rgb_400_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor1200_prepare_rgb_400_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_BLUE, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_RED, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 3, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 3));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_FALSE, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));
  DBG (6, "usb_mid_motor1200_prepare_rgb_400_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor1200_prepare_rgb_300_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor1200_prepare_rgb_300_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_BLUE, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_RED, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 3, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 4, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 4));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_FALSE, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));
  DBG (6, "usb_mid_motor1200_prepare_rgb_300_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor1200_prepare_rgb_200_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor1200_prepare_rgb_200_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_BLUE, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_RED, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 3, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 3));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_TRUE, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));
  DBG (6, "usb_mid_motor1200_prepare_rgb_200_dpi: exit\n");
  return SANE_STATUS_GOOD;
}



SANE_Status
usb_mid_motor1200_prepare_rgb_150_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor1200_prepare_rgb_150_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_BLUE, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_RED, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 3, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 4, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 4));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_TRUE, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));
  DBG (6, "usb_mid_motor1200_prepare_rgb_150_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor1200_prepare_rgb_100_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor1200_prepare_rgb_100_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_BLUE, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_RED, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 3, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 3));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_TRUE, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));
  DBG (6, "usb_mid_motor1200_prepare_rgb_100_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor1200_prepare_rgb_50_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor1200_prepare_rgb_50_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_BLUE, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 3, CH_BLUE, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 4, CH_RED, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 5, CH_RED, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 6, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 6));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_TRUE, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));
  DBG (6, "usb_mid_motor1200_prepare_rgb_50_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor1200_prepare_mono_1200_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor1200_prepare_mono_1200_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 2));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_FALSE, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));
  DBG (6, "usb_mid_motor1200_prepare_mono_1200_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor1200_prepare_mono_600_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor1200_prepare_mono_600_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 3, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 4, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 5, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 6, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 7, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 8, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 9, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 10, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 11, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 12, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 13, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 14, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 15, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 16, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 17, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 18, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 19, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 20, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 21, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 22, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 23, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 24, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 25, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 26, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 26));
  RIE (usb_low_set_cmt_second_position (chip, 24));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_FALSE, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));
  DBG (6, "usb_mid_motor1200_prepare_mono_600_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor1200_prepare_mono_400_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor1200_prepare_mono_400_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 3, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 3));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_FALSE, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));
  DBG (6, "usb_mid_motor1200_prepare_mono_400_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor1200_prepare_mono_300_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor1200_prepare_mono_300_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 3, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 4, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 5, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 6, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 7, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 8, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 9, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 10, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 11, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 12, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 13, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 14, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 15, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 16, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 17, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 18, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 19, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 20, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 21, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 22, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 23, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 24, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 25, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 26, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 26));
  RIE (usb_low_set_cmt_second_position (chip, 24));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_TRUE, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));
  DBG (6, "usb_mid_motor1200_prepare_mono_300_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor1200_prepare_mono_200_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor1200_prepare_mono_200_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 3, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 3));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_TRUE, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));
  DBG (6, "usb_mid_motor1200_prepare_mono_200_dpi: exit\n");
  return SANE_STATUS_GOOD;
}


SANE_Status
usb_mid_motor1200_prepare_mono_150_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor1200_prepare_mono_150_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 2));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_TRUE, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));
  DBG (6, "usb_mid_motor1200_prepare_mono_150_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor1200_prepare_mono_100_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor1200_prepare_mono_100_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 3, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 3));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_TRUE, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));
  DBG (6, "usb_mid_motor1200_prepare_mono_100_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor1200_prepare_mono_50_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor1200_prepare_mono_50_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 3, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 4, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 5, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 6, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 6));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_TRUE, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));
  DBG (6, "usb_mid_motor1200_prepare_mono_50_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor1200_prepare_rgb_half_300_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor1200_prepare_rgb_half_300_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_GREEN, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_BLUE, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 3, CH_BLUE, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 4, CH_RED, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 5, CH_RED, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 6, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 6));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_FALSE, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));
  DBG (6, "usb_mid_motor1200_prepare_rgb_half_300_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor1200_prepare_rgb_bi_full_300_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor1200_prepare_rgb_bi_full_300_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_BLUE, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 3, CH_BLUE, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 4, CH_RED, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 5, CH_RED, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 6, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 6));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_TRUE, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_io_3 (chip, SANE_FALSE));
  DBG (6, "usb_mid_motor1200_prepare_rgb_bi_full_300_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor1200_prepare_rgb_bi_full_x2300_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor1200_prepare_rgb_bi_full_x2300_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_GREEN, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_BLUE, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 3, CH_BLUE, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 4, CH_RED, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 5, CH_RED, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 6, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 6));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_TRUE, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));
  DBG (6, "usb_mid_motor1200_prepare_rgb_bi_full_x2300_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor1200_prepare_mono_half_300_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor1200_prepare_mono_half_300_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 3, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 4, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 4));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_FALSE, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));
  DBG (6, "usb_mid_motor1200_prepare_mono_half_300_dpi: exit\n");
  return SANE_STATUS_GOOD;

}

SANE_Status
usb_mid_motor1200_prepare_mono_bi_full_300_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor1200_prepare_mono_bi_full_300_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 2));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_TRUE, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));
  DBG (6, "usb_mid_motor1200_prepare_mono_bi_full_300_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor1200_prepare_mono_bi_full_x2300_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor1200_prepare_mono_bi_full_x2300_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 2));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_TRUE, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));
  DBG (6, "usb_mid_motor1200_prepare_mono_bi_full_x2300_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor1200_prepare_rgb (ma1017 * chip, SANE_Word dpi)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor1200_prepare_rgb: start\n");
  RIE (usb_low_move_motor_home (chip, SANE_FALSE, SANE_FALSE));
  /* No Motor & Forward */
  RIE (usb_low_set_motor_direction (chip, SANE_FALSE));
  RIE (usb_low_enable_motor (chip, SANE_TRUE));
  switch (dpi)
    {
    case 1200:
      return usb_mid_motor1200_prepare_rgb_1200_dpi (chip);
      break;
    case 600:
      return usb_mid_motor1200_prepare_rgb_600_dpi (chip);
      break;
    case 400:
      return usb_mid_motor1200_prepare_rgb_400_dpi (chip);
      break;
    case 300:
      return usb_mid_motor1200_prepare_rgb_300_dpi (chip);
      break;
    case 200:
      return usb_mid_motor1200_prepare_rgb_200_dpi (chip);
      break;
    case 150:
      return usb_mid_motor1200_prepare_rgb_150_dpi (chip);
      break;
    case 100:
      return usb_mid_motor1200_prepare_rgb_100_dpi (chip);
      break;
    case 50:
      return usb_mid_motor1200_prepare_rgb_50_dpi (chip);
      break;
    default:
      DBG (3, "usb_mid_motor1200_prepare_rgb: unmatched dpi: %d\n", dpi);
      return SANE_STATUS_INVAL;
      break;
    }
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor1200_prepare_mono (ma1017 * chip, SANE_Word dpi)
{
  SANE_Status status;

  DBG (3, "usb_mid_motor1200_prepare_mono: start\n");
  RIE (usb_low_move_motor_home (chip, SANE_FALSE, SANE_FALSE));
  /* No Motor & Forward */
  RIE (usb_low_set_motor_direction (chip, SANE_FALSE));
  RIE (usb_low_enable_motor (chip, SANE_TRUE));
  switch (dpi)
    {
    case 1200:
      return usb_mid_motor1200_prepare_mono_1200_dpi (chip);
      break;
    case 600:
      return usb_mid_motor1200_prepare_mono_600_dpi (chip);
      break;
    case 400:
      return usb_mid_motor1200_prepare_mono_400_dpi (chip);
      break;
    case 300:
      return usb_mid_motor1200_prepare_mono_300_dpi (chip);
      break;
    case 200:
      return usb_mid_motor1200_prepare_mono_200_dpi (chip);
      break;
    case 150:
      return usb_mid_motor1200_prepare_mono_150_dpi (chip);
      break;
    case 100:
      return usb_mid_motor1200_prepare_mono_100_dpi (chip);
      break;
    case 50:
      return usb_mid_motor1200_prepare_mono_50_dpi (chip);
      break;
    default:
      DBG (3, "usb_mid_motor1200_prepare_mono_: unmatched dpi: %d\n", dpi);
      return SANE_STATUS_INVAL;
      break;
    }
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor1200_prepare_calibrate_rgb (ma1017 * chip, SANE_Word dpi)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor1200_prepare_calibrate_rgb: start\n");
  RIE (usb_low_move_motor_home (chip, SANE_FALSE, SANE_FALSE));
  /* No Motor & Forward */
  RIE (usb_low_set_motor_direction (chip, SANE_FALSE));
  RIE (usb_low_enable_motor (chip, SANE_TRUE));
  switch (dpi)
    {
    case 1200:
    case 400:
    case 300:
      return usb_mid_motor1200_prepare_rgb_half_300_dpi (chip);
      break;
    case 600:
    case 200:
    case 150:
      return usb_mid_motor1200_prepare_rgb_bi_full_300_dpi (chip);
      break;
    case 100:
    case 50:
      return usb_mid_motor1200_prepare_rgb_bi_full_x2300_dpi (chip);
      break;
    default:
      DBG (3, "usb_mid_motor1200_prepare_calibrate_rgb: unmatched dpi: "
	   "%d\n", dpi);
      return SANE_STATUS_INVAL;
      break;
    }
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor1200_prepare_calibrate_mono (ma1017 * chip, SANE_Word dpi)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor1200_prepare_calibrate_mono: start\n");
  RIE (usb_low_move_motor_home (chip, SANE_FALSE, SANE_FALSE));
  /* No Motor & Forward */
  RIE (usb_low_set_motor_direction (chip, SANE_FALSE));
  RIE (usb_low_enable_motor (chip, SANE_TRUE));
  switch (dpi)
    {
    case 1200:
    case 600:
    case 400:
      return usb_mid_motor1200_prepare_mono_half_300_dpi (chip);
      break;
    case 300:
    case 200:
      return usb_mid_motor1200_prepare_mono_bi_full_300_dpi (chip);
      break;
    case 150:
    case 100:
    case 50:
      return usb_mid_motor1200_prepare_mono_bi_full_x2300_dpi (chip);
      break;
    default:
      DBG (3, "usb_mid_motor1200_prepare_calibrate_mono: unmatched dpi: %d\n",
	   dpi);
      return SANE_STATUS_INVAL;
      break;
    }
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor1200_prepare_step (ma1017 * chip, SANE_Word step_count)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor1200_prepare_step: start\n");
  RIE (usb_low_set_motor_movement (chip, SANE_TRUE, SANE_TRUE, SANE_FALSE));
  /* Make it in 600dpi */
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));	/* (IO3) ? High power : Low power */
  RIE (usb_low_move_motor_home (chip, SANE_FALSE, SANE_FALSE));
  /* No Motor & Forward */
  if (step_count == 1)
    {
      RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_TRUE, SANE_FALSE));
      RIE (usb_low_set_cmt_table (chip, 1, CH_GREEN, SANE_FALSE, SANE_FALSE));
      RIE (usb_low_set_cmt_table_length (chip, 1));
      RIE (usb_low_set_cmt_second_position (chip, 0));
      RIE (usb_low_set_cmt_loop_count (chip, step_count));
    }
  else if (step_count % 2 == 1)
    {
      RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_TRUE, SANE_FALSE));
      RIE (usb_low_set_cmt_table (chip, 1, CH_GREEN, SANE_TRUE, SANE_FALSE));
      RIE (usb_low_set_cmt_table (chip, 2, CH_GREEN, SANE_TRUE, SANE_FALSE));
      RIE (usb_low_set_cmt_table (chip, 3, CH_GREEN, SANE_FALSE, SANE_FALSE));
      RIE (usb_low_set_cmt_table_length (chip, 3));
      RIE (usb_low_set_cmt_second_position (chip, 1));
      RIE (usb_low_set_cmt_loop_count (chip, (step_count - 1) / 2));
    }
  else
    {
      RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_TRUE, SANE_FALSE));
      RIE (usb_low_set_cmt_table (chip, 1, CH_GREEN, SANE_TRUE, SANE_FALSE));
      RIE (usb_low_set_cmt_table (chip, 2, CH_GREEN, SANE_FALSE, SANE_FALSE));
      RIE (usb_low_set_cmt_table_length (chip, 2));
      RIE (usb_low_set_cmt_second_position (chip, 0));
      RIE (usb_low_set_cmt_loop_count (chip, step_count / 2));
    }
  RIE (usb_low_enable_motor (chip, SANE_TRUE));
  DBG (6, "usb_mid_motor1200_prepare_step: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor1200_prepare_home (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor1200_prepare_home: start\n");
  if (chip->sensor == ST_NEC600)
    RIE (usb_low_set_motor_movement
	 (chip, SANE_FALSE, SANE_TRUE, SANE_FALSE));
  else
    RIE (usb_low_set_motor_movement (chip, SANE_TRUE, SANE_TRUE, SANE_FALSE));
  /* Make it in 600dpi */
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));	/* (IO3) ? High power : Low power */
  RIE (usb_low_move_motor_home (chip, SANE_TRUE, SANE_TRUE));
  DBG (6, "usb_mid_motor1200_prepare_home: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor1200_prepare_adjust (ma1017 * chip, Channel channel)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor1200_prepare_adjust: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, channel, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, channel, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 2, channel, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 2));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  DBG (6, "usb_mid_motor1200_prepare_adjust: exit\n");
  return SANE_STATUS_GOOD;
}


SANE_Word
usb_mid_motor1200_rgb_capability (SANE_Word dpi)
{
  DBG (6, "usb_mid_motor1200_rgb_capability: start\n");
  switch (dpi)
    {
    case 1200:
    case 400:
      return 3008; /* 2816 */ ;
    case 600:
      return 3008;
    case 200:
      return 5056;
    case 300:
      return 3008;
    case 150:
      return 5056;
    case 100:
    case 50:
      return 10048;
    default:
      DBG (3, "usb_mid_motor1200_rgb_capability: unmatched dpi: %d\n", dpi);
      return 0;
    }
}

SANE_Word
usb_mid_motor1200_mono_capability (SANE_Word dpi)
{
  DBG (5, "usb_mid_motor1200_mono_capability: start\n");
  switch (dpi)
    {
    case 1200:
    case 400:
      return 3008;
    case 600:
      return 3008;
    case 200:
      return 5056;
    case 300:
      return 5056;
    case 150:
    case 100:
    case 50:
      return 10048;
    default:
      DBG (3, "usb_mid_motor1200_mono_capability: unmatched dpi: %d\n", dpi);
      return 0;
    }
}

/* ---------------600 dpi motor function declarations --------------------- */


static SANE_Word usb_mid_motor600_optical_dpi[] =
  { 600, 300, 200, 150, 100, 50, 0 };


SANE_Status
usb_mid_motor600_prepare_rgb_600_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor600_prepare_rgb_600_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_BLUE, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_RED, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 3, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 4, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 4));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_FALSE, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_io_3 (chip, SANE_FALSE));
  DBG (6, "usb_mid_motor600_prepare_rgb_600_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor600_prepare_rgb_300_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor600_prepare_rgb_300_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_BLUE, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_RED, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 3, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 4, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 4));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_TRUE, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_io_3 (chip, SANE_FALSE));
  DBG (6, "usb_mid_motor600_prepare_rgb_300_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor600_prepare_rgb_200_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor600_prepare_rgb_200_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_BLUE, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_RED, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 3, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 4, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 5, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 5));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_FALSE, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));
  DBG (6, "usb_mid_motor600_prepare_rgb_200_dpi: exit\n");
  return SANE_STATUS_GOOD;
}



SANE_Status
usb_mid_motor600_prepare_rgb_150_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor600_prepare_rgb_150_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_BLUE, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_RED, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 3, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 3));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_TRUE, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));
  DBG (6, "usb_mid_motor600_prepare_rgb_150_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor600_prepare_rgb_100_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor600_prepare_rgb_100_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_BLUE, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_RED, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 3, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 4, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 5, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 5));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_TRUE, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));
  DBG (6, "usb_mid_motor600_prepare_rgb_100_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor600_prepare_rgb_50_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor600_prepare_rgb_50_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_BLUE, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_RED, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 3, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 3));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_TRUE, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));
  DBG (6, "usb_mid_motor600_prepare_rgb_50_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor600_prepare_mono_600_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor600_prepare_mono_600_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 2));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_FALSE, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));
  DBG (6, "usb_mid_motor600_prepare_mono_600_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor600_prepare_mono_300_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor600_prepare_mono_300_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 2));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_TRUE, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));
  DBG (6, "usb_mid_motor600_prepare_mono_300_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor600_prepare_mono_200_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor600_prepare_mono_200_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 3, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 3));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_FALSE, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));
  DBG (6, "usb_mid_motor600_prepare_mono_200_dpi: exit\n");
  return SANE_STATUS_GOOD;
}


SANE_Status
usb_mid_motor600_prepare_mono_150_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor600_prepare_mono_150_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 2));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_TRUE, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));
  DBG (6, "usb_mid_motor600_prepare_mono_150_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor600_prepare_mono_100_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor600_prepare_mono_100_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 3, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 3));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_TRUE, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));
  DBG (6, "usb_mid_motor600_prepare_mono_100_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor600_prepare_mono_50_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor600_prepare_mono_50_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 3, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 3));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_TRUE, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));
  DBG (6, "usb_mid_motor600_prepare_mono_50_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor600_prepare_rgb_half_300_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor600_prepare_rgb_half_300_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_GREEN, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_BLUE, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 3, CH_BLUE, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 4, CH_RED, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 5, CH_RED, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 6, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 6));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_FALSE, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));
  DBG (6, "usb_mid_motor600_prepare_rgb_half_300_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor600_prepare_rgb_bi_full_300_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor600_prepare_rgb_bi_full_300_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_BLUE, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 3, CH_BLUE, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 4, CH_RED, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 5, CH_RED, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 6, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 6));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_TRUE, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_io_3 (chip, SANE_FALSE));
  DBG (6, "usb_mid_motor600_prepare_rgb_bi_full_300_dpi: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor600_prepare_mono_half_300_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor600_prepare_mono_half_300_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_GREEN, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 2));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_FALSE, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));
  DBG (6, "usb_mid_motor600_prepare_mono_half_300_dpi: exit\n");
  return SANE_STATUS_GOOD;

}

SANE_Status
usb_mid_motor600_prepare_mono_bi_full_300_dpi (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor600_prepare_mono_bi_full_300_dpi: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, CH_GREEN, SANE_TRUE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 2, CH_GREEN, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 2));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  RIE (usb_low_set_motor_movement (chip, SANE_TRUE, SANE_TRUE, SANE_FALSE));
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));
  DBG (6, "usb_mid_motor600_prepare_mono_bi_full_300_dpi: exit\n");
  return SANE_STATUS_GOOD;
}


SANE_Status
usb_mid_motor600_prepare_rgb (ma1017 * chip, SANE_Word dpi)
{
  DBG (6, "usb_mid_motor600_prepare_rgb: start\n");
  switch (dpi)
    {
    case 600:
      return usb_mid_motor600_prepare_rgb_600_dpi (chip);
      break;
    case 300:
      return usb_mid_motor600_prepare_rgb_300_dpi (chip);
      break;
    case 200:
      return usb_mid_motor600_prepare_rgb_200_dpi (chip);
      break;
    case 150:
      return usb_mid_motor600_prepare_rgb_150_dpi (chip);
      break;
    case 100:
      return usb_mid_motor600_prepare_rgb_100_dpi (chip);
      break;
    case 50:
      return usb_mid_motor600_prepare_rgb_50_dpi (chip);
      break;
    default:
      DBG (3, "usb_mid_motor600_prepare_rgb: unmatched dpi: %d\n", dpi);
      return SANE_STATUS_INVAL;
      break;
    }
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor600_prepare_mono (ma1017 * chip, SANE_Word dpi)
{
  DBG (6, "usb_mid_motor600_prepare_mono: start\n");
  switch (dpi)
    {
    case 600:
      return usb_mid_motor600_prepare_mono_600_dpi (chip);
      break;
    case 300:
      return usb_mid_motor600_prepare_mono_300_dpi (chip);
      break;
    case 200:
      return usb_mid_motor600_prepare_mono_200_dpi (chip);
      break;
    case 150:
      return usb_mid_motor600_prepare_mono_150_dpi (chip);
      break;
    case 100:
      return usb_mid_motor600_prepare_mono_100_dpi (chip);
      break;
    case 50:
      return usb_mid_motor600_prepare_mono_50_dpi (chip);
      break;
    default:
      DBG (3, "usb_mid_motor600_prepare_mono_: unmatched dpi: %d\n", dpi);
      return SANE_STATUS_INVAL;
      break;
    }
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor600_prepare_calibrate_rgb (ma1017 * chip, SANE_Word dpi)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor600_prepare_calibrate_rgb: start\n");
  RIE (usb_low_move_motor_home (chip, SANE_FALSE, SANE_FALSE));
  /* No Motor & Forward */
  RIE (usb_low_set_motor_direction (chip, SANE_FALSE));
  RIE (usb_low_enable_motor (chip, SANE_TRUE));
  switch (dpi)
    {
    case 600:
    case 200:
      return usb_mid_motor600_prepare_rgb_half_300_dpi (chip);
      break;
    case 300:
    case 150:
    case 100:
    case 50:
      return usb_mid_motor600_prepare_rgb_bi_full_300_dpi (chip);
      break;
    default:
      DBG (3, "usb_mid_motor600_prepare_calibrate_rgb: unmatched dpi: %d\n",
	   dpi);
      return SANE_STATUS_INVAL;
      break;
    }
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor600_prepare_calibrate_mono (ma1017 * chip, SANE_Word dpi)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor600_prepare_calibrate_mono: start\n");
  RIE (usb_low_move_motor_home (chip, SANE_FALSE, SANE_FALSE));
  /* No Motor & Forward */
  RIE (usb_low_set_motor_direction (chip, SANE_FALSE));
  RIE (usb_low_enable_motor (chip, SANE_TRUE));
  switch (dpi)
    {
    case 600:
    case 200:
      return usb_mid_motor600_prepare_mono_half_300_dpi (chip);
      break;
    case 300:
    case 150:
    case 100:
    case 50:
      return usb_mid_motor600_prepare_mono_bi_full_300_dpi (chip);
      break;
    default:
      DBG (3, "usb_mid_motor600_prepare_calibrate_mono: unmatched dpi: %d\n",
	   dpi);
      return SANE_STATUS_INVAL;
      break;
    }
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor600_prepare_step (ma1017 * chip, SANE_Word step_count)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor600_prepare_step: start\n");
  RIE (usb_low_set_motor_movement (chip, SANE_FALSE, SANE_TRUE, SANE_FALSE));
  /* Make it in 300dpi */
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));	/* (IO3) ? High power : Low power */
  RIE (usb_low_move_motor_home (chip, SANE_FALSE, SANE_FALSE));
  /* No Motor & Forward */
  if (step_count == 1)
    {
      RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_TRUE, SANE_FALSE));
      RIE (usb_low_set_cmt_table (chip, 1, CH_GREEN, SANE_FALSE, SANE_FALSE));
      RIE (usb_low_set_cmt_table_length (chip, 1));
      RIE (usb_low_set_cmt_second_position (chip, 0));
      RIE (usb_low_set_cmt_loop_count (chip, step_count));
    }
  else if (step_count % 2 == 1)
    {
      RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_TRUE, SANE_FALSE));
      RIE (usb_low_set_cmt_table (chip, 1, CH_GREEN, SANE_TRUE, SANE_FALSE));
      RIE (usb_low_set_cmt_table (chip, 2, CH_GREEN, SANE_TRUE, SANE_FALSE));
      RIE (usb_low_set_cmt_table (chip, 3, CH_GREEN, SANE_FALSE, SANE_FALSE));
      RIE (usb_low_set_cmt_table_length (chip, 3));
      RIE (usb_low_set_cmt_second_position (chip, 1));
      RIE (usb_low_set_cmt_loop_count (chip, (step_count - 1) / 2));
    }
  else
    {
      RIE (usb_low_set_cmt_table (chip, 0, CH_GREEN, SANE_TRUE, SANE_FALSE));
      RIE (usb_low_set_cmt_table (chip, 1, CH_GREEN, SANE_TRUE, SANE_FALSE));
      RIE (usb_low_set_cmt_table (chip, 2, CH_GREEN, SANE_FALSE, SANE_FALSE));
      RIE (usb_low_set_cmt_table_length (chip, 2));
      RIE (usb_low_set_cmt_second_position (chip, 0));
      RIE (usb_low_set_cmt_loop_count (chip, step_count / 2));
    }
  RIE (usb_low_enable_motor (chip, SANE_TRUE));
  DBG (6, "usb_mid_motor600_prepare_step: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor600_prepare_home (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor600_prepare_home: start\n");
  RIE (usb_low_set_motor_movement (chip, SANE_FALSE, SANE_TRUE, SANE_TRUE));
  /* Make it in 600dpi */
  RIE (usb_low_set_io_3 (chip, SANE_TRUE));	/* (IO3) ? High power : Low power */
  RIE (usb_low_move_motor_home (chip, SANE_TRUE, SANE_TRUE));
  DBG (6, "usb_mid_motor600_prepare_home: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_motor600_prepare_adjust (ma1017 * chip, Channel channel)
{
  SANE_Status status;

  DBG (6, "usb_mid_motor600_prepare_adjust: start\n");
  RIE (usb_low_set_cmt_table (chip, 0, channel, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 1, channel, SANE_FALSE, SANE_TRUE));
  RIE (usb_low_set_cmt_table (chip, 2, channel, SANE_FALSE, SANE_FALSE));
  RIE (usb_low_set_cmt_table_length (chip, 2));
  RIE (usb_low_set_cmt_second_position (chip, 0));
  RIE (usb_low_set_cmt_loop_count (chip, 0xefff));
  DBG (6, "usb_mid_motor600_prepare_adjust: exit\n");
  return SANE_STATUS_GOOD;
}


SANE_Word
usb_mid_motor600_rgb_capability (SANE_Word dpi)
{
  DBG (6, "usb_mid_motor600_rgb_capability: start\n");
  switch (dpi)
    {
    case 600:
    case 300:
    case 200:
      return 2600;
    case 100:
      return 4500;
    case 150:
    case 50:
      return 9000;
    default:
      DBG (3, "usb_mid_motor600_rgb_capability: unmatched dpi: %d\n", dpi);
      return 0;
    }
}

SANE_Word
usb_mid_motor600_mono_capability (SANE_Word dpi)
{
  DBG (5, "usb_mid_motor600_mono_capability: start\n");
  switch (dpi)
    {
    case 600:
    case 200:
      return 2600;
    case 300:
    case 100:
      return 4500;
    case 150:
    case 50:
      return 9000;
    default:
      DBG (3, "usb_mid_motor600_mono_capability: unmatched dpi: %d\n", dpi);
      return 0;
    }
}

/* ------------------ motor function declarations ------------------------ */
static SANE_Status
usb_mid_motor_prepare_home (ma1017 * chip)
{
  if (chip->motor == MT_600)
    return usb_mid_motor600_prepare_home (chip);
  else
    return usb_mid_motor1200_prepare_home (chip);
}

static SANE_Status
usb_mid_motor_prepare_rgb (ma1017 * chip, SANE_Word dpi)
{
  if (chip->motor == MT_600)
    return usb_mid_motor600_prepare_rgb (chip, dpi);
  else
    return usb_mid_motor1200_prepare_rgb (chip, dpi);
}

static SANE_Status
usb_mid_motor_prepare_mono (ma1017 * chip, SANE_Word dpi)
{
  if (chip->motor == MT_600)
    return usb_mid_motor600_prepare_mono (chip, dpi);
  else
    return usb_mid_motor1200_prepare_mono (chip, dpi);
}

static SANE_Status
usb_mid_motor_prepare_adjust (ma1017 * chip, Channel channel)
{
  if (chip->motor == MT_600)
    return usb_mid_motor600_prepare_adjust (chip, channel);
  else
    return usb_mid_motor1200_prepare_adjust (chip, channel);
}

static SANE_Status
usb_mid_motor_prepare_calibrate_rgb (ma1017 * chip, SANE_Word dpi)
{
  if (chip->motor == MT_600)
    return usb_mid_motor600_prepare_calibrate_rgb (chip, dpi);
  else
    return usb_mid_motor1200_prepare_calibrate_rgb (chip, dpi);
}

static SANE_Status
usb_mid_motor_prepare_calibrate_mono (ma1017 * chip, SANE_Word dpi)
{
  if (chip->motor == MT_600)
    return usb_mid_motor600_prepare_calibrate_mono (chip, dpi);
  else
    return usb_mid_motor1200_prepare_calibrate_mono (chip, dpi);
}

static SANE_Status
usb_mid_motor_prepare_step (ma1017 * chip, SANE_Word step_count)
{
  if (chip->motor == MT_600)
    return usb_mid_motor600_prepare_step (chip, step_count);
  else
    return usb_mid_motor1200_prepare_step (chip, step_count);
}

static SANE_Word
usb_mid_motor_rgb_capability (ma1017 * chip, SANE_Word dpi)
{
  if (chip->motor == MT_600)
    return usb_mid_motor600_rgb_capability (dpi);
  else
    return usb_mid_motor1200_rgb_capability (dpi);
}

static SANE_Word
usb_mid_motor_mono_capability (ma1017 * chip, SANE_Word dpi)
{
  if (chip->motor == MT_600)
    return usb_mid_motor600_mono_capability (dpi);
  else
    return usb_mid_motor1200_mono_capability (dpi);
}


static SANE_Status
usb_mid_motor_get_dpi (ma1017 * chip, SANE_Word wanted_dpi, SANE_Word * dpi)
{
  SANE_Word *dpi_list;
  SANE_Word i;

  if (!dpi)
    return SANE_STATUS_INVAL;

  if (chip->motor == MT_600)
    dpi_list = usb_mid_motor600_optical_dpi;
  else if (chip->motor == MT_1200)
    dpi_list = usb_mid_motor1200_optical_dpi;
  else
    return SANE_STATUS_INVAL;

  for (i = 0; dpi_list[i] != 0; i++)
    {
      if (wanted_dpi > dpi_list[i])
	break;
    }
  if (i)
    i--;
  *dpi = dpi_list[i];
  DBG (5, "usb_mid_motor_get_dpi: wanted %d dpi, got %d dpi\n", wanted_dpi,
       *dpi);
  return SANE_STATUS_GOOD;
}

/* ----------------------------- frontend ------------------------------- */

SANE_Status
usb_mid_front_set_front_end_mode (ma1017 * chip, SANE_Byte mode)
{
  SANE_Status status;

  DBG (6, "usb_mid_front_set_front_end_mode: start\n");
  RIE (usb_low_set_serial_format (chip, mode));
  DBG (6, "usb_mid_front_set_front_end_mode: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_front_enable (ma1017 * chip, SANE_Bool is_enable)
{
  SANE_Status status;

  DBG (6, "usb_mid_front_enable: start\n");
  RIE (usb_low_turn_frontend_mode (chip, is_enable));
  DBG (6, "usb_mid_front_enable: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_front_set_top_reference (ma1017 * chip, SANE_Byte top)
{
  SANE_Status status;

  DBG (6, "usb_mid_front_set_top_reference: start\n");
  RIE (usb_mid_front_enable (chip, SANE_TRUE));
  RIE (usb_low_set_serial_byte1 (chip, 0x00));
  RIE (usb_low_set_serial_byte2 (chip, top));
  RIE (usb_mid_front_enable (chip, SANE_FALSE));
  DBG (6, "usb_mid_front_set_top_reference: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_front_set_red_offset (ma1017 * chip, SANE_Byte offset)
{
  SANE_Status status;

  DBG (6, "usb_mid_front_set_red_offset: start\n");
  RIE (usb_mid_front_enable (chip, SANE_TRUE));
  RIE (usb_low_set_serial_byte1 (chip, 0x10));
  RIE (usb_low_set_serial_byte2 (chip, offset));
  RIE (usb_mid_front_enable (chip, SANE_FALSE));
  DBG (6, "usb_mid_front_set_red_offset: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_front_set_green_offset (ma1017 * chip, SANE_Byte offset)
{
  SANE_Status status;

  DBG (6, "usb_mid_front_set_green_offset: start\n");
  RIE (usb_mid_front_enable (chip, SANE_TRUE));
  RIE (usb_low_set_serial_byte1 (chip, 0x50));
  RIE (usb_low_set_serial_byte2 (chip, offset));
  RIE (usb_mid_front_enable (chip, SANE_FALSE));
  DBG (6, "usb_mid_front_set_green_offset: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_front_set_blue_offset (ma1017 * chip, SANE_Byte offset)
{
  SANE_Status status;

  DBG (6, "usb_mid_front_set_blue_offset: start\n");
  RIE (usb_mid_front_enable (chip, SANE_TRUE));
  RIE (usb_low_set_serial_byte1 (chip, 0x30));
  RIE (usb_low_set_serial_byte2 (chip, offset));
  RIE (usb_mid_front_enable (chip, SANE_FALSE));
  DBG (6, "usb_mid_front_set_blue_offset: exit\n");
  return SANE_STATUS_GOOD;
}

#if 0
/* CCD */
SANE_Word
usb_mid_frontend_max_offset_index (ma1017 * chip)
{
  DBG (6, "usb_mid_front_max_offset_index: start (chip = %p)\n", chip);

  DBG (6, "usb_mid_front_max_offset_index: exit\n");
  return (OFFSET_TABLE_SIZE - 1);
}
#endif

SANE_Status
usb_mid_front_set_red_pga (ma1017 * chip, SANE_Byte pga)
{
  SANE_Status status;

  DBG (6, "usb_mid_front_set_red_pga: start\n");
  RIE (usb_mid_front_enable (chip, SANE_TRUE));
  RIE (usb_low_set_serial_byte1 (chip, 0x40));
  RIE (usb_low_set_serial_byte2 (chip, pga));
  RIE (usb_mid_front_enable (chip, SANE_FALSE));
  DBG (6, "usb_mid_front_set_red_pga: start\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_front_set_green_pga (ma1017 * chip, SANE_Byte pga)
{
  SANE_Status status;

  DBG (6, "usb_mid_front_set_green_pga: start\n");
  RIE (usb_mid_front_enable (chip, SANE_TRUE));
  RIE (usb_low_set_serial_byte1 (chip, 0x20));
  RIE (usb_low_set_serial_byte2 (chip, pga));
  RIE (usb_mid_front_enable (chip, SANE_FALSE));
  DBG (6, "usb_mid_front_set_green_pga: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_front_set_blue_pga (ma1017 * chip, SANE_Byte pga)
{
  SANE_Status status;

  DBG (6, "usb_mid_front_set_blue_pga: start\n");
  RIE (usb_mid_front_enable (chip, SANE_TRUE));
  RIE (usb_low_set_serial_byte1 (chip, 0x60));
  RIE (usb_low_set_serial_byte2 (chip, pga));
  RIE (usb_mid_front_enable (chip, SANE_FALSE));
  DBG (6, "usb_mid_front_set_blue_pga: exit\n");
  return SANE_STATUS_GOOD;
}

SANE_Status
usb_mid_front_set_rgb_signal (ma1017 * chip)
{
  SANE_Status status;

  DBG (6, "usb_mid_front_set_rgb_signal: start\n");
  RIE (usb_low_set_red_ref (chip, 0xEF));
  RIE (usb_low_set_green_ref (chip, 0xF7));
  RIE (usb_low_set_blue_ref (chip, 0xFF));
  DBG (6, "usb_mid_front_set_rgb_signal: exit\n");
  return SANE_STATUS_GOOD;
}
