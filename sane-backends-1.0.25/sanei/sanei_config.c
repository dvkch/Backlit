/* sane - Scanner Access Now Easy.
   Copyright (C) 1997 Jeffrey S. Freedman
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

   This file provides generic configuration support.  */

#include "../include/sane/config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/param.h>

#include "../include/sane/sanei.h"
#include "../include/sane/sanei_config.h"

#define BACKEND_NAME	sanei_config
#include "../include/sane/sanei_debug.h"

#ifndef PATH_MAX
# define PATH_MAX	1024
#endif

#if defined(_WIN32) || defined(HAVE_OS2_H)
# define DIR_SEP	";"
# define PATH_SEP	'\\'
#else
# define DIR_SEP	":"
# define PATH_SEP	'/'
#endif

#define DEFAULT_DIRS	"." DIR_SEP STRINGIFY(PATH_SANE_CONFIG_DIR)

#ifdef __BEOS__
#include <FindDirectory.h>
#endif

static char *dir_list;

const char *
sanei_config_get_paths ()
{
#ifdef __BEOS__
  char result[PATH_MAX];
#endif
  void *mem;
  char *dlist;
  size_t len;

  if (!dir_list)
    {
      DBG_INIT();

      dlist = getenv ("SANE_CONFIG_DIR");
      if (dlist)
	dir_list = strdup (dlist);
#ifdef __BEOS__
      /* ~/config/settings/SANE takes precedence over /etc/sane.d/ */
      if (!dir_list)
	{
	  if (find_directory(B_USER_SETTINGS_DIRECTORY, 0, true, result, PATH_MAX) == B_OK)
	    {
	      strcat(result,"/SANE");
	      strcat(result,DIR_SEP); /* do append the default ones */
	      dir_list = strdup (result);
	    }
	}
#endif
      if (dir_list)
	{
	  len = strlen (dir_list);
	  if ((len > 0) && (dir_list[len - 1] == DIR_SEP[0]))
	    {
	      /* append default search directories: */
	      mem = malloc (len + sizeof (DEFAULT_DIRS));
	      memcpy (mem, dir_list, len);
	      memcpy ((char *) mem + len, DEFAULT_DIRS, sizeof (DEFAULT_DIRS));
	      free (dir_list);
	      dir_list = mem;
	    }
	}
      else
	{
	  /* Create a copy, since we might call free on it */
	  dir_list = strdup (DEFAULT_DIRS);
	}
    }
  DBG (5, "sanei_config_get_paths: using config directories  %s\n", dir_list);

  return dir_list;
}

FILE *
sanei_config_open (const char *filename)
{
  char *next, *dir, result[PATH_MAX];
  const char *cfg_dir_list;
  FILE *fp;
  char *copy;

  cfg_dir_list = sanei_config_get_paths ();
  if (!cfg_dir_list)
    {
      DBG(2, "sanei_config_open: could not find config file `%s'\n", filename);
      return NULL;
    }

  copy = strdup (cfg_dir_list);

  for (next = copy; (dir = strsep (&next, DIR_SEP)) != 0; )
    {
      snprintf (result, sizeof (result), "%s%c%s", dir, PATH_SEP, filename);
      DBG(4, "sanei_config_open: attempting to open `%s'\n", result);
      fp = fopen (result, "r");
      if (fp)
	{
	  DBG(3, "sanei_config_open: using file `%s'\n", result);
	  break;
	}
    }
  free (copy);

  if (!fp)
    DBG(2, "sanei_config_open: could not find config file `%s'\n", filename);

  return fp;
}

const char *
sanei_config_skip_whitespace (const char *str)
{
  while (str && *str && isspace (*str))
    ++str;
  return str;
}

const char *
sanei_config_get_string (const char *str, char **string_const)
{
  const char *start;
  size_t len;

  str = sanei_config_skip_whitespace (str);

  if (*str == '"')
    {
      start = ++str;
      while (*str && *str != '"')
	++str;
      len = str - start;
      if (*str == '"')
	++str;
      else
	start = 0;		/* final double quote is missing */
    }
  else
    {
      start = str;
      while (*str && !isspace (*str))
	++str;
      len = str - start;
    }
  if (start)
    *string_const = strndup (start, len);
  else
    *string_const = 0;
  return str;
}

char *
sanei_config_read (char *str, int n, FILE *stream)
{
   char* rc;
   char* start;
   int   len;

      /* read line from stream */
   rc = fgets( str, n, stream);
   if (rc == NULL)
      return NULL;

      /* remove ending whitespaces */
   len = strlen( str);
   while( (0 < len) && (isspace( str[--len])) )
      str[len] = '\0';

      /* remove starting whitespaces */
   start = str;
   while( isspace( *start))
      start++;

   if (start != str) 
      do {
         *str++ = *start++;
      } while( *str);

   return rc;
}


