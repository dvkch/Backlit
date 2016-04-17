/* sane - Scanner Access Now Easy.
   Copyright (C) 2000-2003 Jochen Eisinger <jochen.eisinger@gmx.net>
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

   This file implements a SANE backend for Mustek PP flatbed scanners.  */

#include "../include/sane/config.h"

#if defined(HAVE_STDLIB_H)
# include <stdlib.h>
#endif
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#if defined(HAVE_STRING_H)
# include <string.h>
#elif defined(HAVE_STRINGS_H)
# include <strings.h>
#endif
#if defined(HAVE_UNISTD_H)
# include <unistd.h>
#endif
#include <math.h>
#include <fcntl.h>
#include <time.h>
#if defined(HAVE_SYS_TIME_H)
# include <sys/time.h>
#endif
#if defined(HAVE_SYS_TYPES_H)
# include <sys/types.h>
#endif
#include <sys/wait.h>

#define BACKEND_NAME	mustek_pp

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"

#include "../include/sane/sanei_backend.h"

#include "../include/sane/sanei_config.h"
#define MUSTEK_PP_CONFIG_FILE "mustek_pp.conf"

#include "../include/sane/sanei_pa4s2.h"

#include "mustek_pp.h"
#include "mustek_pp_drivers.h"

#define MIN(a,b)	((a) < (b) ? (a) : (b))
   
/* converts millimeter to pixels at a given resolution */
#define	MM_TO_PIXEL(mm, dpi)	(((float )mm * 5.0 / 127.0) * (float)dpi)
   /* and back */
#define PIXEL_TO_MM(pixel, dpi) (((float )pixel / (float )dpi) * 127.0 / 5.0)

/* if you change the source, please set MUSTEK_PP_STATE to "devel". Do *not*
 * change the MUSTEK_PP_BUILD. */
#define MUSTEK_PP_BUILD	13
#define MUSTEK_PP_STATE	"beta"


/* auth callback... since basic user authentication is done by saned, this
 * callback mechanism isn't used */
SANE_Auth_Callback sane_auth;

/* count of present devices */
static int num_devices = 0;

/* list of present devices */
static Mustek_pp_Device *devlist = NULL;

/* temporary array of configuration options used during device attachment */
static Mustek_pp_config_option *cfgoptions = NULL;
static int numcfgoptions = 0;

/* list of pointers to the SANE_Device structures of the Mustek_pp_Devices */
static SANE_Device **devarray = NULL;

/* currently active Handles */
static Mustek_pp_Handle *first_hndl = NULL;

static SANE_String_Const       mustek_pp_modes[4] = {SANE_VALUE_SCAN_MODE_LINEART, SANE_VALUE_SCAN_MODE_GRAY, SANE_VALUE_SCAN_MODE_COLOR, NULL};
static SANE_Word               mustek_pp_modes_size = 10;
 
static SANE_String_Const       mustek_pp_speeds[6] = {"Slowest", "Slower", "Normal", "Faster", "Fastest", NULL};
static SANE_Word               mustek_pp_speeds_size = 8;
static SANE_Word               mustek_pp_depths[5] = {4, 8, 10, 12, 16};   

/* prototypes */
static void free_cfg_options(int *numoptions, Mustek_pp_config_option** options);
static SANE_Status do_eof(Mustek_pp_Handle *hndl);
static SANE_Status do_stop(Mustek_pp_Handle *hndl);
static int reader_process (Mustek_pp_Handle * hndl, int pipe);
static SANE_Status sane_attach(SANE_String_Const port, SANE_String_Const name, 
			SANE_Int driver, SANE_Int info);
static void init_options(Mustek_pp_Handle *hndl);
static void attach_device(SANE_String *driver, SANE_String *name, 
		   SANE_String *port, SANE_String *option_ta);


/*
 * Auxiliary function for freeing arrays of configuration options, 
 */
static void 
free_cfg_options(int *numoptions, Mustek_pp_config_option** options)
{
   int i;
   if (*numoptions)
   {
      for (i=0; i<*numoptions; ++i)
      {
         free ((*options)[i].name);
         free ((*options)[i].value);
      }
      free (*options);
   }
   *options = NULL;
   *numoptions = 0;
}

/* do_eof:
 * 	closes the pipeline
 *
 * ChangeLog:
 *
 * Description:
 * 	closes the pipe (read-only end)
 */
static SANE_Status
do_eof (Mustek_pp_Handle *hndl)
{
	if (hndl->pipe >= 0) {

		close (hndl->pipe);
		hndl->pipe = -1;
	}

	return SANE_STATUS_EOF;
}

/* do_stop:
 * 	ends the reader_process and stops the scanner
 *
 * ChangeLog:
 *
 * Description:
 * 	kills the reader process with a SIGTERM and cancels the scanner
 */
static SANE_Status
do_stop(Mustek_pp_Handle *hndl)
{

	int	exit_status;

	do_eof (hndl);

	if (hndl->reader > 0) {

		DBG (3, "do_stop: terminating reader process\n");
		kill (hndl->reader, SIGTERM);

		while (wait (&exit_status) != hndl->reader);

		DBG ((exit_status == SANE_STATUS_GOOD ? 3 : 1),
			       "do_stop: reader_process terminated with status ``%s''\n",
			       sane_strstatus(exit_status));
		hndl->reader = 0;
		hndl->dev->func->stop (hndl);

		return exit_status;

	}

	hndl->dev->func->stop (hndl);

	return SANE_STATUS_GOOD;
}

/* sigterm_handler:
 * 	cancel scanner when receiving a SIGTERM
 *
 * ChangeLog:
 *
 * Description:
 *	just exit... reader_process takes care that nothing bad will happen
 *
 * EDG - Jan 14, 2004:
 *      Make sure that the parport is released again by the child process
 *      under all circumstances, because otherwise the parent process may no 
 *      longer be able to claim it (they share the same file descriptor, and
 *      the kernel doesn't release the child's claim because the file
 *      descriptor isn't cleaned up). If that would happen, the lamp may stay
 *      on and may not return to its home position, unless the scanner
 *      frontend is restarted.
 *      (This happens only when sanei_pa4s2 uses libieee1284 AND
 *      libieee1284 goes via /dev/parportX).
 *
 */
static int fd_to_release = 0;
/*ARGSUSED*/
static RETSIGTYPE
sigterm_handler (int signal __UNUSED__)
{
	sanei_pa4s2_enable(fd_to_release, SANE_FALSE);
	_exit (SANE_STATUS_GOOD);
}

/* reader_process:
 * 	receives data from the scanner and stuff it into the pipeline
 *
 * ChangeLog:
 *
 * Description:
 * 	The signal handle for SIGTERM is initialized.
 *
 */
