/*
 * SANE backend for Xerox Phaser 3200MFP
 * Copyright 2008 ABC <abc@telekom.ru>
 *
 *	Network Scanners Support
 *	Copyright 2010 Alexander Kuznetsov <acca(at)cpan.org>
 *
 * This program is licensed under GPL + SANE exception.
 * More info at http://www.sane-project.org/license.html
 */

#define DEBUG_NOT_STATIC
#define BACKEND_NAME xerox_mfp

#include "../include/sane/config.h"
#include "../include/lassert.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_thread.h"
#include "../include/sane/sanei_usb.h"
#include "../include/sane/sanei_config.h"
#include "../include/sane/sanei_backend.h"
#include "xerox_mfp.h"

#define BACKEND_BUILD 13
#define XEROX_CONFIG_FILE "xerox_mfp.conf"

static const SANE_Device **devlist = NULL;	/* sane_get_devices array */
static struct device *devices_head = NULL;	/* sane_get_devices list */

enum { TRANSPORT_USB, TRANSPORT_TCP, TRANSPORTS_MAX };
transport available_transports[TRANSPORTS_MAX] = {
    { "usb", usb_dev_request, usb_dev_open, usb_dev_close, usb_configure_device },
    { "tcp", tcp_dev_request, tcp_dev_open, tcp_dev_close, tcp_configure_device },
};

static int resolv_state(int state)
{
  if (state & STATE_DOCUMENT_JAM)
    return SANE_STATUS_JAMMED;
  if (state & STATE_NO_DOCUMENT)
    return SANE_STATUS_NO_DOCS;
  if (state & STATE_COVER_OPEN)
    return SANE_STATUS_COVER_OPEN;
  if (state & STATE_INVALID_AREA)
    return SANE_STATUS_INVAL; /* sane_start: implies SANE_INFO_RELOAD_OPTIONS */
  if (state & STATE_WARMING)
#ifdef SANE_STATUS_WARMING_UP
    return SANE_STATUS_WARMING_UP;
#else
    return SANE_STATUS_DEVICE_BUSY;
#endif
  if (state & STATE_LOCKING)
#ifdef SANE_STATUS_HW_LOCKED
    return SANE_STATUS_HW_LOCKED;
#else
    return SANE_STATUS_JAMMED;
#endif
  if (state & ~STATE_NO_ERROR)
    return SANE_STATUS_DEVICE_BUSY;
  return 0;
}

static char *str_cmd(int cmd)
{
  switch (cmd) {
    case CMD_ABORT:		return "ABORT";
    case CMD_INQUIRY:		return "INQUIRY";
    case CMD_RESERVE_UNIT:	return "RESERVE_UNIT";
    case CMD_RELEASE_UNIT:	return "RELEASE_UNIT";
    case CMD_SET_WINDOW:	return "SET_WINDOW";
    case CMD_READ:		return "READ";
    case CMD_READ_IMAGE:	return "READ_IMAGE";
    case CMD_OBJECT_POSITION:	return "OBJECT_POSITION";
  }
  return "unknown";
}

#define MAX_DUMP 70
static void dbg_dump(struct device *dev)
{
  int i;
  char dbuf[MAX_DUMP * 3 + 1], *dptr = dbuf;
  int nzlen = dev->reslen;
  int dlen = MIN(dev->reslen, MAX_DUMP);

  for (i = dev->reslen - 1; i >= 0; i--, nzlen--)
    if (dev->res[i] != 0)
      break;

  dlen = MIN(dlen, nzlen + 1);

  for (i = 0; i < dlen; i++, dptr += 3)
    sprintf(dptr, " %02x", dev->res[i]);

  DBG (5, "[%lu]%s%s\n", (u_long)dev->reslen, dbuf,
       (dlen < (int)dev->reslen)? "..." : "");
}

/* one command to device */
/* return 0: on error, 1: success */
static int dev_command (struct device *dev, SANE_Byte * cmd, size_t reqlen)
{
  SANE_Status status;
  size_t sendlen = cmd[3] + 4;
  SANE_Byte *res = dev->res;


  assert (reqlen <= sizeof (dev->res));	/* requested len */
  dev->reslen = sizeof (dev->res);	/* doing full buffer to flush stalled commands */

  if (cmd[2] == CMD_SET_WINDOW) {
    /* Set Window have wrong packet length, huh. */
    sendlen = 25;
  }

  if (cmd[2] == CMD_READ_IMAGE) {
    /* Read Image is raw data, don't need to read response */
    res = NULL;
  }

  dev->state = 0;
  DBG (4, ":: dev_command(%s[%#x], %lu)\n", str_cmd(cmd[2]), cmd[2],
       (u_long)reqlen);
  status = dev->io->dev_request(dev, cmd, sendlen, res, &dev->reslen);
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "%s: dev_request: %s\n", __FUNCTION__, sane_strstatus (status));
    dev->state = SANE_STATUS_IO_ERROR;
    return 0;
  }

  if (!res) {
    /* if not need response just return success */
    return 1;
  }

  /* normal command reply, some sanity checking */
  if (dev->reslen < reqlen) {
    DBG (1, "%s: illegal response len %lu, need %lu\n",
	 __FUNCTION__, (u_long)dev->reslen, (u_long)reqlen);
    dev->state = SANE_STATUS_IO_ERROR;
    return 0;
  } else {
    size_t pktlen;		/* len specified in packet */

    if (DBG_LEVEL > 3)
      dbg_dump(dev);

    if (dev->res[0] != RES_CODE) {
      DBG (2, "%s: illegal data header %02x\n", __FUNCTION__, dev->res[0]);
      dev->state = SANE_STATUS_IO_ERROR;
      return 0;
    }
    pktlen = dev->res[2] + 3;
    if (dev->reslen != pktlen) {
      DBG (2, "%s: illegal response len %lu, should be %lu\n",
	   __FUNCTION__, (u_long)pktlen, (u_long)dev->reslen);
      dev->state = SANE_STATUS_IO_ERROR;
      return 0;
    }
    if (dev->reslen > reqlen)
      DBG (2, "%s: too big packet len %lu, need %lu\n",
	   __FUNCTION__, (u_long)dev->reslen, (u_long)reqlen);
  }

  dev->state = 0;
  if (cmd[2] == CMD_SET_WINDOW ||
      cmd[2] == CMD_OBJECT_POSITION ||
      cmd[2] == CMD_READ ||
      cmd[2] == CMD_RESERVE_UNIT) {
    if (dev->res[1] == STATUS_BUSY)
      dev->state = SANE_STATUS_DEVICE_BUSY;
    else if (dev->res[1] == STATUS_CANCEL)
      dev->state = SANE_STATUS_CANCELLED;
    else if (dev->res[1] == STATUS_CHECK)
      dev->state = resolv_state((cmd[2] == CMD_READ)?
				(dev->res[12] << 8 | dev->res[13]) :
				(dev->res[4] << 8 | dev->res[5]));

    if (dev->state)
      DBG (3, "%s(%s[%#x]): => %d: %s\n",
	   __FUNCTION__, str_cmd(cmd[2]), cmd[2],
	   dev->state, sane_strstatus(dev->state));
  }

  return 1;
}

