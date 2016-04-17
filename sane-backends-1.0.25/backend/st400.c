/*
vim: ts=4 sw=4 noexpandtab
 */

/* st400.c - SANE module for Siemens ST400 flatbed scanner

   Copyright (C) 1999-2000 Ingo Wilken (Ingo.Wilken@informatik.uni-oldenburg.de)

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

   *************************************************************************

   This file implements a SANE backend for the Siemens ST400 flatbed scanner.

   Unfortunately, I have no documentation for this scanner.  All I have
   is an old PC Pascal source and an Amiga C source (derived from the
   Pascal source).  Both are quite primitive, and so is this backend...

   Version numbers of this backend follow SANE version scheme:  The first
   number is SANE's major version (i.e. the version of the SANE API that
   this backend conforms to), the second is the version of this backend.
   Thus, version 1.2 is the second release of this backend for SANE v1.

   1.0 (08 Mar 1999): First public release, for SANE v1.0.0
   1.1 (12 Mar 1999): Fixed some stupid bugs (caused crash if accessed via net).
   1.2 (23 Apr 1999): Oops, got threshold backwards.
                      Tested with SANE 1.0.1.
   1.3 (27 Apr 1999): Seems the separate MODE SELECT to switch the light on
                      is not necessary.  Removed debugging output via syslog,
                      it was only used to track down a bug in saned.  Some
					  minor cleanups.  Removed illegal version check (only
					  frontends should do this).  Made "maxread" and "delay"
					  config options instead of compile-time #define's.
					  Added model check via INQUIRY, and related changes.
   1.4 (29 Jun 1999): New config options to configure scanner models.
					  See st400.conf for details.  These options are only
					  for testing, and will be removed someday.
   1.5 (26 Mar 2000): Check for optnum >= 0.  Fixed some hardcoded paths in
   					  manpage.  ST400 locks up with reads >64KB - added
					  maxread entry to model struct.  Tested with SANE 1.0.2.
   1.6 (08 Apr 2000): Minor cleanups.
   1.7 (18 Dec 2001): Security fix from Tim Waugh.  Dump inquiry data to
					  "$HOME/st400.dump" instead of "/tmp/st400.dump".
*/

#include "../include/sane/config.h"

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/sanei_config.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_scsi.h"
#include "../include/sane/sanei_debug.h"
#ifndef PATH_MAX
#	define PATH_MAX	1024
#endif

#define BACKEND_NAME	st400
#include "../include/sane/sanei_backend.h"

#include "st400.h"

/* supported scanners */
static ST400_Model st400_models[] = {
{ 8, "SIEMENS", 16, "ST 400", 6, 0x200000UL, 65536UL, NULL, "Siemens", "ST400", "flatbed scanner" },
{ 8, "SIEMENS", 16, "ST 800", 6, 0x200000UL, 65536UL, NULL, "Siemens", "ST800", "flatbed scanner" },	/* to be tested */
{ 0, "", 0, "", 6, 0x200000UL, 65536UL, NULL, "Unknown", "untested", "flatbed scanner" },	/* matches anything */

	/* must be last */
	{ 0, NULL, 0, NULL, 0, 0, 0, NULL, NULL, NULL, NULL }
};


static ST400_Device *st400_devices = NULL;
static unsigned int st400_num_devices = 0;
static const SANE_Device **st400_device_array = NULL;
/* The array pointer must stay valid between calls to sane_get_devices().
 * So we cannot modify or deallocate the array when a new device is attached
 * - until the next call to sane_get_devices().  The array_valid bit indicates
 * whether the array is still in sync with the device list or not (if new
 * devices have been attached in the meantime).
 */
static struct {
	unsigned array_valid: 1;
} st400_status = { 0 };
static size_t st400_maxread = 0;
static size_t st400_light_delay = 0;
static int st400_dump_data = 0;

/* SCSI commands */
#define CMD_TEST_UNIT_READY		0x00
#define CMD_INQUIRY				0x12
#define CMD_MODE_SELECT			0x15
#define CMD_RESERVE				0x16
#define CMD_RELEASE				0x17
#define CMD_START_STOP			0x1b
#define CMD_SET_WINDOW			0x24
#define CMD_READ_CAPACITY		0x25	/* get window settings - unused */
#define CMD_READ10				0x28

/* prototypes */
static SANE_Status st400_inquiry( int fd, ST400_Model **modelP );
static SANE_Status st400_sense_handler( int fd, SANE_Byte *result, void *arg );
static SANE_Status st400_attach( const char *devname, ST400_Device **devP );
static SANE_Status st400_attach_one( const char *device );
static void st400_init_options( ST400_Device *dev );
static SANE_Status st400_set_window( ST400_Device *dev );
static SANE_Status st400_wait_ready( int fd );
static SANE_Status st400_cmd6( int fd, SANE_Byte cmd, SANE_Byte ctrl );
static SANE_Status st400_read10( int fd, SANE_Byte *buf, size_t *lenP );
static void st400_reset_options( ST400_Device *dev );

#undef min
#define min(a, b)		((a) < (b) ? (a) : (b))
#define maxval(bpp)		((1<<(bpp))-1)

/* Debugging levels */
#define DERR	0		/* errors */
#define DINFO	1		/* misc information */
#define DSENSE	2		/* SCSI sense */
#define DSCSI	3		/* SCSI commands */
#define DOPT	4		/* option control */
#define DVAR	5		/* important variables */
#define DCODE	6		/* code flow */


/*********************************************************************
 * low-level SCSI functions
 *********************************************************************/

#define set24(m, x)	do {										\
	*((SANE_Byte *)(m)+0) = (SANE_Byte)(((x) >> 16) & 0xff);	\
	*((SANE_Byte *)(m)+1) = (SANE_Byte)(((x) >> 8) & 0xff);		\
	*((SANE_Byte *)(m)+2) = (SANE_Byte)(((x) >> 0) & 0xff);		\
} while(0)
#define set16(m, x)	do {										\
	*((SANE_Byte *)(m)+0) = (SANE_Byte)(((x) >> 8) & 0xff);		\
	*((SANE_Byte *)(m)+1) = (SANE_Byte)(((x) >> 0) & 0xff);		\
} while(0)


