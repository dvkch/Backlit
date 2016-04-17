/* sane - Scanner Access Now Easy.
   Copyright (C) 2001-2002 Matthew C. Duggan and Simon Krix
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

   -----

   This file is part of the canon_pp backend, supporting Canon CanoScan 
   Parallel scanners and also distributed as part of the stand-alone driver.  

   canon_pp-dev.c: $Revision$

   Misc constants for Canon CanoScan Parallel scanners and high-level scan 
   functions.

   Simon Krix <kinsei@users.sourceforge.net>
   */

#ifdef _AIX
#include <lalloca.h>
#endif

#ifndef NOSANE
#include "../include/sane/config.h"
#endif

#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <ieee1284.h>
#include "canon_pp-io.h"
#include "canon_pp-dev.h"

#ifdef NOSANE

/* No SANE, Things that only apply to stand-alone */
#include <stdio.h>
#include <stdarg.h>

static void DBG(int level, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	if (level < 50) vfprintf(stderr, format, args);
	va_end(args);
}
#else

/* Definitions which only apply to SANE compiles */
#ifndef VERSION
#define VERSION "$Revision$"
#endif

#define DEBUG_DECLARE_ONLY
#include "canon_pp.h"
#include "../include/sane/sanei_config.h"
#include "../include/sane/sanei_backend.h"

#endif

struct scanner_hardware_desc {
	char *name;
	unsigned int natural_xresolution;
	unsigned int natural_yresolution;
	unsigned int scanbedlength;
	unsigned int scanheadwidth;       /* 0 means provided by scanner */
	unsigned int type;
};

static const struct scanner_hardware_desc 
	/* The known scanner types */
	hw_fb320p = { "FB320P", 2, 2, 3508, 2552, 0 },
	hw_fb330p = { "FB330P", 2, 2, 3508,    0, 1 },
	hw_fb620p = { "FB620P", 3, 3, 7016, 5104, 0 },
	hw_fb630p = { "FB630P", 3, 3, 7016,    0, 1 },
	hw_n640p  = { "N640P",  3, 3, 7016,    0, 1 },
	hw_n340p  = { "N340P",  2, 2, 3508,    0, 1 },

	/* A few generic scanner descriptions for aliens */
	hw_alien600 = { "Unknown 600dpi", 3, 3, 7016, 0, 1 },
	hw_alien300 = { "Unknown 300dpi", 2, 2, 3508, 0, 1 },
	hw_alien = { "Unknown (600dpi?)", 3, 3, 7016, 0, 1 };

/* ID table linking ID strings with hardware descriptions */
struct scanner_id {
	char *id;
	const struct scanner_hardware_desc *hw;
};
static const struct scanner_id scanner_id_table[] = { 
	{ "CANON   IX-03055C", &hw_fb320p },
	{ "CANON   IX-06025C", &hw_fb620p },
	{ "CANON   IX-03075E", &hw_fb330p },
	{ "CANON   IX-06075E", &hw_fb630p },
	{ "CANON   IX-03095G", &hw_n340p },
	{ "CANON   IX-06115G", &hw_n640p },
	{ NULL, NULL } };

/*const int scanline_count = 6;*/
static const char *header = "#CANONPP";
static const int fileversion = 3;

/* Internal functions */
static unsigned long column_sum(image_segment *image, int x);
static int adjust_output(image_segment *image, scan_parameters *scanp, 
		scanner_parameters *scannerp);
static int check8(unsigned char *p, int s);
/* Converts from weird scanner format -> sequential data */
static void convdata(unsigned char *srcbuffer, unsigned char *dstbuffer, 
		int width, int mode);
/* Sets up the scan command. This could use a better name
   (and a rewrite). */
static int scanner_setup_params(unsigned char *buf, scanner_parameters *sp, 
		scan_parameters *scanp);

/* file reading and writing helpers */
static int safe_write(int fd, const char *p, unsigned long len);
static int safe_read(int fd, char *p, unsigned long len);

/* Command sending loop (waiting for ready status) */
static int send_command(struct parport *port, unsigned char *buf, int bufsize, 
		int delay, int timeout);

/* Commands ================================================ */

/* Command_1[] moved to canon_pp-io.c for neatness */


/* Read device ID command */
/* after this 0x26 (38) bytes are read */
static unsigned char cmd_readid[] = { 0xfe, 0x20, 0, 0, 0, 0, 0, 0, 0x26, 0 };

/* Reads 12 bytes of unknown information */
static unsigned char cmd_readinfo[] = { 0xf3, 0x20, 0, 0, 0, 0, 0, 0, 0x0c, 0 };

/* Scan init command: Always followed immediately by command cmd_scan */
static unsigned char cmd_initscan[] = { 0xde, 0x20, 0, 0, 0, 0, 0, 0, 0x2e, 0 };

/* Scan information block */
static unsigned char cmd_scan[45] =
{ 0x11, 0x2c, 0x11, 0x2c, 0x10, 0x4b, 0x10, 0x4b, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0x08, 0x08, 0x01, 0x01, 0x80, 0x01,
	0x80, 0x80, 0x02, 0, 0, 0xc1, 0, 0x08, 0x01, 0x01,
	0, 0, 0, 0, 0
};

/* Read 6 byte buffer status block */
static unsigned char cmd_buf_status[] = { 0xf3, 0x21, 0, 0, 0, 0, 0, 0, 0x06, 0 };

/* Request a block of image data */
static unsigned char cmd_packet_req[] = {0xd4, 0x20, 0, 0, 0, 0, 0, 0x09, 0x64, 0};

