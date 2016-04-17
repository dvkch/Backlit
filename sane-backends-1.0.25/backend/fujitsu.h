#ifndef FUJITSU_H
#define FUJITSU_H

/* 
 * Part of SANE - Scanner Access Now Easy.
 * Please see opening comment in fujitsu.c
 */

/* ------------------------------------------------------------------------- 
 * This option list has to contain all options for all scanners supported by
 * this driver. If a certain scanner cannot handle a certain option, there's
 * still the possibility to say so, later.
 */
enum fujitsu_Option
{
  OPT_NUM_OPTS = 0,

  OPT_STANDARD_GROUP,
  OPT_SOURCE, /*fb/adf/front/back/duplex*/
  OPT_MODE,   /*mono/gray/color*/
  OPT_RES,    /*a range or a list*/

  OPT_GEOMETRY_GROUP,
  OPT_PAGE_WIDTH,
  OPT_PAGE_HEIGHT,
  OPT_TL_X,
  OPT_TL_Y,
  OPT_BR_X,
  OPT_BR_Y,

  OPT_ENHANCEMENT_GROUP,
  OPT_BRIGHTNESS,
  OPT_CONTRAST,
  OPT_GAMMA,
  OPT_THRESHOLD,

  /*IPC*/
  OPT_RIF,
  OPT_HT_TYPE,
  OPT_HT_PATTERN,
  OPT_OUTLINE,
  OPT_EMPHASIS,
  OPT_SEPARATION,
  OPT_MIRRORING,
  OPT_WL_FOLLOW,
  OPT_IPC_MODE,

  /*IPC/DTC*/
  OPT_BP_FILTER,
  OPT_SMOOTHING,
  OPT_GAMMA_CURVE,
  OPT_THRESHOLD_CURVE,
  OPT_THRESHOLD_WHITE,
  OPT_NOISE_REMOVAL,
  OPT_MATRIX_5,
  OPT_MATRIX_4,
  OPT_MATRIX_3,
  OPT_MATRIX_2,

  /*IPC/SDTC*/
  OPT_VARIANCE,

  OPT_ADVANCED_GROUP,
  OPT_AWD,
  OPT_ALD,
  OPT_COMPRESS,
  OPT_COMPRESS_ARG,
  OPT_DF_ACTION,
  OPT_DF_SKEW,
  OPT_DF_THICKNESS,
  OPT_DF_LENGTH,
  OPT_DF_DIFF,
  OPT_DF_RECOVERY,
  OPT_PAPER_PROTECT,
  OPT_ADV_PAPER_PROT,
  OPT_STAPLE_DETECT,
  OPT_BG_COLOR,
  OPT_DROPOUT_COLOR,
  OPT_BUFF_MODE,
  OPT_PREPICK,
  OPT_OVERSCAN,
  OPT_SLEEP_TIME,
  OPT_OFF_TIME,
  OPT_DUPLEX_OFFSET,
  OPT_GREEN_OFFSET,
  OPT_BLUE_OFFSET,
  OPT_LOW_MEM,
  OPT_SIDE,
  OPT_HWDESKEWCROP,
  OPT_SWDESKEW,
  OPT_SWDESPECK,
  OPT_SWCROP,
  OPT_SWSKIP,
  OPT_HALT_ON_CANCEL,

  OPT_ENDORSER_GROUP,
  OPT_ENDORSER,
  OPT_ENDORSER_BITS,
  OPT_ENDORSER_VAL,
  OPT_ENDORSER_STEP,
  OPT_ENDORSER_Y,
  OPT_ENDORSER_FONT,
  OPT_ENDORSER_DIR,
  OPT_ENDORSER_SIDE,
  OPT_ENDORSER_STRING,

  OPT_SENSOR_GROUP,
  OPT_TOP,
  OPT_A3,
  OPT_B4,
  OPT_A4,
  OPT_B5,
  OPT_HOPPER,
  OPT_OMR,
  OPT_ADF_OPEN,
  OPT_SLEEP,
  OPT_SEND_SW,
  OPT_MANUAL_FEED,
  OPT_SCAN_SW,
  OPT_FUNCTION,
  OPT_INK_EMPTY,
  OPT_DOUBLE_FEED,
  OPT_ERROR_CODE,
  OPT_SKEW_ANGLE,
  OPT_INK_REMAIN,
  OPT_DUPLEX_SW,
  OPT_DENSITY_SW,

