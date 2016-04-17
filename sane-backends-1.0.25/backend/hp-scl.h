/* sane - Scanner Access Now Easy.
   Copyright (C) 1997 Geoffrey T. Dairiki
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

   This file is part of a SANE backend for HP Scanners supporting
   HP Scanner Control Language (SCL).
*/

#ifndef HP_SCL_INCLUDED
#define HP_SCL_INCLUDED

#define HP_SCL_PACK(id, group, char) \
        ((SANE_Word)(id) << 16 | ((group) & 0xFF) << 8 | ((char) & 0xFF))
#define SCL_INQ_ID(code)		((code) >> 16)
#define SCL_GROUP_CHAR(code)		((char)(((code) >> 8) & 0xFF))
#define SCL_PARAM_CHAR(code)		((char)((code) & 0xFF))

#define HP_SCL_CONTROL(id,g,c)		HP_SCL_PACK(id,g,c)
#define HP_SCL_COMMAND(g,c)		HP_SCL_PACK(0,g,c)
#define HP_SCL_PARAMETER(id)		HP_SCL_PACK(id, 0, 0)
#define HP_SCL_DATA_TYPE(id)		HP_SCL_PACK(id, 1, 0)

#define IS_SCL_CONTROL(scl)	(SCL_INQ_ID(scl) && SCL_PARAM_CHAR(scl))
#define IS_SCL_COMMAND(scl)	(!SCL_INQ_ID(scl) && SCL_PARAM_CHAR(scl))
#define IS_SCL_PARAMETER(scl)	(SCL_INQ_ID(scl) && !SCL_PARAM_CHAR(scl))
#define IS_SCL_DATA_TYPE(scl)	(SCL_GROUP_CHAR(scl) == '\001')

#define SCL_AUTO_BKGRND		HP_SCL_CONTROL(10307, 'a', 'B')
#define SCL_COMPRESSION		HP_SCL_CONTROL(10308, 'a', 'C')
#define SCL_DOWNLOAD_TYPE	HP_SCL_CONTROL(10309, 'a', 'D')
#define SCL_X_SCALE		HP_SCL_CONTROL(10310, 'a', 'E')
#define SCL_Y_SCALE		HP_SCL_CONTROL(10311, 'a', 'F')
#define SCL_DATA_WIDTH		HP_SCL_CONTROL(10312, 'a', 'G')
#define SCL_INVERSE_IMAGE	HP_SCL_CONTROL(10314, 'a', 'I')
#define SCL_BW_DITHER		HP_SCL_CONTROL(10315, 'a', 'J')
#define SCL_CONTRAST		HP_SCL_CONTROL(10316, 'a', 'K')
#define SCL_BRIGHTNESS		HP_SCL_CONTROL(10317, 'a', 'L')
#define SCL_MIRROR_IMAGE        HP_SCL_CONTROL(10318, 'a', 'M')
#define SCL_SHARPENING		HP_SCL_CONTROL(10319, 'a', 'N')
#define SCL_RESERVED1           HP_SCL_CONTROL(10320, 'a', 'O')
#define SCL_X_RESOLUTION	HP_SCL_CONTROL(10323, 'a', 'R')
#define SCL_Y_RESOLUTION	HP_SCL_CONTROL(10324, 'a', 'S')
#define SCL_OUTPUT_DATA_TYPE	HP_SCL_CONTROL(10325, 'a', 'T')
#define SCL_DOWNLOAD_LENGTH	HP_SCL_CONTROL(10328, 'a', 'W')
#define SCL_PRELOAD_ADF         HP_SCL_CONTROL(10468, 'f', 'C')
#define SCL_MEDIA               HP_SCL_CONTROL(10469, 'f', 'D')
#define SCL_10470               HP_SCL_CONTROL(10470, 'f', 'E')
#define SCL_LAMPTEST            HP_SCL_CONTROL(10477, 'f', 'L')
#define SCL_X_EXTENT		HP_SCL_CONTROL(10481, 'f', 'P')
#define SCL_Y_EXTENT		HP_SCL_CONTROL(10482, 'f', 'Q')
#define SCL_START_SCAN		HP_SCL_COMMAND('f', 'S')
#define SCL_10485               HP_SCL_CONTROL(10485, 'f', 'T')
#define SCL_10488               HP_SCL_CONTROL(10488, 'f', 'W')
#define SCL_X_POS		HP_SCL_CONTROL(10489, 'f', 'X')
#define SCL_Y_POS		HP_SCL_CONTROL(10490, 'f', 'Y')
#define SCL_XPA_SCAN		HP_SCL_COMMAND('u', 'D')
#define SCL_SPEED		HP_SCL_CONTROL(10950, 'u', 'E')
#define SCL_FILTER		HP_SCL_CONTROL(10951, 'u', 'F')
#define SCL_10952		HP_SCL_CONTROL(10952, 'u', 'G')
#define SCL_XPA_DISABLE         HP_SCL_CONTROL(10953, 'u', 'H')
#define SCL_TONE_MAP		HP_SCL_CONTROL(10956, 'u', 'K')
#define SCL_CALIBRATE		HP_SCL_COMMAND('u', 'R')
#define SCL_ADF_SCAN		HP_SCL_COMMAND('u', 'S')
#define SCL_MATRIX		HP_SCL_CONTROL(10965, 'u', 'T')
#define SCL_UNLOAD		HP_SCL_CONTROL(10966, 'u', 'U')
#define SCL_10967		HP_SCL_CONTROL(10967, 'u', 'V')
#define SCL_CHANGE_DOC		HP_SCL_CONTROL(10969, 'u', 'X')
#define SCL_ADF_BFEED		HP_SCL_CONTROL(10970, 'u', 'Y')
/* Clear Errors does not follow command syntax Esc*o0E, it is only Esc*oE */
/* #define SCL_CLEAR_ERRORS	HP_SCL_COMMAND('o', 'E')  */