static int str_at_offset(char *str, size_t offset, unsigned char *data)
{
	size_t len;

	len = strlen(str);
	return !strncmp((char *)&data[offset], str, len);
}


static SANE_Status
st400_inquiry( int fd, ST400_Model **modelP )
{
	struct { SANE_Byte cmd, lun, reserved[2], tr_len, ctrl; } scsi_cmd;
/*
	struct {
		SANE_Byte devtype, devqual, version;
		SANE_Byte reserved1, additionallength;
		SANE_Byte reserved2[2], flags;
		SANE_Byte vendor[8], product[16], release[4];
		SANE_Byte reserved3[60];
	} inqdata;
*/
	struct { SANE_Byte bytes[96]; } inqdata;

	size_t inqlen;
	SANE_Status status;
	ST400_Model *model;

	inqlen = sizeof(inqdata);
	memset(&scsi_cmd, 0, sizeof(scsi_cmd));
	scsi_cmd.cmd = CMD_INQUIRY;
	scsi_cmd.tr_len = inqlen;

	DBG(DSCSI, "SCSI: sending INQUIRY (%lu bytes)\n", (u_long)inqlen);
	status = sanei_scsi_cmd(fd, &scsi_cmd, sizeof(scsi_cmd), &inqdata, &inqlen);
	DBG(DSCSI, "SCSI: result=%s (%lu bytes)\n", sane_strstatus(status), (u_long)inqlen);
	if( status != SANE_STATUS_GOOD )
		return status;

	if( st400_dump_data ) {
		const char *home = getenv ("HOME");
		char basename[] = "st400.dump";
		char *name;
		FILE *fp;

		if (home) {
			name = malloc (strlen (home) + sizeof (basename) + 1);
			sprintf (name, "%s/%s", home, basename);
		} else name = basename;

		fp = fopen(name, "ab");
		if( fp != NULL ) {
			fwrite(inqdata.bytes, 1, inqlen, fp);
			fclose(fp);
		}

		if (name != basename)
			free (name);
	}

	if( inqlen != sizeof(inqdata) )
		return SANE_STATUS_IO_ERROR;

	for( model = st400_models; model->inq_vendor; model++ ) {
		if( str_at_offset(model->inq_vendor, model->inq_voffset, inqdata.bytes) && str_at_offset(model->inq_model, model->inq_moffset, inqdata.bytes) ) {
			*modelP = model;
			DBG(DINFO, "found matching scanner model \"%s %s\" in list\n", model->sane_vendor, model->sane_model);
			return SANE_STATUS_GOOD;
		}
	}

	return SANE_STATUS_UNSUPPORTED;
}

static SANE_Status
st400_cmd6( int fd, SANE_Byte cmd, SANE_Byte ctrl )
{
	struct { SANE_Byte cmd, lun, reserved[2], tr_len, ctrl; } scsi_cmd;
	SANE_Status status;

	memset(&scsi_cmd, 0, sizeof(scsi_cmd));
	scsi_cmd.cmd = cmd;
	scsi_cmd.ctrl = ctrl;

	DBG(DSCSI, "SCSI: sending cmd6 0x%02x (ctrl=%d)\n", (int)cmd, (int)ctrl);
	status = sanei_scsi_cmd(fd, &scsi_cmd, sizeof(scsi_cmd), 0, 0);
	DBG(DSCSI, "SCSI: result=%s\n", sane_strstatus(status));

	return status;
}

#define st400_test_ready(fd)	st400_cmd6(fd, CMD_TEST_UNIT_READY, 0)
#define st400_reserve(fd)		st400_cmd6(fd, CMD_RESERVE, 0)
#define st400_release(fd)		st400_cmd6(fd, CMD_RELEASE, 0)
#define st400_start_scan(fd)	st400_cmd6(fd, CMD_START_STOP, 0)
#define st400_light_on(fd)		st400_cmd6(fd, CMD_MODE_SELECT, 0x80)
#define st400_light_off(fd)		st400_cmd6(fd, CMD_MODE_SELECT, 0)


static SANE_Status
st400_wait_ready( int fd )
{
#define SLEEPTIME	100000L		/* 100ms */
	long max_sleep = 60000000L;	/* 60 seconds */
	SANE_Status status;

	DBG(DCODE, "st400_wait_ready(%d)\n", fd);

	while(1) {
		status = st400_test_ready(fd);
		switch( status ) {
			case SANE_STATUS_DEVICE_BUSY:
				if( max_sleep > 0 ) {
					usleep(SLEEPTIME);	/* retry after 100ms */
					max_sleep -= SLEEPTIME;
					break;
				}
				/* else fall through */
			default:
				DBG(DERR, "st400_wait_ready: failed, error=%s\n", sane_strstatus(status));
				/* fall through */
			case SANE_STATUS_GOOD:
				return status;
		}
	}
	/*NOTREACHED*/
}