/* "*SCANEND" command - returns the scanner to transparent mode */
static unsigned char cmd_scanend[] =
{ 0x1b, 0x2a, 0x53, 0x43, 0x41, 0x4e, 0x45, 0x4e, 0x44, 0x0d };

/* Reads BLACK calibration image */
static unsigned char cmd_calblack[] ={0xf8, 0x20, 0, 0, 0, 0, 0, 0x4a, 0xc4, 0};

/* Clear the existing gamma table and create a new one */
static unsigned char cmd_cleargamma[] = {0xc5, 0x20, 0, 0, 0, 0, 0, 0, 0, 0};

/* Read back the gamma table values */
static unsigned char cmd_readgamma[] = {0xf6, 0x20, 0, 0, 0, 0, 0, 0, 0x20, 0};

/* Reads COLOUR (R,G or B) calibration image */
static unsigned char cmd_calcolour[]={0xf9, 0x20, 0, 0, 0, 0, 0, 0x4a, 0xc4, 0};

/* Abort scan */
static unsigned char cmd_abort[] = {0xef, 0x20, 0, 0, 0, 0, 0, 0, 0, 0};

/* Upload the gamma table (followed by 32 byte write) */
static unsigned char cmd_setgamma[] = {0xe6, 0x20, 0, 0, 0, 0, 0, 0, 0x20, 0};

#if 0
/* Something about RGB gamma/gain values? Not currently used by this code */
static unsigned char command_14[32] =
{ 0x2, 0x0, 0x3, 0x7f,
	0x2, 0x0, 0x3, 0x7f,
	0x2, 0x0, 0x3, 0x7f,
	0, 0, 0, 0,
	0x12, 0xd1, 0x14, 0x82,
	0, 0, 0, 0,
	0x0f, 0xff, 
	0x0f, 0xff, 
	0x0f, 0xff, 0, 0 };
#endif


/* Misc functions =================================== */

/*
 * safe_write(): a small wrapper which ensures all the data is written in calls
 * to write(), since the POSIX call doesn't ensure it. 
 */
static int safe_write(int fd, const char *p, unsigned long len) {
	int diff; 
	unsigned long total = 0;

	do {
		diff = write(fd, p+total, len-total);
		if (diff < 0) 
		{
			if (errno == EINTR) continue;
			return -1;
		}
		total += diff;
	} while (len > total);

	return 0;

}

/* same dealie for read, except in the case of read the return of 0 bytes with 
 * no INTR error indicates EOF */
static int safe_read(int fd, char *p, unsigned long len) {
	int diff;
	unsigned long total = 0;

	do {
		diff = read(fd, p+total, len-total);
		if (diff <= 0) 
		{
			if (errno == EINTR) continue;
			if (diff == 0) return -2;
			return -1;

		}
		total += diff;
	} while (len > total);

	return 0;

}


/* Scan-related functions =================================== */

int sanei_canon_pp_init_scan(scanner_parameters *sp, scan_parameters *scanp)
{
	/* Command for: Initialise and begin the scan procedure */
	unsigned char command_b[56];

	/* Buffer for buffer info block */
	unsigned char buffer_info_block[6];

	/* The image size the scanner says we asked for 
	   (based on the scanner's replies) */
	int true_scanline_size, true_scanline_count;

	/* The image size we expect to get (based on *scanp) */
	int expected_scanline_size, expected_scanline_count;

	/* Set up the default scan command packet */
	memcpy(command_b, cmd_initscan, 10);
	memcpy(command_b+10, cmd_scan, 45);

	/* Load the proper settings into it */
	scanner_setup_params(command_b+10, sp, scanp);

	/* Add checksum byte */
	command_b[55] = check8(command_b+10, 45);

	if (send_command(sp->port, command_b, 56, 50000, 1000000))
		return -1;
		
	/* Ask the scanner about the buffer */
	if (send_command(sp->port, cmd_buf_status, 10, 50000, 1000000))
		return -1;

	/* Read buffer information block */
	sanei_canon_pp_read(sp->port, 6, buffer_info_block);

	if (check8(buffer_info_block, 6)) 
		DBG(1, "init_scan: ** Warning: Checksum error reading buffer "
				"info block.\n");

	expected_scanline_count = scanp->height;

	switch(scanp->mode)
	{
		case 0: /* greyscale; 10 bits per pixel */
			expected_scanline_size = scanp->width * 1.25; break;
		case 1: /* true-colour; 30 bits per pixel */
			expected_scanline_size = scanp->width * 3.75; break;
		default: 
			DBG(1, "init_scan: Illegal mode %i requested in "
					"init_scan().\n", scanp->mode);
			DBG(1, "This is a bug. Please report it.\n");
			return -1;
	}

	/* The scanner's idea of the length of each scanline in bytes */
	true_scanline_size = (buffer_info_block[0]<<8) | buffer_info_block[1]; 
	/* The scanner's idea of the number of scanlines in total */
	true_scanline_count = (buffer_info_block[2]<<8) | buffer_info_block[3]; 

	if ((expected_scanline_size != true_scanline_size) 
			|| (expected_scanline_count != true_scanline_count))
	{
		DBG(10, "init_scan: Warning: Scanner is producing an image "
				"of unexpected size:\n");
		DBG(10, "expected: %i bytes wide, %i scanlines tall.\n", 
				expected_scanline_size, 
				expected_scanline_count);
		DBG(10, "true: %i bytes wide, %i scanlines tall.\n", 
				true_scanline_size, true_scanline_count);

		if (scanp->mode == 0)
			scanp->width = true_scanline_size / 1.25;
		else
			scanp->width = true_scanline_size / 3.75;				

		scanp->height = true_scanline_count;
	}
	return 0;	
}