  /* must come last: */
  NUM_OPTIONS
};

/* used to control the max page-height, which varies by resolution */
struct y_size
{
  int res;
  int len;
};

struct fujitsu
{
  /* --------------------------------------------------------------------- */
  /* immutable values which are set during init of scanner.                */
  struct fujitsu *next;
  char device_name[1024];             /* The name of the device from sanei */
  int missing; 				/* used to mark unplugged scanners */

  /* --------------------------------------------------------------------- */
  /* immutable values which are set during reading of config file.         */
  int buffer_size;
  int connection;               /* hardware interface type */

  /* --------------------------------------------------------------------- */
  /* immutable values which are set during inquiry probing of the scanner. */
  /* members in order found in scsi data...                                */
  char vendor_name[9];          /* raw data as returned by SCSI inquiry.   */
  char model_name[17];          /* raw data as returned by SCSI inquiry.   */
  char version_name[5];         /* raw data as returned by SCSI inquiry.   */

  int color_raster_offset;      /* offset between r and b scan line and    */
                                /* between b and g scan line (0 or 4)      */

  int duplex_raster_offset;     /* offset between front and rear page when */
                                /* when scanning 3091 style duplex         */

  /* --------------------------------------------------------------------- */
  /* immutable values which are set during std VPD probing of the scanner. */
  /* members in order found in scsi data...                                */
  int basic_x_res;
  int basic_y_res;
  int step_x_res[6]; /*one for each mode*/
  int step_y_res[6]; /*one for each mode*/
  int max_x_res;
  int max_y_res;
  int min_x_res;
  int min_y_res;

  int std_res[16]; /*some scanners only support a few resolutions*/

  /* max scan size in pixels comes from scanner in basic res units */
  int max_x_basic;
  int max_y_basic;

  int can_overflow;
  int can_mode[6]; /* mode specific */

  /* --------------------------------------------------------------------- */
  /* immutable values which are set during vndr VPD probing of the scanner */
  /* members in order found in scsi data...                                */
  int has_adf;
  int has_flatbed;
  int has_transparency;
  int has_duplex;
  int has_endorser_b;
  int has_barcode;
  int has_operator_panel;
  int has_endorser_f;

  int adbits;
  int buffer_bytes;

  /*supported scsi commands*/
  int has_cmd_msen10;
  int has_cmd_msel10;

  int has_cmd_lsen;
  int has_cmd_lsel;
  int has_cmd_change;
  int has_cmd_rbuff;
  int has_cmd_wbuff;
  int has_cmd_cav;
  int has_cmd_comp;
  int has_cmd_gdbs;

  int has_cmd_op;
  int has_cmd_send;
  int has_cmd_read;
  int has_cmd_gwin;
  int has_cmd_swin;
  int has_cmd_sdiag;
  int has_cmd_rdiag;
  int has_cmd_scan;
  
  int has_cmd_msen6;
  int has_cmd_copy;
  int has_cmd_rel;
  int has_cmd_runit;
  int has_cmd_msel6;
  int has_cmd_inq;
  int has_cmd_rs;
  int has_cmd_tur;

  /*FIXME: there are more vendor cmds? */
  int has_cmd_subwindow;
  int has_cmd_endorser;
  int has_cmd_hw_status;
  int has_cmd_hw_status_2;
  int has_cmd_hw_status_3;
  int has_cmd_scanner_ctl;
  int has_cmd_device_restart;

  /*FIXME: do we need the vendor window param list? */

  int brightness_steps;
  int threshold_steps;
  int contrast_steps;

  int num_internal_gamma;
  int num_download_gamma;
  int num_internal_dither;
  int num_download_dither;

  int has_df_recovery;
  int has_paper_protect;
  int has_adv_paper_prot;
  int has_staple_detect;

