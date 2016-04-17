/* sane - Scanner Access Now Easy.
   Copyright (C) Marian Eichholz 2001
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

/* ======================================================================

Userspace scan tool for the Microtek 3600 scanner

$Id$

(C) Marian Eichholz 2001

26.4.2001: Added an abstraction layer for TransferControlMsg.

====================================================================== */

#include "sm3600-scantool.h"

/* **********************************************************************

TransferControlMsg()

********************************************************************** */

static int TransferControlMsg(TInstance *this,
		       int nReqType,
		       int nRequest, 
		       int nValue,
		       int nIndex,
		       void *pBuffer,
		       int  cchBuffer,
		       int  cJiffiesTimeout)
{
  SANE_Status err;

  cJiffiesTimeout = cJiffiesTimeout;

  err = sanei_usb_control_msg (this->hScanner,
			 nReqType,
			 nRequest,
			 nValue,
			 nIndex,
			 cchBuffer,
			 pBuffer);
  if (err)
    return err;
  return cchBuffer;
}

/* **********************************************************************

cch=BulkRead()

********************************************************************** */

static int TransferBulkRead(TInstance *this,
	     int nEndPoint,
	     void *pBuffer,
	     int cchMax,
	     int cJiffiesTimeout)
{
  int err;
  size_t sz = cchMax;

  nEndPoint = nEndPoint;
  cJiffiesTimeout = cJiffiesTimeout;

  err = sanei_usb_read_bulk(this->hScanner,
			    pBuffer,
			    &sz);
  if (err)
    return err;
  return sz;
}

/* **********************************************************************

RegWrite(iRegister, cb, ulValue)
RegWriteArray(iRegister, cb, unsigned char uchValues)

********************************************************************** */

__SM3600EXPORT__
TState RegWrite(TInstance *this, int iRegister, int cb, unsigned long ulValue)
{
  char *pchBuffer;
  int   i;
  TBool bOk=true;
  INST_ASSERT();
  /* some rough assertions */
  if (cb<1 || cb>4)
  return SetError(this,SANE_STATUS_INVAL,"unsupported control transfer size %d",cb);
  pchBuffer=malloc(cb);
  CHECK_POINTER(pchBuffer);
  for (i=0; i<cb; i++)
  {
    pchBuffer[i]=(char)(ulValue&0xFF);
    ulValue=ulValue>>8;
  }
  if (!bOk)
  {
    free(pchBuffer);
    return SetError(this,SANE_STATUS_IO_ERROR,
		    "error in reg out: %d,%d,%08X",iRegister,cb,ulValue);
  }
  i=TransferControlMsg(this,  /* handle */
		    0x40,                  /* request type */
		    0x08,                  /* request */
		    iRegister,             /* value */
		    0,                     /* index */
		    pchBuffer, cb,         /* bytes, size */
		    USB_TIMEOUT_JIFFIES);                /* TO, jiffies... */
  free(pchBuffer);
  if (i<0)
    return SetError(this,SANE_STATUS_IO_ERROR,"error during register write");
  return SANE_STATUS_GOOD;
}

__SM3600EXPORT__
TState RegWriteArray(TInstance *this, int iRegister, int cb, unsigned char *pchBuffer)
{
  int   i;
  INST_ASSERT();
  /* some rough assertions */
  i=TransferControlMsg(this,        /* handle */
		    0x40,                  /* request type */
		    0x08,                  /* request */
		    iRegister,             /* value */
		    0,                     /* index */
		    pchBuffer, cb,         /* bytes, size */
		    USB_TIMEOUT_JIFFIES);                /* TO, jiffies... */
  if (i<0)
    return SetError(this,SANE_STATUS_IO_ERROR,"error during register write");
  return SANE_STATUS_GOOD;
}

/* **********************************************************************

MemWriteArray(iAddress, cb, ulValue)

********************************************************************** */