/* Wake the scanner, detect it, and fill sp with stuff */
int sanei_canon_pp_initialise(scanner_parameters *sp, int mode)
{
	unsigned char scanner_info[12];
	const struct scanner_id *cur_id;
	const struct scanner_hardware_desc *hw;

	/* Hopefully take the scanner out of transparent mode */
	if (sanei_canon_pp_wake_scanner(sp->port, mode))
	{
		DBG(10, "initialise: could not wake scanner\n");
		return 1;
	}

	/* This block of code does something unknown but necessary */
	DBG(50, "initialise: >> scanner_init\n");
	if (sanei_canon_pp_scanner_init(sp->port))
	{
		/* If we're using an unsupported ieee1284 mode here, this is 
		 * where it will fail, so fall back to nibble. */
		sanei_canon_pp_set_ieee1284_mode(M1284_NIBBLE);
		if (sanei_canon_pp_scanner_init(sp->port))
		{
			DBG(10, "initialise: Could not init scanner.\n");
			return 1;
		}
	}
	DBG(50, "initialise: << scanner_init\n");

	/* Read Device ID */
	memset(sp->id_string, 0, sizeof sp->id_string);
	if (send_command(sp->port, cmd_readid, 10, 10000, 100000))
		return -1;
	sanei_canon_pp_read(sp->port, 38, (unsigned char *)(sp->id_string));

	/* Read partially unknown data */
	if (send_command(sp->port, cmd_readinfo, 10, 10000, 100000))
		return -1;
	sanei_canon_pp_read(sp->port, 12, scanner_info);

	if (check8(scanner_info, 12))
	{
		DBG(10, "initialise: Checksum error reading Info Block.\n");
		return 2;
	}

	sp->scanheadwidth = (scanner_info[2] << 8) | scanner_info[3];

	/* Set up various known values */
	cur_id = scanner_id_table;
	while (cur_id->id)
	{
		if (!strncmp(sp->id_string+8, cur_id->id, strlen(cur_id->id)))
			break;
		cur_id++;
	}

	if (cur_id->id)
	{
		hw = cur_id->hw;
	}
	else if (sp->scanheadwidth == 5104) 
	{
		/* Guess 600dpi scanner */
		hw = &hw_alien600;
	}
	else if (sp->scanheadwidth == 2552) 
	{
		/* Guess 300dpi scanner */
		hw = &hw_alien300;
	}
	else
	{
		/* Guinea Pigs :) */
		hw = &hw_alien;
	}

	strcpy(sp->name, hw->name);
	sp->natural_xresolution = hw->natural_xresolution;	
	sp->natural_yresolution = hw->natural_yresolution;	
	sp->scanbedlength = hw->scanbedlength;
	if (hw->scanheadwidth)
		sp->scanheadwidth = hw->scanheadwidth;
	sp->type = hw->type;

	return 0;	
}

/* Shut scanner down */
int sanei_canon_pp_close_scanner(scanner_parameters *sp)
{
	/* Put scanner in transparent mode */	
	sanei_canon_pp_sleep_scanner(sp->port);

	/* Free memory (with purchase of memory of equal or greater value) */
	if (sp->blackweight != NULL) 
	{
		free(sp->blackweight);
		sp->blackweight = NULL;
	}
	if (sp->redweight != NULL)
	{
		free(sp->redweight);
		sp->redweight = NULL;
	}
	if (sp->greenweight != NULL)
	{
		free(sp->greenweight);
		sp->greenweight = NULL;
	}
	if (sp->blueweight != NULL)
	{
		free(sp->blueweight);
		sp->blueweight = NULL;
	}

	return 0;	
}

/* Read the calibration information from file */
int sanei_canon_pp_load_weights(const char *filename, scanner_parameters *sp)
{
	int fd;
	int cal_data_size = sp->scanheadwidth * sizeof(unsigned long);
	int cal_file_size;

	char buffer[10];
	int temp, ret;

	/* Open file */
	if ((fd = open(filename, O_RDONLY)) == -1)
		return -1;

	/* Read header and check it's right */
	ret = safe_read(fd, buffer, strlen(header) + 1);
	if ((ret < 0) || strcmp(buffer, header) != 0)
	{
		DBG(1,"Calibration file header is wrong, recalibrate please\n");
		close(fd);
		return -2;
	}

	/* Read and check file version (the calibrate file 
	   format changes from time to time) */
	ret = safe_read(fd, (char *)&temp, sizeof(int));

	if ((ret < 0) || (temp != fileversion))
	{
		DBG(1,"Calibration file is wrong version, recalibrate please\n");
		close(fd);
		return -3;
	}

	/* Allocate memory for calibration values */
	if (((sp->blueweight = malloc(cal_data_size)) == NULL)
			|| ((sp->redweight = malloc(cal_data_size)) == NULL)
			|| ((sp->greenweight = malloc(cal_data_size)) == NULL)
			|| ((sp->blackweight = malloc(cal_data_size)) == NULL)) 
		return -4;

	/* Read width of calibration data */
	ret = safe_read(fd, (char *)&cal_file_size, sizeof(cal_file_size));

	if ((ret < 0) || (cal_file_size != sp->scanheadwidth))
	{
		DBG(1, "Calibration doesn't match scanner, recalibrate?\n");
		close(fd);
		return -5;
	}

	/* Read calibration data */
	if (safe_read(fd, (char *)(sp->blackweight), cal_data_size) < 0)
	{
		DBG(1, "Error reading black calibration data, recalibrate?\n");
		close(fd);
		return -6;
	}

	if (safe_read(fd, (char *)sp->redweight, cal_data_size) < 0)
	{
		DBG(1, "Error reading red calibration data, recalibrate?\n");
		close(fd);
		return -7;
	}

	if (safe_read(fd, (char *)sp->greenweight, cal_data_size) < 0)
	{
		DBG(1, "Error reading green calibration data, recalibrate?\n");
		close(fd);
		return -8;
	}

	if (safe_read(fd, (char *)sp->blueweight, cal_data_size) < 0)
	{
		DBG(1, "Error reading blue calibration data, recalibrate?\n");
		close(fd);
		return -9;
	}

	/* Read white-balance/gamma data */
	
	if (safe_read(fd, (char *)&(sp->gamma), 32) < 0)
	{
		close(fd);
		return -10;
	}

	close(fd);	

	return 0;
}

