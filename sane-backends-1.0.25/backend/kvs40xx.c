/*
   Copyright (C) 2009, Panasonic Russia Ltd.
   Copyright (C) 2010,2011, m. allan noah
*/
/*
   Panasonic KV-S40xx USB-SCSI scanner driver.
*/

#include "../include/sane/config.h"

#include <ctype.h> /*isspace*/
#include <math.h> /*tan*/

#include <string.h>
#include <unistd.h>
#include <pthread.h>
#define DEBUG_NOT_STATIC
#include "../include/sane/sanei_backend.h"
#include "../include/sane/sane.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei.h"
#include "../include/sane/sanei_config.h"
#include "../include/sane/sanei_usb.h"
#include "../include/sane/sanei_scsi.h"
#include "lassert.h"

#include "kvs40xx.h"

#include "sane/sanei_debug.h"

#define DATA_TAIL 0x200

struct known_device
{
  const SANE_Int id;
  const SANE_Device scanner;
};

static const struct known_device known_devices[] = {
  {
   KV_S4085C,
   {
    "MATSHITA",
    "KV-S4085C",
    "High Speed Color ADF Scanner",
    "scanner"
   },
  },
  {
   KV_S4065C,
   {
    "MATSHITA",
    "KV-S4065C",
    "High Speed Color ADF Scanner",
    "scanner"
   },
  },
  {
   KV_S7075C,
   {
    "MATSHITA",
    "KV-S7075C",
    "High Speed Color ADF Scanner",
    "scanner"
   },
  },
};

static inline SANE_Status buf_init(struct buf *b, SANE_Int sz)
{
	const int num = sz / BUF_SIZE + 1;
	b->buf = (u8 **) realloc(b->buf, num * sizeof(u8 *));
	if (!b->buf)
		return SANE_STATUS_NO_MEM;
	memset(b->buf, 0, num * sizeof(void *));
	b->size = b->head = b->tail = 0;
	b->sem = 0;
	b->st = SANE_STATUS_GOOD;
	pthread_cond_init(&b->cond, NULL);
	pthread_mutex_init(&b->mu, NULL);
	return SANE_STATUS_GOOD;
}

static inline void buf_deinit(struct buf *b)
{
	int i;
	if (!b->buf)
		return;
	for (i = b->head; i < b->tail; i++)
		if (b->buf[i])
			free(b->buf[i]);
	free(b->buf);
	b->buf = NULL;
	b->head = b->tail = 0;
}

static inline SANE_Status new_buf(struct buf *b, u8 ** p)
{
	b->buf[b->tail] = (u8 *) malloc(BUF_SIZE);
	if (!b->buf[b->tail])
		return SANE_STATUS_NO_MEM;
	*p = b->buf[b->tail];
	++b->tail;
	return SANE_STATUS_GOOD;
}

static inline SANE_Status buf_get_err(struct buf *b)
{
	return b->size ? SANE_STATUS_GOOD : b->st;
}

static inline void buf_set_st(struct buf *b, SANE_Status st)
{
	pthread_mutex_lock(&b->mu);
	b->st = st;
	if (buf_get_err(b))
		pthread_cond_signal(&b->cond);
	pthread_mutex_unlock(&b->mu);
}

static inline void buf_cancel(struct buf *b)
{
	buf_set_st(b, SANE_STATUS_CANCELLED);
}

static inline void push_buf(struct buf *b, SANE_Int sz)
{
	pthread_mutex_lock(&b->mu);
	b->sem++;
	b->size += sz;
	pthread_cond_signal(&b->cond);
	pthread_mutex_unlock(&b->mu);
}

static inline u8 *get_buf(struct buf *b, SANE_Int * sz)
{
	SANE_Status err = buf_get_err(b);
	if (err)
		return NULL;

	pthread_mutex_lock(&b->mu);
	while (!b->sem && !buf_get_err(b))
		pthread_cond_wait(&b->cond, &b->mu);
	b->sem--;
	err = buf_get_err(b);
	if (!err) {
		*sz = b->size < BUF_SIZE ? b->size : BUF_SIZE;
		b->size -= *sz;
	}
	pthread_mutex_unlock(&b->mu);
	return err ? NULL : b->buf[b->head];
}

