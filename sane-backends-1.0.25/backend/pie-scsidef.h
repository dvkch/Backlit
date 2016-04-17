/* ----------------------------------------------------------------------------- */

/* sane - Scanner Access Now Easy.

    pie-scsidef.h: scsi-definiton header file for PIE scanner driver.

    Copyright (C) 2000 Simon Munton, based on the umax-scsidef.h by Oliver Rauch & Michael Johnson

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

/* --------------------------------------------------------------------------------------------------------- */


#ifndef PIE_SCSIDEF_H
#define PIE_SCSIDEF_H



/* --------------------------------------------------------------------------------------------------------- */

/* I'm using functions derived from Eric Youngdale's scsiinfo
 * program here for dealing with parts of SCSI commands.
 */

static inline void setbitfield(unsigned char * pageaddr, int mask, int shift, int val)
{ *pageaddr = (*pageaddr & ~(mask << shift)) | ((val & mask) << shift); }

static inline void resetbitfield(unsigned char * pageaddr, int mask, int shift, int val)
{ *pageaddr = (*pageaddr & ~(mask << shift)) | (((!val) & mask) << shift); }

static inline int getbitfield(unsigned char * pageaddr, int mask, int shift)
{ return ((*pageaddr >> shift) & mask); }

/* ------------------------------------------------------------------------- */

static inline int getnbyte(unsigned char * pnt, int nbytes)
{
 unsigned int result = 0;
 int i;

  for(i=0; i<nbytes; i++)
    result = (result << 8) | (pnt[i] & 0xff);
  return result;
}

/* ------------------------------------------------------------------------- */

static inline int getnbyte1(unsigned char * pnt, int nbytes)
{
 unsigned int result = 0;
 int i;

  for(i=nbytes-1; i >= 0; i--)
    result = (result << 8) | (pnt[i] & 0xff);
  return result;
}

/* ------------------------------------------------------------------------- */

static inline void putnbyte(unsigned char * pnt, unsigned int value, unsigned int nbytes)
{
  int i;

  for(i=nbytes-1; i>= 0; i--)
  {
    pnt[i] = value & 0xff;
    value = value >> 8;
  }
}


/* ------------------------------------------------------------------------- */

static inline void putnbyte1(unsigned char * pnt, unsigned int value, unsigned int nbytes)
{
  unsigned int i;

  for(i=0; i< nbytes; i++)
  {
    pnt[i] = value & 0xff;
    value = value >> 8;
  }
}


/* --------------------------------------------------------------------------------------------------------- */


/* Not all of these are defined in scsi.h, so we'll make sure
 * we agree about them here...
 */
#define TEST_UNIT_READY         0x00
#define REQUEST_SENSE           0x03
#define INQUIRY                 0x12
#define RESERVE_UNIT            0x16
#define RELEASE_UNIT            0x17
#define SCAN                    0x1B
#define READ			0x08
#define WRITE			0x0A
#define PARAM			0x0F
#define MODE			0x15

/* --------------------------------------------------------------------------------------------------------- */


#define STD_WDB_LEN 0x28					     /* wdb_len if nothing is set by inquiry */


/* --------------------------------------------------------------------------------------------------------- */


/* SCSI commands */

typedef struct
{
  unsigned char *cmd;
  size_t size;
} scsiblk;


/* --------------------------------------------------------------------------------------------------------- */


#define set_inquiry_return_size(icb,val)			icb[0x04]=val
static unsigned char inquiryC[] = { INQUIRY, 0x00, 0x00, 0x00, 0x7c, 0x00 };
static scsiblk inquiry = { inquiryC, sizeof(inquiryC) };


/* --------------------------------------------------------------------------------------------------------- */


#define get_inquiry_periph_qual(in)				getbitfield(in, 0x07, 5)
#  define IN_periph_qual_lun		0x00
#  define IN_periph_qual_nolun		0x03
#define get_inquiry_periph_devtype(in)				getbitfield(in, 0x1f, 0)
#  define IN_periph_devtype_scanner	0x06
#  define IN_periph_devtype_unknown	0x1f