#define SCL_INQUIRE_PRESENT_VALUE	HP_SCL_COMMAND('s', 'R')
#define SCL_INQUIRE_MINIMUM_VALUE	HP_SCL_COMMAND('s', 'L')
#define SCL_INQUIRE_MAXIMUM_VALUE	HP_SCL_COMMAND('s', 'H')
#define SCL_INQUIRE_DEVICE_PARAMETER	HP_SCL_COMMAND('s', 'E')
#define SCL_UPLOAD_BINARY_DATA		HP_SCL_COMMAND('s', 'U')

#define SCL_HP_MODEL_1		HP_SCL_PARAMETER(3)
#define SCL_HP_MODEL_2		HP_SCL_PARAMETER(10)
#define SCL_HP_MODEL_3		HP_SCL_PARAMETER(9)
#define SCL_HP_MODEL_4		HP_SCL_PARAMETER(11)
#define SCL_HP_MODEL_5		HP_SCL_PARAMETER(12)
#define SCL_HP_MODEL_6		HP_SCL_PARAMETER(14)
#define SCL_HP_MODEL_8		HP_SCL_PARAMETER(15)
#define SCL_HP_MODEL_9		HP_SCL_PARAMETER(16)
#define SCL_HP_MODEL_10		HP_SCL_PARAMETER(17)
#define SCL_HP_MODEL_11		HP_SCL_PARAMETER(18)
#define SCL_HP_MODEL_12		HP_SCL_PARAMETER(19)
#define SCL_HP_MODEL_14		HP_SCL_PARAMETER(21)
#define SCL_HP_MODEL_16		HP_SCL_PARAMETER(31)
#define SCL_HP_MODEL_17		HP_SCL_PARAMETER(32)

#define SCL_ADF_CAPABILITY      HP_SCL_PARAMETER(24)
#define SCL_ADF_BIN		HP_SCL_PARAMETER(25)
#define SCL_ADF_RDY_UNLOAD	HP_SCL_PARAMETER(27)

#define SCL_CURRENT_ERROR_STACK	HP_SCL_PARAMETER(257)
#define SCL_CURRENT_ERROR	HP_SCL_PARAMETER(259)
#define SCL_OLDEST_ERROR	HP_SCL_PARAMETER(261)
#define SCL_PIXELS_PER_LINE	HP_SCL_PARAMETER(1024)
#define SCL_BYTES_PER_LINE	HP_SCL_PARAMETER(1025)
#define SCL_NUMBER_OF_LINES	HP_SCL_PARAMETER(1026)
#define SCL_ADF_READY		HP_SCL_PARAMETER(1027)

#define SCL_DEVPIX_RESOLUTION	HP_SCL_PARAMETER(1028)

#define SCL_AUTO_SPEED		HP_SCL_PARAMETER(1040)

#define SCL_FRONT_BUTTON        HP_SCL_PARAMETER(1044)

#define SCL_PRELOADED		HP_SCL_PARAMETER(1045)

/* The following is not documented */
#define SCL_SECONDARY_SCANDIR   HP_SCL_PARAMETER(1047)

#define SCL_BW8x8DITHER		HP_SCL_DATA_TYPE(0)
#define SCL_8x8TONE_MAP		HP_SCL_DATA_TYPE(1)
#define SCL_8x9MATRIX_COEFF	HP_SCL_DATA_TYPE(2)
#define SCL_8x8DITHER		HP_SCL_DATA_TYPE(3)
#define SCL_CAL_STRIP		HP_SCL_DATA_TYPE(4)
#define SCL_BW16x16DITHER	HP_SCL_DATA_TYPE(5)
#define SCL_10x8TONE_MAP	HP_SCL_DATA_TYPE(6)
#define SCL_10x3MATRIX_COEFF	HP_SCL_DATA_TYPE(8)
#define SCL_10x9MATRIX_COEFF	HP_SCL_DATA_TYPE(9)
#define SCL_7x12TONE_MAP	HP_SCL_DATA_TYPE(10)
#define SCL_BW7x12TONE_MAP	HP_SCL_DATA_TYPE(11)
#define SCL_RGB_GAINS     	HP_SCL_DATA_TYPE(11)
#define SCL_CALIB_MAP     	HP_SCL_DATA_TYPE(14)

#endif /*  HP_SCL_INCLUDED */