  int has_rif;
  int has_dtc;
  int has_sdtc;
  int has_outline;
  int has_emphasis;
  int has_autosep;
  int has_mirroring;
  int has_wl_follow;
  int has_subwindow;
  int has_diffusion;
  int has_ipc3;
  int has_rotation;
  int has_hybrid_crop_deskew;
  int has_off_mode;

  int has_comp_MH;
  int has_comp_MR;
  int has_comp_MMR;
  int has_comp_JBIG;
  int has_comp_JPG1;
  int has_comp_JPG2;
  int has_comp_JPG3;
  int has_op_halt;

  /*FIXME: more endorser data? */
  int endorser_type_f;
  int endorser_type_b;

  /*FIXME: barcode data? */

  /* overscan size in pixels comes from scanner in basic res units */
  int os_x_basic;
  int os_y_basic;

  /* --------------------------------------------------------------------- */
  /* immutable values which are gathered by mode_sense command     */

  int has_MS_autocolor;
  int has_MS_prepick;
  int has_MS_sleep;
  int has_MS_duplex;
  int has_MS_rand;
  int has_MS_bg;
  int has_MS_df;
  int has_MS_dropout; /* dropout color specified in mode select data */
  int has_MS_buff;
  int has_MS_auto;
  int has_MS_lamp;
  int has_MS_jobsep;

  /* --------------------------------------------------------------------- */
  /* immutable values which are hard coded because they are not in vpd     */
  /* this section replaces all the old 'switch (s->model)' code            */

  /* the scan size in 1/1200th inches, NOT basic_units or sane units */
  int max_x;
  int max_y;
  struct y_size max_y_by_res[4];
  int min_x;
  int min_y;
  int max_x_fb;
  int max_y_fb;

  int has_back;         /* not all duplex scanners can do adf back side only */
  int color_interlace;  /* different models interlace colors differently     */
  int duplex_interlace; /* different models interlace sides differently      */
  int jpeg_interlace;   /* different models interlace jpeg sides differently */
  int cropping_mode;    /* lower-end scanners dont crop from paper size      */
  int ghs_in_rs;
  int window_gamma;
  int endorser_string_len;
  int has_pixelsize;
  int has_short_pixelsize; /* m3091/2 put weird stuff at end, ignore it */

  int broken_diag_serial;   /* some scanners are just plain borked */
  int need_q_table;         /* some scanners wont work without these */
  int need_diag_preread;    
  int late_lut;    
  int hopper_before_op;     /* some scanners dont like OP when hopper empty */
  int no_wait_after_op;     /* some scanners dont like TUR after OP */

  int has_vuid_mono;    /* mono set window data */
  int has_vuid_3091;    /* 3091/2 set window data */
  int has_vuid_color;   /* color set window data */

  int reverse_by_mode[6]; /* mode specific */
  int ppl_mod_by_mode[6]; /* mode specific scanline length limitation */

  /* --------------------------------------------------------------------- */
  /* immutable values which are set during serial number probing scanner   */
  char serial_name[28];        /* 16 char model, ':', 10 byte serial, null */

  /* --------------------------------------------------------------------- */
  /* struct with pointers to device/vendor/model names, and a type value */
  /* used to inform sane frontend about the device */
  SANE_Device sane;

  /* --------------------------------------------------------------------- */
  /* changeable SANE_Option structs provide our interface to frontend.     */
  /* some options require lists of strings or numbers, we keep them here   */
  /* instead of in global vars so that they can differ for each scanner    */

  /* long array of option structs */
  SANE_Option_Descriptor opt[NUM_OPTIONS];

  /*mode group*/
  SANE_String_Const mode_list[7];
  SANE_String_Const source_list[5];

  SANE_Int res_list[17];
  SANE_Range res_range;

  /*geometry group*/
  SANE_Range tl_x_range;
  SANE_Range tl_y_range;
  SANE_Range br_x_range;
  SANE_Range br_y_range;
  SANE_Range paper_x_range;
  SANE_Range paper_y_range;

