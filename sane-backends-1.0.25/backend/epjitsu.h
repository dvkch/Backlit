#ifndef EPJITSU_H
#define EPJITSU_H

/* 
 * Part of SANE - Scanner Access Now Easy.
 * Please see opening comment in epjitsu.c
 */

/* ------------------------------------------------------------------------- 
 * This option list has to contain all options for all scanners supported by
 * this driver. If a certain scanner cannot handle a certain option, there's
 * still the possibility to say so, later.
 */
enum scanner_Option
{
  OPT_NUM_OPTS = 0,

  OPT_MODE_GROUP,
  OPT_SOURCE,   /*adffront/adfback/adfduplex/fb*/
  OPT_MODE,     /*mono/gray/color*/
  OPT_RES,

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
  OPT_GAMMA,
  OPT_THRESHOLD,
  OPT_THRESHOLD_CURVE,

  OPT_SENSOR_GROUP,
  OPT_SCAN_SW,
  OPT_HOPPER,
  OPT_TOP,
  OPT_ADF_OPEN,
  OPT_SLEEP,

  /* must come last: */
  NUM_OPTIONS
};

#define FIRMWARE_LENGTH 0x10000
#define MAX_IMG_PASS 0x10000
#define MAX_IMG_BLOCK 0x80000

struct image {
  int width_pix;
  int width_bytes;
  int height;
  int pages;
  int x_res;
  int y_res;
  int x_start_offset;
  int x_offset_bytes;
  int y_skip_offset;
  unsigned char * buffer;
};

struct transfer {
  int plane_width;   /* in RGB pixels */
  int plane_stride;  /* in bytes */
  int line_stride;   /* in bytes */

  int total_bytes;
  int rx_bytes;
  int done;
  int x_res;
  int y_res;

  unsigned char * raw_data;
  struct image * image;
};

struct page {
  int bytes_total;
  int bytes_scanned;
  int bytes_read;
  int lines_rx;   /* received from scanner */
  int lines_pass; /* passed thru from scanner to user (might be smaller than tx for 225dpi) */
  int lines_tx;   /* transmitted to user */
  int done;
  struct image *image;
};

struct scanner
{
  /* --------------------------------------------------------------------- */
  /* immutable values which are set during init of scanner.                */
  struct scanner *next;

  int missing;

  int model;
  int usb_power;

  int has_fb;
  int has_adf;
  int has_adf_duplex;

  int min_res;
  int max_res;

  float white_factor[3];
  int adf_height_padding;

  /* the scan size in 1/1200th inches, NOT basic_units or sane units */
  int max_x;
  int max_y;
  int min_x;
  int min_y;

  /* --------------------------------------------------------------------- */
  /* immutable values which are set during inquiry probing of the scanner. */
  SANE_Device sane; /*contains: name, vendor, model, type*/

  /* --------------------------------------------------------------------- */
  /* changeable SANE_Option structs provide our interface to frontend.     */

  /* long array of option structs */
  SANE_Option_Descriptor opt[NUM_OPTIONS];

  /* --------------------------------------------------------------------- */
  /* some options require lists of strings or numbers, we keep them here   */
  /* instead of in global vars so that they can differ for each scanner    */

  /*mode group, room for lineart, gray, color, null */
  SANE_String_Const source_list[5];
  SANE_String_Const mode_list[4];
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
  SANE_Range threshold_curve_range;

  /* --------------------------------------------------------------------- */
  /* changeable vars to hold user input. modified by SANE_Options above    */

  /*mode group*/
  int source;         /* adf or fb */
  int mode;           /* color,lineart,etc */
  int resolution;     /* dpi */

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
  int gamma;
  int threshold;
  int threshold_curve;

  int height;         /* may run out on adf */

  /* --------------------------------------------------------------------- */
  /* values which are set by user parameter changes, scanner specific      */
  unsigned char * setWindowCoarseCal;   /* sent before coarse cal */
  size_t setWindowCoarseCalLen;

  unsigned char * setWindowFineCal;   /* sent before fine cal */
  size_t setWindowFineCalLen;

  unsigned char * setWindowSendCal;   /* sent before send cal */
  size_t setWindowSendCalLen;

  unsigned char * sendCal1Header; /* part of 1b c3 command */
  size_t sendCal1HeaderLen;

  unsigned char * sendCal2Header; /* part of 1b c4 command */
  size_t sendCal2HeaderLen;

  unsigned char * setWindowScan;  /* sent before scan */
  size_t setWindowScanLen;
  
  /* --------------------------------------------------------------------- */
  /* values which are set by scanning functions to keep track of pages, etc */
  int started;
  int side;

  /* holds temp buffers for getting 16 lines of cal data */
  struct transfer cal_image;
  struct image coarsecal;
  struct image darkcal;
  struct image lightcal;

  /* holds temp buffer for building calibration data */
  struct transfer cal_data;
  struct image sendcal;

  /* scanner transmits more data per line than requested */
  /* due to padding and/or duplex interlacing */
  /* the scan struct holds these larger numbers, but image buffer is unused */
  struct {
      int done;
      int x_res;
      int y_res;
      int height;
      int rx_bytes;
      int width_bytes;
      int total_bytes;
  } fullscan;

