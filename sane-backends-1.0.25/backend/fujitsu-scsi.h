#ifndef FUJITSU_SCSI_H
#define FUJITSU_SCSI_H

/* 
 * Part of SANE - Scanner Access Now Easy.
 * 
 * Please see to opening comments in fujitsu.c
 */

/****************************************************/

#define USB_COMMAND_CODE   0x43
#define USB_COMMAND_LEN    0x1F
#define USB_COMMAND_OFFSET 0x13
#define USB_COMMAND_TIME   30000
#define USB_DATA_TIME      30000
#define USB_STATUS_CODE    0x53
#define USB_STATUS_LEN     0x0D
#define USB_STATUS_OFFSET  0x09
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

#define RS_return_size          0x12
#define set_RS_return_size(icb,val)        icb[0x04]=val

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
#define get_RS_SKSV(b)                    getbitfield(b+0x0f,1,7) /* valid=0 */
#define get_RS_SKSB(b)                    getnbyte(b+0x0f, 3)

/* when RS is 0x05/0x26 bad bytes listed in sksb */
#define get_RS_offending_byte(b)          getnbyte(b+0x10, 2)

/* 3091 and 3092 use RS instead of ghs. RS must be 0x00/0x80 */
/* in ascq */
#define get_RS_adf_open(in)        getbitfield(in+0x0d, 1, 7)
#define get_RS_send_sw(in)         getbitfield(in+0x0d, 1, 5)
#define get_RS_scan_sw(in)         getbitfield(in+0x0d, 1, 4)
#define get_RS_duplex_sw(in)       getbitfield(in+0x0d, 1, 2)
#define get_RS_top(in)             getbitfield(in+0x0d, 1, 1)
#define get_RS_hopper(in)          getbitfield(in+0x0d, 1, 0)

/* in sksb */
#define get_RS_function(in)        getbitfield(in+0x0f, 0x0f, 3)
#define get_RS_density(in)         getbitfield(in+0x0f, 0x07, 0)

/* ==================================================================== */
/* INQUIRY */
#define INQUIRY_code            0x12
#define INQUIRY_len             6

#define INQUIRY_std_len         96
#define INQUIRY_vpd_len         204        /* unlikely maximum value */

#define set_IN_evpd(icb, val)              setbitfield(icb + 1, 1, 0, val)
#define set_IN_page_code(icb, val)         icb[0x02]=val
#define set_IN_return_size(icb,val)        icb[0x04]=val
#define set_IN_length(out,n)               out[0x04]=n-5

#define get_IN_periph_qual(in)             getbitfield(in, 0x07, 5)
#define IN_periph_qual_lun                    0x00
#define IN_periph_qual_nolun                  0x03
#define get_IN_periph_devtype(in)          getbitfield(in, 0x1f, 0)
#define IN_periph_devtype_scanner             0x06
#define IN_periph_devtype_unknown             0x1f
#define get_IN_response_format(in)         getbitfield(in + 0x03, 0x07, 0)
#define IN_recognized                         0x02
#define get_IN_vendor(in, buf)             strncpy(buf, (char *)in + 0x08, 0x08)
#define get_IN_product(in, buf)            strncpy(buf, (char *)in + 0x10, 0x010)
#define get_IN_version(in, buf)            strncpy(buf, (char *)in + 0x20, 0x04)
#define get_IN_color_offset(in)            getnbyte (in+0x2A, 2) /* offset between colors */

/* these only in some scanners */
#define get_IN_long_gray(in)               getbitfield(in+0x2C, 1, 1)
#define get_IN_long_color(in)              getbitfield(in+0x2C, 1, 0)

#define get_IN_emulation(in)               getbitfield(in+0x2D, 1, 6)
#define get_IN_cmp_cga(in)                 getbitfield(in+0x2D, 1, 5)
#define get_IN_bg_back(in)                 getbitfield(in+0x2D, 1, 3)
#define get_IN_bg_front(in)                getbitfield(in+0x2D, 1, 2)
#define get_IN_bg_fb(in)                   getbitfield(in+0x2D, 1, 1)
#define get_IN_has_back(in)                getbitfield(in+0x2D, 1, 0)

#define get_IN_duplex_offset(in)           getnbyte (in+0x2E, 2)

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
#define get_IN_std_res_200(in)             getbitfield(in+ 0x12, 1, 0)
#define get_IN_std_res_180(in)             getbitfield(in+ 0x12, 1, 1)
#define get_IN_std_res_160(in)             getbitfield(in+ 0x12, 1, 2)
#define get_IN_std_res_150(in)             getbitfield(in+ 0x12, 1, 3)
#define get_IN_std_res_120(in)             getbitfield(in+ 0x12, 1, 4)
#define get_IN_std_res_100(in)             getbitfield(in+ 0x12, 1, 5)
#define get_IN_std_res_75(in)              getbitfield(in+ 0x12, 1, 6)
#define get_IN_std_res_60(in)              getbitfield(in+ 0x12, 1, 7)
#define get_IN_std_res_1200(in)            getbitfield(in+ 0x13, 1, 0)
#define get_IN_std_res_800(in)             getbitfield(in+ 0x13, 1, 1)
#define get_IN_std_res_600(in)             getbitfield(in+ 0x13, 1, 2)
#define get_IN_std_res_480(in)             getbitfield(in+ 0x13, 1, 3)
#define get_IN_std_res_400(in)             getbitfield(in+ 0x13, 1, 4)
#define get_IN_std_res_320(in)             getbitfield(in+ 0x13, 1, 5)
#define get_IN_std_res_300(in)             getbitfield(in+ 0x13, 1, 6)
#define get_IN_std_res_240(in)             getbitfield(in+ 0x13, 1, 7)
#define get_IN_window_width(in)            getnbyte(in + 0x14, 4)
#define get_IN_window_length(in)           getnbyte(in + 0x18, 4)
#define get_IN_overflow(in)                getbitfield(in+0x1c, 1, 0)
#define get_IN_monochrome(in)              getbitfield(in+0x1c, 1, 1)
#define get_IN_half_tone(in)               getbitfield(in+0x1c, 1, 2)
#define get_IN_multilevel(in)              getbitfield(in+0x1c, 1, 3)
#define get_IN_monochrome_rgb(in)          getbitfield(in+0x1c, 1, 5)
#define get_IN_half_tone_rgb(in)           getbitfield(in+0x1c, 1, 6)
#define get_IN_multilevel_rgb(in)          getbitfield(in+0x1c, 1, 7)

/* vendor unique section */
#define get_IN_adf(in)                     getbitfield(in+0x20, 1, 7)
#define get_IN_flatbed(in)                 getbitfield(in+0x20, 1, 6)
#define get_IN_transparency(in)            getbitfield(in+0x20, 1, 5)
#define get_IN_duplex(in)                  getbitfield(in+0x20, 1, 4)
#define get_IN_endorser_b(in)              getbitfield(in+0x20, 1, 3)
#define get_IN_barcode(in)                 getbitfield(in+0x20, 1, 2)
#define get_IN_operator_panel(in)          getbitfield(in+0x20, 1, 1)
#define get_IN_endorser_f(in)              getbitfield(in+0x20, 1, 0)

