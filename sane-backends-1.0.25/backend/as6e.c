/* sane - Scanner Access Now Easy.
   Artec AS6E backend.
   Copyright (C) 2000 Eugene S. Weiss
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

   If you write modifications of your own for SANE, it is your choice
   whether to permit this exception to apply to your modifications.
   If you do not wish that, delete this exception notice.

   This file implements a backend for the Artec AS6E by making a bridge
   to the as6edriver program.  The as6edriver program can be found at
   http://as6edriver.sourceforge.net .  */




#include "../include/sane/config.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>

#include "../include/sane/sane.h"
#include "../include/sane/saneopts.h"

#define BACKENDNAME as6e
#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_config.h"

#include "as6e.h"

static int num_devices;
static AS6E_Device *first_dev;
static AS6E_Scan *first_handle;
static const SANE_Device **devlist = 0;

static SANE_Status attach (const char *devname, AS6E_Device ** devp);
/* static SANE_Status attach_one (const char *dev);  */

static const SANE_String_Const mode_list[] = {
  SANE_VALUE_SCAN_MODE_LINEART,
  SANE_VALUE_SCAN_MODE_GRAY,
  SANE_VALUE_SCAN_MODE_COLOR,
  0
};

static const SANE_Word resolution_list[] = {
  4, 300, 200, 100, 50
};

static const SANE_Range x_range = {
  SANE_FIX (0),
  SANE_FIX (215.91),
  SANE_FIX (0)
};

static const SANE_Range y_range = {
  SANE_FIX (0),
  SANE_FIX (297.19),
  SANE_FIX (0)
};


static const SANE_Range brightness_range = {
  -100,
  100,
  1
};

static const SANE_Range contrast_range = {
  -100,
  100,
  1
};

/*--------------------------------------------------------------------------*/
static SANE_Int
as6e_unit_convert (SANE_Fixed value)
{

  double precise;
  SANE_Int return_value;

  precise = SANE_UNFIX (value);
  precise = (precise * 300) / MM_PER_INCH;
  return_value = precise;
  return return_value;
}

