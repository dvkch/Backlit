
/* ------------------------------------------------------------------------- */

/* coolscan-scsidef.h: scsi-definiton header file for COOLSCAN scanner driver.

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

/* ------------------------------------------------------------------------- */

#ifndef COOLSCAN_SCSIDEF_H
#define COOLSCAN_SCSIDEF_H

/* ========================================================================= */

/* I'm using functions derived from Eric Youngdale's scsiinfo
 * program here for dealing with parts of SCSI commands.
 */

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

/* Not all of these are defined in scsi.h, so we'll make sure
 * we agree about them here...
 */
#undef  WRITE_BUFFER		/* correct size write_buffer for scanner */
#define TEST_UNIT_READY         0x00
#define REQUEST_SENSE           0x03
#define INQUIRY                 0x12
#define MODE_SELECT		0x15
#define RESERVE_UNIT            0x16
#define RELEASE_UNIT            0x17
#define MODE_SENSE		0x1a
#define SCAN                    0x1b
#define SEND_DIAGNOSTIC		0x1d
#define SET_WINDOW              0x24
#define GET_WINDOW              0x25
#define READ                    0x28
#define SEND                    0x2a
#define OBJECT_POSITION         0x31
#define WRITE_BUFFER            0x3b
#define READ_BUFFER	        0x3c
#define SABORT			0xc0
#define COMMAND_C1		0xc1
#define AUTO_FOCUS		0xc2
#define UNIT_MOVE		0xe0


/* ==================================================================== */

/* wdb_len if nothing is set by inquiry */
#define STD_WDB_LEN 0x28

/* ==================================================================== */
/* SCSI commands */

typedef struct
{
  unsigned char *cmd;
  int size;
}
scsiblk;

/* ==================================================================== */

#define set_inquiry_return_size(icb,val)             icb[0x04]=val
static unsigned char inquiryC[] =
{INQUIRY, 0x00, 0x00, 0x00, 0x1f, 0x00};
static scsiblk inquiry =
{inquiryC, sizeof (inquiryC)};

#define get_inquiry_periph_qual(in)             getbitfield(in, 0x07, 5)
#define IN_periph_qual_lun                    0x00
#define IN_periph_qual_nolun                  0x03
#define get_inquiry_periph_devtype(in)          getbitfield(in, 0x1f, 0)
#define IN_periph_devtype_scanner             0x06
#define IN_periph_devtype_unknown             0x1f
#define get_inquiry_response_format(in)         getbitfield(in + 0x03, 0x07, 0)
#define IN_recognized                         0x02
#define get_inquiry_additional_length(in)       in[0x04]
#define get_inquiry_length(in)                  in[0x03]
#define set_inquiry_length(out,n)               out[0x04]=n-5

#define get_inquiry_vendor(in, buf)             strncpy(buf, in + 0x08, 0x08)
#define get_inquiry_product(in, buf)            strncpy(buf, in + 0x10, 0x010)
#define get_inquiry_version(in, buf)            strncpy(buf, in + 0x20, 0x04)


/* ==================================================================== */
/*
   static unsigned char mode_selectC[] = {
   MODE_SELECT, 0x10, 0x00, 0x00, 0x00, 0x00    

   static scsiblk mode_select = { mode_selectC,sizeof(mode_selectC) };
 */
/* ==================================================================== */

static unsigned char test_unit_readyC[] =
{
  TEST_UNIT_READY, 0x00, 0x00, 0x00, 0x00, 0x00
};

static scsiblk test_unit_ready =
{test_unit_readyC, sizeof (test_unit_readyC)};

/* ==================================================================== */

static unsigned char reserve_unitC[] =
{
  RESERVE_UNIT, 0x00, 0x00, 0x00, 0x00, 0x00
};

static scsiblk reserve_unit =
{reserve_unitC, sizeof (reserve_unitC)};

/* ==================================================================== */

static unsigned char release_unitC[] =
{
  RELEASE_UNIT, 0x00, 0x00, 0x00, 0x00, 0x00
};

static scsiblk release_unit =
{release_unitC, sizeof (release_unitC)};

