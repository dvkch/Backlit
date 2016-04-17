/* 
   tstbackend -- backend test utility

   Uses the SANE library.
   Copyright (C) 2002 Frank Zago (sane at zago dot net)
   Copyright (C) 2013 Stéphane Voltz <stef.dev@free.fr> : sane_get_devices test

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
*/

#define BUILD 19				/* 2013-03-29 */

#include "../include/sane/config.h"

#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"

static struct option basic_options[] = {
	{"device-name", required_argument, NULL, 'd'},
	{"level", required_argument, NULL, 'l'},
	{"scan", NULL, NULL, 's'},
	{"recursion", required_argument, NULL, 'r'},
	{"get-devices", required_argument, NULL, 'g'},
	{"help", 0, NULL, 'h'}
};

static void
test_options (SANE_Device * device, int can_do_recursive);

enum message_level {
	MSG,						/* info message */
	INF,						/* non-urgent warning */
	WRN,						/* warning */
	ERR,						/* error, test can continue */
	FATAL,						/* error, test can't/mustn't continue */
	BUG							/* bug in tstbackend */
};

int message_number_wrn = 0;
int message_number_err = 0;
#ifdef HAVE_LONG_LONG
long long checks_done = 0;
#else
/* It may overflow, but it's no big deal. */
long int checks_done = 0;
#endif

int test_level;
int verbose_level;

/* Maybe add that to sane.h */
#define SANE_OPTION_IS_GETTABLE(cap)	(((cap) & (SANE_CAP_SOFT_DETECT | SANE_CAP_INACTIVE)) == SANE_CAP_SOFT_DETECT)

/*--------------------------------------------------------------------------*/

/* Display the message error statistics. */
static void display_stats(void)
{
#ifdef HAVE_LONG_LONG
	printf("warnings: %d  error: %d  checks: %lld\n", 
		   message_number_wrn, message_number_err, checks_done);
#else
	printf("warnings: %d  error: %d  checks: %ld\n", 
		   message_number_wrn, message_number_err, checks_done);
#endif
}

/* 
 * If the condition is false, display a message with some headers
 * depending on the level.
 *
 * Returns the condition.
 *
 */
#ifdef __GNUC__
static int check(enum message_level, int condition, const char *format, ...) __attribute__ ((format (printf, 3, 4)));
#endif
static int check(enum message_level level, int condition, const char *format, ...)
{
	char str[1000];
	va_list args;

	if (level != MSG && level != INF) checks_done ++;
	
	if (condition != 0) 
		return condition;

	va_start(args, format);
	vsprintf(str, format, args);
	va_end(args);

	switch(level) {
	case MSG:
		printf("          %s\n", str);
		break;
	case INF:					/* info */
		printf("info    : %s\n", str);
		break;
	case WRN:					/* warning */
		printf("warning : %s\n", str);
		message_number_wrn ++;
		break;
	case ERR:					/* error */
		printf("ERROR   : %s\n", str);
		message_number_err ++;
		break;
	case FATAL:					/* fatal error */
		printf("FATAL ERROR : %s\n", str);
		message_number_err ++;
		break;		
	case BUG:					/* bug in tstbackend */
		printf("tstbackend BUG : %s\n", str);
		break;				
	}

	if (level == FATAL || level == BUG) {
		/* Fatal error. Generate a core dump. */
		display_stats();
		abort();
	}

	fflush(stdout);

	return(0);
}

/*--------------------------------------------------------------------------*/

#define GUARDS_SIZE 4			/* 4 bytes */
#define GUARD1 ((SANE_Word)0x5abf8ea5)
#define GUARD2 ((SANE_Word)0xa58ebf5a)

/* Allocate the requested memory plus enough room to store some guard bytes. */
static void *guards_malloc(size_t size)
{
	unsigned char *ptr;

	size += 2*GUARDS_SIZE;
	ptr = malloc(size);

	assert(ptr);

	ptr += GUARDS_SIZE;

	return(ptr);
}

/* Free some memory allocated by guards_malloc. */
static void guards_free(void *ptr)
{
	unsigned char *p = ptr;

	p -= GUARDS_SIZE;
	free(p);
}

/* Set the guards */
static void guards_set(void *ptr, size_t size)
{
	SANE_Word *p;

	p = (SANE_Word *)(((unsigned char *)ptr) - GUARDS_SIZE);
	*p = GUARD1;

	p = (SANE_Word *)(((unsigned char *)ptr) + size);
	*p = GUARD2;
}

/* Check that the guards have not been tampered with. */
static void guards_check(void *ptr, size_t size)
{
	SANE_Word *p;

	p = (SANE_Word *)(((unsigned char *)ptr) - GUARDS_SIZE);
	check(FATAL, (*p == GUARD1),
		  "guard before the block has been tampered");

	p = (SANE_Word *)(((unsigned char *)ptr) + size);
	check(FATAL, (*p == GUARD2),
		  "guard after the block has been tampered");
}

/*--------------------------------------------------------------------------*/

static void 
test_parameters (SANE_Device * device, SANE_Parameters *params)
{
	SANE_Status status;
	SANE_Parameters p;

	status = sane_get_parameters (device, &p);
	check(FATAL, (status == SANE_STATUS_GOOD), 
		  "cannot get the parameters (error %s)", sane_strstatus(status));

	check(FATAL, ((p.format == SANE_FRAME_GRAY) ||
				  (p.format == SANE_FRAME_RGB) ||
				  (p.format == SANE_FRAME_RED) ||
				  (p.format == SANE_FRAME_GREEN) ||
				  (p.format == SANE_FRAME_BLUE)),
		  "parameter format is not a known SANE_FRAME_* (%d)", p.format);

	check(FATAL, ((p.last_frame == SANE_FALSE) || 
				  (p.last_frame == SANE_TRUE)),
		  "parameter last_frame is neither SANE_FALSE or SANE_TRUE (%d)", p.last_frame);

	check(FATAL, ((p.depth == 1) ||
				  (p.depth == 8) ||
				  (p.depth == 16)),
		  "parameter depth is neither 1, 8 or 16 (%d)", p.depth);

	if (params) {
		*params = p;
	}
}

/* Try to set every option in a word list. */
static void
test_options_word_list (SANE_Device * device, int option_num, 
						const SANE_Option_Descriptor *opt,
						int can_do_recursive)
{
	SANE_Status status;
	int i;
	SANE_Int val_int;
	SANE_Int info;

	check(FATAL, (opt->type == SANE_TYPE_INT || 
			  opt->type == SANE_TYPE_FIXED),
		  "type must be SANE_TYPE_INT or SANE_TYPE_FIXED (%d)", opt->type);

	if (!SANE_OPTION_IS_SETTABLE(opt->cap)) return;

	for (i=1; i<opt->constraint.word_list[0]; i++) {
		
		info = 0x1010;			/* garbage */

		val_int = opt->constraint.word_list[i];
		status = sane_control_option (device, option_num, 
									  SANE_ACTION_SET_VALUE, &val_int, &info);
		
		check(FATAL, (status == SANE_STATUS_GOOD),
			  "cannot set a settable option (status=%s)", sane_strstatus(status));

		check(WRN, ((info & ~(SANE_INFO_RELOAD_OPTIONS | 
							SANE_INFO_RELOAD_PARAMS)) == 0),
			  "sane_control_option set an invalid info (%d)", info);

		if ((info & SANE_INFO_RELOAD_OPTIONS) && can_do_recursive) {
			test_options(device, can_do_recursive-1);
		}
		if (info & SANE_INFO_RELOAD_PARAMS) {
			test_parameters(device, NULL);
		}

		/* The option might have become inactive or unsettable. Skip it. */
		if (!SANE_OPTION_IS_ACTIVE(opt->cap) || 
			!SANE_OPTION_IS_SETTABLE(opt->cap)) 
			return;

	}
}