/*--------------------------------------------------------------------------*/

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * buf, SANE_Int max_len,
	   SANE_Int * len)
{
  AS6E_Scan *s = handle;
  SANE_Word buffer_offset = 0;
  int written = 0, bytes_read = 0, maxbytes;
  SANE_Word bytecounter, linebufcounter, ctlbytes;
  SANE_Byte *linebuffer;

  DBG (3, "reading %d bytes, %d bytes in carryover buffer\n", max_len,
       s->scan_buffer_count);

  if ((unsigned int) s->image_counter >= s->bytes_to_read)
    {
      *len = 0;
      if (s->scanning)
	{
	  read (s->as6e_params.ctlinpipe, &written, sizeof (written));
	  if (written != -1)
	    DBG (3, "pipe error\n");
	  DBG (3, "trying  to read -1 ...written = %d\n", written);
	}
      s->scanning = SANE_FALSE;
      DBG (1, "image data complete, sending EOF...\n");
      return SANE_STATUS_EOF;
    }				/*image complete */

  linebuffer = s->line_buffer;
  if (s->scan_buffer_count > 0)
    {				/*there are leftover bytes from the last call */
      if (s->scan_buffer_count <= max_len)
	{
	  for (*len = 0; *len < s->scan_buffer_count; (*len)++)
	    {
	      buf[*len] = s->scan_buffer[*len];
	      buffer_offset++;
	    }
	  s->scan_buffer_count = 0;
	  if (s->scan_buffer_count == max_len)
	    {
	      s->scan_buffer_count = 0;
	      s->image_counter += max_len;
	      DBG (3, "returning %d bytes from the carryover buffer\n", *len);
	      return SANE_STATUS_GOOD;
	    }
	}
      else
	{
	  for (*len = 0; *len < max_len; (*len)++)
	    buf[*len] = s->scan_buffer[*len];

	  for (bytecounter = max_len;
	       bytecounter < s->scan_buffer_count; bytecounter++)
	    s->scan_buffer[bytecounter - max_len]
	      = s->scan_buffer[bytecounter];

	  s->scan_buffer_count -= max_len;
	  s->image_counter += max_len;
	  DBG (3, "returning %d bytes from the carryover buffer\n", *len);
	  return SANE_STATUS_GOOD;
	}
    }
  else
    {
      *len = 0;			/*no bytes in the buffer */
      if (!s->scanning)
	{
	  DBG (1, "scan over returning %d\n", *len);
	  if (s->scan_buffer_count)
	    return SANE_STATUS_GOOD;
	  else
	    return SANE_STATUS_EOF;
	}
    }
  while (*len < max_len)
    {
      DBG (3, "trying to read number of bytes...\n");
      ctlbytes = read (s->as6e_params.ctlinpipe, &written, sizeof (written));
      DBG (3, "bytes written = %d, ctlbytes =%d\n", written, ctlbytes);
      fflush (stdout);
      if ((s->cancelled) && (written == 0))
	{			/*first clear -1 from pipe */
	  DBG (1, "sending SANE_STATUS_CANCELLED\n");
	  read (s->as6e_params.ctlinpipe, &written, sizeof (written));
	  s->scanning = SANE_FALSE;
	  return SANE_STATUS_CANCELLED;
	}
      if (written == -1)
	{
	  DBG (1, "-1READ Scanner through. returning %d bytes\n", *len);
	  s->image_counter += *len;
	  s->scanning = SANE_FALSE;
	  return SANE_STATUS_GOOD;
	}
      linebufcounter = 0;
      DBG (3,
	   "linebufctr reset, len =%d written =%d bytes_read =%d, max = %d\n",
	   *len, written, bytes_read, max_len);
      maxbytes = written;
      while (linebufcounter < written)
	{
	  DBG (4, "trying to read data pipe\n");
	  bytes_read =
	    read (s->as6e_params.datapipe, linebuffer + linebufcounter,
		  maxbytes);
	  linebufcounter += bytes_read;
	  maxbytes -= bytes_read;
	  DBG (3, "bytes_read = %d linebufcounter = %d\n", bytes_read,
	       linebufcounter);
	}
      DBG (3, "written =%d max_len =%d  len =%d\n", written, max_len, *len);
      if (written <= (max_len - *len))
	{
	  for (bytecounter = 0; bytecounter < written; bytecounter++)
	    {
	      buf[bytecounter + buffer_offset] = linebuffer[bytecounter];
	      (*len)++;
	    }
	  buffer_offset += written;
	  DBG (3, "buffer offset = %d\n", buffer_offset);
	}
      else if (max_len > *len)
	{			/*there's still room to send data */
	  for (bytecounter = 0; bytecounter < (max_len - *len); bytecounter++)
	    buf[bytecounter + buffer_offset] = linebuffer[bytecounter];
	  DBG (3, "topping off buffer\n");
	  for (bytecounter = (max_len - *len); bytecounter < written;
	       bytecounter++)
	    {

	      s->scan_buffer[s->scan_buffer_count + bytecounter -
			     (max_len - *len)] = linebuffer[bytecounter];
	    }
	  s->scan_buffer_count += (written - (max_len - *len));
	  *len = max_len;
	}
      else
	{			/*everything goes into the carryover buffer */
	  for (bytecounter = 0; bytecounter < written; bytecounter++)
	    s->scan_buffer[s->scan_buffer_count + bytecounter]
	      = linebuffer[bytecounter];
	  s->scan_buffer_count += written;
	}
    }				/*while there's space in the buffer */
  s->image_counter += *len;
  DBG (3, "image ctr = %d bytes_to_read = %lu returning %d\n",
       s->image_counter, (u_long) s->bytes_to_read, *len);

  return SANE_STATUS_GOOD;
}

