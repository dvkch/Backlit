#ifndef SP15C_SCSI_H
#define SP15C_SCSI_H

static const char RCSid_sh[] = "$Header$";
/* sane - Scanner Access Now Easy.

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

   This file implements a SANE backend for Fujitsu ScanParner 15c
   flatbed/ADF scanners.  It was derived from the COOLSCAN driver.
   Written by Randolph Bentson <bentson@holmsjoen.com> */

/* ------------------------------------------------------------------------- */
/*
 * $Log$
 * Revision 1.6  2005/09/19 19:57:48  fzago-guest
 * Replaced __unused__ with __sane_unused__ to avoid a namespace conflict.
 *
 * Revision 1.5  2004/11/13 19:53:04  fzago-guest
 * Fixes some warnings.
 *
 * Revision 1.4  2003/12/27 17:48:38  hmg-guest
 * Silenced some compilation warnings.
 *
 * Revision 1.3  2000/08/12 15:09:37  pere
 * Merge devel (v1.0.3) into head branch.
 *
 * Revision 1.1.2.4  2000/03/14 17:47:12  abel
 * new version of the Sharp backend added.
 *
 * Revision 1.1.2.3  2000/02/14 14:20:19  pere
 * Make lint_catcher static to avoid link problems with duplicate symbols.
 *
 * Revision 1.1.2.2  2000/01/26 03:51:48  pere
 * Updated backends sp15c (v1.12) and m3096g (v1.11).
 *
 * Revision 1.7  2000/01/05 05:27:34  bentson
 * indent to barfin' GNU style
 *
 * Revision 1.6  1999/12/03 20:56:44  bentson
 * add MEDIA CHECK command
 *
 * Revision 1.5  1999/11/24 15:57:50  bentson
 * add license
 *
 * Revision 1.4  1999/11/23 19:04:08  bentson
 * clean up and enhance inquiry command
 *
 * Revision 1.3  1999/11/23 06:42:19  bentson
 * add 4-bit grayscale support; fix color count field placement
 *
 * Revision 1.2  1999/11/22 18:15:43  bentson
 * more work on color support
 *
 * Revision 1.1  1999/11/19 15:09:08  bentson
 * cribbed from m3096g
 *
 */

/****************************************************/

static inline void
setbitfield (unsigned char *pageaddr, int mask, int shift, int val) \
{
  *pageaddr = (*pageaddr & ~(mask << shift)) | ((val & mask) << shift);
}

/* ------------------------------------------------------------------------- */

static inline void
resetbitfield (unsigned char *pageaddr, int mask, int shift, int val) \
{
  *pageaddr = (*pageaddr & ~(mask << shift)) | (((!val) & mask) << shift);
}

/* ------------------------------------------------------------------------- */

static inline int
getbitfield (unsigned char *pageaddr, int mask, int shift) \
{
  return ((*pageaddr >> shift) & mask);
}

/* ------------------------------------------------------------------------- */

static inline int
getnbyte (unsigned char *pnt, int nbytes) \
{
  unsigned int result = 0;
  int i;

#ifdef DEBUG
  assert (nbytes < 5);
#endif
  for (i = 0; i < nbytes; i++)
    result = (result << 8) | (pnt[i] & 0xff);
  return result;
}

/* ------------------------------------------------------------------------- */

static inline void
putnbyte (unsigned char *pnt, unsigned int value, unsigned int nbytes) \
{
  int i;

#ifdef DEBUG
  assert (nbytes < 5);
#endif
  for (i = nbytes - 1; i >= 0; i--)
    \
    {
      pnt[i] = value & 0xff;
      value = value >> 8;
    }
}


/* ==================================================================== */
/* SCSI commands */

typedef struct
{
  unsigned char *cmd;
  unsigned int size;
}
scsiblk;

/* ==================================================================== */

#define RESERVE_UNIT            0x16
#define RELEASE_UNIT            0x17
#define INQUIRY                 0x12
#define REQUEST_SENSE           0x03
#define SEND_DIAGNOSTIC			0x1d
#define TEST_UNIT_READY         0x00
#define SET_WINDOW              0x24
#define SET_SUBWINDOW           0xc0
#define OBJECT_POSITION         0x31
#define MEDIA_CHECK             0x08
#define SEND                    0x2a
#define READ                    0x28
#define MODE_SELECT				0x15
#define MODE_SENSE				0x1a
#define SCAN                    0x1b

/* ==================================================================== */

static unsigned char reserve_unitC[] =
{RESERVE_UNIT, 0x00, 0x00, 0x00, 0x00, 0x00};
static scsiblk reserve_unitB =
{reserve_unitC, sizeof (reserve_unitC)};