/* Try to set every option in a string list. */
static void
test_options_string_list (SANE_Device * device, int option_num, 
						  const SANE_Option_Descriptor *opt,
						  int can_do_recursive)
{
	SANE_Int info;
	SANE_Status status;
	SANE_String val_string;
	int i;

	check(FATAL, (opt->type == SANE_TYPE_STRING),
		  "type must be SANE_TYPE_STRING (%d)", opt->type);

	if (!SANE_OPTION_IS_SETTABLE(opt->cap)) return;

	for (i=0; opt->constraint.string_list[i] != NULL; i++) {

		val_string = strdup(opt->constraint.string_list[i]);
		assert(val_string);

		check(WRN, (strlen(val_string) < (size_t)opt->size),
			  "string [%s] is longer than the max size (%d)",
			  val_string, opt->size);

		info = 0xE1000;			/* garbage */
		status = sane_control_option (device, option_num, 
									  SANE_ACTION_SET_VALUE, val_string, &info);

		check(FATAL, (status == SANE_STATUS_GOOD),
			  "cannot set a settable option (status=%s)", sane_strstatus(status));

		check(WRN, ((info & ~(SANE_INFO_RELOAD_OPTIONS | 
							SANE_INFO_RELOAD_PARAMS)) == 0),
			  "sane_control_option set an invalid info (%d)", info);

		free(val_string);

		if ((info & SANE_INFO_RELOAD_OPTIONS) && can_do_recursive) {
			test_options(device, can_do_recursive-1);
		}
		if (info & SANE_INFO_RELOAD_PARAMS) {
			test_parameters(device, NULL);
		}

		/* The option might have become inactive or unsettable. Skip it. */
		if (!SANE_OPTION_IS_ACTIVE(opt->cap) || 
			!SANE_OPTION_IS_SETTABLE(opt->cap)) 
			return;
	}
}