/* Mode is 0 for greyscale source data or 1 for RGB */
static void convert_to_rgb(image_segment *dest, unsigned char *src, 
		int width, int scanlines, int mode)
{
	int curline;

	const int colour_size = width * 1.25;
	const int scanline_size = (mode == 0 ? colour_size : colour_size * 3);

	for (curline = 0; curline < scanlines; curline++)
	{

		if (mode == 0) /* Grey */
		{
			convdata(src + (curline * scanline_size), 
					dest->image_data + 
					(curline * width * 2), width, 1);
		}
		else if (mode == 1) /* Truecolour */
		{
			/* Red */
			convdata(src + (curline * scanline_size), 
					dest->image_data + 
					(curline * width *3*2) + 4, width, 2);
			/* Green */
			convdata(src + (curline * scanline_size) + colour_size, 
					dest->image_data + 
					(curline * width *3*2) + 2, width, 2);
			/* Blue */
			convdata(src + (curline * scanline_size) + 
					(2 * colour_size), dest->image_data + 
					(curline * width *3*2), width, 2);
		}

	} /* End of scanline loop */

}

int sanei_canon_pp_read_segment(image_segment **dest, scanner_parameters *sp, 
		scan_parameters *scanp, int scanline_number, int do_adjust,
		int scanlines_left)
{
	unsigned char *input_buffer = NULL;
	image_segment *output_image = NULL;

	unsigned char packet_header[4];
	unsigned char packet_req_command[10];

	int read_data_size;
	int scanline_size;

	if (scanp->mode == 1) /* RGB */
		scanline_size = scanp->width * 3.75;
	else /* Greyscale */
		scanline_size = scanp->width * 1.25;

	read_data_size = scanline_size * scanline_number;

	/* Allocate output_image struct */
	if ((output_image = malloc(sizeof(*output_image))) == NULL)
	{
		DBG(1, "read_segment: Error: Not enough memory for scanner "
				"input buffer\n");
		goto error_out;
	}

	/* Allocate memory for input buffer */
	if ((input_buffer = malloc(scanline_size * scanline_number)) == NULL)
	{
		DBG(1, "read_segment: Error: Not enough memory for scanner "
				"input buffer\n");
		goto error_out;
	}

	output_image->width = scanp->width;
	output_image->height = scanline_number;

	/* Allocate memory for dest image segment */

	output_image->image_data = 
		malloc(output_image->width * output_image->height * 
				(scanp->mode ? 3 : 1) * 2);

	if (output_image->image_data == NULL)
	{
		DBG(1, "read_segment: Error: Not enough memory for "
				"image data\n");
		goto error_out;
	}

	/* Set up packet request command */
	memcpy(packet_req_command, cmd_packet_req, 10);
	packet_req_command[7] = ((read_data_size + 4) & 0xFF00) >> 8;
	packet_req_command[8] = (read_data_size + 4) & 0xFF;

	/* Send packet req. and wait for the scanner's READY signal */
	if (send_command(sp->port, packet_req_command, 10, 9000, 2000000))
	{
		DBG(1, "read_segment: Error: didn't get response within 2s "
				"of sending request");
		goto error_out;
	}

	/* Read packet header */
	if (sanei_canon_pp_read(sp->port, 4, packet_header))
	{
		DBG(1, "read_segment: Error reading packet header\n");
		goto error_out;
	}

	if ((packet_header[2]<<8) + packet_header[3] != read_data_size)
	{
		DBG(1, "read_segment: Error: Expected data size: %i bytes.\n", 
				read_data_size);
		DBG(1, "read_segment: Expecting %i bytes times %i "
				"scanlines.\n", scanline_size, scanline_number);
		DBG(1, "read_segment: Actual data size: %i bytes.\n", 
				(packet_header[2] << 8) + packet_header[3]);
		goto error_out;
	}

	/* Read scanlines_this_packet scanlines into the input buf */
	
	if (sanei_canon_pp_read(sp->port, read_data_size, input_buffer))
	{
		DBG(1, "read_segment: Segment read incorrectly, and we don't "
				"know how to recover.\n");
		goto error_out;
	}

	/* This is the only place we can abort safely - 
	 * between reading one segment and requesting the next one. */
	if (sp->abort_now) goto error_out;

	if (scanlines_left >= (scanline_number * 2)) 
	{
		DBG(100, "read_segment: Speculatively starting more scanning "
				"(%d left)\n", scanlines_left);
		sanei_canon_pp_write(sp->port, 10, packet_req_command);
		/* Don't read status, it's unlikely to be ready *just* yet */
	}

	DBG(100, "read_segment: Convert to RGB\n");
	/* Convert data */
	convert_to_rgb(output_image, input_buffer, scanp->width, 
			scanline_number, scanp->mode);

	/* Adjust pixel readings according to calibration data */
	if (do_adjust) {
		DBG(100, "read_segment: Adjust output\n");
		adjust_output(output_image, scanp, sp);
	}

	/* output */
	*dest = output_image;
	/* finished with this now */
	free(input_buffer);
	return 0;	

	error_out:
	if (output_image && output_image->image_data) 
		free(output_image->image_data);
	if (output_image) free(output_image);
	if (input_buffer) free(input_buffer);
	sp->abort_now = 0;
	return -1;
}

