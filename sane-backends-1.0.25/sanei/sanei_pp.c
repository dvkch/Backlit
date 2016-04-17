/* sane - Scanner Access Now Easy.
 * Copyright (C) 2003-2005 Gerhard Jaeger <gerhard@gjaeger.de>
 * based on work done by Jochen Eisinger <jochen.eisinger@gmx.net>
 * also parts from libieee1284 by Tim Waugh <tim@cyberelk.net>
 * This file is part of the SANE package.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 *
 * As a special exception, the authors of SANE give permission for
 * additional uses of the libraries contained in this release of SANE.
 *
 * The exception is that, if you link a SANE library with other files
 * to produce an executable, this does not by itself cause the
 * resulting executable to be covered by the GNU General Public
 * License.  Your use of that executable is in no way restricted on
 * account of linking the SANE library code into it.
 *
 * This exception does not, however, invalidate any other reasons why
 * the executable file might be covered by the GNU General Public
 * License.
 *
 * If you submit changes to SANE to the maintainers to be included in
 * a subsequent release, you agree by submitting the changes that
 * those changes may be distributed with this exception intact.
 *
 * If you write modifications of your own for SANE, it is your choice
 * whether to permit this exception to apply to your modifications.
 * If you do not wish that, delete this exception notice. 
 *
 * This file implements an interface for accessing the parallelport
 */

/* debug levels:
 * 0 - nothing
 * 1 - errors
 * 2 - warnings
 * 3 - things nice to know
 * 4 - code flow
 * 5 - detailed flow
 * 6 - everything 
 *
 * These debug levels can be set using the environment variable
 * SANE_DEBUG_SANEI_PP
 */

#include "../include/sane/config.h"

#define BACKEND_NAME sanei_pp

#define _TEST_LOOPS 1000
#define _MAX_PORTS    20

#ifndef _VAR_NOT_USED
# define _VAR_NOT_USED(x)	((x)=(x))
#endif

/* uncomment this to have some parameter checks on in/out functions,
 * note: that this will slow down the calls
 */
#if 0
# define _PARANOIA
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#ifdef HAVE_LIMITS_H
# include <limits.h>
#else
# ifndef ULONG_MAX
#  define ULONG_MAX 4294967295UL
# endif
#endif
#if defined (ENABLE_PARPORT_DIRECTIO)
# undef HAVE_LIBIEEE1284
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
#  include <asm/io.h>
# elif defined(HAVE_SYS_HW_H)
#  include <sys/hw.h>
# elif defined(__i386__)  && ( defined (__GNUC__) || defined (__ICC) )

static __inline__ void
outb( u_char value, u_long port )
{
  __asm__ __volatile__ ("outb %0,%1"::"a" (value), "d" ((u_short) port));
}

static __inline__ u_char
inb( u_long port )
{
  u_char value;

  __asm__ __volatile__ ("inb %1,%0":"=a" (value):"d" ((u_short) port));
  return value;
}
# endif
#elif defined(HAVE_LIBIEEE1284)
# include <ieee1284.h>
#else
# if defined(__GNUC__)
#  warning "No I/O support for this architecture!"
# endif
# define IO_SUPPORT_MISSING
#endif

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/sanei_debug.h"
#include "../include/sane/sanei_pp.h"

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

/** our global init flag... */
static int           first_time = SANE_TRUE;
static unsigned long pp_thresh  = 0;

#if (defined (HAVE_IOPERM) || defined (HAVE_LIBIEEE1284)) && !defined (IO_SUPPORT_MISSING)

typedef struct {
	
#ifndef HAVE_LIBIEEE1284
	const char name[6];
	u_long     base;        /**< i/o base address                */
	u_char     ctrl;        /**< for restoring CTRL register     */
	u_char     ecp_ctrl;    /**< for restoring ECP-CTRL register */
#endif

	u_int in_use;           /**< port in use?      */
	u_int claimed;          /**< port claimed?     */
	
	int caps;               /**< port capabilities */

} PortRec, *Port;

#if defined (HAVE_LIBIEEE1284)

static struct parport_list  pplist;
static PortRec              port[_MAX_PORTS];

#else

/** redefine the CAPability flags */
enum ieee1284_capabilities
{
	CAP1284_RAW    = (1<<0), 
	CAP1284_NIBBLE = (1<<1), /* SPP mode             */ 
	CAP1284_BYTE   = (1<<2), /* PS/2 bidirectional   */
	CAP1284_COMPAT = (1<<3),
	CAP1284_BECP   = (1<<4),
	CAP1284_ECP    = (1<<5), /* ECP                  */
	CAP1284_ECPRLE = (1<<6), /* ECP with RLE support */
	CAP1284_ECPSWE = (1<<7),
	CAP1284_EPP    = (1<<8), /* EPP hardware support */
	CAP1284_EPPSL  = (1<<9), /* EPP 1.7              */
	CAP1284_EPPSWE = (1<<10) /* EPP software support */
};

static PortRec port[] = {
	{ "0x378", 0x378, 0, 0, SANE_FALSE, SANE_FALSE, 0 },
	{ "0x278", 0x278, 0, 0, SANE_FALSE, SANE_FALSE, 0 },
	{ "0x3BC", 0x3BC, 0, 0, SANE_FALSE, SANE_FALSE, 0 }
};

#endif

/** depending on the interface we use, define the port macros
 */
#if defined(HAVE_LIBIEEE1284)

