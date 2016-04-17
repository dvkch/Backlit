/*
  sane - Scanner Access Now Easy.
  Copyright (C) 2006 Jon Chambers <jon@jon.demon.co.uk>
 
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
 
  Dell 1600n network scan driver for SANE.
   
  To debug:
  SANE_DEBUG_DELL1600N_NET=255 scanimage --verbose 2>scan.errs 1>scan.png
*/

/***********************************************************
 * INCLUDES
 ***********************************************************/

#include "../include/sane/config.h"
#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"

#define BACKEND_NAME    dell1600n_net
#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>

/* :NOTE: these are likely to be platform-specific! */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>

#include <jpeglib.h>
#include <tiffio.h>

/* OS/2... */
#ifndef SHUT_RDWR
#define SHUT_RDWR 2
#endif


/***********************************************************
 * DEFINITIONS
 ***********************************************************/

/* Maximum number of scanners */
#define MAX_SCANNERS 32

/* version number */
#define DRIVER_VERSION SANE_VERSION_CODE( SANE_CURRENT_MAJOR, V_MINOR, 0 )

/* size of buffer for socket communication */
#define SOCK_BUF_SIZE 2048

/* size of registation name */
#define REG_NAME_SIZE 64

struct DeviceRecord
{
  SANE_Device m_device;
  char * m_pName;        /* storage of name */
  char * m_pModel;       /* storage of model */
};

/* a buffer struct to store "stuff" */
struct ComBuf
{
  size_t m_capacity;		/* current allocated size in bytes */
  size_t m_used;		/* current used size in bytes */
  unsigned char *m_pBuf;	/* storage (or NULL if none allocated) */
};

/* state data for a single scanner connection */
struct ScannerState
{
  int m_udpFd;			/* file descriptor to UDP socket */
  int m_tcpFd;			/* file descriptor to TCP socket */
  struct sockaddr_in m_sockAddr;	/* printer address */
  struct ComBuf m_buf;		/* buffer for network data */
  struct ComBuf m_imageData;	/* storage for decoded image data */
  int m_numPages;	        /* number of complete pages (host byte order) */
  struct ComBuf m_pageInfo;	/* "array" of numPages PageInfo structs */
  int m_bFinish;		/* set non-0 to signal that we are finished */
  int m_bCancelled;		/* set non-0 that bFinish state arose from cancelation */
  char m_regName[REG_NAME_SIZE];	/* name with which to register */
  unsigned short m_xres;	/* x resolution (network byte order) */
  unsigned short m_yres;	/* y resolution (network byte order) */
  unsigned int m_composition;	/* composition (0x01=>TIFF/PDF,0x40=>JPEG) (network byte order) */
  unsigned char m_brightness;	/* brightness */
  unsigned int m_compression;	/* compression (0x08=>CCIT Group 4,0x20=>JPEG) (network byte order) */
  unsigned int m_fileType;	/* file type (2=>TIFF,4=>PDF,8=>JPEG)(network byte order) */
  unsigned int m_pixelWidth;	/* width in pixels (network byte order) */
  unsigned int m_pixelHeight;	/* height in pixels (network byte order) */
  unsigned int m_bytesRead;	/* bytes read by SANE (host byte order) */
  unsigned int m_currentPageBytes;/* number of bytes of current page read (host byte order) */
};

/* state data for a single page 
   NOTE: all ints are in host byte order 
*/
struct PageInfo
{
  int m_width;                 /* pixel width */
  int m_height;                /* pixel height */
  int m_totalSize;             /* total page size (bytes) */
  int m_bytesRemaining;        /* number of bytes not yet passed to SANE client */
};

/* struct for in-memory jpeg decompression */
struct JpegDataDecompState
{
  struct jpeg_decompress_struct m_cinfo;	/* base struct */
  unsigned char *m_pData;	/* data pointer */
  unsigned int m_bytesRemaining;	/* amount of unprocessed data */
};

/* initial ComBuf allocation */
#define INITIAL_COM_BUF_SIZE 1024

/***********************************************************
 * FUNCTION PROTOTYPES
 ***********************************************************/

/* print hex buffer to stdout */
static void HexDump (int debugLevel, const unsigned char *buf,
		     size_t bufSize);

/* clears gKnownDevices array */
static void ClearKnownDevices (void);

/* initialise a ComBuf struct */
static int InitComBuf (struct ComBuf *pBuf);

/* free a ComBuf struct */
static void FreeComBuf (struct ComBuf *pBuf);

/* add data to a ComBuf struct */
static int AppendToComBuf (struct ComBuf *pBuf, const unsigned char *pData,
			   size_t datSize);

/* remove data from the front of a ComBuf struct */
static int PopFromComBuf (struct ComBuf *pBuf, size_t datSize);

/* initialise a packet */
static int InitPacket (struct ComBuf *pBuf, char type);

/* append message to a packet */
static int AppendMessageToPacket (struct ComBuf *pBuf,
				  char messageType,
				  char *messageName,
				  char valueType,
				  void *pValue, size_t valueLen);

/* write length data to packet header */
static void FinalisePacket (struct ComBuf *pBuf);

/* \return 1 if message is complete, 0 otherwise */
static int MessageIsComplete (unsigned char *pData, size_t size);

/* process a registration broadcast response
   \return DeviceRecord pointer on success (caller frees), NULL on failure 
*/
static struct DeviceRecord *ProcessFindResponse (unsigned char *pData, size_t size);

/* frees a scanner state struct stored in gOpenScanners */
static void FreeScannerState (int iHandle);

/* \return 1 if iHandle is a valid member of gOpenScanners, 0 otherwise */
static int ValidScannerNumber (int iHandle);

/* process UDP responses, \return 0 in success, >0 otherwise */
static int ProcessUdpResponse (unsigned char *pData, size_t size,
			       struct ScannerState *pState);

/* process TCP responses, \return 0 in success, >0 otherwise */
static int ProcessTcpResponse (struct ScannerState *pState,
			       struct ComBuf *pTcpBufBuf);

/* Process the data from a single scanned page, \return 0 in success, >0 otherwise */
static int ProcessPageData (struct ScannerState *pState);

/* Libjpeg decompression interface */
static void JpegDecompInitSource (j_decompress_ptr cinfo);
static boolean JpegDecompFillInputBuffer (j_decompress_ptr cinfo);
static void JpegDecompSkipInputData (j_decompress_ptr cinfo, long numBytes);
static void JpegDecompTermSource (j_decompress_ptr cinfo);

/***********************************************************
 * GLOBALS
 ***********************************************************/

/* Results of last call to sane_get_devices */
static struct DeviceRecord *gKnownDevices[MAX_SCANNERS];

/* Array of open scanner device states.
   :NOTE: (int)SANE_Handle is an offset into this array */
static struct ScannerState *gOpenScanners[MAX_SCANNERS];

/* scanner port */
static unsigned short gScannerPort = 1124;

/* ms to wait for registration replies */
static unsigned short gRegReplyWaitMs = 300;

/***********************************************************
 * FUNCTION IMPLEMENTATIONS
 ***********************************************************/

SANE_Status
sane_init (SANE_Int * version_code,
	   SANE_Auth_Callback __sane_unused__ authorize)
{

  /* init globals */
  memset (gKnownDevices, 0, sizeof (gKnownDevices));
  memset (gOpenScanners, 0, sizeof (gOpenScanners));

  /* report version */
  *version_code = DRIVER_VERSION;

  /* init debug */
  DBG_INIT ();

  return SANE_STATUS_GOOD;

} /* sane_init */

/***********************************************************/

void
sane_exit (void)
{

  int iHandle;

  /* clean up */
  ClearKnownDevices ();

  for (iHandle = 0; iHandle < MAX_SCANNERS; ++iHandle)
    {
      if (gOpenScanners[iHandle])
	FreeScannerState (iHandle);
    }

} /* sane_exit */

/***********************************************************/

