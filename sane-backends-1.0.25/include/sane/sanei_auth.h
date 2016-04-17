/* sane - Scanner Access Now Easy.
   Copyright (C) 2000 Jochen Eisinger <jochen.eisinger@gmx.net>
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

/** @file sanei_auth.h
 * Interface for authorization of resources
 *
 * This file implements an interface for user authorization. The authorization
 * call is forwarded to the frontend which asks for a username and password.
 * An MD5 digest is used if supported by the frontend.
 *
 * @sa sanei.h sanei_backend.h
 */

#ifndef sanei_auth_h
#define sanei_auth_h

#include "../include/sane/sane.h"

/** Check authorization for a resource
 *
 * This function looks for the file SANE_CONFIG_DIR/backend.users.
 * If this file doesn't exist, sanei_authorize always returns SANE_STATUS_GOOD.
 * The file backend.users contains a list of usernames, passwords, and
 * resources:
 *
 * username:password:resource
 * username:password:resource
 *
 * If the requested resource isn't listed in this file, sanei_authorize
 * return SANE_SATUS_GOOD. In all other cases, sanei_authorize sends a
 * challenge to the frontend of the form
 * 
 * resource$MD5$randomstring
 * 
 * where randomstring consists of the PID, the time, and some random
 * characters. It accepts two forms of answers
 *
 * std: username:password
 * md5: username:$MD5$m5digest
 *
 * where md5digest is md5(randomstring password).
 * 
 * If this username/password/resource triple is listed in backend.users
 * sanei_authorize returns SANE_STATUS_GOOD, in all other cases it returns
 * SANE_STATUS_ACCESS_DENIED.
 *
 * @param resource resource to authorize
 * @param backend backend name
 * @param authorize auth callback
 *
 * @return
 * - SANE_STATUS_GOOD - access is granted
 * - SANE_STATUS_ACCESS_DENIED - access is denied
 */

SANE_Status
sanei_authorize (const char *resource,
		 const char *backend, SANE_Auth_Callback authorize);

#endif /* sanei_auth_h */