/* 
check8: Calculates the checksum-8 for s bytes pointed to by p.

For messages from the scanner, this should normally end up returning
0, since the last byte of most packets is the value that makes the 
total up to 0 (or 256 if you're left-handed).
Hence, usage: if (check8(buffer, size)) {DBG(10, "checksum error!\n");} 

Can also be used to generate valid checksums for sending to the scanner.
*/
static int check8(unsigned char *p, int s) {
	int total=0,i;
	for(i=0;i<s;i++)
		total-=(signed char)p[i];
	total &=0xFF;
	return total;
}

/* Converts from scanner format -> linear
   width is in pixels, not bytes. */
/* This function could use a rewrite */
static void convdata(unsigned char *srcbuffer, unsigned char *dstbuffer, 
		int width, int mode)
/* This is a tricky (read: crap) function (read: hack) which is why I probably 
   spent more time commenting it than programming it. The thing to remember 
   here is that the scanner uses interpolated scanlines, so it's
   RRRRRRRGGGGGGBBBBBB not RGBRGBRGBRGBRGB. So, the calling function just 
   increments the destination pointer slightly to handle green, then a bit 
   more for blue. If you don't understand, tough. */
{
	int count;
	int i, j, k;

	for (count = 0; count < width; count++)
	{
		/* The scanner stores data in a bizzare butchered 10-bit 
		   format.  I'll try to explain it in 100 words or less:

		   Scanlines are made up of groups of 4 pixels. Each group of 
		   4 is stored inside 5 bytes. The first 4 bytes of the group 
		   contain the lowest 8 bits of one pixel each (in the right 
		   order). The 5th byte contains the most significant 2 bits 
		   of each pixel in the same order. */

		i = srcbuffer[count + (count >> 2)]; /* Low byte for pixel */
		j = srcbuffer[(((count / 4) + 1) * 5) - 1]; /* "5th" byte */
		j = j >> ((count % 4) * 2); /* Get upper 2 bits of intensity */
		j = j & 0x03; /* Can't hurt */
		/* And the final 10-bit pixel value is: */
		k = (j << 8) | i; 

		/* now we return this as a 16 bit value */
		k = k << 6;

		if (mode == 1) /* Scanner -> Grey */
		{
			dstbuffer[count * 2] = HIGH_BYTE(k);
			dstbuffer[(count * 2) + 1] = LOW_BYTE(k);
		}
		else if (mode == 2) /* Scanner -> RGB */
		{
			dstbuffer[count * 3 * 2] = HIGH_BYTE(k);
			dstbuffer[(count * 3 * 2) + 1] = LOW_BYTE(k);			
		}

	}
}

static int adjust_output(image_segment *image, scan_parameters *scanp, 
		scanner_parameters *scannerp)
/* Needing a good cleanup */
{
	/* light and dark points for the CCD sensor in question 
	 * (stored in file as 0-1024, scaled to 0-65536) */
	unsigned long hi, lo;
	/* The result of our calculations */
	unsigned long result;
	unsigned long temp;
	/* The CCD sensor which read the current pixel - this is a tricky value
	   to get right. */
	int ccd, scaled_xoff;
	/* Loop variables */
	unsigned int scanline, pixelnum, colour;
	unsigned long int pixel_address;
	unsigned int cols = scanp->mode ? 3 : 1;

	for (scanline = 0; scanline < image->height; scanline++)
	{
		for (pixelnum = 0; pixelnum < image->width; pixelnum++)
		{
			/* Figure out CCD sensor number */
			/* MAGIC FORMULA ALERT! */
			ccd = (pixelnum << (scannerp->natural_xresolution - 
						scanp->xresolution)) + (1 << 
						(scannerp->natural_xresolution 
						 - scanp->xresolution)) - 1;

			scaled_xoff = scanp->xoffset << 
				(scannerp->natural_xresolution - 
				 scanp->xresolution);

			ccd += scaled_xoff;	

			for (colour = 0; colour < cols; colour++)
			{
				/* Address of pixel under scrutiny */
				pixel_address = 
					(scanline * image->width * cols * 2) +
					(pixelnum * cols * 2) + (colour * 2);

				/* Dark value is easy 
				 * Range of lo is 0-18k */
				lo = (scannerp->blackweight[ccd]) * 3;

				/* Light value depends on the colour, 
				 * and is an average in greyscale mode. */
				if (scanp->mode == 1) /* RGB */
				{
					switch (colour)
					{
						case 0: hi = scannerp->redweight[ccd] * 3; 
							break;
						case 1: hi = scannerp->greenweight[ccd] * 3; 
							break;
						default: hi = scannerp->blueweight[ccd] * 3; 
							 break;
					}
				}
				else /* Grey - scanned using green */
				{
					hi = scannerp->greenweight[ccd] * 3; 
				}

				/* Check for bad calibration data as it 
				   can cause a divide-by-0 error */
				if (hi <= lo)
				{
					DBG(1, "adjust_output: Bad cal data!"
							" hi: %ld lo: %ld\n"
							"Recalibrate, that "
							"should fix it.\n", 
							hi, lo);
					return -1;
				}

				/* Start with the pixel value in result */
				result = MAKE_SHORT(*(image->image_data + 
							pixel_address), 
						*(image->image_data + 
							pixel_address + 1));

				result = result >> 6; /* Range now = 0-1023 */
					/*
				if (scanline == 10)
					DBG(200, "adjust_output: Initial pixel"
							" value: %ld\n", 
							result);
							*/
				result *= 54;         /* Range now = 0-54k */

				/* Clip to dark and light values */
				if (result < lo) result = lo;
				if (result > hi) result = hi;

				/* result = (base-lo) * max_value / (hi-lo) */
				temp = result - lo;
				temp *= 65536;
				temp /= (hi - lo);

				/* Clip output  result has been clipped to lo,
				 * and hi >= lo, so temp can't be < 0 */
				if (temp > 65535)
					temp = 65535;
				/*
				if (scanline == 10)
				{
					DBG(200, "adjust_output: %d: base = "
							"%lu, result %lu (%lu "
							"- %lu)\n", pixelnum, 
							result, temp, lo, hi);
				}			
				*/
				result = temp;

				/* Store the value back where it came 
				 * from (always bigendian) */
				*(image->image_data + pixel_address)
					= HIGH_BYTE(result);
				*(image->image_data + pixel_address+1)
					= LOW_BYTE(result);
			}
		}
	}
	/*DBG(100, "Finished adjusting output\n");*/
	return 0;
}