/* Test the consistency of the options. */
static void
test_options (SANE_Device * device, int can_do_recursive)
{
	SANE_Word info;
	SANE_Int num_dev_options;
	SANE_Status status;
	const SANE_Option_Descriptor *opt;
	int option_num;
	void *optval;				/* value for the option */
	size_t optsize;				/* size of the optval buffer */

	/* 
	 * Test option 0 
	 */
	opt = sane_get_option_descriptor (device, 0);
	check(FATAL, (opt != NULL),
		  "cannot get option descriptor for option 0 (it must exist)");
	check(INF, (opt->cap == SANE_CAP_SOFT_DETECT),
		  "invalid capabilities for option 0 (%d)", opt->cap);
	check(ERR, (opt->type == SANE_TYPE_INT),
		  "option 0 type must be SANE_TYPE_INT");

	/* Get the number of options. */
	status = sane_control_option (device, 0, SANE_ACTION_GET_VALUE, &num_dev_options, 0);
	check(FATAL, (status == SANE_STATUS_GOOD),
		  "cannot get option 0 value");

	/* Try to change the number of options. */
	status = sane_control_option (device, 0, SANE_ACTION_SET_VALUE,
								  &num_dev_options, &info);
	check(WRN, (status != SANE_STATUS_GOOD),
		  "the option 0 value can be set");

	/* 
	 * Test all options
	 */
	option_num = 0;
	for (option_num = 0; option_num < num_dev_options; option_num++) {

		/* Get the option descriptor */
		opt = sane_get_option_descriptor (device, option_num);
		check(FATAL, (opt != NULL),
			  "cannot get option descriptor for option %d", option_num);
		check(WRN, ((opt->cap & ~(SANE_CAP_SOFT_SELECT |
					SANE_CAP_HARD_SELECT |
					SANE_CAP_SOFT_DETECT |
					SANE_CAP_EMULATED |
					SANE_CAP_AUTOMATIC |
					SANE_CAP_INACTIVE |
					SANE_CAP_ADVANCED)) == 0),
			  "invalid capabilities for option [%d, %s] (%x)", option_num, opt->name, opt->cap);
		check(WRN, (opt->title != NULL),
			  "option [%d, %s] must have a title", option_num, opt->name);
		check(WRN, (opt->desc != NULL),
			  "option [%d, %s] must have a description", option_num, opt->name);

		if (!SANE_OPTION_IS_ACTIVE (opt->cap)) {
			/* Option not active. Skip the remaining tests. */
			continue;
		}

		if(verbose_level) {
			printf("checking option ""%s""\n",opt->title);
		}

		if (opt->type == SANE_TYPE_GROUP) {
			check(INF, (opt->name == NULL || *opt->name == 0),
				  "option [%d, %s] has a name", option_num, opt->name);
			check(ERR, (!SANE_OPTION_IS_SETTABLE (opt->cap)),
				  "option [%d, %s], group option is settable", option_num, opt->name);
		} else {
			if (option_num == 0) {
				check(ERR, (opt->name != NULL && *opt->name ==0),
					  "option 0 must have an empty name (ie. \"\")");
			} else {
				check(ERR, (opt->name != NULL && *opt->name !=0),
					  "option %d must have a name", option_num);
			}
		}

		/* The option name must contain only "a".."z",
		   "0".."9" and "-" and must start with "a".."z". */
		if (opt->name && opt->name[0]) {
			const char *p = opt->name;

			check(ERR, (*p >= 'a' && *p <= 'z'),
				  "name for option [%d, %s] must start with in letter in [a..z]",
				  option_num, opt->name);

			p++;

			while(*p) {
				check(ERR, ((*p >= 'a' && *p <= 'z') ||
							(*p == '-') ||
							(*p >= '0' && *p <= '9')),
					  "name for option [%d, %s] must only have the letters [-a..z0..9]",
					  option_num, opt->name);
				p++;
			}
		}

		optval = NULL;
		optsize = 0;

		switch(opt->type) {
		case SANE_TYPE_BOOL:
			check(WRN, (opt->size == sizeof(SANE_Word)),
				  "size of option %s is incorrect", opt->name);
			optval = guards_malloc(opt->size);
			optsize = opt->size;
			check(WRN, (opt->constraint_type == SANE_CONSTRAINT_NONE),
				  "invalid constraint type for option [%d, %s] (%d)", option_num, opt->name, opt->constraint_type);
			break;
			
		case SANE_TYPE_INT:
		case SANE_TYPE_FIXED:
			check(WRN, (opt->size > 0 && (opt->size % sizeof(SANE_Word) == 0)),
				  "invalid size for option %s", opt->name);
			optval = guards_malloc(opt->size);
			optsize = opt->size;
			check(WRN, (opt->constraint_type == SANE_CONSTRAINT_NONE ||
						opt->constraint_type == SANE_CONSTRAINT_RANGE ||
						opt->constraint_type == SANE_CONSTRAINT_WORD_LIST),
				  "invalid constraint type for option [%d, %s] (%d)", option_num, opt->name, opt->constraint_type);
			break;

		case SANE_TYPE_STRING:	
			check(WRN, (opt->size >= 1),
				  "size of option [%d, %s] must be at least 1 for the NUL terminator", option_num, opt->name);
			check(INF, (opt->unit == SANE_UNIT_NONE),
				  "unit of option [%d, %s] is not SANE_UNIT_NONE", option_num, opt->name);
			check(WRN, (opt->constraint_type == SANE_CONSTRAINT_STRING_LIST ||
					  opt->constraint_type == SANE_CONSTRAINT_NONE),
				  "invalid constraint type for option [%d, %s] (%d)", option_num, opt->name, opt->constraint_type);
			optval = guards_malloc(opt->size);
			optsize = opt->size;
			break;

		case SANE_TYPE_BUTTON:  
		case SANE_TYPE_GROUP:
			check(INF, (opt->unit == SANE_UNIT_NONE),
				  "option [%d, %s], unit is not SANE_UNIT_NONE", option_num, opt->name);
			check(INF, (opt->size == 0),
				  "option [%d, %s], size is not 0", option_num, opt->name);
			check(WRN, (opt->constraint_type == SANE_CONSTRAINT_NONE),
				  "invalid constraint type for option [%d, %s] (%d)", option_num, opt->name, opt->constraint_type);
			break;

		default:
			check(ERR, 0,
				  "invalid type %d for option %s", 
				  opt->type, opt->name);
			break;
		}
		
		if (optval) {
			/* This is an option with a value */

			/* get with NULL info.
			 *
			 * The SANE standard is not explicit on that subject. I
			 * consider that an inactive option shouldn't be read by a
			 * frontend because its value is meaningless. I think
			 * that, in that case, SANE_STATUS_INVAL is an appropriate
			 * return.  
			 */
			guards_set(optval, optsize);
			status = sane_control_option (device, option_num,
										  SANE_ACTION_GET_VALUE, optval, NULL);
			guards_check(optval, optsize);

			if (SANE_OPTION_IS_GETTABLE (opt->cap)) {
				check(ERR, (status == SANE_STATUS_GOOD),
					  "cannot get option [%d, %s] value, although it is active (%s)", option_num, opt->name, sane_strstatus(status));
			} else {
				check(ERR, (status == SANE_STATUS_INVAL),
					  "was able to get option [%d, %s] value, although it is not active", option_num, opt->name);
			}

			/* set with NULL info */
			guards_set(optval, optsize);
			status = sane_control_option (device, option_num, 
										  SANE_ACTION_SET_VALUE, optval, NULL);
			guards_check(optval, optsize);
			if (SANE_OPTION_IS_SETTABLE (opt->cap) && SANE_OPTION_IS_ACTIVE (opt->cap)) {
				check(ERR, (status == SANE_STATUS_GOOD),
					  "cannot set option [%d, %s] value, although it is active and settable (%s)", option_num, opt->name, sane_strstatus(status));
			} else {
				check(ERR, (status == SANE_STATUS_INVAL),
					  "was able to set option [%d, %s] value, although it is not active or settable", option_num, opt->name);
			}  
			
			/* Get with invalid info. Since if is a get, info should be either
			 * ignored or set to 0. */
			info = 0xdeadbeef;
			guards_set(optval, optsize);
			status = sane_control_option (device, option_num, SANE_ACTION_GET_VALUE, 
										  optval, &info);
			guards_check(optval, optsize);
			if (SANE_OPTION_IS_GETTABLE (opt->cap)) {
				check(ERR, (status == SANE_STATUS_GOOD),
					  "cannot get option [%d, %s] value, although it is active (%s)", option_num, opt->name, sane_strstatus(status));
			} else {
				check(ERR, (status == SANE_STATUS_INVAL),
					  "was able to get option [%d, %s] value, although it is not active", option_num, opt->name);
			}
			check(ERR, ((info == (SANE_Int)0xdeadbeef) || (info == 0)),
				  "when getting option [%d, %s], info was set to %x", option_num, opt->name, info);
			
			/* Set with invalid info. Info should be reset by the backend. */
			info = 0x10000;
			guards_set(optval, optsize);
			status = sane_control_option (device, option_num, 
										  SANE_ACTION_SET_VALUE, optval, &info);
			guards_check(optval, optsize);
			if (SANE_OPTION_IS_SETTABLE (opt->cap) && SANE_OPTION_IS_ACTIVE (opt->cap)) {
				check(ERR, (status == SANE_STATUS_GOOD),
					  "cannot set option [%d, %s] value, although it is active and settable (%s)", option_num, opt->name, sane_strstatus(status));

				check(ERR, ((info & ~(SANE_INFO_INEXACT | 
									  SANE_INFO_RELOAD_OPTIONS | 
									  SANE_INFO_RELOAD_PARAMS)) == 0),
					  "sane_control_option set some wrong bit in info (%d)", info);

				if (info & SANE_INFO_RELOAD_PARAMS) {
					test_parameters(device, NULL);
				}
			} else {
				check(ERR, (status == SANE_STATUS_INVAL),
					  "was able to set option [%d, %s] value, although it is not active or settable", option_num, opt->name);
			}

			/* Ask the backend to set the option automatically. */
			guards_set(optval, optsize);
			status = sane_control_option (device, option_num, 
										  SANE_ACTION_SET_AUTO, optval, &info);
			guards_check(optval, optsize);
			if (SANE_OPTION_IS_SETTABLE (opt->cap) && 
				SANE_OPTION_IS_ACTIVE (opt->cap) &&
				(opt->cap & SANE_CAP_AUTOMATIC)) {
				check(ERR, (status == SANE_STATUS_GOOD),
					  "cannot set the option [%d, %s] automatically.", option_num, opt->name);
			} else {
				check(ERR, (status != SANE_STATUS_GOOD),
					  "was able to automatically set option [%d, %s], although it is not active or settable or automatically settable", option_num, opt->name);
			}
			if (info & SANE_INFO_RELOAD_PARAMS) {
				test_parameters(device, NULL);
			}
		}

		if (optval) {
			guards_free(optval);
			optval = NULL; 
		}

		/* Some capabilities checks. */
		check(ERR, ((opt->cap & (SANE_CAP_HARD_SELECT | SANE_CAP_SOFT_SELECT)) !=
					(SANE_CAP_HARD_SELECT | SANE_CAP_SOFT_SELECT)),
			  "option [%d, %s], SANE_CAP_HARD_SELECT and SANE_CAP_SOFT_SELECT are mutually exclusive", option_num, opt->name);
		if (opt->cap & SANE_CAP_SOFT_SELECT) {
			check(ERR, ((opt->cap & SANE_CAP_SOFT_DETECT) != 0),
				  "option [%d, %s], SANE_CAP_SOFT_DETECT must be set if SANE_CAP_SOFT_SELECT is set", option_num, opt->name);
		}
		if ((opt->cap & (SANE_CAP_SOFT_SELECT | 
						 SANE_CAP_HARD_SELECT | 
						 SANE_CAP_SOFT_DETECT)) == SANE_CAP_SOFT_DETECT) {
			check(ERR, (!SANE_OPTION_IS_SETTABLE (opt->cap)),
				  "option [%d, %s], must not be settable", option_num, opt->name);
		}
		
		if (!SANE_OPTION_IS_SETTABLE (opt->cap)) {
			/* Unsettable option. Ignore the rest of the test. */
			continue;
		}
	
		/* Check that will sane_control_option copy the string
		 * parameter and not just store a pointer to it. */
		if (opt->type == SANE_TYPE_STRING) {
			SANE_String val_string2;
			char *optstr;

			optstr = guards_malloc(opt->size);
			val_string2 = guards_malloc(opt->size);				

			/* Poison the current value. */
			strncpy(optstr, "-pOiSoN-", opt->size-1);
			optstr[opt->size-1] = 0;

			/* Get the value */
			guards_set(optstr, opt->size);
			status = sane_control_option (device, option_num, SANE_ACTION_GET_VALUE, 
										  optstr, NULL);
			guards_check(optstr, opt->size);
			check(FATAL, (status == SANE_STATUS_GOOD),
				  "cannot get option [%d, %s] value", option_num, opt->name);
			check(FATAL, (strcmp(optstr, "-pOiSoN-") != 0),
				  "sane_control_option did not set a value");

			/* Set the value */
			guards_set(optstr, opt->size);
			status = sane_control_option (device, option_num, 
										  SANE_ACTION_SET_VALUE, optstr, NULL);
			guards_check(optstr, opt->size);
			check(ERR, (status == SANE_STATUS_GOOD),
				  "cannot set option [%d, %s] value", option_num, opt->name);

			/* Poison the returned value. */
			strncpy(optstr, "-pOiSoN-", opt->size-1);
			optstr[opt->size-1] = 0;

			/* Read again the value and compare. */
			guards_set(val_string2, opt->size);			
			status = sane_control_option (device, option_num, SANE_ACTION_GET_VALUE, 
										  val_string2, NULL);
			guards_check(val_string2, opt->size);
			check(ERR, (status == SANE_STATUS_GOOD),
				  "cannot get option [%d, %s] value", option_num, opt->name);
			
			check(FATAL, (strcmp(optstr, val_string2) != 0),
				  "sane_control_option did not copy the string parameter for option [%d, %s]", option_num, opt->name);

			guards_free(optstr);
			guards_free(val_string2);
		}

		/* Try both boolean options. */
		if (opt->type == SANE_TYPE_BOOL) {
			SANE_Bool org_v;
			SANE_Bool v;
			
			status = sane_control_option (device, option_num, SANE_ACTION_GET_VALUE, 
										  &org_v, &info);
			check(ERR, (status == SANE_STATUS_GOOD),
				  "cannot get boolean option [%d, %s] value (%s)", option_num, opt->name, sane_strstatus(status));
			/* Invert the condition. */
			switch(org_v) {
			case SANE_FALSE:
				v = SANE_TRUE;
				break;
			case SANE_TRUE:
				v = SANE_FALSE;
				break;
			default:
				check(ERR, 0,
					  "invalid boolean value %d for option [%d, %s]", 
					  org_v, option_num, opt->name);
			}
			
			/* Set the opposite of the current value. */
			status = sane_control_option (device, option_num, 
										  SANE_ACTION_SET_VALUE, &v, &info);
			check(ERR, (status == SANE_STATUS_GOOD),
				  "cannot set boolean option [%d, %s] value (%s)", option_num, opt->name, sane_strstatus(status));
			check(ERR, (v != org_v),
				  "boolean values should be different");

			if (info & SANE_INFO_RELOAD_PARAMS) {
				test_parameters(device, NULL);
			}

			/* Set the initial value. */
			v = org_v;
			status = sane_control_option (device, option_num, 
										  SANE_ACTION_SET_VALUE, &v, &info);
			check(ERR, (status == SANE_STATUS_GOOD),
				  "cannot set boolean option [%d, %s] value (%s)", option_num, opt->name, sane_strstatus(status));
			check(ERR, (v == org_v),
				  "boolean values should be the same");

			if (info & SANE_INFO_RELOAD_PARAMS) {
				test_parameters(device, NULL);
			}			
		}
		
		/* Try to set an invalid option. */
		switch(opt->type) {
		case SANE_TYPE_BOOL: {
			SANE_Word v;	/* should be SANE_Bool instead */
				
			v = -1;				/* invalid value. must be SANE_FALSE or SANE_TRUE */
			status = sane_control_option (device, option_num, 
										  SANE_ACTION_SET_VALUE, &v, NULL);
			check(ERR, (status != SANE_STATUS_GOOD),
				  "was able to set an invalid value for boolean option [%d, %s]", option_num, opt->name);

			v = 2;				/* invalid value. must be SANE_FALSE or SANE_TRUE */
			status = sane_control_option (device, option_num, 
										  SANE_ACTION_SET_VALUE, &v, NULL);
			check(ERR, (status != SANE_STATUS_GOOD),
				  "was able to set an invalid value for boolean option [%d, %s]", option_num, opt->name);
		}
		break;

		case SANE_TYPE_FIXED:
		case SANE_TYPE_INT: {
			SANE_Int *v;
			unsigned int i;

			v = guards_malloc(opt->size);

			/* I can only think of a test for
			 * SANE_CONSTRAINT_RANGE. This tests the behaviour of
			 * sanei_constrain_value(). */
			if (opt->constraint_type == SANE_CONSTRAINT_RANGE) {
				for(i=0; i<opt->size / sizeof(SANE_Int); i++)
					v[i] = opt->constraint.range->min - 1;	/* invalid range */

				guards_set(v, opt->size);
				status = sane_control_option (device, option_num, 
											  SANE_ACTION_SET_VALUE, v, &info);
				guards_check(v, opt->size);
				check(ERR, (status == SANE_STATUS_GOOD && (info & SANE_INFO_INEXACT) ),
					  "incorrect return when setting an invalid range value for option [%d, %s] (status %s, info %x)", option_num, opt->name, sane_strstatus(status), info);

				/* Set the corrected value. */
				guards_set(v, opt->size);
				status = sane_control_option (device, option_num, 
											  SANE_ACTION_SET_VALUE, v, &info);
				guards_check(v, opt->size);
				check(ERR, (status == SANE_STATUS_GOOD && !(info & SANE_INFO_INEXACT) ),
					  "incorrect return when setting an invalid range value for option [%d, %s] (status %s, info %x)", option_num, opt->name, sane_strstatus(status), info);


				for(i=0; i<opt->size / sizeof(SANE_Int); i++)
					v[i] = opt->constraint.range->max + 1; /* invalid range */

				guards_set(v, opt->size);
				status = sane_control_option (device, option_num, 
											  SANE_ACTION_SET_VALUE, v, &info);
				guards_check(v, opt->size);
				check(ERR, (status == SANE_STATUS_GOOD && (info & SANE_INFO_INEXACT) ),
					  "incorrect return when setting an invalid range value for option [%d, %s] (status %s, info %x)", option_num, opt->name, sane_strstatus(status), info);

				/* Set the corrected value. */
				guards_set(v, opt->size);
				status = sane_control_option (device, option_num, 
											  SANE_ACTION_SET_VALUE, v, &info);
				guards_check(v, opt->size);
				check(ERR, (status == SANE_STATUS_GOOD && !(info & SANE_INFO_INEXACT) ),
					  "incorrect return when setting a valid range value for option [%d, %s] (status %s, info %x)", option_num, opt->name, sane_strstatus(status), info);
			}

			guards_free(v);
		}
		break;

		default:
			break;
		}
		
		/* TODO: button */
		
		/* 
		 * Here starts all the recursive stuff. After the test, it is
		 * possible that the value is not settable nor active
		 * anymore. 
		 */
		
		/* Try to set every option in a list */
		switch(opt->constraint_type) {
		case SANE_CONSTRAINT_WORD_LIST:
			check(FATAL, (opt->constraint.word_list != NULL),
				  "no constraint list for option [%d, %s]", option_num, opt->name);
			test_options_word_list (device, option_num, opt, can_do_recursive);
			break;

		case SANE_CONSTRAINT_STRING_LIST:
			check(FATAL, (opt->constraint.string_list != NULL),
				  "no constraint list for option [%d, %s]", option_num, opt->name);
			test_options_string_list (device, option_num, opt, can_do_recursive);
			break;
			
		case SANE_CONSTRAINT_RANGE:
			check(FATAL, (opt->constraint.range != NULL),
				  "no constraint range for option [%d, %s]", option_num, opt->name);
			check(FATAL, (opt->constraint.range->max >= opt->constraint.range->min),
				  "incorrect range for option [%d, %s] (min=%d > max=%d)", 
				  option_num, opt->name, opt->constraint.range->min, opt->constraint.range->max);
			/* Recurse. */
			if (can_do_recursive) {
				test_options(device, can_do_recursive-1);
			}
			break;

		case SANE_CONSTRAINT_NONE:
			check(INF, (opt->constraint.range == NULL),
				  "option [%d, %s] has some constraint value set", option_num, opt->name);

			/* Recurse. */
			if (can_do_recursive) {
				test_options(device, can_do_recursive-1);
			}
			break;
		}

		/* End of the test for that option. */
	}		

	/* test random non-existing options. */
	opt = sane_get_option_descriptor (device, -1);
	check(ERR, (opt == NULL),
		  "was able to get option descriptor for option -1");

	opt = sane_get_option_descriptor (device, num_dev_options+1);
	check(ERR, (opt == NULL),
		  "was able to get option descriptor for option %d", num_dev_options+1);

	opt = sane_get_option_descriptor (device, num_dev_options+2);
	check(ERR, (opt == NULL),
		  "was able to get option descriptor for option %d", num_dev_options+2);
	
	opt = sane_get_option_descriptor (device, num_dev_options+50);
	check(ERR, (opt == NULL),
		  "was able to get option descriptor for option %d", num_dev_options+50);
}

