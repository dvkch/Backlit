/* sane - Scanner Access Now Easy.
 
   Copyright (C) 1997, 1998 Franck Schnefra, Michel Roelofs,
   Emmanuel Blot, Mikko Tyolajarvi, David Mosberger-Tang, Wolfgang Goeller,
   Petter Reinholdtsen, Gary Plewa, Sebastien Sable, Oliver Schwartz
   and Kevin Charter
 
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
 
   This file is a component of the implementation of a backend for many
   of the AGFA SnapScan and Acer Vuego/Prisa flatbed scanners. */


/* $Id$
   SnapScan backend scan data sources */

#ifndef SNAPSCAN_SOURCES_H
#define SNAPSCAN_SOURCES_H

typedef struct source Source;

typedef SANE_Int (*SourceRemaining) (Source *ps);
typedef SANE_Int (*SourceBytesPerLine) (Source *ps);
typedef SANE_Int (*SourcePixelsPerLine) (Source *ps);
typedef SANE_Status (*SourceGet) (Source *ps, SANE_Byte *pbuf, SANE_Int *plen);
typedef SANE_Status (*SourceDone) (Source *ps);

#define SOURCE_GUTS \
    SnapScan_Scanner *pss;\
    SourceRemaining remaining;\
    SourceBytesPerLine bytesPerLine;\
    SourcePixelsPerLine pixelsPerLine;\
    SourceGet get;\
    SourceDone done

struct source
{
    SOURCE_GUTS;
};

static SANE_Status Source_init (Source *pself,
                                SnapScan_Scanner *pss,
                                SourceRemaining remaining,
                                SourceBytesPerLine bytesPerLine,
                                SourcePixelsPerLine pixelsPerLine,
                                SourceGet get,
                                SourceDone done);

/* base sources */

#endif

/*
 * $Log$
 * Revision 1.5  2001/12/17 22:51:50  oliverschwartz
 * Update to snapscan-20011212 (snapscan 1.4.3)
 *
 * Revision 1.5  2001/12/12 19:44:59  oliverschwartz
 * Clean up CVS log
 *
 * Revision 1.4  2001/09/18 15:01:07  oliverschwartz
 * - Read scanner id string again after firmware upload
 *   to indentify correct model
 * - Make firmware upload work for AGFA scanners
 * - Change copyright notice
 *
 * Revision 1.3  2001/03/17 22:53:21  sable
 * Applying Mikael Magnusson patch concerning Gamma correction
 * Support for 1212U_2
 *
 * Revision 1.2  2000/10/13 03:50:27  cbagwell
 * Updating to source from SANE 1.0.3.  Calling this versin 1.1
 * */
