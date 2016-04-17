#ifndef CANON_DR_H
#define CANON_DR_H

/* 
 * Part of SANE - Scanner Access Now Easy.
 * Please see opening comments in canon_dr.c
 */

/* ------------------------------------------------------------------------- 
 * This option list has to contain all options for all scanners supported by
 * this driver. If a certain scanner cannot handle a certain option, there's
 * still the possibility to say so, later.
 */
enum scanner_Option
{
  OPT_NUM_OPTS = 0,

  OPT_STANDARD_GROUP,
  OPT_SOURCE, /*fb/adf/front/back/duplex*/
  OPT_MODE,   /*mono/gray/color*/
  OPT_RES,    /*a range or a list*/

  OPT_GEOMETRY_GROUP,
  OPT_TL_X,
  OPT_TL_Y,
  OPT_BR_X,
  OPT_BR_Y,
  OPT_PAGE_WIDTH,
  OPT_PAGE_HEIGHT,

  OPT_ENHANCEMENT_GROUP,
  OPT_BRIGHTNESS,
  OPT_CONTRAST,
  OPT_THRESHOLD,
  OPT_RIF,

  OPT_ADVANCED_GROUP,
  OPT_COMPRESS,
  OPT_COMPRESS_ARG,
  OPT_DF_THICKNESS,
  OPT_DF_LENGTH,
  OPT_ROLLERDESKEW,
  OPT_SWDESKEW,
  OPT_SWDESPECK,
  OPT_SWCROP,
  OPT_STAPLEDETECT,
  OPT_DROPOUT_COLOR_F,
  OPT_DROPOUT_COLOR_B,
  OPT_BUFFERMODE,
  OPT_SIDE,

  /*sensor group*/
  OPT_SENSOR_GROUP,
  OPT_START,
  OPT_STOP,
  OPT_BUTT3,
  OPT_NEWFILE,
  OPT_COUNTONLY,
  OPT_BYPASSMODE,
  OPT_COUNTER,
  OPT_ADF_LOADED,
  OPT_CARD_LOADED,

  /* must come last: */
  NUM_OPTIONS
};

struct img_params
{
  int mode;           /*color,lineart,etc*/
  int source;         /*fb,adf front,adf duplex,etc*/

  int dpi_x;          /*these are in dpi */
  int dpi_y;

  int tl_x;           /*these are in 1200dpi units */
  int tl_y;
  int br_x;
  int br_y;
  int page_x;
  int page_y;

  int width;          /*these are in pixels*/
  int height;

  SANE_Frame format;  /*SANE_FRAME_**/
  int bpp;            /* 1,8,24 */
  int Bpl;            /* in bytes */

  int valid_width;    /*some machines have black padding*/
  int valid_Bpl;      

  /* done yet? */
  int eof[2];

  /* how far we have read/written */
  int bytes_sent[2];

  /* total to read/write */
  int bytes_tot[2];

  /* dumb scanners send extra data */
  int skip_lines[2];

};

struct scanner
{
  /* --------------------------------------------------------------------- */
  /* immutable values which are set during init of scanner.                */
  struct scanner *next;
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

  /* --------------------------------------------------------------------- */
  /* immutable values which are set during std VPD probing of the scanner. */
  /* members in order found in scsi data...                                */
  int basic_x_res;
  int basic_y_res;
  int step_x_res;
  int step_y_res;
  int max_x_res;
  int max_y_res;
  int min_x_res;
  int min_y_res;

  int std_res_x[16];
  int std_res_y[16];

  /* max scan size in pixels converted to 1200dpi units */
  int max_x;
  int max_y;

  /*FIXME: 4 more unknown values here*/
  int can_grayscale;
  int can_halftone;
  int can_monochrome;
  int can_overflow;

  /* --------------------------------------------------------------------- */
  /* immutable values which are hard coded because they are not in vpd     */

  int brightness_steps;
  int threshold_steps;
  int contrast_steps;
  int ppl_mod;       /* modulus of scanline width */

  /* the scan size in 1/1200th inches, NOT basic_units or sane units */
  int min_x;
  int min_y;
  int valid_x;
  int max_x_fb;
  int max_y_fb;

  int can_color;     /* actually might be in vpd, but which bit? */
  int need_ccal;     /* scanner needs software to help with afe calibration */
  int need_fcal;     /* scanner needs software to help with fine calibration */
  int need_fcal_buffer; /* software to apply calibration stored in scanner*/
  int ccal_version;  /* 0 in most scanners, 3 in newer ones */

