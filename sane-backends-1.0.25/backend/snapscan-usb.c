/*
  Snapscan 1212U modifications for the Snapscan SANE backend
 
  Copyright (C) 2000 Henrik Johansson
 
  Henrik Johansson (henrikjo@post.urfors.se)

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

  This file implements USB equivalents to the SCSI routines used by the Snapscan
  backend.
  
  History

  0.1        2000-02-01

     First version released

  0.2   2000-02-12

     The Send Diagnostics SCSI command seems to hang some 1212U scanners.
     Bypassing this command fixes the problem. This bug was reported by
     Dmitri (dmitri@advantrix.com).

  0.3   2000-02-13

     The "Set window" command returns with status "Device busy" when the
     scanner is busy. One consequence is that some frontends exits with an
     error message if it's started when the scanner is warming up.
     A solution was suggested by Dmitri (dmitri@advantrix.com)
     The idea is that a SCSI command which returns "device busy" is stored
     in a "TODO" queue. The send command function is modified to first send
     commands in the queue before the intended command.
     So far this strategy has worked flawlessly. Thanks Dmitri!
*/

/* $Id$
   SnapScan backend scan data sources */

#include "snapscan-usb.h"
#include "snapscan-mutex.c"

#ifndef SHM_R
#define SHM_R 0
#endif

#ifndef SHM_W
#define SHM_W 0
#endif

/* Global variables */

static snapscan_mutex_t snapscan_mutex;
static sense_handler_type usb_sense_handler;
static void* usb_pss;

struct urb_counters_t {
    unsigned long read_urbs;
    unsigned long write_urbs;
};

static struct urb_counters_t* urb_counters = NULL;

/* Forward declarations */
static SANE_Status usb_request_sense(SnapScan_Scanner *pss);

static SANE_Status snapscani_usb_cmd(int fd, const void *src, size_t src_size,
                    void *dst, size_t * dst_size)
{
    static const char me[] = "snapscani_usb_cmd";
    int status;

    DBG (DL_CALL_TRACE, "%s(%d,0x%lx,%lu,0x%lx,0x%lx (%lu))\n", me,
         fd, (u_long) src,(u_long) src_size,(u_long) dst, (u_long) dst_size,(u_long) (dst_size ? *dst_size : 0));

    while(bqhead) {
        status = atomic_usb_cmd(fd, bqhead->src, bqhead->src_size, NULL, NULL);
        if(status == SANE_STATUS_DEVICE_BUSY) {
            if(is_queueable(src)) {
                enqueue_bq(fd,src,src_size);
                return SANE_STATUS_GOOD;
            } else {
                sleep(1);
                continue;
            }
        }
        dequeue_bq();
    }

    status = atomic_usb_cmd(fd,src,src_size,dst,dst_size);

    if ((status == SANE_STATUS_DEVICE_BUSY) && is_queueable(src) ) {
        enqueue_bq(fd,src,src_size);
        return SANE_STATUS_GOOD;
    }

    return status;
}

static SANE_Status atomic_usb_cmd(int fd, const void *src, size_t src_size,
                    void *dst, size_t * dst_size)
{
    static const char me[] = "atomic_usb_cmd";

    int status;
    sigset_t all,oldset;

    DBG (DL_CALL_TRACE, "%s(%d,0x%lx,%lu,0x%lx,0x%lx (%lu))\n", me,
         fd, (u_long) src,(u_long) src_size,(u_long) dst, (u_long) dst_size,(u_long) (dst_size ? *dst_size : 0));

    /* Prevent the calling process from being killed */
    sigfillset(&all);
    sigprocmask(SIG_BLOCK, &all, &oldset);

    /* Make sure we are alone */
    snapscani_mutex_lock(&snapscan_mutex);

    status = usb_cmd(fd,src,src_size,dst,dst_size);

    snapscani_mutex_unlock(&snapscan_mutex);

    /* Now it is ok to be killed */
    sigprocmask(SIG_SETMASK, &oldset, NULL);

    return status;

}

static SANE_Status snapscani_usb_open(const char *dev, int *fdp,
    sense_handler_type sense_handler, void* pss)
{
    static const char me[] = "snapscani_usb_open";

    DBG (DL_CALL_TRACE, "%s(%s)\n", me, dev);

    if(!snapscani_mutex_open(&snapscan_mutex, dev)) {
        DBG (DL_MAJOR_ERROR, "%s: Can't get semaphore\n", me);
        return SANE_STATUS_INVAL;
    }
    usb_sense_handler=sense_handler;
    usb_pss = pss;
    urb_counters->read_urbs = 0;
    urb_counters->write_urbs = 0;    
    return sanei_usb_open(dev, fdp);
}


