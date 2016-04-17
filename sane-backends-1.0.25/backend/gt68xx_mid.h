/* sane - Scanner Access Now Easy.

   Copyright (C) 2002 Sergey Vlasov <vsu@altlinux.ru>
   
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

#ifndef GT68XX_MID_H
#define GT68XX_MID_H

/** @file
 * @brief Image data unpacking.
 */

#include "gt68xx_low.h"
#include "../include/sane/sane.h"

typedef struct GT68xx_Delay_Buffer GT68xx_Delay_Buffer;
typedef struct GT68xx_Line_Reader GT68xx_Line_Reader;

struct GT68xx_Delay_Buffer
{
  SANE_Int line_count;
  SANE_Int read_index;
  SANE_Int write_index;
  unsigned int **lines;
  SANE_Byte *mem_block;
};

/**
 * Object for reading image data line by line, with line distance correction.
 *
 * This object handles reading the image data from the scanner line by line and
 * converting it to internal format.  Internally each image sample is
 * represented as <code>unsigned int</code> value, scaled to 16-bit range
 * (0-65535).  For color images the data for each primary color is stored as
 * separate lines.
 */
struct GT68xx_Line_Reader
{
  GT68xx_Device *dev;			/**< Low-level interface object */
  GT68xx_Scan_Parameters params;	/**< Scan parameters */

#if 0
  /** Number of bytes in the returned scanlines */
  SANE_Int bytes_per_line;

  /** Number of bytes per pixel in the returned scanlines */
  SANE_Int bytes_per_pixel;
#endif

  /** Number of pixels in the returned scanlines */
  SANE_Int pixels_per_line;

  SANE_Byte *pixel_buffer;

  GT68xx_Delay_Buffer r_delay;
  GT68xx_Delay_Buffer g_delay;
  GT68xx_Delay_Buffer b_delay;
  SANE_Bool delays_initialized;

    SANE_Status (*read) (GT68xx_Line_Reader * reader,
			 unsigned int **buffer_pointers_return);
};

/**
 * Create a new GT68xx_Line_Reader object.
 *
 * @param dev           The low-level scanner interface object.
 * @param params        Scan parameters prepared by gt68xx_device_setup_scan().
 * @param final_scan    SANE_TRUE for the final scan, SANE_FALSE for
 *                      calibration scans.
 * @param reader_return Location for the returned object.
 *
 * @return
 * - SANE_STATUS_GOOD   - on success
 * - SANE_STATUS_NO_MEM - cannot allocate memory for object or buffers
 * - other error values - failure of some internal functions
 */
static SANE_Status
gt68xx_line_reader_new (GT68xx_Device * dev,
			GT68xx_Scan_Parameters * params,
			SANE_Bool final_scan,
			GT68xx_Line_Reader ** reader_return);

/**
 * Destroy the GT68xx_Line_Reader object.
 *
 * @param reader  The GT68xx_Line_Reader object to destroy.
 */
static SANE_Status gt68xx_line_reader_free (GT68xx_Line_Reader * reader);

/**
 * Read a scanline from the GT68xx_Line_Reader object.
 *
 * @param reader      The GT68xx_Line_Reader object.
 * @param buffer_pointers_return Array of pointers to image lines (1 or 3
 * elements)
 *
 * This function reads a full scanline from the device, unpacks it to internal
 * buffers and returns pointer to these buffers in @a
 * buffer_pointers_return[i].  For monochrome scan, only @a
 * buffer_pointers_return[0] is filled; for color scan, elements 0, 1, 2 are
 * filled with pointers to red, green, and blue data.  The returned pointers
 * are valid until the next call to gt68xx_line_reader_read(), or until @a
 * reader is destroyed.
 *
 * @return
 * - SANE_STATUS_GOOD  - read completed successfully
 * - other error value - an error occured
 */
static SANE_Status
gt68xx_line_reader_read (GT68xx_Line_Reader * reader,
			 unsigned int **buffer_pointers_return);

#endif /* not GT68XX_MID_H */

/* vim: set sw=2 cino=>2se-1sn-1s{s^-1st0(0u0 smarttab expandtab: */
