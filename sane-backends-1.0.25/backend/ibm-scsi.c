/* sane - Scanner Access Now Easy.
   
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
   If you do not wish that, delete this exception notice. */

/* 
*/

#include <time.h>


/* SCSI commands that the Ibm scanners understand: */
#define IBM_SCSI_TEST_UNIT_READY	0x00
#define IBM_SCSI_SET_WINDOW	        0x24
#define IBM_SCSI_GET_WINDOW	        0x25
#define IBM_SCSI_READ_SCANNED_DATA	0x28
#define IBM_SCSI_INQUIRY		0x12
#define IBM_SCSI_MODE_SELECT		0x15
#define IBM_SCSI_START_SCAN		0x1b
#define IBM_SCSI_MODE_SENSE		0x1a
#define IBM_SCSI_GET_BUFFER_STATUS	0x34
#define IBM_SCSI_OBJECT_POSITION      0x31

/* How long do we wait for scanner to have data for us */
#define MAX_WAITING_TIME       15

/* for object_position command */
#define OBJECT_POSITION_UNLOAD 0
#define OBJECT_POSITION_LOAD   1
   
struct scsi_window_cmd {
        SANE_Byte opcode;
        SANE_Byte byte2;
        SANE_Byte reserved[4];
        SANE_Byte len[3];
        SANE_Byte control;
};

struct scsi_mode_select_cmd {
        SANE_Byte opcode;
        SANE_Byte byte2;
#define SMS_SP  0x01
#define SMS_PF  0x10
        SANE_Byte page_code; /* for mode_sense, reserved for mode_select */
        SANE_Byte unused[1];
        SANE_Byte len;
        SANE_Byte control;
};

struct scsi_mode_header {
         SANE_Byte data_length;   /* Sense data length */
         SANE_Byte medium_type;
         SANE_Byte dev_spec;
         SANE_Byte blk_desc_len;
};

/* next struct introduced by mf */
struct scsi_object_position_cmd {
	SANE_Byte opcode;
	SANE_Byte position_func;
	SANE_Byte count[3];
	SANE_Byte res[3];
	SANE_Byte control;
	SANE_Byte res2;
};

struct scsi_get_buffer_status_cmd {
        SANE_Byte opcode;
        SANE_Byte byte2;
        SANE_Byte res[5];
        SANE_Byte len[2];
        SANE_Byte control;
};

struct scsi_status_desc {
        SANE_Byte window_id;
        SANE_Byte byte2;
        SANE_Byte available[3];
        SANE_Byte filled[3];
};

struct scsi_status_data {
        SANE_Byte len[3];
        SANE_Byte byte4;
        struct scsi_status_desc desc;
};

struct scsi_start_scan_cmd {
        SANE_Byte opcode;
        SANE_Byte byte2;
        SANE_Byte unused[2];
        SANE_Byte len;
        SANE_Byte control;
};

struct scsi_read_scanner_cmd {
        SANE_Byte opcode;
        SANE_Byte byte2;
        SANE_Byte data_type;
        SANE_Byte byte3;
        SANE_Byte data_type_qualifier[2];
        SANE_Byte len[3];
        SANE_Byte control;
};

static SANE_Status
test_unit_ready (int fd)
{
  static SANE_Byte cmd[6];
  SANE_Status status;
  DBG (11, ">> test_unit_ready\n");

  cmd[0] = IBM_SCSI_TEST_UNIT_READY;
  memset (cmd, 0, sizeof (cmd));
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), 0, 0);

  DBG (11, "<< test_unit_ready\n");
  return (status);
}

static SANE_Status
inquiry (int fd, void *buf, size_t  * buf_size)
{
  static SANE_Byte cmd[6];
  SANE_Status status;
  DBG (11, ">> inquiry\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = IBM_SCSI_INQUIRY;
  cmd[4] = *buf_size;  
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), buf, buf_size);

  DBG (11, "<< inquiry\n");
  return (status);
}

static SANE_Status
mode_select (int fd, struct mode_pages *mp)
{
  static struct {
    struct scsi_mode_select_cmd cmd;
    struct scsi_mode_header smh;
    struct mode_pages mp;
  } select_cmd;
  SANE_Status status;
  DBG (11, ">> mode_select\n");

  memset (&select_cmd, 0, sizeof (select_cmd));
  select_cmd.cmd.opcode = IBM_SCSI_MODE_SELECT;
  select_cmd.cmd.byte2 |= SMS_PF;
  select_cmd.cmd.len = sizeof(select_cmd.smh) + sizeof(select_cmd.mp);
/* next line by mf */
/*  select_cmd.cmd.page_code= 20; */
  memcpy (&select_cmd.mp, mp, sizeof(*mp));
  status = sanei_scsi_cmd (fd, &select_cmd, sizeof (select_cmd), 0, 0);

  DBG (11, "<< mode_select\n");
  return (status);
}

