/*
   sane-desc.c -- generate list of supported SANE devices

   Copyright (C) 2002-2006 Henning Meier-Geinitz <henning@meier-geinitz.de>
   Copyright (C) 2004 Jose Gato <jgato@gsyc.escet.urjc.es> (XML output)
   Copyright (C) 2006 Mattias Ellert <mattias.ellert@tsl.uu.se> (plist output)
   Copyright (C) 2009 Dr. Ing. Dieter Jurzitza <dieter.jurzitza@t-online.de>
   Copyright (C) 2013 Tom Gundersen <teg@jklm.no> (hwdb output)

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

#include <../include/sane/config.h>

#include "lgetopt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>
#include <ctype.h>
#include <time.h>

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/sanei_config.h"

#define SANE_DESC_VERSION "3.5"

#define MAN_PAGE_LINK "http://www.sane-project.org/man/%s.5.html"
#define COLOR_MINIMAL      "\"#B00000\""
#define COLOR_BASIC        "\"#FF9000\""
#define COLOR_GOOD         "\"#90B000\""
#define COLOR_COMPLETE     "\"#007000\""
#define COLOR_UNTESTED     "\"#0000B0\""
#define COLOR_UNSUPPORTED  "\"#F00000\""
#define COLOR_NEW          "\"#F00000\""
#define COLOR_UNKNOWN      "\"#000000\""

#define DEVMODE  "0664"
#define DEVOWNER "root"
#define DEVGROUP "scanner"

#ifndef PATH_MAX
# define PATH_MAX 1024
#endif

#define DBG_ERR current_debug_level = 0; debug_call
#define DBG_WARN current_debug_level = 1; debug_call
#define DBG_INFO current_debug_level = 2; debug_call
#define DBG_DBG current_debug_level = 3; debug_call

typedef enum output_mode
{
  output_mode_ascii = 0,
  output_mode_xml,
  output_mode_html_backends,
  output_mode_html_backends_split,
  output_mode_html_mfgs,
  output_mode_statistics,
  output_mode_usermap,
  output_mode_db,
  output_mode_udev,
  output_mode_udevacl,
  output_mode_udevhwdb,
  output_mode_hwdb,
  output_mode_plist,
  output_mode_hal,
  output_mode_halnew
}
output_mode;

typedef enum parameter_type
{
  param_none = 0,
  param_string,
  param_two_strings,
  param_three_strings
}
parameter_type;

typedef enum status_entry
{
  status_unknown,
  status_unsupported,
  status_untested,
  status_minimal,
  status_basic,
  status_good,
  status_complete
}
status_entry;

typedef enum device_type
{
  type_unknown,
  type_scanner,
  type_stillcam,
  type_vidcam,
  type_meta,
  type_api
}
device_type;

typedef enum level
{
  level_backend,
  level_mfg,
  level_model,
  level_desc
}
level;

typedef struct url_entry
{
  struct url_entry *next;
  char *name;
}
url_entry;

typedef struct model_entry
{
  struct model_entry *next;
  char *name;
  char *interface;
  struct url_entry *url;
  char *comment;
  enum status_entry status;
  char *usb_vendor_id;
  char *usb_product_id;
  SANE_Bool ignore_usb_id;
  char *scsi_vendor_id;
  char *scsi_product_id;
  SANE_Bool scsi_is_processor;
}
model_entry;

typedef struct desc_entry
{
  struct desc_entry *next;
  char *desc;
  struct url_entry *url;
  char *comment;
}
desc_entry;

typedef struct mfg_entry
{
  struct mfg_entry *next;
  char *name;
  struct url_entry *url;
  char *comment;
  struct model_entry *model;
}
mfg_entry;

typedef struct type_entry
{
  struct type_entry *next;
  enum device_type type;
  struct desc_entry *desc;
  struct mfg_entry *mfg;
}
type_entry;

typedef struct backend_entry
{
  struct backend_entry *next;
  char *name;
  char *version;
  char *manpage;
  struct url_entry *url;
  char *comment;
  struct type_entry *type;
  SANE_Bool new;
}
backend_entry;

typedef struct model_record_entry
{
  struct model_record_entry *next;
  char *name;
  char *interface;
  struct url_entry *url;
  char *comment;
  enum status_entry status;
  char *usb_vendor_id;
  char *usb_product_id;
  char *scsi_vendor_id;
  char *scsi_product_id;
  SANE_Bool scsi_is_processor;
  struct backend_entry *be;
}
model_record_entry;

typedef struct mfg_record_entry
{
  struct mfg_record_entry *next;
  char *name;
  char *comment;
  struct url_entry *url;
  struct model_record_entry *model_record;
}
mfg_record_entry;

typedef int  statistics_type [status_complete + 1];


typedef struct manufacturer_model_type
{
  struct manufacturer_model_type * next;
  char *name;
}
manufacturer_model_type;

typedef struct usbid_type
{
  struct usbid_type * next;
  char *usb_vendor_id;
  char *usb_product_id;
  struct manufacturer_model_type *name;
}
usbid_type;

typedef struct scsiid_type
{
  struct scsiid_type * next;
  char *scsi_vendor_id;
  char *scsi_product_id;
  SANE_Bool is_processor;
  struct manufacturer_model_type *name;
}
scsiid_type;

static char *program_name;
static int debug = 0;
static int current_debug_level = 0;
static char *search_dir_spec = 0;
static backend_entry *first_backend = 0;
static enum output_mode mode = output_mode_ascii;
static char *title = 0;
static char *intro = 0;
static SANE_String desc_name = 0;
static const char *status_name[] =
  {"Unknown", "Unsupported", "Untested", "Minimal", "Basic",
   "Good", "Complete"};
static const char *device_type_name[] =
  {"Unknown", "Scanners", "Still cameras", "Video Cameras", "Meta backends",
   "APIs"};
static const char *device_type_aname[] =
  {"UKNOWN", "SCANNERS", "STILL", "VIDEO", "META",
   "API"};
static const char *status_color[] =
  {COLOR_UNKNOWN, COLOR_UNSUPPORTED, COLOR_UNTESTED, COLOR_MINIMAL,
   COLOR_BASIC, COLOR_GOOD, COLOR_COMPLETE};


static void
debug_call (const char *fmt, ...)
{
  va_list ap;
  char *level_txt;

  va_start (ap, fmt);
  if (debug >= current_debug_level)
    {
      /* print to stderr */
      switch (current_debug_level)
	{
	case 0:
	  level_txt = "ERROR:";
	  break;
	case 1:
	  level_txt = "Warning:";
	  break;
	case 2:
	  level_txt = "Info:";
	  break;
	default:
	  level_txt = "";
	  break;
	}
      if (desc_name)
	fprintf (stderr, "%s: %8s ", desc_name, level_txt);
      else
	fprintf (stderr, "[%s] %8s ", program_name, level_txt);
      vfprintf (stderr, fmt, ap);
    }
  va_end (ap);
}

static void
print_usage (char *program_name)
{
  printf ("Usage: %s [-s dir] [-m mode] [-d level] [-h] [-V]\n",
	  program_name);
  printf ("  -s|--search-dir dir    "
	  "Specify the directory that contains .desc files \n"
	  "                         "
	  "(multiple directories can be concatenated by \":\")\n");
  printf ("  -m|--mode mode         "
	  "Output mode (ascii, html-backends-split, html-mfgs,\n"
	  "                         xml, statistics, usermap, db, udev, udev+acl, udev+hwdb, hwdb, plist, hal, hal-new)\n");
  printf ("  -t|--title \"title\"     The title used for HTML pages\n");
  printf ("  -i|--intro \"intro\"     A short description of the "
	  "contents of the page\n");
  printf ("  -d|--debug-level level Specify debug level (0-3)\n");
  printf ("  -h|--help              Print help message\n");
  printf ("  -V|--version           Print version information\n");
  printf ("Report bugs to <henning@meier-geinitz.de>\n");
}

static void
print_version (void)
{
  printf ("sane-desc %s (%s)\n", SANE_DESC_VERSION, PACKAGE_STRING);
  printf ("Copyright (C) 2002-2006 Henning Meier-Geinitz "
	  "<henning@meier-geinitz.de>\n"
	  "sane-desc comes with NO WARRANTY, to the extent permitted by "
	  "law.\n"
	  "You may redistribute copies of sane-desc under the terms of the "
	  "GNU General\n"
	  "Public License.\n"
	  "For more information about these matters, see the file named "
	  "COPYING.\n");
}

static SANE_Bool
get_options (int argc, char **argv)
{
  int longindex;
  int opt;
  static struct option desc_options[] = {
    {"search-dir", required_argument, NULL, 's'},
    {"mode", required_argument, NULL, 'm'},
    {"title", required_argument, NULL, 't'},
    {"intro", required_argument, NULL, 'i'},
    {"debug-level", required_argument, NULL, 'd'},
    {"help", 0, NULL, 'h'},
    {"version", 0, NULL, 'V'},
    {0, 0, 0, 0}
  };

  while ((opt = getopt_long (argc, argv, "s:m:t:i:d:hV", desc_options,
			     &longindex)) != -1)
    {
      switch (opt)
	{
	case 'h':
	  print_usage (argv[0]);
	  exit (0);
	case 'V':
	  print_version ();
	  exit (0);
	case 's':
	  search_dir_spec = strdup (optarg);
	  DBG_INFO ("setting search directory to `%s'\n", search_dir_spec);
	  break;
	case 'm':
	  if (strcmp (optarg, "ascii") == 0)
	    {
	      DBG_INFO ("Output mode: %s\n", optarg);
	      mode = output_mode_ascii;
	    }
	  else if (strcmp (optarg, "xml") == 0)
	    {
	      DBG_INFO ("Output mode: %s\n", optarg);
	      mode = output_mode_xml;
	    }
	  else if (strcmp (optarg, "html-backends-split") == 0)
	    {
	      DBG_INFO ("Output mode: %s\n", optarg);
	      mode = output_mode_html_backends_split;
	    }
	  else if (strcmp (optarg, "html-mfgs") == 0)
	    {
	      DBG_INFO ("Output mode: %s\n", optarg);
	      mode = output_mode_html_mfgs;
	    }
	  else if (strcmp (optarg, "statistics") == 0)
	    {
	      DBG_INFO ("Output mode: %s\n", optarg);
	      mode = output_mode_statistics;
	    }
	  else if (strcmp (optarg, "usermap") == 0)
	    {
	      DBG_INFO ("Output mode: %s\n", optarg);
	      mode = output_mode_usermap;
	    }
	  else if (strcmp (optarg, "db") == 0)
	    {
	      DBG_INFO ("Output mode: %s\n", optarg);
	      mode = output_mode_db;
	    }
	  else if (strcmp (optarg, "udev") == 0)
	    {
	      DBG_INFO ("Output mode: %s\n", optarg);
	      mode = output_mode_udev;
	    }
	  else if (strcmp (optarg, "udev+acl") == 0)
	    {
	      DBG_INFO ("Output mode: %s\n", optarg);
	      mode = output_mode_udevacl;
	    }
	  else if (strcmp (optarg, "udev+hwdb") == 0)
	    {
	      DBG_INFO ("Output mode: %s\n", optarg);
	      mode = output_mode_udevhwdb;
	    }
	  else if (strcmp (optarg, "hwdb") == 0)
	    {
	      DBG_INFO ("Output mode: %s\n", optarg);
	      mode = output_mode_hwdb;
	    }
	  else if (strcmp (optarg, "plist") == 0)
	    {
	      DBG_INFO ("Output mode: %s\n", optarg);
	      mode = output_mode_plist;
	    }
	  else if (strcmp (optarg, "hal") == 0)
	    {
	      DBG_INFO ("Output mode: %s\n", optarg);
	      mode = output_mode_hal;
	    }
	  else if (strcmp (optarg, "hal-new") == 0)
	    {
	      DBG_INFO ("Output mode: %s\n", optarg);
	      mode = output_mode_halnew;
	    }
	  else
	    {
	      DBG_ERR ("Unknown output mode: %s\n", optarg);
	      exit (1);
	    }
	  break;
	case 't':
	  title = optarg;
	  DBG_INFO ("setting title to `%s'\n", optarg);
	  break;
	case 'i':
	  intro = optarg;
	  DBG_INFO ("setting intro to `%s'\n", optarg);
	  break;
	case 'd':
	  debug = atoi (optarg);
	  DBG_INFO ("setting debug level to %d\n", debug);
	  break;
	case '?':
	  DBG_ERR ("unknown option (use -h for help)\n");
	  return SANE_FALSE;
	case ':':
	  DBG_ERR ("missing parameter (use -h for help)\n");
	  return SANE_FALSE;
	default:
	  DBG_ERR ("missing option (use -h for help)\n");
	  return SANE_FALSE;
	}
    }
  if (!search_dir_spec)
    search_dir_spec = ".";
  return SANE_TRUE;
}

static int
char_compare (char char1, char char2)
{
  char1 = toupper (char1);
  char2 = toupper (char2);

  if (char1 < char2)
    return -1;
  else if (char1 > char2)
    return 1;
  else
    return 0;
}

static int
num_compare (char *num_string1, char *num_string2)
{
  int num1 = atoi (num_string1);
  int num2 = atoi (num_string2);
  if (num1 < num2)
    return -1;
  else if (num1 > num2)
    return 1;
  else
    return 0;
}

/* Compare two strings, try to sort numbers correctly (600 < 1200) */
static int
string_compare (char *string1, char *string2)
{
  int count = 0;
  int compare = 0;

  if (!string1)
    {
      if (!string2)
	return 0;
      else
	return 1;
    }
  else if (!string2)
    return -1;

  while (string1[count] && string2[count])
    {
      if (isdigit (string1[count]) && isdigit (string2[count]))
	compare = num_compare (&string1[count], &string2[count]);
      else
	compare = char_compare (string1[count], string2[count]);
      if (compare != 0)
	return compare;
      count++;
    }
  return char_compare (string1[count], string2[count]);
}

/* Add URLs to the end of the list if they are unique */
static url_entry *
update_url_list (url_entry * first_url, char *new_url)
{
  url_entry *url = first_url;
  SANE_Bool found = SANE_FALSE;

  while (url && url->name)
    {
      if (string_compare (url->name, new_url) == 0)
	found = SANE_TRUE;
      url = url->next;
    }
  if (found)
    return first_url;

  url = first_url;
  if (url)
    {
      while (url->next)
	url = url->next;
      url->next = calloc (1, sizeof (url_entry));
      url = url->next;
    }
  else
    {
      first_url = calloc (1, sizeof (url_entry));
      url = first_url;
    }
  if (!url)
    {
      DBG_ERR ("update_url_list: couldn't calloc url_entry\n");
      exit (1);
    }
  url->name = new_url;
  return first_url;
}