__SM3600EXPORT__
TState MemWriteArray(TInstance *this, int iAddress,
		     int cb, unsigned char *pchBuffer)
{
  int   i;
  INST_ASSERT();
  /* some rough assertions */
  i=TransferControlMsg(this,
		    0x40,                  /* request type */
		    0x09,                  /* request */
		    iAddress,              /* value */
		    0,                     /* index */
		    pchBuffer, cb,         /* bytes, size */
		    10000);                /* TO, jiffies... */
  if (i<0)
    return SetError(this,SANE_STATUS_IO_ERROR,"error during memory write");
  return SANE_STATUS_GOOD;
}

/* **********************************************************************

MemReadArray(iRegister, cb, ulValue)

********************************************************************** */

#ifdef INSANE_VERSION

__SM3600EXPORT__
TState MemReadArray(TInstance *this, int iAddress, int cb, unsigned char *pchBuffer)
{
  int   i;
  INST_ASSERT();
  /* some rough assertions */
  i=TransferControlMsg(this,
		    0xC0,                  /* request type */
		    0x01,                  /* request */
		    iAddress,              /* value */
		    0,                     /* index */
		    pchBuffer, cb,         /* bytes, size */
		    USB_TIMEOUT_JIFFIES);                /* TO, jiffies... */
  if (i<0)
    return SetError(this,SANE_STATUS_IO_ERROR,"error during memory read");
  return SANE_STATUS_GOOD;
}

/* **********************************************************************

RegCheck(iRegister, cb, ulValue)

********************************************************************** */

__SM3600EXPORT__
TState RegCheck(TInstance *this, int iRegister, int cch, unsigned long ulValue)
{
  char *pchBuffer,*pchTransfer;
  int   i,rcCode;
  TBool bOk;
  INST_ASSERT();
  if (cch<1 || cch>3)
    return SetError(this,SANE_STATUS_INVAL,"unsupported control transfer size %d",cch);
  pchBuffer=malloc(cch);
  pchTransfer=calloc(1,cch);
  rcCode=SANE_STATUS_GOOD;
  if (!pchBuffer || !pchTransfer)
    {
      if (pchBuffer) free(pchBuffer);
      if (pchTransfer) free(pchTransfer);
      rcCode=SetError(this, SANE_STATUS_NO_MEM, "no memory in RegCheck()");
    }
  bOk=true;
  for (i=0; !rcCode && i<cch; i++)
    {
      pchBuffer[i]=(char)(ulValue&0x00FF);
      ulValue=(ulValue>>8);
    }
  if (!rcCode)
    {
      if (!bOk)
	rcCode=SetError(this,SANE_STATUS_IO_ERROR,
			"error in reg out: %d,%d,%08X",iRegister,cch,ulValue);
      else
	{
	  i=TransferControlMsg(this,  /* handle */
		    0xC0,                  /* request type */
		    0x00,                  /* request */
		    iRegister,             /* value */
		    0,                     /* index */
		    pchTransfer, cch,      /* bytes, size */
		    USB_TIMEOUT_JIFFIES);                /* TO, jiffies... */
	  if (i<0)
	    rcCode=SetError(this,SANE_STATUS_IO_ERROR,
			    "error during register check");
	}
    }
  if (!rcCode && memcmp(pchTransfer,pchBuffer,cch))
    {
      DumpBuffer(stdout,pchTransfer,cch);
      rcCode=SetError(this,SANE_STATUS_IO_ERROR,
		      "check register failed for %d,%d,%08X",
		      iRegister,cch,ulValue);
    }
  free(pchTransfer); free(pchBuffer);
  return rcCode;
}

/* **********************************************************************

cchRead=BulkRead(fh,cchBulk)

********************************************************************** */