/* ==================================================================== */

static unsigned char mode_senseC[] =
{
  MODE_SENSE, 0x18, 0x03, 0x00, 0x00, 0x00,	/* PF set, page type 03 */
};
#define set_MS_DBD(b, val)  setbitfield(b, 0x01, 3, (val?1:0))
#define set_MS_len(b, val)	putnbyte(b+0x04, val, 1)
#define get_MS_MUD(b)		getnbyte(b+(0x04+((int)*(b+0x3)))+0x4,2)

static scsiblk mode_sense =
{mode_senseC, sizeof (mode_senseC)};

/* ==================================================================== */

static unsigned char set_windowC[] =
{
  SET_WINDOW, 0x00,		/* opcode, lun */
  0x00, 0x00, 0x00, 0x00,	/* reserved */
  0x00, 0x00, 0x00,		/* transfer length; needs to be set */
  0x00,				/* control byte */
};
#define set_SW_xferlen(sb, len) putnbyte(sb + 0x06, len, 3)

static scsiblk set_window =
{set_windowC, sizeof (set_windowC)};

/* ==================================================================== */

static unsigned char get_windowC[] =
{
  GET_WINDOW, 0x01,		/* opcode, lun, misc (should be 0x01? */
  0x00, 0x00, 0x00,		/* reserved */
  0x00,				/* Window identifier */
  0x00, 0x00, 0x00,		/* transfer length; needs to be get */
  0x00,				/* control byte */
};
#define set_GW_xferlen(sb, len) putnbyte(sb + 0x06, len, 3)
#define set_WindowID_wid(sb, val) sb[5] = val

static scsiblk get_window =
{get_windowC, sizeof (get_windowC)};

/* ==================================================================== */

/* We use the same structure for both SET WINDOW and GET WINDOW. */
static unsigned char window_parameter_data_blockC[] =
{
  0x00, 0x00,			/* window data length, in case of get, or 0 in case of set */
  0x00, 0x00, 0x00, 0x00,	/* reserved */
  0x00, 0x00,			/* Window Descriptor Length, value must be
				   set by SHORT_WDB define */
};
#define set_WPDB_wdblen(sb, len) putnbyte(sb + 0x06, len, 2)


static scsiblk window_parameter_data_block =
{
  window_parameter_data_blockC, sizeof (window_parameter_data_blockC)
};


/* ==================================================================== */