#define inb_data(fd) ieee1284_read_data(pplist.portv[fd]);
#define inb_stat(fd) (ieee1284_read_status(pplist.portv[fd]) ^ S1284_INVERTED)
#define inb_ctrl(fd) (ieee1284_read_control(pplist.portv[fd]) ^ C1284_INVERTED)

static inline u_char inb_eppdata(int fd)
{
	u_char val;
	ieee1284_epp_read_data(pplist.portv[fd], 0, (char *)&val, 1);
	return val;
}

static inline void outb_eppdata(int fd, u_char val)
{
	ieee1284_epp_write_data(pplist.portv[fd], 0, (const char *)&val, 1);
}

#define outb_data(fd,val) ieee1284_write_data(pplist.portv[fd], val)
#define outb_ctrl(fd,val) ieee1284_write_control(pplist.portv[fd], \
                                                   (val) ^ C1284_INVERTED)
static inline void outb_addr(int fd, u_char val)
{
	ieee1284_epp_write_addr (pplist.portv[fd], 0, (char *)&val, 1);
}

#else

#define inb_data(fd)    inb(port[fd].base)
#define inb_stat(fd)    inb(port[fd].base + 1)
#define inb_ctrl(fd)    inb(port[fd].base + 2)
#define inb_eppdata(fd) inb(port[fd].base + 4)

#define outb_data(fd,val)    outb(val, port[fd].base)
#define outb_stat(fd,val)    outb(val, port[fd].base + 1)
#define outb_ctrl(fd,val)    outb(val, port[fd].base + 2)
#define outb_addr(fd,val)    outb(val, port[fd].base + 3)
#define outb_eppdata(fd,val) outb(val, port[fd].base + 4)

#ifdef HAVE_IOPL
# define _SET_IOPL()        iopl(3)
# define inbyte400(fd)      inb(port[fd].base + 0x400)
# define inbyte402(fd)      inb(port[fd].base + 0x402)
# define outbyte400(fd,val) outb(val, port[fd].base + 0x400)
# define outbyte402(fd,val) outb(val, port[fd].base + 0x402)
#else
# define _SET_IOPL()
# define inbyte400(fd)
# define inbyte402(fd,val)
# define outbyte400(fd,val)
# define outbyte402(fd,val)
#endif
#endif

/* should also be in unistd.h */
extern int setuid (uid_t);

#if defined (HAVE_LIBIEEE1284)