  /* The page structs contain data about the progress as the application reads */
  /* data from the front/back image buffers via the sane_read() function */
  struct page pages[2];

  /* scanner transmits data in blocks, up to 512k */
  /* but always ends on a scanline. */
  /* the block struct holds the most recent buffer */
  struct transfer block_xfr;
  struct image    block_img;

  /* temporary buffers used by dynamic threshold code */
  struct image  dt;
  unsigned char dt_lut[256];

  /* final-sized front image, always used */
  struct image front;

  /* final-sized back image, only used during duplex/backside */
  struct image back;

  /* --------------------------------------------------------------------- */
  /* values used by the command and data sending function                  */
  int fd;                       /* The scanner device file descriptor.     */

  /* --------------------------------------------------------------------- */
  /* values which are used by the get hardware status command              */
  time_t last_ghs;

  int hw_scan_sw;
  int hw_hopper;
  int hw_top;
  int hw_adf_open;
  int hw_sleep;
};

#define MODEL_NONE 0
#define MODEL_S300 1
#define MODEL_FI60F 2
#define MODEL_S1100 3
#define MODEL_S1300i 4
#define MODEL_FI65F 5

#define USB_COMMAND_TIME   10000
#define USB_DATA_TIME      10000

#define SIDE_FRONT 0
#define SIDE_BACK 1

#define SOURCE_FLATBED 0
#define SOURCE_ADF_FRONT 1
#define SOURCE_ADF_BACK 2
#define SOURCE_ADF_DUPLEX 3

#define MODE_COLOR 0
#define MODE_GRAYSCALE 1
#define MODE_LINEART 2

#define WINDOW_COARSECAL 0
#define WINDOW_FINECAL 1
#define WINDOW_SENDCAL 2
#define WINDOW_SCAN 3

#define EPJITSU_PAPER_INGEST 1
#define EPJITSU_PAPER_EJECT 0

/* ------------------------------------------------------------------------- */

#define MM_PER_UNIT_UNFIX SANE_UNFIX(SANE_FIX(MM_PER_INCH / 1200.0))
#define MM_PER_UNIT_FIX SANE_FIX(SANE_UNFIX(SANE_FIX(MM_PER_INCH / 1200.0)))

#define SCANNER_UNIT_TO_FIXED_MM(number) SANE_FIX((number) * MM_PER_UNIT_UNFIX)
#define FIXED_MM_TO_SCANNER_UNIT(number) SANE_UNFIX(number) / MM_PER_UNIT_UNFIX

#define PIX_TO_SCANNER_UNIT(number, dpi) SANE_UNFIX(SANE_FIX((number) * 1200 / dpi ))
#define SCANNER_UNIT_TO_PIX(number, dpi) SANE_UNFIX(SANE_FIX((number) * dpi / 1200 ))

#define CONFIG_FILE "epjitsu.conf"

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

static SANE_Status attach_one (const char *devicename);
static SANE_Status connect_fd (struct scanner *s);
static SANE_Status disconnect_fd (struct scanner *s);

static SANE_Status
do_cmd(struct scanner *s, int shortTime,
 unsigned char * cmdBuff, size_t cmdLen,
 unsigned char * outBuff, size_t outLen,
 unsigned char * inBuff, size_t * inLen
);

/*
static SANE_Status load_calibration (struct scanner *s);
static SANE_Status read_from_scanner_gray(struct scanner *s);
*/

/* commands */
static SANE_Status load_fw(struct scanner *s);
static SANE_Status get_ident(struct scanner *s);

static SANE_Status change_params(struct scanner *s);

static SANE_Status destroy(struct scanner *s);
static SANE_Status teardown_buffers(struct scanner *s);
static SANE_Status setup_buffers(struct scanner *s);

static SANE_Status object_position(struct scanner *s, int ingest);
static SANE_Status six5 (struct scanner *s);
static SANE_Status coarsecal(struct scanner *s);
static SANE_Status finecal(struct scanner *s);
static SANE_Status send_lut(struct scanner *s);
static SANE_Status lamp(struct scanner *s, unsigned char set);
static SANE_Status set_window(struct scanner *s, int window);
static SANE_Status scan(struct scanner *s);

static SANE_Status read_from_scanner(struct scanner *s, struct transfer *tp);
static SANE_Status descramble_raw(struct scanner *s, struct transfer * tp);
static SANE_Status copy_block_to_page(struct scanner *s, int side);
static SANE_Status binarize_line(struct scanner *s, unsigned char *lineOut, int width);

static SANE_Status get_hardware_status (struct scanner *s);

static SANE_Status load_lut (unsigned char * lut, int in_bits, int out_bits,
  int out_min, int out_max, int slope, int offset);

static int get_page_width (struct scanner *s);
static int get_page_height (struct scanner *s);
static unsigned char get_stat(struct scanner *s);

/* utils */
static void update_transfer_totals(struct transfer * t);
static void hexdump (int level, char *comment, unsigned char *p, int l);
static size_t maxStringSize (const SANE_String_Const strings[]);

#endif /* EPJITSU_H */
