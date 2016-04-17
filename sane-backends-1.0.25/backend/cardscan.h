#ifndef CARDSCAN_H
#define CARDSCAN_H

/* 
 * Part of SANE - Scanner Access Now Easy.
 * Please see opening comment in cardscan.c
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
  OPT_MODE,   /*mono/gray/color*/

  /* must come last: */
  NUM_OPTIONS
};

/* values common to calib and image data */
#define HEADER_SIZE 64
#define PIXELS_PER_LINE 1208

/* values for calib data */
#define CAL_COLOR_SIZE (PIXELS_PER_LINE * 3)
#define CAL_GRAY_SIZE PIXELS_PER_LINE

/* values for image data */
#define MAX_PAPERLESS_LINES 210

struct scanner
{
  /* --------------------------------------------------------------------- */
  /* immutable values which are set during init of scanner.                */
  struct scanner *next;
  char *device_name;            /* The name of the scanner device for sane */

  /* --------------------------------------------------------------------- */
  /* immutable values which are set during inquiry probing of the scanner. */
  SANE_Device sane;
  char * vendor_name;
  char * product_name;

  /* --------------------------------------------------------------------- */
  /* immutable values which are set during reading of config file.         */
  int has_cal_buffer; 
  int lines_per_block; 
  int color_block_size; 
  int gray_block_size; 

  /* --------------------------------------------------------------------- */
  /* changeable SANE_Option structs provide our interface to frontend.     */

  /* long array of option structs */
  SANE_Option_Descriptor opt[NUM_OPTIONS];

  /* --------------------------------------------------------------------- */
  /* some options require lists of strings or numbers, we keep them here   */
  /* instead of in global vars so that they can differ for each scanner    */

  /*mode group*/
  SANE_String_Const mode_list[3];

  /* --------------------------------------------------------------------- */
  /* changeable vars to hold user input. modified by SANE_Options above    */

  /*mode group*/
  int mode;           /*color,lineart,etc*/

  /* --------------------------------------------------------------------- */
  /* values which are derived from setting the options above */
  /* the user never directly modifies these */

  /* this is defined in sane spec as a struct containing:
	SANE_Frame format;
	SANE_Bool last_frame;
	SANE_Int lines;
	SANE_Int depth; ( binary=1, gray=8, color=8 (!24) )
	SANE_Int pixels_per_line;
	SANE_Int bytes_per_line;
  */
  SANE_Parameters params;

  /* --------------------------------------------------------------------- */
  /* calibration data read once */
  unsigned char cal_color_b[CAL_COLOR_SIZE];
  unsigned char cal_gray_b[CAL_GRAY_SIZE];
  unsigned char cal_color_w[CAL_COLOR_SIZE];
  unsigned char cal_gray_w[CAL_GRAY_SIZE];

  /* --------------------------------------------------------------------- */
  /* values which are set by scanning functions to keep track of pages, etc */
  int started;
  int paperless_lines;

  /* buffer part of image */
  unsigned char buffer[PIXELS_PER_LINE * 3 * 32];

  /* how far we have read from scanner into buffer */
  int bytes_rx;

  /* how far we have written from buffer to frontend */
  int bytes_tx;

  /* --------------------------------------------------------------------- */
  /* values used by the command and data sending function                  */
  int fd;                       /* The scanner device file descriptor.     */

};

#define USB_COMMAND_TIME   10000
#define USB_DATA_TIME      10000

#define MODE_COLOR 0
#define MODE_GRAYSCALE 1

/* ------------------------------------------------------------------------- */

#define MM_PER_UNIT_UNFIX SANE_UNFIX(SANE_FIX(MM_PER_INCH / 1200.0))
#define MM_PER_UNIT_FIX SANE_FIX(SANE_UNFIX(SANE_FIX(MM_PER_INCH / 1200.0)))

#define SCANNER_UNIT_TO_FIXED_MM(number) SANE_FIX((number) * MM_PER_UNIT_UNFIX)
#define FIXED_MM_TO_SCANNER_UNIT(number) SANE_UNFIX(number) / MM_PER_UNIT_UNFIX

#define CONFIG_FILE "cardscan.conf"

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

static SANE_Status load_calibration (struct scanner *s);

static SANE_Status heat_lamp_color(struct scanner *s);
static SANE_Status heat_lamp_gray(struct scanner *s);

static SANE_Status read_from_scanner_color(struct scanner *s);
static SANE_Status read_from_scanner_gray(struct scanner *s);

static SANE_Status power_down(struct scanner *s);

static void hexdump (int level, char *comment, unsigned char *p, int l);

static size_t maxStringSize (const SANE_String_Const strings[]);

#endif /* CARDSCAN_H */