/* Calibration run.  Aborting allowed at "safe" points where the scanner won't
 * be left in a crap state. */
int sanei_canon_pp_calibrate(scanner_parameters *sp, char *cal_file) 
{
	int count, readnum, colournum, scanlinenum;
	int outfile;

	int scanline_size;

	int scanline_count = 6;
	/* Don't change this unless you also want to change do_adjust */
	const int calibration_reads = 3;

	unsigned char command_buffer[10];

	image_segment image;
	unsigned char *databuf;

	char colours[3][6] = {"Red", "Green", "Blue"};

	/* Calibration data is monochromatic (greyscale format) */
	scanline_size = sp->scanheadwidth * 1.25;

	/* 620P has to be difficult here... */
	if (!(sp->type) ) scanline_count = 8;

	/* Probably shouldn't have to abort *just* yet, but may as well check */
	if (sp->abort_now) return -1;

	DBG(40, "Calibrating %ix%i pixels calibration image "
			"(%i bytes each scan).\n", 
			sp->scanheadwidth, scanline_count, 
			scanline_size * scanline_count);

	/* Allocate memory for calibration data */	
	sp->blackweight = (unsigned long *)
		calloc(sizeof(unsigned long), sp->scanheadwidth);
	sp->redweight = (unsigned long *)
		calloc(sizeof(unsigned long), sp->scanheadwidth);
	sp->greenweight = (unsigned long *)
		calloc(sizeof(unsigned long), sp->scanheadwidth);
	sp->blueweight = (unsigned long *)
		calloc(sizeof(unsigned long), sp->scanheadwidth);

	/* The data buffer needs to hold a number of images (calibration_reads)
	 * per colour, each sp->scanheadwidth x scanline_count */
	databuf = malloc(scanline_size * scanline_count * calibration_reads*3);

	/* And allocate space for converted image data in this image_segment */
	image.image_data = malloc(scanline_count * sp->scanheadwidth * 2 * 
			calibration_reads);
	image.width = sp->scanheadwidth;
	image.height = scanline_count * calibration_reads;

	/* Sending the "dark calibration" command */
	memcpy(command_buffer, cmd_calblack, 10);

	/* Which includes the size of data we expect the scanner to return */
	command_buffer[7] = ((scanline_size * scanline_count) & 0xff00) >> 8;
	command_buffer[8] = (scanline_size * scanline_count) & 0xff;

	DBG(40, "Step 1/3: Calibrating black level...\n");
	for (readnum = 0; readnum < calibration_reads; readnum++)
	{
		DBG(40, "  * Black scan number %d/%d.\n", readnum + 1, 
				calibration_reads);

		if (sp->abort_now) return -1;

		if (send_command(sp->port, command_buffer, 10, 100000, 5000000))
		{
			DBG(1, "Error reading black level!\n");
			free (image.image_data);
			free(databuf);
			return -1;
			
		}

		/* Black reference data */
		sanei_canon_pp_read(sp->port, scanline_size * scanline_count,
				databuf + 
				(readnum * scanline_size * scanline_count));
	}

	/* Convert scanner format to a greyscale 16bpp image */
	for (scanlinenum = 0; 
			scanlinenum < scanline_count * calibration_reads; 
			scanlinenum++)
	{
		convdata(databuf + (scanlinenum * scanline_size), 
				image.image_data + 
				(scanlinenum * sp->scanheadwidth*2), 
				sp->scanheadwidth, 1);
	}

	/* Take column totals */
	for (count = 0; count < sp->scanheadwidth; count++)
	{
		/* Value is normalised as if we took 6 scanlines, even if we 
		 * didn't (620P I'm looking at you!) */
		sp->blackweight[count] = (column_sum(&image, count) * 6) 
			/ scanline_count >> 6;
	}

	/* 620P has to be difficult here... */
	if (!(sp->type) )
	{
		scanline_count = 6;
		image.height = scanline_count * calibration_reads;
	}

	DBG(40, "Step 2/3: Gamma tables...\n");
	DBG(40, "  * Requesting creation of new of gamma tables...\n");
	if (sp->abort_now) return -1;
	if (send_command(sp->port, cmd_cleargamma, 10, 100000, 5000000))
	{
		DBG(1,"Error sending gamma command!\n");
		free (image.image_data);
		free(databuf);
		return -1;
	}

	DBG(20, "  * Snoozing for 15 seconds while the scanner calibrates...");
	usleep(15000000);
	DBG(40, "done.\n");
	
	DBG(40, "  * Requesting gamma table values...");
	if (send_command(sp->port, cmd_readgamma, 10, 100000, 10000000))
	{
		DBG(1,"Error sending gamma table request!\n");
		free (image.image_data);
		free(databuf);
		return -1;
	}
	DBG(40, "done.\n");

	DBG(40, "  * Reading white-balance/gamma data... ");
	sanei_canon_pp_read(sp->port, 32, sp->gamma);
	DBG(40, "done.\n");

	if (sp->abort_now) return -1;

	memcpy(command_buffer, cmd_calcolour, 10);

	/* Set up returned data size */
	command_buffer[7] = ((scanline_size * scanline_count) & 0xff00) >> 8;
	command_buffer[8] = (scanline_size * scanline_count) & 0xff;

	DBG(40, "Step 3/3: Calibrating sensors...\n");
	/* Now for the RGB high-points */
	for (colournum = 1; colournum < 4; colournum++)
	{
		/* Set the colour we want to read */
		command_buffer[3] = colournum;
		for (readnum = 0; readnum < 3; readnum++)
		{
			DBG(10, "  * %s sensors, scan number %d/%d.\n", 
					colours[colournum-1], readnum + 1,
					calibration_reads);

			if (sp->abort_now) return -1;
			if (send_command(sp->port, command_buffer, 10,
						100000, 5000000))
			{
				DBG(1,"Error sending scan request!");
				free (image.image_data);
				free(databuf);
				return -1;
			}

			sanei_canon_pp_read(sp->port, scanline_size * 
					scanline_count, databuf + 
					(readnum * scanline_size * 
					 scanline_count));

		}

		/* Convert colour data from scanner format to RGB data */
		for (scanlinenum = 0; scanlinenum < scanline_count * 
				calibration_reads; scanlinenum++)
		{
			convdata(databuf + (scanlinenum * scanline_size), 
					image.image_data + 
					(scanlinenum * sp->scanheadwidth * 2), 
					sp->scanheadwidth, 1);
		}

		/* Sum each column of the image and store the results in sp */
		for (count = 0; count < sp->scanheadwidth; count++)
		{
			if (colournum == 1)
				sp->redweight[count] = 
					column_sum(&image, count) >> 6;	
			else if (colournum == 2)
				sp->greenweight[count] = 
					column_sum(&image, count) >> 6;	
			else
				sp->blueweight[count] = 
					column_sum(&image, count) >> 6;	
		}

	}

	if (sp->abort_now) return -1;

	/* cal_file == NUL indicates we want an in-memory scan only */
	if (cal_file != NULL)
	{
		DBG(40, "Writing calibration to %s\n", cal_file);
		outfile = open(cal_file, O_WRONLY | O_TRUNC | O_CREAT, 0600);
		if (outfile < 0)
		{
			DBG(10, "Error opening cal file for writing\n");
		}

		/* Header */
		if (safe_write(outfile, header, strlen(header) + 1) < 0)
			DBG(10, "Write error on calibration file %s", cal_file);
		if (safe_write(outfile, (const char *)&fileversion, sizeof(int)) < 0)
			DBG(10, "Write error on calibration file %s", cal_file);

		/* Data */
		if (safe_write(outfile, (char *)&(sp->scanheadwidth), 
					sizeof(sp->scanheadwidth)) < 0)
			DBG(10, "Write error on calibration file %s", cal_file);
		if (safe_write(outfile, (char *)(sp->blackweight), 
					sp->scanheadwidth * sizeof(long)) < 0)
			DBG(10, "Write error on calibration file %s", cal_file);
		if (safe_write(outfile, (char *)(sp->redweight), 
					sp->scanheadwidth * sizeof(long)) < 0)
			DBG(10, "Write error on calibration file %s", cal_file);
		if (safe_write(outfile, (char *)(sp->greenweight), 
					sp->scanheadwidth * sizeof(long)) < 0)
			DBG(10, "Write error on calibration file %s", cal_file);
		if (safe_write(outfile, (char *)(sp->blueweight), 
					sp->scanheadwidth * sizeof(long)) < 0)
			DBG(10, "Write error on calibration file %s", cal_file);
		if (safe_write(outfile, (char *)(sp->gamma), 32) < 0)
			DBG(10, "Write error on calibration file %s", cal_file);

		close(outfile);
	}

	free(databuf);
	free(image.image_data);

	return 0;
}