/*--------------------------------------------------------------------------*/
void
sane_cancel (SANE_Handle h)
{
  AS6E_Scan *s = h;
  SANE_Word test;
  DBG (2, "trying to cancel...\n");
  if (s->scanning)
    {
      test = kill (s->child_pid, SIGUSR1);
      if (test == 0)
	s->cancelled = SANE_TRUE;
    }
}

/*--------------------------------------------------------------------------*/

SANE_Status
sane_start (SANE_Handle handle)
{
  AS6E_Scan *s = handle;
  SANE_Status status;
  int repeat = 1;
  SANE_Word numbytes;
  int scan_params[8];
  /* First make sure we have a current parameter set.  Some of the
   * parameters will be overwritten below, but that's OK.  */
  DBG (2, "sane_start\n");
  status = sane_get_parameters (s, 0);
  if (status != SANE_STATUS_GOOD)
    return status;
  DBG (1, "Got params again...\n");
  numbytes = write (s->as6e_params.ctloutpipe, &repeat, sizeof (repeat));
  if (numbytes != sizeof (repeat))
    return (SANE_STATUS_IO_ERROR);
  DBG (1, "sending start_scan signal\n");
  scan_params[0] = s->as6e_params.resolution;
  if (strcmp (s->value[OPT_MODE].s, SANE_VALUE_SCAN_MODE_COLOR) == 0)
    scan_params[1] = 0;
  else if (strcmp (s->value[OPT_MODE].s, SANE_VALUE_SCAN_MODE_GRAY) == 0)
    scan_params[1] = 1;
  else if (strcmp (s->value[OPT_MODE].s, SANE_VALUE_SCAN_MODE_LINEART) == 0)
    scan_params[1] = 2;
  else
    return (SANE_STATUS_JAMMED);	/*this should never happen */
  scan_params[2] = s->as6e_params.startpos;
  scan_params[3] = s->as6e_params.stoppos;
  scan_params[4] = s->as6e_params.startline;
  scan_params[5] = s->as6e_params.stopline;
  scan_params[6] = s->value[OPT_BRIGHTNESS].w;
  scan_params[7] = s->value[OPT_CONTRAST].w;
  DBG (1, "scan params = %d %d %d %d %d %d %d %d\n", scan_params[0],
       scan_params[1], scan_params[2], scan_params[3],
       scan_params[4], scan_params[5], scan_params[6], scan_params[7]);
  numbytes =
    write (s->as6e_params.ctloutpipe, scan_params, sizeof (scan_params));
  if (numbytes != sizeof (scan_params))
    return (SANE_STATUS_IO_ERROR);
  s->scanning = SANE_TRUE;
  s->scan_buffer_count = 0;
  s->image_counter = 0;
  s->cancelled = 0;
  return (SANE_STATUS_GOOD);
}