static void snapscani_usb_close(int fd) {
    static const char me[] = "snapscani_usb_close";
    SANE_Word vendor_id, product_id;
    
    DBG (DL_CALL_TRACE, "%s(%d)\n", me, fd);
    DBG (DL_DATA_TRACE,"1st read %ld write %ld\n", urb_counters->read_urbs, urb_counters->write_urbs);
    
    /* Check if URB counting is needed. If yes, ensure the number of sent and
       received URBs is even.
       Odd number of URBs only cause problems with libusb and certain 
       scanner models. On other scanner models, sending additional commands
       seems to cause problems (e.g. 1212u_2).
       If sanei_usb_get_vendor_product returns an error there's probably no
       libusb, so everything's fine.
    */
    if (sanei_usb_get_vendor_product(fd, &vendor_id, &product_id) == SANE_STATUS_GOOD)
    {
        /* Exclude 1212u_2 */
        if (!((vendor_id == USB_VENDOR_AGFA) && (product_id == USB_PRODUCT_1212U2)))        
        {
            if ((urb_counters->read_urbs & 0x01) && (urb_counters->write_urbs & 0x01))
            {
                char cmd[] = {TEST_UNIT_READY, 0, 0, 0, 0, 0};
        
                snapscani_usb_cmd (fd, cmd, sizeof (cmd), NULL, 0);
            }
            else if (urb_counters->read_urbs & 0x01)
            {
                size_t read_bytes;
                char cmd[] = {TEST_UNIT_READY, 0, 0, 0, 0, 0};
                char cmd2[] = {INQUIRY, 0, 0, 0, 120, 0};
                char data[120];
        
                read_bytes = 120;
                snapscani_usb_cmd (fd, cmd2, sizeof (cmd2), data, &read_bytes);
                snapscani_usb_cmd (fd, cmd, sizeof (cmd), NULL, 0);
            }
            else if (urb_counters->write_urbs & 0x01)
            {
                size_t read_bytes;
                char cmd[] = {INQUIRY, 0, 0, 0, 120, 0};
                char data[120];
        
                read_bytes = 120;
                snapscani_usb_cmd (fd, cmd, sizeof (cmd), data, &read_bytes);
            }
            DBG (DL_DATA_TRACE,"2nd read %ld write %ld\n", urb_counters->read_urbs, 
                urb_counters->write_urbs);
        }
    }
    urb_counters->read_urbs = 0;
    urb_counters->write_urbs = 0;
    snapscani_mutex_close(&snapscan_mutex);
    sanei_usb_close(fd);
}

static int usb_cmdlen(int cmd)
{
    switch(cmd) {
    case TEST_UNIT_READY:
    case INQUIRY:
    case SCAN:
    case REQUEST_SENSE:
    case RESERVE_UNIT:
    case RELEASE_UNIT:
    case SEND_DIAGNOSTIC:
        return 6;
    case SEND:
    case SET_WINDOW:
    case READ:
    case GET_DATA_BUFFER_STATUS:
        return 10;
    }
    return 0;
}

static char *usb_debug_data(char *str,const char *data, int len) {
    char tmpstr[10];
    int i;

    str[0]=0;
    for(i=0; i < (len < 10 ? len : 10); i++) {
        sprintf(tmpstr," 0x%02x",((int)data[i]) & 0xff);
        if(i%16 == 0 && i != 0)
            strcat(str,"\n");
        strcat(str,tmpstr);
    }
    if(i < len)
        strcat(str," ...");
    return str;
}

#define RETURN_ON_FAILURE(x) if((status = x) != SANE_STATUS_GOOD) return status;

static SANE_Status usb_write(int fd, const void *buf, size_t n) {
    char dbgmsg[16384];
    SANE_Status status;
    size_t bytes_written = n;

    static const char me[] = "usb_write";
    DBG(DL_DATA_TRACE, "%s: writing: %s\n",me,usb_debug_data(dbgmsg,buf,n));

    status = sanei_usb_write_bulk(fd, (const SANE_Byte*)buf, &bytes_written);
    if(bytes_written != n) {
      DBG (DL_MAJOR_ERROR, "%s Only %lu bytes written\n",me, (u_long) bytes_written);
        status = SANE_STATUS_IO_ERROR;
    }
    urb_counters->write_urbs += (bytes_written + 7) / 8;
    DBG (DL_DATA_TRACE, "Written %lu bytes\n", (u_long) bytes_written);
    return status;
}