  int has_counter;
  int has_rif;
  int has_adf;
  int has_flatbed;
  int has_duplex;
  int has_back;         /* not all duplex scanners can do adf back side only */
  int has_card;         /* P215 has a card reader instead of fb */
  int has_comp_JPEG;
  int has_buffer;
  int has_df;
  int has_df_ultra;
  int has_btc;
  int has_ssm;           /* older scanners use this set scan mode command */         
  int has_ssm2;          /* newer scanners user this similar command */
  int has_ssm_pay_head_len; /* newer scanners put the length twice in ssm */
  int can_read_sensors;
  int can_read_panel;
  int can_write_panel;
  int rgb_format;       /* meaning unknown */
  int padding;          /* meaning unknown */

  int always_op;        /* send object pos between pages */
  int invert_tly;       /* weird bug in some smaller scanners */
  int unknown_byte2;    /* weird byte, required, meaning unknown */
  int padded_read;      /* some machines need extra 12 bytes on reads */
  int extra_status;     /* some machines need extra status read after cmd */
  int fixed_width;      /* some machines always scan full width */
  int even_Bpl;         /* some machines require even bytes per line */

  int gray_interlace[2];  /* different models interlace heads differently    */
  int color_interlace[2]; /* different models interlace colors differently   */
  int color_inter_by_res[16]; /* and some even change by resolution */
  int duplex_interlace; /* different models interlace sides differently      */
  int jpeg_interlace;   /* different models interlace jpeg sides differently */
  int duplex_offset;    /* number of lines of padding added to front (1/1200)*/
  int duplex_offset_side; /* padding added to front or back? */

  int sw_lut;           /* no hardware brightness/contrast support */
  int bg_color;         /* needed to fill in after rotation */

  int reverse_by_mode[6]; /* mode specific */

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
  SANE_String_Const source_list[8];

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
  SANE_Range threshold_range;

  /*advanced group*/
  SANE_String_Const compress_list[3];
  SANE_Range compress_arg_range;
  SANE_Range swdespeck_range;
  SANE_String_Const do_color_list[8];

  /*sensor group*/
  SANE_Range counter_range;

  /* --------------------------------------------------------------------- */
  /* changeable vars to hold user input. modified by SANE_Options above    */

  /* the user's requested image params */
  /* exposed in standard and geometry option groups */
  struct img_params u;

  /*enhancement group*/
  int brightness;
  int contrast;
  int threshold;
  int rif;

  /*advanced group*/
  int compress;
  int compress_arg;
  int df_length;
  int df_thickness;
  int dropout_color_f;
  int dropout_color_b;
  int buffermode;
  int rollerdeskew;
  int swdeskew;
  int swdespeck;
  int swcrop;
  int stapledetect;

  /* --------------------------------------------------------------------- */
  /* values which are derived from setting the options above */
  /* the user never directly modifies these */

  /* the scanner image params (what we ask from scanner) */
  struct img_params s;

  /* the intermediate image params (like user, but possible higher depth) */
  struct img_params i;

  /* the brightness/contrast LUT for dumb scanners */
  unsigned char lut[256];

  /* --------------------------------------------------------------------- */
  /* values which are set by calibration functions                         */
  int c_res;
  int c_mode;

  int c_offset[2];
  int c_gain[2];
  int c_exposure[2][3];

  int f_res;
  int f_mode;

  unsigned char * f_offset[2];
  unsigned char * f_gain[2];

  /* --------------------------------------------------------------------- */
  /* values which are set by scanning functions to keep track of pages, etc */
  int started;
  int reading;
  int cancelled;
  int side;
  int prev_page;
  int jpeg_stage;
  int jpeg_ff_offset;

  unsigned char * buffers[2];

  /* --------------------------------------------------------------------- */
  /* values used by the command and data sending functions (scsi/usb)      */
  int fd;                      /* The scanner device file descriptor.      */
  size_t rs_info;

  /* --------------------------------------------------------------------- */
  /* values used to hold hardware or control panel status                  */

  int panel_start;
  int panel_stop;
  int panel_butt3;
  int panel_new_file;
  int panel_count_only;
  int panel_bypass_mode;
  int panel_enable_led;
  int panel_counter;
  int sensor_adf_loaded;
  int sensor_card_loaded;