static unsigned char window_descriptor_blockC[] =
{
/* Any field maked with 'R' (e.g. R0x55) indicate a filed provided for
 * development. In normal operation, 0 is set here. If any other value is set,
 * operationis not guaranteed! */

#define max_WDB_size 0xff
#define used_WDB_size 0x75

  0x00,				/* 0x00 */
				/* Window Identifier */
#define set_WD_wid(sb, val) sb[0] = val
#define WD_wid_all 0x00		/* Only one supported */
  0x00,				/* 0x01 */
				/* reserved, AUTO */
#define set_WD_auto(sb, val) setbitfield(sb + 0x01, 1, 0, val)
#define get_WD_auto(sb)	getbitfield(sb + 0x01, 1, 0)
  0x00, 0x00,			/* 0x02 */
				/* X Resolution in dpi */
#define set_WD_Xres(sb, val) putnbyte(sb + 0x02, val, 2)
#define get_WD_Xres(sb)	getnbyte(sb + 0x02, 2)
  0x00, 0x00,			/* 0x04 */
				/* Y Resolution in dpi */
#define set_WD_Yres(sb, val) putnbyte(sb + 0x04, val, 2)
#define get_WD_Yres(sb)	getnbyte(sb + 0x04, 2)
  0x00, 0x00, 0x00, 0x00,	/* 0x06 */
  /* Upper Left X in 1200|2700pt/inch */
#define set_WD_ULX(sb, val) putnbyte(sb + 0x06, val, 4)
#define get_WD_ULX(sb) getnbyte(sb + 0x06, 4)
  0x00, 0x00, 0x00, 0x00,	/* 0x0a */
  /* Upper Left Y in 1200|2700pt/inch */
#define set_WD_ULY(sb, val) putnbyte(sb + 0x0a, val, 4)
#define get_WD_ULY(sb) getnbyte(sb + 0x0a, 4)
  0x00, 0x00, 0x00, 0x00,	/* 0x0e */
  /* Width 1200pt/inch */
#define set_WD_width(sb, val) putnbyte(sb + 0x0e, val, 4)
#define get_WD_width(sb) getnbyte(sb + 0x0e, 4)
  0x00, 0x00, 0x00, 0x00,	/* 0x12 */
  /* Length 1200pt/inch */
#define set_WD_length(sb, val) putnbyte(sb + 0x12, val, 4)
#define get_WD_length(sb) getnbyte(sb + 0x12, 4)
  0x00,				/* 0x16 */
  /* Brightness */
#define set_WD_brightness(sb, val) sb[0x16] = val
#define get_WD_brightness(sb)  sb[0x16]
  0x00,				/* 0x17 */
				/* Reserved */
  0x00,				/* 0x18 */
				/* Contrast */
#define set_WD_contrast(sb, val) sb[0x18] = val
#define get_WD_contrast(sb) sb[0x18]
  0x05,				/* 0x19 */
				/* Image Mode */
#define set_WD_composition(sb, val)  sb[0x19] = val
#define get_WD_composition(sb) sb[0x19]
#define     WD_comp_grey          0x02
#define     WD_comp_gray          0x02
#define     WD_comp_rgb_full      0x05
  0x08,				/* 0x1a */				/* Bits/Pixel */
#define set_WD_bitsperpixel(sb, val) sb[0x1a] = val
#define get_WD_bitsperpixel(sb)	sb[0x1a]
#define WD_bits_8    0x08
  0, 0, 0, 0, 0,		/* 0x1b */
   /* Reserved */
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0,			/* 0x28 */
   /* X-axis pixel count (1-2592 */
#define get_WD_xpixels(sb) getnbyte(sb + 0x28, 4)
  0, 0, 0, 0,			/* 0x2c */
 /* Y-axis pixel count (1-3888) */
#define get_WD_ypixels(sb) getnbyte(sb + 0x2c, 4)
  0x01,				/* 0x30 */
				/* Reserved, negative/positive
				 * reserved, drop-out color
				 * (Green) */
#define set_WD_negative(sb, val) setbitfield(sb + 0x30, 0x1, 4, (val?1:0))
#define get_WD_negative(sb) getbitfield(sb + 0x30, 0x1, 4)
#define WD_Negative 0x01
#define WD_Positive 0x00
#define set_WD_dropoutcolor(sb, val) setbitfield(sb + 0x30, 0x3, 0, val)
#define get_WD_dropoutcolor(sb)	getbitfield(sb + 0x30, 0x3, 0)
#define WD_Dropout_Red 0x00
#define WD_Dropout_Green 0x01
#define WD_Dropout_Blue 0x02
  0x00,				/* 0x31 */
  /* scan mode */
#define set_WD_scanmode(sb, val) setbitfield(sb + 0x31, 0x3, 4, val)
#define get_WD_scanmode(sb) getbitfield(sb + 0x31, 0x3, 4)
#define WD_Scan 0x00
#define WD_Prescan 0x01
  0x40,				/* 0x32 */
				/* Data transfer mode */
#define set_WD_transfermode(sb, val) setbitfield(sb + 0x32, 0x3, 6, val)
#define get_WD_transfermode(sb)	getbitfield(sb + 0x32, 0x3, 6)
#define WD_LineSequence	0x2
#define WD_DotSequence	0x1
  0x02,				/* 0x33 */
				/* Gamma selection */
#define set_WD_gammaselection(sb, val)	putnbyte(sb + 0x33, val, 1)
#define get_WD_gammaselection(sb) getnbyte(sb + 0x33, 1)
#define WD_Linear	0x2
#define WD_Monitor	0x3
  0,				/* 0x34 */
   /* Reserved */
  0x40,				/* 0x35 */
   /* Reserved, shading, analog gamma, averaging */
#define set_WD_shading(sb, val)	setbitfield(sb + 0x35, 0x1, 6, val)
#define get_WD_shading(sb) getbitfield(sb + 0x35, 0x1, 6)
#define WD_Shading_ON	0x0
#define WD_Shading_OFF	0x1
#define set_WD_analog_gamma_R(sb, val) setbitfield(sb + 0x35, 0x1, 5, val)
#define set_WD_analog_gamma_G(sb, val) setbitfield(sb + 0x35, 0x1, 4, val)
#define set_WD_analog_gamma_B(sb, val) setbitfield(sb + 0x35, 0x1, 3, val)
#define get_WD_analog_gamma_R(sb) getbitfield(sb + 0x35, 0x1, 5)
#define get_WD_analog_gamma_G(sb) getbitfield(sb + 0x35, 0x1, 4)
#define get_WD_analog_gamma_B(sb) getbitfield(sb + 0x35, 0x1, 3)
#define WD_Analog_Gamma_ON	0x0
#define WD_Analog_Gamma_OFF	0x1
#define set_WD_averaging(sb, val) setbitfield(sb + 0x35, 0x7, 0, (val?7:0))
#define get_WD_averaging(sb) getbitfield(sb + 0x35, 0x7, 0)
#define WD_Averaging_ON	0x0
#define WD_Averaging_OFF	0x1

  0,				/* 0x36 */
  /* reserved */
  0,				/* 0x37 */
  /* R brightness */
#define set_WD_brightness_R(b, val)  putnbyte(b + 0x37, val, 1)
#define get_WD_brightness_R(b)  getnbyte(b + 0x37, 1)
  0,				/* 0x38 */
				/* G brightness */
#define set_WD_brightness_G(b, val)  putnbyte(b + 0x38, val, 1)
#define get_WD_brightness_G(b)  getnbyte(b + 0x38, 1)
  0,				/* 0x39 */
				/* B brightness */
#define set_WD_brightness_B(b, val)  putnbyte(b + 0x39, val, 1)
#define get_WD_brightness_B(b)  getnbyte(b + 0x39, 1)
  0,				/* 0x3a */
				/* R contrast */
#define set_WD_contrast_R(b, val) putnbyte(b + 0x3a, val, 1)
#define get_WD_contrast_R(b) getnbyte(b + 0x3a, 1)
  0,				/* 0x3b */
  /* G contrast */
#define set_WD_contrast_G(b, val) putnbyte(b + 0x3b, val, 1)
#define get_WD_contrast_G(b) getnbyte(b + 0x3b, 1)
  0,				/* 0x3c */
  /* B contrast */
#define set_WD_contrast_B(b, val) putnbyte(b + 0x3c, val, 1)
#define get_WD_contrast_B(b) getnbyte(b + 0x3c, 1)
  0, 0, 0, 0,			/* 0x3d */
					/* Reserved */
  0, 0, 0, 0, 0, 0, 0, 0,
  0,				/* 0x49 */
  /* R exposure time adjustment [0, 12-200] */
#define set_WD_exposure_R(b, val) putnbyte(b + 0x49, val, 1)
#define get_WD_exposure_R(b) getnbyte(b + 0x49, 1)
  0,				/* 0x4a */
  /* G exposure time adjustment [0, 12-200] */
#define set_WD_exposure_G(b, val) putnbyte(b + 0x4a, val, 1)
#define get_WD_exposure_G(b) getnbyte(b + 0x4a, 1)
  0,				/* 0x4b */
  /* B exposure time adjustment [0, 12-200] */
#define set_WD_exposure_B(b, val) putnbyte(b + 0x4b, val, 1)
#define get_WD_exposure_B(b) getnbyte(b + 0x4b, 1)
  0, 0, 0, 0,			/* 0x4c */
  /* Reserved */
  0, 0,
  0,				/* 0x52 */
  /* Amount of R shift [0, 128+-15] */
#define set_WD_shift_R(b, val)	putnbyte(b + 0x52, val, 1)
#define get_WD_shift_R(b) getnbyte(b + 0x52, 1)
  0,				/* 0x53 */
				/* Amount of G shift [0, 128+-15] */
#define set_WD_shift_G(b, val) putnbyte(b + 0x53, val, 1)
#define get_WD_shift_G(b) getnbyte(b + 0x53, 1)
  0,				/* 0x54 */
				/* Amount of B shift [0, 128+-15] */
#define set_WD_shift_B(b, val) putnbyte(b + 0x54, val, 1)
#define get_WD_shift_B(b) getnbyte(b + 0x54, 1)
  0,				/* R0x55 */
				/* Amount of R offset [0-255] */
  0,				/* R0x56 */
				/* Amount of G offset [0-255] */
  0,				/* R0x57 */
				/* Amount of B offset [0-255] */
  0, 0,				/* 0x58 */
  /* Maximum resolution (for GET WINDOW: [2700]) */
#define get_WD_maxres(b) getnbyte(b + 0x58, 2)
  0, 0,				/* 0x5a */
				/* Reserved */
  0,				/* 0x5c */
				/* LUT-R, LUT-G */
#define set_WD_LUT_R(b, val) setbitfield(b + 0x5c, 0x0f, 4, val)
#define set_WD_LUT_G(b, val) setbitfield(b + 0x5c, 0x0f, 0, val)
#define get_WD_LUT_R(b)	getbitfield(b + 0x5c, 0x0f, 4)
#define get_WD_LUT_G(b)	getbitfield(b + 0x5c, 0x0f, 0)
  0,				/* 0x5d */
				/* LUT-B, reserved */
#define set_WD_LUT_B(b, val) setbitfield(b + 0x5d, 0x0f, 4, val)
#define get_WD_LUT_B(b)	getbitfield(b + 0x5d, 0x0f, 4)
  0,				/* R0x5e */
  /* LS-1000: reserved. LS-20: R B/W reference point */
  0,				/* R0x5f */
  /* LS-1000: reserved. LS-20: G B/W reference point */
  0,				/* R0x60 */
  /* LS-1000: reserved. LS-20: B B/W reference point */
  0,				/* R0x61 */
  /* R exposure time unit [0-7] (LS-1000); [0, 2-1] (LS-20) */
  0,				/* R0x62 */
  /* G exposure time unit [0-7] (LS-1000); [0, 2-1] (LS-20) */
  0,				/* R0x63 */
  /* B exposure time unit [0-7] (LS-1000); [0, 2-1] (LS-20) */
  0,				/* 0x64 */
  /* Reserved */
  0,				/* 0x65 */
    /* Reserved, stop */
#define set_WD_stop(b, val) setbitfield(b+0x65, 0x01, 0, val)
#define get_WD_stop(b)	getbitfield(b+0x65, 0x01, 0)
  0,				/* R0x66 */
  /* R gain [0-4] (LS-1000), [0-255] (LS-20) */
  0,				/* R0x67 */
  /* G gain [0-4] (LS-1000), [0-255] (LS-20) */
  0,				/* R0x68 */
  /* B gain [0-4] (LS-1000), [0-255] (LS-20) */
  0, 0, 0, 0,			/* R0x69 */
  /* R exposure time variable [0, 64-65535] */
  0, 0, 0, 0,			/* R0x6d */
  /* G exposure time variable [0, 64-65535] */
  0, 0, 0, 0,			/* R0x71 */
    /* B exposure time variable [0, 64-65535] */
  /* 0x75 (last) */

};