/*--------------------------------------------------------------------------*/

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  AS6E_Scan *s = handle;
  SANE_String mode;
  SANE_Word divisor = 1;
  DBG (2, "sane_get_parameters\n");
  if (!s->scanning)
    {
      memset (&s->sane_params, 0, sizeof (s->sane_params));
      s->as6e_params.resolution = s->value[OPT_RESOLUTION].w;
      s->as6e_params.startpos = as6e_unit_convert (s->value[OPT_TL_X].w);
      s->as6e_params.stoppos = as6e_unit_convert (s->value[OPT_BR_X].w);
      s->as6e_params.startline = as6e_unit_convert (s->value[OPT_TL_Y].w);
      s->as6e_params.stopline = as6e_unit_convert (s->value[OPT_BR_Y].w);
      if ((s->as6e_params.resolution == 200)
	  || (s->as6e_params.resolution == 100))
	divisor = 3;
      else if (s->as6e_params.resolution == 50)
	divisor = 6;		/*get legal values for 200 dpi */
      s->as6e_params.startpos = (s->as6e_params.startpos / divisor) * divisor;
      s->as6e_params.stoppos = (s->as6e_params.stoppos / divisor) * divisor;
      s->as6e_params.startline =
	(s->as6e_params.startline / divisor) * divisor;
      s->as6e_params.stopline = (s->as6e_params.stopline / divisor) * divisor;
      s->sane_params.pixels_per_line =
	(s->as6e_params.stoppos -
	 s->as6e_params.startpos) * s->as6e_params.resolution / 300;
      s->sane_params.lines =
	(s->as6e_params.stopline -
	 s->as6e_params.startline) * s->as6e_params.resolution / 300;
      mode = s->value[OPT_MODE].s;
/*      if ((strcmp (s->mode, SANE_VALUE_SCAN_MODE_LINEART) == 0) ||
	  (strcmp (s->mode, SANE_VALUE_SCAN_MODE_HALFTONE) == 0))
	{
	  s->sane_params.format = SANE_FRAME_GRAY;
	  s->sane_params.bytes_per_line = (s->sane_params.pixels_per_line + 7) / 8;
	  s->sane_params.depth = 1;
	}  */
/*else*/ if ((strcmp (mode, SANE_VALUE_SCAN_MODE_GRAY) == 0)
	     || (strcmp (mode, SANE_VALUE_SCAN_MODE_LINEART) == 0))
	{
	  s->sane_params.format = SANE_FRAME_GRAY;
	  s->sane_params.bytes_per_line = s->sane_params.pixels_per_line;
	  s->sane_params.depth = 8;
	}			/*grey frame */
      else
	{
	  s->sane_params.format = SANE_FRAME_RGB;
	  s->sane_params.bytes_per_line = 3 * s->sane_params.pixels_per_line;
	  s->sane_params.depth = 8;
	}			/*color frame */
      s->bytes_to_read = s->sane_params.lines * s->sane_params.bytes_per_line;
      s->sane_params.last_frame = SANE_TRUE;
    }				/*!scanning */

  if (params)
    *params = s->sane_params;
  return (SANE_STATUS_GOOD);
}

/*--------------------------------------------------------------------------*/
SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *val, SANE_Int * info)
{
  AS6E_Scan *s = handle;
  SANE_Status status = 0;
  SANE_Word cap;
  DBG (2, "sane_control_option\n");
  if (info)
    *info = 0;
  if (s->scanning)
    return SANE_STATUS_DEVICE_BUSY;
  if (option >= NUM_OPTIONS)
    return SANE_STATUS_INVAL;
  cap = s->options_list[option].cap;
  if (!SANE_OPTION_IS_ACTIVE (cap))
    return SANE_STATUS_INVAL;
  if (action == SANE_ACTION_GET_VALUE)
    {
      DBG (1, "sane_control_option %d, get value\n", option);
      switch (option)
	{
	  /* word options: */
	case OPT_RESOLUTION:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_NUM_OPTS:
	case OPT_CONTRAST:
	case OPT_BRIGHTNESS:
	  *(SANE_Word *) val = s->value[option].w;
	  return (SANE_STATUS_GOOD);
	  /* string options: */
	case OPT_MODE:
	  strcpy (val, s->value[option].s);
	  return (SANE_STATUS_GOOD);
	}
    }
  else if (action == SANE_ACTION_SET_VALUE)
    {
      DBG (1, "sane_control_option %d, set value\n", option);
      if (!SANE_OPTION_IS_SETTABLE (cap))
	return (SANE_STATUS_INVAL);
/*      status = sanei_constrain_value (s->options_list[option], val, info);*/
      if (status != SANE_STATUS_GOOD)
	return (status);
      switch (option)
	{
	  /* (mostly) side-effect-free word options: */
	case OPT_RESOLUTION:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_TL_X:
	case OPT_TL_Y:
	  if (info && s->value[option].w != *(SANE_Word *) val)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  /* fall through */
	case OPT_NUM_OPTS:
	case OPT_CONTRAST:
	case OPT_BRIGHTNESS:
	  s->value[option].w = *(SANE_Word *) val;
	  DBG (1, "set brightness to\n");
	  return (SANE_STATUS_GOOD);
	case OPT_MODE:
	  if (s->value[option].s)
	    free (s->value[option].s);
	  s->value[option].s = strdup (val);
	  return (SANE_STATUS_GOOD);
	}
    }
  return (SANE_STATUS_INVAL);
}