static inline void pop_buf(struct buf *b)
{
	free(b->buf[b->head]);
	b->buf[b->head] = NULL;
	++b->head;
}

SANE_Status
sane_init (SANE_Int __sane_unused__ * version_code,
	   SANE_Auth_Callback __sane_unused__ authorize)
{
  DBG_INIT ();
  DBG (DBG_INFO, "This is panasonic kvs40xx driver\n");

  *version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR, 1);

  /* Initialize USB */
  sanei_usb_init ();

  return SANE_STATUS_GOOD;
}

/*
 * List of available devices, allocated by sane_get_devices, released
 * by sane_exit()
 */
static SANE_Device **devlist = NULL;
static unsigned curr_scan_dev = 0;

void
sane_exit (void)
{
  if (devlist)
    {
      int i;
      for (i = 0; devlist[i]; i++)
	{
	  free ((void *) devlist[i]);
	}
      free ((void *) devlist);
      devlist = NULL;
    }
}

SANE_Status
attach (SANE_String_Const devname);

SANE_Status
attach (SANE_String_Const devname)
{
  int i = 0;
  if (devlist)
    {
      for (; devlist[i]; i++);
      devlist = realloc (devlist, sizeof (SANE_Device *) * (i + 1));
      if (!devlist)
	return SANE_STATUS_NO_MEM;
    }
  else
    {
      devlist = malloc (sizeof (SANE_Device *) * 2);
      if (!devlist)
	return SANE_STATUS_NO_MEM;
    }
  devlist[i] = malloc (sizeof (SANE_Device));
  if (!devlist[i])
    return SANE_STATUS_NO_MEM;
  memcpy (devlist[i], &known_devices[curr_scan_dev].scanner,
	  sizeof (SANE_Device));
  devlist[i]->name = strdup (devname);
  /* terminate device list with NULL entry: */
  devlist[i + 1] = 0;
  DBG (DBG_INFO, "%s device attached\n", devname);
  return SANE_STATUS_GOOD;
}

/* Get device list */
SANE_Status
sane_get_devices (const SANE_Device *** device_list,
		  SANE_Bool __sane_unused__ local_only)
{
  if (devlist)
    {
      int i;
      for (i = 0; devlist[i]; i++)
	{
	  free ((void *) devlist[i]);
	}
      free ((void *) devlist);
      devlist = NULL;
    }

  for (curr_scan_dev = 0;
       curr_scan_dev <
       sizeof (known_devices) / sizeof (known_devices[0]); curr_scan_dev++)
    {
      sanei_usb_find_devices (PANASONIC_ID,
			      known_devices[curr_scan_dev].id, attach);
    }

  for (curr_scan_dev = 0;
       curr_scan_dev <
       sizeof (known_devices) / sizeof (known_devices[0]); curr_scan_dev++)
    {
      sanei_scsi_find_devices (known_devices[curr_scan_dev].
			       scanner.vendor,
			       known_devices[curr_scan_dev].
			       scanner.model, NULL, -1, -1, -1, -1, attach);
    }
  if(device_list)
    *device_list = (const SANE_Device **) devlist;
  return SANE_STATUS_GOOD;
}

