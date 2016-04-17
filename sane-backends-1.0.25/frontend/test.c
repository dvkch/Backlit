/* sane - Scanner Access Now Easy.
   Copyright (C) 1997 Andreas Beck
   This file is part of the SANE package.

   SANE is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2 of the License, or (at your
   option) any later version.

   SANE is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License
   along with sane; see the file COPYING.  If not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   This file implements a simple SANE frontend (well it rather is a 
   transport layer, but seen from libsane it is a frontend) which acts 
   as a NETSANE server. The NETSANE specifications should have come 
   with this package.
   Feel free to enhance this program ! It needs extension especially
   regarding crypto-support and authentication.
 */

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>

#include <netdb.h>
#include <netinet/in.h>

#include "../include/sane/sane.h"

void
auth_callback (SANE_String_Const domain,
	       SANE_Char *username,
	       SANE_Char *password)
{
  printf ("Client '%s' requested authorization.\nUser:\n", domain);
  scanf ("%s", username);
  printf ("Password:\n");
  scanf ("%s", password);
  return;
}

void
testsane (const char *dev_name)
{
  int hlp, x;
  SANE_Status bla;
  SANE_Int blubb;
  SANE_Handle hand;
  SANE_Parameters pars;
  const SANE_Option_Descriptor *sod;
  const SANE_Device **device_list;
  char buffer[2048];

  bla = sane_init (&blubb, auth_callback);
  fprintf (stderr, "Init : stat=%d ver=%x\nPress Enter to continue...",
	   bla, blubb);
  getchar ();
  if (bla != SANE_STATUS_GOOD)
    return;

  bla = sane_get_devices (&device_list, SANE_FALSE);
  fprintf (stderr, "GetDev : stat=%s\n", sane_strstatus (bla));
  if (bla != SANE_STATUS_GOOD)
    return;

  bla = sane_open (dev_name, &hand);
  fprintf (stderr, "Open : stat=%s hand=%p\n", sane_strstatus (bla), hand);
  if (bla != SANE_STATUS_GOOD)
    return;

  bla = sane_set_io_mode (hand, 0);
  fprintf (stderr, "SetIoMode : stat=%s\n", sane_strstatus (bla));

  for (hlp = 0; hlp < 9999; hlp++)
    {
      sod = sane_get_option_descriptor (hand, hlp);
      if (sod == NULL)
	break;
      fprintf (stderr, "Gopt(%d) : stat=%p\n", hlp, sod);
      fprintf (stderr, "name : %s\n", sod->name);
      fprintf (stderr, "title: %s\n", sod->title);
      fprintf (stderr, "desc : %s\n", sod->desc);

      fprintf (stderr, "type : %d\n", sod->type);
      fprintf (stderr, "unit : %d\n", sod->unit);
      fprintf (stderr, "size : %d\n", sod->size);
      fprintf (stderr, "cap  : %d\n", sod->cap);
      fprintf (stderr, "ctyp : %d\n", sod->constraint_type);
      switch (sod->constraint_type)
	{
	case SANE_CONSTRAINT_NONE:
	  break;
	case SANE_CONSTRAINT_STRING_LIST:
	  fprintf (stderr, "stringlist:\n");
	  break;
	case SANE_CONSTRAINT_WORD_LIST:
	  fprintf (stderr, "wordlist (%d) : ", sod->constraint.word_list[0]);
	  for (x = 1; x <= sod->constraint.word_list[0]; x++)
	    fprintf (stderr, " %d ", sod->constraint.word_list[x]);
	  fprintf (stderr, "\n");
	  break;
	case SANE_CONSTRAINT_RANGE:
	  fprintf (stderr, "range: %d-%d %d \n", sod->constraint.range->min,
		   sod->constraint.range->max, sod->constraint.range->quant);
	  break;
	}
    }

  bla = sane_get_parameters (hand, &pars);
  fprintf (stderr,
	   "Parm : stat=%s form=%d,lf=%d,bpl=%d,pixpl=%d,lin=%d,dep=%d\n",
	   sane_strstatus (bla),
	   pars.format, pars.last_frame,
	   pars.bytes_per_line, pars.pixels_per_line,
	   pars.lines, pars.depth);
  if (bla != SANE_STATUS_GOOD)
    return;

  bla = sane_start (hand);
  fprintf (stderr, "Start : stat=%s\n", sane_strstatus (bla));
  if (bla != SANE_STATUS_GOOD)
    return;

  do
    {
      bla = sane_read (hand, buffer, sizeof (buffer), &blubb);
      /*printf("Read : stat=%s len=%d\n",sane_strstatus (bla),blubb); */
      if (bla != SANE_STATUS_GOOD)
	{
	  if (bla == SANE_STATUS_EOF)
	    break;
	  return;
	}
      fwrite (buffer, 1, blubb, stdout);
    }
  while (1);

  sane_cancel (hand);
  fprintf (stderr, "Cancel.\n");

  sane_close (hand);
  fprintf (stderr, "Close\n");

  for (hlp = 0; hlp < 20; hlp++)
    fprintf (stderr, "STRS %d=%s\n", hlp, sane_strstatus (hlp));

  fprintf (stderr, "Exit.\n");
}

int
main (int argc, char *argv[])
{
  if (argc != 2 && argc != 3)
    {
      fprintf (stderr, "Usage: %s devicename [hostname]\n", argv[0]);
      exit (0);
    }
  if (argc == 3)
    {
      char envbuf[1024];
      sprintf (envbuf, "SANE_NET_HOST=%s", argv[2]);
      putenv (envbuf);
    }

  fprintf (stderr, "This is a SANE test application.\n"
	   "Now connecting to device %s.\n", argv[1]);
  testsane (argv[1]);
  sane_exit ();
  return 0;
}