#if 0
static SANE_Status
mode_sense (int fd, struct mode_pages *mp, SANE_Byte page_code)
{
  static struct scsi_mode_select_cmd cmd; /* no type, we can reuse it for sensing */
  static struct {
    struct scsi_mode_header smh;
    struct mode_pages mp;
  } select_data;
  static size_t select_size = sizeof(select_data);
  SANE_Status status;
  DBG (11, ">> mode_sense\n");

  memset (&cmd, 0, sizeof (cmd));
  cmd.opcode = IBM_SCSI_MODE_SENSE;
  cmd.page_code = page_code;
  cmd.len = sizeof(select_data);
  status = sanei_scsi_cmd (fd, &cmd, sizeof (cmd), &select_data, &select_size);
  memcpy (mp, &select_data.mp, sizeof(*mp));

  DBG (11, "<< mode_sense\n");
  return (status);
}
#endif

static SANE_Status
trigger_scan (int fd)
{
  static struct scsi_start_scan_cmd cmd;
  static char   window_id_list[1] = { '\0' }; /* scan start data out */
  static size_t wl_size = 1;
  SANE_Status status;
  DBG (11, ">> trigger scan\n");

  memset (&cmd, 0, sizeof (cmd));
  cmd.opcode = IBM_SCSI_START_SCAN;
  cmd.len = wl_size;
/* next line by mf */
/* cmd.unused[0] = 1; */
  if (wl_size)
    status = sanei_scsi_cmd (fd, &cmd, sizeof (cmd), &window_id_list, &wl_size);
  else
    status = sanei_scsi_cmd (fd, &cmd, sizeof (cmd), 0, 0);

  DBG (11, "<< trigger scan\n");
  return (status);
}

static SANE_Status
set_window (int fd, struct ibm_window_data *iwd)
{

  static struct {
    struct scsi_window_cmd cmd;
    struct ibm_window_data iwd;
  } win;

  SANE_Status status;
  DBG (11, ">> set_window\n");

  memset (&win, 0, sizeof (win));
  win.cmd.opcode = IBM_SCSI_SET_WINDOW;
  _lto3b(sizeof(*iwd), win.cmd.len);
  memcpy (&win.iwd, iwd, sizeof(*iwd));
  status = sanei_scsi_cmd (fd, &win, sizeof (win), 0, 0);

  DBG (11, "<< set_window\n");
  return (status);
}

static SANE_Status
get_window (int fd, struct ibm_window_data *iwd)
{
 
  static struct scsi_window_cmd cmd;
  static size_t iwd_size;
  SANE_Status status;

  iwd_size = sizeof(*iwd);
  DBG (11, ">> get_window datalen = %lu\n", (unsigned long) iwd_size);

  memset (&cmd, 0, sizeof (cmd));
  cmd.opcode = IBM_SCSI_GET_WINDOW;
#if 1  /* it was if 0 */
  cmd.byte2 |= (SANE_Byte)0x01; /* set Single bit to get one window desc. */
#endif
  _lto3b(iwd_size, cmd.len);
  status = sanei_scsi_cmd (fd, &cmd, sizeof (cmd), iwd, &iwd_size);

  DBG (11, "<< get_window, datalen = %lu\n", (unsigned long) iwd_size);
  return (status);
}

static SANE_Status
read_data (int fd, void *buf, size_t * buf_size)
{
  static struct scsi_read_scanner_cmd cmd;
  SANE_Status status;
  DBG (11, ">> read_data %lu\n", (unsigned long) *buf_size);

  memset (&cmd, 0, sizeof (cmd));
  cmd.opcode = IBM_SCSI_READ_SCANNED_DATA;
  _lto3b(*buf_size, cmd.len);
  status = sanei_scsi_cmd (fd, &cmd, sizeof (cmd), buf, buf_size);

  DBG (11, "<< read_data %lu\n", (unsigned long) *buf_size);
  return (status);
}

