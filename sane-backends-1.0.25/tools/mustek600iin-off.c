/*
   off.c - Switch the Mustek 600 II N off

   This utility accesses the I/O-ports directly and must therefore be
   run with euid root, or must at least have access to /dev/port.
   Compile with:
   gcc -DHAVE_SYS_IO_H -O2 -Wall -s -o off off.c
   The -O2 optimization is needed to allow inline functions !
   Copyright (C) 1997-1999 Andreas Czechanowski, DL4SDC

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   andreas.czechanowski@ins.uni-stuttgart.de
 */

#include "../include/sane/config.h"
#include "../include/sane/sanei.h"

#define MUSTEK_CONF	STRINGIFY(PATH_SANE_CONFIG_DIR) "/mustek.conf"
#define PORT_DEV	"/dev/port"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_SYS_IO_H
# include <sys/io.h>	/* use where available (glibc 2.x, for example) */
#elif HAVE_ASM_IO_H
# include <asm/io.h>	/* ugly, but backwards compatible */
#elif defined(__i386__)  && defined (__GNUC__)

static __inline__ void
outb (u_char value, u_long port)
{
  __asm__ __volatile__ ("outb %0,%1"::"a" (value), "d" ((u_short) port));
}

static __inline__ u_char
inb (u_long port)
{
  u_char value;

  __asm__ __volatile__ ("inb %1,%0":"=a" (value):"d" ((u_short) port));
  return value;
}
#endif

char *Mustek_Conf = MUSTEK_CONF;

int allowed_ports[] =
{
  0x26b, 0x26c,
  0x2ab, 0x2ac,
  0x2eb, 0x2ec,
  0x22b, 0x22c,
  0x32b, 0x32c,
  0x36b, 0x36c,
  0x3ab, 0x3ac,
  0x3eb, 0x3ec,
  -1
};

void
usage (void)
{
  fprintf (stderr, "Usage: off [port]\n"
	   "  switches the Mustek 600 II N off that is connected to\n"
	   "  base address <port>. If address is not given, reads it\n"
	   "  from SANE config file <%s>.\n", Mustek_Conf);
}

void
noaccess (int portaddr)
{
  fprintf (stderr, "Access to port 0x%03x not allowed !\n", portaddr);
}

int
check_port (int portaddr)
{
  int i, j;

  for (i = 0; (j = allowed_ports[i]) != -1; i++)
    {
      if (j == portaddr)
	return j;
    }
  return -1;
}

int
str2int (char *ch)
{
  int i;

  i = strtol (ch, NULL, 0);
  return i;
}

int
main (int argc, char **argv)
{
  char *cp;
  int portaddr = 0;
  FILE *fp;
  int pfd;

  /* get config file name from environment if variable is set */
  if (NULL != (cp = getenv ("MUSTEK_CONF")))
    {
      Mustek_Conf = cp;
    }

  /* if port is explicitly given, try this one */
  if (argc > 1)
    {
      portaddr = str2int (argv[1]);
    }
  /* else try to look it up from SANE's mustek.conf file */
  else if (NULL != (fp = fopen (MUSTEK_CONF, "r")))
    {
      char line[256];

      while (NULL != fgets (line, 255, fp))
	{
	  if ('#' == *line)
	    continue;
	  if (0 != (portaddr = str2int (line)))
	    break;
	}
      fclose (fp);
    }
  else
    {
      fprintf (stderr, "Mustek config file <%s> not found\n", Mustek_Conf);
      usage ();
      exit (1);
    }

  if (check_port (portaddr) < 0 || check_port (portaddr + 1) < 0)
    {
      fprintf (stderr, "invalid port address specified !\n");
      usage ();
      exit (1);
    }

  /* we need the control port, not the data port, so... */
  portaddr++;

  fprintf (stderr, "using control port address 0x%03x\n", portaddr);
  /* try to get I/O permission from the kernel */
  if (ioperm (portaddr, 1, 1) == 0)
    {
      outb (0x00, portaddr);
    }
  /* else try to open /dev/port to access the I/O port */
  else if ((pfd = open (PORT_DEV, O_RDWR, 0666)) >= 0)
    {
      char offcmd[] =
      {0x00};

      if ((lseek (pfd, portaddr, SEEK_SET) != portaddr)
	  || (write (pfd, offcmd, 1) != 1))
	{
	  perror ("error handling /dev/port");
	  exit (1);
	}
      close (pfd);
    }
  else
    {
      fprintf (stderr, "Could not get port access:\n"
	       "Neither via ioperm(), nor via /dev/port.\n"
	       "This program must be run setuid root,\n"
	       "or the user must have access to /dev/port.\n");
      exit (1);
    }
  printf ("successfully sent OFF-command to control port at 0x%03x.\n",
	  portaddr);

  exit (0);
}