#define get_IN_mp_stacker(in)              getbitfield(in+0x21, 1, 7)
#define get_IN_prepick(in)                 getbitfield(in+0x21, 1, 6)
#define get_IN_mf_detect(in)               getbitfield(in+0x21, 1, 5)
#define get_IN_paperprot(in)               getbitfield(in+0x21, 1, 4)
#define get_IN_adbits(in)                  getbitfield(in+0x21, 0x0f, 0)

#define get_IN_buffer_bytes(in)            getnbyte(in + 0x22, 4)

/*supported scsi commands*/
#define get_IN_has_cmd_msen10(in)       getbitfield(in+0x26, 1, 1)
#define get_IN_has_cmd_msel10(in)	getbitfield(in+0x26, 1, 0)

#define get_IN_has_cmd_lsen(in)		getbitfield(in+0x27, 1, 7)
#define get_IN_has_cmd_lsel(in)		getbitfield(in+0x27, 1, 6)
#define get_IN_has_cmd_change(in)	getbitfield(in+0x27, 1, 5)
#define get_IN_has_cmd_rbuff(in)	getbitfield(in+0x27, 1, 4)
#define get_IN_has_cmd_wbuff(in)	getbitfield(in+0x27, 1, 3)
#define get_IN_has_cmd_cav(in)		getbitfield(in+0x27, 1, 2)
#define get_IN_has_cmd_comp(in)		getbitfield(in+0x27, 1, 1)
#define get_IN_has_cmd_gdbs(in)		getbitfield(in+0x27, 1, 0)

#define get_IN_has_cmd_op(in)		getbitfield(in+0x28, 1, 7)
#define get_IN_has_cmd_send(in)		getbitfield(in+0x28, 1, 6)
#define get_IN_has_cmd_read(in)		getbitfield(in+0x28, 1, 5)
#define get_IN_has_cmd_gwin(in)		getbitfield(in+0x28, 1, 4)
#define get_IN_has_cmd_swin(in)		getbitfield(in+0x28, 1, 3)
#define get_IN_has_cmd_sdiag(in)	getbitfield(in+0x28, 1, 2)
#define get_IN_has_cmd_rdiag(in)	getbitfield(in+0x28, 1, 1)
#define get_IN_has_cmd_scan(in)		getbitfield(in+0x28, 1, 0)
  
#define get_IN_has_cmd_msen6(in)	getbitfield(in+0x29, 1, 7)
#define get_IN_has_cmd_copy(in)		getbitfield(in+0x29, 1, 6)
#define get_IN_has_cmd_rel(in)		getbitfield(in+0x29, 1, 5)
#define get_IN_has_cmd_runit(in)	getbitfield(in+0x29, 1, 4)
#define get_IN_has_cmd_msel6(in)	getbitfield(in+0x29, 1, 3)
#define get_IN_has_cmd_inq(in)		getbitfield(in+0x29, 1, 2)
#define get_IN_has_cmd_rs(in)		getbitfield(in+0x29, 1, 1)
#define get_IN_has_cmd_tur(in)		getbitfield(in+0x29, 1, 0)

/* more stuff here? (vendor commands) */
#define get_IN_has_cmd_subwindow(in)       getbitfield(in+0x2b, 1, 0) 
#define get_IN_has_cmd_endorser(in)        getbitfield(in+0x2b, 1, 1)
#define get_IN_has_cmd_hw_status(in)       getbitfield(in+0x2b, 1, 2)
#define get_IN_has_cmd_hw_status_2(in)     getbitfield(in+0x2b, 1, 3)
#define get_IN_has_cmd_hw_status_3(in)     getbitfield(in+0x2b, 1, 4)
#define get_IN_has_cmd_scanner_ctl(in)     getbitfield(in+0x31, 1, 1)
#define get_IN_has_cmd_device_restart(in)  getbitfield(in+0x31, 1, 2)

#define get_IN_brightness_steps(in)        getnbyte(in+0x52, 1)
#define get_IN_threshold_steps(in)         getnbyte(in+0x53, 1)
#define get_IN_contrast_steps(in)          getnbyte(in+0x54, 1)

#define get_IN_num_dither_internal(in)     getbitfield(in+0x56, 15, 4)
#define get_IN_num_dither_download(in)     getbitfield(in+0x56, 15, 0)

#define get_IN_num_gamma_internal(in)      getbitfield(in+0x57, 15, 4)
#define get_IN_num_gamma_download(in)      getbitfield(in+0x57, 15, 0)

#define get_IN_ipc_bw_rif(in)              getbitfield(in+0x58, 1, 7)
#define get_IN_ipc_dtc(in)                 getbitfield(in+0x58, 1, 6)
#define get_IN_ipc_sdtc(in)                getbitfield(in+0x58, 1, 5)
#define get_IN_ipc_outline_extraction(in)  getbitfield(in+0x58, 1, 4)
#define get_IN_ipc_image_emphasis(in)      getbitfield(in+0x58, 1, 3)
#define get_IN_ipc_auto_separation(in)     getbitfield(in+0x58, 1, 2)
#define get_IN_ipc_mirroring(in)           getbitfield(in+0x58, 1, 1)
#define get_IN_ipc_wl_follow(in)           getbitfield(in+0x58, 1, 0)

#define get_IN_ipc_subwindow(in)           getbitfield(in+0x59, 1, 7)
#define get_IN_ipc_diffusion(in)           getbitfield(in+0x59, 1, 6)
#define get_IN_ipc_ipc3(in)                getbitfield(in+0x59, 1, 5)
#define get_IN_ipc_rotation(in)            getbitfield(in+0x59, 1, 4)
#define get_IN_ipc_hybrid_crop_deskew(in)  getbitfield(in+0x59, 1, 3)
#define get_IN_ipc_ipc2_byte67(in)         getbitfield(in+0x59, 1, 0)

#define get_IN_compression_MH(in)          getbitfield(in+0x5a, 1, 7)
#define get_IN_compression_MR(in)          getbitfield(in+0x5a, 1, 6)
#define get_IN_compression_MMR(in)         getbitfield(in+0x5a, 1, 5)
#define get_IN_compression_JBIG(in)        getbitfield(in+0x5a, 1, 4)
#define get_IN_compression_JPG_BASE(in)    getbitfield(in+0x5a, 1, 3)
#define get_IN_compression_JPG_EXT(in)     getbitfield(in+0x5a, 1, 2)
#define get_IN_compression_JPG_INDEP(in)   getbitfield(in+0x5a, 1, 1)

#define get_IN_compression_JPG_gray(in)    getbitfield(in+0x5b, 3, 6)
#define IN_comp_JPG_gray_unsup 1
#define IN_comp_JPG_gray_color 2
#define IN_comp_JPG_gray_gray  3
#define get_IN_compression_JPG_YUV_422(in) getbitfield(in+0x5b, 1, 0)

#define get_IN_endorser_b_mech(in)          getbitfield(in+0x5c, 1, 7)
#define get_IN_endorser_b_stamp(in)         getbitfield(in+0x5c, 1, 6)
#define get_IN_endorser_b_elec(in)          getbitfield(in+0x5c, 1, 5)
#define get_IN_endorser_max_id(in)          getbitfield(in+0x5c, 0x0f, 0)