SANE_Status
sane_get_devices (const SANE_Device *** device_list,
		  SANE_Bool __sane_unused__ local_only)
{

  int ret;
  unsigned char sockBuf[SOCK_BUF_SIZE];
  int sock, optYes;
  struct DeviceRecord *pDevice;
  struct ComBuf queryPacket;
  struct sockaddr_in remoteAddr;
  unsigned char ucVal;
  fd_set readFds;
  struct timeval selTimeVal;
  int nread, iNextDevice;
  FILE *fConfig;
  char configBuf[ 256 ];
  const char *pVal;
  int valLen;

  /* init variables */
  ret = SANE_STATUS_GOOD;
  sock = 0;
  pDevice = NULL;
  optYes = 1;
  InitComBuf (&queryPacket);

  /* clear previous results */
  ClearKnownDevices ();
  iNextDevice = 0;

  /* look for a config file */
  fConfig = sanei_config_open( "dell1600n_net.conf" );
  if ( fConfig )
    {
      while ( ! feof( fConfig ) )
        {
          if ( ! sanei_config_read ( configBuf, sizeof( configBuf ), fConfig ) ) break;

          /* skip whitespace */
          pVal = sanei_config_skip_whitespace ( configBuf );

          /* skip comments */
          if ( *pVal == '#' ) continue;

          /* process named_scanner */
          valLen = strlen( "named_scanner:" );
          if ( ! strncmp( pVal, "extra_scanner:", valLen ) ){

            pVal = sanei_config_skip_whitespace ( pVal + valLen );

            pDevice = malloc (sizeof (struct DeviceRecord));
            if (!pDevice)
              {
                DBG (1, "sane_get_devices: memory allocation failure\n");
                break;
              }

            pDevice->m_pName = strdup (pVal);
            pDevice->m_device.vendor = "Dell";
            pDevice->m_pModel = strdup( "1600n" );
            pDevice->m_device.type = "multi-function peripheral";

            pDevice->m_device.name = pDevice->m_pName;
            pDevice->m_device.model = pDevice->m_pModel;

            /* add to list */
            gKnownDevices[iNextDevice++] = pDevice;

            continue;
          } /* if */

        } /* while */

      /* Close the file */
      fclose( fConfig );
    }

  /* open UDP socket */
  sock = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock == -1)
    {
      DBG (1, "Error creating socket\n");
      ret = SANE_STATUS_NO_MEM;
      goto cleanup;
    }
  setsockopt (sock, SOL_SOCKET, SO_BROADCAST, &optYes, sizeof (optYes));

  /* prepare select mask */
  FD_ZERO (&readFds);
  FD_SET (sock, &readFds);
  selTimeVal.tv_sec = 0;
  selTimeVal.tv_usec = gRegReplyWaitMs * 1000;

  /* init a packet */
  InitPacket (&queryPacket, 0x01);

  /* add query */
  ucVal = 0;
  AppendMessageToPacket (&queryPacket, 0x25, "std-scan-discovery-all",
			 0x02, &ucVal, sizeof (ucVal));

  FinalisePacket (&queryPacket);

  DBG (10, "Sending:\n");
  HexDump (10, queryPacket.m_pBuf, queryPacket.m_used);


  remoteAddr.sin_family = AF_INET;
  remoteAddr.sin_port = htons (gScannerPort);
  remoteAddr.sin_addr.s_addr = 0xFFFFFFFF;	/* broadcast */

  if (sendto (sock, queryPacket.m_pBuf, queryPacket.m_used, 0,
    &remoteAddr, sizeof (remoteAddr)) == -1)
    {
      DBG (1, "Error sending broadcast packet\n");
      ret = SANE_STATUS_NO_MEM;
      goto cleanup;
    }

  /* process replies */
  while (select (sock + 1, &readFds, NULL, NULL, &selTimeVal))
    {

      /* break if we've got no more storage space in array */
      if (iNextDevice >= MAX_SCANNERS)
        {
          DBG (1, "sane_get_devices: more than %d devices, ignoring\n",
            MAX_SCANNERS);
          break;
        }

      nread = read (sock, sockBuf, sizeof (sockBuf));
      DBG (5, "Got a broadcast response, (%d bytes)\n", nread);

      if (nread <= 0)
        break;

      HexDump (10, sockBuf, nread);

      /* process response (skipping bad ones) */
      if (!(pDevice = ProcessFindResponse (sockBuf, nread))) continue;

      /* add to list */
      gKnownDevices[iNextDevice++] = pDevice;

    } /* while */

  /* report our finds */
  *device_list = (const SANE_Device **) gKnownDevices;

cleanup:

  if (sock)
    close (sock);
  FreeComBuf (&queryPacket);

  return ret;

} /* sane_get_devices */

/***********************************************************/

SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle * handle)
{

  int iHandle = -1, i;
  SANE_Status status = SANE_STATUS_GOOD;
  struct hostent *pHostent;
  char *pDot;

  DBG( 5, "sane_open: %s\n", devicename );

  /* find the next available scanner pointer in gOpenScanners */
  for (i = 0; i < MAX_SCANNERS; ++i)
    {

      if (gOpenScanners[i]) continue;

      iHandle = i;
      break;

    } /* for */
  if (iHandle == -1)
    {
      DBG (1, "sane_open: no space left in gOpenScanners array\n");
      status = SANE_STATUS_NO_MEM;
      goto cleanup;
    }

  /* allocate some space */
  if (!(gOpenScanners[iHandle] = malloc (sizeof (struct ScannerState))))
    {
      status = SANE_STATUS_NO_MEM;
      goto cleanup;
    }

  /* init data */
  memset (gOpenScanners[iHandle], 0, sizeof (struct ScannerState));
  InitComBuf (&gOpenScanners[iHandle]->m_buf);
  InitComBuf (&gOpenScanners[iHandle]->m_imageData);
  InitComBuf (&gOpenScanners[iHandle]->m_pageInfo);
  gOpenScanners[iHandle]->m_xres = ntohs (200);
  gOpenScanners[iHandle]->m_yres = ntohs (200);
  gOpenScanners[iHandle]->m_composition = ntohl (0x01);
  gOpenScanners[iHandle]->m_brightness = 0x80;
  gOpenScanners[iHandle]->m_compression = ntohl (0x08);
  gOpenScanners[iHandle]->m_fileType = ntohl (0x02);


  /* look up scanner name */
  pHostent = gethostbyname (devicename);
  if ((!pHostent) || (!pHostent->h_addr_list))
    {
      DBG (1, "sane_open: error looking up scanner name %s\n", devicename);
      status = SANE_STATUS_INVAL;
      goto cleanup;
    }

  /* open a UDP socket */
  if (!(gOpenScanners[iHandle]->m_udpFd =
    socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)))
    {
      DBG (1, "sane_open: error opening socket\n");
      status = SANE_STATUS_IO_ERROR;
      goto cleanup;
    }

  /* connect to the scanner */
  memset (&gOpenScanners[iHandle]->m_sockAddr, 0,
    sizeof (gOpenScanners[iHandle]->m_sockAddr));
  gOpenScanners[iHandle]->m_sockAddr.sin_family = AF_INET;
  gOpenScanners[iHandle]->m_sockAddr.sin_port = htons (gScannerPort);
  memcpy (&gOpenScanners[iHandle]->m_sockAddr.sin_addr,
    pHostent->h_addr_list[0], pHostent->h_length);
  if (connect (gOpenScanners[iHandle]->m_udpFd,
    (struct sockaddr *) &gOpenScanners[iHandle]->m_sockAddr,
    sizeof (gOpenScanners[iHandle]->m_sockAddr)))
    {
      DBG (1, "sane_open: error connecting to %s:%d\n", devicename,
        gScannerPort);
      status = SANE_STATUS_IO_ERROR;
      goto cleanup;
    }

  /* set fallback registration name */
  sprintf (gOpenScanners[iHandle]->m_regName, "Sane");

  /* try to fill in hostname */
  gethostname (gOpenScanners[iHandle]->m_regName, REG_NAME_SIZE);

  /* just in case... */
  gOpenScanners[iHandle]->m_regName[REG_NAME_SIZE - 1] = 0;

  /* chop off any domain (if any) */
  if ((pDot = strchr (gOpenScanners[iHandle]->m_regName, '.')))
    *pDot = 0;

  DBG (5, "sane_open: connected to %s:%d as %s\n", devicename, gScannerPort,
       gOpenScanners[iHandle]->m_regName);


  /* set the handle */
  *handle = (SANE_Handle) (unsigned long)iHandle;

  return status;