/*--------------------------------------------------------------------------*/
const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  AS6E_Scan *s = handle;
  DBG (2, "sane_get_option_descriptor\n");
  if ((unsigned) option >= NUM_OPTIONS)
    return (0);
  return (&s->options_list[option]);
}

/*--------------------------------------------------------------------------*/

void
sane_close (SANE_Handle handle)
{
  AS6E_Scan *prev, *s;
  SANE_Word repeat = 0;
  DBG (2, "sane_close\n");
  /* remove handle from list of open handles: */
  prev = 0;
  for (s = first_handle; s; s = s->next)
    {
      if (s == handle)
	break;
      prev = s;
    }
  if (!s)
    {
      DBG (1, "close: invalid handle %p\n", handle);
      return;			/* oops, not a handle we know about */
    }

  if (s->scanning)
    sane_cancel (handle);
  write (s->as6e_params.ctloutpipe, &repeat, sizeof (repeat));
  close (s->as6e_params.ctloutpipe);
  free (s->scan_buffer);
  free (s->line_buffer);
  if (prev)
    prev->next = s->next;
  else
    first_handle = s;
  free (handle);
}

/*--------------------------------------------------------------------------*/
void
sane_exit (void)
{
  AS6E_Device *next;
  DBG (2, "sane_exit\n");
  while (first_dev != NULL)
    {
      next = first_dev->next;
      free (first_dev);
      first_dev = next;
    }
  if (devlist)
    free (devlist);
}

/*--------------------------------------------------------------------------*/
static SANE_Status
as6e_open (AS6E_Scan * s)
{

  int data_processed, exec_result, as6e_status;
  int ctloutpipe[2], ctlinpipe[2], datapipe[2];
  char inpipe_desc[32], outpipe_desc[32], datapipe_desc[32];
  pid_t fork_result;
  DBG (1, "as6e_open\n");
  memset (inpipe_desc, '\0', sizeof (inpipe_desc));
  memset (outpipe_desc, '\0', sizeof (outpipe_desc));
  memset (datapipe_desc, '\0', sizeof (datapipe_desc));
  if ((pipe (ctloutpipe) == 0) && (pipe (ctlinpipe) == 0)
      && (pipe (datapipe) == 0))
    {
      fork_result = fork ();
      if (fork_result == (pid_t) - 1)
	{
	  DBG (1, "Fork failure");
	  return (SANE_STATUS_IO_ERROR);
	}

      if (fork_result == 0)
	{			/*in child */
	  sprintf (inpipe_desc, "%d", ctlinpipe[WRITEPIPE]);
	  sprintf (outpipe_desc, "%d", ctloutpipe[READPIPE]);
	  sprintf (datapipe_desc, "%d", datapipe[WRITEPIPE]);
	  exec_result =
	    execlp ("as6edriver", "as6edriver", "-s", inpipe_desc,
		    outpipe_desc, datapipe_desc, (char *) 0);
	  DBG (1, "The SANE backend was unable to start \"as6edriver\".\n");
	  DBG (1, "This must be installed in a driectory in your PATH.\n");
	  DBG (1, "To aquire the as6edriver program,\n");
	  DBG (1, "go to http://as6edriver.sourceforge.net.\n");
	  write (ctlinpipe[WRITEPIPE], &exec_result, sizeof (exec_result));
	  exit (-1);
	}
      else
	{			/*parent process */
	  data_processed =
	    read (ctlinpipe[READPIPE], &as6e_status, sizeof (as6e_status));
	  DBG (1, "%d - read %d status = %d\n", getpid (), data_processed,
	       as6e_status);
	  if (as6e_status == -2)
	    {
	      DBG (1, "Port access denied.\n");
	      return (SANE_STATUS_IO_ERROR);
	    }
	  if (as6e_status == -1)
	    {
	      DBG (1, "Could not contact scanner.\n");
	      return (SANE_STATUS_IO_ERROR);
	    }

	  if (as6e_status == 1)
	    DBG (1, "Using nibble mode.\n");
	  if (as6e_status == 2)
	    DBG (1, "Using byte mode.\n");
	  if (as6e_status == 3)
	    DBG (1, "Using EPP mode.\n");
	  s->as6e_params.ctlinpipe = ctlinpipe[READPIPE];
	  s->as6e_params.ctloutpipe = ctloutpipe[WRITEPIPE];
	  s->as6e_params.datapipe = datapipe[READPIPE];
	  s->child_pid = fork_result;
	  return (SANE_STATUS_GOOD);
	}			/*else */
    }
  else
    return (SANE_STATUS_IO_ERROR);
}


