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
*/

/** @file sanei_config.h
 * Generic configuration support.
 *
 * Use the functions of this header file if you want to read and analyze
 * configuration files.
*/

#ifndef sanei_config_h
#define sanei_config_h 1

#include <stdio.h>
#include <sane/sane.h>

/** Search configuration file \a name along directory list and return file
 * pointer if such a file exists.  
 *
 * The following directory list is used:
 *	1st: SANE_CONFIG_DIR environment variable.
 *	2nd: PATH_SANE_CONFIG_DIR set during configuration.
 *	3rd: Current directory.
 * @param name filename with extension but without path (such as "mustek.conf")
 *
 * @return file pointer, or NULL if not found
 *
 */
extern FILE *sanei_config_open (const char *name);

/** Read a line from configuration file.
 *
 * Strips all unwanted chars.  Use this instead of fgets() to remove
 * line ending chars on all known platforms.
 *
 * @param str points to the buffer for the line
 * @param n size of the buffer
 * @param stream file pointer
 *
 * @return \a str on success and NULL on error
*/
extern char *sanei_config_read (char *str, int n, FILE *stream);

/** Remove all whitespace from the beginning of a string.
 *
 * @param str string
 *
 * @return string without leading whitespace
 *
 */
extern const char *sanei_config_skip_whitespace (const char *str);


/** Scan a string constant from a line of text and return a malloced copy
 * of it.
 *
 * It's the responsibility of the caller to free the returned string constant
 * at an appropriate time.  Whitespace in front of the string constant is
 * ignored.  Whitespace can be included in the string constant by enclosing it
 * in double-quotes.
 *
 * @param str line of text to scan for a string constant
 * @param string_const copy of the string constant
 *
 * @return a pointer to the position in str where the scan stopped
 */
extern const char *sanei_config_get_string (const char *str,
					    char **string_const);

/** Expand device name patterns into a list of devices.
 *
 * Apart from a normal device name (such as /dev/sdb), this function currently
 * supports SCSI device specifications of the form:
 *
 *	scsi VENDOR MODEL TYPE BUS CHANNEL ID LUN
 *
 * Where VENDOR is the desired vendor name.  MODEL is the desired model name.
 * TYPE is the desired device type.  All of these can be set to * to match
 * anything.  To include whitespace in these strings, enclose them in
 * double-quotes (").  BUS, ID, and LUN are the desired SCSI bus, id, and
 * logical-unit numbers.  These can be set to * or simply omitted to match
 * anything.
 *
 * @param name device name pattern
 * @param attach attach function
 */
extern void sanei_config_attach_matching_devices (const char *name,
						  SANE_Status (*attach)
						  (const char *dev));

/** this structure holds the description of configuration options. There is
 * a list for options and another for their values. 
 * These lists are used when the configuration file is
 * parsed. Read values are stored in Option_Value. Helpers functions are 
 * provided to access values easily */
typedef struct
{
  /** number of options */
  SANE_Int count;

  /** NULL terminated list of configuration option */
  SANE_Option_Descriptor **descriptors;

  /** values for the configuration options */
  void **values;

} SANEI_Config;

/** Parse configuration file, reading configuration options and trying to
 * attach devices found in file.
 *
 * The option are gathered in a single configuration structure. Each time
 * a line holds a value that is not an option, the attach function is called
 * with the name found and the configuration structure with it's current values.
 *
 * @param config_file name of the configuration file to read
 * @param config configuration structure to be filled during configuration
 *  	  parsing and passed to the attach callback function
 * @param config_attach attach with config callback function
 *
 * @return SANE_STATUS_GOOD if no errors
 *         SANE_STATUS_ACCESS_DENIED if configuration file can't be opened
 */
extern SANE_Status sanei_configure_attach (
  const char *config_file,
  SANEI_Config *config,
  SANE_Status (*config_attach)(SANEI_Config *config, const char *devname)
);

/** Return the list of config directories, extracted from the SANE_CONFIG_DIR
 * environment variable and the default paths.
 * @return a string containing the configuration paths, separated by the
 *         operating system's path separator
 */
extern const char *sanei_config_get_paths (void);

#endif	/* sanei_config_h */