/* one short command to device */
static int dev_cmd (struct device *dev, SANE_Byte command)
{
  SANE_Byte cmd[4] = { REQ_CODE_A, REQ_CODE_B };
  cmd[2] = command;
  return dev_command (dev, cmd, (command == CMD_INQUIRY)? 70 : 32);
}

/* stop scanning operation. return previous status */
static SANE_Status dev_stop(struct device *dev)
{
  int state = dev->state;

  DBG (3, "%s: %p, scanning %d, reserved %d\n", __FUNCTION__,
       (void *)dev, dev->scanning, dev->reserved);
  dev->scanning = 0;

  /* release */
  if (!dev->reserved)
   return state;
  dev->reserved = 0;
  dev_cmd(dev, CMD_RELEASE_UNIT);
  DBG (3, "total image %d*%d size %d (win %d*%d), %d*%d %d data: %d, out %d bytes\n",
       dev->para.pixels_per_line, dev->para.lines,
       dev->total_img_size,
       dev->win_width, dev->win_len,
       dev->pixels_per_line, dev->ulines, dev->blocks,
       dev->total_data_size, dev->total_out_size);
  dev->state = state;
  return state;
}

SANE_Status ret_cancel(struct device *dev, SANE_Status ret)
{
  dev_cmd(dev, CMD_ABORT);
  if (dev->scanning) {
    dev_stop(dev);
    dev->state = SANE_STATUS_CANCELLED;
  }
  return ret;
}

static int cancelled(struct device *dev)
{
  if (dev->cancel)
    return ret_cancel(dev, 1);
  return 0;
}

/* issue command and wait until scanner is not busy */
/* return 0 on error/blocking, 1 is ok and ready */
static int dev_cmd_wait(struct device *dev, int cmd)
{
  int sleeptime = 10;

  do {
    if (cancelled(dev))
      return 0;
    if (!dev_cmd(dev, cmd)) {
      dev->state = SANE_STATUS_IO_ERROR;
      return 0;
    } else if (dev->state) {
      if (dev->state != SANE_STATUS_DEVICE_BUSY)
	return 0;
      else {
	if (dev->non_blocking) {
	  dev->state = SANE_STATUS_GOOD;
	  return 0;
	} else {
	  if (sleeptime > 1000)
	    sleeptime = 1000;
	  DBG (4, "(%s) sleeping(%d ms).. [%x %x]\n",
	       str_cmd(cmd), sleeptime, dev->res[4], dev->res[5]);
	  usleep(sleeptime * 1000);
	  if (sleeptime < 1000)
	    sleeptime *= (sleeptime < 100)? 10 : 2;
	}
      } /* BUSY */
    }
  } while (dev->state == SANE_STATUS_DEVICE_BUSY);

  return 1;
}

static int inq_dpi_bits[] = {
  75, 150, 0, 0,
  200, 300, 0, 0, 
  600, 0, 0, 1200,
  100, 0, 0, 2400,
  0, 4800, 0, 9600
};

static int res_dpi_codes[] = {
  75, 0, 150, 0,
  0, 300, 0, 600,
  1200, 200, 100, 2400,
  4800, 9600
};

static int SANE_Word_sort(const void * a, const void * b)
{
  return *(const SANE_Word *)a - *(const SANE_Word *)b;
}

/* resolve inquired dpi list to dpi_list array */
static void resolv_inq_dpi(struct device *dev)
{
  unsigned int i;
  int res = dev->resolutions;

  assert(sizeof(inq_dpi_bits) < sizeof(dev->dpi_list));
  for (i = 0; i < sizeof(inq_dpi_bits) / sizeof(int); i++)
    if (inq_dpi_bits[i] && (res & (1 << i)))
      dev->dpi_list[++dev->dpi_list[0]] = inq_dpi_bits[i];
  qsort(&dev->dpi_list[1], dev->dpi_list[0], sizeof(SANE_Word), SANE_Word_sort);
}

static unsigned int dpi_to_code(int dpi)
{
  unsigned int i;

  for (i = 0; i < sizeof(res_dpi_codes) / sizeof(int); i++) {
    if (dpi == res_dpi_codes[i])
      return i;
  }
  return 0;
}

static int string_match_index(const SANE_String_Const s[], SANE_String m)
{
  int i;

  for (i = 0; *s; i++) {
    SANE_String_Const x = *s++;
    if (strcasecmp(x, m) == 0)
      return i;
  }
  return 0;
}

static SANE_String string_match(const SANE_String_Const s[], SANE_String m)
{
  return UNCONST(s[string_match_index(s, m)]);
}

static size_t max_string_size (SANE_String_Const s[])
{
  size_t max = 0;

  while (*s) {
    size_t size = strlen(*s++) + 1;
    if (size > max)
      max = size;
  }
  return max;
}

static SANE_String_Const doc_sources[] = {
  "Flatbed", "ADF", "Auto", NULL
};

static int doc_source_to_code[] = {
  0x40, 0x20, 0x80
};

static SANE_String_Const scan_modes[] = {
  SANE_VALUE_SCAN_MODE_LINEART,
  SANE_VALUE_SCAN_MODE_HALFTONE,
  SANE_VALUE_SCAN_MODE_GRAY,
  SANE_VALUE_SCAN_MODE_COLOR,
  NULL
};

static int scan_mode_to_code[] = {
  0x00, 0x01, 0x03, 0x05
};

