/* sane - Scanner Access Now Easy.
   Copyright (C) 2000-2003 Jochen Eisinger <jochen.eisinger@gmx.net>
   Copyright (C) 2003 James Perry (scsi_pp functions)
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

   This file implements an interface for the Mustek PP chipset A4S2 */

/* debug levels:
   0 - nothing
   1 - errors
   2 - warnings
   3 - things nice to know
   4 - code flow
   5 - detailed flow
   6 - everything 

   These debug levels can be set using the environment variable
   SANE_DEBUG_SANEI_PA4S2 */

#include "../include/sane/config.h"

#define BACKEND_NAME sanei_pa4s2
#include "../include/sane/sanei_backend.h"	/* pick up compatibility defs */

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#if defined(HAVE_LIBIEEE1284)

# include <ieee1284.h>

#elif defined(ENABLE_PARPORT_DIRECTIO)

# if defined(HAVE_SYS_IO_H)
#  if defined (__ICC) && __ICC >= 700
#   define __GNUC__ 2
#  endif
#  include <sys/io.h>
#  ifndef SANE_HAVE_SYS_IO_H_WITH_INB_OUTB
#   define IO_SUPPORT_MISSING
#  endif
#  if defined (__ICC) && __ICC >= 700
#   undef __GNUC__
#  elif defined(__ICC) && defined(HAVE_ASM_IO_H)
#   include <asm/io.h>
#  endif
# elif defined(HAVE_ASM_IO_H)
#  include <asm/io.h>		/* ugly, but backwards compatible */
# elif defined(HAVE_SYS_HW_H)
#  include <sys/hw.h>
# elif defined(__i386__)  && ( defined (__GNUC__) || defined (__ICC) )

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

# else
#  define IO_SUPPORT_MISSING
# endif

#else

# define IO_SUPPORT_MISSING

#endif /* HAVE_LIBIEEE1284 */

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/sanei_pa4s2.h"


#ifdef NDEBUG
#define DBG_INIT()  /* basically, this is already done in sanei_debug.h... */

#define TEST_DBG_INIT()

#else /* !NDEBUG */

static int sanei_pa4s2_dbg_init_called = SANE_FALSE;

#if (!defined __GNUC__ || __GNUC__ < 2 || \
     __GNUC_MINOR__ < (defined __cplusplus ? 6 : 4))

#define TEST_DBG_INIT() if (sanei_pa4s2_dbg_init_called == SANE_FALSE) \
                          {                                            \
                            DBG_INIT();                                \
                            DBG(6, "sanei_pa4s2: interface called for" \
                              " the first time\n");                    \
                            sanei_pa4s2_dbg_init_called = SANE_TRUE;   \
                          }
#else

#define TEST_DBG_INIT() if (sanei_pa4s2_dbg_init_called == SANE_FALSE)   \
                          {                                              \
                            DBG_INIT();                                  \
                            DBG(6, "%s: interface called for"            \
                              " the first time\n", __PRETTY_FUNCTION__); \
                            sanei_pa4s2_dbg_init_called = SANE_TRUE;     \
                          }

#endif

#endif /* NDEBUG */

#if defined(STDC_HEADERS)
# include <errno.h>
# include <stdio.h>
# include <stdlib.h>
#endif
#if defined(HAVE_STRING_H)
# include <string.h>
#elif defined(HAVE_STRINGS_H)
# include <strings.h>
#endif
#if defined(HAVE_SYS_TYPES_H)
# include <sys/types.h>
#endif

#include "../include/sane/saneopts.h"


#if (defined (HAVE_IOPERM) || defined (HAVE_LIBIEEE1284)) && !defined (IO_SUPPORT_MISSING)

#if defined(STDC_HEADERS)
# include <errno.h>
# include <stdio.h>
# include <stdlib.h>
#endif
#if defined(HAVE_STRING_H)
# include <string.h>
#elif defined(HAVE_STRINGS_H)
# include <strings.h>
#endif
#if defined(HAVE_SYS_TYPES_H)
# include <sys/types.h>
#endif

#include "../include/sane/saneopts.h"

#define PA4S2_MODE_NIB	0
#define PA4S2_MODE_UNI	1
#define PA4S2_MODE_EPP	2

#define PA4S2_ASIC_ID_1013	0xA8
#define PA4S2_ASIC_ID_1015	0xA5
#define PA4S2_ASIC_ID_1505	0xA2


typedef struct
  {
#ifndef HAVE_LIBIEEE1284
    const char name[6];
    u_long base;		/* i/o base address */
#endif
    u_int in_use;		/* port in use? */
    u_int enabled;		/* port enabled? */
    u_int mode;			/* protocoll */
    u_char prelock[3];		/* state of port */
#ifdef HAVE_LIBIEEE1284
    int caps;
#endif
  }
PortRec, *Port;

#if defined (HAVE_LIBIEEE1284)

static struct parport_list pplist;
static PortRec *port;

#else

static PortRec port[] =
{
  {"0x378", 0x378, SANE_FALSE, SANE_FALSE, PA4S2_MODE_NIB,
    {0, 0, 0}},
  {"0x278", 0x278, SANE_FALSE, SANE_FALSE, PA4S2_MODE_NIB,
    {0, 0, 0}},
  {"0x3BC", 0x3BC, SANE_FALSE, SANE_FALSE, PA4S2_MODE_NIB,
    {0, 0, 0}}
};

#endif

static u_int sanei_pa4s2_interface_options = SANEI_PA4S2_OPT_DEFAULT;

extern int setuid (uid_t);	/* should also be in unistd.h */

static int pa4s2_open (const char *dev, SANE_Status * status);
static void pa4s2_readbegin_epp (int fd, u_char reg);
static u_char pa4s2_readbyte_epp (int fd);
static void pa4s2_readend_epp (int fd);
static void pa4s2_readbegin_uni (int fd, u_char reg);
static u_char pa4s2_readbyte_uni (int fd);
static void pa4s2_readend_uni (int fd);
static void pa4s2_readbegin_nib (int fd, u_char reg);
static u_char pa4s2_readbyte_nib (int fd);
static void pa4s2_readend_nib (int fd);
static void pa4s2_writebyte_any (int fd, u_char reg, u_char val);
static int pa4s2_enable (int fd, u_char * prelock);
static int pa4s2_disable (int fd, u_char * prelock);
static int pa4s2_close (int fd, SANE_Status * status);

#if defined (HAVE_LIBIEEE1284)

static const char * pa4s2_libieee1284_errorstr(int error)
{

  switch (error)
    {

      case E1284_OK:
        return "Everything went fine";

      case E1284_NOTIMPL:
        return "Not implemented in libieee1284";

      case E1284_NOTAVAIL:
        return "Not available on this system";

      case E1284_TIMEDOUT:
        return "Operation timed out";

      case E1284_REJECTED:
        return "IEEE 1284 negotiation rejected";

      case E1284_NEGFAILED:
        return "Negotiation went wrong";

      case E1284_NOMEM:
        return "No memory left";

      case E1284_INIT:
        return "Error initializing port";

      case E1284_SYS:
        return "Error interfacing system";

      case E1284_NOID:
        return "No IEEE 1284 ID available";

      case E1284_INVALIDPORT:
        return "Invalid port";

      default:
        return "Unknown error";

    }
}

#endif