static SANE_Status usb_read(SANE_Int fd, void *buf, size_t n) {
    char dbgmsg[16384];
    static const char me[] = "usb_read";
    SANE_Status status;
    size_t bytes_read = n;

    status = sanei_usb_read_bulk(fd, (SANE_Byte*)buf, &bytes_read);
    if (bytes_read != n) {
        DBG (DL_MAJOR_ERROR, "%s Only %lu bytes read\n",me, (u_long) bytes_read);
        status = SANE_STATUS_IO_ERROR;
    }
    urb_counters->read_urbs += ((63 + bytes_read) / 64); 
    DBG(DL_DATA_TRACE, "%s: reading: %s\n",me,usb_debug_data(dbgmsg,buf,n));
    DBG(DL_DATA_TRACE, "Read %lu bytes\n", (u_long) bytes_read);
    return status;
}

static SANE_Status usb_read_status(int fd, int *scsistatus, int *transaction_status,
                                   char command)
{
    static const char me[] = "usb_read_status";
    unsigned char status_buf[8];
    int scsistat;
    int status;

    RETURN_ON_FAILURE(usb_read(fd,status_buf,8));

    if(transaction_status)
        *transaction_status = status_buf[0];

    scsistat = (status_buf[1] & STATUS_MASK) >> 1;

    if(scsistatus)
        *scsistatus = scsistat;

    switch(scsistat) {
    case GOOD:
        return SANE_STATUS_GOOD;
    case CHECK_CONDITION:
        if (usb_pss != NULL) {
            if (command != REQUEST_SENSE) {
                return usb_request_sense(usb_pss);
            }
            else {
                /* Avoid recursive calls of usb_request_sense */
                return SANE_STATUS_GOOD;
            }
        } else {
            DBG (DL_MAJOR_ERROR, "%s: scanner structure not set, returning default error\n",
                me);
            return SANE_STATUS_DEVICE_BUSY;
        }
        break;
    case BUSY:
        return SANE_STATUS_DEVICE_BUSY;
    default:
        return SANE_STATUS_IO_ERROR;
    }
}


static SANE_Status usb_cmd(int fd, const void *src, size_t src_size,
                    void *dst, size_t * dst_size)
{
  static const char me[] = "usb_cmd";
  int status,tstatus;
  int cmdlen,datalen;
  char command;

  DBG (DL_CALL_TRACE, "%s(%d,0x%lx,%lu,0x%lx,0x%lx (%lu))\n", me,
       fd, (u_long) src,(u_long) src_size,(u_long) dst, (u_long) dst_size,(u_long) (dst_size ? *dst_size : 0));

  /* Since the  "Send Diagnostic" command isn't supported by
     all Snapscan USB-scanners it's disabled .
  */
  command = ((const char *)src)[0];
  if(command == SEND_DIAGNOSTIC)
      return(SANE_STATUS_GOOD);

  cmdlen = usb_cmdlen(*((const char *)src));
  datalen = src_size - cmdlen;

  DBG(DL_DATA_TRACE, "%s: cmdlen=%d, datalen=%d\n",me,cmdlen,datalen);

  /* Send command to scanner */
  RETURN_ON_FAILURE( usb_write(fd,src,cmdlen) );

  /* Read status */
  RETURN_ON_FAILURE( usb_read_status(fd, NULL, &tstatus, command) );

  /* Send data only if the scanner is expecting it */
  if(datalen > 0 && (tstatus == TRANSACTION_WRITE)) {
      /* Send data to scanner */
      RETURN_ON_FAILURE( usb_write(fd, ((const SANE_Byte *) src) + cmdlen, datalen) );

      /* Read status */
      RETURN_ON_FAILURE( usb_read_status(fd, NULL, &tstatus, command) );
  }

  /* Receive data only when new data is waiting */
  if(dst_size && *dst_size && (tstatus == TRANSACTION_READ)) {
      RETURN_ON_FAILURE( usb_read(fd,dst,*dst_size) );

      /* Read status */
      RETURN_ON_FAILURE( usb_read_status(fd, NULL, &tstatus, command) );
  }

  if(tstatus != TRANSACTION_COMPLETED) {
      if(tstatus == TRANSACTION_WRITE)
          DBG(DL_MAJOR_ERROR,
              "%s: The transaction should now be completed, but the scanner is expecting more data" ,me);
      else
          DBG(DL_MAJOR_ERROR,
              "%s: The transaction should now be completed, but the scanner has more data to send" ,me);
      return SANE_STATUS_IO_ERROR;
  }

  return status;
}

