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

static Lexmark_Model model_list[] = {
  {
   0x043d,			/* vendor id */
   0x007c,			/* product id */
   0xb2,			/* submodel id */
   "Lexmark X1100",		/* name */
   "Lexmark",			/* vendor */
   "X1100/rev. B2",		/* model */
   X1100_MOTOR,
   /* X1100 series has 2 sensors */
   X1100_B2_SENSOR,
   1235,			/* first x-coordinate of Home Point */
   1258},			/* second x-coordinate of Home Point */
  {
   0x043d,			/* vendor id */
   0x007c,			/* product id */
   0x2c,			/* submodel id */
   "Lexmark X1100",		/* name */
   "Lexmark",			/* vendor */
   "X1100/rev. 2C",		/* model */
   A920_MOTOR,			/* X1100 series has 2 sensors, 2C or B2. It
				   is detected at sane_open() */
   X1100_2C_SENSOR,
   1235,			/* first x-coordinate of Home Point */
   1258},			/* second x-coordinate of Home Point */
  {
   0x413c,			/* vendor id */
   0x5105,			/* product id */
   0x2c,			/* submodel id */
   "Dell A920",			/* name */
   "Dell",			/* vendor */
   "A920",			/* model */
   A920_MOTOR,
   A920_SENSOR,
   1235,			/* first x-coordinate of Home Point */
   1258},			/* second x-coordinate of Home Point */
  {
   0x043d,			/* vendor id */
   0x007d,			/* product id */
   0x87,			/* submodel id */
   "Lexmark X1200",		/* name */
   "Lexmark",			/* vendor */
   "X1200/USB1.1",		/* model */
   A920_MOTOR,
   X1200_SENSOR,
   1235,			/* first x-coordinate of Home Point */
   1258},			/* second x-coordinate of Home Point */
  {
   0x043d,			/* vendor id */
   0x007d,			/* product id */
   0x97,			/* submodel id */
   "Lexmark X1200",		/* name */
   "Lexmark",			/* vendor */
   "X1200/USB2.0",		/* model */
   A920_MOTOR,
   X1200_USB2_SENSOR,
   1235,			/* first x-coordinate of Home Point */
   1258},			/* second x-coordinate of Home Point */
  {
   0x043d,			/* vendor id */
   0x0060,			/* product id */
   0x00,			/* submodel id */
   "Lexmark X74",		/* name */
   "Lexmark",			/* vendor */
   "X74",			/* model */
   X74_MOTOR,
   X74_SENSOR,
   1222,			/* first x-coordinate of Home Point */
   1322},			/* second x-coordinate of Home Point */
  {				/* termination model, must be last */
   0, 0, 0, NULL, NULL, NULL, 0, 0, 0, 0}
};				/* end models description */