cleanup:

  if (iHandle != -1)
    FreeScannerState (iHandle);

  return status;

} /* sane_open */

/***********************************************************/

void
sane_close (SANE_Handle handle)
{

  DBG( 5, "sane_close: %lx\n", (unsigned long)handle );

  FreeScannerState ((unsigned long) handle);

} /* sane_close */

/***********************************************************/

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle __sane_unused__ handle,
			    SANE_Int option)
{

  static SANE_Option_Descriptor numOptions = {
    "num_options",
    "Number of options",
    "Number of options",
    SANE_TYPE_INT,
    SANE_UNIT_NONE,
    1,
    0,
    0,
    {0}
  };

  if (option == 0)
    return &numOptions;
  else
    return NULL;

} /* sane_get_option_descriptor */

/***********************************************************/

SANE_Status
sane_control_option (SANE_Handle __sane_unused__ handle, SANE_Int option,
		     SANE_Action action, void *value,
		     SANE_Int __sane_unused__ * info)
{

  static int numOptions = 1;

  if (action == SANE_ACTION_GET_VALUE && option == 0)
    *(int *) value = numOptions;

  return SANE_STATUS_GOOD;

} /* sane_control_option  */

/***********************************************************/

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  int iHandle = (int) (unsigned long)handle;
  unsigned int width, height, imageSize;
  struct PageInfo pageInfo;

  if (!gOpenScanners[iHandle])
    return SANE_STATUS_INVAL;

  /* fetch page info */
  memcpy( & pageInfo, gOpenScanners[iHandle]->m_pageInfo.m_pBuf, sizeof( pageInfo ) );

  width = pageInfo.m_width;
  height = pageInfo.m_height;
  imageSize = width * height * 3;

  DBG( 5, "sane_get_parameters: bytes remaining on this page: %d, num pages: %d, size: %dx%d\n", 
       pageInfo.m_bytesRemaining,
       gOpenScanners[iHandle]->m_numPages,
       width,
       height );

  DBG (5,
       "sane_get_parameters: handle %x: bytes outstanding: %lu, image size: %d\n",
       iHandle, (unsigned long)gOpenScanners[iHandle]->m_imageData.m_used, imageSize);

  /* check for enough data */
  /*
  if (gOpenScanners[iHandle]->m_imageData.m_used < imageSize)
    {
      DBG (1, "sane_get_parameters: handle %d: not enough data: %d < %d\n",
	   iHandle, gOpenScanners[iHandle]->m_imageData.m_used, imageSize);
      return SANE_STATUS_INVAL;
    }
  */


  params->format = SANE_FRAME_RGB;
  params->last_frame = SANE_TRUE;
  params->lines = height;
  params->depth = 8;
  params->pixels_per_line = width;
  params->bytes_per_line = width * 3;

  return SANE_STATUS_GOOD;

} /* sane_get_parameters  */

/***********************************************************/

SANE_Status
sane_start (SANE_Handle handle)
{

  SANE_Status status = SANE_STATUS_GOOD;
  struct ComBuf buf;
  unsigned char sockBuf[SOCK_BUF_SIZE];
  int iHandle, nread;
  int errorCheck = 0;
  struct sockaddr_in myAddr;
  socklen_t addrSize;
  fd_set readFds;
  struct timeval selTimeVal;

  iHandle = (int) (unsigned long)handle;

  DBG( 5, "sane_start: %x\n", iHandle );

  /* fetch and check scanner index */
  if (!ValidScannerNumber (iHandle))
    return SANE_STATUS_INVAL;

  /* check if we still have oustanding pages of data on this handle */
  if (gOpenScanners[iHandle]->m_imageData.m_used){

    /* remove empty page */
    PopFromComBuf ( & gOpenScanners[iHandle]->m_pageInfo, sizeof( struct PageInfo ) );
    return SANE_STATUS_GOOD;

  }

  /* determine local IP address */
  addrSize = sizeof (myAddr);
  if (getsockname (gOpenScanners[iHandle]->m_udpFd, &myAddr, &addrSize))
    {
      DBG (1, "sane_start: Error getting own IP address\n");
      return SANE_STATUS_IO_ERROR;
    }

  /* init a buffer for our registration message */
  errorCheck |= InitComBuf (&buf);

  /* build packet */
  errorCheck |= InitPacket (&buf, 1);
  errorCheck |=
    AppendMessageToPacket (&buf, 0x22, "std-scan-subscribe-user-name", 0x0b,
      gOpenScanners[iHandle]->m_regName,
      strlen (gOpenScanners[iHandle]->m_regName));
  errorCheck |=
    AppendMessageToPacket (&buf, 0x22, "std-scan-subscribe-ip-address", 0x0a,
      &myAddr.sin_addr, 4);
  FinalisePacket (&buf);

  /* check nothing went wrong along the way */
  if (errorCheck)
    {
      status = SANE_STATUS_NO_MEM;
      goto cleanup;
    }

  /* send the packet */
  send (gOpenScanners[iHandle]->m_udpFd, buf.m_pBuf, buf.m_used, 0);


  /* loop until done */
  gOpenScanners[iHandle]->m_bFinish = 0;
  while (!gOpenScanners[iHandle]->m_bFinish)
    {

      /* prepare select mask */
      FD_ZERO (&readFds);
      FD_SET (gOpenScanners[iHandle]->m_udpFd, &readFds);
      selTimeVal.tv_sec = 1;
      selTimeVal.tv_usec = 0;



      DBG (5, "sane_start: waiting for scan signal\n");

      /* wait again if nothing received */
      if (!select (gOpenScanners[iHandle]->m_udpFd + 1,
        &readFds, NULL, NULL, &selTimeVal))
        continue;

      /* read from socket */
      nread =
        read (gOpenScanners[iHandle]->m_udpFd, sockBuf, sizeof (sockBuf));

      if (nread <= 0)
        {
          DBG (1, "sane_start: read returned %d\n", nread);
          break;
        }

      /* process the response */
      if (ProcessUdpResponse (sockBuf, nread, gOpenScanners[iHandle]))
        {
          status = SANE_STATUS_IO_ERROR;
          goto cleanup;
        }

    } /* while */

  /* check whether we were cancelled */
  if ( gOpenScanners[iHandle]->m_bCancelled ) status = SANE_STATUS_CANCELLED;

cleanup:

  FreeComBuf (&buf);
  
  return status;

} /* sane_start */

