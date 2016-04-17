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
/*
#define STUBS
extern int sanei_debug_hp;*/
#define DEBUG_DECLARE_ONLY
#include "../include/sane/config.h"

#include <stdlib.h>
#include <string.h>
#include "../include/lassert.h"

#include "hp.h"

typedef struct hp_alloc_s  Alloc;

struct hp_alloc_s
{
    Alloc *	prev;
    Alloc *	next;
    hp_byte_t	buf[1];
};

static Alloc  head[] = {{ head, head, {0} }};

#define DATA_OFFSET	  (head->buf - (hp_byte_t *)head)
#define VOID_TO_ALLOCP(p) ((Alloc *)((hp_byte_t *)(p) - DATA_OFFSET))
#define ALLOCSIZE(sz) 	  (sz + DATA_OFFSET)


void *
sanei_hp_alloc (size_t sz)
{
  Alloc * new = malloc(ALLOCSIZE(sz));

  if (!new)
      return 0;
  (new->next = head->next)->prev = new;
  (new->prev = head)->next = new;
  return new->buf;
}

void *
sanei_hp_allocz (size_t sz)
{
  void * new = sanei_hp_alloc(sz);

  if (!new)
      return 0;
  memset(new, 0, sz);
  return new;
}

void *
sanei_hp_memdup (const void * src, size_t sz)
{
  char * new = sanei_hp_alloc(sz);
  if (!new)
      return 0;
  return memcpy(new, src, sz);
}

char *
sanei_hp_strdup (const char * str)
{
  return sanei_hp_memdup(str, strlen(str) + 1);
}

void *
sanei_hp_realloc (void * ptr, size_t sz)
{
  if (ptr)
    {
      Alloc * old = VOID_TO_ALLOCP(ptr);
      Alloc  copy = *old;
      Alloc * new = realloc(old, ALLOCSIZE(sz));
      if (!new)
	  return 0;
      if (new != old)
	  (new->prev = copy.prev)->next = (new->next = copy.next)->prev = new;
      return new->buf;
    }
  else
      return sanei_hp_alloc(sz);
}

void
sanei_hp_free (void * ptr)
{
  Alloc * old = VOID_TO_ALLOCP(ptr);

  assert(old && old != head);
  (old->next->prev = old->prev)->next = old->next;
  old->next = old->prev = 0;	/* so we can puke on multiple free's */
  free(old);
}

void
sanei_hp_free_all (void)
{
  Alloc * ptr;
  Alloc * next;

  for (ptr = head->next; ptr != head; ptr = next)
    {
      next = ptr->next;
      free(ptr);
    }
  head->next = head->prev = head;
}