#define get_IN_endorser_f_mech(in)          getbitfield(in+0x5d, 1, 7)
#define get_IN_endorser_f_stamp(in)         getbitfield(in+0x5d, 1, 6)
#define get_IN_endorser_f_elec(in)          getbitfield(in+0x5d, 1, 5)
#define get_IN_endorser_f_type(in)          getbitfield(in+0x5d, 3, 2)
#define get_IN_endorser_b_type(in)          getbitfield(in+0x5d, 3, 0)

#define get_IN_connection(in)               getbitfield(in+0x62, 3, 0)

#define get_IN_endorser_type_ext(in)        getbitfield(in+0x63, 1, 4)
#define get_IN_endorser_pre_back(in)        getbitfield(in+0x63, 1, 3)
#define get_IN_endorser_pre_front(in)       getbitfield(in+0x63, 1, 2)
#define get_IN_endorser_post_back(in)       getbitfield(in+0x63, 1, 1)
#define get_IN_endorser_post_front(in)      getbitfield(in+0x63, 1, 0)

#define get_IN_x_overscan_size(in)  getnbyte(in + 0x64, 2)
#define get_IN_y_overscan_size(in)  getnbyte(in + 0x66, 2)

#define get_IN_default_bg_adf_b(in)      getbitfield(in+0x68, 1, 3)
#define get_IN_default_bg_adf_f(in)      getbitfield(in+0x68, 1, 2)
#define get_IN_default_bg_fb(in)         getbitfield(in+0x68, 1, 1)

#define get_IN_auto_color(in)         getbitfield(in+0x69, 1, 7)
#define get_IN_blank_skip(in)         getbitfield(in+0x69, 1, 6)
#define get_IN_multi_image(in)        getbitfield(in+0x69, 1, 5)
#define get_IN_f_b_type_indep(in)     getbitfield(in+0x69, 1, 4)
#define get_IN_f_b_res_indep(in)      getbitfield(in+0x69, 1, 3)

#define get_IN_dropout_spec(in)       getbitfield(in+0x6a, 1, 7)
#define get_IN_dropout_non(in)        getbitfield(in+0x6a, 1, 7)
#define get_IN_dropout_white(in)      getbitfield(in+0x6a, 1, 7)

#define get_IN_skew_check(in)         getbitfield(in+0x6d, 1, 7)
#define get_IN_new_fd_roll(in)        getbitfield(in+0x6d, 1, 6)
#define get_IN_paper_prot_2(in)       getbitfield(in+0x6d, 1, 1)

#define get_IN_evpd_len(in)           getnbyte(in + 0x6f, 1)

#define get_IN_paper_count(in)        getbitfield(in+0x70, 1, 7)
#define get_IN_paper_number(in)       getbitfield(in+0x70, 1, 6)
#define get_IN_ext_send_to(in)        getbitfield(in+0x70, 1, 5)
#define get_IN_staple_det(in)         getbitfield(in+0x70, 1, 4)
#define get_IN_pause_host(in)         getbitfield(in+0x70, 1, 3)
#define get_IN_pause_panel(in)        getbitfield(in+0x70, 1, 2)
#define get_IN_pause_conf(in)         getbitfield(in+0x70, 1, 1)
#define get_IN_hq_print(in)           getbitfield(in+0x70, 1, 0)

#define get_IN_ext_GHS_len(in)        getnbyte(in + 0x71, 1)

#define get_IN_smbc_func(in)          getbitfield(in+0x72, 1, 7)
#define get_IN_imprint_chk_b(in)      getbitfield(in+0x72, 1, 6)
#define get_IN_imprint_chk_f(in)      getbitfield(in+0x72, 1, 5)
#define get_IN_force_w_bg(in)         getbitfield(in+0x72, 1, 4)
#define get_IN_mf_recover_lvl(in)     getbitfield(in+0x72, 0x0f, 0)

#define get_IN_first_read_time(in)    getbitfield(in+0x73, 1, 7)
#define get_IN_div_scanning(in)       getbitfield(in+0x73, 1, 6)
#define get_IN_start_job(in)          getbitfield(in+0x73, 1, 5)
#define get_IN_lifetime_log(in)       getbitfield(in+0x73, 1, 4)
#define get_IN_imff_save_rest(in)     getbitfield(in+0x73, 1, 3)
#define get_IN_wide_scsi_type(in)     getbitfield(in+0x73, 0x07, 0)

#define get_IN_lut_hybrid_crop(in)    getbitfield(in+0x74, 1, 7)
#define get_IN_over_under_amt(in)     getbitfield(in+0x74, 1, 6)
#define get_IN_rgb_lut(in)            getbitfield(in+0x74, 1, 5)
#define get_IN_num_lut_dl(in)         getbitfield(in+0x74, 0x0f, 0)

/*byte 75 is poorly documented*/

#define get_IN_erp_lot6_supp(in)      getbitfield(in+0x76, 1, 7)
#define get_IN_mode_change_jpeg(in)   getbitfield(in+0x76, 1, 5)
#define get_IN_mode_change_irdc(in)   getbitfield(in+0x76, 1, 4)
#define get_IN_mode_change_iomf(in)   getbitfield(in+0x76, 1, 3)
#define get_IN_sync_next_feed(in)     getbitfield(in+0x76, 0x07, 0)

#define get_IN_imp_func3(in)          getbitfield(in+0x77, 1, 7)

#define get_IN_reset_ms(in)           getbitfield(in+0x78, 1, 7)
#define get_IN_read_size(in)          getbitfield(in+0x78, 1, 6)
#define get_IN_start_end_ms(in)       getbitfield(in+0x78, 1, 5)

#define get_IN_op_halt(in)            getbitfield(in+0x7a, 1, 7)

/* some scanners need evpd inquiry data manipulated */
#define set_IN_page_length(in,val)    in[0x04]=val

/* ==================================================================== */
/* page codes used by mode_sense and mode_select */
#define MS_pc_unk     0x2c /* Used by iX500 */
#define MS_pc_patch   0x2e /* Patch code scanning */
#define MS_pc_counter 0x2f /* Page number and counter reset */
#define MS_pc_autocolor 0x32 /* Automatic color detection */
#define MS_pc_prepick 0x33 /* Prepick next adf page */
#define MS_pc_sleep   0x34 /* Sleep mode */
#define MS_pc_duplex  0x35 /* ADF duplex transfer mode */
#define MS_pc_rand    0x36 /* All sorts of device controls */
#define MS_pc_bg      0x37 /* Backing switch control */
#define MS_pc_df      0x38 /* Double feed detection */
#define MS_pc_dropout 0x39 /* Drop out color */
#define MS_pc_buff    0x3a /* Scan buffer control */
#define MS_pc_auto    0x3c /* Auto paper size detection */
#define MS_pc_lamp    0x3d /* Lamp light timer set */
#define MS_pc_jobsep  0x3e /* Detect job separation sheet */
#define MS_pc_all     0x3f /* Only used with mode_sense */