/*--------------------------------------------------------------------------*/
SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  char dev_name[PATH_MAX];
  size_t len;
  FILE *fp = NULL;

  DBG_INIT ();
  DBG (2, "sane_init (authorize %s null)\n", (authorize) ? "!=" : "==");
  if (version_code)
    *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, 0);
/*  fp = sanei_config_open (AS6E_CONFIG_FILE);*/
  if (!fp)
    {
      return (attach ("as6edriver", 0));
    }

  while (fgets (dev_name, sizeof (dev_name), fp))
    {
      if (dev_name[0] == '#')	/* ignore line comments */
	continue;
      len = strlen (dev_name);
      if (dev_name[len - 1] == '\n')
	dev_name[--len] = '\0';
      if (!len)
	continue;		/* ignore empty lines */
/*      sanei_config_attach_matching_devices (dev_name, attach_one);*/
    }
  fclose (fp);
  return (SANE_STATUS_GOOD);
}

/*--------------------------------------------------------------------------*/
/*
static SANE_Status
attach_one (const char *dev)
{
  DBG (2, "attach_one\n");
  attach (dev, 0);
  return (SANE_STATUS_GOOD);
}
  */
/*--------------------------------------------------------------------------*/
SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  static const SANE_Device **devlist = 0;
  AS6E_Device *dev;
  int i;
  DBG (3, "sane_get_devices (local_only = %d)\n", local_only);
  if (devlist)
    free (devlist);
  devlist = malloc ((num_devices + 1) * sizeof (devlist[0]));
  if (!devlist)
    return SANE_STATUS_NO_MEM;
  i = 0;
  for (dev = first_dev; i < num_devices; dev = dev->next)
    devlist[i++] = &dev->sane;
  devlist[i++] = 0;
  *device_list = devlist;
  return (SANE_STATUS_GOOD);
}


/*--------------------------------------------------------------------------*/

static size_t
max_string_size (const SANE_String_Const strings[])
{
  size_t size, max_size = 0;
  int i;
  for (i = 0; strings[i]; ++i)
    {
      size = strlen (strings[i]) + 1;
      if (size > max_size)
	max_size = size;
    }

  return (max_size);
}

/*--------------------------------------------------------------------------*/