static SANE_Status
st400_set_window( ST400_Device *dev )
{
	unsigned short xoff, yoff;
	SANE_Byte th;

	struct {
		/* 10byte command */
		SANE_Byte cmd, lun, reserved1[4], tr_len[3], ctrl;

		/* 40byte window struct */
		SANE_Byte reserved2[6], wd_len[2], winnr, reserved3;
		SANE_Byte x_res[2], y_res[2];			/* resolution: 200, 300, 400 */
		SANE_Byte x_ul[2], y_ul[2];				/* upper left corner */
		SANE_Byte width[2], height[2];
		SANE_Byte reserved4, threshold;
		SANE_Byte reserved5, halftone;			/* ht: 0 or 2 */
		SANE_Byte bitsperpixel, reserved6[13];	/* bpp: 1 or 8 */
	} scsi_cmd;
	/* The PC/Amiga source uses reserved5 to indicate A4/A5 paper size
	 * (values 4 and 5), but a comment implies that this is only for the
	 * scanning program and the value is ignored by the scanner.
	 */
	SANE_Status status;

	memset(&scsi_cmd, 0, sizeof(scsi_cmd));
	scsi_cmd.cmd = CMD_SET_WINDOW;
	set24(scsi_cmd.tr_len, 40);
	set16(scsi_cmd.wd_len, 32);

	/* These offsets seem to be required to avoid damaging the scanner:
	 * If a scan with 0/0 as the top left corner is started, the scanner
	 * seems to try to move the carriage over the bottom end (not a
	 * pretty sound).
	 */
	xoff = (11L * dev->val[OPT_RESOLUTION]) / 100;
	yoff = 6;
	th = (double)maxval(dev->model->bits) * SANE_UNFIX(dev->val[OPT_THRESHOLD]) / 100.0;

	scsi_cmd.winnr = 1;
	set16(scsi_cmd.x_res, (unsigned short)dev->val[OPT_RESOLUTION]);
	set16(scsi_cmd.y_res, (unsigned short)dev->val[OPT_RESOLUTION]);
	set16(scsi_cmd.x_ul, dev->x + xoff);
	set16(scsi_cmd.y_ul, dev->wy + yoff);
	set16(scsi_cmd.width, dev->w);
	set16(scsi_cmd.height, dev->wh);
	scsi_cmd.threshold = th;
	scsi_cmd.halftone = (dev->val[OPT_DEPTH] == 1) ? 0 : 2;
	scsi_cmd.bitsperpixel = dev->val[OPT_DEPTH];

	DBG(DSCSI, "SCSI: sending SET_WINDOW (x=%hu y=%hu w=%hu h=%hu wy=%hu wh=%hu th=%d\n", dev->x, dev->y, dev->w, dev->h, dev->wy, dev->wh, (int)th);
	status = sanei_scsi_cmd(dev->fd, &scsi_cmd, sizeof(scsi_cmd), 0, 0);
	DBG(DSCSI, "SCSI: result=%s\n", sane_strstatus(status));

	return status;
}

static SANE_Status
st400_read10( int fd, SANE_Byte *buf, size_t *lenP )
{
	struct { SANE_Byte cmd, lun, res[4], tr_len[3], ctrl; } scsi_cmd;
	SANE_Status status;

	memset(&scsi_cmd, 0, sizeof(scsi_cmd));
	scsi_cmd.cmd = CMD_READ10;
	set24(scsi_cmd.tr_len, *lenP);

	DBG(DSCSI, "SCSI: sending READ10 (%lu bytes)\n", (u_long)(*lenP));
	status = sanei_scsi_cmd(fd, &scsi_cmd, sizeof(scsi_cmd), buf, lenP);
	DBG(DSCSI, "SCSI: result=%s (%lu bytes)\n", sane_strstatus(status), (u_long)(*lenP));

	return status;
}

static SANE_Status
st400_fill_scanner_buffer( ST400_Device *dev )
{
	SANE_Status status;

	DBG(DCODE, "st400_fill_scanner_buffer(%p)\n", (void *) dev);

	if( dev->lines_to_read == 0 )
		dev->status.eof = 1;
	if( dev->status.eof )
		return SANE_STATUS_EOF;

	dev->wh = dev->model->bufsize / dev->params.bytes_per_line;
	if( dev->wh > dev->lines_to_read )
		dev->wh = dev->lines_to_read;
	DBG(DVAR, "dev->wh = %hu\n", dev->wh);

	status = st400_set_window(dev);
	if( status != SANE_STATUS_GOOD )
		return status;

	status = st400_start_scan(dev->fd);
	if( status != SANE_STATUS_GOOD )
		return status;

	dev->wy += dev->wh;
	dev->lines_to_read -= dev->wh;
	dev->bytes_in_scanner = dev->wh * dev->params.bytes_per_line;

	return SANE_STATUS_GOOD;
}

static SANE_Status
st400_fill_backend_buffer( ST400_Device *dev )
{
	size_t r;
	SANE_Status status;

	DBG(DCODE, "st400_fill_backend_buffer(%p)\n", (void *) dev);

	if( dev->bytes_in_scanner == 0 ) {
		status = st400_fill_scanner_buffer(dev);
		if( status != SANE_STATUS_GOOD )
			return status;
	}

	r = min(dev->bufsize, dev->bytes_in_scanner);
	status = st400_read10(dev->fd, dev->buffer, &r);
	if( status == SANE_STATUS_GOOD ) {
		dev->bufp = dev->buffer;
		dev->bytes_in_buffer = r;
		dev->bytes_in_scanner -= r;

		if( r == 0 )
			dev->status.eof = 1;
	}

	return status;
}

static SANE_Status
st400_sense_handler( int fd, SANE_Byte *result, void *arg )
{
	/* ST400_Device *dev = arg; */
	SANE_Status status;

	fd = fd;
	arg = arg; /* silence compilation warnings */

	switch( result[0] & 0x0f ) {
		case 0x0:
			status = SANE_STATUS_GOOD;
			break;
		case 0x1:
			DBG(DSENSE, "SCSI: sense RECOVERED_ERROR\n");
			status = SANE_STATUS_GOOD;	/* ?? */
			break;
		case 0x2:
			DBG(DSENSE, "SCSI: sense NOT_READY\n");
			status = SANE_STATUS_DEVICE_BUSY;
			break;
		case 0x4:
			DBG(DSENSE, "SCSI: sense HARDWARE_ERROR\n");
			status = SANE_STATUS_IO_ERROR;
			break;
		case 0x5:
			DBG(DSENSE, "SCSI: sense ILLEGAL_REQUEST\n");
			status = SANE_STATUS_IO_ERROR;
			break;
		case 0x6:
			DBG(DSENSE, "SCSI: sense UNIT_ATTENTION\n");
			status = SANE_STATUS_DEVICE_BUSY;
			break;
		case 0xb:
			DBG(DSENSE, "SCSI: sense ABORTED_COMMAND\n");
			status = SANE_STATUS_CANCELLED;	/* ?? */
			break;
		default:
			DBG(DSENSE, "SCSI: sense unknown (%d)\n", result[0] & 0x0f);
			status = SANE_STATUS_IO_ERROR;
			break;
	}
	return status;
}


/*********************************************************************
 * Sane initializing stuff
 *********************************************************************/