/* ==================================================================== */
/* MODE_SELECT */
#define MODE_SELECT_code        0x15
#define MODE_SELECT_len         6

#define set_MSEL_pf(sb, val) setbitfield(sb + 1, 1, 4, val)
#define set_MSEL_xferlen(sb, val) sb[0x04] = (unsigned char)val

/* MS payloads are combined 4 byte header and 8 or 10 byte page 
 * there is also 'descriptor block' & 'vendor-specific block'
 * but fujitsu seems not to use these */
/* 10 byte page only used by dropout? */
#define MSEL_header_len           4
#define MSEL_data_min_len         8
#define MSEL_data_max_len         10

#define set_MSEL_pc(sb, val) sb[0x00]=val
#define set_MSEL_page_len(sb, val) sb[0x01]=val

#define set_MSEL_sleep_mode(sb, val) sb[0x02]=val

#define set_MSEL_transfer_mode(sb, val) setbitfield(sb + 0x02, 0x01, 0, val)

#define set_MSEL_bg_enable(sb, val) setbitfield(sb + 2, 1, 7, val)
#define set_MSEL_bg_front(sb, val) setbitfield(sb + 2, 1, 5, val)
#define set_MSEL_bg_back(sb, val) setbitfield(sb + 2, 1, 4, val)
#define set_MSEL_bg_fb(sb, val) setbitfield(sb + 2, 1, 3, val)

#define set_MSEL_df_enable(sb, val) setbitfield(sb + 2, 1, 7, val)
#define set_MSEL_df_continue(sb, val) setbitfield(sb + 2, 1, 6, val)
#define set_MSEL_df_skew(sb, val) setbitfield(sb + 2, 1, 5, val)
#define set_MSEL_df_thickness(sb, val) setbitfield(sb + 2, 1, 4, val)
#define set_MSEL_df_length(sb, val) setbitfield(sb + 2, 1, 3, val)
#define set_MSEL_df_diff(sb, val) setbitfield(sb + 2, 3, 0, val)
#define MSEL_df_diff_DEFAULT 0 
#define MSEL_df_diff_10MM 1
#define MSEL_df_diff_15MM 2
#define MSEL_df_diff_20MM 3
#define set_MSEL_df_paperprot(sb, val) setbitfield(sb + 3, 3, 6, val)
#define set_MSEL_df_stapledet(sb, val) setbitfield(sb + 3, 3, 4, val)
#define set_MSEL_df_recovery(sb, val)  setbitfield(sb + 3, 3, 2, val)
#define set_MSEL_df_paperprot2(sb, val) setbitfield(sb + 5, 3, 6, val)

#define set_MSEL_dropout_front(sb, val) setbitfield(sb + 0x02, 0x0f, 0, val)
#define set_MSEL_dropout_back(sb, val) setbitfield(sb + 0x02, 0x0f, 4, val)
#define MSEL_dropout_DEFAULT 0
#define MSEL_dropout_GREEN   8
#define MSEL_dropout_RED     9
#define MSEL_dropout_BLUE    11
#define MSEL_dropout_CUSTOM  12

#define set_MSEL_buff_mode(sb, val) setbitfield(sb + 0x02, 0x03, 6, val)
#define set_MSEL_buff_clear(sb, val) setbitfield(sb + 0x03, 0x03, 6, val)

#define set_MSEL_prepick(sb, val) setbitfield(sb + 0x02, 0x03, 6, val)

/*more automatic stuff with this one...*/
#define set_MSEL_awd(sb, val)           setbitfield(sb + 0x02, 0x01, 7, val)
#define set_MSEL_w_wfill(sb, val)       setbitfield(sb + 0x02, 0x01, 6, val)
#define set_MSEL_req_driv_lut(sb, val)  setbitfield(sb + 0x02, 0x01, 1, val)
#define set_MSEL_req_driv_crop(sb, val) setbitfield(sb + 0x02, 0x01, 0, val)

#define set_MSEL_ald(sb, val)       setbitfield(sb + 0x03, 0x01, 7, val)
#define set_MSEL_l_wfill(sb, val)   setbitfield(sb + 0x03, 0x01, 6, val)

#define set_MSEL_deskew(sb, val)    setbitfield(sb + 0x04, 0x01, 7, val)

#define set_MSEL_overscan(sb, val)  setbitfield(sb + 0x05, 0x03, 6, val)
#define set_MSEL_overcrop(sb, val)  setbitfield(sb + 0x05, 0x01, 5, val)
#define set_MSEL_undercrop(sb, val) setbitfield(sb + 0x05, 0x01, 4, val)

#define set_MSEL_over_under_amt(sb, val) sb[0x06]=val

/*buffer, prepick, overscan and df use these*/
#define MSEL_DEFAULT 0
#define MSEL_OFF 2
#define MSEL_ON 3

/* ==================================================================== */
/* RESERVE_UNIT */
#define RESERVE_UNIT_code       0x16
#define RESERVE_UNIT_len        6

/* ==================================================================== */
/* RELEASE_UNIT */

#define RELEASE_UNIT_code       0x17
#define RELEASE_UNIT_len        6

/* ==================================================================== */
/* MODE_SENSE */
#define MODE_SENSE_code         0x1a
#define MODE_SENSE_len          6

#define MODE_SENSE_data_len     0x14

#define set_MSEN_DBD(b, val)    setbitfield(b, 0x01, 3, (val?1:0))
#define set_MSEN_pc(sb, val)    setbitfield(sb + 0x02, 0x3f, 0, val)
#define set_MSEN_xfer_length(sb, val) sb[0x04] = (unsigned char)val
#define get_MSEN_MUD(b)		getnbyte(b+(0x04+((int)*(b+0x3)))+0x4,2)

/* ==================================================================== */
/* SCAN */
#define SCAN_code               0x1b
#define SCAN_len                6

#define set_SC_xfer_length(sb, val) sb[0x04] = (unsigned char)val

/* ==================================================================== */
/* READ_DIAGNOSTIC */
#define READ_DIAGNOSTIC_code    0x1c
#define READ_DIAGNOSTIC_len     6

#define set_RD_xferlen(in, len) putnbyte(in + 3, len, 2)

/* for 'FIRST READ DATE \0YMD' */
#define RD_frd_len                      10
#define get_RD_date_status(in)          in[0]
#define RD_date_stored			0
#define RD_date_not_stored		0xff

/* for 'GET FIRST DATE  ' */
#define RD_gfd_len                      10
#define get_RD_date_year(in)            in[1]
#define get_RD_date_month(in)           in[2]
#define get_RD_date_date(in)            in[3]

/* for 'GET DEVICE ID   ' */
#define RD_gdi_len                      10
#define get_RD_id_serial(in)            getnbyte (in, 4)

/* ==================================================================== */
/* SEND_DIAGNOSTIC */
#define SEND_DIAGNOSTIC_code    0x1d
#define SEND_DIAGNOSTIC_len     6

#define set_SD_slftst(in, val) setbitfield(in + 1, 1, 2, val)
#define set_SD_xferlen(in, len) putnbyte(in + 3, len, 2)