/* Get an option descriptor by the name of the option. */
static const SANE_Option_Descriptor *get_optdesc_by_name(SANE_Handle device, const char *name, int *option_num)
{
	const SANE_Option_Descriptor *opt;
	SANE_Int num_dev_options;
	SANE_Status status;	
	
	/* Get the number of options. */
	status = sane_control_option (device, 0, SANE_ACTION_GET_VALUE, &num_dev_options, 0);
	check(FATAL, (status == SANE_STATUS_GOOD),
		  "cannot get option 0 value (%s)", sane_strstatus(status));

	for (*option_num = 0; *option_num < num_dev_options; (*option_num)++) {

		/* Get the option descriptor */
		opt = sane_get_option_descriptor (device, *option_num);
		check(FATAL, (opt != NULL),
			  "cannot get option descriptor for option %d", *option_num);
	
		if (opt->name && strcmp(opt->name, name) == 0) {
			return(opt);
		}
	}
	return(NULL);
}

/* Set the first value for an option. That equates to the minimum for a
 * range or the first element in a list. */
static void set_min_value(SANE_Handle device, int option_num, 
						  const SANE_Option_Descriptor *opt)
{
	SANE_Status status;	
	SANE_String val_string;
	SANE_Int val_int;
	int rc;

	check(BUG, (SANE_OPTION_IS_SETTABLE(opt->cap)),
		  "option is not settable");

	switch(opt->constraint_type) {
	case SANE_CONSTRAINT_WORD_LIST:
		rc = check(ERR, (opt->constraint.word_list[0] > 0),
				   "no value in the list for option %s", opt->name);
		if (!rc) return;
		val_int = opt->constraint.word_list[1];
		status = sane_control_option (device, option_num,
									  SANE_ACTION_SET_VALUE, &val_int, NULL);
		check(ERR, (status == SANE_STATUS_GOOD),
			  "cannot set option %s to %d (%s)", opt->name, val_int, sane_strstatus(status));
		break;

	case SANE_CONSTRAINT_STRING_LIST:
		rc = check(ERR, (opt->constraint.string_list[0] != NULL),
				   "no value in the list for option %s", opt->name);
		if (!rc) return;
		val_string = strdup(opt->constraint.string_list[0]);
		assert(val_string);
		status = sane_control_option (device, option_num, 
									  SANE_ACTION_SET_VALUE, val_string, NULL);
		check(ERR, (status == SANE_STATUS_GOOD),
			  "cannot set option %s to [%s] (%s)", opt->name, val_string, sane_strstatus(status));
		free(val_string);
		break;

	case SANE_CONSTRAINT_RANGE:
		val_int = opt->constraint.range->min;
		status = sane_control_option (device, option_num,
									  SANE_ACTION_SET_VALUE, &val_int, NULL);
		check(ERR, (status == SANE_STATUS_GOOD),
			  "cannot set option %s to %d (%s)", opt->name, val_int, sane_strstatus(status));
		break;
		
	default:
		abort();
	}
}