static int
pa4s2_init (SANE_Status *status)
{
  static int first_time = SANE_TRUE;
#if defined (HAVE_LIBIEEE1284)
  int result, n;
#endif

  DBG (6, "pa4s2_init: static int first_time = %u\n", first_time);

  if (first_time == SANE_FALSE)
    {
      DBG (5, "pa4s2_init: sanei already initalized\n");
      status = SANE_STATUS_GOOD;
      return 0;
    }

  DBG (5, "pa4s2_init: called for the first time\n");

  first_time = SANE_FALSE;

#if defined (HAVE_LIBIEEE1284)

  DBG (4, "pa4s2_init: initializing libieee1284\n");
  result = ieee1284_find_ports (&pplist, 0);

  if (result)
    {
      DBG (1, "pa4s2_init: initializing IEEE 1284 failed (%s)\n",
           pa4s2_libieee1284_errorstr (result));
      first_time = SANE_TRUE;
      *status = SANE_STATUS_INVAL;
      return -1;
    }

  DBG (3, "pa4s2_init: %d ports reported by IEEE 1284 library\n", pplist.portc);
  
  for (n=0; n<pplist.portc; n++)
    DBG (6, "pa4s2_init: port %d is `%s`\n", n, pplist.portv[n]->name);


  DBG (6, "pa4s2_init: allocating port list\n");
  if ((port = calloc(pplist.portc, sizeof(PortRec))) == NULL)
    {
      DBG (1, "pa4s2_init: not enough free memory\n");
      ieee1284_free_ports(&pplist);
      first_time = SANE_TRUE;
      *status = SANE_STATUS_NO_MEM;
      return -1;
    }

#else

  DBG (4, "pa4s2_init: trying to setuid root\n");

  if (0 > setuid (0))
    {

      DBG (1, "pa4s2_init: setuid failed: errno = %d\n", errno);
      DBG (5, "pa4s2_init: returning SANE_STATUS_INVAL\n");

      *status = SANE_STATUS_INVAL;
      first_time = SANE_TRUE;
      return -1;

    }

  DBG (3, "pa4s2_init: the application is now root\n");
  DBG (3, "pa4s2_init: this is a high security risk...\n");

  DBG (6, "pa4s2_init: ... you'd better start praying\n");

  /* PS: no, i don't trust myself either */

  /* PPS: i'd try rsbac or similar if i were you */

#endif

  DBG (5, "pa4s2_init: initialized successfully\n");
  *status = SANE_STATUS_GOOD;
  return 0;
}

static int
pa4s2_open (const char *dev, SANE_Status * status)
{

  int n, result;
#if !defined (HAVE_LIBIEEE1284)
  u_long base;
#endif
  
  DBG (4, "pa4s2_open: trying to attach dev `%s`\n", dev);

  if ((result = pa4s2_init(status)) != 0)
    {

      DBG (1, "pa4s2_open: failed to initialize\n");
      return result;
    }
  
#if !defined (HAVE_LIBIEEE1284)

  {
    char *end;

    DBG (5, "pa4s2_open: reading port number\n");

    base = strtol (dev, &end, 0);

    if ((end == dev) || (*end != '\0'))
      {

	DBG (1, "pa4s2_open: `%s` is not a valid port number\n", dev);
	DBG (6, "pa4s2_open: the part I did not understand was ...`%s`\n", end);
	DBG (5, "pa4s2_open: returning SANE_STATUS_INVAL\n");

	*status = SANE_STATUS_INVAL;

	return -1;

      }

  }

  DBG (6, "pa4s2_open: read port number 0x%03lx\n", base);

  if (base == 0)
    {

      DBG (1, "pa4s2_open: 0x%03lx is not a valid base address\n", base);
      DBG (5, "pa4s2_open: returning SANE_STATUS_INVAL\n");

      *status = SANE_STATUS_INVAL;
      return -1;

    }
#endif

  DBG (5, "pa4s2_open: looking up port in list\n");

#if defined (HAVE_LIBIEEE1284)

  for (n = 0; n < pplist.portc; n++)
    if (!strcmp(pplist.portv[n]->name, dev))
      break;

  if (pplist.portc <= n)
    {
      DBG (1, "pa4s2_open: `%s` is not a valid device name\n", dev);
      DBG (5, "pa4s2_open: returning SANE_STATUS_INVAL\n");

      *status = SANE_STATUS_INVAL;
      return -1;
    }

#else

  for (n = 0; n < NELEMS (port); n++)
    if (port[n].base == base)
      break;

  if (NELEMS (port) <= n)
    {

      DBG (1, "pa4s2_open: 0x%03lx is not a valid base address\n",
	   base);
      DBG (5, "pa4s2_open: returning SANE_STATUS_INVAL\n");

      *status = SANE_STATUS_INVAL;
      return -1;
    }

#endif

  DBG (6, "pa4s2_open: port is in list at port[%d]\n", n);

  if (port[n].in_use == SANE_TRUE)
    {

#if defined (HAVE_LIBIEEE1284)
      DBG (1, "pa4s2_open: device `%s` is already in use\n", dev);
#else
      DBG (1, "pa4s2_open: port 0x%03lx is already in use\n", base);
#endif
      DBG (5, "pa4s2_open: returning SANE_STATUS_DEVICE_BUSY\n");

      *status = SANE_STATUS_DEVICE_BUSY;
      return -1;

    }

  DBG (5, "pa4s2_open: setting up port data\n");

#if defined (HAVE_LIBIEEE1284)
  DBG (6, "pa4s2_open: name=%s in_use=SANE_TRUE\n", dev);
#else
  DBG (6, "pa4s2_open: base=0x%03lx in_use=SANE_TRUE\n", base);
#endif
  DBG (6, "pa4s2_open: enabled=SANE_FALSE mode=PA4S2_MODE_NIB\n");
  port[n].in_use = SANE_TRUE;
  port[n].enabled = SANE_FALSE;
  port[n].mode = PA4S2_MODE_NIB;


#if defined (HAVE_LIBIEEE1284)

  DBG (5, "pa4s2_open: opening device\n");
  result = ieee1284_open (pplist.portv[n], 0, &port[n].caps);

  if (result)
    {
      DBG (1, "pa4s2_open: could not open device `%s` (%s)\n",
           dev, pa4s2_libieee1284_errorstr (result));
      port[n].in_use = SANE_FALSE;
      DBG (6, "pa4s2_open: marking port %d as unused\n", n);
      *status = SANE_STATUS_ACCESS_DENIED;
      return -1;
    }

#else

  DBG (5, "pa4s2_open: getting io permissions\n");

  /* TODO: insert FreeBSD compatible code here */

  if (ioperm (port[n].base, 5, 1))
    {

      DBG (1, "pa4s2_open: cannot get io privilege for port 0x%03lx\n", 
      		port[n].base);


      DBG (5, "pa4s2_open: marking port[%d] as unused\n", n);
      port[n].in_use = SANE_FALSE;

      DBG (5, "pa4s2_open: returning SANE_STATUS_IO_ERROR\n");
      *status = SANE_STATUS_IO_ERROR;
      return -1;

    }
#endif

  DBG (3, "pa4s2_open: device `%s` opened...\n", dev);

  DBG (5, "pa4s2_open: returning SANE_STATUS_GOOD\n");
  *status = SANE_STATUS_GOOD;

  DBG (4, "pa4s2_open: open dev `%s` as fd %u\n", dev, n);

  return n;

}

#if defined(HAVE_LIBIEEE1284)


#define inbyte0(fd)	ieee1284_read_data(pplist.portv[fd]);
#define inbyte1(fd)	(ieee1284_read_status(pplist.portv[fd]) ^ S1284_INVERTED)
#define inbyte2(fd)	(ieee1284_read_control(pplist.portv[fd]) ^ C1284_INVERTED)
static u_char inbyte4(int fd)
{
  char val;
  ieee1284_epp_read_data(pplist.portv[fd], 0, &val, 1);
  return (u_char)val;
}

#define outbyte0(fd,val)	ieee1284_write_data(pplist.portv[fd], val)
#define outbyte1(fd,val)	/* ieee1284_write_status(pplist.portv[fd], (val) ^ S1284_INVERTED) */
#define outbyte2(fd,val)	ieee1284_write_control(pplist.portv[fd], (val) ^ C1284_INVERTED)

static void outbyte3(int fd, u_char val)
{
  ieee1284_epp_write_addr (pplist.portv[fd], 0, (char *)&val, 1);
}

#else

#define inbyte0(fd)	inb(port[fd].base)
#define inbyte1(fd)	inb(port[fd].base + 1)
#define inbyte2(fd)	inb(port[fd].base + 2)
#define inbyte4(fd)	inb(port[fd].base + 4)

#define outbyte0(fd,val)	outb(val, port[fd].base)
#define outbyte1(fd,val)	outb(val, port[fd].base + 1)
#define outbyte2(fd,val)	outb(val, port[fd].base + 2)
#define outbyte3(fd,val)	outb(val, port[fd].base + 3)

#endif


static void
pa4s2_readbegin_epp (int fd, u_char reg)
{

#if defined(HAVE_LIBIEEE1284)
  DBG (6, "pa4s2_readbegin_epp: selecting register %u at '%s'\n",
       (int) reg, pplist.portv[fd]->name);
#else
  DBG (6, "pa4s2_readbegin_epp: selecting register %u at 0x%03lx\n",
       (int) reg, port[fd].base);
#endif

  outbyte0 (fd, 0x20);
  outbyte2 (fd, 0x04);
  outbyte2 (fd, 0x06);
  outbyte2 (fd, 0x04);
  outbyte3 (fd, reg + 0x18);

}

