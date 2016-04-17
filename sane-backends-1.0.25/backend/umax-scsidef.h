/* --------------------------------------------------------------------------------------------------------- */

/* umax-scsidef.h: scsi-definiton header file for UMAX scanner driver.

    Copyright (C) 1996-1997 Michael K. Johnson
    Copyright (C) 1997-2002 Oliver Rauch

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


#ifndef UMAX_SCSIDEF_H
#define UMAX_SCSIDEF_H

#include "umax.h"


/* --------------------------------------------------------------------------------------------------------- */

/* I'm using functions derived from Eric Youngdale's scsiinfo
 * program here for dealing with parts of SCSI commands.
 */

static inline void setbitfield(unsigned char * pageaddr, int mask, int shift, int val) \
{ *pageaddr = (*pageaddr & ~(mask << shift)) | ((val & mask) << shift); }

static inline void resetbitfield(unsigned char * pageaddr, int mask, int shift, int val) \
{ *pageaddr = (*pageaddr & ~(mask << shift)) | (((!val) & mask) << shift); }

static inline int getbitfield(unsigned char * pageaddr, int mask, int shift) \
{ return ((*pageaddr >> shift) & mask); }

/* ------------------------------------------------------------------------- */

static inline int getnbyte(unsigned char * pnt, int nbytes) \
{
 unsigned int result = 0;
 int i;

  for(i=0; i<nbytes; i++)
    result = (result << 8) | (pnt[i] & 0xff);
  return result;
}

/* ------------------------------------------------------------------------- */