/* ==================================================================== */

static unsigned char release_unitC[] =
{RELEASE_UNIT, 0x00, 0x00, 0x00, 0x00, 0x00};
static scsiblk release_unitB =
{release_unitC, sizeof (release_unitC)};

/* ==================================================================== */

static unsigned char inquiryC[] =
{INQUIRY, 0x00, 0x00, 0x00, 0x1f, 0x00};
static scsiblk inquiryB =
{inquiryC, sizeof (inquiryC)};

#define set_IN_return_size(icb,val)        icb[0x04]=val
#define set_IN_length(out,n)               out[0x04]=n-5

#define get_IN_periph_qual(in)             getbitfield(in, 0x07, 5)
#define IN_periph_qual_lun                    0x00
#define IN_periph_qual_nolun                  0x03
#define get_IN_periph_devtype(in)          getbitfield(in, 0x1f, 0)
#define IN_periph_devtype_scanner             0x06
#define IN_periph_devtype_unknown             0x1f
#define get_IN_response_format(in)         getbitfield(in + 0x03, 0x0f, 0)
#define IN_recognized                         0x02
#define get_IN_additional_length(in)       in[0x04]
#define get_IN_vendor(in, buf)             strncpy(buf, in + 0x08, 0x08)
#define get_IN_product(in, buf)            strncpy(buf, in + 0x10, 0x010)
#define get_IN_version(in, buf)            strncpy(buf, in + 0x20, 0x04)
#define get_IN_color_mode(in)              getbitfield(in + 0x24, 0xf, 0)
#define get_IN_color_seq(in)               getbitfield(in + 0x24, 0x7, 4)
#define get_IN_adf(in)                     getbitfield(in + 0x24, 0x1, 7)

/* ==================================================================== */

static unsigned char request_senseC[] =
{REQUEST_SENSE, 0x00, 0x00, 0x00, 0x00, 0x00};
static scsiblk request_senseB =
{request_senseC, sizeof (request_senseC)};

#define set_RS_allocation_length(sb,val) sb[0x04] = (unsigned char)val
/* defines for request sense return block */
#define get_RS_information_valid(b)       getbitfield(b + 0x00, 1, 7)
#define get_RS_error_code(b)              getbitfield(b + 0x00, 0x7f, 0)
#define get_RS_filemark(b)                getbitfield(b + 0x02, 1, 7)
#define get_RS_EOM(b)                     getbitfield(b + 0x02, 1, 6)
#define get_RS_ILI(b)                     getbitfield(b + 0x02, 1, 5)
#define get_RS_sense_key(b)               getbitfield(b + 0x02, 0x0f, 0)
#define get_RS_information(b)             getnbyte(b+0x03, 4)	/* normally 0 */
#define get_RS_additional_length(b)       b[0x07]	/* always 10 */
#define get_RS_ASC(b)                     b[0x0c]
#define get_RS_ASCQ(b)                    b[0x0d]
#define get_RS_SKSV(b)                    getbitfield(b+0x0f,1,7)	/* valid, always 0 */

#define rs_return_block_size              18	/* Says Nikon */

/* ==================================================================== */

static unsigned char send_diagnosticC[] =
{SEND_DIAGNOSTIC, 0x04, 0x00, 0x00, 0x00, 0x00};
static scsiblk send_diagnosticB =
{send_diagnosticC, sizeof (send_diagnosticC)};

/* ==================================================================== */

static unsigned char test_unit_readyC[] =
{TEST_UNIT_READY, 0x00, 0x00, 0x00, 0x00, 0x00};
static scsiblk test_unit_readyB =
{test_unit_readyC, sizeof (test_unit_readyC)};

/* ==================================================================== */

static unsigned char set_windowC[] =
{SET_WINDOW, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,};
      /* opcode,  lun,  _____4 X reserved____,  _transfer length,  ctl */
static scsiblk set_windowB =
{set_windowC, sizeof (set_windowC)};
#define set_SW_xferlen(sb, len) putnbyte(sb + 0x06, len, 3)
#define get_SW_xferlen(sb) getnbyte(sb + 0x06, 3)

/* ==================================================================== */

static unsigned char set_subwindowC[] =
{SET_SUBWINDOW};
static scsiblk set_subwindowB =
{set_subwindowC, sizeof (set_subwindowC)};

/* ==================================================================== */