static u_char
pa4s2_readbyte_epp (int fd)
{

  u_char val = inbyte4 (fd);

#if defined(HAVE_LIBIEEE1284)
  DBG (6, "pa4s2_readbyte_epp: reading value 0x%02x from '%s'\n",
       (int) val, pplist.portv[fd]->name);
#else
  DBG (6, "pa4s2_readbyte_epp: reading value 0x%02x at 0x%03lx\n",
       (int) val, port[fd].base);
#endif

  return val;

}

static void
pa4s2_readend_epp (int fd)
{

  DBG (6, "pa4s2_readend_epp: end of reading sequence\n");

  outbyte2 (fd, 0x04);
  outbyte2 (fd, 0x00);
  outbyte2 (fd, 0x04);

}

static void
pa4s2_readbegin_uni (int fd, u_char reg)
{

#if defined(HAVE_LIBIEEE1284)
  DBG (6, "pa4s2_readbegin_uni: selecting register %u for '%s'\n",
       (int) reg, pplist.portv[fd]->name);
#else
  DBG (6, "pa4s2_readbegin_uni: selecting register %u at 0x%03lx\n",
       (int) reg, port[fd].base);
#endif

  outbyte0 (fd, reg | 0x58);
  outbyte2 (fd, 0x04);
  outbyte2 (fd, 0x06);
  outbyte2 (fd, 0x04);
  outbyte2 (fd, 0x04);

}

static u_char
pa4s2_readbyte_uni (int fd)
{
  u_char val;

  outbyte2 (fd, 0x05);
  val = inbyte2(fd);
  val <<= 4;
  val &= 0xE0;
  val |= (inbyte1(fd) >> 3);
  outbyte2 (fd, 0x04);

#if defined(HAVE_LIBIEEE1284)
  DBG (6, "pa4s2_readbyte_uni: reading value 0x%02x from '%s'\n",
       (int) val, pplist.portv[fd]->name);
#else
  DBG (6, "pa4s2_readbyte_uni: reading value 0x%02x at 0x%03lx\n",
       (int) val, port[fd].base);
#endif

  return val;
}

static void
pa4s2_readend_uni (int fd)
{

  DBG (6, "pa4s2_readend_uni: end of reading sequence for fd %d\n", fd);

}

static void
pa4s2_readbegin_nib (int fd, u_char reg)
{

#if defined(HAVE_LIBIEEE1284)
  DBG (6, "pa4s2_readbegin_nib: selecting register %u at '%s'\n",
       (int) reg, pplist.portv[fd]->name);
#else
  DBG (6, "pa4s2_readbegin_nib: selecting register %u at 0x%03lx\n",
       (int) reg, port[fd].base);
#endif


  outbyte0 (fd, reg | 0x18);
  outbyte2 (fd, 0x04);
  outbyte2 (fd, 0x06);
  outbyte2 (fd, 0x04);
  outbyte2 (fd, 0x04);

}

static u_char
pa4s2_readbyte_nib (int fd)
{

  u_char val;
  
  outbyte2 (fd, 0x05);
  val = inbyte1(fd);
  val >>= 4;
  outbyte0 (fd, 0x58);
  val |= inbyte1(fd) & 0xF0;
  val ^= 0x88;
  outbyte0 (fd, 0x00);
  outbyte2 (fd, 0x04);

#if defined(HAVE_LIBIEEE1284)
  DBG (6, "pa4s2_readbyte_nib: reading value 0x%02x from '%s'\n",
  	(int) val, pplist.portv[fd]->name);
#else
  DBG (6, "pa4s2_readbyte_nib: reading value 0x%02x at 0x%03lx\n",
       (int) val, port[fd].base);
#endif

  return val;

}

static void
pa4s2_readend_nib (int fd)
{
  DBG (6, "pa4s2_readend_nib: end of reading sequence for fd %d\n", fd);
}

static void
pa4s2_writebyte_any (int fd, u_char reg, u_char val)
{

  /* somebody from Mustek asked me once, why I was writing the same
     value repeatedly to a port. Well, actually I don't know, it just
     works. Maybe the repeated writes could be replaced by appropriate
     delays or even left out completly.
   */
#if defined(HAVE_LIBIEEE1284)
  DBG (6, "pa4s2_writebyte_any: writing value 0x%02x"
       " in reg %u to '%s'\n", (int) val, (int) reg, pplist.portv[fd]->name);
#else
  DBG (6, "pa4s2_writebyte_any: writing value 0x%02x"
       " in reg %u at 0x%03lx\n", (int) val, (int) reg, port[fd].base);
#endif

  outbyte0 (fd, reg | 0x10);
  outbyte2 (fd, 0x04);
  outbyte2 (fd, 0x06);
  outbyte2 (fd, 0x06);
  outbyte2 (fd, 0x06);
  outbyte2 (fd, 0x06);
  outbyte2 (fd, 0x04);
  outbyte2 (fd, 0x04);
  outbyte0 (fd, val);
  outbyte2 (fd, 0x05);
  outbyte2 (fd, 0x05);
  outbyte2 (fd, 0x05);
  outbyte2 (fd, 0x04);
  outbyte2 (fd, 0x04);
  outbyte2 (fd, 0x04);
  outbyte2 (fd, 0x04);
}

static int
pa4s2_enable (int fd, u_char * prelock)
{
#if defined (HAVE_LIBIEEE1284)
  int result;
  result = ieee1284_claim (pplist.portv[fd]);

  if (result)
    {
      DBG (1, "pa4s2_enable: failed to claim the port (%s)\n",
      	pa4s2_libieee1284_errorstr(result));
      return -1;
    }
#endif

  prelock[0] = inbyte0 (fd);
  prelock[1] = inbyte1 (fd);
  prelock[2] = inbyte2 (fd);
  outbyte2 (fd, (prelock[2] & 0x0F) | 0x04);

  DBG (6, "pa4s2_enable: prelock[] = {0x%02x, 0x%02x, 0x%02x}\n",
       (int) prelock[0], (int) prelock[1], (int) prelock[2]);

  outbyte0 (fd, 0x15);
  outbyte0 (fd, 0x95);
  outbyte0 (fd, 0x35);
  outbyte0 (fd, 0xB5);
  outbyte0 (fd, 0x55);
  outbyte0 (fd, 0xD5);
  outbyte0 (fd, 0x75);
  outbyte0 (fd, 0xF5);
  outbyte0 (fd, 0x01);
  outbyte0 (fd, 0x81);

  return 0;
}

static int
pa4s2_disable (int fd, u_char * prelock)
{

  if ((sanei_pa4s2_interface_options & SANEI_PA4S2_OPT_ALT_LOCK) != 0)
    {

      DBG (6, "pa4s2_disable: using alternative command set\n");

      outbyte0 (fd, 0x00);
      outbyte2 (fd, 0x04);
      outbyte2 (fd, 0x06);
      outbyte2 (fd, 0x04);

    }

  outbyte2 (fd, prelock[2] & 0x0F);

  outbyte0 (fd, 0x15);
  outbyte0 (fd, 0x95);
  outbyte0 (fd, 0x35);
  outbyte0 (fd, 0xB5);
  outbyte0 (fd, 0x55);
  outbyte0 (fd, 0xD5);
  outbyte0 (fd, 0x75);
  outbyte0 (fd, 0xF5);
  outbyte0 (fd, 0x00);
  outbyte0 (fd, 0x80);

  outbyte0 (fd, prelock[0]);
  outbyte1 (fd, prelock[1]);
  outbyte2 (fd, prelock[2]);

#if defined(HAVE_LIBIEEE1284)
  ieee1284_release (pplist.portv[fd]);
#endif

  DBG (6, "pa4s2_disable: state restored\n");

  return 0;

}