static SANE_Status
st400_attach( const char *devname, ST400_Device **devP )
{
	ST400_Device *dev;
	ST400_Model *model;
	SANE_Status status;
	int fd;

	DBG(DCODE, "st400_attach(%s, %p)\n", devname, (void *) devP);
	if( devP )
		*devP = NULL;

	for( dev = st400_devices; dev != NULL; dev = dev->next ) {
		if( strcmp(dev->sane.name, devname) == 0 ) {
			if( devP )
				*devP = dev;
			DBG(DCODE, "st400_attach: found device in list\n");
			return SANE_STATUS_GOOD;
		}
	}

	dev = calloc(1, sizeof(*dev));
	if( !dev )
		return SANE_STATUS_NO_MEM;
	DBG(DCODE, "st400_attach: new device struct at %p\n", (void *) dev);

	status = sanei_scsi_open(devname, &fd, st400_sense_handler, dev);
	if( status == SANE_STATUS_GOOD ) {
		status = st400_inquiry(fd, &model);
		if( status == SANE_STATUS_GOOD )
			status = st400_test_ready(fd);
		sanei_scsi_close(fd);
	}
	if( status != SANE_STATUS_GOOD ) {
		free(dev);
		return status;
	}

	/* initialize device structure */
	dev->sane.name = strdup(devname);
	if( !dev->sane.name ) {
		free(dev);
		return SANE_STATUS_NO_MEM;
	}
	dev->sane.vendor = model->sane_vendor;
	dev->sane.model = model->sane_model;
	dev->sane.type = model->sane_type;
	dev->status.open = 0;
	dev->status.scanning = 0;
	dev->status.eof = 0;
	dev->fd = -1;
	dev->buffer = NULL;
	dev->model = model;

	st400_init_options(dev);

	DBG(DCODE, "st400_attach: everything ok, adding device to list\n");

	dev->next = st400_devices;
	st400_devices = dev;
	++st400_num_devices;
	st400_status.array_valid = 0;

	if( devP )
		*devP = dev;
	return SANE_STATUS_GOOD;
}

static SANE_Status
st400_attach_one( const char *device )
{
	DBG(DCODE, "st400_attach_one(%s)\n", device);
	return st400_attach(device, NULL);
}

static SANE_Status
st400_config_get_arg(char **optP, unsigned long *argP, size_t linenum)
{
	int n;

	linenum = linenum; /* silence compilation warnings */

	if( sscanf(*optP, "%lu%n", argP, &n) == 1 ) {
		*optP += n;
		*optP = (char *)sanei_config_skip_whitespace(*optP);
		return SANE_STATUS_GOOD;
	}
	return SANE_STATUS_INVAL;
}


static SANE_Status
st400_config_get_single_arg(char *opt, unsigned long *argP, size_t linenum)
{
	int n;

	if( sscanf(opt, "%lu%n", argP, &n) == 1 ) {
		opt += n;
		opt = (char *)sanei_config_skip_whitespace(opt);
		if( *opt == '\0' )
			return SANE_STATUS_GOOD;
		else {
			DBG(DERR, "extraneous arguments at line %lu: %s\n", (u_long)linenum, opt);
			return SANE_STATUS_INVAL;
		}
	}
	DBG(DERR, "invalid option argument at line %lu: %s\n", (u_long)linenum, opt);
	return SANE_STATUS_INVAL;
}


static SANE_Status
st400_config_do_option(char *opt, size_t linenum)
{
	unsigned long arg;
	SANE_Status status;
	int i;

	status = SANE_STATUS_GOOD;

	opt = (char *)sanei_config_skip_whitespace(opt);
	if( strncmp(opt, "maxread", 7) == 0 && isspace(opt[7]) ) {
		opt += 8;
		status = st400_config_get_single_arg(opt, &arg, linenum);
		if( status == SANE_STATUS_GOOD ) {
			if( arg == 0 )
				st400_maxread = sanei_scsi_max_request_size;
			else
				st400_maxread = (size_t)arg;
		}
	}
	else
	if( strncmp(opt, "delay", 5) == 0 && isspace(opt[5]) ) {
		opt += 6;
		status = st400_config_get_single_arg(opt, &arg, linenum);
		if( status == SANE_STATUS_GOOD )
			st400_light_delay = (size_t)arg;
	}
	else
	if( strncmp(opt, "scanner_bufsize", 15) == 0 && isspace(opt[15]) ) {
		opt += 16;
		status = st400_config_get_single_arg(opt, &arg, linenum);
		if( status == SANE_STATUS_GOOD )
			if( st400_devices )
				st400_devices->model->bufsize = arg;	/* FIXME: changes bufsize for all scanners of this model! */
	}
	else
	if( strncmp(opt, "scanner_bits", 12) == 0 && isspace(opt[12]) ) {
		opt += 13;
		status = st400_config_get_single_arg(opt, &arg, linenum);
		if( status == SANE_STATUS_GOOD )
			if( st400_devices )
				st400_devices->model->bits = arg;	/* FIXME */
	}
	else
	if( strncmp(opt, "scanner_maxread", 15) == 0 && isspace(opt[15]) ) {
		opt += 16;
		status = st400_config_get_single_arg(opt, &arg, linenum);
		if( status == SANE_STATUS_GOOD )
			if( st400_devices )
				st400_devices->model->maxread = arg;	/* FIXME */
	}
	else
	if( strncmp(opt, "scanner_resolutions", 19) == 0 && isspace(opt[19]) ) {
		opt += 20;
		st400_devices->model->dpi_list = malloc(16 * sizeof(SANE_Int));
		i = 0;
		do {
			status = st400_config_get_arg(&opt, &arg, linenum);
			if( status == SANE_STATUS_GOOD ) {
				++i;
				st400_devices->model->dpi_list[i] = (SANE_Int)arg;
			}
		} while( status == SANE_STATUS_GOOD && i < 15 );
		st400_devices->model->dpi_list[0] = i;
		DBG(DINFO, "%d entries for resolution\n", i);
		status = SANE_STATUS_GOOD;
	}
	else
	if( strncmp(opt, "dump_inquiry", 12) == 0 ) {
		st400_dump_data = 1;
	}
	if( st400_devices )
		st400_reset_options(st400_devices);
	return status;
}