static scsiblk window_descriptor_block =
{
  window_descriptor_blockC, sizeof (window_descriptor_blockC)
};



/* LS-30 has different window-descriptor ! 
 */

static unsigned char window_descriptor_blockC_LS30[] =
{

#define used_WDB_size_LS30 0x32

  0x00,				/* 0x00 */
				/* Window Identifier */
#define WD_wid_0 0x00		/* Only one supported */
#define WD_wid_1 0x01		
#define WD_wid_2 0x02		
#define WD_wid_3 0x03		
#define WD_wid_4 0x04		
#define WD_wid_9 0x09	
  0x00,			        /* reserved, AUTO */

  0x00, 0x00,			/* 0x02 */
				/* X Resolution in dpi */

  0x00, 0x00,			/* 0x04 */
				/* Y Resolution in dpi */

  0x00, 0x00, 0x00, 0x00,	/* 0x06 */
  /* Upper Left X in 2700pt/inch */
  0x00, 0x00, 0x00, 0x00,	/* 0x0a */
  /* Upper Left Y in 2700pt/inch */
  0x00, 0x00, 0x00, 0x00,	/* 0x0e */
  /* Width 1200pt/inch */
  0x00, 0x00, 0x00, 0x00,	/* 0x12 */
  /* Length 1200pt/inch */
  0x00,				/* 0x16 */
  /* Brightness */
  0x00,				/* 0x17 */
				/* Reserved */
  0x00,				/* 0x18 */
				/* Contrast */
  0x05,				/* 0x19 */
				/* Image Mode */
  0x08,				/* 0x1a */
				/* Bits/Pixel */
#define WD_bits_10    0x0a
  0, 0, 0, 0, 0,		/* 0x1b */
   /* Reserved */
  0, 0, 0, 0, 0, 0, 0, 0, 0,    /* 0x20 */
  0,  		               	/* 0x29 */
   /* Negative/positive prevue/scan */
#define set_WD_negative_LS30(sb, val) setbitfield(sb + 0x29, 0x1, 0, (val?0:1))
#define get_WD_negative_LS30(sb) getbitfield(sb + 0x29, 0x1, 0)
  /* scan mode */
#define set_WD_scanmode_LS30(sb, val) setbitfield(sb + 0x29, 0x3, 0, val)
#define get_WD_scanmode_LS30(sb) getbitfield(sb + 0x29, 0x3, 0)

  0x04,                          /* 0x2a */
  0x02,                          /* 0x2b */
  0x01,                          /* 0x2c */
  0xff,                          /* 0x2d */
  0,0,                           /* 0x2e */
  0,0,                           /* 0x30 */
#define set_gain_LS30(sb, val) putnbyte(sb + 0x2e, val, 4)
#define get_gain_LS30(sb) getnbyte(sb + 0x2e, 4)
};