static int
reader_process (Mustek_pp_Handle * hndl, int pipe)
{
	sigset_t	sigterm_set;
	struct SIGACTION act;
	FILE *fp;
	SANE_Status status;
	int line;
	int size, elem;

	SANE_Byte *buffer;

	sigemptyset (&sigterm_set);
	sigaddset (&sigterm_set, SIGTERM);
	
	if (!(buffer = malloc (hndl->params.bytes_per_line)))
		return SANE_STATUS_NO_MEM;
	
	if (!(fp = fdopen(pipe, "w")))
		return SANE_STATUS_IO_ERROR;
        
	fd_to_release = hndl->fd;
	memset (&act, 0, sizeof(act));
	act.sa_handler = sigterm_handler;
	sigaction (SIGTERM, &act, NULL);

	if ((status = hndl->dev->func->start (hndl)) != SANE_STATUS_GOOD)
		return status;

        size = hndl->params.bytes_per_line;
  	elem = 1;

	for (line=0; line<hndl->params.lines ; line++) {

		sigprocmask (SIG_BLOCK, &sigterm_set, NULL);

		hndl->dev->func->read (hndl, buffer);
                
                if (getppid() == 1) {
                    /* The parent process has died. Stop the scan (to make
                       sure that the lamp is off and returns home). This is
                       a safety measure to make sure that we don't break 
                       the scanner in case the frontend crashes. */
		    DBG (1, "reader_process: front-end died; aborting.\n");
                    hndl->dev->func->stop (hndl);
                    return SANE_STATUS_CANCELLED;
                }

		sigprocmask (SIG_UNBLOCK, &sigterm_set, NULL);

		fwrite (buffer, size, elem, fp);
	}

	fclose (fp);

	free (buffer);

	return SANE_STATUS_GOOD;
}
		


/* sane_attach:
 * 	adds a new entry to the Mustek_pp_Device *devlist list
 *
 * ChangeLog:
 *
 * Description:
 * 	After memory for a new device entry is allocated, the
 * 	parameters for the device are determined by a call to
 * 	capabilities().
 *
 * 	Afterwards the new device entry is inserted into the
 * 	devlist
 *
 */
static SANE_Status
sane_attach (SANE_String_Const port, SANE_String_Const name, SANE_Int driver, SANE_Int info)
{
	Mustek_pp_Device	*dev;

	DBG (3, "sane_attach: attaching device ``%s'' to port %s (driver %s v%s by %s)\n", 
			name, port, Mustek_pp_Drivers[driver].driver,
				Mustek_pp_Drivers[driver].version,
				Mustek_pp_Drivers[driver].author);

	if ((dev = malloc (sizeof (Mustek_pp_Device))) == NULL) {

		DBG (1, "sane_attach: not enough free memory\n");
		return SANE_STATUS_NO_MEM;

	}

	memset (dev, 0, sizeof (Mustek_pp_Device));

	memset (&dev->sane, 0, sizeof (SANE_Device));

	dev->func = &Mustek_pp_Drivers[driver];

	dev->sane.name = dev->name = strdup (name);
	dev->port = strdup (port);
        dev->info = info; /* Modified by EDG */
        
        /* Transfer the options parsed from the configuration file */
        dev->numcfgoptions = numcfgoptions;
        dev->cfgoptions = cfgoptions;
        numcfgoptions = 0;
        cfgoptions = NULL;

	dev->func->capabilities (info, &dev->model, &dev->vendor, &dev->type,
			&dev->maxres, &dev->minres, &dev->maxhsize, &dev->maxvsize,
			&dev->caps);

	dev->sane.model = dev->model;
	dev->sane.vendor = dev->vendor;
	dev->sane.type = dev->type;

	dev->next = devlist;
	devlist = dev;

	num_devices++;

	return SANE_STATUS_GOOD;
}


/* init_options:
 * 	Sets up the option descriptors for a device
 *
 * ChangeLog:
 *
 * Description:
 */
