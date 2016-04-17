/*
 * epsonds-jpeg.c - Epson ESC/I-2 driver, JPEG support.
 *
 * Copyright (C) 2015 Tower Technologies
 * Author: Alessandro Zummo <a.zummo@towertech.it>
 *
 * This file is part of the SANE package.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2.
 */

#define DEBUG_DECLARE_ONLY

#include <math.h>

#include "epsonds.h"
#include "epsonds-jpeg.h"
#include "epsonds-ops.h"

#define min(A,B) (((A)<(B)) ? (A) : (B))

typedef struct
{
	struct jpeg_source_mgr pub;

	epsonds_scanner *s;
	JOCTET *buffer;

	SANE_Byte *linebuffer;
	SANE_Int linebuffer_size;
	SANE_Int linebuffer_index;
}
epsonds_src_mgr;

METHODDEF(void)
jpeg_init_source(j_decompress_ptr UNUSEDARG cinfo)
{
}

METHODDEF(void)
jpeg_term_source(j_decompress_ptr UNUSEDARG cinfo)
{
}

METHODDEF(boolean)
jpeg_fill_input_buffer(j_decompress_ptr cinfo)
{
	epsonds_src_mgr *src = (epsonds_src_mgr *)cinfo->src;
	int avail, size;

	/* read from the scanner or the ring buffer */

	avail = eds_ring_avail(src->s->current);
	if (avail == 0) {
		return FALSE;
	}

	/* read from scanner if no data? */
	size = min(1024, avail);

	eds_ring_read(src->s->current, src->buffer, size);

	src->pub.next_input_byte = src->buffer;
	src->pub.bytes_in_buffer = size;

	return TRUE;
}

METHODDEF (void)
jpeg_skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
	epsonds_src_mgr *src = (epsonds_src_mgr *)cinfo->src;

	if (num_bytes > 0) {

		while (num_bytes > (long)src->pub.bytes_in_buffer) {
			num_bytes -= (long)src->pub.bytes_in_buffer;
			jpeg_fill_input_buffer(cinfo);
		}

		src->pub.next_input_byte += (size_t) num_bytes;
		src->pub.bytes_in_buffer -= (size_t) num_bytes;
	}
}

SANE_Status
eds_jpeg_start(epsonds_scanner *s)
{
	epsonds_src_mgr *src;

	s->jpeg_cinfo.err = jpeg_std_error(&s->jpeg_err);

	jpeg_create_decompress(&s->jpeg_cinfo);

	s->jpeg_cinfo.src = (struct jpeg_source_mgr *)(*s->jpeg_cinfo.mem->alloc_small)((j_common_ptr)&s->jpeg_cinfo,
						JPOOL_PERMANENT, sizeof(epsonds_src_mgr));

	memset(s->jpeg_cinfo.src, 0x00, sizeof(epsonds_src_mgr));

	src = (epsonds_src_mgr *)s->jpeg_cinfo.src;
	src->s = s;

	src->buffer = (JOCTET *)(*s->jpeg_cinfo.mem->alloc_small)((j_common_ptr)&s->jpeg_cinfo,
							JPOOL_PERMANENT,
							1024 * sizeof(JOCTET));

	src->pub.init_source = jpeg_init_source;
	src->pub.fill_input_buffer = jpeg_fill_input_buffer;
	src->pub.skip_input_data = jpeg_skip_input_data;
	src->pub.resync_to_restart = jpeg_resync_to_restart;
	src->pub.term_source = jpeg_term_source;
	src->pub.bytes_in_buffer = 0;
	src->pub.next_input_byte = NULL;

	s->jpeg_header_seen = 0;

	return SANE_STATUS_GOOD;
}

SANE_Status
eds_jpeg_read_header(epsonds_scanner *s)
{
	epsonds_src_mgr *src = (epsonds_src_mgr *)s->jpeg_cinfo.src;

	if (jpeg_read_header(&s->jpeg_cinfo, TRUE)) {

		s->jdst = sanei_jpeg_jinit_write_ppm(&s->jpeg_cinfo);

		if (jpeg_start_decompress(&s->jpeg_cinfo)) {

			int size;

			DBG(3, "%s: w: %d, h: %d, components: %d\n",
				__func__,
				s->jpeg_cinfo.output_width, s->jpeg_cinfo.output_height,
				s->jpeg_cinfo.output_components);

			size = s->jpeg_cinfo.output_width * s->jpeg_cinfo.output_components * 1;

			src->linebuffer = (*s->jpeg_cinfo.mem->alloc_large)((j_common_ptr)&s->jpeg_cinfo,
				JPOOL_PERMANENT, size);

			src->linebuffer_size = 0;
			src->linebuffer_index = 0;

			s->jpeg_header_seen = 1;

			return SANE_STATUS_GOOD;

		} else {
			DBG(0, "%s: decompression failed\n", __func__);
			return SANE_STATUS_IO_ERROR;
		}
	} else {
		DBG(0, "%s: cannot read JPEG header\n", __func__);
		return SANE_STATUS_IO_ERROR;
	}
}

void
eds_jpeg_finish(epsonds_scanner *s)
{
	jpeg_destroy_decompress(&s->jpeg_cinfo);
}

void
eds_jpeg_read(SANE_Handle handle, SANE_Byte *data,
	   SANE_Int max_length, SANE_Int *length)
{
	epsonds_scanner *s = handle;

	struct jpeg_decompress_struct cinfo = s->jpeg_cinfo;
	epsonds_src_mgr *src = (epsonds_src_mgr *)s->jpeg_cinfo.src;

	int l;

	*length = 0;

	/* copy from line buffer if available */
	if (src->linebuffer_size && src->linebuffer_index < src->linebuffer_size) {

		*length = src->linebuffer_size - src->linebuffer_index;

		if (*length > max_length)
			*length = max_length;

		memcpy(data, src->linebuffer + src->linebuffer_index, *length);
		src->linebuffer_index += *length;

		return;
	}

	if (cinfo.output_scanline >= cinfo.output_height) {
		*length = 0;
		return;
	}

	/* scanlines of decompressed data will be in s->jdst->buffer
	 * only one line at time is supported
	 */

	l = jpeg_read_scanlines(&cinfo, s->jdst->buffer, 1);
	if (l == 0) {
		return;
	}

	/* from s->jdst->buffer to linebuffer
	 * linebuffer holds width * bytesperpixel
	 */

	(*s->jdst->put_pixel_rows)(&cinfo, s->jdst, 1, (char *)src->linebuffer);

	*length = cinfo.output_width * cinfo.output_components * 1;
	src->linebuffer_size = *length;
	src->linebuffer_index = 0;

	if (*length > max_length)
		*length = max_length;

	memcpy(data, src->linebuffer + src->linebuffer_index, *length);
	src->linebuffer_index += *length;
}


