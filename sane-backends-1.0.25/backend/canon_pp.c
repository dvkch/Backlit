/* sane - Scanner Access Now Easy.
   Copyright (C) 2001-2002 Matthew C. Duggan and Simon Krix
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

   -----

   canon_pp.c: $Revision$

   This file is part of the canon_pp backend, supporting Canon FBX30P 
   and NX40P scanners
   */

#ifdef  _AIX
#include  <lalloca.h>		/* MUST come first for AIX! */
#endif

#define BACKEND_NAME canon_pp

#define THREE_BITS 0xE0
#define TWO_BITS 0xC0
#define MM_PER_IN 25.4

#ifndef NOSANE
#include "../include/sane/config.h"
#endif

#ifndef VERSION
#define VERSION "$Revision$"
#endif

#include  <string.h>
#include  <math.h>
#include  <unistd.h>
#include  <sys/stat.h>
#include  <sys/types.h>
#include  <stdlib.h>
#include  <errno.h>
#include  <ieee1284.h>

#include  "../include/sane/sane.h"
#include  "../include/sane/saneopts.h"

#include "canon_pp-dev.h"
#include "canon_pp-io.h"
#include "canon_pp.h"

/* #include  "../include/sane/sanei_pio.h" */
#include  "../include/sane/sanei_config.h"
#include  "../include/sane/sanei_backend.h"
/* #include  "../include/sane/sanei_debug.h" */


/* Prototypes */
static SANE_Status init_device(struct parport *pp);

/* create a calibration file and give it initial values */
static int init_cal(char *file);

static SANE_Status fix_weights_file(CANONP_Scanner *cs);

static SANE_Status detect_mode(CANONP_Scanner *cs);

/* Global Variables (ack!) */

/* The first device in a linked list of devices */
static CANONP_Scanner *first_dev = NULL;
/* The default scanner to open */
static char *def_scanner = NULL;
/* The number of devices */
static int num_devices = 0;
/* ieee1284 parallel ports */
struct parport_list pl;
/* leftover from the last read */
static SANE_Byte *read_leftover = NULL;
/* leftover from the last read */
static SANE_Bool force_nibble = SANE_FALSE;

/* Constants */

/* Colour Modes */
static const SANE_String_Const cmodes[] = { 
	SANE_VALUE_SCAN_MODE_GRAY, 
	SANE_VALUE_SCAN_MODE_COLOR, 
	NULL };

/* bit depths */
static const SANE_String_Const depths[] = { "8", "12", NULL };
/* resolutions */
static const SANE_Int res300[] = {3, 75, 150, 300};
static const SANE_Int res600[] = {4, 75, 150, 300, 600};


/*************************************************************************
 *
 * sane_init()
 *
 * Initialises data for the list of scanners, stored in canon-p.conf.
 *
 * Scanners are not sent any commands until sane_open() is called.
 *
 *************************************************************************/
	SANE_Status
sane_init (SANE_Int *vc, SANE_Auth_Callback cb)
{
	SANE_Status status = SANE_STATUS_GOOD;
	int i, tmp; 
	int tmp_im = INITMODE_AUTO;
	FILE *fp;
	char line[81]; /* plus 1 for a null */
	char *tmp_wf, *tmp_port;
	CANONP_Scanner *s_tmp;


	DBG_INIT();

#if defined PACKAGE && defined VERSION
	DBG(2, ">> sane_init (version %s null, authorize %s null): " PACKAGE " " VERSION "\n", 
	    (vc) ? "!=" : "==", (cb) ? "!=" : "==");
#endif

	if(vc)
		*vc = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, 0);

	DBG(2,"sane_init: >> ieee1284_find_ports\n");
	/* Find lp ports */
	tmp = ieee1284_find_ports(&pl, 0);
	DBG(2,"sane_init: %d << ieee1284_find_ports\n", tmp);

	if (tmp != E1284_OK)
	{
		DBG(1,"sane_init: Error trying to get port list\n");
		return SANE_STATUS_IO_ERROR;
	}


	if (pl.portc < 1)
	{
		DBG(1,"sane_init: Error, no parallel ports found.\n");
		return SANE_STATUS_IO_ERROR;
	}

	DBG(10,"sane_init: %i parallel port(s) found.\n", pl.portc);
	/* Setup data structures for each port */
	for(i=0; i<pl.portc; i++)
	{
		DBG(10,"sane_init: port %s\n", pl.portv[i]->name);
		status = init_device(pl.portv[i]);
		/* Now's a good time to quit if we got an error */
		if (status != SANE_STATUS_GOOD) return status;
	}

	/* This should never be true here */
	if (num_devices == 0)
		status = SANE_STATUS_IO_ERROR;

	/* just to be extra sure, the line will always have an end: */
	line[sizeof(line)-1] = '\0';

	/* 
	 * Read information from config file: pixel weight location and default 
	 * port.
	 */
	if((fp = sanei_config_open(CANONP_CONFIG_FILE)))
	{
		while(sanei_config_read(line, sizeof (line) - 1, fp))
		{
			DBG(100, "sane_init: >%s<\n", line);
			if(line[0] == '#')	/* ignore line comments */
				continue;
			if(!strlen(line))
				continue;	/* ignore empty lines */

			if(strncmp(line,"calibrate ", 10) == 0)
			{
				/* warning: pointer trickyness ahead 
				 * Do not free tmp_port! */
				DBG(40, "sane_init: calibrate line, %s\n", 
						line);
				tmp_wf = strdup(line+10);
				tmp_port = strstr(tmp_wf, " ");
				if ((tmp_port == tmp_wf) || (tmp_port == NULL))
				{
					/* They have used an old style config 
					 * file which does not specify scanner
					 * Assume first port */
					DBG(1, "sane_init: old config line:"
							"\"%s\".  Please add "
							"a port argument.\n", 
							line);

					/* first_dev should never be null here
					 * because we found at least one 
					 * parallel port above */
					first_dev->weights_file = tmp_wf;
					DBG(100, "sane_init: Successfully "
							"parsed (old) cal, "
							"weight file is "
							"'%s'.\n", tmp_wf);
					continue;

				}

				/* Now find which scanner wants 
				 * this calibration file */
				s_tmp = first_dev;
				DBG(100, "sane_init: Finding scanner on port "
						"'%s'\n", tmp_port+1);
				while (s_tmp != NULL)
				{
					if (!strcmp(s_tmp->params.port->name,
								tmp_port+1))
					{
						DBG(100, "sane_init: Found!\n");
						/* Now terminate the weight 
						 * file string */
						*tmp_port = '\0';
						s_tmp->weights_file = tmp_wf;
						DBG(100, "sane_init: Parsed "
								"cal, for port"
								" '%s', weight"
								" file is '%s'"
								".\n", 
								s_tmp->params.
								  port->name,
								tmp_wf);
						break;
					}
					s_tmp = s_tmp->next;
				}
				if (s_tmp == NULL)
				{
					/* we made it all the way through the
					 * list and didn't find the port */
					free(tmp_wf);
					DBG(10, "sane_init: calibrate line is "
							"for unknown port!\n");
				}
				continue;
			}


			if(strncmp(line,"ieee1284 ", 9) == 0)
			{
				DBG(100, "sane_init: Successfully parsed "
						"default scanner.\n");
				/* this will be our default scanner */
				def_scanner = strdup(line+9);
				continue;
			}

			if(strncmp(line,"force_nibble", 12) == 0)
			{
				DBG(100, "sane_init: force_nibble "
						"requested.\n");
				force_nibble = SANE_TRUE;
				continue;
			}

			if(strncmp(line,"init_mode ", 10) == 0)
			{

				/* parse what sort of initialisation mode to 
				 * use */
				if (strncmp(line+10, "FB620P", 6) == 0)
					tmp_im = INITMODE_20P;
				else if (strncmp(line+10, "FB630P", 6) == 0)
					tmp_im = INITMODE_30P;
				else if (strncmp(line+10, "AUTO", 4) == 0)
					tmp_im = INITMODE_AUTO;

				/* now work out which port it blongs to */

				tmp_port = strstr(line+10, " ");

				if (tmp_port == NULL)
				{
					/* first_dev should never be null here
					 * because we found at least one 
					 * parallel port above */
					first_dev->init_mode = tmp_im;
					DBG(100, "sane_init: Parsed init-1.\n");
					continue;
				}


				s_tmp = first_dev;
				while (s_tmp != NULL)
				{
					if (!strcmp(s_tmp->params.port->name,
								tmp_port+1))
					{
						s_tmp->init_mode = tmp_im;
						DBG(100, "sane_init: Parsed "
								"init.\n");
						break;
					}
					s_tmp = s_tmp->next;
				}
				if (s_tmp == NULL)
				{
					/* we made it all the way through the
					 * list and didn't find the port */
					DBG(10, "sane_init: init_mode line is "
							"for unknown port!\n");
				}

				continue;
			}
			DBG(1, "sane_init: Unknown configuration command!");

		} 
		fclose (fp);
	}	

	/* There should now be a LL of ports starting at first_dev */

	for (s_tmp = first_dev; s_tmp != NULL; s_tmp = s_tmp->next)
	{
		/* Assume there's no scanner present until proven otherwise */
		s_tmp->scanner_present = SANE_FALSE;

		/* Try to detect if there's a scanner there, and if so, 
		 * what sort of scanner it is */
		status = detect_mode(s_tmp);

		if (status != SANE_STATUS_GOOD) 
		{
			DBG(10,"sane_init: Error detecting port mode on %s!\n",
					s_tmp->params.port->name);
			s_tmp->scanner_present = SANE_FALSE;
			continue;
		} 
		
		/* detect_mode suceeded, so the port is open.  This beholdens
		 * us to call ieee1284_close in any of the remaining error
		 * cases in this loop. */
#if 0
		tmp = sanei_canon_pp_detect(s_tmp->params.port, 
				s_tmp->init_mode);


		if (tmp && (s_tmp->ieee1284_mode != M1284_NIBBLE))
		{
			/* A failure, try again in nibble mode... */
			DBG(1, "sane_init: Failed on ECP mode, falling "
					"back to nibble mode\n");

			s_tmp->ieee1284_mode = M1284_NIBBLE;
			sanei_canon_pp_set_ieee1284_mode(s_tmp->ieee1284_mode);
			tmp = sanei_canon_pp_detect(s_tmp->params.port, 
					s_tmp->init_mode);
		}
		/* still no go? */
		if (tmp)
		{
			DBG(1,"sane_init: couldn't find a scanner on port "
					"%s\n", s_tmp->params.port->name);

			ieee1284_close(s_tmp->params.port);
			continue;
		}
		
#endif
		/* all signs point to yes, try it out */
		if (ieee1284_claim(s_tmp->params.port) != E1284_OK) {
			DBG(10, "sane_init: Couldn't claim port %s.\n",
				s_tmp->params.port->name);

			ieee1284_close(s_tmp->params.port);
			continue;
		}
		
		DBG(2, "sane_init: >> initialise\n");
		tmp = sanei_canon_pp_initialise(&(s_tmp->params), 
				s_tmp->init_mode);
		DBG(2, "sane_init: << %d initialise\n", tmp);
		if (tmp) {
			DBG(10, "sane_init: Couldn't contact scanner on port "
				"%s. Probably no scanner there?\n",
				s_tmp->params.port->name);
			ieee1284_release(s_tmp->params.port);
			ieee1284_close(s_tmp->params.port);
			s_tmp->scanner_present = SANE_FALSE;
			continue;
		}

		/* put it back to sleep until we're ready to 
		 * open for business again - this will only work
		 * if we actually have a scanner there! */
		DBG(100, "sane_init: And back to sleep again\n");
		sanei_canon_pp_sleep_scanner(s_tmp->params.port);

		/* leave the port open but not claimed - this is regardless 
		 * of the return value of initialise */
		ieee1284_release(s_tmp->params.port);

		/* Finally, we're sure there's a scanner there! Now we
		 * just have to load the weights file...*/

		if (fix_weights_file(s_tmp) != SANE_STATUS_GOOD) {
			DBG(1, "sane_init: Eeek! fix_weights_file failed for "
				"scanner on port %s!\n", 
				s_tmp->params.port->name);
			/* non-fatal.. scans will look ugly as sin unless
			 * they calibrate */
		}

		/* Cocked, locked and ready to rock */
		s_tmp->hw.model = s_tmp->params.name;
		s_tmp->scanner_present = SANE_TRUE;
	}

	DBG(2, "<< sane_init\n");

	return status;
}