static const char *pp_libieee1284_errorstr( int error )
{
	switch (error) {

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

/** show the caps
 */
static int
pp_showcaps( int caps )
{
	int  mode = 0;
	char ct[1024];

    ct[0] = '\0';
	
	if( caps & CAP1284_NIBBLE ) {
		strcat( ct, "SPP " );
		mode |= SANEI_PP_MODE_SPP;
	}
		
	if( caps & CAP1284_BYTE ) {
		strcat( ct, "PS/2 " );
		mode |= SANEI_PP_MODE_BIDI;
	}

	if( caps &  CAP1284_EPP ) {
		strcat( ct, "EPP " );
		mode |= SANEI_PP_MODE_EPP;
	}
	
	if( caps &  CAP1284_EPPSWE ) {
		strcat( ct, "EPPSWE " );
		mode |= SANEI_PP_MODE_EPP;
	}

	if( caps &  CAP1284_ECP ) {
		strcat( ct, "ECP " );
		mode |= SANEI_PP_MODE_ECP;
	}

	if( caps &  CAP1284_ECPRLE ) {
		strcat( ct, "ECPRLE " );
		mode |= SANEI_PP_MODE_ECP;
	}

	DBG( 4, "Supported Modes: %s\n", ct );
	return mode;
}

#ifndef HAVE_LIBIEEE1284

/** probe the parallel port
 */
static int
pp_probe( int fd )
{
#ifdef HAVE_IOPL 
	SANE_Byte c;
	int       i, j;
#endif
	SANE_Byte a, b;
	int       retv = 0;
    
	DBG( 4, "pp_probe: port 0x%04lx\n", port[fd].base );

	/* SPP check */
	outbyte402( fd, 0x0c );
	outb_ctrl ( fd, 0x0c );
	outb_data ( fd, 0x55 );
	a = inb_data( fd );
	if( a != 0x55 ) {
	    DBG( 4, "pp_probe: nothing supported :-(\n" );
		return retv;
	}

	DBG( 4, "pp_probe: SPP port present\n" );
	retv += CAP1284_NIBBLE;

	/* check for ECP */
#ifdef HAVE_IOPL

	/* clear at most 1k of data from FIFO */
	for( i = 1024; i > 0; i-- ) { 
		a = inbyte402( fd );
		if ((a & 0x03) == 0x03)
			goto no_ecp;
		if (a & 0x01)
		    break;
		inbyte400( fd ); /* Remove byte from FIFO */
	}

	if (i <= 0)
		goto no_ecp;

	b = a ^ 3;
	outbyte402( fd, b );
	c = inbyte402( fd );

	if (a == c) {
		outbyte402( fd, 0xc0 ); /* FIFO test */
		j = 0;
		while (!(inbyte402( fd ) & 0x01) && (j < 1024)) {
	    	inbyte402( fd );
	    	j++;
		}
		if (j >= 1024)
		    goto no_ecp;
		i = 0;
		j = 0;
		while (!(inbyte402( fd ) & 0x02) && (j < 1024)) {
		    outbyte400( fd, 0x00 );
	    	i++;
		    j++;
		}
		if (j >= 1024)
		    goto no_ecp;
		j = 0;
		while (!(inbyte402( fd ) & 0x01) && (j < 1024)) {
	    	inbyte400( fd );
		    j++;
		}
		if (j >= 1024)
		    goto no_ecp;

    	DBG( 4, "pp_probe: ECP with a %i byte FIFO present\n", i );
		retv += CAP1284_ECP;
    }
    
no_ecp:
#endif
	/* check for PS/2 compatible port */
	if( retv & CAP1284_ECP ) {
		outbyte402( fd, 0x20 );
	}

	outb_data( fd, 0x55 );
	outb_ctrl( fd, 0x0c );
	a = inb_data( fd );
	outb_data( fd, 0x55 );
	outb_ctrl( fd, 0x2c );
	b = inb_data( fd );
	if( a != b ) {
    	DBG( 4, "pp_probe: PS/2 bidirectional port present\n");
		retv += CAP1284_BYTE;
    }

    /* check for EPP support */
	if( port[fd].base & 0x007 ) {
	    DBG( 4, "pp_probe: EPP not supported at this address\n" );
		return retv;
    }
#ifdef HAVE_IOPL
    if( retv & CAP1284_ECP ) {
		for( i = 0x00; i < 0x80; i += 0x20 ) {
	    	outbyte402( fd, i );

		    a = inb_stat( fd );
		    outb_stat( fd, a );
		    outb_stat( fd, (a & 0xfe));
		    a = inb_stat( fd );
	    	if (!(a & 0x01)) {
			    DBG( 2, "pp_probe: "
			            "Failed Intel bug check. (Phony EPP in ECP)\n" );
				return retv;
		    }
		}
	    DBG( 4, "pp_probe: Passed Intel bug check.\n" );
		outbyte402( fd, 0x80 );
    }
#endif

	a = inb_stat( fd );
	outb_stat( fd, a );
	outb_stat( fd, (a & 0xfe));
	a = inb_stat( fd );

	if (a & 0x01) {
		outbyte402( fd, 0x0c );
		outb_ctrl ( fd, 0x0c );
		return retv;
	}

	outb_ctrl( fd, 0x04 );
	inb_eppdata ( fd );
	a = inb_stat( fd );
	outb_stat( fd, a );
	outb_stat( fd, (a & 0xfe));

	if( a & 0x01 ) {
	    DBG( 4, "pp_probe: EPP 1.9 with hardware direction protocol\n");
		retv += CAP1284_EPP;
    } else {
		/* The EPP timeout bit was not set, this could either be:
		 * EPP 1.7
		 * EPP 1.9 with software direction
		 */
		outb_ctrl( fd, 0x24 );
		inb_eppdata ( fd );
		a = inb_stat( fd );
		outb_stat( fd, a );
		outb_stat( fd, (a & 0xfe));
		if( a & 0x01 ) {
			DBG( 4, "pp_probe: EPP 1.9 with software direction protocol\n" );
		    retv += CAP1284_EPPSWE;
		} else {
			DBG( 4, "pp_probe: EPP 1.7\n" );
			retv += CAP1284_EPPSL;
		}
	}

	outbyte402( fd, 0x0c );
	outb_ctrl ( fd, 0x0c );
    return retv;
}

/**
 */
static int pp_set_scpmode( int fd )
{
	SANE_Byte tmp;
	DBG( 4, "pp_set_scpmode\n" );
	
#ifdef HAVE_IOPL
	tmp  = inbyte402( fd );
	tmp &= 0x1f;
	outbyte402( fd, tmp );
#endif
	tmp  = inb_ctrl( fd );
	tmp &= 0x0f;
	outb_ctrl ( fd, tmp );
	
	return SANE_STATUS_GOOD;
}

static int pp_set_bidimode( int fd )
{
	SANE_Byte tmp;
	DBG( 4, "pp_set_bidimode\n" );
#ifdef HAVE_IOPL
	tmp = inbyte402( fd );
	tmp = (tmp & 0x1f) | 0x20;
	outbyte402( fd, tmp );
#endif
	tmp = inb_ctrl( fd );
	tmp = (tmp & 0x0f) | 0x20;
	outb_ctrl ( fd, tmp );
	
	return SANE_STATUS_GOOD;
}

static int pp_set_eppmode( int fd )
{
	SANE_Byte tmp;
	DBG( 4, "pp_set_eppmode\n" );
#ifdef HAVE_IOPL
	tmp = inbyte402( fd );
	tmp = (tmp & 0x1f) | 0x80;
	outbyte402( fd, tmp );
#endif
	tmp = inb_ctrl( fd );
	tmp = (tmp & 0xf0) | 0x40;
	outb_ctrl ( fd, tmp );
	
	return SANE_STATUS_GOOD;
}

static int pp_set_ecpmode( int fd )
{
#ifdef HAVE_IOPL
	SANE_Byte tmp;
#endif
	
	DBG( 4, "pp_set_ecpmode\n" );
#ifdef HAVE_IOPL
	tmp = inbyte402( fd );
	tmp = (tmp & 0x1f) | 0x60;
	outbyte402( fd, tmp );
	return SANE_STATUS_GOOD;
#endif
	return SANE_STATUS_UNSUPPORTED;
}

/** set the parallel port mode
 */
static int
pp_setmode( int fd, int mode )
{
	int ret;

	if( 0 == (mode & port[fd].caps)) {
		DBG( 2, "pp_setmode: mode not supported %d\n", mode );
		return SANE_STATUS_UNSUPPORTED;
	}
	
	switch( mode ) {
		case SANEI_PP_MODE_SPP:  ret = pp_set_scpmode( fd );  break;
		case SANEI_PP_MODE_BIDI: ret = pp_set_bidimode( fd ); break;
		case SANEI_PP_MODE_EPP:  ret = pp_set_eppmode( fd );  break;
		case SANEI_PP_MODE_ECP:  ret = pp_set_ecpmode( fd );  break;

		default:
			DBG( 2, "pp_setmode: invalid mode %d\n", mode );
			return SANE_STATUS_INVAL;
	}

	return ret;
}

#endif

static unsigned long
pp_time_diff( struct timeval *start, struct timeval *end )
{
	double s, e, r;

	s = (double)start->tv_sec * 1000000.0 + (double)start->tv_usec;
	e = (double)end->tv_sec   * 1000000.0 + (double)end->tv_usec;

	if( e > s )
		r = (e - s);
	else
		r = (s - e);

	if( r <= (double)ULONG_MAX )
		return (unsigned long)r;

	return 0;
}

/**
 */
static unsigned long
pp_calculate_thresh( void )
{
	unsigned long  i, r, ret;
	struct timeval start, end, deadline;

	gettimeofday( &start, NULL);

	for( i = _TEST_LOOPS; i; i-- ) {

		gettimeofday( &deadline, NULL );
		deadline.tv_usec += 10;
		deadline.tv_sec  += deadline.tv_usec / 1000000;
		deadline.tv_usec %= 1000000;
	}

	gettimeofday( &end, NULL);

	r   = pp_time_diff( &start, &end );
	ret = r/_TEST_LOOPS;
	return ret;
}

/**
 */
static void
pp_calibrate_delay( void )
{
	unsigned long  r, i;
	struct timeval start, end;

    for( i = 0; i < 5; i++ ) {

		pp_thresh = pp_calculate_thresh();
		gettimeofday( &start, NULL);

		for( i = _TEST_LOOPS; i; i-- ) {
			sanei_pp_udelay( 1 );
		}
		gettimeofday( &end, NULL);

		r = pp_time_diff( &start, &end );

		DBG( 4, "pp_calibrate_delay: Delay expected: "
				"%u, real %lu, pp_thresh=%lu\n", _TEST_LOOPS, r, pp_thresh );

		if( r >= _TEST_LOOPS ) {
			return;
		}
	}

	DBG( 4, "pp_calibrate_delay: pp_thresh set to 0\n" );
	pp_thresh = 0;
}

static SANE_Status
pp_init( void )
{
#if defined (HAVE_LIBIEEE1284)
	int result, i;
#endif

	if( first_time == SANE_FALSE ) {
		DBG( 5, "pp_init: already initalized\n" );
		return SANE_STATUS_GOOD;
    }

	DBG( 5, "pp_init: called for the first time\n");
	first_time = SANE_FALSE;

#if defined (HAVE_LIBIEEE1284)

	DBG( 4, "pp_init: initializing libieee1284\n");
	result = ieee1284_find_ports( &pplist, 0 );

	if (result) {
		DBG (1, "pp_init: initializing IEEE 1284 failed (%s)\n",
				 pp_libieee1284_errorstr( result ));
		first_time = SANE_TRUE;
		return SANE_STATUS_INVAL;
	}

	DBG( 3, "pp_init: %d ports reported by IEEE 1284 library\n", pplist.portc);
  
	for( i = 0; i < pplist.portc; i++ )
		DBG( 6, "pp_init: port %d is `%s`\n", i, pplist.portv[i]->name);

	/* we support only up to _MAX_PORTS... */
	if( pplist.portc > _MAX_PORTS ) {
		DBG (1, "pp_init: Lib IEEE 1284 reports too much ports: %d\n", 
		        pplist.portc );

		ieee1284_free_ports( &pplist );
		first_time = SANE_TRUE;
		return SANE_STATUS_UNSUPPORTED;
	}
	memset( port, 0, sizeof(port));

#else

	DBG( 4, "pp_init: trying to setuid root\n");
	if( 0 > setuid( 0 )) {

		DBG( 1, "pp_init: setuid failed: errno = %d\n", errno );
		DBG( 5, "pp_init: returning SANE_STATUS_INVAL\n" );

		first_time = SANE_TRUE;
		return SANE_STATUS_INVAL;
	}

	DBG( 3, "pp_init: the application is now root\n" );

#endif

	DBG( 5, "pp_init: initialized successfully\n" );
	return SANE_STATUS_GOOD;
}

static int
pp_open( const char *dev, SANE_Status * status )
{
	int i;
#if !defined (HAVE_LIBIEEE1284)
	u_long base;
#else
	int result;
#endif
  
	DBG( 4, "pp_open: trying to attach dev `%s`\n", dev );

#if !defined (HAVE_LIBIEEE1284)
{
	char *end;

	DBG( 5, "pp_open: reading port number\n" );

	base = strtol( dev, &end, 0 );
	if ((end == dev) || (*end != '\0')) {

		DBG( 1, "pp_open: `%s` is not a valid port number\n", dev);
		DBG( 6, "pp_open: the part I did not understand was ...`%s`\n", end);
		*status = SANE_STATUS_INVAL;
		return -1;
	}
}

	DBG( 6, "pp_open: read port number 0x%03lx\n", base );
	if( base == 0 ) {

		DBG( 1, "pp_open: 0x%03lx is not a valid base address\n", base );
		*status = SANE_STATUS_INVAL;
		return -1;
	}
#endif

	DBG( 5, "pp_open: looking up port in list\n" );

#if defined (HAVE_LIBIEEE1284)
	for( i = 0; i < pplist.portc; i++ ) {
		DBG( 5, "pp_open: checking >%s<\n", pplist.portv[i]->name );
		if( !strcmp(pplist.portv[i]->name, dev))
			break;
	}

	if( pplist.portc <= i ) {
		DBG( 1, "pp_open: `%s` is not a valid device name\n", dev );
		*status = SANE_STATUS_INVAL;
		return -1;
	}

#else

	for( i = 0; i < NELEMS(port); i++ ) {
		if( port[i].base == base )
			break;
	}

	if (NELEMS (port) <= i) {

		DBG( 1, "pp_open: 0x%03lx is not a valid base address\n", base );
		*status = SANE_STATUS_INVAL;
		return -1;
	}

#endif

	DBG( 6, "pp_open: port is in list at port[%d]\n", i);

	if( port[i].in_use == SANE_TRUE) {

#if defined (HAVE_LIBIEEE1284)
		DBG( 1, "pp_open: device `%s` is already in use\n", dev );
#else
		DBG( 1, "pp_open: port 0x%03lx is already in use\n", base );
#endif
		*status = SANE_STATUS_DEVICE_BUSY;
		return -1;
	}

	port[i].in_use  = SANE_TRUE;
	port[i].claimed = SANE_FALSE;

#if defined (HAVE_LIBIEEE1284)

	DBG( 5, "pp_open: opening device\n" );
	result = ieee1284_open( pplist.portv[i], 0, &port[i].caps );
	if (result) {
		DBG( 1, "pp_open: could not open device `%s` (%s)\n",
				dev, pp_libieee1284_errorstr (result));
		port[i].in_use = SANE_FALSE;
		*status = SANE_STATUS_ACCESS_DENIED;
		return -1;
	}

#else

	DBG( 5, "pp_open: getting io permissions\n" );

	/* TODO: insert FreeBSD compatible code here */

	if( ioperm( port[i].base, 5, 1 )) {
		DBG( 1, "pp_open: cannot get io privilege for port 0x%03lx\n", 
				port[i].base);

		port[i].in_use = SANE_FALSE;
		*status = SANE_STATUS_IO_ERROR;
		return -1;
	}

	/* save the CTRL register settings */
#ifdef HAVE_IOPL
	_SET_IOPL();
	port[i].ecp_ctrl = inbyte402(i);
	port[i].ctrl     = inb_ctrl(i);
#endif
	
	/* check the capabilities of this port */
	port[i].caps = pp_probe( i );
#endif

	port[i].caps = pp_showcaps( port[i].caps );
	DBG( 3, "pp_open: device `%s` opened...\n", dev );
	*status = SANE_STATUS_GOOD;
	return i;
}

static int
pp_close( int fd, SANE_Status *status )
{
#if defined(HAVE_LIBIEEE1284)
	int result;
#endif
	DBG( 4, "pp_close: fd=%d\n", fd );

#if defined(HAVE_LIBIEEE1284)
	DBG( 6, "pp_close: this is port '%s'\n", pplist.portv[fd]->name );
#else
	DBG( 6, "pp_close: this is port 0x%03lx\n", port[fd].base );
	DBG( 6, "pp_close: restoring the CTRL registers\n" );
	outb_ctrl( fd, port[fd].ctrl );
#ifdef HAVE_IOPL
	outbyte402( fd, port[fd].ecp_ctrl );
#endif
#endif

	if( port[fd].claimed == SANE_TRUE ) {
		sanei_pp_release( fd );
	}

	DBG( 5, "pp_close: trying to free io port\n" );
#if defined(HAVE_LIBIEEE1284)
	if((result = ieee1284_close(pplist.portv[fd])) < 0) {
#else
	if( ioperm( port[fd].base, 5, 0 )) {
#endif
#if defined(HAVE_LIBIEEE1284)
		DBG( 1, "pp_close: can't free port '%s' (%s)\n", 
				pplist.portv[fd]->name, pp_libieee1284_errorstr(result));
#else
		DBG( 1, "pp_close: can't free port 0x%03lx\n", port[fd].base );
#endif
		*status = SANE_STATUS_IO_ERROR;
		return -1;
	}

	DBG( 5, "pp_close: marking port as unused\n" );
	port[fd].in_use = SANE_FALSE;

	*status = SANE_STATUS_GOOD;
	return 0;
}

/** exported functions **/

SANE_Status
sanei_pp_init( void )
{
	SANE_Status result;
	
	DBG_INIT();

	result = pp_init();
	if( result != SANE_STATUS_GOOD ) {
		return result;
	}

	pp_calibrate_delay();
	return SANE_STATUS_GOOD;
}

SANE_Status
sanei_pp_open( const char *dev, int *fd )
{
	SANE_Status status;

	DBG( 4, "sanei_pp_open: called for device '%s'\n", dev);

	*fd = pp_open( dev, &status );
	if( *fd  == -1 ) {
		DBG( 5, "sanei_pp_open: connection failed\n" );
		return status;
	}

	DBG( 6, "sanei_pp_open: connected to device using fd %u\n", *fd );

	return SANE_STATUS_GOOD;
}

void
sanei_pp_close( int fd )
{
	SANE_Status status;

	DBG( 4, "sanei_pp_close: fd = %d\n", fd );

#if defined(HAVE_LIBIEEE1284)
	if((fd < 0) || (fd >= pplist.portc)) {
#else
	if((fd < 0) || (fd >= NELEMS (port))) {
#endif
		DBG( 2, "sanei_pp_close: fd %d is invalid\n", fd );
		return;
	}

	if( port[fd].in_use == SANE_FALSE ) {

		DBG( 2, "sanei_pp_close: port is not in use\n" );
#if defined(HAVE_LIBIEEE1284)
		DBG( 6, "sanei_pp_close: port is '%s'\n", pplist.portv[fd]->name );
#else
		DBG( 6, "sanei_pp_close: port is 0x%03lx\n", port[fd].base );
#endif
      return;
    }

	DBG( 5, "sanei_pp_close: freeing resources\n" );
	if( pp_close (fd, &status) == -1 ) {
		DBG( 5, "sanei_pp_close: failed\n" );
		return;
	}
	DBG( 5, "sanei_pp_close: finished\n" );
}

SANE_Status
sanei_pp_claim( int fd )
{
#if defined (HAVE_LIBIEEE1284)
	int result;
#endif
	DBG( 4, "sanei_pp_claim: fd = %d\n", fd );

#if defined (HAVE_LIBIEEE1284)
	if((fd < 0) || (fd >= pplist.portc)) {
		DBG( 2, "sanei_pp_claim: fd %d is invalid\n", fd );
		return SANE_STATUS_INVAL;
	}
	
	result = ieee1284_claim (pplist.portv[fd]);
	if (result) {
		DBG (1, "sanei_pp_claim: failed (%s)\n",
				pp_libieee1284_errorstr(result));
		return -1;
	}
#endif

	port[fd].claimed = SANE_TRUE;
	
	return SANE_STATUS_GOOD;
}

SANE_Status
sanei_pp_release( int fd )
{
	DBG( 4, "sanei_pp_release: fd = %d\n", fd );

#if defined(HAVE_LIBIEEE1284)
	if((fd < 0) || (fd >= pplist.portc)) {
		DBG( 2, "sanei_pp_release: fd %d is invalid\n", fd );
		return SANE_STATUS_INVAL;
	}
		
	ieee1284_release( pplist.portv[fd] );
#endif
	port[fd].claimed = SANE_FALSE;
	
	return SANE_STATUS_GOOD;
}

SANE_Status
sanei_pp_outb_data( int fd, SANE_Byte val )
{
#ifdef _PARANOIA
	DBG( 4, "sanei_pp_outb_data: called for fd %d\n", fd );

#if defined(HAVE_LIBIEEE1284)
	if ((fd < 0) || (fd >= pplist.portc)) {
#else
	if ((fd < 0) || (fd >= NELEMS (port))) {
#endif
		DBG( 2, "sanei_pp_outb_data: invalid fd %d\n", fd );
		return SANE_STATUS_INVAL;
    }
#endif
	outb_data( fd, val );
	return SANE_STATUS_GOOD;
}

SANE_Status
sanei_pp_outb_ctrl( int fd, SANE_Byte val )
{
#ifdef _PARANOIA
	DBG( 4, "sanei_pp_outb_ctrl: called for fd %d\n", fd );

#if defined(HAVE_LIBIEEE1284)
	if ((fd < 0) || (fd >= pplist.portc)) {
#else
	if ((fd < 0) || (fd >= NELEMS (port))) {
#endif
		DBG( 2, "sanei_pp_outb_ctrl: invalid fd %d\n", fd );
		return SANE_STATUS_INVAL;
    }
#endif
	outb_ctrl( fd, val );
	return SANE_STATUS_GOOD;
}

SANE_Status
sanei_pp_outb_addr( int fd, SANE_Byte val )
{
#ifdef _PARANOIA
	DBG( 4, "sanei_pp_outb_addr: called for fd %d\n", fd );

#if defined(HAVE_LIBIEEE1284)
	if ((fd < 0) || (fd >= pplist.portc)) {
#else
	if ((fd < 0) || (fd >= NELEMS (port))) {
#endif
		DBG( 2, "sanei_pp_outb_addr: invalid fd %d\n", fd );
		return SANE_STATUS_INVAL;
    }
#endif
    outb_addr( fd, val );
	return SANE_STATUS_GOOD;
}

SANE_Status
sanei_pp_outb_epp( int fd, SANE_Byte val )
{
#ifdef _PARANOIA
	DBG( 4, "sanei_pp_outb_epp: called for fd %d\n", fd );

#if defined(HAVE_LIBIEEE1284)
	if ((fd < 0) || (fd >= pplist.portc)) {
#else
	if ((fd < 0) || (fd >= NELEMS (port))) {
#endif
		DBG( 2, "sanei_pp_outb_epp: invalid fd %d\n", fd );
		return SANE_STATUS_INVAL;
    }
#endif
    outb_eppdata( fd, val );
	return SANE_STATUS_GOOD;
}

SANE_Byte
sanei_pp_inb_data( int fd )
{
#ifdef _PARANOIA
	DBG( 4, "sanei_pp_inb_data: called for fd %d\n", fd );
	
#if defined(HAVE_LIBIEEE1284)
	if ((fd < 0) || (fd >= pplist.portc)) {
#else
	if ((fd < 0) || (fd >= NELEMS (port))) {
#endif
		DBG( 2, "sanei_pp_inb_data: invalid fd %d\n", fd );
		return SANE_STATUS_INVAL;
    }
#endif
	return inb_data( fd );
}

SANE_Byte
sanei_pp_inb_stat( int fd )
{
#ifdef _PARANOIA
	DBG( 4, "sanei_pp_inb_stat: called for fd %d\n", fd );
	
#if defined(HAVE_LIBIEEE1284)
	if ((fd < 0) || (fd >= pplist.portc)) {
#else
	if ((fd < 0) || (fd >= NELEMS (port))) {
#endif
		DBG( 2, "sanei_pp_outb_stat: invalid fd %d\n", fd );
		return SANE_STATUS_INVAL;
    }
#endif
	return inb_stat( fd );
}

SANE_Byte
sanei_pp_inb_ctrl( int fd )
{
#ifdef _PARANOIA
	DBG( 4, "sanei_pp_inb_ctrl: called for fd %d\n", fd );
#if defined(HAVE_LIBIEEE1284)
	if ((fd < 0) || (fd >= pplist.portc)) {
#else
	if ((fd < 0) || (fd >= NELEMS (port))) {
#endif
		DBG( 2, "sanei_pp_inb_ctrl: invalid fd %d\n", fd );
		return SANE_STATUS_INVAL;
    }
#endif
	return inb_ctrl( fd );
}

SANE_Byte
sanei_pp_inb_epp( int fd )
{
#ifdef _PARANOIA
	DBG( 4, "sanei_pp_inb_epp: called for fd %d\n", fd );
	
#if defined(HAVE_LIBIEEE1284)
	if ((fd < 0) || (fd >= pplist.portc)) {
#else
	if ((fd < 0) || (fd >= NELEMS (port))) {
#endif
		DBG( 2, "sanei_pp_inb_epp: invalid fd %d\n", fd );
		return SANE_STATUS_INVAL;
	}
#endif
	return inb_eppdata( fd );
}

SANE_Status
sanei_pp_getmodes( int fd, int *mode )
{
#if defined(HAVE_LIBIEEE1284)
	if ((fd < 0) || (fd >= pplist.portc)) {
#else
	if ((fd < 0) || (fd >= NELEMS (port))) {
#endif
		DBG( 2, "sanei_pp_getmodes: invalid fd %d\n", fd );
		return SANE_STATUS_INVAL;
	}

	if( mode )
		*mode = port[fd].caps;

	return SANE_STATUS_GOOD;
}

SANE_Status
sanei_pp_setmode( int fd, int mode )
{
#if defined(HAVE_LIBIEEE1284)
	int result;

	if ((fd < 0) || (fd >= pplist.portc)) {
#else
	if ((fd < 0) || (fd >= NELEMS (port))) {
#endif
		DBG( 2, "sanei_pp_setmode: invalid fd %d\n", fd );
		return SANE_STATUS_INVAL;
	}

	if((mode !=	SANEI_PP_MODE_SPP) && (mode !=	SANEI_PP_MODE_BIDI) &&
	   (mode !=	SANEI_PP_MODE_EPP) && (mode !=	SANEI_PP_MODE_ECP)) {
		DBG( 2, "sanei_pp_setmode: invalid mode %d\n", mode );
		return SANE_STATUS_INVAL;
	}
	
#if defined(HAVE_LIBIEEE1284)
	switch( mode ) {
		case SANEI_PP_MODE_SPP:  mode = M1284_NIBBLE; break;
		case SANEI_PP_MODE_BIDI: mode = M1284_BYTE;   break;
		case SANEI_PP_MODE_EPP:  mode = M1284_EPP;    break;
		case SANEI_PP_MODE_ECP:  mode = M1284_ECP;    break;

		default:
			DBG( 2, "sanei_pp_setmode: invalid mode %d\n", mode );
			return SANE_STATUS_INVAL;
	}

	result = ieee1284_negotiate( pplist.portv[fd], mode );

	/* negotiation might fails, but the port-mode should be set... */
	if((E1284_OK != result) && (E1284_NEGFAILED != result)) {
		DBG( 2, "sanei_pp_setmode failed: %s\n",
		        pp_libieee1284_errorstr( result ));
		return SANE_STATUS_INVAL;
	}

	return SANE_STATUS_GOOD;
#else
    return pp_setmode( fd, mode );
#endif
}

void
sanei_pp_udelay( unsigned long usec )
{
	struct timeval now, deadline;

	if( usec == 0 )
		return;

	gettimeofday( &deadline, NULL );
	deadline.tv_usec += usec;
	deadline.tv_sec  += deadline.tv_usec / 1000000;
	deadline.tv_usec %= 1000000;

	if( usec < pp_thresh )
		return;
	
	do {
		gettimeofday( &now, NULL );
	} while ((now.tv_sec < deadline.tv_sec) ||
		(now.tv_sec == deadline.tv_sec && now.tv_usec < deadline.tv_usec));
}

SANE_Status
sanei_pp_set_datadir( int fd, int rev )
{
#if defined(HAVE_LIBIEEE1284)
	if ((fd < 0) || (fd >= pplist.portc)) {
#else
	SANE_Byte tmp;

	if ((fd < 0) || (fd >= NELEMS (port))) {
#endif
		DBG( 2, "sanei_pp_setdir: invalid fd %d\n", fd );
		return SANE_STATUS_INVAL;
    }

#if defined(HAVE_LIBIEEE1284)
	ieee1284_data_dir( pplist.portv[fd], rev );
#else
	tmp = inb_ctrl( fd );
	if( SANEI_PP_DATAIN == rev )
		tmp |= SANEI_PP_CTRL_DIRECTION;
	else
		tmp &= ~SANEI_PP_CTRL_DIRECTION;
	outb_ctrl( fd, tmp );
#endif

    return SANE_STATUS_GOOD;
}

SANE_Bool
sanei_pp_uses_directio( void )
{
#if defined(HAVE_LIBIEEE1284)
	return SANE_FALSE;
#else
	return SANE_TRUE;
#endif
}

#else /* !HAVE_IOPERM */

SANE_Status
sanei_pp_init( void )
{
	DBG_INIT();
	_VAR_NOT_USED( first_time );
	return SANE_STATUS_GOOD;
}

SANE_Status
sanei_pp_open( const char *dev, int *fd )
{
	if (fd)
		*fd = -1;

	DBG( 4, "sanei_pp_open: called for device `%s`\n", dev );
	DBG( 3, "sanei_pp_open: support not compiled\n" );
	DBG( 6, "sanei_pp_open: basically, this backend does only compile\n" );
	DBG( 6, "sanei_pp_open: on x86 architectures. Furthermore it\n" );
	DBG( 6, "sanei_pp_open: needs ioperm() and inb()/outb() calls.\n" );
	DBG( 6, "sanei_pp_open: alternativly it makes use of libieee1284\n" );
	DBG( 6, "sanei_pp_open: (which isn't present either)\n");
	return SANE_STATUS_INVAL;
}

void
sanei_pp_close( int fd )
{
	DBG( 4, "sanei_pp_close: called for fd %d\n", fd );
	DBG( 2, "sanei_pp_close: fd %d is invalid\n", fd );
	return;
}

SANE_Status
sanei_pp_claim( int fd )
{
	DBG( 4, "sanei_pp_claim: called for fd %d\n", fd );
	DBG( 2, "sanei_pp_claim: fd %d is invalid\n", fd );
	return SANE_STATUS_INVAL;
}

SANE_Status
sanei_pp_release( int fd )
{
	DBG( 4, "sanei_pp_release: called for fd %d\n", fd );
	DBG( 2, "sanei_pp_release: fd %d is invalid\n", fd );
	return SANE_STATUS_INVAL;
}

SANE_Status
sanei_pp_outb_data( int fd, SANE_Byte val )
{
	_VAR_NOT_USED( val );
	DBG( 4, "sanei_pp_outb_data: called for fd %d\n", fd );
	DBG( 2, "sanei_pp_outb_data: fd %d is invalid\n", fd );
	return SANE_STATUS_INVAL;
}

SANE_Status
sanei_pp_outb_ctrl( int fd, SANE_Byte val )
{
	_VAR_NOT_USED( val );
	DBG( 4, "sanei_pp_outb_ctrl: called for fd %d\n", fd );
	DBG( 2, "sanei_pp_outb_ctrl: fd %d is invalid\n", fd );
	return SANE_STATUS_INVAL;
}

SANE_Status
sanei_pp_outb_addr( int fd, SANE_Byte val )
{
	_VAR_NOT_USED( val );
	DBG( 4, "sanei_pp_outb_addr: called for fd %d\n", fd );
	DBG( 2, "sanei_pp_outb_addr: fd %d is invalid\n", fd );
	return SANE_STATUS_INVAL;
}

SANE_Byte
sanei_pp_inb_data( int fd )
{
	DBG( 4, "sanei_pp_inb_data: called for fd %d\n", fd );
	DBG( 2, "sanei_pp_inb_data: fd %d is invalid\n", fd );
	return SANE_STATUS_INVAL;
}

SANE_Byte sanei_pp_inb_stat( int fd )
{
	DBG( 4, "sanei_pp_inb_stat: called for fd %d\n", fd );
	DBG( 2, "sanei_pp_inb_stat: fd %d is invalid\n", fd );
	return SANE_STATUS_INVAL;
}

SANE_Byte sanei_pp_inb_ctrl( int fd )
{
	DBG( 4, "sanei_pp_inb_ctrl: called for fd %d\n", fd );
	DBG( 2, "sanei_pp_inb_ctrl: fd %d is invalid\n", fd );
	return SANE_STATUS_INVAL;
}

SANE_Byte sanei_pp_inb_epp ( int fd )
{
	DBG( 4, "sanei_pp_inb_epp: called for fd %d\n", fd );
	DBG( 2, "sanei_pp_inb_epp: fd %d is invalid\n", fd );
	return SANE_STATUS_INVAL;
}

SANE_Status
sanei_pp_getmodes( int fd, int *mode )
{
	_VAR_NOT_USED( mode );
	DBG( 4, "sanei_pp_getmodes: called for fd %d\n", fd );
	DBG( 1, "sanei_pp_getmodes: fd %d is invalid\n", fd );
	return SANE_STATUS_INVAL;
}

SANE_Status
sanei_pp_setmode( int fd, int mode )
{
	_VAR_NOT_USED( mode );
	DBG( 4, "sanei_pp_setmode: called for fd %d\n", fd );
	DBG( 1, "sanei_pp_setmode: fd %d is invalid\n", fd );
	return SANE_STATUS_INVAL;
}

void
sanei_pp_udelay( unsigned long usec )
{
	_VAR_NOT_USED( usec );
	_VAR_NOT_USED( pp_thresh );
	DBG( 2, "sanei_pp_udelay: not supported\n" );
}

SANE_Status
sanei_pp_set_datadir( int fd, int rev )
{
	_VAR_NOT_USED( rev );
	DBG( 4, "sanei_pp_setdir: called for fd %d\n", fd );
	DBG( 1, "sanei_pp_setdir: fd %d is invalid\n", fd );
	return SANE_STATUS_INVAL;
}

SANE_Bool
sanei_pp_uses_directio( void )
{
	DBG( 1, "sanei_pp_uses_directio: not supported\n" );
	return SANE_FALSE;
}

#endif /* !HAVE_IOPERM */