SANE_Status
sane_init( SANE_Int *versionP, SANE_Auth_Callback authorize )
{
	FILE *fp;
	SANE_Status status;

	DBG_INIT();
	DBG(DCODE, "sane_init: version %s null, authorize %s null\n", (versionP) ? "!=" : "==", (authorize) ? "!=" : "==");

	if( versionP != NULL )
		*versionP = SANE_VERSION_CODE(SANE_CURRENT_MAJOR, V_MINOR, 0);

	status = SANE_STATUS_GOOD;
	if( (fp = sanei_config_open(ST400_CONFIG_FILE)) != NULL ) {
		char line[PATH_MAX], *str;
		size_t len, linenum;

		linenum = 0;
		DBG(DCODE, "sane_init: reading config file\n");
		while( sanei_config_read(line, sizeof(line), fp) ) {
			++linenum;
			str = line;
			if( str[0] == '#' )
				continue;	/* ignore comments */
			str = (char *)sanei_config_skip_whitespace(str);
			len = strlen(str);
			if( !len )
				continue;	/* ignore empty lines */
			if( strncmp(str, "option", 6) == 0 && isspace(str[6]) ) {
				DBG(DCODE, "sane_init: config line <%s>\n", line);
				status = st400_config_do_option(str+7, linenum);
			}
			else {
				DBG(DCODE, "sane_init: attaching device <%s>\n", line);
				sanei_config_attach_matching_devices(line, st400_attach_one);
			}
			if( status != SANE_STATUS_GOOD )
				break;
		}
		DBG(DCODE, "sane_init: closing config file\n");
		fclose(fp);
	}

	if( status == SANE_STATUS_GOOD && st400_devices == NULL ) {
		DBG(DCODE, "sane_init: attaching default device <%s>\n", ST400_DEFAULT_DEVICE);
		sanei_config_attach_matching_devices(ST400_DEFAULT_DEVICE, st400_attach_one);
	}

	return status;
}

void
sane_exit( void )
{
	ST400_Device *dev;

	DBG(DCODE, "sane_exit()\n");

	while( (dev = st400_devices) != NULL ) {
		st400_devices = dev->next;

		sane_close(dev);
		free((char *)(dev->sane.name));
		free(dev);
	}
	st400_num_devices = 0;
	if( st400_device_array ) {
		DBG(DCODE, "sane_exit: freeing device array\n");
		free(st400_device_array);
		st400_device_array = NULL;
		st400_status.array_valid = 0;
	}
}

SANE_Status
sane_get_devices( const SANE_Device ***devarrayP, SANE_Bool local_only )
{
	ST400_Device *dev;
	unsigned int i;

	DBG(DCODE, "sane_get_devices(%p, %d)\n", (void *) devarrayP, (int)local_only);

	if( !st400_status.array_valid ) {
		if( st400_device_array ) {
			DBG(DCODE, "sane_get_devices: freeing old device array\n");
			free(st400_device_array);
		}
		st400_device_array = malloc((st400_num_devices + 1) * sizeof(*st400_device_array));
		if( !st400_device_array )
			return SANE_STATUS_NO_MEM;
		DBG(DCODE, "sane_get_devices: new device array at %p\n", (void *) st400_device_array);

		dev = st400_devices;
		for( i = 0; i < st400_num_devices; i++ ) {
			st400_device_array[i] = &dev->sane;
			dev = dev->next;
		}
		st400_device_array[st400_num_devices] = NULL;
		st400_status.array_valid = 1;
	}
	DBG(DCODE, "sane_get_devices: %u entries in device array\n", st400_num_devices);
	if( devarrayP )
		*devarrayP = st400_device_array;
	return SANE_STATUS_GOOD;
}


SANE_Status
sane_open( SANE_String_Const devicename, SANE_Handle *handleP )
{
	ST400_Device *dev;
	SANE_Status status;

	DBG(DCODE, "sane_open(%s, %p)\n", devicename, (void *) handleP);

	*handleP = NULL;
	if( devicename && devicename[0] ) {
		status = st400_attach(devicename, &dev);
		if( status != SANE_STATUS_GOOD )
			return status;
	}
	else
		dev = st400_devices;

	if( !dev )
		return SANE_STATUS_INVAL;

	if( dev->status.open )
		return SANE_STATUS_DEVICE_BUSY;

	dev->status.open = 1;
	st400_reset_options(dev);
	*handleP = (SANE_Handle)dev;

	return SANE_STATUS_GOOD;
}


void
sane_close( SANE_Handle handle )
{
	ST400_Device *dev = handle;

	DBG(DCODE, "sane_close(%p)\n", handle);

	if( dev->status.open ) {
		sane_cancel(dev);
		dev->status.open = 0;
	}
}


/*
 * options
 */
static void
st400_reset_options( ST400_Device *dev )
{
	DBG(DCODE, "st400_reset_options(%p)\n", (void *) dev);

	dev->val[OPT_NUM_OPTS]	= NUM_OPTIONS;
	dev->val[OPT_RESOLUTION]	= dev->opt[OPT_RESOLUTION].constraint.word_list[1];
	dev->val[OPT_DEPTH]		= dev->opt[OPT_DEPTH].constraint.word_list[1];
    dev->val[OPT_THRESHOLD]	= SANE_FIX(50.0);
	dev->opt[OPT_THRESHOLD].cap &= ~SANE_CAP_INACTIVE;
    dev->val[OPT_TL_X]		= SANE_FIX(0.0);
	dev->val[OPT_TL_Y]		= SANE_FIX(0.0);
	dev->val[OPT_BR_X]		= SANE_FIX(0.0);
	dev->val[OPT_BR_Y]		= SANE_FIX(0.0);

	if( dev->model->dpi_list )
		dev->opt[OPT_RESOLUTION].constraint.word_list = dev->model->dpi_list;
}