/* Busy queue data structures and function implementations*/

static int is_queueable(const char *src)
{
    switch(src[0]) {
    case SEND:
    case SET_WINDOW:
    case SEND_DIAGNOSTIC:
        return 1;
    default:
        return 0;
    }
}

static struct usb_busy_queue *bqhead=NULL,*bqtail=NULL;
static int bqelements=0;

static int enqueue_bq(int fd,const void *src, size_t src_size)
{
    static const char me[] = "enqueue_bq";
    struct usb_busy_queue *bqe;

    DBG (DL_CALL_TRACE, "%s(%d,%p,%lu)\n", me, fd,src, (u_long) src_size);

    if((bqe = malloc(sizeof(struct usb_busy_queue))) == NULL)
        return -1;

    if((bqe->src = malloc(src_size)) == NULL)
        return -1;

    memcpy(bqe->src,src,src_size);
    bqe->src_size=src_size;

    bqe->next=NULL;

    if(bqtail) {
        bqtail->next=bqe;
        bqtail = bqe;
    } else
        bqhead = bqtail = bqe;

    bqelements++;
    DBG(DL_DATA_TRACE, "%s: Busy queue: elements=%d, bqhead=%p, bqtail=%p\n",
        me,bqelements,(void*)bqhead,(void*)bqtail);
    return 0;
}

static void dequeue_bq()
{
    static const char me[] = "dequeue_bq";
    struct usb_busy_queue *tbqe;

    DBG (DL_CALL_TRACE, "%s()\n", me);

    if(!bqhead)
        return;

    tbqe = bqhead;
    bqhead = bqhead->next;
    if(!bqhead)
        bqtail=NULL;

    if(tbqe->src)
        free(tbqe->src);
    free(tbqe);

    bqelements--;
    DBG(DL_DATA_TRACE, "%s: Busy queue: elements=%d, bqhead=%p, bqtail=%p\n",
        me,bqelements,(void*)bqhead,(void*)bqtail);
}

static SANE_Status usb_request_sense(SnapScan_Scanner *pss) {
    static const char *me = "usb_request_sense";
    size_t read_bytes = 0;
    u_char cmd[] = {REQUEST_SENSE, 0, 0, 0, 20, 0};
    u_char data[20];
    SANE_Status status;

    read_bytes = 20;

    DBG (DL_CALL_TRACE, "%s\n", me);
    status = usb_cmd (pss->fd, cmd, sizeof (cmd), data, &read_bytes);
    if (status != SANE_STATUS_GOOD)
    {
        DBG (DL_MAJOR_ERROR, "%s: usb command error: %s\n",
             me, sane_strstatus (status));
    }
    else
    {
        if (usb_sense_handler) {
            status = usb_sense_handler (pss->fd, data, (void *) pss);
        } else {
            DBG (DL_MAJOR_ERROR, "%s: No sense handler for USB\n", me);
            status = SANE_STATUS_UNSUPPORTED;
        }
    }
    return status;
}

#if defined USE_PTHREAD || defined HAVE_OS2_H || defined __BEOS__
static SANE_Status snapscani_usb_shm_init(void)
{
    unsigned int shm_size = sizeof(struct urb_counters_t);
    urb_counters = (struct urb_counters_t*) malloc(shm_size);
    if (urb_counters == NULL)
    {
        return SANE_STATUS_NO_MEM;
    }
    memset(urb_counters, 0, shm_size);
    return SANE_STATUS_GOOD;
        
}