#define SD_frd_string                   "FIRST READ DATE \0YMD"
#define SD_frd_len                      20
#define set_SD_frd_year(in, b) 		putnbyte(in + 0x11, b, 1)
#define set_SD_frd_month(in, b) 	putnbyte(in + 0x12, b, 1)
#define set_SD_frd_date(in, b) 		putnbyte(in + 0x13, b, 1)

#define SD_gfd_string                   "GET FIRST DATE  "
#define SD_gfd_len                      16

#define SD_gdi_string                   "GET DEVICE ID   "
#define SD_gdi_len                      16

#define SD_preread_string               "SET PRE READMODE"
#define SD_preread_stringlen            16
#define SD_preread_len                  32
#define set_SD_preread_xres(in, b)      putnbyte(in + 0x10, b, 2)
#define set_SD_preread_yres(in, b)      putnbyte(in + 0x12, b, 2)
#define set_SD_preread_paper_width(sb, val)   putnbyte(sb + 0x14, val, 4)
#define set_SD_preread_paper_length(sb, val)  putnbyte(sb + 0x18, val, 4)
#define set_SD_preread_composition(sb, val)   putnbyte(sb + 0x1c, val, 1)
#define set_SD_preread_escan(sb, val)   putnbyte(sb + 0x1d, val, 1)

#define SD_powoff_string                "SET POWOFF TIME "
#define SD_powoff_stringlen             16
#define SD_powoff_len                   18
#define set_SD_powoff_disable(in, val)  setbitfield(in + 16, 1, 7, val)
#define set_SD_powoff_interval(in, val) setbitfield(in + 16, 0x7f, 0, val)
#define set_SD_powoff_notify(sb, val)   putnbyte(sb + 0x17, val, 1)

/* ==================================================================== */
/* SET_WINDOW */
#define SET_WINDOW_code         0x24
#define SET_WINDOW_len          10

#define set_SW_xferlen(sb, len) putnbyte(sb + 0x06, len, 3)

#define SW_header_len		8
#define SW_desc_len		64

/* ==================================================================== */
/* GET_WINDOW */
#define GET_WINDOW_code         0x25
#define GET_WINDOW_len          0

/* ==================================================================== */
/* READ */
#define READ_code               0x28
#define READ_len                10

#define set_R_datatype_code(sb, val) sb[0x02] = val
#define R_datatype_imagedata		0x00
#define R_datatype_pixelsize		0x80
#define R_datatype_papersize		0x81
#define R_datatype_effective_id         0x82
#define set_R_window_id(sb, val)       sb[0x05] = val
#define set_R_xfer_length(sb, val)     putnbyte(sb + 0x06, val, 3)

#define R_PSIZE_len                0x20
#define get_PSIZE_num_x(in)            getnbyte(in + 0x00, 4)
#define get_PSIZE_num_y(in)            getnbyte(in + 0x04, 4)
#define get_PSIZE_paper_w(in)          getnbyte(in + 0x08, 4)
#define get_PSIZE_paper_l(in)          getnbyte(in + 0x0C, 4)
#define get_PSIZE_req_driv_crop(in)    getbitfield(in + 0x10, 1, 7)
#define get_PSIZE_req_driv_lut(in)     getbitfield(in + 0x10, 1, 6)
#define get_PSIZE_req_driv_valid(in)   getbitfield(in + 0x10, 1, 0)

#define R_PAPER_len                0x08
#define get_PAPER_job_sep(in)          getnbyte(in + 0x02, 1)
#define get_PAPER_paper_w(in)          getnbyte(in + 0x03, 1)

/* ==================================================================== */
/* SEND */
#define SEND_code               0x2a
#define SEND_len                10

#define set_S_xfer_datatype(sb, val) sb[0x02] = (unsigned char)val
/*#define S_datatype_imagedatai		0x00
#define S_datatype_halftone_mask        0x02
#define S_datatype_gamma_function       0x03*/
#define S_datatype_lut_data             0x83
#define S_datatype_jpg_q_table          0x88
#define S_datatype_endorser_data        0x90
/*#define S_EX_datatype_lut		0x01
#define S_EX_datatype_shading_data	0xa0
#define S_user_reg_gamma		0xc0
#define S_device_internal_info		0x03
#define set_S_datatype_qual_upper(sb, val) sb[0x04] = (unsigned char)val
#define S_DQ_none	0x00
#define S_DQ_Rcomp	0x06
#define S_DQ_Gcomp	0x07
#define S_DQ_Bcomp	0x08
#define S_DQ_Reg1	0x01
#define S_DQ_Reg2	0x02
#define S_DQ_Reg3	0x03*/
#define set_S_xfer_id(sb, val)    putnbyte(sb + 4, val, 2)
#define set_S_xfer_length(sb, val)    putnbyte(sb + 6, val, 3)

/*lut*/
#define S_lut_header_len   0x0a
#define set_S_lut_order(sb, val)    putnbyte(sb + 2, val, 1)
#define S_lut_order_single 0x10
#define set_S_lut_ssize(sb, val)    putnbyte(sb + 4, val, 2)
#define set_S_lut_dsize(sb, val)    putnbyte(sb + 6, val, 2)
#define S_lut_data_min_len          256
#define S_lut_data_max_len          1024

/*q-table*/
#define S_q_table_header_len   0x0a
#define S_q_table_y_len        0x40
#define set_S_q_table_y_len(sb, val)    putnbyte(sb + 4, val, 2)
#define S_q_table_uv_len       0x40
#define set_S_q_table_uv_len(sb, val)   putnbyte(sb + 6, val, 2)

/*endorser*/
#define S_e_data_min_len	18 /*minimum 18 bytes no string bytes*/
#define S_e_data_max_len	98 /*maximum 18 bytes plus 80 string bytes*/

#define set_S_endorser_data_id(sb, val) sb[0] = val

#define set_S_endorser_stamp(sb, val) setbitfield(sb + 0x01, 1, 7, val)
#define set_S_endorser_elec(sb, val) setbitfield(sb + 0x01, 1, 6, val)
#define set_S_endorser_decr(sb, val) setbitfield(sb + 0x01, 1, 5, val)
#define S_e_decr_inc 0
#define S_e_decr_dec 1
#define set_S_endorser_lap24(sb, val) setbitfield(sb + 0x01, 1, 4, val)
#define S_e_lap_24bit 1
#define S_e_lap_16bit 0
#define set_S_endorser_ctstep(sb, val) setbitfield(sb + 0x01, 0x03, 0, val)

#define set_S_endorser_ulx(sb, val) putnbyte(sb + 0x02, val, 4)
#define set_S_endorser_uly(sb, val) putnbyte(sb + 0x06, val, 4)

#define set_S_endorser_font(sb, val) sb[0xa] = val
#define S_e_font_horiz 0
#define S_e_font_vert 1
#define S_e_font_horiz_narrow 2
#define set_S_endorser_size(sb, val) sb[0xb] = val

#define set_S_endorser_revs(sb, val) setbitfield(sb + 0x0c, 0x01, 7, val)
#define S_e_revs_fwd 0
#define S_e_revs_rev 1
#define set_S_endorser_bold(sb, val) setbitfield(sb + 0x0c, 0x01, 2, val)
#define set_S_endorser_dirs(sb, val) setbitfield(sb + 0x0c, 0x03, 0, val)
#define S_e_dir_left_right 0
#define S_e_dir_top_bottom 1
#define S_e_dir_right_left 2
#define S_e_dir_bottom_top 3

