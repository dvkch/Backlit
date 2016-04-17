/* sane - Scanner Access Now Easy.
   Copyright (C) 1996 David Mosberger-Tang and Andreas Beck
   This file is part of the SANE package.

   SANE is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2 of the License, or (at your
   option) any later version.

   SANE is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License
   along with sane; see the file COPYING.  If not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

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

/** @file sanei_codec_ascii.h
 * ASCII codec for network and file transmissions
 *
 * Instead translating data to a byte stream this codec uses ASCII hex numbers.
 * Therefore it can be used for streams that are not 8-bit clean or which can
 * only use printable characters. It's currently used for saving/restoring
 * data to/from disk.
 *
 * @sa sanei_codec_bin.h sanei_net.h sanei_wire.h
 */
#ifndef sanei_codec_ascii_h
#define sanei_codec_ascii_h

/** Initialize the ascii codec
 *
 * Set the i/o functions of the Wire to those of the ASCII codec.
 *
 * @param w Wire
 */
extern void sanei_codec_ascii_init (Wire *w);

#endif /* sanei_codec_ascii_h */