/* Set the last value for an option. That equates to the maximum for a
 * range or the last element in a list. */
static void set_max_value(SANE_Handle device, int option_num, 
						  const SANE_Option_Descriptor *opt)
{
	SANE_Status status;	
	SANE_String val_string;
	SANE_Int val_int;
	int i;
	int rc;

	check(BUG, (SANE_OPTION_IS_SETTABLE(opt->cap)),
		  "option is not settable");

	switch(opt->constraint_type) {
	case SANE_CONSTRAINT_WORD_LIST:
		rc = check(ERR, (opt->constraint.word_list[0] > 0),
				   "no value in the list for option %s", opt->name);
		if (!rc) return;
		val_int = opt->constraint.word_list[opt->constraint.word_list[0]];
		status = sane_control_option (device, option_num,
									  SANE_ACTION_SET_VALUE, &val_int, NULL);
		check(ERR, (status == SANE_STATUS_GOOD),
			  "cannot set option %s to %d (%s)", opt->name, val_int, sane_strstatus(status));
		break;

	case SANE_CONSTRAINT_STRING_LIST:
		rc = check(ERR, (opt->constraint.string_list[0] != NULL),
				   "no value in the list for option %s", opt->name);
		if (!rc) return;
		for (i=1; opt->constraint.string_list[i] != NULL; i++);
		val_string = strdup(opt->constraint.string_list[i-1]);
		assert(val_string);
		status = sane_control_option (device, option_num, 
									  SANE_ACTION_SET_VALUE, val_string, NULL);
		check(ERR, (status == SANE_STATUS_GOOD),
			  "cannot set option %s to [%s] (%s)", opt->name, val_string, sane_strstatus(status));
		free(val_string);
		break;

	case SANE_CONSTRAINT_RANGE:
		val_int = opt->constraint.range->max;
		status = sane_control_option (device, option_num,
									  SANE_ACTION_SET_VALUE, &val_int, NULL);
		check(ERR, (status == SANE_STATUS_GOOD),
			  "cannot set option %s to %d (%s)", opt->name, val_int, sane_strstatus(status));
		break;
		
	default:
		abort();
	}
}

/* Set a random value for an option amongst the possible values. */
static void set_random_value(SANE_Handle device, int option_num, 
							 const SANE_Option_Descriptor *opt)
{
	SANE_Status status;	
	SANE_String val_string;
	SANE_Int val_int;
	int i;
	int rc;

	check(BUG, (SANE_OPTION_IS_SETTABLE(opt->cap)),
		  "option is not settable");

	switch(opt->constraint_type) {
	case SANE_CONSTRAINT_WORD_LIST:
		rc = check(ERR, (opt->constraint.word_list[0] > 0),
				   "no value in the list for option %s", opt->name);
		if (!rc) return;
		i=1+(rand() % opt->constraint.word_list[0]);
		val_int = opt->constraint.word_list[i];
		status = sane_control_option (device, option_num,
									  SANE_ACTION_SET_VALUE, &val_int, NULL);
		check(ERR, (status == SANE_STATUS_GOOD),
			  "cannot set option %s to %d (%s)", opt->name, val_int, sane_strstatus(status));
		break;

	case SANE_CONSTRAINT_STRING_LIST:
		rc = check(ERR, (opt->constraint.string_list[0] != NULL),
				   "no value in the list for option %s", opt->name);
		if (!rc) return;
		for (i=0; opt->constraint.string_list[i] != NULL; i++);
		i = rand() % i;
		val_string = strdup(opt->constraint.string_list[0]);
		assert(val_string);
		status = sane_control_option (device, option_num, 
									  SANE_ACTION_SET_VALUE, val_string, NULL);
		check(ERR, (status == SANE_STATUS_GOOD),
			  "cannot set option %s to [%s] (%s)", opt->name, val_string, sane_strstatus(status));
		free(val_string);
		break;

	case SANE_CONSTRAINT_RANGE:
		i = opt->constraint.range->max - opt->constraint.range->min;
		i = rand() % i;
		val_int = opt->constraint.range->min + i;
		status = sane_control_option (device, option_num,
									  SANE_ACTION_SET_VALUE, &val_int, NULL);
		check(ERR, (status == SANE_STATUS_GOOD),
			  "cannot set option %s to %d (%s)", opt->name, val_int, sane_strstatus(status));
		break;
		
	default:
		abort();
	}
}

/*--------------------------------------------------------------------------*/

/* Returns a string with the value of an option. */
static char *get_option_value(SANE_Handle device, const char *option_name)
{
	const SANE_Option_Descriptor *opt;
	void *optval;				/* value for the option */
	int optnum;
	static char str[100];
	SANE_Status status;	

	opt = get_optdesc_by_name(device, option_name, &optnum);
	if (opt) {
		
		optval = guards_malloc(opt->size);
		status = sane_control_option (device, optnum,
									  SANE_ACTION_GET_VALUE, optval, NULL);
		
		if (status == SANE_STATUS_GOOD) {
			switch(opt->type) {
				
			case SANE_TYPE_BOOL:
				if (*(SANE_Word*) optval == SANE_FALSE) {
					strcpy(str, "FALSE");
				} else {
					strcpy(str, "TRUE");
				}
				break;
			
			case SANE_TYPE_INT:
				sprintf(str, "%d", *(SANE_Word*) optval);
				break;
				
			case SANE_TYPE_FIXED: {
				int i;
				i = SANE_UNFIX(*(SANE_Word*) optval);
				sprintf(str, "%d", i);
			}
			break;
				
			case SANE_TYPE_STRING:
				strcpy(str, optval);
				break;
				
			default:
				str[0] = 0;
			}
		} else {
			/* Shouldn't happen. */
			strcpy(str, "backend default");
		}

		guards_free(optval);

	} else {
		/* The option does not exists. */
		strcpy(str, "backend default");
	}

	return(str);
}

/* Display the parameters that used for a scan. */
static char *display_scan_parameters(SANE_Handle device)
{
	static char str[150];
	char *p = str;

	*p = 0;
	
	p += sprintf(p, "scan mode=[%s] ", get_option_value(device, SANE_NAME_SCAN_MODE));
	p += sprintf(p, "resolution=[%s] ", get_option_value(device, SANE_NAME_SCAN_RESOLUTION));

	p += sprintf(p, "tl_x=[%s] ", get_option_value(device, SANE_NAME_SCAN_TL_X));
	p += sprintf(p, "tl_y=[%s] ", get_option_value(device, SANE_NAME_SCAN_TL_Y));
	p += sprintf(p, "br_x=[%s] ", get_option_value(device, SANE_NAME_SCAN_BR_X));
	p += sprintf(p, "br_y=[%s] ", get_option_value(device, SANE_NAME_SCAN_BR_Y));

	return(str);
}

