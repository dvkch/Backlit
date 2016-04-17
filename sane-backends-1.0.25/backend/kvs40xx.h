#ifndef __KVS40XX_H
#define __KVS40XX_H

/*
   Copyright (C) 2009, Panasonic Russia Ltd.
*/
/*
   Panasonic KV-S40xx USB-SCSI scanner driver.
*/

#include "../include/sane/config.h"
#include <semaphore.h>

#undef  BACKEND_NAME
#define BACKEND_NAME kvs40xx

#define DBG_ERR  1
#define DBG_WARN 2
#define DBG_MSG  3
#define DBG_INFO 4
#define DBG_DBG  5

#define PANASONIC_ID 	0x04da
#define KV_S4085C 	0x100c
#define KV_S4065C 	0x100d
#define KV_S7075C 	0x100e

#define KV_S4085CL 	(KV_S4085C|0x10000)
#define KV_S4085CW 	(KV_S4085C|0x20000)
#define KV_S4065CL 	(KV_S4065C|0x10000)
#define KV_S4065CW 	(KV_S4065C|0x20000)

#define USB	1
#define SCSI	2
#define BULK_HEADER_SIZE	12
#define MAX_READ_DATA_SIZE	(0x10000-0x100)
#define BUF_SIZE MAX_READ_DATA_SIZE

#define INCORRECT_LENGTH 0xfafafafa

typedef unsigned char u8;
typedef unsigned u32;
typedef unsigned short u16;

#define SIDE_FRONT      0x00
#define SIDE_BACK       0x80

/* options */
typedef enum
{
  NUM_OPTS = 0,

  /* General options */
  MODE_GROUP,
  MODE,				/* scanner modes */
  RESOLUTION,			/* X and Y resolution */
  SOURCE,

  DUPLEX,			/* Duplex mode */
  FEEDER_MODE,			/* Feeder mode, fixed to Continous */
  LENGTHCTL,			/* Length control mode */
  LONG_PAPER,
  MANUALFEED,			/* Manual feed mode */
  FEED_TIMEOUT,			/* Feed timeout */
  DBLFEED,			/* Double feed detection mode */
  DFEED_SENCE,
  DFSTOP,
  DFEED_L,
  DFEED_C,
  DFEED_R,
  STAPELED_DOC,			/* Detect stapled document */
  FIT_TO_PAGE,			/* Scanner shrinks image to fit scanned page */

  /* Geometry group */
  GEOMETRY_GROUP,
  PAPER_SIZE,			/* Paper size */
  LANDSCAPE,			/* true if landscape */
  TL_X,				/* upper left X */
  TL_Y,				/* upper left Y */
  BR_X,				/* bottom right X */
  BR_Y,				/* bottom right Y */

  ADVANCED_GROUP,
  BRIGHTNESS,			/* Brightness */
  CONTRAST,			/* Contrast */
  THRESHOLD,			/* Binary threshold */
  AUTOMATIC_THRESHOLD,
  WHITE_LEVEL,
  NOISE_REDUCTION,
  INVERSE,			/* Monochrome reversing */
  IMAGE_EMPHASIS,		/* Image emphasis */
  GAMMA_CORRECTION,		/* Gamma correction */
  LAMP,				/* Lamp -- color drop out */
  RED_CHROMA,
  BLUE_CHROMA,
  HALFTONE_PATTERN,		/* Halftone pattern */
  COMPRESSION,			/* JPEG Compression */
  COMPRESSION_PAR,		/* Compression parameter */
  DESKEW,
  STOP_SKEW,
  CROP,
  MIRROR,
  BTMPOS,
  TOPPOS,

  /* must come last: */
  NUM_OPTIONS
} KV_OPTION;


struct buf
{
  u8 **buf;
  volatile int head;
  volatile int tail;
  volatile unsigned size;
  volatile int sem;
  volatile SANE_Status st;
  pthread_mutex_t mu;
  pthread_cond_t cond;
};

struct scanner
{
  char name[128];
  unsigned id;
  volatile int scanning;
  int page;
  int side;
  int bus;
  SANE_Int file;
  SANE_Option_Descriptor opt[NUM_OPTIONS];
  Option_Value val[NUM_OPTIONS];
  SANE_Parameters params;
  u8 *buffer;
  struct buf buf[2];
  u8 *data;
  unsigned side_size;
  unsigned read;
  pthread_t thread;
};

struct window
{
  u8 reserved[6];
  u8 window_descriptor_block_length[2];

