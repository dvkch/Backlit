/* sane - Scanner Access Now Easy.
   Copyright (C) 1997 Geoffrey T. Dairiki
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

   This file is part of a SANE backend for HP Scanners supporting
   HP Scanner Control Language (SCL).
*/

#ifndef HP_H_INCLUDED
#define HP_H_INCLUDED
#include <limits.h>
#include <sys/types.h>
#include "../include/sane/sane.h"

#undef BACKEND_NAME
#define BACKEND_NAME	hp
#define DEBUG_NOT_STATIC
#include "../include/sane/sanei_debug.h"

#ifdef __GNUC__
#define UNUSEDARG __attribute__ ((unused))
#else
#define UNUSEDARG
#endif

/* FIXME: these should be options? */
#undef ENABLE_7x12_TONEMAPS
#define ENABLE_16x16_DITHERS
#define ENABLE_10BIT_MATRIXES

#define FAKE_COLORSEP_MATRIXES

#undef ENABLE_CUSTOM_MATRIX

#define HP_CONFIG_FILE 	 STRINGIFY(BACKEND_NAME) ".conf"

#define DEVPIX_PER_INCH		300.0
#define MM_PER_DEVPIX		(MM_PER_INCH / DEVPIX_PER_INCH)

/*
 * Macros for testing return values of functions which
 * return SANE_Status types.
 */
#define FAILED(status)		(status != SANE_STATUS_GOOD)
#define UNSUPPORTED(status)	(status == SANE_STATUS_UNSUPPORTED)
#define RETURN_IF_FAIL(try)	do {					\
  SANE_Status status = (try);						\
  if (FAILED(status))							\
      return status;							\
} while (0)
#define CATCH_RET_FAIL(try, catch) do {					\
  SANE_Status status = (try);						\
  if (FAILED(status)) { (catch); return status; }			\
} while (0)

#ifndef DBG_LEVEL
#define DBG_LEVEL	PASTE(sanei_debug_, BACKEND_NAME)
#endif
#ifndef NDEBUG
# define DBGDUMP(level, buf, size) \
    do { if (DBG_LEVEL >= (level)) sanei_hp_dbgdump(buf, size); } while (0)
#else
# define DBGDUMP(level, buf, size)
#endif


 /*
  *
  */

typedef unsigned int	hp_bool_t;
typedef unsigned char	hp_byte_t;

typedef enum { HP_CONNECT_SCSI, HP_CONNECT_DEVICE,
               HP_CONNECT_PIO, HP_CONNECT_USB, HP_CONNECT_RESERVE } HpConnect;

typedef struct
{
  HpConnect connect;
  hp_bool_t got_connect_type;
  hp_bool_t use_scsi_request;
  hp_bool_t use_image_buffering;
  hp_bool_t dumb_read;
} HpDeviceConfig;

#define HP_SCL_INQID_MIN   10306
#define HP_SCL_INQID_MAX   10971

typedef struct
{
  hp_bool_t checked, is_supported;
  int minval, maxval;
} HpSclSupport;

typedef struct
{                              /* Flags for simulated commands */
  hp_bool_t sclsimulate[HP_SCL_INQID_MAX - HP_SCL_INQID_MIN + 1];
  hp_bool_t gamma_simulate;
  unsigned char brightness_map[256]; /* Map to simulate brightness level */
  unsigned char contrast_map[256];   /* Map to simulate contrast level */
  unsigned char gamma_map[256];      /* Map to simulate custom gamma table */
} HpSimulate;

/* Information about a connected device */
typedef struct
{
  char devname[64];            /* unique device name */

  hp_bool_t config_is_up;      /* flag if config-struct is valid */
  HpDeviceConfig config;       /* device configuration*/
                               /* List of command support */
  HpSclSupport sclsupport[HP_SCL_INQID_MAX - HP_SCL_INQID_MIN + 1];

  HpSimulate simulate;         /* Info about simulations */

  hp_bool_t  unload_after_scan;
  int        active_xpa;
  int        max_model;
} HpDeviceInfo;

HpDeviceInfo *sanei_hp_device_info_get (const char *dev_name);

/* hp-scl.c */
#if INT_MAX > 30000
typedef	int HpScl;
#else
typedef long int HpScl;
#endif

void sanei_hp_init_openfd (void);

typedef struct
{
  int lines;
  int bytes_per_line;     /* as received from scanner */
  int bits_per_channel;
  hp_bool_t out8;         /* This flag is set and only set, when data with */
                          /* depths > 8 is to be mapped to 8 bit output. */
  hp_bool_t mirror_vertical;
  hp_bool_t invert;
  HpScl startscan;
} HpProcessData;

/* hp-option.c */
typedef SANE_Option_Descriptor *	HpSaneOption;

typedef const struct hp_choice_s *	HpChoice;
typedef struct hp_option_s *		HpOption;
typedef const struct hp_option_descriptor_s *	HpOptionDescriptor;
typedef struct hp_optset_s *		HpOptSet;

/* hp-accessor.c */
typedef struct hp_data_s *		HpData;
typedef struct hp_accessor_s *	HpAccessor;
typedef struct hp_accessor_vector_s *	HpAccessorVector;
typedef struct hp_accessor_choice_s *	HpAccessorChoice;

/* hp-device.c */
typedef struct hp_device_s *	HpDevice;
hp_bool_t sanei_hp_device_simulate_get (const char *devname, HpScl scl);
HpDevice sanei_hp_device_get (const char *dev_name);

/* hp-handle.c */
typedef struct hp_handle_s *	HpHandle;

/* hp-scsi.c */
typedef struct hp_scsi_s * 	HpScsi;

/* hp-scl.c */
hp_bool_t sanei_hp_is_active_xpa (HpScsi scsi);
int sanei_hp_get_max_model (HpScsi scsi);
int sanei_hp_is_flatbed_adf (HpScsi scsi);
HpConnect sanei_hp_get_connect (const char *devname);
HpConnect sanei_hp_scsi_get_connect (HpScsi this);

/* hp.c */
#ifndef NDEBUG
void sanei_hp_dbgdump (const void * bufp, size_t len);
#endif

/* hp-hpmem.c */
void *	sanei_hp_alloc(size_t sz);
void *	sanei_hp_allocz(size_t sz);
void *	sanei_hp_memdup(const void * src, size_t sz);
char *	sanei_hp_strdup(const char * str);
void *	sanei_hp_realloc(void * ptr, size_t sz);
void	sanei_hp_free(void * ptr);
void	sanei_hp_free_all(void);

#endif /* HP_H_INCLUDED */