/* Do a scan to test the correctness of the backend. */
static void test_scan(SANE_Handle device)
{
	const SANE_Option_Descriptor *opt;
	SANE_Status status;	
	int option_num;
	SANE_Int val_int;
	unsigned char *image = NULL;
	SANE_Parameters params;
	size_t to_read;
	SANE_Int len=0;
	int ask_len;
	int rc;
	int fd;

	/* Set the largest scan possible.  
	 *
	 * For that test, the corner
	 * position must exists and be SANE_CONSTRAINT_RANGE (this is not
	 * a SANE requirement though).
	 */
	opt = get_optdesc_by_name(device, SANE_NAME_SCAN_TL_X, &option_num);
	if (opt) set_min_value(device, option_num, opt);

	opt = get_optdesc_by_name(device, SANE_NAME_SCAN_TL_Y, &option_num);
	if (opt) set_min_value(device, option_num, opt);

	opt = get_optdesc_by_name(device, SANE_NAME_SCAN_BR_X, &option_num);
	if (opt) set_max_value(device, option_num, opt);

	opt = get_optdesc_by_name(device, SANE_NAME_SCAN_BR_Y, &option_num);
	if (opt) set_max_value(device, option_num, opt);

#define IMAGE_SIZE (512 * 1024)
	image = guards_malloc(IMAGE_SIZE);

	/* Try a read outside of a scan. */
	status = sane_read (device, image, len, &len);
	check(ERR, (status != SANE_STATUS_GOOD),
		  "it is possible to sane_read outside of a scan");

	/* Try to set the I/O mode outside of a scan. */
	status = sane_set_io_mode (device, SANE_FALSE);
	check(ERR, (status == SANE_STATUS_INVAL),
		  "it is possible to sane_set_io_mode outside of a scan");	
	status = sane_set_io_mode (device, SANE_TRUE);
	check(ERR, (status == SANE_STATUS_INVAL ||
				status == SANE_STATUS_UNSUPPORTED),
		  "it is possible to sane_set_io_mode outside of a scan");	

	/* Test sane_get_select_fd outside of a scan. */
	status = sane_get_select_fd(device, &fd);
	check(ERR, (status == SANE_STATUS_INVAL || 
				status == SANE_STATUS_UNSUPPORTED),
		  "sane_get_select_fd outside of a scan returned an invalid status (%s)", 
		  sane_strstatus (status));

	if (test_level > 2) {
		/* Do a scan, reading byte per byte */
		check(MSG, 0, "TEST: scan byte per byte - %s", display_scan_parameters(device));

		test_parameters(device, &params);
		status = sane_start (device);
		rc = check(ERR, (status == SANE_STATUS_GOOD),
				   "cannot start the scan (%s)", sane_strstatus (status));
		if (!rc) goto the_end;

		/* sane_set_io_mode with SANE_FALSE is always supported. */
		status = sane_set_io_mode (device, SANE_FALSE);
		check(ERR, (status == SANE_STATUS_GOOD),
			  "sane_set_io_mode with SANE_FALSE must return SANE_STATUS_GOOD");

		/* test sane_set_io_mode with SANE_TRUE. */	
		status = sane_set_io_mode (device, SANE_TRUE);
		check(ERR, (status == SANE_STATUS_GOOD || 
					status == SANE_STATUS_UNSUPPORTED),
			  "sane_set_io_mode with SANE_TRUE returned an invalid status (%s)", 
			  sane_strstatus (status));

		/* Put the backend back into blocking mode. */
		status = sane_set_io_mode (device, SANE_FALSE);
		check(ERR, (status == SANE_STATUS_GOOD),
			  "sane_set_io_mode with SANE_FALSE must return SANE_STATUS_GOOD");

		/* Test sane_get_select_fd */
		fd = 0x76575;				/* won't exists */
		status = sane_get_select_fd(device, &fd);
		check(ERR, (status == SANE_STATUS_GOOD || 
					status == SANE_STATUS_UNSUPPORTED),
			  "sane_get_select_fd returned an invalid status (%s)", 
			  sane_strstatus (status));
		if (status == SANE_STATUS_GOOD) {
			check(ERR, (fd != 0x76575),
				  "sane_get_select_fd didn't set the fd although it should have");
			check(ERR, (fd >= 0),
				  "sane_get_select_fd returned an invalid fd");
		}
	
		/* Check that it is not possible to set an option. It is probably
		 * a requirement stated indirectly in the section 4.4 on code
		 * flow.  
		 */
		status = sane_control_option (device, option_num, 
									  SANE_ACTION_SET_VALUE, 
									  &val_int , NULL);
		check(WRN, (status != SANE_STATUS_GOOD),
			  "it is possible to set a value during a scan");

		test_parameters(device, &params);

		if (params.bytes_per_line != 0 && params.lines != 0) {
		
			to_read = params.bytes_per_line * params.lines;
			while(SANE_TRUE) {
				len = 76457645;		/* garbage */
				guards_set(image, 1);
				status = sane_read (device, image, 1, &len);
				guards_check(image, 1);

				if (status == SANE_STATUS_EOF) {
					/* End of scan */
					check(ERR, (len == 0),
						  "the length returned is not 0");
					break;
				}

				rc = check(ERR, (status == SANE_STATUS_GOOD),
						   "scan stopped - status is %s", sane_strstatus (status));
				if (!rc) {
					check(ERR, (len == 0),
						  "the length returned is not 0");
					break;
				}

				/* The scanner can only return 1. If it returns 0, we may
				 * loop forever. */
				rc = check(ERR, (len == 1),
						   "backend returned 0 bytes - skipping test");
				if (!rc) {
					break;
				}

				to_read -= len;
			}

			if (params.lines != -1) {
				check(ERR, (to_read == 0),
					  "scan ended, but data was truncated");
			}
		}

		sane_cancel(device);
	}

	/* Try a read outside a scan. */
	ask_len = 1;
	guards_set(image, ask_len);
	status = sane_read (device, image, ask_len, &len);
	guards_check(image, ask_len);
	check(ERR, (status != SANE_STATUS_GOOD),
		  "it is possible to sane_read outside a scan");


	/* 
	 * Do a partial scan 
	 */
	check(MSG, 0, "TEST: partial scan - %s", display_scan_parameters(device));

	status = sane_start (device);
	rc = check(ERR, (status == SANE_STATUS_GOOD),
			   "cannot start the scan (%s)", sane_strstatus (status));
	if (!rc) goto the_end;

	test_parameters(device, &params);

	if (params.bytes_per_line != 0 && params.lines != 0) {
		
		len = 10;

		guards_set(image, 1);
		status = sane_read (device, image, 1, &len);
		guards_check(image, 1);

		check(ERR, (len == 1),
			  "sane_read() didn't return 1 byte as requested");
	}
		
	sane_cancel(device);


	/* 
	 * Do a scan, reading random length. 
	 */
	check(MSG, 0, "TEST: scan random length - %s", display_scan_parameters(device));

	test_parameters(device, &params);

	/* Try a read outside a scan. */
	ask_len = 20;
	guards_set(image, ask_len);
	status = sane_read (device, image, ask_len, &len);
	guards_check(image, ask_len);
	check(ERR, (status != SANE_STATUS_GOOD),
		  "it is possible to sane_read outside a scan");

	status = sane_start (device);
	rc = check(ERR, (status == SANE_STATUS_GOOD),
			   "cannot start the scan (%s)", sane_strstatus (status));
	if (!rc) goto the_end;

	/* Check that it is not possible to set an option. */
	status = sane_control_option (device, option_num, 
								  SANE_ACTION_SET_VALUE, 
								  &val_int , NULL);
	check(WRN, (status != SANE_STATUS_GOOD),
		  "it is possible to set a value during a scan");

	test_parameters(device, &params);

	if (params.bytes_per_line != 0 && params.lines != 0) {
		
		to_read = params.bytes_per_line * params.lines;
		srandom(time(NULL));
		
		while (SANE_TRUE) {

			ask_len = rand() & 0x7ffff;	/* 0 to 512K-1 */
			if (ask_len == 0) len = 1;
			len = ask_len + 4978; /* garbage */

			guards_set(image, ask_len);
			status = sane_read (device, image, ask_len, &len);
			guards_check(image, ask_len);

			if (status == SANE_STATUS_EOF) {
				/* End of scan */
				check(ERR, (len == 0),
					  "the length returned is not 0");
				break;
			}

			rc = check(ERR, (status == SANE_STATUS_GOOD),
					   "scan stopped - status is %s", sane_strstatus (status));
			if (!rc) {
				check(ERR, (len == 0),
					  "the length returned is not 0");
				break;
			}

			/* The scanner cannot return 0. If it returns 0, we may
			 * loop forever. */
			rc = check(ERR, (len > 0),
					   "backend didn't return any data - skipping test");
			if (!rc) {
				break;
			}
			rc = check(ERR, (len <= ask_len),
					   "backend returned too much data (%d / %d) - skipping test",
					   len, ask_len);
			if (!rc) {
				break;
			}
			
			to_read -= len;
		}

		if (params.lines != -1) {
			check(ERR, (to_read == 0),
				  "scan ended, but data was truncated");
		}
	}

	sane_cancel(device);

	/* Try a read outside a scan. */
	ask_len = 30;
	guards_set(image, ask_len);
	status = sane_read (device, image, ask_len, &len);
	guards_check(image, ask_len);
	check(ERR, (status != SANE_STATUS_GOOD),
		  "it is possible to sane_read outside a scan");

	/*
	 * Do a scan with a fixed size and a big buffer
	 */
	check(MSG, 0, "TEST: scan with a big max_len - %s", display_scan_parameters(device));

	test_parameters(device, &params);

	status = sane_start (device);
	rc = check(ERR, (status == SANE_STATUS_GOOD),
			   "cannot start the scan (%s)", sane_strstatus (status));
	if (!rc) goto the_end;

	test_parameters(device, &params);

	if (params.bytes_per_line != 0 && params.lines != 0) {
		
		to_read = params.bytes_per_line * params.lines;
		while(SANE_TRUE) {
			ask_len = IMAGE_SIZE;
			len = rand();		/* garbage */

			guards_set(image, ask_len);
			status = sane_read (device, image, ask_len, &len);
			guards_check(image, ask_len);

			if (status == SANE_STATUS_EOF) {
				/* End of scan */
				check(ERR, (len == 0),
					  "the length returned is not 0");
				break;
			}

			rc = check(ERR, (status == SANE_STATUS_GOOD),
					   "scan stopped - status is %s", sane_strstatus (status));
			if (!rc) {
				check(ERR, (len == 0),
					  "the length returned is not 0");
				break;
			}

			/* If the scanner return 0, we may loop forever. */
			rc = check(ERR, (len > 0),
					   "backend didn't return any data - skipping test");
			if (!rc) {
				break;
			}

			rc = check(ERR, (len <= ask_len),
					   "backend returned too much data (%d / %d) - skipping test",
					   len, ask_len);
			if (!rc) {
				break;
			}

			to_read -= len;
		}

		if (params.lines != -1) {
			check(ERR, (to_read == 0),
				  "scan ended, but data was truncated");
		}
	}

	sane_cancel(device);

 the_end:
	if (image) guards_free(image);
}