static unsigned long column_sum(image_segment *image, int x)
/* This gives us a number from 0-n*65535 where n is the height of the image */
{
	unsigned int row, p;
	unsigned long total = 0;

	p = x;
	for (row = 0; row < image->height; row++)
	{
		total+= MAKE_SHORT(image->image_data[2*p],
				image->image_data[2*p+1]);
		p += image->width;
	}
	return total;
}


static int scanner_setup_params(unsigned char *buf, scanner_parameters *sp, 
		scan_parameters *scanp)
{
	int scaled_width, scaled_height;
	int scaled_xoff, scaled_yoff;

	/* Natural resolution (I think) */
	if (sp->scanheadwidth == 2552)
	{
		buf[0] = 0x11; /* 300 | 0x1000 */
		buf[1] = 0x2c;
		buf[2] = 0x11;
		buf[3] = 0x2c;
	} else {
		buf[0] = 0x12; /* 600 | 0x1000*/
		buf[1] = 0x58;
		buf[2] = 0x12;
		buf[3] = 0x58;
	}

	scaled_width = scanp->width << 
		(sp->natural_xresolution - scanp->xresolution);
	/* YO! This needs fixing if we ever use yresolution! */
	scaled_height = scanp->height << 
		(sp->natural_xresolution - scanp->xresolution);
	scaled_xoff = scanp->xoffset << 
		(sp->natural_xresolution - scanp->xresolution);
	scaled_yoff = scanp->yoffset << 
		(sp->natural_xresolution - scanp->xresolution);

	/* Input resolution */
	buf[4] = (((75 << scanp->xresolution) & 0xff00) >> 8) | 0x10;
	buf[5] = (75 << scanp->xresolution) & 0xff;
	/* Interpolated resolution */
	buf[6] = (((75 << scanp->xresolution) & 0xff00) >> 8) | 0x10;;
	buf[7] = (75 << scanp->xresolution) & 0xff;

	/* X offset */
	buf[8] = (scaled_xoff & 0xff000000) >> 24;
	buf[9] = (scaled_xoff & 0xff0000) >> 16;
	buf[10] = (scaled_xoff & 0xff00) >> 8;
	buf[11] = scaled_xoff & 0xff;

	/* Y offset */
	buf[12] = (scaled_yoff & 0xff000000) >> 24;
	buf[13] = (scaled_yoff & 0xff0000) >> 16;
	buf[14] = (scaled_yoff & 0xff00) >> 8;
	buf[15] = scaled_yoff & 0xff;

	/* Width of image to be scanned */	
	buf[16] = (scaled_width & 0xff000000) >> 24;
	buf[17] = (scaled_width & 0xff0000) >> 16;
	buf[18] = (scaled_width & 0xff00) >> 8;
	buf[19] = scaled_width & 0xff;

	/* Height of image to be scanned */
	buf[20] = (scaled_height & 0xff000000) >> 24;
	buf[21] = (scaled_height & 0xff0000) >> 16;
	buf[22] = (scaled_height & 0xff00) >> 8;
	buf[23] = scaled_height & 0xff;


	/* These appear to be the only two colour mode possibilities. 
	   Pure black-and-white mode probably just uses greyscale and
	   then gets its contrast adjusted by the driver. I forget. */
	if (scanp->mode == 1) /* Truecolour */
		buf[24] = 0x08;
	else /* Greyscale */
		buf[24] = 0x04;

	return 0;
}