static void
initialize_options_list (AS6E_Scan * s)
{

  SANE_Int option;
  DBG (2, "initialize_options_list\n");
  for (option = 0; option < NUM_OPTIONS; ++option)
    {
      s->options_list[option].size = sizeof (SANE_Word);
      s->options_list[option].cap =
	SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }

  s->options_list[OPT_NUM_OPTS].name = SANE_NAME_NUM_OPTIONS;
  s->options_list[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  s->options_list[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
  s->options_list[OPT_NUM_OPTS].type = SANE_TYPE_INT;
  s->options_list[OPT_NUM_OPTS].unit = SANE_UNIT_NONE;
  s->options_list[OPT_NUM_OPTS].size = sizeof (SANE_Word);
  s->options_list[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
  s->options_list[OPT_NUM_OPTS].constraint_type = SANE_CONSTRAINT_NONE;
  s->value[OPT_NUM_OPTS].w = NUM_OPTIONS;
  s->options_list[OPT_MODE].name = SANE_NAME_SCAN_MODE;
  s->options_list[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  s->options_list[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
  s->options_list[OPT_MODE].type = SANE_TYPE_STRING;
  s->options_list[OPT_MODE].size = max_string_size (mode_list);
  s->options_list[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->options_list[OPT_MODE].constraint.string_list = mode_list;
  s->value[OPT_MODE].s = strdup (mode_list[2]);
  s->options_list[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  s->options_list[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  s->options_list[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  s->options_list[OPT_RESOLUTION].type = SANE_TYPE_INT;
  s->options_list[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
  s->options_list[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  s->options_list[OPT_RESOLUTION].constraint.word_list = resolution_list;
  s->value[OPT_RESOLUTION].w = 200;
  s->options_list[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  s->options_list[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  s->options_list[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  s->options_list[OPT_TL_X].type = SANE_TYPE_FIXED;
  s->options_list[OPT_TL_X].unit = SANE_UNIT_MM;
  s->options_list[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->options_list[OPT_TL_X].constraint.range = &x_range;
  s->value[OPT_TL_X].w = s->options_list[OPT_TL_X].constraint.range->min;
  s->options_list[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  s->options_list[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  s->options_list[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  s->options_list[OPT_TL_Y].type = SANE_TYPE_FIXED;
  s->options_list[OPT_TL_Y].unit = SANE_UNIT_MM;
  s->options_list[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->options_list[OPT_TL_Y].constraint.range = &y_range;
  s->value[OPT_TL_Y].w = s->options_list[OPT_TL_Y].constraint.range->min;
  s->options_list[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  s->options_list[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  s->options_list[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  s->options_list[OPT_BR_X].type = SANE_TYPE_FIXED;
  s->options_list[OPT_BR_X].unit = SANE_UNIT_MM;
  s->options_list[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->options_list[OPT_BR_X].constraint.range = &x_range;
  s->value[OPT_BR_X].w = s->options_list[OPT_BR_X].constraint.range->max;
  s->options_list[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  s->options_list[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  s->options_list[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  s->options_list[OPT_BR_Y].type = SANE_TYPE_FIXED;
  s->options_list[OPT_BR_Y].unit = SANE_UNIT_MM;
  s->options_list[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->options_list[OPT_BR_Y].constraint.range = &y_range;
  s->value[OPT_BR_Y].w = s->options_list[OPT_BR_Y].constraint.range->max;
  s->options_list[OPT_CONTRAST].name = SANE_NAME_CONTRAST;
  s->options_list[OPT_CONTRAST].title = SANE_TITLE_CONTRAST;
  s->options_list[OPT_CONTRAST].desc = SANE_DESC_CONTRAST;
  s->options_list[OPT_CONTRAST].type = SANE_TYPE_INT;
  s->options_list[OPT_CONTRAST].unit = SANE_UNIT_NONE;
  s->options_list[OPT_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
  s->options_list[OPT_CONTRAST].constraint.range = &brightness_range;
  s->value[OPT_BRIGHTNESS].w = 10;
  s->options_list[OPT_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
  s->options_list[OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
  s->options_list[OPT_BRIGHTNESS].desc = SANE_DESC_BRIGHTNESS;
  s->options_list[OPT_BRIGHTNESS].type = SANE_TYPE_INT;
  s->options_list[OPT_BRIGHTNESS].unit = SANE_UNIT_NONE;
  s->options_list[OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
  s->options_list[OPT_BRIGHTNESS].constraint.range = &contrast_range;
  s->value[OPT_CONTRAST].w = -32;
}

/*--------------------------------------------------------------------------*/
static int
check_for_driver (const char *devname)
{
#define NAMESIZE 128
  struct stat statbuf;
  mode_t modes;
  char *path;
  char fullname[NAMESIZE];
  char dir[NAMESIZE];
  int count = 0, offset = 0;

  path = getenv ("PATH");
  if (!path)
    return 0;
  while (path[count] != '\0')
    {
      memset (fullname, '\0', sizeof (fullname));
      memset (dir, '\0', sizeof (dir));
      while ((path[count] != ':') && (path[count] != '\0'))
	{
	  dir[count - offset] = path[count];
	  count++;
	}
      /* use sizeof(fullname)-1 to make sure there is at least one padded null byte */
      strncpy (fullname, dir, sizeof(fullname)-1);
      /* take into account that fullname already contains non-null bytes */
      strncat (fullname, "/", sizeof(fullname)-strlen(fullname)-1);
      strncat (fullname, devname, sizeof(fullname)-strlen(fullname)-1);
      if (!stat (fullname, &statbuf))
	{
	  modes = statbuf.st_mode;
	  if (S_ISREG (modes))
	    return (1);		/* found as6edriver */
	}
      if (path[count] == '\0')
	return (0);		/* end of path --no driver found */
      count++;
      offset = count;
    }
  return (0);
}


/*--------------------------------------------------------------------------*/
static SANE_Status
attach (const char *devname, AS6E_Device ** devp)
{

  AS6E_Device *dev;

/*  SANE_Status status;  */
  DBG (2, "attach\n");
  for (dev = first_dev; dev; dev = dev->next)
    {
      if (strcmp (dev->sane.name, devname) == 0)
	{
	  if (devp)
	    *devp = dev;
	  return (SANE_STATUS_GOOD);
	}
    }
  dev = malloc (sizeof (*dev));
  if (!dev)
    return (SANE_STATUS_NO_MEM);
  memset (dev, 0, sizeof (*dev));
  dev->sane.name = strdup (devname);
  if (!check_for_driver (devname))
    {
      free (dev);
      return (SANE_STATUS_INVAL);
    }

  dev->sane.model = "AS6E";
  dev->sane.vendor = "Artec";
  dev->sane.type = "flatbed scanner";
  ++num_devices;
  dev->next = first_dev;
  first_dev = dev;
  if (devp)
    *devp = dev;
  return (SANE_STATUS_GOOD);
}


/*--------------------------------------------------------------------------*/
SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle * handle)
{
  SANE_Status status;
  AS6E_Device *dev;
  AS6E_Scan *s;
  DBG (2, "sane_open\n");
  if (devicename[0])
    {
      for (dev = first_dev; dev; dev = dev->next)
	if (strcmp (dev->sane.name, devicename) == 0)
	  break;
      if (!dev)
	{
	  status = attach (devicename, &dev);
	  if (status != SANE_STATUS_GOOD)
	    return (status);
	}
    }
  else
    {
      /* empty devicname -> use first device */
      dev = first_dev;
    }
  if (!dev)
    return SANE_STATUS_INVAL;
  s = malloc (sizeof (*s));
  if (!s)
    return SANE_STATUS_NO_MEM;
  memset (s, 0, sizeof (*s));
  s->scan_buffer = malloc (SCAN_BUF_SIZE);
  if (!s->scan_buffer)
    return SANE_STATUS_NO_MEM;
  memset (s->scan_buffer, 0, SCAN_BUF_SIZE);
  s->line_buffer = malloc (SCAN_BUF_SIZE);
  if (!s->line_buffer)
    return SANE_STATUS_NO_MEM;
  memset (s->line_buffer, 0, SCAN_BUF_SIZE);
  status = as6e_open (s);
  if (status != SANE_STATUS_GOOD)
    return status;
  initialize_options_list (s);
  s->scanning = 0;
  /* insert newly opened handle into list of open handles: */
  s->next = first_handle;
  first_handle = s;
  *handle = s;
  return (status);
}

/*--------------------------------------------------------------------------*/
SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  DBG (2, "sane_set_io_mode( %p, %d )\n", handle, non_blocking);
  return (SANE_STATUS_UNSUPPORTED);
}

/*---------------------------------------------------------------------------*/
SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int * fd)
{
  DBG (2, "sane_get_select_fd( %p, %p )\n",(void *)  handle, (void *) fd);
  return SANE_STATUS_UNSUPPORTED;
}