static SANE_Status
object_position (int fd, int load)
{
  static struct scsi_object_position_cmd cmd;
  SANE_Status status;
  DBG (11, ">> object_position\n");

#if 0
  /* At least the Ricoh 420 doesn't like that command */
  DBG (11, "object_position: ignored\n");
  return SANE_STATUS_GOOD;
#endif

  memset (&cmd, 0, sizeof (cmd));
  cmd.opcode = IBM_SCSI_OBJECT_POSITION;
  if (load)
    cmd.position_func = OBJECT_POSITION_LOAD;
  else
    cmd.position_func = OBJECT_POSITION_UNLOAD;
  _lto3b(1, cmd.count);
  status = sanei_scsi_cmd (fd, &cmd, sizeof (cmd), 0, 0);

  DBG (11, "<< object_position\n");
  return (status);
}

static SANE_Status
get_data_status (int fd, struct scsi_status_desc *dbs)
{
  static struct scsi_get_buffer_status_cmd cmd;
  static struct scsi_status_data ssd;
  size_t ssd_size = sizeof(ssd);
  SANE_Status status;
  DBG (11, ">> get_data_status %lu\n", (unsigned long) ssd_size);

  memset (&cmd, 0, sizeof (cmd));
  cmd.opcode = IBM_SCSI_GET_BUFFER_STATUS;
  _lto2b(ssd_size, cmd.len);
  status = sanei_scsi_cmd (fd, &cmd, sizeof (cmd), &ssd, &ssd_size);

  memcpy (dbs, &ssd.desc, sizeof(*dbs));
  if (status == SANE_STATUS_GOOD && 
      ((unsigned int) _3btol(ssd.len) <= sizeof(*dbs) || _3btol(ssd.desc.filled) == 0)) {
    DBG (11, "get_data_status: busy\n");
    status = SANE_STATUS_DEVICE_BUSY;
  }

  DBG (11, "<< get_data_status %lu\n", (unsigned long) ssd_size);
  return (status);
}

#if 0
static SANE_Status
ibm_wait_ready_tur (int fd)
{
  struct timeval now, start;
  SANE_Status status;

  gettimeofday (&start, 0);

  while (1)
    {
      DBG(3, "scsi_wait_ready: sending TEST_UNIT_READY\n");

      status = sanei_scsi_cmd (fd, test_unit_ready, sizeof (test_unit_ready),
                               0, 0);
      switch (status)
        {
        default:
          /* Ignore errors while waiting for scanner to become ready.
             Some SCSI drivers return EIO while the scanner is
             returning to the home position.  */
          DBG(1, "scsi_wait_ready: test unit ready failed (%s)\n",
              sane_strstatus (status));
          /* fall through */
        case SANE_STATUS_DEVICE_BUSY:
          gettimeofday (&now, 0);
          if (now.tv_sec - start.tv_sec >= MAX_WAITING_TIME)
            {
              DBG(1, "ibm_wait_ready: timed out after %lu seconds\n",
                  (u_long) (now.tv_sec - start.tv_sec));
              return SANE_STATUS_INVAL;
            }
          usleep (100000);      /* retry after 100ms */
          break;

        case SANE_STATUS_GOOD:
          return status;
        }
    }
  return SANE_STATUS_INVAL;
}
#endif

static SANE_Status
ibm_wait_ready (Ibm_Scanner * s)
{
  struct scsi_status_desc dbs;
  time_t now, start;
  SANE_Status status;

  start = time(NULL);

  while (1)
    {
      status = get_data_status (s->fd, &dbs);

      switch (status)
        {
        default:
          /* Ignore errors while waiting for scanner to become ready.
             Some SCSI drivers return EIO while the scanner is
             returning to the home position.  */
          DBG(1, "scsi_wait_ready: get datat status failed (%s)\n",
              sane_strstatus (status));
          /* fall through */
        case SANE_STATUS_DEVICE_BUSY:
          now = time(NULL);
          if (now - start >= MAX_WAITING_TIME)
            {
              DBG(1, "ibm_wait_ready: timed out after %lu seconds\n",
                  (u_long) (now - start));
              return SANE_STATUS_INVAL;
            }
          break;

        case SANE_STATUS_GOOD:
	  DBG(11, "ibm_wait_ready: %d bytes ready\n", _3btol(dbs.filled));
	  return status;
	  break;
	}
      usleep (1000000);      /* retry after 100ms */
    }
  return SANE_STATUS_INVAL;
}
