/* sane - Scanner Access Now Easy.

   Copyright (C) 2005 Pierre Willenbrock <pierre@pirsoft.dnsalias.org>

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

/*
 * Conversion filters for genesys backend
 */

static SANE_Status
FUNC_NAME(genesys_reorder_components_cis) (
    uint8_t *src_data,
    uint8_t *dst_data,
    unsigned int lines,
    unsigned int pixels)
{
    unsigned int x, y;
    uint8_t *src[3];
    uint8_t *dst = dst_data;
    unsigned int rest = pixels * 2 * BYTES_PER_COMPONENT;

    src[0] = src_data + pixels * BYTES_PER_COMPONENT * 0;
    src[1] = src_data + pixels * BYTES_PER_COMPONENT * 1;
    src[2] = src_data + pixels * BYTES_PER_COMPONENT * 2;

    for(y = 0; y < lines; y++) {
	for(x = 0; x < pixels; x++) {

#ifndef DOUBLE_BYTE
	    *dst++ = *src[0]++;
	    *dst++ = *src[1]++;
	    *dst++ = *src[2]++;
#else
#  ifndef WORDS_BIGENDIAN
	    *dst++ = *src[0]++;
	    *dst++ = *src[0]++;
	    *dst++ = *src[1]++;
	    *dst++ = *src[1]++;
	    *dst++ = *src[2]++;
	    *dst++ = *src[2]++;
#  else
	    *dst++ = src[0][1];
	    *dst++ = src[0][0];
	    *dst++ = src[1][1];
	    *dst++ = src[1][0];
	    *dst++ = src[2][1];
	    *dst++ = src[2][0];
	    src[0] += 2;
	    src[1] += 2;
	    src[2] += 2;
#  endif
#endif
	}

	src[0] += rest;
	src[1] += rest;
	src[2] += rest;
    }
    return SANE_STATUS_GOOD;
}

static SANE_Status
FUNC_NAME(genesys_reorder_components_cis_bgr) (
    uint8_t *src_data,
    uint8_t *dst_data,
    unsigned int lines,
    unsigned int pixels)
{
    unsigned int x, y;
    uint8_t *src[3];
    uint8_t *dst = dst_data;
    unsigned int rest = pixels * 2 * BYTES_PER_COMPONENT;

    src[0] = src_data + pixels * BYTES_PER_COMPONENT * 0;
    src[1] = src_data + pixels * BYTES_PER_COMPONENT * 1;
    src[2] = src_data + pixels * BYTES_PER_COMPONENT * 2;

    for(y = 0; y < lines; y++) {
	for(x = 0; x < pixels; x++) {
#ifndef DOUBLE_BYTE
	    *dst++ = *src[2]++;
	    *dst++ = *src[1]++;
	    *dst++ = *src[0]++;
#else
#  ifndef WORDS_BIGENDIAN
	    *dst++ = *src[2]++;
	    *dst++ = *src[2]++;
	    *dst++ = *src[1]++;
	    *dst++ = *src[1]++;
	    *dst++ = *src[0]++;
	    *dst++ = *src[0]++;
#  else
	    *dst++ = src[2][1];
	    *dst++ = src[2][0];
	    *dst++ = src[1][1];
	    *dst++ = src[1][0];
	    *dst++ = src[0][1];
	    *dst++ = src[0][0];
	    src[0] += 2;
	    src[1] += 2;
	    src[2] += 2;
#  endif
#endif
	}

	src[0] += rest;
	src[1] += rest;
	src[2] += rest;
    }
    return SANE_STATUS_GOOD;
}

static SANE_Status
FUNC_NAME(genesys_reorder_components_bgr) (
    uint8_t *src_data,
    uint8_t *dst_data,
    unsigned int lines,
    unsigned int pixels)
{
    unsigned int c;
    uint8_t *src = src_data;
    uint8_t *dst = dst_data;

    for(c = 0; c < lines * pixels; c++) {

#ifndef DOUBLE_BYTE
	*dst++ = src[2];
	*dst++ = src[1];
	*dst++ = src[0];
	src += 3;
#else
#  ifndef WORDS_BIGENDIAN
	*dst++ = src[2 * 2 + 0];
	*dst++ = src[2 * 2 + 1];
	*dst++ = src[1 * 2 + 0];
	*dst++ = src[1 * 2 + 1];
	*dst++ = src[0 * 2 + 0];
	*dst++ = src[0 * 2 + 1];
#  else
	*dst++ = src[2 * 2 + 1];
	*dst++ = src[2 * 2 + 0];
	*dst++ = src[1 * 2 + 1];
	*dst++ = src[1 * 2 + 0];
	*dst++ = src[0 * 2 + 1];
	*dst++ = src[0 * 2 + 0];
#  endif
	src += 3 * 2;
#endif

    }
    return SANE_STATUS_GOOD;
}