static void
st400_init_options( ST400_Device *dev )
{
	static const SANE_Int depth_list[] = { 2, 1, 8 };
	static const SANE_Int dpi_list[] = { 3, 200, 300, 400 };
	static const SANE_Range thres_range = {
		SANE_FIX(0.0), SANE_FIX(100.0), SANE_FIX(0.0)
	};
	static const SANE_Range x_range = {
		SANE_FIX(0.0), SANE_FIX(ST400_MAX_X * MM_PER_INCH), SANE_FIX(0.0)
	};
	static const SANE_Range y_range = {
		SANE_FIX(0.0), SANE_FIX(ST400_MAX_Y * MM_PER_INCH), SANE_FIX(0.0)
	};

	DBG(DCODE, "st400_init_options(%p)\n", (void *)dev);

	dev->opt[OPT_NUM_OPTS].name	= SANE_NAME_NUM_OPTIONS;
	dev->opt[OPT_NUM_OPTS].title	= SANE_TITLE_NUM_OPTIONS;
	dev->opt[OPT_NUM_OPTS].desc		= SANE_DESC_NUM_OPTIONS;
	dev->opt[OPT_NUM_OPTS].type	= SANE_TYPE_INT;
	dev->opt[OPT_NUM_OPTS].unit	= SANE_UNIT_NONE;
	dev->opt[OPT_NUM_OPTS].size = sizeof(SANE_Word);
	dev->opt[OPT_NUM_OPTS].cap	= SANE_CAP_SOFT_DETECT;
	dev->opt[OPT_NUM_OPTS].constraint_type = SANE_CONSTRAINT_NONE;

	dev->opt[OPT_MODE_GROUP].title= "Scan Mode";
	dev->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;

	dev->opt[OPT_RESOLUTION].name	= SANE_NAME_SCAN_RESOLUTION;
	dev->opt[OPT_RESOLUTION].title= SANE_TITLE_SCAN_RESOLUTION;
	dev->opt[OPT_RESOLUTION].desc	= SANE_DESC_SCAN_RESOLUTION;
	dev->opt[OPT_RESOLUTION].type	= SANE_TYPE_INT;
	dev->opt[OPT_RESOLUTION].unit	= SANE_UNIT_DPI;
	dev->opt[OPT_RESOLUTION].size = sizeof(SANE_Word);
	dev->opt[OPT_RESOLUTION].cap	= SANE_CAP_SOFT_SELECT|SANE_CAP_SOFT_DETECT;
	dev->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
	dev->opt[OPT_RESOLUTION].constraint.word_list = dpi_list;

	dev->opt[OPT_DEPTH].name	= SANE_NAME_BIT_DEPTH;
	dev->opt[OPT_DEPTH].title	= SANE_TITLE_BIT_DEPTH;
	dev->opt[OPT_DEPTH].desc	= SANE_DESC_BIT_DEPTH;
	dev->opt[OPT_DEPTH].type	= SANE_TYPE_INT;
	dev->opt[OPT_DEPTH].unit	= SANE_UNIT_BIT;
	dev->opt[OPT_DEPTH].size	= sizeof(SANE_Word);
	dev->opt[OPT_DEPTH].cap	= SANE_CAP_SOFT_SELECT|SANE_CAP_SOFT_DETECT;
	dev->opt[OPT_DEPTH].constraint_type = SANE_CONSTRAINT_WORD_LIST;
	dev->opt[OPT_DEPTH].constraint.word_list = depth_list;

	dev->opt[OPT_THRESHOLD].name	= SANE_NAME_THRESHOLD;
	dev->opt[OPT_THRESHOLD].title	= SANE_TITLE_THRESHOLD;
	dev->opt[OPT_THRESHOLD].desc	= SANE_DESC_THRESHOLD;
	dev->opt[OPT_THRESHOLD].type	= SANE_TYPE_FIXED;
	dev->opt[OPT_THRESHOLD].unit	= SANE_UNIT_PERCENT;
	dev->opt[OPT_THRESHOLD].size	= sizeof(SANE_Word);
	dev->opt[OPT_THRESHOLD].cap	= SANE_CAP_SOFT_SELECT|SANE_CAP_SOFT_DETECT;
	dev->opt[OPT_THRESHOLD].constraint_type = SANE_CONSTRAINT_RANGE;
	dev->opt[OPT_THRESHOLD].constraint.range = &thres_range;

	dev->opt[OPT_GEOMETRY_GROUP].title= "Geometry";
	dev->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;

	dev->opt[OPT_TL_X].name	= SANE_NAME_SCAN_TL_X;
	dev->opt[OPT_TL_X].title	= SANE_TITLE_SCAN_TL_X;
	dev->opt[OPT_TL_X].desc	= SANE_DESC_SCAN_TL_X;
	dev->opt[OPT_TL_X].type	= SANE_TYPE_FIXED;
	dev->opt[OPT_TL_X].unit	= SANE_UNIT_MM;
	dev->opt[OPT_TL_X].size	= sizeof(SANE_Word);
	dev->opt[OPT_TL_X].cap	= SANE_CAP_SOFT_SELECT|SANE_CAP_SOFT_DETECT;
	dev->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
	dev->opt[OPT_TL_X].constraint.range = &x_range;

	dev->opt[OPT_TL_Y].name	= SANE_NAME_SCAN_TL_Y;
	dev->opt[OPT_TL_Y].title	= SANE_TITLE_SCAN_TL_Y;
	dev->opt[OPT_TL_Y].desc	= SANE_DESC_SCAN_TL_Y;
	dev->opt[OPT_TL_Y].type	= SANE_TYPE_FIXED;
	dev->opt[OPT_TL_Y].unit	= SANE_UNIT_MM;
	dev->opt[OPT_TL_Y].size	= sizeof(SANE_Word);
	dev->opt[OPT_TL_Y].cap	= SANE_CAP_SOFT_SELECT|SANE_CAP_SOFT_DETECT;
	dev->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
	dev->opt[OPT_TL_Y].constraint.range = &y_range;

	dev->opt[OPT_BR_X].name	= SANE_NAME_SCAN_BR_X;
	dev->opt[OPT_BR_X].title	= SANE_TITLE_SCAN_BR_X;
	dev->opt[OPT_BR_X].desc	= SANE_DESC_SCAN_BR_X;
	dev->opt[OPT_BR_X].type	= SANE_TYPE_FIXED;
	dev->opt[OPT_BR_X].unit	= SANE_UNIT_MM;
	dev->opt[OPT_BR_X].size	= sizeof(SANE_Word);
	dev->opt[OPT_BR_X].cap	= SANE_CAP_SOFT_SELECT|SANE_CAP_SOFT_DETECT;
	dev->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
	dev->opt[OPT_BR_X].constraint.range = &x_range;

	dev->opt[OPT_BR_Y].name	= SANE_NAME_SCAN_BR_Y;
	dev->opt[OPT_BR_Y].title	= SANE_TITLE_SCAN_BR_Y;
	dev->opt[OPT_BR_Y].desc	= SANE_DESC_SCAN_BR_Y;
	dev->opt[OPT_BR_Y].type	= SANE_TYPE_FIXED;
	dev->opt[OPT_BR_Y].unit	= SANE_UNIT_MM;
	dev->opt[OPT_BR_Y].size	= sizeof(SANE_Word);
	dev->opt[OPT_BR_Y].cap	= SANE_CAP_SOFT_SELECT|SANE_CAP_SOFT_DETECT;
	dev->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
	dev->opt[OPT_BR_Y].constraint.range = &y_range;

	st400_reset_options(dev);
}

