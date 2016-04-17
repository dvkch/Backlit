/*******************************************************************************
 * SANE - Scanner Access Now Easy.

   avision.h 

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

   *****************************************************************************

   This backend is based upon the Tamarack backend and adapted to the Avision
   scanners by René Rebe and Meino Cramer.
   
   Check the avision.c file for detailed copyright and change-log
   information.

********************************************************************************/

#ifndef avision_h
#define avision_h

#ifdef HAVE_STDINT_H
# include <stdint.h>            /* available in ISO C99 */
#else
# include <sys/types.h>
typedef uint8_t uint8_t;
typedef uint16_t uint16_t;
typedef uint32_t uint32_t;
#endif /* HAVE_STDINT_H */

#ifndef PATH_MAX
# define PATH_MAX 1024
#endif

typedef enum Avision_ConnectionType {
  AV_SCSI,
  AV_USB
} Avision_ConnectionType;

/* information needed for device access */
typedef struct Avision_Connection {
  Avision_ConnectionType connection_type;
  int scsi_fd;			/* SCSI filedescriptor */
  SANE_Int usb_dn;		/* USB (libusb or scanner.c) device number */
  enum {
    AVISION_USB_UNTESTED_STATUS, /* status type untested */
    AVISION_USB_INT_STATUS,      /* interrupt endp. (USB 1.x device) status */
    AVISION_USB_BULK_STATUS      /* bulk endp. (USB 2.0 device) status */
  } usb_status;
  
} Avision_Connection;

typedef struct Avision_HWEntry {
  const char* scsi_mfg;
  const char* scsi_model;

  int   usb_vendor;
  int   usb_product;

  const char* real_mfg;
  const char* real_model;
  
  /* feature overwrites - as embedded CPUs have 16bit enums - this
     would need a change ... */
    /* force no calibration */
  #define AV_NO_CALIB ((uint64_t)1<<0)
    
    /* force all in one command calibration */
  #define AV_ONE_CALIB_CMD ((uint64_t)1<<1)
    
    /* no gamma table */
  #define AV_NO_GAMMA ((uint64_t)1<<2)
    
    /* light check is bogus */
  #define AV_LIGHT_CHECK_BOGUS ((uint64_t)1<<3)
    
    /* no button though the device advertise it */
  #define AV_NO_BUTTON ((uint64_t)1<<4)
    
    /* if the scan area needs to be forced to A3 */
  #define AV_FORCE_A3 ((uint64_t)1<<5)
    
    /* if the scan area and resolution needs to be forced for films */
  #define AV_FORCE_FILM ((uint64_t)1<<6)
    
    /* does not suport, or very broken background (added for AV610C2) */
  #define AV_NO_BACKGROUND ((uint64_t)1<<7)
    
    /* is film scanner - no detection yet */
  #define AV_FILMSCANNER ((uint64_t)1<<8)

    /* fujitsu adaption */
  #define AV_FUJITSU ((uint64_t)1<<9)

    /* gray calibration data has to be uploaded on the blue channel ... ? */
  #define AV_GRAY_CALIB_BLUE ((uint64_t)1<<10)
    
    /* Interrupt endpoint button readout (so far AV220) */
  #define AV_INT_BUTTON ((uint64_t)1<<11)

    /* send acceleration table ... */
  #define AV_ACCEL_TABLE ((uint64_t)1<<12)

    /* non-interlaced scanns up to 300 dpi (AV32xx / AV83xx) */
  #define AV_NON_INTERLACED_DUPLEX_300 ((uint64_t)1<<13)

    /* do not read multiples of 64 bytes - stalls the USB chip */
  #define AV_NO_64BYTE_ALIGN ((uint64_t)1<<14)

    /* force channel-by-channel calibration */
  #define AV_MULTI_CALIB_CMD ((uint64_t)1<<15)

    /* non color scans are faster with a filter applied (AV32xx) */
  #define AV_FASTER_WITH_FILTER ((uint64_t)1<<16)

    /* interlaced data with 1 line distance */
  #define AV_2ND_LINE_INTERLACED ((uint64_t)1<<17)

    /* does not keep the window though it advertices so */
  #define AV_DOES_NOT_KEEP_WINDOW ((uint64_t)1<<18)

    /* does not keep the gamma though it advertices so */
  #define AV_DOES_NOT_KEEP_GAMMA ((uint64_t)1<<19)

    /* advertises ADF is BGR order, but isn't (or vice versa) */
  #define AV_ADF_BGR_ORDER_INVERT ((uint64_t)1<<20)

    /* allows 12bit mode, though not flagged */
  #define AV_12_BIT_MODE ((uint64_t)1<<21)

    /* very broken background raster */
  #define AV_BACKGROUND_QUIRK ((uint64_t)1<<22)
	
    /* though marked as GRAY only the scanner can do GRAY modes */
  #define AV_GRAY_MODES ((uint64_t)1<<23)

    /* no seperate, single REAR scan (AV122, DM152, ...) */
  #define AV_NO_REAR ((uint64_t)1<<24)
    
    /* only scan with some known good hardware resolutions, as the
       scanner fails to properly interpoloate in between (e.g.  AV121,
       DM152 on duplex scans - but also the AV600), software scale and
       interpolate to all the others */
  #define AV_SOFT_SCALE ((uint64_t)1<<25)
    
    /* does keep window though it does not advertice it - the AV122/DM152
       mess up image data if window is resend between ADF pages */
  #define AV_DOES_KEEP_WINDOW ((uint64_t)1<<26)
    
    /* does keep gamma though it does not advertice it */
  #define AV_DOES_KEEP_GAMMA ((uint64_t)1<<27)
    
    /* does the scanner contain a Cancel button? */
  #define AV_CANCEL_BUTTON ((uint64_t)1<<28)
    
    /* is the rear image offset? */
  #define AV_REAR_OFFSET ((uint64_t)1<<29)

    /* some devices do not need a START_SCAN, even hang with it */
  #define AV_NO_START_SCAN ((uint64_t)1<<30)
    
  #define AV_INT_STATUS ((uint64_t)1<<31)

    /* force no calibration */
  #define AV_NO_TUNE_SCAN_LENGTH ((uint64_t)1<<32)

    /* for gray scans, set grey filter */
  #define AV_USE_GRAY_FILTER ((uint64_t)1<<33)

    /* For (HP) scanners with flipping duplexers */
  #define AV_ADF_FLIPPING_DUPLEX ((uint64_t)1<<34)

    /* For scanners which need to have their firmware read to properly function. */
  #define AV_FIRMWARE ((uint64_t)1<<35)
    
    /* maybe more ...*/
  uint64_t feature_type;

} Avision_HWEntry;

