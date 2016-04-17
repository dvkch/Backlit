/* SANE - Scanner Access Now Easy.
 
   Copyright (C) 2008 by Louis Lagendijk

   This file is part of the SANE package.

   Data structures and definitions for
   bjnp backend for the Common UNIX Printing System (CUPS).

   These coded instructions, statements, and computer programs are the
   property of Louis Lagendijk and are protected by Federal copyright
   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
   "LICENSE" which should have been included with this file.  If this
   file is missing or damaged, see the license at "http://www.cups.org/".

   This file is subject to the Apple OS-Developed Software exception.

   SANE is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   SANE is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with sane; see the file COPYING.  If not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

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

/* 
 *  BJNP definitions 
 */

/* selection of options */
/* This works now, disable when it gives you problems */
#define PIXMA_BJNP_USE_STATUS 1 

/* sizes */

#define BJNP_PRINTBUF_MAX 1400		/* size of printbuffer */
#define BJNP_CMD_MAX 2048		/* size of BJNP response buffer */
#define BJNP_RESP_MAX 2048		/* size of BJNP response buffer */
#define BJNP_SOCK_MAX 256		/* maximum number of open sockets */
#define BJNP_MODEL_MAX 64		/* max allowed size for make&model */
#define BJNP_STATUS_MAX 256		/* max size for status string */
#define BJNP_IEEE1284_MAX 1024		/* max. allowed size of IEEE1284 id */
#define BJNP_METHOD_MAX 16		/* max length of method */
#define BJNP_HOST_MAX 128       	/* max length of hostname or address */
#define BJNP_PORT_MAX 64		/* max length of port string */
#define BJNP_ARGS_MAX 128		/* max length of argument string */
#define BJNP_SERIAL_MAX 16		/* maximum length of serial number */
#define BJNP_NO_DEVICES 16		/* max number of open devices */
#define BJNP_SCAN_BUF_MAX 65536		/* size of scanner data intermediate buffer */
#define BJNP_BLOCKSIZE_START 512	/* startsize for last block detection */

/* timers */
#define BJNP_BROADCAST_INTERVAL 10 	/* ms between broadcasts */
#define BJNP_BC_RESPONSE_TIMEOUT 500  	/* waiting time for broadc. responses */
#define BJNP_TIMEOUT_UDP 4 		/* standard UDP timeout in seconds */
#define BJNP_TIMEOUT_TCP 4		/* standard TCP timeout in seconds */
#define BJNP_USLEEP_MS 1000          	/* sleep for 1 msec */

/* retries */
#define BJNP_MAX_SELECT_ATTEMPTS 3   	/* max nr of retries on select (EINTR) */
#define BJNP_MAX_BROADCAST_ATTEMPTS 2	/* number of broadcast packets to be sent */
#define BJNP_UDP_RETRY_MAX 3		/* max nt of retries on a udp command */

#define bjnp_dbg DBG
#include "../include/sane/sanei_debug.h"

/* loglevel definitions */

#define LOG_CRIT 0
#define LOG_NOTICE 1
#define LOG_INFO 2
#define LOG_DEBUG 3
#define LOG_DEBUG2 4
#define LOG_DEBUG3 5

#define BJNP_RESTART_POLL -1

/*************************************/
/* BJNP protocol related definitions */
/*************************************/

/* port numbers */
typedef enum bjnp_port_e
{
  MFNP_PORT_SCAN = 8610,
  BJNP_PORT_PRINT = 8611,
  BJNP_PORT_SCAN = 8612,
  BJNP_PORT_3 = 8613,
  BJNP_PORT_4 = 8614
} bjnp_port_t;

typedef enum 
{
  PROTOCOL_BJNP = 0,
  PROTOCOL_MFNP = 1,
  PROTOCOL_NONE =2
} bjnp_protocol_t;

typedef struct 
{
  bjnp_protocol_t protocol_version;
  int default_port;
  char * proto_string;
  char * method_string;
} bjnp_protocol_defs_t;

bjnp_protocol_defs_t bjnp_protocol_defs[] = 
{
  {PROTOCOL_BJNP, BJNP_PORT_SCAN,"BJNP", "bjnp"},
  {PROTOCOL_MFNP, MFNP_PORT_SCAN,"MFNP", "mfnp"},
  {PROTOCOL_NONE, -1, NULL, NULL}
};