  /* values which are used to track the frontend's access to sensors  */
  char panel_read[OPT_COUNTER - OPT_START + 1];
  char sensors_read[OPT_CARD_LOADED - OPT_ADF_LOADED + 1];
};

#define CONNECTION_SCSI   0 /* SCSI interface */
#define CONNECTION_USB    1 /* USB interface */

#define SIDE_FRONT 0
#define SIDE_BACK 1

#define CHAN_RED 0
#define CHAN_GREEN 1
#define CHAN_BLUE 2

#define SOURCE_FLATBED 0
#define SOURCE_ADF_FRONT 1
#define SOURCE_ADF_BACK 2
#define SOURCE_ADF_DUPLEX 3
#define SOURCE_CARD_FRONT 4
#define SOURCE_CARD_BACK 5
#define SOURCE_CARD_DUPLEX 6

static const int dpi_list[] = {
60,75,100,120,150,160,180,200,
240,300,320,400,480,600,800,1200
};

#define DPI_60 0
#define DPI_75 1
#define DPI_100 2
#define DPI_120 3
#define DPI_150 4
#define DPI_160 5
#define DPI_180 6
#define DPI_200 7
#define DPI_240 8
#define DPI_300 9
#define DPI_320 10
#define DPI_400 11
#define DPI_480 12
#define DPI_600 13
#define DPI_800 14
#define DPI_1200 15

#define COMP_NONE WD_cmp_NONE
#define COMP_JPEG WD_cmp_JPEG

#define JPEG_STAGE_NONE 0
#define JPEG_STAGE_SOF 1

/* these are same as scsi data to make code easier */
#define MODE_LINEART WD_comp_LA
#define MODE_HALFTONE WD_comp_HT
#define MODE_GRAYSCALE WD_comp_GS
#define MODE_COLOR WD_comp_CG

enum {
 COLOR_NONE = 0,
 COLOR_RED,
 COLOR_GREEN,
 COLOR_BLUE,
 COLOR_EN_RED,
 COLOR_EN_GREEN,
 COLOR_EN_BLUE
};

/* these are same as scsi data to make code easier */
#define COLOR_WHITE 1
#define COLOR_BLACK 2

#define GRAY_INTERLACE_NONE 0
#define GRAY_INTERLACE_2510 1
#define GRAY_INTERLACE_gG 2

#define COLOR_INTERLACE_UNK 0
#define COLOR_INTERLACE_RGB 1
#define COLOR_INTERLACE_BGR 2
#define COLOR_INTERLACE_BRG 3
#define COLOR_INTERLACE_GBR 4
#define COLOR_INTERLACE_RRGGBB 5
#define COLOR_INTERLACE_rRgGbB 6
#define COLOR_INTERLACE_2510 7

#define DUPLEX_INTERLACE_NONE 0
#define DUPLEX_INTERLACE_FFBB 1
#define DUPLEX_INTERLACE_FBFB 2
#define DUPLEX_INTERLACE_2510 3

#define JPEG_INTERLACE_ALT 0 
#define JPEG_INTERLACE_NONE 1 

#define CROP_RELATIVE 0
#define CROP_ABSOLUTE 1

/* ------------------------------------------------------------------------- */

#define MM_PER_UNIT_UNFIX SANE_UNFIX(SANE_FIX(MM_PER_INCH / 1200.0))
#define MM_PER_UNIT_FIX SANE_FIX(SANE_UNFIX(SANE_FIX(MM_PER_INCH / 1200.0)))

#define SCANNER_UNIT_TO_FIXED_MM(number) SANE_FIX((number) * MM_PER_UNIT_UNFIX)
#define FIXED_MM_TO_SCANNER_UNIT(number) SANE_UNFIX(number) / MM_PER_UNIT_UNFIX

#define CANON_DR_CONFIG_FILE "canon_dr.conf"

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

static SANE_Status connect_fd (struct scanner *s);
static SANE_Status disconnect_fd (struct scanner *s);

static SANE_Status sense_handler (int scsi_fd, u_char * result, void *arg);

static SANE_Status init_inquire (struct scanner *s);
static SANE_Status init_vpd (struct scanner *s);
static SANE_Status init_model (struct scanner *s);
static SANE_Status init_panel (struct scanner *s);
static SANE_Status init_user (struct scanner *s);
static SANE_Status init_options (struct scanner *s);

