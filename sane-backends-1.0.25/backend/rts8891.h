/* sane - Scanner Access Now Easy.

   Copyright (C) 2007-2012 stef.dev@free.fr
   
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

#ifndef RTS8891_H
#define RTS8891_H

#define ENABLE(OPTION)  s->opt[OPTION].cap &= ~SANE_CAP_INACTIVE
#define DISABLE(OPTION) s->opt[OPTION].cap |=  SANE_CAP_INACTIVE
#define IS_ACTIVE(OPTION) (((s->opt[OPTION].cap) & SANE_CAP_INACTIVE) == 0)

#define RTS8891_CONFIG_FILE "rts8891.conf"

#ifndef SANE_I18N
#define SANE_I18N(text) text
#endif /*  */

#define COLOR_MODE              SANE_VALUE_SCAN_MODE_COLOR
#define GRAY_MODE               SANE_VALUE_SCAN_MODE_GRAY
#define LINEART_MODE            SANE_VALUE_SCAN_MODE_LINEART

/* preferred number of bytes to keep in buffer */
#define PREFERED_BUFFER_SIZE 2097152	/* all scanner memory */

/** List of SANE options
 */
enum Rts8891_Option
{ OPT_NUM_OPTS = 0,
  OPT_STANDARD_GROUP,
  OPT_MODE,
  OPT_PREVIEW,
  OPT_RESOLUTION,

  OPT_GEOMETRY_GROUP,
  OPT_TL_X,			/* top-left x */
  OPT_TL_Y,			/* top-left y */
  OPT_BR_X,			/* bottom-right x */
  OPT_BR_Y,			/* bottom-right y */

  /* advanced image enhancement options */
  OPT_ENHANCEMENT_GROUP,
  OPT_THRESHOLD,
  OPT_CUSTOM_GAMMA,		/* toggle to enable custom gamma tables */
  OPT_GAMMA_VECTOR,
  OPT_GAMMA_VECTOR_R,
  OPT_GAMMA_VECTOR_G,
  OPT_GAMMA_VECTOR_B,

  /* advanced options */
  OPT_ADVANCED_GROUP,
  OPT_LAMP_ON,
  OPT_LAMP_OFF,

  /* button group */
  OPT_SENSOR_GROUP,
  OPT_BUTTON_1,
  OPT_BUTTON_2,
  OPT_BUTTON_3,
  OPT_BUTTON_4,
  OPT_BUTTON_5,
  OPT_BUTTON_6,
  OPT_BUTTON_7,
  OPT_BUTTON_8,
  OPT_BUTTON_9,
  OPT_BUTTON_10,
  OPT_BUTTON_11,
  /* must come last: */
  NUM_OPTIONS
};

/**
 * enumeration of configuration options
 */
enum Rts8891_Configure_Option
{
  CFG_MODEL_NUMBER = 0,		/* first option number must be zero */
  CFG_SENSOR_NUMBER,		
  CFG_ALLOW_SHARING,
  NUM_CFG_OPTIONS		/* MUST be last */
};

/** Scanner object. This struct holds informations usefull for
 * the functions defined in SANE's standard. Informations closer 
 * to the hardware are in the Rts8891_Device structure. There is
 * as many session structure than frontends using the scanner.
 */
typedef struct Rts8891_Session
{

  /**< Next handle in linked list */
  struct Rts8891_Session *next;

  /**< Low-level device object */
  struct Rts8891_Device *dev;

  /* SANE data */

  /**< We are currently scanning */
  SANE_Bool scanning;
  /**< Data read is in non blocking mode */
  SANE_Bool non_blocking;
  /**< Gray scans are emulated */
  SANE_Bool emulated_gray;
  SANE_Option_Descriptor opt[NUM_OPTIONS];
					   /**< Option descriptors */
  Option_Value val[NUM_OPTIONS];	   /**< Option values */
  SANE_Parameters params;		   /**< SANE Parameters */

   /**< bytes to send to frontend for the scan */
  SANE_Int to_send;

   /**< bytes currently sent to frontend during the scan */
  SANE_Int sent;
} Rts8891_Session;

#endif /* not RTS8891_H */
