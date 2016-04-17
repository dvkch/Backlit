/* sane - Scanner Access Now Easy.

   Copyright (C) 2006-2010 Stéphane Voltz <stef.dev@free.fr>
   Copyright (C) 2010 "Torsten Houwaart" <ToHo@gmx.de> X74 support

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

static Lexmark_Sensor sensor_list[] = {
  {
   X1100_B2_SENSOR,
   /* start x, end x and target average for offset calibration */
   48, 80, 6,
   /* usable pixel sensor startx */
   106,
   /* default gain */
   16,
   /* gain calibration targets */
   180, 180, 180, 180,
   /* shading correction targets */
   260, 260, 260, 260,
   /* offset and gain fallback */
   0x70, 17},
  {
   X1100_2C_SENSOR,
   /* start x, end x and target average for offset calibration */
   48, 80, 12,
   /* usable pixel sensor startx */
   106,
   /* default gain */
   10,
   /* gain calibration */
   140, 150, 150, 150,
   /* shading correction */
   260, 260, 260, 260,
   /* offset and gain fallback */
   0x70, 11},
  {				/* USB 1.1 settings */
   X1200_SENSOR,
   /* start x, end x and target average for offset calibration */
   32, 64, 15,
   /* usable pixel sensor startx */
   136,
   /* default gain */
   16,
   /* gain calibration */
   180, 180, 180, 180,
   /* shading correction */
   260, 260, 260, 260,
   /* offset and gain fallback */
   0x86, 16},
  {				/* this one is a 1200 on USB2.0 */
   X1200_USB2_SENSOR,
   /* start x, end x and target average for offset calibration */
   32, 64, 12,
   /* usable pixel sensor startx */
   136,
   /* default gain */
   16,
   /* gain calibration */
   180, 180, 180, 180,
   /* shading correction */
   260, 260, 260, 260,
   /* offset and gain fallback */
   0x86, 16},
  {
   A920_SENSOR,
   /* start x, end x and target average for offset calibration */
   48, 80, 6,
   /* usable pixel sensor startx */
   106,
   /* default gain */
   12,
   /* gain calibration target */
   130, 145, 150, 145,
   /* gain calibration target */
   260, 260, 260, 260,
   /* offset and gain fallback */
   0x70, 13},
  {
   X74_SENSOR,
   /* start x TDONE, end x and target average for offset calibration */
   /*36,68,12, */
   20, 52, 12,
   /* usable pixel sensor startx */
   /*104, */
   104,
   /* default gain */
   10,
   /* gain calibration target */
   130, 145, 150, 145,
   /* gain calibration target */
   260, 260, 260, 260,
   /* offset and gain fallback */
   0x70, 13},
  /* termination list sensor, must be last */
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};