/***********************************************************/

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * data,
	   SANE_Int max_length, SANE_Int * length)
{

  int iHandle = (int) (unsigned long)handle;
  int dataSize;
  struct PageInfo pageInfo;

  DBG( 5, "sane_read: %x (max_length=%d)\n", iHandle, max_length );

  *length = 0;

  if (!gOpenScanners[iHandle])
    return SANE_STATUS_INVAL;

  /* check for end of data (no further pages) */
  if ( ( ! gOpenScanners[iHandle]->m_imageData.m_used ) 
       || ( ! gOpenScanners[iHandle]->m_numPages ) )
    {
      /* remove empty page if there are no more cached pages */
      PopFromComBuf ( & gOpenScanners[iHandle]->m_pageInfo, sizeof( struct PageInfo ) );

      return SANE_STATUS_EOF;
    }

  /* fetch page info */
  memcpy( & pageInfo, gOpenScanners[iHandle]->m_pageInfo.m_pBuf, sizeof( pageInfo ) );

  /* check for end of page data (we still have further cached pages) */
  if ( pageInfo.m_bytesRemaining < 1 ) return SANE_STATUS_EOF;

  /*  send the remainder of the current image */
  dataSize = pageInfo.m_bytesRemaining;

  /* unless there's not enough room in the output buffer */
  if (dataSize > max_length)
    dataSize = max_length;

  /* update the data sent counters */
  gOpenScanners[iHandle]->m_bytesRead += dataSize;
  pageInfo.m_bytesRemaining -= dataSize;

  /* update counter */
  memcpy( gOpenScanners[iHandle]->m_pageInfo.m_pBuf, & pageInfo, sizeof( pageInfo ) );

  /* check for end of page */
  if ( pageInfo.m_bytesRemaining < 1 ){

    /* yes, so remove page info */
    gOpenScanners[iHandle]->m_numPages--;

  } /* if */

  DBG (5,
       "sane_read: sending %d bytes, image total %d, %d page bytes remaining, %lu total remaining, image: %dx%d\n",
       dataSize, gOpenScanners[iHandle]->m_bytesRead, pageInfo.m_bytesRemaining ,
       (unsigned long)(gOpenScanners[iHandle]->m_imageData.m_used - dataSize), 
       pageInfo.m_width,
       pageInfo.m_height);

  /* copy the data */
  memcpy (data, gOpenScanners[iHandle]->m_imageData.m_pBuf, dataSize);
  if (PopFromComBuf (&gOpenScanners[iHandle]->m_imageData, dataSize))
    return SANE_STATUS_NO_MEM;

  *length = dataSize;

  return SANE_STATUS_GOOD;

} /* sane_read */

/***********************************************************/

void
sane_cancel (SANE_Handle handle)
{
  int iHandle = (int) (unsigned long)handle;

  DBG( 5, "sane_cancel: %x\n", iHandle );

  /* signal that bad things are afoot */
  gOpenScanners[iHandle]->m_bFinish = 1;
  gOpenScanners[iHandle]->m_bCancelled = 1;

} /* sane_cancel */

/***********************************************************/

SANE_Status
sane_set_io_mode (SANE_Handle __sane_unused__ handle,
		  SANE_Bool __sane_unused__ non_blocking)
{

  return SANE_STATUS_UNSUPPORTED;

} /* sane_set_io_mode */

/***********************************************************/

SANE_Status
sane_get_select_fd (SANE_Handle __sane_unused__ handle,
		    SANE_Int __sane_unused__ * fd)
{

  return SANE_STATUS_UNSUPPORTED;

} /* sane_get_select_fd */

/***********************************************************/

/* Clears the contents of gKnownDevices and zeros it */
void
ClearKnownDevices ()
{

  int i;

  for (i = 0; i < MAX_SCANNERS; ++i)
    {

      if (gKnownDevices[i])
        {
          if (gKnownDevices[i]->m_pName) free ( gKnownDevices[i]->m_pName );
          if (gKnownDevices[i]->m_pModel) free ( gKnownDevices[i]->m_pModel );
          free ( gKnownDevices[i] );
        }
      gKnownDevices[i] = NULL;

    }

} /* ClearKnownDevices */

/***********************************************************/

/* print hex buffer to debug output */
void
HexDump (int debugLevel, const unsigned char *buf, size_t bufSize)
{

  unsigned int i, j;

  char itemBuf[16] = { 0 }, lineBuf[256] = { 0 };

  if (DBG_LEVEL < debugLevel)
    return;

  for (i = 0; i < bufSize; ++i)
    {

      if (!(i % 16))
        sprintf (lineBuf, "%p: ", (buf + i));

      sprintf (itemBuf, "%02x ", (const unsigned int) buf[i]);

      strncat (lineBuf, itemBuf, sizeof (lineBuf));

      if ((i + 1) % 16)
        continue;

      /* print string equivalent */
      for (j = i - 15; j <= i; ++j)
        {

      if ((buf[j] >= 0x20) && (!(buf[j] & 0x80)))
        {
          sprintf (itemBuf, "%c", buf[j]);
        }
      else
        {
          sprintf (itemBuf, ".");
        }
    strncat (lineBuf, itemBuf, sizeof (lineBuf));

    } /* for j */

      DBG (debugLevel, "%s\n", lineBuf);
      lineBuf[0] = 0;

    } /* for i */

  if (i % 16)
    {

      for (j = (i % 16); j < 16; ++j)
        {
          strncat (lineBuf, "   ", sizeof (lineBuf));
        }
      for (j = 1 + i - ((i + 1) % 16); j < i; ++j)
        {
          if ((buf[j] >= 0x20) && (!(buf[j] & 0x80)))
            {
              sprintf (itemBuf, "%c", buf[j]);
            }
          else
            {
              strcpy (itemBuf, ".");
            }
          strncat (lineBuf, itemBuf, sizeof (lineBuf));
        }
      DBG (debugLevel, "%s\n", lineBuf);
    }
} /* HexDump */

/***********************************************************/

/* initialise a ComBuf struct
   \return 0 on success, >0 on failure
*/
int
InitComBuf (struct ComBuf *pBuf)
{

  memset (pBuf, 0, sizeof (struct ComBuf));

  pBuf->m_pBuf = malloc (INITIAL_COM_BUF_SIZE);
  if (!pBuf->m_pBuf)
    return 1;

  pBuf->m_capacity = INITIAL_COM_BUF_SIZE;
  pBuf->m_used = 0;

  return 0;

} /* InitComBuf */

/***********************************************************/

/* free a ComBuf struct */
void
FreeComBuf (struct ComBuf *pBuf)
{

  if (pBuf->m_pBuf)
    free (pBuf->m_pBuf);
  memset (pBuf, 0, sizeof (struct ComBuf));

} /* FreeComBuf */

/***********************************************************/

/* add data to a ComBuf struct
   \return 0 on success, >0 on failure
   \note If pData is NULL then buffer size will be increased but no copying will take place
   \note In case of failure pBuf will be released using FreeComBuf 
*/
int
AppendToComBuf (struct ComBuf *pBuf, const unsigned char *pData,
		size_t datSize)
{

  size_t newSize;

  /* check we have enough space */
  if (pBuf->m_used + datSize > pBuf->m_capacity)
    {
      /* nope - allocate some more */
      newSize = pBuf->m_used + datSize + INITIAL_COM_BUF_SIZE;
      pBuf->m_pBuf = realloc (pBuf->m_pBuf, newSize);
      if (!pBuf->m_pBuf)
        {
          DBG (1, "AppendToComBuf: memory allocation error");
          FreeComBuf (pBuf);
          return (1);
        }
      pBuf->m_capacity = newSize;
    } /* if */

  /* add data */
  if (pData)
    memcpy (pBuf->m_pBuf + pBuf->m_used, pData, datSize);
  pBuf->m_used += datSize;

  return 0;

} /* AppendToComBuf */

/***********************************************************/

/* append message to a packet
   \return 0 if  ok, 1 if bad */
int
AppendMessageToPacket (struct ComBuf *pBuf,	/* packet to which to append */
		       char messageType,	/* type of message */
		       char *messageName,	/* name of message */
		       char valueType,	        /* type of value */
		       void *pValue,	        /* pointer to value */
		       size_t valueLen	        /* length of value (bytes) */
  )
{

  unsigned short slen;

  /* message type */
  AppendToComBuf (pBuf, (void *) &messageType, 1);

  /* message length */
  slen = htons (strlen (messageName));
  AppendToComBuf (pBuf, (void *) &slen, 2);

  /* and name */
  AppendToComBuf (pBuf, (void *) messageName, strlen (messageName));

  /* and value type */
  AppendToComBuf (pBuf, (void *) &valueType, 1);

  /* value length */
  slen = htons (valueLen);
  AppendToComBuf (pBuf, (void *) &slen, 2);

  /* and value */
  return (AppendToComBuf (pBuf, (void *) pValue, valueLen));

} /* AppendMessageToPacket */

