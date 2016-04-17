#ifndef KODAK_H
#define KODAK_H

/* 
 * Part of SANE - Scanner Access Now Easy.
 * Please see opening comment in kodak.c
 */

/* ------------------------------------------------------------------------- 
 * This option list has to contain all options for all scanners supported by
 * this driver. If a certain scanner cannot handle a certain option, there's
 * still the possibility to say so, later.
 */
enum kodak_Option
{
  OPT_NUM_OPTS = 0,

  OPT_MODE_GROUP,
  OPT_SOURCE, /*front/back/duplex*/
  OPT_MODE,   /*mono/ht/gray/color*/
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
  OPT_THRESHOLD,
  OPT_RIF,

  /* must come last: */
  NUM_OPTIONS
};

struct scanner
{
  /* --------------------------------------------------------------------- */
  /* immutable values which are set during init of scanner.                */
  struct scanner *next;
  char *device_name;            /* The name of the scanner device for sane */

  /* --------------------------------------------------------------------- */
  /* immutable values which are set during reading of config file.         */
  int buffer_size;

  /* --------------------------------------------------------------------- */
  /* immutable values which are set during inquiry probing of the scanner. */
  /* members in order found in scsi data...                                */
  SANE_Device sane;

  char vendor_name[9];          /* null-term data returned by SCSI inquiry.*/
  char product_name[17];        /* null-term data returned by SCSI inquiry.*/
  char version_name[5];         /* null-term data returned by SCSI inquiry.*/
  char build_name[3];           /* null-term data returned by SCSI inquiry.*/

  /* --------------------------------------------------------------------- */
  /* immutable values which are set during INQUIRY probing of the scanner, */
  /* or in the init_model cleanup routine...                               */

  /* which modes scanner has */
  int s_mode[4];

  /* min and max resolution for each mode */
  int s_res_min[4];
  int s_res_max[4];

  /* in 1/1200th inches, NOT sane units */
  int s_width_min;
  int s_width_max;
  int s_length_min;
  int s_length_max;

  int s_brightness_steps;
  int s_contrast_steps;
  int s_threshold_steps;
  int s_rif;

  /* --------------------------------------------------------------------- */
  /* changeable SANE_Option structs provide our interface to frontend.     */
  /* some options require lists of strings or numbers, we keep them here   */
  /* instead of in global vars so that they can differ for each scanner    */

  /* long array of option structs */
  SANE_Option_Descriptor opt[NUM_OPTIONS];

  /*mode group*/
  SANE_String_Const o_source_list[4];
  SANE_String_Const o_mode_list[5];
  SANE_Int o_res_list[4][6];

  /*geometry group*/
  SANE_Range o_tl_x_range;
  SANE_Range o_tl_y_range;
  SANE_Range o_br_x_range;
  SANE_Range o_br_y_range;
  SANE_Range o_page_x_range;
  SANE_Range o_page_y_range;

  /*enhancement group*/
  SANE_Range o_brightness_range;
  SANE_Range o_contrast_range;
  SANE_Range o_threshold_range;

  /* --------------------------------------------------------------------- */
  /* changeable vars to hold user input. modified by SANE_Options above    */

  /*mode group*/
  int u_mode;           /* color,lineart,etc */
  int u_source;         /* adf front,adf duplex,etc */
  int u_res;            /* resolution in dpi                       */

  /*geometry group*/
  /* The desired size of the scan, all in 1/1200 inch */
  int u_tl_x;
  int u_tl_y;
  int u_br_x;
  int u_br_y;
  int u_page_width;
  int u_page_height;

  /*enhancement group*/
  int u_brightness;
  int u_contrast;
  int u_threshold;
  int u_rif;
  int u_compr;

  /* --------------------------------------------------------------------- */
  /* values which are set by the scanner's post-scan image header          */
  int i_bytes;
  int i_id;
  int i_dpi;
  int i_tlx;
  int i_tly;
  int i_width;
  int i_length;
  int i_bpp;
  int i_compr;

  /* --------------------------------------------------------------------- */
  /* values which are set by scanning functions to keep track of pages, etc */
  int started;

  /* total to read/write */
  int bytes_tot;

  /* how far we have read */
  int bytes_rx;

  /* how far we have written */
  int bytes_tx;

  /* size of buffer */
  int bytes_buf;

  /* the buffer */
  unsigned char * buffer;

  /* --------------------------------------------------------------------- */
  /* values which used by the command and data sending functions (scsi/usb)*/
  int fd;                      /* The scanner device file descriptor.     */
  size_t rs_info;

};

#define DEFAULT_BUFFER_SIZE 32768

#define SIDE_FRONT 0
#define SIDE_BACK 1

#define SOURCE_ADF_FRONT 0
#define SOURCE_ADF_BACK 1
#define SOURCE_ADF_DUPLEX 2

#define MODE_LINEART   0
#define MODE_HALFTONE  1
#define MODE_GRAYSCALE 2
#define MODE_COLOR     3

/* ------------------------------------------------------------------------- */

#define MM_PER_INCH    25.4
#define MM_PER_UNIT_UNFIX SANE_UNFIX(SANE_FIX(MM_PER_INCH / 1200.0))
#define MM_PER_UNIT_FIX SANE_FIX(SANE_UNFIX(SANE_FIX(MM_PER_INCH / 1200.0)))

#define SCANNER_UNIT_TO_FIXED_MM(number) SANE_FIX((number) * MM_PER_UNIT_UNFIX)
#define FIXED_MM_TO_SCANNER_UNIT(number) SANE_UNFIX(number) / MM_PER_UNIT_UNFIX

#define KODAK_CONFIG_FILE "kodak.conf"

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

static SANE_Status attach_one (const char *name);
static SANE_Status connect_fd (struct scanner *s);
static SANE_Status disconnect_fd (struct scanner *s);
static SANE_Status sense_handler (int scsi_fd, u_char * result, void *arg);

static SANE_Status init_inquire (struct scanner *s);
static SANE_Status init_model (struct scanner *s);
static SANE_Status init_user (struct scanner *s);
static SANE_Status init_options (struct scanner *s);

static SANE_Status
do_cmd(struct scanner *s, int runRS, int shortTime,
 unsigned char * cmdBuff, size_t cmdLen,
 unsigned char * outBuff, size_t outLen,
 unsigned char * inBuff, size_t * inLen
);

#if 0 /* unused */
static SANE_Status wait_scanner (struct scanner *s);
#endif

static SANE_Status do_cancel (struct scanner *scanner);

static SANE_Status set_window (struct scanner *s);
static SANE_Status read_imageheader(struct scanner *s);
static SANE_Status send_sc(struct scanner *s);

static SANE_Status read_from_scanner(struct scanner *s);
static SANE_Status copy_buffer(struct scanner *s, unsigned char * buf, int len);
static SANE_Status read_from_buffer(struct scanner *s, SANE_Byte * buf, SANE_Int max_len, SANE_Int * len);

static void hexdump (int level, char *comment, unsigned char *p, int l);

static size_t maxStringSize (const SANE_String_Const strings[]);

#endif /* KODAK_H */