/* Do several scans at different scan mode and resolution. */
static void test_scans(SANE_Device * device)
{
	const SANE_Option_Descriptor *scan_mode_opt;
	const SANE_Option_Descriptor *resolution_mode_opt;
	SANE_Status status;	
	int scan_mode_optnum;
	int resolution_mode_optnum;
	SANE_String val_string;
	int i;
	int rc;

	/* For that test, the requirements are:
	 *   SANE_NAME_SCAN_MODE exists and is a SANE_CONSTRAINT_STRING_LIST
	 *   SANE_NAME_SCAN_RESOLUTION exists and is either a SANE_CONSTRAINT_WORD_LIST or a SANE_CONSTRAINT_RANGE.
	 *
	 * These are not a SANE requirement, though.
	 */

	scan_mode_opt = get_optdesc_by_name(device, SANE_NAME_SCAN_MODE, &scan_mode_optnum);
	if (scan_mode_opt) {

		rc = check(INF, (scan_mode_opt->type == SANE_TYPE_STRING),
				   "option [%s] is not a SANE_TYPE_STRING - skipping test", SANE_NAME_SCAN_MODE);
		if (!rc) return;
		rc = check(INF, (scan_mode_opt->constraint_type == SANE_CONSTRAINT_STRING_LIST),
				   "constraint for option [%s] is not SANE_CONSTRAINT_STRING_LIST - skipping test", SANE_NAME_SCAN_MODE);
		if (!rc) return;
		rc = check(INF, (SANE_OPTION_IS_SETTABLE(scan_mode_opt->cap)),
				   "option [%s] is not settable - skipping test", SANE_NAME_SCAN_MODE);
		if (!rc) return;
	}

	resolution_mode_opt = get_optdesc_by_name(device, SANE_NAME_SCAN_RESOLUTION, &resolution_mode_optnum);
	if (resolution_mode_opt) {
		rc = check(INF, (SANE_OPTION_IS_SETTABLE(resolution_mode_opt->cap)),
				   "option [%s] is not settable - skipping test", SANE_NAME_SCAN_RESOLUTION);
		if (!rc) return;
	}

	if (scan_mode_opt) {
		/* Do several scans, with several resolution. */
		for (i=0; scan_mode_opt->constraint.string_list[i] != NULL; i++) {
		
			val_string = strdup(scan_mode_opt->constraint.string_list[i]);
			assert(val_string);
		
			status = sane_control_option (device, scan_mode_optnum, 
										  SANE_ACTION_SET_VALUE, val_string, NULL);
			check(FATAL, (status == SANE_STATUS_GOOD),
				  "cannot set a settable option (status=%s)", sane_strstatus(status));
		
			free(val_string);

			if (resolution_mode_opt) {
				set_min_value(device, resolution_mode_optnum, 
							  resolution_mode_opt);			
				test_scan(device);

				set_max_value(device, resolution_mode_optnum, 
							  resolution_mode_opt);
				test_scan(device);

				set_random_value(device, resolution_mode_optnum, 
								 resolution_mode_opt);
				test_scan(device);
			} else {
				test_scan(device);
			}
		}
	} else {
		if (resolution_mode_opt) {
			set_min_value(device, resolution_mode_optnum, 
						  resolution_mode_opt);			
			test_scan(device);

			set_max_value(device, resolution_mode_optnum, 
						  resolution_mode_opt);
			test_scan(device);

			set_random_value(device, resolution_mode_optnum, 
							 resolution_mode_opt);
			test_scan(device);
		} else {
			test_scan(device);
		}
	}
}

/** test sane_get_devices
 * test sane_get_device function, if time is greter than 0,
 * loop to let tester plug/unplug device to check for correct
 * hotplug detection
 * @param device_list device list to fill
 * @param time time to loop
 * @return 0 on success
 */
static int test_get_devices(const SANE_Device ***device_list, int time)
{
int loop=0;
int i;
const SANE_Device *dev;
SANE_Status status;

	status = sane_get_devices (device_list, SANE_TRUE);
	check(FATAL, (status == SANE_STATUS_GOOD),
		  "sane_get_devices() failed (%s)", sane_strstatus (status));

	/* Verify that the SANE doc (or tstbackend) is up to date */
	for (i=0; (*device_list)[i] != NULL; i++) {

		dev = (*device_list)[i];

		check(FATAL, (dev->name != NULL), "device name is NULL");
		check(FATAL, (dev->vendor != NULL), "device vendor is NULL");
		check(FATAL, (dev->type != NULL), "device type is NULL");
		check(FATAL, (dev->model != NULL), "device model is NULL");
		
		check(INF, ((strcmp(dev->type, "flatbed scanner") == 0) ||
					(strcmp(dev->type, "frame grabber") == 0) ||
					(strcmp(dev->type, "handheld scanner") == 0) ||
					(strcmp(dev->type, "still camera") == 0) ||
					(strcmp(dev->type, "video camera") == 0) ||
					(strcmp(dev->type, "virtual device") == 0) ||
					(strcmp(dev->type, "film scanner") == 0) ||
					(strcmp(dev->type, "multi-function peripheral") == 0) ||
					(strcmp(dev->type, "sheetfed scanner") == 0)),
					"unknown device type [%s]. Update SANE doc section \"Type Strings\"", dev->type);

		check(INF, (
					(strcmp(dev->vendor, "AGFA") == 0) ||
					(strcmp(dev->vendor, "Abaton") == 0) ||
					(strcmp(dev->vendor, "Acer") == 0) ||
					(strcmp(dev->vendor, "Apple") == 0) ||
					(strcmp(dev->vendor, "Artec") == 0) ||
					(strcmp(dev->vendor, "Avision") == 0) ||
					(strcmp(dev->vendor, "CANON") == 0) ||
					(strcmp(dev->vendor, "Connectix") == 0) ||
					(strcmp(dev->vendor, "Epson") == 0) ||
					(strcmp(dev->vendor, "Fujitsu") == 0) ||
					(strcmp(dev->vendor, "Gphoto2") == 0) ||
					(strcmp(dev->vendor, "Hewlett-Packard") == 0) ||
					(strcmp(dev->vendor, "IBM") == 0) ||
					(strcmp(dev->vendor, "Kodak") == 0) ||
                                        (strcmp(dev->vendor, "Lexmark") == 0) ||
					(strcmp(dev->vendor, "Logitech") == 0) ||
					(strcmp(dev->vendor, "Microtek") == 0) ||
					(strcmp(dev->vendor, "Minolta") == 0) ||
					(strcmp(dev->vendor, "Mitsubishi") == 0) ||
					(strcmp(dev->vendor, "Mustek") == 0) ||
					(strcmp(dev->vendor, "NEC") == 0) ||
					(strcmp(dev->vendor, "Nikon") == 0) ||
					(strcmp(dev->vendor, "Noname") == 0) ||
					(strcmp(dev->vendor, "Plustek") == 0) ||
					(strcmp(dev->vendor, "Polaroid") == 0) ||
					(strcmp(dev->vendor, "Relisys") == 0) ||
					(strcmp(dev->vendor, "Ricoh") == 0) ||
					(strcmp(dev->vendor, "Sharp") == 0) ||
					(strcmp(dev->vendor, "Siemens") == 0) ||
					(strcmp(dev->vendor, "Tamarack") == 0) ||
					(strcmp(dev->vendor, "UMAX") == 0)),
			  "unknown device vendor [%s]. Update SANE doc section \"Vendor Strings\"", dev->vendor);
	}

	/* loop on detecting device to let time to plug/unplug scanners */
	while(loop<time) {
		/* print and free detected device list */
		check(MSG, 0, "DETECTED DEVICES:");
		for (i=0; (*device_list)[i] != NULL; i++) {
			dev = (*device_list)[i];
			check(MSG, 0, "\t%s:%s %s:%s", dev->vendor, dev->name, dev->type, dev->model);
		}
		if(i==0) {
			check(MSG, 0, "\tnone...");
		}
		sleep(1);
		(*device_list) = NULL;
		status = sane_get_devices (device_list, SANE_TRUE);
		check(FATAL, (status == SANE_STATUS_GOOD),
		  "sane_get_devices() failed (%s)", sane_strstatus (status));
		loop++;
	}
	return 0;
}

