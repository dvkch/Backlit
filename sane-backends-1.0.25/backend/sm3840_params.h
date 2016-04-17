/* sane - Scanner Access Now Easy.

   ScanMaker 3840 Backend
   Copyright (C) 2005-7 Earle F. Philhower, III
   earle@ziplabel.com - http://www.ziplabel.com

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

#ifndef _SM3840_Params
#define _SM3840_Params
typedef struct SM3840_Params
{
  int gray;			/* 0, 1 */
  int halftone;			/* 0, 1 (also set gray=1) */
  int lineart;			/* 0, 1 (also set gray=1) */

  int dpi;			/* 150, 300, 600, 1200 */
  int bpp;			/* 8, 16 */

  double gain;			/* 0.01...9.9 */
  int offset;			/* 0..4095 */

  int lamp;			/* 1..15 mins */

  int threshold;		/* 0...255 luminosity */

  /* Input to configure(), in inches */
  double top, left;
  double width, height;

  /* Output from configure(), in pixels */
  int topline;
  int scanlines;
  int leftpix;
  int scanpix;
  int linelen;			/* bytes per line */
} SM3840_Params;
#endif
