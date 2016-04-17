#ifndef CANON_DR_CMD_H
#define CANON_DR_CMD_H

/* 
 * Part of SANE - Scanner Access Now Easy.
 * Please see opening comments in canon_dr.c
 */

/****************************************************/

#define USB_HEADER_LEN     12
#define USB_COMMAND_LEN    12
#define USB_STATUS_LEN     4
#define USB_COMMAND_TIME   30000
#define USB_DATA_TIME      30000
#define USB_STATUS_TIME    30000

/*static inline void */
static void
setbitfield (unsigned char *pageaddr, int mask, int shift, int val)
{
  *pageaddr = (*pageaddr & ~(mask << shift)) | ((val & mask) << shift);
}

/* ------------------------------------------------------------------------- */

/*static inline int */
static int
getbitfield (unsigned char *pageaddr, int mask, int shift)
{
  return ((*pageaddr >> shift) & mask);
}

/* ------------------------------------------------------------------------- */

static int
getnbyte (unsigned char *pnt, int nbytes)
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

/*static inline void */
static void
putnbyte (unsigned char *pnt, unsigned int value, unsigned int nbytes)
{
  int i;

#ifdef DEBUG
  assert (nbytes < 5);
#endif
  for (i = nbytes - 1; i >= 0; i--)

    {
      pnt[i] = value & 0xff;
      value = value >> 8;
    }
}

/* ==================================================================== */
/* extended status packet */
#define get_ES_length(b)             getnbyte(b+0x04, 4)

/* ==================================================================== */
/* SCSI commands */

#define set_SCSI_opcode(out, val)          out[0]=val
#define set_SCSI_lun(out, val)             setbitfield(out + 1, 7, 5, val)

/* ==================================================================== */
/* TEST_UNIT_READY */
#define TEST_UNIT_READY_code    0x00
#define TEST_UNIT_READY_len     6

/* ==================================================================== */
/* REQUEST_SENSE */
#define REQUEST_SENSE_code      0x03
#define REQUEST_SENSE_len       6

#define RS_return_size          0x0e
#define set_RS_return_size(icb,val)        icb[0x04]=val

/* defines for request sense return block */
#define get_RS_information_valid(b)       getbitfield(b + 0x00, 1, 7)
#define get_RS_error_code(b)              getbitfield(b + 0x00, 0x7f, 0)
#define get_RS_filemark(b)                getbitfield(b + 0x02, 1, 7)
#define get_RS_EOM(b)                     getbitfield(b + 0x02, 1, 6)
#define get_RS_ILI(b)                     getbitfield(b + 0x02, 1, 5)
#define get_RS_sense_key(b)               getbitfield(b + 0x02, 0x0f, 0)
#define get_RS_information(b)             getnbyte(b+0x03, 4)	/* normally 0 */
#define get_RS_additional_length(b)       b[0x07]	/* always 6? */
#define get_RS_ASC(b)                     b[0x0c]
#define get_RS_ASCQ(b)                    b[0x0d]
#define get_RS_SKSV(b)                    getbitfield(b+0x0f,1,7) /* valid=0 */
#define get_RS_SKSB(b)                    getnbyte(b+0x0f, 3)

/* when RS is 0x05/0x26 bad bytes listed in sksb */
/* #define get_RS_offending_byte(b)          getnbyte(b+0x10, 2) */

/* ==================================================================== */
/* INQUIRY */
#define INQUIRY_code            0x12
#define INQUIRY_len             6

#define INQUIRY_std_len         0x30
#define INQUIRY_vpd_len         0x1e

#define set_IN_evpd(icb, val)              setbitfield(icb + 1, 1, 0, val)
#define set_IN_page_code(icb, val)         icb[0x02]=val
#define set_IN_return_size(icb,val)        icb[0x04]=val

#define get_IN_periph_qual(in)             getbitfield(in, 0x07, 5)
#define IN_periph_qual_lun                    0x00
#define IN_periph_qual_nolun                  0x03
#define get_IN_periph_devtype(in)          getbitfield(in, 0x1f, 0)
#define IN_periph_devtype_scanner             0x06
#define IN_periph_devtype_unknown             0x1f
#define get_IN_response_format(in)         getbitfield(in + 0x03, 0x07, 0)
#define IN_recognized                         0x02
#define get_IN_vendor(in, buf)            strncpy(buf, (char *)in + 0x08, 0x08)
#define get_IN_product(in, buf)           strncpy(buf, (char *)in + 0x10, 0x010)
#define get_IN_version(in, buf)           strncpy(buf, (char *)in + 0x20, 0x04)