#define set_S_endorser_string_length(sb, len)  sb[0x11] = len
#define set_S_endorser_string(sb,val,len) memcpy(sb+0x12,val,(size_t)len)

/* ==================================================================== */
/* OBJECT_POSITION */
#define OBJECT_POSITION_code    0x31
#define OBJECT_POSITION_len     10

#define set_OP_action(b,val)    setbitfield(b+0x01, 0x07, 0, val)
#define OP_Discharge	0x00
#define OP_Feed	        0x01
#define OP_Halt	        0x04

/* ==================================================================== */
/* SET_SUBWINDOW */
#define SET_SUBWINDOW_code      0xc0
#define SET_SUBWINDOW_len       0

/* ==================================================================== */
/* ENDORSER */
#define ENDORSER_code           0xc1
#define ENDORSER_len            10

#define set_E_xferlen(sb, val) putnbyte(sb + 0x7, val, 2)

/*endorser data*/
#define ED_min_len               4
#define ED_max_len               6

#define set_ED_endorser_data_id(sb, val) sb[0] = val

/* enable/disable endorser printing*/
#define set_ED_stop(sb, val) setbitfield(sb + 0x01, 1, 7, val)
#define ED_start 0
#define ED_stop 1
/* specifies the side of a document to be printed */
#define set_ED_side(sb, val) setbitfield(sb + 0x01, 1, 6, val)
#define ED_front 0
#define ED_back 1

/* format of the counter 16/24 bit*/
#define set_ED_lap24(sb, val) setbitfield(sb + 0x01, 1, 5, val)
#define ED_lap_16bit 0
#define ED_lap_24bit 1

/* initial count */
#define set_ED_initial_count_16(sb, val) putnbyte(sb + 0x02, val, 2)
#define set_ED_initial_count_24(sb, val) putnbyte(sb + 0x03, val, 3)

/* ==================================================================== */
/* GET_HW_STATUS*/
#define GET_HW_STATUS_code      0xc2
#define GET_HW_STATUS_len       10

#define set_GHS_allocation_length(sb, len) putnbyte(sb + 0x07, len, 2)

#define GHS_data_len            12

#define get_GHS_top(in)             getbitfield(in+0x02, 1, 7)
#define get_GHS_A3(in)              getbitfield(in+0x02, 1, 3)
#define get_GHS_B4(in)              getbitfield(in+0x02, 1, 2)
#define get_GHS_A4(in)              getbitfield(in+0x02, 1, 1)
#define get_GHS_B5(in)              getbitfield(in+0x02, 1, 0)

#define get_GHS_hopper(in)          !getbitfield(in+0x03, 1, 7)
#define get_GHS_omr(in)             getbitfield(in+0x03, 1, 6)
#define get_GHS_adf_open(in)        getbitfield(in+0x03, 1, 5)
#define get_GHS_imp_open(in)        getbitfield(in+0x03, 1, 4)
#define get_GHS_fb_open(in)         getbitfield(in+0x03, 1, 3)
#define get_GHS_paper_end(in)       getbitfield(in+0x03, 1, 2)
#define get_GHS_fb_on(in)           getbitfield(in+0x03, 1, 1)

#define get_GHS_sleep(in)           getbitfield(in+0x04, 1, 7)
#define get_GHS_clean(in)           getbitfield(in+0x04, 1, 6)
#define get_GHS_send_sw(in)         getbitfield(in+0x04, 1, 2)
#define get_GHS_manual_feed(in)     getbitfield(in+0x04, 1, 1)
#define get_GHS_scan_sw(in)         getbitfield(in+0x04, 1, 0)

#define get_GHS_picalm(in)          getbitfield(in+0x05, 1, 7)
#define get_GHS_fadalm(in)          getbitfield(in+0x05, 1, 6)
#define get_GHS_brkalm(in)          getbitfield(in+0x05, 1, 5)
#define get_GHS_sepalm(in)          getbitfield(in+0x05, 1, 4)
#define get_GHS_function(in)        getbitfield(in+0x05, 0x0f, 0)

#define get_GHS_ink_empty(in)      getbitfield(in+0x06, 1, 7)
#define get_GHS_consume(in)        getbitfield(in+0x06, 1, 6)
#define get_GHS_overskew(in)       getbitfield(in+0x06, 1, 5)
#define get_GHS_overthick(in)      getbitfield(in+0x06, 1, 4)
#define get_GHS_plen(in)           getbitfield(in+0x06, 1, 3)
#define get_GHS_ink_side(in)       getbitfield(in+0x06, 1, 2)
#define get_GHS_mf_to(in)          getbitfield(in+0x06, 1, 1)
#define get_GHS_double_feed(in)    getbitfield(in+0x06, 1, 0)

#define get_GHS_error_code(in)      in[0x07]

#define get_GHS_skew_angle(in)      getnbyte(in+0x08, 2)

#define get_GHS_ink_remain(in)      in[0x0a]

/* ==================================================================== */
/* SCANNER_CONTROL */
#define SCANNER_CONTROL_code    0xf1
#define SCANNER_CONTROL_len     10

#define set_SC_ric(icb, val)                   setbitfield(icb + 1, 1, 4, val)
#define set_SC_function(icb, val)              setbitfield(icb + 1, 0xf, 0, val)
#define SC_function_adf                        0x00
#define SC_function_fb                         0x01
#define SC_function_fb_hs                      0x02
#define SC_function_lamp_off                   0x03
#define SC_function_cancel                     0x04
#define SC_function_lamp_on                    0x05
#define SC_function_lamp_normal                0x06
#define SC_function_lamp_saving                0x07
#define SC_function_panel                      0x08
#define SC_function_scan_complete              0x09
#define SC_function_eject_complete             0x0a
#define SC_function_manual_feed                0x0c

#define set_SC_ric_dtq(sb, val) sb[2] = val
#define set_SC_ric_len(sb, val) putnbyte(sb + 0x06, val, 3)

/* ==================================================================== */
/* window descriptor macros for SET_WINDOW and GET_WINDOW */

#define set_WPDB_wdblen(sb, len) putnbyte(sb + 0x06, len, 2)

/* ==================================================================== */

  /* 0x00 - Window Identifier
   *        0x00 for 3096
   *        0x00 (front) or 0x80 (back) for 3091
   */
#define set_WD_wid(sb, val) sb[0] = val
#define WD_wid_front 0x00
#define WD_wid_back 0x80

  /* 0x01 - Reserved (bits 7-1), AUTO (bit 0)
   *        Use 0x00 for 3091, 3096
   */
#define set_WD_auto(sb, val) setbitfield(sb + 0x01, 1, 0, val)
#define get_WD_auto(sb)	getbitfield(sb + 0x01, 1, 0)

  /* 0x02,0x03 - X resolution in dpi
   *        3091 supports 50-300 in steps of 1
   *        3096 suppors 200,240,300,400; or 100-1600 in steps of 4
   *             if image processiong option installed
   */