/* commands */
typedef enum bjnp_cmd_e
{
  CMD_UDP_DISCOVER = 0x01,	/* discover if service type is listening at this port */
  CMD_UDP_START_SCAN = 0x02,	/* start scan pressed, sent from scanner to 224.0.0.1 */
  CMD_UDP_JOB_DETAILS = 0x10,	/* send print/ scanner job owner details */
  CMD_UDP_CLOSE = 0x11,		/* request connection closure */
  CMD_UDP_GET_STATUS = 0x20,	/* get printer status  */
  CMD_TCP_REQ = 0x20,		/* read data from device */
  CMD_TCP_SEND = 0x21,		/* send data to device */
  CMD_UDP_GET_ID = 0x30,	/* get printer identity */
  CMD_UDP_POLL = 0x32		/* poll scanner for button status */
} bjnp_cmd_t;

/* command type */

typedef enum uint8_t
{
  BJNP_CMD_PRINT = 0x1,		/* printer command */
  BJNP_CMD_SCAN = 0x2,		/* scanner command */
  BJNP_RES_PRINT = 0x81,	/* printer response */
  BJNP_RES_SCAN = 0x82		/* scanner response */
} bjnp_cmd_type_t;

/***************************/
/* BJNP protocol structure */
/***************************/

/* The common protocol header */

struct  __attribute__ ((__packed__)) BJNP_command
{
  char BJNP_id[4];		/* string: BJNP */
  uint8_t dev_type;		/* 1 = printer, 2 = scanner */
                                /* responses have MSB set */
  uint8_t cmd_code;		/* command code/response code */
  int16_t unknown1;		/* unknown, always 0? */
  int16_t seq_no;		/* sequence number */
  uint16_t session_id;		/* session id for printing */
  uint32_t payload_len;		/* length of command buffer */
};

/* Layout of the init response buffer */

struct  __attribute__ ((__packed__)) DISCOVER_RESPONSE
{
  struct BJNP_command response;	/* reponse header */
  char unknown1[4];		/* 00 01 08 00 */
  char mac_len;			/* length of mac address */
  char addr_len;		/* length of address field */
  unsigned char mac_addr[6];	/* printers mac address */
  union  {
    struct __attribute__ ((__packed__)) {
       unsigned char ipv4_addr[4];
     } ipv4;
     struct  __attribute__ ((__packed__)) {
       unsigned char ipv6_addr_1[16];	
       unsigned char ipv6_addr_2[16];
     } ipv6;
  } addresses;
};

/* layout of payload for the JOB_DETAILS command */

struct  __attribute__ ((__packed__)) JOB_DETAILS
{
  struct BJNP_command cmd;	/* command header */
  char unknown[8];		/* don't know what these are for */
  char hostname[64];		/* hostname of sender */
  char username[64];		/* username */
  char jobtitle[256];		/* job title */
};

/* layout of the poll command, not everything is complete */

struct  __attribute__ ((__packed__)) POLL_DETAILS
{
  struct BJNP_command cmd;      /* command header */
  uint16_t type;                /* 0, 1, 2 or 5 */
                                /* 05 = reset status */
  union {
    struct  __attribute__ ((__packed__)) {
	char empty0[78];	/* type 0 has only 0 */
      } type0;			/* length = 80 */

    struct __attribute__ ((__packed__)) {
      char empty1[6];		/* 0 */
      char user_host[64];       /* unicode user <space> <space> hostname */
      uint64_t emtpy2;		/* 0 */
    } type1;			/* length = 80 */ 

    struct __attribute__ ((__packed__)) {
      uint16_t empty_1;         /* 00 00 */
      uint32_t dialog;          /* constant dialog id, from previous response */
      char user_host[64];       /* unicode user <space> <space> hostname */
      uint32_t unknown_1;	/* 00 00 00 14 */
      uint32_t empty_2[5];      /* only 0 */
      uint32_t unknown_2;	/* 00 00 00 10 */
      char ascii_date[16];      /* YYYYMMDDHHMMSS  only for type 2 */
    } type2;			/* length = 116 */