/* the VPD response */
#define get_IN_page_length(in)             in[0x04]
#define get_IN_basic_x_res(in)             getnbyte(in + 0x05, 2)
#define get_IN_basic_y_res(in)             getnbyte(in + 0x07, 2)
#define get_IN_step_x_res(in)              getbitfield(in+0x09, 1, 0)
#define get_IN_step_y_res(in)              getbitfield(in+0x09, 1, 4)
#define get_IN_max_x_res(in)               getnbyte(in + 0x0a, 2)
#define get_IN_max_y_res(in)               getnbyte(in + 0x0c, 2)
#define get_IN_min_x_res(in)               getnbyte(in + 0x0e, 2)
#define get_IN_min_y_res(in)               getnbyte(in + 0x10, 2)
#define get_IN_std_res_60(in)              getbitfield(in+ 0x12, 1, 7)
#define get_IN_std_res_75(in)              getbitfield(in+ 0x12, 1, 6)
#define get_IN_std_res_100(in)             getbitfield(in+ 0x12, 1, 5)
#define get_IN_std_res_120(in)             getbitfield(in+ 0x12, 1, 4)
#define get_IN_std_res_150(in)             getbitfield(in+ 0x12, 1, 3)
#define get_IN_std_res_160(in)             getbitfield(in+ 0x12, 1, 2)
#define get_IN_std_res_180(in)             getbitfield(in+ 0x12, 1, 1)
#define get_IN_std_res_200(in)             getbitfield(in+ 0x12, 1, 0)
#define get_IN_std_res_240(in)             getbitfield(in+ 0x13, 1, 7)
#define get_IN_std_res_300(in)             getbitfield(in+ 0x13, 1, 6)
#define get_IN_std_res_320(in)             getbitfield(in+ 0x13, 1, 5)
#define get_IN_std_res_400(in)             getbitfield(in+ 0x13, 1, 4)
#define get_IN_std_res_480(in)             getbitfield(in+ 0x13, 1, 3)
#define get_IN_std_res_600(in)             getbitfield(in+ 0x13, 1, 2)
#define get_IN_std_res_800(in)             getbitfield(in+ 0x13, 1, 1)
#define get_IN_std_res_1200(in)            getbitfield(in+ 0x13, 1, 0)
#define get_IN_window_width(in)            getnbyte(in + 0x14, 4)
#define get_IN_window_length(in)           getnbyte(in + 0x18, 4)
#define get_IN_awd(in)                     getbitfield(in+0x1c, 1, 7)
#define get_IN_ce_emphasis(in)             getbitfield(in+0x1c, 1, 6)
#define get_IN_c_emphasis(in)              getbitfield(in+0x1c, 1, 5)
#define get_IN_high_quality(in)            getbitfield(in+0x1c, 1, 4)
#define get_IN_multilevel(in)              getbitfield(in+0x1c, 1, 3)
#define get_IN_half_tone(in)               getbitfield(in+0x1c, 1, 2)
#define get_IN_monochrome(in)              getbitfield(in+0x1c, 1, 1)
#define get_IN_overflow(in)                getbitfield(in+0x1c, 1, 0)

/* some scanners need evpd inquiry data manipulated */
#define set_IN_page_length(in,val)             in[0x04]=val

/* ==================================================================== */
/* RESERVE_UNIT */
#define RESERVE_UNIT_code       0x16
#define RESERVE_UNIT_len        6

/* ==================================================================== */
/* RELEASE_UNIT */

#define RELEASE_UNIT_code       0x17
#define RELEASE_UNIT_len        6

/* ==================================================================== */
/* SCAN */
#define SCAN_code               0x1b
#define SCAN_len                6

#define set_SC_xfer_length(sb, val) sb[0x04] = (unsigned char)val

/* ==================================================================== */
/* SET_WINDOW */
#define SET_WINDOW_code         0x24
#define SET_WINDOW_len          10

#define set_SW_xferlen(sb, len) putnbyte(sb + 0x06, len, 3)

#define SW_header_len		8
#define SW_desc_len		0x2c

/* ==================================================================== */
/* GET_WINDOW */
#define GET_WINDOW_code         0x25
#define GET_WINDOW_len          0