/** test test_default
 * test by scanning using default values
 * @param device device to use for the scan
 */
static void test_default(SANE_Device * device)
{
	test_scan(device);
}

static void usage(const char *execname)
{
	printf("Usage: %s [-d backend_name] [-l test_level] [-s] [-r recursion_level] [-g time (s)]\n", execname);
	printf("\t-v\tverbose level\n");
	printf("\t-d\tbackend name\n");
	printf("\t-l\tlevel of testing (0=some, 1=0+options, 2=1+scans, 3=longest tests)\n");
	printf("\t-s\tdo a scan during open/close tests\n");
	printf("\t-r\trecursion level for option testing (the higher, the longer)\n");
	printf("\t-g\ttime to loop on sane_get_devices function to test scannet hotplug detection (time is in seconds).\n");
}

int
main (int argc, char **argv)
{
	char *devname = NULL;
	SANE_Status status;
	SANE_Int version_code;
	SANE_Handle device;
	int ch;
	int index;
	int i;
	const SANE_Device **device_list;
	int rc;
	int recursion_level;
	int time;
	int default_scan;

	printf("tstbackend, Copyright (C) 2002 Frank Zago\n");
	printf("tstbackend comes with ABSOLUTELY NO WARRANTY\n");
	printf("This is free software, and you are welcome to redistribute it\n");
	printf("under certain conditions. See COPYING file for details\n\n");
	printf("This is tstbackend build %d\n\n", BUILD);

	/* Read the command line options. */
	opterr = 0;
	recursion_level = 5;		/* 5 levels or recursion should be enough */
	test_level = 0;			/* basic tests only */
	time = 0;			/* no get devices loop */
	default_scan = 0;

	while ((ch = getopt_long (argc, argv, "-v:d:l:r:g:h:s", basic_options,
							  &index)) != EOF) {
		switch(ch) {
		case 'v':
			verbose_level = atoi(optarg);
			break;

		case 'd':
			devname = strdup(optarg);
			break;

		case 'l':
			test_level = atoi(optarg);
			if (test_level < 0 || test_level > 4) {
				fprintf(stderr, "invalid test_level\n");
				return(1);
			}
			break;

		case 's':
			default_scan = 1;
			break;

		case 'r':
			recursion_level = atoi(optarg);
			break;

		case 'g':
			time = atoi(optarg);
			break;

		case 'h':
			usage(argv[0]);
			return(0);

		case '?':
			fprintf(stderr, "invalid option\n");
			return(1);

		default:
			fprintf(stderr, "bug in tstbackend\n");
			return(1);
		}
	}

	/* First test */
	check(MSG, 0, "TEST: init/exit");
	for (i=0; i<10; i++) {
		/* Test 1. init/exit with a version code */
		status = sane_init(&version_code, NULL);
		check(FATAL, (status == SANE_STATUS_GOOD),
			  "sane_init failed with %s", sane_strstatus (status));
		check(FATAL, (SANE_VERSION_MAJOR(version_code) == 1),
			  "invalid SANE version linked");
		sane_exit();

		/* Test 2. init/exit without a version code */
		status = sane_init(NULL, NULL);
		check(FATAL, (status == SANE_STATUS_GOOD),
			  "sane_init failed with %s", sane_strstatus (status));
		sane_exit();

		/* Test 3. Init/get_devices/open invalid/exit */
		status = sane_init(NULL, NULL);
		check(FATAL, (status == SANE_STATUS_GOOD),
			  "sane_init failed with %s", sane_strstatus (status));

		status = sane_get_devices (&device_list, SANE_TRUE);
		check(FATAL, (status == SANE_STATUS_GOOD),
			  "sane_get_devices() failed (%s)", sane_strstatus (status));

		status = sane_open ("opihndvses75bvt6fg", &device);
		check(WRN, (status == SANE_STATUS_INVAL),
			  "sane_open() failed (%s)", sane_strstatus (status));
		
		if (status == SANE_STATUS_GOOD)
			sane_close(device);

		sane_exit();

		/* Test 4. Init/get_devices/open default/exit */
		status = sane_init(NULL, NULL);
		check(FATAL, (status == SANE_STATUS_GOOD),
			  "sane_init failed with %s", sane_strstatus (status));

		status = sane_get_devices (&device_list, SANE_TRUE);
		check(FATAL, (status == SANE_STATUS_GOOD),
			  "sane_get_devices() failed (%s)", sane_strstatus (status));

		status = sane_open ("", &device);
		if (status == SANE_STATUS_GOOD)
			sane_close(device);

		sane_exit();
	}

	status = sane_init (&version_code, NULL);
	check(FATAL, (status == SANE_STATUS_GOOD),
		  "sane_init failed with %s", sane_strstatus (status));
  
	/* Check the device list */
	rc = test_get_devices(&device_list, time);
	if (rc) goto the_exit;

	if (!devname) {
		/* If no device name was specified explicitly, we look at the
		   environment variable SANE_DEFAULT_DEVICE.  If this variable
		   is not set, we open the first device we find (if any): */
		devname = getenv ("SANE_DEFAULT_DEVICE");
		if (devname) devname = strdup(devname);
	}

	if (!devname) {
		if (device_list[0]) {
			devname = strdup(device_list[0]->name);
		}
	}

	rc = check(ERR, (devname != NULL),
			   "no SANE devices found");
	if (!rc) goto the_exit;

	check(MSG, 0, "using device %s", devname);
	
	/* Test open close */
	check(MSG, 0, "TEST: open/close");
	for (i=0; i<10; i++) {
		status = sane_open (devname, &device);
		rc = check(ERR, (status == SANE_STATUS_GOOD),
				   "sane_open failed with %s for device %s", sane_strstatus (status), devname);
		if (!rc) goto the_exit;

		if (default_scan) {
			test_default (device);
		}
		sane_close (device);
	}

	if (test_level < 1) {
		sane_exit();
		goto the_exit;
	}


	/* Test options */
	check(MSG, 0, "TEST: options consistency");
	status = sane_open (devname, &device);
	check(FATAL, (status == SANE_STATUS_GOOD),
		  "sane_open failed with %s for device %s", sane_strstatus (status), devname);

	test_parameters(device, NULL);
	test_options(device, recursion_level);
	sane_close (device);
	sane_exit();

	if (test_level < 2) {
		goto the_exit;
	}


	/* Test scans */
	check(MSG, 0, "TEST: scan test");
	status = sane_init (&version_code, NULL);
	check(FATAL, (status == SANE_STATUS_GOOD),
		  "sane_init failed with %s", sane_strstatus (status));
	status = sane_open (devname, &device);
	check(FATAL, (status == SANE_STATUS_GOOD),
		  "sane_open failed with %s for device %s", sane_strstatus (status), devname);
	test_scans(device);
	sane_close (device);
	sane_exit();

 the_exit:
	if (devname) free(devname);
	display_stats();
	return(0);
}