/*************************************************************************
 *
 * sane_get_devices()
 *
 * Gives a list of devices avaialable.  In our case, that's the linked
 * list produced by sane_init.
 *
 *************************************************************************/
	SANE_Status
sane_get_devices (const SANE_Device ***dl, SANE_Bool local)
{	
	static const SANE_Device **devlist;
	CANONP_Scanner *dev;
	int i;

	DBG(2, ">> sane_get_devices (%p, %d)\n", (const void*)dl, local);

	if (dl == NULL)
	{
		DBG(1, "sane_get_devices: ERROR: devlist pointer is NULL!");
		return SANE_STATUS_INVAL;
	}

	if (devlist != NULL)
	{
		/* this has been called already */
		*dl = devlist;
		return SANE_STATUS_GOOD;
	}
	devlist = malloc((num_devices + 1) * sizeof(*devlist));
	if (devlist == NULL)
		return SANE_STATUS_NO_MEM;

	i = 0;
	for (dev = first_dev; dev != NULL; dev = dev->next)
	{
		if (dev->scanner_present == SANE_TRUE)
		{
			devlist[i] = &(dev->hw);
			i++;
		}
	}

	devlist[i] = NULL;

	*dl = devlist;

	DBG(2, "<< sane_get_devices\n");
	return SANE_STATUS_GOOD;
}


/*************************************************************************
 *
 * sane_open()
 *
 * Open the scanner described by name.  Ask libieee1284 to claim the port
 * and call Simon's init code.  Also configure data structures.
 *
 *************************************************************************/
	SANE_Status