/* ==================================================================== */
/* READ/SEND page codes */
#define SR_datatype_image		0x00
#define SR_datatype_lut                 0x03
#define SR_datatype_pixelsize  		0x80 /*DR-G1130*/
#define SR_datatype_panel  		0x84
#define SR_datatype_sensors  		0x8b
#define SR_datatype_counters 		0x8c
#define SR_datatype_endorser            0x90
#define SR_datatype_fineoffset          0x90
#define SR_datatype_finegain            0x91

/* ==================================================================== */
/* READ */
#define READ_code               0x28
#define READ_len                10

#define set_R_datatype_code(sb, val)   sb[0x02] = val
#define set_R_xfer_uid(sb, val)        sb[4] = val
#define set_R_xfer_lid(sb, val)        sb[5] = val
#define set_R_xfer_length(sb, val)     putnbyte(sb + 0x06, val, 3)

/*image needs no macros?*/

/*lut unread?*/

/*panel*/
#define R_PANEL_len                     0x08
#define get_R_PANEL_start(in)	        getbitfield(in, 1, 7)
#define get_R_PANEL_stop(in)	        getbitfield(in, 1, 6)
#define get_R_PANEL_butt3(in)	        getbitfield(in, 1, 2)
#define get_R_PANEL_new_file(in)	getbitfield(in+1, 1, 0)
#define get_R_PANEL_count_only(in)	getbitfield(in+1, 1, 1)
#define get_R_PANEL_bypass_mode(in)	getbitfield(in+1, 1, 2)
#define get_R_PANEL_enable_led(in)	getbitfield(in+2, 1, 0)
#define get_R_PANEL_counter(in)         getnbyte(in + 0x04, 4)

/*sensors*/
#define R_SENSORS_len                   0x01
#define get_R_SENSORS_card(in)	        getbitfield(in, 1, 3)
#define get_R_SENSORS_adf(in)	        getbitfield(in, 1, 0)

/*counters*/
#define R_COUNTERS_len                 0x80
#define get_R_COUNTERS_scans(in)       getnbyte(in + 0x04, 4)

/*endorser unread?*/

/*fine gain*/
#define R_FINE_uid_gray           0x07
#define R_FINE_uid_red            0x0c
#define R_FINE_uid_green          0x0a
#define R_FINE_uid_blue           0x09
#define R_FINE_uid_unknown        0x14

/* ==================================================================== */
/* SEND */
#define SEND_code               0x2a
#define SEND_len                10

#define set_S_xfer_datatype(sb, val) sb[0x02] = (unsigned char)val
#define set_S_xfer_id(sb, val)       putnbyte(sb + 4, val, 2)
#define set_S_xfer_length(sb, val)   putnbyte(sb + 6, val, 3)

/*lut*/
#define S_LUT_len                     0x100
#define S_LUT_id_front                0x82
#define S_LUT_id_unk1                 0x84
#define S_LUT_id_unk2                 0x88
#define S_LUT_id_unk3                 0x90

/*panel*/
#define S_PANEL_len                     0x08
#define set_S_PANEL_enable_led(in,val)	setbitfield(in+2, 1, 0, val)
#define set_S_PANEL_counter(sb,val)     putnbyte(sb + 0x04, val, 4)

/*counters*/
/*endorser*/

/* ==================================================================== */
/* OBJECT_POSITION */
#define OBJECT_POSITION_code    0x31
#define OBJECT_POSITION_len     10

#define set_OP_autofeed(b,val) setbitfield(b+0x01, 0x07, 0, val)
#define OP_Discharge	0x00
#define OP_Feed	        0x01

/* ==================================================================== */
/* Page codes used by GET/SET SCAN MODE */
#define SM_pc_adf                      0x01
#define SM_pc_tpu                      0x02
#define SM_pc_scan_ctl                 0x20

#define SM_pc_df                       0x30
#define SM_pc_buffer                   0x32
#define SM_pc_imprinter                0x34
#define SM_pc_dropout                  0x36
#define SM_pc_unknown                  0x37

#define SM_pc_all_pc                   0x3F

/* ==================================================================== */
/* GET SCAN MODE */
#define GET_SCAN_MODE_code               0xd5
#define GET_SCAN_MODE_len                6

#define set_GSM_unknown(sb, val)        sb[0x01] = val
#define set_GSM_page_code(sb, val)      sb[0x02] = val
#define set_GSM_len(sb, val)            sb[0x04] = val

#define GSM_PSIZE_len                   0x5a

/* ==================================================================== */
/* SET SCAN MODE */
#define SET_SCAN_MODE_code               0xd6
#define SET_SCAN_MODE_len                6