static scsiblk window_descriptor_block_LS30 =
{
  window_descriptor_blockC, sizeof (window_descriptor_blockC_LS30)
};

/* ==================================================================== */

/*#define set_WDB_length(length)   (window_descriptor_block.size = (length)) */
#define WPDB_OFF(b)              (b + set_window.size)
#define WDB_OFF(b, n)            (b + set_window.size + \
		                 window_parameter_data_block.size + \
		                 ( window_descriptor_block.size * (n - 1) ) )
#define set_WPDB_wdbnum(sb,n) set_WPDB_wdblen(sb,window_descriptor_block.size*n)



/* ==================================================================== */

static unsigned char scanC[] =
{
  SCAN, 0x00, 0x00, 0x00, 0x00, 0x00
};

static scsiblk scan =
{scanC, sizeof (scanC)};
#define set_SC_xfer_length(sb, val) sb[0x04] = (unsigned char)val

/* ==================================================================== */
/*
   static unsigned char send_diagnosticC[] = {
   SEND_DIAGNOSTIC, 0x04, 0x00, 0x00, 0x00, 0x00
   };

   static scsiblk send_diagnostic = { send_diagnosticC, sizeof(send_diagnosticC) }; */

/* ==================================================================== */

/* sread instead of read because read is a libc primitive */
static unsigned char sreadC[] =
{
  READ, 0x00,
  0x00,				/* Data Type Code */
  0x00,				/* reserved */
  0x00, 0x00,			/* data type qualifier */
  0x00, 0x00, 0x00,		/* transfer length */
  0x00				/* control */
};