/* Open device, return the device handle */
SANE_Status
sane_open (SANE_String_Const devname, SANE_Handle * handle)
{
  unsigned i, j, id = 0;
  struct scanner *s;
  SANE_Int h, bus;
  SANE_Status st = SANE_STATUS_GOOD;
  if (!devlist)
    {
      st = sane_get_devices (NULL, 0);
      if (st)
	return st;
    }
  for (i = 0; devlist[i]; i++)
    {
      if (!strcmp (devlist[i]->name, devname))
	break;
    }
  if (!devlist[i])
    return SANE_STATUS_INVAL;
  for (j = 0; j < sizeof (known_devices) / sizeof (known_devices[0]); j++)
    {
      if (!strcmp (devlist[i]->model, known_devices[j].scanner.model))
	{
	  id = known_devices[j].id;
	  break;
	}
    }

  st = sanei_usb_open (devname, &h);

  if (st == SANE_STATUS_ACCESS_DENIED)
    return st;
  if (st)
    {
      st = sanei_scsi_open (devname, &h, kvs40xx_sense_handler, NULL);
      if (st)
	{
	  return st;
	}
      bus = SCSI;
    }
  else
    {
      bus = USB;
      st = sanei_usb_claim_interface (h, 0);
      if (st)
	{
	  sanei_usb_close (h);
	  return st;
	}
    }

  s = malloc (sizeof (struct scanner));
  if (!s)
    return SANE_STATUS_NO_MEM;
  memset (s, 0, sizeof (struct scanner));
  s->buffer = malloc (MAX_READ_DATA_SIZE + BULK_HEADER_SIZE);
  if (!s->buffer)
    return SANE_STATUS_NO_MEM;

  s->file = h;
  s->bus = bus;
  s->id = id;
  strcpy (s->name, devname);
  *handle = s;
  for (i = 0; i < 3; i++)
    {
      st = kvs40xx_test_unit_ready (s);
      if (st)
	{
	  if (s->bus == SCSI)
	    {
	      sanei_scsi_close (s->file);
	      st = sanei_scsi_open (devname, &h, kvs40xx_sense_handler, NULL);
	      if (st)
		return st;
	    }
	  else
	    {
	      sanei_usb_release_interface (s->file, 0);
	      sanei_usb_close (s->file);
	      st = sanei_usb_open (devname, &h);
	      if (st)
		return st;
	      st = sanei_usb_claim_interface (h, 0);
	      if (st)
		{
		  sanei_usb_close (h);
		  return st;
		}
	    }
	  s->file = h;
	}
      else
	break;
    }
  if (i == 3)
    return SANE_STATUS_DEVICE_BUSY;

  if (id == KV_S4085C || id == KV_S4065C)
    {
      char str[16];
      st = inquiry (s, str);
      if (st)
	goto err;
      if (id == KV_S4085C)
	s->id = !strcmp (str, "KV-S4085CL") ? KV_S4085CL : KV_S4085CW;
      else
	s->id = !strcmp (str, "KV-S4065CL") ? KV_S4065CL : KV_S4065CW;
    }
  kvs40xx_init_options (s);
  st = kvs40xx_set_timeout (s, s->val[FEED_TIMEOUT].w);
  if (st)
    goto err;

  return SANE_STATUS_GOOD;
err:
  sane_close (s);
  return st;
}

/* Close device */
void
sane_close (SANE_Handle handle)
{
  struct scanner *s = (struct scanner *) handle;
  unsigned i;
  hopper_down (s);
  if (s->bus == USB)
    {
      sanei_usb_release_interface (s->file, 0);
      sanei_usb_close (s->file);
    }
  else
    sanei_scsi_close (s->file);

  for (i = 1; i < NUM_OPTIONS; i++)
    {
      if (s->opt[i].type == SANE_TYPE_STRING && s->val[i].s)
	free (s->val[i].s);
    }

  for (i = 0; i < sizeof (s->buf) / sizeof (s->buf[0]); i++)
    buf_deinit (&s->buf[i]);

  free (s->buffer);
  free (s);

}

/* Get option descriptor */
const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  struct scanner *s = handle;

  if ((unsigned) option >= NUM_OPTIONS || option < 0)
    return NULL;
  return s->opt + option;
}

static SANE_Status
wait_document (struct scanner *s)
{
  SANE_Status st;
  int i;
  if (!strcmp ("fb", s->val[SOURCE].s))
    return SANE_STATUS_GOOD;
  if (!strcmp ("off", s->val[MANUALFEED].s))
    return kvs40xx_document_exist (s);

  for (i = 0; i < s->val[FEED_TIMEOUT].w; i++)
    {
      st = kvs40xx_document_exist (s);
      if (st != SANE_STATUS_NO_DOCS)
	return st;
      sleep (1);
    }
  return SANE_STATUS_NO_DOCS;
}

