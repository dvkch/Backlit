/*
   Copyright (C) 2008, Panasonic Russia Ltd.
*/
/* sane - Scanner Access Now Easy.
   Panasonic KV-S1020C / KV-S1025C USB scanners.
*/

#ifndef __KVS1025_LOW_H
#define __KVS1025_LOW_H

#include "kvs1025_cmds.h"

#define VENDOR_ID       0x04DA

typedef enum
{
  KV_S1020C = 0x1007,
  KV_S1025C = 0x1006,
  KV_S1045C = 0x1010
} KV_MODEL_TYPE;

/* Store an integer in 2, 3 or 4 byte in a big-endian array. */
#define Ito16(val, buf) { \
 ((unsigned char *)buf)[0] = ((val) >> 8) & 0xff; \
 ((unsigned char *)buf)[1] = ((val) >> 0) & 0xff; \
}

#define Ito24(val, buf) { \
 ((unsigned char *)buf)[0] = ((val) >> 16) & 0xff; \
 ((unsigned char *)buf)[1] = ((val) >>  8) & 0xff; \
 ((unsigned char *)buf)[2] = ((val) >>  0) & 0xff; \
}

#define Ito32(val, buf) { \
 ((unsigned char *)buf)[0] = ((val) >> 24) & 0xff; \
 ((unsigned char *)buf)[1] = ((val) >> 16) & 0xff; \
 ((unsigned char *)buf)[2] = ((val) >>  8) & 0xff; \
 ((unsigned char *)buf)[3] = ((val) >>  0) & 0xff; \
}

/* 32 bits from an array to an integer (eg ntohl). */
#define B32TOI(buf) \
    ((((unsigned char *)buf)[0] << 24) | \
     (((unsigned char *)buf)[1] << 16) | \
     (((unsigned char *)buf)[2] <<  8) |  \
     (((unsigned char *)buf)[3] <<  0))

/* 24 bits from an array to an integer. */
#define B24TOI(buf) \
     (((unsigned char *)buf)[0] << 16) | \
     (((unsigned char *)buf)[1] <<  8) |  \
     (((unsigned char *)buf)[2] <<  0))

#define SCSI_FD                     int
#define SCSI_BUFFER_SIZE            (0x40000-12)

typedef enum
{
  KV_SCSI_BUS = 0x01,
  KV_USB_BUS = 0x02
} KV_BUS_MODE;

typedef enum
{
  SM_BINARY = 0x00,
  SM_DITHER = 0x01,
  SM_GRAYSCALE = 0x02,
  SM_COLOR = 0x05
} KV_SCAN_MODE;

typedef struct
{
  unsigned char data[16];
  int len;
} CDB;

typedef struct
{
  int width;
  int height;
} KV_PAPER_SIZE;

/* remarked -- KV-S1020C / KV-S1025C supports ADF only
typedef enum
{
    TRUPER_ADF         = 0,
    TRUPER_FLATBED     = 1
} KV_SCAN_SOURCE;
*/

