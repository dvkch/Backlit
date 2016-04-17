/*
 * SANE backend for Xerox Phaser 3200MFP
 * Copyright 2008 ABC <abc@telekom.ru>
 *
 *	Network scanners support
 *	Copyright 2010 Alexander Kuznetsov <acca(at)cpan.org>
 *
 * This program is licensed under GPL + SANE exception.
 * More info at http://www.sane-project.org/license.html
 */

#ifndef xerox_mfp_h
#define xerox_mfp_h

#ifdef __GNUC__
#define UNUSED(x) x __attribute__((unused))
#else
#define UNUSED(x) x
#endif

#define MIN(a,b) (((a) < (b)) ? (a) : (b))

#define UNCONST(ptr) ((void *)(long)(ptr))

#define PNT_PER_MM	(1200. / MM_PER_INCH)

#define PADDING_SIZE	16

#define SWAP_Word(x, y) { SANE_Word z = x; x = y; y = z; }

enum options {
  OPT_NUMOPTIONS,
  OPT_GROUP_STD,
  OPT_RESOLUTION,	/* dpi*/
  OPT_MODE,		/* color */
  OPT_THRESHOLD,	/* brightness */
  OPT_SOURCE,		/* affects max window size */
  OPT_GROUP_GEO,
  OPT_SCAN_TL_X,	/* for (OPT_SCAN_TL_X to OPT_SCAN_BR_Y) */
  OPT_SCAN_TL_Y,
  OPT_SCAN_BR_X,
  OPT_SCAN_BR_Y,
  NUM_OPTIONS
};

typedef struct transport transport;

struct device {
  struct device *next;
  SANE_Device sane;
  int dn;			/* usb file descriptor */
  SANE_Byte res[1024];		/* buffer for responses */
  size_t reslen;		/* response len */
  SANE_Option_Descriptor opt[NUM_OPTIONS];
  Option_Value val[NUM_OPTIONS];
  SANE_Parameters para;
  SANE_Bool non_blocking;
  int scanning;			/* scanning is started */
  int cancel;			/* cancel flag */
  int state;			/* current state */
  int reserved;			/* CMD_RESERVE_UNIT */
  int reading;			/* READ_IMAGE is sent */

  SANE_Byte *data;		/* postprocessing cyclic buffer 64k */
  int datalen;			/* how data in buffer */
  int dataoff;			/* offset of data */
  int dataindex;		/* sequental number */
#define DATAMASK 0xffff		/* mask of data buffer */
#define DATASIZE (DATAMASK + 1)	/* size of data buffer */
  /* 64K will be enough to hold whole line of 2400 dpi of 23cm */
#define DATATAIL(dev) ((dev->dataoff + dev->datalen) & DATAMASK)
#define DATAROOM(dev) dataroom(dev)

  /* data from CMD_INQUIRY: */
  int resolutions;		/* supported resolution bitmask */
  int compositions;		/* supported image compositions bitmask */
  int max_len;			/* effective max len for current doc source */
  int max_win_width;
  int max_win_len;
  int max_len_adf;
  int max_len_fb;
  int line_order;		/* if need post processing */
  SANE_Word dpi_list[30];	/* allowed resolutions */
  int doc_loaded;

  SANE_Range win_x_range;
  SANE_Range win_y_range;

  /* CMD_SET_WINDOW parameters we set: */
  int win_width;		/* in 1200dpi points */
  int win_len;
  double win_off_x;		/* in inches (byte.byte) */
  double win_off_y;
  int resolution;		/* dpi indexed values */
  int composition;		/* MODE_ */
  int doc_source;		/* document source */
  int threshold;		/* brightness */

  /* CMD_READ data. It is per block only, image could be in many blocks */
  int blocklen;			/* image data block len (padding incl.) */
  int vertical;			/* lines in block (padded) */
  int horizontal;		/* b/w: bytes, gray/color: pixels (padded) */
  int final_block;
  int pixels_per_line;
  int bytes_per_line;
  int ulines;			/* up to this block including */
  int y_off;			/* up to this block excluding*/
  int blocks;

  /* stat */
  int total_img_size;		/* predicted image size */
  int total_out_size;		/* total we sent to user */
  int total_data_size;		/* total of what scanner sent us */

  /* transport to use */
  transport *io;
};


/*	Transport abstract layer	*/
struct transport {
    char* ttype;