static SANE_Status read_image_duplex(SANE_Handle handle)
{
	struct scanner *s = (struct scanner *) handle;
	SANE_Status st = SANE_STATUS_GOOD;
	unsigned read, side;
	int i;
	struct side {
		unsigned mx, eof;
		u8 *p;
		struct buf *buf;
	} a[2], *b;

	for (i = 0; i < 2; i++) {
		a[i].mx = BUF_SIZE;
		a[i].eof = 0;
		a[i].buf = &s->buf[i];
		st = new_buf(&s->buf[i], &a[i].p);
		if (st)
			goto err;
	}
	for (b = &a[0], side = SIDE_FRONT; (!a[0].eof || !a[1].eof);) {
		pthread_testcancel();
		if (b->mx == 0) {
			push_buf(b->buf, BUF_SIZE);
			st = new_buf(b->buf, &b->p);
			if (st)
				goto err;
			b->mx = BUF_SIZE;
		}

		st = kvs40xx_read_image_data(s, s->page, side,
					     b->p + BUF_SIZE - b->mx, b->mx,
					     &read);
		b->mx -= read;
		if (st) {
			if (st != INCORRECT_LENGTH
			    && st != SANE_STATUS_EOF)
				goto err;

			if (st == SANE_STATUS_EOF) {
				b->eof = 1;
				push_buf(b->buf, BUF_SIZE - b->mx);
			}
			side ^= SIDE_BACK;
			b = &a[side == SIDE_FRONT ? 0 : 1];
		}
	}

      err:
	for (i = 0; i < 2; i++)
		buf_set_st(&s->buf[i], st);
	return st;
}

static SANE_Status read_image_simplex(SANE_Handle handle)
{
	struct scanner *s = (struct scanner *) handle;
	SANE_Status st = SANE_STATUS_GOOD;

	for (; (!st || st == INCORRECT_LENGTH);) {
		unsigned read, mx;
		unsigned char *p = NULL;
		st = new_buf(&s->buf[0], &p);
		for (read = 0, mx = BUF_SIZE; mx &&
		     (!st || st == INCORRECT_LENGTH); mx -= read) {
			pthread_testcancel();
			st = kvs40xx_read_image_data(s, s->page, SIDE_FRONT,
						     p + BUF_SIZE - mx, mx,
						     &read);
		}
		push_buf(&s->buf[0], BUF_SIZE - mx);
	}
	buf_set_st(&s->buf[0], st);
	return st;
}

static SANE_Status read_data(struct scanner *s)
{
	SANE_Status st;
	int duplex = s->val[DUPLEX].w;
	s->read = 0;
	s->side = SIDE_FRONT;

	st = duplex ? read_image_duplex(s) : read_image_simplex(s);
	if (st && (st != SANE_STATUS_EOF))
		goto err;

	st = kvs40xx_read_picture_element(s, SIDE_FRONT, &s->params);
	if (st)
		goto err;
	if (!s->params.lines) {
		st = SANE_STATUS_INVAL;
		goto err;
	}

	sane_get_parameters(s, NULL);

	s->page++;
	return SANE_STATUS_GOOD;
      err:
	s->scanning = 0;
	return st;
}