/* options */
typedef enum
{
  OPT_NUM_OPTS = 0,

  /* General options */
  OPT_MODE_GROUP,
  OPT_MODE,			/* scanner modes */
  OPT_RESOLUTION,		/* X and Y resolution */
  OPT_DUPLEX,			/* Duplex mode */
  OPT_SCAN_SOURCE,		/* Scan source, fixed to ADF */
  OPT_FEEDER_MODE,		/* Feeder mode, fixed to Continous */
  OPT_LONGPAPER,		/* Long paper mode */
  OPT_LENGTHCTL,		/* Length control mode */
  OPT_MANUALFEED,		/* Manual feed mode */
  OPT_FEED_TIMEOUT,		/* Feed timeout */
  OPT_DBLFEED,			/* Double feed detection mode */
  OPT_FIT_TO_PAGE,		/* Scanner shrinks image to fit scanned page */

  /* Geometry group */
  OPT_GEOMETRY_GROUP,
  OPT_PAPER_SIZE,		/* Paper size */
  OPT_LANDSCAPE,		/* true if landscape; new for Truper 3200/3600 */
  OPT_TL_X,			/* upper left X */
  OPT_TL_Y,			/* upper left Y */
  OPT_BR_X,			/* bottom right X */
  OPT_BR_Y,			/* bottom right Y */

  OPT_ENHANCEMENT_GROUP,
  OPT_BRIGHTNESS,		/* Brightness */
  OPT_CONTRAST,			/* Contrast */
  OPT_AUTOMATIC_THRESHOLD,	/* Binary threshold */
  OPT_HALFTONE_PATTERN,		/* Halftone pattern */
  OPT_AUTOMATIC_SEPARATION,	/* Automatic separation */
  OPT_WHITE_LEVEL,		/* White level */
  OPT_NOISE_REDUCTION,		/* Noise reduction */
  OPT_IMAGE_EMPHASIS,		/* Image emphasis */
  OPT_GAMMA,			/* Gamma */
  OPT_LAMP,			/* Lamp -- color drop out */
  OPT_INVERSE,			/* Inverse image */
  OPT_MIRROR,			/* Mirror image */
  OPT_JPEG,			/* JPEG Compression */
  OPT_ROTATE,			/* Rotate image */

  OPT_SWDESKEW,                 /* Software deskew */
  OPT_SWDESPECK,                /* Software despeckle */
  OPT_SWDEROTATE,               /* Software detect/correct 90 deg. rotation */
  OPT_SWCROP,                   /* Software autocrop */
  OPT_SWSKIP,                   /* Software blank page skip */

  /* must come last: */
  OPT_NUM_OPTIONS
} KV_OPTION;

typedef struct
{
  int memory_size;		/* in MB */
  int min_resolution;		/* in DPI */
  int max_resolution;		/* in DPI */
  int step_resolution;		/* in DPI */
  int support_duplex;		/* 1 if true */
  int support_lamp;		/* 1 if true */
  int max_x_range;		/* in mm */
  int max_y_range;		/* in mm */
} KV_SUPPORT_INFO;

typedef struct kv_scanner_dev
{
  struct kv_scanner_dev *next;

  SANE_Device sane;

  /* Infos from inquiry. */
  char scsi_type;
  char scsi_type_str[32];
  char scsi_vendor[12];
  char scsi_product[20];
  char scsi_version[8];

  /* Bus info */
  KV_BUS_MODE bus_mode;
  SANE_Int usb_fd;
  char device_name[100];
  char *scsi_device_name;
  SCSI_FD scsi_fd;

  KV_MODEL_TYPE model_type;

  SANE_Parameters params[2];

  /* SCSI handling */
  SANE_Byte *buffer0;
  SANE_Byte *buffer;		/* buffer = buffer0 + 12 */
  /* for USB bulk transfer, a 12 bytes container
     is required for each block */
  /* Scanning handling. */
  int scanning;			/* TRUE if a scan is running. */
  int current_page;		/* the current page number, 0 is page 1 */
  int current_side;		/* the current side */
  int bytes_to_read[2];		/* bytes to read */

  /* --------------------------------------------------------------------- */
  /* values used by the software enhancment code (deskew, crop, etc)       */
  SANE_Status deskew_stat;
  int deskew_vals[2];
  double deskew_slope;

  SANE_Status crop_stat;
  int crop_vals[4];

  /* Support info */
  KV_SUPPORT_INFO support_info;

  SANE_Range x_range, y_range;

  /* Options */
  SANE_Option_Descriptor opt[OPT_NUM_OPTIONS];
  Option_Value val[OPT_NUM_OPTIONS];
  SANE_Bool option_set;

  /* Image buffer */
  SANE_Byte *img_buffers[2];
  SANE_Byte *img_pt[2];
  int img_size[2];
} KV_DEV, *PKV_DEV;