static int
pa4s2_close (int fd, SANE_Status * status)
{
#if defined(HAVE_LIBIEEE1284)
  int result;
#endif
  DBG (4, "pa4s2_close: fd=%d\n", fd);

#if defined(HAVE_LIBIEEE1284)
  DBG (6, "pa4s2_close: this is port '%s'\n", pplist.portv[fd]->name);
#else
  DBG (6, "pa4s2_close: this is port 0x%03lx\n", port[fd].base);
#endif

  DBG (5, "pa4s2_close: checking whether port is enabled\n");

  if (port[fd].enabled == SANE_TRUE)
    {

      DBG (6, "pa4s2_close: disabling port\n");
      pa4s2_disable (fd, port[fd].prelock);

    }

  DBG (5, "pa4s2_close: trying to free io port\n");
#if defined(HAVE_LIBIEEE1284)
  if ((result = ieee1284_close(pplist.portv[fd])) < 0)
#else
  if (ioperm (port[fd].base, 5, 0))
#endif
    {

#if defined(HAVE_LIBIEEE1284)
      DBG (1, "pa4s2_close: can't free port '%s' (%s)\n", 
      		pplist.portv[fd]->name, pa4s2_libieee1284_errorstr(result));
#else
      DBG (1, "pa4s2_close: can't free port 0x%03lx\n", port[fd].base);
#endif

      DBG (5, "pa4s2_close: returning SANE_STATUS_IO_ERROR\n");
      *status = SANE_STATUS_IO_ERROR;
      return -1;

    }

  DBG (5, "pa4s2_close: marking port as unused\n");

  port[fd].in_use = SANE_FALSE;

  DBG (5, "pa4s2_close: returning SANE_STATUS_GOOD\n");

  *status = SANE_STATUS_GOOD;

  return 0;

}

const char **
sanei_pa4s2_devices()
{

  SANE_Status status;
  int n;
  const char **devices;

  TEST_DBG_INIT();

  DBG (4, "sanei_pa4s2_devices: invoked\n");

  if ((n = pa4s2_init(&status)) != 0)
    {

      DBG (1, "sanei_pa4s2_devices: failed to initialize (%s)\n",
      		sane_strstatus(status));
      return calloc(1, sizeof(char *));
    }

#if defined(HAVE_LIBIEEE1284)

  if ((devices = calloc((pplist.portc + 1), sizeof(char *))) == NULL)
    {
      DBG (2, "sanei_pa4s2_devices: not enough free memory\n");
      return calloc(1, sizeof(char *));
    }

  for (n=0; n<pplist.portc; n++)
    devices[n] = pplist.portv[n]->name;

#else

  if ((devices = calloc((NELEMS (port) + 1), sizeof(char *))) == NULL)
    {
      DBG (2, "sanei_pa4s2_devices: not enough free memory\n");
      return calloc(1, sizeof(char *));
    }

  for (n=0 ; n<NELEMS (port) ; n++)
    devices[n] = (char *)port[n].name;
#endif

  return devices;
}

/*
 * Needed for SCSI-over-parallel scanners (Paragon 600 II EP)
 */
SANE_Status
sanei_pa4s2_scsi_pp_get_status(int fd, u_char *status)
{
  u_char stat;

  TEST_DBG_INIT ();

  DBG (6, "sanei_pa4s2_scsi_pp_get_status: called for fd %d\n",
       fd);

#if defined(HAVE_LIBIEEE1284)
  if ((fd < 0) || (fd >= pplist.portc))
#else
  if ((fd < 0) || (fd >= NELEMS (port)))
#endif
    {

      DBG (2, "sanei_pa4s2_scsi_pp_get_status: invalid fd %d\n", fd);
      DBG (6, "sanei_pa4s2_scsi_pp_get_status: returning SANE_STATUS_INVAL\n");

      return SANE_STATUS_INVAL;

    }

  if (port[fd].in_use == SANE_FALSE)
    {

      DBG (2, "sanei_pa4s2_scsi_pp_get_status: port is not in use\n");
#if defined(HAVE_LIBIEEE1284)
      DBG (4, "sanei_pa4s2_scsi_pp_get_status: port is '%s'\n",
      		pplist.portv[fd]->name);
#else
      DBG (6, "sanei_pa4s2_scsi_pp_get_status: port is 0x%03lx\n",
	   port[fd].base);
#endif
      DBG (5, "sanei_pa4s2_scsi_pp_get_status: returning SANE_STATUS_INVAL\n");

      return SANE_STATUS_INVAL;

    }

  if (port[fd].enabled == SANE_FALSE)
    {

      DBG (2, "sanei_pa4s2_scsi_pp_get_status: port is not enabled\n");
#if defined(HAVE_LIBIEEE1284)
      DBG (4, "sanei_pa4s2_scsi_pp_get_status: port is '%s'\n",
      		pplist.portv[fd]->name);
#else
      DBG (6, "sanei_pa4s2_scsi_pp_get_status: port is 0x%03lx\n",
	   port[fd].base);
#endif
      DBG (5, "sanei_pa4s2_scsi_pp_get_status: returning SANE_STATUS_INVAL\n");

      return SANE_STATUS_INVAL;

    }

  outbyte2 (fd, 0x4);
  stat = inbyte1 (fd)^0x80;
  *status = (stat&0x2f)|((stat&0x10)<<2)|((stat&0x40)<<1)|((stat&0x80)>>3);
  DBG (5, "sanei_pa4s2_scsi_pp_get_status: status=0x%02X\n", *status);
  DBG (6, "sanei_pa4s2_scsi_pp_get_status: returning SANE_STATUS_GOOD\n");

  return SANE_STATUS_GOOD;
}

/*
 * SCSI-over-parallel scanners need this done when a register is
 * selected
 */ 
SANE_Status
sanei_pa4s2_scsi_pp_reg_select (int fd, int reg)
{
  TEST_DBG_INIT ();

#if defined(HAVE_LIBIEEE1284)
  if ((fd < 0) || (fd >= pplist.portc))
#else
  if ((fd < 0) || (fd >= NELEMS (port)))
#endif
    {

      DBG (2, "sanei_pa4s2_scsi_pp_reg_select: invalid fd %d\n", fd);
      DBG (6, "sanei_pa4s2_scsi_pp_reg_select: returning SANE_STATUS_INVAL\n");

      return SANE_STATUS_INVAL;

    }

  if (port[fd].in_use == SANE_FALSE)
    {

      DBG (2, "sanei_pa4s2_scsi_pp_reg_select: port is not in use\n");
#if defined(HAVE_LIBIEEE1284)
      DBG (4, "sanei_pa4s2_scsi_pp_get_status: port is '%s'\n",
      		pplist.portv[fd]->name);
#else
      DBG (6, "sanei_pa4s2_scsi_pp_get_status: port is 0x%03lx\n",
	   port[fd].base);
#endif
      DBG (5, "sanei_pa4s2_scsi_pp_reg_select: returning SANE_STATUS_INVAL\n");

      return SANE_STATUS_INVAL;

    }

  if (port[fd].enabled == SANE_FALSE)
    {

      DBG (2, "sanei_pa4s2_scsi_pp_reg_select: port is not enabled\n");
#if defined(HAVE_LIBIEEE1284)
      DBG (4, "sanei_pa4s2_scsi_pp_get_status: port is '%s'\n",
      		pplist.portv[fd]->name);
#else
      DBG (6, "sanei_pa4s2_scsi_pp_get_status: port is 0x%03lx\n",
	   port[fd].base);
#endif
      DBG (5, "sanei_pa4s2_scsi_pp_reg_select: returning SANE_STATUS_INVAL\n");

      return SANE_STATUS_INVAL;

    }

#if defined(HAVE_LIBIEEE1284)
  DBG (6, "sanei_pa4s2_scsi_pp_reg_select: selecting register %u at port '%s'\n",
       (int) reg, pplist.portv[fd]->name);
#else
  DBG (6, "sanei_pa4s2_scsi_pp_reg_select: selecting register %u at 0x%03lx\n",
       (int) reg, (u_long)port[fd].base);
#endif

  outbyte0 (fd, reg | 0x58);
  outbyte2 (fd, 0x04);
  outbyte2 (fd, 0x06);
  outbyte2 (fd, 0x04);
  outbyte2 (fd, 0x04);

  return SANE_STATUS_GOOD;
}

/*
 * The SCSI-over-parallel scanners need to be handled a bit differently
 * when opened, as they don't return a valid ASIC ID, so this can't be
 * used for detecting valid read modes
 */