sane_open (SANE_String_Const name, SANE_Handle *h)
{
	CANONP_Scanner *cs;
	SANE_Range *tmp_range;
	int tmp;

	DBG(2, ">> sane_open (h=%p, name=\"%s\")\n", (void *)h, name);

	if ((h == NULL) || (name == NULL)) 
	{
		DBG(2,"sane_open: Null pointer received!\n");
		return SANE_STATUS_INVAL;
	}

	if (!strlen(name))
	{
		DBG(10,"sane_open: Empty name given, assuming first/"
				"default scanner\n");
		if (def_scanner == NULL)
			name = first_dev->params.port->name;
		else
			name = def_scanner;

		/* we don't _have_ to fit this name, so _don't_ fail if it's
		 * not there */

		cs = first_dev;
		while((cs != NULL) && strcmp(cs->params.port->name, name))
			cs = cs->next;

		/* if we didn't find the port they want, or there's no scanner 
		 * there, we just want to find _any_ scanner */
		if ((cs == NULL) || (cs->scanner_present != SANE_TRUE))
		{
			cs = first_dev;
			while((cs != NULL) && 
					(cs->scanner_present == SANE_FALSE))
				cs = cs->next;
		}

	} else {

		/* they're dead keen for this name, so _do_ fail if it's
		 * not there */
		cs = first_dev;
		while((cs != NULL) && strcmp(cs->params.port->name, name))
			cs = cs->next;
	}


	if (cs == NULL) 
	{
		DBG(2,"sane_open: No scanner found or requested port "
				"doesn't exist (%s)\n", name);
		return SANE_STATUS_IO_ERROR;
	}
	if (cs->scanner_present == SANE_FALSE)
	{
		DBG(1,"sane_open: Request to open port with no scanner "
				"(%s)\n", name);
		return SANE_STATUS_IO_ERROR;
	}
	if (cs->opened == SANE_TRUE) 
	{
		DBG(2,"sane_open; Oi!, That scanner's already open.\n");
		return SANE_STATUS_DEVICE_BUSY;
	}

	/* If the scanner has already been opened once, we don't have to do 
	 * this setup again */
	if (cs->setup == SANE_TRUE) 
	{
		cs->opened = SANE_TRUE;
		*h = (SANE_Handle)cs;
		return SANE_STATUS_GOOD;
	}

	tmp = ieee1284_claim(cs->params.port);
	if (tmp != E1284_OK) {
		DBG(1, "sane_open: Could not claim port!\n");
		return SANE_STATUS_IO_ERROR;
	}

	/* I put the scanner to sleep before, better wake it back up */

	DBG(2, "sane_open: >> initialise\n");
	tmp = sanei_canon_pp_initialise(&(cs->params), cs->init_mode);
	DBG(2, "sane_open: << %d initialise\n", tmp);
	if (tmp != 0) {
		DBG(1, "sane_open: initialise returned %d, something is "
				"wrong with the scanner!\n", tmp);

		DBG(1, "sane_open: Can't contact scanner.  Try power "
				"cycling scanner, and unplug any "
				"printers\n");
		ieee1284_release(cs->params.port);
		return SANE_STATUS_IO_ERROR;
	}

	if (cs->weights_file != NULL)
		DBG(2, "sane_open: >> load_weights(%s, %p)\n", 
				cs->weights_file, 
				(const void *)(&(cs->params)));
	else
		DBG(2, "sane_open: >> load_weights(NULL, %p)\n", 
				(const void *)(&(cs->params)));
	tmp = sanei_canon_pp_load_weights(cs->weights_file, &(cs->params));
	DBG(2, "sane_open: << %d load_weights\n", tmp);

	if (tmp != 0) {
		DBG(1, "sane_open: WARNING: Error on load_weights: "
				"returned %d.  This could be due to a corrupt "
				"calibration file.  Try recalibrating and if "
				"problems persist, please report the problem "
				"to the canon_pp maintainer\n", tmp);
		cs->cal_valid = SANE_FALSE;
	} else {
		cs->cal_valid = SANE_TRUE;
		DBG(10, "sane_open: loadweights successful, uploading gamma"
				" profile...\n");
		tmp = sanei_canon_pp_adjust_gamma(&(cs->params));
		if (tmp != 0)
			DBG(1, "sane_open: WARNING: adjust_gamma returned "
					"%d!\n", tmp);

		DBG(10, "sane_open: after adjust_gamma Status = %i\n", 
				sanei_canon_pp_check_status(cs->params.port));
	}
		

	/* Configure ranges etc */

	/* Resolution - determined by magic number */

	if (cs->params.scanheadwidth == 2552)
		cs->opt[OPT_RESOLUTION].constraint.word_list = res300;
	else
		cs->opt[OPT_RESOLUTION].constraint.word_list = res600;


	/* TL-X */
	if(!(tmp_range = malloc(sizeof(*tmp_range))))
		return SANE_STATUS_NO_MEM;
	(*tmp_range).min = 0;
	(*tmp_range).max = 215;
	cs->opt[OPT_TL_X].constraint.range = tmp_range;

	/* TL-Y */
	if(!(tmp_range = malloc(sizeof(*tmp_range))))
		return SANE_STATUS_NO_MEM;
	(*tmp_range).min = 0;
	(*tmp_range).max = 296;
	cs->opt[OPT_TL_Y].constraint.range = tmp_range;

	/* BR-X */
	if(!(tmp_range = malloc(sizeof(*tmp_range))))
		return SANE_STATUS_NO_MEM;
	(*tmp_range).min = 3;
	(*tmp_range).max = 216;
	cs->opt[OPT_BR_X].constraint.range = tmp_range;

	/* BR-Y */
	if(!(tmp_range = malloc(sizeof(*tmp_range))))
		return SANE_STATUS_NO_MEM;
	(*tmp_range).min = 1;
	(*tmp_range).max = 297;
	cs->opt[OPT_BR_Y].constraint.range = tmp_range;


	cs->opened = SANE_TRUE;
	cs->setup = SANE_TRUE;

	*h = (SANE_Handle)cs;

	DBG(2, "<< sane_open\n");

	return SANE_STATUS_GOOD;
}

/*************************************************************************
 *
 * sane_get_option_descriptor()
 *
 * Return the structure for option number opt.
 *
 *************************************************************************/
	const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle h, SANE_Int opt)
{
	CANONP_Scanner *cs = ((CANONP_Scanner *)h);
	/*DBG(2, ">> sane_get_option_descriptor (h=%p, opt=%d)\n", h, opt);*/

	if (h == NULL) {
		DBG(10,"sane_get_option_descriptor: WARNING: h==NULL!\n");
		return NULL;
	}

	if ((unsigned)opt >= NUM_OPTIONS) {
		DBG(10,"sane_get_option_descriptor: Note: opt >= "
				"NUM_OPTIONS!\n");
		return NULL;
	}

	if (cs->opened == SANE_FALSE)
	{
		DBG(1,"sane_get_option_descriptor: That scanner (%p) ain't "
				"open yet\n", h);
		return NULL;
	}

	/*DBG(2, "<< sane_get_option_descriptor\n");*/

	return (cs->opt + opt);
}


/*************************************************************************
 *
 * sane_control_option()
 *
 * Set a value for one of the options provided.
 *
 *************************************************************************/
