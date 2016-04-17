/* REXX */
/* Convert backend-DLL-filenames according to 8.3 naming convention     */
/* necessary for DLLs on OS/2            (C) Franz Bakan  2004,2005     */
/*                                                                      */
/* This file is part of the SANE package.                               */
/*                                                                      */ 
/* This program is free software; you can redistribute it and/or        */
/* modify it under the terms of the GNU General Public License as       */
/* published by the Free Software Foundation; either version 2 of the   */
/* License, or (at your option) any later version.                      */
/*                                                                      */
/* This program is distributed in the hope that it will be useful, but  */
/* WITHOUT ANY WARRANTY; without even the implied warranty of           */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU    */
/* General Public License for more details.                             */
/*                                                                      */
/* You should have received a copy of the GNU General Public License    */
/* along with this program; if not, write to the Free Software          */
/* Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.            */


CALL RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
CALL SysLoadFuncs

rc = SysFileTree('\usr\lib\sane\libsane-*.dll',dlls,O)
DO i=1 TO dlls.0
  PARSE VALUE dlls.i WITH front 'libsane-' backend '.dll'
  IF length(backend) > 7 THEN
    COPY dlls.i front||LEFT(backend,2)||RIGHT(backend,5,' ')||'.dll'
  ELSE
     IF length(backend) > 0 THEN COPY dlls.i front||backend||'.dll'
  DEL dlls.i
END /* do */