  /*enhancement group*/
  SANE_Range brightness_range;
  SANE_Range contrast_range;
  SANE_Range gamma_range;
  SANE_Range threshold_range;

  /*ipc group*/
  SANE_String_Const ht_type_list[4];
  SANE_Range ht_pattern_range;
  SANE_Range emphasis_range;
  SANE_String_Const wl_follow_list[4];
  SANE_String_Const ipc_mode_list[4];
  SANE_Range gamma_curve_range;
  SANE_Range threshold_curve_range;
  SANE_Range variance_range;

  /*advanced group*/
  SANE_String_Const compress_list[3];
  SANE_Range compress_arg_range;
  SANE_String_Const df_action_list[4];
  SANE_String_Const df_diff_list[5];
  SANE_String_Const df_recovery_list[4];
  SANE_String_Const paper_protect_list[4];
  SANE_String_Const adv_paper_prot_list[4];
  SANE_String_Const staple_detect_list[4];
  SANE_String_Const bg_color_list[4];
  SANE_String_Const do_color_list[5];
  SANE_String_Const lamp_color_list[5];
  SANE_String_Const buff_mode_list[4];
  SANE_String_Const prepick_list[4];
  SANE_String_Const overscan_list[4];
  SANE_Range sleep_time_range;
  SANE_Range off_time_range;
  SANE_Range duplex_offset_range;
  SANE_Range green_offset_range;
  SANE_Range blue_offset_range;
  SANE_Range swdespeck_range;
  SANE_Range swskip_range;

  /*endorser group*/
  SANE_Range endorser_bits_range;
  SANE_Range endorser_val_range;
  SANE_Range endorser_step_range;
  SANE_Range endorser_y_range;
  SANE_String_Const endorser_font_list[6];
  SANE_String_Const endorser_dir_list[3];
  SANE_String_Const endorser_side_list[3];

  /* --------------------------------------------------------------------- */
  /* changeable vars to hold user input. modified by SANE_Options above    */

  /*mode group*/
  int u_mode;         /*color,lineart,etc*/
  int source;         /*fb,adf front,adf duplex,etc*/
  int resolution_x;   /* X resolution in dpi                       */
  int resolution_y;   /* Y resolution in dpi                       */

  /*geometry group*/
  /* The desired size of the scan, all in 1/1200 inch */
  int tl_x;
  int tl_y;
  int br_x;
  int br_y;
  int page_width;
  int page_height;

  /*enhancement group*/
  int brightness;
  int contrast;
  double gamma;
  int threshold;

  /* ipc */
  int rif;
  int ht_type;
  int ht_pattern;
  int outline;
  int emphasis;
  int separation;
  int mirroring;
  int wl_follow;
  int ipc_mode;

  /* ipc_mode=DTC */
  int bp_filter;
  int smoothing;
  int gamma_curve;
  int threshold_curve;
  int threshold_white;
  int noise_removal;
  int matrix_5;
  int matrix_4;
  int matrix_3;
  int matrix_2;

  /* ipc_mode = SDTC */
  int variance;

  /*advanced group*/
  int awd;
  int ald;
  int compress;
  int compress_arg;
  int df_action;
  int df_skew;
  int df_thickness;
  int df_length;
  int df_diff;
  int df_recovery;
  int paper_protect;
  int adv_paper_prot;
  int staple_detect;
  int bg_color;
  int dropout_color;
  int buff_mode;
  int prepick;
  int overscan;
  int lamp_color;
  int sleep_time;
  int off_time;
  int duplex_offset;
  int green_offset;
  int blue_offset;
  int low_mem;
  int hwdeskewcrop;
  int swdeskew;
  int swdespeck;
  int swcrop;
  double swskip;
  int halt_on_cancel;

  /*endorser group*/
  int u_endorser;
  int u_endorser_bits;
  int u_endorser_val;
  int u_endorser_step;
  int u_endorser_y;
  int u_endorser_font;
  int u_endorser_dir;
  int u_endorser_side;
  char u_endorser_string[81]; /*max length, plus null byte*/

