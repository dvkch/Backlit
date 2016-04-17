#ifndef __KVS20XX_H
#define __KVS20XX_H

/*
   Copyright (C) 2008, Panasonic Russia Ltd.
   Copyright (C) 2010, m. allan noah
*/
/*
   Panasonic KV-S20xx USB-SCSI scanners.
*/

#include <sys/param.h>

#undef  BACKEND_NAME
#define BACKEND_NAME kvs20xx

#define DBG_ERR  1
#define DBG_WARN 2
#define DBG_MSG  3
#define DBG_INFO 4
#define DBG_DBG  5

#define PANASONIC_ID 	0x04da
#define KV_S2025C 	0xdeadbeef
#define KV_S2045C 	0xdeadbeee
#define KV_S2026C 	0x1000
#define KV_S2046C 	0x1001
#define KV_S2048C 	0x1009
#define KV_S2028C 	0x100a
#define USB	1
#define SCSI	2
#define MAX_READ_DATA_SIZE 0x10000
#define BULK_HEADER_SIZE	12

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

  DUPLEX,			/* Duplex mode */
  FEEDER_MODE,			/* Feeder mode, fixed to Continous */
  LENGTHCTL,			/* Length control mode */
  MANUALFEED,			/* Manual feed mode */
  FEED_TIMEOUT,			/* Feed timeout */
  DBLFEED,			/* Double feed detection mode */
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
  IMAGE_EMPHASIS,		/* Image emphasis */
  GAMMA_CORRECTION,		/* Gamma correction */
  LAMP,				/* Lamp -- color drop out */
  /* must come last: */
  NUM_OPTIONS
} KV_OPTION;

#ifndef SANE_OPTION
typedef union
{
  SANE_Bool b;		/**< bool */
  SANE_Word w;		/**< word */
  SANE_Word *wa;	/**< word array */
  SANE_String s;	/**< string */
}
Option_Value;
#define SANE_OPTION 1
#endif

struct scanner
{
  unsigned id;
  int scanning;
  int page;
  int side;
  int bus;
  SANE_Int file;
  SANE_Option_Descriptor opt[NUM_OPTIONS];
  Option_Value val[NUM_OPTIONS];
  SANE_Parameters params;
  u8 *buffer;
  u8 *data;
  unsigned side_size;
  unsigned read;
  unsigned dummy_size;
  unsigned saved_dummy_size;
};

struct window
{
  u8 reserved[6];
  u16 window_descriptor_block_length;

  u8 window_identifier;
  u8 reserved2;
  u16 x_resolution;
  u16 y_resolution;
  u32 upper_left_x;
  u32 upper_left_y;
  u32 width;
  u32 length;
  u8 brightness;
  u8 threshold;
  u8 contrast;
  u8 image_composition;
  u8 bit_per_pixel;
  u16 halftone_pattern;
  u8 reserved3;
  u16 bit_ordering;
  u8 compression_type;
  u8 compression_argument;
  u8 reserved4[6];

  u8 vendor_unique_identifier;
  u8 nobuf_fstspeed_dfstop;
  u8 mirror_image;
  u8 image_emphasis;
  u8 gamma_correction;
  u8 mcd_lamp_dfeed_sens;
  u8 reserved5;
  u8 document_size;
  u32 document_width;
  u32 document_length;
  u8 ahead_deskew_dfeed_scan_area_fspeed_rshad;
  u8 continuous_scanning_pages;
  u8 automatic_threshold_mode;
  u8 automatic_separation_mode;
  u8 standard_white_level_mode;
  u8 b_wnr_noise_reduction;
  u8 mfeed_toppos_btmpos_dsepa_hsepa_dcont_rstkr;
  u8 stop_mode;
} __attribute__((__packed__));

void kvs20xx_init_options (struct scanner *);
void kvs20xx_init_window (struct scanner *s, struct window *wnd, int wnd_id);

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

#if __BYTE_ORDER == __BIG_ENDIAN
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

#endif /*__KVS20XX_H*/