SANE_Status
sanei_configure_attach (const char *config_file, SANEI_Config * config,
			SANE_Status (*attach) (SANEI_Config * config,
					       const char *devname))
{
  SANE_Char line[PATH_MAX];
  SANE_Char *token, *string;
  SANE_Int len;
  const char *lp, *lp2;
  FILE *fp;
  SANE_Status status = SANE_STATUS_GOOD;
  int i, j, count;
  void *value = NULL;
  int size=0;
  SANE_Bool found;
  SANE_Word *wa;
  SANE_Bool *ba;

  DBG (3, "sanei_configure_attach: start\n");

  /* open configuration file */
  fp = sanei_config_open (config_file);
  if (!fp)
    {
      DBG (2, "sanei_configure_attach: couldn't access %s\n", config_file);
      DBG (3, "sanei_configure_attach: exit\n");
      return SANE_STATUS_ACCESS_DENIED;
    }

  /* loop reading the configuration file, all line beginning by "option " are
   * parsed for value to store in configuration structure, other line are 
   * used are device to try to attach
   */
  while (sanei_config_read (line, PATH_MAX, fp) && status == SANE_STATUS_GOOD)
    {
      /* skip white spaces at beginning of line */
      lp = sanei_config_skip_whitespace (line);

      /* skip empty lines */
      if (*lp == 0)
	continue;

      /* skip comment line */
      if (line[0] == '#')
	continue;

      len = strlen (line);

      /* delete newline characters at end */
      if (line[len - 1] == '\n')
	line[--len] = '\0';

      lp2 = lp;

      /* to ensure maximum compatibility, we accept line like:
       * option "option_name" "option_value"
       * "option_name" "option_value" 
       * So we parse the line 2 time to find an option */
      /* check if it is an option */
      lp = sanei_config_get_string (lp, &token);
      if (strncmp (token, "option", 6) == 0)
	{
	  /* skip the "option" token */
	  free (token);
	  lp = sanei_config_get_string (lp, &token);
	}

      /* search for a matching descriptor */
      i = 0;
      found = SANE_FALSE;
      while (config!=NULL && i < config->count && !found)
	{
	  if (strcmp (config->descriptors[i]->name, token) == 0)
	    {
	      found = SANE_TRUE;
	      switch (config->descriptors[i]->type)
		{
		case SANE_TYPE_INT:
		  size=config->descriptors[i]->size;
		  value = malloc (size);
		  wa = (SANE_Word *) value;
		  count = config->descriptors[i]->size / sizeof (SANE_Word);
		  for (j = 0; j < count; j++)
		    {
		      lp = sanei_config_get_string (lp, &string);
		      if (string == NULL)
			{
			  DBG (2,
			       "sanei_configure_attach: couldn't find a string to parse");
			  return SANE_STATUS_INVAL;
			}
		      wa[j] = strtol (string, NULL, 0);
		      free (string);
		    }
		  break;
		case SANE_TYPE_BOOL:
		  size=config->descriptors[i]->size;
		  value = malloc (size);
		  ba = (SANE_Bool *) value;
		  count = config->descriptors[i]->size / sizeof (SANE_Bool);
		  for (j = 0; j < count; j++)
		    {
		      lp = sanei_config_get_string (lp, &string);
		      if (string == NULL)
			{
			  DBG (2,
			       "sanei_configure_attach: couldn't find a string to parse");
			  return SANE_STATUS_INVAL;
			}
		      if ((strcmp (string, "1") == 0)
			  || (strcmp (string, "true") == 0))
			{
			  ba[j] = SANE_TRUE;
			}
		      else
			{
			  if ((strcmp (string, "0") == 0)
			      || (strcmp (string, "false") == 0))
			    ba[j] = SANE_FALSE;
			  else
			    {
			      DBG (2,
				   "sanei_configure_attach: couldn't find a valid boolean value");
			      return SANE_STATUS_INVAL;
			    }
			}
		      free (string);
		    }
		  break;
		case SANE_TYPE_FIXED:
		  size=config->descriptors[i]->size;
		  value = malloc (size);
		  wa = (SANE_Word *) value;
		  count = config->descriptors[i]->size / sizeof (SANE_Word);
		  for (j = 0; j < count; j++)
		    {
		      lp = sanei_config_get_string (lp, &string);
		      if (string == NULL)
			{
			  DBG (2,
			       "sanei_configure_attach: couldn't find a string to parse");
			  return SANE_STATUS_INVAL;
			}
		      wa[j] = SANE_FIX(strtod (string, NULL));
		      free (string);
		    }
		  break;
		case SANE_TYPE_STRING:
		  sanei_config_get_string (lp, &string);
		  if (string == NULL)
		    {
		      DBG (2,
			   "sanei_configure_attach: couldn't find a string value to parse");
		      return SANE_STATUS_INVAL;
		    }
		  value = string;
		  size=strlen(string)+1;
		  if(size>config->descriptors[i]->size)
		  {
			  size=config->descriptors[i]->size-1;
			  string[size]=0;
		  }
		  break;
		default:
		  DBG (1,
		       "sanei_configure_attach: incorrect type %d for option %s, skipping option ...\n",
		       config->descriptors[i]->type,
		       config->descriptors[i]->name);
		}
	      
	      /* check decoded value */
	      status = sanei_check_value (config->descriptors[i], value);

	      /* if value OK, copy it in configuration struct */
	      if (status == SANE_STATUS_GOOD)
		{
		  memcpy (config->values[i], value, size);
		}
	      if (value != NULL)
		{
		  free (value);
		  value = NULL;
		}
	    }
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (1,
		   "sanei_configure_attach: failed to parse option '%s', line '%s'\n",
		   token, line);
	    }
	  i++;
	}
      free (token);

      /* not detected as an option, so we call the attach function
       * with it */
      if (!found && status == SANE_STATUS_GOOD)
	{
	  /* if not an option, try to attach */
	  /* to avoid every backend to depend on scsi and usb functions
	   * we call back the backend for attach. In turn it will call
	   * sanei_usb_attach_matching_devices, sanei_config_attach_matching_devices
	   * or other. This means 2 callback functions per backend using this 
	   * function. */
	  DBG (3, "sanei_configure_attach: trying to attach with '%s'\n",
	       lp2);
	  if(attach!=NULL)
	  	attach (config, lp2);
	}
    }

  fclose (fp);
  DBG (3, "sanei_configure_attach: exit\n");
  return status;
}