  /* --------------------------------------------------------------------- */
  /* values which are derived from setting the options above */
  /* the user never directly modifies these */

  int s_mode; /*color,lineart,etc: sent to scanner*/

  /* this is defined in sane spec as a struct containing:
	SANE_Frame format;
	SANE_Bool last_frame;
	SANE_Int lines;
	SANE_Int depth; ( binary=1, gray=8, color=8 (!24) )
	SANE_Int pixels_per_line;
	SANE_Int bytes_per_line;
  */
  SANE_Parameters u_params;
  SANE_Parameters s_params;

  /* also keep a backup copy, in case the software enhancement code overwrites*/
  /*
  SANE_Parameters u_params_bk;
  SANE_Parameters s_params_bk;
  */

  /* --------------------------------------------------------------------- */
  /* values which are set by scanning functions to keep track of pages, etc */
  int started;
  int reading;
  int cancelled;
  int side;

  /* total to read/write */
  int bytes_tot[2];

  /* how far we have read */
  int bytes_rx[2];
  int lines_rx[2]; /*only used by 3091*/
  int eof_rx[2];
  int ili_rx[2];
  int eom_rx;

  /* how far we have written */
  int bytes_tx[2];
  int eof_tx[2];

  /*size of buffers (can be smaller than above*/
  int buff_tot[2];
  int buff_rx[2];
  int buff_tx[2];

  unsigned char * buffers[2];

  /* --------------------------------------------------------------------- */
  /*hardware feature bookkeeping*/
  int req_driv_crop;
  int req_driv_lut;

  /* --------------------------------------------------------------------- */
  /* values used by the software enhancment code (deskew, crop, etc)       */
  SANE_Status deskew_stat;
  int deskew_vals[2];
  double deskew_slope;

  int crop_vals[4];

  /* --------------------------------------------------------------------- */
  /* values used by the compression functions, esp. jpeg with duplex       */
  int jpeg_stage;
  int jpeg_ff_offset;
  int jpeg_front_rst;
  int jpeg_back_rst;
  int jpeg_x_byte;

  /* --------------------------------------------------------------------- */
  /* values which used by the command and data sending functions (scsi/usb)*/
  int fd;                      /* The scanner device file descriptor.     */
  size_t rs_info;
  int rs_eom;
  int rs_ili;

  /* --------------------------------------------------------------------- */
  /* values which are used by the get hardware status command              */

  int hw_top;
  int hw_A3;
  int hw_B4;
  int hw_A4;
  int hw_B5;

  int hw_hopper;
  int hw_omr;
  int hw_adf_open;

  int hw_sleep;
  int hw_send_sw;
  int hw_manual_feed;
  int hw_scan_sw;

  int hw_function;

  int hw_ink_empty;
  int hw_double_feed;

  int hw_error_code;
  int hw_skew_angle;
  int hw_ink_remain;

  int hw_duplex_sw;
  int hw_density_sw;

  /* values which are used to track the frontend's access to sensors  */
  char hw_read[NUM_OPTIONS-OPT_TOP];
};

#define CONNECTION_SCSI   0 /* SCSI interface */
#define CONNECTION_USB    1 /* USB interface */

#define SIDE_FRONT 0
#define SIDE_BACK 1

#define SOURCE_FLATBED 0
#define SOURCE_ADF_FRONT 1
#define SOURCE_ADF_BACK 2
#define SOURCE_ADF_DUPLEX 3

#define COMP_NONE WD_cmp_NONE
#define COMP_JPEG WD_cmp_JPG1

#define JPEG_STAGE_NONE 0
#define JPEG_STAGE_SOI 1
#define JPEG_STAGE_HEAD 2
#define JPEG_STAGE_SOF 3
#define JPEG_STAGE_SOS 4
#define JPEG_STAGE_FRONT 5
#define JPEG_STAGE_BACK 6
#define JPEG_STAGE_EOI 7

#define JFIF_APP0_LENGTH 18