/* Start scanning */
SANE_Status
sane_start (SANE_Handle handle)
{
  struct scanner *s = (struct scanner *) handle;
  SANE_Status st = SANE_STATUS_GOOD;
  int duplex = s->val[DUPLEX].w, i;
  unsigned data_avalible;
  int start = 0;

  if (s->thread)
    {
      pthread_join (s->thread, NULL);
      s->thread = 0;
    }
  if (!s->scanning)
    {
      st = kvs40xx_test_unit_ready (s);
      if (st)
	return st;

      st = wait_document (s);
      if (st)
	return st;

      st = kvs40xx_reset_window (s);
      if (st)
	return st;
      st = kvs40xx_set_window (s, SIDE_FRONT);

      if (st)
	return st;

      if (duplex)
	{
	  st = kvs40xx_set_window (s, SIDE_BACK);
	  if (st)
	    return st;
	}

      st = kvs40xx_scan (s);
      if (st)
	return st;

      if (s->val[CROP].b || s->val[LENGTHCTL].b || s->val[LONG_PAPER].b)
	{
	  unsigned w, h, res = s->val[RESOLUTION].w;
	  SANE_Parameters *p = &s->params;
	  w = 297;		/*A3 */
	  h = 420;
	  p->pixels_per_line = w * res / 25.4 + .5;
	  p->lines = h * res / 25.4 + .5;
	}
      else
	{
	  st = kvs40xx_read_picture_element (s, SIDE_FRONT, &s->params);
	  if (st)
	    return st;
	}

      start = 1;
      s->scanning = 1;
      s->page = 0;
      s->read = 0;
      s->side = SIDE_FRONT;
      sane_get_parameters (s, NULL);
    }

  if (duplex && s->side == SIDE_FRONT && !start)
    {
      s->side = SIDE_BACK;
      s->read = 0;
      return SANE_STATUS_GOOD;
    }
	do {
		st = get_buffer_status(s, &data_avalible);
		if (st)
			goto err;

	} while (!data_avalible);

  for (i = 0; i < (duplex ? 2 : 1); i++)
    {
      st = buf_init (&s->buf[i], s->side_size);
      if (st)
	goto err;
    }

  if (pthread_create (&s->thread, NULL, (void *(*)(void *)) read_data, s))
    {
      st = SANE_STATUS_IO_ERROR;
      goto err;
    }

  if (s->val[CROP].b || s->val[LENGTHCTL].b || s->val[LONG_PAPER].b)
    {
      pthread_join (s->thread, NULL);
      s->thread = 0;
    }

  return SANE_STATUS_GOOD;
err:
  s->scanning = 0;
  return st;
}

SANE_Status
sane_read(SANE_Handle handle, SANE_Byte * buf,
	  SANE_Int max_len, SANE_Int * len)
{
	struct scanner *s = (struct scanner *) handle;
	int duplex = s->val[DUPLEX].w;
	struct buf *b = s->side == SIDE_FRONT ? &s->buf[0] : &s->buf[1];
	SANE_Status err = buf_get_err(b);
	SANE_Int inbuf = 0;
	*len = 0;

	if (!s->scanning)
		return SANE_STATUS_EOF;
	if (err)
		goto out;

	if (s->read) {
		*len =
		    max_len <
		    (SANE_Int) s->read ? max_len : (SANE_Int) s->read;
		memcpy(buf, s->data + BUF_SIZE - s->read, *len);
		s->read -= *len;

		if (!s->read)
			pop_buf(b);
		goto out;
	}

	s->data = get_buf(b, &inbuf);
	if (!s->data)
		goto out;

	*len = max_len < inbuf ? max_len : inbuf;
	if (*len > BUF_SIZE)
		*len = BUF_SIZE;
	memcpy(buf, s->data, *len);
	s->read = inbuf > BUF_SIZE ? BUF_SIZE - *len : inbuf - *len;

	if (!s->read)
		pop_buf(b);
      out:
	err = *len ? SANE_STATUS_GOOD : buf_get_err(b);
	if (err == SANE_STATUS_EOF) {
		if (strcmp(s->val[FEEDER_MODE].s, SANE_I18N("continuous"))) {
			if (!duplex || s->side == SIDE_BACK)
				s->scanning = 0;
		}
		buf_deinit(b);
	} else if (err) {
		unsigned i;
		for (i = 0; i < sizeof(s->buf) / sizeof(s->buf[0]); i++)
			buf_deinit(&s->buf[i]);
	}
	return err;
}

void
sane_cancel (SANE_Handle handle)
{
  unsigned i;
  struct scanner *s = (struct scanner *) handle;
  if (s->scanning && !strcmp (s->val[FEEDER_MODE].s, SANE_I18N ("continuous")))
    {
      stop_adf (s);
    }
  if (s->thread)
    {
      pthread_cancel (s->thread);
      pthread_join (s->thread, NULL);
      s->thread = 0;
    }
  for (i = 0; i < sizeof (s->buf) / sizeof (s->buf[0]); i++)
    buf_deinit (&s->buf[i]);
  s->scanning = 0;
}

SANE_Status
sane_set_io_mode (SANE_Handle __sane_unused__ h, SANE_Bool __sane_unused__ m)
{
  return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
sane_get_select_fd (SANE_Handle __sane_unused__ h,
		    SANE_Int __sane_unused__ * fd)
{
  return SANE_STATUS_UNSUPPORTED;
}