/* Get the next token, ignoring escaped quotation marks */
static const char *
get_token (const char *str, char **string_const)
{
  const char *start;
  size_t len;

  str = sanei_config_skip_whitespace (str);

  if (*str == '"')
    {
      start = ++str;
      while (*str && (*str != '"' || *(str - 1) == '\\'))
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
    *string_const = NULL;
  return str;
}

/* Checks a line for a keyword token and determines keyword/string argument */
static SANE_Status
read_keyword (SANE_String line, SANE_String keyword_token,
	      parameter_type p_type, void *argument)
{
  SANE_String_Const cp;
  SANE_Char *word;

  word = 0;

  cp = get_token (line, &word);

  if (!word)
    {
      DBG_ERR ("read_keyword: missing quotation mark: %s\n", line);
      return SANE_STATUS_INVAL;
    }

  if (strcmp (word, keyword_token) != 0)
    {
      free(word);
      return SANE_STATUS_INVAL;
    }

  free (word);
  word = 0;

  switch (p_type)
    {
    case param_none:
      return SANE_STATUS_GOOD;
    case param_string:
      {
	char *pos;
	cp = get_token (cp, &word);
	if (!word)
	  {
	    DBG_ERR ("read_keyword: missing quotation mark: %s\n", line);
	    return SANE_STATUS_INVAL;
	  }
	/* remove escaped quotations */
	while ((pos = strstr (word, "\\\"")) != 0)
	  *pos = ' ';

	DBG_DBG ("read_keyword: set entry `%s' to `%s'\n", keyword_token,
		 word);
	*(SANE_String *) argument = strdup (word);
	break;
      }
    case param_two_strings:
      {
	char *pos;
	char **strings = malloc (2 * sizeof (SANE_String));

	cp = get_token (cp, &word);
	if (!word)
	  {
	    free(strings);
	    DBG_ERR ("read_keyword: missing quotation mark: %s\n", line);
	    return SANE_STATUS_INVAL;
	  }
	/* remove escaped quotations */
	while ((pos = strstr (word, "\\\"")) != 0)
	  *pos = ' ';
	DBG_INFO ("read_keyword: set first entry of `%s' to `%s'\n", keyword_token,
		 word);
	strings[0] = strdup (word);
	if (word)
	  free (word);

	cp = get_token (cp, &word);
	if (!word)
	  {
	    free(strings);
	    DBG_ERR ("read_keyword: missing quotation mark: %s\n", line);
	    return SANE_STATUS_INVAL;
	  }
	/* remove escaped quotations */
	while ((pos = strstr (word, "\\\"")) != 0)
	  *pos = ' ';
	DBG_INFO ("read_keyword: set second entry of `%s' to `%s'\n", keyword_token,
		 word);
	strings[1] = strdup (word);
	* (SANE_String **) argument = strings;
	break;
      }
    case param_three_strings:
      {
	char *pos;
	char **strings = malloc (3 * sizeof (SANE_String));

	cp = get_token (cp, &word);
	if (!word)
	  {
	    free(strings);
	    DBG_ERR ("read_keyword: missing quotation mark: %s\n", line);
	    return SANE_STATUS_INVAL;
	  }
	/* remove escaped quotations */
	while ((pos = strstr (word, "\\\"")) != 0)
	  *pos = ' ';
	DBG_INFO ("read_keyword: set first entry of `%s' to `%s'\n", keyword_token,
		 word);
	strings[0] = strdup (word);
	if (word)
	  free (word);

	cp = get_token (cp, &word);
	if (!word)
	  {
	    free(strings);
	    DBG_ERR ("read_keyword: missing quotation mark: %s\n", line);
	    return SANE_STATUS_INVAL;
	  }
	/* remove escaped quotations */
	while ((pos = strstr (word, "\\\"")) != 0)
	  *pos = ' ';
	DBG_INFO ("read_keyword: set second entry of `%s' to `%s'\n", keyword_token,
		 word);
	strings[1] = strdup (word);
	if (word)
	  free (word);

	cp = get_token (cp, &word);
	if (!word)
	  {
	    free(strings);
	    DBG_ERR ("read_keyword: missing quotation mark: %s\n", line);
	    return SANE_STATUS_INVAL;
	  }
	/* remove escaped quotations */
	while ((pos = strstr (word, "\\\"")) != 0)
	  *pos = ' ';
	DBG_INFO ("read_keyword: set third entry of `%s' to `%s'\n", keyword_token,
		 word);
	strings[2] = strdup (word);
	* (SANE_String **) argument = strings;
	break;
      }
    default:
      DBG_ERR ("read_keyword: unknown param_type %d\n", p_type);
      return SANE_STATUS_INVAL;
    }

  if (word)
    free (word);
  word = 0;
  return SANE_STATUS_GOOD;
}

/* Check for a all-lowercase 4-digit hex number (e.g. 0x1234) */
static SANE_Bool
check_hex (SANE_String string)
{
  unsigned int i;

  if (strlen (string) != 6)
    return SANE_FALSE;
  if (strncmp (string, "0x", 2) != 0)
    return SANE_FALSE;
  for (i = 0; i < strlen (string); i++)
    {
      if (isupper (string[i]))
	return SANE_FALSE;
    }
  for (i = 2; i < strlen (string); i++)
    {
      if (!isxdigit (string[i]))
	return SANE_FALSE;
    }
  return SANE_TRUE;
}

/* Read and interprete the .desc files */
static SANE_Bool
read_files (void)
{
  struct stat stat_buf;
  DIR *dir;
  struct dirent *dir_entry;
  FILE *fp;
  char file_name[PATH_MAX];
  SANE_Char line[4096], *word;
  SANE_String_Const cp;
  backend_entry *current_backend = 0;
  type_entry *current_type = 0;
  mfg_entry *current_mfg = 0;
  model_entry *current_model = 0;
  enum level current_level = level_backend;
  char *search_dir = search_dir_spec, *end = 0;

  DBG_INFO ("looking for .desc files in `%s'\n", search_dir_spec);

  while (search_dir && search_dir[0])
    {
      end = strchr (search_dir, ':');
      if (end)
	end[0] = '\0';
      DBG_INFO ("reading directory `%s'\n", search_dir);

      if (stat (search_dir, &stat_buf) < 0)
	{
	  DBG_ERR ("cannot stat `%s' (%s)\n", search_dir, strerror (errno));
	  return SANE_FALSE;
	}
      if (!S_ISDIR (stat_buf.st_mode))
	{
	  DBG_ERR ("`%s' is not a directory\n", search_dir);
	  return SANE_FALSE;
	}
      if ((dir = opendir (search_dir)) == 0)
	{
	  DBG_ERR ("cannot read directory `%s' (%s)\n", search_dir,
		   strerror (errno));
	  return SANE_FALSE;
	}

      while ((dir_entry = readdir (dir)) != NULL)
	{
	  if (strlen (dir_entry->d_name) > 5 &&
	      strcmp (dir_entry->d_name + strlen (dir_entry->d_name) - 5,
		      ".desc") == 0)
	    {
	      if (strlen (search_dir)
		  + strlen (dir_entry->d_name) + 1 + 1 > PATH_MAX)
		{
		  DBG_ERR ("filename too long\n");
		  return SANE_FALSE;
		}
	      sprintf (file_name, "%s/%s", search_dir, dir_entry->d_name);
	      DBG_INFO ("-> reading desc file: %s\n", file_name);
	      fp = fopen (file_name, "r");
	      if (!fp)
		{
		  DBG_ERR ("can't open desc file: %s (%s)\n", file_name,
			   strerror (errno));
		  return SANE_FALSE;
		}
	      /* now we check if everything is ok with the previous backend
		 before we read the new one */
	      if (current_backend)
		{
		  type_entry *current_type = current_backend->type;
		  int no_usbids = 0;
		  int no_interface = 0;
		  int no_status = 0;

		  while (current_type)
		    {
		      if (current_type->type == type_scanner ||
			  current_type->type == type_stillcam ||
			  current_type->type == type_vidcam)
			{
			  mfg_entry *current_mfg = current_type->mfg;

			  while (current_mfg)
			    {
			      model_entry *current_model = current_mfg->model;

			      while (current_model)
				{
				  if (current_model->status == status_unknown)
				    {
				      DBG_INFO
					("Backend `%s': `%s' `%s' does not have a status\n",
					 current_backend->name,
					 current_mfg->name,
					 current_model->name);
				      no_status++;
				    }
				  if (!current_model->interface)
				    {
				      DBG_INFO
					("Backend `%s': `%s' `%s' does not have an interface\n",
					 current_backend->name,
					 current_mfg->name,
					 current_model->name);
				      no_interface++;
				    }
				  else if (strstr (current_model->interface, "USB"))
				    {
				      if ((!current_model->usb_vendor_id || !current_model->usb_product_id)
					  && !current_model->ignore_usb_id)
					{
					  DBG_INFO ("`%s' seems to provide a USB device "
						    "without :usbid (%s %s)\n",
						    current_backend->name,
						    current_mfg->name,
						    current_model->name);
					  no_usbids++;
					}
				    }
				  current_model = current_model->next;
				}
			      current_mfg = current_mfg->next;
			    }
			}
		      current_type = current_type->next;
		    }
		  if (no_status)
		    {
		      DBG_WARN ("Backend `%s': %d devices without :status\n",
				current_backend->name, no_status);
		    }
		  if (no_interface)
		    {
		      DBG_WARN ("Backend `%s': %d devices without :interface\n",
				current_backend->name, no_interface);
		    }
		  if (no_usbids)
		    {
		      DBG_WARN ("Backend `%s': %d USB devices without :usbid\n",
				current_backend->name, no_usbids);
		    }
		}
	      desc_name = dir_entry->d_name;
	      current_backend = 0;
	      current_type = 0;
	      current_mfg = 0;
	      current_model = 0;
	      while (sanei_config_read (line, sizeof (line), fp))
		{
		  char *string_entry = 0;
		  char **two_string_entry;
		  char **three_string_entry;
		  word = 0;

		  cp = get_token (line, &word);
		  if (!word || cp == line)
		    {
		      DBG_DBG ("ignoring empty line\n");
		      if (word)
			free (word);
		      word = 0;
		      continue;
		    }
		  if (word[0] == ';')
		    {
		      DBG_DBG ("ignoring comment line\n");
		      free (word);
		      word = 0;
		      continue;
		    }
		  DBG_DBG ("line: %s\n", line);

		  if (read_keyword
		      (line, ":backend", param_string,
		       &string_entry) == SANE_STATUS_GOOD)
		    {
		      backend_entry *be = first_backend, *prev_be =
			0, *new_be = 0;
		      DBG_INFO ("creating backend entry `%s'\n",
				string_entry);

		      new_be = calloc (1, sizeof (backend_entry));
		      if (!new_be)
			{
			  DBG_ERR ("calloc failed (%s)\n", strerror (errno));
			  return SANE_FALSE;
			}
		      new_be->name = string_entry;
		      new_be->new = SANE_FALSE;

		      if (!be)
			{
			  first_backend = new_be;
			  be = new_be;
			}
		      else
			{
			  while (be)
			    {
			      int compare =
				string_compare (new_be->name, be->name);
			      if (compare <= 0)
				{
				  backend_entry *be_tmp = be;
				  be = new_be;
				  be->next = be_tmp;
				  if (!prev_be)
				    first_backend = be;
				  else
				    prev_be->next = be;
				  break;
				}
			      prev_be = be;
			      be = be->next;
			    }
			  if (!be)	/* last entry */
			    {
			      prev_be->next = new_be;
			      be = prev_be->next;
			    }
			}
		      current_backend = be;
		      current_type = 0;
		      current_mfg = 0;
		      current_model = 0;
		      current_level = level_backend;
		      continue;
		    }
		  if (!current_backend)
		    {
		      DBG_ERR ("use `:backend' keyword first\n");
		      return SANE_FALSE;
		    }
		  if (read_keyword
		      (line, ":version", param_string,
		       &string_entry) == SANE_STATUS_GOOD)
		    {
		      if (current_backend->version)
			{
			  DBG_WARN
			    ("overwriting version of backend `%s' to `%s'"
			     "(was: `%s')\n", current_backend->name,
			     string_entry, current_backend->version,
			     current_backend->version);
			}

		      DBG_INFO ("setting version of backend `%s' to `%s'\n",
				current_backend->name, string_entry);
		      current_backend->version = string_entry;
		      continue;
		    }
		  if (read_keyword
		      (line, ":status", param_string,
		       &string_entry) == SANE_STATUS_GOOD)
		    {
		      switch (current_level)
			{
			case level_model:
			  if (current_model->status != status_unknown)
			    {
			      DBG_WARN
				("overwriting status of model `%s' (backend `%s')\n",
				 current_model->name, current_backend->name);
			    }
			  if (strcmp (string_entry, ":minimal") == 0)
			    {
			      DBG_INFO
				("setting status of model `%s' to `minimal'\n",
				 current_model->name);
			      current_model->status = status_minimal;
			    }
			  else if (strcmp (string_entry, ":basic") == 0)
			    {
			      DBG_INFO
				("setting status of model `%s' to `basic'\n",
				 current_model->name);
			      current_model->status = status_basic;
			    }
			  else if (strcmp (string_entry, ":good") == 0)
			    {
			      DBG_INFO
				("setting status of model `%s' to `good'\n",
				 current_model->name);
			      current_model->status = status_good;
			    }
			  else if (strcmp (string_entry, ":complete") == 0)
			    {
			      DBG_INFO
				("setting status of model `%s' to `complete'\n",
				 current_model->name);
			      current_model->status = status_complete;
			    }
			  else if (strcmp (string_entry, ":untested") == 0)
			    {
			      DBG_INFO
				("setting status of model `%s' to `untested'\n",
				 current_model->name);
			      current_model->status = status_untested;
			    }
			  else if (strcmp (string_entry, ":unsupported") == 0)
			    {
			      DBG_INFO
				("setting status of model `%s' to `unsupported'\n",
				 current_model->name);
			      current_model->status = status_unsupported;
			    }
			  else
			    {
			      DBG_ERR
				("unknown status of model `%s': `%s' (backend `%s')\n",
				 current_model->name, string_entry,
				 current_backend->name);
			      current_model->status = status_untested;
			      return SANE_FALSE;
			    }
			  break;
			default:
			  DBG_ERR
			    ("level %d not implemented for :status (backend `%s')\n",
			     current_level, current_backend->name);
			  return SANE_FALSE;
			}


		      continue;
		    }
		  if (read_keyword (line, ":new", param_string, &string_entry)
		      == SANE_STATUS_GOOD)
		    {
		      if (strcmp (string_entry, ":yes") == 0)
			{
			  DBG_INFO
			    ("backend %s is new in this SANE release\n",
			     current_backend->name);
			  current_backend->new = SANE_TRUE;
			}
		      else if (strcmp (string_entry, ":no") == 0)
			{
			  DBG_INFO
			    ("backend %s is NOT new in this SANE release\n",
			     current_backend->name);
			  current_backend->new = SANE_FALSE;
			}
		      else
			{
			  DBG_ERR ("unknown :new parameter of backend `%s': "
				   "`%s'\n", current_backend->name,
				   string_entry);
			  current_backend->new = SANE_FALSE;
			  return SANE_FALSE;
			}
		      continue;
		    }
		  if (read_keyword
		      (line, ":manpage", param_string,
		       &string_entry) == SANE_STATUS_GOOD)
		    {
		      if (current_backend->manpage)
			{
			  DBG_WARN
			    ("overwriting manpage of backend `%s' to `%s'"
			     "(was: `%s')\n", current_backend->name,
			     string_entry, current_backend->manpage);
			}

		      DBG_INFO ("setting manpage of backend `%s' to `%s'\n",
				current_backend->name, string_entry);
		      current_backend->manpage = string_entry;
		      continue;
		    }
		  if (read_keyword
		      (line, ":devicetype", param_string,
		       &string_entry) == SANE_STATUS_GOOD)
		    {
		      type_entry *type = 0;

		      type = current_backend->type;

		      DBG_INFO
			("adding `%s' to list of device types of backend "
			 "`%s'\n", string_entry, current_backend->name);

		      if (type)
			{
			  while (type->next)
			    type = type->next;
			  type->next = calloc (1, sizeof (type_entry));
			  type = type->next;
			}
		      else
			{
			  current_backend->type =
			    calloc (1, sizeof (type_entry));
			  type = current_backend->type;
			}

		      type->type = type_unknown;
		      if (strcmp (string_entry, ":scanner") == 0)
			{
			  DBG_INFO ("setting device type of backend `%s' to "
				    "scanner\n", current_backend->name);
			  type->type = type_scanner;
			}
		      else if (strcmp (string_entry, ":stillcam") == 0)
			{
			  DBG_INFO ("setting device type of backend `%s' to "
				    "still camera\n", current_backend->name);
			  type->type = type_stillcam;
			}
		      else if (strcmp (string_entry, ":vidcam") == 0)
			{
			  DBG_INFO ("setting device type of backend `%s' to "
				    "video camera\n", current_backend->name);
			  type->type = type_vidcam;
			}
		      else if (strcmp (string_entry, ":api") == 0)
			{
			  DBG_INFO ("setting device type of backend `%s' to "
				    "API\n", current_backend->name);
			  type->type = type_api;
			}
		      else if (strcmp (string_entry, ":meta") == 0)
			{
			  DBG_INFO ("setting device type of backend `%s' to "
				    "meta\n", current_backend->name);
			  type->type = type_meta;
			}
		      else
			{
			  DBG_ERR
			    ("unknown device type of backend `%s': `%s'\n",
			     current_backend->name, string_entry);
			  type->type = type_unknown;
			  return SANE_FALSE;
			}
		      current_type = type;
		      current_mfg = 0;
		      current_model = 0;
		      continue;
		    }
		  if (read_keyword
		      (line, ":desc", param_string,
		       &string_entry) == SANE_STATUS_GOOD)
		    {
		      if (!current_type)
			{
			  DBG_ERR
			    ("use `:devicetype' keyword first (backend `%s')\n",
			     current_backend->name);
			  return SANE_FALSE;
			}
		      if (current_type->type < type_meta)
			{
			  DBG_ERR
			    ("use `:desc' for `:api' and `:meta' only (backend `%s')\n",
			     current_backend->name);
			  return SANE_FALSE;
			}

		      if (current_type->desc)
			{
			  DBG_WARN
			    ("overwriting description of  device type of "
			     "backend `%s' to `%s' (was: `%s')\n",
			     current_backend->name, string_entry,
			     current_type->desc);
			}

		      DBG_INFO
			("setting description of backend `%s' to `%s'\n",
			 current_backend->name, string_entry);
		      current_type->desc = calloc (1, sizeof (desc_entry));
		      if (!current_type->desc)
			{
			  DBG_ERR ("calloc failed (%s)\n", strerror (errno));
			  return SANE_FALSE;
			}
		      current_type->desc->desc = string_entry;
		      current_level = level_desc;
		      current_mfg = 0;
		      current_model = 0;
		      continue;
		    }
		  if (read_keyword (line, ":mfg", param_string, &string_entry)
		      == SANE_STATUS_GOOD)
		    {
		      mfg_entry *mfg = 0;

		      if (!current_type)
			{
			  DBG_ERR
			    ("use `:devicetype' keyword first (backend `%s')\n",
			     current_backend->name);
			  return SANE_FALSE;
			}
		      if (current_type->type >= type_meta)
			{
			  DBG_ERR
			    ("use `:mfg' for hardware devices only (backend `%s')\n",
			     current_backend->name);
			  return SANE_FALSE;
			}

		      mfg = current_type->mfg;
		      if (mfg)
			{
			  while (mfg->next)
			    mfg = mfg->next;
			  mfg->next = calloc (1, sizeof (mfg_entry));
			  mfg = mfg->next;
			}
		      else
			{
			  current_type->mfg = calloc (1, sizeof (mfg_entry));
			  mfg = current_type->mfg;
			}

		      if (!mfg)
			{
			  DBG_ERR ("calloc failed (%s)\n", strerror (errno));
			  return SANE_FALSE;
			}
		      mfg->name = string_entry;
		      DBG_INFO ("adding mfg entry %s to backend `%s'\n",
				string_entry, current_backend->name);
		      current_mfg = mfg;
		      current_model = 0;
		      current_level = level_mfg;
		      continue;
		    }
		  if (read_keyword
		      (line, ":model", param_string,
		       &string_entry) == SANE_STATUS_GOOD)
		    {
		      model_entry *model = 0;

		      if (!current_type)
			{
			  DBG_ERR
			    ("use `:devicetype' keyword first (backend `%s')\n",
			     current_backend->name);
			  return SANE_FALSE;
			}
		      if (current_level != level_mfg
			  && current_level != level_model)
			{
			  DBG_ERR
			    ("use `:mfg' keyword first (backend `%s')\n",
			     current_backend->name);
			  return SANE_FALSE;
			}
		      model = current_mfg->model;
		      if (model)
			{
			  while (model->next)
			    model = model->next;
			  model->next = calloc (1, sizeof (model_entry));
			  model = model->next;
			}
		      else
			{
			  current_mfg->model =
			    calloc (1, sizeof (model_entry));
			  model = current_mfg->model;
			}

		      if (!model)
			{
			  DBG_ERR ("calloc failed (%s)\n", strerror (errno));
			  return SANE_FALSE;
			}
		      model->name = string_entry;
		      model->status = status_unknown;
		      DBG_INFO
			("adding model entry %s to manufacturer `%s'\n",
			 string_entry, current_mfg->name);
		      current_model = model;
		      current_level = level_model;
		      continue;
		    }
		  if (read_keyword
		      (line, ":interface", param_string,
		       &string_entry) == SANE_STATUS_GOOD)
		    {
		      if (!current_model)
			{
			  DBG_WARN
			    ("ignored `%s' :interface, only allowed for "
			     "hardware devices\n", current_backend->name);
			  continue;
			}

		      if (current_model->interface)
			{
			  DBG_WARN ("overwriting `%s's interface of model "
				    "`%s' to `%s' (was: `%s')\n",
				    current_backend->name,
				    current_model->name, string_entry,
				    current_model->interface);
			}

		      DBG_INFO ("setting interface of model `%s' to `%s'\n",
				current_model->name, string_entry);
		      current_model->interface = string_entry;
		      continue;
		    }
		  if (read_keyword
		      (line, ":scsi", param_three_strings,
		       &three_string_entry) == SANE_STATUS_GOOD)
		    {
		      if (!current_model)
			{
			  DBG_WARN
			    ("ignored `%s' :scsi, only allowed for "
			     "hardware devices\n", current_backend->name);
			  continue;
			}

		      DBG_INFO ("setting scsi vendor and product ids of model `%s' to `%s/%s'\n",
				current_model->name, three_string_entry[0], three_string_entry[1]);
		      if (strcasecmp (three_string_entry[0], "ignore") == 0)
			 {
				DBG_INFO ("Ignoring `%s's scsi-entries of `%s'\n",
						current_backend->name,
						current_model->name);
				continue;
			 }
			 if (strcasecmp (three_string_entry[2], "processor") == 0){
				current_model->scsi_is_processor = SANE_TRUE;
				current_model->scsi_vendor_id = three_string_entry[0];
				current_model->scsi_product_id = three_string_entry[1];
			 }
			 else
			 {
				DBG_INFO ("scsi-format info in %s is invalid -> break\n", current_backend->name);
				continue;
			 }
		      continue;
		    }
		  if (read_keyword
		      (line, ":usbid", param_two_strings,
		       &two_string_entry) == SANE_STATUS_GOOD)
		    {
		      if (!current_model)
			{
			  DBG_WARN
			    ("ignored `%s' :usbid, only allowed for "
			     "hardware devices\n", current_backend->name);
			  continue;
			}
		      if (strcasecmp (two_string_entry[0], "ignore") == 0)
			{
			  DBG_INFO ("Ignoring `%s's USB ids of `%s'\n",
				    current_backend->name,
				    current_model->name);
			  current_model->ignore_usb_id = SANE_TRUE;
			  continue;
			}
		      if (!check_hex (two_string_entry[0]))
			{
			  DBG_WARN ("`%s's USB vendor id of `%s' is "
				    "not a lowercase 4-digit hex number: "
				    "`%s'\n", current_backend->name,
				    current_model->name, two_string_entry[0]);
			  continue;
			}
		      if (!check_hex (two_string_entry[1]))
			{
			  DBG_WARN ("`%s's USB product id of `%s' is "
				    "not a lowercase 4-digit hex number: "
				    "`%s'\n", current_backend->name,
				    current_model->name, two_string_entry[1]);
			  continue;
			}

		      if (current_model->usb_vendor_id || current_model->usb_product_id)
			{
			  DBG_WARN ("overwriting `%s's USB ids of model "
				    "`%s' to `%s/%s' (was: `%s/%s')\n",
				    current_backend->name,
				    current_model->name, two_string_entry[0],
				    two_string_entry[1],
				    current_model->usb_vendor_id,
				    current_model->usb_product_id);
			}

		      DBG_INFO ("setting USB vendor and product ids of model `%s' to `%s/%s'\n",
				current_model->name, two_string_entry[0], two_string_entry[1]);
		      current_model->usb_vendor_id = two_string_entry[0];
		      current_model->usb_product_id = two_string_entry[1];
		      continue;
		    }
		  if (read_keyword (line, ":url", param_string, &string_entry)
		      == SANE_STATUS_GOOD)
		    {
		      switch (current_level)
			{
			case level_backend:
			  current_backend->url =
			    update_url_list (current_backend->url,
					     string_entry);
			  DBG_INFO ("adding `%s' to list of urls of backend "
				    "`%s'\n", string_entry,
				    current_backend->name);
			  break;
			case level_mfg:
			  current_mfg->url =
			    update_url_list (current_mfg->url, string_entry);
			  DBG_INFO ("adding `%s' to list of urls of mfg "
				    "`%s'\n", string_entry,
				    current_mfg->name);
			  break;
			case level_desc:
			  current_type->desc->url =
			    update_url_list (current_type->desc->url,
					     string_entry);
			  DBG_INFO
			    ("adding `%s' to list of urls of description "
			     "for backend `%s'\n", string_entry,
			     current_backend->name);
			  break;
			case level_model:
			  current_model->url =
			    update_url_list (current_model->url,
					     string_entry);
			  DBG_INFO ("adding `%s' to list of urls of model "
				    "`%s'\n", string_entry,
				    current_model->name);
			  break;
			default:
			  DBG_ERR
			    ("level %d not implemented for :url (backend `%s')\n",
			     current_level, current_backend->name);
			  return SANE_FALSE;
			}
		      continue;
		    }
		  if (read_keyword
		      (line, ":comment", param_string,
		       &string_entry) == SANE_STATUS_GOOD)
		    {
		      switch (current_level)
			{
			case level_backend:
			  current_backend->comment = string_entry;
			  DBG_INFO ("setting comment of backend %s to `%s'\n",
				    current_backend->name, string_entry);
			  break;
			case level_mfg:
			  current_mfg->comment = string_entry;
			  DBG_INFO
			    ("setting comment of manufacturer %s to `%s'\n",
			     current_mfg->name, string_entry);
			  break;
			case level_desc:
			  current_type->desc->comment = string_entry;
			  DBG_INFO ("setting comment of description for "
				    "backend %s to `%s'\n",
				    current_backend->name, string_entry);
			  break;
			case level_model:
			  current_model->comment = string_entry;
			  DBG_INFO ("setting comment of model %s to `%s'\n",
				    current_model->name, string_entry);
			  break;
			default:
			  DBG_ERR
			    ("level %d not implemented for `:comment' (backend `%s')\n",
			     current_level, current_backend->name);
			  return SANE_FALSE;
			}
		      continue;
		    }
		  DBG_ERR
		    ("unknown keyword token in line `%s' of file `%s'\n",
		     line, file_name);
		  return SANE_FALSE;
		}		/* while (sanei_config_readline) */
	      fclose (fp);
	    }			/* if (strlen) */
	}			/* while (direntry) */
      if (closedir(dir) != 0)
	{
	  DBG_ERR ("cannot close directory `%s' (%s)\n", search_dir,
		   strerror (errno));
	  return SANE_FALSE;
	}
      if (end)
	search_dir = end + 1;
      else
	search_dir = (search_dir + strlen (search_dir));
    }

  desc_name = 0;
  if (!first_backend)
    {
      DBG_ERR ("Couldn't find any .desc file\n");
      return SANE_FALSE;
    }
  return SANE_TRUE;
}

/* Create a model_record_entry based on a model_entry */
static model_record_entry *
create_model_record (model_entry * model)
{
  model_record_entry *model_record;

  model_record = calloc (1, sizeof (model_record_entry));
  if (!model_record)
    {
      DBG_ERR ("create_model_record: couldn't calloc model_record_entry\n");
      exit (1);
    }
  model_record->name = model->name;
  model_record->status = model->status;
  model_record->interface = model->interface;
  model_record->url = model->url;
  model_record->comment = model->comment;
  model_record->usb_vendor_id = model->usb_vendor_id;
  model_record->usb_product_id = model->usb_product_id;
  model_record->scsi_vendor_id = model->scsi_vendor_id;
  model_record->scsi_product_id = model->scsi_product_id;
  model_record->scsi_is_processor = model->scsi_is_processor;
  return model_record;
}

/* Calculate the priority of statuses: */
/* minimal, basic, good, complete -> 2, untested -> 1, unsupported -> 0 */
static int
calc_priority (status_entry status)
{
  switch (status)
    {
    case status_untested:
      return 1;
    case status_unsupported:
      return 0;
    default:
      return 2;
    }
}

/* Insert model into list at the alphabetically correct position */
static model_record_entry *
update_model_record_list (model_record_entry * first_model_record,
			  model_entry * model, backend_entry * be)
{
  model_record_entry *model_record = first_model_record;

  if (!first_model_record)
    {
      /* First model for this manufacturer */
      first_model_record = create_model_record (model);
      model_record = first_model_record;
    }
  else
    {
      model_record_entry *prev_model_record = 0;

      while (model_record)
	{
	  int compare = string_compare (model->name, model_record->name);
	  if (compare <= 0)
	    {
	      model_record_entry *tmp_model_record = model_record;
	      if ((compare == 0) &&
		  (string_compare (model->interface, model_record->interface) == 0) &&
		  (string_compare (model->usb_vendor_id, model_record->usb_vendor_id) == 0) &&
		  (string_compare (model->usb_product_id, model_record->usb_product_id) == 0))
		{
		  /* Two entries for the same model */
		  int new_priority = calc_priority (model->status);
		  int old_priority = calc_priority (model_record->status);
		  if (new_priority < old_priority)
		    {
		      DBG_DBG
			("update_model_record_list: model %s ignored, backend %s has "
			 "higher priority\n", model->name,
			 model_record->be->name);
		      return first_model_record;
		    }
		  if (new_priority > old_priority)
		    {
		      DBG_DBG
			("update_model_record_list: model %s overrides the one from backend %s\n",
			 model->name, model_record->be->name);
		      tmp_model_record = model_record->next;
		    }
		}
	      /* correct position */
	      model_record = create_model_record (model);
	      model_record->next = tmp_model_record;
	      if (!prev_model_record)
		first_model_record = model_record;
	      else
		prev_model_record->next = model_record;
	      break;
	    }
	  prev_model_record = model_record;
	  model_record = model_record->next;
	}
      if (!model_record)	/* last entry */
	{
	  prev_model_record->next = create_model_record (model);
	  model_record = prev_model_record->next;
	}
    }				/* if (first_model_record) */
  model_record->be = be;
  DBG_DBG ("update_model_record_list: added model %s\n", model->name);
  return first_model_record;
}


/* Insert manufacturer into list at the alphabetically correct position, */
/* create new record if neccessary */
static mfg_record_entry *
update_mfg_record_list (mfg_record_entry * first_mfg_record, mfg_entry * mfg,
			backend_entry * be)
{
  model_entry *model = mfg->model;
  mfg_record_entry *mfg_record = first_mfg_record;

  while (mfg_record)
    {
      if (string_compare (mfg_record->name, mfg->name) == 0)
	{
	  /* Manufacturer already exists */
	  url_entry *mfg_url = mfg->url;

	  /* Manufacturer comments and (additional) URLs? */
	  if (!mfg_record->comment)
	    mfg_record->comment = mfg->comment;
	  while (mfg_url && mfg_url->name)
	    {
	      mfg_record->url = update_url_list (mfg_record->url,
						 mfg_url->name);
	      mfg_url = mfg_url->next;
	    }
	  break;
	}
      mfg_record = mfg_record->next;
    }

  if (!mfg_record)
    {
      /* Manufacturer doesn't exist yet */
      url_entry *url = mfg->url;

      mfg_record = calloc (1, sizeof (mfg_record_entry));
      if (!mfg_record)
	{
	  DBG_ERR ("update_mfg_record_list: couldn't calloc "
		   "mfg_record_entry\n");
	  exit (1);
	}
      mfg_record->name = mfg->name;
      mfg_record->comment = mfg->comment;
      while (url)
	{
	  mfg_record->url = update_url_list (mfg_record->url, url->name);
	  url = url->next;
	}
      if (first_mfg_record != 0)
	{
	  /* We already have one manufacturer in the list */
	  mfg_record_entry *new_mfg_record = mfg_record;
	  mfg_record_entry *prev_mfg_record = 0;

	  mfg_record = first_mfg_record;

	  while (mfg_record)
	    {
	      int compare =
		string_compare (new_mfg_record->name, mfg_record->name);
	      if (compare <= 0)
		{
		  mfg_record_entry *tmp_mfg_record = mfg_record;
		  mfg_record = new_mfg_record;
		  mfg_record->next = tmp_mfg_record;
		  if (!prev_mfg_record)
		    first_mfg_record = mfg_record;
		  else
		    prev_mfg_record->next = mfg_record;
		  break;
		}
	      prev_mfg_record = mfg_record;
	      mfg_record = mfg_record->next;
	    }
	  if (!mfg_record)	/* last entry */
	    {
	      prev_mfg_record->next = new_mfg_record;
	      mfg_record = prev_mfg_record->next;
	    }
	}
      else
	first_mfg_record = mfg_record;
      DBG_DBG ("update_mfg_record_list: created mfg %s\n", mfg_record->name);
    }				/* if (!mfg_record) */

  /* create model entries */
  while (model)
    {
      mfg_record->model_record =
	update_model_record_list (mfg_record->model_record, model, be);
      model = model->next;
    }
  return first_mfg_record;
}

/* Create a sorted list of manufacturers based on the backends list */
static mfg_record_entry *
create_mfg_list (device_type dev_type)
{
  mfg_record_entry *first_mfg_record = 0;
  backend_entry *be = first_backend;

  DBG_DBG ("create_mfg_list: start\n");
  while (be)
    {
      type_entry *type = be->type;
      while (type)
	{
	  if (type->type == dev_type)
	    {
	      mfg_entry *mfg = type->mfg;
	      while (mfg)
		{
		  first_mfg_record =
		    update_mfg_record_list (first_mfg_record, mfg, be);
		  mfg = mfg->next;
		}
	    }
	  type = type->next;
	}
      be = be->next;
    }
  DBG_DBG ("create_mfg_list: exit\n");
  return first_mfg_record;
}

/* Print an ASCII list with all the information we have */
static void
ascii_print_backends (void)
{
  backend_entry *be;

  be = first_backend;
  while (be)
    {
      url_entry *url = be->url;
      type_entry *type = be->type;

      if (be->name)
	printf ("backend `%s'\n", be->name);
      else
	printf ("backend *none*\n");

      if (be->version)
	printf (" version `%s'\n", be->version);
      else
	printf (" version *none*\n");

      if (be->new)
	printf (" NEW!\n");

      if (be->manpage)
	printf (" manpage `%s'\n", be->manpage);
      else
	printf (" manpage *none*\n");

      if (url)
	while (url)
	  {
	    printf (" url `%s'\n", url->name);
	    url = url->next;
	  }
      else
	printf (" url *none*\n");

      if (be->comment)
	printf (" comment `%s'\n", be->comment);
      else
	printf (" comment *none*\n");

      if (type)
	while (type)
	  {
	    switch (type->type)
	      {
	      case type_scanner:
		printf (" type scanner\n");
		break;
	      case type_stillcam:
		printf (" type stillcam\n");
		break;
	      case type_vidcam:
		printf (" type vidcam\n");
		break;
	      case type_meta:
		printf (" type meta\n");
		break;
	      case type_api:
		printf (" type api\n");
		break;
	      default:
		printf (" type *unknown*\n");
		break;
	      }
	    if (type->desc)
	      {
		url_entry *url = type->desc->url;
		printf ("  desc `%s'\n", type->desc->desc);
		if (url)
		  while (url)
		    {
		      printf ("   url `%s'\n", url->name);
		      url = url->next;
		    }
		else
		  printf ("   url *none*\n");

		if (type->desc->comment)
		  printf ("   comment `%s'\n", type->desc->comment);
		else
		  printf ("   comment *none*\n");
	      }
	    else if (type->type >= type_meta)
	      printf ("  desc *none*\n");

	    if (type->mfg)
	      {
		mfg_entry *mfg = type->mfg;
		while (mfg)
		  {
		    model_entry *model = mfg->model;
		    url_entry *url = mfg->url;

		    printf ("  mfg `%s'\n", mfg->name);
		    if (url)
		      while (url)
			{
			  printf ("   url `%s'\n", url->name);
			  url = url->next;
			}
		    else
		      printf ("   url *none*\n");

		    if (mfg->comment)
		      printf ("   comment `%s'\n", mfg->comment);
		    else
		      printf ("   comment *none*\n");

		    if (model)
		      while (model)
			{
			  url_entry *url = model->url;
			  printf ("   model `%s'\n", model->name);
			  if (model->interface)
			    printf ("    interface `%s'\n", model->interface);
			  else
			    printf ("    interface *none*\n");

			  if (model->usb_vendor_id)
			    printf ("    usb-vendor-id `%s'\n", model->usb_vendor_id);
			  else
			    printf ("    usb-vendor-id *none*\n");

			  if (model->usb_product_id)
			    printf ("    usb-product-id `%s'\n", model->usb_product_id);
			  else
			    printf ("    usb-product-id *none*\n");

			  switch (model->status)
			    {
			    case status_minimal:
			      printf ("    status minimal\n");
			      break;
			    case status_basic:
			      printf ("    status basic\n");
			      break;
			    case status_good:
			      printf ("    status good\n");
			      break;
			    case status_complete:
			      printf ("    status complete\n");
			      break;
			    case status_untested:
			      printf ("    status untested\n");
			      break;
			    case status_unsupported:
			      printf ("    status unsupported\n");
			      break;
			    default:
			      printf ("    status *unknown*\n");
			      break;
			    }

			  if (url)
			    while (url)
			      {
				printf ("    url `%s'\n", url->name);
				url = url->next;
			      }
			  else
			    printf ("    url *none*\n");

			  if (model->comment)
			    printf ("    comment `%s'\n", model->comment);
			  else
			    printf ("    comment *none*\n");

			  model = model->next;
			}
		    else
		      printf ("   model *none*\n");

		    mfg = mfg->next;
		  }		/* while (mfg) */
	      }
	    else if (type->type < type_meta)
	      printf ("  mfg *none*\n");
	    type = type->next;
	  }			/* while (type) */
      else
	printf (" type *none*\n");
      be = be->next;
    }				/* while (be) */
}


static char *
clean_string (char *c)
{
  /* not avoided characters */

  char *aux;

  aux = malloc (strlen (c) * sizeof (char) * 6);

  *aux = '\0';

  while (*c != '\0')
    {

      /*limit to printable ASCII only*/
      if(*c < 0x20 || *c > 0x7e){
        c++;
        continue;
      }

      switch (*c)
	{
	case '<':
	  aux = strcat (aux, "&lt;");
	  break;
	case '>':
	  aux = strcat (aux, "&gt;");
	  break;
	case '\'':
	  aux = strcat (aux, "&apos;");
	  break;
	case '&':
	  aux = strcat (aux, "&amp;");
	  break;
	default:
	  aux = strncat (aux, c, 1);
	}
      c = c + 1;
    }
  return aux;
}

/* Print an XML list with all the information we have */
static void
xml_print_backends (void)
{
  backend_entry *be;

  be = first_backend;
  printf ("<backends>\n");
  while (be)
    {
      url_entry *url = be->url;
      type_entry *type = be->type;

      if (be->name)
	printf ("<backend name=\"%s\">\n", clean_string (be->name));
      else
	printf ("<backend name=\"*none\">\n");

      if (be->version)
	printf ("<version>%s</version> \n", clean_string (be->version));
      else
	printf ("<version>*none*</version>\n");

      if (be->new)
	printf ("<new state=\"yes\"/>\n");
      else
	printf ("<new state=\"no\"/>\n");


      if (be->manpage)
	printf (" <manpage>%s</manpage>\n", clean_string (be->manpage));
      else
	printf (" <manpage>*none*</manpage>\n");

      if (url)
	while (url)
	  {
	    printf (" <url>%s</url>\n", clean_string (url->name));
	    url = url->next;
	  }
      else
	printf (" <url>*none*</url>\n");

      if (be->comment)
	printf (" <comment>%s</comment>\n", clean_string (be->comment));
      else
	printf (" <comment>*none*</comment>\n");

      if (type)
	while (type)
	  {

	    switch (type->type)
	      {
	      case type_scanner:
		printf (" <type def=\"scanner\">\n");
		break;
	      case type_stillcam:
		printf (" <type def=\"stillcam\">\n");
		break;
	      case type_vidcam:
		printf (" <type def=\"vidcam\">\n");
		break;
	      case type_meta:
		printf (" <type def=\"meta\">\n");
		break;
	      case type_api:
		printf (" <type def=\"api\">\n");
		break;
	      default:
		printf (" <type def=\"*unknown*\">\n");
		break;
	      }
	    if (type->desc)
	      {
		url_entry *url = type->desc->url;
		printf ("   <desc>%s</desc>\n",
			clean_string (type->desc->desc));
		if (url)
		  while (url)
		    {
		      printf ("   <url>%s</url>\n", clean_string (url->name));
		      url = url->next;
		    }
		else
		  printf ("   <url>*none*</url>\n");

		if (type->desc->comment)
		  printf ("   <comment>%s</comment>\n",
			  clean_string (type->desc->comment));
		else
		  printf ("   <comment>*none*</comment>\n");
	      }
	    else if (type->type >= type_meta)
	      printf ("  <desc>*none*</desc>\n");

	    if (type->mfg)
	      {
		mfg_entry *mfg = type->mfg;
		while (mfg)
		  {
		    model_entry *model = mfg->model;
		    url_entry *url = mfg->url;

		    printf (" <mfg name=\"%s\">\n", clean_string (mfg->name));
		    if (url)
		      while (url)
			{
			  printf ("  <url>`%s'</url>\n",
				  clean_string (url->name));
			  url = url->next;
			}
		    else
		      printf ("  <url>*none*</url>\n");

		    if (mfg->comment)
		      printf ("  <comment>%s</comment>\n",
			      clean_string (mfg->comment));
		    else
		      printf ("  <comment>*none*</comment>\n");

		    if (model)
		      while (model)
			{
			  url_entry *url = model->url;
			  printf ("   <model name=\"%s\">\n",
				  clean_string (model->name));
			  if (model->interface)
			    printf ("    <interface>%s</interface>\n",
				    clean_string (model->interface));
			  else
			    printf ("    <interface>*none*</interface>\n");

			  if (model->usb_vendor_id)
			    printf ("    <usbvendorid>%s</usbvendorid>\n",
				    clean_string (model->usb_vendor_id));
			  else
			    printf ("    <usbvendorid>*none*</usbvendorid>\n");
			  if (model->usb_product_id)
			    printf ("    <usbproductid>%s</usbproductid>\n",
				    clean_string (model->usb_product_id));
			  else
			    printf ("    <usbproductid>*none*</usbproductid>\n");

			  switch (model->status)
			    {
			    case status_minimal:
			      printf ("    <status>minimal</status>\n");
			      break;
			    case status_basic:
			      printf ("    <status>basic</status>\n");
			      break;
			    case status_good:
			      printf ("    <status>good</status>\n");
			      break;
			    case status_complete:
			      printf ("    <status>complete</status>\n");
			      break;
			    case status_untested:
			      printf ("    <status>untested</status>\n");
			      break;
			    case status_unsupported:
			      printf ("    <status>unsupported</status>\n");
			      break;
			    default:
			      printf ("    <status>*unknown*</status>\n");
			      break;
			    }

			  if (url)
			    while (url)
			      {
				printf ("    <url>%s</url>\n",
					clean_string (url->name));
				url = url->next;
			      }
			  else
			    printf ("    <url>*none*</url>\n");

			  if (model->comment)
			    printf ("    <comment>%s</comment>\n",
				    clean_string (model->comment));
			  else
			    printf ("    <comment>*none*</comment>\n");

			  model = model->next;
			  printf ("   </model>\n");
			}	/* while (model) */
		    else
		      printf ("   <model name=\"*none*\" />\n");

		    printf (" </mfg>\n");
		    mfg = mfg->next;
		  }		/* while (mfg) */
	      }
	    else if (type->type < type_meta)
	      printf ("  <mfg>*none*</mfg>\n");
	    type = type->next;
	    printf (" </type>\n");
	  }			/* while (type) */
      else
	printf (" <type>*none*</type>\n");
      printf ("</backend>\n");
      be = be->next;

    }				/* while (be) */
  printf ("</backends>\n");
}

/* calculate statistics about supported devices per device type*/
static void
calculate_statistics_per_type (device_type dev_type, statistics_type num)
{
  backend_entry *be = first_backend;

  while (be)
    {
      type_entry *type = be->type;

      while (type)
	{
	  if (type->type == dev_type)
	    {
	      mfg_entry *mfg = type->mfg;
	      model_entry *model;

	      if (type->desc)
		{
		  num[status_complete]++;
		  type = type->next;
		  continue;
		}

	      if (!mfg)
		{
		  type = type->next;
		  continue;
		}

	      mfg = type->mfg;
	      while (mfg)
		{
		  model = mfg->model;
		  if (model)
		    {
		      while (model)
			{
			  enum status_entry status = model->status;
			  num[status]++;
			  model = model->next;
			}	/* while (model) */
		    }		/* if (num_models) */
		  mfg = mfg->next;
		}		/* while (mfg) */
	    }			/* if (type->type) */
	  type = type->next;
	}			/* while (type) */
      be = be->next;
    }				/* while (be) */
}

static void
html_print_statistics_cell (const char * color, int number)
{
  printf ("<td align=center><font color=%s>%d</font></td>\n",
	  color, number);
}

static void
html_print_statistics_per_type (device_type dev_type)
{
  statistics_type num = {0, 0, 0, 0, 0, 0, 0};
  status_entry status;

  calculate_statistics_per_type (dev_type, num);
  printf ("<tr>\n");
  printf("<td align=center><a href=\"#%s\">%s</a></td>\n",
	 device_type_aname [dev_type], device_type_name [dev_type]);

  html_print_statistics_cell
    (COLOR_UNKNOWN,
     num[status_minimal] + num[status_basic] + num[status_good] +
     num[status_complete] + num[status_untested] + num[status_unsupported]);
  if (dev_type == type_scanner || dev_type == type_stillcam
      || dev_type == type_vidcam)
    {
      html_print_statistics_cell
	(COLOR_UNKNOWN,
	 num[status_minimal] + num[status_basic] + num[status_good] +
	 num[status_complete]);
      for (status = status_complete; status >= status_unsupported; status--)
	html_print_statistics_cell (status_color [status], num [status]);
    }
  else
    {
      printf ("<td align=center colspan=7>n/a</td>\n");
    }
  printf ("</tr>\n");
}

/* print html statistcis */
static void
html_print_summary (void)
{
  device_type dev_type;
  status_entry status;

  printf ("<h2>Summary</h2>\n");
  printf ("<table border=1>\n");
  printf ("<tr bgcolor=E0E0FF>\n");
  printf ("<th align=center rowspan=3>Device type</th>\n");
  printf ("<th align=center colspan=8>Number of devices</th>\n");
  printf ("</tr>\n");
  printf ("<tr bgcolor=E0E0FF>\n");
  printf ("<th align=center rowspan=2>Total</th>\n");
  printf ("<th align=center colspan=5>Supported</th>\n");
  printf ("<th align=center rowspan=2><font color=" COLOR_UNTESTED
	  ">%s</font></th>\n", status_name[status_untested]);
  printf ("<th align=center rowspan=2><font color=" COLOR_UNSUPPORTED
	  ">%s</font></th>\n", status_name[status_unsupported]);
  printf ("</tr>\n");
  printf ("<tr bgcolor=E0E0FF>\n");
  printf ("<th align=center>Sum</th>\n");
  for (status = status_complete; status >= status_minimal; status--)
    printf ("<th align=center><font color=%s>%s</font></th>\n",
	    status_color[status], status_name[status]);
  printf ("</tr>\n");
  for (dev_type = type_scanner; dev_type <= type_api; dev_type++)
    html_print_statistics_per_type (dev_type);
  printf ("</table>\n");
}


/* Generate a name used for <a name=...> HTML tags */
static char *
html_generate_anchor_name (device_type dev_type, char *manufacturer_name)
{
  char *name = malloc (strlen (manufacturer_name) + 1 + 2);
  char *pointer = name;
  char type_char;

  if (!name)
    {
      DBG_ERR ("html_generate_anchor_name: couldn't malloc\n");
      return 0;
    }

  switch (dev_type)
    {
    case type_scanner:
      type_char = 'S';
      break;
    case type_stillcam:
      type_char = 'C';
      break;
    case type_vidcam:
      type_char = 'V';
      break;
    case type_meta:
      type_char = 'M';
      break;
    case type_api:
      type_char = 'A';
      break;
    default:
      type_char = 'Z';
      break;
    }

  snprintf (name, strlen (manufacturer_name) + 1 + 2, "%c-%s",
	    type_char, manufacturer_name);

  while (*pointer)
    {
      if (!isalnum (*pointer))
	*pointer = '-';
      else
	*pointer = toupper (*pointer);
      pointer++;
    }
  return name;
}


/* Generate one table per backend of all backends providing models */
/* of type dev_type */
static void
html_backends_split_table (device_type dev_type)
{
  backend_entry *be = first_backend;
  SANE_Bool first = SANE_TRUE;

  printf ("<p><b>Backends</b>: \n");
  while (be)			/* print link list */
    {
      type_entry *type = be->type;
      SANE_Bool found = SANE_FALSE;

      while (type)
	{
	  if (type->type == dev_type)
	    found = SANE_TRUE;
	  type = type->next;
	}
      if (found)
	{
	  if (!first)
	    printf (", \n");
	  first = SANE_FALSE;
	  printf ("<a href=\"#%s\">%s</a>",
		  html_generate_anchor_name (dev_type, be->name), be->name);
	}
      be = be->next;
    }
  be = first_backend;
  if (first)
    printf ("(none)\n");

  printf ("</p>\n");


  while (be)
    {
      type_entry *type = be->type;

      while (type)
	{
	  if (type->type == dev_type)
	    {
	      mfg_entry *mfg = type->mfg;
	      model_entry *model;

	      printf ("<h3><a name=\"%s\">Backend: %s\n",
		      html_generate_anchor_name (type->type, be->name),
		      be->name);

	      if (be->version || be->new)
		{
		  printf ("(");
		  if (be->version)
		    {
		      printf ("%s", be->version);
		      if (be->new)
			printf (", <font color=" COLOR_NEW ">NEW!</font>");
		    }
		  else
		    printf ("<font color=" COLOR_NEW ">NEW!</font>");
		  printf (")\n");
		}
	      printf ("</a></h3>\n");

	      printf ("<p>\n");

	      if (be->url && be->url->name)
		{
		  url_entry *url = be->url;
		  printf ("<b>Link(s):</b> \n");
		  while (url)
		    {
		      if (url != be->url)
			printf (", ");
		      printf ("<a href=\"%s\">%s</a>", url->name, url->name);
		      url = url->next;
		    }
		  printf ("<br>\n");
		}
	      if (be->manpage)
		printf ("<b>Manual page:</b> <a href=\"" MAN_PAGE_LINK
			"\">%s</a><br>\n", be->manpage, be->manpage);

	      if (be->comment)
		printf ("<b>Comment:</b> %s<br>\n", be->comment);


	      if (type->desc)
		{
		  if (type->desc->desc)
		    {
		      if (type->desc->url && type->desc->url->name)
			printf ("<b>Description:</b> "
				"<a href=\"%s\">%s</a><br>\n",
				type->desc->url->name, type->desc->desc);
		      else
			printf ("<b>Description:</b> %s<br>\n",
				type->desc->desc);
		    }

		  if (type->desc->comment)
		    printf ("<b>Comment:</b> %s<br>\n", type->desc->comment);
		  printf ("</p>\n");
		  type = type->next;
		  continue;
		}
	      printf ("</p>\n");

	      if (!mfg)
		{
		  type = type->next;
		  continue;
		}

	      printf ("<table border=1>\n");

	      printf ("<tr bgcolor=E0E0FF>\n");
	      printf ("<th align=center>Manufacturer</th>\n");
	      printf ("<th align=center>Model</th>\n");
	      printf ("<th align=center>Interface</th>\n");
	      printf ("<th align=center>USB id</th>\n");
	      printf ("<th align=center>Status</th>\n");
	      printf ("<th align=center>Comment</th>\n");
	      printf ("</tr>\n");

	      mfg = type->mfg;
	      while (mfg)
		{
		  model = mfg->model;
		  if (model)
		    {
		      int num_models = 0;

		      while (model)	/* count models for rowspan */
			{
			  model = model->next;
			  num_models++;
			}
		      model = mfg->model;
		      printf ("<tr>\n");
		      printf ("<td align=center rowspan=%d>\n", num_models);
		      if (mfg->url && mfg->url->name)
			printf ("<a href=\"%s\">%s</a>\n", mfg->url->name,
				mfg->name);
		      else
			printf ("%s\n", mfg->name);

		      while (model)
			{
			  enum status_entry status = model->status;

			  if (model != mfg->model)
			    printf ("<tr>\n");

			  if (model->url && model->url->name)
			    printf
			      ("<td align=center><a href=\"%s\">%s</a></td>\n",
			       model->url->name, model->name);
			  else
			    printf ("<td align=center>%s</td>\n",
				    model->name);

			  if (model->interface)
			    printf ("<td align=center>%s</td>\n",
				    model->interface);
			  else
			    printf ("<td align=center>?</td>\n");

			  if (model->usb_vendor_id && model->usb_product_id)
			    printf ("<td align=center>%s/%s</td>\n",
				    model->usb_vendor_id, model->usb_product_id);
			  else
			    printf ("<td align=center>&nbsp;</td>\n");

			  printf ("<td align=center><font color=%s>%s</font></td>\n",
				  status_color[status], status_name[status]);

			  if (model->comment && model->comment[0] != 0)
			    printf ("<td>%s</td>\n", model->comment);
			  else
			    printf ("<td>&nbsp;</td>\n");

			  model = model->next;
			  printf ("</tr>\n");
			}	/* while (model) */
		    }		/* if (num_models) */
		  mfg = mfg->next;
		}		/* while (mfg) */
	      printf ("</table>\n");
	    }			/* if (type->type) */
	  type = type->next;
	}			/* while (type) */
      be = be->next;
    }				/* while (be) */
  /*  printf ("</table>\n"); */
}

/* Generate one table per manufacturer constructed of all backends */
/* providing models of type dev_type */
static void
html_mfgs_table (device_type dev_type)
{
  mfg_record_entry *mfg_record = 0, *first_mfg_record = 0;

  first_mfg_record = create_mfg_list (dev_type);
  mfg_record = first_mfg_record;

  printf ("<p><b>Manufacturers</b>: \n");
  while (mfg_record)
    {
      if (mfg_record != first_mfg_record)
	printf (", \n");
      printf ("<a href=\"#%s\">%s</a>",
	      html_generate_anchor_name (type_unknown, mfg_record->name),
	      mfg_record->name);
      mfg_record = mfg_record->next;
    }
  mfg_record = first_mfg_record;
  if (!mfg_record)
    printf ("(none)\n");
  printf ("</p>\n");
  while (mfg_record)
    {
      model_record_entry *model_record = mfg_record->model_record;

      printf ("<h3><a name=\"%s\">Manufacturer: %s</a></h3>\n",
	      html_generate_anchor_name (type_unknown, mfg_record->name),
	      mfg_record->name);
      printf ("<p>\n");
      if (mfg_record->url && mfg_record->url->name)
	{
	  url_entry *url = mfg_record->url;
	  printf ("<b>Link(s):</b> \n");
	  while (url)
	    {
	      if (url != mfg_record->url)
		printf (", ");
	      printf ("<a href=\"%s\">%s</a>", url->name, url->name);
	      url = url->next;
	    }
	  printf ("<br>\n");
	}
      if (mfg_record->comment)
	printf ("<b>Comment:</b> %s<br>\n", mfg_record->comment);
      printf ("</p>\n");
      if (!model_record)
	{
	  mfg_record = mfg_record->next;
	  continue;
	}
      printf ("<table border=1>\n");
      printf ("<tr bgcolor=E0E0FF>\n");

      printf ("<th align=center>Model</th>\n");
      printf ("<th align=center>Interface</th>\n");
      printf ("<th align=center>USB id</th>\n");
      printf ("<th align=center>Status</th>\n");
      printf ("<th align=center>Comment</th>\n");
      printf ("<th align=center>Backend</th>\n");
      printf ("<th align=center>Manpage</th>\n");
      printf ("</tr>\n");

      while (model_record)
	{
	  enum status_entry status = model_record->status;

	  if (model_record->url && model_record->url->name)
	    printf ("<tr><td align=center><a "
		    "href=\"%s\">%s</a></td>\n",
		    model_record->url->name, model_record->name);
	  else
	    printf ("<tr><td align=center>%s</td>\n", model_record->name);

	  if (model_record->interface)
	    printf ("<td align=center>%s</td>\n", model_record->interface);
	  else
	    printf ("<td align=center>?</td>\n");

	  if (model_record->usb_vendor_id && model_record->usb_product_id)
	    printf ("<td align=center>%s/%s</td>\n",
		    model_record->usb_vendor_id, model_record->usb_product_id);
	  else
	    printf ("<td align=center>&nbsp;</td>\n");

	  printf ("<td align=center><font color=%s>%s</font></td>\n",
		  status_color[status], status_name[status]);

	  if (model_record->comment && model_record->comment[0] != 0)
	    printf ("<td>%s</td>\n", model_record->comment);
	  else
	    printf ("<td>&nbsp;</td>\n");

	  printf ("<td align=center>\n");
	  if (model_record->be->url && model_record->be->url->name)
	    printf ("<a href=\"%s\">%s</a>\n",
		    model_record->be->url->name, model_record->be->name);
	  else
	    printf ("%s", model_record->be->name);

	  if (model_record->be->version || model_record->be->new)
	    {
	      printf ("<br>(");
	      if (model_record->be->version)
		{
		  printf ("%s", model_record->be->version);
		  if (model_record->be->new)
		    printf (", <font color=" COLOR_NEW ">NEW!</font>");
		}
	      else
		printf ("<font color=" COLOR_NEW ">NEW!</font>");
	      printf (")\n");
	    }

	  printf ("</td>\n");
	  if (model_record->be->manpage)
	    printf ("<td align=center><a href=\""
		    MAN_PAGE_LINK "\">%s</a></td>\n",
		    model_record->be->manpage, model_record->be->manpage);
	  else
	    printf ("<td align=center>?</td>\n");

	  printf ("</tr>\n");
	  model_record = model_record->next;
	}			/* while model_record */
      printf ("</table>\n");
      mfg_record = mfg_record->next;
    }				/* while mfg_record */
}

/* Print the HTML headers and an introduction */
static void
html_print_header (void)
{
  printf
    ("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">\n"
     "<html> <head>\n"
     "<meta http-equiv=\"Content-Type\" content=\"text/html; "
     "charset=iso-8859-1\">\n");
  printf ("<title>%s</title>\n", title);
  printf
    ("</head>\n"
     "<body bgcolor=FFFFFF>\n"
     "<div align=center>\n"
     "<img src=\"http://www.sane-project.org/images/sane.png\" alt=\"SANE\">\n");
  printf ("<h1>%s</h1>\n", title);
  printf ("</div>\n" "<hr>\n");
  printf ("%s\n", intro);
  printf
    ("<p>This is only a summary!\n"
     "Please consult the manpages and the author-supplied webpages\n"
     "for more detailed (and usually important) information\n"
     "concerning each backend.</p>\n");
  printf
    ("<p>If you have new information or corrections, please file a\n"
     "<a href=\"http://www.sane-project.org/bugs.html\">bug report</a>\n"
     "with as many details as possible. Also please tell us if your scanner \n"
     "isn't mentioned in this list at all.</p>\n"
     "<p>For an explanation of the tables, see the\n"
     "<a href=\"#legend\">legend</a>.\n");
}

/* Print the HTML footers and contact information */
static void
html_print_footer (void)
{
  time_t current_time = time (0);

  printf
    ("<hr>\n"
     "<a href=\"http://www.sane-project.org/\">SANE homepage</a>\n"
     "<address>\n"
     "<a href=\"http://www.sane-project.org/imprint.html\"\n"
     ">Contact</a>\n" "</address>\n" "<font size=-1>\n");
  printf ("This page was last updated on %s by sane-desc %s from %s\n",
	  asctime (localtime (&current_time)), SANE_DESC_VERSION, PACKAGE_STRING);
  printf ("</font>\n");
  printf ("</body> </html>\n");
}


/* print parts of the legend */
static void
html_print_legend_backend (void)
{
  printf
    ("  <dt><b>Backend:</b></dt>\n"
     "  <dd>Name of the backend,  in parentheses if available:\n"
     "      Version of backend/driver; newer versions may be\n"
     "      available from their home sites.<br>"
     "      <font color=" COLOR_NEW ">NEW!</font> means brand-new to the\n"
     "      current release of SANE.<br>\n"
     "      UNMAINTAINED means that nobody maintains that backend. Expect no \n"
     "      new features or newly supported devices. You are welcome to take over \n"
     "      maintainership.\n" "  </dd>\n");
}

static void
html_print_legend_link (void)
{
  printf
    ("  <dt><b>Link(s):</b></dt>\n"
     "  <dd>Link(s) to more extensive and\n"
     "      detailed information, if it exists, or the email address\n"
     "      of the author or maintainer.\n");
}

static void
html_print_legend_manual (void)
{
  printf
    ("  <dt><b>Manual Page:</b></dt>\n"
     "  <dd>A link to the man-page online, if it exists.</dd>\n");
}

static void
html_print_legend_comment (void)
{
  printf
    ("  <dt><b>Comment:</b></dt>\n"
     "  <dd>More information about the backend or model, e.g. the level of "
     "      support and possible problems.</dd>\n");
}

static void
html_print_legend_manufacturer (void)
{
  printf
    ("  <dt><b>Manufacturer:</b></dt>\n"
     "  <dd>Manufacturer, vendor or brand name of the device.</dd>\n");
}

static void
html_print_legend_model (void)
{
  printf
    ("  <dt><b>Model:</b></dt>\n" "  <dd>Name of the device.</dd>\n");
}

static void
html_print_legend_interface (void)
{
  printf
    ("  <dt><b>Interface:</b></dt>\n"
     "  <dd>How the device is connected to the computer.</dd>\n");
}

static void
html_print_legend_usbid (void)
{
  printf
    ("  <dt><b>USB id:</b></dt>\n"
     "  <dd>The USB vendor and product ids as printed by sane-find-scanner -q (only applicable for USB devices).</dd>\n");
}

static void
html_print_legend_status (void)
{
  printf
    ("  <dt><b>Status</b>:</dt>\n"
     "  <dd>Indicates how many of the features the device provides \n"
     "      are supported by SANE.\n"
     "      <ul><li><font color=" COLOR_UNSUPPORTED ">unsupported</font>"
     "        means the device is not supported at least by this backend. "
     "        It may be supported by other backends, however.\n");
  printf
    ("      <li><font color=" COLOR_UNTESTED ">untested</font> means the "
     "        device may be supported but couldn't be tested. Be very "
     "        careful and report success/failure.\n"
     "      <li><font color=" COLOR_MINIMAL ">minimal</font> means that the\n"
     "        device is detected and scans at least in one mode. But the quality \n"
     "        is bad or important features won't work.\n");
  printf
    ("      <li><font color=" COLOR_BASIC ">basic</font> means it works at \n"
     "        least in the most important modes but quality is not perfect.\n"
     "      <li><font color=" COLOR_GOOD
     ">good</font> means the device is usable \n"
     "        for day-to-day work. Some rather exotic features may be missing.\n"
     "      <li><font color=" COLOR_COMPLETE
     ">complete</font> means the backends \n"
     "        supports everything the device can do.\n" "      </ul></dd>\n");
}

static void
html_print_legend_description (void)
{
  printf
    ("  <dt><b>Description</b>:</dt>\n"
     "  <dd>The scope of application of the backend.\n");
}

/* Print the HTML page with one table of models per backend */
static void
html_print_backends_split (void)
{
  if (!title)
    title = "SANE: Backends (Drivers)";
  if (!intro)
    intro = "<p> The following table summarizes the backends/drivers "
      "distributed with the latest version of sane-backends, and the hardware "
      "or software they support. </p>";

  html_print_header ();

  html_print_summary ();

  printf ("<h2><a name=\"SCANNERS\">Scanners</a></h2>\n");
  html_backends_split_table (type_scanner);

  printf ("<h2><a name=\"STILL\">Still Cameras</a></h2>\n");
  html_backends_split_table (type_stillcam);

  printf ("<h2><a name=\"VIDEO\">Video Cameras</a></h2>\n");
  html_backends_split_table (type_vidcam);

  printf ("<h2><a name=\"API\">APIs</a></h2>\n");
  html_backends_split_table (type_api);

  printf ("<h2><a name=\"META\">Meta Backends</a></h2>\n");
  html_backends_split_table (type_meta);

  printf ("<h3><a name=\"legend\">Legend:</a></h3>\n" "<blockquote><dl>\n");

  html_print_legend_backend ();
  html_print_legend_link ();
  html_print_legend_manual ();
  html_print_legend_comment ();
  html_print_legend_manufacturer ();
  html_print_legend_model ();
  html_print_legend_interface ();
  html_print_legend_usbid ();
  html_print_legend_status ();
  html_print_legend_description ();

  printf ("</dl></blockquote>\n");

  html_print_footer ();
}

/* Print the HTML page with one table of models per manufacturer */
static void
html_print_mfgs (void)
{
  if (!title)
    title = "SANE: Supported Devices";

  if (!intro)
    intro = "<p> The following table summarizes the devices supported "
      "by the latest version of sane-backends. </p>";

  html_print_header ();

  html_print_summary ();

  printf ("<h2><a name=\"SCANNERS\">Scanners</a></h2>\n");
  html_mfgs_table (type_scanner);

  printf ("<h2><a name=\"STILL\">Still Cameras</a></h2>\n");
  html_mfgs_table (type_stillcam);

  printf ("<h2><a name=\"VIDEO\">Video Cameras</a></h2>\n");
  html_mfgs_table (type_vidcam);

  printf ("<h2><a name=\"API\">APIs</a></h2>\n");
  html_backends_split_table (type_api);

  printf ("<h2><a name=\"META\">Meta Backends</a></h2>\n");
  html_backends_split_table (type_meta);

  printf
    ("<h3><a name=\"legend\">Legend:</a></h3>\n" "<blockquote>\n" "<dl>\n");

  html_print_legend_model ();
  html_print_legend_interface ();
  html_print_legend_usbid ();
  html_print_legend_status ();
  html_print_legend_comment ();
  html_print_legend_backend ();
  html_print_legend_manual ();

  html_print_legend_manufacturer ();
  html_print_legend_description ();

  printf ("</dl>\n" "</blockquote>\n");

  html_print_footer ();
}


/* print statistics about supported devices */
static void
print_statistics_per_type (device_type dev_type)
{
  statistics_type num = {0, 0, 0, 0, 0, 0, 0};

  calculate_statistics_per_type (dev_type, num);

  printf (" Total:       %4d\n",
	  num[status_minimal] + num[status_basic] + num[status_good] +
	  num[status_complete] + num[status_untested] + num[status_untested] +
	  num[status_unsupported]);
  if (dev_type == type_scanner || dev_type == type_stillcam
      || dev_type == type_vidcam)
    {
      printf (" Supported:   %4d (complete: %d, good: %d, basic: %d, "
	      "minimal: %d)\n",
	      num[status_minimal] + num[status_basic] + num[status_good] +
	      num[status_complete], num[status_complete], num[status_good],
	      num[status_basic], num[status_minimal]);
      printf (" Untested:    %4d\n", num[status_untested]);
      printf (" Unsupported: %4d\n", num[status_unsupported]);
    }
}

static void
print_statistics (void)
{
  printf ("Number of known devices:\n");
  printf ("Scanners:\n");
  print_statistics_per_type (type_scanner);
  printf ("Still cameras:\n");
  print_statistics_per_type (type_stillcam);
  printf ("Video cameras:\n");
  print_statistics_per_type (type_vidcam);
  printf ("Meta backends:\n");
  print_statistics_per_type (type_meta);
  printf ("API backends:\n");
  print_statistics_per_type (type_api);
}

static usbid_type *
create_usbid (char *manufacturer, char *model,
	      char *usb_vendor_id, char *usb_product_id)
{
  usbid_type * usbid = calloc (1, sizeof (usbid_type));

  usbid->usb_vendor_id = strdup (usb_vendor_id);
  usbid->usb_product_id = strdup (usb_product_id);
  usbid->name = calloc (1, sizeof (manufacturer_model_type));
  usbid->name->name = calloc (1, strlen (manufacturer) + strlen (model) + 3);
  sprintf (usbid->name->name, "%s %s", manufacturer, model);
  usbid->name->next = 0;
  usbid->next = 0;
  DBG_DBG ("New USB ids: %s/%s (%s %s)\n", usb_vendor_id, usb_product_id,
	    manufacturer, model);
  return usbid;
}

static scsiid_type *
create_scsiid (char *manufacturer, char *model,
	       char *scsi_vendor_id, char *scsi_product_id, SANE_Bool is_processor)
{
  scsiid_type * scsiid = calloc (1, sizeof (scsiid_type));

  scsiid->scsi_vendor_id = strdup (scsi_vendor_id);
  scsiid->scsi_product_id = strdup (scsi_product_id);
  scsiid->is_processor = is_processor;
  scsiid->name = calloc (1, sizeof (manufacturer_model_type));
  scsiid->name->name = calloc (1, strlen (manufacturer) + strlen (model) + 3);
  sprintf (scsiid->name->name, "%s %s", manufacturer, model);
  scsiid->name->next = 0;
  scsiid->next = 0;
  DBG_DBG ("New SCSI ids: %s/%s (%s %s)\n", scsi_vendor_id, scsi_product_id,
	    manufacturer, model);
  return scsiid;
}

static usbid_type *
add_usbid (usbid_type *first_usbid, char *manufacturer, char *model,
	   char *usb_vendor_id, char *usb_product_id)
{
  usbid_type *usbid = first_usbid;
  usbid_type *prev_usbid = 0, *tmp_usbid = 0;

  if (!first_usbid)
    first_usbid = create_usbid (manufacturer, model, usb_vendor_id, usb_product_id);
  else
    {
      while (usbid)
	{
	  if (strcmp (usb_vendor_id, usbid->usb_vendor_id) == 0 &&
	      strcmp (usb_product_id, usbid->usb_product_id) == 0)
	    {
	      manufacturer_model_type *man_mod = usbid->name;

	      while (man_mod->next)
		man_mod = man_mod->next;
	      man_mod->next = malloc (sizeof (manufacturer_model_type));
	      man_mod->next->name = malloc (strlen (manufacturer) + strlen (model) + 3);
	      sprintf (man_mod->next->name, "%s %s", manufacturer, model);
	      man_mod->next->next = 0;
	      DBG_DBG ("Added manufacturer/model %s %s to USB ids %s/%s\n", manufacturer, model,
			usb_vendor_id, usb_product_id);
	      break;
	    }
	  if (strcmp (usb_vendor_id, usbid->usb_vendor_id) < 0 ||
	      (strcmp (usb_vendor_id, usbid->usb_vendor_id) == 0 &&
	       strcmp (usb_product_id, usbid->usb_product_id) < 0))
	    {

	      tmp_usbid = create_usbid (manufacturer, model, usb_vendor_id, usb_product_id);
	      tmp_usbid->next = usbid;
	      if (prev_usbid)
		prev_usbid->next = tmp_usbid;
	      else
		first_usbid = tmp_usbid;
	      break;
	    }
	  prev_usbid = usbid;
	  usbid = usbid->next;
	}
      if (!usbid)
	{
	  prev_usbid->next = create_usbid (manufacturer, model, usb_vendor_id, usb_product_id);
	  usbid = prev_usbid->next;
	}
    }
  return first_usbid;
}

static scsiid_type *
add_scsiid (scsiid_type *first_scsiid, char *manufacturer, char *model,
	    char *scsi_vendor_id, char *scsi_product_id, SANE_Bool is_processor)
{
  scsiid_type *scsiid = first_scsiid;
  scsiid_type *prev_scsiid = 0, *tmp_scsiid = 0;

  if (!first_scsiid)
    first_scsiid = create_scsiid (manufacturer, model, scsi_vendor_id, scsi_product_id, is_processor);
  else
    {
      while (scsiid)
	{
	  if (strcmp (scsi_vendor_id, scsiid->scsi_vendor_id) == 0 &&
	      strcmp (scsi_product_id, scsiid->scsi_product_id) == 0)
	    {
	      manufacturer_model_type *man_mod = scsiid->name;

	      while (man_mod->next)
		man_mod = man_mod->next;
	      man_mod->next = malloc (sizeof (manufacturer_model_type));
	      man_mod->next->name = malloc (strlen (manufacturer) + strlen (model) + 3);
	      sprintf (man_mod->next->name, "%s %s", manufacturer, model);
	      man_mod->next->next = 0;
	      DBG_DBG ("Added manufacturer/model %s %s to SCSI ids %s/%s\n", manufacturer, model,
			scsi_vendor_id, scsi_product_id);
	      break;
	    }
	  if (strcmp (scsi_vendor_id, scsiid->scsi_vendor_id) < 0 ||
	      (strcmp (scsi_vendor_id, scsiid->scsi_vendor_id) == 0 &&
	       strcmp (scsi_product_id, scsiid->scsi_product_id) < 0))
	    {

	      tmp_scsiid = create_scsiid (manufacturer, model, scsi_vendor_id, scsi_product_id, is_processor);
	      tmp_scsiid->next = scsiid;
	      if (prev_scsiid)
		prev_scsiid->next = tmp_scsiid;
	      else
		first_scsiid = tmp_scsiid;
	      break;
	    }
	  prev_scsiid = scsiid;
	  scsiid = scsiid->next;
	}
      if (!scsiid)
	{
	  prev_scsiid->next = create_scsiid (manufacturer, model, scsi_vendor_id, scsi_product_id, is_processor);
	  scsiid = prev_scsiid->next;
	}
    }
  return first_scsiid;
}

static usbid_type *
create_usbids_table (void)
{
  backend_entry *be;
  usbid_type *first_usbid = NULL;

  if (!first_backend)
    return NULL;

  for (be = first_backend; be; be = be->next)
    {
      type_entry *type;

      if (!be->type)
	continue;

      for (type = be->type; type; type = type->next)
	{
	  mfg_entry *mfg;

	  if (!type->mfg)
	      continue;

	  for (mfg = type->mfg; mfg; mfg = mfg->next)
	    {
	      model_entry *model;

	      if (!mfg->model)
		continue;

	      for (model = mfg->model; model; model = model->next)
		{
		  if ((model->status == status_unsupported)
		      || (model->status == status_unknown))
		    continue;

		  if (model->usb_vendor_id && model->usb_product_id)
		    {
		      first_usbid = add_usbid (first_usbid, mfg->name,
					       model->name,
					       model->usb_vendor_id,
					       model->usb_product_id);
		    }
		} /* for (model) */
	    } /* for (mfg) */
	} /* for (type) */
    } /* for (be) */

  return first_usbid;
}

static scsiid_type *
create_scsiids_table (void)
{
  backend_entry *be;
  scsiid_type *first_scsiid = NULL;

  if (!first_backend)
    return NULL;

  for (be = first_backend; be; be = be->next)
    {
      type_entry *type;

      if (!be->type)
	continue;

      for (type = be->type; type; type = type->next)
	{
	  mfg_entry *mfg;

	  if (!type->mfg)
	    continue;

	  for (mfg = type->mfg; mfg; mfg = mfg->next)
	    {
	      model_entry *model;

	      if (!mfg->model)
		continue;

	      for (model = mfg->model; model; model = model->next)
		{
		  if ((model->status == status_unsupported)
		      || (model->status == status_unknown))
		    continue;

		  if (model->scsi_vendor_id && model->scsi_product_id)
		    {
		      first_scsiid = add_scsiid (first_scsiid, mfg->name,
						 model->name,
						 model->scsi_vendor_id,
						 model->scsi_product_id,
						 model->scsi_is_processor);
		    }
		} /* for (model) */
	    } /* for (mfg) */
	} /* for (type) */
    } /* for (be) */

  return first_scsiid;
}

/* print USB usermap file to be used by the hotplug tools */
static void
print_usermap_header (void)
{
  time_t current_time = time (0);

  printf
    ("# This file was automatically created based on description files (*.desc)\n"
    "# by sane-desc %s from %s on %s"
    "#\n"
    ,
    SANE_DESC_VERSION, PACKAGE_STRING, asctime (localtime (&current_time)));

  printf
     ("# The entries below are used to detect a USB device and change owner\n"
     "# and permissions on the \"device node\" used by libusb.\n"
     "#\n"
     "# The 0x0003 match flag means the device is matched by its vendor and\n"
     "# product IDs.\n"
     "#\n"
     "# Sample entry (replace 0xVVVV and 0xPPPP with vendor ID and product ID\n"
     "# respectively):\n"
     "#\n"
     );

  printf
    ("# libusbscanner 0x0003 0xVVVV 0xPPPP 0x0000 0x0000 0x00 0x00 0x00 0x00 "
     "0x00 0x00 0x00000000\n"
     "# usb module match_flags idVendor idProduct bcdDevice_lo bcdDevice_hi "
     "bDeviceClass bDeviceSubClass bDeviceProtocol bInterfaceClass "
     "bInterfaceSubClass bInterfaceProtocol driver_info\n"
     "#\n"
     );

  printf
     ("# If your scanner isn't listed below, you can add it as explained above.\n"
     "#\n"
     "# If your scanner is supported by some external backend (brother, epkowa,\n"
     "# hpaio, etc) please ask the author of the backend to provide proper\n"
     "# device detection support for your OS\n"
     "#\n"
     "# If the scanner is supported by sane-backends, please mail the entry to\n"
     "# the sane-devel mailing list (sane-devel@lists.alioth.debian.org).\n"
     "#\n"
     );

}

static void
print_usermap (void)
{
  usbid_type *usbid = create_usbids_table ();

  print_usermap_header ();
  while (usbid)
    {
      manufacturer_model_type * name = usbid->name;

      printf ("# ");
      while (name)
	{
	  if (name != usbid->name)
	    printf (" | ");
	  printf ("%s", name->name);
	  name = name->next;
	}
      printf ("\n");
      printf ("libusbscanner 0x0003 %s %s ", usbid->usb_vendor_id,
	      usbid->usb_product_id);
      printf ("0x0000 0x0000 0x00 0x00 0x00 0x00 0x00 0x00 0x00000000\n");
      usbid = usbid->next;
    }
}

/* print libsane.db file for hotplug-ng */
static void
print_db_header (void)
{
  time_t current_time = time (0);
  printf ("# This file was automatically created based on description files (*.desc)\n"
	  "# by sane-desc %s from %s on %s",
	  SANE_DESC_VERSION, PACKAGE_STRING, asctime (localtime (&current_time)));
  printf
    ("#\n"
     "# The entries below are used to detect a USB device when it's plugged in\n"
     "# and then run a script to change the ownership and\n"
     "# permissions on the \"device node\" used by libusb.\n"
     "# Sample entry (replace 0xVVVV and 0xPPPP with vendor ID and product ID\n"
     "# respectively):\n");
  printf
    ("#\n"
     "# 0xVVVV<tab>0xPPPP<tab>%s:%s<tab>%s<tab>[/usr/local/bin/foo.sh]\n"
     "# Fields:\n"
     "#   vendor ID\n"
     "#   product ID\n"
     "#   ownership (user:group)\n"
     "#   permissions\n"
     "#   path of an optional script to run (it can be omitted)\n"
     "#\n"
     , DEVOWNER, DEVGROUP, DEVMODE);

  printf
     ("# If your scanner isn't listed below, you can add it as explained above.\n"
     "#\n"
     "# If your scanner is supported by some external backend (brother, epkowa,\n"
     "# hpaio, etc) please ask the author of the backend to provide proper\n"
     "# device detection support for your OS\n"
     "#\n"
     "# If the scanner is supported by sane-backends, please mail the entry to\n"
     "# the sane-devel mailing list (sane-devel@lists.alioth.debian.org).\n"
     "#\n"
     );
}

static void
print_db (void)
{
  usbid_type *usbid = create_usbids_table ();

  print_db_header ();
  while (usbid)
    {
      manufacturer_model_type * name = usbid->name;

      printf ("# ");
      while (name)
	{
	  if (name != usbid->name)
	    printf (" | ");
	  printf ("%s", name->name);
	  name = name->next;
	}
      printf ("\n");
      printf ("%s\t%s\t%s:%s\t%s\t\n", usbid->usb_vendor_id,
	      usbid->usb_product_id, DEVOWNER, DEVGROUP, DEVMODE);
      usbid = usbid->next;
    }
}

/* print libsane.rules for Linux udev */
static void
print_udev_header (void)
{
  time_t current_time = time (0);
  printf ("# This file was automatically created based on description files (*.desc)\n"
	  "# by sane-desc %s from %s on %s",
	  SANE_DESC_VERSION, PACKAGE_STRING, asctime (localtime (&current_time)));

  printf
    ("#\n"
     "# udev rules file for supported USB and SCSI devices\n"
     "#\n"
     "# The SCSI device support is very basic and includes only\n"
     "# scanners that mark themselves as type \"scanner\" or\n"
     "# SCSI-scanners from HP and other vendors that are entitled \"processor\"\n"
     "# but are treated accordingly.\n"
     "#\n");
  printf
    ("# To add a USB device, add a rule to the list below between the\n"
     "# LABEL=\"libsane_usb_rules_begin\" and LABEL=\"libsane_usb_rules_end\" lines.\n"
     "#\n"
     "# To run a script when your device is plugged in, add RUN+=\"/path/to/script\"\n"
     "# to the appropriate rule.\n"
     "#\n"
     );
  printf
     ("# If your scanner isn't listed below, you can add it as explained above.\n"
     "#\n"
     "# If your scanner is supported by some external backend (brother, epkowa,\n"
     "# hpaio, etc) please ask the author of the backend to provide proper\n"
     "# device detection support for your OS\n"
     "#\n"
     "# If the scanner is supported by sane-backends, please mail the entry to\n"
     "# the sane-devel mailing list (sane-devel@lists.alioth.debian.org).\n"
     "#\n"
     );
}

static void
print_udev (void)
{
  usbid_type *usbid = create_usbids_table ();
  scsiid_type *scsiid = create_scsiids_table ();
  int i;

  print_udev_header ();
  printf("ACTION!=\"add\", GOTO=\"libsane_rules_end\"\n"
	 "ENV{DEVTYPE}==\"usb_device\", GOTO=\"libsane_create_usb_dev\"\n"
	 "SUBSYSTEMS==\"scsi\", GOTO=\"libsane_scsi_rules_begin\"\n"
	 "SUBSYSTEM==\"usb_device\", GOTO=\"libsane_usb_rules_begin\"\n"
	 "SUBSYSTEM!=\"usb_device\", GOTO=\"libsane_usb_rules_end\"\n"
	 "\n");

  printf("# Kernel >= 2.6.22 jumps here\n"
	 "LABEL=\"libsane_create_usb_dev\"\n"
	 "\n");

  printf("# For Linux >= 2.6.22 without CONFIG_USB_DEVICE_CLASS=y\n"
	 "# If the following rule does not exist on your system yet, uncomment it\n"
	 "# ENV{DEVTYPE}==\"usb_device\", "
	 "MODE=\"0664\", OWNER=\"root\", GROUP=\"root\"\n"
	 "\n");

  printf("# Kernel < 2.6.22 jumps here\n"
	 "LABEL=\"libsane_usb_rules_begin\"\n"
	 "\n");

  while (usbid)
    {
      manufacturer_model_type * name = usbid->name;

      i = 0;
      printf ("# ");
      while (name)
	{
	  if ((name != usbid->name) && (i > 0))
	    printf (" | ");
	  printf ("%s", name->name);
	  name = name->next;

	  i++;

	  /*
	   * Limit the number of model names on the same line to 3,
	   * as udev cannot handle very long lines and prints a warning
	   * message while loading the rules files.
	   */
	  if ((i == 3) && (name != NULL))
	    {
	      printf("\n# ");
	      i = 0;
	    }
	}
      printf ("\n");

      if (mode == output_mode_udevacl)
	printf ("ATTRS{idVendor}==\"%s\", ATTRS{idProduct}==\"%s\", ENV{libsane_matched}=\"yes\"\n",
		usbid->usb_vendor_id + 2,  usbid->usb_product_id + 2);
      else
	printf ("ATTRS{idVendor}==\"%s\", ATTRS{idProduct}==\"%s\", MODE=\"%s\", GROUP=\"%s\", ENV{libsane_matched}=\"yes\"\n",
		usbid->usb_vendor_id + 2,  usbid->usb_product_id + 2, DEVMODE, DEVGROUP);

      usbid = usbid->next;
    }

  printf("\n# The following rule will disable USB autosuspend for the device\n");
  printf("ENV{libsane_matched}==\"yes\", RUN+=\"/bin/sh -c 'if test -e /sys/$env{DEVPATH}/power/control; then echo on > /sys/$env{DEVPATH}/power/control; elif test -e /sys/$env{DEVPATH}/power/level; then echo on > /sys/$env{DEVPATH}/power/level; fi'\"\n");

  printf ("\nLABEL=\"libsane_usb_rules_end\"\n\n");

  printf ("SUBSYSTEMS==\"scsi\", GOTO=\"libsane_scsi_rules_begin\"\n");
  printf ("GOTO=\"libsane_scsi_rules_end\"\n\n");
  printf ("LABEL=\"libsane_scsi_rules_begin\"\n");
  printf ("# Generic: SCSI device type 6 indicates a scanner\n");

  if (mode == output_mode_udevacl)
    printf ("KERNEL==\"sg[0-9]*\", ATTRS{type}==\"6\", ENV{libsane_matched}=\"yes\"\n");
  else
    printf ("KERNEL==\"sg[0-9]*\", ATTRS{type}==\"6\", MODE=\"%s\", GROUP=\"%s\", ENV{libsane_matched}=\"yes\"\n", DEVMODE, DEVGROUP);


  printf ("# Some scanners advertise themselves as SCSI device type 3\n");

  printf ("# Wildcard: for some Epson SCSI scanners\n");
  if (mode == output_mode_udevacl)
    printf ("KERNEL==\"sg[0-9]*\", ATTRS{type}==\"3\", ATTRS{vendor}==\"EPSON\", ATTRS{model}==\"SCANNER*\", ENV{libsane_matched}=\"yes\"\n");
  else
    printf ("KERNEL==\"sg[0-9]*\", ATTRS{type}==\"3\", ATTRS{vendor}==\"EPSON\", ATTRS{model}==\"SCANNER*\", MODE=\"%s\", GROUP=\"%s\", ENV{libsane_matched}=\"yes\"\n",
	    DEVMODE, DEVGROUP);

  while (scsiid)
    {
      manufacturer_model_type * name = scsiid->name;

      if (!scsiid->is_processor)
	{
	  scsiid = scsiid->next;
	  continue;
	}

      /* Wildcard for Epson scanners: vendor = EPSON, product = SCANNER* */
      if ((strcmp(scsiid->scsi_vendor_id, "EPSON") == 0)
	  && (strncmp(scsiid->scsi_product_id, "SCANNER", 7) == 0))
	{
	  scsiid = scsiid->next;
	  continue;
	}

      i = 0;
      printf ("# ");
      while (name)
        {
          if ((name != scsiid->name) && (i > 0))
            printf (" | ");
          printf ("%s", name->name);
          name = name->next;

	  i++;

	  /*
	   * Limit the number of model names on the same line to 3,
	   * as udev cannot handle very long lines and prints a warning
	   * message while loading the rules files.
	   */
	  if ((i == 3) && (name != NULL))
	    {
	      printf("\n# ");
	      i = 0;
	    }
        }
      printf ("\n");

      if (mode == output_mode_udevacl)
	printf ("KERNEL==\"sg[0-9]*\", ATTRS{type}==\"3\", ATTRS{vendor}==\"%s\", ATTRS{model}==\"%s\", ENV{libsane_matched}=\"yes\"\n",
		scsiid->scsi_vendor_id, scsiid->scsi_product_id);
      else
	printf ("KERNEL==\"sg[0-9]*\", ATTRS{type}==\"3\", ATTRS{vendor}==\"%s\", ATTRS{model}==\"%s\", MODE=\"%s\", GROUP=\"%s\", ENV{libsane_matched}=\"yes\"\n",
	      scsiid->scsi_vendor_id, scsiid->scsi_product_id, DEVMODE, DEVGROUP);

      scsiid = scsiid->next;
    }
  printf ("LABEL=\"libsane_scsi_rules_end\"\n");

  if (mode == output_mode_udevacl)
    printf("\nENV{libsane_matched}==\"yes\", RUN+=\"/bin/setfacl -m g:%s:rw $env{DEVNAME}\"\n", DEVGROUP);
  else
    printf ("\nENV{libsane_matched}==\"yes\", MODE=\"664\", GROUP=\"scanner\"\n");

  printf ("\nLABEL=\"libsane_rules_end\"\n");
}


/* print libsane.rules for Linux udev */
static void
print_udevhwdb_header (void)
{
  time_t current_time = time (0);
  printf ("# This file was automatically created based on description files (*.desc)\n"
	  "# by sane-desc %s from %s on %s",
	  SANE_DESC_VERSION, PACKAGE_STRING, asctime (localtime (&current_time)));

  printf
    ("#\n"
     "# udev rules file for supported USB and SCSI devices\n"
     "#\n"
     "# For the list of supported USB devices see /usr/lib/udev/hwdb.d/20-sane.hwdb\n"
     "#\n"
     "# The SCSI device support is very basic and includes only\n"
     "# scanners that mark themselves as type \"scanner\" or\n"
     "# SCSI-scanners from HP and other vendors that are entitled \"processor\"\n"
     "# but are treated accordingly.\n"
     "#\n");
  printf
     ("# If your SCSI scanner isn't listed below, you can add it to a new rules\n"
     "# file under /etc/udev/rules.d/.\n"
     "#\n"
     "# If your scanner is supported by some external backend (brother, epkowa,\n"
     "# hpaio, etc) please ask the author of the backend to provide proper\n"
     "# device detection support for your OS\n"
     "#\n"
     "# If the scanner is supported by sane-backends, please mail the entry to\n"
     "# the sane-devel mailing list (sane-devel@lists.alioth.debian.org).\n"
     "#\n"
     );
}

static void
print_udevhwdb (void)
{
  scsiid_type *scsiid = create_scsiids_table ();
  int i;

  print_udevhwdb_header ();
  printf("ACTION!=\"add\", GOTO=\"libsane_rules_end\"\n\n");

  printf("# The following rule will disable USB autosuspend for the device\n");
  printf("ENV{DEVTYPE}==\"usb_device\", ENV{libsane_matched}==\"yes\", TEST==\"power/control\", ATTR{power/control}=\"on\"\n\n");

  printf ("SUBSYSTEMS==\"scsi\", GOTO=\"libsane_scsi_rules_begin\"\n");
  printf ("GOTO=\"libsane_rules_end\"\n\n");
  printf ("LABEL=\"libsane_scsi_rules_begin\"\n");
  printf ("KERNEL!=\"sg[0-9]*\", GOTO=\"libsane_rules_end\"\n\n");

  printf ("# Generic: SCSI device type 6 indicates a scanner\n");
  printf ("ATTRS{type}==\"6\", ENV{libsane_matched}=\"yes\"\n\n");

  printf ("# Some scanners advertise themselves as SCSI device type 3\n\n");

  printf ("# Wildcard: for some Epson SCSI scanners\n");
  printf ("ATTRS{type}==\"3\", ATTRS{vendor}==\"EPSON\", ATTRS{model}==\"SCANNER*\", ENV{libsane_matched}=\"yes\"\n\n");

  while (scsiid)
    {
      manufacturer_model_type * name = scsiid->name;

      if (!scsiid->is_processor)
	{
	  scsiid = scsiid->next;
	  continue;
	}

      /* Wildcard for Epson scanners: vendor = EPSON, product = SCANNER* */
      if ((strcmp(scsiid->scsi_vendor_id, "EPSON") == 0)
	  && (strncmp(scsiid->scsi_product_id, "SCANNER", 7) == 0))
	{
	  scsiid = scsiid->next;
	  continue;
	}

      i = 0;
      printf ("# ");
      while (name)
        {
          if ((name != scsiid->name) && (i > 0))
            printf (" | ");
          printf ("%s", name->name);
          name = name->next;

	  i++;

	  /*
	   * Limit the number of model names on the same line to 3,
	   * as udev cannot handle very long lines and prints a warning
	   * message while loading the rules files.
	   */
	  if ((i == 3) && (name != NULL))
	    {
	      printf("\n# ");
	      i = 0;
	    }
        }
      printf ("\n");

      printf ("ATTRS{type}==\"3\", ATTRS{vendor}==\"%s\", ATTRS{model}==\"%s\", ENV{libsane_matched}=\"yes\"\n\n",
		scsiid->scsi_vendor_id, scsiid->scsi_product_id);

      scsiid = scsiid->next;
    }

  printf ("\nLABEL=\"libsane_rules_end\"\n");
}

/* print /usr/lib/udev/hwdb.d/20-sane.conf for Linux hwdb */
static void
print_hwdb_header (void)
{
  time_t current_time = time (0);
  printf ("# This file was automatically created based on description files (*.desc)\n"
	  "# by sane-desc %s from %s on %s",
	  SANE_DESC_VERSION, PACKAGE_STRING, asctime (localtime (&current_time)));

  printf
    ("#\n"
     "# hwdb file for supported USB devices\n"
     "#\n");
  printf
     ("# If your scanner isn't listed below, you can add it to a new hwdb file\n"
     "# under /etc/udev/hwdb.d/.\n"
     "#\n"
     "# If your scanner is supported by some external backend (brother, epkowa,\n"
     "# hpaio, etc) please ask the author of the backend to provide proper\n"
     "# device detection support for your OS\n"
     "#\n"
     "# If the scanner is supported by sane-backends, please mail the entry to\n"
     "# the sane-devel mailing list (sane-devel@lists.alioth.debian.org).\n"
     "#\n"
     );
}

static void
print_hwdb (void)
{
  usbid_type *usbid = create_usbids_table ();
  char *vendor_id;
  char *product_id;
  int i,j;

  print_hwdb_header ();

  while (usbid)
    {
      manufacturer_model_type * name = usbid->name;

      i = 0;
      printf ("# ");
      while (name)
	{
	  if ((name != usbid->name) && (i > 0))
	    printf (" | ");
	  printf ("%s", name->name);
	  name = name->next;

	  i++;

	  /*
	   * Limit the number of model names on the same line to 3,
	   * as udev cannot handle very long lines and prints a warning
	   * message while loading the rules files.
	   */
	  if ((i == 3) && (name != NULL))
	    {
	      printf("\n# ");
	      i = 0;
	    }
	}
      printf ("\n");

      vendor_id = strdup(usbid->usb_vendor_id + 2);
      product_id = strdup(usbid->usb_product_id + 2);

      for(j = 0; j < 4; j++) {
        vendor_id[j] = toupper(vendor_id[j]);
        product_id[j] = toupper(product_id[j]);
      }

      printf ("usb:v%sp%s*\n libsane_matched=yes\n\n",
		vendor_id, product_id);

      free(vendor_id);
      free(product_id);

      usbid = usbid->next;
    }
}

static void
print_plist (void)
{
  usbid_type *usbid = create_usbids_table ();

  printf ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  printf ("<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n");
  printf ("<plist version=\"1.0\">\n");
  printf ("<dict>\n");
  printf ("\t<key>device info version</key>\n");
  printf ("\t<string>2.0</string>\n");
  printf ("\t<key>usb</key>\n");
  printf ("\t<dict>\n");
  printf ("\t\t<key>IOUSBDevice</key>\n");
  printf ("\t\t<array>\n");
  while (usbid)
    {
      printf ("\t\t\t<dict>\n");
      printf ("\t\t\t\t<key>device type</key>\n");
      printf ("\t\t\t\t<string>scanner</string>\n");
      printf ("\t\t\t\t<key>product</key>\n");
      printf ("\t\t\t\t<string>%s</string>\n", usbid->usb_product_id);
      printf ("\t\t\t\t<key>vendor</key>\n");
      printf ("\t\t\t\t<string>%s</string>\n", usbid->usb_vendor_id);
      printf ("\t\t\t</dict>\n");
      usbid = usbid->next;
    }
  printf ("\t\t</array>\n");
  printf ("\t</dict>\n");
  printf ("</dict>\n");
  printf ("</plist>\n");
}


static void
print_hal (int new)
{
  int i;
  SANE_Bool in_match;
  char *last_vendor;
  scsiid_type *scsiid = create_scsiids_table ();
  usbid_type *usbid = create_usbids_table ();

  printf ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  printf ("<deviceinfo version=\"0.2\">\n");
  printf ("  <device>\n");
  printf ("    <!-- SCSI-SUBSYSTEM -->\n");
  printf ("    <match key=\"info.category\" string=\"scsi_generic\">\n");
  printf ("      <!-- Some SCSI Scanners announce themselves \"processor\" -->\n");
  printf ("      <match key=\"@info.parent:scsi.type\" string=\"processor\">\n");

  last_vendor = "";
  in_match = SANE_FALSE;
  while (scsiid)
    {
      manufacturer_model_type * name = scsiid->name;

      if (!scsiid->is_processor)
	{
	  scsiid = scsiid->next;
	  continue;
	}

      if (strcmp(last_vendor, scsiid->scsi_vendor_id) != 0)
	{
	  if (in_match)
	    printf ("        </match>\n");

	  printf ("        <match key=\"@info.parent:scsi.vendor\" string=\"%s\">\n", scsiid->scsi_vendor_id);
	  last_vendor = scsiid->scsi_vendor_id;
	  in_match = SANE_TRUE;
	}

      printf ("          <!-- SCSI Scanner ");
      while (name)
	{
	  if (name != scsiid->name)
	    printf (" | ");
	  printf ("\"%s\"", name->name);
	  name = name->next;
	}
      printf (" -->\n");
      printf ("          <match key=\"@info.parent:scsi.model\" string=\"%s\">\n", scsiid->scsi_product_id);
      printf ("            <append key=\"info.capabilities\" type=\"strlist\">scanner</append>\n");
      printf ("          </match>\n");

      scsiid = scsiid->next;
    }

  if (in_match)
    printf ("        </match>\n");

  printf ("      </match>\n");
  printf ("    </match>\n");
  printf ("    <!-- USB-SUBSYSTEM -->\n");

  if (new)
    printf ("    <match key=\"info.subsystem\" string=\"usb\">\n");
  else
    printf ("    <match key=\"info.bus\" string=\"usb\">\n");

  last_vendor = "";
  in_match = SANE_FALSE;
  while (usbid)
    {
      manufacturer_model_type * name = usbid->name;

      if (strcmp(last_vendor, usbid->usb_vendor_id) != 0)
	{
	  if (in_match)
	    printf ("      </match>\n");

	  printf ("      <match key=\"usb.vendor_id\" int=\"%s\">\n", usbid->usb_vendor_id);
	  last_vendor = usbid->usb_vendor_id;
	  in_match = SANE_TRUE;
	}

      i = 0;
      printf ("        <!-- ");
      while (name)
	{
	  if ((name != usbid->name) && (i > 0))
	    printf (" | ");

	  printf ("%s", name->name);
	  name = name->next;
	  i++;

	  if ((i == 3) && (name != NULL))
	    {
	      printf("\n             ");
	      i = 0;
	    }
	}
      printf (" -->\n");
      printf ("        <match key=\"usb.product_id\" int=\"%s\">\n", usbid->usb_product_id);
      printf ("          <append key=\"info.capabilities\" type=\"strlist\">scanner</append>\n");
      printf ("          <merge key=\"scanner.access_method\" type=\"string\">proprietary</merge>\n");
      printf ("        </match>\n");

      usbid = usbid->next;
    }

  if (in_match)
    printf ("      </match>\n");

  printf ("    </match>\n");

  printf ("  </device>\n");
  printf ("</deviceinfo>\n");
}

int
main (int argc, char **argv)
{
  program_name = strrchr (argv[0], '/');
  if (program_name)
    ++program_name;
  else
    program_name = argv[0];

  if (!get_options (argc, argv))
    return 1;
  if (!read_files ())
    return 1;
  switch (mode)
    {
    case output_mode_ascii:
      ascii_print_backends ();
      break;
    case output_mode_xml:
      xml_print_backends ();
      break;
    case output_mode_html_backends_split:
      html_print_backends_split ();
      break;
    case output_mode_html_mfgs:
      html_print_mfgs ();
      break;
    case output_mode_statistics:
      print_statistics ();
      break;
    case output_mode_usermap:
      print_usermap ();
      break;
    case output_mode_db:
      print_db ();
      break;
    case output_mode_udev:
    case output_mode_udevacl:
      print_udev ();
      break;
    case output_mode_udevhwdb:
      print_udevhwdb ();
      break;
    case output_mode_hwdb:
      print_hwdb ();
      break;
    case output_mode_plist:
      print_plist ();
      break;
    case output_mode_hal:
      print_hal (0);
      break;
    case output_mode_halnew:
      print_hal (1);
      break;
    default:
      DBG_ERR ("Unknown output mode\n");
      return 1;
    }

  return 0;
}