typedef enum {
  AV_ASIC_Cx = 0,
  AV_ASIC_C1 = 1,
  AV_ASIC_W1 = 2,
  AV_ASIC_C2 = 3,
  AV_ASIC_C5 = 5,
  AV_ASIC_C6 = 6,
  AV_ASIC_C7 = 7,
  AV_ASIC_OA980 = 128,
  AV_ASIC_OA982 = 129
} asic_type;

typedef enum {
  AV_THRESHOLDED,
  AV_DITHERED,
  AV_GRAYSCALE,       /* all gray needs to be before color for is_color() */
  AV_GRAYSCALE12,
  AV_GRAYSCALE16,
  AV_TRUECOLOR,
  AV_TRUECOLOR12,
  AV_TRUECOLOR16,
  AV_COLOR_MODE_LAST
} color_mode;

typedef enum {
  AV_NORMAL,
  AV_TRANSPARENT,
  AV_ADF,
  AV_ADF_REAR,
  AV_ADF_DUPLEX,
  AV_SOURCE_MODE_LAST
} source_mode;

typedef enum {
  AV_NORMAL_DIM,
  AV_TRANSPARENT_DIM,
  AV_ADF_DIM,
  AV_SOURCE_MODE_DIM_LAST
} source_mode_dim;

enum Avision_Option
{
  OPT_NUM_OPTS = 0,      /* must come first */
  
  OPT_MODE_GROUP,
  OPT_MODE,
  OPT_RESOLUTION,
#define OPT_RESOLUTION_DEFAULT 150
  OPT_SPEED,
  OPT_PREVIEW,
  
  OPT_SOURCE,            /* scan source normal, transparency, ADF */
  
  OPT_GEOMETRY_GROUP,
  OPT_TL_X,	         /* top-left x */
  OPT_TL_Y,	         /* top-left y */
  OPT_BR_X,	         /* bottom-right x */
  OPT_BR_Y,		 /* bottom-right y */
  