/* these are same as scsi data to make code easier */
#define MODE_LINEART WD_comp_LA
#define MODE_HALFTONE WD_comp_HT
#define MODE_GRAYSCALE WD_comp_GS
#define MODE_COLOR_LINEART WD_comp_CL
#define MODE_COLOR_HALFTONE WD_comp_CH
#define MODE_COLOR WD_comp_CG

/* these are same as dropout scsi data to make code easier */
#define COLOR_DEFAULT 0
#define COLOR_GREEN 8
#define COLOR_RED 9
#define COLOR_BLUE 11

#define COLOR_WHITE 1
#define COLOR_BLACK 2

#define COLOR_INTERLACE_UNK 0
#define COLOR_INTERLACE_RGB 1
#define COLOR_INTERLACE_BGR 2
#define COLOR_INTERLACE_RRGGBB 3
#define COLOR_INTERLACE_3091 4

#define DUPLEX_INTERLACE_ALT 0 
#define DUPLEX_INTERLACE_NONE 1 
#define DUPLEX_INTERLACE_3091 2 

#define JPEG_INTERLACE_ALT 0 
#define JPEG_INTERLACE_NONE 1 

#define CROP_RELATIVE 0 
#define CROP_ABSOLUTE 1 

#define DF_DEFAULT 0
#define DF_CONTINUE 1
#define DF_STOP 2

#define FONT_H  0
#define FONT_HB 1
#define FONT_HN 2
#define FONT_V  3
#define FONT_VB 4

#define DIR_TTB 0
#define DIR_BTT 1

/* endorser type, same as scsi inquiry data */
#define ET_OLD	0
#define ET_30	1
#define ET_40	2

/* ------------------------------------------------------------------------- */

#define MM_PER_UNIT_UNFIX SANE_UNFIX(SANE_FIX(MM_PER_INCH / 1200.0))
#define MM_PER_UNIT_FIX SANE_FIX(SANE_UNFIX(SANE_FIX(MM_PER_INCH / 1200.0)))

#define SCANNER_UNIT_TO_FIXED_MM(number) SANE_FIX((number) * MM_PER_UNIT_UNFIX)
#define FIXED_MM_TO_SCANNER_UNIT(number) SANE_UNFIX(number) / MM_PER_UNIT_UNFIX

#define FUJITSU_CONFIG_FILE "fujitsu.conf"

#ifndef PATH_MAX
#  define PATH_MAX 1024
#endif

/* ------------------------------------------------------------------------- */

SANE_Status sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize);

SANE_Status sane_get_devices (const SANE_Device *** device_list,
                              SANE_Bool local_only);

SANE_Status sane_open (SANE_String_Const name, SANE_Handle * handle);

SANE_Status sane_set_io_mode (SANE_Handle h, SANE_Bool non_blocking);

SANE_Status sane_get_select_fd (SANE_Handle h, SANE_Int * fdp);

const SANE_Option_Descriptor * sane_get_option_descriptor (SANE_Handle handle,
                                                          SANE_Int option);

SANE_Status sane_control_option (SANE_Handle handle, SANE_Int option,
                                 SANE_Action action, void *val,
                                 SANE_Int * info);

SANE_Status sane_start (SANE_Handle handle);

SANE_Status sane_get_parameters (SANE_Handle handle,
                                 SANE_Parameters * params);

SANE_Status sane_read (SANE_Handle handle, SANE_Byte * buf, SANE_Int max_len,
                       SANE_Int * len);

void sane_cancel (SANE_Handle h);

void sane_close (SANE_Handle h);

void sane_exit (void);

/* ------------------------------------------------------------------------- */

static SANE_Status attach_one_scsi (const char *name);
static SANE_Status attach_one_usb (const char *name);
static SANE_Status attach_one (const char *devicename, int connType);

static SANE_Status connect_fd (struct fujitsu *s);
static SANE_Status disconnect_fd (struct fujitsu *s);

static SANE_Status sense_handler (int scsi_fd, u_char * result, void *arg);