static SANE_Range threshold = {
  SANE_FIX(30), SANE_FIX(70), SANE_FIX(10)
};

static void reset_options(struct device *dev)
{
  dev->val[OPT_RESOLUTION].w = 150;
  dev->val[OPT_MODE].s = string_match(scan_modes, SANE_VALUE_SCAN_MODE_COLOR);

  /* if docs loaded in adf use it as default source, flatbed oterwise */
  dev->val[OPT_SOURCE].s = UNCONST(doc_sources[(dev->doc_loaded)? 1 : 0]);

  dev->val[OPT_THRESHOLD].w = SANE_FIX(50);

  /* this is reported maximum window size, will be fixed later */
  dev->win_x_range.min = SANE_FIX(0);
  dev->win_x_range.max = SANE_FIX((double)dev->max_win_width / PNT_PER_MM);
  dev->win_x_range.quant = SANE_FIX(1);
  dev->win_y_range.min = SANE_FIX(0);
  dev->win_y_range.max = SANE_FIX((double)dev->max_win_len / PNT_PER_MM);
  dev->win_y_range.quant = SANE_FIX(1);
  dev->val[OPT_SCAN_TL_X].w = dev->win_x_range.min;
  dev->val[OPT_SCAN_TL_Y].w = dev->win_y_range.min;
  dev->val[OPT_SCAN_BR_X].w = dev->win_x_range.max;
  dev->val[OPT_SCAN_BR_Y].w = dev->win_y_range.max;
}