  OPT_OVERSCAN_TOP,      /* overscan for auto-crop/deskew, if supported */
  OPT_OVERSCAN_BOTTOM,
  OPT_BACKGROUND,        /* background raster lines to read out */
  
  OPT_ENHANCEMENT_GROUP,
  OPT_BRIGHTNESS,
  OPT_CONTRAST,
  OPT_QSCAN,
  OPT_QCALIB,
  
  OPT_GAMMA_VECTOR,      /* first must be gray */
  OPT_GAMMA_VECTOR_R,    /* then r g b vector */
  OPT_GAMMA_VECTOR_G,
  OPT_GAMMA_VECTOR_B,

  OPT_EXPOSURE,          /* film exposure adjustment */
  OPT_IR,                /* infra-red */
  OPT_MULTISAMPLE,       /* multi-sample */
  
  OPT_MISC_GROUP,
  OPT_FRAME,             /* Film holder control */

  OPT_POWER_SAVE_TIME,   /* set power save time to the scanner */

  OPT_MESSAGE,           /* optional message from the scanner display */  
  OPT_NVRAM,             /* retrieve NVRAM values as pretty printed text */
  
  OPT_PAPERLEN,          /* Use paper_length field to detect double feeds */
  OPT_ADF_FLIP,          /* For flipping duplex, reflip the document */

  NUM_OPTIONS            /* must come last */
};

typedef struct Avision_Dimensions
{
  /* in dpi */
  int xres;
  int yres;
  
  /* in pixels */
  long tlx;
  long tly;
  long brx;
  long bry;
  
  /* in pixels */
  int line_difference;
  int rear_offset; /* in pixels of HW res */

  /* interlaced duplex scan */
  SANE_Bool interlaced_duplex;
  
  /* in dpi, likewise - different if software scaling required */
  int hw_xres;
  int hw_yres;
  
  int hw_pixels_per_line;
  int hw_bytes_per_line;
  int hw_lines;
  
} Avision_Dimensions;

/* this contains our low-level info - not relevant for the SANE interface  */
typedef struct Avision_Device
{
  struct Avision_Device* next;
  SANE_Device sane;
  Avision_Connection connection;
  
  /* structs used to store config options */
  SANE_Range dpi_range;
  SANE_Range x_range;
  SANE_Range y_range;
  SANE_Range speed_range;

  asic_type inquiry_asic_type;
  SANE_Bool inquiry_new_protocol;

  SANE_Bool inquiry_nvram_read;
  SANE_Bool inquiry_power_save_time;

  SANE_Bool inquiry_light_box;
  SANE_Bool inquiry_adf;
  SANE_Bool inquiry_duplex;
  SANE_Bool inquiry_duplex_interlaced;
  SANE_Bool inquiry_paper_length;
  SANE_Bool inquiry_batch_scan;
  SANE_Bool inquiry_detect_accessories;
  SANE_Bool inquiry_needs_calibration;
  SANE_Bool inquiry_needs_gamma;
  SANE_Bool inquiry_keeps_gamma;
  SANE_Bool inquiry_keeps_window;
  SANE_Bool inquiry_calibration;
  SANE_Bool inquiry_3x3_matrix;
  SANE_Bool inquiry_needs_software_colorpack;
  SANE_Bool inquiry_needs_line_pack;
  SANE_Bool inquiry_adf_need_mirror;
  SANE_Bool inquiry_adf_bgr_order;
  SANE_Bool inquiry_light_detect;
  SANE_Bool inquiry_light_control;
  SANE_Bool inquiry_exposure_control;
  
  int       inquiry_max_shading_target;
  SANE_Bool inquiry_button_control;
  unsigned int inquiry_buttons;
  SANE_Bool inquiry_tune_scan_length;
  SANE_Bool inquiry_background_raster;
  int       inquiry_background_raster_pixel;
  
  enum {AV_FLATBED,
	AV_FILM,
	AV_SHEETFEED
  } scanner_type;
  
  /* the list of available color modes */
  SANE_String_Const color_list[AV_COLOR_MODE_LAST + 1];
  color_mode color_list_num[AV_COLOR_MODE_LAST];
  color_mode color_list_default;
  
  /* the list of available source modes */
  SANE_String_Const source_list[AV_SOURCE_MODE_LAST + 1];
  source_mode source_list_num[AV_SOURCE_MODE_LAST];
  
  int inquiry_optical_res;        /* in dpi */
  int inquiry_max_res;            /* in dpi */
  
  double inquiry_x_ranges  [AV_SOURCE_MODE_DIM_LAST]; /* in mm */
  double inquiry_y_ranges  [AV_SOURCE_MODE_DIM_LAST]; /* in mm */
  
  int inquiry_color_boundary;
  int inquiry_gray_boundary;
  int inquiry_dithered_boundary;
  int inquiry_thresholded_boundary;
  int inquiry_line_difference; /* software color pack */

  int inquiry_channels_per_pixel;
  int inquiry_bits_per_channel;
  int inquiry_no_gray_modes;
  
  int scsi_buffer_size; /* nice to have SCSI buffer size */
  int read_stripe_size; /* stripes to be read at-a-time */

  /* film scanner atributes - maybe these should be in the scanner struct? */
  SANE_Range frame_range;
  SANE_Word current_frame;
  SANE_Word holder_type;
  
  /* some versin corrections */
  uint16_t data_dq; /* was ox0A0D - but hangs some new scanners */
  
  Avision_HWEntry* hw;
} Avision_Device;