SANE_Status
sane_control_option (SANE_Handle h, SANE_Int opt, SANE_Action act,
		void *val, SANE_Word *info)
{
	CANONP_Scanner *cs = ((CANONP_Scanner *)h);
	int i = 0, tmp, maxresi;

	DBG(2, ">> sane_control_option (h=%p, opt=%d, act=%d)\n",
			h,opt,act); 
	/* Do some sanity checks on the parameters 
	 * note that val can be null for buttons */
	if ((h == NULL) || ((val == NULL) && (opt != OPT_CAL)))   
		/* || (info == NULL))  - Don't check this any more.. 
		 * frontends seem to like passing a null */
	{
		DBG(1,"sane_control_option: Frontend passed me a null! "
				"(h=%p,val=%p,info=%p)\n",(void*)h,
				val,(void*)info);
		return SANE_STATUS_INVAL;
	}

	if (((unsigned)opt) >= NUM_OPTIONS) 
	{
		DBG(1,"sane_control_option: I don't do option %d.\n", opt);
		return SANE_STATUS_INVAL;
	}

	if (cs->opened == SANE_FALSE)
	{
		DBG(1,"sane_control_option: That scanner (%p) ain't "
				"open yet\n", h);
		return SANE_STATUS_INVAL;
	}

	if (cs->scanning == SANE_TRUE)
	{
		DBG(1,"sane_control_option: That scanner (%p) is scanning!\n",
				h);
		return SANE_STATUS_DEVICE_BUSY;
	}

	switch(act) 
	{
		case SANE_ACTION_GET_VALUE:
			switch (opt) 
			{
				case OPT_COLOUR_MODE:
					strcpy((char *)val, 
							cmodes[cs->vals[opt]]);
					break;
				case OPT_DEPTH:
					strcpy((char *)val, 
							depths[cs->vals[opt]]);
					break;
				case OPT_RESOLUTION:
					*((int *)val) = res600[cs->vals[opt]];
					break;
				default:
					*((int *)val) = cs->vals[opt];
					break;
			}
			break;
		case SANE_ACTION_SET_VALUE:
			/* val has been checked for NULL if opt != OPT_CAL */
			if (opt != OPT_CAL) i = *((int *)val);
			if (info != NULL) *info = 0;
			switch (opt) {
				case OPT_NUM_OPTIONS:
					/* you can't set that! */
					return SANE_STATUS_INVAL;
				case OPT_RESOLUTION:
					i = cs->vals[opt];
					cs->vals[opt] = 1;
					maxresi = cs->opt[OPT_RESOLUTION].
						constraint.word_list[0];

					while ((cs->vals[opt] <= maxresi) && 
							(res600[cs->vals[opt]]
							 < *((int *)val)))
					{
						cs->vals[opt] += 1;
					}

					if (res600[cs->vals[opt]] != 
							*((int *)val))
					{
						if (info != NULL) *info |= 
							SANE_INFO_INEXACT;
					}
					break;
				case OPT_COLOUR_MODE:
					cs->vals[opt] = 0;
					while ((cmodes[cs->vals[opt]] != NULL)
							&& strcmp(cmodes[cs->vals[opt]], 
								(char *)val))
					{
						cs->vals[opt] += 1;
					}
					if (info != NULL) *info |= 
						SANE_INFO_RELOAD_PARAMS;
					break;
				case OPT_DEPTH:
					cs->vals[opt] = 0;
					while ((depths[cs->vals[opt]] != NULL)
							&& strcmp(depths[cs->vals[opt]], 
								(char *)val))
					{
						cs->vals[opt] += 1;
					}
					if (info != NULL) *info |= 
						SANE_INFO_RELOAD_PARAMS;
					break;
				case OPT_TL_X:
				case OPT_BR_X:
				case OPT_TL_Y:
				case OPT_BR_Y:
					if ((i<cs->opt[opt].constraint.range->min) || (i>cs->opt[opt].constraint.range->max))
						return SANE_STATUS_INVAL;
					cs->vals[opt] = i;
					break;
				case OPT_CAL:
					/* Call the calibration code */
					if ((cs->weights_file==NULL) ||
							cs->cal_readonly
					   )
						DBG(2, ">> calibrate(x, " 
								"NULL)\n");
					else
						DBG(2, ">> calibrate(x,"
								"%s)\n",
								cs->weights_file);

					if (cs->cal_readonly) tmp = 
						sanei_canon_pp_calibrate(
								&(cs->params), 
								NULL);
					else tmp = sanei_canon_pp_calibrate(
							&(cs->params), 
							cs->weights_file);

					DBG(2, "<< %d calibrate\n", 
							tmp);
					if (tmp != 0) {
						DBG(1, "sane_control_option: "
								"WARNING: "
								"calibrate "
								"returned %d!", 
								tmp);
						cs->cal_valid = 
							SANE_FALSE;
						return SANE_STATUS_IO_ERROR;
					} else {
						cs->cal_valid = 
							SANE_TRUE;
					}

					break;
					/*case OPT_PREVIEW:
					  if (i) cs->vals[opt] = 1;
					  else cs->vals[opt] = 0;
					  break;*/
				default:
					/* Should never happen */
					return SANE_STATUS_INVAL;
			}
			break;
		case SANE_ACTION_SET_AUTO:
			DBG(2, "sane_control_option: attempt at "
					"automatic control! (unsupported)\n");
			/* Auto? are they mad? I'm not that smart! */
			/* fall through. */
		default:
			return SANE_STATUS_INVAL;
	}


	DBG(2, "<< sane_control_option\n");
	return SANE_STATUS_GOOD;
}


/*************************************************************************
 *
 * sane_get_parameters()
 *
 * Get information about the next packet. If a scan hasn't started, results
 * only have to be best guesses.
 *
 *************************************************************************/
	SANE_Status
sane_get_parameters (SANE_Handle h, SANE_Parameters *params)
{
	int res, max_width, max_height, max_res;
        CANONP_Scanner *cs = ((CANONP_Scanner *)h);
	DBG(2, ">> sane_get_parameters (h=%p, params=%p)\n", (void*)h, 
			(void*)params);

	if (h == NULL) return SANE_STATUS_INVAL;

	if (cs->opened == SANE_FALSE)
	{
		DBG(1,"sane_get_parameters: That scanner (%p) ain't "
				"open yet\n", h);
		return SANE_STATUS_INVAL;
	}

	/* We use 600 res list here because the 300 res list is just a shorter
	 * version, so this will always work. */
	res = res600[cs->vals[OPT_RESOLUTION]];

	/* 
	 * These don't change whether we're scanning or not 
	 * NOTE: Assumes options don't change after scanning commences, which
         *       is part of the standard
	 */

	/* Copy the options stored in the vals into the scaninfo */
	params->pixels_per_line = 
                ((cs->vals[OPT_BR_X] - cs->vals[OPT_TL_X]) * res) / MM_PER_IN;
	params->lines = ((cs->vals[OPT_BR_Y] - cs->vals[OPT_TL_Y]) * res) 
                / MM_PER_IN;

	/* FIXME: Magic numbers ahead! */

	max_res = cs->params.scanheadwidth == 2552 ? 300 : 600;

	/* x values have to be divisible by 4 (round down) */
	params->pixels_per_line -= (params->pixels_per_line%4);

        /* Can't scan less than 64 */
        if (params->pixels_per_line < 64) params->pixels_per_line = 64;

	max_width = cs->params.scanheadwidth / (max_res / res);

	max_height = (cs->params.scanheadwidth == 2552 ? 3508 : 7016) / 
                                        (max_res / res);

        if(params->pixels_per_line > max_width) 
                params->pixels_per_line = max_width;
        if(params->lines > max_height) params->lines = max_height;


	params->depth = cs->vals[OPT_DEPTH] ? 16 : 8;

	switch (cs->vals[OPT_COLOUR_MODE]) 
	{
		case 0:
			params->format = SANE_FRAME_GRAY;
			break;
		case 1:
			params->format = SANE_FRAME_RGB;
			break;
		default:
			/* shouldn't happen */
			break;
	}


	if (!(params->pixels_per_line)) {
		params->last_frame = SANE_TRUE;
		params->lines = 0;
	} 

	/* Always the "last frame" */
	params->last_frame = SANE_TRUE;

	params->bytes_per_line = params->pixels_per_line * (params->depth/8) *
		(cs->vals[OPT_COLOUR_MODE] ? 3 : 1);

	DBG(10, "get_params: bytes_per_line=%d, pixels_per_line=%d, lines=%d\n"
                "max_res=%d, res=%d, max_height=%d, br_y=%d, tl_y=%d, "
		"mm_per_in=%f\n",
                params->bytes_per_line, params->pixels_per_line, params->lines,
                max_res, res, max_height, cs->vals[OPT_BR_Y], 
		cs->vals[OPT_TL_Y], MM_PER_IN);

	DBG(2, "<< sane_get_parameters\n");
	return SANE_STATUS_GOOD;
}


/*************************************************************************
 *
 * sane_start()
 *
 * Starts scanning an image.
 *
 *************************************************************************/
	SANE_Status