static SANE_Status init_inquire (struct fujitsu *s);
static SANE_Status init_vpd (struct fujitsu *s);
static SANE_Status init_ms (struct fujitsu *s);
static SANE_Status init_model (struct fujitsu *s);
static SANE_Status init_user (struct fujitsu *s);
static SANE_Status init_options (struct fujitsu *scanner);
static SANE_Status init_interlace (struct fujitsu *scanner);
static SANE_Status init_serial (struct fujitsu *scanner);

static SANE_Status
do_cmd(struct fujitsu *s, int runRS, int shortTime,
 unsigned char * cmdBuff, size_t cmdLen,
 unsigned char * outBuff, size_t outLen,
 unsigned char * inBuff, size_t * inLen
);

static SANE_Status
do_scsi_cmd(struct fujitsu *s, int runRS, int shortTime,
 unsigned char * cmdBuff, size_t cmdLen,
 unsigned char * outBuff, size_t outLen,
 unsigned char * inBuff, size_t * inLen
);

static SANE_Status
do_usb_cmd(struct fujitsu *s, int runRS, int shortTime,
 unsigned char * cmdBuff, size_t cmdLen,
 unsigned char * outBuff, size_t outLen,
 unsigned char * inBuff, size_t * inLen
);

static SANE_Status wait_scanner (struct fujitsu *s);

static SANE_Status object_position (struct fujitsu *s, int action);

static SANE_Status scanner_control (struct fujitsu *s, int function);
static SANE_Status scanner_control_ric (struct fujitsu *s, int bytes, int side);

static SANE_Status mode_select_df(struct fujitsu *s);

static SANE_Status mode_select_dropout(struct fujitsu *s);

static SANE_Status mode_select_bg(struct fujitsu *s);

static SANE_Status mode_select_buff (struct fujitsu *s);

static SANE_Status mode_select_prepick (struct fujitsu *s);

static SANE_Status mode_select_auto (struct fujitsu *s);

static SANE_Status set_sleep_mode(struct fujitsu *s);
static SANE_Status set_off_mode(struct fujitsu *s);

static int must_downsample (struct fujitsu *s);
static int must_fully_buffer (struct fujitsu *s);
static int get_page_width (struct fujitsu *s);
static int get_page_height (struct fujitsu *s);
static int set_max_y (struct fujitsu *s);

static SANE_Status send_lut (struct fujitsu *s);
static SANE_Status send_endorser (struct fujitsu *s);
static SANE_Status endorser (struct fujitsu *s);
static SANE_Status set_window (struct fujitsu *s);
static SANE_Status get_pixelsize(struct fujitsu *s, int actual);

static SANE_Status update_params (struct fujitsu *s);
static SANE_Status update_u_params (struct fujitsu *s);

static SANE_Status start_scan (struct fujitsu *s);

static SANE_Status check_for_cancel(struct fujitsu *s);

static SANE_Status read_from_JPEGduplex(struct fujitsu *s);
static SANE_Status read_from_3091duplex(struct fujitsu *s);
static SANE_Status read_from_scanner(struct fujitsu *s, int side);

static SANE_Status copy_3091(struct fujitsu *s, unsigned char * buf, int len, int side);
static SANE_Status copy_JPEG(struct fujitsu *s, unsigned char * buf, int len, int side);
static SANE_Status copy_buffer(struct fujitsu *s, unsigned char * buf, int len, int side);

static SANE_Status read_from_buffer(struct fujitsu *s, SANE_Byte * buf, SANE_Int max_len, SANE_Int * len, int side);
static SANE_Status downsample_from_buffer(struct fujitsu *s, SANE_Byte * buf, SANE_Int max_len, SANE_Int * len, int side);

static SANE_Status setup_buffers (struct fujitsu *s);

static SANE_Status get_hardware_status (struct fujitsu *s, SANE_Int option);

static SANE_Status buffer_deskew(struct fujitsu *s, int side);
static SANE_Status buffer_crop(struct fujitsu *s, int side);
static SANE_Status buffer_despeck(struct fujitsu *s, int side);
static int buffer_isblank(struct fujitsu *s, int side);

static void hexdump (int level, char *comment, unsigned char *p, int l);

static size_t maxStringSize (const SANE_String_Const strings[]);

#endif /* FUJITSU_H */
