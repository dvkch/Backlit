/**
   Copyright (C) 2001-2012 Stéphane Voltz <stef.dev@free.fr>
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

   This file implements a SANE backend for Umax PP flatbed scanners.  */

#undef BACKEND_NAME
#define BACKEND_NAME umax_pp_low

#include "../include/sane/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_IO_H
#include <sys/io.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include "../include/sane/sanei_debug.h"
#include <errno.h>

#ifdef HAVE_DEV_PPBUS_PPI_H
#include <dev/ppbus/ppi.h>
#include <dev/ppbus/ppbconf.h>
#endif

#ifdef HAVE_MACHINE_CPUFUNC_H
#include <machine/cpufunc.h>
#endif

#ifdef HAVE_I386_SET_IOPERM
#include <machine/sysarch.h>
#endif

#ifdef HAVE_LINUX_PPDEV_H
#include <sys/ioctl.h>
#include <linux/parport.h>
#include <linux/ppdev.h>
#endif

/*************************************************/
/* here we define sanei_inb/sanei_outb based on  */
/* OS dependant inb/outb definitions             */
/* SANE_INB is defined whenever a valid inb/outb */
/* definition has been found                     */
/* once all these work, it might be moved to     */
/* sanei_pio.c                                   */
/*************************************************/

#ifdef ENABLE_PARPORT_DIRECTIO

#if (! defined SANE_INB ) && ( defined HAVE_SYS_HW_H )	/* OS/2 EMX case */
#define SANE_INB 1
static int
sanei_ioperm (int start, int length, int enable)
{
  if (enable)
    return _portaccess (port, port + length - 1);
  return 0;
}

static unsigned char
sanei_inb (unsigned int port)
{
  return _inp8 (port) & 0xFF;
}

static void
sanei_outb (unsigned int port, unsigned char value)
{
  _outp8 (port, value);
}

static void
sanei_insb (unsigned int port, unsigned char *addr, unsigned long count)
{
  _inps8 (port, (unsigned char *) addr, count);
}

static void
sanei_insl (unsigned int port, unsigned char *addr, unsigned long count)
{
  _inps32 (port, (unsigned long *) addr, count);
}

static void
sanei_outsb (unsigned int port, const unsigned char *addr,
	     unsigned long count)
{
  _outps8 (port, (unsigned char *) addr, count);
}

static void
sanei_outsl (unsigned int port, const unsigned char *addr,
	     unsigned long count)
{
  _outps32 (port, (unsigned long *) addr, count);
}
#endif /* OS/2 EMX case */



#if (! defined SANE_INB ) && ( defined HAVE_MACHINE_CPUFUNC_H )	/* FreeBSD case */
#define SANE_INB 2
static int
sanei_ioperm (int start, int length, int enable)
{
#ifdef HAVE_I386_SET_IOPERM
  return i386_set_ioperm (start, length, enable);
#else
  int fd = 0;

  /* makes compilers happy */
  start = length + enable;
  fd = open ("/dev/io", O_RDONLY);
  if (fd > 0)
    return 0;
  return -1;
#endif
}

static unsigned char
sanei_inb (unsigned int port)
{
  return inb (port);
}

static void
sanei_outb (unsigned int port, unsigned char value)
{
  outb (port, value);
}

static void
sanei_insb (unsigned int port, unsigned char *addr, unsigned long count)
{
  insb (port, addr, count);
}

static void
sanei_insl (unsigned int port, unsigned char *addr, unsigned long count)
{
  insl (port, addr, count);
}

static void
sanei_outsb (unsigned int port, const unsigned char *addr,
	     unsigned long count)
{
  outsb (port, addr, count);
}

static void
sanei_outsl (unsigned int port, const unsigned char *addr,
	     unsigned long count)
{
  outsl (port, addr, count);
}
#endif /* FreeBSD case */


/* linux GCC on i386 */
#if ( ! defined SANE_INB ) && ( defined HAVE_SYS_IO_H ) && ( defined __GNUC__ ) && ( defined __i386__ )
#define SANE_INB 3

static int
sanei_ioperm (int start, int length, int enable)
{
#ifdef HAVE_IOPERM
  return ioperm (start, length, enable);
#else
  /* linux without ioperm ? hum ... */
  /* makes compilers happy */
  start = length + enable;
  return 0;
#endif
}

static unsigned char
sanei_inb (unsigned int port)
{
  return inb (port);
}

static void
sanei_outb (unsigned int port, unsigned char value)
{
  outb (value, port);
}

static void
sanei_insb (unsigned int port, unsigned char *addr, unsigned long count)
{
  insb (port, addr, count);
}

static void
sanei_insl (unsigned int port, unsigned char *addr, unsigned long count)
{
  insl (port, addr, count);
}

static void
sanei_outsb (unsigned int port, const unsigned char *addr,
	     unsigned long count)
{
  outsb (port, addr, count);
}

static void
sanei_outsl (unsigned int port, const unsigned char *addr,
	     unsigned long count)
{
  /* oddly, 32 bit I/O are done with outsw instead of the expected outsl */
  outsw (port, addr, count);
}
#endif /* linux GCC on i386 */


/* linux GCC non i386 */
#if ( ! defined SANE_INB ) && ( defined HAVE_SYS_IO_H ) && ( defined __GNUC__ ) && ( ! defined __i386__ )
#define SANE_INB 4
static int
sanei_ioperm (int start, int length, int enable)
{
#ifdef HAVE_IOPERM
  return ioperm (start, length, enable);
#else
  /* linux without ioperm ? hum ... */
  /* makes compilers happy */
  start = length + enable;
  return 0;
#endif
}

static unsigned char
sanei_inb (unsigned int port)
{
  return inb (port);
}

static void
sanei_outb (unsigned int port, unsigned char value)
{
  outb (value, port);
}

static void
sanei_insb (unsigned int port, unsigned char *addr, unsigned long count)
{
  int i;

  for (i = 0; i < count; i++)
    addr[i] = sanei_inb (port);
}

static void
sanei_insl (unsigned int port, unsigned char *addr, unsigned long count)
{
  int i;

  for (i = 0; i < count * 4; i++)
    addr[i] = sanei_inb (port);
}

static void
sanei_outsb (unsigned int port, const unsigned char *addr,
	     unsigned long count)
{
  int i;

  for (i = 0; i < count; i++)
    sanei_outb (port, addr[i]);
}

static void
sanei_outsl (unsigned int port, const unsigned char *addr,
	     unsigned long count)
{
  int i;

  for (i = 0; i < count * 4; i++)
    sanei_outb (port, addr[i]);
}
#endif /* linux GCC non i386 */


/* ICC on i386 */
#if ( ! defined SANE_INB ) && ( defined __INTEL_COMPILER ) && ( defined __i386__ )
#define SANE_INB 5
static int
sanei_ioperm (int start, int length, int enable)
{
#ifdef HAVE_IOPERM
  return ioperm (start, length, enable);
#else
  /* ICC without ioperm() ... */
  /* makes compilers happy */
  start = length + enable;
  return 0;
#endif
}
static unsigned char
sanei_inb (unsigned int port)
{
  unsigned char ret;

  __asm__ __volatile__ ("inb %%dx,%%al":"=a" (ret):"d" ((u_int) port));
  return ret;
}

static void
sanei_outb (unsigned int port, unsigned char value)
{
  __asm__ __volatile__ ("outb %%al,%%dx"::"a" (value), "d" ((u_int) port));
}

static void
sanei_insb (unsigned int port, void *addr, unsigned long count)
{
  __asm__ __volatile__ ("rep ; insb":"=D" (addr), "=c" (count):"d" (port),
			"0" (addr), "1" (count));
}

static void
sanei_insl (unsigned int port, void *addr, unsigned long count)
{
  __asm__ __volatile__ ("rep ; insl":"=D" (addr), "=c" (count):"d" (port),
			"0" (addr), "1" (count));
}

static void
sanei_outsb (unsigned int port, const void *addr, unsigned long count)
{
  __asm__ __volatile__ ("rep ; outsb":"=S" (addr), "=c" (count):"d" (port),
			"0" (addr), "1" (count));
}

static void
sanei_outsl (unsigned int port, const void *addr, unsigned long count)
{
  __asm__ __volatile__ ("rep ; outsl":"=S" (addr), "=c" (count):"d" (port),
			"0" (addr), "1" (count));
}

#endif /* ICC on i386 */

/* direct io requested, but no valid inb/oub */
#if ( ! defined SANE_INB) && ( defined ENABLE_PARPORT_DIRECTIO )
#warning "ENABLE_PARPORT_DIRECTIO cannot be used du to lack of inb/out definition"
#undef ENABLE_PARPORT_DIRECTIO
#endif

#endif /* ENABLE_PARPORT_DIRECTIO */
/*
 * no inb/outb without --enable-parport-directio *
 */
#ifndef ENABLE_PARPORT_DIRECTIO
#define SANE_INB 0
static int
sanei_ioperm (int start, int length, int enable)
{
  /* make compilers happy */
  enable = start + length;

  /* returns failure */
  return -1;
}

static unsigned char
sanei_inb (unsigned int port)
{
  /* makes compilers happy */
  port = 0;
  return 255;
}

static void
sanei_outb (unsigned int port, unsigned char value)
{
  /* makes compilers happy */
  port = 0;
  value = 0;
}

static void
sanei_insb (unsigned int port, unsigned char *addr, unsigned long count)
{
  /* makes compilers happy */
  if (addr)
    {
      port = 0;
      count = 0;
    }
}

static void
sanei_insl (unsigned int port, unsigned char *addr, unsigned long count)
{
  /* makes compilers happy */
  if (addr)
    {
      port = 0;
      count = 0;
    }
}

static void
sanei_outsb (unsigned int port, const unsigned char *addr,
	     unsigned long count)
{
  /* makes compilers happy */
  if (addr)
    {
      port = 0;
      count = 0;
    }
}

static void
sanei_outsl (unsigned int port, const unsigned char *addr,
	     unsigned long count)
{
  /* makes compilers happy */
  if (addr)
    {
      port = 0;
      count = 0;
    }
}
#endif /* ENABLE_PARPORT_DIRECTIO is not defined */

/* we need either direct io or ppdev */
#if ! defined ENABLE_PARPORT_DIRECTIO && ! defined HAVE_LINUX_PPDEV_H && ! defined HAVE_DEV_PPBUS_PPI_H
#define IO_SUPPORT_MISSING
#endif


#include "umax_pp_low.h"

#ifdef DMALLOC
#include "dmalloc.h"
#endif

#ifndef __IO__
#define __IO__

#define DATA                   gPort+0x00
#define STATUS                 gPort+0x01
#define CONTROL                gPort+0x02
#define EPPADDR                 gPort+0x03
#define EPPDATA                gPort+0x04

#define ECPDATA                gPort+0x400
#define ECR		       gPort+0x402

#define FIFO_WAIT	      1000
#endif

static int fonc001 (void);
static int foncSendWord (int *cmd);

static void setEPPMode (int mode);
static int getEPPMode (void);
static void setModel (int model);
static int getModel (void);
static int ringScanner (int count, unsigned long delay);
static int testVersion (int no);

static int probePS2 (unsigned char *dest);
static int probeEPP (unsigned char *dest);
static int probeECP (unsigned char *dest);

static int sendCommand (int cmd);
static void SPPResetLPT (void);
static int sendWord (int *cmd);
static int sendData (int *cmd, int len);
static int receiveData (int *cmd, int len);
static int sendLength (int *cmd, int len);

static int waitAck (void);
static void init001 (void);
static int init002 (int arg);
static int init005 (int arg);

/* 610p comm functions */
static int putByte610p (int data);
static int EPPputByte610p (int data);
static int sendLength610p (int *cmd);
static int sendData610p (int *cmd, int len);
static int receiveData610p (int *cmd, int len);
static int connect610p (void);
static int sync610p (void);
static int cmdSync610p (int cmd);
static int EPPcmdSync610p (int cmd);
static int getStatus610p (void);
static int EPPgetStatus610p (void);
static int disconnect610p (void);
static int EPPsendWord610p (int *cmd);
static int SPPsendWord610p (int *cmd);
static int cmdSet610p (int cmd, int len, int *buffer);
static int cmdGet610p (int cmd, int len, int *buffer);
static int EPPcmdSet610p (int cmd, int len, int *buffer);
static int EPPcmdGet610p (int cmd, int len, int *buffer);
static int initScanner610p (int recover);
static int cmdGetBuffer610p (int cmd, int len, unsigned char *buffer);


/* parport mode setting */
static void compatMode (void);
static void byteMode (void);
static void ECPFifoMode (void);

/* block transfer init */
static void ECPSetBuffer (int size);

/* mode dependant operations */
static int PS2Something (int reg);
static void PS2bufferRead (int size, unsigned char *dest);
static void PS2bufferWrite (int size, unsigned char *source);
static int PS2registerRead (int reg);
static void PS2registerWrite (int reg, int value);

static int EPPconnect (void);
static int EPPregisterRead (int reg);
static void EPPregisterWrite (int reg, int value);
static void EPPbufferRead (int size, unsigned char *dest);
static void EPPbufferWrite (int size, unsigned char *source);
static void EPPRead32Buffer (int size, unsigned char *dest);
static void EPPWrite32Buffer (int size, unsigned char *source);

static int ECPconnect (void);
static void ECPdisconnect (void);
static int ECPregisterRead (int reg);
static void ECPregisterWrite (int reg, int value);
static int ECPbufferRead (int size, unsigned char *dest);
static void ECPbufferWrite (int size, unsigned char *source);
static int waitFifoEmpty (void);
static int waitFifoNotEmpty (void);
static int waitFifoFull (void);

/* generic operations */
static int connect (void);
static void disconnect (void);
static void bufferRead (int size, unsigned char *dest);
static void bufferWrite (int size, unsigned char *source);
static int pausedBufferRead (int size, unsigned char *dest);


static void ClearRegister (int reg);

static int connect_epat (int r08);
static int prologue (int r08);
static int epilogue (void);

static int cmdSet (int cmd, int len, int *buffer);
static int cmdGet (int cmd, int len, int *buffer);
static int cmdSetGet (int cmd, int len, int *buffer);


static int cmdGetBuffer (int cmd, int len, unsigned char *buffer);
static int cmdGetBuffer32 (int cmd, int len, unsigned char *buffer);
static int cmdGetBlockBuffer (int cmd, int len, int window,
			      unsigned char *buffer);

static void bloc2Decode (int *op);
static void bloc8Decode (int *op);
/*
 * high level operations
 */
static int loadDefaultTables (void);
static int inquire (void);
static int offsetCalibration (int color, int *offRed, int *offGreen,
			      int *offBlue);
static int coarseGainCalibration (int color, int dcRed, int dcGreen,
				  int dcBlue, int *vgaRed, int *vgaGreen,
				  int *vgaBlue);
static int
shadingCalibration (int color, int dcRed, int dcGreen, int dcBlue,
		    int vgaRed, int vgaGreen, int vgaBlue, int *calibration);
static int leftShadingCalibration610p (int color, int dcRed, int dcGreen,
				       int dcBlue, int vgaRed, int vgaGreen,
				       int vgaBlue, int *calibration);

#define WRITESLOW(x,y) \
        PS2registerWrite((x),(y)); \
        DBG(16,"PS2registerWrite(0x%X,0x%X) passed...   (%s:%d)\n",(x),(y),__FILE__,__LINE__);
#define SLOWNIBBLEREGISTERREAD(x,y) \
        tmp=PS2registerRead(x);\
        if(tmp!=y)\
        {\
                DBG(0,"PS2registerRead: found 0x%X expected 0x%X (%s:%d)\n",tmp,y,__FILE__,__LINE__);\
                /*return 0;*/ \
        }\
        DBG(16,"PS2registerRead(0x%X)=0x%X passed... (%s:%d)\n",x,y,__FILE__,__LINE__);
#define REGISTERWRITE(x,y) \
        registerWrite((x),(y)); \
        DBG(16,"registerWrite(0x%X,0x%X) passed...   (%s:%d)\n",(x),(y),__FILE__,__LINE__);
#define REGISTERREAD(x,y) \
        tmp=registerRead(x);\
        if(tmp!=y)\
        {\
                DBG(0,"registerRead, found 0x%X expected 0x%X (%s:%d)\n",tmp,y,__FILE__,__LINE__);\
                return 0;\
        }\
        DBG(16,"registerRead(0x%X)=0x%X passed... (%s:%d)\n",x,y,__FILE__,__LINE__);
#define TRACE(level,msg)        DBG(level, msg"  (%s:%d)\n",__FILE__,__LINE__);
#define CMDSYNC(x)        if(sanei_umax_pp_cmdSync(x)!=1)\
                        {\
                                DBG(0,"cmdSync(0x%02X) failed (%s:%d)\n",x,__FILE__,__LINE__);\
                                return 0;\
                        }\
                        DBG(16,"cmdSync(0x%02X)=%02X passed ... (%s:%d)\n",x,sanei_umax_pp_scannerStatus(),__FILE__,__LINE__)
#define CMDSETGET(cmd,len,sent) if(cmdSetGet(cmd,len,sent)!=1)\
                                {\
                                        DBG(0,"cmdSetGet(0x%02X,%d,sent) failed (%s:%d)\n",cmd,len,__FILE__,__LINE__);\
                                        return 0;\
                                }\
                                TRACE(16,"cmdSetGet() passed ...")
#define YOFFSET                40
#define YOFFSET1220P        40
#define YOFFSET2000P        40
#define COMPLETIONWAIT        if(completionWait()==0)\
                        {\
                                DBG(0,"completionWait() failed (%s:%d)\n",__FILE__,__LINE__);\
                                return 0;\
                        }\
                        TRACE(16,"completionWait() passed ...")
#define MOVE(x,y,t)        if(move(x,y,t)==0)\
                        {\
                                DBG(0,"move(%d,%d,buffer) failed (%s:%d)\n",x,y,__FILE__,__LINE__);\
                                return 0;\
                        }\
                        TRACE(16,"move() passed ...")
#define CMDGETBUF(cmd,len,sent) if(cmdGetBuffer(cmd,len,sent)!=1)\
                                {\
                                        DBG(0,"cmdGetBuffer(0x%02X,%ld,buffer) failed (%s:%d)\n",cmd,(long)len,__FILE__,__LINE__);\
                                        return 0;\
                                }\
                                DBG(16,"cmdGetBuffer(%ld) passed ... (%s:%d)\n",(long)len,__FILE__,__LINE__);
#define CMDGETBUF32(cmd,len,sent) if(cmdGetBuffer32(cmd,len,sent)!=1)\
                                {\
                                        DBG(0,"cmdGetBuffer32(0x%02X,%ld,buffer) failed (%s:%d)\n",cmd,(long)len,__FILE__,__LINE__);\
                                        return 0;\
                                }\
                                TRACE(16,"cmdGetBuffer32() passed ...")
#define CMDSET(cmd,len,sent) if(cmdSet(cmd,len,sent)!=1)\
                                {\
                                        DBG(0,"cmdSet(0x%02X,%d,sent) failed (%s:%d)\n",cmd,len,__FILE__,__LINE__);\
                                        return 0;\
                                }\
                                TRACE(16,"cmdSet() passed ...")
#define CMDGET(cmd,len,sent) if(cmdGet(cmd,len,sent)!=1)\
                                {\
                                        DBG(0,"cmdGet(0x%02X,%d,read) failed (%s:%d)\n",cmd,len,__FILE__,__LINE__);\
                                        return 0;\
                                }\
                                TRACE(16,"cmdGet() passed ...")
static int gPort = 0x378;
static float targetCode = 250.0;

/* global control vars */
static int gControl = 0;
static int gData = 0;
static int g674 = 0;		/* semble indiquer qu'on utilise les IRQ */
static int g67D = 0;
static int g67E = 0;
static int gEPAT = 0;		/* signals fast mode ? */
static int g6FE = 0;
static int gECP = 0;

static int gLeft = 144;		/* default value for 1220P */

/* default gamma translation table */
static int ggamma[256] =
  { 0x00, 0x06, 0x0A, 0x0D, 0x10, 0x12, 0x14, 0x17, 0x19, 0x1B, 0x1D,
  0x1F,
  0x21, 0x23, 0x24, 0x26, 0x28, 0x2A, 0x2B, 0x2D, 0x2E, 0x30, 0x31, 0x33,
  0x34, 0x36, 0x37, 0x39, 0x3A, 0x3B, 0x3D, 0x3E, 0x40, 0x41, 0x42, 0x43,
  0x45, 0x46, 0x47, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4F, 0x50, 0x51, 0x52,
  0x53, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5E, 0x5F, 0x60,
  0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C,
  0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
  0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82, 0x83, 0x84,
  0x85, 0x86, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
  0x90, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x97, 0x98, 0x99,
  0x9A, 0x9B, 0x9C, 0x9D, 0x9D, 0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA2, 0xA3,
  0xA4, 0xA5, 0xA6, 0xA7, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAC, 0xAD,
  0xAE, 0xAF, 0xB0, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB4, 0xB5, 0xB6, 0xB7,
  0xB8, 0xB8, 0xB9, 0xBA, 0xBB, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xBF, 0xC0,
  0xC1, 0xC2, 0xC2, 0xC3, 0xC4, 0xC5, 0xC5, 0xC6, 0xC7, 0xC8, 0xC8, 0xC9,
  0xCA, 0xCB, 0xCB, 0xCC, 0xCD, 0xCE, 0xCE, 0xCF, 0xD0, 0xD1, 0xD1, 0xD2,
  0xD3, 0xD4, 0xD4, 0xD5, 0xD6, 0xD6, 0xD7, 0xD8, 0xD9, 0xD9, 0xDA, 0xDB,
  0xDC, 0xDC, 0xDD, 0xDE, 0xDE, 0xDF, 0xE0, 0xE1, 0xE1, 0xE2, 0xE3, 0xE3,
  0xE4, 0xE5, 0xE6, 0xE6, 0xE7, 0xE8, 0xE8, 0xE9, 0xEA, 0xEA, 0xEB, 0xEC,
  0xEC, 0xED, 0xEE, 0xEF, 0xEF, 0xF0, 0xF1, 0xF1, 0xF2, 0xF3, 0xF3, 0xF4,
  0xF5, 0xF5, 0xF6, 0xF7, 0xF7, 0xF8, 0xF9, 0xF9, 0xFA, 0xFB, 0xFB, 0xFC,
  0xFD, 0xFE, 0xFE, 0xFF
};


/* default gamma translation table */
static int *ggGreen = ggamma;
static int *ggBlue = ggamma;
static int *ggRed = ggamma;
static int gParport = 0;
static int gCancel = 0;
static int gAutoSettings = 1;


/*****************************************************************************/
/*                    output one byte on given port                          */
/*****************************************************************************/
static void Outb (int port, int value);

/*****************************************************************************/
/*         ouput 'size' bytes stored in 'source' on given port               */
/*****************************************************************************/
static void Outsb (int port, unsigned char *source, int size);

/*****************************************************************************/
/*       ouput 'size' 32 bits words stored in 'source' on given port         */
/*****************************************************************************/
static void Outsw (int port, unsigned char *source, int size);


/*****************************************************************************/
/*                   input one byte from given port                          */
/*****************************************************************************/
static int Inb (int port);

/*****************************************************************************/
/*       input 'size' bytes from given port ans store them in 'dest'         */
/*****************************************************************************/
static void Insb (int port, unsigned char *dest, int size);

/*****************************************************************************/
/*     input 'size' 32 bits word from given port ans store them in 'dest'    */
/*****************************************************************************/
static void Insw (int port, unsigned char *dest, int size);



char **
sanei_parport_find_port (void)
{
  char **ports = NULL;
#ifdef ENABLE_PARPORT_DIRECTIO
  int i, addr, ecpaddr;
  int found = 0;
  char name[80], buffer[80];
  FILE *fic = NULL;

  /* direct I/O detection */
  /* linux 2.4 + 2.6 with proc support */
  for (i = 0; i < 4; i++)
    {
      /* try to ensure loading of lp module */
      sprintf (name, "/dev/lp%d", i);
      fic = fopen (name, "wb");
      if (fic != NULL)
	fclose (fic);
      sprintf (name, "/proc/sys/dev/parport/parport%d/base-addr", i);
      fic = fopen (name, "rb");
      if (fic != NULL)
	{
	  fread (buffer, 64, 1, fic);
	  fclose (fic);
	  if (sscanf (buffer, "%d %d", &addr, &ecpaddr) > 0)
	    {
	      DBG (16, "parport at 0x%X\n", addr);
	      ports =
		(char **) realloc (ports, (found + 2) * sizeof (char *));
	      ports[found] = (char *) malloc (19);
	      sprintf (ports[found], "0x%X", addr);
	      found++;
	      ports[found] = NULL;
	    }
	}
    }
#endif
  return ports;
}


char **
sanei_parport_find_device (void)
{
  char *devices[] = {
    /* FreeBSD */
    "/dev/ppi0",
    "/dev/ppi1",
    "/dev/ppi2",
    "/dev/ppi3",
    /* linux ppdev with devfs */
    "/dev/parports/0",
    "/dev/parports/1",
    "/dev/parports/2",
    "/dev/parports/3",
    /* linux ppdev */
    "/dev/parport0",
    "/dev/parport1",
    "/dev/parport2",
    "/dev/parport3",
    NULL
  };
  int i, file;
  int rc = 0;
  int found = 0;
  char **ports = NULL;


  /* device finding */
  i = 0;
  while (devices[i] != NULL)
    {
      DBG (16, "Controling %s: ", devices[i]);
      file = open (devices[i], O_RDWR);
      if (file < 0)
	{
	  switch (errno)
	    {
	    case ENOENT:
#ifdef ENIO
	    case ENXIO:
#endif
#ifdef ENODEV
	    case ENODEV:
#endif
	      DBG (16, "no %s device ...\n", devices[i]);
	      break;
	    case EACCES:
	      DBG (16, "current user cannot use existing %s device ...\n",
		   devices[i]);
	      break;
	    default:
	      perror (devices[i]);
	    }
	}
      else
	{
#ifdef HAVE_LINUX_PPDEV_H
	  /* on kernel < 2.4.23, you have to CLAIM the device 
	   * to check it really exists
	   * we may hang if another program already claimed it
	   */
	  rc = ioctl (file, PPCLAIM);
	  if (rc)
	    {
	      switch (errno)
		{
		case ENOENT:
#ifdef ENXIO
		case ENXIO:
#endif
#ifdef ENODEV
		case ENODEV:
#endif
		  DBG (16, "no %s device ...\n", devices[i]);
		  break;
		case EACCES:
		  DBG (16, "current user cannot use existing %s device ...\n",
		       devices[i]);
		  break;
		default:
		  DBG (16, "errno=%d\n", errno);
		  perror (devices[i]);
		}
	    }
	  else
	    {
	      rc = ioctl (file, PPRELEASE);
	    }
#endif /* HAVE_LINUX_PPDEV_H */
	  close (file);
	  if (!rc)
	    {
	      DBG (16, "adding %s to valid devices ...\n", devices[i]);
	      ports =
		(char **) realloc (ports, (found + 2) * sizeof (char *));
	      ports[found] = strdup (devices[i]);
	      found++;
	      ports[found] = NULL;
	    }
	}

      /* suite */
      i++;
    }
  return ports;
}



/*
 * gain direct acces to IO port, and set parport to the 'right' mode
 * returns 1 on success, 0 an failure
 */


int
sanei_umax_pp_initPort (int port, char *name)
{
  int fd, ectr;
  int found = 0, ecp = 1;
#if ((defined HAVE_IOPERM)||(defined HAVE_MACHINE_CPUFUNC_H)||(defined HAVE_LINUX_PPDEV_H))
  int mode, modes, rc;
#endif
#ifdef HAVE_LINUX_PPDEV_H
  char strmodes[160];
#endif

  /* since this function must be called before */
  /* any other, we put debug init here         */
  DBG_INIT ();
  DBG (1, "SANE_INB level %d\n", SANE_INB);

  /* sets global vars */
  ggGreen = ggamma;
  ggBlue = ggamma;
  ggRed = ggamma;
  gParport = 0;
  gCancel = 0;
  gAutoSettings = 1;
  gControl = 0;
  gData = 0;
  g674 = 0;
  g67D = 0;
  g67E = 0;
  gEPAT = 0;
  g6FE = 0;
  sanei_umax_pp_setparport (0);


  DBG (1, "sanei_umax_pp_InitPort(0x%X,%s)\n", port, name);
#ifndef ENABLE_PARPORT_DIRECTIO
  if ((name == NULL) || ((name != NULL) && (strlen (name) < 4)))
    {
      DBG (0, "sanei_umax_pp_InitPort cannot use direct hardware access\n");
      DBG (0, "if not compiled with --enable-parport-directio\n");
      return 0;
    }
#endif


  /* init global var holding port value */
  gPort = port;

#ifdef IO_SUPPORT_MISSING
  DBG (1, "*** Direct I/O or ppdev unavailable, giving up ***\n");
  return 0;
#else


#ifdef HAVE_LINUX_PPDEV_H
  if (name != NULL)
    {
      if (strlen (name) > 3)
	{
	  /* ppdev opening and configuration                               */
	  found = 0;
	  fd = open (name, O_RDWR | O_NOCTTY | O_NONBLOCK);
	  if (fd < 0)
	    {
	      switch (errno)
		{
		case ENOENT:
		  DBG (1, "umax_pp: '%s' does not exist \n", name);
		  break;
		case EACCES:
		  DBG (1,
		       "umax_pp: current user has not R/W permissions on '%s' \n",
		       name);
		  break;
		}
	      return 0;

	    }
	  /* claim port */
	  if (ioctl (fd, PPCLAIM))
	    {
	      DBG (1, "umax_pp: cannot claim port '%s'\n", name);
	    }
	  else
	    {
	      /* we check if parport does EPP or ECP */
#ifdef PPGETMODES
	      if (ioctl (fd, PPGETMODES, &modes))
		{
		  DBG (16,
		       "umax_pp: ppdev couldn't gave modes for port '%s'\n",
		       name);
		}
	      else
		{
		  sprintf (strmodes, "\n");
		  if (modes & PARPORT_MODE_PCSPP)
		    sprintf (strmodes, "%s\t\tPARPORT_MODE_PCSPP\n",
			     strmodes);
		  if (modes & PARPORT_MODE_TRISTATE)
		    sprintf (strmodes, "%s\t\tPARPORT_MODE_TRISTATE\n",
			     strmodes);
		  if (modes & PARPORT_MODE_EPP)
		    sprintf (strmodes, "%s\t\tPARPORT_MODE_EPP\n", strmodes);
		  if (modes & PARPORT_MODE_ECP)
		    {
		      sprintf (strmodes, "%s\t\tPARPORT_MODE_ECP\n",
			       strmodes);
		      gECP = 1;
		    }
		  if (modes & PARPORT_MODE_COMPAT)
		    sprintf (strmodes, "%s\t\tPARPORT_MODE_COMPAT\n",
			     strmodes);
		  if (modes & PARPORT_MODE_DMA)
		    sprintf (strmodes, "%s\t\tPARPORT_MODE_DMA\n", strmodes);
		  DBG (32, "parport modes: %X\n", modes);
		  DBG (32, "parport modes: %s\n", strmodes);
		  if (!(modes & PARPORT_MODE_EPP)
		      && !(modes & PARPORT_MODE_ECP))
		    {
		      DBG (1,
			   "port 0x%X does not have EPP or ECP, giving up ...\n",
			   port);
		      mode = IEEE1284_MODE_COMPAT;
		      ioctl (fd, PPSETMODE, &mode);
		      ioctl (fd, PPRELEASE);
		      close (fd);
		      return 0;
		    }
		}

#else
	      DBG (16,
		   "umax_pp: ppdev used to build SANE doesn't have PPGETMODES.\n");
	      /* faking result */
	      modes = 0xFFFFFFFF;
#endif
	      mode = 0;

	      /* prefered mode is EPP */
	      if (modes & PARPORT_MODE_EPP)
		{
		  mode = IEEE1284_MODE_EPP;

		  /* negot allways fail here ... */
		  rc = ioctl (fd, PPNEGOT, &mode);
		  if (rc)
		    {
		      DBG (16,
			   "umax_pp: ppdev couldn't negociate mode IEEE1284_MODE_EPP for '%s' (ignored)\n",
			   name);
		    }
		  if (ioctl (fd, PPSETMODE, &mode))
		    {
		      DBG (16,
			   "umax_pp: ppdev couldn't set mode to IEEE1284_MODE_EPP for '%s'\n",
			   name);
		      /* signal failure for ECP test */
		      mode = 0;
		    }
		  else
		    {
		      DBG (16,
			   "umax_pp: mode set to PARPORT_MODE_EPP for '%s'\n",
			   name);
		    }
		}

	      if ((modes & PARPORT_MODE_ECP) && (mode == 0))
		{
		  mode = IEEE1284_MODE_ECP;
		  rc = ioctl (fd, PPNEGOT, &mode);
		  if (rc)
		    {
		      DBG (16,
			   "umax_pp: ppdev couldn't negociate mode IEEE1284_MODE_ECP for '%s' (ignored)\n",
			   name);
		    }
		  if (ioctl (fd, PPSETMODE, &mode))
		    {
		      DBG (16,
			   "umax_pp: ppdev couldn't set mode to IEEE1284_MODE_ECP for '%s'\n",
			   name);
		      DBG (1,
			   "port 0x%X can't be set to EPP or ECP, giving up ...\n",
			   port);

		      mode = IEEE1284_MODE_COMPAT;
		      ioctl (fd, PPSETMODE, &mode);
		      ioctl (fd, PPRELEASE);
		      close (fd);
		      return 0;
		    }
		  else
		    {
		      gECP = 1;
		      DBG (16,
			   "umax_pp: mode set to PARPORT_MODE_ECP for '%s'\n",
			   name);
		    }
		}


	      /* allways start in compat mode (for probe) */
	      mode = IEEE1284_MODE_COMPAT;
	      rc = ioctl (fd, PPSETMODE, &mode);
	      if (rc)
		DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n",
		     strerror (errno), __FILE__, __LINE__);
	      mode = 0;		/* data forward */
	      rc = ioctl (fd, PPDATADIR, &mode);
	      if (rc)
		DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n",
		     strerror (errno), __FILE__, __LINE__);
	      mode = 1;		/* FW IDLE */
	      rc = ioctl (fd, PPSETPHASE, &mode);
	      if (rc)
		DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n",
		     strerror (errno), __FILE__, __LINE__);
	      found = 1;

	    }

	  if (!found)
	    {
	      DBG (1, "device %s does not fit ...\n", name);
	    }
	  else
	    {
	      DBG (1, "Using %s ...\n", name);
	      sanei_umax_pp_setparport (fd);
	      return 1;
	    }
	}
    }
#endif /* HAVE_LINUX_PPDEV_H */


#ifdef HAVE_DEV_PPBUS_PPI_H
/* the ppi device let user access to parallel port on freebsd */
  if (name != NULL)
    {
      if (strlen (name) > 3)
	{
	  /* ppi opening and configuration                               */
	  found = 0;
	  fd = open (name, O_RDONLY);
	  if (fd < 0)
	    {
	      switch (errno)
		{
		case ENOENT:
		  DBG (1, "umax_pp: '%s' does not exist \n", name);
		  break;
		case EACCES:
		  DBG (1,
		       "umax_pp: current user has not read permissions on '%s' \n",
		       name);
		  break;
		}
	      return 0;
	    }
	  else
	    {
	      DBG (1, "Using %s ...\n", name);
	      sanei_umax_pp_setparport (fd);
	      return 1;
	    }
	}
    }
#endif /* HAVE_DEV_PPBUS_PPI_H */

  if (port < 0x400)
    {
      if (sanei_ioperm (port, 8, 1) != 0)
	{
	  DBG (1, "sanei_ioperm() could not gain access to 0x%X\n", port);
	  return 0;
	}
      DBG (1, "sanei_ioperm(0x%X, 8, 1) OK ...\n", port);
    }

#ifdef HAVE_IOPERM
  /* ECP i/o range */
  if (iopl (3) != 0)
    {
      DBG (1, "iopl could not raise IO permission to level 3\n");
      DBG (1, "*NO* ECP support\n");
      ecp = 0;

    }
  else
    {
      /* any ECP out there ? */
      ectr = Inb (ECR);
      if (ectr != 0xFF)
	{
	  gECP = 1;

	}
    }
#else
  ecp = 0;
#endif



#endif /* IO_SUPPORT_MISSING */
  return 1;
}






static void
Outb (int port, int value)
{
#ifndef IO_SUPPORT_MISSING

#ifdef HAVE_LINUX_PPDEV_H
  int fd, rc, mode, exmode;
  unsigned char val;


  fd = sanei_umax_pp_getparport ();
  val = (unsigned char) value;
  if (fd > 0)
    {
      /* there should be ECR that doesn't go through ppdev */
      /* it will leave when all the I/O will be done with ppdev   */
      switch (port - gPort)
	{
	case 0:
	  rc = ioctl (fd, PPWDATA, &val);
	  if (rc)
	    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
#ifdef IOLOG
	  DBG (0, "outb DATA,%02X\n", value);
#endif
	  return;
	case 2:
	  mode = val & 0x20;
	  rc = ioctl (fd, PPDATADIR, &mode);
	  if (rc)
	    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n",
		 strerror (errno), __FILE__, __LINE__);
	  val = val & 0xDF;
	  rc = ioctl (fd, PPWCONTROL, &val);
	  if (rc)
	    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n",
		 strerror (errno), __FILE__, __LINE__);
#ifdef IOLOG
	  DBG (0, "outb CONTROL,%02X\n", value);
#endif
	  return;
	case 4:
	  rc = ioctl (fd, PPGETMODE, &exmode);
	  if (rc)
	    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  mode = 0;		/* data forward */
	  rc = ioctl (fd, PPDATADIR, &mode);
	  if (rc)
	    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
	  rc = ioctl (fd, PPSETMODE, &mode);
	  if (rc)
	    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  rc = write (fd, &val, 1);
	  if (rc != 1)
	    DBG (0, "ppdev short write (%s:%d)\n", __FILE__, __LINE__);
	  rc = ioctl (fd, PPSETMODE, &exmode);
	  if (rc)
	    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
#ifdef IOLOG
	  DBG (0, "outb EPPDATA,%02X\n", value);
#endif
	  return;
	case 3:
	  rc = ioctl (fd, PPGETMODE, &exmode);
	  if (rc)
	    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  mode = 0;		/* data forward */
	  rc = ioctl (fd, PPDATADIR, &mode);
	  if (rc)
	    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  mode = IEEE1284_MODE_EPP | IEEE1284_ADDR;
	  rc = ioctl (fd, PPSETMODE, &mode);
	  if (rc)
	    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  rc = write (fd, &val, 1);
	  if (rc != 1)
	    DBG (0, "ppdev short write (%s:%d)\n", __FILE__, __LINE__);
	  rc = ioctl (fd, PPSETMODE, &exmode);
	  if (rc)
	    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  return;
	case 0x400:
	case 0x402:
	  break;
	default:
	  DBG (16, "Outb(0x%03X,0x%02X) escaped ppdev\n", port, value);
	  return;
	}
    }
#endif /* HAVE_LINUX_PPDEV_H */


#ifdef HAVE_DEV_PPBUS_PPI_H
  int fd, rc;
  unsigned char val;


  fd = sanei_umax_pp_getparport ();
  val = (unsigned char) value;
  if (fd > 0)
    {
      switch (port - gPort)
	{
	case 0:
	  rc = ioctl (fd, PPISDATA, &val);
	  if (rc)
	    DBG (0, "ppi ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  return;
	case 1:
	  rc = ioctl (fd, PPISSTATUS, &val);
	  if (rc)
	    DBG (0, "ppi ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  return;
	case 2:
	  rc = ioctl (fd, PPISCTRL, &val);
	  if (rc)
	    DBG (0, "ppi ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  return;
	case 3:
	  rc = ioctl (fd, PPISEPPA, &val);
	  if (rc)
	    DBG (0, "ppi ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  return;
	case 4:
	  rc = ioctl (fd, PPISEPPD, &val);
	  if (rc)
	    DBG (0, "ppi ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  return;
	case 0x402:
	  rc = ioctl (fd, PPISECR, &val);
	  if (rc)
	    DBG (0, "ppi ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  return;
	default:
	  DBG (16, "Outb(0x%03X,0x%02X) escaped ppi\n", port, value);
	  return;
	}
    }
#endif /* HAVE_DEV_PPBUS_PPI_H */

  sanei_outb (port, value);

#endif /* IO_SUPPORT_MISSING */
}





static int
Inb (int port)
{
  int res = 0xFF;
#ifndef IO_SUPPORT_MISSING

#ifdef HAVE_LINUX_PPDEV_H
  int fd, rc, mode;
  unsigned char val;


  fd = sanei_umax_pp_getparport ();
  if (fd > 0)
    {
      /* there should be ECR that doesn't go through ppdev */
      /* it will leave when all the I/O will be done with ppdev   */
      switch (port - gPort)
	{
	case 0:
	  rc = ioctl (fd, PPRDATA, &val);
	  if (rc)
	    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  res = val;
#ifdef IOLOG
	  DBG (0, "inb  DATA,%02X\n", res);
#endif
	  return res;

	case 1:
	  rc = ioctl (fd, PPRSTATUS, &val);
	  if (rc)
	    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  res = val;
#ifdef IOLOG
	  DBG (0, "inb  STATUS,%02X\n", res);
#endif
	  return res;

	case 2:
	  rc = ioctl (fd, PPRCONTROL, &val);
	  if (rc)
	    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  res = val;
#ifdef IOLOG
	  DBG (0, "inb  CONTROL,%02X\n", res);
#endif
	  return res;

	case 4:
	  mode = 1;		/* data_reverse */
	  rc = ioctl (fd, PPDATADIR, &mode);
	  if (rc)
	    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
	  rc = ioctl (fd, PPSETMODE, &mode);
	  if (rc)
	    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  rc = read (fd, &val, 1);
	  if (rc != 1)
	    DBG (0, "ppdev short read (%s:%d)\n", __FILE__, __LINE__);
	  mode = 0;		/* data_forward */
	  rc = ioctl (fd, PPDATADIR, &mode);
	  if (rc)
	    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  res = val;
#ifdef IOLOG
	  DBG (0, "inb  EPPDATA,%02X\n", res);
#endif
	  return res;
	case 0x400:
	case 0x402:
	default:
	  DBG (16, "Inb(0x%03X) escaped ppdev\n", port);
	  return 0xFF;
	}
    }
#endif /* HAVE_LINUX_PPDEV_H */


#ifdef HAVE_DEV_PPBUS_PPI_H
  int fd, rc;
  unsigned char val;


  fd = sanei_umax_pp_getparport ();
  if (fd > 0)
    {
      switch (port - gPort)
	{
	case 0:
	  rc = ioctl (fd, PPIGDATA, &val);
	  if (rc)
	    DBG (0, "ppi ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  res = val;
	  return res;
	case 1:
	  rc = ioctl (fd, PPIGSTATUS, &val);
	  if (rc)
	    DBG (0, "ppi ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  res = val;
	  return res;
	case 2:
	  rc = ioctl (fd, PPIGCTRL, &val);
	  if (rc)
	    DBG (0, "ppi ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  res = val;
	  return res;
	case 3:
	  rc = ioctl (fd, PPIGEPPA, &val);
	  if (rc)
	    DBG (0, "ppi ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  res = val;
	  return res;
	case 4:
	  rc = ioctl (fd, PPIGEPPD, &val);
	  if (rc)
	    DBG (0, "ppi ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  res = val;
	  return res;
	case 0x402:
	  rc = ioctl (fd, PPIGECR, &val);
	  if (rc)
	    DBG (0, "ppi ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  res = val;
	  return res;
	default:
	  DBG (16, "Inb(0x%03X) escaped ppi\n", port);
	  return 0xFF;
	}
    }
#endif /* HAVE_DEV_PPBUS_PPI_H */

  res = sanei_inb (port);

#endif /* IO_SUPPORT_MISSING */
  return res;
}


static void
Insb (int port, unsigned char *dest, int size)
{
#ifndef IO_SUPPORT_MISSING

#ifdef HAVE_DEV_PPBUS_PPI_H
  int i;
  if (sanei_umax_pp_getparport () > 0)
    {
      for (i = 0; i < size; i++)
	dest[i] = Inb (port);
      return;
    }
#endif /* HAVE_DEV_PPBUS_PPI_H */
  sanei_insb (port, dest, size);

#endif
}

static void
Outsb (int port, unsigned char *source, int size)
{
#ifndef IO_SUPPORT_MISSING

#ifdef HAVE_DEV_PPBUS_PPI_H
  int i;

  if (sanei_umax_pp_getparport () > 0)
    {
      for (i = 0; i < size; i++)
	Outb (port, source[i]);
      return;
    }
#endif /* HAVE_DEV_PPBUS_PPI_H */

  sanei_outsb (port, source, size);

#endif
}



/* size = nb words */
static void
Insw (int port, unsigned char *dest, int size)
{
#ifndef IO_SUPPORT_MISSING

#ifdef HAVE_DEV_PPBUS_PPI_H
  int i;

  if (sanei_umax_pp_getparport () > 0)
    {
      for (i = 0; i < size * 4; i++)
	dest[i] = Inb (port);
      return;
    }
#endif /* HAVE_DEV_PPBUS_PPI_H */

  sanei_insl (port, dest, size);

#endif
}

static void
Outsw (int port, unsigned char *source, int size)
{
#ifndef IO_SUPPORT_MISSING

#ifdef HAVE_DEV_PPBUS_PPI_H
  int i;

  if (sanei_umax_pp_getparport () > 0)
    {
      for (i = 0; i < size * 4; i++)
	Outb (port, source[i]);
      return;
    }
#endif /* HAVE_DEV_PPBUS_PPI_H */

  sanei_outsl (port, source, size);

#endif
}


/* we're trying to gather information on the scanner here, */
/* and published it through an easy interface              */
/* will turn it into a struct when 610P code will be done  */
static int scannerStatus = 0;
static time_t gTime = 0;
static time_t gDelay = 0;
static int epp32 = 1;
static int gMode = 0;
static int gprobed = 0;
static int model = 0x15;
static int astra = 0;
static int hasUTA = 0;

/* signals that scan width matches full ccd width */
static int fullCCDWidth = 0;

int
sanei_umax_pp_getfull (void)
{
  return fullCCDWidth;
}

void
sanei_umax_pp_setfull (int mod)
{
  fullCCDWidth = mod;
}


int
sanei_umax_pp_UTA (void)
{
  return hasUTA;
}

int
sanei_umax_pp_scannerStatus (void)
{
  struct timeval tv;

  /* the 610P ASIC needs some time to settle down after probe */
  if ((gTime > 0) && (gDelay > 0))
    {
      gettimeofday (&tv, NULL);
      /* delay elapsed ? */
      if (tv.tv_sec - gTime < gDelay)
	/* still waiting */
	return ASIC_BIT;
      /* wait finished */
      gDelay = 0;
      gTime = 0;
    }

  /* 0x07 variant returns status with bit 0 or 1 allways set to 1 */
  /* so we mask it out                                            */
  return scannerStatus & 0xFC;
}

static int
getEPPMode (void)
{
  if (epp32)
    return 32;
  return 8;
}

static void
setEPPMode (int mode)
{
  if (mode == 8)
    epp32 = 0;
  else
    epp32 = 1;
}

static int
getModel (void)
{
  return model;
}

static void
setModel (int mod)
{
  model = mod;
}


int
sanei_umax_pp_getparport (void)
{
  return gParport;
}

void
sanei_umax_pp_setparport (int fd)
{
  gParport = fd;
}

int
sanei_umax_pp_getport (void)
{
  return gPort;
}

void
sanei_umax_pp_setport (int port)
{
  gPort = port;
}

int
sanei_umax_pp_getastra (void)
{
  return astra;
}

void
sanei_umax_pp_setastra (int mod)
{
  astra = mod;
}

int
sanei_umax_pp_getLeft (void)
{
  switch (sanei_umax_pp_getastra ())
    {
    case 610:
      return 92;
    default:
      return 144;
    }
}

void
sanei_umax_pp_setLeft (int mod)
{
  gLeft = mod;
}

int
sanei_umax_pp_getauto (void)
{
  return gAutoSettings;
}

void
sanei_umax_pp_setauto (int autoset)
{
  gAutoSettings = autoset;
}

#ifdef HAVE_LINUX_PPDEV_H
/* set to the parallel port needed using ppdev 
 * returns 1 if ok, 0 else 
 */
static int
ppdev_set_mode (int mode)
{
  int fd, rc;

  /* check we have ppdev working */
  fd = sanei_umax_pp_getparport ();
  if (fd > 0)
    {
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	{
	  DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	       __FILE__, __LINE__);
	  return 0;
	}
      return 1;
    }
  return 0;
}
#endif

/* set parallel port mode to 'compatible'*/
static void
compatMode (void)
{
#ifdef HAVE_LINUX_PPDEV_H
  if (ppdev_set_mode (IEEE1284_MODE_COMPAT))
    return;
#endif
  if (!gECP)
    return;
  Outb (ECR, 0x15);
}

/* set parallel port mode to 'bidirectionel'*/
static void
byteMode (void)
{
#ifdef HAVE_LINUX_PPDEV_H
  if (ppdev_set_mode (IEEE1284_MODE_BYTE))
    return;
#endif
  if (!gECP)
    return;
  Outb (ECR, 0x35);		/* or 0x34 */
}

/* set parallel port mode to 'fifo'*/
static void
ECPFifoMode (void)
{
  /* bits 7:5 :
     000 Standard Mode
     001 Byte Mode
     010 Parallel Port FIFO Mode
     011 ECP FIFO Mode
     100 EPP Mode
     101 Reserved
     110 FIFO Test Mode
     111 Configuration Mode
   */
  /* logged as 74/75 */
#ifdef HAVE_LINUX_PPDEV_H
  if (ppdev_set_mode (IEEE1284_MODE_ECP))
    return;
#endif
  if (!gECP)
    return;
  Outb (ECR, 0x75);
}

/* wait for ack bit */
/* return 1 on success, 0 on error */
static int
waitAck ()
{
  unsigned char breg = 0;
  int i = 0;

  Outb (CONTROL, 0x0C);		/* select printer + initialize printer */
  Outb (CONTROL, 0x0C);
  Outb (CONTROL, 0x0C);
  breg = Inb (STATUS) & 0xF8;
  while ((i < 1024) && ((breg & 0x04) == 0))
    {
      Outb (CONTROL, 0x0E);	/* autolinefeed ?.. */
      Outb (CONTROL, 0x0E);
      Outb (CONTROL, 0x0E);
      breg = Inb (STATUS) & 0xF8;
      i++;
      usleep (1000);
    }
  if (i == 1024)
    {
      DBG (1, "waitAck failed, time-out waiting for Ack (%s:%d)\n",
	   __FILE__, __LINE__);
      /* return 0; seems to be non-blocking ... */
    }
  Outb (CONTROL, 0x04);		/* printer reset */
  Outb (CONTROL, 0x04);
  Outb (CONTROL, 0x04);
  return 1;
}

static int
waitFifoEmpty (void)
{
  int i;
  unsigned char breg;

  breg = Inb (ECR);
  i = 0;
  while ((i < FIFO_WAIT) && ((breg & 0x01) == 0))
    {
      breg = Inb (ECR);
      i++;
    }
  if (i == FIFO_WAIT)
    {
      DBG (0, "waitFifoEmpty failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return 0;
    }
  return 1;
}

static int
waitFifoNotEmpty (void)
{
  int i;
  unsigned char breg;

  breg = Inb (ECR);
  i = 0;
  while ((i < FIFO_WAIT) && ((breg & 0x01) != 0))
    {
      breg = Inb (ECR);
      i++;
      /* usleep (2000); */
    }
  if (i == FIFO_WAIT)
    {
      DBG (0, "waitFifoNotEmpty failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return 0;
    }
  return 1;
}


static int
waitFifoFull (void)
{
  int i;
  unsigned char breg;

  breg = Inb (ECR);

  /* two wait loop */
  /* the first apply to fast data transfer (ie when no scaaning is undergoing) */
  /* and we fallback to the second in case the scanner is answering slowly */
  i = 0;
  while ((i < FIFO_WAIT) && ((breg & 0x02) == 0))
    {
      breg = Inb (ECR);
      i++;
    }
  /* don't need to wait any longer */
  if (i < FIFO_WAIT)
    return 1;
  i = 0;
  while ((i < FIFO_WAIT) && ((breg & 0x02) == 0))
    {
      breg = Inb (ECR);
      i++;
      usleep (500);
    }
  if (i == FIFO_WAIT)
    {
      DBG (0, "waitFifoFull failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return 0;
    }
  return 1;
}

/*
 * surely some register reading in PS2 mode
 * only one nibble is accessed, may be
 * PS2RegisterLowNibbleRead(reg)
 */
static int
PS2Something (int reg)
{
  unsigned char breg, low, high = 0;

  Outb (CONTROL, 0x04);
  Outb (DATA, reg);		/* register number ? */
  Outb (CONTROL, 0x06);
  Outb (CONTROL, 0x06);
  Outb (CONTROL, 0x06);
  breg = Inb (STATUS) & 0xF8;
  low = breg;
  breg = breg & 0x08;
  /* surely means register(0x10)=0x0B */
  /* since reg & 0x08 != 0, high and low nibble
   * differ, but we don't care, since we surely expect it
   * to be 0
   */
  if (breg != 0x08)
    {
      DBG (0, "PS2Something failed, expecting 0x08, got 0x%02X (%s:%d)\n",
	   breg, __FILE__, __LINE__);
    }
  Outb (CONTROL, 0x07);
  Outb (CONTROL, 0x07);
  Outb (CONTROL, 0x07);
  Outb (CONTROL, 0x07);
  Outb (CONTROL, 0x07);
  Outb (CONTROL, 0x04);
  Outb (CONTROL, 0x04);
  Outb (CONTROL, 0x04);
  if (breg != 0x08)
    high = Inb (STATUS) & 0xF0;
  return high + ((low & 0xF0) >> 4);
}

static int
PS2Read (void)
{
  int res;
  int tmp;

  res = Inb (STATUS);
  res = Inb (STATUS);
  res = res & 0xF0;
  Outb (CONTROL, 5);
  Outb (CONTROL, 5);
  Outb (CONTROL, 5);
  Outb (CONTROL, 5);
  Outb (CONTROL, 5);
  Outb (CONTROL, 5);
  Outb (CONTROL, 4);
  Outb (CONTROL, 4);
  Outb (CONTROL, 4);
  Outb (CONTROL, 4);
  Outb (CONTROL, 4);
  Outb (CONTROL, 4);

  tmp = Inb (STATUS);
  tmp = Inb (STATUS);
  tmp = (tmp & 0xF0) >> 4;
  res = res | tmp;
  Outb (CONTROL, 5);
  Outb (CONTROL, 5);
  Outb (CONTROL, 5);
  Outb (CONTROL, 5);
  Outb (CONTROL, 5);
  Outb (CONTROL, 5);
  Outb (CONTROL, 4);
  Outb (CONTROL, 4);
  Outb (CONTROL, 4);
  Outb (CONTROL, 4);
  Outb (CONTROL, 4);
  Outb (CONTROL, 4);

  return res;
}


/******************************************************************************/
/* PS2registerWrite: write value in register, slow method                            */
/******************************************************************************/
static void
PS2registerWrite (int reg, int value)
{
  /* select register */
  Outb (DATA, reg | 0x60);
  Outb (DATA, reg | 0x60);
  Outb (CONTROL, 0x01);
  Outb (CONTROL, 0x01);
  Outb (CONTROL, 0x01);

  /* send value */
  Outb (DATA, value);
  Outb (DATA, value);
  Outb (CONTROL, 0x04);
  Outb (CONTROL, 0x04);
  Outb (CONTROL, 0x04);
}



/*****************************************************************************/
/* Send command returns 0 on failure, 1 on success                           */
/*****************************************************************************/
static int
sendCommand (int cmd)
{
  int control;
  int tmp;
  int val;
  int i;
  int gbufferRead[256];		/* read buffer for command 0x10 */


  if (g674 != 0)
    {
      DBG (0, "No scanner attached, sendCommand(0x%X) failed\n", cmd);
      return 0;
    }

  control = Inb (CONTROL) & 0x3F;
  tmp = cmd & 0xF8;


  if ((g67D != 1) && (tmp != 0xE0) && (tmp != 0x20) && (tmp != 0x40)
      && (tmp != 0xD0) && (tmp != 0x08) && (tmp != 0x48))
    {
      Outb (CONTROL, 4);	/* reset printer */
    }
  else
    {
      val = control & 0x1F;
      if (g67D != 1)
	val = val & 0xF;	/* no IRQ */
      val = val | 0x4;
      Outb (CONTROL, val);
      Outb (CONTROL, val);
    }

  /* send sequence */
  Outb (DATA, 0x22);
  Outb (DATA, 0x22);
  Outb (DATA, 0xAA);
  Outb (DATA, 0xAA);
  Outb (DATA, 0x55);
  Outb (DATA, 0x55);
  Outb (DATA, 0x00);
  Outb (DATA, 0x00);
  Outb (DATA, 0xFF);
  Outb (DATA, 0xFF);
  Outb (DATA, 0x87);
  Outb (DATA, 0x87);
  Outb (DATA, 0x78);
  Outb (DATA, 0x78);
  Outb (DATA, cmd);
  Outb (DATA, cmd);

  cmd = cmd & 0xF8;

  if ((g67D == 1) && (cmd == 0xE0))
    {
      val = Inb (CONTROL);
      val = (val & 0xC) | 0x01;
      Outb (CONTROL, val);
      Outb (CONTROL, val);
      val = val & 0xC;
      Outb (CONTROL, val);
      Outb (CONTROL, val);
      goto sendCommandEnd;
    }

  if ((cmd != 8) && (cmd != 0x48))
    {
      tmp = Inb (CONTROL);
      tmp = Inb (CONTROL);
      tmp = tmp & 0x1E;
      if (g67D != 1)
	tmp = tmp & 0xE;
      Outb (CONTROL, tmp);
      Outb (CONTROL, tmp);
    }

  if (cmd == 0x10)
    {
      tmp = PS2Read ();
      tmp = tmp * 256 + PS2Read ();
      goto sendCommandEnd;
    }

  if (cmd == 8)
    {
      if (g67D == 1)
	{
	  i = 0;
	  while (i < g67E)
	    {
	      tmp = Inb (CONTROL);
	      tmp = Inb (CONTROL);
	      tmp = (tmp & 0x1E) | 0x1;
	      Outb (CONTROL, tmp);
	      Outb (CONTROL, tmp);
	      gbufferRead[i] = Inb (STATUS);
	      tmp = tmp & 0x1E;
	      Outb (CONTROL, tmp);
	      Outb (CONTROL, tmp);

	      /* next read */
	      i++;
	      if (i < g67E)
		{
		  Outb (DATA, i | 8);
		  Outb (DATA, i | 8);
		}
	    }
	  goto sendCommandEnd;
	}
      else
	{
	  DBG (0, "UNEXPLORED BRANCH %s:%d\n", __FILE__, __LINE__);
	  return 0;
	}
    }

  /*  */
  if (cmd == 0)
    {
      i = 0;
      do
	{
	  tmp = Inb (CONTROL);
	  tmp = (tmp & 0xE) | 0x1;
	  Outb (CONTROL, tmp);
	  Outb (CONTROL, tmp);
	  tmp = tmp & 0xE;
	  Outb (CONTROL, tmp);
	  Outb (CONTROL, tmp);

	  i++;
	  if (i < g67E)
	    {
	      Outb (DATA, i);
	      Outb (DATA, i);
	    }
	}
      while (i < g67E);
      goto sendCommandEnd;
    }

  if (cmd == 0x48)
    {
      /* this case is unneeded, should fit with the rest */
      tmp = Inb (CONTROL) & 0x1E;
      if (g67D != 1)
	tmp = tmp & 0xE;
      Outb (CONTROL, tmp | 0x1);
      Outb (CONTROL, tmp | 0x1);
      Outb (CONTROL, tmp);
      Outb (CONTROL, tmp);
      goto sendCommandEnd;
    }

  /*  */
  tmp = Inb (CONTROL) & 0x1E;
  if (g67D != 1)
    tmp = tmp & 0xE;
  Outb (CONTROL, tmp | 0x1);
  Outb (CONTROL, tmp | 0x1);
  Outb (CONTROL, tmp);
  Outb (CONTROL, tmp);

  /* success */
sendCommandEnd:

  if (cmd == 0x48)
    Outb (CONTROL, (control & 0xF) | 0x4);
  if (cmd == 0x30)
    Outb (CONTROL, (gControl & 0xF) | 0x4);

  /* end signature */
  Outb (DATA, 0xFF);
  Outb (DATA, 0xFF);

  if (cmd == 8)
    {
      Outb (CONTROL, control);
      return 1;
    }

  if (cmd == 0x30)
    {
      Outb (CONTROL, gControl);
      return 1;
    }

  if (cmd != 0xE0)
    Outb (CONTROL, control);

  return 1;
}


static void
ClearRegister (int reg)
{

  /* choose register */
  Outb (DATA, reg);
  Outb (DATA, reg);
  Outb (CONTROL, 1);
  Outb (CONTROL, 1);
  if ((g6FE == 0) || (g674 != 0))
    {
      Outb (CONTROL, 1);
      Outb (CONTROL, 1);
      Outb (CONTROL, 1);
      Outb (CONTROL, 1);
    }

  /* clears it by not sending new value */
  Outb (CONTROL, 4);
  Outb (CONTROL, 4);
  Outb (CONTROL, 4);
  Outb (CONTROL, 4);
}

static void
SPPResetLPT (void)
{
  Outb (CONTROL, 0x04);
}


static int
PS2registerRead (int reg)
{
  int low, high;


  /* send register number */
  Outb (DATA, reg);
  Outb (DATA, reg);

  /* get low nibble */
  Outb (CONTROL, 1);
  Outb (CONTROL, 3);
  Outb (CONTROL, 3);
  Outb (CONTROL, 3);
  Outb (CONTROL, 3);
  low = Inb (STATUS);
  low = Inb (STATUS);

  /* get high nibble */
  Outb (CONTROL, 4);
  Outb (CONTROL, 4);
  Outb (CONTROL, 4);
  high = Inb (STATUS);
  high = Inb (STATUS);

  /* merge nibbles and return */
  high = (high & 0xF0) | ((low & 0xF0) >> 4);
  return high;
}


static void
PS2bufferRead (int size, unsigned char *dest)
{
  int high;
  int low;
  int i;
  int count;
  int bytel, byteh;

  /* init transfer */
  Outb (DATA, 7);
  Outb (DATA, 7);
  Outb (CONTROL, 1);
  Outb (CONTROL, 1);
  Outb (CONTROL, 3);
  Outb (CONTROL, 3);
  Outb (CONTROL, 3);
  Outb (DATA, 0xFF);
  Outb (DATA, 0xFF);
  count = (size - 2) / 2;
  i = 0;
  bytel = 0x06;			/* signals low byte of word  */
  byteh = 0x07;			/* signals high byte of word */

  /* read loop */
  while (count > 0)
    {
      /* low nibble byte 0 */
      Outb (CONTROL, bytel);
      Outb (CONTROL, bytel);
      Outb (CONTROL, bytel);
      low = Inb (STATUS);
      if ((low & 0x08) == 0)
	{
	  /* high nibble <> low nibble */
	  Outb (CONTROL, bytel & 0x05);
	  Outb (CONTROL, bytel & 0x05);
	  Outb (CONTROL, bytel & 0x05);
	  high = Inb (STATUS);
	}
      else
	{
	  high = low;
	}
      low = low & 0xF0;
      high = high & 0xF0;
      dest[i] = (unsigned char) (high | (low >> 4));
      i++;

      /* low nibble byte 1 */
      Outb (CONTROL, byteh);
      Outb (CONTROL, byteh);
      Outb (CONTROL, byteh);
      low = Inb (STATUS);
      if ((low & 0x08) == 0)
	{
	  /* high nibble <> low nibble */
	  Outb (CONTROL, byteh & 0x05);
	  Outb (CONTROL, byteh & 0x05);
	  Outb (CONTROL, byteh & 0x05);
	  high = Inb (STATUS);
	}
      else
	{
	  high = low;
	}
      low = low & 0xF0;
      high = high & 0xF0;
      dest[i] = (unsigned char) (high | (low >> 4));
      i++;

      /* next read */
      count--;
    }

  /* final read        */
  /* low nibble byte 0 */
  Outb (CONTROL, bytel);
  Outb (CONTROL, bytel);
  Outb (CONTROL, bytel);
  low = Inb (STATUS);
  if ((low & 0x08) == 0)
    {
      /* high nibble <> low nibble */
      Outb (CONTROL, bytel & 0x05);
      Outb (CONTROL, bytel & 0x05);
      Outb (CONTROL, bytel & 0x05);
      high = Inb (STATUS);
    }
  else
    {
      high = low;
    }
  low = low & 0xF0;
  high = high & 0xF0;
  dest[i] = (unsigned char) (high | (low >> 4));
  i++;

  /* uneven size need an extra read */
  if ((size & 0x01) == 1)
    {
      /* low nibble byte 1 */
      Outb (CONTROL, byteh);
      Outb (CONTROL, byteh);
      Outb (CONTROL, byteh);
      low = Inb (STATUS);
      if ((low & 0x08) == 0)
	{
	  /* high nibble <> low nibble */
	  Outb (CONTROL, byteh & 0x05);
	  Outb (CONTROL, byteh & 0x05);
	  Outb (CONTROL, byteh & 0x05);
	  high = Inb (STATUS);
	}
      else
	{
	  high = low;
	}
      low = low & 0xF0;
      high = high & 0xF0;
      dest[i] = (unsigned char) (high | (low >> 4));
      i++;

      /* swap high/low word */
      count = bytel;
      bytel = byteh;
      byteh = count;
    }

  /* final byte ... */
  Outb (DATA, 0xFD);
  Outb (DATA, 0xFD);
  Outb (DATA, 0xFD);

  /* low nibble */
  Outb (CONTROL, byteh);
  Outb (CONTROL, byteh);
  Outb (CONTROL, byteh);
  low = Inb (STATUS);
  if ((low & 0x08) == 0)
    {
      /* high nibble <> low nibble */
      Outb (CONTROL, byteh & 0x05);
      Outb (CONTROL, byteh & 0x05);
      Outb (CONTROL, byteh & 0x05);
      high = Inb (STATUS);
    }
  else
    {
      high = low;
    }
  low = low & 0xF0;
  high = high & 0xF0;
  dest[i] = (unsigned char) (high | (low >> 4));
  i++;

  /* reset port */
  Outb (DATA, 0x00);
  Outb (DATA, 0x00);
  Outb (CONTROL, 0x04);
}

static void
PS2bufferWrite (int size, unsigned char *source)
{
  int i;
  int count;
  int val;

  /* init buffer write */
  i = 0;
  count = size / 2;
  Outb (DATA, 0x67);
  Outb (CONTROL, 0x01);
  Outb (CONTROL, 0x01);
  Outb (CONTROL, 0x05);

  /* write loop */
  while (count > 0)
    {
      /* low byte of word */
      val = source[i];
      i++;
      Outb (DATA, val);
      Outb (DATA, val);
      Outb (CONTROL, 0x04);
      Outb (CONTROL, 0x04);
      Outb (CONTROL, 0x04);
      Outb (CONTROL, 0x04);

      /* high byte of word */
      val = source[i];
      i++;
      Outb (DATA, val);
      Outb (DATA, val);
      Outb (CONTROL, 0x05);
      Outb (CONTROL, 0x05);
      Outb (CONTROL, 0x05);
      Outb (CONTROL, 0x05);

      /* next write */
      count--;
    }

  /* termination sequence */
  Outb (CONTROL, 0x05);
  Outb (CONTROL, 0x05);
  Outb (CONTROL, 0x05);
  Outb (CONTROL, 0x05);
  Outb (CONTROL, 0x07);
  Outb (CONTROL, 0x07);
  Outb (CONTROL, 0x07);
  Outb (CONTROL, 0x07);
  Outb (CONTROL, 0x04);
  Outb (CONTROL, 0x04);
}




static void
init001 (void)
{
  int i;
  int val;
  int status;

  ClearRegister (0);
  Outb (CONTROL, 0x0C);
  if (g674 != 0)
    {
      Outb (CONTROL, 0x0C);
      Outb (CONTROL, 0x0C);
      Outb (CONTROL, 0x0C);
    }
  Outb (DATA, 0x40);
  if (g674 != 0)
    {
      Outb (DATA, 0x40);
      Outb (DATA, 0x40);
      Outb (DATA, 0x40);
    }
  Outb (CONTROL, 0x06);
  Outb (CONTROL, 0x06);
  Outb (CONTROL, 0x06);
  if (g674 != 0)
    {
      Outb (CONTROL, 0x06);
      Outb (CONTROL, 0x06);
      Outb (CONTROL, 0x06);
    }

  /* sync loop */
  i = 256;
  do
    {
      status = Inb (STATUS);
      i--;
    }
  while ((i > 0) && ((status & 0x40)));
  val = 0x0C;
  if (i > 0)
    {
      Outb (CONTROL, 0x07);
      Outb (CONTROL, 0x07);
      Outb (CONTROL, 0x07);
      if (g674 != 0)
	{
	  Outb (CONTROL, 0x07);
	  Outb (CONTROL, 0x07);
	  Outb (CONTROL, 0x07);
	}
      val = 0x04;
      Outb (CONTROL, val);
      Outb (CONTROL, val);
      Outb (CONTROL, val);
      if (g674 != 0)
	{
	  Outb (CONTROL, val);
	  Outb (CONTROL, val);
	  Outb (CONTROL, val);
	}
    }
  val = val | 0x0C;
  Outb (CONTROL, val);
  Outb (CONTROL, val);
  Outb (CONTROL, val);
  if (g674 != 0)
    {
      Outb (CONTROL, val);
      Outb (CONTROL, val);
      Outb (CONTROL, val);
    }
  val = val & 0xF7;
  Outb (CONTROL, val);
  Outb (CONTROL, val);
  Outb (CONTROL, val);
  if (g674 != 0)
    {
      Outb (CONTROL, val);
      Outb (CONTROL, val);
      Outb (CONTROL, val);
    }
}

/* SPP register reading */
static int
init002 (int arg)
{
  int data;

  if (arg == 1)
    return 0;
  Outb (DATA, 0x0B);
  Outb (CONTROL, 0x04);
  Outb (CONTROL, 0x04);
  Outb (CONTROL, 0x04);
  Outb (CONTROL, 0x0C);
  Outb (CONTROL, 0x0C);
  Outb (CONTROL, 0x0C);
  Outb (CONTROL, 0x04);
  Outb (CONTROL, 0x24);
  Outb (CONTROL, 0x24);
  Outb (CONTROL, 0x26);
  Outb (CONTROL, 0x26);

  data = Inb (DATA);
  Outb (CONTROL, 0x04);
  if (data == gEPAT)
    {
      return 1;
    }
  return 0;
}

/*
 * connecct to the EPAT chip, and
 * prepare command sending
 */
static int
ECPconnect (void)
{
  int ret, control, data;

  /* these 3 lines set to 'inital mode' */
  byteMode ();			/*Outb (ECR, 0x20); */
  Outb (DATA, 0x04);		/* gData */
  Outb (CONTROL, 0x0C);		/* gControl */

  Inb (ECR);			/* 0x35 */
  byteMode ();			/*Outb (ECR, 0x20); */
  byteMode ();			/*Outb (ECR, 0x20); */

  gData = Inb (DATA);
  gControl = Inb (CONTROL);

  data = Inb (DATA);
  control = Inb (CONTROL);
  Outb (CONTROL, control & 0x1F);
  control = Inb (CONTROL);
  Outb (CONTROL, control & 0x1F);
  sendCommand (0xE0);

  Outb (DATA, 0xFF);
  Outb (DATA, 0xFF);
  ClearRegister (0);
  Outb (CONTROL, 0x0C);
  Outb (CONTROL, 0x04);
  ClearRegister (0);
  ret = PS2Something (0x10);
  if (ret != 0x0B)
    {
      DBG (16,
	   "PS2Something returned 0x%02X, 0x0B expected (%s:%d)\n", ret,
	   __FILE__, __LINE__);
    }
  return 1;
}

static void
EPPdisconnect (void)
{
  if (getModel () != 0x07)
    sendCommand (40);
  sendCommand (30);
  Outb (DATA, gData);
  Outb (CONTROL, gControl);
}

static void
ECPdisconnect (void)
{
  int control;

  if (getModel () != 0x07)	/* guessed */
    sendCommand (40);		/* guessed */
  sendCommand (0x30);
  control = Inb (CONTROL) | 0x01;
  Outb (CONTROL, control);
  Outb (CONTROL, control);
  control = control & 0x04;
  Outb (CONTROL, control);
  Outb (CONTROL, control);
  control = control | 0x08;
  Outb (CONTROL, control);
  Outb (DATA, 0xFF);
  Outb (DATA, 0xFF);
  Outb (CONTROL, control);
}

static int
ECPregisterRead (int reg)
{
  unsigned char breg, value;

#ifdef HAVE_LINUX_PPDEV_H
  int rc, mode, fd;
  unsigned char bval;

  fd = sanei_umax_pp_getparport ();
  if (fd > 0)
    {
      Outb (CONTROL, 0x04);
      ECPFifoMode ();
      Outb (DATA, reg);
      mode = 1;			/* data_reverse */
      rc = ioctl (fd, PPDATADIR, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      rc = read (fd, &bval, 1);
      if (rc != 1)
	DBG (0, "ppdev short read (%s:%d)\n", __FILE__, __LINE__);
      value = bval;
      breg = (Inb (CONTROL) & 0x3F);
      if (breg != 0x20)
	{
	  DBG (0,
	       "ECPregisterRead failed, expecting 0x20, got 0x%02X (%s:%d)\n",
	       breg, __FILE__, __LINE__);
	}

      /* restore port state */
      mode = 0;			/* data_forward */
      rc = ioctl (fd, PPDATADIR, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      Outb (CONTROL, 0x04);
      byteMode ();
      return value;
    }
#endif

  Outb (CONTROL, 0x4);

  /* ECP FIFO mode, interrupt bit, dma disabled, 
     service bit, fifo full=0, fifo empty=0 */
  ECPFifoMode ();		/*Outb (ECR, 0x60); */
  if (waitFifoEmpty () == 0)
    {
      DBG (0, "ECPregisterRead failed, FIFO time-out (%s:%d)\n",
	   __FILE__, __LINE__);
    }
  breg = Inb (ECR);

  Outb (DATA, reg);
  if (waitFifoEmpty () == 0)
    {
      DBG (0, "ECPregisterRead failed, FIFO time-out (%s:%d)\n",
	   __FILE__, __LINE__);
    }
  breg = Inb (ECR);

  /* byte mode, interrupt bit, dma disabled, 
     service bit, fifo full=0, fifo empty=0 */
  byteMode ();			/*Outb (ECR, 0x20); */
  Outb (CONTROL, 0x20);		/* data reverse */

  /* ECP FIFO mode, interrupt bit, dma disabled, 
     service bit, fifo full=0, fifo empty=0 */
  ECPFifoMode ();		/*Outb (ECR, 0x60); */
  if (waitFifoNotEmpty () == 0)
    {
      DBG (0, "ECPregisterRead failed, FIFO time-out (%s:%d)\n",
	   __FILE__, __LINE__);
    }
  breg = Inb (ECR);
  value = Inb (ECPDATA);

  /* according to the spec bit 7 and 6 are unused */
  breg = (Inb (CONTROL) & 0x3F);
  if (breg != 0x20)
    {
      DBG (0, "ECPregisterRead failed, expecting 0x20, got 0x%02X (%s:%d)\n",
	   breg, __FILE__, __LINE__);
    }
  Outb (CONTROL, 0x04);
  byteMode ();
  return value;
}

static int
EPPregisterRead (int reg)
{
#ifdef HAVE_LINUX_PPDEV_H
  int fd, mode, rc;
  unsigned char breg, bval;
#endif
  int control;
  int value;


#ifdef HAVE_LINUX_PPDEV_H
  /* check we have ppdev working */
  fd = sanei_umax_pp_getparport ();
  if (fd > 0)
    {
      breg = (unsigned char) (reg);
      mode = IEEE1284_MODE_EPP | IEEE1284_ADDR;
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      rc = write (fd, &breg, 1);
      if (rc != 1)
	DBG (0, "ppdev short write (%s:%d)\n", __FILE__, __LINE__);

      mode = 1;			/* data_reverse */
      rc = ioctl (fd, PPDATADIR, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);

      mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      rc = read (fd, &bval, 1);
      if (rc != 1)
	DBG (0, "ppdev short read (%s:%d)\n", __FILE__, __LINE__);
      value = bval;

      mode = 0;			/* forward */
      rc = ioctl (fd, PPDATADIR, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);

      return value;
    }
  /* if not, direct hardware access */
#endif

  Outb (EPPADDR, reg);
  control = Inb (CONTROL);
  control = (control & 0x1F) | 0x20;
  Outb (CONTROL, control);
  value = Inb (EPPDATA);
  control = Inb (CONTROL);
  control = control & 0x1F;
  Outb (CONTROL, control);
  return 0xFF;
  /* return value; */
}

static int
registerRead (int reg)
{
  switch (gMode)
    {
    case UMAX_PP_PARPORT_ECP:
      return ECPregisterRead (reg);
    case UMAX_PP_PARPORT_BYTE:
      DBG (0, "STEF: gMode BYTE in registerRead !!\n");
      return 0xFF;
    case UMAX_PP_PARPORT_EPP:
      return EPPregisterRead (reg);
    case UMAX_PP_PARPORT_PS2:
      DBG (0, "STEF: gMode PS2 in registerRead !!\n");
      return PS2registerRead (reg);
    default:
      DBG (0, "STEF: gMode unset in registerRead !!\n");
      return 0xFF;
    }
}


static void
ECPregisterWrite (int reg, int value)
{
  unsigned char breg;

#ifdef HAVE_LINUX_PPDEV_H
  int rc, fd;

  fd = sanei_umax_pp_getparport ();
  if (fd > 0)
    {
      ECPFifoMode ();
      Outb (DATA, reg);
      breg = value;
      rc = write (fd, &breg, 1);
      if (rc != 1)
	DBG (0, "ppdev short write (%s:%d)\n", __FILE__, __LINE__);
      Outb (CONTROL, 0x04);
      byteMode ();
      return;
    }
#endif

  /* standard mode, interrupt bit, dma disabled, 
     service bit, fifo full=0, fifo empty=0 */
  compatMode ();
  Outb (CONTROL, 0x04);		/* reset ? */

  /* ECP FIFO mode, interrupt bit, dma disabled, 
     service bit, fifo full=0, fifo empty=0 */
  ECPFifoMode ();		/*Outb (ECR, 0x60); */
  if (waitFifoEmpty () == 0)
    {
      DBG (0, "ECPregisterWrite failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return;
    }
  breg = Inb (ECR);

  Outb (DATA, reg);
  if (waitFifoEmpty () == 0)
    {
      DBG (0, "ECPregisterWrite failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return;
    }
  breg = Inb (ECR);

  Outb (ECPDATA, value);
  if (waitFifoEmpty () == 0)
    {
      DBG (0, "ECPregisterWrite failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return;
    }
  breg = Inb (ECR);
  Outb (CONTROL, 0x04);
  byteMode ();
  return;
}

static void
EPPregisterWrite (int reg, int value)
{
#ifdef HAVE_LINUX_PPDEV_H
  int fd, mode, rc;
  unsigned char breg, bval;
#endif

  reg = reg | 0x40;

#ifdef HAVE_LINUX_PPDEV_H
  /* check we have ppdev working */
  fd = sanei_umax_pp_getparport ();
  if (fd > 0)
    {
      breg = (unsigned char) (reg);
      mode = IEEE1284_MODE_EPP | IEEE1284_ADDR;
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      rc = write (fd, &breg, 1);
      if (rc != 1)
	DBG (0, "ppdev short write (%s:%d)\n", __FILE__, __LINE__);

      bval = (unsigned char) (value);
      mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      rc = write (fd, &bval, 1);

      return;
    }
  /* if not, direct hardware access */
#endif
  Outb (EPPADDR, reg);
  Outb (EPPDATA, value);
}

static void
registerWrite (int reg, int value)
{
  switch (gMode)
    {
    case UMAX_PP_PARPORT_PS2:
      PS2registerWrite (reg, value);
      DBG (0, "STEF: gMode PS2 in registerWrite !!\n");
      break;
    case UMAX_PP_PARPORT_ECP:
      ECPregisterWrite (reg, value);
      break;
    case UMAX_PP_PARPORT_EPP:
      EPPregisterWrite (reg, value);
      break;
    case UMAX_PP_PARPORT_BYTE:
      DBG (0, "STEF: gMode BYTE in registerWrite !!\n");
      break;
    default:
      DBG (0, "STEF: gMode unset in registerWrite !!\n");
      break;
    }
}

static void
EPPBlockMode (int flag)
{
#ifdef HAVE_LINUX_PPDEV_H
  int fd, mode, rc;
  unsigned char bval;

  /* check we have ppdev working */
  fd = sanei_umax_pp_getparport ();
  if (fd > 0)
    {
      bval = (unsigned char) (flag);
      mode = IEEE1284_MODE_EPP | IEEE1284_ADDR;
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      rc = write (fd, &bval, 1);
      return;
    }
#endif
  Outb (EPPADDR, flag);
}

static void
EPPbufferRead (int size, unsigned char *dest)
{
#ifdef HAVE_LINUX_PPDEV_H
  int fd, mode, rc, nb;
  unsigned char bval;
#endif
  int control;

#ifdef HAVE_LINUX_PPDEV_H
  /* check we have ppdev working */
  fd = sanei_umax_pp_getparport ();
  if (fd > 0)
    {

      bval = 0x80;
      mode = IEEE1284_MODE_EPP | IEEE1284_ADDR;
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      rc = write (fd, &bval, 1);

      mode = 1;			/* data_reverse */
      rc = ioctl (fd, PPDATADIR, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
#ifdef PPSETFLAGS
      mode = PP_FASTREAD;
      rc = ioctl (fd, PPSETFLAGS, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
#endif
      mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      nb = 0;
      while (nb < size - 1)
	{
	  rc = read (fd, dest + nb, size - 1 - nb);
	  nb += rc;
	}

      mode = 0;			/* forward */
      rc = ioctl (fd, PPDATADIR, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      bval = 0xA0;
      mode = IEEE1284_MODE_EPP | IEEE1284_ADDR;
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      rc = write (fd, &bval, 1);

      mode = 1;			/* data_reverse */
      rc = ioctl (fd, PPDATADIR, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      rc = read (fd, dest + size - 1, 1);

      mode = 0;			/* forward */
      rc = ioctl (fd, PPDATADIR, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);

      return;
    }
  /* if not, direct hardware access */
#endif

  EPPBlockMode (0x80);
  control = Inb (CONTROL);
  Outb (CONTROL, (control & 0x1F) | 0x20);	/* reverse */
  Insb (EPPDATA, dest, size - 1);
  control = Inb (CONTROL);

  Outb (CONTROL, (control & 0x1F));	/* forward */
  EPPBlockMode (0xA0);
  control = Inb (CONTROL);

  Outb (CONTROL, (control & 0x1F) | 0x20);	/* reverse */
  Insb (EPPDATA, (unsigned char *) (dest + size - 1), 1);

  control = Inb (CONTROL);
  Outb (CONTROL, (control & 0x1F));	/* forward */
}


/* block transfer init */
static void
ECPSetBuffer (int size)
{
  static int last = 0;
  unsigned char breg;

  /* routine XX */
  compatMode ();
  Outb (CONTROL, 0x04);		/* reset ? */

  /* we set size only if it has changed */
  /* from last time        */
  if (size == last)
    return;
  last = size;

  /* mode and size setting */
  ECPFifoMode ();		/*Outb (ECR, 0x60);         */
  if (waitFifoEmpty () == 0)
    {
      DBG (0, "ECPSetBuffer failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return;
    }
  breg = Inb (ECR);

  Outb (DATA, 0x0E);
  if (waitFifoEmpty () == 0)
    {
      DBG (0, "ECPSetBuffer failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return;
    }
  breg = Inb (ECR);

  Outb (ECPDATA, 0x0B);		/* R0E=0x0B */
  if (waitFifoEmpty () == 0)
    {
      DBG (0, "ECPSetBuffer failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return;
    }
  breg = Inb (ECR);

  Outb (DATA, 0x0F);		/* R0F=size MSB */
  if (waitFifoEmpty () == 0)
    {
      DBG (0, "ECPSetBuffer failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return;
    }
  breg = Inb (ECR);

  Outb (ECPDATA, size / 256);
  if (waitFifoEmpty () == 0)
    {
      DBG (0, "ECPSetBuffer failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return;
    }
  breg = Inb (ECR);

  Outb (DATA, 0x0B);		/* R0B=size LSB */
  if (waitFifoEmpty () == 0)
    {
      DBG (0, "ECPSetBuffer failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return;
    }
  breg = Inb (ECR);

  Outb (ECPDATA, size % 256);
  if (waitFifoEmpty () == 0)
    {
      DBG (0, "ECPSetBuffer failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return;
    }
  breg = Inb (ECR);
  DBG (16, "ECPSetBuffer(%d) passed ...\n", size);
}



static int
ECPbufferRead (int size, unsigned char *dest)
{
  int breg, n, idx, remain;

  idx = 0;
  n = size / 16;
  remain = size - 16 * n;

  /* block transfer */
  breg = Inb (ECR);		/* 0x15,0x75 expected: fifo empty */

  byteMode ();			/*Outb (ECR, 0x20);            byte mode */
  Outb (CONTROL, 0x04);

  ECPFifoMode ();		/*Outb (ECR, 0x60); */
  if (waitFifoEmpty () == 0)
    {
      DBG (0, "ECPbufferRead failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return idx;
    }
  breg = Inb (ECR);

  Outb (DATA, 0x80);
  if (waitFifoEmpty () == 0)
    {
      DBG (0, "ECPbufferRead failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return idx;
    }
  breg = Inb (ECR);		/* 0x75 expected */

  byteMode ();			/*Outb (ECR, 0x20);            byte mode */
  Outb (CONTROL, 0x20);		/* data reverse */
  ECPFifoMode ();		/*Outb (ECR, 0x60); */

  while (n > 0)
    {
      if (waitFifoFull () == 0)
	{
	  DBG (0,
	       "ECPbufferRead failed, time-out waiting for FIFO idx=%d (%s:%d)\n",
	       idx, __FILE__, __LINE__);
	  return idx;
	}
      Insb (ECPDATA, dest + idx, 16);
      idx += 16;
      n--;
    }

  /* reading trailing bytes */
  while (remain > 0)
    {
      if (waitFifoNotEmpty () == 0)
	{
	  DBG (0, "ECPbufferRead failed, FIFO time-out (%s:%d)\n",
	       __FILE__, __LINE__);
	}
      dest[idx] = Inb (ECPDATA);
      idx++;
      remain--;
    }

  return idx;
}

static void
EPPbufferWrite (int size, unsigned char *source)
{
#ifdef HAVE_LINUX_PPDEV_H
  int fd, mode, rc;
  unsigned char bval;
#endif


#ifdef HAVE_LINUX_PPDEV_H
  /* check we have ppdev working */
  fd = sanei_umax_pp_getparport ();
  if (fd > 0)
    {
      bval = 0xC0;
      mode = IEEE1284_MODE_EPP | IEEE1284_ADDR;
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      rc = write (fd, &bval, 1);

      mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      rc = write (fd, source, size);
      return;
    }
  /* if not, direct hardware access */
#endif
  EPPBlockMode (0xC0);
  Outsb (EPPDATA, source, size);
}

static void
ECPbufferWrite (int size, unsigned char *source)
{
  unsigned char breg;
  int n, idx;

  /* until we know to handle that case, fail */
  if (size % 16 != 0)
    {
      DBG (0, "ECPbufferWrite failed, size %%16 !=0 (%s:%d)\n",
	   __FILE__, __LINE__);
      return;
    }

  /* prepare actual transfer */
  compatMode ();
  Outb (CONTROL, 0x04);		/* reset ? */
  breg = Inb (CONTROL);
  Outb (CONTROL, 0x04);		/* data forward */
  ECPFifoMode ();		/*Outb (ECR, 0x60);         */
  if (waitFifoEmpty () == 0)
    {
      DBG (0, "ECPWriteBuffer failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return;
    }
  breg = Inb (ECR);
  breg = (Inb (STATUS)) & 0xF8;
  n = 0;
  while ((n < 1024) && (breg != 0xF8))
    {
      breg = (Inb (STATUS)) & 0xF8;
      n++;
    }
  if (breg != 0xF8)
    {
      DBG (0,
	   "ECPbufferWrite failed, expected status=0xF8, got 0x%02X (%s:%d)\n",
	   breg, __FILE__, __LINE__);
      return;
    }

  /* wait for FIFO empty (bit 0) */
  if (waitFifoEmpty () == 0)
    {
      DBG (0, "ECPbufferWrite failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return;
    }
  breg = Inb (ECR);

  /* block transfer direction 
   * 0x80 means from scanner to PC, 0xC0 means PC to scanner 
   */
  Outb (DATA, 0xC0);

  n = size / 16;
  idx = 0;
  while (n > 0)
    {
      /* wait for FIFO empty */
      if (waitFifoEmpty () == 0)
	{
	  DBG (0,
	       "ECPbufferWrite failed, time-out waiting for FIFO (%s:%d)\n",
	       __FILE__, __LINE__);
	  return;
	}
      breg = Inb (ECR);

      Outsb (ECPDATA, source + idx * 16, 16);
      idx++;
      n--;
    }


  /* final FIFO check and go to Byte mode */
  if (waitFifoEmpty () == 0)
    {
      DBG (0, "ECPbufferWrite failed, time-out waiting for FIFO (%s:%d)\n",
	   __FILE__, __LINE__);
      return;
    }
  breg = Inb (ECR);
  Outb (CONTROL, 0x04);
  byteMode ();
}

static void
bufferWrite (int size, unsigned char *source)
{
  switch (gMode)
    {
    case UMAX_PP_PARPORT_PS2:
      PS2bufferWrite (size, source);
      DBG (0, "STEF: gMode PS2 in bufferWrite !!\n");
      break;
    case UMAX_PP_PARPORT_ECP:
      ECPbufferWrite (size, source);
      break;
    case UMAX_PP_PARPORT_EPP:
      switch (getEPPMode ())
	{
	case 32:
	  EPPWrite32Buffer (size, source);
	  break;
	default:
	  EPPbufferWrite (size, source);
	  break;
	}
      break;
    default:
      DBG (0, "STEF: gMode PS2 in bufferWrite !!\n");
      break;
    }
  return;
}

static void
bufferRead (int size, unsigned char *dest)
{
  switch (gMode)
    {
    case UMAX_PP_PARPORT_PS2:
      PS2bufferRead (size, dest);
      DBG (0, "STEF: gMode PS2 in bufferRead !!\n");
      break;
    case UMAX_PP_PARPORT_ECP:
      ECPbufferRead (size, dest);
      break;
    case UMAX_PP_PARPORT_EPP:
      switch (getEPPMode ())
	{
	case 32:
	  EPPRead32Buffer (size, dest);
	  break;
	default:
	  EPPbufferRead (size, dest);
	  break;
	}
      break;
    default:
      DBG (0, "STEF: gMode unset in bufferRead !!\n");
      break;
    }
  return;
}

static int
connect (void)
{
  if (sanei_umax_pp_getastra () == 610)
    return connect610p ();

  switch (gMode)
    {
    case UMAX_PP_PARPORT_PS2:
      DBG (0, "STEF: unimplemented gMode PS2 in connect() !!\n");
      return 0;
      break;
    case UMAX_PP_PARPORT_ECP:
      return ECPconnect ();
      break;
    case UMAX_PP_PARPORT_BYTE:
      DBG (0, "STEF: unimplemented gMode BYTE in connect() !!\n");
      return 0;
      break;
    case UMAX_PP_PARPORT_EPP:
      return EPPconnect ();
    default:
      DBG (0, "STEF: gMode unset in connect() !!\n");
      break;
    }
  return 0;
}


static void
disconnect (void)
{
  if (sanei_umax_pp_getastra () == 610)
    disconnect610p ();
  switch (gMode)
    {
    case UMAX_PP_PARPORT_PS2:
      DBG (0, "STEF: unimplemented gMode PS2 in disconnect() !!\n");
      break;
    case UMAX_PP_PARPORT_ECP:
      ECPdisconnect ();
      break;
    case UMAX_PP_PARPORT_BYTE:
      DBG (0, "STEF: unimplemented gMode BYTE in disconnect() !!\n");
      break;
    case UMAX_PP_PARPORT_EPP:
      EPPdisconnect ();
      break;
    default:
      DBG (0, "STEF: gMode unset in disconnect() !!\n");
      break;
    }
}


/* returns 0 if mode OK, else -1 */
static int
checkEPAT (void)
{
  int version;

  version = registerRead (0x0B);
  if (version == 0xC7)
    return 0;
  DBG (0, "checkEPAT: expected EPAT version 0xC7, got 0x%X! (%s:%d)\n",
       version, __FILE__, __LINE__);
  return -1;

}

static int
init005 (int arg)
{
  int count = 5;
  int res;

  while (count > 0)
    {
      registerWrite (0x0A, arg);
      Outb (DATA, 0xFF);
      res = registerRead (0x0A);

      /* failed ? */
      if (res != arg)
	return 1;

      /* ror arg */
      res = arg & 0x01;
      arg = arg / 2;
      if (res == 1)
	arg = arg | 0x80;

      /* next loop */
      count--;
    }
  return 0;
}

/* write 1 byte in EPP mode, returning scnner's status */
static int
EPPputByte610p (int data)
{
  int status, control;

  status = Inb (STATUS) & 0xF8;
  if ((status != 0xC8) && (status != 0xC0) && (status != 0xD0))
    {
      DBG (0,
	   "EPPputByte610p failed, expected 0xC8, 0xD0 or 0xC0 got 0x%02X ! (%s:%d)\n",
	   status, __FILE__, __LINE__);
      return 0;
    }
  control = (Inb (CONTROL) & 0x44) | 0x44;	/* data forward, bit 5 cleared (!!) */
  Outb (CONTROL, control);
  Outb (EPPDATA, data);
  return status;
}

static int
putByte610p (int data)
{
  int status, control, j;

  if (gMode == UMAX_PP_PARPORT_EPP)
    return EPPputByte610p (data);
  j = 0;
  do
    {
      status = Inb (STATUS) & 0xF8;
      j++;
    }
  while ((j < 20) && (status & 0x08));

  if ((status != 0xC8) && (status != 0xC0))
    {
      DBG (0,
	   "putByte610p failed, expected 0xC8 or 0xC0 got 0x%02X ! (%s:%d)\n",
	   status, __FILE__, __LINE__);
      return 0;
    }
  control = Inb (CONTROL) & 0x1F;	/* data forward */
  Outb (CONTROL, control);

  Outb (DATA, data);
  Outb (CONTROL, 0x07);
  status = Inb (STATUS) & 0xF8;
  if ((status != 0x48) && (status != 0x40))
    {
      DBG (0,
	   "putByte610p failed, expected 0x48 or 0x40 got 0x%02X ! (%s:%d)\n",
	   status, __FILE__, __LINE__);
      return 0;
    }


  Outb (CONTROL, 0x05);
  status = Inb (STATUS) & 0xF8;
  Outb (CONTROL, control);
  return status;
}


/* 1 OK, 0 failure */
static int
sync610p (void)
{
  int status;

  Outb (DATA, 0x40);
  Outb (CONTROL, 0x06);
  status = Inb (STATUS) & 0xF8;
  if (status != 0x38)
    {
      DBG (0, "sync610p failed (got 0x%02X expected 0x38)! (%s:%d)\n",
	   status, __FILE__, __LINE__);
      return 0;
    }
  Outb (CONTROL, 0x07);
  status = Inb (STATUS) & 0xF8;
  if (status != 0x38)
    {
      DBG (0, "sync610p failed (got 0x%02X expected 0x38)! (%s:%d)\n",
	   status, __FILE__, __LINE__);
      return 0;
    }
  Outb (CONTROL, 0x04);
  status = Inb (STATUS) & 0xF8;
  if (status != 0xF8)
    {
      DBG (0, "sync610p failed (got 0x%02X expected 0xF8)! (%s:%d)\n",
	   status, __FILE__, __LINE__);
      return 0;
    }
  Outb (CONTROL, 0x05);
  Inb (CONTROL);		/* 0x05 expected */
  Outb (CONTROL, 0x04);
  return 1;
}

static int
EPPcmdSync610p (int cmd)
{
  int word[5];
  int status;
  int i;

  word[0] = 0;
  word[1] = 0;
  word[2] = 0;
  word[3] = cmd;

  connect610p ();
  sync610p ();

  /* sends magic seal 55 AA */
  status = EPPputByte610p (0x55);
  if ((status != 0xC8) && (status != 0xC0) && (status != 0xD0))
    {
      DBG (1,
	   "EPPcmdSync610p: Found 0x%X expected 0xC8, 0xC0 or 0xD0 (%s:%d)\n",
	   status, __FILE__, __LINE__);
      return 0;
    }
  status = EPPputByte610p (0xAA);
  if ((status != 0xC8) && (status != 0xC0) && (status != 0xD0))
    {
      DBG (1,
	   "EPPcmdSync610p: Found 0x%X expected 0xC8, 0xC0 or 0xD0 (%s:%d)\n",
	   status, __FILE__, __LINE__);
      return 0;
    }

  status = EPPgetStatus610p ();
  if (status == 0xC0)
    for (i = 0; i < 10; i++)
      status = Inb (STATUS) & 0xF8;
  if (status != 0xC8)
    {
      DBG (0, "EPPcmdSync610p: Found 0x%X expected 0xC8 (%s:%d)\n", status,
	   __FILE__, __LINE__);
      /*return 0; */
    }

  /* sends 4 bytes of data */
  for (i = 0; i < 4; i++)
    {
      status = EPPputByte610p (word[i]);
    }
  if (status != 0xC8)
    {
      DBG (0, "EPPcmdSync610p: Found 0x%X expected 0xC8 (%s:%d)\n", status,
	   __FILE__, __LINE__);
      /*return 0; */
    }

  /* tests status */
  Outb (DATA, 0xFF);
  if (cmd == 0xC2)
    {
      status = EPPgetStatus610p ();
      if (status != 0xC0)
	{
	  DBG (0, "EPPcmdSync610p: Found 0x%X expected 0xC0 (%s:%d)\n",
	       status, __FILE__, __LINE__);
	  /*return 0; */
	}
    }
  status = EPPgetStatus610p ();
  if (status != 0xC0)
    {
      DBG (0, "EPPcmdSync610p: Found 0x%X expected 0xC0 (%s:%d)\n", status,
	   __FILE__, __LINE__);
      /*return 0; */
    }
  disconnect610p ();
  return 1;
}

static int
cmdSync610p (int cmd)
{
  int word[5];
  int status;

  if (gMode == UMAX_PP_PARPORT_EPP)
    return EPPcmdSync610p (cmd);

  word[0] = 0;
  word[1] = 0;
  word[2] = 0;
  word[3] = cmd;

  connect610p ();
  sync610p ();
  if (sendLength610p (word) == 0)
    {
      DBG (0, "sendLength610p() failed... (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }
  if (cmd == 0xC2)
    {
      status = getStatus610p ();
      if (status != 0xC0)
	{
	  DBG (1, "Found 0x%X expected 0xC0  (%s:%d)\n", status, __FILE__,
	       __LINE__);
	  return 0;
	}
    }
  status = getStatus610p ();
  if (status != 0xC0)
    {
      DBG (1, "Found 0x%X expected 0xC0  (%s:%d)\n", status, __FILE__,
	   __LINE__);
      return 0;
    }
  disconnect610p ();
  return 1;
}

static int
EPPgetStatus610p (void)
{
  int data, status, control, i;

  control = Inb (CONTROL) & 0xA4;
  control = control | 0xE0;
  Outb (CONTROL, control);
  status = Inb (STATUS) & 0xF8;
  if (status & 0x08)
    {
      for (i = 1; i < 10; i++)
	status = Inb (STATUS) & 0xF8;
    }
  else
    {
      data = Inb (EPPDATA);
      scannerStatus = data;
    }
  return status;
}

static int
getStatus610p (void)
{
  int data, status;

  byteMode ();
  status = Inb (STATUS) & 0xF8;
  Outb (CONTROL, 0x26);		/* data reverse */
  data = Inb (DATA);
  scannerStatus = data;
  Outb (CONTROL, 0x24);
  return status;
}


int
sendLength610p (int *cmd)
{
  int ret, i, wait;
/* 55,AA,x,y,z,t */

  byteMode ();
  wait = putByte610p (0x55);
  if ((wait != 0xC8) && (wait != 0xC0))
    {
      DBG (0,
	   "sendLength610p failed, expected 0xC8 or 0xC0 got 0x%02X ! (%s:%d)\n",
	   wait, __FILE__, __LINE__);
      return 0;
    }
  wait = putByte610p (0xAA);
  if ((wait != 0xC8) && (wait != 0xC0))
    {
      DBG (0,
	   "sendLength610p failed, expected 0xC8 or 0xC0 got 0x%02X ! (%s:%d)\n",
	   wait, __FILE__, __LINE__);
      return 0;
    }

  /* if wait=C0, we have to ... wait */
  if (wait == 0xC0)
    {
      byteMode ();
      wait = Inb (STATUS);	/* C0 expected */
      Outb (CONTROL, 0x26);
      ret = Inb (DATA);		/* 88 expected */
      Outb (CONTROL, 0x24);
      for (i = 0; i < 10; i++)
	wait = Inb (STATUS);	/* C8 expected */
      byteMode ();
    }

  for (i = 0; i < 3; i++)
    {
      ret = putByte610p (cmd[i]);
      if (ret != 0xC8)
	{
	  DBG (0,
	       "sendLength610p failed, expected 0xC8 got 0x%02X ! (%s:%d)\n",
	       ret, __FILE__, __LINE__);
	  return 0;
	}
    }
  ret = putByte610p (cmd[3]);
  if ((ret != 0xC0) && (ret != 0xD0))
    {
      DBG (0,
	   "sendLength610p failed, expected 0xC0 or 0xD0 got 0x%02X ! (%s:%d)\n",
	   ret, __FILE__, __LINE__);
      return 0;
    }
  return 1;
}

/* 1 OK, 0 failure */
static int
disconnect610p (void)
{
  int control, i;

  Outb (CONTROL, 0x04);
  for (i = 0; i < 41; i++)
    {
      control = Inb (CONTROL) & 0x3F;
      if (control != 0x04)
	{
	  DBG (0, "disconnect610p failed (idx %d=%02X)! (%s:%d)\n",
	       i, control, __FILE__, __LINE__);
	  return 0;
	}
    }
  Outb (CONTROL, 0x0C);
  control = Inb (CONTROL) & 0x3F;
  if (control != 0x0C)
    {
      DBG (0, "disconnect610p failed expected 0x0C got %02X (%s:%d)\n",
	   control, __FILE__, __LINE__);
      return 0;
    }
  /* XXX STEF XXX Outb (DATA, gData); */
  Outb (DATA, 0xFF);
  return 1;
}

/* 1 OK, 0 failure */
/* 0: short connect, 1 long connect */
static int
connect610p (void)
{
  int control;

  gData = Inb (DATA);		/* to gDATA ? */

  Outb (DATA, 0xAA);
  Outb (CONTROL, 0x0E);
  control = Inb (CONTROL);	/* 0x0E expected */
  control = Inb (CONTROL) & 0x3F;
  if (control != 0x0E)
    {
      DBG (0, "connect610p control=%02X, expected 0x0E (%s:%d)\n", control,
	   __FILE__, __LINE__);
    }

  Outb (DATA, 0x00);
  Outb (CONTROL, 0x0C);
  control = Inb (CONTROL);	/* 0x0C expected */
  control = Inb (CONTROL) & 0x3F;
  if (control != 0x0C)
    {
      DBG (0, "connect610p control=%02X, expected 0x0C (%s:%d)\n", control,
	   __FILE__, __LINE__);
    }

  Outb (DATA, 0x55);
  Outb (CONTROL, 0x0E);
  control = Inb (CONTROL);	/* 0x0E expected */
  control = Inb (CONTROL) & 0x3F;
  if (control != 0x0E)
    {
      DBG (0, "connect610p control=%02X, expected 0x0E (%s:%d)\n", control,
	   __FILE__, __LINE__);
    }

  Outb (DATA, 0xFF);
  Outb (CONTROL, 0x0C);
  control = Inb (CONTROL);	/* 0x0C expected */
  control = Inb (CONTROL) & 0x3F;
  if (control != 0x0C)
    {
      DBG (0, "connect610p control=%02X, expected 0x0C (%s:%d)\n", control,
	   __FILE__, __LINE__);
    }

  Outb (CONTROL, 0x04);
  control = Inb (CONTROL);	/* 0x04 expected */
  control = Inb (CONTROL) & 0x3F;
  if (control != 0x04)
    {
      DBG (0, "connect610p control=%02X, expected 0x04 (%s:%d)\n", control,
	   __FILE__, __LINE__);
    }
  return 1;
}

/* 1 OK, 0 failure */
static int
EPPconnect (void)
{
  int control;
  int data;

  /* initial values, don't hardcode */
  Outb (DATA, 0x04);
  Outb (CONTROL, 0x0C);

  data = Inb (DATA);
  control = Inb (CONTROL);
  Outb (CONTROL, control & 0x1F);
  control = Inb (CONTROL);
  Outb (CONTROL, control & 0x1F);

  if (sendCommand (0xE0) != 1)
    {
      DBG (0, "EPPconnect: sendCommand(0xE0) failed! (%s:%d)\n", __FILE__,
	   __LINE__);
      return 0;
    }
  ClearRegister (0);
  init001 ();
  return 1;
}



static void
EPPRead32Buffer (int size, unsigned char *dest)
{
#ifdef HAVE_LINUX_PPDEV_H
  int fd, mode, rc, nb;
  unsigned char bval;
#endif
  int control;

#ifdef HAVE_LINUX_PPDEV_H
  /* check we have ppdev working */
  fd = sanei_umax_pp_getparport ();
  if (fd > 0)
    {

      bval = 0x80;
      mode = IEEE1284_MODE_EPP | IEEE1284_ADDR;
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      rc = write (fd, &bval, 1);

      mode = 1;			/* data_reverse */
      rc = ioctl (fd, PPDATADIR, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
#ifdef PPSETFLAGS
      mode = PP_FASTREAD;
      rc = ioctl (fd, PPSETFLAGS, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
#endif
      mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      nb = 0;
      while (nb < size - 4)
	{
	  rc = read (fd, dest + nb, size - 4 - nb);
	  nb += rc;
	}

      rc = read (fd, dest + size - 4, 3);

      mode = 0;			/* forward */
      rc = ioctl (fd, PPDATADIR, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      bval = 0xA0;
      mode = IEEE1284_MODE_EPP | IEEE1284_ADDR;
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      rc = write (fd, &bval, 1);

      mode = 1;			/* data_reverse */
      rc = ioctl (fd, PPDATADIR, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      rc = read (fd, dest + size - 1, 1);

      mode = 0;			/* forward */
      rc = ioctl (fd, PPDATADIR, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);

      return;
    }
  /* if not, direct hardware access */
#endif

  EPPBlockMode (0x80);

  control = Inb (CONTROL);
  Outb (CONTROL, (control & 0x1F) | 0x20);
  Insw (EPPDATA, dest, size / 4 - 1);

  Insb (EPPDATA, (unsigned char *) (dest + size - 4), 3);
  control = Inb (CONTROL);
  Outb (CONTROL, (control & 0x1F));

  EPPBlockMode (0xA0);
  control = Inb (CONTROL);
  Outb (CONTROL, (control & 0x1F) | 0x20);

  Insb (EPPDATA, (unsigned char *) (dest + size - 1), 1);
  control = Inb (CONTROL);
  Outb (CONTROL, (control & 0x1F));
}

static void
EPPWrite32Buffer (int size, unsigned char *source)
{
#ifdef HAVE_LINUX_PPDEV_H
  int fd, mode, rc;
  unsigned char bval;
#endif

  if ((size % 4) != 0)
    {
      DBG (0, "EPPWrite32Buffer: size %% 4 != 0!! (%s:%d)\n", __FILE__,
	   __LINE__);
    }
#ifdef HAVE_LINUX_PPDEV_H
  /* check we have ppdev working */
  fd = sanei_umax_pp_getparport ();
  if (fd > 0)
    {

      bval = 0xC0;
      mode = IEEE1284_MODE_EPP | IEEE1284_ADDR;
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      rc = write (fd, &bval, 1);

#ifdef PPSETFLAGS
      mode = PP_FASTWRITE;
      rc = ioctl (fd, PPSETFLAGS, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
#endif
      mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      rc = write (fd, source, size);

      return;
    }
  /* if not, direct hardware access */
#endif
  EPPBlockMode (0xC0);
  Outsw (EPPDATA, source, size / 4);
}






/* returns 0 if ERROR cleared in STATUS within 1024 inb, else 1 */
static int
WaitOnError (void)
{
  int c = 0;
  int count = 1024;
  int status;

  do
    {
      do
	{
	  status = Inb (STATUS) & 0x08;
	  if (status != 0)
	    {
	      count--;
	      if (count == 0)
		c = 1;
	    }
	}
      while ((count > 0) && (status != 0));
      if (status == 0)
	{
	  status = Inb (STATUS) & 0x08;
	  if (status == 0)
	    c = 0;
	}
    }
  while ((status != 0) && (c == 0));
  return c;
}



#ifdef HAVE_LINUX_PPDEV_H
/* read up to size bytes, returns bytes read */
static int
ParportpausedBufferRead (int size, unsigned char *dest)
{
  unsigned char status, bval;
  int error;
  int word;
  int bread;
  int c;
  int fd, rc, mode;

  /* WIP check */
  if (gMode == UMAX_PP_PARPORT_ECP)
    {
      DBG (0, "ECP access not implemented yet (WIP) ! (%s:%d)\n",
	   __FILE__, __LINE__);
    }

  /* init */
  bread = 0;
  error = 0;
  fd = sanei_umax_pp_getparport ();

  mode = 1;			/* data_reverse */
  rc = ioctl (fd, PPDATADIR, &mode);
  if (rc)
    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	 __FILE__, __LINE__);
#ifdef PPSETFLAGS
  mode = PP_FASTREAD;
  rc = ioctl (fd, PPSETFLAGS, &mode);
  if (rc)
    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	 __FILE__, __LINE__);
#endif
  mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
  rc = ioctl (fd, PPSETMODE, &mode);
  if (rc)
    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	 __FILE__, __LINE__);

  if ((size & 0x03) != 0)
    {
      while ((!error) && ((size & 0x03) != 0))
	{
	  rc = read (fd, dest, 1);
	  size--;
	  dest++;
	  bread++;
	  rc = ioctl (fd, PPRSTATUS, &status);
	  if (rc)
	    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
		 __FILE__, __LINE__);
	  error = status & 0x08;
	}
      if (error)
	{
	  DBG (0, "Read error (%s:%d)\n", __FILE__, __LINE__);
	  return 0;
	}
    }

  /* from here, we read 1 byte, then size/4-1 32 bits words, and then
     3 bytes, pausing on ERROR bit of STATUS */
  size -= 4;

  /* sanity test, seems to be wrongly handled ... */
  if (size == 0)
    {
      DBG (0, "case not handled! (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }

  word = 0;
  error = 0;
  bread += size;
  do
    {
      do
	{
	  rc = read (fd, dest, 1);
	  size--;
	  dest++;
	readstatus:
	  if (size > 0)
	    {
	      rc = ioctl (fd, PPRSTATUS, &status);
	      if (rc)
		DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n",
		     strerror (errno), __FILE__, __LINE__);
	      word = status & 0x10;
	      error = status & 0x08;
	    }
	}
      while ((size > 0) && (!error) && (!word));
    }
  while ((size < 4) && (!error) && (size > 0));

  /* here size=0 or error=8 or word=0x10 */
  if ((word) && (!error) && (size))
    {
      rc = read (fd, dest, 4);
      dest += 4;
      size -= 4;
      if (size != 0)
	error = 0x08;
    }
  if (!error)
    {
      c = 0;
      rc = ioctl (fd, PPRSTATUS, &status);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      error = status & 0x08;
      if (error)
	c = WaitOnError ();
    }
  else
    {				/* 8282 */
      c = WaitOnError ();
      if (c == 0)
	goto readstatus;
    }
  if (c == 1)
    {
      bread -= size;
    }
  else
    {
      bread += 3;
      size = 3;
      do
	{
	  do
	    {
	      rc = read (fd, dest, 1);
	      dest++;
	      size--;
	      if (size)
		{
		  rc = ioctl (fd, PPRSTATUS, &status);
		  if (rc)
		    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n",
			 strerror (errno), __FILE__, __LINE__);
		  error = status & 0x08;
		  if (!error)
		    {
		      rc = ioctl (fd, PPRSTATUS, &status);
		      if (rc)
			DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n",
			     strerror (errno), __FILE__, __LINE__);
		      error = status & 0x08;
		    }
		}
	    }
	  while ((size > 0) && (!error));
	  c = 0;
	  if (error)
	    c = WaitOnError ();
	}
      while ((size > 0) && (c == 0));
    }

  /* end reading */
  mode = 0;			/* forward */
  rc = ioctl (fd, PPDATADIR, &mode);
  if (rc)
    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	 __FILE__, __LINE__);
  bval = 0xA0;
  mode = IEEE1284_MODE_EPP | IEEE1284_ADDR;
  rc = ioctl (fd, PPSETMODE, &mode);
  if (rc)
    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	 __FILE__, __LINE__);
  rc = write (fd, &bval, 1);

  mode = 1;			/* data_reverse */
  rc = ioctl (fd, PPDATADIR, &mode);
  if (rc)
    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	 __FILE__, __LINE__);
  mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
  rc = ioctl (fd, PPSETMODE, &mode);
  if (rc)
    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	 __FILE__, __LINE__);
  rc = read (fd, dest, 1);
  bread++;

  mode = 0;			/* forward */
  rc = ioctl (fd, PPDATADIR, &mode);
  if (rc)
    DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	 __FILE__, __LINE__);
  return bread;
}
#endif

/* read up to size bytes, returns bytes read */
static int
DirectpausedBufferRead (int size, unsigned char *dest)
{
  int control;
  int status;
  int error;
  int word;
  int read;
  int c;

  /* init */
  read = 0;
  error = 0;
  control = Inb (CONTROL) & 0x1F;
  Outb (CONTROL, control | 0x20);
  if ((size & 0x03) != 0)
    {
      /* 8174 */
      while ((!error) && ((size & 0x03) != 0))
	{
	  Insb (EPPDATA, dest, 1);
	  size--;
	  dest++;
	  read++;
	  status = Inb (STATUS) & 0x1F;
	  error = status & 0x08;
	}
      if (error)
	{
	  DBG (0, "Read error (%s:%d)\n", __FILE__, __LINE__);
	  return 0;
	}
    }

  /* from here, we read 1 byte, then size/4-1 32 bits words, and then
     3 bytes, pausing on ERROR bit of STATUS */
  size -= 4;

  /* sanity test, seems to be wrongly handled ... */
  if (size == 0)
    {
      DBG (0, "case not handled! (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }

  word = 0;
  error = 0;
  read += size;
  do
    {
      do
	{
	  Insb (EPPDATA, dest, 1);
	  size--;
	  dest++;
	readstatus:
	  if (size > 0)
	    {
	      status = Inb (STATUS) & 0x1F;
	      word = status & 0x10;
	      error = status & 0x08;
	    }
	}
      while ((size > 0) && (!error) && (!word));
    }
  while ((size < 4) && (!error) && (size > 0));

  /* here size=0 or error=8 or word=0x10 */
  if ((word) && (!error) && (size))
    {
      if (epp32)
	Insw (EPPDATA, dest, 1);
      else
	Insb (EPPDATA, dest, 4);
      dest += 4;
      size -= 4;
      if (size != 0)
	error = 0x08;
    }
  if (!error)
    {
      c = 0;
      error = Inb (STATUS) & 0x08;
      if (error)
	c = WaitOnError ();
    }
  else
    {				/* 8282 */
      c = WaitOnError ();
      if (c == 0)
	goto readstatus;
    }
  if (c == 1)
    {
      read -= size;
    }
  else
    {
      read += 3;
      size = 3;
      do
	{
	  do
	    {
	      Insb (EPPDATA, dest, 1);
	      dest++;
	      size--;
	      if (size)
		{
		  error = Inb (STATUS) & 0x08;
		  if (!error)
		    error = Inb (STATUS) & 0x08;
		}
	    }
	  while ((size > 0) && (!error));
	  c = 0;
	  if (error)
	    c = WaitOnError ();
	}
      while ((size > 0) && (c == 0));
    }

  /* end reading */
  control = Inb (CONTROL) & 0x1F;
  Outb (CONTROL, control);
  EPPBlockMode (0xA0);
  control = Inb (CONTROL) & 0x1F;
  Outb (CONTROL, control | 0x20);
  Insb (EPPDATA, dest, 1);
  read++;
  control = Inb (CONTROL) & 0x1F;
  Outb (CONTROL, control);
  return read;
}


int
pausedBufferRead (int size, unsigned char *dest)
{
  EPPBlockMode (0x80);
#ifdef HAVE_LINUX_PPDEV_H
  if (sanei_umax_pp_getparport () > 0)
    return ParportpausedBufferRead (size, dest);
#endif
  /* only EPP hardware access for now */
  if (gMode == UMAX_PP_PARPORT_EPP)
    return DirectpausedBufferRead (size, dest);
  return 0;
}




/* returns 1 on success, 0 otherwise */
static int
sendWord1220P (int *cmd)
{
  int i;
  int reg;
  int try = 0;

  /* send header */
  reg = registerRead (0x19) & 0xF8;
retry:
  registerWrite (0x1C, 0x55);
  reg = registerRead (0x19) & 0xF8;

  registerWrite (0x1C, 0xAA);
  reg = registerRead (0x19) & 0xF8;

  /* sync when needed */
  if ((reg & 0x08) == 0x00)
    {
      reg = registerRead (0x1C);
      DBG (16, "UTA: reg1C=0x%02X   (%s:%d)\n", reg, __FILE__, __LINE__);
      if (((reg & 0x10) != 0x10) && (reg != 0x6B) && (reg != 0xAB)
	  && (reg != 0x23))
	{
	  DBG (0, "sendWord failed (reg1C=0x%02X)   (%s:%d)\n", reg, __FILE__,
	       __LINE__);
	  return 0;
	}
      for (i = 0; i < 10; i++)
	{
	  usleep (1000);
	  reg = registerRead (0x19) & 0xF8;
	  if (reg != 0xC8)
	    {
	      DBG (0, "Unexpected reg19=0x%2X  (%s:%d)\n", reg, __FILE__,
		   __LINE__);
	    }
	}
      do
	{
	  if ((reg != 0xC0) && (reg != 0xC8))
	    {
	      DBG (0, "Unexpected reg19=0x%2X  (%s:%d)\n", reg, __FILE__,
		   __LINE__);
	    }
	  /* 0xF0 certainly means error */
	  if ((reg == 0xC0) || (reg == 0xD0))
	    {
	      try++;
	      goto retry;
	    }
	  reg = registerRead (0x19) & 0xF8;
	}
      while (reg != 0xC8);
    }

  /* send word */
  i = 0;
  while ((reg == 0xC8) && (cmd[i] != -1))
    {
      registerWrite (0x1C, cmd[i]);
      i++;
      reg = registerRead (0x19) & 0xF8;
    }
  TRACE (16, "sendWord() passed ");
  if ((reg != 0xC0) && (reg != 0xD0))
    {
      DBG (0, "sendWord failed  got 0x%02X instead of 0xC0 or 0xD0 (%s:%d)\n",
	   reg, __FILE__, __LINE__);
      DBG (0, "Blindly going on .....\n");
    }
  if (((reg == 0xC0) || (reg == 0xD0)) && (cmd[i] != -1))
    {
      DBG (0, "sendWord failed: short send  (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }
  reg = registerRead (0x1C);
  DBG (16, "sendWord, reg1C=0x%02X (%s:%d)\n", reg, __FILE__, __LINE__);
  /* model 0x07 has always the last bit set to 1, and even bit 1 */
  /* when UTA is present, we get 0x6B there */
  scannerStatus = reg & 0xFC;
  if (scannerStatus == 0x68)
    hasUTA = 1;

  reg = reg & 0x10;
  if ((reg != 0x10) && (scannerStatus != 0x68) && (scannerStatus != 0xA8))
    {
      DBG (0, "sendWord failed: acknowledge not received (%s:%d)\n", __FILE__,
	   __LINE__);
      return 0;
    }
  if (try)
    {
      DBG (0, "sendWord retry success (retry %d time%s) ... (%s:%d)\n", try,
	   (try > 1) ? "s" : "", __FILE__, __LINE__);
    }
  return 1;
}


/* returns 1 on success, 0 otherwise */
static int
SPPsendWord610p (int *cmd)
{
  int i, j;
  int tmp, status;

#ifdef HAVE_LINUX_PPDEV_H
  int exmode, mode, rc, fd;
#endif
  connect610p ();

#ifdef HAVE_LINUX_PPDEV_H
  fd = sanei_umax_pp_getparport ();
  if (fd > 0)
    {
      rc = ioctl (fd, PPGETMODE, &exmode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
      mode = IEEE1284_MODE_COMPAT;
      rc = ioctl (fd, PPSETMODE, &mode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
    }
#endif

  Outb (DATA, 0x55);
  Outb (CONTROL, 0x05);
  status = Inb (STATUS) & 0xF8;
  if (status != 0x88)
    {
      DBG (0, "SPPsendWord610p found 0x%02X expected 0x88  (%s:%d)\n", status,
	   __FILE__, __LINE__);
      return 0;
    }
  Outb (CONTROL, 0x04);

  Outb (DATA, 0xAA);
  Outb (CONTROL, 0x05);
  status = Inb (STATUS) & 0xF8;
  if (status != 0x88)
    {
      DBG (0, "SPPsendWord610p found 0x%02X expected 0x88  (%s:%d)\n", status,
	   __FILE__, __LINE__);
      return 0;
    }
  Outb (CONTROL, 0x04);

  for (i = 0; i < 4; i++)
    {
      Outb (DATA, cmd[i]);
      Outb (CONTROL, 0x05);
      status = Inb (STATUS) & 0xF8;
      if (status != 0x88)
	{
	  DBG (0, "SPPsendWord610p found 0x%02X expected 0x88  (%s:%d)\n",
	       status, __FILE__, __LINE__);
	  return 0;
	}
      Outb (CONTROL, 0x04);
    }

  Outb (CONTROL, 0x07);
  Outb (DATA, 0xFF);
  tmp = Inb (DATA);
  if (tmp != 0xFF)
    {
      DBG (0, "SPPsendWord610p found 0x%X expected 0xFF  (%s:%d)\n", tmp,
	   __FILE__, __LINE__);
      return 0;
    }
  status = Inb (STATUS) & 0xF8;
  j = 0;
  while ((j < 256) && (status & 0x08))
    {
      j++;
      status = Inb (STATUS) & 0xF8;
    }
  if ((status != 0x80) && (status != 0xA0))
    {
      DBG (0, "SPPsendWord610p found 0x%X expected 0x80 or 0xA0 (%s:%d)\n",
	   status, __FILE__, __LINE__);
      return 0;
    }
  Outb (DATA, 0x7F);
  status = Inb (STATUS) & 0xF8;
  if (status != 0xC0)
    {
      DBG (0, "SPPsendWord610p found 0x%X expected 0xC0  (%s:%d)\n", status,
	   __FILE__, __LINE__);
      return 0;
    }
  Outb (DATA, 0xFF);
  if (cmd[3] == 0xC2)
    {
      Outb (CONTROL, 0x07);
      Outb (DATA, 0xFF);
      tmp = Inb (DATA);
      if (tmp != 0xFF)
	{
	  DBG (0, "SPPsendWord610p found 0x%X expected 0xFF  (%s:%d)\n", tmp,
	       __FILE__, __LINE__);
	  return 0;
	}
      status = Inb (STATUS) & 0xF8;
      if ((status != 0x80) && (status != 0xA0))
	{
	  DBG (0,
	       "SPPsendWord610p found 0x%X expected 0x80 or 0xA0 (%s:%d)\n",
	       status, __FILE__, __LINE__);
	  return 0;
	}
      Outb (DATA, 0x7F);
      status = Inb (STATUS) & 0xF8;
      if (status != 0xC0)
	{
	  DBG (0, "SPPsendWord610p found 0x%X expected 0xC0  (%s:%d)\n",
	       status, __FILE__, __LINE__);
	  return 0;
	}
      Outb (DATA, 0xFF);
    }

#ifdef HAVE_LINUX_PPDEV_H
  fd = sanei_umax_pp_getparport ();
  if (fd > 0)
    {
      rc = ioctl (fd, PPSETMODE, &exmode);
      if (rc)
	DBG (0, "ppdev ioctl returned <%s>  (%s:%d)\n", strerror (errno),
	     __FILE__, __LINE__);
    }
#endif
  disconnect610p ();

  return 1;
}

/* returns 1 on success, 0 otherwise */
static int
EPPsendWord610p (int *cmd)
{
  int i;
  int tmp, control;

  /* send magic tag */
  tmp = Inb (STATUS) & 0xF8;
  if (tmp != 0xC8)
    {
      DBG (0,
	   "EPPsendWord610p failed, expected tmp=0xC8 , found 0x%02X (%s:%d)\n",
	   tmp, __FILE__, __LINE__);
      return 0;
    }

  /* sets to EPP, and get sure that data direction is forward */
  tmp = (Inb (CONTROL) & 0x44) | 0x44;	/* !! */
  Outb (CONTROL, tmp);
  Outb (EPPDATA, 0x55);

  /* bit0 is timeout bit in EPP mode, should we take care of it ? */
  tmp = Inb (STATUS) & 0xF8;
  if (tmp != 0xC8)
    {
      DBG (0,
	   "EPPsendWord610p failed, expected tmp=0xC8 , found 0x%02X (%s:%d)\n",
	   tmp, __FILE__, __LINE__);
      return 0;
    }
  tmp = (Inb (CONTROL) & 0x44) | 0x44;
  Outb (CONTROL, tmp);
  Outb (EPPDATA, 0xAA);

  control = (Inb (CONTROL) & 0xE0) | 0xE4;
  Outb (CONTROL, control);	/* bit 7 + data reverse + reset */
  for (i = 0; i < 10; i++)
    {
      tmp = Inb (STATUS) & 0xF8;
      if (tmp != 0xC8)
	{
	  DBG (0,
	       "EPPsendWord610p failed, expected tmp=0xC8 , found 0x%02X (%s:%d)\n",
	       tmp, __FILE__, __LINE__);
	  return 0;
	}
    }

  i = 0;
  while ((tmp == 0xC8) && (cmd[i] != -1))
    {
      tmp = Inb (STATUS) & 0xF8;
      control = (Inb (CONTROL) & 0x44) | 0x44;	/* !! */
      Outb (CONTROL, control);
      Outb (EPPDATA, cmd[i]);
      i++;
    }

  /* end */
  Outb (DATA, 0xFF);
  control = (Inb (CONTROL) & 0x44) | 0xE4;
  Outb (CONTROL, control);	/* data reverse + ????? */
  tmp = Inb (STATUS) & 0xF8;
  if (tmp == 0xC8)
    {
      for (i = 0; i < 9; i++)
	tmp = Inb (STATUS) & 0xF8;
      scannerStatus = tmp;
    }
  else
    {
      scannerStatus = Inb (EPPDATA);
    }
  if ((tmp != 0xC0) && (tmp != 0xD0))
    {
      DBG (0,
	   "EPPsendWord610p failed  got 0x%02X instead of 0xC0 or 0xD0 (%s:%d)\n",
	   tmp, __FILE__, __LINE__);
      return 0;
    }
  return 1;
}

/*
 * 0: failure
 * 1: success
 */
static int
sendWord (int *cmd)
{
  switch (sanei_umax_pp_getastra ())
    {
    case 610:
      return sendLength610p (cmd);
    case 1220:
    case 1600:
    case 2000:
    default:
      return sendWord1220P (cmd);
    }
}



/******************************************************************************/
/* ringScanner: returns 1 if scanner present, else 0                          */
/******************************************************************************/
/* 
 * in fact this function is really close to CPP macro in
 * /usr/src/linux/drivers/block/paride/epat.c .....
 * we have almost CPP(8)
 */

static int
ringScanner (int count, unsigned long delay)
{
  int status;
  int data;
  int control;
  int ret = 1;

  /* save state */
  data = Inb (DATA);
  control = Inb (CONTROL) & 0x1F;

  /* send -irq,+reset */
  Outb (CONTROL, (control & 0xF) | 0x4);

  /* unhandled case */
  if (g674 == 1)
    {
      DBG (1, "OUCH! %s:%d\n", __FILE__, __LINE__);
      return 0;
    }

  /* send ring string */
  Outb (DATA, 0x22);
  usleep (delay);
  Outb (DATA, 0x22);
  usleep (delay);
  if (count == 5)
    {
      Outb (DATA, 0x22);
      usleep (delay);
      Outb (DATA, 0x22);
      usleep (delay);
      Outb (DATA, 0x22);
      usleep (delay);
    }
  Outb (DATA, 0xAA);
  usleep (delay);
  Outb (DATA, 0xAA);
  usleep (delay);
  if (count == 5)
    {
      Outb (DATA, 0xAA);
      usleep (delay);
      Outb (DATA, 0xAA);
      usleep (delay);
      Outb (DATA, 0xAA);
      usleep (delay);
    }
  Outb (DATA, 0x55);
  usleep (delay);
  Outb (DATA, 0x55);
  usleep (delay);
  if (count == 5)
    {
      Outb (DATA, 0x55);
      usleep (delay);
      Outb (DATA, 0x55);
      usleep (delay);
      Outb (DATA, 0x55);
      usleep (delay);
    }
  Outb (DATA, 0x00);
  usleep (delay);
  Outb (DATA, 0x00);
  usleep (delay);
  if (count == 5)
    {
      Outb (DATA, 0x00);
      usleep (delay);
      Outb (DATA, 0x00);
      usleep (delay);
      Outb (DATA, 0x00);
      usleep (delay);
    }
  Outb (DATA, 0xFF);
  usleep (delay);
  Outb (DATA, 0xFF);
  usleep (delay);
  if (count == 5)
    {
      Outb (DATA, 0xFF);
      usleep (delay);
      Outb (DATA, 0xFF);
      usleep (delay);
      Outb (DATA, 0xFF);
      usleep (delay);
    }

  /* OK ? */
  status = Inb (STATUS) & 0xF8;
  usleep (delay);
  if ((status & 0xB8) != 0xB8)
    {
      DBG (1, "status %d doesn't match! %s:%d\n", status, __FILE__, __LINE__);
      ret = 0;
    }

  /* if OK send 0x87 */
  if (ret)
    {
      Outb (DATA, 0x87);
      usleep (delay);
      Outb (DATA, 0x87);
      usleep (delay);
      if (count == 5)
	{
	  Outb (DATA, 0x87);
	  usleep (delay);
	  Outb (DATA, 0x87);
	  usleep (delay);
	  Outb (DATA, 0x87);
	  usleep (delay);
	}
      status = Inb (STATUS);
      /* status = 126 when scanner not connected .... */
      if ((status & 0xB8) != 0x18)
	{
	  DBG (1, "status %d doesn't match! %s:%d\n", status, __FILE__,
	       __LINE__);
	  ret = 0;
	}
    }

  /* if OK send 0x78 */
  if (ret)
    {
      Outb (DATA, 0x78);
      usleep (delay);
      Outb (DATA, 0x78);
      usleep (delay);
      if (count == 5)
	{
	  Outb (DATA, 0x78);
	  usleep (delay);
	  Outb (DATA, 0x78);
	  usleep (delay);
	  Outb (DATA, 0x78);
	  usleep (delay);
	}
      status = Inb (STATUS);
      if ((status & 0x30) != 0x30)
	{
	  DBG (1, "status %d doesn't match! %s:%d\n", status, __FILE__,
	       __LINE__);
	  ret = 0;
	}
    }

  /* ring OK, send termination */
  if (ret)
    {
      Outb (DATA, 0x08);
      usleep (delay);
      Outb (DATA, 0x08);
      usleep (delay);
      if (count == 5)
	{
	  Outb (DATA, 0x08);
	  usleep (delay);
	  Outb (DATA, 0x08);
	  usleep (delay);
	  Outb (DATA, 0x08);
	  usleep (delay);
	}
      Outb (DATA, 0xFF);
      usleep (delay);
      Outb (DATA, 0xFF);
      usleep (delay);
      if (count == 5)
	{
	  Outb (DATA, 0xFF);
	  usleep (delay);
	  Outb (DATA, 0xFF);
	  usleep (delay);
	  Outb (DATA, 0xFF);
	  usleep (delay);
	}
    }

  /* restore state */
  Outb (CONTROL, control);
  Outb (DATA, data);
  return ret;
}

/*****************************************************************************/
/* test some version       : returns 1 on success, 0 otherwise               */
/*****************************************************************************/


static int
testVersion (int no)
{
  int data;
  int status;
  int control;
  int count;
  int tmp;

  data = Inb (DATA);
  control = Inb (CONTROL) & 0x3F;
  Outb (CONTROL, (control & 0x1F) | 0x04);

  /* send magic sequence */
  Outb (DATA, 0x22);
  Outb (DATA, 0x22);
  Outb (DATA, 0x22);
  Outb (DATA, 0x22);
  Outb (DATA, 0xAA);
  Outb (DATA, 0xAA);
  Outb (DATA, 0xAA);
  Outb (DATA, 0xAA);
  Outb (DATA, 0xAA);
  Outb (DATA, 0xAA);
  Outb (DATA, 0x55);
  Outb (DATA, 0x55);
  Outb (DATA, 0x55);
  Outb (DATA, 0x55);
  Outb (DATA, 0x55);
  Outb (DATA, 0x55);
  Outb (DATA, 0x00);
  Outb (DATA, 0x00);
  Outb (DATA, 0x00);
  Outb (DATA, 0x00);
  Outb (DATA, 0x00);
  Outb (DATA, 0x00);
  Outb (DATA, 0xFF);
  Outb (DATA, 0xFF);
  Outb (DATA, 0xFF);
  Outb (DATA, 0xFF);
  Outb (DATA, 0xFF);
  Outb (DATA, 0xFF);
  Outb (DATA, 0x87);
  Outb (DATA, 0x87);
  Outb (DATA, 0x87);
  Outb (DATA, 0x87);
  Outb (DATA, 0x87);
  Outb (DATA, 0x87);
  Outb (DATA, 0x78);
  Outb (DATA, 0x78);
  Outb (DATA, 0x78);
  Outb (DATA, 0x78);
  Outb (DATA, 0x78);
  Outb (DATA, 0x78);
  tmp = no | 0x88;
  Outb (DATA, tmp);
  Outb (DATA, tmp);
  Outb (DATA, tmp);
  Outb (DATA, tmp);
  Outb (DATA, tmp);
  Outb (DATA, tmp);

  /* test status */
  status = Inb (STATUS);
  status = Inb (STATUS);
  if ((status & 0xB8) != 0)
    {
      /* 1600P fails here */
      DBG (64, "status %d doesn't match! %s:%d\n", status, __FILE__,
	   __LINE__);
      Outb (CONTROL, control);
      Outb (DATA, data);
      return 0;
    }

  count = 0xF0;
  do
    {
      tmp = no | 0x80;
      Outb (DATA, tmp);
      Outb (DATA, tmp);
      Outb (DATA, tmp);
      Outb (DATA, tmp);
      Outb (DATA, tmp);
      Outb (DATA, tmp);
      tmp = no | 0x88;
      Outb (DATA, tmp);
      Outb (DATA, tmp);
      Outb (DATA, tmp);
      Outb (DATA, tmp);
      Outb (DATA, tmp);
      Outb (DATA, tmp);

      /* command received ? */
      status = Inb (STATUS);
      status = ((status << 1) & 0x70) | (status & 0x80);
      if (status != count)
	{
	  /* since failure is expected, we dont't alaways print */
	  /* this message ...                                   */
	  DBG (2, "status %d doesn't match count 0x%X! %s:%d\n", status,
	       count, __FILE__, __LINE__);
	  Outb (CONTROL, control);
	  Outb (DATA, data);
	  return 0;
	}

      /* next */
      count -= 0x10;
    }
  while (count > 0);

  /* restore port , successful exit */
  Outb (CONTROL, control);
  Outb (DATA, data);
  return 1;
}


/* sends len bytes to scanner        */
/* needs data channel to be set up   */
/* returns 1 on success, 0 otherwise */
static int
sendLength (int *cmd, int len)
{
  int i;
  int reg, wait;
  int try = 0;

  /* send header */
retry:
  wait = registerRead (0x19) & 0xF8;

  registerWrite (0x1C, 0x55);
  reg = registerRead (0x19) & 0xF8;

  registerWrite (0x1C, 0xAA);
  reg = registerRead (0x19) & 0xF8;

  /* sync when needed */
  if ((wait & 0x08) == 0x00)
    {
      reg = registerRead (0x1C);
      while (((reg & 0x10) != 0x10) && (reg != 0x6B) && (reg != 0xAB)
	     && (reg != 0x23))
	{
	  DBG (0,
	       "sendLength failed, expected reg & 0x10=0x10 , found 0x%02X (%s:%d)\n",
	       reg, __FILE__, __LINE__);
	  if (try > 10)
	    {
	      DBG (0, "Aborting...\n");
	      return 0;
	    }
	  else
	    {
	      DBG (0, "Retrying ...\n");
	    }
	  /* resend */
	  epilogue ();
	  prologue (0x10);
	  try++;
	  goto retry;
	}
      for (i = 0; i < 10; i++)
	{
	  reg = registerRead (0x19) & 0xF8;
	  if (reg != 0xC8)
	    {
	      DBG (0, "Unexpected reg19=0x%2X  (%s:%d)\n", reg, __FILE__,
		   __LINE__);
	      /* 0xF0 certainly means error */
	      if ((reg == 0xC0) || (reg == 0xD0) || (reg == 0x80))
		{
		  /* resend */
		  try++;
		  if (try > 20)
		    {
		      DBG (0, "sendLength retry failed (%s:%d)\n", __FILE__,
			   __LINE__);
		      return 0;
		    }

		  epilogue ();
		  sendCommand (0x00);
		  sendCommand (0xE0);
		  Outb (DATA, 0x00);
		  Outb (CONTROL, 0x01);
		  Outb (CONTROL, 0x04);
		  sendCommand (0x30);

		  prologue (0x10);
		  goto retry;
		}
	    }
	}
      do
	{
	  if ((reg != 0xC0) && (reg != 0xD0) && (reg != 0xC8))
	    {
	      /* status has changed while waiting */
	      /* but it's too early               */
	      DBG (0, "Unexpected reg19=0x%2X  (%s:%d)\n", reg, __FILE__,
		   __LINE__);
	    }
	  /* 0xF0 certainly means error */
	  if ((reg == 0xC0) || (reg == 0xD0) || (reg == 0x80))
	    {
	      /* resend */
	      try++;
	      epilogue ();

	      sendCommand (0x00);
	      sendCommand (0xE0);
	      Outb (DATA, 0x00);
	      Outb (CONTROL, 0x01);
	      Outb (CONTROL, 0x04);
	      sendCommand (0x30);

	      prologue (0x10);
	      goto retry;
	    }
	  reg = registerRead (0x19) & 0xF8;
	}
      while (reg != 0xC8);
    }

  /* send bytes */
  i = 0;
  while ((reg == 0xC8) && (i < len))
    {
      /* write byte */
      registerWrite (0x1C, cmd[i]);
      reg = registerRead (0x19) & 0xF8;

      /* 1B handling: escape it to confirm value  */
      if (cmd[i] == 0x1B)
	{
	  registerWrite (0x1C, cmd[i]);
	  reg = registerRead (0x19) & 0xF8;
	}
      i++;
    }
  DBG (16, "sendLength, reg19=0x%02X (%s:%d)\n", reg, __FILE__, __LINE__);
  if ((reg != 0xC0) && (reg != 0xD0))
    {
      DBG (0,
	   "sendLength failed  got 0x%02X instead of 0xC0 or 0xD0 (%s:%d)\n",
	   reg, __FILE__, __LINE__);
      DBG (0, "Blindly going on .....\n");
    }

  /* check if 'finished status' received too early */
  if (((reg == 0xC0) || (reg == 0xD0)) && (i != len))
    {
      DBG (0, "sendLength failed: sent only %d bytes out of %d (%s:%d)\n", i,
	   len, __FILE__, __LINE__);
      return 0;
    }


  reg = registerRead (0x1C);
  DBG (16, "sendLength, reg1C=0x%02X (%s:%d)\n", reg, __FILE__, __LINE__);

  /* model 0x07 has always the last bit set to 1 */
  scannerStatus = reg & 0xFC;
  reg = reg & 0x10;
  if ((reg != 0x10) && (scannerStatus != 0x68) && (scannerStatus != 0xA8))
    {
      DBG (0, "sendLength failed: acknowledge not received (%s:%d)\n",
	   __FILE__, __LINE__);
      return 0;
    }
  if (try)
    {
      DBG (0, "sendLength retry success (retry %d time%s) ... (%s:%d)\n", try,
	   (try > 1) ? "s" : "", __FILE__, __LINE__);
    }
  return 1;
}


/* sends data bytes to scanner       */
/* needs data channel to be set up   */
/* returns 1 on success, 0 otherwise */
static int
sendData610p (int *cmd, int len)
{
  int i, status, j;

  i = 0;
  status = 0xC8;
  /* while ((i < len) && ((status & 0x08) == 0x08)) XXX STEF XXX */
  while (i < len)
    {
      /* escape special values */
      if (cmd[i] == 0x1B)
	status = putByte610p (0x1B);
      if (i > 0)
	{
	  if ((cmd[i] == 0xAA) && (cmd[i - 1] == 0x55))
	    status = putByte610p (0x1B);
	}
      /* regular values */
      status = putByte610p (cmd[i]);
      i++;
    }
  j = 0;
  while ((status & 0x08) && (j < 256))
    {
      status = getStatus610p ();
      j++;
    }
  if ((status != 0xC0) && (status != 0xD0))
    {
      DBG (0,
	   "sendData610p() failed, status=0x%02X, expected 0xC0 or 0xD0 (%s:%d)\n",
	   status, __FILE__, __LINE__);
      return 0;
    }

  /* check if 'finished status' received too early */
  if (i < len)
    {
      DBG (0, "sendData610p failed: sent only %d bytes out of %d (%s:%d)\n",
	   i, len, __FILE__, __LINE__);
      return 0;
    }
  return 1;
}


/* sends data bytes to scanner       */
/* needs data channel to be set up   */
/* returns 1 on success, 0 otherwise */
static int
sendData (int *cmd, int len)
{
  int i;
  int reg;

  if (sanei_umax_pp_getastra () == 610)
    return sendData610p (cmd, len);

  /* send header */
  reg = registerRead (0x19) & 0xF8;

  /* send bytes */
  i = 0;
  while ((reg == 0xC8) && (i < len))
    {
      /* write byte */
      registerWrite (0x1C, cmd[i]);
      reg = registerRead (0x19) & 0xF8;

      /* 1B handling: escape it to confirm value  */
      if (cmd[i] == 0x1B)
	{
	  registerWrite (0x1C, 0x1B);
	  reg = registerRead (0x19) & 0xF8;
	}

      /* escape 55 AA pattern by adding 1B */
      if ((i < len - 1) && (cmd[i] == 0x55) && (cmd[i + 1] == 0xAA))
	{
	  registerWrite (0x1C, 0x1B);
	  reg = registerRead (0x19) & 0xF8;
	}

      /* next value */
      i++;
    }
  DBG (16, "sendData, reg19=0x%02X (%s:%d)\n", reg, __FILE__, __LINE__);
  if ((reg != 0xC0) && (reg != 0xD0))
    {
      DBG (0, "sendData failed  got 0x%02X instead of 0xC0 or 0xD0 (%s:%d)\n",
	   reg, __FILE__, __LINE__);
      DBG (0, "Blindly going on .....\n");
    }

  /* check if 'finished status' received too early */
  if (((reg == 0xC0) || (reg == 0xD0)) && (i < len))
    {
      DBG (0, "sendData failed: sent only %d bytes out of %d (%s:%d)\n", i,
	   len, __FILE__, __LINE__);
      return 0;
    }


  reg = registerRead (0x1C);
  DBG (16, "sendData, reg1C=0x%02X (%s:%d)\n", reg, __FILE__, __LINE__);

  /* model 0x07 has always the last bit set to 1 */
  scannerStatus = reg & 0xFC;
  reg = reg & 0x10;
  if ((reg != 0x10) && (scannerStatus != 0x68) && (scannerStatus != 0xA8)
      && (scannerStatus != 0x20))
    {
      DBG (0, "sendData failed: acknowledge not received (%s:%d)\n", __FILE__,
	   __LINE__);
      return 0;
    }
  return 1;
}


/* receive data bytes from scanner   */
/* needs data channel to be set up   */
/* returns 1 on success, 0 otherwise */
/* uses pausedBufferRead             */
static int
pausedReadData (int size, unsigned char *dest)
{
  int reg;
  int tmp;
  int read;

  REGISTERWRITE (0x0E, 0x0D);
  REGISTERWRITE (0x0F, 0x00);
  reg = registerRead (0x19) & 0xF8;
  if ((reg != 0xC0) && (reg != 0xD0))
    {
      DBG (0, "Unexpected reg19: 0x%02X instead of 0xC0 or 0xD0 (%s:%d)\n",
	   reg, __FILE__, __LINE__);
      return 0;
    }
  if (gMode == UMAX_PP_PARPORT_ECP)
    {
      REGISTERWRITE (0x1A, 0x44);
    }
  REGISTERREAD (0x0C, 0x04);
  REGISTERWRITE (0x0C, 0x44);	/* sets data direction ? */
  if (gMode == UMAX_PP_PARPORT_ECP)
    {
      compatMode ();
      Outb (CONTROL, 0x04);	/* reset ? */
      ECPSetBuffer (size);
      read = ECPbufferRead (size, dest);
      DBG (16, "ECPbufferRead(%d,dest) passed (%s:%d)\n", size, __FILE__,
	   __LINE__);
      REGISTERWRITE (0x1A, 0x84);
    }
  else
    {
      read = pausedBufferRead (size, dest);
    }
  if (read < size)
    {
      DBG (16,
	   "pausedBufferRead(%d,dest) failed, only got %d bytes (%s:%d)\n",
	   size, read, __FILE__, __LINE__);
      return 0;
    }
  DBG (16, "pausedBufferRead(%d,dest) passed (%s:%d)\n", size, __FILE__,
       __LINE__);
  REGISTERWRITE (0x0E, 0x0D);
  REGISTERWRITE (0x0F, 0x00);
  return 1;
}



/* receive data bytes from scanner   */
/* needs data channel to be set up   */
/* returns 1 on success, 0 otherwise */
static int
receiveData610p (int *cmd, int len)
{
  int i;
  int status;

  i = 0;
  status = 0xD0;
  byteMode ();
  while (i < len)
    {
      status = Inb (STATUS) & 0xF8;
      Outb (CONTROL, 0x26);	/* data reverse+ 'reg' */
      cmd[i] = Inb (DATA);
      Outb (CONTROL, 0x24);	/* data reverse+ 'reg' */
      i++;
    }
  if (status != 0xC0)
    {
      DBG (0, "receiveData610p failed  got 0x%02X instead of 0xC0 (%s:%d)\n",
	   status, __FILE__, __LINE__);
      DBG (0, "Blindly going on .....\n");
    }

  /* check if 'finished status' received to early */
  if ((status == 0xC0) && (i != len))
    {
      DBG (0,
	   "receiveData610p failed: received only %d bytes out of %d (%s:%d)\n",
	   i, len, __FILE__, __LINE__);
      return 0;
    }
  return 1;
}

/* receive data bytes from scanner   */
/* needs data channel to be set up   */
/* returns 1 on success, 0 otherwise */
static int
receiveData (int *cmd, int len)
{
  int i;
  int reg;

  /* send header */
  reg = registerRead (0x19) & 0xF8;

  /* send bytes */
  i = 0;
  while (((reg == 0xD0) || (reg == 0xC0)) && (i < len))
    {
      /* write byte */
      cmd[i] = registerRead (0x1C);
      reg = registerRead (0x19) & 0xF8;
      i++;
    }
  DBG (16, "receiveData, reg19=0x%02X (%s:%d)\n", reg, __FILE__, __LINE__);
  if ((reg != 0xC0) && (reg != 0xD0))
    {
      DBG (0, "sendData failed  got 0x%02X instead of 0xC0 or 0xD0 (%s:%d)\n",
	   reg, __FILE__, __LINE__);
      DBG (0, "Blindly going on .....\n");
    }

  /* check if 'finished status' received to early */
  if (((reg == 0xC0) || (reg == 0xD0)) && (i != len))
    {
      DBG (0,
	   "receiveData failed: received only %d bytes out of %d (%s:%d)\n",
	   i, len, __FILE__, __LINE__);
      return 0;
    }


  reg = registerRead (0x1C);
  DBG (16, "receiveData, reg1C=0x%02X (%s:%d)\n", reg, __FILE__, __LINE__);

  /* model 0x07 has always the last bit set to 1 */
  scannerStatus = reg & 0xF8;
  reg = reg & 0x10;
  if ((reg != 0x10) && (scannerStatus != 0x68) && (scannerStatus != 0xA8))
    {
      DBG (0, "receiveData failed: acknowledge not received (%s:%d)\n",
	   __FILE__, __LINE__);
      return 0;
    }
  return 1;
}


/* 1=success, 0 failed */
static int
fonc001 (void)
{
  int i;
  int res;
  int reg;

  res = 1;
  while (res == 1)
    {
      registerWrite (0x1A, 0x0C);
      registerWrite (0x18, 0x40);

      /* send 0x06 */
      registerWrite (0x1A, 0x06);
      for (i = 0; i < 10; i++)
	{
	  reg = registerRead (0x19) & 0xF8;
	  if ((reg & 0x78) == 0x38)
	    {
	      res = 0;
	      break;
	    }
	}
      if (res == 1)
	{
	  registerWrite (0x1A, 0x00);
	  registerWrite (0x1A, 0x0C);
	}
    }

  /* send 0x07 */
  registerWrite (0x1A, 0x07);
  res = 1;
  for (i = 0; i < 10; i++)
    {
      reg = registerRead (0x19) & 0xF8;
      if ((reg & 0x78) == 0x38)
	{
	  res = 0;
	  break;
	}
    }
  if (res != 0)
    return 0;

  /* send 0x04 */
  registerWrite (0x1A, 0x04);
  res = 1;
  for (i = 0; i < 10; i++)
    {
      reg = registerRead (0x19) & 0xF8;
      if ((reg & 0xF8) == 0xF8)
	{
	  res = 0;
	  break;
	}
    }
  if (res != 0)
    return 0;

  /* send 0x05 */
  registerWrite (0x1A, 0x05);
  res = 1;
  for (i = 0; i < 10; i++)
    {
      reg = registerRead (0x1A);
      if (reg == 0x05)
	{
	  res = 0;
	  break;
	}
    }
  if (res != 0)
    return 0;

  /* end */
  registerWrite (0x1A, 0x84);
  return 1;
}







/* 1 OK, 0 failed */
static int
foncSendWord (int *cmd)
{
  prologue (0x10);
  if (sendWord (cmd) == 0)
    {
      DBG (0, "sendWord(cmd) failed (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }
  epilogue ();

  return 1;
}


static int
cmdSetDataBuffer (int *data)
{
  int cmd1[] = { 0x00, 0x00, 0x22, 0x88, -1 };	/* 34 bytes write on channel 8 */
  int cmd2[] =
    { 0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0C, 0x00, 0x03, 0xC1, 0x80,
    0x00, 0x20, 0x02, 0x00, 0x16, 0x41, 0xE0, 0xAC, 0x03, 0x03, 0x00, 0x00,
    0x46, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, -1
  };
  int cmd3[] = { 0x00, 0x08, 0x00, 0x84, -1 };	/* 2048 bytes size write on channel 4 (data) */
  int cmd4[] = { 0x00, 0x08, 0x00, 0xC4, -1 };	/* 2048 bytes size read on channel 4 (data) */
  int i;
  unsigned char dest[2048];

  /* cmdSet(8,34,cmd2), but without prologue/epilogue */
  /* set block length to 34 bytes on 'channel 8' */
  sendWord (cmd1);
  DBG (16, "sendWord(cmd1) passed (%s:%d) \n", __FILE__, __LINE__);

  /* sendData */
  sendData (cmd2, 0x22);
  DBG (16, "sendData(cmd2) passed (%s:%d) \n", __FILE__, __LINE__);

  if (DBG_LEVEL >= 128)
    {
      bloc8Decode (cmd2);
    }

  /* set block length to 2048, write on 'channel 4' */
  sendWord (cmd3);
  DBG (16, "sendWord(cmd3) passed (%s:%d) \n", __FILE__, __LINE__);

  if (sendData (data, 2048) == 0)
    {
      DBG (0, "sendData(data,%d) failed (%s:%d)\n", 2048, __FILE__, __LINE__);
      return 0;
    }
  TRACE (16, "sendData(data,2048) passed ...");

  /* read back all data sent to 'channel 4' */
  sendWord (cmd4);
  DBG (16, "sendWord(cmd4) passed (%s:%d) \n", __FILE__, __LINE__);

  if (pausedReadData (2048, dest) == 0)
    {
      DBG (16, "pausedReadData(2048,dest) failed (%s:%d)\n", __FILE__,
	   __LINE__);
      return 0;
    }
  DBG (16, "pausedReadData(2048,dest) passed (%s:%d)\n", __FILE__, __LINE__);

  /* dest should hold the same data than donnees */
  for (i = 0; i < 2047; i++)
    {
      if (data[i] != (int) (dest[i]))
	{
	  DBG
	    (0,
	     "Warning data read back differs: expected %02X found dest[%d]=%02X ! (%s:%d)\n",
	     data[i], i, dest[i], __FILE__, __LINE__);
	}
    }
  return 1;
}


/* 1: OK
   0: end session failed */

int
sanei_umax_pp_endSession (void)
{
  int zero[5] = { 0, 0, 0, 0, -1 };

  if (sanei_umax_pp_getastra () != 610)
    {
      prologue (0x00);
      sendWord (zero);
      epilogue ();
      sanei_umax_pp_cmdSync (0xC2);
      sanei_umax_pp_cmdSync (0x00);	/* cancels any pending operation */
      sanei_umax_pp_cmdSync (0x00);	/* cancels any pending operation */
    }
  else
    {
      CMDSYNC (0x00);
      CMDSYNC (0xC2);
      CMDSYNC (0x00);
      CMDSYNC (0x00);
    }
  compatMode ();

  /* restore port state */
  Outb (DATA, gData);
  Outb (CONTROL, gControl);

  /* OUF */
  DBG (1, "End session done ...\n");
  return 1;
}


/* initialize scanner with default values
 * and do head re-homing if needed */
int
initScanner610p (int recover)
{
  int first, rc, x;
  int cmd55AA[9] = { 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, -1 };
  int cmd02[17] = { 0x02, 0x80, 0x00, 0x40, 0x30, 0x00, 0xC0, 0x2F,
    0x2F, 0x07, 0x00, 0x00, 0x00, 0x80, 0xF0, 0x00, -1
  };
  int op01[17] =
    { 0x01, 0x00, 0x32, 0x70, 0x00, 0x00, 0xC0, 0x2F, 0x17, 0x05, 0x00, 0x00,
    0x00, 0x80, 0xA4, 0x00, -1
  };
  int op11[17] =
    { 0x01, 0x80, 0x0C, 0x70, 0x00, 0x00, 0xC0, 0x2F, 0x17, 0x01, 0x00, 0x00,
    0x00, 0x80, 0xA4, 0x00, -1
  };
  int op21[17] =
    { 0x01, 0x00, 0x01, 0x40, 0x30, 0x00, 0xC0, 0x2F, 0x17, 0x05, 0x00, 0x00,
    0x00, 0x80, 0xF4, 0x00, -1
  };
  int op31[17] =
    { 0x01, 0x00, 0x39, 0x73, 0x00, 0x00, 0xC0, 0x2F, 0x17, 0x05, 0x00, 0x00,
    0x00, 0x80, 0xB4, 0x00, -1
  };

  int op02[35] = { 0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0C,
    0x00, 0x04, 0x40, 0x01, 0x00, 0x20, 0x02, 0x00,
    0x76, 0x00, 0x75, 0xEF, 0x06, 0x00, 0x00, 0xF6,
    0x4D, 0xA0, 0x00, 0x8B, 0x4D, 0x4B, 0xD0, 0x68,
    0xDF, 0x1B, -1
  };
  int op22[35] = { 0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0C,
    0x00, 0x03, 0xC1, 0x80, 0x00, 0x20, 0x02, 0x00,
    0x16, 0x80, 0x15, 0x78, 0x03, 0x03, 0x00, 0x00,
    0x46, 0xA0, 0x00, 0x8B, 0x4D, 0x4B, 0xD0, 0x68,
    0xDF, 0x1B, -1
  };

  int op03[9] = { 0x00, 0x00, 0x00, 0xAA, 0xCC, 0xEE, 0xFF, 0xFF, -1 };
  struct timeval tv;

  byteMode ();			/* just to get sure */
  first = 0;
  rc = inquire ();

  /* get time to handle settle time delay */
  gettimeofday (&tv, NULL);
  gTime = tv.tv_sec;
  /* default delay */
  gDelay = 5;

  if (rc == 0)
    {
      DBG (0, "inquire() failed ! (%s:%d) \n", __FILE__, __LINE__);
      return 0;
    }
  if (rc == 2)
    {
      /* same value used by windows driver */
      gDelay = 45;
      DBG (1, "inquire() signals re-homing needed ... (%s:%d) \n",
	   __FILE__, __LINE__);
      first = 1;
    }
  DBG (1, "inquire() passed ... (%s:%d) \n", __FILE__, __LINE__);

  rc = loadDefaultTables ();
  if (rc == 0)
    {
      DBG (0, "loadDefaultTables() failed ! (%s:%d) \n", __FILE__, __LINE__);
      return 0;
    }
  DBG (1, "loadDefaultTables() passed ... (%s:%d) \n", __FILE__, __LINE__);
  if (recover)
    first = 1;

  CMDSETGET (2, 0x10, cmd02);
  CMDSETGET (1, 0x08, cmd55AA);

  if (!first)
    {
      CMDSYNC (0x00);
      CMDSYNC (0xC2);
      CMDSYNC (0x00);
      DBG (1, "initScanner610p done ...\n");
      return 1;
    }

  /* here we do re-homing 
   * since it is first probe or recover */
  /* move forward */
  CMDSYNC (0xC2);
  if (!recover)
    {
      CMDSETGET (2, 0x10, op01);
      CMDSETGET (8, 0x22, op02);
      CMDSYNC (0xC2);
      CMDSYNC (0x00);
      CMDSETGET (4, 0x08, op03);
      CMDSYNC (0x40);
      CMDSYNC (0xC2);
      sleep (2);
    }

  /* move backward */
  CMDSETGET (2, 0x10, op11);
  CMDSETGET (8, 0x22, op02);
  CMDSYNC (0xC2);
  CMDSYNC (0x00);
  CMDSYNC (0x00);
  CMDSETGET (4, 0x08, op03);
  CMDSYNC (0x40);
  CMDSYNC (0xC2);
  sleep (2);

  /* means 'CONTINUE MOVE' */
  CMDSYNC (0x00);
  while ((scannerStatus & MOTOR_BIT) == 0)
    {
      CMDSYNC (0xC2);
      CMDSETGET (2, 0x10, op21);
      CMDSETGET (8, 0x22, op22);
      CMDSYNC (0x40);
      usleep (20000);
    }
  CMDSYNC (0xC2);
  CMDSYNC (0x00);

  /* send head away */
  if (!recover)
    {
      CMDSETGET (2, 0x10, op31);
      CMDSETGET (8, 0x22, op02);
      if (DBG_LEVEL > 8)
	{
	  bloc2Decode (op31);
	  bloc8Decode (op02);
	}
      CMDSYNC (0xC2);
      CMDSYNC (0x00);
      CMDSETGET (4, 0x08, op03);
      CMDSYNC (0x40);
      CMDSYNC (0xC2);
      sleep (9);
    }

  CMDSYNC (0x00);

  /* this code has been added, without corresponding logs/
   * it seem I just can't found 'real' parking command ...
   */
  /* send park command */
  if (sanei_umax_pp_park () == 0)
    {
      TRACE (0, "sanei_umax_pp_park failed! ");
      return 0;
    }
  /* and wait it to succeed */
  if (sanei_umax_pp_parkWait () == 0)
    {
      TRACE (0, "sanei_umax_pp_parkWait failed! ");
      return 0;
    }

  /* override gamma table with 610P defaults */
  for (x = 0; x < 256; x++)
    {
      ggRed[x] = x;
      ggGreen[x] = x;
      ggBlue[x] = x;
    }

  DBG (1, "initScanner610p done ...\n");
  return 1;
}

/* 1: OK
   2: homing happened
   3: scanner busy
   0: init failed 

   init transport layer
   init scanner
*/

int
sanei_umax_pp_initScanner (int recover)
{
  int i;
  int status;
  int readcmd[64];
  /* in umax1220u, this buffer is opc[16] */
  int sentcmd[17] =
    { 0x02, 0x80, 0x00, 0x70, 0x00, 0x00, 0x00, 0x2F, 0x2F, 0x07, 0x00,
    0x00, 0x00, 0x80, 0xF0, 0x00, -1
  };
  int cmdA7[9] = { 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, -1 };

  if (sanei_umax_pp_getastra () == 610)
    return initScanner610p (recover);

  if (getModel () == 0x07)
    sentcmd[15] = 0x00;
  else
    sentcmd[15] = 0x18;

  /* fails here if there is an unfinished previous scan */
  CMDSETGET (0x02, 16, sentcmd);

  /* needs some init */
  if (sentcmd[15] == 0x18)
    {
      sentcmd[15] = 0x00;	/* was 0x18 */
      CMDSETGET (0x02, 16, sentcmd);

      /* in umax1220u, this buffer does not exist */
      CMDSETGET (0x01, 8, cmdA7);
    }


  /* ~ opb3: inquire status */
  CMDGET (0x08, 36, readcmd);
  if (DBG_LEVEL >= 32)
    {
      bloc8Decode (readcmd);
    }
  DBG (16, "cmdGet(0x08,36,readcmd) passed (%s:%d)\n", __FILE__, __LINE__);

  /* is the scanner busy parking ? */
  status = sanei_umax_pp_scannerStatus ();
  DBG (8, "INQUIRE SCANNER STATUS IS 0x%02X  (%s:%d)\n", status, __FILE__,
       __LINE__);
  if ((!recover) && (status & MOTOR_BIT) == 0x00)
    {
      DBG (1, "Warning: scanner motor on, giving up ...  (%s:%d)\n", __FILE__,
	   __LINE__);
      return 3;
    }

  /* head homing needed ? */
  if ((readcmd[34] != 0x1A) || (recover == 1))
    {				/* homing needed, readcmd[34] should be 0x48 */
      int op01[17] =
	{ 0x01, 0x00, 0x32, 0x70, 0x00, 0x00, 0x60, 0x2F, 0x17, 0x05, 0x00,
	0x00, 0x00, 0x80, 0xE4, 0x00, -1
      };
      int op05[17] =
	{ 0x01, 0x00, 0x01, 0x70, 0x00, 0x00, 0x60, 0x2F, 0x13, 0x05, 0x00,
	0x00, 0x00, 0x80, 0xF0, 0x00, -1
      };
      int op02[37] =
	{ 0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0C, 0x00, 0x04, 0x40,
	0x01, 0x00, 0x20, 0x02, 0x00, 0x16, 0x00, 0x70, 0x9F, 0x06, 0x00,
	0x00, 0xF6, 0x4D, 0xA0, 0x00, 0x8B, 0x49, 0x2A, 0xE9, 0x68, 0xDF,
	0x0B, 0x1A, 0x00, -1
      };
      int op04[37] =
	{ 0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0C, 0x00, 0x03, 0xC1,
	0x80, 0x00, 0x20, 0x02, 0x00, 0x16, 0x80, 0x15, 0x78, 0x03, 0x03,
	0x00, 0x00, 0x46, 0xA0, 0x00, 0x8B, 0x49, 0x2A, 0xE9, 0x68, 0xDF,
	0x0B, 0x1A, 0x00, -1
      };
      int op03[9] = { 0x00, 0x00, 0x00, 0xAA, 0xCC, 0xEE, 0xFF, 0xFF, -1 };

      CMDSYNC (0xC2);
      CMDSETGET (0x02, 16, op01);
      CMDSETGET (0x08, 36, op02);

      if (!recover)
	{
	  CMDSYNC (0xC2);
	  CMDSYNC (0x00);
	  CMDSETGET (0x04, 8, op03);
	  CMDSYNC (0x40);
	  do
	    {
	      sleep (1);
	      CMDSYNC (0xC2);
	    }
	  while ((sanei_umax_pp_scannerStatus () & 0x90) != 0x90);

	  op01[2] = 0x1E;
	  op01[9] = 0x01;
	  CMDSETGET (0x02, 16, op01);
	  CMDSETGET (0x08, 36, op02);
	  CMDSYNC (0x00);
	  CMDSYNC (0x00);
	  CMDSETGET (0x04, 8, op03);

	  CMDSYNC (0x40);
	  do
	    {
	      sleep (1);
	      CMDSYNC (0xC2);
	    }
	  while ((sanei_umax_pp_scannerStatus () & 0x90) != 0x90);
	  CMDSYNC (0x00);
	}

      for (i = 0; i < 4; i++)
	{

	  do
	    {
	      usleep (500000);
	      CMDSYNC (0xC2);
	      status = sanei_umax_pp_scannerStatus ();
	      status = status & 0x10;
	    }
	  while (status != 0x10);	/* was 0x90 */
	  CMDSETGET (0x02, 16, op05);
	  CMDSETGET (0x08, 36, op04);
	  CMDSYNC (0x40);
	  status = sanei_umax_pp_scannerStatus ();
	  DBG (16, "loop %d passed, status=0x%02X (%s:%d)\n", i, status,
	       __FILE__, __LINE__);
	}



      /* get head back home ... */
      do
	{
	  i++;
	  do
	    {
	      usleep (500000);
	      CMDSYNC (0xC2);
	      status = sanei_umax_pp_scannerStatus ();
	      status = status & 0x10;
	    }
	  while (status != 0x10);	/* was 0x90 */
	  CMDSETGET (0x02, 16, op05);
	  CMDSETGET (0x08, 36, op04);
	  CMDSYNC (0x40);
	  status = sanei_umax_pp_scannerStatus ();
	  DBG (16, "loop %d passed, status=0x%02X (%s:%d)\n", i, status,
	       __FILE__, __LINE__);
	}
      while ((status & MOTOR_BIT) == 0x00);	/* 0xD0 when head is back home */

      do
	{
	  usleep (500000);
	  CMDSYNC (0xC2);
	}
      while ((sanei_umax_pp_scannerStatus () & 0x90) != 0x90);


      /* don't do automatic home sequence on recovery */
      if (!recover)
	{
	  CMDSYNC (0x00);
	  op01[2] = 0x1A;
	  op01[3] = 0x74;	/* was 0x70 */
	  op01[9] = 0x05;	/* initial value */
	  op01[14] = 0xF4;	/* was 0xE4 */
	  CMDSETGET (0x02, 16, op01);
	  CMDSETGET (0x08, 36, op02);
	  CMDSYNC (0xC2);
	  CMDSYNC (0x00);
	  CMDSETGET (0x04, 8, op03);
	  CMDSYNC (0x40);

	  /* wait for automatic homing sequence */
	  /* to complete, thus avoiding         */
	  /* scanning too early                 */
	  do
	    {
	      /* the sleep is here to prevent */
	      /* excessive CPU usage, can be  */
	      /* removed, if we don't care    */
	      sleep (3);
	      CMDSYNC (0xC2);
	      DBG (16, "PARKING polling status is 0x%02X   (%s:%d)\n",
		   sanei_umax_pp_scannerStatus (), __FILE__, __LINE__);
	    }
	  while (sanei_umax_pp_scannerStatus () == 0x90);
	}

      /* signal homing */
      return 2;
    }


  /* end ... */
  DBG (1, "Scanner init done ...\n");
  return 1;
}


/* 
        1: OK
           2: failed, try again
           0: init failed 

        initialize the transport layer
   
   */

static int
initTransport610p (void)
{
  int tmp, i;
  int zero[5] = { 0, 0, 0, 0, -1 };

  /* test EPP availability */
  connect610p ();
  if (sync610p () == 0)
    {
      DBG (0,
	   "sync610p failed! Scanner not present or powered off ...  (%s:%d)\n",
	   __FILE__, __LINE__);
      return 0;
    }
  if (EPPsendWord610p (zero) == 0)
    {
      DBG (1, "No EPP mode detected\n");
      gMode = UMAX_PP_PARPORT_BYTE;
    }
  else
    {
      DBG (1, "EPP mode detected\n");
      gMode = UMAX_PP_PARPORT_EPP;
    }
  disconnect610p ();

  /* set up to bidirectionnal */
  /* in fact we could add support for EPP */
  /* but let's make 610 work first */
  if (gMode == UMAX_PP_PARPORT_BYTE)
    {
      byteMode ();

      /* reset after failure */
      /* set to data reverse */
      Outb (CONTROL, 0x2C);
      Inb (CONTROL);
      for (i = 0; i < 10; i++)
	Outb (DATA, 0xAA);
      tmp = Inb (DATA);
      tmp = Inb (DATA);
      if (tmp != 0xFF)
	{
	  DBG (1, "Found 0x%X expected 0xFF  (%s:%d)\n", tmp, __FILE__,
	       __LINE__);
	}
      for (i = 0; i < 4; i++)
	{
	  Outb (DATA, 0x00);
	  tmp = Inb (DATA);
	  if (tmp != 0xFF)
	    {
	      DBG (1, "Found 0x%X expected 0xFF  (%s:%d)\n", tmp, __FILE__,
		   __LINE__);
	      return 0;
	    }
	  Outb (DATA, 0xFF);
	  tmp = Inb (DATA);
	  if (tmp != 0xFF)
	    {
	      DBG (1, "Found 0x%X expected 0xFF  (%s:%d)\n", tmp, __FILE__,
		   __LINE__);
	      return 0;
	    }
	}
      TRACE (16, "RESET done... ");
      byteMode ();

      if (SPPsendWord610p (zero) == 0)
	{
	  DBG (0, "SPPsendWord610p(zero) failed! (%s:%d)\n", __FILE__,
	       __LINE__);
	  return 0;
	}
      TRACE (16, "SPPsendWord610p(zero) passed... ");
    }

  /* OK ! */
  TRACE (1, "initTransport610p done... ");
  return 1;
}

/* 
        1: OK
           2: failed, try again
           0: init failed 

        initialize the transport layer
   
   */

static int
initTransport1220P (int recover)	/* ECP OK !! */
{
  int i, j;
  int reg, tmp;
  unsigned char *dest = NULL;
  int zero[5] = { 0, 0, 0, 0, -1 };
  int model, nb;

  connect ();
  DBG (16, "connect() passed... (%s:%d)\n", __FILE__, __LINE__);
  gEPAT = 0xC7;
  reg = registerRead (0x0B);
  if (reg != gEPAT)
    {
      DBG (16, "Error! expected reg0B=0x%02X, found 0x%02X! (%s:%d) \n",
	   gEPAT, reg, __FILE__, __LINE__);
      DBG (16, "Scanner needs probing ... \n");
      if (sanei_umax_pp_probeScanner (recover) != 1)
	{
	  return 0;
	}
      else
	{
	  return 2;		/* signals retry initTransport() */
	}
    }

  reg = registerRead (0x0D);
  reg = (reg & 0xE8) | 0x43;
  registerWrite (0x0D, reg);
  REGISTERWRITE (0x0C, 0x04);
  reg = registerRead (0x0A);
  if (reg != 0x00)
    {
      if (reg != 0x1C)
	{
	  DBG (0, "Warning! expected reg0A=0x00, found 0x%02X! (%s:%d) \n",
	       reg, __FILE__, __LINE__);
	}
      else
	{
	  DBG (16, "Scanner in idle state .... (%s:%d)\n", __FILE__,
	       __LINE__);
	}
    }

  /* model detection: redone since we might not be probing each time ... */
  /* write addr in 0x0E, read value at 0x0F                              */
  REGISTERWRITE (0x0E, 0x01);
  model = registerRead (0x0F);
  setModel (model);

  REGISTERWRITE (0x0A, 0x1C);
  if (gMode == UMAX_PP_PARPORT_ECP)
    {
      REGISTERWRITE (0x08, 0x10);
    }
  else
    {
      REGISTERWRITE (0x08, 0x21);
    }
  REGISTERWRITE (0x0E, 0x0F);
  REGISTERWRITE (0x0F, 0x0C);

  REGISTERWRITE (0x0A, 0x1C);
  REGISTERWRITE (0x0E, 0x10);
  REGISTERWRITE (0x0F, 0x1C);
  if (gMode == UMAX_PP_PARPORT_ECP)
    {
      REGISTERWRITE (0x0F, 0x00);
    }
  REGISTERWRITE (0x0A, 0x11);

  dest = (unsigned char *) (malloc (65536));
  if (dest == NULL)
    {
      DBG (0, "Failed to allocate 64 Ko !\n");
      return 0;
    }
  for (i = 0; i < 256; i++)
    {
      dest[i * 2] = i;
      dest[i * 2 + 1] = 0xFF - i;
      dest[512 + i * 2] = i;
      dest[512 + i * 2 + 1] = 0xFF - i;
    }
  nb = 150;
  for (i = 0; i < nb; i++)
    {
      bufferWrite (0x400, dest);
      DBG (16,
	   "Loop %d: bufferWrite(0x400,dest) passed... (%s:%d)\n", i,
	   __FILE__, __LINE__);
    }

  REGISTERWRITE (0x0A, 0x18);
  REGISTERWRITE (0x0A, 0x11);

  if (gMode == UMAX_PP_PARPORT_ECP)
    {
      ECPSetBuffer (0x400);
    }
  for (i = 0; i < nb; i++)
    {
      /* XXX Compat/Byte ??? XXX */
      bufferRead (0x400, dest);
      for (j = 0; j < 256; j++)
	{
	  if (dest[j * 2] != j)
	    {
	      DBG (0,
		   "Altered buffer value at %03X, expected %02X, found %02X\n",
		   j * 2, j, dest[j * 2]);
	      return 0;
	    }
	  if (dest[j * 2 + 1] != 0xFF - j)
	    {
	      DBG
		(0,
		 "Altered buffer value at %03X, expected %02X, found %02X\n",
		 j * 2 + 1, 0xFF - j, dest[j * 2 + 1]);
	      return 0;
	    }
	  if (dest[512 + j * 2] != j)
	    {
	      DBG (0,
		   "Altered buffer value at %03X, expected %02X, found %02X\n",
		   512 + j * 2, j, dest[512 + j * 2]);
	      return 0;
	    }
	  if (dest[512 + j * 2 + 1] != 0xFF - j)
	    {
	      DBG
		(0,
		 "Altered buffer value at %03X, expected 0x%02X, found 0x%02X\n",
		 512 + j * 2 + 1, 0xFF - j, dest[512 + j * 2 + 1]);
	      return 0;
	    }
	}
      DBG (16, "Loop %d: bufferRead(0x400,dest) passed... (%s:%d)\n",
	   i, __FILE__, __LINE__);
    }
  REGISTERWRITE (0x0A, 0x18);
  /* ECP: "HEAVY" reconnect here */
  if (gMode == UMAX_PP_PARPORT_ECP)
    {
      epilogue ();
      /* 3 line: set to initial parport state ? */
      byteMode ();		/*Outb (ECR, 0x20); */
      Outb (DATA, 0x04);
      Outb (CONTROL, 0x0C);

      /* the following is a variant of connect(); */
      Inb (ECR);
      Inb (ECR);
      byteMode ();		/*Outb (ECR, 0x20); */
      byteMode ();		/*Outb (ECR, 0x20); */
      Inb (CONTROL);
      Outb (CONTROL, 0x0C);
      Inb (DATA);
      sendCommand (0xE0);
      Outb (DATA, 0XFF);
      Outb (DATA, 0XFF);
      ClearRegister (0);
      WRITESLOW (0x0E, 0x0A);
      SLOWNIBBLEREGISTERREAD (0x0F, 0x08);
      /* resend value OR'ed 0x08 ? */
      WRITESLOW (0x0F, 0x08);
      WRITESLOW (0x08, 0x10);
      disconnect ();
      prologue (0x10);
    }

  if (fonc001 () != 1)
    {
      DBG (0, "fonc001() failed ! (%s:%d) \n", __FILE__, __LINE__);
      return 0;
    }
  DBG (16, "fonc001() passed ...  (%s:%d) \n", __FILE__, __LINE__);

  /* sync */
  if (sendWord (zero) == 0)
    {
      DBG (0, "sendWord(zero) failed (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }
  DBG (16, "sendWord(zero) passed (%s:%d)\n", __FILE__, __LINE__);
  epilogue ();

  /* OK ! */
  free (dest);
  DBG (1, "initTransport1220P done ...\n");
  return 1;
}

/* 
        1: OK
           2: failed, try again
           0: init failed 

        initialize the transport layer
   
   */

int
sanei_umax_pp_initTransport (int recover)
{
  TRACE (16, "sanei_umax_pp_initTransport");
  switch (sanei_umax_pp_getastra ())
    {
    case 610:
      return initTransport610p ();
    case 1220:
    case 1600:
    case 2000:
    default:
      return initTransport1220P (recover);
    }
}


/* 1: OK
   0: probe failed */

static int
probe610p (int recover)
{
  if (initTransport610p () == 0)
    {
      DBG (0, "initTransport610p() failed (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }

  /* make sure we won't try 1220/200P later 
   * since we got here, we have a 610, and in any case
   * NOT a 1220P/2000P, since no EPAT present */
  sanei_umax_pp_setastra (610);

  if (initScanner610p (recover) == 0)
    {
      DBG (0, "initScanner610p() failed (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }
  /* successfull end ... */
  DBG (1, "UMAX Astra 610p detected\n");
  DBG (1, "probe610p done ...\n");
  return 1;
}


  /* 
   * try PS2 mode
   * returns 1 on success, 0 on failure
   */
int
probePS2 (unsigned char *dest)
{
  int i, tmp;

  /* write/read full buffer */
  for (i = 0; i < 256; i++)
    {
      WRITESLOW (0x0A, i);
      SLOWNIBBLEREGISTERREAD (0x0A, i);
      WRITESLOW (0x0A, 0xFF - i);
      SLOWNIBBLEREGISTERREAD (0x0A, 0xFF - i);
    }

  /* end test for nibble byte/byte mode */

  /* now we try nibble buffered mode */
  WRITESLOW (0x13, 0x01);
  WRITESLOW (0x13, 0x00);	/*reset something */
  WRITESLOW (0x0A, 0x11);
  for (i = 0; i < 10; i++)	/* 10 ~ 11 ? */
    {
      PS2bufferRead (0x400, dest);
      DBG (16, "Loop %d: PS2bufferRead passed ... (%s:%d)\n", i, __FILE__,
	   __LINE__);
    }

  /* write buffer */
  for (i = 0; i < 10; i++)
    {
      PS2bufferWrite (0x400, dest);
      DBG (16, "Loop %d: PS2bufferWrite passed ... (%s:%d)\n", i, __FILE__,
	   __LINE__);
    }

  SLOWNIBBLEREGISTERREAD (0x0C, 0x04);
  WRITESLOW (0x13, 0x01);
  WRITESLOW (0x13, 0x00);
  WRITESLOW (0x0A, 0x18);

  return 1;
}

  /*
   * try EPP 8 then 32 bits
   * returns 1 on success, 0 on failure
   */
int
probeEPP (unsigned char *dest)
{
  int tmp, i, j;
  int reg;

  /* test EPP MODE */
  setEPPMode (8);
  gMode = UMAX_PP_PARPORT_EPP;
  ClearRegister (0);
  DBG (16, "ClearRegister(0) passed... (%s:%d)\n", __FILE__, __LINE__);
  WRITESLOW (0x08, 0x22);
  init001 ();
  DBG (16, "init001() passed... (%s:%d)\n", __FILE__, __LINE__);
  gEPAT = 0xC7;
  init002 (0);
  DBG (16, "init002(0) passed... (%s:%d)\n", __FILE__, __LINE__);

  REGISTERWRITE (0x0A, 0);

  /* catch any failure to read back data in EPP mode */
  reg = registerRead (0x0A);
  if (reg != 0)
    {
      DBG (0, "registerRead, found 0x%X expected 0x00 (%s:%d)\n", reg,
	   __FILE__, __LINE__);
      if (reg == 0xFF)
	{
	  DBG (0,
	       "*** It appears that EPP data transfer doesn't work    ***\n");
	  DBG (0,
	       "*** Please read SETTING EPP section in sane-umax_pp.5 ***\n");
	}
      return 0;
    }
  else
    {
      DBG (16, "registerRead(0x0A)=0x00 passed... (%s:%d)\n", __FILE__,
	   __LINE__);
    }
  registerWrite (0x0A, 0xFF);
  DBG (16, "registerWrite(0x%X,0x%X) passed...   (%s:%d)\n", 0x0A, 0xFF,
       __FILE__, __LINE__);
  REGISTERREAD (0x0A, 0xFF);
  for (i = 1; i < 256; i++)
    {
      REGISTERWRITE (0x0A, i);
      REGISTERREAD (0x0A, i);
      REGISTERWRITE (0x0A, 0xFF - i);
      REGISTERREAD (0x0A, 0xFF - i);
    }

  REGISTERWRITE (0x13, 0x01);
  REGISTERWRITE (0x13, 0x00);
  REGISTERWRITE (0x0A, 0x11);

  for (i = 0; i < 10; i++)
    {
      bufferRead (0x400, dest);
      for (j = 0; j < 512; j++)
	{
	  if (dest[2 * j] != (j % 256))
	    {
	      DBG (0, "Loop %d, char %d bufferRead failed! (%s:%d)\n", i,
		   j * 2, __FILE__, __LINE__);
	      return 0;
	    }
	  if (dest[2 * j + 1] != (0xFF - (j % 256)))
	    {
	      DBG (0, "Loop %d, char %d bufferRead failed! (%s:%d)\n", i,
		   j * 2 + 1, __FILE__, __LINE__);
	      return 0;
	    }
	}
      DBG (16, "Loop %d: bufferRead(0x400,dest) passed... (%s:%d)\n", i,
	   __FILE__, __LINE__);
    }

  for (i = 0; i < 10; i++)
    {
      bufferWrite (0x400, dest);
      DBG (16, "Loop %d: bufferWrite(0x400,dest) passed... (%s:%d)\n", i,
	   __FILE__, __LINE__);
    }


  REGISTERREAD (0x0C, 4);
  REGISTERWRITE (0x13, 0x01);
  REGISTERWRITE (0x13, 0x00);
  REGISTERWRITE (0x0A, 0x18);

  Outb (DATA, 0x0);
  ClearRegister (0);
  init001 ();

  if (checkEPAT () != 0)
    return 0;
  DBG (16, "checkEPAT() passed... (%s:%d)\n", __FILE__, __LINE__);

  tmp = Inb (CONTROL) & 0x1F;
  Outb (CONTROL, tmp);
  Outb (CONTROL, tmp);

  WRITESLOW (0x08, 0x21);
  init001 ();
  DBG (16, "init001() passed... (%s:%d)\n", __FILE__, __LINE__);
  WRITESLOW (0x08, 0x21);
  init001 ();
  DBG (16, "init001() passed... (%s:%d)\n", __FILE__, __LINE__);
  SPPResetLPT ();


  if (init005 (0x80))
    {
      DBG (0, "init005(0x80) failed... (%s:%d)\n", __FILE__, __LINE__);
    }
  DBG (16, "init005(0x80) passed... (%s:%d)\n", __FILE__, __LINE__);
  if (init005 (0xEC))
    {
      DBG (0, "init005(0xEC) failed... (%s:%d)\n", __FILE__, __LINE__);
    }
  DBG (16, "init005(0xEC) passed... (%s:%d)\n", __FILE__, __LINE__);


  /* write/read buffer loop */
  for (i = 0; i < 256; i++)
    {
      REGISTERWRITE (0x0A, i);
      REGISTERREAD (0x0A, i);
      REGISTERWRITE (0x0A, 0xFF - i);
      REGISTERREAD (0x0A, 0xFF - i);
    }
  DBG (16, "EPP write/read buffer loop passed... (%s:%d)\n", __FILE__,
       __LINE__);

  REGISTERWRITE (0x13, 0x01);
  REGISTERWRITE (0x13, 0x00);
  REGISTERWRITE (0x0A, 0x11);

  /* test EPP32 mode */
  /* we set 32 bits I/O mode first, then step back to */
  /* 8bits if tests fail                              */
  setEPPMode (32);
  for (i = 0; (i < 10) && (getEPPMode () == 32); i++)
    {
      bufferRead (0x400, dest);
      /* if 32 bit I/O work, we should have a buffer */
      /* filled by 00 FF 01 FE 02 FD 03 FC .....     */
      for (j = 0; j < 0x200; j++)
	{
	  if ((dest[j * 2] != j % 256)
	      || (dest[j * 2 + 1] != 0xFF - (j % 256)))
	    {
	      DBG (1, "Setting EPP I/O to 8 bits ... (%s:%d)\n", __FILE__,
		   __LINE__);
	      setEPPMode (8);
	      /* leave out current loop since an error was detected */
	      break;
	    }
	}
      DBG (16, "Loop %d: bufferRead(0x400) passed... (%s:%d)\n", i,
	   __FILE__, __LINE__);
    }
  DBG (1, "%d bits EPP data transfer\n", getEPPMode ());


  for (i = 0; i < 10; i++)
    {
      bufferWrite (0x400, dest);
      DBG (16, "Loop %d: bufferWrite(0x400,dest) passed... (%s:%d)\n", i,
	   __FILE__, __LINE__);
    }



  REGISTERREAD (0x0C, 0x04);
  REGISTERWRITE (0x13, 0x01);
  REGISTERWRITE (0x13, 0x00);
  REGISTERWRITE (0x0A, 0x18);

  WRITESLOW (0x08, 0x21);
  init001 ();
  DBG (16, "init001() passed... (%s:%d)\n", __FILE__, __LINE__);
  SPPResetLPT ();

  if (init005 (0x80))
    {
      DBG (0, "init005(0x80) failed... (%s:%d)\n", __FILE__, __LINE__);
    }
  DBG (16, "init005(0x80) passed... (%s:%d)\n", __FILE__, __LINE__);
  if (init005 (0xEC))
    {
      DBG (0, "init005(0xEC) failed... (%s:%d)\n", __FILE__, __LINE__);
    }
  DBG (16, "init005(0xEC) passed... (%s:%d)\n", __FILE__, __LINE__);


  /* write/read buffer loop */
  for (i = 0; i < 256; i++)
    {
      REGISTERWRITE (0x0A, i);
      REGISTERREAD (0x0A, i);
      REGISTERWRITE (0x0A, 0xFF - i);
      REGISTERREAD (0x0A, 0xFF - i);
    }
  DBG (16, "EPP write/read buffer loop passed... (%s:%d)\n", __FILE__,
       __LINE__);

  REGISTERWRITE (0x13, 0x01);
  REGISTERWRITE (0x13, 0x00);
  REGISTERWRITE (0x0A, 0x11);


  for (i = 0; i < 10; i++)
    {
      bufferRead (0x400, dest);
      DBG (16, "Loop %d: bufferRead(0x400) passed... (%s:%d)\n", i,
	   __FILE__, __LINE__);
    }

  for (i = 0; i < 10; i++)
    {
      bufferWrite (0x400, dest);
      DBG (16, "Loop %d: bufferWrite(0x400,dest) passed... (%s:%d)\n", i,
	   __FILE__, __LINE__);
    }
  REGISTERREAD (0x0C, 0x04);
  REGISTERWRITE (0x13, 0x01);
  REGISTERWRITE (0x13, 0x00);
  REGISTERWRITE (0x0A, 0x18);
  gMode = UMAX_PP_PARPORT_EPP;
  return 1;
}

  /*
   * try ECP mode
   * returns 1 on success, 0 on failure
   */
int
probeECP (unsigned char *dest)
{
  int i, j, tmp;
  unsigned char breg;

  /* if ECP not available, fail */
  if (gECP != 1)
    {
      DBG (1, "Hardware can't do ECP, giving up (%s:%d) ...\n", __FILE__,
	   __LINE__);
      return 0;
    }
  gMode = UMAX_PP_PARPORT_ECP;

/* clean from EPP failure */
  breg = Inb (CONTROL);
  Outb (CONTROL, breg & 0x04);

/* reset sequence */
  byteMode ();			/*Outb (ECR, 0x20);            byte mode */
  Outb (CONTROL, 0x04);
  Outb (CONTROL, 0x0C);
  Outb (CONTROL, 0x0C);
  Outb (CONTROL, 0x0C);
  Outb (CONTROL, 0x0C);
  for (i = 0; i < 256; i++)
    {
      breg = (Inb (STATUS)) & 0xF8;
      if (breg != 0x48)
	{
	  DBG (0,
	       "probeECP() failed at sync step %d, status=0x%02X, expected 0x48 (%s:%d)\n",
	       i, breg, __FILE__, __LINE__);
	  return 0;
	}
    }
  Outb (CONTROL, 0x0E);
  Outb (CONTROL, 0x0E);
  Outb (CONTROL, 0x0E);
  breg = (Inb (STATUS)) & 0xF8;
  if (breg != 0x48)
    {
      DBG (0, "probeECP() failed, status=0x%02X, expected 0x48 (%s:%d)\n",
	   breg, __FILE__, __LINE__);
      return 0;
    }
  Outb (CONTROL, 0x04);
  Outb (CONTROL, 0x04);
  Outb (CONTROL, 0x04);
  breg = Inb (STATUS) & 0xF8;
  breg = (Inb (STATUS)) & 0xF8;
  if (breg != 0xC8)
    {
      DBG (0, "probeECP() failed, status=0x%02X, expected 0xC8 (%s:%d)\n",
	   breg, __FILE__, __LINE__);
      return 0;
    }
/* end of reset sequence */

  Outb (DATA, 0x00);
  ClearRegister (0);

/* utile ? semble tester le registre de configuration
 * inb ECR,35
 * inb 77B,FF
 * inb ECR,35
 */
/* routine A */
  breg = Inb (CONTROL);		/* 0x04 évidemment! */
  breg = Inb (ECR);
  breg = Inb (ECR);
  breg = Inb (ECR);
  breg = Inb (ECR);
  breg = Inb (CONTROL);
  byteMode ();			/*Outb (ECR, 0x20);            byte mode */
  /*byteMode ();                        Outb (ECR, 0x20); */
  breg = Inb (CONTROL);
  Outb (CONTROL, 0x04);
  Outb (CONTROL, 0x04);
  breg = Inb (ECR);		/* 35 expected */
  breg = Inb (ECR);		/* 35 expected */
  breg = Inb (ECR);		/* 35 expected */
  breg = Inb (ECR);		/* 35 expected */
  Outb (CONTROL, 0x04);
  Outb (CONTROL, 0x04);
  byteMode ();
  byteMode ();

  ClearRegister (0);

/* routine C */
  PS2registerWrite (0x08, 0x01);

  Outb (CONTROL, 0x0C);
  Outb (CONTROL, 0x04);

  ClearRegister (0);

  breg = PS2Something (0x10);
  if (breg != 0x0B)
    {
      DBG (0, "probeECP() failed, reg10=0x%02X, expected 0x0B (%s:%d)\n",
	   breg, __FILE__, __LINE__);
      /* return 0; */
    }


  for (i = 0; i < 256; i++)
    {
      ECPregisterWrite (0x0A, i);
      breg = ECPregisterRead (0x0A);
      if (breg != i)
	{
	  DBG (0, "probeECP(), loop %d failed (%s:%d)\n", i, __FILE__,
	       __LINE__);
	  return 0;
	}
      ECPregisterWrite (0x0A, 0xFF - i);
      breg = ECPregisterRead (0x0A);
      if (breg != 0xFF - i)
	{
	  DBG (0, "probeECP(), loop %d failed (%s:%d)\n", i, __FILE__,
	       __LINE__);
	  return 0;
	}
    }
  DBG (16, "probeECP(), loop passed (%s:%d)\n", __FILE__, __LINE__);

  ECPregisterWrite (0x13, 0x01);
  ECPregisterWrite (0x13, 0x00);
  ECPregisterWrite (0x0A, 0x11);

  /* there is one buffer transfer size set up */
  /* subsequent reads are done in a row       */
  ECPSetBuffer (0x400);
  for (i = 0; i < 10; i++)
    {
      /* if (i > 0) */
      compatMode ();
      Outb (CONTROL, 0x04);	/* reset ? */

      ECPbufferRead (1024, dest);
      /* check content of the returned buffer */
      for (j = 0; j < 256; j++)
	{
	  if (dest[j * 2] != j)
	    {
	      DBG (0,
		   "Altered buffer value at %03X, expected %02X, found %02X\n",
		   j * 2, j, dest[j * 2]);
	      return 0;
	    }
	  if (dest[j * 2 + 1] != 0xFF - j)
	    {
	      DBG
		(0,
		 "Altered buffer value at %03X, expected %02X, found %02X\n",
		 j * 2 + 1, 0xFF - j, dest[j * 2 + 1]);
	      return 0;
	    }
	  if (dest[512 + j * 2] != j)
	    {
	      DBG (0,
		   "Altered buffer value at %03X, expected %02X, found %02X\n",
		   512 + j * 2, j, dest[512 + j * 2]);
	      return 0;
	    }
	  if (dest[512 + j * 2 + 1] != 0xFF - j)
	    {
	      DBG
		(0,
		 "Altered buffer value at %03X, expected 0x%02X, found 0x%02X\n",
		 512 + j * 2 + 1, 0xFF - j, dest[512 + j * 2 + 1]);
	      return 0;
	    }
	}
      Outb (CONTROL, 0x04);
      byteMode ();
    }

  for (i = 0; i < 10; i++)
    ECPbufferWrite (1024, dest);

  breg = ECPregisterRead (0x0C);
  if (breg != 0x04)
    {
      DBG (0, "Warning! expected reg0C=0x04, found 0x%02X! (%s:%d) \n", breg,
	   __FILE__, __LINE__);
    }

  ECPregisterWrite (0x13, 0x01);
  ECPregisterWrite (0x13, 0x00);
  ECPregisterWrite (0x0A, 0x18);

  /* reset printer ? */
  Outb (DATA, 0x00);
  Outb (CONTROL, 0x00);
  Outb (CONTROL, 0x04);

  for (i = 0; i < 3; i++)
    {				/* will go in a function */
      ClearRegister (0);
      if (waitAck () != 1)
	{
	  DBG (0, "probeECP failed because of waitAck() (%s:%d) \n", __FILE__,
	       __LINE__);
	  /* return 0; may fail without harm ... ??? */
	}
      /* are these 2 out really needed ? */
      PS2registerWrite (0x08, 0x01);
      Outb (CONTROL, 0x0C);	/* select + reset */
      Outb (CONTROL, 0x04);	/* reset */
    }

  /* prologue of the 'rotate test' */
  ClearRegister (0);
  breg = PS2Something (0x10);
  if (breg != 0x0B)
    {
      DBG (0,
	   "PS2Something returned 0x%02X, 0x0B expected (%s:%d)\n", breg,
	   __FILE__, __LINE__);
    }
  Outb (CONTROL, 0x04);		/* reset */

  if (init005 (0x80))
    {
      DBG (0, "init005(0x80) failed... (%s:%d)\n", __FILE__, __LINE__);
    }
  DBG (16, "init005(0x80) passed... (%s:%d)\n", __FILE__, __LINE__);
  if (init005 (0xEC))
    {
      DBG (0, "init005(0xEC) failed... (%s:%d)\n", __FILE__, __LINE__);
    }
  DBG (16, "init005(0xEC) passed... (%s:%d)\n", __FILE__, __LINE__);

  for (i = 0; i < 256; i++)
    {
      REGISTERWRITE (0x0A, i);
      REGISTERREAD (0x0A, i);
      REGISTERWRITE (0x0A, 0xFF - i);
      REGISTERREAD (0x0A, 0xFF - i);
    }
  DBG (16, "ECPprobe(), write/read buffer loop passed (%s:%d)\n", __FILE__,
       __LINE__);

  REGISTERWRITE (0x13, 0x01);
  REGISTERWRITE (0x13, 0x00);
  REGISTERWRITE (0x0A, 0x11);

  /* should be a function */
  /* in probeEPP(), we begin 32 bit mode test here */
  for (i = 0; i < 10; i++)
    {
      compatMode ();
      Outb (CONTROL, 0x04);	/* reset ? */

      ECPbufferRead (0x400, dest);
      /* check content of the returned buffer */
      for (j = 0; j < 256; j++)
	{
	  if (dest[j * 2] != j)
	    {
	      DBG (0,
		   "Altered buffer value at %03X, expected %02X, found %02X\n",
		   j * 2, j, dest[j * 2]);
	      return 0;
	    }
	  if (dest[j * 2 + 1] != 0xFF - j)
	    {
	      DBG
		(0,
		 "Altered buffer value at %03X, expected %02X, found %02X\n",
		 j * 2 + 1, 0xFF - j, dest[j * 2 + 1]);
	      return 0;
	    }
	  if (dest[512 + j * 2] != j)
	    {
	      DBG (0,
		   "Altered buffer value at %03X, expected %02X, found %02X\n",
		   512 + j * 2, j, dest[512 + j * 2]);
	      return 0;
	    }
	  if (dest[512 + j * 2 + 1] != 0xFF - j)
	    {
	      DBG
		(0,
		 "Altered buffer value at %03X, expected 0x%02X, found 0x%02X\n",
		 512 + j * 2 + 1, 0xFF - j, dest[512 + j * 2 + 1]);
	      return 0;
	    }
	}
      Outb (CONTROL, 0x04);
      byteMode ();
    }

  for (i = 0; i < 10; i++)
    ECPbufferWrite (1024, dest);

  REGISTERREAD (0x0C, 0x04);
  REGISTERWRITE (0x13, 0x01);
  REGISTERWRITE (0x13, 0x00);
  REGISTERWRITE (0x0A, 0x18);
  waitAck ();

  return 1;
}



/* 1: OK
   0: probe failed */

int
sanei_umax_pp_probeScanner (int recover)
{
  int tmp, i, j;
  int reg, nb;
  unsigned char *dest = NULL;
  int initbuf[2049];
  int voidbuf[2049];
  int val;
  int zero[5] = { 0, 0, 0, 0, -1 };
  int model;

  /* saves port state */
  gData = Inb (DATA);
  gControl = Inb (CONTROL);

  if (sanei_umax_pp_getastra () == 610)
    return probe610p (recover);

  /* save and set CONTROL */
  tmp = (Inb (CONTROL)) & 0x1F;
  tmp = (Inb (CONTROL)) & 0x1F;
  Outb (CONTROL, tmp);
  Outb (CONTROL, tmp);

  tmp = Inb (DATA);
  tmp = Inb (CONTROL) & 0x3F;
  Outb (CONTROL, tmp);

  tmp = Inb (CONTROL) & 0x3F;
  tmp = Inb (DATA);
  tmp = Inb (CONTROL) & 0x3F;
  tmp = Inb (DATA);

  /*  any scanner ? */
  /* fast detect */
  tmp = ringScanner (2, 0);
  if (!tmp)
    {
      DBG (1, "No scanner detected by 'ringScanner(2,0)'...\n");
      tmp = ringScanner (5, 0);
      if (!tmp)
	{
	  DBG (1, "No scanner detected by 'ringScanner(5,0)'...\n");
	  tmp = ringScanner (5, 10000);
	  if (!tmp)
	    {
	      DBG (1, "No scanner detected by 'ringScanner(5,10000)'...\n");
	      tmp = ringScanner (5, 10000);
	      if (!tmp)
		{
		  DBG (1,
		       "No scanner detected by 'ringScanner(5,10000)'...\n");
		}
	    }
	}
    }
  if (!tmp)
    {
      DBG (1, "No 1220P/2000P scanner detected by 'ringScanner()'...\n");
    }
  DBG (16, "ringScanner passed...\n");

  gControl = Inb (CONTROL) & 0x3F;
  g67D = 1;
  if (sendCommand (0x30) == 0)
    {
      DBG (0, "sendCommand(0x30) (%s:%d) failed ...\n", __FILE__, __LINE__);
      return 0;
    }
  DBG (16, "sendCommand(0x30) passed ... (%s:%d)\n", __FILE__, __LINE__);
  g67E = 4;			/* bytes to read */
  if (sendCommand (0x00) == 0)
    {
      DBG (0, "sendCommand(0x00) (%s:%d) failed ...\n", __FILE__, __LINE__);
      return 0;
    }
  DBG (16, "sendCommand(0x00) passed... (%s:%d)\n", __FILE__, __LINE__);
  g67E = 0;			/* bytes to read */
  if (testVersion (0) == 0)
    {
      DBG (16, "testVersion(0) (%s:%d) failed ...\n", __FILE__, __LINE__);
    }
  DBG (16, "testVersion(0) passed...\n");
  /* must fail for 1220P and 2000P */
  if (testVersion (1) == 0)	/* software doesn't do it for model 0x07 */
    {				/* but it works ..                       */
      DBG (16, "testVersion(1) failed (expected) ... (%s:%d)\n", __FILE__,
	   __LINE__);
    }
  else
    {
      DBG (16, "Unexpected success on testVersion(1) ... (%s:%d)\n", __FILE__,
	   __LINE__);
    }
  if (testVersion (0) == 0)
    {
      DBG (16, "testVersion(0) (%s:%d) failed ...\n", __FILE__, __LINE__);
    }
  DBG (16, "testVersion(0) passed...\n");
  /* must fail */
  if (testVersion (1) == 0)
    {
      DBG (16, "testVersion(1) failed (expected) ... (%s:%d)\n", __FILE__,
	   __LINE__);
    }
  else
    {
      DBG (16, "Unexpected success on testVersion(1) ... (%s:%d)\n", __FILE__,
	   __LINE__);
    }

  Outb (DATA, 0x04);
  Outb (CONTROL, 0x0C);
  Outb (DATA, 0x04);
  Outb (CONTROL, 0x0C);

  gControl = Inb (CONTROL) & 0x3F;
  Outb (CONTROL, gControl & 0xEF);



  if (sendCommand (0x40) == 0)
    {
      DBG (0, "sendCommand(0x40) (%s:%d) failed ...\n", __FILE__, __LINE__);
      return 0;
    }
  DBG (16, "sendCommand(0x40) passed...\n");
  if (sendCommand (0xE0) == 0)
    {
      DBG (0, "sendCommand(0xE0) (%s:%d) failed ...\n", __FILE__, __LINE__);
      return 0;
    }
  DBG (16, "sendCommand(0xE0) passed...\n");

  ClearRegister (0);
  DBG (16, "ClearRegister(0) passed...\n");

  SPPResetLPT ();
  DBG (16, "SPPResetLPT() passed...\n");

  Outb (CONTROL, 4);
  Outb (CONTROL, 4);


  /* test PS2 mode */

  tmp = PS2registerRead (0x0B);
  if (tmp == 0xC7)
    {
      /* epat C7 detected */
      DBG (16, "PS2registerRead(0x0B)=0x%X passed...\n", tmp);

      PS2registerWrite (8, 0);
      DBG (16, "PS2registerWrite(8,0) passed...   (%s:%d)\n", __FILE__,
	   __LINE__);

      tmp = PS2registerRead (0x0A);
      if (tmp != 0x00)
	{
	  if (tmp == 0x1C)
	    {
	      DBG (16, "Previous probe detected ... (%s:%d)\n", __FILE__,
		   __LINE__);
	    }
	  else
	    {
	      DBG (0, "Found 0x%X expected 0x00  (%s:%d)\n", tmp, __FILE__,
		   __LINE__);
	    }
	}
      DBG (16, "PS2registerRead(0x0A)=0x%X passed ...(%s:%d)\n", tmp,
	   __FILE__, __LINE__);

    }
  else
    {
      DBG (4, "Found 0x%X expected 0xC7 (%s:%d)\n", tmp, __FILE__, __LINE__);
      if ((tmp == 0xFF) && (sanei_umax_pp_getparport () < 1))
	{
	  DBG (0,
	       "It is likely that the hardware address (0x%X) you specified is wrong\n",
	       gPort);
	  return 0;
	}
      /* probe for a 610p, since we can't detect an EPAT */
      DBG (1, "Trying 610p (%s:%d)\n", __FILE__, __LINE__);
      return probe610p (recover);
    }

  /* clear register 3 */
  ClearRegister (3);
  DBG (16, "ClearRegister(3) passed...\n");

  /* wait ? */
  i = 65535;
  while (i > 0)
    {
      tmp = Inb (DATA);
      tmp = Inb (DATA);
      i--;
    }
  DBG (16, "FFFF in loop passed...\n");

  ClearRegister (0);
  DBG (16, "ClearRegister(0) passed... (%s:%d)\n", __FILE__, __LINE__);
  fflush (stdout);

  /* 1220/2000P branch */
  WRITESLOW (0x0E, 1);

  /* register 0x0F used only once: model number ? Or ASIC revision ? */
  /* comm mode ?                                                     */
  model = PS2registerRead (0x0F);
  DBG (1, "UMAX Astra 1220/1600/2000 P ASIC detected (mode=%d)\n", model);
  setModel (model);
  DBG (16, "PS2registerRead(0x0F) passed... (%s:%d)\n", __FILE__, __LINE__);

  /* scanner powered off */
  if (model == 0x1B)
    {
      DBG (0, "Register 0x0F=0x1B, scanner powered off ! (%s:%d)\n", __FILE__,
	   __LINE__);
      return 0;
    }


  if ((model != 0x1F) && (model != 0x07))
    {
      DBG
	(0,
	 "Unexpected value for for register 0x0F, expected 0x07 or 0x1F, got 0x%02X ! (%s:%d)\n",
	 model, __FILE__, __LINE__);
      DBG (0, "There is a new scanner revision in town, or a bug ....\n");
    }

  WRITESLOW (0x08, 0x02);
  WRITESLOW (0x0E, 0x0F);
  WRITESLOW (0x0F, 0x0C);
  WRITESLOW (0x0C, 0x04);
  tmp = PS2registerRead (0x0D);
  if ((tmp != 0x00) && (tmp != 0x40))
    {
      DBG
	(0,
	 "Unexpected value for for register 0x0D, expected 0x00 or 0x40, got 0x%02X ! (%s:%d)\n",
	 tmp, __FILE__, __LINE__);
    }
  WRITESLOW (0x0D, 0x1B);
  switch (model)
    {
    case 0x1F:
      WRITESLOW (0x12, 0x14);
      SLOWNIBBLEREGISTERREAD (0x12, 0x10);
      break;
    case 0x07:
      WRITESLOW (0x12, 0x00);
      SLOWNIBBLEREGISTERREAD (0x12, 0x00);
      /* we may get 0x20, in this case some color aberration may occur */
      /* must depend on the parport */
      /* model 0x07 + 0x00=>0x20=2000P */
      break;
    default:
      WRITESLOW (0x12, 0x00);
      SLOWNIBBLEREGISTERREAD (0x12, 0x20);
      break;
    }
  SLOWNIBBLEREGISTERREAD (0x0D, 0x18);
  SLOWNIBBLEREGISTERREAD (0x0C, 0x04);
  SLOWNIBBLEREGISTERREAD (0x0A, 0x00);
  WRITESLOW (0x0E, 0x0A);
  WRITESLOW (0x0F, 0x00);
  WRITESLOW (0x0E, 0x0D);
  WRITESLOW (0x0F, 0x00);
  dest = (unsigned char *) malloc (65536);
  if (dest == NULL)
    {
      DBG (0, "Failed to allocate 64K (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }

  gMode = UMAX_PP_PARPORT_PS2;
  if (probePS2 (dest))
    {				/* PS2 mode works */
      DBG (16, "probePS2 passed ... (%s:%d)\n", __FILE__, __LINE__);
      gprobed = UMAX_PP_PARPORT_PS2;
    }

  Outb (CONTROL, 4);
  SLOWNIBBLEREGISTERREAD (0x0A, 0x18);
  WRITESLOW (0x08, 0x40);
  WRITESLOW (0x08, 0x60);
  WRITESLOW (0x08, 0x22);

  gMode = UMAX_PP_PARPORT_EPP;
  if (probeEPP (dest))
    {				/* EPP mode works */
      gprobed = UMAX_PP_PARPORT_EPP;
      gMode = UMAX_PP_PARPORT_EPP;
      DBG (16, "probeEPP passed ... (%s:%d)\n", __FILE__, __LINE__);
    }
  else
    {				/* EPP fails, try ECP */
      DBG (16, "probeEPP failed ... (%s:%d)\n", __FILE__, __LINE__);
      gMode = UMAX_PP_PARPORT_ECP;
      if (probeECP (dest))
	{			/* ECP mode works */
	  DBG (16, "probeECP passed ... (%s:%d)\n", __FILE__, __LINE__);
	  gprobed = UMAX_PP_PARPORT_ECP;
	}
      else
	{			/* ECP and EPP fail, give up */
	  /* PS2 could be used */
	  DBG (16, "probeECP failed ... (%s:%d)\n", __FILE__, __LINE__);
	  DBG (1, "No EPP or ECP mode working, giving up ... (%s:%d)\n",
	       __FILE__, __LINE__);
	  free (dest);
	  return 0;
	}
    }

  /* some operations here may have to go into probeEPP/probeECP */
  g6FE = 1;
  if (gMode == UMAX_PP_PARPORT_ECP)
    {
      WRITESLOW (0x08, 0x01);
      Outb (CONTROL, 0x0C);
      Outb (CONTROL, 0x04);
      ClearRegister (0);
      tmp = PS2Something (0x10);
      if (tmp != 0x0B)
	{
	  DBG (0,
	       "PS2Something returned 0x%02X, 0x0B expected (%s:%d)\n", tmp,
	       __FILE__, __LINE__);
	}
    }
  else
    {
      WRITESLOW (0x08, 0x21);
      init001 ();
      DBG (16, "init001() passed... (%s:%d)\n", __FILE__, __LINE__);
    }


  reg = registerRead (0x0D);
  reg = (reg & 0xE8) | 0x43;
  registerWrite (0x0D, reg);

  REGISTERWRITE (0x0A, 0x18);
  REGISTERWRITE (0x0E, 0x0F);
  REGISTERWRITE (0x0F, 0x0C);

  REGISTERWRITE (0x0A, 0x1C);
  REGISTERWRITE (0x0E, 0x10);
  REGISTERWRITE (0x0F, 0x1C);


  reg = registerRead (0x0D);	/* 0x48 expected */
  reg = registerRead (0x0D);
  reg = registerRead (0x0D);
  reg = (reg & 0xB7) | 0x03;
  registerWrite (0x0D, reg);
  DBG (16, "(%s:%d) passed \n", __FILE__, __LINE__);

  reg = registerRead (0x12);	/* 0x10 for model 0x0F, 0x20 for model 0x07 */
  /* 0x00 when in ECP mode ... */
  reg = reg & 0xEF;
  registerWrite (0x12, reg);
  DBG (16, "(%s:%d) passed \n", __FILE__, __LINE__);

  reg = registerRead (0x0A);
  if (reg != 0x1C)
    {
      DBG (0, "Warning! expected reg0A=0x1C, found 0x%02X! (%s:%d) \n", reg,
	   __FILE__, __LINE__);
    }
  DBG (16, "(%s:%d) passed \n", __FILE__, __LINE__);

  /*Inb(CONTROL);       ECP 0x04 expected */
  disconnect ();
  DBG (16, "disconnect() passed... (%s:%d)\n", __FILE__, __LINE__);
  connect ();
  DBG (16, "connect() passed... (%s:%d)\n", __FILE__, __LINE__);


  /* some sort of countdown, some warming-up ? */
  /* maybe some data sent to the stepper motor */
  /* if (model == 0x07) */
  {
    /* REGISTERWRITE (0x0A, 0x00);
       reg = registerRead (0x0D);
       reg = (reg & 0xE8);
       registerWrite (0x0D, reg);
       DBG (16, "(%s:%d) passed \n", __FILE__, __LINE__); */
    epilogue ();
    prologue (0x10);

    reg = registerRead (0x13);
    if (reg != 0x00)
      {
	DBG (0, "Warning! expected reg13=0x00, found 0x%02X! (%s:%d) \n",
	     reg, __FILE__, __LINE__);
      }
    REGISTERWRITE (0x13, 0x81);
    usleep (10000);
    REGISTERWRITE (0x13, 0x80);
    /* could it be step-motor values ? */
    REGISTERWRITE (0x0E, 0x04);	/* FF->R04 */
    REGISTERWRITE (0x0F, 0xFF);
    REGISTERWRITE (0x0E, 0x05);	/* 03->R05 */
    REGISTERWRITE (0x0F, 0x03);
    REGISTERWRITE (0x10, 0x66);
    usleep (10000);

    REGISTERWRITE (0x0E, 0x04);	/* FF->R04 */
    REGISTERWRITE (0x0F, 0xFF);
    REGISTERWRITE (0x0E, 0x05);	/* 01 ->R05 */
    REGISTERWRITE (0x0F, 0x01);
    REGISTERWRITE (0x10, 0x55);
    usleep (10000);

    REGISTERWRITE (0x0E, 0x04);	/* FF -> R04 */
    REGISTERWRITE (0x0F, 0xFF);
    REGISTERWRITE (0x0E, 0x05);	/* 00 -> R05 */
    REGISTERWRITE (0x0F, 0x00);
    REGISTERWRITE (0x10, 0x44);
    usleep (10000);

    REGISTERWRITE (0x0E, 0x04);	/* 7F -> R04 */
    REGISTERWRITE (0x0F, 0x7F);
    REGISTERWRITE (0x0E, 0x05);	/* 00 -> R05 */
    REGISTERWRITE (0x0F, 0x00);
    REGISTERWRITE (0x10, 0x33);
    usleep (10000);

    REGISTERWRITE (0x0E, 0x04);	/* 3F -> R04 */
    REGISTERWRITE (0x0F, 0x3F);
    REGISTERWRITE (0x0E, 0x05);
    REGISTERWRITE (0x0F, 0x00);	/* 00 -> R05 */
    REGISTERWRITE (0x10, 0x22);
    usleep (10000);

    REGISTERWRITE (0x0E, 0x04);
    REGISTERWRITE (0x0F, 0x00);
    REGISTERWRITE (0x0E, 0x05);
    REGISTERWRITE (0x0F, 0x00);
    REGISTERWRITE (0x10, 0x11);
    usleep (10000);

    REGISTERWRITE (0x13, 0x81);
    usleep (10000);
    REGISTERWRITE (0x13, 0x80);

    REGISTERWRITE (0x0E, 0x04);
    REGISTERWRITE (0x0F, 0x00);
    REGISTERWRITE (0x0E, 0x05);
    REGISTERWRITE (0x0F, 0x00);
    usleep (10000);

    reg = registerRead (0x10);
    DBG (1, "Count-down value is 0x%02X  (%s:%d)\n", reg, __FILE__, __LINE__);
    /* 2 reports of CF, was FF first (typo ?) */
    /* CF seems a valid value                 */
    /* in case of CF, we may have timeout ... */
    /*if (reg != 0x00)
       {
       DBG (0, "Warning! expected reg10=0x00, found 0x%02X! (%s:%d) \n",
       reg, __FILE__, __LINE__);
       } */
    REGISTERWRITE (0x13, 0x00);
  }
/* end of countdown */
  DBG (16, "(%s:%d) passed \n", __FILE__, __LINE__);
  /* *NOT* epilogue(); (when EPP) */
  /*REGISTERWRITE (0x0A, 0x00);
     REGISTERREAD (0x0D, 0x40);
     REGISTERWRITE (0x0D, 0x00); */
  epilogue ();
  prologue (0x10);
  REGISTERWRITE (0x0E, 0x0D);
  REGISTERWRITE (0x0F, 0x00);
  REGISTERWRITE (0x0A, 0x1C);

  /* real start of high level protocol ? */
  if (fonc001 () != 1)
    {
      DBG (0, "fonc001() failed ! (%s:%d) \n", __FILE__, __LINE__);
      return 0;
    }
  DBG (16, "fonc001() passed (%s:%d) \n", __FILE__, __LINE__);
  reg = registerRead (0x19) & 0xC8;
  /* if reg=E8 or D8 , we have a 'messed' scanner */

  /* 4 tranform buffers + 'void' are sent: 1 B&W, and 3 RGB ? */
  memset (initbuf, 0x00, 2048 * sizeof (int));
  memset (voidbuf, 0x00, 2048 * sizeof (int));

  initbuf[512] = 0xFF;
  initbuf[513] = 0xAA;
  initbuf[514] = 0x55;

  for (j = 0; j < 4; j++)
    {
      for (i = 0; i < 256; i++)
	{
	  voidbuf[512 * j + 2 * i] = i;
	  voidbuf[512 * j + 2 * i] = 0xFF - i;
	}
    }

  /* one pass (B&W ?) */
  if (cmdSetDataBuffer (initbuf) != 1)
    {
      DBG (0, "cmdSetDataBuffer(initbuf) failed ! (%s:%d) \n", __FILE__,
	   __LINE__);
      return 0;
    }
  DBG (16, "cmdSetDataBuffer(initbuf) passed... (%s:%d)\n", __FILE__,
       __LINE__);
  if (cmdSetDataBuffer (voidbuf) != 1)
    {
      DBG (0, "cmdSetDataBuffer(voidbuf) failed ! (%s:%d) \n", __FILE__,
	   __LINE__);
      return 0;
    }
  DBG (16, "cmdSetDataBuffer(voidbuf) passed... (%s:%d)\n", __FILE__,
       __LINE__);

  /* everything above the FF 55 AA tag is 'void' */
  /* it seems that the buffer is reused and only the beginning is initalized */
  for (i = 515; i < 2048; i++)
    initbuf[i] = voidbuf[i];

  /* three pass (RGB ?) */
  for (i = 0; i < 3; i++)
    {
      if (cmdSetDataBuffer (initbuf) != 1)
	{
	  DBG (0, "cmdSetDataBuffer(initbuf) failed ! (%s:%d) \n", __FILE__,
	       __LINE__);
	  return 0;
	}
      DBG (16, "Loop %d: cmdSetDataBuffer(initbuf) passed... (%s:%d)\n", i,
	   __FILE__, __LINE__);
      if (cmdSetDataBuffer (voidbuf) != 1)
	{
	  DBG (0, "Loop %d: cmdSetDataBuffer(voidbuf) failed ! (%s:%d) \n", i,
	       __FILE__, __LINE__);
	  return 0;
	}
    }


  /* memory size testing ? */
  /* load 150 Ko in scanner */
  REGISTERWRITE (0x1A, 0x00);
  REGISTERWRITE (0x1A, 0x0C);
  REGISTERWRITE (0x1A, 0x00);
  REGISTERWRITE (0x1A, 0x0C);


  REGISTERWRITE (0x0A, 0x11);	/* start */
  nb = 150;
  for (i = 0; i < nb; i++)	/* 300 for ECP ??? */
    {
      bufferWrite (0x400, dest);
      DBG (16, "Loop %d: bufferWrite(0x400,dest) passed... (%s:%d)\n", i,
	   __FILE__, __LINE__);
    }
  REGISTERWRITE (0x0A, 0x18);	/* end */

  /* read them back */
  REGISTERWRITE (0x0A, 0x11);	/*start transfert */
  if (gMode == UMAX_PP_PARPORT_ECP)
    {
      ECPSetBuffer (0x400);
    }

  for (i = 0; i < nb; i++)	/* 300 for ECP ??? */
    {
      bufferRead (0x400, dest);
      DBG (16, "Loop %d: bufferRead(0x400,dest) passed... (%s:%d)\n", i,
	   __FILE__, __LINE__);
    }
  REGISTERWRITE (0x0A, 0x18);	/*end transfer */

  /* fully disconnect, then reconnect */
  if (gMode == UMAX_PP_PARPORT_ECP)
    {
      epilogue ();
      sendCommand (0xE0);
      Outb (DATA, 0xFF);
      Outb (DATA, 0xFF);
      ClearRegister (0);
      WRITESLOW (0x0E, 0x0A);
      SLOWNIBBLEREGISTERREAD (0x0F, 0x00);
      WRITESLOW (0x0F, 0x08);
      WRITESLOW (0x08, 0x10);	/* 0x10 ?? */
      prologue (0x10);
    }


  /* almost cmdSync(0x00) which halts any pending operation */
  if (fonc001 () != 1)
    {
      DBG (0, "fonc001() failed ! (%s:%d) \n", __FILE__, __LINE__);
      return 0;
    }
  DBG (16, "Fct001() passed (%s:%d) \n", __FILE__, __LINE__);
  if (sendWord (zero) == 0)
    {
      DBG (0, "sendWord(zero) failed (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }
  epilogue ();
  DBG (16, "sendWord(zero) passed (%s:%d)\n", __FILE__, __LINE__);


  /* end transport init */
  /* now high level (connected) protocol begins */
  val = sanei_umax_pp_initScanner (recover);
  if (val == 0)
    {
      DBG (0, "initScanner() failed (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }


  /* if no homing .... */
  if (val == 1)
    {
      CMDSYNC (0);
      CMDSYNC (0xC2);
      CMDSYNC (0);
    }

  /* set port to its initial state */
  Outb (DATA, gData);
  Outb (CONTROL, gControl);

  free (dest);
  DBG (1, "probe done ...\n");
  return 1;
}


static int
disconnect_epat (void)
{
  REGISTERWRITE (0x0A, 0x00);
  registerRead (0x0D);
  REGISTERWRITE (0x0D, 0x00);
  disconnect ();
  return 1;
}


static int
connect_epat (int r08)
{
  int reg;

  if (connect () != 1)
    {
      DBG (0, "connect_epat: connect() failed! (%s:%d)\n", __FILE__,
	   __LINE__);
      return 0;
    }

  reg = registerRead (0x0B);
  if (reg != gEPAT)
    {
      /* ASIC version            is not */
      /* the one expected (epat c7)     */
      DBG (0, "Error! expected reg0B=0x%02X, found 0x%02X! (%s:%d) \n", gEPAT,
	   reg, __FILE__, __LINE__);
      /* we try to clean all */
      disconnect ();
      return 0;
    }
  reg = registerRead (0x0D);
  reg = (reg | 0x43) & 0xEB;
  REGISTERWRITE (0x0D, reg);
  REGISTERWRITE (0x0C, 0x04);
  reg = registerRead (0x0A);
  if (reg != 0x00)
    {
      /* a previous unfinished command */
      /* has left an uncleared value   */
      DBG (0, "Warning! expected reg0A=0x00, found 0x%02X! (%s:%d) \n", reg,
	   __FILE__, __LINE__);
    }
  REGISTERWRITE (0x0A, 0x1C);
  if (r08 != 0)
    {
      if (gMode == UMAX_PP_PARPORT_ECP)
	{
	  REGISTERWRITE (0x08, r08);	/* 0x01 or 0x10 ??? */
	}
      else
	{
	  REGISTERWRITE (0x08, 0x21);
	}
    }
  REGISTERWRITE (0x0E, 0x0F);
  REGISTERWRITE (0x0F, 0x0C);
  REGISTERWRITE (0x0A, 0x1C);
  REGISTERWRITE (0x0E, 0x10);
  REGISTERWRITE (0x0F, 0x1C);
  if (gMode == UMAX_PP_PARPORT_ECP)
    {
      REGISTERWRITE (0x0F, 0x00);
    }
  return 1;
}


static int
prologue (int r08)
{
  switch (sanei_umax_pp_getastra ())
    {
    case 610:
      connect610p ();
      return sync610p ();
    case 1220:
    case 1600:
    case 2000:
    default:
      return connect_epat (r08);
    }
}



static int
epilogue (void)
{
  switch (sanei_umax_pp_getastra ())
    {
    case 610:
      return disconnect610p ();
    case 1220:
    case 1600:
    case 2000:
    default:
      return disconnect_epat ();
    }
}


static int
EPPcmdGet610p (int cmd, int len, int *val)
{
  int word[4];
  int i, status, control;

  word[0] = len / 65536;
  word[1] = len / 256 % 256;
  word[2] = len % 256;
  word[3] = (cmd & 0x3F) | 0x80 | 0x40;	/* 0x40 means 'read' */

  connect610p ();
  sync610p ();

  /* sends magic seal 55 AA */
  status = EPPputByte610p (0x55);
  if (status != 0xC8)
    {
      DBG (1, "EPPcmdGet610p: Found 0x%X expected 0xC8  (%s:%d)\n", status,
	   __FILE__, __LINE__);
      return 0;
    }
  status = EPPputByte610p (0xAA);
  if (status != 0xC8)
    {
      DBG (1, "EPPcmdGet610p: Found 0x%02X expected 0xC8  (%s:%d)\n", status,
	   __FILE__, __LINE__);
      return 0;
    }

  /* tests status */
  /* scannerStatus=0x58 */
  status = EPPgetStatus610p ();
  if (status != 0xC8)
    {
      DBG (1,
	   "EPPcmdGet610p: Found 0x%X expected 0xC8, status=0x%02X  (%s:%d)\n",
	   status, scannerStatus, __FILE__, __LINE__);
      return 0;
    }


  /* sends length of data */
  for (i = 0; (i < 4) && (status == 0xC8); i++)
    {
      status = EPPputByte610p (word[i]);
    }
  if (status != 0xC8)
    {
      DBG (1, "EPPcmdGet610p: loop %d, found 0x%02X expected 0xC8  (%s:%d)\n",
	   i, status, __FILE__, __LINE__);
      return 0;
    }

  Outb (DATA, 0xFF);

  /* tests status */
  /* scannerStatus=0x58 */
  status = EPPgetStatus610p ();
  if (status != 0xD0)
    {
      DBG (1,
	   "EPPcmdGet610p: Found 0x%X expected 0xD0, status=0x%02X  (%s:%d)\n",
	   status, scannerStatus, __FILE__, __LINE__);
      return 0;
    }

  /* data reverse */
  control = Inb (CONTROL) & 0xF4;
  control = control | 0xA0;

  /* receive data */
  i = 0;
  while (i < len)
    {
      status = Inb (STATUS) & 0xF8;
      if (status & 0x08)
	{
	  DBG (1,
	       "EPPcmdGet610p: loop %d, found 0x%X expected 0xD0 or 0xC0  (%s:%d)\n",
	       i, status, __FILE__, __LINE__);
	  return 0;
	}
      val[i] = Inb (EPPDATA);
      i++;
    }

  if (DBG_LEVEL >= 8)
    {
      char *str = NULL;

      str = malloc (3 * len + 1);
      if (str != NULL)
	{
	  for (i = 0; i < len; i++)
	    {
	      sprintf (str + 3 * i, "%02X ", val[i]);
	    }
	  str[3 * i] = 0x00;
	  DBG (8, "String received for %02X: %s\n", cmd, str);
	  free (str);
	}
      else
	{
	  TRACE (8, "not enough memory for debugging ...");
	}
    }

  /* scannerStatus=0x58 */
  status = EPPgetStatus610p ();
  scannerStatus = status;
  if (status != 0xC0)
    {
      DBG (0, "EPPcmdGet610p: Found 0x%02X expected 0xC0  (%s:%d)\n", status,
	   __FILE__, __LINE__);
      return 0;
    }
  disconnect610p ();
  return 1;
}



static int
cmdGet610p (int cmd, int len, int *val)
{
  int word[5];
  int i, j, status;

  if ((cmd == 8) && (len > 0x23))
    len = 0x23;

  if (gMode == UMAX_PP_PARPORT_EPP)
    return EPPcmdGet610p (cmd, len, val);

  /* compute word */
  word[0] = len / 65536;
  word[1] = len / 256 % 256;
  word[2] = len % 256;
  word[3] = (cmd & 0x3F) | 0x80 | 0x40;	/* 0x40 means 'read' */
  word[4] = -1;

  connect610p ();
  sync610p ();
  if (sendLength610p (word) == 0)
    {
      DBG (0, "sendLength610p(word) failed... (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }
  status = getStatus610p ();
  scannerStatus = status;
  if ((status != 0xC0) && (status != 0xD0))
    {
      DBG (0, "Found 0x%02X expected 0xC0 or 0xD0 (%s:%d)\n", status,
	   __FILE__, __LINE__);
      return 0;
    }
  if (receiveData610p (val, len) == 0)
    {
      DBG (0, "sendData610p(val,%d) failed  (%s:%d)\n", len, __FILE__,
	   __LINE__);
      return 0;
    }
  status = getStatus610p ();
  scannerStatus = status;
  j = 0;
  while ((j < 256) && (status & 0x08))
    {
      status = getStatus610p ();
      j++;
    }
  if (status != 0xC0)
    {
      DBG (0, "Found 0x%02X expected 0xC0  (%s:%d)\n", status, __FILE__,
	   __LINE__);
      return 0;
    }
  disconnect610p ();

  if (DBG_LEVEL >= 8)
    {
      char *str = NULL;

      str = malloc (3 * len + 1);
      if (str != NULL)
	{
	  for (i = 0; i < len; i++)
	    {
	      sprintf (str + 3 * i, "%02X ", val[i]);
	    }
	  str[3 * i] = 0x00;
	  DBG (8, "String received for %02X: %s\n", cmd, str);
	  free (str);
	}
      else
	{
	  TRACE (8, "not enough memory for debugging ...");
	}
    }
  return 1;
}


static int
EPPcmdSet610p (int cmd, int len, int *val)
{
  int word[5];
  int i, status;

  if ((cmd == 8) && (len > 0x23))
    {
      /* blank useless extra bytes */
      for (i = 0x22; i < len; i++)
	val[i] = 0x00;
    }

  word[0] = len / 65536;
  word[1] = len / 256 % 256;
  word[2] = len % 256;
  word[3] = (cmd & 0x3F) | 0x80;

  connect610p ();
  sync610p ();

  /* sends magic seal 55 AA */
  status = EPPputByte610p (0x55);
  if ((status != 0xC8) && (status!=0xC0))
    {
      DBG (0, "EPPcmdSet610p: Found 0x%X expected 0xC0 or 0xC8  (%s:%d)\n", status,
	   __FILE__, __LINE__);
      return 0;
    }
  status = EPPputByte610p (0xAA);
  if ((status != 0xC8) && (status!=0xC0))
    {
      DBG (0, "EPPcmdSet610p: Found 0x%X expected 0xC0 or 0xC8  (%s:%d)\n", status,
	   __FILE__, __LINE__);
      return 0;
    }

  /* tests status */
  status = EPPgetStatus610p ();
  if ((status != 0xC8) && (status!=0xC0))
    {
      DBG (0, "EPPcmdSet610p: Found 0x%02X expected 0xC0 or 0xC8  (%s:%d)\n", status,
	   __FILE__, __LINE__);
      return 0;
    }

  /* sends length of data */
  for (i = 0; i < 4; i++)
    {
      status = EPPputByte610p (word[i]);
    }
  if ((status != 0xC8) && (status!=0xC0))
    {
      DBG (0, "EPPcmdSet610p: loop %d, found 0x%02X expected 0xC0 or 0xC8  (%s:%d)\n",
	   i, status, __FILE__, __LINE__);
      return 0;
    }

  Outb (DATA, 0xFF);

  /* tests status */
  status = EPPgetStatus610p ();
  if (status != 0xC0)
    {
      DBG (0, "Found 0x%X expected 0xC0 (%s:%d)\n", status, __FILE__,
	   __LINE__);
      return 0;
    }

  /* sends data */
  status = 0xC8;
  for (i = 0; (i < len) && (status == 0xC8); i++)
    {
      /* escape special values with ESC (0x1B) */
      if (val[i] == 0x1B)
	status = EPPputByte610p (0x1B);
      if (i > 0)
	{
	  if ((val[i] == 0xAA) && (val[i - 1] == 0x55))
	    status = EPPputByte610p (0x1B);
	}
      /* now send data */
      status = EPPputByte610p (val[i]);
    }

  if (status != 0xC8)
    {
      DBG (0, "EPPcmdSet610p: loop %d, found 0x%02X expected 0xC8 (%s:%d)\n",
	   i, status, __FILE__, __LINE__);
      return 0;
    }

  Outb (DATA, 0xFF);
  status = EPPgetStatus610p ();
  if (status != 0xC0)
    {
      DBG (0, "Found 0x%X expected 0xC0  (%s:%d)\n", status, __FILE__,
	   __LINE__);
      return 0;
    }

  disconnect610p ();
  return 1;
}


static int
cmdSet610p (int cmd, int len, int *val)
{
  int word[5];
  int i, j, status;

  if (gMode == UMAX_PP_PARPORT_EPP)
    return EPPcmdSet610p (cmd, len, val);

  if ((cmd == 8) && (len > 0x23))
    {
      /* blank useless extra bytes */
      for (i = 0x22; i < len; i++)
	val[i] = 0x00;
    }

  /* compute word */
  word[0] = len / 65536;
  word[1] = len / 256 % 256;
  word[2] = len % 256;
  word[3] = (cmd & 0x3F) | 0x80;
  word[4] = -1;

  connect610p ();
  sync610p ();
  if (sendLength610p (word) == 0)
    {
      DBG (0, "sendLength610p(word) failed... (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }
  status = getStatus610p ();
  scannerStatus = status;
  if ((status != 0xC0) && (status != 0xD0))
    {
      DBG (1, "Found 0x%X expected 0xC0 or 0xD0 (%s:%d)\n", status, __FILE__,
	   __LINE__);
      return 0;
    }
  if (sendData610p (val, len) == 0)
    {
      DBG (1, "sendData610p(val,%d) failed  (%s:%d)\n", len, __FILE__,
	   __LINE__);
      return 0;
    }
  status = getStatus610p ();
  scannerStatus = status;
  j = 0;
  while ((j < 256) && (status & 0x08))
    {
      status = getStatus610p ();
      j++;
    }
  if (status != 0xC0)
    {
      DBG (1, "Found 0x%X expected 0xC0  (%s:%d)\n", status, __FILE__,
	   __LINE__);
      /* return 0; */
    }
  disconnect610p ();
  return 1;
}

static int
cmdSet (int cmd, int len, int *val)
{
  int word[5];
  int i;

  if (DBG_LEVEL >= 8)
    {
      char *str = NULL;

      str = malloc (3 * len + 1);
      if (str != NULL)
	{
	  for (i = 0; i < len; i++)
	    {
	      sprintf (str + 3 * i, "%02X ", val[i]);
	    }
	  str[3 * i] = 0x00;
	  DBG (8, "String sent     for %02X: %s\n", cmd, str);
	  free (str);
	}
      else
	{
	  TRACE (8, "not enough memory for debugging ...");
	}
    }

  if (sanei_umax_pp_getastra () == 610)
    return cmdSet610p (cmd, len, val);

  /* cmd 08 as 2 length depending upon model */
  if ((cmd == 8) && (getModel () == 0x07))
    {
      len = 35;
    }

  /* compute word */
  word[0] = len / 65536;
  word[1] = len / 256 % 256;
  word[2] = len % 256;
  word[3] = (cmd & 0x3F) | 0x80;

  if (!prologue (0x10))
    {
      DBG (0, "cmdSet: prologue failed !   (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }

  /* send data */
  if (sendLength (word, 4) == 0)
    {
      DBG (0, "sendLength(word,4) failed (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }
  TRACE (16, "sendLength(word,4) passed ...");

  /* head end */
  epilogue ();

  if (len > 0)
    {
      /* send body */
      if (!prologue (0x10))
	{
	  DBG (0, "cmdSet: prologue failed !   (%s:%d)\n", __FILE__,
	       __LINE__);
	}

      /* send data */
      if (sendData (val, len) == 0)
	{
	  DBG (0, "sendData(word,%d) failed (%s:%d)\n", len, __FILE__,
	       __LINE__);
	  epilogue ();
	  return 0;
	}
      TRACE (16, "sendData(val,len) passed ...");
      /* body end */
      epilogue ();
    }
  return 1;
}

static int
cmdGet (int cmd, int len, int *val)
{
  int word[5];
  int i;

  if (sanei_umax_pp_getastra () == 610)
    return cmdGet610p (cmd, len, val);

  /* cmd 08 as 2 length depending upon model */
  if ((cmd == 8) && (getModel () == 0x07))
    {
      len = 35;
    }

  /* compute word */
  word[0] = len / 65536;
  word[1] = len / 256 % 256;
  word[2] = len % 256;
  word[3] = (cmd & 0x3F) | 0x80 | 0x40;	/* 0x40 means 'read' */
  word[4] = -1;

  /* send header */
  if (!prologue (0x10))
    {
      DBG (0, "cmdGet: prologue failed !   (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }

  /* send data */
  if (sendLength (word, 4) == 0)
    {
      DBG (0, "sendLength(word,4) failed (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }
  TRACE (16, "sendLength(word,4) passed ...");

  /* head end */
  epilogue ();


  /* send header */
  if (!prologue (0x10))
    {
      DBG (0, "cmdGet: prologue failed !   (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }

  /* get actual data */
  if (receiveData (val, len) == 0)
    {
      DBG (0, "receiveData(val,len) failed (%s:%d)\n", __FILE__, __LINE__);
      epilogue ();
      return 0;
    }
  if (DBG_LEVEL >= 8)
    {
      char *str = NULL;

      str = malloc (3 * len + 1);
      if (str != NULL)
	{
	  for (i = 0; i < len; i++)
	    {
	      sprintf (str + 3 * i, "%02X ", val[i]);
	    }
	  str[3 * i] = 0x00;
	  DBG (8, "String received for %02X: %s\n", cmd, str);
	  free (str);
	}
      else
	{
	  TRACE (8, "not enough memory for debugging ...");
	}
    }
  epilogue ();
  return 1;
}



static int
cmdSetGet (int cmd, int len, int *val)
{
  int *tampon;
  int i;

  /* model revision 0x07 uses 35 bytes buffers */
  /* cmd 08 as 2 length depending upon model */
  if ((cmd == 8) && (getModel () == 0x07))
    {
      len = 0x23;
    }

  /* first we send */
  if (cmdSet (cmd, len, val) == 0)
    {
      DBG (0, "cmdSetGet failed !  (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }

  tampon = (int *) malloc (len * sizeof (int));
  memset (tampon, 0x00, len * sizeof (int));
  if (tampon == NULL)
    {
      DBG (0, "Failed to allocate room for %d int ! (%s:%d)\n", len, __FILE__,
	   __LINE__);
      epilogue ();
      return 0;
    }

  /* then we receive */
  if (cmdGet (cmd, len, tampon) == 0)
    {
      DBG (0, "cmdSetGet failed !  (%s:%d)\n", __FILE__, __LINE__);
      free (tampon);
      epilogue ();
      return 0;
    }

  /* check and copy */
  for (i = 0; (i < len) && (val[i] >= 0); i++)
    {
      if (tampon[i] != val[i])
	{
	  DBG
	    (0,
	     "Warning data read back differs: expected %02X found tampon[%d]=%02X ! (%s:%d)\n",
	     val[i], i, tampon[i], __FILE__, __LINE__);
	}
      val[i] = tampon[i];
    }


  /* OK */
  free (tampon);
  return 1;
}


/* 1 OK, 0 failed */
static int
EPPcmdGetBuffer610p (int cmd, int len, unsigned char *buffer)
{
  int status, i, tmp, control;
  int word[5];
  int count, needed, max;
#ifdef HAVE_LINUX_PPDEV_H
  int fd, mode, rc;
#endif
  int loop, wait, remain;

  /* first we set length and channel */
  /* compute word */
  word[0] = len / 65536;
  word[1] = (len / 256) % 256;
  word[2] = len % 256;
  word[3] = (cmd & 0x0F) | 0xC0;
  word[4] = -1;

  connect610p ();		/* start of EPPsendLength610p */
  sync610p ();

  /* sends magic seal 55 AA */
  status = EPPputByte610p (0x55);
  if ((status != 0xD0) && (status != 0xC8))
    {
      DBG (0, "EPPcmdGetBuffer610p: Found 0x%X expected 0xC8 or 0xD0 (%s:%d)\n",
	   status, __FILE__, __LINE__);
      return 0;
    }
  status = EPPputByte610p (0xAA);
  if ((status != 0xD0) && (status != 0xC8))
    {
      DBG (0, "EPPcmdGetBuffer610p: Found 0x%02X expected 0xC8 or 0xD0 (%s:%d)\n",
	   status, __FILE__, __LINE__);
      return 0;
    }

  /* tests status */
  status = EPPgetStatus610p ();
  if ((status != 0xD0) && (status != 0xC8))
    {
      DBG (0, "EPPcmdGetBuffer610p: Found 0x%X expected 0xC8 or 0xD0 (%s:%d)\n",
	   status, __FILE__, __LINE__);
      return 0;
    }

  /* sends length of data */
  for (i = 0; i < 4; i++)
    {
      status = EPPputByte610p (word[i]);
    }
  if ((status != 0xC0) && (status != 0xC8))
    {
      DBG (0,
	   "EPPcmdGetBuffer610p: loop %d, found 0x%02X expected 0xC0 or 0xC8  (%s:%d)\n",
	   i, status, __FILE__, __LINE__);
      return 0;
    }

  Outb (DATA, 0xFF);

  /* test status */
  status = EPPgetStatus610p ();
  if ((status != 0xC0) && (status != 0xD0))
    {
      DBG (0,
	   "EPPcmdGetBuffer610p: Found 0x%X expected 0xC0 or 0xD0 (%s:%d)\n",
	   status, __FILE__, __LINE__);
      /*return 0; */
    }
  disconnect610p ();		/* end of EPPsendLength610p */

  /* max data read in one go */
  if (sanei_umax_pp_getfull () == 1)
    max = 2550 / 3;
  else
    max = 32768;

  /* loop until enough data is read */
  count = 0;
  while (count < len)
    {
      if (len - count > max)
	needed = max;
      else
	needed = len - count;
      if (needed % 4)
	remain = needed % 4;
      else
	remain = 4;
      loop = (needed - remain) / 2;
      wait = 0;
      DBG (32, "EPPcmdGetBuffer610p: %d loops to do \n", loop);

      status = 0x20;

      /* wait for data ready */
      while ((status & 0x80) != 0x80)
	{
	  /* this is SPPgetStatus */
	  connect610p ();
	  Outb (CONTROL, 0x07);
	  Outb (DATA, 0xFF);
	  tmp = Inb (DATA);
	  if (tmp != 0xFF)
	    {
	      DBG (0,
		   "EPPcmdGetBuffer610p found 0x%02X expected 0xFF  (%s:%d)\n",
		   tmp, __FILE__, __LINE__);
	      return 0;
	    }
	  status = Inb (STATUS) & 0xF8;
	  if ((status & 0x80) != 0x80)
	    {
	      disconnect610p ();
	      usleep(1000);
	    }
	  else
	    {
	      Outb (CONTROL, 0x04);
	      sync610p ();
	      Outb (DATA, 0xFF);
	      control = (Inb (CONTROL) & 0x44) | 0xE4;
	      Outb (CONTROL, control);
	    }
	}

      /* EPP block read */
      /* there is one form for full CCD width reads, and another for other
         reads */
#ifdef HAVE_LINUX_PPDEV_H
      /* check we have ppdev working */
      fd = sanei_umax_pp_getparport ();
      if (fd > 0)
	{
	  mode = 1;		/* data_reverse */
	  rc = ioctl (fd, PPDATADIR, &mode);
	  if (rc)
	    DBG (0,
		 "EPPcmdGetBuffer610p: ppdev ioctl returned <%s>  (%s:%d)\n",
		 strerror (errno), __FILE__, __LINE__);

#ifdef PPSETFLAGS
	  mode = PP_FASTREAD;
	  rc = ioctl (fd, PPSETFLAGS, &mode);
	  if (rc)
	    DBG (0,
		 "EPPcmdGetBuffer610p: ppdev ioctl returned <%s>  (%s:%d)\n",
		 strerror (errno), __FILE__, __LINE__);
#endif
	  mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
	  rc = ioctl (fd, PPSETMODE, &mode);
	  if (rc)
	    {
	      DBG (0,
		   "EPPcmdGetBuffer610p: ppdev ioctl returned <%s>  (%s:%d)\n",
		   strerror (errno), __FILE__, __LINE__);
	      return 0;
	    }
	  if (sanei_umax_pp_getfull () == 1)
	    {
	      do
		{
		  rc = read (fd, buffer + count, needed);
		}
	      while (rc == EAGAIN);
	      if (rc < 0)
		{
		  DBG (0,
		       "EPPcmdGetBuffer610p: ppdev read failed <%s> (%s:%d)\n",
		       strerror (errno), __FILE__, __LINE__);
		  return 0;
		}
#ifdef IOLOG
	      DBG (0, "insb *%d\n", rc);
#endif
	      needed = rc;
	    }
	  else
	    {
		  for (loop = 0; (loop < needed) && (wait==0); loop++)
		    {
		      status = Inb (STATUS) & 0xF8;
		      if ((status != 0xD0) && (status != 0xC0)
			  && (status != 0xC8))
			{
			  DBG (0,
			       "EPPcmdGetBuffer610p found 0x%02X expected 0xD0 or 0xC0 (%s:%d)\n",
			       status, __FILE__, __LINE__);
			  return 0;
			}
		      if (status == 0xC8)
		        {
		          wait = 1;
		          needed=loop;
			}
		      else
		        {
		          tmp = Inb (EPPDATA);
		          buffer[count + loop] = tmp;
			}
		    }
	    }
	}
      else
#endif /* HAVE_LINUX_PPDEV_H */
	{
	  Insb (EPPDATA, buffer + count, needed);
	}
      count += needed;
      disconnect610p ();
    }
  usleep (10000);
  /* ??? CMDSYNC (0x00); */
  /* everything went fine */
  return 1;
}

/* 1 OK, 0 failed */
static int
cmdGetBuffer610p (int cmd, int len, unsigned char *buffer)
{
  int status, i, tmp;
  int word[5];
  int read, needed, max;

  if (gMode == UMAX_PP_PARPORT_EPP)
    return EPPcmdGetBuffer610p (cmd, len, buffer);

  /* first we set length and channel */
  /* compute word */
  word[0] = len / 65536;
  word[1] = (len / 256) % 256;
  word[2] = len % 256;
  word[3] = (cmd & 0x0F) | 0xC0;
  word[4] = -1;

  connect610p ();
  sync610p ();
  if (sendLength610p (word) == 0)
    {
      DBG (0, "sendLength610p(word) failed... (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }
  status = getStatus610p ();
  scannerStatus = status;
  if ((status != 0xC0) && (status != 0xD0))
    {
      DBG (1, "Found 0x%X expected 0xC0 or 0xD0 (%s:%d)\n", status, __FILE__,
	   __LINE__);
      return 0;
    }
  disconnect610p ();

  if (sanei_umax_pp_getfull () == 1)
    max = 2550 / 3;
  else
    max = 32768;
  read = 0;
  while (read < len)
    {
      if (len - read > max)
	needed = max;
      else
	needed = len - read;

      if (sanei_umax_pp_getfull () == 0)
	status = getStatus610p ();
      else
	status = 0x20;

      /* wait for data ready */
      while ((status & 0x80) == 0x00)
	{
	  connect610p ();
	  Outb (CONTROL, 0x07);
	  Outb (DATA, 0xFF);
	  tmp = Inb (DATA);
	  if (tmp != 0xFF)
	    {
	      DBG (0,
		   "cmdGetBuffer610p found 0x%02X expected 0xFF  (%s:%d)\n",
		   tmp, __FILE__, __LINE__);
	      return 0;
	    }
	  status = Inb (STATUS) & 0xF8;
	  if ((status & 0x80) == 0x00)
	    disconnect610p ();
	  else
	    {
	      Outb (CONTROL, 0x04);
	      sync610p ();
	      byteMode ();
	    }
	}

      i = 0;
      while (i < needed)
	{
	  if (sanei_umax_pp_getfull () == 0)
	    {
	      status = Inb (STATUS) & 0xF8;
	      if (status == 0xC8)
		{
		  for (tmp = 0; tmp < 18; tmp++)
		    status = Inb (STATUS) & 0xF8;
		  break;
		}
	    }
	  Outb (CONTROL, 0x26);	/* data reverse+ 'reg' */
	  buffer[read + i] = Inb (DATA);
	  Outb (CONTROL, 0x24);	/* data reverse+ 'reg' */
	  i++;
	}
      byteMode ();
      disconnect610p ();
      read += i;
    }

  return 1;
}


/* 1 OK, 0 failed */
static int
cmdGetBuffer (int cmd, int len, unsigned char *buffer)
{
  int reg, tmp, i;
  int word[5], read;
  int needed;

  if (sanei_umax_pp_getastra () == 610)
    return cmdGetBuffer610p (cmd, len, buffer);

  /* compute word */
  word[0] = len / 65536;
  word[1] = len / 256 % 256;
  word[2] = len % 256;
  word[3] = (cmd & 0x0F) | 0xC0;
  word[4] = -1;

  /* send word: len+addr(?) */
  if (foncSendWord (word) == 0)
    {
      DBG (0, "foncSendWord(word) failed (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }
  DBG (16, "(%s:%d) passed \n", __FILE__, __LINE__);

  prologue (0x10);

  REGISTERWRITE (0x0E, 0x0D);
  REGISTERWRITE (0x0F, 0x00);

  i = 0;
  reg = registerRead (0x19) & 0xF8;

  /* wait if busy */
  while ((reg & 0x08) == 0x08)
    reg = registerRead (0x19) & 0xF8;
  if ((reg != 0xC0) && (reg != 0xD0))
    {
      DBG (0, "cmdGetBuffer failed (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }

  if (gMode == UMAX_PP_PARPORT_ECP)
    {
      REGISTERWRITE (0x1A, 0x44);
    }

  read = 0;
  reg = registerRead (0x0C);
  if (reg != 0x04)
    {
      DBG (0, "cmdGetBuffer failed: unexpected status 0x%02X  ...(%s:%d)\n",
	   reg, __FILE__, __LINE__);
      return 0;
    }
  REGISTERWRITE (0x0C, reg | 0x40);

  /* actual data */
  read = 0;
  while (read < len)
    {
      needed = len - read;
      if (needed > 32768)
	needed = 32768;
      if (gMode == UMAX_PP_PARPORT_ECP)
	{
	  compatMode ();
	  Outb (CONTROL, 0x04);	/* reset ? */
	  ECPSetBuffer (needed);
	  tmp = ECPbufferRead (needed, buffer + read);
	  DBG (16, "ECPbufferRead(%d,buffer+read) passed (%s:%d)\n", needed,
	       __FILE__, __LINE__);
	  REGISTERWRITE (0x1A, 0x84);
	}
      else
	{
	  tmp = pausedBufferRead (needed, buffer + read);
	}
      if (tmp < needed)
	{
	  DBG (64, "cmdGetBuffer only got %d bytes out of %d ...(%s:%d)\n",
	       tmp, needed, __FILE__, __LINE__);
	}
      else
	{
	  DBG (64,
	       "cmdGetBuffer got all %d bytes out of %d , read=%d...(%s:%d)\n",
	       tmp, 32768, read, __FILE__, __LINE__);
	}
      read += tmp;
      DBG (16, "Read %d bytes out of %d (last block is %d bytes) (%s:%d)\n",
	   read, len, tmp, __FILE__, __LINE__);
      if (read < len)
	{
	  /* wait for scanner to be ready */
	  reg = registerRead (0x19) & 0xF8;
	  DBG (64, "Status after block read is 0x%02X (%s:%d)\n", reg,
	       __FILE__, __LINE__);
	  if ((reg & 0x08) == 0x08)
	    {
	      int pass = 0;

	      do
		{
		  reg = registerRead (0x19) & 0xF8;
		  usleep (100);
		  pass++;
		}
	      while ((pass < 32768) && ((reg & 0x08) == 0x08));
	      DBG (64, "Status after waiting is 0x%02X (pass=%d) (%s:%d)\n",
		   reg, pass, __FILE__, __LINE__);
	      if ((reg != 0xC0) && (reg != 0xD0))
		{
		  DBG (0,
		       "Unexpected status 0x%02X, expected 0xC0 or 0xD0 ! (%s:%d)\n",
		       reg, __FILE__, __LINE__);
		  DBG (0, "Going on...\n");
		}
	    }

	  /* signal we want next data chunk */
	  if (gMode == UMAX_PP_PARPORT_ECP)
	    {
	      REGISTERWRITE (0x1A, 0x44);
	    }
	  reg = registerRead (0x0C);
	  registerWrite (0x0C, reg | 0x40);
	}
    }

  /* OK ! */
  REGISTERWRITE (0x0E, 0x0D);
  REGISTERWRITE (0x0F, 0x00);

  /* epilogue */
  epilogue ();
  return 1;
}

/* 1 OK, 0 failed */
static int
cmdGetBuffer32 (int cmd, int len, unsigned char *buffer)
{
  int reg, tmp, i;
  int word[5], read;

  /* compute word */
  word[0] = len / 65536;
  word[1] = len / 256 % 256;
  word[2] = len % 256;
  word[3] = (cmd & 0x3F) | 0x80 | 0x40;

  if (!prologue (0x10))
    {
      DBG (0, "cmdSet: prologue failed !   (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }

  /* send data */
  if (sendLength (word, 4) == 0)
    {
      DBG (0, "sendLength(word,4) failed (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }
  TRACE (16, "sendLength(word,4) passed ...");

  /* head end */
  epilogue ();

  prologue (0x10);
  REGISTERWRITE (0x0E, 0x0D);
  REGISTERWRITE (0x0F, 0x00);

  i = 0;
  reg = registerRead (0x19) & 0xF8;

  /* wait if busy */
  while ((reg & 0x08) == 0x08)
    reg = registerRead (0x19) & 0xF8;
  if ((reg != 0xC0) && (reg != 0xD0))
    {
      DBG (0, "cmdGetBuffer32 failed: unexpected status 0x%02X  ...(%s:%d)\n",
	   reg, __FILE__, __LINE__);
      return 0;
    }
  reg = registerRead (0x0C);
  if (reg != 0x04)
    {
      DBG (0, "cmdGetBuffer32 failed: unexpected status 0x%02X  ...(%s:%d)\n",
	   reg, __FILE__, __LINE__);
      return 0;
    }
  REGISTERWRITE (0x0C, reg | 0x40);

  read = 0;
  while (read < len)
    {
      if (read + 1700 < len)
	{
	  tmp = 1700;
	  bufferRead (tmp, buffer + read);
	  reg = registerRead (0x19) & 0xF8;
	  if ((read + tmp < len) && (reg & 0x08) == 0x08)
	    {
	      do
		{
		  reg = registerRead (0x19) & 0xF8;
		}
	      while ((reg & 0x08) == 0x08);
	      if ((reg != 0xC0) && (reg != 0xD0))
		{
		  DBG (0,
		       "Unexpected status 0x%02X, expected 0xC0 or 0xD0 ! (%s:%d)\n",
		       reg, __FILE__, __LINE__);
		  DBG (0, "Going on...\n");
		}
	    }
	  reg = registerRead (0x0C);
	  registerWrite (0x0C, reg | 0x40);
	  read += tmp;
	}
      else
	{
	  tmp = len - read;
	  bufferRead (tmp, buffer + read);
	  read += tmp;
	  if ((read < len))
	    {
	      reg = registerRead (0x19) & 0xF8;
	      while ((reg & 0x08) == 0x08)
		{
		  reg = registerRead (0x19) & 0xF8;
		}
	    }
	}
    }

  /* OK ! */
  epilogue ();
  return 1;
}

int
sanei_umax_pp_cmdSync (int cmd)
{
  int word[5];


  if (sanei_umax_pp_getastra () == 610)
    return cmdSync610p (cmd);

  /* compute word */
  word[0] = 0x00;
  word[1] = 0x00;
  word[2] = 0x00;
  word[3] = cmd;

  if (!prologue (0x10))
    {
      DBG (0, "cmdSync: prologue failed !   (%s:%d)\n", __FILE__, __LINE__);
    }

  /* send data */
  if (sendLength (word, 4) == 0)
    {
      DBG (0, "sendLength(word,4) failed (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }
  TRACE (16, "sendLength(word,4) passed ...");

  /* end OK */
  epilogue ();

  return 1;
}


/* numbers of bytes read, else 0 (failed)                             */
/* read data by chunk EXACTLY the width of the scan area in the given */
/* resolution . If a valid file descriptor is given, we write data    */
/* in it according to the color mode, before polling the scanner      */
/* len should not be bigger than 2 Megs                                      */

int
cmdGetBlockBuffer (int cmd, int len, int window, unsigned char *buffer)
{
#ifdef HAVE_SYS_TIME_H
  struct timeval td, tf;
  float elapsed;
#endif
  int reg, i;
  int word[5], read;

  /* compute word */
  word[0] = len / 65536;
  word[1] = len / 256 % 256;
  word[2] = len % 256;
  word[3] = (cmd & 0x3F) | 0x80 | 0x40;

  if (!prologue (0x10))
    {
      DBG (0, "cmdGetBlockBuffer: prologue failed !   (%s:%d)\n", __FILE__,
	   __LINE__);
    }

  /* send data */
  if (sendLength (word, 4) == 0)
    {
      DBG (0, "sendLength(word,4) failed (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }
  TRACE (16, "sendLength(word,4) passed ...");
  /* head end */
  epilogue ();



  if (!prologue (0x10))
    {
      DBG (0, "cmdGetBlockBuffer: prologue failed !   (%s:%d)\n", __FILE__,
	   __LINE__);
    }


  REGISTERWRITE (0x0E, 0x0D);
  REGISTERWRITE (0x0F, 0x00);

  i = 0;

  /* init counter */
  read = 0;

  /* read scanner state */
  reg = registerRead (0x19) & 0xF8;


  /* read loop */
  while (read < len)
    {
      /* wait for the data to be ready */
#ifdef HAVE_SYS_TIME_H
      gettimeofday (&td, NULL);
#endif
      while ((reg & 0x08) == 0x08)
	{
	  reg = registerRead (0x19) & 0xF8;
#ifdef HAVE_SYS_TIME_H
	  gettimeofday (&tf, NULL);
	  elapsed =
	    ((tf.tv_sec * 1000000 + tf.tv_usec) -
	     (td.tv_sec * 1000000 + td.tv_usec)) / 1000000;
	  if (elapsed > 3)
	    {
	      DBG
		(0,
		 "Time-out (%.2f s) waiting for scanner ... giving up on status 0x%02X !   (%s:%d)\n",
		 elapsed, reg, __FILE__, __LINE__);
	      epilogue ();
	      return read;
	    }
#endif
	}
      if ((reg != 0xC0) && (reg != 0xD0) && (reg != 0x00))
	{
	  DBG (0,
	       "Unexpected status 0x%02X, expected 0xC0 or 0xD0 ! (%s:%d)\n",
	       reg, __FILE__, __LINE__);
	  DBG (0, "Going on...\n");
	}

      /* signals next chunk */
      reg = registerRead (0x0C);
      if (reg != 0x04)
	{
	  DBG (0,
	       "cmdGetBlockBuffer failed: unexpected value reg0C=0x%02X  ...(%s:%d)\n",
	       reg, __FILE__, __LINE__);
	  return 0;
	}
      REGISTERWRITE (0x0C, reg | 0x40);


      /* there is always a full block ready when scanner is ready */
      /* 32 bits I/O read , window must match the width of scan   */
      bufferRead (window, buffer + read);

      /* add bytes read */
      read += window;


      DBG (16, "Read %d bytes out of %d (last block is %d bytes) (%s:%d)\n",
	   read, len, window, __FILE__, __LINE__);

      /* test status after read */
      reg = registerRead (0x19) & 0xF8;
    }


  /* wait for the data to be ready */
#ifdef HAVE_SYS_TIME_H
  gettimeofday (&td, NULL);
#endif
  while ((reg & 0x08) == 0x08)
    {
      reg = registerRead (0x19) & 0xF8;
#ifdef HAVE_SYS_TIME_H
      gettimeofday (&tf, NULL);
      elapsed =
	((tf.tv_sec * 1000000 + tf.tv_usec) -
	 (td.tv_sec * 1000000 + td.tv_usec)) / 1000000;
      if (elapsed > 3)
	{
	  DBG
	    (0,
	     "Time-out (%.2f s) waiting for scanner ... giving up on status 0x%02X !   (%s:%d)\n",
	     elapsed, reg, __FILE__, __LINE__);
	  epilogue ();
	  return read;
	}
#endif
    }
  if ((reg != 0xC0) && (reg != 0xD0) && (reg != 0x00))
    {
      DBG (0, "Unexpected status 0x%02X, expected 0xC0 or 0xD0 ! (%s:%d)\n",
	   reg, __FILE__, __LINE__);
      DBG (0, "Going on...\n");
    }

  REGISTERWRITE (0x0E, 0x0D);
  REGISTERWRITE (0x0F, 0x00);


  /* OK ! */
  epilogue ();
  return read;
}

/* 
 * encodes DC offsets: must be in [0..0x0F] range
 */
static void
encodeDC (int dcRed, int dcGreen, int dcBlue, int *motor)
{
  motor[11] = (motor[11] & 0x0F) | dcRed << 4;
  motor[12] = (motor[12] & 0xC3) | dcGreen << 2;
  motor[13] = (motor[13] & 0xF0) | dcBlue;
}

static void
decodeDC (int *motor)
{
  DBG (0, "DC (R,G,B)=(%d,%d,%d)\n",
       (motor[11] & 0xF0) >> 4, (motor[12] & 0x3C) >> 2, motor[13] & 0x0F);
}


/* 
 * encodes VGA : must be in [0..0x0F] range
 */
static void
encodeVGA (int vgaRed, int vgaGreen, int vgaBlue, int *motor)
{
  if (sanei_umax_pp_getastra () > 610)
    {
      motor[10] = (vgaRed << 4) | vgaGreen;
      motor[11] = (motor[11] & 0xF0) | vgaBlue;
    }
  else
    {
      motor[10] = (vgaGreen << 4) | vgaBlue;
      motor[11] = (motor[11] & 0xF0) | vgaRed;
      /* ancien 
         F00: vert 
         0F0: bleu
         00F: rouge   
         motor[10] = (vgaRed << 4) | vgaGreen;
         motor[11] = (motor[11] & 0xF0) | vgaBlue; */
    }
}

static void
decodeVGA (int *motor)
{
  if (sanei_umax_pp_getastra () > 610)
    {
      DBG (0, "VGA (R,G,B)=(%d,%d,%d)\n",
	   (motor[10] & 0xF0) >> 4, (motor[10] & 0x0F), (motor[11] & 0x0F));
    }
  else
    {
      DBG (0, "VGA (R,G,B)=(%d,%d,%d)\n",
	   (motor[11] & 0x0F), (motor[10] & 0xF0) >> 4, (motor[10] & 0x0F));
    }
}

/*
 * this function encodes total head movement which includes
 * y movement and scan area height
 * height is scan area height
 * ypos   is head movement before scan
 * total move will be ypos+height */
static void
encodeHY (int height, int ypos, int *motor)
{
  motor[0] = height % 256;
  motor[1] = (height / 256) & 0x3F;
  motor[1] = motor[1] | (ypos & 0x03) << 6;
  motor[2] = (ypos >> 2) % 256;
  motor[3] = (motor[3] & 0xF0) | ((ypos >> 10) & 0x0F);
}

/*
 * this function encodes x start and x end on the CCD
 * w is width of scanning area
 * x is start of scanning area
 * dpi is x resolution
 * color is non zero if scanning in color
 * bytes is on aoverride for bpl, since it sin't clear today when
 * the formula has to be applied
 */
static void
encodeWX (int width, int xstart, int dpi, int color, int *ccd, int bytes)
{
  int xend;
  int bpl;
  int x;

  xend = xstart + width;
  x = xstart - 1;

  /* x start encoding */
  ccd[17] = x % 256;
  ccd[18] = (ccd[18] & 0xF0) | ((x / 256) & 0x0F);
  /* models >=1220P have a 600 dpi CCD: x is bigger */
  if (sanei_umax_pp_getastra () > 610)
    {
      if (x > 0x1000)
	ccd[33] |= 0x40;
      else
	ccd[33] &= 0xBF;
    }

  /* x end encoding */
  ccd[18] = (ccd[18] & 0x0F) | ((xend % 16) << 4);
  ccd[19] = (xend / 16) % 256;
  /* models >=1220P have a 600 dpi CCD: x is bigger */
  if (sanei_umax_pp_getastra () > 610)
    {
      if (xend > 0x1000)
	ccd[33] |= 0x80;
      else
	ccd[33] &= 0x7F;
    }

  /* now bytes per line */
  /* bpl = (op[24] - 0x41) * 256 + 8192 * (op[34] & 0x01) + op[23]; */
  bpl = (color == 0 ? 1 : 3) * width * dpi;
  if (sanei_umax_pp_getastra () > 610)
    {
      bpl /= 600;
      if (bpl >= 8192)
	ccd[34] |= 0x01;
      else
	ccd[34] &= 0xFE;
    }
  else
    {
      bpl /= 300;
    }
  if (bytes > 0)
    bpl = bytes;
  ccd[23] = bpl % 256;
  ccd[24] = 0x41 + ((bpl / 256) & 0x1F);
}

  /* cropping coefficient: last 2 bytes gives the coefficient applied */
  /* to data scanned to get the actual image resolution               */
static void
encodeCoefficient (int color, int dpi, int *calibration)
{
  int w, idx = 0;
  int *coeff;

  /* 75, 150, 300, 600 and 1200 dpi */
  int color610p[4][2] =
    { {0x88, 0x88}, {0xAA, 0xAA}, {0xFF, 0xFF}, {0xFF, 0xFF} };
  int gray610p[4][2] =
    { {0x88, 0x01}, {0xAA, 0x11}, {0xFF, 0xAA}, {0xFF, 0xFF} };

  /* FF means coeff=1
   * AA       coeff=1/2
   * 88       coeff=1/4
   * 80       coeff=1/8
   * first coeff for CCD (x)
   * second coeff for motor steps (y)
   */
  int color1220p[5][2] =
    { {0x80, 0xAA}, {0x88, 0xFF}, {0xAA, 0xFF}, {0xFF, 0xFF}, {0xFF, 0xFF} };
  int gray1220p[5][2] =
    { {0x80, 0x88}, {0x88, 0xAA}, {0xAA, 0xFF}, {0xFF, 0xFF}, {0xFF, 0xFF} };

  switch (dpi)
    {
    case 1200:
      idx = 4;
      break;
    case 600:
      idx = 3;
      break;
    case 300:
      idx = 2;
      break;
    case 150:
      idx = 1;
      break;
    case 75:
      idx = 0;
      break;
    }

  if (sanei_umax_pp_getastra () < 1210)
    {
      w = 2550;
      if (color >= RGB_MODE)
	coeff = color610p[idx];
      else
	coeff = gray610p[idx];
    }
  else
    {
      w = 5100;
      if (color >= RGB_MODE)
	coeff = color1220p[idx];
      else
	coeff = gray1220p[idx];
    }

  /* x coefficient */
  calibration[3 * w + 768] = coeff[0];

  /* y coefficient */
  calibration[3 * w + 769] = coeff[1];
}


static void
bloc2Decode (int *op)
{
  int i;
  int scanh;
  int skiph;
  int dpi = 0;
  int dir = 0;
  int color = 0;
  char str[64];

  for (i = 0; i < 16; i++)
    sprintf (str + 3 * i, "%02X ", (unsigned char) op[i]);
  str[48] = 0x00;
  DBG (0, "Command bloc 2: %s\n", str);


  scanh = op[0] + (op[1] & 0x3F) * 256;
  skiph = ((op[1] & 0xC0) >> 6) + (op[2] << 2) + ((op[3] & 0x0F) << 10);

  if (op[3] & 0x10)
    dir = 1;
  else
    dir = 0;

  /* XXX STEF XXX seems to conflict with DC definitions */
  if (op[13] & 0x40)
    color = 1;
  else
    color = 0;

  /* op[6]=0x60 at 600 and 1200 dpi */
  if ((op[8] == 0x17) && (op[9] != 0x05))
    dpi = 150;
  if ((op[8] == 0x17) && (op[9] == 0x05))
    dpi = 300;
  if ((op[9] == 0x05) && (op[14] & 0x08))
    dpi = 1200;
  if ((dpi == 0) && ((op[14] & 0x08) == 0))
    dpi = 600;



  DBG (0, "\t->scan height   =0x%04X (%d)\n", scanh, scanh);
  DBG (0, "\t->skip height   =0x%04X (%d)\n", skiph, skiph);
  DBG (0, "\t->y dpi         =0x%04X (%d)\n", dpi, dpi);
  decodeVGA (op);
  decodeDC (op);
  if (dir)
    DBG (0, "\t->forward direction\n");
  else
    DBG (0, "\t->reverse direction\n");
  if (color)
    DBG (0, "\t->color scan       \n");
  else
    DBG (0, "\t->no color scan    \n");

  /* byte 14 */
  if (op[14] & 0x20)
    {
      DBG (0, "\t->lamp on    \n");
    }
  else
    {
      DBG (0, "\t->lamp off    \n");
    }
  if (op[14] & 0x04)
    {
      DBG (0, "\t->normal scan (head stops at each row)    \n");
    }
  else
    {
      DBG (0, "\t->move and scan (head doesn't stop at each row)    \n");
    }
  DBG (0, "\n");
}


static void
bloc8Decode (int *op)
{
  int i, bpl;
  int xskip;
  int xend, len;
  char str[128];

  if (sanei_umax_pp_getastra () < 1220)
    len = 34;
  else
    len = 36;
  for (i = 0; i < len; i++)
    sprintf (str + 3 * i, "%02X ", (unsigned char) op[i]);
  str[3 * i] = 0x00;
  DBG (0, "Command bloc 8: %s\n", str);

  xskip = op[17] + 256 * (op[18] & 0x0F);
  if (op[33] & 0x40)
    xskip += 0x1000;
  xend = (op[18] & 0xF0) / 16 + 16 * op[19];
  if (op[33] & 0x80)
    xend += 0x1000;
  if (len > 34)
    bpl = (op[24] - 0x41) * 256 + 8192 * (op[34] & 0x01) + op[23];
  else
    bpl = (op[24] - 0x41) * 256 + op[23];

  DBG (0, "\t->xskip     =0x%X (%d)\n", xskip, xskip);
  DBG (0, "\t->xend      =0x%X (%d)\n", xend, xend);
  DBG (0, "\t->scan width=0x%X (%d)\n", xend - xskip - 1, xend - xskip - 1);
  DBG (0, "\t->bytes/line=0x%X (%d)\n", bpl, bpl);
  DBG (0, "\t->raw       =0x%X (%d)\n", op[24] * 256 + op[23],
       op[24] * 256 + op[23]);
  DBG (0, "\n");
}
static int
completionWait (void)
{
  /* for 610P, wait and sync is done while 
   * reading data from the scanner */
  CMDSYNC (0x40);
  usleep (100000);
  CMDSYNC (0xC2);
  if ((sanei_umax_pp_getastra () == 610)
      || ((sanei_umax_pp_scannerStatus () & 0x90) == 0x90))
    return 1;
  do
    {
      usleep (100000);
      CMDSYNC (0xC2);
    }
  while ((sanei_umax_pp_scannerStatus () & 0x90) != 0x90);
  CMDSYNC (0xC2);
  return 1;
}

int
sanei_umax_pp_setLamp (int on)
{
  int buffer[17];
  int state;

  /* reset? */
  sanei_umax_pp_cmdSync (0x00);
  sanei_umax_pp_cmdSync (0xC2);
  sanei_umax_pp_cmdSync (0x00);

  /* get status */
  cmdGet (0x02, 16, buffer);
  state = buffer[14] & LAMP_STATE;
  buffer[16] = -1;
  if ((state == 0) && (on == 0))
    {
      DBG (0, "Lamp already off ... (%s:%d)\n", __FILE__, __LINE__);
      return 1;
    }
  if ((state) && (on))
    {
      DBG (2, "Lamp already on ... (%s:%d)\n", __FILE__, __LINE__);
      return 1;
    }

  /* set lamp state */
  if (on)
    buffer[14] = buffer[14] | LAMP_STATE;
  else
    buffer[14] = buffer[14] & ~LAMP_STATE;
  CMDSETGET (0x02, 16, buffer);
  TRACE (16, "setLamp passed ...");
  return 1;
}

static int num = 0;
static void
Dump (int len, unsigned char *data, char *name)
{
  FILE *fic;
  char titre[80];

  if (name == NULL)
    {
      sprintf (titre, "dump%04d.bin", num);
      num++;
    }
  else
    {
      sprintf (titre, "%s", name);
    }
  fic = fopen (titre, "wb");
  if (fic == NULL)
    {
      DBG (0, "could not open %s for writing\n", titre);
      return;
    }
  fwrite (data, 1, len, fic);
  fclose (fic);
}


static void
DumpNB (int width, int height, unsigned char *data, char *name)
{
  FILE *fic;
  char titre[80];

  if (name == NULL)
    {
      sprintf (titre, "dump%04d.pnm", num);
      num++;
    }
  else
    {
      sprintf (titre, "%s", name);
    }
  fic = fopen (titre, "wb");
  if (fic == NULL)
    {
      DBG (0, "could not open %s for writing\n", titre);
      return;
    }
  fprintf (fic, "P5\n%d %d\n255\n", width, height);
  fwrite (data, width, height, fic);
  fclose (fic);
}


/* dump data has received from scanner (red line/green line/blue line)
   to a color pnm file */
static void
DumpRVB (int width, int height, unsigned char *data, char *name)
{
  FILE *fic;
  char titre[80];
  int y, x;

  if (name == NULL)
    {
      sprintf (titre, "dump%04d.pnm", num);
      num++;
    }
  else
    {
      sprintf (titre, "%s", name);
    }
  fic = fopen (titre, "wb");
  if (fic == NULL)
    {
      DBG (0, "could not open %s for writing\n", titre);
      return;
    }
  fprintf (fic, "P6\n%d %d\n255\n", width, height);
  for (y = 0; y < height; y++)
    {
      for (x = 0; x < width; x++)
	{
	  fputc (data[3 * y * width + 2 * width + x], fic);
	  fputc (data[3 * y * width + width + x], fic);
	  fputc (data[3 * y * width + x], fic);
	}
    }
  fclose (fic);
}

/* dump a color buffer in a color PNM */
static void
DumpRGB (int width, int height, unsigned char *data, char *name)
{
  FILE *fic;
  char titre[80];
  int y, x;

  if (name == NULL)
    {
      sprintf (titre, "dump%04d.pnm", num);
      num++;
    }
  else
    {
      sprintf (titre, "%s", name);
    }
  fic = fopen (titre, "wb");
  fprintf (fic, "P6\n%d %d\n255\n", width, height);
  if (fic == NULL)
    {
      DBG (0, "could not open %s for writing\n", titre);
      return;
    }
  for (y = 0; y < height; y++)
    {
      for (x = 0; x < width; x++)
	{
	  fputc (data[3 * y * width + x * 3], fic);
	  fputc (data[3 * y * width + x * 3 + 1], fic);
	  fputc (data[3 * y * width + x * 3 + 2], fic);
	}
    }
  fclose (fic);
}

static int
evalGain (int sum, int count)
{
  int gn;
  float pct;
  float avg;
  float area=50;
  float coeff=2.5;
  float cnst=0.9;


  /* after ~ 60 * 10 scans , it looks like 1 step is a 0.57% increase   */
  /* so we take the value and compute the percent increase to reach 250 */
  /* (target code) not 255, because we want some room for inaccuracy    */
  /* pct=100-(value*100)/250                                            */
  /* then correction is pct/0.57                                        */
  avg = (float) (sum) / (float) (count);
  pct = 100.0 - (avg * 100.0) / targetCode;
  gn = (int) (pct / 0.57);
 
  /* give gain for dark areas a boost */
#ifdef UMAX_PP_DANGEROUS_EXPERIMENT
  if(getenv("AREA")!=NULL)
        cnst=atol(getenv("AREA"));
  if(getenv("COEFF")!=NULL)
        cnst=atol(getenv("COEFF"));
  if(getenv("CNST")!=NULL)
        cnst=atol(getenv("CNST"));
#endif

  pct = gn;
  avg = exp((-pct)/area)*coeff+cnst;
  gn = gn * avg;
  
  /* bound checking : there are sightings of >127 values being negative */
  if (gn < 0)
    gn = 0;
  else if (gn > 127)
    gn = 127;
  return gn;
}

static void
computeCalibrationData (int color, int width, unsigned char *source,
			int *data)
{
  int p, i, l;
  int sum;


  memset (data, 0, (3 * 5100 + 768 + 3) * sizeof (int));


  /* 0 -> 5099 */
  for (i = 0; i < width; i++)
    {				/* red calibration data */
      if (color >= RGB_MODE)
	{
	  /* compute average */
	  sum = 0;
	  for (l = 0; l < 66; l++)
	    sum += source[i + l * 5100 * 3];
	  data[i] = evalGain (sum, l);
	}
      else
	data[i] = 0x00;
    }


  /* 5100 -> 10199: green data */
  p = 5100;
  for (i = 0; i < width; i++)
    {
      /* compute average */
      sum = 0;
      for (l = 0; l < 66; l++)
	sum += source[i + l * 5100 * 3 + 5100];
      data[p + i] = evalGain (sum, l);
    }


  /* 10200 -> 15299: blue */
  p = 10200;
  for (i = 0; i < width; i++)
    {
      if (color >= RGB_MODE)
	{
	  /* compute average */
	  sum = 0;
	  for (l = 0; l < 66; l++)
	    sum += source[i + l * 5100 * 3 + 5100 * 2];
	  data[p + i] = evalGain (sum, l);
	}
      else
	data[p + i] = 0x00;
    }


  /* gamma tables */
  for (i = 0; i < 256; i++)
    data[15300 + i] = ggRed[i];
  for (i = 0; i < 256; i++)
    data[15300 + 256 + i] = ggGreen[i];
  for (i = 0; i < 256; i++)
    data[15300 + 512 + i] = ggBlue[i];
  data[16070] = -1;
}



/* move head by the distance given using precision or not */
/* 0: failed  
   1: success                                          */
static int
move (int distance, int precision, unsigned char *buffer)
{
  int header[17] = {
    0x01, 0x00, 0x00, 0x20, 0x00, 0x00, 0x60, 0x2F,
    0x2F, 0x01, 0x00, 0x00, 0x00, 0x80, 0xA4, 0x00,
    -1
  };
  int body[37] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00,
    0x6E, 0xF6, 0x79, 0xBF, 0x01, 0x00, 0x00, 0x00,
    0x46, 0xA0, 0x00, 0x8B, 0x49, 0x2A, 0xE9, 0x68,
    0xDF, 0x13, 0x1A, 0x00,
    -1
  };
  int end[9] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, -1 };
  int steps, len, cmdlen;
  unsigned char tmp[0x200];
  unsigned char *ptr;

  if (distance == 0)
    return 0;

  if (buffer == NULL)
    ptr = tmp;
  else
    ptr = buffer;

  /* build commands */
  if (distance < 0)
    {
      /* header */
      steps = -distance - 1;
      header[3] = 0x20;
      header[9] = 0x01;

      /* reverse direction body by default */

      /* end */
      end[1] = 0xFF;
      end[2] = 0xFF;
      end[3] = -1;
      len = 3;
    }
  else
    {
      /* header */
      steps = distance - 1;
      header[3] = 0x70;
      header[9] = 0x05;

      /* body */
      body[2] = 0x04;
      body[4] = 0x02;
      body[7] = 0x0C;
      body[9] = 0x04;
      body[10] = 0x40;
      body[11] = 0x01;
      /* end */
      len = 8;
    }
  if (steps > 0)
    {
      encodeHY (1, steps, header);
    }


  if (sanei_umax_pp_getastra () < 1220)
    {
      header[6] = 0xC0;

      body[16] = 0x76;		/* encodeWX */
      body[17] = 0x00;
      body[18] = 0x15;
      body[19] = 0x70;
      body[20] = 0x01;
      body[21] = 0x00;

      body[28] = 0x4D;
      body[29] = 0x4B;
      body[30] = 0xD0;

      cmdlen = 0x22;
    }
  else
    cmdlen = 0x24;

  /* precision: default header set to precision on */
  if (precision == PRECISION_OFF)
    {
      if (sanei_umax_pp_getastra () == 1600)
	header[8] = 0x15;
      else
	header[8] = 0x17;
      if (sanei_umax_pp_getastra () > 610)
	header[14] = 0xAC;
      body[20] = 0x06;
    }
  CMDSETGET (0x02, 16, header);
  CMDSETGET (0x08, cmdlen, body);
  if (DBG_LEVEL >= 128)
    {
      bloc2Decode (header);
      bloc8Decode (body);
    }
  CMDSYNC (0xC2);
  if ((sanei_umax_pp_scannerStatus () & 0x80)
      || (sanei_umax_pp_getastra () < 1220))
    {
      CMDSYNC (0x00);
    }
  CMDSETGET (4, len, end);
  COMPLETIONWAIT;
  CMDGETBUF (4, 0x200, ptr);
  if (DBG_LEVEL >= 128)
    {
      Dump (0x200, ptr, NULL);
    }
  DBG (16, "MOVE STATUS IS 0x%02X  (%s:%d)\n", sanei_umax_pp_scannerStatus (),
       __FILE__, __LINE__);
  CMDSYNC (0x00);
  return 1;
}



/* for each column, finds the row where white/black transition occurs
        then returns the average */
static float
edgePosition (int width, int height, unsigned char *data)
{
  int ecnt, x, y;
  float epos;
  int d, dmax, dpos, i;
  unsigned char *dbuffer = NULL;

  if (DBG_LEVEL > 128)
    {
      dbuffer = (unsigned char *) malloc (3 * width * height);
      memset (dbuffer, 0x00, 3 * width * height);
    }
  epos = 0;
  ecnt = 0;
  for (x = 0; x < width; x++)
    {
      /* edge: white->black drop  */
      /* loop stops on black area */
      dmax = 0;
      dpos = 0;
      d = 0;
      i = 0;
      for (y = 10; (y < height) && (data[i] > 10); y++)
	{
	  i = x + y * width;
	  d = data[i - width] - data[i];
	  if (d > dmax)
	    {
	      dmax = d;
	      dpos = y;
	    }
	  if ((DBG_LEVEL > 128) && (dbuffer != NULL))
	    {
	      dbuffer[i * 3] = data[i];
	      dbuffer[i * 3 + 1] = data[i];
	      dbuffer[i * 3 + 2] = data[i];
	    }
	}
      epos += dpos;
      ecnt++;
      if ((DBG_LEVEL > 128) && (dbuffer != NULL))
	{
	  dbuffer[(x + dpos * width) * 3] = 0xFF;
	  dbuffer[(x + dpos * width) * 3 + 1] = 0x00;
	  dbuffer[(x + dpos * width) * 3 + 2] = 0x00;
	}
    }
  if (ecnt == 0)
    epos = 70;
  else
    epos = epos / ecnt;
  if ((DBG_LEVEL > 128) && (dbuffer != NULL))
    {
      i = ((int) epos) * width;
      for (x = 0; x < width; x++)
	{
	  dbuffer[(x + i) * 3] = 0x00;
	  dbuffer[(x + i) * 3 + 1] = 0xFF;
	  dbuffer[(x + i) * 3 + 2] = 0xFF;
	}
      for (y = 0; y < height; y++)
	{
	  dbuffer[(width / 2 + y * width) * 3] = 0x00;
	  dbuffer[(width / 2 + y * width) * 3 + 1] = 0xFF;
	  dbuffer[(width / 2 + y * width) * 3 + 2] = 0x00;
	}
      DumpRGB (width, height, dbuffer, NULL);
      free (dbuffer);
    }
  return epos;
}



static int
moveToOrigin (void)
{
  unsigned char buffer[54000];
  float edge;
  int val, delta = 188;
  int header[17] = {
    0xB4, 0x00, 0x00, 0x70, 0x00, 0x00, 0x60, 0x2F,
    0x2F, 0x05, 0x00, 0x00, 0x00, 0x80, 0xA4, 0x00, -1
  };

  int body[37] = {
    0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0C,
    0x00, 0x04, 0x40, 0x01, 0x00, 0x00, 0x04, 0x00,
    0x6E, 0xFB, 0xC4, 0xE5, 0x06, 0x00, 0x00, 0x60,
    0x4D, 0xA0, 0x00, 0x8B, 0x49, 0x2A, 0xE9, 0x68,
    0xDF, 0x13, 0x1A, 0x00, -1
  };

  int end[9] = {
    0x06, 0xF4, 0xFF, 0x81, 0x1B, 0x00, 0x08, 0x00,
    -1
  };
  int opsc03[9] = {
    0x00, 0x00, 0x00, 0xAA, 0xCC, 0xEE, 0x80, 0xFF,
    -1
  };
  int w = 300, h = 180, len = 36;

  switch (sanei_umax_pp_getastra ())
    {
    case 1600:
      header[8] = 0x2B;

      body[29] = 0x1A;
      body[30] = 0xEE;

      end[0] = 0x19;
      end[1] = 0xD5;
      end[4] = 0x1B;

    case 1220:
    case 2000:
      w = 300;
      h = 180;
      len = 36;
      delta = -188;
      CMDSYNC (0x00);
      CMDSYNC (0xC2);
      CMDSYNC (0x00);
      MOVE (196, PRECISION_OFF, NULL);
      break;

    case 610:

      w = 512;
      h = 90;
      len = 34;
      delta = -81;

      opsc03[6] = 0xFF;		/* instead of 0x80 */

      encodeHY (h, 60, header);

      /* will add encodeDpi(dpi,cmd) */
      header[6] = 0xC0;
      header[8] = 0x17;

      body[13] = 0x20;
      body[14] = 0x02;

      body[16] = 0x76;

      encodeWX (0x200, 0x501, 300, 0, body, 0x500);

      /* fixed values for all 610p commands */
      body[28] = 0x4D;
      body[29] = 0x4B;
      body[30] = 0xD0;

      /* LM9811 command block ? */
      end[0] = 0x88;
      end[1] = 0xE6;
      end[2] = 0xFD;
      end[3] = 0x8E;
      end[4] = 0x30;

      break;
    }


  /* scan an area where is a white and a black regions, which  */
  /* can be detected and gives this offset to the origin of the */
  /* scanning windows                                          */
  CMDSETGET (2, 0x10, header);
  CMDSETGET (8, len, body);
  CMDSETGET (1, 0x08, end);

  CMDSYNC (0xC2);
  CMDSYNC (0x00);
  /* signals black & white ? */
  CMDSETGET (4, 8, opsc03);
  COMPLETIONWAIT;
  CMDGETBUF (4, w * h, buffer);	/* get find position data */
  if (DBG_LEVEL > 128)
    {
      DumpNB (w, h, buffer, NULL);
    }

  /* detection of 1600P is a by product of origin finding */
  edge = 0.0;
  for (val = 0; val < w * h; val++)
    if (buffer[val] > edge)
      edge = buffer[val];
  DBG (32, "MAX VALUE=%f        (%s:%d)\n", edge, __FILE__, __LINE__);
  if ((edge <= 30) && (sanei_umax_pp_getastra () != 1600))
    {
      DBG (2, "moveToOrigin() detected a 1600P");
      sanei_umax_pp_setastra (1600);
    }
  edge = edgePosition (w, h, buffer);
  /* rounded to lowest integer, since upping origin might lead */
  /* to bump in the other side if doing a full size preview    */
  val = (int) (edge);

  delta += val;
  DBG (64, "Edge=%f, val=%d, delta=%d\n", edge, val, delta);

  /* move back to y-coordinate origin */
  if (sanei_umax_pp_getastra () < 1220)
    {
      MOVE (delta, PRECISION_OFF, NULL);
    }
  else
    {
      MOVE (delta, PRECISION_ON, NULL);
    }

  /* head successfully set to the origin */
  return 1;
}


/* park head: returns 1 on success, 0 otherwise  */
int
sanei_umax_pp_park (void)
{
  int header610[17] = {
    0x01, 0x00, 0x01, 0x40, 0x30, 0x00, 0xC0, 0x2F, 0x17, 0x05, 0x00, 0x00,
    0x00, 0x80, 0xF4, 0x00, -1
  };
  int body610[35] = {
    0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0C, 0x00, 0x03, 0xC1, 0x80,
    0x00, 0x20, 0x02, 0x00, 0x16, 0x80, 0x15, 0x78, 0x03, 0x03, 0x00, 0x00,
    0x46, 0xA0, 0x00, 0x8B, 0x4D, 0x4B, 0xD0, 0x68, 0xDF, 0x1B, -1
  };

  int header[17] =
    { 0x01, 0x00, 0x01, 0x70, 0x00, 0x00, 0x60, 0x2F, 0x13, 0x05, 0x00, 0x00,
    0x00, 0x80, 0xF0, 0x00, -1
  };
  int body[37] =
    { 0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0C, 0x00, 0x03, 0xC1, 0x80,
    0x00, 0x00, 0x04, 0x00, 0x16, 0x80, 0x15, 0x78, 0x03, 0x03, 0x00, 0x00,
    0x46, 0xA0, 0x00, 0x8B, 0x49, 0x2A, 0xE9, 0x68, 0xDF, 0x1B, 0x1A, 0x00,
    -1
  };

  int status = 0x90;

  CMDSYNC (0x00);

  if (sanei_umax_pp_getastra () > 610)
    {
      CMDSETGET (0x02, 0x10, header);
      CMDSETGET (0x08, 0x24, body);
    }
  else
    {
      CMDSETGET (0x02, 0x10, header610);
      CMDSETGET (0x08, 0x22, body610);
    }
  CMDSYNC (0x40);


  status = sanei_umax_pp_scannerStatus ();
  DBG (16, "PARKING STATUS is 0x%02X (%s:%d)\n", status, __FILE__, __LINE__);
  DBG (1, "Park command issued ...\n");
  return 1;
}


/* calibrates CCD: returns 1 on success, 0 on failure */
static int
shadingCalibration1220p (int color,
			 int dcRed, int dcGreen, int dcBlue,
			 int vgaRed, int vgaGreen, int vgaBlue,
			 int *calibration)
{
  int opsc32[17] =
    { 0x4A, 0x00, 0x00, 0x70, 0x00, 0x00, 0x60, 0x00, 0x17, 0x05, 0xA5, 0x08,
    0x00, 0x00, 0xAC, 0x00, -1
  };
  int opsc41[37] =
    { 0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0C, 0x00, 0x04, 0x40, 0x01,
    0x00, 0x00, 0x04, 0x00, 0x6E, 0x90, 0xD0, 0x47, 0x06, 0x00, 0x00, 0xC4,
    0x5C, 0xA0, 0x00, 0x8B, 0x49, 0x2A, 0xE9, 0x68, 0xDF, 0x93, 0x1B, 0x00,
    -1
  };
  int opscnb[37] =
    { 0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0C, 0x00, 0x04, 0x40, 0x01,
    0x00, 0x00, 0x04, 0x00, 0x6E, 0x90, 0xD0, 0x47, 0x06, 0x00, 0x00, 0xEC,
    0x54, 0xA0, 0x00, 0x8B, 0x49, 0x2A, 0xE9, 0x68, 0xDF, 0x93, 0x1A, 0x00,
    -1
  };
  int opsc04[9] = { 0x06, 0xF4, 0xFF, 0x81, 0x1B, 0x00, 0x00, 0x00, -1 };
  int commit[9] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, -1 };
  int size;
  int width = 5100;		/* full usable CCD width */
  unsigned char buffer[0x105798];

  /* 1600P have a different CCD command block */
  if (sanei_umax_pp_getastra () == 1600)
    {
      opsc04[0] = 0x19;
      opsc04[1] = 0xD5;
      opsc04[4] = 0x1B;

      opsc41[29] = 0x1A;
      opsc41[30] = 0xEE;
    }

  /* step back by 67 ticks:                     */
  /* since we're going to scan 66 lines of data */
  /* which are going to be used as calibration  */
  /* data                                       */
  /* we are on the white area just before       */
  /* the user scan area                         */
  MOVE (-67, PRECISION_ON, NULL);


  CMDSYNC (0x00);

  /* get calibration data */
  /*
     if (sanei_umax_pp_getauto ())
     {                           auto settings doesn't use offset 
     offset = 0x000;
     }
     else
     {                           manual settings 
     gain = 0x777;
     offset = 0x000;
     }
   */
  encodeDC (dcRed, dcGreen, dcBlue, opsc32);
  encodeVGA (vgaRed, vgaGreen, vgaBlue, opsc32);
  if (sanei_umax_pp_getastra () == 1600)
    {
      opsc32[13] = 0x03;
    }

  /* 1220P/2000P shading calibration */
  if (color < RGB_MODE)
    {
      opsc32[0] -= 4;
      opsc32[13] = 0xC0;
    }
  CMDSETGET (2, 0x10, opsc32);
  if (DBG_LEVEL >= 64)
    {
      bloc2Decode (opsc32);
    }
  if (color < RGB_MODE)
    {
      CMDSETGET (8, 0x24, opscnb);
      if (DBG_LEVEL >= 64)
	{
	  bloc8Decode (opscnb);
	}
      opsc04[6] = 0x85;
    }
  else
    {
      CMDSETGET (8, 0x24, opsc41);
      if (DBG_LEVEL >= 64)
	{
	  bloc8Decode (opsc41);
	}
      opsc04[6] = 0x0F;
      if (sanei_umax_pp_getastra () == 1600)
	opsc04[7] = 0xC0;
      else
	opsc04[7] = 0x70;
    }

  /* BUG BW noisy here */
  CMDSETGET (1, 0x08, opsc04);
  CMDSYNC (0xC2);
  CMDSYNC (0x00);
  CMDSETGET (4, 0x08, commit);	/* opsc03 hangs it */
  COMPLETIONWAIT;

  opsc04[0] = 0x06;
  if (color >= RGB_MODE)
    size = 3 * width * 70;
  else
    size = width * 66;
  if (getEPPMode () == 32)
    {
      cmdGetBuffer32 (4, size, buffer);
    }
  else
    {
      CMDGETBUF (4, size, buffer);
    }
  if (DBG_LEVEL >= 128)
    {
      Dump (size, buffer, NULL);
      if (color >= RGB_MODE)
	{
	  DumpRVB (width, 66, buffer, NULL);
	}
      else
	{
	  DumpNB (width, 66, buffer, NULL);
	}
    }
  computeCalibrationData (color, width, buffer, calibration);

  DBG (1, "shadingCalibration1220p() done ...\n");
  return 1;
}


/* returns number of bytes read or 0 on failure */
int
sanei_umax_pp_readBlock (long len, int window, int dpi, int last,
			 unsigned char *buffer)
{
  DBG (8, "readBlock(%ld,%d,%d,%d)\n", len, window, dpi, last);
  /* EPP block reading is available only when dpi >=600 */
  if ((dpi >= 600)
      && (gMode != UMAX_PP_PARPORT_ECP) && (sanei_umax_pp_getastra () > 610))
    {
      DBG (8, "cmdGetBlockBuffer(4,%ld,%d);\n", len, window);
      len = cmdGetBlockBuffer (4, len, window, buffer);
      if (len == 0)
	{
	  DBG (0, "cmdGetBlockBuffer(4,%ld,%d) failed (%s:%d)\n", len, window,
	       __FILE__, __LINE__);
	  gCancel = 1;
	}
    }
  else
    {
      if ((sanei_umax_pp_getastra () < 1210) && (len > 0xFDCE))
	{
	  len = 0xFDCE;
	  last = 0;
	}
      DBG (8, "cmdGetBuffer(4,%ld);\n", len);
      if (cmdGetBuffer (4, len, buffer) != 1)
	{
	  DBG (0, "cmdGetBuffer(4,%ld) failed (%s:%d)\n", len, __FILE__,
	       __LINE__);
	  gCancel = 1;
	}
    }
  if (!last)
    {
      /* sync with scanner */
      if (sanei_umax_pp_cmdSync (0xC2) == 0)
	{
	  DBG (0, "Warning cmdSync(0xC2) failed! (%s:%d)\n", __FILE__,
	       __LINE__);
	  DBG (0, "Trying again ...\n");
	  if (sanei_umax_pp_cmdSync (0xC2) == 0)
	    {
	      DBG (0, " failed again! (%s:%d)\n", __FILE__, __LINE__);
	      DBG (0, "Aborting ...\n");
	      gCancel = 1;
	    }
	  else
	    DBG (0, " success ...\n");
	}
    }
  return len;
}

int
sanei_umax_pp_scan (int x, int y, int width, int height, int dpi, int color,
		    int gain, int offset)
{
#ifdef HAVE_SYS_TIME_H
  struct timeval td, tf;
  float elapsed;
#endif
  unsigned char *buffer;
  long int somme, len, read, blocksize;
  FILE *fout = NULL;
  int *dest = NULL;
  int bpl, hp;
  int th, tw, bpp;
  int nb;
  int bx, by, delta;
  int reserve, rc, remain, dataoffset;

  if (gain != 0 || offset != 0)
    sanei_umax_pp_setauto (0);


  /* colors don't come in sync, so we must increase y */
  /* to have extra lines to reorder datas             */
  if (sanei_umax_pp_getastra () > 610)
    {
      switch (dpi)
	{
	case 1200:
	  delta = 8;
	  break;
	case 600:
	  delta = 4;
	  break;
	case 300:
	  delta = 2;
	  break;
	case 150:
	  delta = 1;
	  break;
	default:
	  delta = 0;
	  break;
	}
    }
  else
    {
      if (color >= RGB_MODE)
	{
	  switch (dpi)
	    {
	    case 600:
	      delta = 16;
	      break;
	    case 300:
	      delta = 8;
	      break;
	    case 150:
	      delta = 4;
	      break;
	    default:
	      delta = 2;
	      break;
	    }
	}
      else
	delta = 0;
    }

  /* in color mode, we need extra lines to reorder data */
  if (color >= RGB_MODE)
    {
      if (sanei_umax_pp_getastra () <= 610)
	dataoffset = 4 * delta;
      else
	dataoffset = 2 * delta;
    }
  else
    dataoffset = 0;

  rc = sanei_umax_pp_startScan
    (x, y - dataoffset, width, height + dataoffset, dpi, color, gain,
     offset, &bpp, &tw, &th);
  if (rc == 1)
    {
      /* blocksize must be multiple of the number of bytes per line */
      /* max is 2096100                                             */
      /* so blocksize will hold a round number of lines, easing the */
      /* write data to file operation                               */
      bpl = bpp * tw;
      hp = 2096100 / bpl;
      blocksize = hp * bpl;
      nb = 0;
      somme = bpl * th;
      DBG (8, "Getting buffer %d*%d*%d=%ld=0x%lX    (%s:%d)  \n", bpp, tw, th,
	   somme, somme, __FILE__, __LINE__);

      /* correct th to be usable scan height */
      th -= dataoffset;

      /* we need a 2 * delta lines reserve to reorder data */
      if (color >= RGB_MODE)
	{
	  reserve = 2 * delta * bpl;
	  if (sanei_umax_pp_getastra () < 1210)
	    dataoffset = reserve;
	  else
	    dataoffset = 0;
	}
      else
	{
	  reserve = 0;
	  dataoffset = 0;
	}

      /* get scanned data */

      /* allocate memory */
      buffer = (unsigned char *) malloc (blocksize + reserve);
      if (buffer == NULL)
	{
	  DBG (0, "Failed to allocate %ld bytes, giving up....\n",
	       blocksize + reserve);
	  DBG (0, "Try to scan at lower resolution, or a smaller area.\n");
	  gCancel = 1;
	}

      /* open output file */
      fout = fopen ("out.pnm", "wb");
      if (fout == NULL)
	{
	  DBG (0, "Failed to open 'out.pnm', giving up....\n");
	  gCancel = 1;
	}
      else
	{
	  /* write pnm header */
	  if (color >= RGB_MODE)
	    fprintf (fout, "P6\n%d %d\n255\n", tw, th - 2 * delta);
	  else
	    fprintf (fout, "P5\n%d %d\n255\n", tw, th);
	}

      /* read some line first until we got clean data */
      read = 0;
      remain = 0;
      while (read < dataoffset)
	{
	  if (read == 0)
	    len = dataoffset;
	  else
	    len = dataoffset - read;
	  len = sanei_umax_pp_readBlock (len, tw, dpi, 0, buffer + read);
	  if (len == 0)
	    {
	      DBG (0,
		   "sanei_umax_pp_readBlock failed, cancelling scan ...\n");
	      gCancel = 1;
	    }
	  read += len;
	}

      /* in color mode we have to fill the 'reserve' area 
       * so that we can reorder data lines */
      while ((read - dataoffset < reserve) && (!gCancel))
	{
	  len = reserve - read + dataoffset;
	  len =
	    sanei_umax_pp_readBlock (len, tw, dpi, 0,
				     buffer + read - dataoffset);
	  if (len == 0)
	    {
	      DBG (0,
		   "sanei_umax_pp_readBlock failed, cancelling scan ...\n");
	      gCancel = 1;
	    }
	  read += len;
	}

      /* data reading loop */
#ifdef HAVE_SYS_TIME_H
      gettimeofday (&td, NULL);
#endif
      while ((read < somme) && (!gCancel))
	{
	  /* 2096100 max */
	  if (somme - read > blocksize - remain)
	    len = blocksize - remain;
	  else
	    len = somme - read;
	  len =
	    sanei_umax_pp_readBlock (len, tw, dpi, (len < blocksize),
				     buffer + reserve + remain);
	  if (len == 0)
	    {
	      DBG (0,
		   "sanei_umax_pp_readBlock failed, cancelling scan ...\n");
	      gCancel = 1;
	    }

	  read += len;
	  nb++;
	  DBG (8, "Read %ld bytes out of %ld ...\n", read, somme);
	  DBG (8, "Read %d blocks ... \n", nb);

	  /* write partial buffer to file */
	  if (len)
	    {
	      if (color >= RGB_MODE)
		{
		  /* using an image format that doesn't need   */
		  /* reordering would speed up write operation */

		  /* don't forget remaining bytes from previous block */
		  hp = (len + remain) / bpl;
		  remain = (len + remain) - hp * bpl;
		  switch (sanei_umax_pp_getastra ())
		    {
		    case 610:
		      /* first comes RED
		       * then        BLUE
		       * and finally GREEN */
		      for (by = 0; by < hp; by++)
			{
			  for (bx = 0; bx < tw; bx++)
			    {
			      /* scanner data: red line, blue line then green line */
			      /* red */
			      fputc (buffer
				     [3 * (by - 2 * delta) * tw + bx +
				      reserve], fout);
			      /* green */
			      fputc (buffer
				     [3 * by * tw + 2 * tw +
				      bx + reserve], fout);
			      /* blue */
			      fputc (buffer
				     [3 * (by - delta) * tw + tw + bx +
				      reserve], fout);
			    }
			}
		      /* copy tail lines for next block */
		      /* memcpy (buffer, 
		       *         (buffer + reserve) + (hp * bpl - reserve), 
		       *         reserve + remain); */
		      memcpy (buffer, buffer + hp * bpl, reserve + remain);
		      break;
		    case 1600:
		      for (by = 0; by < hp; by++)
			{
			  for (bx = 0; bx < tw; bx++)
			    {
			      fputc (buffer[3 * by * tw + 2 * tw + bx], fout);
			      fputc (buffer[3 * by * tw + bx], fout);
			      fputc (buffer[3 * by * tw + tw + bx], fout);
			    }
			}
		      break;
		    default:
		      for (by = 0; by < hp; by++)
			{
			  for (bx = 0; bx < tw; bx++)
			    {
			      fputc (buffer[3 * by * tw + 2 * tw + bx], fout);
			      fputc (buffer[3 * by * tw + tw + bx], fout);
			      fputc (buffer[3 * by * tw + bx], fout);
			    }
			}
		      /* put remaining partial lines at start of buffer */
		      memcpy (buffer, buffer + hp * bpl, remain);
		      break;
		    }
		}
	      else
		fwrite (buffer, len, 1, fout);
	    }
	}

#ifdef HAVE_SYS_TIME_H
      gettimeofday (&tf, NULL);

      /* scan time are high enough to forget about usec */
      elapsed = tf.tv_sec - td.tv_sec;
      DBG (8, "%ld bytes transferred in %f seconds ( %.2f Kb/s)\n", somme,
	   elapsed, (somme / elapsed) / 1024.0);
#endif

      /* release ressources */
      if (fout != NULL)
	fclose (fout);
      free (dest);
      free (buffer);
    }				/* if start scan OK */
  else
    {
      DBG (0, "startScan failed..... \n");
    }

  /* terminate any pending command */
  if (sanei_umax_pp_cmdSync (0x00) == 0)
    {
      DBG (0, "Warning cmdSync(0x00) failed! (%s:%d)\n", __FILE__, __LINE__);
      DBG (0, "Trying again ... ");
      if (sanei_umax_pp_cmdSync (0x00) == 0)
	{
	  DBG (0, " failed again! (%s:%d)\n", __FILE__, __LINE__);
	  DBG (0, "Blindly going on ...\n");
	}
      else
	DBG (0, " success ...\n");

    }

  /* parking */
  if (sanei_umax_pp_park () == 0)
    DBG (0, "Park failed !!! (%s:%d)\n", __FILE__, __LINE__);


  /* end ... */
  DBG (16, "Scan done ...\n");
  return 1;
}





/*
 * returns 0 on error, 1 on success: ie head parked
 */
int
sanei_umax_pp_parkWait (void)
{
  int status;

  /* check if head is at home */
  DBG (16, "entering parkWait ...\n");
  do
    {
      usleep (1000);
      CMDSYNC (0x40);
      status = sanei_umax_pp_scannerStatus ();
    }
  while ((status & MOTOR_BIT) == 0x00);
  DBG (16, "parkWait done ...\n");
  return 1;
}






/* starts scan: return 1 on success */
int
sanei_umax_pp_startScan (int x, int y, int width, int height, int dpi,
			 int color, int gain, int offset, int *rbpp,
			 int *rtw, int *rth)
{
  unsigned char *buffer;
  int *dest = NULL;
  int rc = 0;
  int calibration[3 * 5100 + 768 + 2 + 1];
  int xdpi, ydpi, h;
  int th, tw, bpp;
  int hwdpi = 600;		/* CCD hardware dpi */
  /* DC offsets */
  int dcRed, dcGreen, dcBlue;
  int vgaRed, vgaGreen, vgaBlue;
  int len, delta;

  int lm9811[9] = {
    0x06, 0xF4, 0xFF, 0x81, 0x1B, 0x00, 0x00, 0x00,
    -1
  };

  int motor[17] = {
    0xA4, 0x80, 0x07, 0x50, 0xEC, 0x03, 0x00, 0x2F,
    0x17, 0x07, 0x84, 0x08, 0x90, 0x00, 0xAC, 0x00,
    -1
  };

  int ccd[37] =
    { 0x00, 0x00, 0xB0, 0x4F, 0xD8, 0xE7, 0xFA, 0x10, 0xEF, 0xC4, 0x3C, 0x71,
    0x0F, 0x00, 0x04, 0x00, 0x6E, 0x61, 0xA1, 0x24, 0xC4, 0x7E, 0x00, 0xAE,
    0x41, 0xA0, 0x0A, 0x8B, 0x49, 0x2A, 0xE9, 0x68, 0xDF, 0x33, 0x1A, 0x00,
    -1
  };


#ifdef UMAX_PP_DANGEROUS_EXPERIMENT
  FILE *f = NULL;
  char line[1024], *ptr;
  int *base = NULL;
  int channel;
  int max = 0;
#endif


  if (sanei_umax_pp_getastra () == 610)
    {
      hwdpi = 300;
      len = 0x22;

      lm9811[0] = 0x88;
      lm9811[1] = 0xE6;
      lm9811[2] = 0xFD;
      lm9811[3] = 0x8E;
      lm9811[4] = 0x30;
      lm9811[5] = 0x00;
      lm9811[6] = 0x8F;
      lm9811[7] = 0x80;

      motor[3] = 0x30;
      motor[4] = 0x0E;
      motor[5] = 0x02;
      motor[12] = 0xAA;

      ccd[0] = 0x00;
      ccd[1] = 0x00;
      ccd[2] = 0xD8;
      ccd[3] = 0x27;
      ccd[4] = 0xEC;
      ccd[5] = 0x53;
      ccd[6] = 0x7D;
      ccd[7] = 0x8A;
      ccd[8] = 0x77;
      ccd[9] = 0xE2;
      ccd[10] = 0x9E;
      ccd[11] = 0xF8;
      ccd[12] = 0x07;
      ccd[13] = 0x20;
      ccd[14] = 0x02;
      ccd[15] = 0x00;
      ccd[16] = 0x76;
      ccd[17] = 0x5D;
      ccd[18] = 0xE0;
      ccd[19] = 0x13;
      ccd[20] = 0xE2;
      ccd[21] = 0x20;
      ccd[22] = 0x00;
      ccd[23] = 0xA8;
      ccd[24] = 0x41;
      ccd[25] = 0xA0;
      ccd[26] = 0x0A;
      ccd[27] = 0x8B;
      ccd[28] = 0x4D;
      ccd[29] = 0x4B;
      ccd[30] = 0xD0;
      ccd[31] = 0x68;
      ccd[32] = 0xDF;
      ccd[33] = 0x13;
    }
  else
    {
      len = 0x24;
      hwdpi = 600;
    }
  DBG (8, "startScan(%d,%d,%d,%d,%d,%d,%X);\n", x, y, width, height, dpi,
       color, gain);
  buffer = (unsigned char *) malloc (2096100);
  if (buffer == NULL)
    {
      DBG (0, "Failed to allocate 2096100 bytes... (%s:%d)\n", __FILE__,
	   __LINE__);
      return 0;
    }

  /* 1600P have a different CCD command block */
  if (sanei_umax_pp_getastra () == 1600)
    {
      lm9811[0] = 0x19;
      lm9811[1] = 0xD5;
      lm9811[4] = 0x1B;
      lm9811[7] = 0x70;

      ccd[29] = 0x1A;
      ccd[30] = 0xEE;

      motor[13] = 0x03;		/* may be blur filter */
    }

  /* matches intTransport610P */
  /* get scanner status */
  rc = inquire ();
  if (rc == 0)
    {
      DBG (0, "inquire() failed ! (%s:%d) \n", __FILE__, __LINE__);
      return 0;
    }
  DBG (16, "inquire() passed ... (%s:%d)\n", __FILE__, __LINE__);

  dest = (int *) malloc (65536 * sizeof (int));

  rc = loadDefaultTables ();
  if (rc == 0)
    {
      DBG (0, "loadDefaultTables() failed ! (%s:%d) \n", __FILE__, __LINE__);
      return 0;
    }
  DBG (16, "loadDefaultTables() passed ... (%s:%d)\n", __FILE__, __LINE__);

  /* find and move to zero */
  if (moveToOrigin () == 0)
    {
      DBG (0, "moveToOrigin() failed ... (%s:%d)\n", __FILE__, __LINE__);
    }
  else
    {
      DBG (16, "moveToOrigin() passed ... (%s:%d)\n", __FILE__, __LINE__);
    }

  /* 1600P have a different CCD command block */
  if (sanei_umax_pp_getastra () == 1600)
    {
      lm9811[0] = 0x19;
      lm9811[1] = 0xD5;
      lm9811[4] = 0x1B;
      lm9811[7] = 0x70;

      ccd[29] = 0x1A;
      ccd[30] = 0xEE;

      motor[13] = 0x03;		/* may be blur filter */
    }

  /* XXX STEF XXX : done even is manual settings, some day skip it
   * and move head the right amount */
  if (offsetCalibration (color, &dcRed, &dcGreen, &dcBlue) == 0)
    {
      DBG (0, "offsetCalibration failed !!! (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }
  DBG (16, "offsetCalibration(%d=>%d,%d,%d) passed ... (%s:%d)\n",
       color, dcRed, dcGreen, dcBlue, __FILE__, __LINE__);

  if (coarseGainCalibration
      (color, dcRed, dcGreen, dcBlue, &vgaRed, &vgaGreen, &vgaBlue) == 0)
    {
      DBG (0, "coarseGainCalibration failed !!! (%s:%d)\n", __FILE__,
	   __LINE__);
      return 0;
    }
  DBG (16,
       "coarseGainCalibration(%d,%d,%d,%d=>%d,%d,%d) passed ... (%s:%d)\n",
       color, dcRed, dcGreen, dcBlue, vgaRed, vgaGreen, vgaBlue,
       __FILE__, __LINE__);

  /* manual setting overrides evaluated values */
  if (!sanei_umax_pp_getauto ())
    {
      dcRed = (offset & 0xF00) >> 8;
      dcGreen = (offset & 0x0F0) >> 4;
      dcBlue = offset & 0x00F;
      vgaRed = (gain & 0xF00) >> 8;
      vgaGreen = (gain & 0x0F0) >> 4;
      vgaBlue = gain & 0x00F;
    }

  /* ccd calibration is allways done */
  /* with final dc and vga */
  if (shadingCalibration
      (color, dcRed, dcGreen, dcBlue, vgaRed, vgaGreen, vgaBlue,
       calibration) == 0)
    {
      DBG (0, "shadingCalibration failed !!! (%s:%d)\n", __FILE__, __LINE__);
      return 0;
    }
  DBG (16,
       "shadingCalibration(%d,%d,%d,%d,%d,%d,%d) passed ... (%s:%d)\n",
       color, dcRed, dcGreen, dcBlue, vgaRed, vgaGreen, vgaBlue, __FILE__,
       __LINE__);

  /* gamma or left shading calibration ? */
  if (sanei_umax_pp_getastra () <= 610)
    {
      if (leftShadingCalibration610p
	  (color, dcRed, dcGreen, dcBlue, vgaRed, vgaGreen, vgaBlue,
	   calibration) == 0)
	{
	  DBG (0, "leftShadingCalibration610p failed !!! (%s:%d)\n", __FILE__,
	       __LINE__);
	  return 0;
	}
      DBG (16,
	   "leftShadingCalibration610p(%d,%d,%d,%d,%d,%d,%d) passed ... (%s:%d)\n",
	   color, dcRed, dcGreen, dcBlue, vgaRed, vgaGreen, vgaBlue, __FILE__,
	   __LINE__);
    }

  /* 1220P: x dpi is from 75 to 600 max, any modes */
  /*  610P: x dpi is from 75 to 300 max, any modes */
  if (dpi > hwdpi)
    xdpi = hwdpi;
  else
    xdpi = dpi;


  /* EPPRead32Buffer does not work                         */
  /* with length not multiple of four bytes, so we enlarge */
  /* width to meet this criteria ...                       */
  if ((getEPPMode () == 32) && (xdpi >= 600) && (width & 0x03)
      && (sanei_umax_pp_getastra () > 610))
    {
      width += (4 - (width & 0x03));
      /* in case we go too far on the right */
      if (x + width > 5100)
	{
	  x = 5100 - width;
	}
    }

  /* compute target size */
  th = (height * dpi) / hwdpi;
  tw = (width * xdpi) / hwdpi;

  /* corrects y to match exact scan area start 
   * and lets room for a leading zone so that 
   * we can reorder data */
  switch (sanei_umax_pp_getastra ())
    {
    case 610:
      if (color >= RGB_MODE)
	switch (dpi)
	  {
	  case 600:
	    y += 64;
	    break;
	  case 300:
	    y += 32;
	    break;
	  case 150:
	    y += 16;
	    break;
	  case 75:
	    y += 8;
	    break;
	  }
      else
	y += 80;
    default:
      y += 8;
      break;
    }

  /* for 1220P/2000P move fast to scan target if possible */
  /* it is faster to move at low resolution, then scan */
  /* than scan & move at high resolution               */
  if ((sanei_umax_pp_getastra () > 610 && (dpi > 300)) && (y > 100))
    {
      /* move at 150 dpi resolution */
      move (y / 2, PRECISION_OFF, NULL);

      /* keep the remainder for scan */
      y = y % 4;
    }

  /* build final scan command */

  /* round width and height */
  width = (tw * hwdpi) / xdpi;
  height = (th * hwdpi) / dpi;

  ydpi = dpi;
  if (ydpi < 300)
    {
      if ((color >= RGB_MODE) && (sanei_umax_pp_getastra () > 610))
	{
	  if (dpi < 150)
	    ydpi = 150;
	}
      else
	ydpi = 300;
    }
  if ((color < RGB_MODE) && (sanei_umax_pp_getastra () <= 610))
    ydpi = 600;

  /* at maximum resolution                      */
  if (color >= RGB_MODE)
    {
      h = ((height * ydpi) / hwdpi) + 8;
      bpp = 3;
    }
  else
    {
      h = ((height * ydpi) / hwdpi) + 4;
      if (sanei_umax_pp_getastra () <= 610)
	h += 16;
      bpp = 1;
    }

  /* sets y resolution */
  switch (ydpi)
    {
    case 1200:
      motor[6] = 0x60;
      motor[8] = 0x5E;		/* *WORKING* value */
      motor[8] = 0x5F;		/* 5F gives wrong colors ? */
      motor[8] = 0x58;
      motor[9] = 0x05;
      /* XXX test value XXX motor[14] = motor[14] & 0xF0;  ~ 0x08 -> scan AND move */
      /* XXX test value XXX motor[14] = (motor[14] & 0xF0) | 0x04;         -> 600 dpi ? */
      /* XXX test value XXX motor[14] = (motor[14] & 0xF0) | 0x0C; */
      motor[14] = motor[14] & 0xF0;	/* *WORKING* 1200 dpi */
      break;

    case 600:
      if (sanei_umax_pp_getastra () <= 610)
	{
	  motor[6] = 0xC0;
	  motor[7] = 0x2F;
	  motor[14] = motor[14] & 0xF0;
	  /* if (color >= RGB_MODE)
	     motor[14] |= 0x04; XXX STEF XXX */
	}
      else
	{
	  motor[6] = 0x60;
	  motor[14] = (motor[14] & 0xF0) | 0x04;
	}
      motor[8] = 0x2F;
      motor[9] = 0x05;
      break;

    case 300:
      if (sanei_umax_pp_getastra () <= 610)
	{
	  motor[6] = 0xC0;
	  motor[14] = motor[14] & 0xF0;
	  if (color >= RGB_MODE)
	    motor[14] |= 0x04;
	}
      else
	{
	  motor[6] = 0x00;
	  motor[14] = (motor[14] & 0xF0) | 0x0C;
	}
      motor[8] = 0x17;
      motor[9] = 0x05;

      /* si | 0C h=2*w, si | 04 h=w ? */

      break;

    case 150:
      if (sanei_umax_pp_getastra () <= 610)
	{
	  motor[6] = 0xC0;
	  motor[9] = 0x05;
	  motor[14] = motor[14] & 0xF0;
	  if (color >= RGB_MODE)
	    motor[14] |= 0x04;
	}
      else
	{
	  motor[6] = 0x00;
	  motor[9] = 0x07;
	  motor[14] = (motor[14] & 0xF0) | 0x0C;
	}
      motor[8] = 0x17;
      break;
    }

  /* different values for 610P in B&W */
  if ((sanei_umax_pp_getastra () <= 610) && (color < RGB_MODE))
    {
      motor[7] = 0xC8;
      motor[8] = 0x2F;
      motor[9] = 0x05;
    }

  /* y start -1 */
  y = (y * ydpi) / hwdpi;

  if (color >= RGB_MODE)
    {
      /* 00 seems to give better results ?     */
      /* 80 some more gain, lamp power level ? */
      /* 8x does not make much difference      */
      lm9811[6] = 0x8F;
      switch (sanei_umax_pp_getastra ())
	{
	case 610:
	  lm9811[7] = 0x80;
	  motor[13] = 0x20;
	  break;
	case 1600:
	  lm9811[7] = 0x70;
	  motor[13] = 0x03;
	  break;
	default:
	  lm9811[7] = 0xF0;
	  motor[13] = 0x09;
	}
    }
  else
    {
      motor[13] = 0xC0;
      lm9811[6] = 0x80 | vgaGreen;
      switch (sanei_umax_pp_getastra ())
	{
	case 610:
	  lm9811[7] = 0xA0;
	  lm9811[6] = lm9811[6] | 0x40;
	  motor[13] = 0x6F;
	  break;
	case 1600:
	  lm9811[7] = 0x20;
	  motor[13] = 0xC3;
	  break;
	default:
	  lm9811[7] = 0xA0;
	  motor[13] = 0xC9;
	}
    }

  encodeCoefficient (color, dpi, calibration);
  encodeWX (width, x, dpi, color, ccd, 0);
  encodeHY (h, y, motor);
  encodeDC (dcRed, dcGreen, dcBlue, motor);
  encodeVGA (vgaRed, vgaGreen, vgaBlue, motor);

#ifdef UMAX_PP_DANGEROUS_EXPERIMENT
  /*motor[13] = 0x80;           B&W bit */
  /*motor[13] = 0x40;           green bit */
  /*motor[13] = 0x20;           red bit */
  /*motor[13] = 0x10;           blue bit */
  /* with cmd 01, may be use to do 3 pass scanning ? */
  /* bits 0 to 3 seem related to sharpness */
  f = fopen ("/tmp/dangerous.params", "rb");
  if (f != NULL)
    {
      fgets (line, 1024, f);
      while (!feof (f))
	{
	  channel = 0;
	  if (sscanf (line, "CMD%1d", &channel) != 1)
	    channel = 0;
	  switch (channel)
	    {
	    case 0:
	      break;
	    case 1:
	      base = lm9811;
	      max = 8;
	      break;
	    case 2:
	      base = motor;
	      max = 16;
	      break;
	    case 8:
	      base = ccd;
	      max = 36;
	      break;
	    default:
	      channel = 0;
	    }
	  printf ("CMD%d BEFORE: ", channel);
	  for (i = 0; i < max; i++)
	    printf ("%02X ", base[i]);
	  printf ("\n");
	  if (channel > 0)
	    {
	      ptr = line + 6;
	      for (i = 0; (i < max) && ((ptr - line) < strlen (line)); i++)
		{
		  if (ptr[0] != '-')
		    {
		      sscanf (ptr, "%X", base + i);
		    }
		  ptr += 3;
		}
	    }
	  printf ("CMD%d AFTER : ", channel);
	  for (i = 0; i < max; i++)
	    printf ("%02X ", base[i]);
	  printf ("\n");
	  fgets (line, 1024, f);
	}
      fclose (f);
    }
#endif

  if (DBG_LEVEL >= 64)
    {
      bloc2Decode (motor);
      bloc8Decode (ccd);
    }

  CMDSYNC (0x00);
  CMDSETGET (2, 0x10, motor);
  CMDSETGET (8, len, ccd);
  CMDSETGET (1, 0x08, lm9811);
  CMDSYNC (0xC2);

  /* 3 ccd lines + 3 gamma tables + end tag */
  if (sanei_umax_pp_getastra () <= 610)
    {
      /* XXX STEF XXX : there is a 4 pixels shift to the right 
       * the first shading correction value applies to the forth
       * pixel of scan (at 300 dpi), we allready shift to the left
       * when doing shadingCalibration, but now we have to move coeffs
       * to match x coordinate */
      delta = x - sanei_umax_pp_getLeft ();
      if (delta)
	{
	  memcpy (calibration,
		  calibration + delta, (7650 - delta) * sizeof (int));
	}
      CMDSET (4, 0x20E4, calibration);
    }
  else
    {
      CMDSET (4, 0x3EC6, calibration);
    }

  COMPLETIONWAIT;

  *rbpp = bpp;
  *rtw = tw;
  *rth = th;

  free (buffer);
  return 1;
}

/* 
 * check the scanner model. Return 1220 for
 * a 1220P, or 2000 for a 2000P.
 * and 610 for a 610p
 * values less than 610 are errors
 */
int
sanei_umax_pp_checkModel (void)
{
  int *dest = NULL;
  int state[16];
  int err = 0;
  int i;

  int opsc35[37] =
    { 0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0C, 0x00, 0x03, 0xC1, 0x80,
    0x00, 0x00, 0x04, 0x00, 0x16, 0x41, 0xE0, 0xAC, 0x03, 0x03, 0x00, 0x00,
    0x46, 0xA0, 0x00, 0x8B, 0x49, 0x2A, 0xE9, 0x68, 0xDF, 0x13, 0x1A, 0x00,
    -1
  };

  /* if we have already detected a scanner different from */
  /* default type, no need to check again                 */
  if (sanei_umax_pp_getastra ())
    return sanei_umax_pp_getastra ();

  /* get scanner status */
  CMDGET (0x02, 16, state);
  CMDSETGET (0x08, 36, opsc35);
  CMDSYNC (0xC2);

  dest = (int *) malloc (65536 * sizeof (int));
  if (dest == NULL)
    {
      DBG (0, "%s:%d failed to allocate 256 Ko !\n", __FILE__, __LINE__);
      return 0;
    }

  /* init some buffer : default calibration data ? */
  dest[0] = 0x00;
  dest[1] = 0x00;
  dest[2] = 0x00;
  for (i = 0; i < 768; i++)
    dest[i + 3] = i % 256;
  dest[768 + 3] = 0xAA;
  dest[768 + 4] = 0xAA;
  dest[768 + 5] = -1;
  CMDSETGET (0x04, 768 + 5, dest);


  /* check buffer returned */
  for (i = 0; i < 768; i++)
    {
      if (dest[i + 3] != (i % 256))
	{
	  DBG
	    (0,
	     "Error data altered: byte %d=0x%02X, should be 0x%02X !    (%s:%d)\n",
	     i, dest[i + 3], i % 256, __FILE__, __LINE__);
	  err = 1;
	}
    }
  if (err)
    return 0;


  /* new part of buffer ... */
  for (i = 0; i < 256; i++)
    {
      dest[i * 2] = i;
      dest[i * 2 + 1] = 0x00;
    }
  CMDSETGET (0x08, 36, opsc35);
  CMDSYNC (0xC2);
  CMDSET (0x04, 512, dest);

  /* another new part ... */
  for (i = 0; i < 256; i++)
    {
      dest[i * 2] = i;
      dest[i * 2 + 1] = 0x04;	/* instead of 0x00 */
    }
  opsc35[2] = 0x06;		/* instead of 0x04, write flag ? */
  CMDSETGET (0x08, 36, opsc35);
  CMDSYNC (0xC2);
  CMDSET (0x04, 512, dest);

  opsc35[2] = 0x04;		/* return to initial value, read flag? */
  CMDSETGET (0x08, 36, opsc35);
  CMDGET (0x04, 512, dest);

  /* check buffer returned */
  /* if 0x4 are still 0x04, we got a 1220P, else it is a 2000P */
  for (i = 0; i < 256; i++)
    {
      if ((dest[2 * i] != i)
	  || ((dest[2 * i + 1] != 0x04) && (dest[2 * i + 1] != 0x00)))
	{
	  DBG
	    (0,
	     "Error data altered: expected %d=(0x%02X,0x04), found (0x%02X,0x%02X) !    (%s:%d)\n",
	     i, i, dest[i * 2], dest[i * 2 + 1], __FILE__, __LINE__);
	  err = 0;
	}
    }

  /* if buffer unchanged, we have a 1600P, or a 1220P */
  /* if data has turned into 0, we have a 2000P       */
  if (dest[1] == 0x00)
    {
      sanei_umax_pp_setastra (2000);
      err = 2000;
    }
  else
    {
      /* detects 1600  by finding black scans */
      /* we defaults to 1220 */
      sanei_umax_pp_setastra (1220);
      moveToOrigin ();
      err = sanei_umax_pp_getastra ();

      /* parking */
      CMDSYNC (0xC2);
      CMDSYNC (0x00);
      if (sanei_umax_pp_park () == 0)
	DBG (0, "Park failed !!! (%s:%d)\n", __FILE__, __LINE__);

      /* poll parking */
      do
	{
	  sleep (1);
	  CMDSYNC (0x40);
	}
      while ((sanei_umax_pp_scannerStatus () & MOTOR_BIT) == 0x00);
    }

  /* return guessed model number */
  CMDSYNC (0x00);
  return err;
}



/* sets, resets gamma tables */

void
sanei_umax_pp_gamma (int *red, int *green, int *blue)
{
  if (red != NULL)
    {
      ggRed = red;
    }
  else
    {
      ggRed = ggamma;
    }

  if (green != NULL)
    {
      ggGreen = green;
    }
  else
    {
      ggGreen = ggamma;
    }

  if (blue != NULL)
    {
      ggBlue = blue;
    }
  else
    {
      ggBlue = ggamma;
    }
}

/* initialize scanner by loading default transformation table */
/* O: failure
 * 1: OK
 */
int
loadDefaultTables (void)
{
  int cmd01[36] = {
    0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0C,
    0x00, 0x03, 0xC1, 0x80, 0x60, 0x20, 0x00, 0x00,
    0x16, 0x41, 0xE0, 0xAC, 0x03, 0x03, 0x00, 0x00,
    0x46, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xF0, 0x13, -1
  };
  int opsc35[37] = {
    0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0C,
    0x00, 0x03, 0xC1, 0x80, 0x00, 0x00, 0x04, 0x00,
    0x16, 0x41, 0xE0, 0xAC, 0x03, 0x03, 0x00, 0x00,
    0x46, 0xA0, 0x00, 0x8B, 0x49, 0x2A, 0xE9, 0x68,
    0xDF, 0x13, 0x1A, 0x00,
    -1
  };
  int i, len, *cmd, err;
  int buffer[774];
  int rc = 1;


  /* 1600P have a different CCD command block */
  if (sanei_umax_pp_getastra () == 1600)
    {
      opsc35[29] = 0x1A;
      opsc35[30] = 0xEE;
    }

  if (sanei_umax_pp_getastra () <= 610)
    {
      len = 0x22;
      cmd = cmd01;
      /* XXX STEF XXX if we send F0, we get 0x10 back */
      cmd[0x21] = 0x10;
    }
  else
    {
      len = 0x24;
      cmd = opsc35;
    }

  /* set and reread first table */
  /* since 1660P seems to have another type of CCD 
   * this table is not sent/needed
   */
  err = 0;
  if (sanei_umax_pp_getastra () != 1600)
    {
      CMDSETGET (8, len, cmd);
      CMDSYNC (0xC2);
      buffer[0] = 0x00;
      buffer[1] = 0x00;
      buffer[2] = 0x00;
      for (i = 0; i < 768; i++)
	buffer[i + 3] = i % 256;
      if (sanei_umax_pp_getastra () <= 610)
	{
	  buffer[768 + 3] = 0xFF;
	  buffer[768 + 4] = 0xFF;
	}
      else
	{
	  buffer[768 + 3] = 0xAA;
	  buffer[768 + 4] = 0xAA;
	}
      buffer[768 + 5] = -1;
      CMDSETGET (4, 0x305, buffer);


      /* check buffer returned */
      for (i = 0; i < 768; i++)
	{
	  if (buffer[i + 3] != (i % 256))
	    {
	      DBG
		(0,
		 "Error data altered: byte %d=0x%02X, should be 0x%02X !    (%s:%d)\n",
		 i, buffer[i + 3], i % 256, __FILE__, __LINE__);
	      err = 1;
	    }
	}
      if (err)
	return 0;
    }

  /* second table ... */
  for (i = 0; i < 256; i++)
    {
      buffer[i * 2] = i;
      buffer[i * 2 + 1] = 0;
    }
  CMDSETGET (8, len, cmd);
  CMDSYNC (0xC2);
  CMDSET (4, 0x200, buffer);

  /* third table ... */
  if (sanei_umax_pp_getastra () <= 610)
    {
      for (i = 0; i < 256; i++)
	{
	  buffer[i * 2] = i;
	  buffer[i * 2 + 1] = 0x01;	/* instead of 0x00 */
	}
    }
  else
    {
      for (i = 0; i < 256; i++)
	{
	  buffer[i * 2] = i;
	  buffer[i * 2 + 1] = 0x04;	/* instead of 0x00 */
	}
    }
  opsc35[2] = 0x06;
  cmd01[1] = 0x80;
  CMDSETGET (8, len, cmd);
  CMDSYNC (0xC2);
  CMDSET (4, 0x200, buffer);

  opsc35[2] = 0x04;
  cmd01[1] = 0x00;
  CMDSETGET (8, len, cmd);
  CMDGET (4, 0x200, buffer);
  /* check buffer returned */
  /* if 0x4 are still 0x0 (hum..), we got a 1220P, else it is a 2000P */
  for (i = 0; i < 256; i++)
    {
      if ((buffer[2 * i] != i)
	  || ((buffer[2 * i + 1] != 0x04) && (buffer[2 * i + 1] != 0x01)
	      && (buffer[2 * i + 1] != 0x00)))
	{
	  DBG
	    (0,
	     "Error data altered: expected %d=(0x%02X,0x04), found (0x%02X,0x%02X) !    (%s:%d)\n",
	     i, i, buffer[i * 2], buffer[i * 2 + 1], __FILE__, __LINE__);
	  err = 1;
	}
    }
  if (err)
    return 0;

  return rc;
}

/* inquire scanner status 
 * O: failure
 * 1: OK
 * 2: first scanner init, needs re-homing
 */
int
inquire (void)
{
  int cmd01[36] = {
    0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0C,
    0x00, 0x03, 0xC1, 0x80, 0x60, 0x20, 0x00, 0x00,
    0x16, 0x41, 0xE0, 0xAC, 0x03, 0x03, 0x00, 0x00,
    0x46, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xF0, 0x1B, -1
  };
  int buffer[37];
  int rc = 1, first, i;
  char str[106];


  /* inquiry: ask for RAM, CCD, ... */
  CMDSET (8, 0x23, cmd01);
  CMDGET (8, 0x23, buffer);

  if (DBG_LEVEL > 8)
    {
      for (i = 0; i < 35; i++)
	sprintf (str + 3 * i, "%02X ", buffer[i]);
      str[105] = 0x00;
      DBG (8, "SCANNER INFORMATION=%s\n", str);
    }

  /* get state */
  CMDGET (2, 0x10, buffer);
  first = 1;
  for (i = 0; i < 14; i++)
    {
      if (buffer[i] != 0)
	first = 0;
    }
  if (buffer[15] != 0)
    first = 0;
  if (first)
    rc = 2;

  if (DBG_LEVEL > 8)
    {
      for (i = 0; i < 16; i++)
	sprintf (str + 3 * i, "%02X ", buffer[i]);
      str[48] = 0x00;
      DBG (8, "SCANNER STATE=%s\n", str);
    }

  return rc;
}

/*
 * computes DC offset to have black pixel really black out of
 * CCD ie black gives 0
 * 1220P implements the method described in LM9811 datasheet
 * returns 1 and DC offsets in the corresponding vars on success .
 * On failure, returns 0.
 */
static int
offsetCalibration1220p (int color, int *offRed, int *offGreen, int *offBlue)
{
  unsigned char buffer[5300];
  int i, val;
  int commit[9] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, -1 };
  int opsc04[9] = { 0x06, 0xF4, 0xFF, 0x81, 0x1B, 0x00, 0x00, 0x00, -1 };
  int opsc10[9] = { 0x06, 0xF4, 0xFF, 0x81, 0x1B, 0x00, 0x08, 0x00, -1 };
  int opsc38[37] =
    { 0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0C, 0x00, 0x04, 0x40, 0x01,
    0x00, 0x00, 0x04, 0x00, 0x6E, 0x18, 0x10, 0x03, 0x06, 0x00, 0x00, 0x00,
    0x46, 0xA0, 0x00, 0x8B, 0x49, 0x2A, 0xE9, 0x68, 0xDF, 0x13, 0x1A, 0x00,
    -1
  };
  int opsc48[17] =
    { 0x09, 0x00, 0x00, 0x70, 0x00, 0x00, 0x60, 0x2F, 0x2F, 0x00, 0x00, 0x00,
    0x00, 0x40, 0xA4, 0x00, -1
  };
  float low, high;
  DBG (16, "entering offsetCalibration1220p() ... (%s:%d)\n", __FILE__,
       __LINE__);

  /* really dirty hack: somethig is buggy in BW mode    */
  /* we override mode with color until the bug is found */
  /* color = RGB_MODE; */

  /* 1600P have a different CCD command block */
  if (sanei_umax_pp_getastra () == 1600)
    {
      opsc04[0] = 0x19;
      opsc04[1] = 0xD5;
      opsc04[4] = 0x1B;
      opsc04[7] = 0x20;

      opsc10[0] = 0x19;
      opsc10[1] = 0xD5;
      opsc10[4] = 0x1B;
      opsc10[7] = 0x20;

      opsc48[8] = 0x2B;
      opsc48[11] = 0x20;
      opsc48[12] = 0x08;
      opsc48[13] = 0x42;
    }

  /* offset calibration, scan 24 bytes of black in each color */
  /* component see section 5.1 of LM9811 datasheet */
  if (color >= RGB_MODE)
    {
      CMDSETGET (2, 0x10, opsc48);
      CMDSETGET (8, 0x24, opsc38);
      CMDSETGET (1, 0x08, opsc04);	/* scan std, no 'enhancing' */
      CMDSYNC (0xC2);
      if (sanei_umax_pp_scannerStatus () & 0x80)
	{
	  CMDSYNC (0x00);
	}
      CMDSETGET (4, 0x08, commit);	/* commit ? */
      COMPLETIONWAIT;
      CMDGETBUF (4, 0x18, buffer);
      if (DBG_LEVEL >= 128)
	Dump (0x18, buffer, NULL);
      val = 0;
      for (i = 0; i < 24; i++)
	val += buffer[i];
      low = (float) val / i;	/* Vadc1 */


      CMDSYNC (0x00);
      opsc04[7] = opsc04[7] | 0x10;	/* enhanced ? */
      CMDSETGET (1, 0x08, opsc04);
      COMPLETIONWAIT;
      CMDGETBUF (4, 0x18, buffer);
      val = 0;
      for (i = 0; i < 24; i++)
	val += buffer[i];
      high = (float) val / i;	/* Vadc2 */
      if (DBG_LEVEL >= 128)
	Dump (0x18, buffer, NULL);
      *offRed = 15.0 - ((high - low) * 2);

      /* block that repeats */
      /* must be monochrome since hscan=1 */
      opsc48[0] = 0x01;
      if (sanei_umax_pp_getastra () == 1600)
	{
	  opsc48[12] = 0x0C;
	  opsc48[13] = 0x82;
	}
      else
	{
	  opsc48[12] = 0x04;
	  opsc48[13] = 0x80;
	}
      CMDSETGET (2, 0x10, opsc48);
      CMDSETGET (8, 0x24, opsc38);
      opsc04[7] = opsc04[7] & 0x20;
      CMDSETGET (1, 0x08, opsc04);
      CMDSYNC (0xC2);
      if (sanei_umax_pp_scannerStatus () & 0x80)
	{
	  CMDSYNC (0x00);
	}
      CMDSETGET (4, 0x08, commit);
      COMPLETIONWAIT;
      CMDGETBUF (4, 0x18, buffer);
      if (DBG_LEVEL >= 128)
	Dump (0x18, buffer, NULL);
      val = 0;
      for (i = 0; i < 24; i++)
	val += buffer[i];
      low = (float) val / i;

      CMDSYNC (0x00);
      opsc04[7] = opsc04[7] | 0x10;	/* gain ? */
      CMDSETGET (1, 0x08, opsc04);
      COMPLETIONWAIT;
      CMDGETBUF (4, 0x18, buffer);
      if (DBG_LEVEL >= 128)
	Dump (0x18, buffer, NULL);
      val = 0;
      for (i = 0; i < 24; i++)
	val += buffer[i];
      high = (float) val / i;

      *offBlue = 15.0 - ((high - low) * 2);
    }

  /* block that repeats */
  if (color < RGB_MODE)
    {
      opsc48[0] = 0x05;		/* B&H height is 5 */
      opsc48[13] = 0xC0;	/* B&W mode */
    }
  else
    {
      opsc48[0] = 0x05;		/* color height is 5 (+4 ?) */
      opsc48[13] = 0xC1;	/* some strange mode ? */
    }
  if (sanei_umax_pp_getastra () == 1600)
    opsc48[13] = opsc48[13] | 0x02;
  CMDSETGET (2, 0x10, opsc48);
  CMDSETGET (8, 0x24, opsc38);
  opsc04[7] = opsc04[7] & 0x20;
  CMDSETGET (1, 0x08, opsc04);
  CMDSYNC (0xC2);
  if (sanei_umax_pp_scannerStatus () & 0x80)
    {
      CMDSYNC (0x00);
    }
  CMDSETGET (4, 0x08, commit);
  COMPLETIONWAIT;
  CMDGETBUF (4, 0x18, buffer);
  if (DBG_LEVEL >= 128)
    Dump (0x18, buffer, NULL);
  val = 0;
  for (i = 0; i < 24; i++)
    val += buffer[i];
  low = (float) val / i;

  CMDSYNC (0x00);
  opsc04[7] = opsc04[7] | 0x10;
  CMDSETGET (1, 0x08, opsc04);
  COMPLETIONWAIT;
  CMDGETBUF (4, 0x18, buffer);
  if (DBG_LEVEL >= 128)
    Dump (0x18, buffer, NULL);
  val = 0;
  for (i = 0; i < 24; i++)
    val += buffer[i];
  high = (float) val / i;

  *offGreen = 15.0 - ((high - low) * 2);

  /*DBG (1, "STEF: offsets(RED,GREEN,BLUE=(%d,%d,%d)\n", *offRed, *offGreen,
   *offBlue);*/
  DBG (16, "offsetCalibration1220p() done ...\n");
  return 1;
}

/*
 * computes DC offset to have black pixel really black out of
 * CCD ie black gives 0
 * 610P doesn't implement method described in LM9811 datasheet
 * but scan a black line with decreasing offsets until the 
 * scanned data reach a 'good black level'.
 * returns 1 and DC offsets in the corresponding vars on success .
 * On failure, returns 0.
 */
static int
offsetCalibration610p (int color, int *offRed, int *offGreen, int *offBlue)
{
  int motor[17] = {
    0x11, 0x00, 0x00, 0x70, 0x00, 0x00, 0xC0, 0x2F,
    0x17, 0x00, 0x00, 0xF0, 0x7D, 0x5F, 0xA4, 0x00,
    -1
  };

  int ccd[37] = {
    0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0C,
    0x00, 0x04, 0x40, 0x01, 0x00, 0x20, 0x02, 0x00,
    0x76, 0x12, 0xB0, 0x03, 0x06, 0x00, 0x00, 0x00,
    0x46, 0xA0, 0x00, 0x8B, 0x4D, 0x4B, 0xD0, 0x68,
    0xDF, 0x13, 0x1A, 0x00,
    -1
  };

  int lm9811[9] = {
    0x88, 0xE6, 0xFD, 0x8E, 0x30, 0x00, 0x40, 0xF0,
    -1
  };

  int commit[9] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF,
    -1
  };


  int offset;
  int level;
  unsigned char data[40];
  int i;
  int len;
  int w;

  DBG (16, "entering offsetCalibration610P() ... (%s:%d)\n", __FILE__,
       __LINE__);

  if (sanei_umax_pp_getastra () < 1220)
    {
      len = 0x22;
      w = 40;
    }
  else
    {
      len = 0x24;
      w = 40;
    }

  *offRed = 0;
  *offGreen = 0;
  *offBlue = 0;

  /* first color channel: used both in color and b&w modes */
  /* offset to the max */
  /* supposed to be green componant */
  offset = 0x10;
  do
    {
      offset--;

      /* 7D: 0111 1101 */
      /* sets for the current offset */
      motor[12] = (offset << 2) | 0x40 | 0x01;
      lm9811[7] = offset << 4;

      /* sends commands */
      CMDSYNC (0x00);
      CMDSETGET (2, 0x10, motor);
      CMDSETGET (8, len, ccd);
      CMDSETGET (1, 0x08, lm9811);
      CMDSYNC (0xC2);
      CMDSETGET (4, 0x08, commit);
      COMPLETIONWAIT;
      CMDGETBUF (4, w, data);
      if (DBG_LEVEL > 128)
	{
	  DumpNB (w, 1, data, NULL);
	}
      level = 0;
      for (i = 0; i < w; i++)
	level += data[i];
    }
  /* loop while average >0.5 */
  while ((offset > 0) && ((level * 2) / w > 1));
  *offGreen = offset;

  /* offset calibration for the two other channels when in color */
  if (color >= RGB_MODE)
    {
      motor[0] = 0x01;

      offset = 0x10;
      do
	{
	  offset--;

	  /* sets for the current offset */
	  motor[13] = 0x90 | offset;
	  lm9811[7] = offset << 4;

	  /* sends commands */
	  CMDSYNC (0x00);
	  CMDSETGET (2, 0x10, motor);
	  CMDSETGET (8, len, ccd);
	  CMDSETGET (1, 0x08, lm9811);
	  CMDSYNC (0xC2);
	  CMDSETGET (4, 0x08, commit);
	  COMPLETIONWAIT;
	  CMDGETBUF (4, w, data);
	  if (DBG_LEVEL > 128)
	    {
	      DumpNB (w, 1, data, NULL);
	    }
	  level = 0;
	  for (i = 0; i < w; i++)
	    level += data[i];
	}
      /* loop while average >0.5 */
      while ((offset > 0) && ((level * 2) / w > 1));
      *offBlue = offset;

      /* last color component */
      motor[0] = 0x09;
      ccd[13] = 0xD0 | (ccd[13] & 0x0F);

      offset = 0x10;
      do
	{
	  offset--;

	  /* sets for the current offset */
	  motor[11] = offset << 4;
	  lm9811[7] = offset << 4;

	  /* sends commands */
	  CMDSYNC (0x00);
	  CMDSETGET (2, 0x10, motor);
	  CMDSETGET (8, len, ccd);
	  CMDSETGET (1, 0x08, lm9811);
	  CMDSYNC (0xC2);
	  CMDSYNC (0x00);
	  CMDSETGET (4, 0x08, commit);

	  COMPLETIONWAIT;
	  CMDGETBUF (4, w, data);
	  if (gMode == UMAX_PP_PARPORT_EPP)
	    {
	      CMDSYNC (0x00);
	    }
	  if (DBG_LEVEL > 128)
	    {
	      DumpNB (w, 1, data, NULL);
	    }
	  level = 0;
	  for (i = 0; i < w; i++)
	    level += data[i];
	}
      /* loop while average >0.5 */
      while ((offset > 0) && ((level * 2) / w > 1));
      *offRed = offset;
    }
  else
    {
      *offRed = 0x0F;
      *offBlue = 0x0F;
    }

  return 1;
}

/* 
 * generic offset calibration function
 */
static int
offsetCalibration (int color, int *dcRed, int *dcGreen, int *dcBlue)
{
  if (sanei_umax_pp_getastra () <= 610)
    {
      if (offsetCalibration610p (color, dcRed, dcGreen, dcBlue) == 0)
	{
	  DBG (0, "offsetCalibration610p failed !!! (%s:%d)\n", __FILE__,
	       __LINE__);
	  return 0;
	}
      DBG (16, "offsetCalibration610p(%d=>%d,%d,%d) passed ... (%s:%d)\n",
	   color, *dcRed, *dcGreen, *dcBlue, __FILE__, __LINE__);
    }
  else
    {
      if (offsetCalibration1220p (color, dcRed, dcGreen, dcBlue) == 0)
	{
	  DBG (0, "offsetCalibration1220p failed !!! (%s:%d)\n", __FILE__,
	       __LINE__);
	  return 0;
	}
      DBG (16, "offsetCalibration1220p(%d=>%d,%d,%d) passed ... (%s:%d)\n",
	   color, *dcRed, *dcGreen, *dcBlue, __FILE__, __LINE__);
    }
  return 1;
}

/*
 * computes Video Gain Amplifier : LM9811 can corrects up to 3dB of
 * light variation. So we must adjust VGA so that dynamic range is
 * within 3db. It is achieved when min white pixel >= max white pixel / 2.8
 * So we scan a white area and decrease gain until this condition is met.
 * returns 1 and VGA values in the corresponding vars on success .
 * On failure, returns 0.
 */
static int
coarseGainCalibration610p (int color, int dcRed, int dcGreen, int dcBlue,
			   int *vgaRed, int *vgaGreen, int *vgaBlue)
{
  int motor[17] = {
    0x11, 0x00, 0x00, 0x70, 0x00, 0x00, 0xC0, 0x2F,
    0x17, 0x00, 0xFF, 0xAF, 0xAA, 0x6A, 0xA4, 0x00,
    -1
  };

  int ccd[37] = {
    0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0C,
    0x00, 0x04, 0x40, 0x01, 0x00, 0x20, 0x02, 0x00,
    0x76, 0x41, 0xE0, 0xAC, 0x06, 0x00, 0x00, 0x9C,
    0x4A, 0xA0, 0x00, 0x8B, 0x4D, 0x4B, 0xD0, 0x68,
    0xDF, 0x13, 0x1A, 0x00,
    -1
  };

  /*
   * lm9811[7]= VGA << 4
   * lm9811[6]= 0x40 | DC offset
   */
  int lm9811[9] = {
    0x88, 0xE6, 0xFD, 0x8E, 0x30, 0x00, 0x40, 0xF0,
    -1
  };

  int commit[9] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF,
    -1
  };

  unsigned char data[5400];
  int i, len;
  int w, xstart, xend;
  int min, max;

  TRACE (16, "entering coarseGainCalibration610p ...\n");
  if (sanei_umax_pp_getastra () < 1220)
    {
      w = 2700;
      len = 0x22;
    }
  else
    {
      w = 5400;
      len = 0x24;
    }

  /* move back to desired area */
  MOVE (-69, PRECISION_OFF, NULL);

  /* first scan : taking a reference full width scan to 
   * find usable full width of the CCD
   */
  *vgaRed = 0x08;
  *vgaGreen = 0x00;
  *vgaBlue = 0x00;

  /* scanning red component -> h=1 */
  motor[0] = 0x01;
  motor[13] = 0xAA;		/* will be 6A below */

  encodeDC (dcRed, dcGreen, dcBlue, motor);
  encodeVGA (*vgaRed, *vgaGreen, *vgaBlue, motor);

  lm9811[7] = dcRed << 4;
  lm9811[6] = 0x40 | *vgaRed;

  CMDSYNC (0x00);
  CMDSETGET (2, 0x10, motor);
  CMDSETGET (8, len, ccd);
  CMDSETGET (1, 0x08, lm9811);
  CMDSYNC (0xC2);
  CMDSETGET (4, 0x08, commit);

  COMPLETIONWAIT;
  CMDGETBUF (4, w, data);
  if (gMode == UMAX_PP_PARPORT_EPP)
    {
      CMDSYNC (0x00);
    }
  if (DBG_LEVEL > 128)
    {
      DumpNB (w, 1, data, NULL);
    }

  /* find usable CCD area */
  i = 0;
  while ((i < w) && (data[i] <= (targetCode * 2) / 5))
    i++;
  xstart = i;
  i = w - 1;
  while ((i > 0) && (data[i] <= (targetCode * 2) / 5))
    i--;
  xend = i;
  DBG (32, "coarseGainCalibration610p: xstart=%d, xend=%d ->left=%d\n",
       xstart, xend, ((xend + xstart - w) / 2));
  /* choose best 'left' position */
  sanei_umax_pp_setLeft ((xend + xstart - w) / 2);

  /* now do VGA calibration for green (=B&W channel) */
  motor[0] = 0x11;
  motor[13] = 0x60 | (motor[13] & 0x0F);

  *vgaRed = 0x0F;
  *vgaGreen = 0x0F;
  *vgaBlue = 0x0F;

  for (*vgaGreen = 0x0F; *vgaGreen > 0; (*vgaGreen)--)
    {
      encodeDC (dcRed, dcGreen, dcBlue, motor);
      encodeVGA (*vgaRed, *vgaGreen, *vgaBlue, motor);

      lm9811[7] = dcGreen << 4;
      lm9811[6] = 0x40 | *vgaGreen;

      CMDSETGET (2, 0x10, motor);
      CMDSETGET (8, len, ccd);
      CMDSETGET (1, 0x08, lm9811);
      CMDSYNC (0xC2);
      CMDSYNC (0x00);
      CMDSETGET (4, 0x08, commit);

      COMPLETIONWAIT;
      CMDGETBUF (4, w, data);
      if (gMode == UMAX_PP_PARPORT_EPP)
	{
	  CMDSYNC (0x00);
	}

      if (DBG_LEVEL > 128)
	{
	  DumpNB (w, 1, data, NULL);
	}

      min = 0xFF;
      max = 0x00;
      for (i = xstart; i <= xend; i++)
	{
	  if (data[i] < min)
	    min = data[i];
	  if (data[i] > max)
	    max = data[i];
	}
      if ((max <= targetCode) && (min > (((float) targetCode) / 2.8)))
	break;
      DBG (32, "coarseGainCalibration610p, target/2.8=%f\n",
	   (((float) targetCode) / 2.8));
      DBG (32, "coarseGainCalibration610p, green: min=%d, max=%d\n", min,
	   max);
    }

  if (color >= RGB_MODE)
    {
      motor[0] = 0x01;
      motor[13] = 0xA0 | (motor[13] & 0x0F);

      for (*vgaBlue = 0x0F; *vgaBlue > 0; (*vgaBlue)--)
	{
	  encodeDC (dcRed, dcGreen, dcBlue, motor);
	  encodeVGA (*vgaRed, *vgaGreen, *vgaBlue, motor);

	  lm9811[7] = dcBlue << 4;
	  lm9811[6] = 0x40 | *vgaBlue;

	  CMDSETGET (2, 0x10, motor);
	  CMDSETGET (8, len, ccd);
	  CMDSETGET (1, 0x08, lm9811);
	  CMDSYNC (0xC2);
	  CMDSYNC (0x00);
	  CMDSETGET (4, 0x08, commit);

	  COMPLETIONWAIT;
	  CMDGETBUF (4, w, data);
	  if (gMode == UMAX_PP_PARPORT_EPP)
	    {
	      CMDSYNC (0x00);
	    }

	  if (DBG_LEVEL > 128)
	    {
	      DumpNB (w, 1, data, NULL);
	    }

	  min = 0xFF;
	  max = 0x00;
	  for (i = xstart; i <= xend; i++)
	    {
	      if (data[i] < min)
		min = data[i];
	      if (data[i] > max)
		max = data[i];
	    }
	  if ((max <= targetCode) && (min > (((float) targetCode) / 2.8)))
	    break;
	  DBG (32, "coarseGainCalibration610p, blue: min=%d, max=%d\n", min,
	       max);
	}

      motor[0] = 0x09;
      motor[13] = 0xE0 | (motor[13] & 0x0F);

      for (*vgaRed = 0x0F; *vgaRed > 0; (*vgaRed)--)
	{
	  encodeDC (dcRed, dcGreen, dcBlue, motor);
	  encodeVGA (*vgaRed, *vgaGreen, *vgaBlue, motor);

	  lm9811[7] = dcRed << 4;
	  lm9811[6] = 0x40 | *vgaRed;

	  CMDSETGET (2, 0x10, motor);
	  CMDSETGET (8, len, ccd);
	  CMDSETGET (1, 0x08, lm9811);
	  CMDSYNC (0xC2);
	  CMDSYNC (0x00);
	  CMDSETGET (4, 0x08, commit);

	  COMPLETIONWAIT;
	  CMDGETBUF (4, w, data);
	  if (gMode == UMAX_PP_PARPORT_EPP)
	    {
	      CMDSYNC (0x00);
	    }

	  if (DBG_LEVEL > 128)
	    {
	      DumpNB (w, 1, data, NULL);
	    }

	  min = 0xFF;
	  max = 0x00;
	  for (i = xstart; i <= xend; i++)
	    {
	      if (data[i] < min)
		min = data[i];
	      if (data[i] > max)
		max = data[i];
	    }
	  if ((max <= targetCode) && (min > (((float) targetCode) / 2.8)))
	    break;
	  DBG (32, "coarseGainCalibration610p, red: min=%d, max=%d\n", min,
	       max);
	}
    }
  else
    {
      *vgaRed = 0x0F;
      *vgaBlue = 0x0F;
    }

  TRACE (16, "coarseGainCalibration610p end ...\n");
  return 1;
}

/* same as above, but for 1220P/1600P/200P */
static int
coarseGainCalibration1220p (int color, int dcRed, int dcGreen,
			    int dcBlue, int *vgaRed, int *vgaGreen,
			    int *vgaBlue)
{
  unsigned char buffer[5300];
  int i;
  double sum;
  int xstart = 540;
  int xend = 5100;
  int commit[9] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, -1 };
  int opsc04[9] = { 0x06, 0xF4, 0xFF, 0x81, 0x1B, 0x00, 0x00, 0x00, -1 };
  int opsc10[9] = { 0x06, 0xF4, 0xFF, 0x81, 0x1B, 0x00, 0x08, 0x00, -1 };
  int opsc18[17] =
    { 0x01, 0x00, 0x00, 0x70, 0x00, 0x00, 0x60, 0x2F, 0x2F, 0x00, 0x88, 0x08,
    0x00, 0x80, 0xA4, 0x00, -1
  };
  int opsc39[37] =
    { 0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0C, 0x00, 0x04, 0x40, 0x01,
    0x00, 0x00, 0x04, 0x00, 0x6E, 0x41, 0x20, 0x24, 0x06, 0x00, 0x00, 0x00,
    0x46, 0xA0, 0x00, 0x8B, 0x49, 0x2A, 0xE9, 0x68, 0xDF, 0x13, 0x1A, 0x00,
    -1
  };
  int opsc40[37] =
    { 0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0C, 0x00, 0x04, 0x40, 0x01,
    0x00, 0x00, 0x04, 0x00, 0x6E, 0x41, 0x60, 0x4F, 0x06, 0x00, 0x00, 0x00,
    0x46, 0xA0, 0x00, 0x8B, 0x49, 0x2A, 0xE9, 0x68, 0xDF, 0x93, 0x1A, 0x00,
    -1
  };
  int motor[17] =
    { 0x09, 0x00, 0x00, 0x70, 0x00, 0x00, 0x60, 0x2F, 0x2F, 0x00, 0xA5, 0x09,
    0x00, 0x40, 0xA4, 0x00, -1
  };

  DBG (16, "entering coarseGainCalibration1220p() ... (%s:%d) \n", __FILE__,
       __LINE__);

  /* temporay workaround */
  color = RGB_MODE;

  /* initialize VGA components */
  *vgaGreen = 0;
  *vgaRed = 2;
  *vgaBlue = 2;

  CMDSETGET (2, 0x10, opsc18);
  CMDSETGET (8, 0x24, opsc39);
  opsc04[7] = opsc04[7] & 0x20;
  opsc04[6] = 0x06;		/* one channel gain value */
  CMDSETGET (1, 0x08, opsc10);	/* was opsc04, extraneaous string */
  /* that prevents using move .... */
  CMDSYNC (0xC2);
  CMDSYNC (0x00);
  CMDSETGET (4, 0x08, commit);
  COMPLETIONWAIT;
  CMDGETBUF (4, 0x200, buffer);
  if (DBG_LEVEL >= 128)
    Dump (0x200, buffer, NULL);


  /* auto correction of gain levels */
  /* first color component X        */
  if (color >= RGB_MODE)
    {
      if (sanei_umax_pp_getastra () == 1600)
	{
	  motor[11] |= 0x20;
	  motor[12] = 0x08;
	  motor[13] |= 0x02;

	  opsc04[7] |= 0x20;
	}
      encodeDC (dcRed, dcGreen, dcBlue, motor);
      encodeVGA (*vgaRed, 0, 0, motor);
      CMDSETGET (2, 0x10, motor);
      CMDSETGET (8, 0x24, opsc40);
      if (DBG_LEVEL >= 128)
	{
	  bloc2Decode (motor);
	  bloc8Decode (opsc40);
	}
      opsc04[6] = *vgaRed;
      CMDSETGET (1, 0x08, opsc04);
      CMDSYNC (0xC2);
      CMDSYNC (0x00);
      CMDSETGET (4, 0x08, commit);
      COMPLETIONWAIT;
      CMDGETBUF (4, 0x14B4, buffer);
      if (DBG_LEVEL >= 128)
	Dump (0x14B4, buffer, NULL);
      sum = 0;
      for (i = xstart; i < xend; i++)
	sum += buffer[i];
      sum = sum / (xend - xstart);
      while ((opsc04[6] < 0x0F) && (sum < 140))
	{
	  CMDSYNC (0x00);
	  opsc04[6]++;
	  CMDSETGET (1, 0x000008, opsc04);
	  COMPLETIONWAIT;
	  CMDGETBUF (4, 0x0014B4, buffer);
	  if (DBG_LEVEL >= 128)
	    Dump (0x14B4, buffer, NULL);
	  sum = 0;
	  for (i = xstart; i < xend; i++)
	    sum += buffer[i];
	  sum = sum / (xend - xstart);
	}
      *vgaRed = opsc04[6];

      /* blue */
      encodeDC (dcRed, dcGreen, dcBlue, motor);
      encodeVGA (0, 0, *vgaBlue, motor);
      if (sanei_umax_pp_getastra () == 1600)
	{
	  motor[11] |= 0x20;
	  motor[12] = 0x08;
	  motor[13] |= 0x02;

	  opsc04[7] |= 0x20;
	}
      CMDSETGET (2, 0x10, motor);
      CMDSETGET (8, 0x24, opsc40);
      opsc04[6] = *vgaBlue;
      CMDSETGET (1, 0x08, opsc04);
      CMDSYNC (0xC2);
      CMDSYNC (0x00);
      CMDSETGET (4, 0x08, commit);
      COMPLETIONWAIT;
      CMDGETBUF (4, 0x14B4, buffer);
      if (DBG_LEVEL >= 128)
	Dump (0x14B4, buffer, NULL);
      sum = 0;
      for (i = xstart; i < xend; i++)
	sum += buffer[i];
      sum = sum / (xend - xstart);
      while ((opsc04[6] < 0x0F) && (sum < 140))
	{
	  CMDSYNC (0x00);
	  opsc04[6]++;
	  CMDSETGET (1, 0x08, opsc04);
	  COMPLETIONWAIT;
	  CMDGETBUF (4, 0x14B4, buffer);
	  if (DBG_LEVEL >= 128)
	    Dump (0x14B4, buffer, NULL);
	  sum = 0;
	  for (i = xstart; i < xend; i++)
	    sum += buffer[i];
	  sum = sum / (xend - xstart);
	}
      *vgaBlue = opsc04[6];
    }


  /* component Z: B&W component (green ...) */
  encodeDC (dcRed, dcGreen, dcBlue, motor);
  encodeVGA (0, *vgaGreen, 0, motor);
  if (color < RGB_MODE)
    motor[0] = 0x01;		/* in BW, scan zone doesn't have an extra 4 points */
  else
    motor[0] = 0x05;		/*  extra 4 points */
  motor[13] = 0xC0;		/* B&W */
  if (sanei_umax_pp_getastra () == 1600)
    {
      motor[11] |= 0x20;
      motor[12] = 0x08;
      motor[13] |= 0x02;

      opsc04[7] |= 0x20;
    }
  CMDSETGET (2, 0x10, motor);
  if (DBG_LEVEL >= 128)
    {
      bloc2Decode (motor);
    }
  CMDSETGET (8, 0x24, opsc40);
  opsc04[6] = *vgaGreen;
  CMDSETGET (1, 0x08, opsc04);
  CMDSYNC (0xC2);
  CMDSYNC (0x00);
  CMDSETGET (4, 0x08, commit);
  COMPLETIONWAIT;
  /* B&W hangs here XXX STEF XXX */
  CMDGETBUF (4, 0x14B4, buffer);
  if (DBG_LEVEL >= 128)
    Dump (0x14B4, buffer, NULL);
  sum = 0;
  for (i = xstart; i < xend; i++)
    sum += buffer[i];
  sum = sum / (xend - xstart);
  while ((opsc04[6] < 0x07) && (sum < 110))
    {
      CMDSYNC (0x00);
      opsc04[6]++;
      CMDSETGET (1, 0x08, opsc04);
      COMPLETIONWAIT;
      CMDGETBUF (4, 0x0014B4, buffer);
      if (DBG_LEVEL >= 128)
	Dump (0x14B4, buffer, NULL);
      sum = 0;
      for (i = xstart; i < xend; i++)
	sum += buffer[i];
      sum = sum / (xend - xstart);
    }
  *vgaGreen = opsc04[6];
  DBG (1, "coarseGainCalibration1220p()=%d,%d,%d done ...\n", *vgaRed,
       *vgaGreen, *vgaBlue);
  return 1;
}

/*
 * generic function
 */
static int
coarseGainCalibration (int color, int dcRed, int dcGreen, int dcBlue,
		       int *vgaRed, int *vgaGreen, int *vgaBlue)
{
  if (sanei_umax_pp_getastra () <= 610)
    {
      if (coarseGainCalibration610p
	  (color, dcRed, dcGreen, dcBlue, vgaRed, vgaGreen, vgaBlue) == 0)
	{
	  DBG (0, "coarseGainCalibration610p failed !!! (%s:%d)\n", __FILE__,
	       __LINE__);
	  return 0;
	}
      DBG (16,
	   "coarseGainCalibration610p passed ... (%s:%d)\n", __FILE__,
	   __LINE__);
    }
  else
    {
      if (coarseGainCalibration1220p
	  (color, dcRed, dcGreen, dcBlue, vgaRed, vgaGreen, vgaBlue) == 0)
	{
	  DBG (0, "coarseGainCalibration1220p failed !!! (%s:%d)\n", __FILE__,
	       __LINE__);
	  return 0;
	}
      DBG (16,
	   "coarseGainCalibration1220p passed ... (%s:%d)\n", __FILE__,
	   __LINE__);
    }
  return 1;
}

/*
 * computes PGA offset for each pixel of the ccd.
 * We scan a white area with PGA=0 and computes the
 * offset to push the result in the correctable range
 * returns 1 and PGA values in 'calibration' var on success .
 * On failure, returns 0.
 */
static int
shadingCalibration610p (int color, int dcRed, int dcGreen, int dcBlue,
			int vgaRed, int vgaGreen, int vgaBlue,
			int *calibration)
{
  int motor[17] = {
    0x5A, 0x80, 0x02, 0x70, 0x00, 0x00, 0xC0, 0x00,
    0x17, 0x05, 0x6C, 0xAB, 0xAA, 0x2A, 0xA4, 0x00,
    -1
  };

  int ccd[37] = {
    0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x0C,
    0x00, 0x04, 0x40, 0x01, 0x00, 0x20, 0x02, 0x00,
    0x76, 0x5D, 0x40, 0xA5, 0x06, 0x00, 0x00, 0xE2,
    0x5E, 0xA0, 0x00, 0x8B, 0x4D, 0x4B, 0xD0, 0x68,
    0xDF, 0x13, 0x1A, 0x00,
    -1
  };

  /*
   * lm9811[7]= VGA << 4
   * lm9811[6]= 0x40 | DC offset
   */
  int lm9811[9] = {
    0x88, 0xE6, 0xFD, 0x8E, 0x30, 0x00, 0x0F, 0x80,
    -1
  };

  int commit[9] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF,
    -1
  };

  int len, dpi, size;
  int bpp = 3;			/* defaults to color scan value */
  int w, h, x, y;
  int sum, i;
  float avg, pct, coeff = 0;
  unsigned char *data = NULL;
  int top, bottom;

  TRACE (16, "entering shadingCalibration610p ...\n");
  len = 0x22;
  w = 2550;
  y = 10;
  dpi = 300;
  h = 90;
  top = 8;
  bottom = 8;

  /* move back first */
  MOVE (-31, PRECISION_OFF, NULL);

  /* gray scanning handling */
  if (color < RGB_MODE)
    {
      lm9811[7] = dcGreen << 4;
      lm9811[6] = 0x40 | vgaGreen;
      bpp = 1;

      motor[13] = 0x6F;
    }

  data = (unsigned char *) malloc (w * h * bpp);
  if (data == NULL)
    {
      DBG (0, "shadingCalibration610p: failed to allocate memory (%s:%d)\n",
	   __FILE__, __LINE__);
      return 0;
    }
  memset (data, 0x00, w * h * bpp);

  /* prepare scan command */
  x = sanei_umax_pp_getLeft ();
  encodeWX (w, x, dpi, color, ccd, bpp * w);
  encodeHY (h, y, motor);
  encodeDC (dcRed, dcGreen, dcBlue, motor);
  encodeVGA (vgaRed, vgaGreen, vgaBlue, motor);
  if (DBG_LEVEL > 128)
    {
      bloc2Decode (motor);
      bloc8Decode (ccd);
    }

  CMDSYNC (0x00);
  CMDSETGET (2, 0x10, motor);
  CMDSETGET (8, len, ccd);
  CMDSETGET (1, 0x08, lm9811);
  CMDSYNC (0xC2);
  CMDSETGET (4, 0x08, commit);
  COMPLETIONWAIT;

  /* picture height is scan area height minus y          */
  /* then we substract 14 or 6 lines that aren't scanned */
  if (color < RGB_MODE)
    h = h - y - 14;
  else
    h = h - y - 6;
  size = w * bpp * h;

  DBG (128,
       "shadingCalibration610p: trying to read 0x%06X bytes ... (%s:%d)\n",
       size, __FILE__, __LINE__);
  /* since we know that each scan line matches CCD width, we signals
   * that data reading doens't need to sync on each byte, but at each
   * row end
   */
  sanei_umax_pp_setfull (1);
  CMDGETBUF (4, size, data);
  sanei_umax_pp_setfull (0);

  /* computes correction here ... */
  /* debug image files */
  /* data is in R B G order */
  if (DBG_LEVEL > 128)
    DumpNB (w * bpp, h, data, NULL);

  /* zeroes all shading coefficients first */
  memset (calibration, 0x00, 3 * w * sizeof (int));

  /* in gray scans, we have only green component (i=3) */
  if (color < RGB_MODE)
    {

      /* build green only coefficients */
      for (x = 4; x < w; x++)
	{
	  sum = 0;
	  for (y = top; y < h - bottom; y++)
	    sum += data[(y * bpp) * w + x];
	  avg = ((float) (sum)) / ((float) (h - (top + bottom)));
	  /* XXX ICI XXX avg=128==>2 */
	  /*coeff = (256.0 * (targetCode / avg - 1.0)) / 2.00;*/
	  coeff = (256.0 * (targetCode / avg - 1.0)) / ((avg*3.5)/targetCode);
	  if (coeff < 0)
	    coeff = 0;
	  if (coeff > 255)
	    coeff = 255;
	  calibration[x + 2 * w - 4] = (int) (coeff + 0.5);
	}
    }
  else
    {
      for (i = 0; i < 3; i++)
	{
	  for (x = 4; x < w; x++)
	    {
	      sum = 0;
	      for (y = top; y < h - bottom; y++)
		sum += data[(y * bpp + i) * w + x];
	      avg = ((float) (sum)) / ((float) (h - (top + bottom)));
	      /* one step increase means a 0.71% increase of the final
	         pixel */
	      pct = 100.0 - (avg * 100.0) / targetCode;
	      switch (i)
		{
		case 0:	/* RED  1.80 */
		case 1:	/* BLUE : 2.10 */
		  coeff = (int) (pct / 0.57 + 0.5);
		  break;
		case 2:	/* GREEN 1.50 */
		  coeff = (int) (pct / 0.45 + 0.5);
		  break;
		}
	      if (coeff < 0)
		coeff = 0;
	      if (coeff > 255)
		coeff = 255;
	      calibration[x + i * w - 4] = (int) (coeff + 0.5);
	    }
	  /* 100 in coeffs -> +104 on picture */
	}
    }

  /* use default color tables */
  for (x = 0; x < 256; x++)
    {
      calibration[3 * w + x] = ggRed[x];
      calibration[3 * w + x + 256] = ggGreen[x];
      calibration[3 * w + x + 512] = ggBlue[x];
    }

  if (DBG_LEVEL > 128)
    {
      DumpNB (w * bpp, h, data, NULL);
      DumpNB (w, h * bpp, data, NULL);
    }

  free (data);
  TRACE (16, "shadingCalibration610p end ...\n");
  return 1;
}

/*
 * build CCD correction: a white area below the top is scanned without
 * correction, and the data are used to compute the coefficents needed
 * to correct the light/CCD variations
 */
static int
shadingCalibration (int color, int dcRed, int dcGreen, int dcBlue,
		    int vgaRed, int vgaGreen, int vgaBlue, int *calibration)
{
  if (sanei_umax_pp_getastra () < 1220)
    return shadingCalibration610p (color, dcRed, dcGreen, dcBlue, vgaRed,
				   vgaGreen, vgaBlue, calibration);
  return shadingCalibration1220p (color, dcRed, dcGreen, dcBlue, vgaRed,
				  vgaGreen, vgaBlue, calibration);
}


/*
 * this is certainly gamma calibration
 * We scan a white area with PGA=0 and computes the
 * offset to push the result in the correctable range
 * returns 1 and PGA values in 'calibration' var on success .
 * On failure, returns 0.
 */
static int
leftShadingCalibration610p (int color, int dcRed, int dcGreen, int dcBlue,
			    int vgaRed, int vgaGreen, int vgaBlue,
			    int *calibration)
{
  int motor[17] = {
    0x14, 0x80, 0x02, 0x60, 0xDE, 0x01, 0xC0, 0x2F,
    0x17, 0x00, 0x6C, 0xAB, 0xAA, 0x2A, 0xA4, 0x00,
    -1
  };

  int ccd[37] = {
    0x00, 0x00, 0xD8, 0x27, 0xEC, 0x53, 0x7D, 0x8A,
    0x77, 0xE3, 0x1D, 0x79, 0x07, 0x20, 0x02, 0x00,
    0x76, 0x41, 0x80, 0xA3, 0xE5, 0x1D, 0x00, 0xF2,
    0x5D, 0xA0, 0x00, 0x8B, 0x4D, 0x4B, 0xD0, 0x68,
    0xDF, 0x13, 0x1A, 0x00,
    -1
  };

  /*
   * lm9811[7]= VGA << 4
   * lm9811[6]= 0x40 | DC offset
   * lm9811[6].bit7 = use shading data
   */
  int lm9811[9] = {
    0x88, 0xE6, 0xFD, 0x8E, 0x30, 0x00, 0x8F, 0x80,
    -1
  };

  int *commit = NULL;

  int len, dpi, size;
  int w, h, x, y;
  int ofst;
  unsigned char *data = NULL;

  TRACE (16, "entering leftShadingCalibration610p ...\n");
  if (sanei_umax_pp_getastra () < 1220)
    {
      len = 0x22;
      ofst = 28;
      w = 2550;
      x = 94 - ofst;		/* left shift compared to shading calibration */
      y = 10;
      dpi = 75;
      h = 20;
    }
  else
    {
      len = 0x24;
      ofst = 28;
      w = 5100;
      x = 180;
      y = 10;
      dpi = 600;
      h = 67;
    }

  commit = (int *) malloc ((w * 3 + 5) * sizeof (int));
  if (commit == NULL)
    {
      DBG (0,
	   "leftShadingCalibration610p: failed to allocate memory (%s:%d)\n",
	   __FILE__, __LINE__);
      return 0;
    }

  data = (unsigned char *) malloc (w * h * 3);
  if (data == NULL)
    {
      DBG (0,
	   "leftShadingCalibration610p: failed to allocate memory (%s:%d)\n",
	   __FILE__, __LINE__);
      free (commit);
      return 0;
    }

  /* prepare scan command */
  encodeWX (w, x, dpi, color, ccd, 7410);
  encodeHY (h, y, motor);
  encodeDC (dcRed, dcGreen, dcBlue, motor);
  encodeVGA (vgaRed, vgaGreen, vgaBlue, motor);
  if (DBG_LEVEL > 128)
    {
      bloc2Decode (motor);
      bloc8Decode (ccd);
    }

  /* build shading calibration data */
  memset (commit, 0x00, (3 * w + 5) * sizeof (int));
  for (x = ofst; x < w; x++)
    {
      commit[x] = calibration[x - ofst];
      commit[x + w] = calibration[x - ofst + w];
      commit[x + 2 * w] = calibration[x - ofst + 2 * w];
    }
  /* image data cropping coefficient  */
  commit[3 * w + 3] = 0xFF;
  commit[3 * w + 4] = 0xFF;

  CMDSYNC (0x00);
  CMDSETGET (2, 0x10, motor);
  CMDSETGET (8, len, ccd);
  CMDSETGET (1, 0x08, lm9811);
  CMDSYNC (0xC2);
  CMDSETGET (4, 3 * w + 5, commit);
  free (commit);
  COMPLETIONWAIT;

  if (color >= RGB_MODE)
    {
      /* picture height is scan area height minus y    */
      h = h - y;
      size = w * 3 * h;
    }
  else
    {
      h = h - y - 1;
      size = w * h;
    }
  DBG (128,
       "leftShadingCalibration610p: trying to read 0x%06X bytes ... (%s:%d)\n",
       size, __FILE__, __LINE__);
  CMDGETBUF (4, size, data);
  if (DBG_LEVEL > 128)
    DumpNB (3 * w, h, data, NULL);

  /* XXX STEF XXX */
  /* build coefficients for the 25 left pixels */
  /* and compute gamma correction ?            */

  free (data);
  TRACE (16, "leftShadingCalibration610p end ...\n");
  return 1;
}