const SANE_Option_Descriptor *
sane_get_option_descriptor( SANE_Handle handle, SANE_Int optnum )
{
	ST400_Device *dev = handle;

	DBG(DOPT, "sane_get_option_descriptor(%p, %d)\n", handle, (int)optnum);

	if( dev->status.open && optnum >= 0 && optnum < NUM_OPTIONS )
		return &dev->opt[optnum];

	return NULL;
}

SANE_Status
sane_control_option( SANE_Handle handle, SANE_Int optnum,
	SANE_Action action, void *valP, SANE_Int *infoP)
{
	ST400_Device *dev = handle;
	SANE_Status status;

	DBG(DCODE, "sane_control_option(%p, %d, %d, %p, %p)\n", (void *) handle, (int)optnum, (int)action, valP, (void *) infoP);

	if( infoP )
		*infoP = 0;

	if( !dev->status.open )
		return SANE_STATUS_INVAL;
	if( dev->status.scanning )
		return SANE_STATUS_DEVICE_BUSY;

	if( optnum < 0 || optnum >= NUM_OPTIONS )
		return SANE_STATUS_INVAL;

	switch( action ) {
		case SANE_ACTION_GET_VALUE:

			DBG(DOPT, "getting option %d (value=%d)\n", (int)optnum, (int)dev->val[optnum]);

			switch( optnum ) {
				case OPT_NUM_OPTS:
				case OPT_RESOLUTION:
				case OPT_DEPTH:
				case OPT_THRESHOLD:
				case OPT_TL_X:
				case OPT_TL_Y:
				case OPT_BR_X:
				case OPT_BR_Y:
					*(SANE_Word *)valP = dev->val[optnum];
					break;
				default:
					return SANE_STATUS_INVAL;
			}
			break;

		case SANE_ACTION_SET_VALUE:
			if( !SANE_OPTION_IS_SETTABLE(dev->opt[optnum].cap) )
				return SANE_STATUS_INVAL;
			status = sanei_constrain_value(&dev->opt[optnum], valP, infoP);
			if( status != SANE_STATUS_GOOD )
				return status;

			DBG(DOPT, "setting option %d to %d\n", (int)optnum, (int)*(SANE_Word *)valP);

			switch( optnum ) {
				case OPT_DEPTH:
					if( *(SANE_Word *)valP != 1 )
						dev->opt[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
					else
						dev->opt[OPT_THRESHOLD].cap &= ~SANE_CAP_INACTIVE;
					if( infoP )
						*infoP |= SANE_INFO_RELOAD_OPTIONS;
					/* fall through */
				case OPT_RESOLUTION:
				case OPT_TL_X:
				case OPT_TL_Y:
				case OPT_BR_X:
				case OPT_BR_Y:
					if( infoP )
						*infoP |= SANE_INFO_RELOAD_PARAMS;
					/* fall through */
				case OPT_THRESHOLD:
					dev->val[optnum] = *(SANE_Word *)valP;
					break;
				default:
					return SANE_STATUS_INVAL;
			}
			break;

		case SANE_ACTION_SET_AUTO:

			DBG(DOPT, "automatic option setting\n");

			return SANE_STATUS_UNSUPPORTED;

		default:
			return SANE_STATUS_INVAL;
	}
	return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_parameters( SANE_Handle handle, SANE_Parameters *paramsP )
{
	ST400_Device *dev = handle;

	DBG(DCODE, "sane_get_parameters(%p, %p)\n", handle, (void *) paramsP);

	if( !dev->status.open )
		return SANE_STATUS_INVAL;

	if( !dev->status.scanning ) {
		double width, height, dpi;

		dev->params.format = SANE_FRAME_GRAY;
		dev->params.last_frame = SANE_TRUE;
		dev->params.lines = 0;
		dev->params.depth = dev->val[OPT_DEPTH];

		width  = SANE_UNFIX(dev->val[OPT_BR_X] - dev->val[OPT_TL_X]);
		height = SANE_UNFIX(dev->val[OPT_BR_Y] - dev->val[OPT_TL_Y]);
		dpi = dev->val[OPT_RESOLUTION];

		/* make best-effort guess at what parameters will look like once
		   scanning starts.  */
		if( dpi > 0.0  &&  width > 0.0  &&  height > 0.0 ) {
			double dots_per_mm = dpi / MM_PER_INCH;

			dev->params.pixels_per_line = width * dots_per_mm + 0.5;
			dev->params.lines = height * dots_per_mm + 0.5;

			if( dev->params.depth == 1 ) {
				/* Pad to an even multiple of 8.  This way we can simply
				 * copy the bytes from the scanner to the SANE buffer
				 * (only need to invert them).
				 */
				dev->params.pixels_per_line += 7;
				dev->params.pixels_per_line &= ~7;

				/*dev->params.bytes_per_line = (dev->params.pixels_per_line + 7)/8;*/
				dev->params.bytes_per_line = dev->params.pixels_per_line/8;
			}
			else
				dev->params.bytes_per_line = dev->params.pixels_per_line;

			dev->x = SANE_UNFIX(dev->val[OPT_TL_X]) * dots_per_mm + 0.5;
			dev->y = SANE_UNFIX(dev->val[OPT_TL_Y]) * dots_per_mm + 0.5;
			dev->w = dev->params.pixels_per_line;
			dev->h = dev->params.lines;

			DBG(DVAR, "parameters: bpl=%d, x=%hu, y=%hu, w=%hu, h=%hu\n", (int)dev->params.bytes_per_line, dev->x, dev->y, dev->w, dev->h);
		}
	}

	if( paramsP )
		*paramsP = dev->params;
	return SANE_STATUS_GOOD;
}


SANE_Status
sane_start( SANE_Handle handle )
{
	ST400_Device *dev = handle;
	SANE_Status status;

	DBG(DCODE, "sane_start(%p)\n", handle);

	if( !dev->status.open )
		return SANE_STATUS_INVAL;
	if( dev->status.scanning )
		return SANE_STATUS_DEVICE_BUSY;

	status = sane_get_parameters(dev, NULL);
	if( status != SANE_STATUS_GOOD )
		return status;

	if( !dev->buffer ) {
		if( st400_maxread > 0 )
			dev->bufsize = min(st400_maxread, (unsigned int) sanei_scsi_max_request_size);
		else
		if( dev->model->maxread > 0 )
			dev->bufsize = min(dev->model->maxread, (unsigned int) sanei_scsi_max_request_size);
		else
			dev->bufsize = sanei_scsi_max_request_size;
		DBG(DVAR, "allocating %lu bytes buffer\n", (u_long)dev->bufsize);
		dev->buffer = malloc(dev->bufsize);
		if( !dev->buffer )
			return SANE_STATUS_NO_MEM;
	}
	dev->bufp = dev->buffer;
	dev->bytes_in_buffer = 0;

	if( dev->fd < 0 ) {
		status = sanei_scsi_open(dev->sane.name, &dev->fd, st400_sense_handler, dev);
		if( status != SANE_STATUS_GOOD )
			goto return_error;
	}

	dev->status.eof = 0;

	status = st400_wait_ready(dev->fd);
	if( status != SANE_STATUS_GOOD )
		goto close_and_return;

	status = st400_reserve(dev->fd);
	if( status != SANE_STATUS_GOOD )
		goto close_and_return;

	if( st400_light_delay > 0 ) {
		status = st400_light_on(dev->fd);
		if( status != SANE_STATUS_GOOD )
			goto release_and_return;
		usleep(st400_light_delay * 100000);	/* 1/10 seconds */
	}

	dev->wy = dev->y;
	dev->lines_to_read = dev->h;
	dev->bytes_in_scanner = 0;

	status = st400_fill_scanner_buffer(dev);
	if( status != SANE_STATUS_GOOD )
		goto lightoff_and_return;

	/* everything ok */
	dev->status.scanning = 1;
	return SANE_STATUS_GOOD;

lightoff_and_return:
	if( st400_light_delay )
		st400_light_off(dev->fd);
release_and_return:
	st400_release(dev->fd);
close_and_return:
	sanei_scsi_close(dev->fd);
return_error:
	dev->fd = -1;
	return status;
}

void
sane_cancel( SANE_Handle handle )
{
	ST400_Device *dev = handle;

	DBG(DCODE, "sane_cancel(%p)\n", handle);

	if( dev->status.scanning ) {
#if 0
		st400_stop_scan(dev->fd);
#endif
		if( st400_light_delay ) 
			st400_light_off(dev->fd);
		st400_release(dev->fd);
		sanei_scsi_close(dev->fd);
		dev->status.scanning = 0;
		dev->fd = -1;
	}
	if( dev->buffer ) {
		free(dev->buffer);
		dev->buffer = NULL;
	}
}


SANE_Status
sane_read( SANE_Handle handle, SANE_Byte *buf, SANE_Int maxlen, SANE_Int *lenP )
{
	ST400_Device *dev = handle;
	SANE_Status status;
	size_t r, i;
	SANE_Byte val;

	DBG(DCODE, "sane_read(%p, %p, %d, %p)\n", handle, buf, (int)maxlen, (void *) lenP);

	*lenP = 0;
	if( !dev->status.scanning )
		return SANE_STATUS_INVAL;
	if( dev->status.eof )
		return SANE_STATUS_EOF;

	status = SANE_STATUS_GOOD;
	while( maxlen > 0 ) {
		if( dev->bytes_in_buffer == 0 ) {
			status = st400_fill_backend_buffer(dev);
			if( status == SANE_STATUS_EOF )
				return SANE_STATUS_GOOD;
			if( status != SANE_STATUS_GOOD ) {
				*lenP = 0;
				return status;
			}
		}

		r = min((SANE_Int) dev->bytes_in_buffer, maxlen);

		if( dev->val[OPT_DEPTH] == 1 || dev->model->bits == 8 ) {
			/* This is simple.  We made sure the scanning are is aligned to
			 * 8 pixels (see sane_get_parameters()), so we can simply copy
			 * the stuff - only need to invert it.
			 */
			for( i = 0; i < r; i++ )
				*buf++ = ~(*dev->bufp++);
		}
		else {
			SANE_Byte mv;

			/* The scanner sends bytes with 6bit-values (0..63), where 0 means
			 * white.  To convert to 8bit, we invert the values (so 0 means
			 * black) and then shift them two bits to the left and replicate
			 * the most- significant bits in the lowest two bits of the
			 * 8bit-value:
			 *     bit-pattern x x 5 4 3 2 1 0  becomes  5 4 3 2 1 0 5 4
			 * This is more accurate than simply shifting the values two bits
			 * to the left (e.g. 6bit-white 00111111 gets converted to 8bit-
			 * white 11111111 instead of almost-white 11111100) and is still
			 * reasonably fast.
			 */
			mv = (SANE_Byte)maxval(dev->model->bits);

			/* Note: this works with any bit depth <= 8 */
			for( i = 0; i < r; i++ ) {
				val = mv - *dev->bufp++;
				val <<= (8 - dev->model->bits);
				val += (val >> dev->model->bits);
				*buf++ = val;
			}
		}
		maxlen -= r;
		dev->bytes_in_buffer -= r;
		*lenP += r;
	}
	return status;
}


/*********************************************************************
 * Advanced functions (not supported)
 *********************************************************************/

SANE_Status
sane_set_io_mode( SANE_Handle handle, SANE_Bool nonblock )
{
	DBG(DCODE, "sane_set_io_mode(%p, %d)\n", handle, (int)nonblock);

	if( nonblock == SANE_TRUE )
		return SANE_STATUS_UNSUPPORTED;
	return SANE_STATUS_GOOD;
}

SANE_Status
sane_get_select_fd( SANE_Handle handle, SANE_Int *fdP )
{
	DBG(DCODE, "sane_get_select_fd(%p, %p)\n", handle, (void *) fdP);

	return SANE_STATUS_UNSUPPORTED;
}
/* The End */