    struct __attribute__ ((__packed__)) {
      uint16_t empty_1;         /* 00 00 */
      uint32_t dialog;          /* constant dialog id, from previous response */
      char user_host[64];       /* unicode user <space> <space> hostname */
      uint32_t unknown_1;	/* 00 00 00 14 */
      uint32_t key;		/* copied from key field in status msg */
      uint32_t unknown_3[5];    /* only 0 */
    } type5;			/* length = 100 */

  } extensions;
};

/* the poll response layout */

struct  __attribute__ ((__packed__)) POLL_RESPONSE
{
  struct BJNP_command cmd;	/* command header */
  
  unsigned char result[4];	/* unknown stuff, result[2] = 80 -> status is available*/
                                /* result[8] is dialog, size? */
  uint32_t dialog;		/* to be returned in next request */
  uint32_t unknown_2;		/* returns the 00 00 00 14 from unknown_2 in request */
  uint32_t key;			/* to be returned in type 5 status reset */
  unsigned char status[20];	/* interrupt status */
};

/* Layout of ID and status responses */

struct  __attribute__ ((__packed__)) IDENTITY
{
  struct BJNP_command cmd;
  union  __attribute__ ((__packed__)) 
    {
      struct __attribute__ ((__packed__)) payload_s
        {
          uint16_t id_len;		/* length of identity */
          char id[BJNP_IEEE1284_MAX];	/* identity */
        } bjnp;
      struct __attribute__ ((__packed__)) mfnp
        {
          char id[BJNP_IEEE1284_MAX];
         } mfnp;
    } payload;
};


/* response to TCP print command */

struct  __attribute__ ((__packed__)) SCAN_BUF
{
  struct BJNP_command cmd;
  char scan_data[65536];
};

/**************************/
/* Local enum definitions */
/**************************/

typedef enum bjnp_paper_status_e
{
  BJNP_PAPER_UNKNOWN = -1,
  BJNP_PAPER_OK = 0,
  BJNP_PAPER_OUT = 1
} bjnp_paper_status_t;

typedef enum
{
  BJNP_STATUS_GOOD,
  BJNP_STATUS_INVAL,
  BJNP_STATUS_ALREADY_ALLOCATED
} BJNP_Status;

/* button polling */

typedef enum
{
  BJNP_POLL_STOPPED = 0,
  BJNP_POLL_STARTED = 1,
  BJNP_POLL_STATUS_RECEIVED = 2
} BJNP_polling_status_e;

typedef union
{
  struct sockaddr_storage storage;
  struct sockaddr addr;
  struct sockaddr_in ipv4;
  struct sockaddr_in6 ipv6;
} bjnp_sockaddr_t;

typedef enum
{
  BJNP_ADDRESS_IS_LINK_LOCAL = 0,
  BJNP_ADDRESS_IS_GLOBAL = 1,
  BJNP_ADDRESS_HAS_FQDN = 2
} bjnp_address_type_t;


/*
 * Device information for opened devices
 */

typedef struct device_s
{
  int open;			/* connection to scanner is opened */
  
  /* protocol version */
  int protocol;
  char *protocol_string;

  /* sockets */

  int tcp_socket;		/* open tcp socket for communcation to scannner */
  int16_t serial;		/* sequence number of command */

  /* communication state */

  int session_id;		/* session id used in bjnp protocol for TCP packets */
  int last_cmd;			/* last command sent */

  /* TCP bulk read state information */

  size_t blocksize;		/* size of (TCP) blocks returned by the scanner */
  size_t scanner_data_left;	/* TCP data left from last read request */
  char last_block;		/* last TCP read command was shorter than blocksize */

  /* device information */
  char mac_address[BJNP_HOST_MAX];
 		 		/* mac-address, used as device serial no */
  bjnp_sockaddr_t * addr;	/* ip-address of the scanner */
  int address_level;		/* link local, public or has a FQDN */
  int bjnp_timeout;		/* timeout (msec) for next poll command */

#ifdef PIXMA_BJNP_USE_STATUS
  /* polling state information */

  char polling_status;		/* status polling ongoing */
  uint32_t dialog;		/* poll dialog */
  uint32_t status_key;		/* key of last received status message */
#endif
} bjnp_device_t;

