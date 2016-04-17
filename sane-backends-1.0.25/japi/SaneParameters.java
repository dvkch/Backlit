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
//	SaneParameters.java - Sane parameters.
//
//	Written: 10/14/97 - JSF
//

public class SaneParameters
{
	//
	//	Frame values:
	//
public static final int FRAME_GRAY = 0;	// band covering human visual range 
public static final int FRAME_RGB = 1;	// pixel-interleaved 
					//   red/green/blue bands 
public static final int FRAME_RED = 2;	// red band only 
public static final int FRAME_GREEN = 3;// green band only 
public static final int FRAME_BLUE = 4;	// blue band only 

	//
	//	Class members:
	//
public int format;
public boolean lastFrame;
public int bytesPerLine;
public int pixelsPerLine;
public int lines;
public int depth;
}