#define set_SSM_pf(sb, val)             setbitfield(sb + 1, 1, 4, val)
#define set_SSM_pay_len(sb, val)        sb[0x04] = val

/* the payload */
#define SSM_PAY_len                     0x14
#define SSM_PAY_HEAD_len                0x13
#define set_SSM_pay_head_len(sb, val)   sb[0x01] = val
#define set_SSM_page_code(sb, val)      sb[0x04] = val
#define SSM_PAGE_len                    0x0e
#define set_SSM_page_len(sb, val)       sb[0x05] = val

/* for DF (0x30) page */
#define set_SSM_DF_deskew_roll(sb, val) setbitfield(sb+7, 1, 5, val)
#define set_SSM_DF_staple(sb, val)      setbitfield(sb+7, 1, 4, val)
#define set_SSM_DF_thick(sb, val)       setbitfield(sb+7, 1, 2, val)
#define set_SSM_DF_len(sb, val)         setbitfield(sb+7, 1, 0, val)
#define set_SSM_DF_textdir(sb, val)     setbitfield(sb+9, 0xf, 0, val)

/* for BUFFER (0x32) page */
#define set_SSM_BUFF_duplex(sb, val)    setbitfield(sb+6, 1, 1, val)
#define set_SSM_BUFF_unk(sb, val)       sb[0x07] = val
#define set_SSM_BUFF_async(sb, val)     setbitfield(sb+0x0a, 1, 6, val)
#define set_SSM_BUFF_ald(sb, val)       setbitfield(sb+0x0a, 1, 5, val)
#define set_SSM_BUFF_fb(sb, val)        setbitfield(sb+0x0a, 1, 4, val)
#define set_SSM_BUFF_card(sb, val)      setbitfield(sb+0x0a, 1, 3, val)

/* for DO (0x36) page */
#define SSM_DO_none                     0
#define SSM_DO_red                      1
#define SSM_DO_green                    2
#define SSM_DO_blue                     3
#define set_SSM_DO_unk1(sb, val)        sb[0x07] = val
#define set_SSM_DO_unk2(sb, val)        sb[0x09] = val
#define set_SSM_DO_f_do(sb, val)        sb[0x0b] = val
#define set_SSM_DO_b_do(sb, val)        sb[0x0c] = val
#define set_SSM_DO_f_en(sb, val)        sb[0x0d] = val
#define set_SSM_DO_b_en(sb, val)        sb[0x0e] = val

/* ==================================================================== */
/* Cancel */
#define CANCEL_code               0xd8
#define CANCEL_len                6

/* ==================================================================== */
/* Coarse Calibration */
#define COR_CAL_code                0xe1
#define COR_CAL_len                 10

#define set_CC_version(sb, val)     sb[5] = val
#define set_CC_xferlen(sb, len)     putnbyte(sb + 0x06, len, 3)

/* the payload */
#define CC_pay_len                  0x20
#define CC_pay_ver                  0x00

#define set_CC_f_gain(sb, val)      sb[0] = val
#define set_CC_unk1(sb, val)        sb[1] = val
#define set_CC_f_offset(sb, val)    sb[2] = val
#define set_CC_unk2(sb, val)        sb[3] = val

#define set_CC_exp_f_r1(sb, val)    putnbyte(sb + 0x04, val, 2)
#define set_CC_exp_f_g1(sb, val)    putnbyte(sb + 0x06, val, 2)
#define set_CC_exp_f_b1(sb, val)    putnbyte(sb + 0x08, val, 2)
#define set_CC_exp_f_r2(sb, val)    putnbyte(sb + 0x0a, val, 2)
#define set_CC_exp_f_g2(sb, val)    putnbyte(sb + 0x0c, val, 2)
#define set_CC_exp_f_b2(sb, val)    putnbyte(sb + 0x0e, val, 2)

#define set_CC_b_gain(sb, val)      sb[0x10] = val
#define set_CC_b_offset(sb, val)    sb[0x12] = val

#define set_CC_exp_b_r1(sb, val)    putnbyte(sb + 0x14, val, 2)
#define set_CC_exp_b_g1(sb, val)    putnbyte(sb + 0x16, val, 2)
#define set_CC_exp_b_b1(sb, val)    putnbyte(sb + 0x18, val, 2)
#define set_CC_exp_b_r2(sb, val)    putnbyte(sb + 0x1a, val, 2)
#define set_CC_exp_b_g2(sb, val)    putnbyte(sb + 0x1c, val, 2)
#define set_CC_exp_b_b2(sb, val)    putnbyte(sb + 0x1e, val, 2)