static inline void putnbyte(unsigned char * pnt, unsigned int value, unsigned int nbytes) \
{
  int i;

  for(i=nbytes-1; i>= 0; i--) \
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
#define SET_WINDOW              0x24
#define READ                    0x28
#define SEND                    0x2A
#define OBJECT_POSITION         0x31
#define GET_DATA_BUFFER_STATUS  0x34
#undef  WRITE_BUFFER							 /* correct write_buffer for scanner */
#define WRITE_BUFFER            0x3B
#define GET_LAMP_STATUS         0x5E
#define SET_LAMP_STATUS         0x5F

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
static unsigned char inquiryC[] = { INQUIRY, 0x00, 0x02, 0x00, 0xff, 0x00 };
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
#define get_inquiry_CBHS(in)					getbitfield(in + 0x01, 0x03, 3)
#define set_inquiry_CBHS(in, val)				setbitfield(in + 0x01, 0x03, 3, val)
#  define IN_CBHS_50			0x00
#  define IN_CBHS_255			0x01
#  define IN_CBHS_auto			0x02
#define get_inquiry_translamp(in)				getbitfield(in + 0x01, 0x01, 2)
#define get_inquiry_transavail(in)				getbitfield(in + 0x01, 0x01, 1)
#define get_inquiry_scanmode(in)				getbitfield(in + 0x01, 0x01, 0)
#  define IN_flatbed			0x00
#  define IN_adf			0x01

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

#define get_inquiry_0x05(in)					in[0x05]
#define get_inquiry_0x06(in)					in[0x06]

#define get_inquiry_scsi_byte(in)				in[0x07]
#define get_inquiry_scsi_reladr(in)				getbitfield(in + 0x07, 0x01, 7)
#define get_inquiry_scsi_wbus32(in)				getbitfield(in + 0x07, 0x01, 6)
#define get_inquiry_scsi_wbus16(in)				getbitfield(in + 0x07, 0x01, 5)
#define get_inquiry_scsi_sync(in)				getbitfield(in + 0x07, 0x01, 4)
#define get_inquiry_scsi_linked(in)				getbitfield(in + 0x07, 0x01, 3)
#define get_inquiry_scsi_R(in)					getbitfield(in + 0x07, 0x01, 2)
#define get_inquiry_scsi_cmdqueue(in)				getbitfield(in + 0x07, 0x01, 1)
#define get_inquiry_scsi_sftre(in)				getbitfield(in + 0x07, 0x01, 0)

#define get_inquiry_vendor(in, buf)				strncpy(buf, in + 0x08, 0x08)
#define get_inquiry_product(in, buf)				strncpy(buf, in + 0x10, 0x010)
#define get_inquiry_version(in, buf)				strncpy(buf, in + 0x20, 0x04)

#define set_inquiry_fw_quality(in, val)				setbitfield(in + 0x24, 1, 0, val)
#define get_inquiry_fw_quality(in)				getbitfield(in + 0x24, 1, 0)
#define get_inquiry_fw_fast_preview(in)				getbitfield(in + 0x24, 1, 1)
#define get_inquiry_fw_shadow_comp(in)				getbitfield(in + 0x24, 1, 2)
#define get_inquiry_fw_reselection(in)				getbitfield(in + 0x24, 1, 3)
#define get_inquiry_fw_lamp_int_cont(in)			getbitfield(in + 0x24, 1, 4)
#define get_inquiry_fw_batch_scan(in)				getbitfield(in + 0x24, 1, 5)
#define get_inquiry_fw_calibration(in)				getbitfield(in + 0x24, 1, 6)
#define get_inquiry_fw_adjust_exposure_tf(in)			getbitfield(in + 0x24, 1, 7)

#define get_inquiry_exposure_time_step_unit(in)			in[0x25]
#define get_inquiry_exposure_time_max(in)			getnbyte(in + 0x26, 2)
#define get_inquiry_exposure_time_lhg_min(in)			in[0x2a]
#define get_inquiry_exposure_time_color_min(in)			in[0x2b]
#define get_inquiry_exposure_time_lh_def_fb(in)			in[0x2c]
#define get_inquiry_exposure_time_lh_def_uta(in)		in[0x2d]
#define get_inquiry_exposure_time_gray_def_fb(in)		in[0x2e]
#define get_inquiry_exposure_time_gray_def_uta(in)		in[0x2f]
#define get_inquiry_exposure_time_def_r_fb(in)			in[0x30]
#define get_inquiry_exposure_time_def_g_fb(in)			in[0x31]
#define get_inquiry_exposure_time_def_b_fb(in)			in[0x32]
#define get_inquiry_exposure_time_def_r_uta(in)			in[0x33]
#define get_inquiry_exposure_time_def_g_uta(in)			in[0x34]
#define get_inquiry_exposure_time_def_b_uta(in)			in[0x35]

#define get_inquiry_0x36(in)					in[0x36]
#define get_inquiry_0x37(in)					in[0x37]

/* bytes 0x38 - 0x5f reserved by SCSI */

#define get_inquiry_sc_feature_byte0(in)			in[0x60]
#define get_inquiry_sc_three_pass_color(in)			getbitfield(in + 0x60, 1, 0)
#define get_inquiry_sc_one_pass_color(in)			getbitfield(in + 0x60, 1, 1)
#define get_inquiry_sc_lineart(in)				getbitfield(in + 0x60, 1, 2)
#define get_inquiry_sc_halftone(in)				getbitfield(in + 0x60, 1, 3)
#define get_inquiry_sc_gray(in)					getbitfield(in + 0x60, 1, 4)
#define get_inquiry_sc_color(in)				getbitfield(in + 0x60, 1, 5)
#define get_inquiry_sc_uta(in)					getbitfield(in + 0x60, 1, 6)
#define get_inquiry_sc_adf(in)					getbitfield(in + 0x60, 1, 7)

#define set_inquiry_sc_three_pass_color(in,val)			setbitfield(in + 0x60, 1, 0, val)
#define set_inquiry_sc_one_pass_color(in,val)			setbitfield(in + 0x60, 1, 1, val)
#define set_inquiry_sc_lineart(in,val)				setbitfield(in + 0x60, 1, 2, val)
#define set_inquiry_sc_halftone(in,val)				setbitfield(in + 0x60, 1, 3, val)
#define set_inquiry_sc_gray(in,val)				setbitfield(in + 0x60, 1, 4, val)
#define set_inquiry_sc_color(in,val)				setbitfield(in + 0x60, 1, 5, val)
#define set_inquiry_sc_uta(in,val)				setbitfield(in + 0x60, 1, 6, val)
#define set_inquiry_sc_adf(in,val)				setbitfield(in + 0x60, 1, 7, val)

#define get_inquiry_sc_feature_byte1(in)			in[0x61]
#define get_inquiry_sc_double_res(in)				getbitfield(in + 0x61, 1, 0)
#define get_inquiry_sc_high_byte_first(in)			getbitfield(in + 0x61, 1, 1)
#define get_inquiry_sc_bi_image_reverse(in)			getbitfield(in + 0x61, 1, 2)
#define get_inquiry_sc_multi_image_reverse(in)			getbitfield(in + 0x61, 1, 3)
#define get_inquiry_sc_no_shadow(in)				getbitfield(in + 0x61, 1, 4)
#define get_inquiry_sc_no_highlight(in)				getbitfield(in + 0x61, 1, 5)
#define get_inquiry_sc_downloadable_fw(in)			getbitfield(in + 0x61, 1, 6)
#define get_inquiry_sc_paper_length_14(in)			getbitfield(in + 0x61, 1, 7)

#define get_inquiry_sc_feature_byte2(in)			in[0x62]
#define get_inquiry_sc_uploadable_shade(in)			getbitfield(in + 0x62, 1, 0)
#define get_inquiry_fw_support_color(in)			getbitfield(in + 0x62, 1, 1)
#define get_inquiry_analog_gamma(in)				getbitfield(in + 0x62, 1, 2)
#define get_inquiry_xy_coordinate_base(in)			getbitfield(in + 0x62, 1, 3)
#define get_inquiry_lineart_order(in)				getbitfield(in + 0x62, 1, 4)
#define get_inquiry_start_density(in)				getbitfield(in + 0x62, 1, 5)
#define get_inquiry_hw_x_scaling(in)				getbitfield(in + 0x62, 1, 6)
#define get_inquiry_hw_y_scaling(in)				getbitfield(in + 0x62, 1, 7)

#define get_inquiry_sc_feature_byte3(in)			in[0x63]
#define get_inquiry_ADF_no_paper(in)				getbitfield(in + 0x63, 1, 0)
#define get_inquiry_ADF_cover_open(in)				getbitfield(in + 0x63, 1, 1)
#define get_inquiry_ADF_paper_jam(in)				getbitfield(in + 0x63, 1, 2)
#define get_inquiry_0x63_bit3(in)				getbitfield(in + 0x63, 1, 3)
#define get_inquiry_0x63_bit4(in)				getbitfield(in + 0x63, 1, 4)
#define get_inquiry_lens_cal_in_doc_pos(in)			getbitfield(in + 0x63, 1, 5)
#define get_inquiry_manual_focus(in)				getbitfield(in + 0x63, 1, 6)
#define get_inquiry_sel_uta_lens_cal_pos(in)			getbitfield(in + 0x63, 1, 7)

#define get_inquiry_gamma_download_available(in)		getbitfield(in + 0x64, 1, 7)
#define get_inquiry_0x64_bit6(in)				getbitfield(in + 0x64, 1, 6)
#define get_inquiry_gamma_type_2(in)				getbitfield(in + 0x64, 1, 5)
#define get_inquiry_0x64_bit4(in)				getbitfield(in + 0x64, 1, 4)
#define get_inquiry_0x64_bit3(in)				getbitfield(in + 0x64, 1, 3)
#define get_inquiry_0x64_bit2(in)				getbitfield(in + 0x64, 1, 2)
#define get_inquiry_gamma_lines(in)				getbitfield(in + 0x64, 0x03, 0)
#  define IN_3_pass			0x01
#  define IN_3_line			0x03

#define get_inquiry_0x65(in)					in[0x65]

#define get_inquiry_gib(in)					in[0x66]
#define get_inquiry_gib_8bpp(in)				getbitfield(in + 0x66, 1, 0) 
#define get_inquiry_gib_9bpp(in)				getbitfield(in + 0x66, 1, 1)
#define get_inquiry_gib_10bpp(in)				getbitfield(in + 0x66, 1, 2) 
#define get_inquiry_gib_12bpp(in)				getbitfield(in + 0x66, 1, 3)
#define get_inquiry_gib_14bpp(in)				getbitfield(in + 0x66, 1, 4)
#define get_inquiry_gib_16bpp(in)				getbitfield(in + 0x66, 1, 5)
#define get_inquiry_0x66_bit6(in)				getbitfield(in + 0x66, 1, 6)
#define get_inquiry_0x66_bit7(in)				getbitfield(in + 0x66, 1, 7)
#  define IN_gib_8bpp			0x01
#  define IN_gib_10bpp			0x04

#define get_inquiry_0x67(in)					in[0x67]

#define get_inquiry_gob(in)					in[0x68]
#define get_inquiry_gob_8bpp(in)				getbitfield(in + 0x68, 1, 0) 
#define get_inquiry_gob_9bpp(in)				getbitfield(in + 0x68, 1, 1)
#define get_inquiry_gob_10bpp(in)				getbitfield(in + 0x68, 1, 2) 
#define get_inquiry_gob_12bpp(in)				getbitfield(in + 0x68, 1, 3)
#define get_inquiry_gob_14bpp(in)				getbitfield(in + 0x68, 1, 4)
#define get_inquiry_gob_16bpp(in)				getbitfield(in + 0x68, 1, 5)
#define get_inquiry_0x68_bit6(in)				getbitfield(in + 0x68, 1, 6)
#define get_inquiry_0x68_bit7(in)				getbitfield(in + 0x68, 1, 7)
#  define IN_gob_8bpp			0x01
#  define IN_gob_10bpp			0x04

#define get_inquiry_hda(in)					getbitfield(in + 0x69, 0x01, 7)
#define get_inquiry_max_halftone_matrix(in)			getbitfield(in + 0x69, 0x7f, 0)
#define get_inquiry_halftones_supported(in)			in[0x6a]
#define get_inquiry_halftones_2x2(in)				getbitfield(in + 0x6a, 1, 0)
#define get_inquiry_halftones_4x4(in)				getbitfield(in + 0x6a, 1, 1)
#define get_inquiry_halftones_6x6(in)				getbitfield(in + 0x6a, 1, 2)
#define get_inquiry_halftones_8x8(in)				getbitfield(in + 0x6a, 1, 3)
#define get_inquiry_halftones_12x12(in)				getbitfield(in + 0x6a, 1, 5)
#  define IN_halftone_2x2		0x01
#  define IN_halftone_4x4		0x02
#  define IN_halftone_6x6		0x04
#  define IN_halftone_8x8		0x08
#  define IN_halftone_12x12		0x20

#define get_inquiry_0x6b(in)					in[0x6b]
#define get_inquiry_0x6c(in)					in[0x6c]

#define get_inquiry_colorseq(in)				getbitfield(in + 0x6d, 0x07, 5)
#  define IN_color_sequence_RGB		0x00
#  define IN_color_sequence_RBG		0x01
#  define IN_color_sequence_GBR		0x02
#  define IN_color_sequence_GRB		0x03
#  define IN_color_sequence_BRG		0x04
#  define IN_color_sequence_BGR		0x05
#  define IN_color_sequence_all		0x07
#define get_inquiry_color_order(in)				getbitfield(in + 0x6d, 0x1f, 0)
#define set_inquiry_color_order(in,val)				setbitfield(in + 0x6d, 0x1f, 0, val)
#define get_inquiry_color_order_pixel(in)			getbitfield(in + 0x6d, 1, 0)
#define get_inquiry_color_order_line_no_ccd(in)			getbitfield(in + 0x6d, 1, 1)
#define get_inquiry_color_order_plane(in)			getbitfield(in + 0x6d, 1, 2)
#define get_inquiry_color_order_line_w_ccd(in)			getbitfield(in + 0x6d, 1, 3)
#define get_inquiry_color_order_reserved(in)			getbitfield(in + 0x6d, 1, 4)
#  define IN_color_ordering_pixel	0x01
#  define IN_color_ordering_line_no_ccd	0x02
#  define IN_color_ordering_plane	0x04
#  define IN_color_ordering_line_w_ccd	0x08

#define get_inquiry_max_vidmem(in)				getnbyte(in + 0x6e, 4)

#define get_inquiry_0x72(in)					in[0x72]
#define get_inquiry_max_opt_res(in)				in[0x73]
#define get_inquiry_max_x_res(in)				in[0x74]
#define get_inquiry_max_y_res(in)				in[0x75]
#define get_inquiry_fb_max_scan_width(in)			getnbyte(in + 0x76, 2)
#define get_inquiry_fb_max_scan_length(in)			getnbyte(in + 0x78, 2)
#define get_inquiry_uta_x_original_point(in)			getnbyte(in + 0x7a, 2)
#define get_inquiry_uta_y_original_point(in)			getnbyte(in + 0x7c, 2)
#define get_inquiry_uta_max_scan_width(in)			getnbyte(in + 0x7e, 2)
#define get_inquiry_uta_max_scan_length(in)			getnbyte(in + 0x80, 2)

#define get_inquiry_0x82(in)					in[0x82]

#define get_inquiry_dor_max_opt_res(in)				in[0x83]
#define get_inquiry_dor_max_x_res(in)				in[0x84]
#define get_inquiry_dor_max_y_res(in)				in[0x85]

#define get_inquiry_dor_x_original_point(in)			getnbyte(in + 0x86, 2)
#define get_inquiry_dor_y_original_point(in)			getnbyte(in + 0x88, 2)
#define get_inquiry_dor_max_scan_width(in)			getnbyte(in + 0x8a, 2)
#define get_inquiry_dor_max_scan_length(in)			getnbyte(in + 0x8c, 2)

#define get_inquiry_0x8e(in)					in[0x8e]

#define get_inquiry_last_calibration_lamp_density(in)		in[0x8f]
#  define IN_min_lamp_density		0x01
#  define IN_max_lamp_density		0xff

#define get_inquiry_0x90(in)					in[0x90]

#define get_inquiry_lamp_warmup_maximum_time(in)		in[0x91]
#define get_inquiry_wdb_length(in)				getnbyte(in + 0x92, 2)
#define get_inquiry_optical_resolution_residue(in)		in[0x94]
#define get_inquiry_x_resolution_residue(in)			in[0x95]
#define get_inquiry_y_resolution_residue(in)			in[0x96]
#define get_inquiry_analog_gamma_table(in)			in[0x97]

#define get_inquiry_0x98(in)					in[0x98]
#define get_inquiry_0x99(in)					in[0x99]

#define get_inquiry_max_calibration_data_lines(in)		in[0x9a]
#define get_inquiry_fb_uta_line_arrangement_mode(in)		in[0x9b]
#define get_inquiry_adf_line_arrangement_mode(in)		in[0x9c]
#define get_inquiry_CCD_line_distance(in)			in[0x9d]

#define set_inquiry_max_calibration_data_lines(out,val)		out[0x9a]=val
#define set_inquiry_fb_uta_line_arrangement_mode(out,val)	out[0x9b]=val
#define set_inquiry_adf_line_arrangement_mode(out,val)		out[0x9c]=val
#define set_inquiry_CCD_line_distance(out,val)			out[0x9d]=val

#define get_inquiry_0x9e(in)					in[0x9e]

#define get_inquiry_dor_optical_resolution_residue(in)		in[0xa0]
#define get_inquiry_dor_x_resolution_residue(in)		in[0xa1]
#define get_inquiry_dor_y_resolution_residue(in)		in[0xa2]


/* --------------------------------------------------------------------------------------------------------- */

/* although the command is defined with 6 bytes length in the doc, 10 bytes seems to be the correct length */
static unsigned char get_lamp_statusC[] = { GET_LAMP_STATUS, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static scsiblk get_lamp_status = { get_lamp_statusC,sizeof(get_lamp_statusC) };
#define get_lamp_status_lamp_on(in)		getbitfield(in, 1, 0)


/* --------------------------------------------------------------------------------------------------------- */


/* although the command is defined with 6 bytes length in the doc, 10 bytes seems to be the correct length */
static unsigned char set_lamp_statusC[] = { SET_LAMP_STATUS, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static scsiblk set_lamp_status = { set_lamp_statusC,sizeof(set_lamp_statusC) };
#define set_lamp_status_lamp_on(in,val)		setbitfield(in + 0x03, 1, 7, val)


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


static unsigned char set_windowC[] =
{
  SET_WINDOW, 0x00,		/* opcode, lun */
  0x00, 0x00, 0x00, 0x00,	/* reserved */
  0x00, 0x00, 0x00,		/* transfer length; needs to be set */
  0x00				/* control byte */
};

#define set_SW_xferlen(sb, len)				putnbyte(sb + 0x06, len, 3)

static scsiblk set_window = { set_windowC, sizeof(set_windowC) };


/* --------------------------------------------------------------------------------------------------------- */


static unsigned char window_parameter_data_blockC[] =
{
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* reserved */
  0x00, 0x00				/* Window Descriptor Length, value must be set by SHORT_WDB define */
};

#define set_WPDB_wdblen(sb, len)			putnbyte(sb + 0x06, len, 2)


static scsiblk window_parameter_data_block =
{ window_parameter_data_blockC, sizeof(window_parameter_data_blockC) };


/* --------------------------------------------------------------------------------------------------------- */


static unsigned char window_descriptor_blockC[] =
{
#define max_WDB_size 0xff
#define used_WDB_size 0x53

/* 0x00 */  0x00,									/* Window Identifier */
#define set_WD_wid(sb, val)				sb[0] = val
#  define WD_wid_bw			0x00
#  define WD_wid_red			0x01
#  define WD_wid_green			0x02
#  define WD_wid_blue			0x03
#  define WD_wid_all			0x00
/* 0x01 */  0x00,									   /* reserved, AUTO */
#define set_WD_auto(sb, val)				setbitfield(sb + 0x01, 1, 0, val)

/* 0x02 */  0x00, 0x00,								      /* X Resolution in dpi */
#define set_WD_Xres(sb, val)				putnbyte(sb + 0x02, val, 2)

/* 0x04 */  0x00, 0x00,								      /* Y Resolution in dpi */
#define set_WD_Yres(sb, val)				putnbyte(sb + 0x04, val, 2)

/* 0x06 */  0x00, 0x00, 0x00, 0x00,					      /* Upper Left X in 1200pt/inch */
#define set_WD_ULX(sb, val)				putnbyte(sb + 0x06, val, 4)

/* 0x0a */  0x00, 0x00, 0x00, 0x00,					      /* Upper Left Y in 1200pt/inch */
#define set_WD_ULY(sb, val)				putnbyte(sb + 0x0a, val, 4)

/* 0x0e */  0x00, 0x00, 0x00, 0x00,							/* Width 1200pt/inch */
#define set_WD_width(sb, val)				putnbyte(sb + 0x0e, val, 4)

/* 0x12 */  0x00, 0x00, 0x00, 0x00,						       /* Length 1200pt/inch */
#define set_WD_length(sb, val)				putnbyte(sb + 0x12, val, 4)

/* 0x16 */  0xff,									       /* Brightness */
#define set_WD_brightness(sb, val)			sb[0x16] = val

/* 0x17 */  0x80,										/* Threshold */
#define set_WD_threshold(sb, val)			sb[0x17] = val

/* 0x18 */  0x80,										 /* Contrast */
#define set_WD_contrast(sb, val)			sb[0x18] = val

/* 0x19 */  0x05,									/* Image Composition */
#define set_WD_composition(sb, val)			sb[0x19] = val
#  define WD_comp_lineart		0x00
#  define WD_comp_dithered		0x01
#  define WD_comp_gray			0x02
#  define WD_comp_gray			0x02
#  define WD_comp_rgb_bilevel		0x03
#  define WD_comp_rgb_dithered		0x04
#  define WD_comp_rgb_full      	0x05

/* 0x1a */  0x08,									       /* Bits/Pixel */
#define set_WD_bitsperpixel(sb, val)			sb[0x1a] = val
#  define WD_bits_1			0x01
#  define WD_bits_8			0x08
#  define WD_bits_10			0x0a

/* 0x1b */  0x00, 0x03,									 /* Halftone pattern */
#define set_WD_halftone(sb, val)			putnbyte(sb + 0x1b, val, 2)
#  define WD_halftone_download		0x00
#  define WD_halftone_12x12_1		0x01
#  define WD_halftone_12x12_2		0x02
#  define WD_halftone_8x8_1		0x03
#  define WD_halftone_8x8_2		0x04
#  define WD_halftone_8x8_3		0x05
#  define WD_halftone_8x8_4		0x06
#  define WD_halftone_8x8_5		0x07
#  define WD_halftone_6x6_1		0x08
#  define WD_halftone_6x6_2		0x09
#  define WD_halftone_6x6_3		0x0a
#  define WD_halftone_4x4_1		0x0b
#  define WD_halftone_4x4_2		0x0c
#  define WD_halftone_4x4_3		0x0d
#  define WD_halftone_2x2_1		0x0e
#  define WD_halftone_2x2_2		0x0f

/* 0x1d */  0x03,							      /* RIF, reserved, padding type */
#define set_WD_RIF(sb, val)				setbitfield(sb + 0x1d, 1, 7, val)
#define set_WD_padding_type(sb,val)			setbitfield(sb + 0x1d, 0x07, 0, val)
#  define WD_padding_byte		0x03
#  define WD_padding_word		0x07

/* 0x1e */  0x00, 0x00,							/* Bit Ordering (0 = left to right;) */
/* 0x20 */  0x00,							      /* Compression Type (reserved) */
/* 0x21 */  0x00,							       /* Compr. Argument (reserved) */
/* 0x22 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,							 /* Reserved */
/* 0x28 */  0x01,										    /* Speed */
#define set_WD_speed(sb, val)				sb[0x28] = val
#  define WD_speed_fastsmear		0x81
#  define WD_speed_slowsmear		0x82
#  define WD_speed_fastclear		0x01
#  define WD_speed_slowclear		0x02
#  define WD_speed_speedbit		0x03
#  define WD_speed_smearbit		0x80
#    define WD_speed_fast		0x01
#    define WD_speed_slow		0x02
#    define WD_speed_smear		0x80

/* 0x29 */  0x00,									     /* Select Color */
#define set_WD_select_color(sb,val)			setbitfield(sb+0x29, 0x07, 5, val)
#  define WD_color_gray			0x00
#  define WD_color_red			0x04
#  define WD_color_green		0x02
#  define WD_color_blue			0x01

/* 0x2a */  0xff,										/* Highlight */
#define set_WD_highlight(sb, val)			sb[0x2a] = val

/* 0x2b */  0x00,										   /* Shadow */
#define set_WD_shadow(sb, val)				sb[0x2b] = val

/* 0x2c */  0x00, 0x00,									     /* Paper Length */
#define set_WD_paperlength(sb, val)			putnbyte(sb + 0x2c, val, 2)

/* 0x2e */  0x0f,									   /* Gamma Function */
#define set_WD_gamma(sb, val)				sb[0x2e] = val
#  define WD_gamma_download		0x00
#  define WD_gamma_builtin1		0x01
#  define WD_gamma_builtin2		0x02
#  define WD_gamma_builtin3		0x03
#  define WD_gamma_builtin4		0x04
#  define WD_gamma_normal		0x0f

/* 0x2f */  0x11,									      /* Scan Module */
#define set_WD_module(sb, val)				sb[0x2f] = val
#  define WD_module_flatbed		0x11
#  define WD_module_transparency	0xff

/* 0x30 */  0x01,						      /* HBT, DOR, reserved, RMIF, CBHS Type */
							   /* CBHS = Contrast, Brightness, Highlight, Shadow */
#define set_WD_CBHS(sb, val)				setbitfield(sb + 0x30, 1, 0, val)
#  define WD_CBHS_50			0
#  define WD_CBHS_255			1
#define set_WD_FF(sb, val)				setbitfield(sb + 0x30, 1, 1, val) /* FF = Fix Focus position */
#define set_WD_RMIF(sb, val)				setbitfield(sb + 0x30, 1, 2, val) /* Reverse Multil Image Frmt */
#define set_WD_FDC(sb, val)				setbitfield(sb + 0x30, 1, 3, val) /* document calibration */
#define set_WD_PF(sb, val)				setbitfield(sb + 0x30, 1, 4, val) /* PF pre focus */
#define set_WD_LCL(sb, val)				setbitfield(sb + 0x30, 1, 5, val) /* LCL (focus position) */
#define set_WD_DOR(sb, val)				setbitfield(sb + 0x30, 1, 6, val) /* Double Optical Resolution */
#define set_WD_HBT(sb, val)				setbitfield(sb + 0x30, 1, 7, val) /* High Byte Transfer */
#  define WD_HBT_HBF			0x00
#  define WD_HBT_LBF			0x01

/* 0x31 */  0x00, 0x00,								      /* scan exposure level */
#define set_WD_scan_exposure_level(sb, val)		putnbyte(sb+0x31, val, 2)

/* 0x33 */  0x00, 0x00,							       /* calibration exposure level */
#define set_WD_calibration_exposure_level(sb, val)	putnbyte(sb+0x33, val, 2)

/* 0x35 */  0x00,										 /* reserved */
/* 0x36 */  0x00,										 /* reserved */
/* 0x37 */  0x00,										 /* reserved */
/* 0x38 */  0x00,							      /* reserved (For Autoexposure) */

/* 0x39 */  0x00,							   /* BS, reserved, Calibration Mode */
#define set_WD_batch(sb, val)				setbitfield(sb + 0x39, 1, 7, val)
#define set_WD_MF(sb, val)				setbitfield(sb + 0x39, 1, 6, val) /* manual focus */
#define set_WD_line_arrangement(sb, val)		setbitfield(sb + 0x39, 1, 5, val)
#  define WD_line_arrengement_by_driver	0x01
#  define WD_line_arrengement_by_fw	0x00
#define set_WD_warmup(sb, val)				setbitfield(sb + 0x39, 1, 4, val)
#define set_WD_calibration(sb, val)			setbitfield(sb + 0x39, 0x0f, 0, val)
#  define WD_calibration_image		0x00
#  define WD_calibration_lineart	0x0f
#  define WD_calibration_dither		0x0e
#  define WD_calibration_gray		0x0d
#  define WD_calibration_rgb		0x0a 
#  define WD_calibration_ignore		0x09

/* 0x3a */  0x01,						   /* Color Sequence, Color Ordering Support */
#define set_WD_color_sequence(sb,val)			setbitfield(sb+0x3a,0x07,5, val)
#  define WD_color_sequence_RGB  0x00
#  define WD_color_sequence_RBG  0x01
#  define WD_color_sequence_GBR  0x02
#  define WD_color_sequence_GRB  0x03
#  define WD_color_sequence_BRG  0x04
#  define WD_color_sequence_BGR  0x05

#define set_WD_color_ordering(sb,val)			setbitfield(sb+0x3a,0x1f,0, val)
#  define WD_color_ordering_pixel       0x01
#  define WD_color_ordering_line_no_ccd 0x02
#  define WD_color_ordering_plane       0x04
#  define WD_color_ordering_line_w_ccd  0x08

/* 0x3b */  0x00,							   /* analog gamma code (set to 1.0) */
#define set_WD_analog_gamma(sb, gamma)			sb[0x3b] = gamma	   /* see analog_gamma_table */

/* 0x3c */  0x00,										 /* Reserved */

/* 0x3d */  0x00,							    /* Next Calibration Lamp Density */
#define set_WD_lamp_c_density(sb, val)			sb[0x3d] = val
#  define WD_lamp_c_density_auto	0x00
#  define WD_lamp_c_density_min		0x01
#  define WD_lamp_c_density_max		0xff

/* 0x3e */  0x00,										 /* Reserved */

/* 0x3f */  0x00,								   /* Next Scan Lamp Density */
#define set_WD_lamp_s_density(sb, val)			sb[0x3f] = val
#  define WD_lamp_s_density_auto	0x00
#  define WD_lamp_s_density_min		0x01
#  define WD_lamp_s_density_max		0xff

/* 0x40 */  0x00, 0x00, 0x00, 0x00,				       /* Next Upper Left Y in 1200 dpt/inch */
#define set_WD_next_upper_left(sb, val)			putnbyte(sb + 0x40, val, 4)

/* 0x44 */  0x00, 0x00, 0x00, 0x00,							      /* Pixel Count */
#define set_WD_pixel_count(sb, val)			putnbyte(sb + 0x44, val, 4)

/* 0x48 */  0x00, 0x00, 0x00, 0x00,							       /* Line Count */
#define set_WD_line_count(sb, val)			putnbyte(sb + 0x48, val, 4)

/* 0x4c */  4, 176,							/* x coordiante base 1200 (pts/inch) */
#define set_WD_x_coordinate_base(sb, val)		putnbyte(sb + 0x4c, val, 2)

/* 0x4e */  4, 176,							/* y coordinate base 1200 (pts/inch) */
#define set_WD_y_coordinate_base(sb, val)		putnbyte(sb + 0x4e, val, 2)

/* 0x50 */  0x00,										 /* reserved */

/* 0x51 */  0x00,						 /* driver calibration need image data lines */
#define set_WD_calibration_data_lines(sb, val)		sb[0x51] = val

/* 0x52 */  0x00									    /* start density */
#define set_WD_start_density(sb, val)			sb[0x52] = val

/* if somone adds here anything, please change used_WDB_size in this file !! */
};


static scsiblk window_descriptor_block = { window_descriptor_blockC, sizeof(window_descriptor_blockC) };


/* --------------------------------------------------------------------------------------------------------- */

 
#define set_WDB_length(length)				(window_descriptor_block.size = (length))

#define WPDB_OFF(b)					(b + set_window.size)

#define WDB_OFF(b, n)					(b + set_window.size + \
							window_parameter_data_block.size + \
							( window_descriptor_block.size * (n - 1) ) )

#define set_WPDB_wdbnum(sb,n)				set_WPDB_wdblen(sb,window_descriptor_block.size*n)


/* --------------------------------------------------------------------------------------------------------- */


static unsigned char scanC[] = { SCAN, 0x00, 0x00, 0x00, 0x01, 0x20, 0x00, 0x00, 0x00 };

static scsiblk scan = { scanC, sizeof(scanC) - 3 };
#define set_SC_xfer_length(sb, val)			sb[0x04] = val
#define set_SC_quality(sb, val)				setbitfield(sb + 0x05, 1, 5, val)
#define set_SC_adf(sb, val)				setbitfield(sb + 0x05, 1, 6, val)
#define set_SC_preview(sb, val)				setbitfield(sb + 0x05, 1, 7, val)
#define set_SC_wid(sb, n, val)				sb[0x05 + n] = val 


/* --------------------------------------------------------------------------------------------------------- */


/* sread instead of read because read is a libc primitive */
static unsigned char sreadC[] = { READ, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static scsiblk sread = { sreadC, sizeof(sreadC) };

#define set_R_datatype_code(sb, val)			sb[0x02] = val
#  define R_datatype_imagedata		0x00
#  define R_datatype_vendorspecific	0x01
#  define R_datatype_halftone		0x02
#  define R_datatype_gamma		0x03
#  define R_datatype_shading		0x80
#  define R_datatype_gain		0x81

#define set_R_datatype_qual_winid(sb, val)		sb[0x05] = val
#define set_R_xfer_length(sb, val)			putnbyte(sb + 0x06, val, 3)


/* --------------------------------------------------------------------------------------------------------- */


static unsigned char sendC[] = { SEND, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static scsiblk send = { sendC, sizeof(sendC) };

#define set_S_datatype_code(sb, val)			sb[0x02] = val
#  define S_datatype_imagedata		0x00
#  define S_datatype_vendorspecific	0x01
#  define S_datatype_halftone		0x02
#  define S_datatype_gamma		0x03
#  define S_datatype_shading		0x80
#  define S_datatype_gain		0x81

#define set_S_datatype_qual_winid(sb, val)		sb[0x05] = val

#define set_S_xfer_length(sb, val)			putnbyte(sb + 0x06, val, 3)
#  define S_xfer_length_gamma(l)	((l * (1024 + 1)) + 1)
#  define S_xfer_length_1line		0x0402
#  define S_xfer_length_2line		0x0803
#  define S_xfer_length_3line		0x0c04

/* FIXME: add Type 0 download curve format */
/* FIXME: add Type 1 download curve format */


static unsigned char gamma_DCF0C[] = { 0x00 };
static scsiblk gamma_DCF0 = { gamma_DCF0C, sizeof(gamma_DCF0C) };
#define set_DCF0_gamma_color(dc,col,val)		dc[1 + (1025 * col)] = val
#  define DCF0_gamma_color_gray		0
#  define DCF0_gamma_color_red		1
#  define DCF0_gamma_color_green	2
#  define DCF0_gamma_color_blue		3

#define set_DCF0_gamma_lines(dc,val)			setbitfield(dc + 0x00, 3, 0, val)
#  define DCF0_gamma_one_line		1
#  define DCF0_gamma_three_lines	3


static unsigned char gamma_DCF1C[] = { 0x00, 0x00 };
static scsiblk gamma_DCF1 = { gamma_DCF1C, sizeof(gamma_DCF1C) };
#define set_DCF1_gamma_color(dc,val)			dc[1] = val
#  define DCF1_gamma_color_gray		0
#  define DCF1_gamma_color_red		1
#  define DCF1_gamma_color_green	2
#  define DCF1_gamma_color_blue		3


static unsigned char gamma_DCF2C[] = { 0x21, 0x00, 0x01, 0x00, 0x01, 0x00 };
static scsiblk gamma_DCF2 = { gamma_DCF2C, sizeof(gamma_DCF2C) };

#define set_DCF2_gamma_color(dc,val)			setbitfield(dc + 0x00, 3, 2, val)
#  define DCF2_gamma_color_gray		0
#  define DCF2_gamma_color_red		1
#  define DCF2_gamma_color_green	2
#  define DCF2_gamma_color_blue		3

#define set_DCF2_gamma_lines(dc,val)			setbitfield(dc + 0x00, 3, 0, val)
#  define DCF2_gamma_one_line		1
#  define DCF2_gamma_three_lines	3

#define set_DCF2_gamma_input_bits(dc,val)		dc[0x02]=val
#define set_DCF2_gamma_output_bits(dc,val)		dc[0x04]=val
#  define DCF2_gamma_8_bits		1
#  define DCF2_gamma_10_bits		4


/* --------------------------------------------------------------------------------------------------------- */


static unsigned char object_positionC[] =
{ OBJECT_POSITION, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static scsiblk object_position = { object_positionC, sizeof(object_positionC) };


/* --------------------------------------------------------------------------------------------------------- */


#ifndef UMAX_HIDE_UNUSED
static unsigned char get_data_buffer_statusC[] =
{
  GET_DATA_BUFFER_STATUS, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x00 };

#define set_GDBS_wait(sb, val)				setbitfield(sb + 0x01, 1, 0, val)
#define set_GDBS_len(sb, nwins)				putnbyte(sb + 0x07, (4 + (nwins * 8)), 2)

static scsiblk get_data_buffer_status = { get_data_buffer_statusC, sizeof(get_data_buffer_statusC) };

  /* DBS is Data Buffer Status, the header that is returned */
#define get_DBS_data_buffer_status_length(sb)		getnbyte(sb, 3)
#define get_DBSD_block_bit(sb)				getbitfield(sb + 3, 1, 0)

  /* DBSD are the Data Buffer Status Descriptors, the records that
   * are returned following the DBS header. */
  /* order is the number of the DBSD, from 1 to (length/8) */
#define get_DBSD_winid(sb, order)			sb[4 + ((order - 1) * 8)]
#define get_DBSD_available_data_buffer(sb,order)	getnbyte(sb+4+((order-1)*8)+2,3)
#define get_DBSD_filled_data_buffer(sb,order)		getnbyte(sb+4+((order-1)*8)+5,3)

#define gdbs_return_block_size 12
#endif


/* --------------------------------------------------------------------------------------------------------- */


#ifndef UMAX_HIDE_UNUSED
 static unsigned char write_bufferC[] =
 { WRITE_BUFFER, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

 static scsiblk write_buffer = { write_bufferC, sizeof(write_bufferC) };
#endif


/* --------------------------------------------------------------------------------------------------------- */


static double analog_gamma_table[16]={1.0000, 1.0434, 1.0887, 1.1361, \
                                     1.1859, 1.2382, 1.2933, 1.3516, \
                                     1.4134, 1.4792, 1.5495, 1.6251, \
                                     1.7067, 1.7954, 1.8926, 2.0000 };


/* --------------------------------------------------------------------------------------------------------- */


static unsigned char request_senseC[] = { REQUEST_SENSE, 0x00, 0x00, 0x00, 0x00, 0x00 };
#define set_RS_allocation_length(sb,val)		sb[0x04]=val
#define set_RS_LUN(sb,val)				setbitfield(sb + 0x01, 7, 5) /* ??? */

static scsiblk request_sense = { request_senseC, sizeof(request_senseC) };

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
#define get_RS_asb_dim_light(b)				getbitfield(b+0x12,1,7)
#define get_RS_asb_no_light(b)				getbitfield(b+0x12,1,6)
#define get_RS_asb_sensor_motor(b)			getbitfield(b+0x12,1,5)
#define get_RS_asb_too_light(b)				getbitfield(b+0x12,1,4)
#define get_RS_asb_calibration(b)			getbitfield(b+0x12,1,3)
#define get_RS_asb_rom(b)				getbitfield(b+0x12,1,2)
#define get_RS_asb_ram(b)				getbitfield(b+0x12,1,1)
#define get_RS_asb_cpu(b)				getbitfield(b+0x12,1,0)
#define get_RS_asb_scsi(b)				getbitfield(b+0x13,1,7)
#define get_RS_asb_timer(b)				getbitfield(b+0x13,1,6)
#define get_RS_asb_filter_motor(b)			getbitfield(b+0x13,1,5)
#define get_RS_asb_dc_adjust(b)				getbitfield(b+0x13,1,1)
#define get_RS_asb_uta_sensor(b)			getbitfield(b+0x13,1,0)

#define get_RS_scanner_error_code(b)			b[0x15]
#define get_RS_SCC_condition_code(b)			b[0x17]
#define get_RS_SCC_calibration_bytesperline(b)		getnbyte(b+0x18, 4)
#define get_RS_SCC_calibration_lines(b)			getnbyte(b+0x1c, 2)
#define get_RS_SCC_calibration_bytespp(b)		b[0x1e]

#define rs_return_block_size		0x1f


/* --------------------------------------------------------------------------------------------------------- */


static char *cbhs_str[]       = { "0-50","0-255", "0-255 autoexposure", "reserved" };

static char *scanmode_str[]   = { "flatbed (FB)", "automatic document feeder (ADF)" };

static char *gamma_lines_str[]= { "reserved",
                            "one line (gray), three pass (color) download",
                            "reserved",
                            "one line (gray), three lines (color) download" };

static char *color_sequence_str[]= { "R->G->B","R->B->G",
                                     "G->B->R","G->R->B",
                                     "B->R->G","B->G->R",
                                     "reserved","all supported"};


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


static char *scanner_error_str[] =
{"no error",      /* 0 */
 "CPU error",
 "err 2",
 "err 3",
 "ROM error",
 "err 5",
 "err 6",
 "err 7",
 "err 8",
 "err 9",
 "buffer error",        /* 10 */
 "system buffer error",
 "shading buffer error",
 "video buffer error",
 "stack buffer error",
 "control buffer error",
 "gamma buffer error",
 "err 17",
 "err 18",
 "err 19",
 "lamp error",          /* 20 */
 "dark error",
 "dim error",
 "light error",
 "lamp adjust control error (by darken)",
 "err 25",
 "err 26",
 "err 27",
 "err 28",
 "err 29",
 "calibration error",   /* 30 */
 "dc offset error",
 "gain error",
 "auto focus error",
 "err 34",
 "err 35",
 "err 36",
 "err 37",
 "err 38",
 "err 39",
 "scsi error",          /* 40 */
 "err 41",
 "asic error",
 "timer error",
 "ccd error",
 "err 45",
 "err 46",
 "err 47",
 "err 48",
 "err 49",
 "uta error",           /* 50 */
 "uta home or motor sensor error",
 "err 52",
 "err 53",
 "err 54",
 "err 55",
 "err 56",
 "err 57",
 "err 58",
 "err 59",
 "adf error",           /* 60 */
 "adf paper jam",
 "adf no paper",
 "adf cover open",
 "err 64",
 "err 65",
 "err 66",
 "err 67",
 "err 68",
 "err 69",
 "fb sensor error",     /* 70 */
 "fb home or motor sensor error",
 "fb filter or motor sensor error",
 "fb lens or motor sensor error"
 "first line position error (LER error, vertical)",
 "first pixel position error (SER error, horizontal)",
 "first pixel position error for lens 2 (SER2 error, horizontal)",
 "err 77",
 "err 78",
 "err 79",
 "err",      /* 80 */
 "err",
 "err",
 "err",
 "err",
 "err",
 "err",
 "err",
 "err",
 "err",
 "err",      /* 90 */
 "err",
 "err",
 "err",
 "err",
 "err",
 "err",
 "err",
 "err",
 "err"       /* 99 */
};


/* --------------------------------------------------------------------------------------------------------- */


#endif