/***********************************************************/

/* Initialise a packet
   \param pBuf : An initialise ComBuf
   \param type : either 0x01 ("normal" ) or 0x02 ("reply" )
   \return 0 on success, >0 otherwise
*/
int
InitPacket (struct ComBuf *pBuf, char type)
{

  char header[8] = { 2, 0, 0, 2, 0, 0, 0, 0 };

  header[2] = type;

  /* reset size */
  pBuf->m_used = 0;

  /* add header */
  return (AppendToComBuf (pBuf, (void *) &header, 8));

} /* InitPacket */

/***********************************************************/

/* write length data to packet header
*/
void
FinalisePacket (struct ComBuf *pBuf)
{

  /* sanity check */
  if (pBuf->m_used < 8)
    return;

  /* set the size */
  *((unsigned short *) (pBuf->m_pBuf + 6)) = htons (pBuf->m_used - 8);

  DBG (20, "FinalisePacket: outgoing packet:\n");
  HexDump (20, pBuf->m_pBuf, pBuf->m_used);

} /* FinalisePacket */

/***********************************************************/

/* \return 1 if message is complete, 0 otherwise */
int
MessageIsComplete (unsigned char *pData, size_t size)
{
  unsigned short dataSize;

  /* sanity check */
  if (size < 8)
    return 0;

  /* :NOTE: we can't just cast to a short as data may not be aligned */
  dataSize = (((unsigned short) pData[6]) << 8) | pData[7];

  DBG (20, "MessageIsComplete: data size = %d\n", dataSize);

  if (size >= (size_t) (dataSize + 8))
    return 1;
  else
    return 0;

} /* MessageIsComplete */

/***********************************************************/

/* process a registration broadcast response
   \return struct DeviceRecord pointer on success (caller frees), NULL on failure 
*/
struct DeviceRecord *
ProcessFindResponse (unsigned char *pData, size_t size)
{

  struct DeviceRecord *pDevice = NULL;
  unsigned short messageSize, nameSize, valueSize;
  unsigned char *pItem, *pEnd, *pValue;
  char printerName[256] = { 0 };
  char printerModel[256] = "1600n";
  char *pModel, *pName;


  DBG (10, "ProcessFindResponse: processing %lu bytes, pData=%p\n",
       (unsigned long)size, pData);

  /* check we have a complete packet */
  if (!MessageIsComplete (pData, size))
    {
      DBG (1, "ProcessFindResponse: Ignoring incomplete packet\n");
      return NULL;
    }

  /* extract data size */
  messageSize = (((unsigned short) (pData[6])) << 8) | pData[7];

  /* loop through items in message */
  pItem = pData + 8;
  pEnd = pItem + messageSize;
  while (pItem < pEnd)
    {

      pItem++;
      nameSize = (((unsigned short) pItem[0]) << 8) | pItem[1];
      pItem += 2;
      pName = (char *) pItem;

      pItem += nameSize;

      pItem++;
      valueSize = (((unsigned short) pItem[0]) << 8) | pItem[1];
      pItem += 2;

      pValue = pItem;

      pItem += valueSize;

      /* process the item */
      if (!strncmp ("std-scan-discovery-ip", pName, nameSize))
        {

          snprintf (printerName, sizeof (printerName), "%d.%d.%d.%d",
            (int) pValue[0],
            (int) pValue[1], (int) pValue[2], (int) pValue[3]);
          DBG (2, "%s\n", printerName);

        }
      else if (!strncmp ("std-scan-discovery-model-name", pName, nameSize))
        {

          memset (printerModel, 0, sizeof (printerModel));
          if (valueSize > (sizeof (printerModel) - 1))
            valueSize = sizeof (printerModel) - 1;
          memcpy (printerModel, pValue, valueSize);
          DBG (2, "std-scan-discovery-model-name: %s\n", printerModel);

        }

    } /* while pItem */

  /* just in case nothing sensible was found */
  if ( ! strlen( printerName ) ) return NULL;

  pDevice = malloc (sizeof (struct DeviceRecord));
  if (!pDevice)
    {
      DBG (1, "ProcessFindResponse: memory allocation failure\n");
      return NULL;
    }

  /* knock off "Dell " from start of model name */
  pModel = printerModel;
  if ( ! strncmp( pModel, "Dell ", 5 ) )
    pModel += 5;

  pDevice->m_pName = strdup( printerName );
  pDevice->m_device.vendor = "Dell";
  pDevice->m_pModel = strdup (pModel);
  pDevice->m_device.type = "multi-function peripheral";

  pDevice->m_device.name = pDevice->m_pName;
  pDevice->m_device.model = pDevice->m_pModel;

  return pDevice;

} /* ProcessFindResponse */

/***********************************************************/

/* frees a scanner state struct stored in gOpenScanners */
void
FreeScannerState (int iHandle)
{

  /* check range etc */
  if (!ValidScannerNumber (iHandle))
    return;

  /* close UDP handle */
  if (gOpenScanners[iHandle]->m_udpFd)
    close (gOpenScanners[iHandle]->m_udpFd);

  /* free m_buf */
  FreeComBuf (&gOpenScanners[iHandle]->m_buf);

  /* free m_imageData */
  FreeComBuf (&gOpenScanners[iHandle]->m_imageData);

  /* free the struct */
  free (gOpenScanners[iHandle]);

  /* set pointer to NULL */
  gOpenScanners[iHandle] = NULL;

} /* FreeScannerState */

/***********************************************************/

/* \return 1 if iHandle is a valid member of gOpenScanners, 0 otherwise */
int
ValidScannerNumber (int iHandle)
{
  /* check range */
  if ((iHandle < 0) || (iHandle >= MAX_SCANNERS))
    {
      DBG (1, "ValidScannerNumber: invalid scanner index %d", iHandle);
      return 0;
    }

  /* check non-NULL pointer */
  if (!gOpenScanners[iHandle])
    {
      DBG (1, "ValidScannerNumber: NULL scanner struct %d", iHandle);
      return 0;
    }

  /* OK */
  return 1;

} /* ValidScannerNumber */

/***********************************************************/

/* process UDP responses
   \return 0 in success, >0 otherwise */
static int
ProcessUdpResponse (unsigned char *pData, size_t size,
		    struct ScannerState *pState)
{

  unsigned short messageSize, nameSize, valueSize;
  unsigned char *pItem, *pEnd, *pValue;
  char sockBuf[SOCK_BUF_SIZE], *pName;
  struct ComBuf tcpBuf;
  int nread;
  unsigned int numUsed;

  HexDump (15, pData, size);

  DBG (10, "ProcessUdpResponse: processing %lu bytes, pData=%p\n",
       (unsigned long)size, pData);

  /* check we have a complete packet */
  if (!MessageIsComplete (pData, size))
    {
      DBG (1, "ProcessUdpResponse: Ignoring incomplete packet\n");
      return 1;
    }

  /* init a com buf for use in tcp communication */
  InitComBuf (&tcpBuf);

  /* extract data size */
  messageSize = (((unsigned short) (pData[6])) << 8) | pData[7];