/* the 'version 3' payload (P-208 and P-215) */
#define CC3_pay_len                  0x28
#define CC3_pay_ver                  0x03

#define set_CC3_gain_f_r(sb, val)    sb[0] = val
#define set_CC3_gain_f_g(sb, val)    sb[1] = val
#define set_CC3_gain_f_b(sb, val)    sb[2] = val

#define set_CC3_off_f_r(sb, val)     sb[4] = val
#define set_CC3_off_f_g(sb, val)     sb[5] = val
#define set_CC3_off_f_b(sb, val)     sb[6] = val

#define set_CC3_exp_f_r(sb, val)     putnbyte(sb + 0x08, val, 2)
#define set_CC3_exp_f_g(sb, val)     putnbyte(sb + 0x0a, val, 2)
#define set_CC3_exp_f_b(sb, val)     putnbyte(sb + 0x0c, val, 2)

#define set_CC3_gain_b_r(sb, val)    sb[0x14] = val
#define set_CC3_gain_b_g(sb, val)    sb[0x15] = val
#define set_CC3_gain_b_b(sb, val)    sb[0x16] = val

#define set_CC3_off_b_r(sb, val)     sb[0x18] = val
#define set_CC3_off_b_g(sb, val)     sb[0x19] = val
#define set_CC3_off_b_b(sb, val)     sb[0x1a] = val

#define set_CC3_exp_b_r(sb, val)     putnbyte(sb + 0x1c, val, 2)
#define set_CC3_exp_b_g(sb, val)     putnbyte(sb + 0x1e, val, 2)
#define set_CC3_exp_b_b(sb, val)     putnbyte(sb + 0x20, val, 2)

/* ==================================================================== */
/* Page codes used by GET/SET SCAN MODE 2 */
#define SM2_pc_df                       0x00
#define SM2_pc_ultra                    0x01
#define SM2_pc_buffer                   0x02
#define SM2_pc_dropout                  0x06

/* ==================================================================== */
/* SET SCAN MODE 2 */
#define SET_SCAN_MODE2_code             0xe5
#define SET_SCAN_MODE2_len              12

#define set_SSM2_page_code(sb, val)     sb[0x02] = val
#define set_SSM2_pay_len(sb, val)       sb[0x08] = val

/* the payload */
#define SSM2_PAY_len                     0x10

/* for DF (0x00) page */
#define set_SSM2_DF_thick(sb, val)       setbitfield(sb+3, 1, 2, val)
#define set_SSM2_DF_len(sb, val)         setbitfield(sb+3, 1, 0, val)

/* for ULTRA (0x01) page */
#define set_SSM2_ULTRA_top(sb, val)      putnbyte(sb + 0x07, val, 2)
#define set_SSM2_ULTRA_bot(sb, val)      putnbyte(sb + 0x09, val, 2)

/* for BUFFER (0x02) page */
#define set_SSM2_BUFF_unk(sb, val)           sb[0x03] = val
#define set_SSM2_BUFF_unk2(sb, val)          sb[0x06] = val
#define set_SSM2_BUFF_sync(sb, val)          sb[0x09] = val

/* for DROPOUT (0x06) page */
#define set_SSM2_DO_do(sb, val)              sb[0x09] = val
#define set_SSM2_DO_en(sb, val)              sb[0x0a] = val

/* ==================================================================== */
/* window descriptor macros for SET_WINDOW and GET_WINDOW */

#define set_WPDB_wdblen(sb, len) putnbyte(sb + 0x06, len, 2)

/* ==================================================================== */

  /* 0x00 - Window Identifier */
#define set_WD_wid(sb, val) sb[0] = val
#define WD_wid_front 0x00
#define WD_wid_back 0x01

  /* 0x01 - Reserved (bits 7-1), AUTO (bit 0) */
#define set_WD_auto(sb, val) setbitfield(sb + 0x01, 1, 0, val)
#define get_WD_auto(sb)	getbitfield(sb + 0x01, 1, 0)

  /* 0x02,0x03 - X resolution in dpi */
#define set_WD_Xres(sb, val) putnbyte(sb + 0x02, val, 2)
#define get_WD_Xres(sb)	getnbyte(sb + 0x02, 2)

  /* 0x04,0x05 - Y resolution in dpi */