#define GET_OPT_VAL_W(dev, idx) ((dev)->val[idx].w)
#define GET_OPT_VAL_L(dev, idx, token) get_optval_list(dev, idx, \
        go_##token##_list, go_##token##_val)

#define IS_DUPLEX(dev) GET_OPT_VAL_W(dev, OPT_DUPLEX)

/* Prototypes in kvs1025_opt.c */

int get_optval_list (const PKV_DEV dev, int idx,
		     const SANE_String_Const * str_list, const int *val_list);
KV_SCAN_MODE kv_get_mode (const PKV_DEV dev);
int kv_get_depth (KV_SCAN_MODE mode);

void kv_calc_paper_size (const PKV_DEV dev, int *w, int *h);

const SANE_Option_Descriptor *kv_get_option_descriptor (PKV_DEV dev,
							SANE_Int option);
void kv_init_options (PKV_DEV dev);
SANE_Status kv_control_option (PKV_DEV dev, SANE_Int option,
			       SANE_Action action, void *val,
			       SANE_Int * info);
void hexdump (int level, const char *comment, unsigned char *p, int l);
void kv_set_window_data (PKV_DEV dev,
			 KV_SCAN_MODE scan_mode,
			 int side, unsigned char *windowdata);

/* Prototypes in kvs1025_low.c */

SANE_Status kv_enum_devices (void);
void kv_get_devices_list (const SANE_Device *** devices_list);
void kv_exit (void);
SANE_Status kv_open (PKV_DEV dev);
SANE_Bool kv_already_open (PKV_DEV dev);
SANE_Status kv_open_by_name (SANE_String_Const devicename,
			     SANE_Handle * handle);
void kv_close (PKV_DEV dev);
SANE_Status kv_send_command (PKV_DEV dev,
			     PKV_CMD_HEADER header,
			     PKV_CMD_RESPONSE response);

/* Commands */

SANE_Status CMD_test_unit_ready (PKV_DEV dev, SANE_Bool * ready);
SANE_Status CMD_read_support_info (PKV_DEV dev);
SANE_Status CMD_scan (PKV_DEV dev);
SANE_Status CMD_set_window (PKV_DEV dev, int side, PKV_CMD_RESPONSE rs);
SANE_Status CMD_reset_window (PKV_DEV dev);
SANE_Status CMD_get_buff_status (PKV_DEV dev, int *front_size,
				 int *back_size);
SANE_Status CMD_wait_buff_status (PKV_DEV dev, int *front_size,
				  int *back_size);
SANE_Status CMD_read_pic_elements (PKV_DEV dev, int page, int side,
				   int *width, int *height);
SANE_Status CMD_read_image (PKV_DEV dev, int page, int side,
			    unsigned char *buffer, int *psize,
			    KV_CMD_RESPONSE * rs);
SANE_Status CMD_wait_document_existanse (PKV_DEV dev);
SANE_Status CMD_get_document_existanse (PKV_DEV dev);
SANE_Status CMD_set_timeout (PKV_DEV dev, SANE_Word timeout);
SANE_Status CMD_request_sense (PKV_DEV dev);
/* Scan routines */

SANE_Status AllocateImageBuffer (PKV_DEV dev);
SANE_Status ReadImageDataSimplex (PKV_DEV dev, int page);
SANE_Status ReadImageDataDuplex (PKV_DEV dev, int page);
SANE_Status ReadImageData (PKV_DEV dev, int page);

SANE_Status buffer_deskew (PKV_DEV dev, int side);
SANE_Status buffer_crop (PKV_DEV dev, int side);
SANE_Status buffer_despeck (PKV_DEV dev, int side);
int buffer_isblank (PKV_DEV dev, int side);
SANE_Status buffer_rotate(PKV_DEV dev, int side);

#endif /* #ifndef __KVS1025_LOW_H */