static scsiblk sread =
{sreadC, sizeof (sreadC)};
#define set_R_data1_code(sb, val) sb[0x01] = val
#define set_R_datatype_code(sb, val) sb[0x02] = val
#define R_datatype_imagedata		0x00
#define R_EX_datatype_LUT		0x01	/* Experiment code */
#define R_image_positions		0x88
#define R_EX_datatype_shading_data	0xa0	/* Experiment code */
#define R_user_reg_gamma		0xc0
#define R_device_internal_info		0xe0
#define set_R_datatype_qual_upper(sb, val) sb[0x04] = val
#define set_R_datatype_qual_lower(sb, val) sb[0x05] = val
#define R_DQ_none	0x00
#define R_DQ_Rcomp	0x06
#define R_DQ_Gcomp	0x07
#define R_DQ_Bcomp	0x08
#define R_DQ_Reg1	0x01
#define R_DQ_Reg2	0x02
#define R_DQ_Reg3	0x03
#define set_R_xfer_length(sb, val)    putnbyte(sb + 0x06, val, 3)

/* ==================================================================== */

/* Length of internal info structure */
#define DI_length 256
/* Functions for picking out data from the internal info structure */
#define get_DI_ADbits(b)	   getnbyte(b + 0x00, 1)
#define get_DI_Outputbits(b)	   getnbyte(b + 0x01, 1)
#define get_DI_MaxResolution(b)	   getnbyte(b + 0x02, 2)
#define get_DI_Xmax(b)		   getnbyte(b + 0x04, 2)
#define get_DI_Ymax(b)		   getnbyte(b + 0x06, 2)
#define get_DI_Xmaxpixel(b)	   getnbyte(b + 0x08, 2)
#define get_DI_Ymaxpixel(b)	   getnbyte(b + 0x0a, 2)
#define get_DI_currentY(b)	   getnbyte(b + 0x10, 2)
#define get_DI_currentFocus(b)	   getnbyte(b + 0x12, 2)
#define get_DI_currentscanpitch(b) getnbyte(b + 0x14, 1)
#define get_DI_autofeeder(b)	   getnbyte(b + 0x1e, 1)
#define get_DI_analoggamma(b)	   getnbyte(b + 0x1f, 1)
#define get_DI_deviceerror0(b)	   getnbyte(b + 0x40, 1)
#define get_DI_deviceerror1(b)	   getnbyte(b + 0x41, 1)
#define get_DI_deviceerror2(b)	   getnbyte(b + 0x42, 1)
#define get_DI_deviceerror3(b)	   getnbyte(b + 0x43, 1)
#define get_DI_deviceerror4(b)	   getnbyte(b + 0x44, 1)
#define get_DI_deviceerror5(b)	   getnbyte(b + 0x45, 1)
#define get_DI_deviceerror6(b)	   getnbyte(b + 0x46, 1)
#define get_DI_deviceerror7(b)	   getnbyte(b + 0x47, 1)
#define get_DI_WBETR_R(b)	   getnbyte(b + 0x80, 2)	/* White balance exposure time variable R */
#define get_DI_WBETR_G(b)	    getnbyte(b + 0x82, 2)
#define get_DI_WBETR_B(b)	    getnbyte(b + 0x84, 2)
#define get_DI_PRETV_R(b)	    getnbyte(b + 0x88, 2)	/* Prescan result exposure tim4e variable R */
#define get_DI_PRETV_G(b)	    getnbyte(b + 0x8a, 2)
#define get_DI_PRETV_B(b)	    getnbyte(b + 0x8c, 2)
#define get_DI_CETV_R(b)	    getnbyte(b + 0x90, 2)	/* Current exposure time variable R */
#define get_DI_CETV_G(b)	    getnbyte(b + 0x92, 2)
#define get_DI_CETV_B(b)	    getnbyte(b + 0x94, 2)
#define get_DI_IETU_R(b)	    getnbyte(b + 0x98, 1)	/* Internal exposure time unit R */
#define get_DI_IETU_G(b)	    getnbyte(b + 0x99, 1)
#define get_DI_IETU_B(b)	    getnbyte(b + 0x9a, 1)
#define get_DI_limitcondition(b)    getnbyte(b + 0xa0, 1)
#define get_DI_offsetdata_R(b)	    getnbyte(b + 0xa1, 1)
#define get_DI_offsetdata_G(b)	    getnbyte(b + 0xa2, 1)
#define get_DI_offsetdata_B(b)	    getnbyte(b + 0xa3, 1)
#define get_DI_poweron_errors(b,to) memcpy(to, (b + 0xa8), 8)