#define set_WD_Xres(sb, val) putnbyte(sb + 0x02, val, 2)
#define get_WD_Xres(sb)	getnbyte(sb + 0x02, 2)

  /* 0x04,0x05 - X resolution in dpi
   *        3091 supports 50-600 in steps of 1; 75,150,300,600 only
   *             in color mode
   *        3096 suppors 200,240,300,400; or 100-1600 in steps of 4
   *             if image processiong option installed
   */
#define set_WD_Yres(sb, val) putnbyte(sb + 0x04, val, 2)
#define get_WD_Yres(sb)	getnbyte(sb + 0x04, 2)

  /* 0x06-0x09 - Upper Left X in 1/1200 inch
   */
#define set_WD_ULX(sb, val) putnbyte(sb + 0x06, val, 4)
#define get_WD_ULX(sb) getnbyte(sb + 0x06, 4)

  /* 0x0a-0x0d - Upper Left Y in 1/1200 inch
   */
#define set_WD_ULY(sb, val) putnbyte(sb + 0x0a, val, 4)
#define get_WD_ULY(sb) getnbyte(sb + 0x0a, 4)

  /* 0x0e-0x11 - Width in 1/1200 inch
   *        3091 left+width max 10200
   *        3096 left+width max 14592
   *        also limited to page size, see bytes 0x35ff.
   */
#define set_WD_width(sb, val) putnbyte(sb + 0x0e, val, 4)
#define get_WD_width(sb) getnbyte(sb + 0x0e, 4)

  /* 0x12-0x15 - Height in 1/1200 inch
   *        3091 top+height max 16832
   *        3096 top+height max 20736, also if left+width>13199,
   *             top+height has to be less than 19843
   */
#define set_WD_length(sb, val) putnbyte(sb + 0x12, val, 4)
#define get_WD_length(sb) getnbyte(sb + 0x12, 4)

  /* 0x16 - Brightness
   *        3091 always use 0x00
   *        3096 if in halftone mode, 8 levels supported (01-1F, 20-3F,
   ..., E0-FF)
   *             use 0x00 for user defined dither pattern
   */
#define set_WD_brightness(sb, val) sb[0x16] = val
#define get_WD_brightness(sb)  sb[0x16]

  /* 0x17 - Threshold
   *        3091 0x00 = use floating slice; 0x01..0xff fixed slice
   *             with 0x01=brightest, 0x80=medium, 0xff=darkest;
   *             only effective for line art mode.
   *        3096 0x00 = use "simplified dynamic treshold", otherwise
   *             same as above but resolution is only 64 steps.
   */
#define set_WD_threshold(sb, val) sb[0x17] = val
#define get_WD_threshold(sb)  sb[0x17]

  /* 0x18 - Contrast
   *        3091 - not supported, always use 0x00
   *        3096 - the same
   */
#define set_WD_contrast(sb, val) sb[0x18] = val
#define get_WD_contrast(sb) sb[0x18]

  /* 0x19 - Image Composition (color mode)
   *        3091 - use 0x00 for line art, 0x01 for halftone, 
   *               0x02 for grayscale, 0x05 for color.
   *        3096 - same but minus color.
   */
#define set_WD_composition(sb, val)  sb[0x19] = val
#define get_WD_composition(sb) sb[0x19]
#define WD_comp_LA 0
#define WD_comp_HT 1
#define WD_comp_GS 2
#define WD_comp_CL 3
#define WD_comp_CH 4
#define WD_comp_CG 5

  /* 0x1a - Depth
   *        3091 - use 0x01 for b/w or 0x08 for gray/color
   *        3096 - use 0x01 for b/w or 0x08 for gray
   */
#define set_WD_bitsperpixel(sb, val) sb[0x1a] = val
#define get_WD_bitsperpixel(sb)	sb[0x1a]

  /* 0x1b,0x1c - Halftone Pattern
   *        3091 byte 1b: 00h default(=dither), 01h dither, 
   *                      02h error dispersion
   *                  1c: 00 dark images, 01h dark text+images, 
   *                      02h light images,
   *                      03h light text+images, 80h download pattern
   *        3096: 1b unused; 1c bit 7=1: use downloadable pattern,
   *              bit 7=0: use builtin pattern; rest of byte 1b denotes
   *              pattern number, three builtin and five downloadable
   *              supported; higher numbers = error.
   */
#define set_WD_ht_type(sb, val) sb[0x1b] = val
#define get_WD_ht_type(sb)	sb[0x1b]
#define WD_ht_type_DEFAULT 0
#define WD_ht_type_DITHER 1
#define WD_ht_type_DIFFUSION 2

#define set_WD_ht_pattern(sb, val) sb[0x1c] = val
#define get_WD_ht_pattern(sb)	   sb[0x1c]

  /* 0x1d - Reverse image, padding type
   *        3091: bit 7=1: reverse black&white
   *              bits 0-2: padding type, must be 0
   *        3096: the same; bit 7 must be set for gray and not 
   *              set for b/w. 
   */
#define set_WD_rif(sb, val) setbitfield(sb + 0x1d, 1, 7, val)
#define get_WD_rif(sb)	getbitfield(sb + 0x1d, 1, 7)

  /* 0x1e,0x1f - Bit ordering
   *        3091 not supported, use 0x00
   *        3096 not supported, use 0x00
   */
#define set_WD_bitorder(sb, val) putnbyte(sb + 0x1e, val, 2)
#define get_WD_bitorder(sb)	getnbyte(sb + 0x1e, 2)

  /* 0x20 - compression type
   *          not supported on smaller models, use 0x00
   */
#define set_WD_compress_type(sb, val)  sb[0x20] = val
#define get_WD_compress_type(sb) sb[0x20]
#define WD_cmp_NONE 0
#define WD_cmp_MH   1
#define WD_cmp_MR   2
#define WD_cmp_MMR  3
#define WD_cmp_JBIG 0x80
#define WD_cmp_JPG1 0x81
#define WD_cmp_JPG2 0x82
#define WD_cmp_JPG3 0x83


  /* 0x21 - compression argument
   *          specify "k" parameter with MR compress,
   *          or with JPEG- Q param, 0-7
   */
#define set_WD_compress_arg(sb, val)  sb[0x21] = val
#define get_WD_compress_arg(sb) sb[0x21]

  /* 0x22-0x27 - reserved */

  /* 0x28 - vendor unique id code, decides meaning of remaining bytes
   *        0xc1 = color mode (fi-series)
   *        0xc0 = weird mode (M3091 and M3092)
   *        0x00 = mono mode (other M-series and fi-series)
   */
#define set_WD_vendor_id_code(sb, val)  sb[0x28] = val
#define get_WD_vendor_id_code(sb) sb[0x28]
#define WD_VUID_MONO 0x00
#define WD_VUID_3091 0xc0
#define WD_VUID_COLOR 0xc1

  /* 0x29 common gamma */
#define set_WD_gamma(sb, val)  sb[0x29] = val
#define get_WD_gamma(sb) sb[0x29]
#define WD_gamma_DEFAULT 0
#define WD_gamma_NORMAL  1
#define WD_gamma_SOFT    2
#define WD_gamma_SHARP   3