#if defined(DOUBLE_BYTE) && defined(WORDS_BIGENDIAN)
static SANE_Status
FUNC_NAME(genesys_reorder_components_endian) (
    uint8_t *src_data,
    uint8_t *dst_data,
    unsigned int lines,
    unsigned int pixels,
    unsigned int channels)
{
    unsigned int c;
    uint8_t *src = src_data;
    uint8_t *dst = dst_data;

    for(c = 0; c < lines * pixels * channels; c++) {
	*dst++ = src[1];
	*dst++ = src[0];
	src += 2;
    }
return SANE_STATUS_GOOD;
}
#endif /*defined(DOUBLE_BYTE) && defined(WORDS_BIGENDIAN)*/


static SANE_Status
FUNC_NAME(genesys_reverse_ccd) (
    uint8_t *src_data,
    uint8_t *dst_data,
    unsigned int lines,
    unsigned int components_per_line,
    unsigned int *ccd_shift,
    unsigned int component_count)
{
    unsigned int x, y, c;
    COMPONENT_TYPE *src = (COMPONENT_TYPE *)src_data;
    COMPONENT_TYPE *dst = (COMPONENT_TYPE *)dst_data;
    COMPONENT_TYPE *srcp;
    COMPONENT_TYPE *dstp;
    unsigned int pitch = components_per_line;
    unsigned int ccd_shift_pitch[12];
    unsigned int *csp;

    for (c = 0; c < component_count; c++)
	ccd_shift_pitch[c] = ccd_shift[c] * pitch;

/*
 * cache efficiency:
   we are processing a single line component_count times, so it should fit
   into the cpu cache for maximum efficiency. our lines take
   maximum 252kb(3 channels, 16bit, 2400dpi, full gl841 shading range)
 * instruction efficiency:
   the innermost loop runs long and consists of 3 adds, one compare,
   2 derefences.
 */
/*
    for (y = 0; y < lines; y++) {
	csp = ccd_shift_pitch;
	for (c = 0; c < component_count; c++) {
	    srcp = src + c + *csp++;
	    dstp = dst + c;
	    for (x = 0; x < pitch; x += component_count) {
		*dstp = *srcp;
		srcp += component_count;
		dstp += component_count;
	    }
	}
	dst += pitch;
	src += pitch;
    }
 */
/*
 * cache efficency:
   here only line_dist_pitch needs to stay in cache. 12*4 = 48 bytes
 * instruction efficiency:
   we have a short running inner loop, consisting of 4 incs, 2 compare, 1 add,
   2 dereference and 1 indexed dereference.
   the enclosing loop is long running, consisting of 1 add, 1 compare.
 */
    srcp = src;
    dstp = dst;
    for (y = 0; y < lines; y++) {
	for (x = 0; x < pitch; x += component_count) {
	    csp = ccd_shift_pitch;
	    for (c = 0; c < component_count && c + x < pitch; c++) {
		*dstp = srcp[*csp++];
		dstp++;
		srcp++;
	    }
	}
    }
    return SANE_STATUS_GOOD;
}

static SANE_Status
FUNC_NAME(genesys_shrink_lines) (
    uint8_t *src_data,
    uint8_t *dst_data,
    unsigned int lines,
    unsigned int src_pixels,
    unsigned int dst_pixels,
    unsigned int channels)
{
    unsigned int dst_x, src_x, y, c, cnt;
    unsigned int avg[3];
    unsigned int count;
    COMPONENT_TYPE *src = (COMPONENT_TYPE *)src_data;
    COMPONENT_TYPE *dst = (COMPONENT_TYPE *)dst_data;

    if (src_pixels > dst_pixels) {
/*average*/
	for (c = 0; c < channels; c++)
	    avg[c] = 0;
	for(y = 0; y < lines; y++) {
	    cnt = src_pixels / 2;
	    src_x = 0;
	    for (dst_x = 0; dst_x < dst_pixels; dst_x++) {
		count = 0;
		while (cnt < src_pixels && src_x < src_pixels) {
		    cnt += dst_pixels;

		    for (c = 0; c < channels; c++)
			avg[c] += *src++;
		    src_x++;
		    count++;
		}
		cnt -= src_pixels;

		for (c = 0; c < channels; c++) {
		    *dst++ = avg[c] / count;
		    avg[c] = 0;
		}
	    }
	}
    } else {
/*interpolate. copy pixels*/
	for(y = 0; y < lines; y++) {
	    cnt = dst_pixels / 2;
	    dst_x = 0;
	    for (src_x = 0; src_x < src_pixels; src_x++) {
		for (c = 0; c < channels; c++)
		    avg[c] = *src++;
		while ((cnt < dst_pixels || src_x + 1 == src_pixels) &&
		       dst_x < dst_pixels) {
		    cnt += src_pixels;

		    for (c = 0; c < channels; c++)
			*dst++ = avg[c];
		    dst_x++;
		}
		cnt -= dst_pixels;
	    }
	}
    }
    return SANE_STATUS_GOOD;
}