SANE_Status
sanei_pa4s2_scsi_pp_open (const char *dev, int *fd)
{

  u_char val;
  SANE_Status status;

  TEST_DBG_INIT ();

  DBG(4, "sanei_pa4s2_scsi_pp_open: called for device '%s'\n", dev);
  DBG(5, "sanei_pa4s2_scsi_pp_open: trying to connect to port\n");

  if ((*fd = pa4s2_open (dev, &status)) == -1)
    {

      DBG (5, "sanei_pa4s2_scsi_pp_open: connection failed\n");

      return status;

    }

  DBG (6, "sanei_pa4s2_scsi_pp_open: connected to device using fd %u\n", *fd);

  DBG (5, "sanei_pa4s2_scsi_pp_open: checking for scanner\n");

  if (sanei_pa4s2_enable (*fd, SANE_TRUE)!=SANE_STATUS_GOOD)
    {
      DBG (3, "sanei_pa4s2_scsi_pp_open: error enabling device\n");
      return SANE_STATUS_IO_ERROR;
    }

  /*
   * Instead of checking ASIC ID, check device status
   */
  if (sanei_pa4s2_scsi_pp_get_status(*fd, &val)!=SANE_STATUS_GOOD)
    {
      DBG (3, "sanei_pa4s2_scsi_pp_open: error getting device status\n");
      sanei_pa4s2_enable (*fd, SANE_FALSE);
      return SANE_STATUS_IO_ERROR;
    }
  val&=0xf0;

  if ((val==0xf0)||(val&0x40)||(!(val&0x20)))
    {
      DBG (3, "sanei_pa4s2_scsi_pp_open: device returned status 0x%02X\n", val);
      sanei_pa4s2_enable (*fd, SANE_FALSE);
      return SANE_STATUS_DEVICE_BUSY;
    }

  if (sanei_pa4s2_enable (*fd, SANE_FALSE)!=SANE_STATUS_GOOD)
    {
      DBG (3, "sanei_pa4s2_scsi_pp_open: error disabling device\n");
      return SANE_STATUS_IO_ERROR;
    }

  /* FIXME: it would be nice to try to use a better mode here, but how to
   * know if it's going to work? */

  DBG (4, "sanei_pa4s2_scsi_pp_open: returning SANE_STATUS_GOOD\n");

  return SANE_STATUS_GOOD;
}

SANE_Status
sanei_pa4s2_open (const char *dev, int *fd)
{

  u_char asic, val;
  SANE_Status status;

  TEST_DBG_INIT ();

  DBG(4, "sanei_pa4s2_open: called for device '%s'\n", dev);
  DBG(5, "sanei_pa4s2_open: trying to connect to port\n");

  if ((*fd = pa4s2_open (dev, &status)) == -1)
    {

      DBG (5, "sanei_pa4s2_open: connection failed\n");

      return status;

    }

  DBG (6, "sanei_pa4s2_open: connected to device using fd %u\n", *fd);

  DBG (5, "sanei_pa4s2_open: checking for scanner\n");

  sanei_pa4s2_enable (*fd, SANE_TRUE);

  DBG (6, "sanei_pa4s2_open: reading ASIC id\n");

  sanei_pa4s2_readbegin (*fd, 0);

  sanei_pa4s2_readbyte (*fd, &asic);

  sanei_pa4s2_readend (*fd);

  switch (asic)
    {

    case PA4S2_ASIC_ID_1013:
      DBG (3, "sanei_pa4s2_open: detected ASIC id 1013\n");
      break;

    case PA4S2_ASIC_ID_1015:
      DBG (3, "sanei_pa4s2_open: detected ASIC id 1015\n");
      break;

    case PA4S2_ASIC_ID_1505:
      DBG (3, "sanei_pa4s2_open: detected ASIC id 1505\n");
      break;

    default:
      DBG (1, "sanei_pa4s2_open: could not find scanner\n");
      DBG (3, "sanei_pa4s2_open: reported ASIC id 0x%02x\n",
	   asic);

      sanei_pa4s2_enable (*fd, SANE_FALSE);
      DBG (5, "sanei_pa4s2_open: closing port\n");

      sanei_pa4s2_close (*fd);

      DBG (5, "sanei_pa4s2_open: returning SANE_STATUS_INVAL\n");

      return SANE_STATUS_INVAL;

    }

  sanei_pa4s2_enable (*fd, SANE_FALSE);

  DBG (4, "sanei_pa4s2_open: trying better modes\n");

  while (port[*fd].mode <= PA4S2_MODE_EPP)
    {

      if ((port[*fd].mode == PA4S2_MODE_UNI) &&
      ((sanei_pa4s2_interface_options & SANEI_PA4S2_OPT_TRY_MODE_UNI) == 0))
	{

	  DBG (3, "sanei_pa4s2_open: skipping mode UNI\n");
	  port[*fd].mode++;
	  continue;

	}

      if ((port[*fd].mode == PA4S2_MODE_EPP) &&
      ((sanei_pa4s2_interface_options & SANEI_PA4S2_OPT_NO_EPP) != 0))
	{
	  DBG (3, "sanei_pa4s2_open: skipping mode EPP\n");
	  break;
	}


      DBG (5, "sanei_pa4s2_open: trying mode %u\n", port[*fd].mode);

      sanei_pa4s2_enable (*fd, SANE_TRUE);

      sanei_pa4s2_readbegin (*fd, 0);

      sanei_pa4s2_readbyte (*fd, &val);

      if (val != asic)
	{

	  sanei_pa4s2_readend (*fd);
	  sanei_pa4s2_enable (*fd, SANE_FALSE);
	  DBG (5, "sanei_pa4s2_open: mode failed\n");
	  DBG (6, "sanei_pa4s2_open: returned ASIC-ID 0x%02x\n",
	       (int) val);
	  break;

	}

      sanei_pa4s2_readend (*fd);
      sanei_pa4s2_enable (*fd, SANE_FALSE);

      DBG (5, "sanei_pa4s2_open: mode works\n");

      port[*fd].mode++;

    }

  port[*fd].mode--;

  if ((port[*fd].mode == PA4S2_MODE_UNI) &&
      ((sanei_pa4s2_interface_options & SANEI_PA4S2_OPT_TRY_MODE_UNI) == 0))
    {
      port[*fd].mode--;
    }

  DBG (5, "sanei_pa4s2_open: using mode %u\n", port[*fd].mode);

  DBG (4, "sanei_pa4s2_open: returning SANE_STATUS_GOOD\n");

  return SANE_STATUS_GOOD;

}

void
sanei_pa4s2_close (int fd)
{

  SANE_Status status;

  TEST_DBG_INIT ();

  DBG (4, "sanei_pa4s2_close: fd = %d\n", fd);

#if defined(HAVE_LIBIEEE1284)
  if ((fd < 0) || (fd >= pplist.portc))
#else
  if ((fd < 0) || (fd >= NELEMS (port)))
#endif
    {

      DBG (2, "sanei_pa4s2_close: fd %d is invalid\n", fd);
      DBG (5, "sanei_pa4s2_close: failed\n");
      return;

    }

  if (port[fd].in_use == SANE_FALSE)
    {

      DBG (2, "sanei_pa4s2_close: port is not in use\n");
#if defined(HAVE_LIBIEEE1284)
      DBG (6, "sanei_pa4s2_close: port is '%s'\n", pplist.portv[fd]->name);
#else
      DBG (6, "sanei_pa4s2_close: port is 0x%03lx\n", port[fd].base);
#endif
      DBG (5, "sanei_pa4s2_close: failed\n");
      return;

    }

  DBG (5, "sanei_pa4s2_close: freeing resources\n");

  if (pa4s2_close (fd, &status) == -1)
    {

      DBG (2, "sanei_pa4s2_close: could not close scanner\n");
      DBG (5, "sanei_pa4s2_close: failed\n");
      return;
    }

  DBG (5, "sanei_pa4s2_close: finished\n");

}