__SM3600EXPORT__
int BulkRead(TInstance *this, FILE *fhOut, unsigned int cchBulk)
{
  int   cchRead,rc;
  char *pchBuffer;
  INST_ASSERT();
  pchBuffer=(char*)malloc(cchBulk);
  CHECK_POINTER(pchBuffer);
  cchRead=0;
  rc=0;
  while (!rc && cchBulk)
    {
      int cchChunk;
      int cchReal;
      
      cchChunk=cchBulk;
      if (cchChunk>0x1000)
	cchChunk=0x1000;
      cchReal=TransferBulkRead(this,
		       0x82,
		       pchBuffer+cchRead,
		       cchChunk,
		       USB_TIMEOUT_JIFFIES);
      dprintf(DEBUG_COMM,"bulk read: %d -> %d\n",cchChunk,cchReal);
      if (cchReal>=0)
	{
	  cchBulk-=cchReal;
	  cchRead+=cchReal;
	  if (cchReal<cchChunk) /* last Chunk of a series */
	    break;
	}
      else
	{
	  rc=SetError(this,SANE_STATUS_IO_ERROR,
		      "bulk read of %d bytes failed: %s",
		      cchChunk,
		      "I/O error"
		      );
	  continue;
	}
    }
  dprintf(DEBUG_COMM,"writing %d bytes\n",cchRead);
  if (fhOut && !rc)
    {
      fwrite(pchBuffer,1,cchRead,fhOut);
      if (ferror(fhOut))
	rc=SetError(this,SANE_STATUS_IO_ERROR,
		    "scan file write failed: %s",
		    strerror(errno));
    }
  free(pchBuffer);
  return rc ? -1 : cchRead;
}

#endif

/* **********************************************************************

cchRead=BulkReadBuffer(puchBuffer, cchBulk)

********************************************************************** */

__SM3600EXPORT__
int BulkReadBuffer(TInstance *this,
		   unsigned char *puchBufferOut,
		   unsigned int cchBulk)
{
  int   cchRead,rc;
  char *pchBuffer;
  INST_ASSERT();
  pchBuffer=(char*)malloc(cchBulk);
  CHECK_POINTER(pchBuffer);
  cchRead=0;
  rc=0;
  while (!rc && cchBulk)
    {
      int cchChunk;
      int cchReal;
      
      cchChunk=cchBulk;
      if (cchChunk>0x1000)
	cchChunk=0x1000;
      cchReal=TransferBulkRead(this,
		       0x82,
		       pchBuffer+cchRead,
		       cchChunk,
		       USB_TIMEOUT_JIFFIES);
      dprintf(DEBUG_COMM,"bulk read: %d -> %d\n",cchChunk,cchReal);
      if (cchReal>=0)
	{
	  cchBulk-=cchReal;
	  cchRead+=cchReal;
	  if (cchReal<cchChunk) /* last Chunk of a series */
	    break;
	}
      else
	rc=SetError(this,SANE_STATUS_IO_ERROR,
		    "bulk read of %d bytes failed: %s",
		    cchChunk,
		    "I/O error"
		    );
    }
  dprintf(DEBUG_COMM,"writing %d bytes\n",cchRead);
  
  if (!rc && puchBufferOut)
    memcpy(puchBufferOut,pchBuffer,cchRead);
  free(pchBuffer);
  return rc ? -1 : cchRead;
}

/* **********************************************************************

RegRead(iRegister, int cch)

Read register in big endian (INTEL-) format.

********************************************************************** */

__SM3600EXPORT__
unsigned int RegRead(TInstance *this, int iRegister, int cch)
{
  char        *pchTransfer;
  int          i;
  unsigned int n;
  INST_ASSERT();
  if (cch<1 || cch>4)
    {
      SetError(this,SANE_STATUS_INVAL,
		    "unsupported control read size %d",cch);
      return 0;
    }
  pchTransfer=calloc(1,cch);
  CHECK_POINTER(pchTransfer);
  i=TransferControlMsg(this,  /* handle */
        0xC0,                  /* request type */
	0x00,                  /* request */
	iRegister,             /* value */
	0,                     /* index */
	pchTransfer, cch,      /* bytes, size */
	USB_TIMEOUT_JIFFIES);                /* TO, jiffies... */
  if (i>=0)
    {
      n=0;
      for (i=cch-1; i>=0; i--)
	n=(n<<8)|(unsigned char)pchTransfer[i];
      free(pchTransfer);
      return n;
    }
  free(pchTransfer);
  SetError(this,SANE_STATUS_IO_ERROR,"error during register read");
  return 0;
}

