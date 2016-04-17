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
   If you do not wish that, delete this exception notice.  */

//
//	SaneOption.java - Sane option descriptor.
//
//	Written: 10/9/97 - JSF
//

public class SaneOption
    {
	//
	// 	Sane option types:
	//
    public static final int TYPE_BOOL = 0;
    public static final int TYPE_INT = 1;
    public static final int TYPE_FIXED = 2;
    public static final int TYPE_STRING = 3;
    public static final int TYPE_BUTTON = 4;
    public static final int TYPE_GROUP = 5;
	//
	//	Sane value units:
	//
    public static final int UNIT_NONE = 0;	// the value is unit-less 
					//   (e.g., # of scans) 
    public static final int UNIT_PIXEL = 1;	// value is number of pixels 
    public static final int UNIT_BIT = 2;	// value is number of bits 
    public static final int UNIT_MM = 3;	// value is millimeters 
    public static final int UNIT_DPI = 4;	// value is res. in dots/inch 
    public static final int UNIT_PERCENT = 5;// value is a percentage 
	//
	//	Option capabilities:
	//
    public static final int CAP_SOFT_SELECT = (1 << 0);
    public static final int CAP_HARD_SELECT = (1 << 1);
    public static final int CAP_SOFT_DETECT = (1 << 2);
    public static final int CAP_EMULATED = (1 << 3);
    public static final int CAP_AUTOMATIC = (1 << 4);
    public static final int CAP_INACTIVE = (1 << 5);
    public static final int CAP_ADVANCED = (1 << 6);
	//
	//	Constraints:
	//
    public static final int CONSTRAINT_NONE = 0;
    public static final int CONSTRAINT_RANGE = 1;
    public static final int CONSTRAINT_WORD_LIST = 2;
    public static final int CONSTRAINT_STRING_LIST = 3;
	//
	//	Actions for controlOption():
	//
    public static final int ACTION_GET_VALUE = 0;
    public static final int ACTION_SET_VALUE = 1;
    public static final int ACTION_SET_AUTO = 2;

	//
	//	Flags passed back in 'info' field param. of
	//	setControlOption():
	//
    public static final int INFO_INEXACT =		(1 << 0);
    public static final int INFO_RELOAD_OPTIONS	=	(1 << 1);
    public static final int INFO_RELOAD_PARAMS =	(1 << 2);

	//
	//	Class members:
	//
    public String name;		// name of this option (command-line name) 
    public String title;	// title of this option (single-line) 
    public String desc;		// description of this option (multi-line) 
    public int type;		// how are values interpreted? (TYPE_)
    public int unit;		// what is the (physical) unit? (UNIT_)
    public int size;
    public int cap;		// capabilities 
    public int constraintType;
				// These are a union in the "C" API:
				// Null-terminated list:
    public String[] stringListConstraint;
				// First element is list-length:
    public int[] wordListConstraint;
    public SaneRange rangeConstraint;
	//
	//	Public methods:
	//
    public boolean is_active()
	{ return ((cap) & CAP_INACTIVE) == 0; }
    public boolean is_settable()
	{ return ((cap) & CAP_SOFT_SELECT) == 0; }
				// Return string describing units.
				// "unitLength" is # mm. preferred.
    public String unitString(double unitLength)	
	{
	switch (unit)
		{
	case UNIT_NONE:		return "none";
	case UNIT_PIXEL:	return "pixel";
	case UNIT_BIT:		return "bit";
	case UNIT_DPI:		return "dpi";
	case UNIT_PERCENT:	return "%";
	case UNIT_MM:
	  	if (unitLength > 9.9 && unitLength < 10.1)
			return "cm";
		else if (unitLength > 25.3 && unitLength < 25.5)
			return "in";
	  	return "mm";
		}
  	return "";
	}
    }