int sanei_canon_pp_abort_scan(scanner_parameters *sp)
{
	/* The abort command (hopefully) */
	sanei_canon_pp_write(sp->port, 10, cmd_abort);
	sanei_canon_pp_check_status(sp->port);
	return 0;
}

/* adjust_gamma: Upload a gamma profile to the scanner */
int sanei_canon_pp_adjust_gamma(scanner_parameters *sp)
{
	sp->gamma[31] = check8(sp->gamma, 31);
	if (sanei_canon_pp_write(sp->port, 10, cmd_setgamma))
		return -1;
	if (sanei_canon_pp_write(sp->port, 32, sp->gamma))
		return -1;

	return 0;
}

int sanei_canon_pp_sleep_scanner(struct parport *port)
{
	/* *SCANEND Command - puts scanner to sleep */
	sanei_canon_pp_write(port, 10, cmd_scanend);
	sanei_canon_pp_check_status(port);

  ieee1284_terminate(port);

	return 0;
	/* FIXME: I murdered Simon's code here */
	/* expect(port, "Enter Transparent Mode", 0x1f, 0x1f, 1000000); */
}

int sanei_canon_pp_detect(struct parport *port, int mode)
{
	/*int caps;*/
	/* This code needs to detect whether or not a scanner is present on 
	 * the port, quickly and reliably. Fast version of 
	 * sanei_canon_pp_initialise() 
	 *
	 * If this detect returns true, a more comprehensive check will 
	 * be conducted
	 * Return values: 
	 * 0 = scanner present
	 * anything else = scanner not present 
	 * PRE: port is open/unclaimed
	 * POST: port is closed/unclaimed
	 */

	/* port is already open, just need to claim it */

	if (ieee1284_claim(port) != E1284_OK)
	{
		DBG(0,"detect: Unable to claim port\n");
		return 2;
	}
	if (sanei_canon_pp_wake_scanner(port, mode))
	{
		DBG(10, "detect: could not wake scanner\n");
		ieee1284_release(port);
		return 3;
	}

	/* Goodo, sleep (snaps fingers) */
	sanei_canon_pp_sleep_scanner(port);

	ieee1284_release(port);
	/* ieee1284_close(port); */

	return 0;
}

static int send_command(struct parport *port, unsigned char *buf, int bufsize, 
		int delay, int timeout)
/* Sends a command until the scanner says it is ready. 
 * sleeps for delay microsecs between reads
 * returns -1 on error, -2 on timeout */
{
	int retries = 0;

	do
	{
		/* Send command */
		if (sanei_canon_pp_write(port, bufsize, buf))
			return -1;

		/* sleep a bit */
		usleep(delay);
	} while (sanei_canon_pp_check_status(port) && 
			retries++ < (timeout/delay));

	if (retries >= (timeout/delay)) return -2;
	return 0;

}