sane_start (SANE_Handle h)
{
	unsigned int i, res, max_width, max_height, max_res, tmp;
	CANONP_Scanner *cs = ((CANONP_Scanner *)h);
	DBG(2, ">> sane_start (h=%p)\n", h);

	if (h == NULL) return SANE_STATUS_INVAL;

	if (cs->scanning) return SANE_STATUS_DEVICE_BUSY;
	if (cs->opened == SANE_FALSE)
	{
		DBG(1,"sane_start: That scanner (%p) ain't "
				"open yet\n", h);
		return SANE_STATUS_INVAL;
	}


	/* We use 600 res list here because the 300 res list is just a shorter
	 * version, so this will always work. */
	res = res600[cs->vals[OPT_RESOLUTION]];

	/* Copy the options stored in the vals into the scaninfo */
	cs->scan.width = ((cs->vals[OPT_BR_X] - cs->vals[OPT_TL_X]) * res) 
                / MM_PER_IN;
	cs->scan.height = ((cs->vals[OPT_BR_Y] - cs->vals[OPT_TL_Y]) * res) 
                / MM_PER_IN;

	cs->scan.xoffset = (cs->vals[OPT_TL_X] * res) / MM_PER_IN;
	cs->scan.yoffset = (cs->vals[OPT_TL_Y] * res) / MM_PER_IN;

        /* 
         * These values have to pass the requirements of not exceeding 
         * dimensions (simple clipping) and both width values have to be some 
         * integer multiple of 4 
         */

	/* FIXME: Magic numbers ahead! */

	max_res = cs->params.scanheadwidth == 2552 ? 300 : 600;

	/* x values have to be divisible by 4 (round down) */
	cs->scan.width -= (cs->scan.width%4);
	cs->scan.xoffset -= (cs->scan.xoffset%4);

	/* Can't scan less than 64 */
        if (cs->scan.width < 64) cs->scan.width = 64;

	max_width = cs->params.scanheadwidth / (max_res / res);

	max_height = (cs->params.scanheadwidth == 2552 ? 3508 : 7016) / 
                                        (max_res / res);

        if (cs->scan.width > max_width) cs->scan.width = max_width;
	if (cs->scan.width + cs->scan.xoffset > max_width) cs->scan.xoffset = 
		max_width - cs->scan.width;
        if (cs->scan.height > max_height) cs->scan.height = max_height;

	/* We pass a value to init_scan which is the power of 2 that 75
	 * is multiplied by for the resolution.  ie:
	 * 75 -> 0
	 * 150 -> 1
	 * 300 -> 2
	 * 600 -> 4
	 *
	 * This rather strange parameter is a result of the way the scanner
	 * takes its resolution argument
	 */

	i = 0;
	while (res > 75)
	{
		i++;
		res = res >> 1;
	}

	/* FIXME? xres == yres for now. */
	cs->scan.xresolution = i;
	cs->scan.yresolution = i;

	if (((cs->vals[OPT_BR_Y] - cs->vals[OPT_TL_Y]) <= 0) || 
			((cs->vals[OPT_BR_X] - cs->vals[OPT_TL_X]) <= 0))
	{
		DBG(1,"sane_start: height = %d, Width = %d. "
				"Can't scan void range!",
				cs->scan.height, cs->scan.width);
		return SANE_STATUS_INVAL;
	}

	cs->scan.mode = cs->vals[OPT_COLOUR_MODE];

	DBG(10, ">> init_scan()\n");
	tmp = sanei_canon_pp_init_scan(&(cs->params), &(cs->scan));
	DBG(10, "<< %d init_scan\n", tmp);

	if (tmp != 0) {
		DBG(1,"sane_start: WARNING: init_scan returned %d!", tmp);
		return SANE_STATUS_IO_ERROR;
	}
	cs->scanning = SANE_TRUE;
	cs->cancelled = SANE_FALSE;
	cs->sent_eof = SANE_FALSE;
	cs->lines_scanned = 0;
	cs->bytes_sent = 0;

	DBG(2, "<< sane_start\n");

	return SANE_STATUS_GOOD;
}


/*************************************************************************
 *
 * sane_read()
 *
 * Reads some information from the buffer.
 *
 *************************************************************************/
	SANE_Status
sane_read (SANE_Handle h, SANE_Byte *buf, SANE_Int maxlen, SANE_Int *lenp)
{
	CANONP_Scanner *cs = ((CANONP_Scanner *)h);
	image_segment *is;
	unsigned int lines, bytes, bpl;
	unsigned int i;
	short *shortptr;
	SANE_Byte *charptr;
	int tmp;

	static SANE_Byte *lbuf;
	static unsigned int bytesleft;

	DBG(2, ">> sane_read (h=%p, buf=%p, maxlen=%d)\n", h, 
			(const void *)buf, maxlen);

	/* default to returning 0 - for errors */
	*lenp = 0;

	if ((h == NULL) || (buf == NULL) || (lenp == NULL)) 
	{
		DBG(1, "sane_read: This frontend's passing me dodgy gear! "
				"(h=%p, buf=%p, lenp=%p)\n", 
				(void*)h, (void*)buf, (void*)lenp);
		return SANE_STATUS_INVAL;
	}

	/* Now we have to see if we have some leftover from last time */

	if (read_leftover != NULL) 
	{
		/* feed some more data in until we've run out - don't care 
		 * whether or not we _think_ the scanner is scanning now, 
		 * because we may still have data left over to send */
		DBG(200, "sane_read: didn't send it all last time\n");

		/* Now feed it some data from lbuf */
		if (bytesleft <= (unsigned int)maxlen)
		{
			/* enough buffer to send the lot */
			memcpy(buf, read_leftover, bytesleft);
			free(lbuf);
			*lenp = bytesleft;
			lbuf = NULL;
			read_leftover = NULL;
			bytesleft = 0;
                        cs->bytes_sent += bytesleft;
			return SANE_STATUS_GOOD;

		} else {
			/* only enough to send maxlen */
			memcpy(buf, read_leftover, maxlen);
			read_leftover += maxlen;
			bytesleft -= maxlen;
			*lenp = maxlen;
                        cs->bytes_sent += maxlen;
		        DBG(100, "sane_read: sent %d bytes, still have %d to "
                                "go\n", maxlen, bytesleft);
			return SANE_STATUS_GOOD;
		}

	} 


	/* Has the last scan ended (other than by cancelling)? */
	if (((unsigned)cs->scan.height <= (unsigned)cs->lines_scanned) 
	    || (cs->sent_eof) || !(cs->scanning))
	{
		cs->sent_eof = SANE_TRUE;
		cs->scanning = SANE_FALSE;
		cs->cancelled = SANE_FALSE;
		cs->lines_scanned = 0;
		cs->bytes_sent = 0;
		read_leftover = NULL;
		return SANE_STATUS_EOF;
	}

	/* At this point we have to read more data from the scanner - or the 
	 * scan has been cancelled, which means we have to call read_segment
	 * to leave the scanner consistant */

	/* Decide how many lines we can fit into this buffer */
	if (cs->vals[OPT_DEPTH] == 0)
		bpl = cs->scan.width * (cs->vals[OPT_COLOUR_MODE] ? 3 : 1);
	else
		bpl = cs->scan.width * (cs->vals[OPT_COLOUR_MODE] ? 6 : 2);

        /* New way: scan a whole scanner buffer full, and return as much as 
         * the frontend wants.  It's faster and more reliable since the 
         * scanners crack the shits if we ask for too many small packets */
	lines = (BUF_MAX * 4 / 5) / bpl;

	if (lines > (cs->scan.height - cs->lines_scanned)) 
		lines = cs->scan.height - cs->lines_scanned;

	if (!lines)
	{
		/* can't fit a whole line into the buffer 
                 * (should never happen!) */
		lines = 1;
	}

	bytes = lines * bpl;

	/* Allocate a local buffer to hold the data while we play */
	if ((lbuf = malloc(bytes)) == NULL)
	{
		DBG(10, "sane_read: Not enough memory to hold a "
				"local buffer.  You're doomed\n");
		return SANE_STATUS_NO_MEM;
	}


	/* This call required a lot of debugging information.. */
	DBG(10, "sane_read: Here's what we're sending read_segment:\n");
	DBG(10, "scanner setup: shw=%d xres=%d yres=%d %d %d id=%s\n",
			cs->params.scanheadwidth,
			cs->params.natural_xresolution,
			cs->params.natural_yresolution,
			cs->params.max_xresolution,
			cs->params.max_yresolution,
			(cs->params.id_string)+8);
	DBG(10, "scan_params->: width=%d, height=%d, xoffset=%d, "
			"yoffset=%d\n\txresolution=%d, yresolution=%d, "
			"mode=%d, (lines=%d)\n",
			cs->scan.width, cs->scan.height, 
			cs->scan.xoffset, cs->scan.yoffset,
			cs->scan.xresolution, cs->scan.yresolution,
			cs->scan.mode, lines);

	DBG(2, ">> read_segment(x, x, x, %d, %d, %d)\n",
			lines, cs->cal_valid, 
			cs->scan.height - cs->lines_scanned);
	tmp = sanei_canon_pp_read_segment(&is, &(cs->params), &(cs->scan), 
			lines, cs->cal_valid, 
			cs->scan.height - cs->lines_scanned);
	DBG(2, "<< %d read_segment\n", tmp);

	if (tmp != 0) {
		if (cs->cancelled)
		{
			DBG(10, "sane_read: cancelling.\n");
			cs->sent_eof = SANE_TRUE;
			cs->scanning = SANE_FALSE;
			read_leftover = NULL;
			sanei_canon_pp_abort_scan(&(cs->params));
			return SANE_STATUS_CANCELLED;
		}
		DBG(1, "sane_read: WARNING: read_segment returned %d!\n", tmp);
		return SANE_STATUS_IO_ERROR;
	}

	DBG(10, "sane_read: bpl=%d, lines=%d, bytes=%d\n", bpl, lines, bytes);

	cs->lines_scanned += lines;

	/* translate data out of buffer */
	if (cs->vals[OPT_DEPTH] == 0)
	{
		/* 8bpp */
		for(i = 0; i < bytes; i++)
		{
			charptr = lbuf + i;
			if (cs->vals[OPT_COLOUR_MODE])
			{
				if (i % 3 == 0) charptr += 2;
				if (i % 3 == 2) charptr -= 2;
			}
			*charptr = *((char *)(is->image_data) + (i*2));
		}
	}
	else
	{
		/* 16bpp */
		for(i = 0; i < (bytes/2); i++)
		{
			shortptr = ((short *)lbuf + i);
			if (cs->vals[OPT_COLOUR_MODE])
			{
				if (i % 3 == 0) shortptr += 2;
				if (i % 3 == 2) shortptr -= 2;
			}
			*shortptr = MAKE_SHORT(
					*((char *)(is->image_data) + (i*2)),
					*((char *)(is->image_data) + (i*2)+1)
					);
		}
	}

	/* Free data structures allocated in read_segment */
	free(is->image_data);
	free(is);

	/* Now feed it some data from lbuf */
	if (bytes <= (unsigned int)maxlen)
	{
		/* enough buffer to send the lot */
		memcpy(buf, lbuf, bytes);
		*lenp = bytes;
		free(lbuf);
		lbuf = NULL;
		read_leftover = NULL;
		bytesleft = 0;
                cs->bytes_sent += bytes;

	} else {
		/* only enough to send maxlen */
		memcpy(buf, lbuf, maxlen);
		*lenp = maxlen;
		read_leftover = lbuf + maxlen;
		bytesleft = bytes - maxlen;
                cs->bytes_sent += maxlen;
		DBG(100, "sane_read: sent %d bytes, still have %d to go\n",
                        maxlen, bytesleft);
	}

	if ((unsigned)cs->lines_scanned >= cs->scan.height)
	{
		/* The scan is over! Don't need to call anything in the 
		 * hardware, it will sort itself out */
		DBG(10, "sane_read: Scan is finished.\n");
		cs->scanning = SANE_FALSE;
		cs->lines_scanned = 0;
		cs->bytes_sent = 0;
	}

	DBG(2, "<< sane_read\n");
	return SANE_STATUS_GOOD;
}