/* all the state relevant for the SANE interface */
typedef struct Avision_Scanner
{
  struct Avision_Scanner* next;
  Avision_Device* hw;
  
  SANE_Option_Descriptor opt [NUM_OPTIONS];
  Option_Value val [NUM_OPTIONS];
  SANE_Int gamma_table [4][256];
  
  /* we now save the calib data because we might need it for 16bit software
     calibration :-( */
  uint8_t* dark_avg_data;
  uint8_t* white_avg_data;
  
  /* background raster data, if duplex first front, then rear */
  uint8_t* background_raster;
  
  /* Parsed option values and variables that are valid only during
     the actual scan: */
  SANE_Bool prepared;		/* first page marker */
  SANE_Bool scanning;           /* scan in progress */
  unsigned int page;            /* page counter, 0: uninitialized, 1: scanning 1st page, ... */

  SANE_Parameters params;       /* scan window */
  Avision_Dimensions avdimen;   /* scan window - detailed internals */
  
  /* Internal data for duplex scans */
  char duplex_rear_fname [PATH_MAX];
  SANE_Bool duplex_rear_valid;
  
  color_mode c_mode;
  source_mode source_mode;
  source_mode_dim source_mode_dim;
  
  /* Avision HW Access Connection (SCSI/USB abstraction) */
  Avision_Connection av_con;

  SANE_Pid reader_pid;	/* process id of reader */
  int read_fds;		/* pipe reading end */
  int write_fds;	/* pipe writing end */
  
} Avision_Scanner;

/* Some Avision driver internal defines */
#define AV_WINID 0

/* Avision SCSI over USB error codes */
#define AVISION_USB_GOOD                    0x00
#define AVISION_USB_REQUEST_SENSE           0x02
#define AVISION_USB_BUSY                    0x08

/* SCSI commands that the Avision scanners understand: */

#define AVISION_SCSI_TEST_UNIT_READY        0x00
#define AVISION_SCSI_REQUEST_SENSE          0x03
#define AVISION_SCSI_MEDIA_CHECK            0x08
#define AVISION_SCSI_INQUIRY                0x12
#define AVISION_SCSI_MODE_SELECT            0x15
#define AVISION_SCSI_RESERVE_UNIT           0x16
#define AVISION_SCSI_RELEASE_UNIT           0x17
#define AVISION_SCSI_SCAN                   0x1b
#define AVISION_SCSI_SET_WINDOW             0x24
#define AVISION_SCSI_READ                   0x28
#define AVISION_SCSI_SEND                   0x2a
#define AVISION_SCSI_OBJECT_POSITION        0x31
#define AVISION_SCSI_GET_DATA_STATUS        0x34

#define AVISION_SCSI_OP_REJECT_PAPER        0x00
#define AVISION_SCSI_OP_LOAD_PAPER          0x01
#define AVISION_SCSI_OP_GO_HOME             0x02
#define AVISION_SCSI_OP_TRANS_CALIB_GRAY    0x04
#define AVISION_SCSI_OP_TRANS_CALIB_COLOR   0x05