static void init_options(struct device *dev)
{
  int i;

  for (i = 0; i < NUM_OPTIONS; i++) {
    dev->opt[i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    dev->opt[i].size = sizeof(SANE_Word);
    dev->opt[i].type = SANE_TYPE_FIXED;
    dev->val[i].s = NULL;
  }

  dev->opt[OPT_NUMOPTIONS].name = SANE_NAME_NUM_OPTIONS;
  dev->opt[OPT_NUMOPTIONS].title = SANE_TITLE_NUM_OPTIONS;
  dev->opt[OPT_NUMOPTIONS].desc = SANE_DESC_NUM_OPTIONS;
  dev->opt[OPT_NUMOPTIONS].type = SANE_TYPE_INT;
  dev->opt[OPT_NUMOPTIONS].cap = SANE_CAP_SOFT_DETECT;
  dev->val[OPT_NUMOPTIONS].w = NUM_OPTIONS;

  dev->opt[OPT_GROUP_STD].name = SANE_NAME_STANDARD;
  dev->opt[OPT_GROUP_STD].title = SANE_TITLE_STANDARD;
  dev->opt[OPT_GROUP_STD].desc = SANE_DESC_STANDARD;
  dev->opt[OPT_GROUP_STD].type = SANE_TYPE_GROUP;
  dev->opt[OPT_GROUP_STD].cap = 0;

  dev->opt[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  dev->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  dev->opt[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  dev->opt[OPT_RESOLUTION].type = SANE_TYPE_INT;
  dev->opt[OPT_RESOLUTION].cap = SANE_CAP_SOFT_SELECT|SANE_CAP_SOFT_DETECT;
  dev->opt[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
  dev->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  dev->opt[OPT_RESOLUTION].constraint.word_list = dev->dpi_list;

  dev->opt[OPT_MODE].name = SANE_NAME_SCAN_MODE;
  dev->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  dev->opt[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
  dev->opt[OPT_MODE].type = SANE_TYPE_STRING;
  dev->opt[OPT_MODE].size = max_string_size(scan_modes);
  dev->opt[OPT_MODE].cap = SANE_CAP_SOFT_SELECT|SANE_CAP_SOFT_DETECT;
  dev->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  dev->opt[OPT_MODE].constraint.string_list = scan_modes;

  dev->opt[OPT_THRESHOLD].name = SANE_NAME_HIGHLIGHT;
  dev->opt[OPT_THRESHOLD].title = SANE_TITLE_THRESHOLD;
  dev->opt[OPT_THRESHOLD].desc = SANE_DESC_THRESHOLD;
  dev->opt[OPT_THRESHOLD].unit = SANE_UNIT_PERCENT;
  dev->opt[OPT_THRESHOLD].cap = SANE_CAP_SOFT_SELECT|SANE_CAP_SOFT_DETECT;
  dev->opt[OPT_THRESHOLD].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_THRESHOLD].constraint.range = &threshold;

  dev->opt[OPT_SOURCE].name = SANE_NAME_SCAN_SOURCE;
  dev->opt[OPT_SOURCE].title = SANE_TITLE_SCAN_SOURCE;
  dev->opt[OPT_SOURCE].desc = SANE_DESC_SCAN_SOURCE;
  dev->opt[OPT_SOURCE].type = SANE_TYPE_STRING;
  dev->opt[OPT_SOURCE].size = max_string_size(doc_sources);
  dev->opt[OPT_SOURCE].cap = SANE_CAP_SOFT_SELECT|SANE_CAP_SOFT_DETECT;
  dev->opt[OPT_SOURCE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  dev->opt[OPT_SOURCE].constraint.string_list = doc_sources;

  dev->opt[OPT_GROUP_GEO].name = SANE_NAME_GEOMETRY;
  dev->opt[OPT_GROUP_GEO].title = SANE_TITLE_GEOMETRY;
  dev->opt[OPT_GROUP_GEO].desc = SANE_DESC_GEOMETRY;
  dev->opt[OPT_GROUP_GEO].type = SANE_TYPE_GROUP;
  dev->opt[OPT_GROUP_GEO].cap = 0;

  dev->opt[OPT_SCAN_TL_X].name = SANE_NAME_SCAN_TL_X;
  dev->opt[OPT_SCAN_TL_X].title = SANE_TITLE_SCAN_TL_X;
  dev->opt[OPT_SCAN_TL_X].desc = SANE_DESC_SCAN_TL_X;
  dev->opt[OPT_SCAN_TL_X].unit = SANE_UNIT_MM;
  dev->opt[OPT_SCAN_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_SCAN_TL_X].constraint.range = &dev->win_x_range;

  dev->opt[OPT_SCAN_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  dev->opt[OPT_SCAN_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  dev->opt[OPT_SCAN_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  dev->opt[OPT_SCAN_TL_Y].unit = SANE_UNIT_MM;
  dev->opt[OPT_SCAN_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_SCAN_TL_Y].constraint.range = &dev->win_y_range;

  dev->opt[OPT_SCAN_BR_X].name = SANE_NAME_SCAN_BR_X;
  dev->opt[OPT_SCAN_BR_X].title = SANE_TITLE_SCAN_BR_X;
  dev->opt[OPT_SCAN_BR_X].desc = SANE_DESC_SCAN_BR_X;
  dev->opt[OPT_SCAN_BR_X].unit = SANE_UNIT_MM;
  dev->opt[OPT_SCAN_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_SCAN_BR_X].constraint.range = &dev->win_x_range;

  dev->opt[OPT_SCAN_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  dev->opt[OPT_SCAN_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  dev->opt[OPT_SCAN_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  dev->opt[OPT_SCAN_BR_Y].unit = SANE_UNIT_MM;
  dev->opt[OPT_SCAN_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  dev->opt[OPT_SCAN_BR_Y].constraint.range = &dev->win_y_range;
}

/* fill parameters from options */
static void set_parameters(struct device *dev)
{
  double px_to_len;

  dev->para.last_frame = SANE_TRUE;
  dev->para.lines = -1;
  px_to_len = 1200.0 / dev->val[OPT_RESOLUTION].w;
#define BETTER_BASEDPI 1
  /* tests prove that 1200dpi base is very inexact
   * so I calculated better values for each axis */
#if BETTER_BASEDPI 
  px_to_len = 1180.0 / dev->val[OPT_RESOLUTION].w;
#endif
  dev->para.pixels_per_line = dev->win_width / px_to_len;
  dev->para.bytes_per_line = dev->para.pixels_per_line;
#if BETTER_BASEDPI
  px_to_len = 1213.9 / dev->val[OPT_RESOLUTION].w;
#endif
  dev->para.lines = dev->win_len / px_to_len;
  if (dev->composition == MODE_LINEART ||
      dev->composition == MODE_HALFTONE) {
    dev->para.format = SANE_FRAME_GRAY;
    dev->para.depth = 1;
    dev->para.bytes_per_line = (dev->para.pixels_per_line + 7) / 8;
  } else if (dev->composition == MODE_GRAY8) {
    dev->para.format = SANE_FRAME_GRAY;
    dev->para.depth = 8;
    dev->para.bytes_per_line = dev->para.pixels_per_line;
  } else if (dev->composition == MODE_RGB24) {
    dev->para.format = SANE_FRAME_RGB;
    dev->para.depth = 8;
    dev->para.bytes_per_line *= 3;
  } else {
    /* this will never happen */
    DBG (1, "%s: impossible image composition %d\n",
	 __FUNCTION__, dev->composition);
    dev->para.format = SANE_FRAME_GRAY;
    dev->para.depth = 8;
  }
}

/* resolve all options related to scan window */
/* called after option changed and in set_window */
static int fix_window(struct device *dev)
{
  double win_width_mm, win_len_mm;
  int i;
  int threshold = SANE_UNFIX(dev->val[OPT_THRESHOLD].w);

  dev->resolution = dpi_to_code(dev->val[OPT_RESOLUTION].w);
  dev->composition = scan_mode_to_code[string_match_index(scan_modes, dev->val[OPT_MODE].s)];

  if (dev->composition == MODE_LINEART ||
      dev->composition == MODE_HALFTONE) {
    dev->opt[OPT_THRESHOLD].cap &= ~SANE_CAP_INACTIVE;
  } else {
    dev->opt[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
  }
  if (threshold < 30) {
    dev->val[OPT_THRESHOLD].w = SANE_FIX(30);
  } else if (threshold > 70) {
    dev->val[OPT_THRESHOLD].w = SANE_FIX(70);
  }
  threshold = SANE_UNFIX(dev->val[OPT_THRESHOLD].w);
  dev->threshold = (threshold - 30) / 10;
  dev->val[OPT_THRESHOLD].w = SANE_FIX(dev->threshold * 10 + 30);

  dev->doc_source = doc_source_to_code[string_match_index(doc_sources, dev->val[OPT_SOURCE].s)];

  /* max window len is dependent of document source */
  if (dev->doc_source == DOC_FLATBED ||
      (dev->doc_source == DOC_AUTO && !dev->doc_loaded))
    dev->max_len = dev->max_len_fb;
  else
    dev->max_len = dev->max_len_adf;

  /* parameters */
  dev->win_y_range.max = SANE_FIX((double)dev->max_len / PNT_PER_MM);

  /* window sanity checking */
  for (i = OPT_SCAN_TL_X; i <= OPT_SCAN_BR_Y; i++) {
    if (dev->val[i].w < dev->opt[i].constraint.range->min)
      dev->val[i].w = dev->opt[i].constraint.range->min;
    if (dev->val[i].w > dev->opt[i].constraint.range->max)
      dev->val[i].w = dev->opt[i].constraint.range->max;
  }

  if (dev->val[OPT_SCAN_TL_X].w > dev->val[OPT_SCAN_BR_X].w)
    SWAP_Word(dev->val[OPT_SCAN_TL_X].w, dev->val[OPT_SCAN_BR_X].w);
  if (dev->val[OPT_SCAN_TL_Y].w > dev->val[OPT_SCAN_BR_Y].w)
    SWAP_Word(dev->val[OPT_SCAN_TL_Y].w, dev->val[OPT_SCAN_BR_Y].w);

  /* recalculate millimeters to inches */
  dev->win_off_x = SANE_UNFIX(dev->val[OPT_SCAN_TL_X].w) / MM_PER_INCH;
  dev->win_off_y = SANE_UNFIX(dev->val[OPT_SCAN_TL_Y].w) / MM_PER_INCH;

  /* calc win size in mm */
  win_width_mm = SANE_UNFIX(dev->val[OPT_SCAN_BR_X].w) -
		    SANE_UNFIX(dev->val[OPT_SCAN_TL_X].w);
  win_len_mm = SANE_UNFIX(dev->val[OPT_SCAN_BR_Y].w) -
		  SANE_UNFIX(dev->val[OPT_SCAN_TL_Y].w);
  /* convert mm to 1200 dpi points */
  dev->win_width = (int)(win_width_mm * PNT_PER_MM);
  dev->win_len = (int)(win_len_mm * PNT_PER_MM);

  /* don't scan if window is zero size */
  if (!dev->win_width || !dev->win_len) {
    /* "The scan cannot be started with the current set of options." */
    dev->state = SANE_STATUS_INVAL;
    return 0;
  }

  return 1;
}

static int dev_set_window (struct device *dev)
{
  SANE_Byte cmd[0x19] = {
    REQ_CODE_A, REQ_CODE_B, CMD_SET_WINDOW, 0x13, MSG_SCANNING_PARAM
  };

  if (!fix_window(dev))
    return 0;

  cmd[0x05] = dev->win_width >> 24;
  cmd[0x06] = dev->win_width >> 16;
  cmd[0x07] = dev->win_width >> 8;
  cmd[0x08] = dev->win_width;
  cmd[0x09] = dev->win_len >> 24;
  cmd[0x0a] = dev->win_len >> 16;
  cmd[0x0b] = dev->win_len >> 8;
  cmd[0x0c] = dev->win_len;
  cmd[0x0d] = dev->resolution;		/* x */
  cmd[0x0e] = dev->resolution;		/* y */
  cmd[0x0f] = (SANE_Byte)floor(dev->win_off_x);
  cmd[0x10] = (SANE_Byte)((dev->win_off_x - floor(dev->win_off_x)) * 100);
  cmd[0x11] = (SANE_Byte)floor(dev->win_off_y);
  cmd[0x12] = (SANE_Byte)((dev->win_off_y - floor(dev->win_off_y)) * 100);
  cmd[0x13] = dev->composition;
  cmd[0x16] = dev->threshold;
  cmd[0x17] = dev->doc_source;

  DBG (5, "OFF xi: %02x%02x yi: %02x%02x,"
       " WIN xp: %02x%02x%02x%02x yp %02x%02x%02x%02x,"
       " MAX %08x %08x\n",
       cmd[0x0f], cmd[0x10], cmd[0x11], cmd[0x12],
       cmd[0x05], cmd[0x06], cmd[0x07], cmd[0x08],
       cmd[0x09], cmd[0x0a], cmd[0x0b], cmd[0x0c],
       dev->max_win_width, dev->max_win_len);

  return dev_command (dev, cmd, 32);
}

static SANE_Status
dev_inquiry (struct device *dev)
{
  SANE_Byte *ptr;
  SANE_Char *optr, *xptr;

  if (!dev_cmd (dev, CMD_INQUIRY))
    return SANE_STATUS_IO_ERROR;
  ptr = dev->res;
  if (ptr[3] != MSG_PRODUCT_INFO) {
    DBG (1, "%s: illegal INQUIRY response %02x\n", __FUNCTION__, ptr[3]);
    return SANE_STATUS_IO_ERROR;
  }

  /* parse reported manufacturer/product names */
  dev->sane.vendor = optr = (SANE_Char *) malloc (33);
  for (ptr += 4; ptr < &dev->res[0x24] && *ptr && *ptr != ' ';)
    *optr++ = *ptr++;
  *optr++ = 0;

  for (; ptr < &dev->res[0x24] && (!*ptr || *ptr == ' '); ptr++)
    /* skip spaces */;

  dev->sane.model = optr = (SANE_Char *) malloc (33);
  xptr = optr;			/* is last non space character + 1 */
  for (; ptr < &dev->res[0x24] && *ptr;) {
    if (*ptr != ' ')
      xptr = optr + 1;
    *optr++ = *ptr++;
  }
  *optr++ = 0;
  *xptr = 0;

  DBG (1, "%s: found %s/%s\n", __FUNCTION__, dev->sane.vendor, dev->sane.model);
  dev->sane.type = strdup ("multi-function peripheral");

  dev->resolutions = dev->res[0x37] << 16 |
    dev->res[0x24] << 8 |
    dev->res[0x25];
  dev->compositions = dev->res[0x27];
  dev->max_win_width = dev->res[0x28] << 24 |
    dev->res[0x29] << 16 |
    dev->res[0x2a] << 8 |
    dev->res[0x2b];
  dev->max_win_len = dev->res[0x2c] << 24 |
    dev->res[0x2d] << 16 |
    dev->res[0x2e] << 8 |
    dev->res[0x2f];
  dev->max_len_adf = dev->res[0x38] << 24 |
    dev->res[0x39] << 16 |
    dev->res[0x3a] << 8 |
    dev->res[0x3b];
  dev->max_len_fb = dev->res[0x3c] << 24 |
    dev->res[0x3d] << 16 |
    dev->res[0x3e] << 8 |
    dev->res[0x3f];
  dev->line_order = dev->res[0x31];
  dev->doc_loaded = (dev->res[0x35] == 0x02) &&
    (dev->res[0x26] & 0x03);

  init_options(dev);
  reset_options(dev);
  fix_window(dev);
  set_parameters(dev);
  resolv_inq_dpi(dev);

  return SANE_STATUS_GOOD;
}

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle h, SANE_Int opt)
{
  struct device *dev = h;

  DBG (3, "%s: %p, %d\n", __FUNCTION__, h, opt);
  if (opt >= NUM_OPTIONS || opt < 0)
    return NULL;
  return &dev->opt[opt];
}

SANE_Status
sane_control_option (SANE_Handle h, SANE_Int opt, SANE_Action act,
		     void *val, SANE_Word * info)
{
  struct device *dev = h;

  DBG (3, "%s: %p, %d, <%d>, %p, %p\n", __FUNCTION__, h, opt, act, val, (void *)info);
  if (!dev || opt >= NUM_OPTIONS || opt < 0)
    return SANE_STATUS_INVAL;

  if (info)
    *info = 0;

  if (act == SANE_ACTION_GET_VALUE) { /* GET */
    if (dev->opt[opt].type == SANE_TYPE_STRING)
      strcpy(val, dev->val[opt].s);
    else
      *(SANE_Word *)val = dev->val[opt].w;
  } else if (act == SANE_ACTION_SET_VALUE) { /* SET */
    SANE_Parameters xpara = dev->para;
    SANE_Option_Descriptor xopt[NUM_OPTIONS];
    Option_Value xval[NUM_OPTIONS];
    int i;

    if (dev->opt[opt].constraint_type == SANE_CONSTRAINT_STRING_LIST) {
      dev->val[opt].s = string_match(dev->opt[opt].constraint.string_list, val);
      if (info && strcasecmp(dev->val[opt].s, val))
	*info |= SANE_INFO_INEXACT;
    } else if (opt == OPT_RESOLUTION)
      dev->val[opt].w = res_dpi_codes[dpi_to_code(*(SANE_Word *)val)];
    else
      dev->val[opt].w = *(SANE_Word *)val;

    memcpy(&xopt, &dev->opt, sizeof(xopt));
    memcpy(&xval, &dev->val, sizeof(xval));
    fix_window(dev);
    set_parameters(dev);

    /* check for side effects */
    if (info) {
      if (memcmp(&xpara, &dev->para, sizeof(xpara)))
	*info |= SANE_INFO_RELOAD_PARAMS;
      if (memcmp(&xopt, &dev->opt, sizeof(xopt)))
	*info |= SANE_INFO_RELOAD_OPTIONS;
      for (i = 0; i < NUM_OPTIONS; i++)
	if (xval[i].w != dev->val[i].w) {
	  if (i == opt)
	    *info |= SANE_INFO_INEXACT;
	  else
	    *info |= SANE_INFO_RELOAD_OPTIONS;
	}
    }
  }

  DBG (4, "%s: %d, <%d> => %08x, %x\n", __FUNCTION__, opt, act,
       val? *(SANE_Word *)val : 0, info? *info : 0);
  return SANE_STATUS_GOOD;
}

static void
dev_free (struct device *dev)
{
  if (!dev)
    return;

  if (dev->sane.name)
    free (UNCONST(dev->sane.name));
  if (dev->sane.vendor)
    free (UNCONST(dev->sane.vendor));
  if (dev->sane.model)
    free (UNCONST(dev->sane.model));
  if (dev->sane.type)
    free (UNCONST(dev->sane.type));
  if (dev->data)
    free(dev->data);
  memset (dev, 0, sizeof (*dev));
  free (dev);
}

static void
free_devices (void)
{
  int i;
  struct device *next;
  struct device *dev;

  if (devlist) {
    free (devlist);
    devlist = NULL;
  }
  for (i = 0, dev = devices_head; dev; dev = next) {
    next = dev->next;
    dev_free (dev);
  }
  devices_head = NULL;
}

static transport *tr_from_devname(SANE_String_Const devname)
{
  if (strncmp("tcp", devname, 3) == 0)
    return &available_transports[TRANSPORT_TCP];
  return &available_transports[TRANSPORT_USB];
}

static SANE_Status
list_one_device (SANE_String_Const devname)
{
  struct device *dev;
  SANE_Status status;
  transport *tr;

  DBG (4, "%s: %s\n", __FUNCTION__, devname);

  for (dev = devices_head; dev; dev = dev->next) {
    if (strcmp (dev->sane.name, devname) == 0)
      return SANE_STATUS_GOOD;
  }

  tr = tr_from_devname(devname);

  dev = calloc (1, sizeof (struct device));
  if (dev == NULL)
    return SANE_STATUS_NO_MEM;

  dev->sane.name = strdup (devname);
  dev->io = tr;
  status = tr->dev_open (dev);
  if (status != SANE_STATUS_GOOD) {
    dev_free (dev);
    return status;
  }

/*  status = dev_cmd (dev, CMD_ABORT);*/
  status = dev_inquiry (dev);
  tr->dev_close (dev);
  if (status != SANE_STATUS_GOOD) {
    DBG (1, "%s: dev_inquiry(%s): %s\n", __FUNCTION__,
	 dev->sane.name, sane_strstatus (status));
    dev_free (dev);
    return status;
  }

  /* good device, add it to list */
  dev->next = devices_head;
  devices_head = dev;
  return SANE_STATUS_GOOD;
}

/* SANE API ignores return code of this callback */
static SANE_Status
list_conf_devices (UNUSED (SANEI_Config * config), const char *devname)
{
  return tr_from_devname(devname)->configure_device(devname, list_one_device);
}

SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback cb)
{
  DBG_INIT ();
  DBG (2, "sane_init: Xerox backend (build %d), version %s null, authorize %s null\n", BACKEND_BUILD,
       (version_code) ? "!=" : "==", (cb) ? "!=" : "==");

  if (version_code)
    *version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR, BACKEND_BUILD);

  sanei_usb_init ();
  return SANE_STATUS_GOOD;
}

void
sane_exit (void)
{
  struct device *dev;

  for (dev = devices_head; dev; dev = dev->next)
    if (dev->dn != -1)
      sane_close(dev); /* implies flush */
  
  free_devices ();
}

SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local)
{
  SANEI_Config config;
  struct device *dev;
  int dev_count;
  int i;

  DBG (3, "%s: %p, %d\n", __FUNCTION__, (const void *)device_list, local);

  if (devlist) {
    if (device_list)
      *device_list = devlist;
    return SANE_STATUS_GOOD;
  }

  free_devices ();

  config.count = 0;
  config.descriptors = NULL;
  config.values = NULL;
  sanei_configure_attach (XEROX_CONFIG_FILE, &config, list_conf_devices);

  for (dev_count = 0, dev = devices_head; dev; dev = dev->next)
    dev_count++;

  devlist = malloc ((dev_count + 1) * sizeof (*devlist));
  if (!devlist)
    {
      DBG (1, "%s: malloc: no memory\n", __FUNCTION__);
      return SANE_STATUS_NO_MEM;
    }

  for (i = 0, dev = devices_head; dev; dev = dev->next)
    devlist[i++] = &dev->sane;
  devlist[i++] = NULL;

  if (device_list)
    *device_list = devlist;
  return SANE_STATUS_GOOD;
}

void
sane_close (SANE_Handle h)
{
  struct device *dev = h;

  if (!dev)
    return;

  DBG (3, "%s: %p (%s)\n", __FUNCTION__, (void *)dev, dev->sane.name);
  dev->io->dev_close(dev);
}

SANE_Status
sane_open (SANE_String_Const name, SANE_Handle * h)
{
  struct device *dev;

  DBG (3, "%s: '%s'\n", __FUNCTION__, name);

  if (!devlist)
    sane_get_devices (NULL, SANE_TRUE);

  if (!name || !*name) {
    /* special case of empty name: open first available device */
    for (dev = devices_head; dev; dev = dev->next) {
      if (dev->dn != -1) {
	if (sane_open (dev->sane.name, h) == SANE_STATUS_GOOD)
	  return SANE_STATUS_GOOD;
      }
    }
  } else {
    for (dev = devices_head; dev; dev = dev->next) {
      if (strcmp(name, dev->sane.name) == 0) {
	*h = dev;
	return dev->io->dev_open(dev);
      }
    }
  }

  return SANE_STATUS_INVAL;
}

SANE_Status
sane_get_parameters (SANE_Handle h, SANE_Parameters * para)
{
  struct device *dev = h;

  DBG (3, "%s: %p, %p\n", __FUNCTION__, h, (void *)para);
  if (!para)
    return SANE_STATUS_INVAL;

  *para = dev->para;
  return SANE_STATUS_GOOD;
}

/* check if image data is ready, and wait if not */
/* 1: image is acquired, 0: error or non_blocking mode */
static int dev_acquire(struct device *dev)
{
  if (!dev_cmd_wait(dev, CMD_READ))
    return dev->state;

  dev->state = SANE_STATUS_GOOD;
  dev->vertical = dev->res[0x08] << 8 | dev->res[0x09];
  dev->horizontal = dev->res[0x0a] << 8 | dev->res[0x0b];
  dev->blocklen = dev->res[4] << 24 |
    dev->res[5] << 16 |
    dev->res[6] << 8 |
    dev->res[7];
  dev->final_block = (dev->res[3] == MSG_END_BLOCK)? 1 : 0;

  dev->pixels_per_line = dev->horizontal;
  dev->bytes_per_line = dev->horizontal;

  if (dev->composition == MODE_RGB24)
    dev->bytes_per_line *= 3;
  else if (dev->composition == MODE_LINEART ||
	   dev->composition == MODE_HALFTONE)
    dev->pixels_per_line *= 8;

  DBG (4, "acquiring, size per band v: %d, h: %d, %sblock: %d, slack: %d\n",
       dev->vertical, dev->horizontal, dev->final_block? "last " : "",
       dev->blocklen, dev->blocklen - (dev->vertical * dev->bytes_per_line));

  if (dev->bytes_per_line > DATASIZE) {
    DBG (1, "%s: unsupported line size: %d bytes > %d\n",
	 __FUNCTION__, dev->bytes_per_line, DATASIZE);
    return ret_cancel(dev, SANE_STATUS_NO_MEM);
  }

  dev->reading = 0; /* need to issue READ_IMAGE */

  dev->dataindex = 0;
  dev->datalen = 0;
  dev->dataoff = 0;

  return 1;
}

static int fill_slack(struct device *dev, SANE_Byte *buf, int maxlen)
{
  const int slack = dev->total_img_size - dev->total_out_size;
  const int havelen = MIN(slack, maxlen);
  int j;

  if (havelen <= 0)
    return 0;
  for (j = 0; j < havelen; j++)
    buf[j] = 255;
  return havelen;
}

static int copy_plain_trim(struct device *dev, SANE_Byte *buf, int maxlen, int *olenp)
{
  int j;
  const int linesize = dev->bytes_per_line;
  int k = dev->dataindex;
  *olenp = 0;
  for (j = 0; j < dev->datalen && *olenp < maxlen; j++, k++) {
    const int x = k % linesize;
    const int y = k / linesize;
    if (y >= dev->vertical)
      break; /* slack */
    if (x < dev->para.bytes_per_line &&
	(y + dev->y_off) < dev->para.lines) {
      *buf++ = dev->data[(dev->dataoff + j) & DATAMASK];
      (*olenp)++;
    }
  }
  dev->dataindex = k;
  return j;
}

/* return: how much data could be freed from cyclic buffer */
/* convert from RRGGBB to RGBRGB */
static int copy_mix_bands_trim(struct device *dev, SANE_Byte *buf, int maxlen, int *olenp) {
  int j;

  const int linesize = dev->bytes_per_line; /* caching real line size */

  /* line number of the head of input buffer,
   * input buffer is always aligned to whole line */
  const int y_off = dev->dataindex / linesize;

  int k = dev->dataindex; /* caching current index of input buffer */

  /* can only copy as much as full lines we have */
  int havelen = dev->datalen / linesize * linesize - k % linesize;

  const int bands = 3;
  *olenp = 0;

  /* while we have data && they can receive */
  for (j = 0; j < havelen && *olenp < maxlen; j++, k++) {
    const int band = (k % bands) * dev->horizontal;
    const int x = k % linesize / bands;
    const int y = k / linesize - y_off; /* y relative to buffer head */
    const int y_rly = y + y_off + dev->y_off; /* global y */

    if (x < dev->para.pixels_per_line &&
	y_rly < dev->para.lines) {
      *buf++ = dev->data[(dev->dataoff + band + x + y * linesize) & DATAMASK];
      (*olenp)++;
    }
  }
  dev->dataindex = k;

  /* how much full lines are finished */
  return (k / linesize - y_off) * linesize;
}

SANE_Status
sane_read (SANE_Handle h, SANE_Byte * buf, SANE_Int maxlen, SANE_Int * lenp)
{
  SANE_Status status;
  struct device *dev = h;

  DBG (3, "%s: %p, %p, %d, %p\n", __FUNCTION__, h, buf, maxlen, (void *)lenp);

  if (lenp)
    *lenp = 0;
  if (!dev)
    return SANE_STATUS_INVAL;

  if (!dev->scanning)
    return SANE_STATUS_EOF;

  /* if there is no data to read or output from buffer */
  if (!dev->blocklen && dev->datalen <= PADDING_SIZE) {

    /* and we don't need to acquire next block */
    if (dev->final_block) {
      int slack = dev->total_img_size - dev->total_out_size;

      /* but we may need to fill slack */
      if (buf && lenp && slack > 0) {
	*lenp = fill_slack(dev, buf, maxlen);
	dev->total_out_size += *lenp;
	DBG (9, "<> slack: %d, filled: %d, maxlen %d\n",
	     slack, *lenp, maxlen);
	return SANE_STATUS_GOOD;
      } else if (slack < 0) {
	/* this will never happen */
	DBG(1, "image overflow %d bytes\n", dev->total_img_size - dev->total_out_size);
      }

      /* that's all */
      dev_stop(dev);
      return SANE_STATUS_EOF;
    }

    /* queue next image block */
    if (!dev_acquire(dev))
      return dev->state;
  }

  if (!dev->reading) {
    if (cancelled(dev))
      return dev->state;
    DBG (5, "READ_IMAGE\n");
    if (!dev_cmd(dev, CMD_READ_IMAGE))
      return SANE_STATUS_IO_ERROR;
    dev->reading++;
    dev->ulines += dev->vertical;
    dev->y_off = dev->ulines - dev->vertical;
    dev->total_data_size += dev->blocklen;
    dev->blocks++;
  }

  do {
    size_t datalen;
    int clrlen; /* cleared lines len */
    int olen; /* output len */

    /* read as much data into the buffer */
    datalen = DATAROOM(dev) & USB_BLOCK_MASK;
    while (datalen && dev->blocklen) {
      SANE_Byte *rbuf = dev->data + DATATAIL(dev);

      DBG (9, "<> request len: %lu, [%d, %d; %d]\n",
	   (u_long)datalen, dev->dataoff, DATATAIL(dev), dev->datalen);
      if ((status = dev->io->dev_request(dev, NULL, 0, rbuf, &datalen)) !=
	  SANE_STATUS_GOOD)
	return status;
      dev->datalen += datalen;
      dev->blocklen -= datalen;
      DBG (9, "<> got %lu, [%d, %d; %d]\n",
	   (u_long)datalen, dev->dataoff, DATATAIL(dev), dev->datalen);
      if (dev->blocklen < 0)
	return ret_cancel(dev, SANE_STATUS_IO_ERROR);

      datalen = DATAROOM(dev) & USB_BLOCK_MASK;
    }

    if (buf && lenp) { /* read mode */
      /* copy will do minimal of valid data */
      if (dev->para.format == SANE_FRAME_RGB && dev->line_order)
	clrlen = copy_mix_bands_trim(dev, buf, maxlen, &olen);
      else
	clrlen = copy_plain_trim(dev, buf, maxlen, &olen);

      dev->datalen -= clrlen;
      dev->dataoff = (dev->dataoff + clrlen) & DATAMASK;
      buf += olen;
      maxlen -= olen;
      *lenp += olen;
      dev->total_out_size += olen;

      DBG (9, "<> olen: %d, clrlen: %d, blocklen: %d/%d, maxlen %d (%d %d %d)\n",
	   olen, clrlen, dev->blocklen, dev->datalen, maxlen,
	   dev->dataindex / dev->bytes_per_line + dev->y_off,
	   dev->y_off, dev->para.lines);

      /* slack beyond last line */
      if (dev->dataindex / dev->bytes_per_line + dev->y_off >= dev->para.lines) {
	dev->datalen = 0;
	dev->dataoff = 0;
      }

      if (!clrlen || maxlen <= 0)
	break;
    } else { /* flush mode */
      dev->datalen = 0;
      dev->dataoff = 0;
    }

  } while (dev->blocklen);

  if (lenp)
    DBG (9, " ==> %d\n", *lenp);

  return SANE_STATUS_GOOD;
}

SANE_Status
sane_start (SANE_Handle h)
{
  struct device *dev = h;

  DBG (3, "%s: %p\n", __FUNCTION__, h);

  dev->cancel = 0;
  dev->scanning = 0;
  dev->total_img_size = 0;
  dev->total_out_size = 0;
  dev->total_data_size = 0;
  dev->blocks = 0;

  if (!dev->reserved) {
    if (!dev_cmd_wait(dev, CMD_RESERVE_UNIT))
      return dev->state;
    dev->reserved++;
  }

  if (!dev_set_window(dev) ||
      (dev->state && dev->state != SANE_STATUS_DEVICE_BUSY))
    return dev_stop(dev);

  if (!dev_cmd_wait(dev, CMD_OBJECT_POSITION))
    return dev_stop(dev);

  if (!dev_cmd(dev, CMD_READ) ||
      (dev->state && dev->state != SANE_STATUS_DEVICE_BUSY))
    return dev_stop(dev);

  dev->scanning = 1;
  dev->final_block = 0;
  dev->blocklen = 0;
  dev->pixels_per_line = 0;
  dev->bytes_per_line = 0;
  dev->ulines = 0;

  set_parameters(dev);

  if (!dev->data && !(dev->data = malloc(DATASIZE)))
    return ret_cancel(dev, SANE_STATUS_NO_MEM);

  if (!dev_acquire(dev))
    return dev->state;

  /* make sure to have dev->para <= of real size */
  if (dev->para.pixels_per_line > dev->pixels_per_line) {
    dev->para.pixels_per_line = dev->pixels_per_line;
    dev->para.bytes_per_line = dev->pixels_per_line;
  }

  if (dev->composition == MODE_RGB24)
    dev->para.bytes_per_line = dev->para.pixels_per_line * 3;
  else if (dev->composition == MODE_LINEART ||
	   dev->composition == MODE_HALFTONE) {
    dev->para.bytes_per_line = (dev->para.pixels_per_line + 7) / 8;
    dev->para.pixels_per_line = dev->para.bytes_per_line * 8;
  } else {
    dev->para.bytes_per_line = dev->para.pixels_per_line;
  }

  dev->total_img_size = dev->para.bytes_per_line * dev->para.lines;

  return SANE_STATUS_GOOD;
}

SANE_Status sane_set_io_mode (SANE_Handle h, SANE_Bool non_blocking)
{
  struct device *dev = h;

  DBG (3, "%s: %p, %d\n", __FUNCTION__, h, non_blocking);

  if (non_blocking)
    return SANE_STATUS_UNSUPPORTED;

  dev->non_blocking = non_blocking;
  return SANE_STATUS_GOOD;
}

SANE_Status sane_get_select_fd (SANE_Handle h, SANE_Int * fdp)
{
  DBG (3, "%s: %p, %p\n", __FUNCTION__, h, (void *)fdp);
  /* supporting of this will require thread creation */
  return SANE_STATUS_UNSUPPORTED;
}

void sane_cancel (SANE_Handle h)
{
  struct device *dev = h;

  DBG (3, "%s: %p\n", __FUNCTION__, h);
  dev->cancel = 1;
}

/* xerox_mfp.c */