#define set_WD_Yres(sb, val) putnbyte(sb + 0x04, val, 2)
#define get_WD_Yres(sb)	getnbyte(sb + 0x04, 2)

  /* 0x06-0x09 - Upper Left X in 1/1200 inch */
#define set_WD_ULX(sb, val) putnbyte(sb + 0x06, val, 4)
#define get_WD_ULX(sb) getnbyte(sb + 0x06, 4)

  /* 0x0a-0x0d - Upper Left Y in 1/1200 inch */
#define set_WD_ULY(sb, val) putnbyte(sb + 0x0a, val, 4)
#define get_WD_ULY(sb) getnbyte(sb + 0x0a, 4)

  /* 0x0e-0x11 - Width in 1/1200 inch */
#define set_WD_width(sb, val) putnbyte(sb + 0x0e, val, 4)
#define get_WD_width(sb) getnbyte(sb + 0x0e, 4)

  /* 0x12-0x15 - Height in 1/1200 inch */
#define set_WD_length(sb, val) putnbyte(sb + 0x12, val, 4)
#define get_WD_length(sb) getnbyte(sb + 0x12, 4)

  /* 0x16 - Brightness */
#define set_WD_brightness(sb, val) sb[0x16] = val
#define get_WD_brightness(sb)  sb[0x16]

  /* 0x17 - Threshold */
#define set_WD_threshold(sb, val) sb[0x17] = val
#define get_WD_threshold(sb)  sb[0x17]

  /* 0x18 - Contrast */
#define set_WD_contrast(sb, val) sb[0x18] = val
#define get_WD_contrast(sb) sb[0x18]

  /* 0x19 - Image Composition (color mode) */
#define set_WD_composition(sb, val)  sb[0x19] = val
#define get_WD_composition(sb) sb[0x19]
#define WD_comp_LA 0
#define WD_comp_HT 1
#define WD_comp_GS 2
#define WD_comp_CL 3
#define WD_comp_CH 4
#define WD_comp_CG 5

  /* 0x1a - Depth */
#define set_WD_bitsperpixel(sb, val) sb[0x1a] = val
#define get_WD_bitsperpixel(sb)	sb[0x1a]

  /* 0x1b,0x1c - Halftone Pattern */
#define set_WD_ht_type(sb, val) sb[0x1b] = val
#define get_WD_ht_type(sb)	sb[0x1b]
#define WD_ht_type_DEFAULT 0
#define WD_ht_type_DITHER 1
#define WD_ht_type_DIFFUSION 2

#define set_WD_ht_pattern(sb, val) sb[0x1c] = val
#define get_WD_ht_pattern(sb)	   sb[0x1c]

  /* 0x1d - Reverse image, reserved area, padding type */
#define set_WD_rif(sb, val)       setbitfield(sb + 0x1d, 1, 7, val)
#define get_WD_rif(sb)	          getbitfield(sb + 0x1d, 1, 7)
#define set_WD_rgb(sb, val)       setbitfield(sb + 0x1d, 7, 4, val)
#define get_WD_rgb(sb)	          getbitfield(sb + 0x1d, 7, 4)
#define set_WD_padding(sb, val)   setbitfield(sb + 0x1d, 7, 0, val)
#define get_WD_padding(sb)	  getbitfield(sb + 0x1d, 7, 0)

  /* 0x1e,0x1f - Bit ordering */
#define set_WD_bitorder(sb, val) putnbyte(sb + 0x1e, val, 2)
#define get_WD_bitorder(sb)	getnbyte(sb + 0x1e, 2)

  /* 0x20 - compression type */
#define set_WD_compress_type(sb, val)  sb[0x20] = val
#define get_WD_compress_type(sb) sb[0x20]
#define WD_cmp_NONE 0
#define WD_cmp_MH   1
#define WD_cmp_MR   2
#define WD_cmp_MMR  3
#define WD_cmp_JPEG 0x80

  /* 0x21 - compression argument
   *          specify "k" parameter with MR compress,
   *          or with JPEG- Q param, 0-7
   */
#define set_WD_compress_arg(sb, val)  sb[0x21] = val
#define get_WD_compress_arg(sb) sb[0x21]

  /* 0x22-0x27 - reserved */

  /* 0x28-0x2c - vendor unique */
  /* FIXME: more params here? */
#define set_WD_reserved2(sb, val)  sb[0x2a] = val
#define get_WD_reserved2(sb)	   sb[0x2a]
  

/* ==================================================================== */

#endif