/* These apply to bitset1.  The values are 0 to 6, shifted 3 bits to the left */
#define AVISION_FILTER_NONE	0x00
#define AVISION_FILTER_RED	0x08
#define AVISION_FILTER_GREEN	0x10
#define AVISION_FILTER_BLUE	0x18
#define AVISION_FILTER_RGB	0x20
#define AVISION_FILTER_CMYK	0x28
#define AVISION_FILTER_GRAY	0x30

/* The SCSI structures that we have to send to an avision to get it to
   do various stuff... */

typedef struct command_header
{
  uint8_t opc;
  uint8_t pad0 [3];
  uint8_t len;
  uint8_t pad1;
} command_header;

typedef struct command_set_window
{
  uint8_t opc;
  uint8_t reserved0 [5];
  uint8_t transferlen [3];
  uint8_t control;
} command_set_window;

typedef struct command_read
{
  uint8_t opc;
  uint8_t bitset1;
  uint8_t datatypecode;
  uint8_t readtype;
  uint8_t datatypequal [2];
  uint8_t transferlen [3];
  uint8_t control;
} command_read;

typedef struct command_scan
{
  uint8_t opc;
  uint8_t bitset0;
  uint8_t reserved0 [2];
  uint8_t transferlen;
  uint8_t bitset1;
} command_scan;

typedef struct command_send
{
  uint8_t opc;
  uint8_t bitset1;
  uint8_t datatypecode;
  uint8_t reserved0;  
  uint8_t datatypequal [2];
  uint8_t transferlen [3];
  uint8_t reserved1;
} command_send;

typedef struct firmware_status
{
  uint8_t download_firmware;
  uint8_t first_effective_pixel_flatbed [2];
  uint8_t first_effective_pixel_adf_front [2];
  uint8_t first_effective_pixel_adf_rear [2];
  uint8_t reserved;
} firmware_status;

typedef struct nvram_data
{
  uint8_t pad_scans [4];
  uint8_t adf_simplex_scans [4];
  uint8_t adf_duplex_scans [4];
  uint8_t flatbed_scans [4];

  uint8_t flatbed_leading_edge [2];
  uint8_t flatbed_side_edge [2];
  uint8_t adf_leading_edge [2];
  uint8_t adf_side_edge [2];
  uint8_t adf_rear_leading_edge [2];
  uint8_t adf_rear_side_edge [2];

  uint8_t born_month [2];
  uint8_t born_day [2];
  uint8_t born_year [2];

  uint8_t first_scan_month [2];
  uint8_t first_scan_day [2];
  uint8_t first_scan_year [2];

  uint8_t vertical_magnification [2];
  uint8_t horizontal_magnification [2];

  uint8_t ccd_type;
  uint8_t scan_speed;

  char     serial [24];

  uint8_t power_saving_time [2];

  uint8_t auto_feed;
  uint8_t roller_count [4];
  uint8_t multifeed_count [4];
  uint8_t jam_count [4];

  uint8_t reserved;
  char     identify_info[16]; 
  char     formal_name[16];

  uint8_t reserved2 [10];
} nvram_data;

typedef struct command_set_window_window
{
  struct {
    uint8_t reserved0 [6];
    uint8_t desclen [2];
  } header;
  
  struct {
    uint8_t winid;
    uint8_t reserved0;
    uint8_t xres [2];
    uint8_t yres [2];
    uint8_t ulx [4];
    uint8_t uly [4];
    uint8_t width [4];
    uint8_t length [4];
    uint8_t brightness;
    uint8_t threshold;
    uint8_t contrast;
    uint8_t image_comp;
    uint8_t bpc;
    uint8_t halftone [2];
    uint8_t padding_and_bitset;
    uint8_t bitordering [2];
    uint8_t compr_type;
    uint8_t compr_arg;
    uint8_t paper_length[2];
    uint8_t reserved1 [4];

    /* Avision specific parameters */
    uint8_t vendor_specific;
    uint8_t paralen; /* bytes following after this byte */
  } descriptor;
  
  struct {
    uint8_t bitset1;
    uint8_t highlight;
    uint8_t shadow;
    uint8_t line_width [2];
    uint8_t line_count [2];
    
    /* the tail is quite version and model specific */
    union {
      struct {
	uint8_t bitset2;
	uint8_t reserved;
      } old;
      
      struct {
	uint8_t bitset2;
	uint8_t ir_exposure_time;
	
	/* optional */
	uint8_t r_exposure_time [2];
	uint8_t g_exposure_time [2];
	uint8_t b_exposure_time [2];
	
	uint8_t bitset3; /* reserved in the v2 */
	uint8_t auto_focus;
	uint8_t line_width_msb;
	uint8_t line_count_msb;
	uint8_t background_lines;
      } normal;
      
      struct {
	uint8_t reserved0 [4];
	uint8_t paper_size;
	uint8_t paperx [4];
	uint8_t papery [4];
	uint8_t reserved1 [2];
      } fujitsu;
    } type;
  } avision;
} command_set_window_window;