SANE_Status
sanei_pa4s2_enable (int fd, int enable)
{

  TEST_DBG_INIT ();

  DBG (4, "sanei_pa4s2_enable: called for fd %d with value %d\n",
       fd, enable);

#if defined(HAVE_LIBIEEE1284)
  if ((fd < 0) || (fd >= pplist.portc))
#else
  if ((fd < 0) || (fd >= NELEMS (port)))
#endif
    {

      DBG (2, "sanei_pa4s2_enable: fd %d is invalid\n", fd);
      DBG (5, "sanei_pa4s2_enable: returning SANE_STATUS_INVAL\n");

      return SANE_STATUS_INVAL;

    }

  if (port[fd].in_use == SANE_FALSE)
    {

      DBG (2, "sanei_pa4s2_enable: port is not in use\n");
#if defined(HAVE_LIBIEEE1284)
      DBG (6, "sanei_pa4s2_close: port is '%s'\n", pplist.portv[fd]->name);
#else
      DBG (6, "sanei_pa4s2_close: port is 0x%03lx\n", port[fd].base);
#endif
      DBG (5, "sanei_pa4s2_enable: returning SANE_STATUS_INVAL\n");

      return SANE_STATUS_INVAL;

    }

  if ((enable != SANE_TRUE) && (enable != SANE_FALSE))
    {

      DBG (2, "sanei_pa4s2_enable: invalid value %d\n", enable);
      DBG (5, "sanei_pa4s2_enable: returning SANE_STATUS_INVAL\n");

      return SANE_STATUS_INVAL;

    }

  if ((unsigned int) enable == port[fd].enabled)
    {

      DBG (3, "sanei_pa4s2_enable: senseless call...\n");
      DBG (4, "sanei_pa4s2_enable: aborting\n");
      DBG (5, "sanei_pa4s2_enable: returning SANE_STATUS_GOOD\n");

      return SANE_STATUS_GOOD;

    }

  if (enable == SANE_TRUE)
    {

#if defined(HAVE_LIBIEEE1284)
      DBG (4, "sanei_pa4s2_enable: enable port '%s'\n", pplist.portv[fd]->name);
#else
      DBG (4, "sanei_pa4s2_enable: enable port 0x%03lx\n", port[fd].base);

      /* io-permissions are not inherited after fork (at least not on
         linux 2.2, although they seem to be inherited on linux 2.4),
         so we should make sure we get the permission */
      
      if (ioperm (port[fd].base, 5, 1))
      {
          DBG (1, "sanei_pa4s2_enable: cannot get io privilege for port"
	       " 0x%03lx\n", port[fd].base);

          DBG (5, "sanei_pa4s2_enable:: marking port[%d] as unused\n", fd);
          port[fd].in_use = SANE_FALSE;

          DBG (5, "sanei_pa4s2_enable:: returning SANE_STATUS_IO_ERROR\n");
          return SANE_STATUS_IO_ERROR;
      }
#endif

      if (pa4s2_enable (fd, port[fd].prelock) != 0)
        {
     	  DBG (1, "sanei_pa4s2_enable: failed to enable port\n");
	  DBG (5, "sanei_pa4s2_enable: returning SANE_STATUS_IO_ERROR\n");

	  return SANE_STATUS_IO_ERROR;
	}

    }
  else
    {

#if defined(HAVE_LIBIEEE1284)
      DBG (4, "sanei_pa4s2_enable: disable port '%s'\n", 
      		pplist.portv[fd]->name);
#else
      DBG (4, "sanei_pa4s2_enable: disable port 0x%03lx\n", port[fd].base);
#endif

      pa4s2_disable (fd, port[fd].prelock);

    }

  port[fd].enabled = enable;

  DBG (5, "sanei_pa4s2_enable: returning SANE_STATUS_GOOD\n");

  return SANE_STATUS_GOOD;
}

SANE_Status
sanei_pa4s2_readbegin (int fd, u_char reg)
{

  TEST_DBG_INIT ();

  DBG (4, "sanei_pa4s2_readbegin: called for fd %d and register %u\n",
       fd, (int) reg);

#if defined(HAVE_LIBIEEE1284)
  if ((fd < 0) || (fd >= pplist.portc))
#else
  if ((fd < 0) || (fd >= NELEMS (port)))
#endif
    {

      DBG (2, "sanei_pa4s2_readbegin: invalid fd %d\n", fd);
      DBG (5, "sanei_pa4s2_readbegin: returning SANE_STATUS_INVAL\n");

      return SANE_STATUS_INVAL;

    }

  if (port[fd].in_use == SANE_FALSE)
    {

      DBG (2, "sanei_pa4s2_readbegin: port is not in use\n");
#if defined(HAVE_LIBIEEE1284)
      DBG (6, "sanei_pa4s2_close: port is '%s'\n", pplist.portv[fd]->name);
#else
      DBG (6, "sanei_pa4s2_close: port is 0x%03lx\n", port[fd].base);
#endif
      DBG (5, "sanei_pa4s2_readbegin: returning SANE_STATUS_INVAL\n");

      return SANE_STATUS_INVAL;

    }

  if (port[fd].enabled == SANE_FALSE)
    {

      DBG (2, "sanei_pa4s2_readbegin: port is not enabled\n");
#if defined(HAVE_LIBIEEE1284)
      DBG (6, "sanei_pa4s2_close: port is '%s'\n", pplist.portv[fd]->name);
#else
      DBG (6, "sanei_pa4s2_close: port is 0x%03lx\n", port[fd].base);
#endif
      DBG (5, "sanei_pa4s2_readbegin: returning SANE_STATUS_INVAL\n");

      return SANE_STATUS_INVAL;

    }

  switch (port[fd].mode)
    {

    case PA4S2_MODE_EPP:

      DBG (5, "sanei_pa4s2_readbegin: EPP readbegin\n");
      pa4s2_readbegin_epp (fd, reg);
      break;

    case PA4S2_MODE_UNI:

      DBG (5, "sanei_pa4s2_readbegin: UNI readbegin\n");
      pa4s2_readbegin_uni (fd, reg);
      break;

    case PA4S2_MODE_NIB:

      DBG (5, "sanei_pa4s2_readbegin: NIB readbegin\n");
      pa4s2_readbegin_nib (fd, reg);
      break;

    default:

      DBG (1, "sanei_pa4s2_readbegin: port info broken\n");
      DBG (3, "sanei_pa4s2_readbegin: invalid port mode\n");
#if defined(HAVE_LIBIEEE1284)
      DBG (6, "sanei_pa4s2_close: port is '%s'\n", pplist.portv[fd]->name);
#else
      DBG (6, "sanei_pa4s2_close: port is 0x%03lx\n", port[fd].base);
#endif
      DBG (5, "sanei_pa4s2_readbegin: return SANE_STATUS_INVAL\n");

      return SANE_STATUS_INVAL;

    }

  DBG (5, "sanei_pa4s2_readbegin: returning SANE_STATUS_GOOD\n");

  return SANE_STATUS_GOOD;
}

SANE_Status
sanei_pa4s2_readbyte (int fd, u_char * val)
{

  TEST_DBG_INIT ();

  DBG (4, "sanei_pa4s2_readbyte: called with fd %d\n", fd);

  if (val == NULL)
    {

      DBG (1, "sanei_pa4s2_readbyte: got NULL pointer as result buffer\n");
      return SANE_STATUS_INVAL;

    }

#if defined(HAVE_LIBIEEE1284)
  if ((fd < 0) || (fd >= pplist.portc))
#else
  if ((fd < 0) || (fd >= NELEMS (port)))
#endif
    {

      DBG (2, "sanei_pa4s2_readbyte: invalid fd %d\n", fd);
      DBG (5, "sanei_pa4s2_readbyte: returning SANE_STATUS_INVAL\n");
      return SANE_STATUS_INVAL;

    }

  if (port[fd].in_use == SANE_FALSE)
    {

      DBG (2, "sanei_pa4s2_readbyte: port is not in use\n");
#if defined(HAVE_LIBIEEE1284)
      DBG (6, "sanei_pa4s2_close: port is '%s'\n", pplist.portv[fd]->name);
#else
      DBG (6, "sanei_pa4s2_close: port is 0x%03lx\n", port[fd].base);
#endif
      DBG (5, "sanei_pa4s2_readbyte: returning SANE_STATUS_INVAL\n");

      return SANE_STATUS_INVAL;

    }

  if (port[fd].enabled == SANE_FALSE)
    {

      DBG (2, "sanei_pa4s2_readbyte: port is not enabled\n");
#if defined(HAVE_LIBIEEE1284)
      DBG (6, "sanei_pa4s2_close: port is '%s'\n", pplist.portv[fd]->name);
#else
      DBG (6, "sanei_pa4s2_close: port is 0x%03lx\n", port[fd].base);
#endif
      DBG (5, "sanei_pa4s2_readbyte: returning SANE_STATUS_INVAL\n");

      return SANE_STATUS_INVAL;

    }

  DBG (4, "sanei_pa4s2_readbyte: we hope, the backend called\n");
  DBG (4, "sanei_pa4s2_readbyte: readbegin, so the port is ok...\n");

  DBG (6, "sanei_pa4s2_readbyte: this means, I did not check it - it's\n");
  DBG (6, "sanei_pa4s2_readbyte: not my fault, if your PC burns down.\n");

  switch (port[fd].mode)
    {

    case PA4S2_MODE_EPP:

      DBG (5, "sanei_pa4s2_readbyte: read in EPP mode\n");
      *val = pa4s2_readbyte_epp (fd);
      break;


    case PA4S2_MODE_UNI:

      DBG (5, "sanei_pa4s2_readbyte: read in UNI mode\n");
      *val = pa4s2_readbyte_uni (fd);
      break;


    case PA4S2_MODE_NIB:

      DBG (5, "sanei_pa4s2_readbyte: read in NIB mode\n");
      *val = pa4s2_readbyte_nib (fd);
      break;

    default:

      DBG (1, "sanei_pa4s2_readbyte: port info broken\n");
      DBG (2, "sanei_pa4s2_readbyte: probably the port wasn't"
	   " correct configured...\n");
      DBG (3, "sanei_pa4s2_readbyte: invalid port mode\n");
      DBG (6, "sanei_pa4s2_readbyte: port mode %u\n",
	   port[fd].mode);
      DBG (6, "sanei_pa4s2_readbyte: I told you!!!\n");
      DBG (5, "sanei_pa4s2_readbyte: return"
	   " SANE_STATUS_INVAL\n");

      return SANE_STATUS_INVAL;
    }

  DBG (5, "sanei_pa4s2_readbyte: read finished\n");

  DBG (6, "sanei_pa4s2_readbyte: got value 0x%02x\n", (int) *val);

  DBG (5, "sanei_pa4s2_readbyte: returning SANE_STATUS_GOOD\n");

  return SANE_STATUS_GOOD;

}