#define get_inquiry_rmb(in)					getbitfield(in + 0x01, 0x01, 7)
#define get_inquiry_0x01_bit6(in)				getbitfield(in + 0x01, 0x01, 6)
#define get_inquiry_0x01_bit5(in)				getbitfield(in + 0x01, 0x01, 5)

#define get_inquiry_iso_version(in)				getbitfield(in + 0x02, 0x03, 6)
#define get_inquiry_ecma_version(in)				getbitfield(in + 0x02, 0x07, 3)
#define get_inquiry_ansi_version(in)				getbitfield(in + 0x02, 0x07, 0)

#define get_inquiry_aenc(in)					getbitfield(in + 0x03, 0x01, 7)
#define get_inquiry_tmiop(in)					getbitfield(in + 0x03, 0x01, 6)
#define get_inquiry_0x03_bit5(in)				getbitfield(in + 0x03, 0x01, 5)
#define get_inquiry_0x03_bit4(in)				getbitfield(in + 0x03, 0x01, 4)
#define get_inquiry_response_format(in)				getbitfield(in + 0x03, 0x0f, 0)
#  define IN_recognized			0x02

#define get_inquiry_additional_length(in)			in[0x04]
#define set_inquiry_length(out,n)				out[0x04]=n-5

#define get_inquiry_vendor(in, buf)				strncpy(buf, in + 0x08, 0x08)
#define get_inquiry_product(in, buf)				strncpy(buf, in + 0x10, 0x010)
#define get_inquiry_version(in, buf)				strncpy(buf, in + 0x20, 0x04)

#define get_inquiry_max_x_res(in)				getnbyte1(in + 0x24, 2)
#define get_inquiry_max_y_res(in)				getnbyte1(in + 0x26, 2)
#define get_inquiry_fb_max_scan_width(in)			getnbyte1(in + 0x28, 2)
#define get_inquiry_fb_max_scan_length(in)			getnbyte1(in + 0x2a, 2)
#define get_inquiry_filters(in)				        in[0x2c]
#define get_inquiry_color_depths(in)				in[0x2d]
#define get_inquiry_color_format(in)				in[0x2e]
#define get_inquiry_image_format(in)				in[0x30]
#define get_inquiry_scan_capability(in)				in[0x31]
#define get_inquiry_optional_devices(in)			in[0x32]
#define get_inquiry_enhancements(in)				in[0x33]
#define get_inquiry_gamma_bits(in)				in[0x34]
#define get_inquiry_last_filter(in)			        in[0x35]
#define get_inquiry_fast_preview_res(in)			getnbyte1(in + 0x36, 2)
#define get_inquiry_halftones(in)				in[0x60]
#define get_inquiry_halftone_max_width(in)			in[0x61]
#define get_inquiry_halftone_max_heighgt(in)			in[0x62]
#define get_inquiry_max_windows(in)				in[0x63]
#define get_inquiry_min_highlight(in)				in[0x65]
#define get_inquiry_max_shadow(in)				in[0x66]
#define get_inquiry_cal_eqn(in)				        in[0x67]
#define get_inquiry_max_exp(in)				        getnbyte1(in + 0x68, 2)
#define get_inquiry_min_exp(in)				        getnbyte1(in + 0x6a, 2)
#define get_inquiry_trans_x1(in)				getnbyte1(in + 0x6c, 2)
#define get_inquiry_trans_y1(in)				getnbyte1(in + 0x6e, 2)
#define get_inquiry_trans_x2(in)				getnbyte1(in + 0x70, 2)
#define get_inquiry_trans_y2(in)				getnbyte1(in + 0x72, 2)

#define INQ_ONE_PASS_COLOR          0x80
#define INQ_FILTER_BLUE             0x08
#define INQ_FILTER_GREEN            0x04
#define INQ_FILTER_RED              0x02
#define INQ_FILTER_NEUTRAL          0x01

#define INQ_COLOR_DEPTH_16          0x20
#define INQ_COLOR_DEPTH_12          0x10
#define INQ_COLOR_DEPTH_10          0x08
#define INQ_COLOR_DEPTH_8           0x04
#define INQ_COLOR_DEPTH_4           0x02
#define INQ_COLOR_DEPTH_1           0x01