static SANE_Status
do_cmd(struct scanner *s, int runRS, int shortTime,
 unsigned char * cmdBuff, size_t cmdLen,
 unsigned char * outBuff, size_t outLen,
 unsigned char * inBuff, size_t * inLen
);

static SANE_Status
do_scsi_cmd(struct scanner *s, int runRS, int shortTime,
 unsigned char * cmdBuff, size_t cmdLen,
 unsigned char * outBuff, size_t outLen,
 unsigned char * inBuff, size_t * inLen
);

static SANE_Status
do_usb_cmd(struct scanner *s, int runRS, int shortTime,
 unsigned char * cmdBuff, size_t cmdLen,
 unsigned char * outBuff, size_t outLen,
 unsigned char * inBuff, size_t * inLen
);

static SANE_Status
do_usb_status(struct scanner *s, int runRS, int shortTime, size_t * extraLength);

static SANE_Status do_usb_clear(struct scanner *s, int clear, int runRS);

static SANE_Status wait_scanner (struct scanner *s);

static SANE_Status object_position (struct scanner *s, int i_load);

static SANE_Status ssm_buffer (struct scanner *s);
static SANE_Status ssm_do (struct scanner *s);
static SANE_Status ssm_df (struct scanner *s);

static int get_color_inter(struct scanner *s, int side, int res);

static int get_page_width (struct scanner *s);
static int get_page_height (struct scanner *s);

static SANE_Status set_window (struct scanner *s);
static SANE_Status update_params (struct scanner *s, int calib);
static SANE_Status update_i_params (struct scanner *s);
static SANE_Status clean_params (struct scanner *s);

static SANE_Status read_sensors(struct scanner *s, SANE_Int option);
static SANE_Status read_panel(struct scanner *s, SANE_Int option);
static SANE_Status send_panel(struct scanner *s);

static SANE_Status start_scan (struct scanner *s, int type);

static SANE_Status check_for_cancel(struct scanner *s);

static SANE_Status read_from_scanner(struct scanner *s, int side, int exact);
static SANE_Status read_from_scanner_duplex(struct scanner *s, int exact);

static SANE_Status copy_simplex(struct scanner *s, unsigned char * buf, int len, int side);
static SANE_Status copy_duplex(struct scanner *s, unsigned char * buf, int len);
static SANE_Status copy_line(struct scanner *s, unsigned char * buf, int side);

static SANE_Status buffer_despeck(struct scanner *s, int side);
static SANE_Status buffer_deskew(struct scanner *s, int side);
static SANE_Status buffer_crop(struct scanner *s, int side);

int * getTransitionsY (struct scanner *s, int side, int top);
int * getTransitionsX (struct scanner *s, int side, int top);

SANE_Status getEdgeIterate (int width, int height, int resolution,
  int * buff, double * finSlope, int * finXInter, int * finYInter);

SANE_Status getEdgeSlope (int width, int height, int * top, int * bot,
  double slope, int * finXInter, int * finYInter);

SANE_Status rotateOnCenter (struct scanner *s, int side,
  int centerX, int centerY, double slope);

static SANE_Status getLine (int height, int width, int * buff,
 int slopes, double minSlope, double maxSlope,
 int offsets, int minOffset, int maxOffset,
 double * finSlope, int * finOffset, int * finDensity);

static SANE_Status load_lut (unsigned char * lut, int in_bits, int out_bits,
  int out_min, int out_max, int slope, int offset);

static SANE_Status read_from_buffer(struct scanner *s, SANE_Byte * buf, SANE_Int max_len, SANE_Int * len, int side);

static SANE_Status image_buffers (struct scanner *s, int setup);
static SANE_Status offset_buffers (struct scanner *s, int setup);
static SANE_Status gain_buffers (struct scanner *s, int setup);

static SANE_Status calibrate_AFE(struct scanner *s);
static SANE_Status calibrate_fine(struct scanner *s);
static SANE_Status calibrate_fine_buffer(struct scanner *s);

static SANE_Status write_AFE (struct scanner *s);
static SANE_Status calibration_scan (struct scanner *s, int);

static void hexdump (int level, char *comment, unsigned char *p, int l);
static void default_globals (void);

static size_t maxStringSize (const SANE_String_Const strings[]);

#endif /* CANON_DR_H */