/* ==================================================================== */

static unsigned char sendC[] =
{
  SEND, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static scsiblk send =
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
#define S_DQ_Reg9	0x09
#define set_S_xfer_length(sb, val)    putnbyte(sb + 0x06, val, 3)

/*
   static unsigned char gamma_user_LUT_LS1K[512] = { 0x00 };
   static scsiblk gamma_user_LUT_LS1K_LS1K = {
   gamma_user_LUT_LS1K, sizeof(gamma_user_LUT_LS1K)
   };
 */

/* ==================================================================== */

static unsigned char object_positionC[] =
{
  OBJECT_POSITION,
  0x00,				/* Auto feeder function */
  0x00, 0x00, 0x00,		/* Count */
  0x00, 0x00, 0x00, 0x00,	/* Reserved */
  0x00				/* Control byte */
};
#define set_OP_autofeed(b,val) setbitfield(b+0x01, 0x07, 0, val)
#define OP_Discharge		0x00
#define OP_Feed			0x01
#define OP_Absolute		0x02	/* For development only */

static scsiblk object_position =
{
  object_positionC, sizeof (object_positionC)
};
/* ==================================================================== */
static unsigned char autofocusC[] =
{
  AUTO_FOCUS, 0x00, 0x00, 0x00,
  0x00,				/* transfer length (0|8) */
  0x00				/* Control byte */
};
#define set_AF_transferlength(b, val)  b[0x04] = (unsigned char)val
#define get_AF_transferlength(b)  ((int)b[0x04] & 0xff)
#define set_AF_XPoint(b, val) putnbyte(b+0x06, val, 4)
#define set_AF_YPoint(b, val) putnbyte(b+0x0a, val, 4)
#define AF_Point_length		8

static scsiblk autofocus =
{autofocusC, sizeof (autofocusC)};

/* ==================================================================== */

static unsigned char command_c1_C[] =
{
  0xc1, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,	       /*  */
  0x00, 0x00			       /* transfer length*/
};
static scsiblk command_c1 =
{command_c1_C, sizeof (command_c1_C)};
/* ==================================================================== */

static unsigned char autofocusLS30C[] =
{
  0xe0, 0x00, 0xa0, 0x00,
  0x00, 0x00, 0x00, 0x00,	       /*  */
  0x09, 0x00			       /* transfer length*/
};

static unsigned char autofocuspos[] =
{
  0x00, 
  0x00, 0x00, 0x05, 0x10,              /* x-position */
  0x00, 0x00, 0x07, 0x9b,	       /* y-position */
};

#define set_AF_transferlength(b, val)  b[0x04] = (unsigned char)val
#define get_AF_transferlength(b)  ((int)b[0x04] & 0xff)
#define set_AF_XPoint(b, val) putnbyte(b+0x06, val, 4)
#define set_AF_YPoint(b, val) putnbyte(b+0x0a, val, 4)
#define AF_Point_length		8

static scsiblk autofocusLS30 =
{autofocusLS30C, sizeof (autofocusLS30C)};
/* ==================================================================== */

static unsigned char commande1C[] =
{
  0xe1, 0x00, 0xc1, 0x00,
  0x00, 0x00, 0x00, 0x00,	       /*  */
  0x0d, 0x00			       /* transfer length*/
};
static scsiblk commande1 =
{commande1C, sizeof (commande1C)};

/* ==================================================================== */
/*
static unsigned char request_senseC[] =
{
  REQUEST_SENSE, 0x00, 0x00, 0x00, 0x00, 0x00
#define set_RS_allocation_length(sb,val) sb[0x04] = (unsigned char)val
};

static scsiblk request_sense =
{
  request_senseC, sizeof (request_senseC)
};
*/
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

#endif