#define INQ_COLOR_FORMAT_INDEX      0x04
#define INQ_COLOR_FORMAT_LINE       0x02
#define INQ_COLOR_FORMAT_PIXEL      0x01

#define INQ_IMG_FMT_OKLINE          0x08
#define INQ_IMG_FMT_BLK_ONE         0x04
#define INQ_IMG_FMT_MOTOROLA        0x02
#define INQ_IMG_FMT_INTEL           0x01

#define INQ_CAP_PWRSAV              0x80
#define INQ_CAP_EXT_CAL             0x40
#define INQ_CAP_FAST_PREVIEW        0x10
#define INQ_CAP_DISABLE_CAL         0x08
#define INQ_CAP_SPEEDS              0x07

#define INQ_OPT_DEV_MPCL            0x80
#define INQ_OPT_DEV_TP1             0x04
#define INQ_OPT_DEV_TP              0x02
#define INQ_OPT_DEV_ADF             0x01

#define INQ_ENHANCE_EDGE            0x02

#define INQ_LAST_FILTER_BLUE        0x08
#define INQ_LAST_FILTER_GREEN       0x04
#define INQ_LAST_FILTER_RED         0x02
#define INQ_LAST_FILTER_NEUTRAL     0x01

#define INQ_DWNLD_HALFTONE          0x80
#define INQ_NUM_HALFTONES           0x7f


/* --------------------------------------------------------------------------------------------------------- */


static unsigned char test_unit_readyC[] = { TEST_UNIT_READY, 0x00, 0x00, 0x00, 0x00, 0x00 };

static scsiblk test_unit_ready = { test_unit_readyC,sizeof(test_unit_readyC) };


/* --------------------------------------------------------------------------------------------------------- */


static unsigned char reserve_unitC[] = { RESERVE_UNIT, 0x00, 0x00, 0x00, 0x00, 0x00 };

static scsiblk reserve_unit = { reserve_unitC, sizeof(reserve_unitC) };


/* --------------------------------------------------------------------------------------------------------- */


static unsigned char release_unitC[] = { RELEASE_UNIT, 0x00, 0x00, 0x00, 0x00, 0x00 };

static scsiblk release_unit = { release_unitC, sizeof(release_unitC) };


/* --------------------------------------------------------------------------------------------------------- */


static unsigned char paramC[] = { PARAM, 0x00, 0x00, 0x00, 0x00, 0x00 };

static scsiblk param = { paramC, sizeof(paramC) };

#define set_param_length(in, l)		putnbyte(in + 3, (l), 2)

#define get_param_scan_width(b)			getnbyte1(b, 2)
#define get_param_scan_lines(b)			getnbyte1(b + 2, 2)
#define get_param_scan_bytes(b)			getnbyte1(b + 4, 2)
#define get_param_scan_filter_offset1(b)	b[6]
#define get_param_scan_filter_offset2(b)	b[7]
#define get_param_scan_period(b)		getnbyte1(b + 8, 4)
#define get_param_scsi_xfer_rate(b)		getnbyte1(b + 12, 2)
#define get_param_scan_available_lines(b)	getnbyte1(b + 14, 2)

/* --------------------------------------------------------------------------------------------------------- */


static unsigned char writeC[] = { WRITE, 0x00, 0x00, 0x00, 0x00, 0x00 };

static scsiblk swrite = { writeC, sizeof(writeC) };

#define set_write_length(in, l)		putnbyte(in + 2, (l), 3)


/* --------------------------------------------------------------------------------------------------------- */


static unsigned char modeC[] = { MODE, 0x00, 0x00, 0x00, 0x00, 0x00 };

static scsiblk smode = { modeC, sizeof(modeC) };

#define set_mode_length(in, l)		putnbyte(in + 3, (l), 2)

/* --------------------------------------------------------------------------------------------------------- */


static unsigned char scanC[] = { SCAN, 0x00, 0x00, 0x00, 0x01, 0x00 };