static void
init_options(Mustek_pp_Handle *hndl)
{
  int i;

  memset (hndl->opt, 0, sizeof (hndl->opt));
  memset (hndl->val, 0, sizeof (hndl->val));

  for (i = 0; i < NUM_OPTIONS; ++i)
    {
      hndl->opt[i].size = sizeof (SANE_Word);
      hndl->opt[i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
    }

  hndl->opt[OPT_NUM_OPTS].name = SANE_NAME_NUM_OPTIONS;
  hndl->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  hndl->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
  hndl->opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
  hndl->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
  hndl->val[OPT_NUM_OPTS].w = NUM_OPTIONS;

  /* "Mode" group: */

  hndl->opt[OPT_MODE_GROUP].title = "Scan Mode";
  hndl->opt[OPT_MODE_GROUP].desc = "";
  hndl->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
  hndl->opt[OPT_MODE_GROUP].cap = 0;
  hndl->opt[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;
  hndl->opt[OPT_MODE_GROUP].size = 0;

  /* scan mode */
  hndl->opt[OPT_MODE].name = SANE_NAME_SCAN_MODE;
  hndl->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  hndl->opt[OPT_MODE].desc = SANE_DESC_SCAN_MODE;
  hndl->opt[OPT_MODE].type = SANE_TYPE_STRING;
  hndl->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  hndl->opt[OPT_MODE].size = mustek_pp_modes_size;
  hndl->opt[OPT_MODE].constraint.string_list = mustek_pp_modes;
  hndl->val[OPT_MODE].s = strdup (mustek_pp_modes[2]);

  /* resolution */
  hndl->opt[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  hndl->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  hndl->opt[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  hndl->opt[OPT_RESOLUTION].type = SANE_TYPE_FIXED;
  hndl->opt[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
  hndl->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_RANGE;
  hndl->opt[OPT_RESOLUTION].constraint.range = &hndl->dpi_range;
  hndl->val[OPT_RESOLUTION].w = SANE_FIX (hndl->dev->minres);
  hndl->dpi_range.min = SANE_FIX (hndl->dev->minres);
  hndl->dpi_range.max = SANE_FIX (hndl->dev->maxres);
  hndl->dpi_range.quant = SANE_FIX (1);

  /* speed */
  hndl->opt[OPT_SPEED].name = SANE_NAME_SCAN_SPEED;
  hndl->opt[OPT_SPEED].title = SANE_TITLE_SCAN_SPEED;
  hndl->opt[OPT_SPEED].desc = SANE_DESC_SCAN_SPEED;
  hndl->opt[OPT_SPEED].type = SANE_TYPE_STRING;
  hndl->opt[OPT_SPEED].size = mustek_pp_speeds_size;
  hndl->opt[OPT_SPEED].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  hndl->opt[OPT_SPEED].constraint.string_list = mustek_pp_speeds;
  hndl->val[OPT_SPEED].s = strdup (mustek_pp_speeds[2]);

  if (! (hndl->dev->caps & CAP_SPEED_SELECT))
	  hndl->opt[OPT_SPEED].cap |= SANE_CAP_INACTIVE;

  /* preview */
  hndl->opt[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
  hndl->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
  hndl->opt[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
  hndl->opt[OPT_PREVIEW].cap = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
  hndl->val[OPT_PREVIEW].w = SANE_FALSE;

  /* gray preview */
  hndl->opt[OPT_GRAY_PREVIEW].name = SANE_NAME_GRAY_PREVIEW;
  hndl->opt[OPT_GRAY_PREVIEW].title = SANE_TITLE_GRAY_PREVIEW;
  hndl->opt[OPT_GRAY_PREVIEW].desc = SANE_DESC_GRAY_PREVIEW;
  hndl->opt[OPT_GRAY_PREVIEW].type = SANE_TYPE_BOOL;
  hndl->val[OPT_GRAY_PREVIEW].w = SANE_FALSE;

  /* color dept */
  hndl->opt[OPT_DEPTH].name = SANE_NAME_BIT_DEPTH;
  hndl->opt[OPT_DEPTH].title = SANE_TITLE_BIT_DEPTH;
  hndl->opt[OPT_DEPTH].desc = 
	  "Number of bits per sample for color scans, typical values are 8 for truecolor (24bpp)"
	  "up to 16 for far-to-many-color (48bpp).";
  hndl->opt[OPT_DEPTH].type = SANE_TYPE_INT;
  hndl->opt[OPT_DEPTH].constraint_type = SANE_CONSTRAINT_WORD_LIST;
  hndl->opt[OPT_DEPTH].constraint.word_list = mustek_pp_depths;
  hndl->opt[OPT_DEPTH].unit = SANE_UNIT_BIT;
  hndl->opt[OPT_DEPTH].size = sizeof(SANE_Word);
  hndl->val[OPT_DEPTH].w = 8;

  if ( !(hndl->dev->caps & CAP_DEPTH))
	  hndl->opt[OPT_DEPTH].cap |= SANE_CAP_INACTIVE;


  /* "Geometry" group: */

  hndl->opt[OPT_GEOMETRY_GROUP].title = "Geometry";
  hndl->opt[OPT_GEOMETRY_GROUP].desc = "";
  hndl->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
  hndl->opt[OPT_GEOMETRY_GROUP].cap = SANE_CAP_ADVANCED;
  hndl->opt[OPT_GEOMETRY_GROUP].constraint_type = SANE_CONSTRAINT_NONE;
  hndl->opt[OPT_GEOMETRY_GROUP].size = 0;

  /* top-left x */
  hndl->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  hndl->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  hndl->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  hndl->opt[OPT_TL_X].type = SANE_TYPE_FIXED;
  hndl->opt[OPT_TL_X].unit = SANE_UNIT_MM;
  hndl->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  hndl->opt[OPT_TL_X].constraint.range = &hndl->x_range;
  hndl->x_range.min = SANE_FIX (0);
  hndl->x_range.max = SANE_FIX (PIXEL_TO_MM(hndl->dev->maxhsize,hndl->dev->maxres));
  hndl->x_range.quant = 0;
  hndl->val[OPT_TL_X].w = hndl->x_range.min;

  /* top-left y */
  hndl->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  hndl->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  hndl->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  hndl->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
  hndl->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
  hndl->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  hndl->opt[OPT_TL_Y].constraint.range = &hndl->y_range;
  hndl->y_range.min = SANE_FIX(0);
  hndl->y_range.max = SANE_FIX(PIXEL_TO_MM(hndl->dev->maxvsize,hndl->dev->maxres));
  hndl->y_range.quant = 0;
  hndl->val[OPT_TL_Y].w = hndl->y_range.min;

  /* bottom-right x */
  hndl->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  hndl->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  hndl->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  hndl->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
  hndl->opt[OPT_BR_X].unit = SANE_UNIT_MM;
  hndl->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  hndl->opt[OPT_BR_X].constraint.range = &hndl->x_range;
  hndl->val[OPT_BR_X].w = hndl->x_range.max;

  /* bottom-right y */
  hndl->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  hndl->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  hndl->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  hndl->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
  hndl->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
  hndl->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  hndl->opt[OPT_BR_Y].constraint.range = &hndl->y_range;
  hndl->val[OPT_BR_Y].w = hndl->y_range.max;

  /* "Enhancement" group: */

  hndl->opt[OPT_ENHANCEMENT_GROUP].title = "Enhancement";
  hndl->opt[OPT_ENHANCEMENT_GROUP].desc = "";
  hndl->opt[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
  hndl->opt[OPT_ENHANCEMENT_GROUP].cap = 0;
  hndl->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;
  hndl->opt[OPT_ENHANCEMENT_GROUP].size = 0;


  /* custom-gamma table */
  hndl->opt[OPT_CUSTOM_GAMMA].name = SANE_NAME_CUSTOM_GAMMA;
  hndl->opt[OPT_CUSTOM_GAMMA].title = SANE_TITLE_CUSTOM_GAMMA;
  hndl->opt[OPT_CUSTOM_GAMMA].desc = SANE_DESC_CUSTOM_GAMMA;
  hndl->opt[OPT_CUSTOM_GAMMA].type = SANE_TYPE_BOOL;
  hndl->val[OPT_CUSTOM_GAMMA].w = SANE_FALSE;

  if ( !(hndl->dev->caps & CAP_GAMMA_CORRECT))
	  hndl->opt[OPT_CUSTOM_GAMMA].cap |= SANE_CAP_INACTIVE;

  /* grayscale gamma vector */
  hndl->opt[OPT_GAMMA_VECTOR].name = SANE_NAME_GAMMA_VECTOR;
  hndl->opt[OPT_GAMMA_VECTOR].title = SANE_TITLE_GAMMA_VECTOR;
  hndl->opt[OPT_GAMMA_VECTOR].desc = SANE_DESC_GAMMA_VECTOR;
  hndl->opt[OPT_GAMMA_VECTOR].type = SANE_TYPE_INT;
  hndl->opt[OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE;
  hndl->opt[OPT_GAMMA_VECTOR].unit = SANE_UNIT_NONE;
  hndl->opt[OPT_GAMMA_VECTOR].size = 256 * sizeof (SANE_Word);
  hndl->opt[OPT_GAMMA_VECTOR].constraint_type = SANE_CONSTRAINT_RANGE;
  hndl->opt[OPT_GAMMA_VECTOR].constraint.range = &hndl->gamma_range;
  hndl->val[OPT_GAMMA_VECTOR].wa = &hndl->gamma_table[0][0];

  /* red gamma vector */
  hndl->opt[OPT_GAMMA_VECTOR_R].name = SANE_NAME_GAMMA_VECTOR_R;
  hndl->opt[OPT_GAMMA_VECTOR_R].title = SANE_TITLE_GAMMA_VECTOR_R;
  hndl->opt[OPT_GAMMA_VECTOR_R].desc = SANE_DESC_GAMMA_VECTOR_R;
  hndl->opt[OPT_GAMMA_VECTOR_R].type = SANE_TYPE_INT;
  hndl->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
  hndl->opt[OPT_GAMMA_VECTOR_R].unit = SANE_UNIT_NONE;
  hndl->opt[OPT_GAMMA_VECTOR_R].size = 256 * sizeof (SANE_Word);
  hndl->opt[OPT_GAMMA_VECTOR_R].constraint_type = SANE_CONSTRAINT_RANGE;
  hndl->opt[OPT_GAMMA_VECTOR_R].constraint.range = &hndl->gamma_range;
  hndl->val[OPT_GAMMA_VECTOR_R].wa = &hndl->gamma_table[1][0];

  /* green gamma vector */
  hndl->opt[OPT_GAMMA_VECTOR_G].name = SANE_NAME_GAMMA_VECTOR_G;
  hndl->opt[OPT_GAMMA_VECTOR_G].title = SANE_TITLE_GAMMA_VECTOR_G;
  hndl->opt[OPT_GAMMA_VECTOR_G].desc = SANE_DESC_GAMMA_VECTOR_G;
  hndl->opt[OPT_GAMMA_VECTOR_G].type = SANE_TYPE_INT;
  hndl->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
  hndl->opt[OPT_GAMMA_VECTOR_G].unit = SANE_UNIT_NONE;
  hndl->opt[OPT_GAMMA_VECTOR_G].size = 256 * sizeof (SANE_Word);
  hndl->opt[OPT_GAMMA_VECTOR_G].constraint_type = SANE_CONSTRAINT_RANGE;
  hndl->opt[OPT_GAMMA_VECTOR_G].constraint.range = &hndl->gamma_range;
  hndl->val[OPT_GAMMA_VECTOR_G].wa = &hndl->gamma_table[2][0];

  /* blue gamma vector */
  hndl->opt[OPT_GAMMA_VECTOR_B].name = SANE_NAME_GAMMA_VECTOR_B;
  hndl->opt[OPT_GAMMA_VECTOR_B].title = SANE_TITLE_GAMMA_VECTOR_B;
  hndl->opt[OPT_GAMMA_VECTOR_B].desc = SANE_DESC_GAMMA_VECTOR_B;
  hndl->opt[OPT_GAMMA_VECTOR_B].type = SANE_TYPE_INT;
  hndl->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
  hndl->opt[OPT_GAMMA_VECTOR_B].unit = SANE_UNIT_NONE;
  hndl->opt[OPT_GAMMA_VECTOR_B].size = 256 * sizeof (SANE_Word);
  hndl->opt[OPT_GAMMA_VECTOR_B].constraint_type = SANE_CONSTRAINT_RANGE;
  hndl->opt[OPT_GAMMA_VECTOR_B].constraint.range = &hndl->gamma_range;
  hndl->val[OPT_GAMMA_VECTOR_B].wa = &hndl->gamma_table[3][0];

  hndl->gamma_range.min = 0;
  hndl->gamma_range.max = 255;
  hndl->gamma_range.quant = 1;

  hndl->opt[OPT_INVERT].name = SANE_NAME_NEGATIVE;
  hndl->opt[OPT_INVERT].title = SANE_TITLE_NEGATIVE;
  hndl->opt[OPT_INVERT].desc = SANE_DESC_NEGATIVE;
  hndl->opt[OPT_INVERT].type = SANE_TYPE_BOOL;
  hndl->val[OPT_INVERT].w = SANE_FALSE;

  if (! (hndl->dev->caps & CAP_INVERT))
	  hndl->opt[OPT_INVERT].cap |= SANE_CAP_INACTIVE;


}

/* attach_device:
 * 	Attempts to attach a device to the list after parsing of a section
 *      of the configuration file.
 *
 * ChangeLog:
 *
 * Description:
 *      After parsing a scanner section of the config file, this function
 *      is called to look for a driver with a matching name. When found,
 *      this driver is called to initialize the device.
 */
static void
attach_device(SANE_String *driver, SANE_String *name, 
              SANE_String *port, SANE_String *option_ta)
{
  int found = 0, driver_no, port_no;
  const char **ports;

  if (!strcmp (*port, "*"))
    {
      ports = sanei_pa4s2_devices();
      DBG (3, "sanei_init: auto probing port\n");
    }
  else
    {
      ports = malloc (sizeof(char *) * 2);
      ports[0] = *port;
      ports[1] = NULL;
    }

  for (port_no=0; ports[port_no] != NULL; port_no++)
    {
      for (driver_no=0 ; driver_no<MUSTEK_PP_NUM_DRIVERS ; driver_no++)
        {
          if (strcasecmp (Mustek_pp_Drivers[driver_no].driver, *driver) == 0)
   	     {
   	       Mustek_pp_Drivers[driver_no].init (
   	         (*option_ta == 0 ? CAP_NOTHING : CAP_TA),
   	         ports[port_no], *name, sane_attach);
   	       found = 1;
   	       break;
   	     }
        }
    }

  free (ports);

  if (found == 0)
    {
      DBG (1, "sane_init: no scanner detected\n");
      DBG (3, "sane_init: either the driver name ``%s'' is invalid, or no scanner was detected\n", *driver);
    }

  free (*name);
  free (*port);
  free (*driver);
  if (*option_ta)
    free (*option_ta);
  *name = *port = *driver = *option_ta = 0;
  
  /* In case of a successful initialization, the configuration options
     should have been transfered to the device, but this function can
     deal with that. */
  free_cfg_options(&numcfgoptions, &cfgoptions);
}
   
/* sane_init:
 *	Reads configuration file and registers hardware driver 
 *
 * ChangeLog:
 *
 * Description:
 * 	in *version_code the SANE version this backend was compiled with and the
 * 	version of the backend is returned. The value of authorize is stored in
 * 	the global variable sane_auth.
 *
 * 	Next the configuration file is read. If it isn't present, all drivers
 * 	are auto-probed with default values (port 0x378, with and without TA).
 *
 * 	The configuration file is expected to contain lines of the form
 *
 * 	  scanner <name> <port> <driver> [<option_ta>]
 *
 * 	where <name> is a arbitrary name to identify this entry
 *            <port> is the port where the scanner is attached to
 *            <driver> is the name of the driver to use
 *
 *      if the optional argument "option_ta" is present the driver uses special
 *      parameters fitting for a trasparency adapter.
 */ 	

SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  FILE *fp;
  char config_line[1024];
  const char *config_line_ptr;
  int line=0, driver_no;
  char *driver = 0, *port = 0, *name = 0, *option_ta = 0;

  DBG_INIT ();
  DBG (3, "sane-mustek_pp, version 0.%d-%s. build for SANE %s\n",
	MUSTEK_PP_BUILD, MUSTEK_PP_STATE, VERSION);
  DBG (3, "backend by Jochen Eisinger <jochen.eisinger@gmx.net>\n");

  if (version_code != NULL)
    *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, MUSTEK_PP_BUILD);

  sane_auth = authorize;


  fp = sanei_config_open (MUSTEK_PP_CONFIG_FILE);

  if (fp == NULL)
    {
      char driver_name[64];
      const char **devices = sanei_pa4s2_devices();
      int device_no;
	
      DBG (2, "sane_init: could not open configuration file\n");
      
      for (device_no = 0; devices[device_no] != NULL; device_no++)
        {
	  DBG (3, "sane_init: trying ``%s''\n", devices[device_no]);
          for (driver_no=0 ; driver_no<MUSTEK_PP_NUM_DRIVERS ; driver_no++)
	    {
	      Mustek_pp_Drivers[driver_no].init(CAP_NOTHING, devices[device_no],
	  	        Mustek_pp_Drivers[driver_no].driver, sane_attach);

	      snprintf (driver_name, 64, "%s-ta",
		    Mustek_pp_Drivers[driver_no].driver);

	      Mustek_pp_Drivers[driver_no].init(CAP_TA, devices[device_no],
		        driver_name, sane_attach);
	    }
	}

      free (devices);
      return SANE_STATUS_GOOD;
    }

  while (sanei_config_read (config_line, 1023, fp))
    {
      line++;
      if ((!*config_line) || (*config_line == '#'))
	continue;

      config_line_ptr = config_line;

      if (strncmp(config_line_ptr, "scanner", 7) == 0)
	{
	  config_line_ptr += 7;
          
          if (name)
          {
             /* Parsing of previous scanner + options is finished. Attach
                the device before we parse the next section. */
             attach_device(&driver, &name, &port, &option_ta);
          }
          
	  config_line_ptr = sanei_config_skip_whitespace (config_line_ptr);
	  if (!*config_line_ptr)
	    {
	      DBG (1, "sane_init: parse error in line %d after ``scanner''\n",
		line);
	      continue;
	    }

	  config_line_ptr = sanei_config_get_string (config_line_ptr, &name);
	  if ((name == NULL) || (!*name))
	    {
	      DBG (1, "sane_init: parse error in line %d after ``scanner''\n",
		line);
	      if (name != NULL)
		free (name);
	      name = 0;
	      continue;
	    }

	  config_line_ptr = sanei_config_skip_whitespace (config_line_ptr);
	  if (!*config_line_ptr)
	    {
	      DBG (1, "sane_init: parse error in line %d after "
		"``scanner %s''\n", line, name);
	      free (name);
	      name = 0;
	      continue;
	    }

	  config_line_ptr = sanei_config_get_string (config_line_ptr, &port);
	  if ((port == NULL) || (!*port))
	    {
	      DBG (1, "sane_init: parse error in line %d after "
		"``scanner %s''\n", line, name);
	      free (name);
	      name = 0;
	      if (port != NULL)
		free (port);
	      port = 0;
	      continue;
	    }

	  config_line_ptr = sanei_config_skip_whitespace (config_line_ptr);
	  if (!*config_line_ptr)
	    {
	      DBG (1, "sane_init: parse error in line %d after "
		"``scanner %s %s''\n", line, name, port);
	      free (name);
	      free (port);
	      name = 0;
	      port = 0;
	      continue;
	    }

	  config_line_ptr = sanei_config_get_string (config_line_ptr, &driver);
	  if ((driver == NULL) || (!*driver))
	    {
	      DBG (1, "sane_init: parse error in line %d after "
		"``scanner %s %s''\n", line, name, port);
	      free (name);
	      name = 0;
	      free (port);
	      port = 0;
	      if (driver != NULL)
		free (driver);
	      driver = 0;
	      continue;
	    }

	  config_line_ptr = sanei_config_skip_whitespace (config_line_ptr);

	  if (*config_line_ptr)
	    {
	      config_line_ptr = sanei_config_get_string (config_line_ptr,
							&option_ta);

	      if ((option_ta == NULL) || (!*option_ta) || 
		  (strcasecmp (option_ta, "use_ta") != 0))
		{
		  DBG (1, "sane_init: parse error in line %d after "
			"``scanner %s %s %s''\n", line, name, port, driver);
		  free (name);
		  free (port);
		  free (driver);
		  if (option_ta)
		    free (option_ta);
		  name = port = driver = option_ta = 0;
		  continue;
		}
	    }

	  if (*config_line_ptr)
	    {
	      DBG (1, "sane_init: parse error in line %d after "
			"``scanner %s %s %s %s\n", line, name, port, driver,
			(option_ta == 0 ? "" : option_ta));
	      free (name);
	      free (port);
	      free (driver);
	      if (option_ta)
		free (option_ta);
	      name = port = driver = option_ta = 0;
	      continue;
	    }
        }
      else if (strncmp(config_line_ptr, "option", 6) == 0)
        {
          /* Format for options: option <name> [<value>] 
             Note that the value is optional. */   
          char *optname, *optval = 0;
          Mustek_pp_config_option *tmpoptions;

          config_line_ptr += 6;         
          config_line_ptr = sanei_config_skip_whitespace (config_line_ptr);
          if (!*config_line_ptr)
	    {
	      DBG (1, "sane_init: parse error in line %d after ``option''\n",
	        line);
	      continue;
	    }

          config_line_ptr = sanei_config_get_string (config_line_ptr, &optname);
          if ((optname == NULL) || (!*optname))
	    {
	      DBG (1, "sane_init: parse error in line %d after ``option''\n",
	        line);
	      if (optname != NULL)
	        free (optname);
	      continue;
	    }

          config_line_ptr = sanei_config_skip_whitespace (config_line_ptr);
          if (*config_line_ptr)
	    {
              /* The option has a value.
                 No need to check the value; that's up to the backend */
	      config_line_ptr = sanei_config_get_string (config_line_ptr, 
                                                         &optval);

   	      config_line_ptr = sanei_config_skip_whitespace (config_line_ptr);
	    }

          if (*config_line_ptr)
	    {
	      DBG (1, "sane_init: parse error in line %d after "
		        "``option %s %s''\n", line, optname, 
		        (optval == 0 ? "" : optval));
	      free (optname);
	      if (optval) 
                 free (optval);
	      continue;
	    }

	  if (!strcmp (optname, "no_epp"))
	    {
	      u_int pa4s2_options;
	      if (name)
		DBG (2, "sane_init: global option found in local scope, "
			"executing anyway\n");
	      free (optname);
	      if (optval)
	        {
	          DBG (1, "sane_init: unexpected value for option no_epp\n");
	          free (optval);
	          continue;
	        }
	      DBG (3, "sane_init: disabling mode EPP\n");
	      sanei_pa4s2_options (&pa4s2_options, SANE_FALSE);
	      pa4s2_options |= SANEI_PA4S2_OPT_NO_EPP;
	      sanei_pa4s2_options (&pa4s2_options, SANE_TRUE);
	      continue;
	    }
	  else if (!name)
	    {
	      DBG (1, "sane_init: parse error in line %d: unexpected "
                      " ``option''\n", line);
	      free (optname);
	      if (optval) 
                 free (optval);
	      continue;
	    }


          /* Extend the (global) array of options */
          tmpoptions = realloc(cfgoptions, 
                               (numcfgoptions+1)*sizeof(cfgoptions[0]));
          if (!tmpoptions)
          {
             DBG (1, "sane_init: not enough memory for device options\n");
             free_cfg_options(&numcfgoptions, &cfgoptions);
             return SANE_STATUS_NO_MEM;
          }

          cfgoptions = tmpoptions;
          cfgoptions[numcfgoptions].name = optname;
          cfgoptions[numcfgoptions].value = optval;
          ++numcfgoptions;
        }
      else
	{
	  DBG (1, "sane_init: parse error at beginning of line %d\n", line);
	  continue;
	}

    }
    
  /* If we hit the end of the file, we still may have to process the
     last driver */
  if (name)
     attach_device(&driver, &name, &port, &option_ta);

  fclose(fp);
  return SANE_STATUS_GOOD;

}	

/* sane_exit:
 *	Unloads all drivers and frees allocated memory
 *
 * ChangeLog:
 *
 * Description:
 * 	All open devices are closed first. Then all registered devices
 * 	are removed.
 *
 */

void
sane_exit (void)
{
  Mustek_pp_Handle *hndl;
  Mustek_pp_Device *dev;

  if (first_hndl)
    DBG (3, "sane_exit: closing open devices\n");

  while (first_hndl)
    {
      hndl = first_hndl;
      sane_close (hndl);
    }

  dev = devlist;
  num_devices = 0;
  devlist = NULL;

  while (dev) {

	  free (dev->port);
	  free (dev->name);
	  free (dev->vendor);
	  free (dev->model);
	  free (dev->type);
          free_cfg_options (&dev->numcfgoptions, &dev->cfgoptions);
	  dev = dev->next;

  }

  if (devarray != NULL)
    free (devarray);
  devarray = NULL;

  DBG (3, "sane_exit: all drivers unloaded\n");

}

/* sane_get_devices:
 * 	Returns a list of registered devices
 *
 * ChangeLog:
 *
 * Description:
 * 	A possible present old device_list is removed first. A new
 * 	devarray is allocated and filled with pointers to the
 * 	SANE_Device structures of the Mustek_pp_Devices
 */
/*ARGSUSED*/
SANE_Status
sane_get_devices (const SANE_Device *** device_list, 
		  SANE_Bool local_only __UNUSED__)
{
  int ctr;
  Mustek_pp_Device *dev;

  if (devarray != NULL)
    free (devarray);

  devarray = malloc ((num_devices + 1) * sizeof (devarray[0]));

  if (devarray == NULL)
    {
      DBG (1, "sane_get_devices: not enough memory for device list\n");
      return SANE_STATUS_NO_MEM;
    }

  dev = devlist;
  
  for (ctr=0 ; ctr<num_devices ; ctr++) {
	  devarray[ctr] = &dev->sane;
	  dev = dev->next;
  }

  devarray[num_devices] = NULL;
  *device_list = (const SANE_Device **)devarray;

  return SANE_STATUS_GOOD;
}

/* sane_open:
 * 	opens a device and prepares it for operation
 *
 * ChangeLog:
 *
 * Description:
 * 	The device identified by ``devicename'' is looked
 * 	up in the list, or if devicename is zero, the
 * 	first device from the list is taken.
 *
 * 	open is called for the selected device.
 *
 * 	The handel is set up with default values, and the
 * 	option descriptors are initialized
 */

SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle * handle)
{

	Mustek_pp_Handle *hndl;
	Mustek_pp_Device *dev;
	SANE_Status status;
	int	fd, i;

	if (devicename[0]) {

		dev = devlist;

		while (dev) {

			if (strcmp (dev->name, devicename) == 0)
				break;

			dev = dev->next;

		}

		if (!dev) {

			DBG (1, "sane_open: unknown devicename ``%s''\n", devicename);
			return SANE_STATUS_INVAL;

		}
	} else
		dev = devlist;

	if (!dev) {
		DBG (1, "sane_open: no devices present...\n");
		return SANE_STATUS_INVAL;
	}

	DBG (3, "sane_open: Using device ``%s'' (driver %s v%s by %s)\n", 
			dev->name, dev->func->driver, dev->func->version, dev->func->author);

	if ((hndl = malloc (sizeof (Mustek_pp_Handle))) == NULL) {

		DBG (1, "sane_open: not enough free memory for the handle\n");
		return SANE_STATUS_NO_MEM;

	}
	
	if ((status = dev->func->open (dev->port, dev->caps, &fd)) != SANE_STATUS_GOOD) {

		DBG (1, "sane_open: could not open device (%s)\n",
				sane_strstatus (status));
		return status;

	}

	hndl->next = first_hndl;
	hndl->dev = dev;
	hndl->fd = fd;
	hndl->state = STATE_IDLE;
	hndl->pipe = -1;

	init_options (hndl);
	
	dev->func->setup (hndl);
        
        /* Initialize driver-specific configuration options. This must be
           done after calling the setup() function because only then the
           driver is guaranteed to be fully initialized */
        for (i = 0; i<dev->numcfgoptions; ++i)
        {
           status = dev->func->config (hndl, 
		  		       dev->cfgoptions[i].name,
				       dev->cfgoptions[i].value);
           if (status != SANE_STATUS_GOOD)
           {
              DBG (1, "sane_open: could not set option %s for device (%s)\n",
            		dev->cfgoptions[i].name, sane_strstatus (status));
              
              /* Question: should the initialization be aborted when an 
                 option cannot be handled ? 
                 The driver should have reasonable built-in defaults, so 
                 an illegal option value or an unknown option should not 
                 be fatal. Therefore, it's probably ok to ignore the error. */
           }
        }

	first_hndl = hndl;
	
	*handle = hndl;

	return SANE_STATUS_GOOD;
}

/* sane_close:
 * 	closes a given device and frees all resources
 *
 * ChangeLog:
 *
 * Description:
 * 	The handle is searched in the list of active handles.
 * 	If it's found, the handle is removed.
 *
 * 	If the associated device is still scanning, the process
 * 	is cancelled.
 *
 * 	Then the backend makes sure, the lamp was at least
 * 	2 seconds on.
 *
 * 	Afterwards the selected handel is closed
 */
void
sane_close (SANE_Handle handle)
{
  Mustek_pp_Handle *prev, *hndl;

  prev = NULL;

  for (hndl = first_hndl; hndl; hndl = hndl->next)
    {
      if (hndl == handle)
	break;
      prev = hndl;
    }

  if (hndl == NULL)
    {
      DBG (2, "sane_close: unknown device handle\n");
      return;
    }

  if (hndl->state == STATE_SCANNING) {
    sane_cancel (handle);
    do_eof (handle);
  }

  if (prev != NULL)
    prev->next = hndl->next;
  else
    first_hndl = hndl->next;

  DBG (3, "sane_close: maybe waiting for lamp...\n");
  if (hndl->lamp_on)
    while (time (NULL) - hndl->lamp_on < 2)
      sleep (1);

  hndl->dev->func->close (hndl);

  DBG (3, "sane_close: device closed\n");

  free (handle);

}

/* sane_get_option_descriptor:
 * 	does what it says
 *
 * ChangeLog:
 *
 * Description:
 *
 */

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  Mustek_pp_Handle *hndl = handle;

  if ((unsigned) option >= NUM_OPTIONS)
    {
      DBG (2, "sane_get_option_descriptor: option %d doesn't exist\n", option);
      return NULL;
    }

  return hndl->opt + option;
}


/* sane_control_option:
 * 	Reads or writes an option
 *
 * ChangeLog:
 *
 * Desription:
 * 	If a pointer to info is given, the value is initialized to zero
 *	while scanning options cannot be read or written. next a basic
 *	check whether the request is valid is done.
 *
 *	Depending on ``action'' the value of the option is either read
 *	(in the first block) or written (in the second block). auto
 *	values aren't supported.
 *
 *	before a value is written, some checks are performed. Depending
 *	on the option, that is written, other options also change 
 *
 */
SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *val, SANE_Int * info)
{
  Mustek_pp_Handle *hndl = handle;
  SANE_Status status;
  SANE_Word w, cap;

  if (info)
    *info = 0;

  if (hndl->state == STATE_SCANNING)
    {
      DBG (2, "sane_control_option: device is scanning\n");
      return SANE_STATUS_DEVICE_BUSY;
    }

  if ((unsigned int) option >= NUM_OPTIONS)
    {
      DBG (2, "sane_control_option: option %d doesn't exist\n", option);
      return SANE_STATUS_INVAL;
    }

  cap = hndl->opt[option].cap;

  if (!SANE_OPTION_IS_ACTIVE (cap))
    {
      DBG (2, "sane_control_option: option %d isn't active\n", option);
      return SANE_STATUS_INVAL;
    }

  if (action == SANE_ACTION_GET_VALUE)
    {

      switch (option)
	{
	  /* word options: */
	case OPT_PREVIEW:
	case OPT_GRAY_PREVIEW:
	case OPT_RESOLUTION:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_NUM_OPTS:
	case OPT_CUSTOM_GAMMA:
	case OPT_INVERT:
	case OPT_DEPTH:

	  *(SANE_Word *) val = hndl->val[option].w;
	  return SANE_STATUS_GOOD;

	  /* word-array options: */
	case OPT_GAMMA_VECTOR:
	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:

	  memcpy (val, hndl->val[option].wa, hndl->opt[option].size);
	  return SANE_STATUS_GOOD;

	  /* string options: */
	case OPT_MODE:
	case OPT_SPEED:

	  strcpy (val, hndl->val[option].s);
	  return SANE_STATUS_GOOD;
	}
    }
  else if (action == SANE_ACTION_SET_VALUE)
    {

      if (!SANE_OPTION_IS_SETTABLE (cap))
	{
	  DBG (2, "sane_control_option: option can't be set (%s)\n",
			  hndl->opt[option].name);
	  return SANE_STATUS_INVAL;
	}

      status = sanei_constrain_value (hndl->opt + option, val, info);

      if (status != SANE_STATUS_GOOD)
	{
	  DBG (2, "sane_control_option: constrain_value failed (%s)\n",
	       sane_strstatus (status));
	  return status;
	}

      switch (option)
	{
	  /* (mostly) side-effect-free word options: */
	case OPT_RESOLUTION:
	case OPT_TL_X:
	case OPT_BR_X:
	case OPT_TL_Y:
	case OPT_BR_Y:
	case OPT_PREVIEW:
	case OPT_GRAY_PREVIEW:
	case OPT_INVERT:
	case OPT_DEPTH:

	  if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS;

	  hndl->val[option].w = *(SANE_Word *) val;
	  return SANE_STATUS_GOOD;

	  /* side-effect-free word-array options: */
	case OPT_GAMMA_VECTOR:
	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:

	  memcpy (hndl->val[option].wa, val, hndl->opt[option].size);
	  return SANE_STATUS_GOOD;

	  /* side-effect-free string options: */
	case OPT_SPEED:

	  if (hndl->val[option].s)
		  free (hndl->val[option].s);

	  hndl->val[option].s = strdup (val);
	  return SANE_STATUS_GOOD;


	  /* options with side-effects: */

	case OPT_CUSTOM_GAMMA:
	  w = *(SANE_Word *) val;

	  if (w == hndl->val[OPT_CUSTOM_GAMMA].w)
	    return SANE_STATUS_GOOD;	/* no change */

	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS;

	  hndl->val[OPT_CUSTOM_GAMMA].w = w;

	  if (w == SANE_TRUE)
	    {
	      const char *mode = hndl->val[OPT_MODE].s;

	      if (strcmp (mode, SANE_VALUE_SCAN_MODE_GRAY) == 0)
		hndl->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
	      else if (strcmp (mode, SANE_VALUE_SCAN_MODE_COLOR) == 0)
		{
		  hndl->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
		  hndl->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
		  hndl->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
		  hndl->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
		}
	    }
	  else
	    {
	      hndl->opt[OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE;
	      hndl->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
	      hndl->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
	      hndl->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
	    }

	  return SANE_STATUS_GOOD;

	case OPT_MODE:
	  {
	    char *old_val = hndl->val[option].s;

	    if (old_val)
	      {
		if (strcmp (old_val, val) == 0)
		  return SANE_STATUS_GOOD;	/* no change */

		free (old_val);
	      }

	    if (info)
	      *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;

	    hndl->val[option].s = strdup (val);

	    hndl->opt[OPT_CUSTOM_GAMMA].cap |= SANE_CAP_INACTIVE;
	    hndl->opt[OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE;
	    hndl->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
	    hndl->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
	    hndl->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;

	    hndl->opt[OPT_DEPTH].cap |= SANE_CAP_INACTIVE;
	    
	    if ((hndl->dev->caps & CAP_DEPTH) && (strcmp(val, SANE_VALUE_SCAN_MODE_COLOR) == 0))
		    hndl->opt[OPT_DEPTH].cap &= ~SANE_CAP_INACTIVE;

	    if (!(hndl->dev->caps & CAP_GAMMA_CORRECT))
		    return SANE_STATUS_GOOD;

	    if (strcmp (val, SANE_VALUE_SCAN_MODE_LINEART) != 0)
	      hndl->opt[OPT_CUSTOM_GAMMA].cap &= ~SANE_CAP_INACTIVE;

	    if (hndl->val[OPT_CUSTOM_GAMMA].w == SANE_TRUE)
	      {
		if (strcmp (val, SANE_VALUE_SCAN_MODE_GRAY) == 0)
		  hndl->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
		else if (strcmp (val, SANE_VALUE_SCAN_MODE_COLOR) == 0)
		  {
		    hndl->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
		    hndl->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
		    hndl->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
		    hndl->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
		  }
	      }

	    return SANE_STATUS_GOOD;
	  }
	}
    }

  DBG (2, "sane_control_option: unknown action\n");
  return SANE_STATUS_INVAL;
}


/* sane_get_parameters:
 * 	returns the set of parameters, that is used for the next scan
 *
 * ChangeLog:
 *
 * Description:
 *
 * 	First of all it is impossible to change the parameter set
 * 	while scanning.
 *
 * 	sane_get_parameters not only returns the parameters for
 * 	the next scan, it also sets them, i.e. converts the
 * 	options in actuall parameters.
 *
 * 	The following parameters are set:
 *
 * 		scanmode:	according to the option SCANMODE, but
 * 				24bit color, if PREVIEW is selected and
 * 				grayscale if GRAY_PREVIEW is selected
 * 		depth:		the bit depth for color modes (if
 * 				supported) or 24bit by default
 * 				(ignored in bw/grayscale or if not
 * 				supported)
 * 		dpi:		resolution 
 * 		invert:		if supported else defaults to false
 * 		gamma:		if supported and selected
 * 		ta:		if supported by the device
 * 		speed:		selected speed (or fastest if not
 * 				supported)
 * 		scanarea:	the scanarea is calculated from the
 * 				selections the user has mode. note
 * 				that the area may slightly differ from
 * 				the scanarea selected due to rounding
 * 				note also, that a scanarea of
 * 				(0,0)-(100,100) will include all pixels
 * 				where 0 <= x < 100 and 0 <= y < 100
 * 	afterwards, all values are copied into the SANE_Parameters
 * 	structure.
 */
SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  Mustek_pp_Handle *hndl = handle;
  char *mode;
      int dpi, ctr;

  if (hndl->state != STATE_SCANNING)
    {


      memset (&hndl->params, 0, sizeof (hndl->params));


      if ((hndl->dev->caps & CAP_DEPTH) && (hndl->mode == MODE_COLOR))
	hndl->depth = hndl->val[OPT_DEPTH].w;
      else
	hndl->depth = 8;
	
      dpi = (int) (SANE_UNFIX (hndl->val[OPT_RESOLUTION].w) + 0.5);
      
      hndl->res = dpi;

      if (hndl->dev->caps & CAP_INVERT)
	hndl->invert = hndl->val[OPT_INVERT].w;
      else
	hndl->invert = SANE_FALSE;

      if (hndl->dev->caps & CAP_TA)
	hndl->use_ta = SANE_TRUE;
      else
	hndl->use_ta = SANE_FALSE;

      if ((hndl->dev->caps & CAP_GAMMA_CORRECT) && (hndl->val[OPT_CUSTOM_GAMMA].w == SANE_TRUE))
	      hndl->do_gamma = SANE_TRUE;
      else
	      hndl->do_gamma = SANE_FALSE;

      if (hndl->dev->caps & CAP_SPEED_SELECT) {

	      for (ctr=SPEED_SLOWEST; ctr<=SPEED_FASTEST; ctr++)
		      if (strcmp(mustek_pp_speeds[ctr], hndl->val[OPT_SPEED].s) == 0)
			      hndl->speed = ctr;

	      

      } else
	      hndl->speed = SPEED_NORMAL;
		      
      mode = hndl->val[OPT_MODE].s;

      if (strcmp (mode, SANE_VALUE_SCAN_MODE_LINEART) == 0)
	hndl->mode = MODE_BW;
      else if (strcmp (mode, SANE_VALUE_SCAN_MODE_GRAY) == 0)
	hndl->mode = MODE_GRAYSCALE;
      else
	hndl->mode = MODE_COLOR;

      if (hndl->val[OPT_PREVIEW].w == SANE_TRUE)
	{

			hndl->speed = SPEED_FASTEST;
			hndl->depth = 8;
			if (! hndl->use_ta)
			hndl->invert = SANE_FALSE;
			hndl->do_gamma = SANE_FALSE;

	  if (hndl->val[OPT_GRAY_PREVIEW].w == SANE_TRUE)
	    hndl->mode = MODE_GRAYSCALE;
	  else {
	    hndl->mode = MODE_COLOR;
	  }

	}

      hndl->topX =
	MIN ((int)
	     (MM_TO_PIXEL (SANE_UNFIX(hndl->val[OPT_TL_X].w), hndl->dev->maxres) +
	      0.5), hndl->dev->maxhsize);
      hndl->topY =
	MIN ((int)
	     (MM_TO_PIXEL (SANE_UNFIX(hndl->val[OPT_TL_Y].w), hndl->dev->maxres) +
	      0.5), hndl->dev->maxvsize);

      hndl->bottomX =
	MIN ((int)
	     (MM_TO_PIXEL (SANE_UNFIX(hndl->val[OPT_BR_X].w), hndl->dev->maxres) +
	      0.5), hndl->dev->maxhsize);
      hndl->bottomY =
	MIN ((int)
	     (MM_TO_PIXEL (SANE_UNFIX(hndl->val[OPT_BR_Y].w), hndl->dev->maxres) +
	      0.5), hndl->dev->maxvsize);
      
      /* If necessary, swap the upper and lower boundaries to avoid negative
         distances. */
      if (hndl->topX > hndl->bottomX) {
	SANE_Int tmp = hndl->topX;
	hndl->topX = hndl->bottomX;
	hndl->bottomX = tmp;
      }
      if (hndl->topY > hndl->bottomY) {
	SANE_Int tmp = hndl->topY;
	hndl->topY = hndl->bottomY;
	hndl->bottomY = tmp;
      }

      hndl->params.pixels_per_line = (hndl->bottomX - hndl->topX) * hndl->res
	/ hndl->dev->maxres;

      hndl->params.bytes_per_line = hndl->params.pixels_per_line;

      switch (hndl->mode)
	{

	case MODE_BW:
	  hndl->params.bytes_per_line /= 8;

	  if ((hndl->params.pixels_per_line % 8) != 0)
	    hndl->params.bytes_per_line++;

	  hndl->params.depth = 1;
	  break;

	case MODE_GRAYSCALE:
	  hndl->params.depth = 8;
	  hndl->params.format = SANE_FRAME_GRAY;
	  break;

	case MODE_COLOR:
	  hndl->params.depth = hndl->depth;
	  hndl->params.bytes_per_line *= 3;
	  if (hndl->depth > 8)
	    hndl->params.bytes_per_line *= 2;
	  hndl->params.format = SANE_FRAME_RGB;
	  break;

	}

      hndl->params.last_frame = SANE_TRUE;

      hndl->params.lines = (hndl->bottomY - hndl->topY) * hndl->res /
	hndl->dev->maxres;
    }
  else
      DBG (2, "sane_get_parameters: can't set parameters while scanning\n");

  if (params != NULL)
    *params = hndl->params;

  return SANE_STATUS_GOOD;

}


/* sane_start:
 * 	starts the scan. data aquisition will start immedially
 *
 * ChangeLog:
 *
 * Description:
 *
 */
SANE_Status
sane_start (SANE_Handle handle)
{
  Mustek_pp_Handle	*hndl = handle;
  int			pipeline[2];

  if (hndl->state == STATE_SCANNING) {
	  DBG (2, "sane_start: device is already scanning\n");
	  return SANE_STATUS_DEVICE_BUSY;

  }

	sane_get_parameters (hndl, NULL);

	if (pipe(pipeline) < 0) {
		DBG (1, "sane_start: could not initialize pipe (%s)\n",
				strerror(errno));
		return SANE_STATUS_IO_ERROR;
	}

	hndl->reader = fork();

	if (hndl->reader == 0) {

		sigset_t	ignore_set;
		struct SIGACTION	act;

		close (pipeline[0]);

		sigfillset (&ignore_set);
		sigdelset (&ignore_set, SIGTERM);
		sigprocmask (SIG_SETMASK, &ignore_set, NULL);

		memset (&act, 0, sizeof(act));
		sigaction (SIGTERM, &act, NULL);

		_exit (reader_process (hndl, pipeline[1]));

	}

	close (pipeline[1]);

	hndl->pipe = pipeline[0];

	hndl->state = STATE_SCANNING;

  return SANE_STATUS_GOOD;

}


/* sane_read:
 * 	receives data from pipeline and passes it to the caller
 *
 * ChangeLog:
 *
 * Description:
 * 	ditto
 */
SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * buf, SANE_Int max_len,
	   SANE_Int * len)
{
  Mustek_pp_Handle	*hndl = handle;
  SANE_Int		nread;


  if (hndl->state == STATE_CANCELLED) {
	  DBG (2, "sane_read: device already cancelled\n");
	  do_eof (hndl);
	  hndl->state = STATE_IDLE;
	  return SANE_STATUS_CANCELLED;
  }

  if (hndl->state != STATE_SCANNING) {
	  DBG (1, "sane_read: device isn't scanning\n");
	  return SANE_STATUS_INVAL;
  }


  *len = nread = 0;

  while (*len < max_len) {

	  nread = read(hndl->pipe, buf + *len, max_len - *len);

	  if (hndl->state == STATE_CANCELLED) {

		  *len = 0;
		  DBG(3, "sane_read: scan was cancelled\n");

		  do_eof (hndl);
		  hndl->state = STATE_IDLE;
		  return SANE_STATUS_CANCELLED;

	  }

	  if (nread < 0) {

		  if (errno == EAGAIN) {

			  if (*len == 0)
				  DBG(3, "sane_read: no data at the moment\n");
			  else
				  DBG(3, "sane_read: %d bytes read\n", *len);

			  return SANE_STATUS_GOOD;

		  } else {

			  DBG(1, "sane_read: IO error (%s)\n", strerror(errno));

			  hndl->state = STATE_IDLE;
			  do_stop(hndl);

			  do_eof (hndl);

			  *len = 0;
			  return SANE_STATUS_IO_ERROR;

		  }
	  }

	  *len += nread;

	  if (nread == 0) {

		  if (*len == 0) {

			DBG (3, "sane_read: read finished\n");
			do_stop(hndl);

			hndl->state = STATE_IDLE;

			return do_eof(hndl);

		  }

		  DBG(3, "sane_read: read last buffer of %d bytes\n",
				  *len);

		  return SANE_STATUS_GOOD;

	  }

  }

  DBG(3, "sane_read: read full buffer of %d bytes\n", *len);

  return SANE_STATUS_GOOD;
}


/* sane_cancel:
 * 	stops a scan and ends the reader process
 *
 * ChangeLog:
 *
 * Description:
 *
 */
void
sane_cancel (SANE_Handle handle)
{
  Mustek_pp_Handle *hndl = handle;

  if (hndl->state != STATE_SCANNING)
	 return;

  hndl->state = STATE_CANCELLED;

  do_stop (hndl);

}


/* sane_set_io_mode:
 * 	toggles between blocking and non-blocking reading
 *
 * ChangeLog:
 *
 * Description:
 *
 */
SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{

	Mustek_pp_Handle	*hndl=handle;

	if (hndl->state != STATE_SCANNING)
		return SANE_STATUS_INVAL;
	

	if (fcntl (hndl->pipe, F_SETFL, non_blocking ? O_NONBLOCK : 0) < 0) {

		DBG(1, "sane_set_io_mode: can't set io mode\n");

		return SANE_STATUS_IO_ERROR;

	}

	return SANE_STATUS_GOOD;
}


/* sane_get_select_fd:
 * 	returns the pipeline fd for direct reading
 *
 * ChangeLog:
 *
 * Description:
 * 	to allow the frontend to receive the data directly it
 * 	can read from the pipeline itself
 */
SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int * fd)
{
	Mustek_pp_Handle	*hndl=handle;

	if (hndl->state != STATE_SCANNING)
		return SANE_STATUS_INVAL;
	
	*fd = hndl->pipe;

	return SANE_STATUS_GOOD;
}

/* include drivers */
#include "mustek_pp_decl.h"
#include "mustek_pp_null.c"
#include "mustek_pp_cis.h"
#include "mustek_pp_cis.c"
#include "mustek_pp_ccd300.h"
#include "mustek_pp_ccd300.c"
