#ifndef KODAK_CMD_H
#define KODAK_CMD_H

/* 
 * Part of SANE - Scanner Access Now Easy.
 * 
 * Please see to opening comments in kodak.c
 */

/* ==================================================================== */
/* helper char array manipulation functions */

static void
setbitfield (unsigned char *pageaddr, int mask, int shift, int val)
{
    *pageaddr = (*pageaddr & ~(mask << shift)) | ((val & mask) << shift);
}

static int
getbitfield (unsigned char *pageaddr, int shift, int mask)
{
    return ((*pageaddr >> shift) & mask);
}

static int
getnbyte (unsigned char *pnt, int nbytes)
{
    unsigned int result = 0;
    int i;
  
    for (i = 0; i < nbytes; i++)
        result = (result << 8) | (pnt[i] & 0xff);
    return result;
}

static void
putnbyte (unsigned char *pnt, unsigned int value, unsigned int nbytes)
{
    int i;
  
    for (i = nbytes - 1; i >= 0; i--) {
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

/* out */
#define REQUEST_SENSE_code      0x03
#define REQUEST_SENSE_len       6

#define RS_return_size                    0x12

/* in */
#define get_RS_information_valid(b)       getbitfield(b + 0x00, 7, 1)
#define get_RS_error_code(b)              getbitfield(b + 0x00, 0, 0x7f)
#define RS_error_code_current             0x70
#define RS_error_code_deferred            0x71

#define get_RS_segment_num(b)             b[1]

#define get_RS_filemark(b)                getbitfield(b + 0x02, 7, 1)
#define get_RS_EOM(b)                     getbitfield(b + 0x02, 6, 1)
#define get_RS_ILI(b)                     getbitfield(b + 0x02, 5, 1)
#define get_RS_sense_key(b)               getbitfield(b + 0x02, 0, 0x0f)

#define get_RS_information(b)             getnbyte(b+0x03, 4)

#define get_RS_additional_length(b)       b[0x07]

#define get_RS_cmd_info(b)                getnbyte(b+8, 4)

#define get_RS_ASC(b)                     b[0x0c]
#define get_RS_ASCQ(b)                    b[0x0d]
#define get_RS_FRUC(b)                    b[0x0e]
#define get_RS_SKSV(b)                    getbitfield(b+0x0f,7,1)
#define get_RS_SKSB(b)                    getnbyte(b+0x0f, 3)

/* ==================================================================== */
/* INQUIRY */

/* out */
#define INQUIRY_code            0x12
#define INQUIRY_len             6

#define set_I_evpd(out, val)              setbitfield(out + 1, 1, 0, val)
#define set_I_page_code(out, val)         out[0x02]=val
#define I_page_code_default               0
#define set_I_data_length(out,n)          out[0x04]=n
#define I_data_len                        128

/* in */
#define get_I_periph_qual(in)             getbitfield(in, 5, 7)
#define I_periph_qual_valid               0x00
#define I_periph_qual_nolun               0x03
#define get_I_periph_devtype(in)          getbitfield(in, 0, 0x1f)
#define I_periph_devtype_scanner          0x06
#define I_periph_devtype_unknown          0x1f

/* dont use these, until vendor */
#define get_I_rmb(in)                     getbitfield(in + 1, 7, 1)
#define get_I_devtype_qual(in)            getbitfield(in + 1, 0, 0x7f)

#define get_I_iso_version(in)             getbitfield(in + 2, 6, 3)
#define get_I_ecma_version(in)            getbitfield(in + 2, 3, 7)
#define get_I_ansi_version(in)            getbitfield(in + 2, 0, 7)

#define get_I_aenc(in)                    getbitfield(in + 3, 7, 1)
#define get_I_trmiop(in)                  getbitfield(in + 3, 6, 1)
#define get_I_resonse_format(in)          getbitfield(in + 3, 0, 0x0f)

#define get_I_length(in)                  getnbyte(in + 4, 1)

#define get_I_reladr(in)                  getbitfield(in + 7, 7, 1)
#define get_I_wbus32(in)                  getbitfield(in + 7, 6, 1)
#define get_I_wbus16(in)                  getbitfield(in + 7, 5, 1)
#define get_I_sync(in)                    getbitfield(in + 7, 4, 1)
#define get_I_linked(in)                  getbitfield(in + 7, 3, 1)
#define get_I_cmdque(in)                  getbitfield(in + 7, 1, 1)
#define get_I_sftre(in)                   getbitfield(in + 7, 0, 1)

#define get_I_vendor(in, buf)             strncpy(buf,(char *)in + 0x08, 0x08)
#define get_I_product(in, buf)            strncpy(buf,(char *)in + 0x10, 0x10)
#define get_I_version(in, buf)            strncpy(buf,(char *)in + 0x20, 0x04)
#define get_I_build(in, buf)              strncpy(buf,(char *)in + 0x24, 0x02)

#define get_I_mf_disable(in)              getbitfield(in + 38, 7, 1)
#define get_I_checkdigit(in)              getbitfield(in + 38, 6, 1)
#define get_I_front_prism(in)             getbitfield(in + 38, 5, 1)
#define get_I_compressed_gray(in)         getbitfield(in + 38, 4, 1)
#define get_I_front_toggle(in)            getbitfield(in + 38, 3, 1)
#define get_I_front_dp1(in)               getbitfield(in + 38, 2, 1)
#define get_I_front_color(in)             getbitfield(in + 38, 1, 1)
#define get_I_front_atp(in)               getbitfield(in + 38, 0, 1)

#define get_I_dp1_180(in)                 getbitfield(in + 39, 7, 1)
#define get_I_mf_pause(in)                getbitfield(in + 39, 6, 1)
#define get_I_rear_prism(in)              getbitfield(in + 39, 5, 1)
#define get_I_uncompressed_gray(in)       getbitfield(in + 39, 4, 1)
#define get_I_rear_toggle(in)             getbitfield(in + 39, 3, 1)
#define get_I_rear_dp1(in)                getbitfield(in + 39, 2, 1)
#define get_I_rear_color(in)              getbitfield(in + 39, 1, 1)
#define get_I_rear_atp(in)                getbitfield(in + 39, 0, 1)

#define get_I_min_bin_res(in)             getnbyte(in + 40, 2)
#define get_I_max_bin_res(in)             getnbyte(in + 42, 2)
#define get_I_min_col_res(in)             getnbyte(in + 44, 2)
#define get_I_max_col_res(in)             getnbyte(in + 46, 2)

#define get_I_max_image_width(in)         getnbyte(in + 48, 4)
#define get_I_max_image_length(in)        getnbyte(in + 52, 4)

/* 56-95 reserved */

#define get_I_finecrop(in)                getbitfield(in + 96, 7, 1)
#define get_I_ithresh(in)                 getbitfield(in + 96, 6, 1)
#define get_I_ecd(in)                     getbitfield(in + 96, 3, 1)
#define get_I_vblr(in)                    getbitfield(in + 96, 2, 1)
#define get_I_elevator(in)                getbitfield(in + 96, 1, 1)
#define get_I_relcrop(in)                 getbitfield(in + 96, 0, 1)

#define get_I_cdeskew(in)                 getbitfield(in + 97, 7, 1)
#define get_I_ia(in)                      getbitfield(in + 97, 6, 1)
#define get_I_patch(in)                   getbitfield(in + 97, 5, 1)
#define get_I_nullmode(in)                getbitfield(in + 97, 4, 1)
#define get_I_sabre(in)                   getbitfield(in + 97, 3, 1)
#define get_I_lddds(in)                   getbitfield(in + 97, 2, 1)
#define get_I_uddds(in)                   getbitfield(in + 97, 1, 1)
#define get_I_fixedgap(in)                getbitfield(in + 97, 0, 1)

#define get_I_hr_printer(in)              getbitfield(in + 98, 6, 1)
#define get_I_elev_100_250(in)            getbitfield(in + 98, 5, 1)
#define get_I_udds_individual(in)         getbitfield(in + 98, 4, 1)
#define get_I_auto_color(in)              getbitfield(in + 98, 3, 1)
#define get_I_wb(in)                      getbitfield(in + 98, 2, 1)
#define get_I_es(in)                      getbitfield(in + 98, 1, 1)
#define get_I_fc(in)                      getbitfield(in + 98, 0, 1)

#define get_I_max_rate(in)                getnbyte(in + 99, 2)
#define get_I_buffer_size(in)             getnbyte(in + 101, 4)

/* ==================================================================== */
/* RESERVE UNIT */

#define RESERVE_UNIT_code       0x16
#define RESERVE_UNIT_len        6

/* ==================================================================== */
/* RELEASE UNIT */

#define RELEASE_UNIT_code       0x17
#define RELEASE_UNIT_len        6

/* ==================================================================== */
/* SCAN */

#define SCAN_code               0x1b
#define SCAN_len                10

#define set_SC_xfer_length(sb, val) sb[0x04] = (unsigned char)val

/* ==================================================================== */
/* SEND DIAGNOSTIC */

#define SEND_DIAGNOSTIC_code    0x1d
#define SEND_DIAGNOSTIC_len     6

/* ==================================================================== */
/* SET WINDOW */

#define SET_WINDOW_code         0x24
#define SET_WINDOW_len          10

/* transfer length is header+descriptor (8+64) */
#define set_SW_xferlen(sb, len) putnbyte(sb + 0x06, len, 3)

/* ==================================================================== */
/* GET WINDOW */

#define GET_WINDOW_code         0x25
#define GET_WINDOW_len          10

#define set_GW_single(sb, val) setbitfield(sb + 1, 1, 0, val)
#define set_GW_wid(sb, len) sb[5] = len

/* window transfer length is for following header+descriptor (8+64) */
#define set_GW_xferlen(sb, len) putnbyte(sb + 0x06, len, 3)

/* ==================================================================== */
/* WINDOW HEADER, used by get and set */

#define WINDOW_HEADER_len       8

/* header transfer length is for following descriptor (64) */
#define set_WH_data_len(sb, len)  putnbyte(sb, len, 2)
#define set_WH_desc_len(sb, len)  putnbyte(sb + 0x06, len, 2)

/* ==================================================================== */
/* WINDOW descriptor, used by get and set */

#define WINDOW_DESCRIPTOR_len   64

#define set_WD_wid(sb, val) sb[0]=val
#define WD_wid_front_binary 0x01
#define WD_wid_back_binary  0x02
#define WD_wid_front_color  0x03
#define WD_wid_back_color   0x04

/* color/gray = 100,150,200,240,300 */
/* binary =             200,240,300,400 */
#define set_WD_Xres(sb, val) putnbyte(sb + 0x02, val, 2)
#define get_WD_Xres(sb)	getnbyte(sb + 0x02, 2)

/* color/gray = 100,150,200,240,300 */
/* binary =             200,240,300,400 */
#define set_WD_Yres(sb, val) putnbyte(sb + 0x04, val, 2)
#define get_WD_Yres(sb)	getnbyte(sb + 0x04, 2)

#define set_WD_ULX(sb, val) putnbyte(sb + 0x06, val, 4)
#define get_WD_ULX(sb) getnbyte(sb + 0x06, 4)

#define set_WD_ULY(sb, val) putnbyte(sb + 0x0a, val, 4)
#define get_WD_ULY(sb) getnbyte(sb + 0x0a, 4)

#define set_WD_width(sb, val) putnbyte(sb + 0x0e, val, 4)
#define get_WD_width(sb) getnbyte(sb + 0x0e, 4)

#define set_WD_length(sb, val) putnbyte(sb + 0x12, val, 4)
#define get_WD_length(sb) getnbyte(sb + 0x12, 4)

#define set_WD_brightness(sb, val) sb[0x16] = val
#define get_WD_brightness(sb)  sb[0x16]

#define set_WD_threshold(sb, val) sb[0x17] = val
#define get_WD_threshold(sb)  sb[0x17]

#define set_WD_contrast(sb, val) sb[0x18] = val
#define get_WD_contrast(sb) sb[0x18]

#define set_WD_composition(sb, val)  sb[0x19] = val
#define get_WD_composition(sb) sb[0x19]
#define WD_compo_LINEART 0
#define WD_compo_HALFTONE 1
#define WD_compo_MULTILEVEL 5

/* 1, 8, or 24 */
#define set_WD_bitsperpixel(sb, val) sb[0x1a] = val
#define get_WD_bitsperpixel(sb)	sb[0x1a]

/* not used? */
#define set_WD_halftone(sb, val) putnbyte(sb + 0x1b, val, 2)
#define get_WD_halftone(sb)	getnbyte(sb + 0x1b, 2)

#define set_WD_rif(sb, val) setbitfield(sb + 0x1d, 1, 7, val)
#define get_WD_rif(sb)	getbitfield(sb + 0x1d, 7, 1)

/* always set to 1? */
#define set_WD_bitorder(sb, val) putnbyte(sb + 0x1e, val, 2)
#define get_WD_bitorder(sb)	getnbyte(sb + 0x1e, 2)

#define set_WD_compress_type(sb, val)  sb[0x20] = val
#define get_WD_compress_type(sb) sb[0x20]
#define WD_compr_NONE 0
#define WD_compr_FAXG4 3
#define WD_compr_JPEG 0x80

/* not used? */
#define set_WD_compress_arg(sb, val)  sb[0x21] = val
#define get_WD_compress_arg(sb) sb[0x21]

/* kodak specific stuff starts here */
#define set_WD_noise_filter(sb, val) setbitfield(sb + 40, 7, 2, val)
#define WD_nf_NONE 0
#define WD_nf_LONE 1
#define WD_nf_MAJORITY 2
#define get_WD_noise_filter(sb)	getbitfield(sb + 40, 7, 1)

#define set_WD_allow_zero(sb, val) setbitfield(sb + 40, 1, 1, val)
#define get_WD_allow_zero(sb, val) getbitfield(sb + 40, 1, 1)

#define set_WD_vblr(sb, val) setbitfield(sb + 40, 1, 0, val)
#define get_WD_vblr(sb, val) getbitfield(sb + 40, 0, 1)

#define set_WD_image_overscan(sb, val) putnbyte(sb + 41, val, 2)

#define set_WD_IS(sb, val) setbitfield(sb + 43, 1, 7, val)

#define set_WD_deskew(sb, val) setbitfield(sb + 43, 1, 6, val)
#define WD_deskew_DISABLE 0
#define WD_deskew_ENABLE 1

#define set_WD_BR(sb, val) setbitfield(sb + 43, 1, 5, val)
#define set_WD_BF(sb, val) setbitfield(sb + 43, 1, 4, val)
#define set_WD_ED(sb, val) setbitfield(sb + 43, 1, 3, val)
#define set_WD_HR(sb, val) setbitfield(sb + 43, 1, 2, val)

#define set_WD_cropping(sb, val) setbitfield(sb + 43, 3, 0, val)
#define WD_crop_AUTO 0
#define WD_crop_FIXED 1
#define WD_crop_RELATIVE 2
#define WD_crop_FINE 3

#define set_WD_ithresh(sb, val) setbitfield(sb + 44, 1, 7, val)
#define WD_ithresh_DISABLE 0
#define WD_ithresh_ENABLE 1

#define set_WD_dropout_color(sb, val) setbitfield(sb + 44, 7, 0, val)

#define set_WD_dropout_bg(sb, val)  sb[45] = val

#define set_WD_dropout_thresh(sb, val)  sb[46] = val

#define set_WD_dropout_source(sb, val)  sb[47] = val

#define set_WD_disable_gamma(sb, val)  sb[48] = val

#define set_WD_ortho_rotation(sb, val)  sb[49] = val

#define set_WD_fine_crop_max(sb, val) putnbyte(sb + 50, val, 2)

#define set_WD_color_detect_thresh(sb, val)  sb[52] = val

#define set_WD_color_detect_area(sb, val)  sb[53] = val

#define set_WD_border_add(sb, val)  sb[54] = val

#define max_WDB_size 0xc8

/* ==================================================================== */
/* READ */

#define READ_code               0x28
#define READ_len                10

/* ==================================================================== */
/* SEND */

#define SEND_code               0x2a
#define SEND_len                10

/* ==================================================================== */
/* READ and SEND are basically the same, so the following 'SR' macros */

/* output */
#define set_SR_datatype_code(sb, val)   sb[0x02] = val
#define SR_datatype_imagedata	        0x00
#define SR_datatype_random              0x80 
#define SR_datatype_imageheader         0x81
#define SR_len_imageheader              1088

#define set_SR_datatype_qual(sb, val)   memcpy(sb + 4, val, 2)
#define set_SR_xfer_length(sb, val)     putnbyte(sb + 6, val, 3)

/* if the data type is 'imageheader': */
#define get_SR_ih_header_length(in)     getnbyte(in + 0x00, 4)
#define get_SR_ih_image_length(in)      getnbyte(in + 0x04, 4)
#define get_SR_ih_image_id(in)          in[8]
#define get_SR_ih_resolution(in)        getnbyte(in + 9, 2)
#define get_SR_ih_ulx(in)               getnbyte(in + 11, 4)
#define get_SR_ih_uly(in)               getnbyte(in + 15, 4)
#define get_SR_ih_width(in)             getnbyte(in + 19, 4)
#define get_SR_ih_length(in)            getnbyte(in + 23, 4)
#define get_SR_ih_bpp(in)               in[27]
#define get_SR_ih_comp_type(in)         in[28]
#define get_SR_ih_ACD(in)               getbit(in + 29, 1, 5)  
#define get_SR_ih_MF(in)                getbit(in + 29, 1, 4)  
#define get_SR_ih_RIF(in)               getbit(in + 29, 1, 2)  
#define get_SR_ih_ID(in)                getbit(in + 29, 1, 1)  
#define get_SR_ih_DE(in)                getbit(in + 29, 1, 0)  
#define get_SR_ih_skew_angle(in)        getnbyte(in + 30, 2)
#define get_SR_ih_ia_level(in)          in[32]
#define get_SR_ih_ia(in)                getnbyte(in + 33, 60)
#define get_SR_ih_print_string(in)      getnbyte(in + 93, 60)
#define get_SR_ih_seqence_counter(in)   getnbyte(in + 173, 4)
#define get_SR_ih_ia_f1_definition(in)  in[177]
#define get_SR_ih_ia_f1_value(in)       getnbyte(in + 178, 18)
#define get_SR_ih_ia_f2_definition(in)  in[196]
#define get_SR_ih_ia_f2_value(in)       getnbyte(in + 197, 18)
#define get_SR_ih_ia_f3_definition(in)  in[215]
#define get_SR_ih_ia_f3_value(in)       getnbyte(in + 216, 18)
#define get_SR_ih_ia_f4_definition(in)  in[234]
#define get_SR_ih_ia_f5_value(in)       getnbyte(in + 235, 18)
#define get_SR_ih_PD(in)                getbit(in + 253, 1, 7)
#define get_SR_ih_patch_type(in)        getbit(in + 253, 0x7f, 0)
#define get_SR_ih_deskew_confidence(in) in[255]
#define get_SR_ih_bt_contrast_perc(in)  in[256]
#define get_SR_ih_bt_contrast(in)       getnbyte(in + 257, 2)
#define get_SR_ih_bt_threshold(in)      in[259]
#define get_SR_ih_sum_histogram(in)     getnbyte(in + 260, 256)
#define get_SR_ih_diff_histogram(in)    getnbyte(in + 516, 256)
#define get_SR_ih_gamma_table(in)       getnbyte(in + 772, 256)
#define get_SR_ih_auto_color_area(in)   in[1028]
#define get_SR_ih_auto_color_thresh(in) in[1029]

/* ==================================================================== */
/* if the data type is 'random', we have all kinds of 2 byte  */
/* qualifiers and oddly sized commands. some are bidirectional    */
/* while others are only send or read */

/* all payloads for these seem to start with 4 byte inclusive length? */
#define set_SR_payload_len(sb, val)     putnbyte(sb, val, 4)

/*  */
#define SR_qual_jpeg_quant  "JQ" /*both*/
#define SR_len_jpeg_quant   262  /*front and back tables*/

/*  */
#define SR_qual_config      "SC" /*both*/
#define SR_len_config       512  /*lots of stuff*/
#define set_SR_sc_length(in,val)     putnbyte(in, val, 4)
#define get_SR_sc_length(in)         getnbyte(in, 4)

#define set_SR_sc_io1(in,val)     putnbyte(in + 4, val, 1)
#define get_SR_sc_io1(in)         getnbyte(in + 4, 1)
#define set_SR_sc_io2(in,val)     putnbyte(in + 5, val, 1)
#define get_SR_sc_io2(in)         getnbyte(in + 5, 1)
#define set_SR_sc_io3(in,val)     putnbyte(in + 6, val, 1)
#define get_SR_sc_io3(in)         getnbyte(in + 6, 1)
#define set_SR_sc_io4(in,val)     putnbyte(in + 7, val, 1)
#define get_SR_sc_io4(in)         getnbyte(in + 7, 1)
#define SR_sc_io_none         0
#define SR_sc_io_front_binary 1
#define SR_sc_io_rear_binary  2
#define SR_sc_io_front_color  3
#define SR_sc_io_rear_color   4

#define set_SR_sc_trans_to(in,val)     putnbyte(in + 13, val, 2)
#define get_SR_sc_trans_to(in)         getnbyte(in + 13, 2)
#define set_SR_sc_trans_to_resp(in,val)     putnbyte(in + 15, val, 1)
#define get_SR_sc_trans_to_resp(in)         getnbyte(in + 15, 1)

#define get_SR_sc_DLS(in)         getbitfield(in + 16, 7, 1)
#define set_SR_sc_DLS(in, val)    setbitfield(in + 16, 1, 7)
#define get_SR_sc_DMS(in)         getbitfield(in + 16, 6, 1)
#define set_SR_sc_DMS(in, val)    setbitfield(in + 16, 1, 6)
#define get_SR_sc_DRS(in)         getbitfield(in + 16, 5, 1)
#define set_SR_sc_DRS(in, val)    setbitfield(in + 16, 1, 5)
#define get_SR_sc_UD_mode(in)     getbitfield(in + 16, 0, 0x1f)
#define set_SR_sc_UD_mode(in, val)   setbitfield(in + 16, 0x1f, 0)
#define get_SR_sc_LD_length(in)      getnbyte(in + 17, 2)
#define set_SR_sc_LD_length(in, val) setnbyte(in + 17, 2)
#define get_SR_sc_DF_resp(in)        getnbyte(in + 19, 1)
#define set_SR_sc_DF_resp(in, val)   setnbyte(in + 19, 1)

#define get_SR_sc_TP_mode(in)     getnbyte(in + 20, 1)
#define get_SR_sc_CPT(in)         getbitfield(in + 21, 0, 1)
#define get_SR_sc_POD_mode(in)    getnbyte(in + 22, 2)
#define get_SR_sc_batch_mode(in)  getnbyte(in + 24, 2)
#define get_SR_sc_batch_level(in) getnbyte(in + 26, 1)
#define get_SR_sc_start_batch(in) getnbyte(in + 27, 1)
#define get_SR_sc_end_batch(in)   getnbyte(in + 28, 1)
#define get_SR_sc_patch_conf_tone(in)   getnbyte(in + 29, 1)

#define get_SR_sc_T6_patch(in)    getbitfield(in + 30, 7, 1)
#define get_SR_sc_T5_patch(in)    getbitfield(in + 30, 6, 1)
#define get_SR_sc_T4_patch(in)    getbitfield(in + 30, 5, 1)
#define get_SR_sc_T3_patch(in)    getbitfield(in + 30, 4, 1)
#define get_SR_sc_T2_patch(in)    getbitfield(in + 30, 3, 1)
#define get_SR_sc_T1_patch(in)    getbitfield(in + 30, 2, 1)
#define get_SR_sc_trans_patch(in) getbitfield(in + 30, 0, 3)

#define get_SR_sc_IA_A_start(in)   getnbyte(in + 31, 18)
#define get_SR_sc_IA_B_start(in)   getnbyte(in + 49, 18)
#define get_SR_sc_IA_C_start(in)   getnbyte(in + 67, 18)
#define get_SR_sc_IA_D_start(in)   getnbyte(in + 85, 18)

#define get_SR_sc_IA_A_def(in)   getnbyte(in + 103, 1)
#define get_SR_sc_IA_B_def(in)   getnbyte(in + 104, 1)
#define get_SR_sc_IA_C_def(in)   getnbyte(in + 105, 1)
#define get_SR_sc_IA_D_def(in)   getnbyte(in + 106, 1)

#define get_SR_sc_IA_ltf_3(in)   getnbyte(in + 107, 1)
#define get_SR_sc_IA_ltf_2(in)   getnbyte(in + 108, 1)
#define get_SR_sc_IA_ltf_1(in)   getnbyte(in + 109, 1)

#define get_SR_sc_DP1_pos(in)    getnbyte(in + 110, 4)
#define get_SR_sc_hires_mode(in) getbitfield(in + 114, 4, 0xf)
#define get_SR_sc_DP1_font(in)   getbitfield(in + 114, 0, 0xf)
#define get_SR_sc_DP1_orient(in) getnbyte(in + 115, 1)
#define get_SR_sc_DP1_IA_format(in) getnbyte(in + 116, 1)
#define get_SR_sc_DP1_date_format(in) getnbyte(in + 117, 1)
#define get_SR_sc_DP1_date_delim(in) getnbyte(in + 118, 1)
#define get_SR_sc_DP1_start_seq(in) getnbyte(in + 119, 4)
#define get_SR_sc_DP1_seq_date_format(in) getnbyte(in + 123, 1)
#define get_SR_sc_DP1_seq_print_width(in) getnbyte(in + 124, 1)

#define get_SR_sc_DP1_msg1(in) getnbyte(in + 125, 40)
#define get_SR_sc_DP1_msg2(in) getnbyte(in + 165, 40)
#define get_SR_sc_DP1_msg3(in) getnbyte(in + 205, 40)
#define get_SR_sc_DP1_msg4(in) getnbyte(in + 245, 40)
#define get_SR_sc_DP1_msg5(in) getnbyte(in + 285, 40)
#define get_SR_sc_DP1_msg6(in) getnbyte(in + 325, 40)
#define get_SR_sc_DP1_l1_template(in) getnbyte(in + 365, 20)
#define get_SR_sc_DP1_l2_template(in) getnbyte(in + 385, 20)
#define get_SR_sc_DP1_l3_template(in) getnbyte(in + 405, 20)

#define get_SR_sc_UDFK1(in) getnbyte(in + 425, 1)
#define get_SR_sc_UDFK2(in) getnbyte(in + 426, 1)
#define get_SR_sc_UDFK3(in) getnbyte(in + 427, 1)
#define get_SR_sc_MPASS(in) getbitfield(in + 428, 1, 1)
#define get_SR_sc_CE(in) getbitfield(in + 428, 0, 1)
#define get_SR_sc_elevator_mode(in) getnbyte(in + 429, 1)
#define get_SR_sc_stacking_mode(in) getnbyte(in + 430, 1)

#define get_SR_sc_energy_star_to(in) getnbyte(in + 433, 1)

#define get_SR_sc_PR1(in) getbitfield(in + 435, 3, 1)
#define get_SR_sc_PR2(in) getbitfield(in + 435, 2, 1)
#define get_SR_sc_PR3(in) getbitfield(in + 435, 1, 1)
#define get_SR_sc_PR4(in) getbitfield(in + 435, 0, 1)

/*  */
#define SR_qual_color_table "CT" /*read*/
#define SR_len_color_table  52

/*  */
#define SR_qual_color_table "CT" /*read*/
#define SR_len_color_table  52

/*  */
#define SR_qual_clear       "CB" /*send*/
#define SR_len_clear        0

/*  */
#define SR_qual_end         "GX" /*send*/
#define SR_len_end          0

/* local and gmt time commands, same format */
#define SR_qual_clock       "LC" /*both*/
#define SR_qual_gmt         "GT" /*send*/
#define SR_len_time          10
#define set_SR_time_hour(sb, val)     putnbyte(sb+4, val, 1)
#define set_SR_time_min(sb, val)     putnbyte(sb+5, val, 1)
#define set_SR_time_mon(sb, val)     putnbyte(sb+6, val, 1)
#define set_SR_time_day(sb, val)     putnbyte(sb+7, val, 1)
#define set_SR_time_year(sb, val)     putnbyte(sb+8, val, 2)

/*  */
#define SR_qual_lamp        "LD" /*????*/
#define SR_len_lamp         0    /*????*/

/*  */
#define SR_qual_log_en    "L0" /*read*/
#define SR_qual_log_fr    "L1" /*read*/
#define SR_qual_log_it    "L2" /*read*/
#define SR_qual_log_de    "L3" /*read*/
#define SR_qual_log_br    "L4" /*read*/
#define SR_qual_log_jp    "L5" /*read*/
#define SR_qual_log_nl    "L6" /*read*/
#define SR_qual_log_es    "L7" /*read*/
#define SR_qual_log_cns   "L8" /*read*/
#define SR_qual_log_cnt   "L9" /*read*/
#define SR_qual_log_ko    "LA" /*read*/
#define SR_qual_log_ru    "LB" /*read*/
#define SR_qual_log_cz    "LE" /*read*/
#define SR_qual_log_tr    "LF" /*read*/

/*  */
#define SR_qual_startstop "SS" /*send*/
#define SR_len_startstop  5
#define set_SR_startstop_cmd(sb, val)     putnbyte(sb+4, val, 1)

/*  */
#define SR_qual_media     "CM" /*read*/
#define SR_len_media      5
#define get_SR_media_status(sb, val)     getnbyte(sb+4, 1)

/*  */
#define SR_qual_img_cal   "IC" /*send*/
#define SR_len_img_cal    0

/*  */
#define SR_qual_udds_cal  "UC" /*send*/
#define SR_len_udds_cal   0

/* ==================================================================== */
/* WRITE BUFFER */

#define WRITE_BUFFER_code       0x3b
#define WRITE_BUFFER_len        10

#define set_WB_mode(sb, val)            setbitfield(sb + 0x01, 7, 0, val)
#define WB_mode_front                   6
#define WB_mode_back                    7
#define set_WB_buffer_id(sb, val)       sb[0x02] = (unsigned char)val
#define WB_buffer_id_block(sb, val)     0
#define WB_buffer_id_last(sb, val)      1
#define set_WB_buffer_offset(sb, val)   putnbyte(sb + 3, val, 3)
#define set_WB_list_length(sb, val)     putnbyte(sb + 6, val, 3)

#endif