  /* loop through items in message */
  pItem = pData + 8;
  pEnd = pItem + messageSize;
  while (pItem < pEnd)
    {

      pItem++;
      nameSize = (((unsigned short) pItem[0]) << 8) | pItem[1];
      pItem += 2;
      pName = (char *) pItem;

      pItem += nameSize;

      pItem++;
      valueSize = (((unsigned short) pItem[0]) << 8) | pItem[1];
      pItem += 2;

      pValue = pItem;

      pItem += valueSize;

      /* process the item */
      if (!strncmp ("std-scan-request-tcp-connection", pName, nameSize))
        {

          /* open TCP socket to scanner */
          if (!(pState->m_tcpFd = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)))
            {
              DBG (1, "ProcessUdpResponse: error opening TCP socket\n");
              return 2;
            }
          if (connect (pState->m_tcpFd,
                       (struct sockaddr *) &pState->m_sockAddr,
                       sizeof (pState->m_sockAddr)))
            {
              DBG (1,
                   "ProcessUdpResponse: error connecting to scanner TCP port\n");
              goto cleanup;
            }

          DBG (1, "ProcessUdpResponse: opened TCP connection to scanner\n");

          /* clear read buf */
          tcpBuf.m_used = 0;

          /* TCP read loop */
          while (1)
            {

              nread = read (pState->m_tcpFd, sockBuf, sizeof (sockBuf));

              if (nread <= 0)
                {
                  DBG (1, "ProcessUdpResponse: TCP read returned %d\n",
                       nread);
                  break;
                }

              /* append message to buffer */
              if (AppendToComBuf (&tcpBuf, (unsigned char *) sockBuf, nread))
                goto cleanup;

              /* process all available responses */
              while (tcpBuf.m_used)
                {

                  /* note the buffer size before the call */
                  numUsed = tcpBuf.m_used;

                  /* process the response */
                  if (ProcessTcpResponse (pState, &tcpBuf))
                    goto cleanup;

                  /* if the buffer size has not changed then assume no more processing is possible */
                  if (numUsed == tcpBuf.m_used)
                    break;

                } /* while */

            } /* while */

          close (pState->m_tcpFd);
          DBG (1, "ProcessUdpResponse: closed TCP connection to scanner\n");

          /* signal end of session */
          pState->m_bFinish = 1;

        } /* if */

    } /* while pItem */

  return 0;

cleanup:

  FreeComBuf (&tcpBuf);
  close (pState->m_tcpFd);
  return 3;

} /* ProcessUdpResponse */

/***********************************************************/

/* process TCP responses, \return 0 in success, >0 otherwise */
int
ProcessTcpResponse (struct ScannerState *pState, struct ComBuf *pTcpBuf)
{

  struct ComBuf buf;
  unsigned short messageSize = 0, nameSize, valueSize, dataChunkSize;
  unsigned char *pItem, *pEnd, *pValue;
  unsigned char *pData = pTcpBuf->m_pBuf;
  char *pName;
  unsigned int uiVal;
  int errorCheck = 0;
  int bProcessImage = 0;

  DBG (10, "ProcessTcpResponse: processing %lu bytes, pData=%p\n",
       (unsigned long)pTcpBuf->m_used, pData);
  HexDump (15, pData, pTcpBuf->m_used);

  /* if message not complete then wait for more to arrive */
  if (!MessageIsComplete (pData, pTcpBuf->m_used))
    {
      DBG (10, "ProcessTcpResponse: incomplete message, returning\n");
      return 0;
    }

  /* init a buffer for our outbound messages */
  if (InitComBuf (&buf))
    {
      errorCheck |= 1;
      goto cleanup;
    }

  /* extract data size */
  messageSize = (((unsigned short) (pData[6])) << 8) | pData[7];

  /* loop through items in message */
  pItem = pData + 8;
  pEnd = pItem + messageSize;
  while (pItem < pEnd)
    {

      pItem++;
      nameSize = (((unsigned short) pItem[0]) << 8) | pItem[1];
      pItem += 2;
      pName = (char *) pItem;

      pItem += nameSize;

      pItem++;
      valueSize = (((unsigned short) pItem[0]) << 8) | pItem[1];
      pItem += 2;

      pValue = pItem;

      pItem += valueSize;

      /* process the item */
      if (!strncmp ("std-scan-session-open", pName, nameSize))
        {

          errorCheck |= InitPacket (&buf, 0x02);
          uiVal = 0;
          errorCheck |=
            AppendMessageToPacket (&buf, 0x22,
                                   "std-scan-session-open-response", 0x05,
                                   &uiVal, sizeof (uiVal));
          FinalisePacket (&buf);
          send (pState->m_tcpFd, buf.m_pBuf, buf.m_used, 0);

        }
      else if (!strncmp ("std-scan-getclientpref", pName, nameSize))
        {

          errorCheck |= InitPacket (&buf, 0x02);
          uiVal = 0;
          errorCheck |=
            AppendMessageToPacket (&buf, 0x22, "std-scan-getclientpref-x1",
                                   0x05, &uiVal, sizeof (uiVal));
          errorCheck |=
            AppendMessageToPacket (&buf, 0x22, "std-scan-getclientpref-x2",
                                   0x05, &uiVal, sizeof (uiVal));
          errorCheck |=
            AppendMessageToPacket (&buf, 0x22, "std-scan-getclientpref-y1",
                                   0x05, &uiVal, sizeof (uiVal));
          errorCheck |=
            AppendMessageToPacket (&buf, 0x22, "std-scan-getclientpref-y2",
                                   0x05, &uiVal, sizeof (uiVal));
          errorCheck |=
            AppendMessageToPacket (&buf, 0x22,
                                   "std-scan-getclientpref-xresolution", 0x04,
                                   &pState->m_xres, sizeof (pState->m_xres));
          errorCheck |=
            AppendMessageToPacket (&buf, 0x22,
                                   "std-scan-getclientpref-yresolution", 0x04,
                                   &pState->m_yres, sizeof (pState->m_yres));
          errorCheck |=
            AppendMessageToPacket (&buf, 0x22,
                                   "std-scan-getclientpref-image-composition",
                                   0x06, &pState->m_composition,
                                   sizeof (pState->m_composition));
          errorCheck |=
            AppendMessageToPacket (&buf, 0x22,
                                   "std-scan-getclientpref-brightness", 0x02,
                                   &pState->m_brightness,
                                   sizeof (pState->m_brightness));
          errorCheck |=
            AppendMessageToPacket (&buf, 0x22,
                                   "std-scan-getclientpref-image-compression",
                                   0x06, &pState->m_compression,
                                   sizeof (pState->m_compression));
          errorCheck |=
            AppendMessageToPacket (&buf, 0x22,
                                   "std-scan-getclientpref-file-type", 0x06,
                                   &pState->m_fileType,
                                   sizeof (pState->m_fileType));
          errorCheck |=
            AppendMessageToPacket (&buf, 0x22,
                                   "std-scan-getclientpref-paper-size-detect",
                                   0x06, &uiVal, sizeof (uiVal));
          errorCheck |=
            AppendMessageToPacket (&buf, 0x22,
                                   "std-scan-getclientpref-paper-scanner-type",
                                   0x06, &uiVal, sizeof (uiVal));
          FinalisePacket (&buf);
          send (pState->m_tcpFd, buf.m_pBuf, buf.m_used, 0);

        }
      else if (!strncmp ("std-scan-document-start", pName, nameSize))
        {
          errorCheck |= InitPacket (&buf, 0x02);
          uiVal = 0;
          errorCheck |=
            AppendMessageToPacket (&buf, 0x22,
                                   "std-scan-document-start-response", 0x05,
                                   &uiVal, sizeof (uiVal));
          FinalisePacket (&buf);
          send (pState->m_tcpFd, buf.m_pBuf, buf.m_used, 0);
        }
      else if (!strncmp ("std-scan-document-file-type", pName, nameSize))
        {
          memcpy (&pState->m_fileType, pValue, sizeof (pState->m_fileType));
          DBG (5, "File type: %x\n", ntohl (pState->m_fileType));
        }
      else
        if (!strncmp ("std-scan-document-image-compression", pName, nameSize))
        {
          memcpy (&pState->m_compression, pValue,
                  sizeof (pState->m_compression));
          DBG (5, "Compression: %x\n", ntohl (pState->m_compression));

        }
      else if (!strncmp ("std-scan-document-xresolution", pName, nameSize))
        {
          memcpy (&pState->m_xres, pValue, sizeof (pState->m_xres));
          DBG (5, "X resolution: %d\n", ntohs (pState->m_xres));
        }
      else if (!strncmp ("std-scan-document-yresolution", pName, nameSize))
        {
          memcpy (&pState->m_yres, pValue, sizeof (pState->m_yres));
          DBG (5, "Y resolution: %d\n", ntohs (pState->m_yres));
        }
      else if (!strncmp ("std-scan-page-widthpixel", pName, nameSize))
        {
          if (1 || !pState->m_pixelWidth)
            {
              memcpy (&pState->m_pixelWidth, pValue,
                      sizeof (pState->m_pixelWidth));
              DBG (5, "Width: %d\n", ntohl (pState->m_pixelWidth));
            }
          else
            {
              DBG (5, "Ignoring width (already have a value)\n");
            }
        }
      else if (!strncmp ("std-scan-page-heightpixel", pName, nameSize))
        {
          if (1 || !pState->m_pixelHeight)
            {
              memcpy (&pState->m_pixelHeight, pValue,
                      sizeof (pState->m_pixelHeight));
              DBG (5, "Height: %d\n", ntohl (pState->m_pixelHeight));
            }
          else
            {
              DBG (5, "Ignoring height (already have a value)\n");
            }
        }
      else if (!strncmp ("std-scan-page-start", pName, nameSize))
        {
          errorCheck |= InitPacket (&buf, 0x02);
          uiVal = 0;
          errorCheck |=
            AppendMessageToPacket (&buf, 0x22, "std-scan-page-start-response",
                                   0x05, &uiVal, sizeof (uiVal));
          FinalisePacket (&buf);
          send (pState->m_tcpFd, buf.m_pBuf, buf.m_used, 0);

          /* reset the data buffer ready to store a new page */
          pState->m_buf.m_used = 0;

          /* init current page size */
          pState->m_currentPageBytes = 0;

          pState->m_pixelWidth = 0;
          pState->m_pixelHeight = 0;
        }
      else if (!strncmp ("std-scan-page-end", pName, nameSize))
        {
          bProcessImage = 1;

          errorCheck |= InitPacket (&buf, 0x02);
          uiVal = 0;
          errorCheck |=
            AppendMessageToPacket (&buf, 0x22, "std-scan-page-end-response",
                                   0x05, &uiVal, sizeof (uiVal));
          FinalisePacket (&buf);
          send (pState->m_tcpFd, buf.m_pBuf, buf.m_used, 0);
        }
      else if (!strncmp ("std-scan-document-end", pName, nameSize))
        {
          errorCheck |= InitPacket (&buf, 0x02);
          uiVal = 0;
          errorCheck |=
            AppendMessageToPacket (&buf, 0x22,
                                   "std-scan-document-end-response", 0x05,
                                   &uiVal, sizeof (uiVal));
          FinalisePacket (&buf);
          send (pState->m_tcpFd, buf.m_pBuf, buf.m_used, 0);

          /* reset the data buffer ready to store a new page */
          pState->m_buf.m_used = 0;
        }
      else if (!strncmp ("std-scan-session-end", pName, nameSize))
        {
          errorCheck |= InitPacket (&buf, 0x02);
          uiVal = 0;
          errorCheck |=
            AppendMessageToPacket (&buf, 0x22,
                                   "std-scan-session-end-response", 0x05,
                                   &uiVal, sizeof (uiVal));
          FinalisePacket (&buf);
          send (pState->m_tcpFd, buf.m_pBuf, buf.m_used, 0);

          /* initialise a shutodwn of the socket */
          shutdown (pState->m_tcpFd, SHUT_RDWR);
        }
      else if (!strncmp ("std-scan-scandata-error", pName, nameSize))
        {
          /* determine the size of data in this chunk */
          dataChunkSize = (pItem[6] << 8) + pItem[7];

          pItem += 8;

          DBG (10, "Reading %d bytes of scan data\n", dataChunkSize);

          /* append message to buffer */
          errorCheck |= AppendToComBuf (&pState->m_buf, pItem, dataChunkSize);

          pItem += dataChunkSize;

          DBG (10, "Accumulated %lu bytes of scan data so far\n",
               (unsigned long)pState->m_buf.m_used);
        } /* if */
    } /* while */

  /* process page data if required */ 
  if ( bProcessImage ) errorCheck |= ProcessPageData (pState);

cleanup:

  /* remove processed data (including 8 byte header) from start of tcp buffer */
  PopFromComBuf (pTcpBuf, messageSize + 8);

  /* free com buf */
  FreeComBuf (&buf);

  return errorCheck;

} /* ProcessTcpResponse */