/*==================================================================*/
/* 0x2a-0x3F - vary based on vuid */

/*==================================================================*/
/* vuid 0x00, mono params */

#define set_WD_outline(sb, val)  setbitfield(sb + 0x2a, 1, 7, val)
#define get_WD_outline(sb) getbitfield(sb + 0x2a, 1, 7)

#define set_WD_emphasis(sb, val)  sb[0x2b] = val
#define get_WD_emphasis(sb) sb[0x2b]

#define set_WD_separation(sb, val)  setbitfield(sb + 0x2c, 1, 7, val)
#define get_WD_separation(sb) getbitfield(sb + 0x2c, 1, 7)

#define set_WD_mirroring(sb, val)  setbitfield(sb + 0x2d, 1, 7, val)
#define get_WD_mirroring(sb) getbitfield(sb + 0x2d, 1, 7)

/* SDTC also called Auto-II mode?*/
#define set_WD_variance(sb, val)  sb[0x2e] = val
#define get_WD_variance(sb) sb[0x2e]

/* DTC also called Auto-I mode?*/
/*warning: filtering uses inverse logic*/
#define set_WD_filtering(sb, val) setbitfield(sb + 0x2f, 1, 7, val)
#define get_WD_filtering(sb) getbitfield(sb + 0x2f, 1, 7)

/*warning: smoothing uses inverse logic*/
#define set_WD_smoothing(sb, val) setbitfield(sb + 0x2f, 3, 5, val)
#define get_WD_smoothing(sb) getbitfield(sb + 0x2f, 3, 5)

#define set_WD_gamma_curve(sb, val) setbitfield(sb + 0x2f, 3, 3, val)
#define get_WD_gamma_curve(sb) getbitfield(sb + 0x2f, 3, 3)

#define set_WD_threshold_curve(sb, val) setbitfield(sb + 0x2f, 7, 0, val)
#define get_WD_threshold_curve(sb) getbitfield(sb + 0x2f, 7, 0)

/*warning: noise removal uses inverse logic*/
#define set_WD_noise_removal(sb, val) setbitfield(sb + 0x30, 1, 5, !val)
#define get_WD_noise_removal(sb) !getbitfield(sb + 0x30, 1, 5)

#define set_WD_matrix5x5(sb, val) setbitfield(sb + 0x30, 1, 4, val)
#define get_WD_matrix5x5(sb) getbitfield(sb + 0x30, 1, 4)
#define set_WD_matrix4x4(sb, val) setbitfield(sb + 0x30, 1, 3, val)
#define get_WD_matrix4x4(sb) getbitfield(sb + 0x30, 1, 3)
#define set_WD_matrix3x3(sb, val) setbitfield(sb + 0x30, 1, 2, val)
#define get_WD_matrix3x3(sb) getbitfield(sb + 0x30, 1, 2)
#define set_WD_matrix2x2(sb, val) setbitfield(sb + 0x30, 1, 1, val)
#define get_WD_matrix2x2(sb) getbitfield(sb + 0x30, 1, 1)

#define set_WD_background(sb, val) setbitfield(sb + 0x30, 1, 0, val)
#define get_WD_background(sb) getbitfield(sb + 0x30, 1, 0)
#define WD_background_WHITE  0
#define WD_background_BLACK  1

/*31 reserved*/

#define set_WD_wl_follow(sb, val) setbitfield(sb + 0x32, 3, 6, val)
#define get_WD_wl_follow(sb) getbitfield(sb + 0x32, 3, 6)
#define WD_wl_follow_DEFAULT  0
#define WD_wl_follow_ON  2
#define WD_wl_follow_OFF 3

#define set_WD_subwindow_list(sb, val) putnbyte(sb + 0x33, val, 2)
#define get_WD_subwindow_list(sb)	getnbyte(sb + 0x33, 2)

/* 0x35-0x3d - paper size */
#define set_WD_paper_selection(sb, val) setbitfield(sb + 0x35, 3, 6, val)
#define WD_paper_SEL_UNDEFINED     0
#define WD_paper_SEL_NON_STANDARD  3

#define set_WD_paper_width_X(sb, val) putnbyte(sb + 0x36, val, 4)
#define get_WD_paper_width_X(sb)	getnbyte(sb + 0x36, 4)

#define set_WD_paper_length_Y(sb, val) putnbyte(sb+0x3a, val, 4)
#define get_WD_paper_length_Y(sb)	getnbyte(sb+0x3a, 4)

/* 3e switch ipc mode */
#define set_WD_ipc_mode(sb, val) setbitfield(sb + 0x3e, 3, 6, val)
#define get_WD_ipc_mode(sb) getbitfield(sb + 0x3e, 3, 6)
#define WD_ipc_DEFAULT  0
#define WD_ipc_DTC      1
#define WD_ipc_SDTC     2

/*3f reserved*/

/*==================================================================*/
/* vuid 0xc1, color params */

#define set_WD_scanning_order(sb, val)  sb[0x2a] = val
#define get_WD_scanning_order(sb) sb[0x2a]
#define WD_SCAN_ORDER_LINE 0
#define WD_SCAN_ORDER_DOT 1
#define WD_SCAN_ORDER_FACE 2

#define set_WD_scanning_order_arg(sb, val)  sb[0x2b] = val
#define get_WD_scanning_order_arg(sb) sb[0x2b]
#define WD_SCAN_ARG_RGB 0
#define WD_SCAN_ARG_RBG 1
#define WD_SCAN_ARG_GRB 2
#define WD_SCAN_ARG_GBR 3
#define WD_SCAN_ARG_BRG 4
#define WD_SCAN_ARG_BGR 5

/*2c-2d reserved*/

/*like vuid 00, but in different location*/
#define set_WD_c1_emphasis(sb, val)  sb[0x2e] = val
#define get_WD_c1_emphasis(sb) sb[0x2e]
#define set_WD_c1_mirroring(sb, val)  setbitfield(sb + 0x2f, 1, 7, val)
#define get_WD_c1_mirroring(sb) getbitfield(sb + 0x2f, 1, 7)

/*30-31 reserved*/

/*32 wlf (see vuid 00)*/

/*33-34 reserved*/

/*35-3d paper size (see vuid 00)*/

/*3e-3f reserved*/

/*==================================================================*/
/* vuid 0xc0, 3091/2 params */

/*2a-2b same as vuid 0xc1*/

#define set_WD_lamp_color(sb, val)  sb[0x2d] = val
#define get_WD_lamp_color(sb) sb[0x2d]
#define WD_LAMP_DEFAULT 0x00
#define WD_LAMP_BLUE 0x01
#define WD_LAMP_RED 0x02
#define WD_LAMP_GREEN 0x04

/*2e-31 reserved*/

#define set_WD_quality(sb, val)  sb[0x32] = val
#define get_WD_quality(sb) sb[0x32]
#define WD_QUAL_NORMAL 0x00
#define WD_QUAL_HIGH   0x02

/*33-34 reserved*/

/*35-3d paper size (see vuid 00)*/

/*3e-3f reserved*/

/*FIXME: more params here*/

/* ==================================================================== */

#endif