typedef struct page_header
{
  uint8_t pad0 [4];
  uint8_t code;
  uint8_t length;
} page_header;

typedef struct avision_page
{
  uint8_t gamma;
  uint8_t thresh;
  uint8_t masks;
  uint8_t delay;
  uint8_t features;
  uint8_t pad0;
} avision_page;

typedef struct calibration_format
{
  uint16_t pixel_per_line;
  uint8_t bytes_per_channel;
  uint8_t lines;
  uint8_t flags;
  uint8_t ability1;
  uint8_t r_gain;
  uint8_t g_gain;
  uint8_t b_gain;
  uint16_t r_shading_target;
  uint16_t g_shading_target;
  uint16_t b_shading_target;
  uint16_t r_dark_shading_target;
  uint16_t g_dark_shading_target;
  uint16_t b_dark_shading_target;
  
  /* not returned but usefull in some places */
  uint8_t channels;
} calibration_format;

typedef struct matrix_3x3
{
  uint16_t v[9];
} matrix_3x3;

typedef struct acceleration_info 
{
  uint16_t total_steps;
  uint16_t stable_steps;
  uint32_t table_units;
  uint32_t base_units;
  uint16_t start_speed;
  uint16_t target_speed;
  uint8_t ability;
  uint8_t table_count;
  uint8_t reserved[6];
} acceleration_info;

/* set/get SCSI highended (big-endian) variables. Declare them as an array
 * of chars endianness-safe, int-size safe ... */
#define set_double(var,val) var[0] = ((val) >> 8) & 0xff;  \
                            var[1] = ((val)     ) & 0xff

#define set_triple(var,val) var[0] = ((val) >> 16) & 0xff; \
                            var[1] = ((val) >> 8 ) & 0xff; \
                            var[2] = ((val)      ) & 0xff

#define set_quad(var,val)   var[0] = ((val) >> 24) & 0xff; \
                            var[1] = ((val) >> 16) & 0xff; \
                            var[2] = ((val) >> 8 ) & 0xff; \
                            var[3] = ((val)      ) & 0xff

#define get_double(var) ((*var << 8) + *(var + 1))

#define get_triple(var) ((*var << 16) + \
                         (*(var + 1) << 8) + *(var + 2))

#define get_quad(var)   ((*var << 24) + \
                         (*(var + 1) << 16) + \
                         (*(var + 2) << 8) + *(var + 3))

/* set/get Avision lowended (little-endian) shading data */
#define set_double_le(var,val) var[0] = ((val)     ) & 0xff;  \
                               var[1] = ((val) >> 8) & 0xff

#define get_double_le(var) ((*(var + 1) << 8) + *var)

#define BIT(n, p) ((n & (1 << p)) ? 1 : 0)

#define SET_BIT(n, p) (n |= (1 << p))
#define CLEAR_BIT(n, p) (n &= ~(1 << p))

/* These should be in saneopts.h */
#define SANE_NAME_FRAME "frame"
#define SANE_TITLE_FRAME SANE_I18N("Number of the frame to scan")
#define SANE_DESC_FRAME  SANE_I18N("Selects the number of the frame to scan")

#define SANE_NAME_DUPLEX "duplex"
#define SANE_TITLE_DUPLEX SANE_I18N("Duplex scan")
#define SANE_DESC_DUPLEX SANE_I18N("Duplex scan provide a scan of the front and back side of the document")

#ifdef AVISION_ENHANCED_SANE
#warning "Compiled Avision backend will violate the SANE standard"
/* Some Avision SANE extensions */
typedef enum
{
  SANE_STATUS_LAMP_WARMING = SANE_STATUS_ACCESS_DENIED + 1	/* lamp is warming up */
}
SANE_Avision_Status;

/* public API extension */

extern SANE_Status ENTRY(media_check) (SANE_Handle handle);

#endif

#endif /* avision_h */