/***********************************************************/

/* remove data from the front of a ComBuf struct
   \return 0 if sucessful, >0 otherwise
*/
int
PopFromComBuf (struct ComBuf *pBuf, size_t datSize)
{

  /* check if we're trying to remove more data than is present */
  if (datSize > pBuf->m_used)
    {
      pBuf->m_used = 0;
      return 1;
    }

  /* check easy cases */
  if ((!datSize) || (datSize == pBuf->m_used))
    {
      pBuf->m_used -= datSize;
      return 0;
    }

  /* move remaining memory contents to start */
  memmove (pBuf->m_pBuf, pBuf->m_pBuf + datSize, pBuf->m_used - datSize);

  pBuf->m_used -= datSize;
  return 0;

} /* PopFromComBuf */

/***********************************************************/

/* Process the data from a single scanned page, \return 0 in success, >0 otherwise */
int
ProcessPageData (struct ScannerState *pState)
{

  FILE *fTmp;
  int fdTmp;
  struct jpeg_source_mgr jpegSrcMgr;
  struct JpegDataDecompState jpegCinfo;
  struct jpeg_error_mgr jpegErr;
  int numPixels, iPixel, width, height, scanLineSize, imageBytes;
  int ret = 0;
  struct PageInfo pageInfo;

  JSAMPLE *pJpegLine = NULL;
  uint32 *pTiffRgba = NULL;
  unsigned char *pOut;
  char tiffErrBuf[1024];

  TIFF *pTiff = NULL;

  /* If there's no data then there's nothing to write */
  if (!pState->m_buf.m_used)
    return 0;

  DBG (1, "ProcessPageData: Got compression %x\n",
       ntohl (pState->m_compression));

  switch (ntohl (pState->m_compression))
    {

    case 0x20:
      /* decode as JPEG if appropriate */
      {

        jpegSrcMgr.resync_to_restart = jpeg_resync_to_restart;
        jpegSrcMgr.init_source = JpegDecompInitSource;
        jpegSrcMgr.fill_input_buffer = JpegDecompFillInputBuffer;
        jpegSrcMgr.skip_input_data = JpegDecompSkipInputData;
        jpegSrcMgr.term_source = JpegDecompTermSource;

        jpegCinfo.m_cinfo.err = jpeg_std_error (&jpegErr);
        jpeg_create_decompress (&jpegCinfo.m_cinfo);
        jpegCinfo.m_cinfo.src = &jpegSrcMgr;
        jpegCinfo.m_bytesRemaining = pState->m_buf.m_used;
        jpegCinfo.m_pData = pState->m_buf.m_pBuf;
        
        jpeg_read_header (&jpegCinfo.m_cinfo, TRUE);
        jpeg_start_decompress (&jpegCinfo.m_cinfo);
        
        /* allocate space for a single scanline */
        scanLineSize = jpegCinfo.m_cinfo.output_width
          * jpegCinfo.m_cinfo.output_components;
        DBG (1, "ProcessPageData: image dimensions: %d x %d, line size: %d\n",
        jpegCinfo.m_cinfo.output_width,
        jpegCinfo.m_cinfo.output_height, scanLineSize);

        pJpegLine = calloc (scanLineSize, sizeof (JSAMPLE));
        if (!pJpegLine)
          {
            DBG (1, "ProcessPageData: memory allocation error\n");
            ret = 1;
            goto JPEG_CLEANUP;
          } /* if */

        /* note dimensions - may be different from those previously reported */
        pState->m_pixelWidth = htonl (jpegCinfo.m_cinfo.output_width);
        pState->m_pixelHeight = htonl (jpegCinfo.m_cinfo.output_height);

        /* decode scanlines */
        while (jpegCinfo.m_cinfo.output_scanline
               < jpegCinfo.m_cinfo.output_height)
          {
            DBG (20, "Reading scanline %d of %d\n",
                 jpegCinfo.m_cinfo.output_scanline,
                 jpegCinfo.m_cinfo.output_height);

            /* read scanline */
            jpeg_read_scanlines (&jpegCinfo.m_cinfo, &pJpegLine, 1);

            /* append to output buffer */
            ret |= AppendToComBuf (&pState->m_imageData,
                                   pJpegLine, scanLineSize);

          } /* while */

        /* update info for this page */
        pageInfo.m_width = jpegCinfo.m_cinfo.output_width;
        pageInfo.m_height = jpegCinfo.m_cinfo.output_height;
        pageInfo.m_totalSize = pageInfo.m_width * pageInfo.m_height * 3;
        pageInfo.m_bytesRemaining = pageInfo.m_totalSize;

        DBG( 1, "Process page data: page %d: JPEG image: %d x %d, %d bytes\n",
             pState->m_numPages, pageInfo.m_width, pageInfo.m_height, pageInfo.m_totalSize );

        ret |= AppendToComBuf( & pState->m_pageInfo, (unsigned char*)& pageInfo, sizeof( pageInfo ) );
        ++( pState->m_numPages );

      JPEG_CLEANUP:
        jpeg_finish_decompress (&jpegCinfo.m_cinfo);
        jpeg_destroy_decompress (&jpegCinfo.m_cinfo);

        if (pJpegLine)
          free (pJpegLine);

        return ret;
      } /* case JPEG */

    case 0x08:
      /* CCITT Group 4 Fax data */
      {
        /* get a temp file
           :TODO: 2006-04-18: Use TIFFClientOpen and do everything in RAM
         */
        fTmp = tmpfile ();
        fdTmp = fileno (fTmp);

        pTiff = TIFFFdOpen (fdTmp, "tempfile", "w");
        if (!pTiff)
          {
            DBG (1, "ProcessPageData: Error opening temp TIFF file");
            ret = SANE_STATUS_IO_ERROR;
            goto TIFF_CLEANUP;
          }

        /* create a TIFF file */
        width = ntohl (pState->m_pixelWidth);
        height = ntohl (pState->m_pixelHeight);
        TIFFSetField (pTiff, TIFFTAG_IMAGEWIDTH, width);
        TIFFSetField (pTiff, TIFFTAG_IMAGELENGTH, height);
        TIFFSetField (pTiff, TIFFTAG_BITSPERSAMPLE, 1);
        TIFFSetField (pTiff, TIFFTAG_PHOTOMETRIC, 0);        /* 0 is white */
        TIFFSetField (pTiff, TIFFTAG_COMPRESSION, 4);        /* CCITT Group 4 */
        TIFFSetField (pTiff, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);

        TIFFWriteRawStrip (pTiff, 0, pState->m_buf.m_pBuf,
                           pState->m_buf.m_used);

        if (0 > TIFFRGBAImageOK (pTiff, tiffErrBuf))
          {
            DBG (1, "ProcessPageData: %s\n", tiffErrBuf);
            ret = SANE_STATUS_IO_ERROR;
            goto TIFF_CLEANUP;
          }

        /* allocate space for RGBA representation of image */
        numPixels = height * width;
        DBG (20, "ProcessPageData: num TIFF RGBA pixels: %d\n", numPixels);
        if (!(pTiffRgba = calloc (numPixels, sizeof (u_long))))
          {
            ret = SANE_STATUS_NO_MEM;
            goto TIFF_CLEANUP;
          }

        /* make space in image buffer to store the results */
        imageBytes = width * height * 3;
        ret |= AppendToComBuf (&pState->m_imageData, NULL, imageBytes);
        if (ret)
          goto TIFF_CLEANUP;

        /* get a pointer to the start of the output data */
        pOut = pState->m_imageData.m_pBuf
          + pState->m_imageData.m_used - imageBytes;

        /* read RGBA image */
        DBG (20, "ProcessPageData: setting up read buffer\n");
        TIFFReadBufferSetup (pTiff, NULL, width * height * sizeof (u_long));
        DBG (20, "ProcessPageData: reading RGBA data\n");
        TIFFReadRGBAImageOriented (pTiff, width, height, pTiffRgba,
                                   ORIENTATION_TOPLEFT, 0);

        /* loop over pixels */
        for (iPixel = 0; iPixel < numPixels; ++iPixel)
          {

            *(pOut++) = TIFFGetR (pTiffRgba[iPixel]);
            *(pOut++) = TIFFGetG (pTiffRgba[iPixel]);
            *(pOut++) = TIFFGetB (pTiffRgba[iPixel]);

          } /* for iRow */



        /* update info for this page */
        pageInfo.m_width = width;
        pageInfo.m_height = height;
        pageInfo.m_totalSize = pageInfo.m_width * pageInfo.m_height * 3;
        pageInfo.m_bytesRemaining = pageInfo.m_totalSize;

        DBG( 1, "Process page data: page %d: TIFF image: %d x %d, %d bytes\n",
             pState->m_numPages, width, height, pageInfo.m_totalSize );

        ret |= AppendToComBuf( & pState->m_pageInfo, (unsigned char*)& pageInfo, sizeof( pageInfo ) );
        ++( pState->m_numPages );

      TIFF_CLEANUP:
        if (pTiff)
          TIFFClose (pTiff);
        if (fTmp)
          fclose (fTmp);
        if (pTiffRgba)
          free (pTiffRgba);
        return ret;

      } /* case CCITT */
    default:
      /* this is not expected or very useful */
      {
        DBG (1, "ProcessPageData: Unexpected compression flag %d\n", ntohl (pState->m_compression));
        ret = SANE_STATUS_IO_ERROR;
      }
    } /* switch */

  return ret;
} /* ProcessPageData */