SANE_Status
sanei_pa4s2_readend (int fd)
{

  TEST_DBG_INIT ();

  DBG (4, "sanei_pa4s2_readend: called for fd %d\n", fd);

#if defined(HAVE_LIBIEEE1284)
  if ((fd < 0) || (fd >= pplist.portc))
#else
  if ((fd < 0) || (fd >= NELEMS (port)))
#endif
    {

      DBG (2, "sanei_pa4s2_readend: invalid fd %d\n", fd);
      DBG (5, "sanei_pa4s2_readend: returning SANE_STATUS_INVAL\n");

      return SANE_STATUS_INVAL;

    }

  if (port[fd].in_use == SANE_FALSE)
    {

      DBG (2, "sanei_pa4s2_readend: port is not in use\n");
#if defined(HAVE_LIBIEEE1284)
      DBG (6, "sanei_pa4s2_close: port is '%s'\n", pplist.portv[fd]->name);
#else
      DBG (6, "sanei_pa4s2_close: port is 0x%03lx\n", port[fd].base);
#endif
      DBG (5, "sanei_pa4s2_readend: returning SANE_STATUS_INVAL\n");

      return SANE_STATUS_INVAL;

    }

  if (port[fd].enabled == SANE_FALSE)
    {

      DBG (2, "sanei_pa4s2_readend: port is not enabled\n");
#if defined(HAVE_LIBIEEE1284)
      DBG (6, "sanei_pa4s2_close: port is '%s'\n", pplist.portv[fd]->name);
#else
      DBG (6, "sanei_pa4s2_close: port is 0x%03lx\n", port[fd].base);
#endif
      DBG (5, "sanei_pa4s2_readend: returning SANE_STATUS_INVAL\n");

      return SANE_STATUS_INVAL;

    }

  DBG (4, "sanei_pa4s2_readend: we hope, the backend called\n");
  DBG (4, "sanei_pa4s2_readend: readbegin, so the port is ok...\n");

  DBG (6, "sanei_pa4s2_readend: this means, I did not check it - it's\n");
  DBG (6, "sanei_pa4s2_readend: not my fault, if your PC burns down.\n");

  switch (port[fd].mode)
    {

    case PA4S2_MODE_EPP:

      DBG (5, "sanei_pa4s2_readend: EPP mode readend\n");
      pa4s2_readend_epp (fd);
      break;


    case PA4S2_MODE_UNI:

      DBG (5, "sanei_pa4s2_readend: UNI mode readend\n");
      pa4s2_readend_uni (fd);
      break;


    case PA4S2_MODE_NIB:

      DBG (5, "sanei_pa4s2_readend: NIB mode readend\n");
      pa4s2_readend_nib (fd);
      break;

    default:

      DBG (1, "sanei_pa4s2_readend: port info broken\n");
      DBG (2, "sanei_pa4s2_readend: probably the port wasn't"
	   " correct configured...\n");
      DBG (3, "sanei_pa4s2_readend: invalid port mode\n");
      DBG (6, "sanei_pa4s2_readend: port mode %u\n",
	   port[fd].mode);
      DBG (6, "sanei_pa4s2_readend: I told you!!!\n");
      DBG (5, "sanei_pa4s2_readend: return"
	   " SANE_STATUS_INVAL\n");

      return SANE_STATUS_INVAL;
    }


  DBG (5, "sanei_pa4s2_readend: returning SANE_STATUS_GOOD\n");

  return SANE_STATUS_GOOD;

}

SANE_Status
sanei_pa4s2_writebyte (int fd, u_char reg, u_char val)
{

  TEST_DBG_INIT ();

  DBG (4, "sanei_pa4s2_writebyte: called for fd %d, reg %u and val %u\n",
       fd, (int) reg, (int) val);

#if defined(HAVE_LIBIEEE1284)
  if ((fd < 0) || (fd >= pplist.portc))
#else
  if ((fd < 0) || (fd >= NELEMS (port)))
#endif
    {

      DBG (2, "sanei_pa4s2_writebyte: invalid fd %d\n", fd);
      DBG (5, "sanei_pa4s2_writebyte: returning SANE_STATUS_INVAL\n");

      return SANE_STATUS_INVAL;

    }

  if (port[fd].in_use == SANE_FALSE)
    {

      DBG (2, "sanei_pa4s2_writebyte: port is not in use\n");
#if defined(HAVE_LIBIEEE1284)
      DBG (6, "sanei_pa4s2_close: port is '%s'\n", pplist.portv[fd]->name);
#else
      DBG (6, "sanei_pa4s2_close: port is 0x%03lx\n", port[fd].base);
#endif
      DBG (5, "sanei_pa4s2_writebyte: returning SANE_STATUS_INVAL\n");

      return SANE_STATUS_INVAL;

    }

  if (port[fd].enabled == SANE_FALSE)
    {

      DBG (2, "sanei_pa4s2_writebyte: port is not enabled\n");
#if defined(HAVE_LIBIEEE1284)
      DBG (6, "sanei_pa4s2_close: port is '%s'\n", pplist.portv[fd]->name);
#else
      DBG (6, "sanei_pa4s2_close: port is 0x%03lx\n", port[fd].base);
#endif
      DBG (5, "sanei_pa4s2_readbegin: returning SANE_STATUS_INVAL\n");

      return SANE_STATUS_INVAL;

    }

  switch (port[fd].mode)
    {

    case PA4S2_MODE_EPP:
    case PA4S2_MODE_UNI:
    case PA4S2_MODE_NIB:

      DBG (5, "sanei_pa4s2_writebyte: NIB/UNI/EPP write\n");
      pa4s2_writebyte_any (fd, reg, val);
      break;

    default:

      DBG (1, "sanei_pa4s2_writebyte: port info broken\n");
      DBG (3, "sanei_pa4s2_writebyte: invalid port mode\n");
      DBG (6, "sanei_pa4s2_writebyte: port mode %u\n",
	   port[fd].mode);
      DBG (5, "sanei_pa4s2_writebyte: return"
	   " SANE_STATUS_INVAL\n");

      return SANE_STATUS_INVAL;

    }

  DBG (5, "sanei_pa4s2_writebyte: returning SANE_STATUS_GOOD\n");

  return SANE_STATUS_GOOD;
}