static scsiblk scan = { scanC, sizeof(scanC) };

#define set_scan_cmd(in, l)		in[4] = l

/* --------------------------------------------------------------------------------------------------------- */


/* sread instead of read because read is a libc primitive */
static unsigned char sreadC[] = { READ, 0x00, 0x00, 0x00, 0x00, 0x00 };

static scsiblk sread = { sreadC, sizeof(sreadC) };

#define set_read_length(in, l)		putnbyte(in + 2, (l), 3)

/* --------------------------------------------------------------------------------------------------------- */

#if 0
static unsigned char request_senseC[] = { REQUEST_SENSE, 0x00, 0x00, 0x00, 0x00, 0x00 };
#define set_RS_allocation_length(sb,val)		sb[0x04]=val
#define set_RS_LUN(sb,val)				setbitfield(sb + 0x01, 7, 5) /* ??? */

static scsiblk request_sense = { request_senseC, sizeof(request_senseC) };
#endif

/* defines for request sense return block */
#define get_RS_information_valid(b)			getbitfield(b + 0x00, 1, 7)
#define get_RS_error_code(b)				getbitfield(b + 0x00, 0x7f, 0)
#define get_RS_filemark(b)				getbitfield(b + 0x02, 1, 7)
#define get_RS_EOM(b)					getbitfield(b + 0x02, 1, 6)
#define get_RS_ILI(b)					getbitfield(b + 0x02, 1, 5)
#define get_RS_sense_key(b)				getbitfield(b + 0x02, 0x0f, 0) 
#define get_RS_information(b)				getnbyte(b+0x03, 4) 
#define get_RS_additional_length(b)			b[0x07]
#define get_RS_ASC(b)					b[0x0c]
#define get_RS_ASCQ(b)					b[0x0d]
#define get_RS_SKSV(b)					getbitfield(b+0x0f,1,7)   /* valid */ 
#define get_RS_CD(b)					getbitfield(b+0x0f,1,6)   /* 1=CDB */
#define get_RS_field_pointer(b)				getnbyte(b+0x10, 2)

#define get_RS_additional_sense(b)			getnbyte(b+0x12, 2)

#define rs_return_block_size		0x1f


/* --------------------------------------------------------------------------------------------------------- */


static char *sense_str[] = {"NO SENSE",
                            "RECOVERED ERROR",
                            "NOT READY",
                            "MEDIUM ERROR",
                            "HARDWARE ERROR",
                            "ILLEGAL REQUEST",
                            "UNIT ATTENTION",
                            "DATA PROTECT",
                            "BLANK CHECK",
                            "VENDOR SPECIFIC",
			    "COPY ABORTED",
                            "ABORTED COMMAND",
                            "EQUAL",
                            "VOLUME OVERFLOW",
                            "MISCOMPARE",
                            "??? - SENSE 0FH" };

/* --------------------------------------------------------------------------------------------------------- */

/* command codes used in the data part of a SCSI write command */

#define SET_POWER_SAVE_CONTROL      0x01
#define DWNLD_GAMMA_TABLE           0x10
#define DWNLD_HALFTONE              0x11
#define SET_SCAN_FRAME              0x12
#define SET_EXP_TIME                0x13
#define SET_HIGHLIGHT_SHADOW        0x14
#define SEND_CAL_DATA               0x16

#define READ_POWER_SAVE_CONTROL     0x81
#define READ_GAMMA_TABLE            0x90
#define READ_HALFTONE               0x91
#define READ_SCAN_FRAME             0x92
#define READ_EXP_TIME               0x93
#define READ_HIGHLIGHT_SHADOW       0x94
#define READ_CAL_INFO               0x95


#define set_command(in, cmd)        putnbyte1(in, cmd, 2)
#define set_data_length(in, len)    putnbyte1(in + 2, len, 2)
#define set_data(in, ofs, val, num) putnbyte1(in + ofs, val, num)



#define FILTER_BLUE		    0x08
#define FILTER_GREEN		    0x04
#define FILTER_RED		    0x02
#define FILTER_NEUTRAL		    0x01


#endif