/*************************************************************************
 *
 * sane_cancel()
 *
 * Cancels a scan in progress
 *
 *************************************************************************/
	void
sane_cancel (SANE_Handle h)
{
	/* Note: assume handle is valid apart from NULLs */
	CANONP_Scanner *cs = ((CANONP_Scanner *)h);

	DBG(2, ">> sane_cancel (h=%p)\n", h);
	if (h == NULL) return;

	read_leftover = NULL;

	if (!(cs->scanning)) 
	{
		DBG(2, "<< sane_cancel (not scanning)\n");
		return;
	}

	cs->cancelled = SANE_TRUE;
	cs->params.abort_now = 1;

	DBG(2, "<< sane_cancel\n");
}


/*************************************************************************
 *
 * sane_close()
 *
 * Closes a scanner handle.  Scanner is assumed to be free after this.
 *
 *************************************************************************/
	void
sane_close (SANE_Handle h)
{
	/* Note: assume handle is valid apart from NULLs */
	CANONP_Scanner *cs = ((CANONP_Scanner *)h);
	DBG(2, ">> sane_close (h=%p)\n", h);
	if (h == NULL) return;

	if (cs->opened == SANE_FALSE)
	{
		DBG(1,"sane_close: That scanner (%p) ain't "
				"open yet\n", h);
		return;
	}

	/* Put scanner back in transparent mode */
	sanei_canon_pp_close_scanner(&(cs->params));

	cs->opened = SANE_FALSE;
	
	/* if it was scanning, it's not any more */
	cs->scanning = SANE_FALSE;
	cs->sent_eof = SANE_TRUE;

	ieee1284_release(cs->params.port);

	DBG(2, "<< sane_close\n");
}


/*************************************************************************
 *
 * sane_exit()
 *
 * Shut it down!
 *
 *************************************************************************/
	void
sane_exit (void)
{
	CANONP_Scanner *dev, *next;

	DBG(2, ">> sane_exit\n");

	for (dev = first_dev; dev != NULL; dev = next)
	{
		next = dev->next;

		/* These were only created if the scanner has been init'd */
		
		/* Should normally nullify pointers after freeing, but in
		 * this case we're about to free the whole structure so 
		 * theres not a lot of point. */

		/* Constraints (mostly) allocated when the scanner is opened */
		if(dev->opt[OPT_TL_X].constraint.range)
			free((void *)(dev->opt[OPT_TL_X].constraint.range));
		if(dev->opt[OPT_TL_Y].constraint.range)
			free((void *)(dev->opt[OPT_TL_Y].constraint.range));
		if(dev->opt[OPT_BR_X].constraint.range)
			free((void *)(dev->opt[OPT_BR_X].constraint.range));
		if(dev->opt[OPT_BR_Y].constraint.range)
			free((void *)(dev->opt[OPT_BR_Y].constraint.range));

		/* Weights file now on a per-scanner basis */
		if (dev->weights_file != NULL)
			free(dev->weights_file);

		if (dev->scanner_present)
		{
			if (dev->opened == SANE_TRUE)
			{
				/* naughty boys, should have closed first */
				ieee1284_release(dev->params.port);
			}
			ieee1284_close(dev->params.port);
		}

		free (dev);
	}

	first_dev = NULL;
	def_scanner = NULL;
	read_leftover = NULL;
	num_devices = 0;

	/* FIXEDME: this created a segfault in DLL code. */
	/* Bug was fixed in libieee1284 0.1.5 */
	ieee1284_free_ports(&pl);

	DBG(2, "<< sane_exit\n");
}


/*************************************************************************
 *
 * init_device()
 *
 * (Not part of the SANE API)
 *
 * Initialises a CANONP_Scanner data structure for a new device.
 * NOTE: The device is not ready to scan until initialise() has been 
 * called in scan library!
 *
 *************************************************************************/