SANE_Status
sanei_pa4s2_options (u_int * options, int set)
{

  TEST_DBG_INIT ();

  DBG (4, "sanei_pa4s2_options: called with options %u and set = %d\n",
       *options, set);

  if ((set != SANE_TRUE) && (set != SANE_FALSE))
    DBG (2, "sanei_pa4s2_options: value of set is invalid\n");

  if ((set == SANE_TRUE) && (*options > 7))
    DBG (2, "sanei_pa4s2_options: value of *options is invalid\n");

  if (set == SANE_TRUE)
    {

      DBG (5, "sanei_pa4s2_options: setting options to %u\n", *options);

      sanei_pa4s2_interface_options = *options;

    }
  else
    {

      DBG (5, "sanei_pa4s2_options: options are set to %u\n",
	   sanei_pa4s2_interface_options);

      *options = sanei_pa4s2_interface_options;

    }

  DBG (5, "sanei_pa4s2_options: returning SANE_STATUS_GOOD\n");

  return SANE_STATUS_GOOD;

}

#else /* !HAVE_IOPERM */


SANE_Status
sanei_pa4s2_open (const char *dev, int *fd)
{

  TEST_DBG_INIT ();

  if (fd)
  	*fd = -1;

  DBG (4, "sanei_pa4s2_open: called for device `%s`\n", dev);
  DBG (3, "sanei_pa4s2_open: A4S2 support not compiled\n");
  DBG (6, "sanei_pa4s2_open: basically, this backend does only compile\n");
  DBG (6, "sanei_pa4s2_open: on x86 architectures. Furthermore it\n");
  DBG (6, "sanei_pa4s2_open: needs ioperm() and inb()/outb() calls.\n");
  DBG (6, "sanei_pa4s2_open: alternativly it makes use of libieee1284\n");
  DBG (6, "sanei_pa4s2_open: (which isn't present either)\n");
  DBG (5, "sanei_pa4s2_open: returning SANE_STATUS_INVAL\n");

  return SANE_STATUS_INVAL;

}

void
sanei_pa4s2_close (int fd)
{

  TEST_DBG_INIT ();

  DBG (4, "sanei_pa4s2_close: called for fd %d\n", fd);
  DBG (2, "sanei_pa4s2_close: fd %d is invalid\n", fd);
  DBG (3, "sanei_pa4s2_close: A4S2 support not compiled\n");
  DBG (6, "sanei_pa4s2_close: so I wonder, why this function is called"
       " anyway.\n");
  DBG (6, "sanei_pa4s2_close: maybe this is a bug in the backend.\n");
  DBG (5, "sanei_pa4s2_close: returning\n");

  return;
}

SANE_Status
sanei_pa4s2_enable (int fd, int enable)
{

  TEST_DBG_INIT ();

  DBG (4, "sanei_pa4s2_enable: called for fd %d with value=%d\n",
       fd, enable);
  DBG (2, "sanei_pa4s2_enable: fd %d is invalid\n", fd);

  if ((enable != SANE_TRUE) && (enable != SANE_FALSE))
    DBG (2, "sanei_pa4s2_enable: value %d is invalid\n", enable);

  DBG (3, "sanei_pa4s2_enable: A4S2 support not compiled\n");
  DBG (6, "sanei_pa4s2_enable: oops, I think there's someone going to\n");
  DBG (6, "sanei_pa4s2_enable: produce a lot of garbage...\n");
  DBG (5, "sanei_pa4s2_enable: returning SANE_STATUS_INVAL\n");

  return SANE_STATUS_INVAL;
}

SANE_Status
sanei_pa4s2_readbegin (int fd, u_char reg)
{

  TEST_DBG_INIT ();

  DBG (4, "sanei_pa4s2_readbegin: called for fd %d and register %d\n",
       fd, (int) reg);
  DBG (2, "sanei_pa4s2_readbegin: fd %d is invalid\n", fd);

  DBG (3, "sanei_pa4s2_readbegin: A4S2 support not compiled\n");
  DBG (6, "sanei_pa4s2_readbegin: don't look - this is going to be\n");
  DBG (6, "sanei_pa4s2_readbegin: worse then you'd expect...\n");
  DBG (5, "sanei_pa4s2_readbegin: returning SANE_STATUS_INVAL\n");

  return SANE_STATUS_INVAL;

}

SANE_Status
sanei_pa4s2_readbyte (int fd, u_char * val)
{

  TEST_DBG_INIT ();

  if (val)
  	*val = 0;

  DBG (4, "sanei_pa4s2_readbyte: called for fd %d\n", fd);
  DBG (2, "sanei_pa4s2_readbyte: fd %d is invalid\n", fd);
  DBG (3, "sanei_pa4s2_readbyte: A4S2 support not compiled\n");
  DBG (6, "sanei_pa4s2_readbyte: shit happens\n");
  DBG (5, "sanei_pa4s2_readbyte: returning SANE_STATUS_INVAL\n");

  return SANE_STATUS_INVAL;
}

SANE_Status
sanei_pa4s2_readend (int fd)
{

  TEST_DBG_INIT ();

  DBG (4, "sanei_pa4s2_readend: called for fd %d\n", fd);
  DBG (2, "sanei_pa4s2_readend: fd %d is invalid\n", fd);
  DBG (3, "sanei_pa4s2_readend: A4S2 support not compiled\n");
  DBG (6, "sanei_pa4s2_readend: it's too late anyway\n");
  DBG (5, "sanei_pa4s2_readend: returning SANE_STATUS_INVAL\n");

  return SANE_STATUS_INVAL;

}

SANE_Status
sanei_pa4s2_writebyte (int fd, u_char reg, u_char val)
{

  TEST_DBG_INIT ();

  DBG (4, "sanei_pa4s2_writebyte: called for fd %d and register %d, "
       "value = %u\n", fd, (int) reg, (int) val);
  DBG (2, "sanei_pa4s2_writebyte: fd %d is invalid\n", fd);
  DBG (3, "sanei_pa4s2_writebyte: A4S2 support not compiled\n");
  DBG (6, "sanei_pa4s2_writebyte: whatever backend you're using, tell\n");
  DBG (6, "sanei_pa4s2_writebyte: the maintainer his code has bugs...\n");
  DBG (5, "sanei_pa4s2_writebyte: returning SANE_STATUS_INVAL\n");

  return SANE_STATUS_INVAL;

}

SANE_Status
sanei_pa4s2_options (u_int * options, int set)
{

  TEST_DBG_INIT ();

  DBG (4, "sanei_pa4s2_options: called with options %u and set = %d\n",
       *options, set);

  if ((set != SANE_TRUE) && (set != SANE_FALSE))
    DBG (2, "sanei_pa4s2_options: value of set is invalid\n");

  if ((set == SANE_TRUE) && (*options > 3))
    DBG (2, "sanei_pa4s2_options: value of *options is invalid\n");

  DBG (3, "sanei_pa4s2_options: A4S2 support not compiled\n");
  DBG (5, "sanei_pa4s2_options: returning SANE_STATUS_INVAL\n");

  return SANE_STATUS_INVAL;

}

const char **
sanei_pa4s2_devices()
{
  TEST_DBG_INIT ();
  DBG (4, "sanei_pa4s2_devices: invoked\n");

  DBG (3, "sanei_pa4s2_devices: A4S2 support not compiled\n");
  DBG (5, "sanei_pa4s2_devices: returning empty list\n");

  return calloc(1, sizeof(char *));
}

SANE_Status
sanei_pa4s2_scsi_pp_get_status(int fd, u_char *status)
{
  TEST_DBG_INIT ();
  DBG (4, "sanei_pa4s2_scsi_pp_get_status: fd=%d, status=%p\n",
       fd, (void *) status);
  DBG (3, "sanei_pa4s2_scsi_pp_get_status: A4S2 support not compiled\n");
  return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
sanei_pa4s2_scsi_pp_reg_select (int fd, int reg)
{
  TEST_DBG_INIT ();
  DBG (4, "sanei_pa4s2_scsi_pp_reg_select: fd=%d, reg=%d\n",
       fd, reg);
  DBG (3, "sanei_pa4s2_devices: A4S2 support not compiled\n");
  return SANE_STATUS_UNSUPPORTED;
}

SANE_Status
sanei_pa4s2_scsi_pp_open (const char *dev, int *fd)
{
  TEST_DBG_INIT ();
  DBG (4, "sanei_pa4s2_scsi_pp_open: dev=%s, fd=%p\n",
       dev, (void *) fd);
  DBG (3, "sanei_pa4s2_scsi_pp_open: A4S2 support not compiled\n");
  return SANE_STATUS_UNSUPPORTED;
}

#endif /* !HAVE_IOPERM */