/***********************************************************/

void
JpegDecompInitSource (j_decompress_ptr cinfo)
/* Libjpeg decompression interface */
{
  cinfo->src->bytes_in_buffer = 0;

} /* JpegDecompInitSource */

/***********************************************************/

boolean
JpegDecompFillInputBuffer (j_decompress_ptr cinfo)
/* Libjpeg decompression interface */
{
  struct JpegDataDecompState *pState = (struct JpegDataDecompState *) cinfo;
  static const unsigned char eoiByte[] = {
    0xFF, JPEG_EOI
  };

  DBG (10, "JpegDecompFillInputBuffer: bytes remaining: %d\n",
       pState->m_bytesRemaining);

  if (!pState->m_bytesRemaining)
    {

      /* no input data available so return dummy data */
      cinfo->src->bytes_in_buffer = 2;
      cinfo->src->next_input_byte = (const JOCTET *) eoiByte;

    }
  else
    {

      /* point to data */
      cinfo->src->bytes_in_buffer = pState->m_bytesRemaining;
      cinfo->src->next_input_byte = (const JOCTET *) pState->m_pData;

      /* note that data is now gone */
      pState->m_bytesRemaining = 0;

    } /* if */

  return TRUE;

} /* JpegDecompFillInputBuffer */

/***********************************************************/

void
JpegDecompSkipInputData (j_decompress_ptr cinfo, long numBytes)
/* Libjpeg decompression interface */
{
  DBG (10, "JpegDecompSkipInputData: skipping %ld bytes\n", numBytes);

  cinfo->src->bytes_in_buffer -= numBytes;
  cinfo->src->next_input_byte += numBytes;

} /* JpegDecompSkipInputData */

/***********************************************************/

void
JpegDecompTermSource (j_decompress_ptr __sane_unused__ cinfo)
/* Libjpeg decompression interface */
{
  /* nothing to do */

} /* JpegDecompTermSource */

/***********************************************************/