static unsigned char object_positionC[] =
{OBJECT_POSITION, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
					 /* ADF, _____Count_____,  ________Reserved______, Ctl */
static scsiblk object_positionB =
{object_positionC, sizeof (object_positionC)};

#define set_OP_autofeed(b,val) setbitfield(b+0x01, 0x07, 0, val)
#define OP_Discharge	0x00
#define OP_Feed			0x01
/* ==================================================================== */

static unsigned char media_checkC[] =
{MEDIA_CHECK, 0x00, 0x00, 0x00, 0x00, 0x00};
				 /* ADF, _Reserved_, Len,  Ctl */
static scsiblk media_checkB =
{media_checkC, sizeof (media_checkC)};

#define set_MC_return_size(sb,val)   sb[0x04]=val
#define get_MC_adf_status(sb)     sb[0x00]
#define MC_ADF_OK	0x01

/* ==================================================================== */
static unsigned char media_parameter_data_blockC[] =
{
  0x00
};
static scsiblk media_parameter_data_blockB =
{media_parameter_data_blockC, sizeof (media_parameter_data_blockC)};
/* ==================================================================== */

static unsigned char sendC[] =
{SEND, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static scsiblk sendB =
{sendC, sizeof (sendC)};

#define set_S_datatype_code(sb, val) sb[0x02] = (unsigned char)val
#define S_datatype_imagedatai		0x00
#define S_EX_datatype_LUT			0x01	/* Experiment code */
#define S_EX_datatype_shading_data	0xa0	/* Experiment code */
#define S_user_reg_gamma			0xc0
#define S_device_internal_info		0x03
#define set_S_datatype_qual_upper(sb, val) sb[0x04] = (unsigned char)val
#define S_DQ_none	0x00
#define S_DQ_Rcomp	0x06
#define S_DQ_Gcomp	0x07
#define S_DQ_Bcomp	0x08
#define S_DQ_Reg1	0x01
#define S_DQ_Reg2	0x02
#define S_DQ_Reg3	0x03
#define set_S_xfer_length(sb, val)    putnbyte(sb + 0x06, val, 3)

/*
   static unsigned char gamma_user_LUT_LS1K[512] = { 0x00 };
   static scsiblk gamma_user_LUT_LS1K_LS1K = {
   gamma_user_LUT_LS1K, sizeof(gamma_user_LUT_LS1K)
   };
 */

/* ==================================================================== */

static unsigned char readC[] =
{READ, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	 /* Type, rsvd, type qual, __xfer length__, Ctl */
static scsiblk readB =
{readC, sizeof (readC)};

#define set_R_datatype_code(sb, val) sb[0x02] = val
#define R_datatype_imagedata		0x00
#define R_pixel_size			0x80
#define set_R_xfer_length(sb, val)    putnbyte(sb + 0x06, val, 3)

/* ==================================================================== */

static unsigned char mode_selectC[] =
{MODE_SELECT, 0x10, 0x00, 0x00, 0x00, 0x00};
static scsiblk mode_selectB =
{mode_selectC, sizeof (mode_selectC)};

/* ==================================================================== */

static unsigned char mode_senseC[] =
{MODE_SENSE, 0x18, 0x03, 0x00, 0x00, 0x00, /* PF set, page type 03 */ };
static scsiblk mode_senseB =
{mode_senseC, sizeof (mode_senseC)};

#define set_MS_DBD(b, val)  setbitfield(b, 0x01, 3, (val?1:0))
#define set_MS_len(b, val)	putnbyte(b+0x04, val, 1)
#define get_MS_MUD(b)		getnbyte(b+(0x04+((int)*(b+0x3)))+0x4,2)

/* ==================================================================== */

static unsigned char scanC[] =
{SCAN, 0x00, 0x00, 0x00, 0x00, 0x00};
static scsiblk scanB =
{scanC, sizeof (scanC)};

#define set_SC_xfer_length(sb, val) sb[0x04] = (unsigned char)val

/* ==================================================================== */

/* We use the same structure for both SET WINDOW and GET WINDOW. */
static unsigned char window_parameter_data_blockC[] =
{
  0x00, 0x00, 0x00,
  0x00, 0x00, 0x00,		/* reserved */
  0x00, 0x00,			/* Window Descriptor Length */
};
static scsiblk window_parameter_data_blockB =
{window_parameter_data_blockC, sizeof (window_parameter_data_blockC)};

#define set_WDPB_wdblen(sb, len) putnbyte(sb + 0x06, len, 2)
#define get_WDPB_wdblen(sb) getnbyte(sb + 0x06, 2)

#define WDB_size_empty	0x28	/* wdb_len if nothing is set by inquiry */
#define WDB_size_BW	0x40
#define WDB_size_Color	0x33
#define WDB_size_max	0xff

/* ==================================================================== */

static unsigned char window_descriptor_blockC[] =
{
  0x00,				/* 0x00 *//* Window Identifier */
#define set_WD_wid(sb, val) sb[0] = val
#define WD_wid_all 0x00		/* Only one supported */
  0x00,				/* 0x01 *//* reserved, AUTO */
#define set_WD_auto(sb, val) setbitfield(sb + 0x01, 1, 0, val)
#define get_WD_auto(sb)	getbitfield(sb + 0x01, 1, 0)
  0x00, 0x00,			/* 0x02 *//* X resolution in dpi, 0 => 400 */
#define set_WD_Xres(sb, val) putnbyte(sb + 0x02, val, 2)
#define get_WD_Xres(sb)	getnbyte(sb + 0x02, 2)
  0x00, 0x00,			/* 0x04 *//* Y resolution in dpi, 0 => 400 */
#define set_WD_Yres(sb, val) putnbyte(sb + 0x04, val, 2)
#define get_WD_Yres(sb)	getnbyte(sb + 0x04, 2)
  0x00, 0x00,
  0x00, 0x00,			/* 0x06 *//* Upper Left X in inch/1200 */
#define set_WD_ULX(sb, val) putnbyte(sb + 0x06, val, 4)
#define get_WD_ULX(sb) getnbyte(sb + 0x06, 4)
  0x00, 0x00,
  0x00, 0x00,			/* 0x0a *//* Upper Left Y in inch/1200 */
#define set_WD_ULY(sb, val) putnbyte(sb + 0x0a, val, 4)
#define get_WD_ULY(sb) getnbyte(sb + 0x0a, 4)
  0x00, 0x00,
  0x00, 0x00,			/* 0x0e *//* Width */
#define set_WD_width(sb, val) putnbyte(sb + 0x0e, val, 4)
#define get_WD_width(sb) getnbyte(sb + 0x0e, 4)
#define WD_width 10200
  0x00, 0x00,
  0x00, 0x00,			/* 0x12 *//* Length */
#define set_WD_length(sb, val) putnbyte(sb + 0x12, val, 4)
#define get_WD_length(sb) getnbyte(sb + 0x12, 4)
#define WD_length 13200
  0x00,				/* 0x16 *//* Brightness */
#define set_WD_brightness(sb, val) sb[0x16] = val
#define get_WD_brightness(sb)  sb[0x16]
#define WD_brightness 0x80
  0x00,				/* 0x17 *//* Threshold */
#define set_WD_threshold(sb, val) sb[0x17] = val
#define get_WD_threshold(sb)  sb[0x17]
#define WD_threshold 0x80
  0x00,				/* 0x18 *//* Contrast */
#define set_WD_contrast(sb, val) sb[0x18] = val
#define get_WD_contrast(sb) sb[0x18]
  0x05,				/* 0x19 *//* Image composition */
#define set_WD_composition(sb, val)  sb[0x19] = val
#define get_WD_composition(sb) sb[0x19]
/* lineart, halftone, greyscale, binary color, dither color, multi-color */
#define WD_comp_LA 0
#define WD_comp_HT 1
#define WD_comp_GS 2
#define WD_comp_BC 3
#define WD_comp_DC 4
#define WD_comp_MC 5
#define WD_comp_G4 10
#define WD_comp_G8 11
  0x08,				/* 0x1a *//* Bits/Pixel */
#define set_WD_bitsperpixel(sb, val) sb[0x1a] = val
#define get_WD_bitsperpixel(sb)	sb[0x1a]
  0x00, 0x00,			/* 0x1b *//* Halftone pattern */
#define set_WD_halftone(sb, val) putnbyte(sb + 0x1b, val, 2)
#define get_WD_halftone(sb)	getnbyte(sb + 0x1b, 2)
  0x00,
/* 0x1d *//*************** STUFF ***************/
#define set_WD_rif(sb, val) setbitfield(sb + 0x1d, 1, 7, val)
#define get_WD_rif(sb)	getbitfield(sb + 0x1d, 1, 7)
#define set_WD_pad(sb, val) setbitfield(sb + 0x1d, 7, 0, val)
#define get_WD_pad(sb)	getbitfield(sb + 0x1d, 7, 0)
  0x00, 0x00,			/* 0x1e *//* bit ordering */
#define set_WD_bitorder(sb, val) putnbyte(sb + 0x1e, val, 2)
#define get_WD_bitorder(sb)	getnbyte(sb + 0x1e, 2)
  0x00,				/* 0x20 *//* compression type */
#define set_WD_compress_type(sb, val)  sb[0x20] = val
#define get_WD_compress_type(sb) sb[0x20]
  0x00,				/* 0x21 *//* compression argument */
#define set_WD_compress_arg(sb, val)  sb[0x21] = val
#define get_WD_compress_arg(sb) sb[0x21]
  0x00, 0x00,
  0x00, 0x00,
  0x00, 0x00,			/* 0x22 *//* reserved */
  0x00,				/* 0x28 *//* vendor id code */
#define set_WD_vendor_id_code(sb, val)  sb[0x28] = val
#define get_WD_vendor_id_code(sb) sb[0x28]
#define WD_bw 0x00
#define WD_color 0xFF
  0x00,				/* 0x29 C *//* parm length */
#define set_WD_parm_length(sb, val)  sb[0x29] = val
#define get_WD_parm_length(sb) sb[0x29]
  0x00,				/* 0x2a C *//* ADF, source, Color */
#define set_WD_adf(sb, val) setbitfield(sb + 0x2a, 1, 7, val)
#define get_WD_adf(sb)	getbitfield(sb + 0x2a, 1, 7)
#define set_WD_source(sb, val) setbitfield(sb + 0x2a, 1, 6, val)
#define get_WD_source(sb)	getbitfield(sb + 0x2a, 1, 6)
#define set_WD_color(sb, val) setbitfield(sb + 0x2a, 7, 3, val)
#define get_WD_color(sb)	getbitfield(sb + 0x2a, 7, 3)
#define WD_color_green 0
#define WD_color_red 1
#define WD_color_greenx 2
#define WD_color_blue 3
#define WD_color_rgb 4
  0x00,				/* 0x2b C *//* highlight color */
#define set_WD_highlight_color(sb, val)  sb[0x2b] = val
#define get_WD_highlight_color(sb) sb[0x2b]
  0x00,				/* 0x2c C *//* shadow value */
#define set_WD_shadow_value(sb, val)  sb[0x2c] = val
#define get_WD_shadow_value(sb) sb[0x2c]
  0x00, 0x00,			/* 0x2d C *//* line width */
#define set_WD_line_width(sb, val) putnbyte(sb + 0x2d, val, 2)
#define get_WD_line_width(sb)	getnbyte(sb + 0x2d, 2)
  0x00, 0x00,			/* 0x2f C *//* line count */
#define set_WD_line_count(sb, val) putnbyte(sb + 0x2f, val, 2)
#define get_WD_line_count(sb)	getnbyte(sb + 0x2f, 2)
  0x00,				/* 0x31 *//* reserved */
  0x00,				/* 0x32 *//* reserved */
  0x00,				/* 0x33 *//* reserved */
  0x00,				/* 0x34 *//* reserved */
  0x00,				/* 0x35 *//* paper size */
#define set_WD_paper_size(sb, val)  sb[0x35] = val
#define get_WD_paper_size(sb) sb[0x35]
  0x00, 0x00,
  0x00, 0x00,			/* 0x36 BW *//* paper width X */
#define set_WD_paper_width_X(sb, val) putnbyte(sb + 0x36, val, 4)
#define get_WD_paper_width_X(sb)	getnbyte(sb + 0x36, 4)
  0x00, 0x00,
  0x00, 0x00,			/* 0x3a BW *//* paper length Y */
#define set_WD_paper_length_Y(sb, val) putnbyte(sb+0x3a, val, 4)
#define get_WD_paper_length_Y(sb)	getnbyte(sb + 0x3a, 4)
  0x00,				/* 0x3e *//* reserved */
  0x00,				/* 0x3f *//* reserved */
		/* 0x40 (last) */
};

static scsiblk window_descriptor_blockB =
{window_descriptor_blockC, sizeof (window_descriptor_blockC)};

/* ==================================================================== */

#if 0
#define set_WDB_length(wdb,len)   (wdb.size = (len))
#define WDB_OFF(b)              (b + set_window.size)
#define WDB_OFF(b, n)            (b + set_window.size + \
		                 window_parameter_data_block.size + \
		                 ( window_descriptor_block.size * (n - 1) ) )
#define set_WDPB_wdbnum(sb,n) set_WPDB_wdblen(sb,window_descriptor_block.size*n)
#endif

/* ==================================================================== */

static scsiblk __sane_unused__ *lint_catcher[] =
{&reserve_unitB,
 &release_unitB,
 &inquiryB,
 &request_senseB,
 &send_diagnosticB,
 &test_unit_readyB,
 &set_windowB,
 &set_subwindowB,
 &object_positionB,
 &media_checkB,
 &media_parameter_data_blockB,
 &sendB,
 &readB,
 &mode_selectB,
 &mode_senseB,
 &scanB,
 &window_parameter_data_blockB,
 &window_descriptor_blockB};

#endif /* SP15C_SCSI_H */