  u8 window_identifier;
  u8 reserved2;
  u8 x_resolution[2];
  u8 y_resolution[2];
  u8 upper_left_x[4];
  u8 upper_left_y[4];
  u8 width[4];
  u8 length[4];
  u8 brightness;
  u8 threshold;
  u8 contrast;
  u8 image_composition;
  u8 bit_per_pixel;
  u8 halftone_pattern[2];
  u8 rif_padding;		/*RIF*/
  u8 bit_ordering[2];
  u8 compression_type;
  u8 compression_argument;
  u8 reserved4[6];

  u8 vendor_unique_identifier;
  u8 nobuf_fstspeed_dfstop;
  u8 mirror_image;
  u8 image_emphasis;
  u8 gamma_correction;
  u8 mcd_lamp_dfeed_sens;
  u8 reserved5;			/*rmoir*/
  u8 document_size;
  u8 document_width[4];
  u8 document_length[4];
  u8 ahead_deskew_dfeed_scan_area_fspeed_rshad;
  u8 continuous_scanning_pages;
  u8 automatic_threshold_mode;
  u8 automatic_separation_mode;
  u8 standard_white_level_mode;
  u8 b_wnr_noise_reduction;
  u8 mfeed_toppos_btmpos_dsepa_hsepa_dcont_rstkr;
  u8 stop_mode;
  u8 red_chroma;
  u8 blue_chroma;
};

struct support_info
{
  /*TODO: */
  unsigned char data[32];
};

void kvs40xx_init_options (struct scanner *);
SANE_Status kvs40xx_test_unit_ready (struct scanner *s);
SANE_Status kvs40xx_set_timeout (struct scanner *s, int timeout);
void kvs40xx_init_window (struct scanner *s, struct window *wnd, int wnd_id);
SANE_Status kvs40xx_set_window (struct scanner *s, int wnd_id);
SANE_Status kvs40xx_reset_window (struct scanner *s);
SANE_Status kvs40xx_read_picture_element (struct scanner *s, unsigned side,
					  SANE_Parameters * p);
SANE_Status read_support_info (struct scanner *s, struct support_info *inf);
SANE_Status kvs40xx_read_image_data (struct scanner *s, unsigned page,
				     unsigned side, void *buf,
				     unsigned max_size, unsigned *size);
SANE_Status kvs40xx_document_exist (struct scanner *s);
SANE_Status get_buffer_status (struct scanner *s, unsigned *data_avalible);
SANE_Status kvs40xx_scan (struct scanner *s);
SANE_Status kvs40xx_sense_handler (int fd, u_char * sense_buffer, void *arg);
SANE_Status stop_adf (struct scanner *s);
SANE_Status hopper_down (struct scanner *s);
SANE_Status inquiry (struct scanner *s, char *id);

static inline u16
swap_bytes16 (u16 x)
{
  return x << 8 | x >> 8;
}
static inline u32
swap_bytes32 (u32 x)
{
  return x << 24 | x >> 24 |
    (x & (u32) 0x0000ff00UL) << 8 | (x & (u32) 0x00ff0000UL) >> 8;
}

#if WORDS_BIGENDIAN
static inline void
set24 (u8 * p, u32 x)
{
  p[2] = x >> 16;
  p[1] = x >> 8;
  p[0] = x >> 0;
}

#define cpu2be16(x) (x)
#define cpu2be32(x) (x)
#define cpu2le16(x) swap_bytes16(x)
#define cpu2le32(x) swap_bytes32(x)
#define le2cpu16(x) swap_bytes16(x)
#define le2cpu32(x) swap_bytes32(x)
#define be2cpu16(x) (x)
#define be2cpu32(x) (x)
#define BIT_ORDERING 0
#elif __BYTE_ORDER == __LITTLE_ENDIAN
static inline void
set24 (u8 * p, u32 x)
{
  p[0] = x >> 16;
  p[1] = x >> 8;
  p[2] = x >> 0;
}

#define cpu2le16(x) (x)
#define cpu2le32(x) (x)
#define cpu2be16(x) swap_bytes16(x)
#define cpu2be32(x) swap_bytes32(x)
#define le2cpu16(x) (x)
#define le2cpu32(x) (x)
#define be2cpu16(x) swap_bytes16(x)
#define be2cpu32(x) swap_bytes32(x)
#define BIT_ORDERING 1
#else
#error __BYTE_ORDER not defined
#endif


static inline u32
get24 (u8 * p)
{
  u32 x = (((u32) p[0]) << 16) | (((u32) p[1]) << 8) | (((u32) p[0]) << 0);
  return x;
}
#endif /*__KVS40XX_H*/