static void snapscani_usb_shm_exit(void)
{
    if (urb_counters)
    {
        free ((void*)urb_counters);
        urb_counters = NULL;
    }
}
#else
#include <sys/ipc.h>
#include <sys/shm.h>
static SANE_Status snapscani_usb_shm_init(void)
{
    unsigned int shm_size = sizeof(struct urb_counters_t);
    void* shm_area = NULL;
    int shm_id = shmget (IPC_PRIVATE, shm_size, IPC_CREAT | SHM_R | SHM_W);
    if (shm_id == -1)
    {
        DBG (DL_MAJOR_ERROR, "snapscani_usb_shm_init: cannot create shared memory segment: %s\n",
            strerror (errno));
        return SANE_STATUS_NO_MEM;
    }
    
    shm_area = shmat (shm_id, NULL, 0);
    if (shm_area == (void *) -1)
    {
        DBG (DL_MAJOR_ERROR, "snapscani_usb_shm_init: cannot attach to shared memory segment: %s\n",
            strerror (errno));
        shmctl (shm_id, IPC_RMID, NULL);
        return SANE_STATUS_NO_MEM;
    }
    
    if (shmctl (shm_id, IPC_RMID, NULL) == -1)
    {
        DBG (DL_MAJOR_ERROR, "snapscani_usb_shm_init: cannot remove shared memory segment id: %s\n",
            strerror (errno));
        shmdt (shm_area);
        shmctl (shm_id, IPC_RMID, NULL);
        return SANE_STATUS_NO_MEM;
    }
    urb_counters = (struct urb_counters_t*) shm_area;
    memset(urb_counters, 0, shm_size);
    return SANE_STATUS_GOOD;
}

static void snapscani_usb_shm_exit(void)
{
    if (urb_counters)
    {
        shmdt (urb_counters);
        urb_counters = NULL;
    }
}
#endif
/*
 * $Log$
 * Revision 1.22  2006/01/26 17:42:30  hmg-guest
 * Added #defines for SHM_R/W for cygwin (patch from Philip Aston <philipa@mail.com>).
 *
 * Revision 1.21  2005/11/02 19:22:06  oliver-guest
 * Fixes for Benq 5000
 *
 * Revision 1.20  2005/07/18 17:37:37  oliver-guest
 * ZETA changes for SnapScan backend
 *
 * Revision 1.19  2004/10/03 17:34:36  hmg-guest
 * 64 bit platform fixes (bug #300799).
 *
 * Revision 1.18  2004/06/16 19:52:26  oliver-guest
 * Don't enforce even number of URB packages on 1212u_2. Fixes bug #300753.
 *
 * Revision 1.17  2004/06/06 14:50:36  oliver-guest
 * Use shared memory functions only when needed
 *
 * Revision 1.16  2004/05/26 22:37:01  oliver-guest
 * Use shared memory for urb counters in snapscan backend
 *
 * Revision 1.15  2004/04/09 11:59:02  oliver-guest
 * Fixes for pthread implementation
 *
 * Revision 1.14  2004/04/08 22:48:13  oliver-guest
 * Use URB counting in snapscan-usb.c (thanks to Jose Alberto Reguero)
 *
 * Revision 1.13  2003/11/08 09:50:27  oliver-guest
 * Fix TPO scanning range for Epson 1670
 *
 * Revision 1.12  2003/07/26 17:16:55  oliverschwartz
 * Changed licence to GPL + SANE exception for snapscan-usb.[ch]
 *
 * Revision 1.11  2002/07/12 23:29:06  oliverschwartz
 * SnapScan backend 1.4.15
 *
 * Revision 1.21  2002/07/12 22:52:42  oliverschwartz
 * use sanei_usb_read_bulk() and sanei_usb_write_bulk()
 *
 * Revision 1.20  2002/04/27 14:36:25  oliverschwartz
 * Pass a char as 'proj' argument for ftok()
 *
 * Revision 1.19  2002/04/10 21:00:33  oliverschwartz
 * Make bqelements static
 *
 * Revision 1.18  2002/03/24 12:16:09  oliverschwartz
 * Better error report in usb_read
 *
 * Revision 1.17  2002/02/05 19:32:39  oliverschwartz
 * Only define union semun if not already defined in <sys/sem.h>. Fixes
 * compilation bugs on Irix and FreeBSD. Fixed by Henning Meier-Geinitz.
 *
 * Revision 1.16  2002/01/14 21:11:56  oliverschwartz
 * Add workaround for bug semctl() call in libc for PPC
 *
 * Revision 1.15  2001/12/09 23:06:44  oliverschwartz
 * - use sense handler for USB if scanner reports CHECK_CONDITION
 *
 * Revision 1.14  2001/11/16 20:23:16  oliverschwartz
 * Merge with sane-1.0.6
 *   - Check USB vendor IDs to avoid hanging scanners
 *   - fix bug in dither matrix computation
 *
 * Revision 1.13  2001/10/09 22:34:23  oliverschwartz
 * fix compiler warnings
 *
 * Revision 1.12  2001/09/18 15:01:07  oliverschwartz
 * - Read scanner id string again after firmware upload
 *   to indentify correct model
 * - Make firmware upload work for AGFA scanners
 * - Change copyright notice
 *
 * */