static SANE_Status init_device(struct parport *pp) 
{
	int i;
	static const char *hw_vendor = "CANON";
	static const char *hw_type = "flatbed scanner";
	static const char *opt_names[] = {
		SANE_NAME_NUM_OPTIONS, 
		SANE_NAME_SCAN_RESOLUTION,
		SANE_NAME_SCAN_MODE,
		SANE_NAME_BIT_DEPTH,
		SANE_NAME_SCAN_TL_X,
		SANE_NAME_SCAN_TL_Y,
		SANE_NAME_SCAN_BR_X,
		SANE_NAME_SCAN_BR_Y,
		SANE_NAME_QUALITY_CAL
#if 0
		SANE_NAME_GAMMA_R,
		SANE_NAME_GAMMA_G,
		SANE_NAME_GAMMA_B
#endif
	};
	static const char *opt_titles[] = {
		SANE_TITLE_NUM_OPTIONS, 
		SANE_TITLE_SCAN_RESOLUTION,
		SANE_TITLE_SCAN_MODE,
		SANE_TITLE_BIT_DEPTH,
		SANE_TITLE_SCAN_TL_X,
		SANE_TITLE_SCAN_TL_Y,
		SANE_TITLE_SCAN_BR_X,
		SANE_TITLE_SCAN_BR_Y,
		SANE_TITLE_QUALITY_CAL
#if 0
		SANE_TITLE_GAMMA_R,
		SANE_TITLE_GAMMA_G,
		SANE_TITLE_GAMMA_B
#endif
	};
	static const char *opt_descs[] = {
		SANE_DESC_NUM_OPTIONS, 
		SANE_DESC_SCAN_RESOLUTION,
		SANE_DESC_SCAN_MODE,
		SANE_DESC_BIT_DEPTH,
		SANE_DESC_SCAN_TL_X,
		SANE_DESC_SCAN_TL_Y,
		SANE_DESC_SCAN_BR_X,
		SANE_DESC_SCAN_BR_Y,
		SANE_DESC_QUALITY_CAL
#if 0
		SANE_DESC_GAMMA_R,
		SANE_DESC_GAMMA_G,
		SANE_DESC_GAMMA_B
#endif
	};

	CANONP_Scanner *cs = NULL;

	DBG(2, ">> init_device\n");

	cs = malloc(sizeof(*cs));
	if (cs == NULL)
	{
		return SANE_STATUS_NO_MEM;
	}
	memset(cs, 0, sizeof(*cs));

#if 0
	if ((cs->params.port = malloc(sizeof(*(cs->params.port)))) == NULL) 
		return SANE_STATUS_NO_MEM;

	memcpy(cs->params.port, pp, sizeof(*pp));
#endif

	cs->params.port = pp;

	/* ensure these are null to start off with, otherwise they might be
	 * erroneously free'd.  Note that we set everything to 0 above
	 * but that's not *always* the same thing */
	cs->params.blackweight = NULL;
	cs->params.redweight = NULL;
	cs->params.greenweight = NULL;
	cs->params.blueweight = NULL;

	/* Set some sensible defaults */
	cs->hw.name = cs->params.port->name;
	cs->hw.vendor = hw_vendor;
	cs->hw.type = hw_type;
	cs->opened = SANE_FALSE;
	cs->scanning = SANE_FALSE;
	cs->cancelled = SANE_FALSE;
	cs->sent_eof = SANE_TRUE;
	cs->lines_scanned = 0;
	cs->bytes_sent = 0;
	cs->init_mode = INITMODE_AUTO;

	DBG(10, "init_device: [configuring options]\n");

	/* take a punt at each option, then we change it later */
	for (i = 0; i < NUM_OPTIONS; i++)
	{
		cs->opt[i].name = opt_names[i];
		cs->opt[i].title = opt_titles[i];
		cs->opt[i].desc = opt_descs[i];
		cs->opt[i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
		cs->opt[i].type = SANE_TYPE_INT;
		cs->opt[i].size = sizeof(SANE_Int);
	}

	DBG(100, "init_device: configuring opt: num_options\n");
	/* The number of options option */

	cs->opt[OPT_NUM_OPTIONS].unit = SANE_UNIT_NONE;
	cs->opt[OPT_NUM_OPTIONS].cap = SANE_CAP_SOFT_DETECT;
	cs->vals[OPT_NUM_OPTIONS] = NUM_OPTIONS;

	DBG(100, "init_device: configuring opt: resolution\n");

	/* The resolution of scanning (X res == Y res for now)*/
	cs->opt[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
	cs->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
	/* should never point at first element (wordlist size) */
	cs->vals[OPT_RESOLUTION] = 1; 

	DBG(100, "init_device: configuring opt: colour mode\n");

	/* The colour mode (0=grey 1=rgb) */
	cs->opt[OPT_COLOUR_MODE].type = SANE_TYPE_STRING;
	cs->opt[OPT_COLOUR_MODE].size = 20;
	cs->opt[OPT_COLOUR_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
	/* Set this one here because it doesn't change by scanner (yet) */
	cs->opt[OPT_COLOUR_MODE].constraint.string_list = cmodes;

	DBG(100, "init_device: configuring opt: bit depth\n");

	/* The bit depth */
	cs->opt[OPT_DEPTH].type = SANE_TYPE_STRING;
	cs->opt[OPT_DEPTH].size = 20;
	cs->opt[OPT_DEPTH].cap |= SANE_CAP_EMULATED;
	cs->opt[OPT_DEPTH].constraint_type = SANE_CONSTRAINT_STRING_LIST;
	cs->opt[OPT_DEPTH].constraint.string_list = depths;

	DBG(100, "init_device: configuring opt: tl-x\n");

	/* The top-left-x */
	cs->opt[OPT_TL_X].unit = SANE_UNIT_MM;
	cs->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;

	DBG(100, "init_device: configuring opt: tl-y\n");

	/* The top-left-y */
	cs->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
	cs->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;

	DBG(100, "init_device: configuring opt: br-x\n");

	/* The bottom-right-x */
	cs->opt[OPT_BR_X].unit = SANE_UNIT_MM;
	cs->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
	/* default scan width */
	cs->vals[OPT_BR_X] = 100;

	DBG(100, "init_device: configuring opt: br-y\n");

	/* The bottom-right-y */
	cs->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
	cs->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
	cs->vals[OPT_BR_Y] = 100;

	DBG(100, "init_device: configuring opt: calibrate\n");

	/* The calibration button */
	cs->opt[OPT_CAL].type = SANE_TYPE_BUTTON;
	cs->opt[OPT_CAL].constraint_type = SANE_CONSTRAINT_NONE;
	if (cs->cal_readonly) 
		cs->opt[OPT_CAL].cap |= SANE_CAP_INACTIVE; 

#if 0
	/* the gamma values (once we do them) */
	cs->opt[OPT_GAMMA_R].caps |= SANE_CAP_ADVANCED;
	cs->opt[OPT_GAMMA_G].caps |= SANE_CAP_ADVANCED;
	cs->opt[OPT_GAMMA_B].caps |= SANE_CAP_ADVANCED;
#endif

	/*
	 * NOTE: Ranges and lists are actually set when scanner is opened, 
	 * becase that's when we find out what sort of scanner it is 
	 */

	DBG(100, "init_device: done opts\n");

	/* add it to the head of the tree */
	cs->next = first_dev;
	first_dev = cs;

	num_devices++;

	DBG(2, "<< init_device\n");

	return SANE_STATUS_GOOD;
}


/*************************************************************************
 *
 * These two are optional ones... maybe if I get really keen? 
 *
 *************************************************************************/
	SANE_Status
sane_set_io_mode (SANE_Handle h, SANE_Bool non_blocking)
{
	DBG(2, ">> sane_set_io_mode (%p, %d) (not really supported)\n",
			h, non_blocking);

	if (non_blocking == SANE_FALSE)
		return SANE_STATUS_GOOD;

	DBG(2, "<< sane_set_io_mode\n");
	return SANE_STATUS_UNSUPPORTED;
}

	SANE_Status
sane_get_select_fd (SANE_Handle h, SANE_Int *fdp)
{
	DBG(2, ">> sane_get_select_fd (%p, %p) (not supported)\n", h, 
			(const void *)fdp);
	DBG(2, "<< sane_get_select_fd\n");
	return SANE_STATUS_UNSUPPORTED;
}


/*************************************************************************
 *
 * init_cal(): Try to create a calibration file
 * has to be changed.
 *
 ************************************************************************/
static int init_cal(char *file)
{
	char *tmp, *path;
	int f, i;

	if ((f = open(file, O_CREAT | O_WRONLY, 0600)) < 0)
	{
		if (errno == ENOENT)
		{
			/* we need to try and make ~/.sane perhaps -
			 * find the last / in the file path, and try 
			 * to create it */
			if ((tmp = strrchr(file, '/')) == NULL)
				return -1;
			path = strdup(file);
			*(path + (tmp-file)) = '\0';
			i = mkdir(path, 0777);
			free(path);
			if (i) return -1;
			/* Path has been created, now try this again.. */
			if ((f = open(file, O_CREAT | O_WRONLY, 0600)) < 0)
				return -1;
		}
		else 
		{
			/* Error is something like access denied - too 
			 * hard to fix, so i give up... */
			return -1;
		}
	}
	/* should probably set defaults here.. */
	close(f);
	return 0;
}

/*************************************************************************
 *
 * fix_weights_file(): Ensures that the weights_file setting for a given 
 * scanner is valid
 *
 ************************************************************************/
static SANE_Status fix_weights_file(CANONP_Scanner *cs)
{
	char *tmp, *myhome, buf[PATH_MAX];
	int i;
	struct stat *f_stat;


	if (cs == NULL)
	{
		DBG(0, "fix_weights_file: FATAL: NULL passed by my code, "
				"please report this!\n");
		return SANE_STATUS_INVAL;
	}

	/* Assume this is false and then correct it */
	cs->cal_readonly = SANE_FALSE;

	if (cs->weights_file == NULL)
	{
		/* Will be of form canon_pp-calibration-parport0 or -0x378 */
		sprintf(buf, "~/.sane/canon_pp-calibration-%s",
				cs->params.port->name);
		cs->weights_file = strdup(buf);
	}

	/* Get the user's home dir if they used ~ */
	if (cs->weights_file[0] == '~')
	{
		if ((tmp = malloc(PATH_MAX)) == NULL) 
			return SANE_STATUS_NO_MEM;
		if ((myhome = getenv("HOME")) == NULL)
		{
			DBG(0,"fix_weights_file: FATAL: ~ used, but $HOME not"
					" set!\n");
			free(tmp);
			tmp = NULL;
			return SANE_STATUS_INVAL;
		}
		strncpy(tmp, myhome, PATH_MAX);
		strncpy(tmp+strlen(tmp), (cs->weights_file)+1, 
				PATH_MAX-strlen(tmp));

		free(cs->weights_file);
		cs->weights_file = tmp;
	}

	if ((f_stat = malloc(sizeof(*f_stat))) == NULL)
		return SANE_STATUS_NO_MEM;

	if(stat(cs->weights_file, f_stat))
	{
		/* this non-intuitive if basically is if we got some error that
		 * wasn't no-such-file, or we can't create the file.. */
		if ((errno != ENOENT) || init_cal(cs->weights_file))
		{
			/* Some nasty error returned. Give up. */
			DBG(2,"fix_weights_file: error stating cal file"
					" (%s)\n", strerror(errno));
			DBG(2,"fix_weights_file: Changes to cal data won't"
					" be saved!\n");
			free(cs->weights_file);
			cs->weights_file = NULL;
		}
	}
	else
	{

		/* No error returned.. Check read/writability */
		i = open(cs->weights_file, O_RDWR | O_APPEND);
		if (i <= 0)
		{
			DBG(10,"fix_weighs_file: Note: Changes to cal data "
					"won't be saved!\n");
			i = open(cs->weights_file, O_RDONLY);
			if (i <= 0)
			{
				/* 
				 * Open failed (do i care why?) 
				 */
				DBG(2,"fix_weights_file: error opening cal "
						"(%s)\n", strerror(errno));
				free(cs->weights_file);
				cs->weights_file = NULL;
			}
			else
			{
				DBG(2,"fix_weights_file: file is read-only, "
						"changes won't be saved\n");
				cs->cal_readonly = SANE_TRUE;
				close(i);
			}
		}
		else
		{
			/* good! */
			DBG(10,"fix_weights_file: Calibration file is good "
					"for opening!\n");
			close(i);
		}
	}

	/* cleanup */
	free(f_stat);

	return SANE_STATUS_GOOD;
}

/* detect_mode
 * PRE:
 *	cs->params.port is not open
 * POST:
 * 	cs->params.port is left opened iff SANE_STATUS_GOOD returned. 
 */

SANE_Status detect_mode(CANONP_Scanner *cs) 
{

	int capabilities, tmp;

	/* Open then claim parallel port using libieee1284 */
	DBG(10,"detect_mode: Opening port %s\n", (cs->params.port->name));

	tmp = ieee1284_open(cs->params.port, 0, &capabilities);

	if (tmp != E1284_OK)
	{
		switch (tmp)
		{
			case E1284_INVALIDPORT:
				DBG(1, "detect_mode: Invalid port.\n");
				break;
			case E1284_SYS:
				DBG(1, "detect_mode: System error: %s\n", 
						strerror(errno));
				break;
			case E1284_INIT:
				DBG(1, "detect_mode: Initialisation error.\n");
				break;
			default:
				DBG(1, "detect_mode: Unknown error.\n");
				break;
		}
		return SANE_STATUS_IO_ERROR;
	}

	DBG(10,"detect_mode: Claiming port.\n");

	if (ieee1284_claim(cs->params.port) != E1284_OK)
	{
		DBG(1,"detect_mode: Unable to claim port\n");
		ieee1284_close(cs->params.port);
		return SANE_STATUS_IO_ERROR;
	}


	/* Check that compatibility-mode (required) is supported */
	if (!(capabilities & CAP1284_COMPAT))
	{
		DBG(0,"detect_mode: Compatibility mode (required) not "
				"supported.\n");
		ieee1284_release(cs->params.port);
		ieee1284_close(cs->params.port);
		return SANE_STATUS_IO_ERROR;
	}

	/* Check capabilities which will enchance speed */
	if (capabilities & CAP1284_ECP)
		DBG(2, "detect_mode: Port supports ECP-H.\n");
	else if (capabilities & CAP1284_ECPSWE)
		DBG(2, "detect_mode: Port supports ECP-S.\n");
	if (capabilities & CAP1284_IRQ)
		DBG(2, "detect_mode: Port supports interrupts.\n");
	if (capabilities & CAP1284_DMA)
		DBG(2, "detect_mode: Port supports DMA.\n");

	/* Check whether ECP mode is possible */
	if (capabilities & CAP1284_ECP)
	{
		cs->ieee1284_mode = M1284_ECP;
		DBG(10, "detect_mode: Using ECP-H Mode\n");
	}	
	else if (capabilities & CAP1284_ECPSWE)
	{
		cs->ieee1284_mode = M1284_ECPSWE;
		DBG(10, "detect_mode: Using ECP-S Mode\n");
	}
	else if (capabilities & CAP1284_NIBBLE)
	{
		cs->ieee1284_mode = M1284_NIBBLE;
		DBG(10, "detect_mode: Using nibble mode\n");
	}
	else
	{
		DBG(0, "detect_mode: No supported parport modes available!\n");
		ieee1284_release(cs->params.port);
		ieee1284_close(cs->params.port);
		return SANE_STATUS_IO_ERROR;
	}

	/* Check to make sure ECP mode really is supported */
	/* Have disabled the hardware ECP check because it's always supported 
	 * by libieee1284 now, and it's too prone to hitting a ppdev bug 
	 */

	/* Disabled check entirely.. check now in initialise when we 
	 * actually do a read */
#if 0
	if ((cs->ieee1284_mode == M1284_ECP) ||
			(cs->ieee1284_mode == M1284_ECPSWE))
	{
		DBG(1, "detect_mode: attempting a 0 byte read, if we hang "
				"here, it's a ppdev bug!\n");
		/* 
		 * 29/06/02 
		 * NOTE:
		 * This causes an infinite loop in ppdev on 2.4.18.  
		 * Not checking on hardware ECP mode should work-around 
		 * effectively.
		 *
		 * I have sent email to twaugh about it, should be fixed in 
		 * 2.4.19 and above.
		 */
		if (ieee1284_ecp_read_data(cs->params.port, 0, NULL, 0) == 
				E1284_NOTIMPL)
		{
			DBG(10, "detect_mode: Your version of libieee1284 "
					"doesn't support ECP mode - defaulting"
					" to nibble mode instead.\n");
			cs->ieee1284_mode = M1284_NIBBLE;
		}
	}
#endif

	if (force_nibble == SANE_TRUE) {
		DBG(10, "detect_mode: Nibble mode force in effect.\n");
		cs->ieee1284_mode = M1284_NIBBLE;
	}

	ieee1284_release(cs->params.port);

	sanei_canon_pp_set_ieee1284_mode(cs->ieee1284_mode);

	return SANE_STATUS_GOOD;
}