    int (*dev_request) (struct device *dev,
		SANE_Byte *cmd, size_t cmdlen,
		SANE_Byte *resp, size_t *resplen);
    SANE_Status (*dev_open) (struct device *dev);
    void (*dev_close) (struct device *dev);
    SANE_Status (*configure_device) (const char *devname, SANE_Status (*cb)(SANE_String_Const devname));
};

/*	USB transport	*/
int		usb_dev_request	(struct device *dev, SANE_Byte *cmd, size_t cmdlen, SANE_Byte *resp, size_t *resplen);
SANE_Status	usb_dev_open	(struct device *dev);
void		usb_dev_close	(struct device *dev);
SANE_Status	usb_configure_device (const char *devname, SANE_Status (*cb)(SANE_String_Const devname));

/*	TCP unicast	*/
int		tcp_dev_request	(struct device *dev, SANE_Byte *cmd, size_t cmdlen, SANE_Byte *resp, size_t *resplen);
SANE_Status	tcp_dev_open	(struct device *dev);
void		tcp_dev_close	(struct device *dev);
SANE_Status	tcp_configure_device (const char *devname, SANE_Status (*cb)(SANE_String_Const devname));

/* device wants transfer buffer to be multiple of 512 */
#define USB_BLOCK_SIZE 512
#define USB_BLOCK_MASK ~(USB_BLOCK_SIZE - 1)

static inline int dataroom(struct device *dev) {
  int tail = DATATAIL(dev);
  if (tail < dev->dataoff)
    return dev->dataoff - tail;
  else if (dev->datalen == DATASIZE) {
    return 0;
  } else
    return DATASIZE - tail;
}

/*	Functions from original xerox_mfp.c, used in -usb.c and -tcp.c	*/
SANE_Status ret_cancel(struct device *dev, SANE_Status ret);

/* a la SCSI commands. */ /* request len, response len, exception */
#define CMD_ABORT		0x06	/*  4, 32 */
#define CMD_INQUIRY		0x12	/*  4, 70 */
#define CMD_RESERVE_UNIT	0x16	/*  4, 32 */
#define CMD_RELEASE_UNIT	0x17	/*  4, 32 */
#define CMD_SET_WINDOW		0x24	/* 25, 32, specified req len is 22 */
#define CMD_READ		0x28	/*  4, 32 */
#define CMD_READ_IMAGE		0x29	/*  4, var + padding[16] */
#define CMD_OBJECT_POSITION	0x31	/*  4, 32 */

/* Packet Headers */
#define REQ_CODE_A		0x1b
#define REQ_CODE_B		0xa8
#define RES_CODE		0xa8

/* Status Codes, going into dev->state */
#define STATUS_GOOD		0x00
#define STATUS_CHECK		0x02	/* MSG_SCANNER_STATE */
#define STATUS_CANCEL		0x04
#define STATUS_BUSY		0x08

/* Message Code */
#define MSG_NO_MESSAGE		0x00
#define MSG_PRODUCT_INFO	0x10	/* CMD_INQUIRY */
#define MSG_SCANNER_STATE	0x20	/* CMD_RESERVE_UNIT, and
					   CMD_READ, CMD_SET_WINDOW, CMD_OBJECT_POSITION */
#define MSG_SCANNING_PARAM	0x30	/* CMD_SET_WINDOW */
#define MSG_PREVIEW_PARAM	0x31	/* CMD_SET_WINDOW */
#define MSG_LINK_BLOCK		0x80	/* CMD_READ */
#define MSG_END_BLOCK		0x81	/* CMD_READ */

/* Scanner State Bits (if MSG_SCANNER_STATE if STATUS_CHECK) */
#define STATE_NO_ERROR		0x001
#define STATE_COMMAND_ERROR	0x002
#define STATE_UNSUPPORTED	0x004
#define STATE_RESET		0x008
#define STATE_NO_DOCUMENT	0x010
#define STATE_DOCUMENT_JAM	0x020
#define STATE_COVER_OPEN	0x040
#define STATE_WARMING		0x080
#define STATE_LOCKING		0x100
#define STATE_INVALID_AREA	0x200
#define STATE_RESOURCE_BUSY	0x400

/* Image Composition */
#define MODE_LINEART		0x00
#define MODE_HALFTONE		0x01
#define MODE_GRAY8		0x03
#define MODE_RGB24		0x05

/* Document Source */
#define DOC_ADF			0x20
#define DOC_FLATBED		0x40
#define DOC_AUTO		0x80

#endif	/* xerox_mfp_h	*/
