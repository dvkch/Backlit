/* sane - Scanner Access Now Easy.
   Copyright (C) 1997 BYTEC GmbH Germany
   Written by Helmut Koeberle, Email: helmut.koeberle@bytec.de
   Modified by Manuel Panea <Manuel.Panea@rzg.mpg.de>
   and Markus Mertinat <Markus.Mertinat@Physik.Uni-Augsburg.DE>

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

/* This file implements the low-level scsi-commands.  */

static SANE_Status
test_unit_ready (int fd)
{
  static u_char cmd[6];
  int status;
  DBG (31, ">> test_unit_ready\n");

  memset (cmd, 0, sizeof (cmd));
  status = sanei_scsi_cmd2 (fd, cmd, sizeof (cmd), NULL, 0, NULL, NULL);

  DBG (31, "<< test_unit_ready\n");
  return (status);
}

#ifdef IMPLEMENT_ALL_SCANNER_SCSI_COMMANDS
static SANE_Status
request_sense (int fd, void *buf, size_t *buf_size)
{
  static u_char cmd[6];
  int status;
  DBG (31, ">> request_sense\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0x03;
  cmd[4] = 14;
  status = sanei_scsi_cmd2 (fd, cmd, sizeof (cmd), NULL, 0, buf, buf_size);

  DBG (31, "<< request_sense\n");
  return (status);
}
#endif

static SANE_Status
inquiry (int fd, int evpd, void *buf, size_t *buf_size)
{
  static u_char cmd[6];
  int status;
  DBG (31, ">> inquiry\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0x12;
  cmd[1] = evpd;
  cmd[2] = evpd ? 0xf0 : 0;
  cmd[4] = evpd ? 74 : 36;
  status = sanei_scsi_cmd2 (fd, cmd, sizeof (cmd), NULL, 0, buf, buf_size);

  DBG (31, "<< inquiry\n");
  return (status);
}

#ifdef IMPLEMENT_ALL_SCANNER_SCSI_COMMANDS
static SANE_Status
mode_select (int fd)
{
  static u_char cmd[6 + 12];
  int status;
  DBG (31, ">> mode_select\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0x15;
  cmd[1] = 16;
  cmd[4] = 12;
  cmd[6 + 4] = 3;
  cmd[6 + 5] = 6;
  cmd[6 + 8] = 0x02;
  cmd[6 + 9] = 0x58;
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), NULL, NULL);

  DBG (31, "<< mode_select\n");
  return (status);
}
#endif

static SANE_Status
reserve_unit (int fd)
{
  static u_char cmd[6];
  int status;
  DBG (31, ">> reserve_unit\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0x16;
  status = sanei_scsi_cmd2 (fd, cmd, sizeof (cmd), NULL, 0, NULL, NULL);

  DBG (31, "<< reserve_unit\n");
  return (status);
}

#ifdef IMPLEMENT_ALL_SCANNER_SCSI_COMMANDS
static SANE_Status
release_unit (int fd)
{
  static u_char cmd[6];
  int status;
  DBG (31, ">> release_unit\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0x17;
  status = sanei_scsi_cmd2 (fd, cmd, sizeof (cmd), NULL, 0, NULL, NULL);

  DBG (31, "<< release_unit\n");
  return (status);
}
#endif

static SANE_Status
mode_sense (int fd, void *buf, size_t *buf_size)
{
  static u_char cmd[6];
  int status;
  DBG (31, ">> mode_sense\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0x1a;
  cmd[2] = 3;
  cmd[4] = 12;
  status = sanei_scsi_cmd2 (fd, cmd, sizeof (cmd), NULL, 0, buf, buf_size);

  DBG (31, "<< mode_sense\n");
  return (status);
}

static SANE_Status
scan (int fd)
{
  static u_char cmd[6 + 1];
  int status;
  DBG (31, ">> scan\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0x1b;
  cmd[4] = 1;
  status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), NULL, NULL);

  DBG (31, "<< scan\n");
  return (status);
}

static SANE_Status
send_diagnostic (int fd)
{
  static u_char cmd[6];
  int status;
  DBG (31, ">> send_diagnostic\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0x1d;
  cmd[1] = 4;
  status = sanei_scsi_cmd2 (fd, cmd, sizeof (cmd), NULL, 0, NULL, NULL);

  DBG (31, "<< send_diagnostic\n");
  return (status);
}

static SANE_Status
set_window (int fd, void *data)
{
  static u_char cmd[10];
  int status;
  DBG (31, ">> set_window\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0x24;
  cmd[8] = 72;
  status = sanei_scsi_cmd2 (fd, cmd, sizeof (cmd), data, 72, NULL, NULL);

  DBG (31, "<< set_window\n");
  return (status);
}

static SANE_Status
get_window (int fd, void *buf, size_t *buf_size)
{
  static u_char cmd[10];
  int status;
  DBG (31, ">> get_window\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0x25;
  cmd[1] = 1;
  cmd[8] = 72;
  status = sanei_scsi_cmd2 (fd, cmd, sizeof (cmd), NULL, 0, buf, buf_size);

  DBG (31, "<< get_window\n");
  return (status);
}

static SANE_Status
read_data (int fd, void *buf, size_t *buf_size)
{
  static u_char cmd[10];
  int status;
  DBG (31, ">> read_data\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0x28;
  cmd[6] = *buf_size >> 16;
  cmd[7] = *buf_size >> 8;
  cmd[8] = *buf_size;
  status = sanei_scsi_cmd2 (fd, cmd, sizeof (cmd), NULL, 0, buf, buf_size);

  DBG (31, "<< read_data\n");
  return (status);
}

static SANE_Status
medium_position (int fd)
{
  static u_char cmd[10];
  int status;
  DBG (31, ">> medium_position\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0x31;
  status = sanei_scsi_cmd2 (fd, cmd, sizeof (cmd), NULL, 0, NULL, NULL);

  DBG (31, "<< medium_position\n");
  return (status);
}

#ifdef IMPLEMENT_ALL_SCANNER_SCSI_COMMANDS
static SANE_Status
execute_shading (int fd)
{
  static u_char cmd[10];
  int status;
  DBG (31, ">> execute shading\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0xe2;
  status = sanei_scsi_cmd2 (fd, cmd, sizeof (cmd), NULL, 0, NULL, NULL);

  DBG (31, "<< execute shading\n");
  return (status);
}
#endif

static SANE_Status
execute_auto_focus (int fd, int AF, int speed, int AE, int count)
{
  static u_char cmd[10];
  int status;
  DBG (7, ">> execute_auto_focus\n");
  DBG (7, ">> focus: mode='%d', speed='%d', AE='%d', count='%d'\n",
       AF, speed, AE, count);

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0xe0;
  cmd[1] = (u_char) AF;
  cmd[2] = (u_char) ((speed << 7) | AE);
#if 1
  cmd[4] = (u_char) count;		/* seems to work, but may be unsafe */
#else					/* The Canon software uses this: */
  cmd[4] = (u_char) (28 * ((int) (count / 28.5)) + 16);
#endif
  status = sanei_scsi_cmd2 (fd, cmd, sizeof (cmd), NULL, 0, NULL, NULL);

  DBG (7, "<< execute_auto_focus\n");
  return (status);
}

static SANE_Status
set_adf_mode (int fd, u_char priority)
{
  static u_char cmd[6];
  int status;

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0xd4;
  cmd[4] = 0x01;

  status = sanei_scsi_cmd2 (fd, cmd, sizeof (cmd), &priority, 1, NULL, NULL);

  return (status);
}

static SANE_Status
get_scan_mode (int fd, u_char page, void *buf, size_t *buf_size)
{
  static u_char cmd[6];
  int status;
  int PageLen = 0x00;

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0xd5;
  cmd[2] = page;

  switch (page)
    {
    case AUTO_DOC_FEEDER_UNIT:
    case TRANSPARENCY_UNIT:
      cmd[4] = 0x0c + PageLen;
      break;

    case SCAN_CONTROL_CONDITIONS:
      cmd[4] = 0x14 + PageLen;
      break;

    case SCAN_CONTROL_CON_FB1200:
      cmd[2] = 0x20;
      cmd[4] = 0x17 + PageLen;
      break;

    default:
      cmd[4] = 0x24 + PageLen;
      break;
    }

  DBG (31, "get scan mode: cmd[4]='0x%0X'\n", cmd[4]);
  status = sanei_scsi_cmd2 (fd, cmd, sizeof (cmd), NULL, 0, buf, buf_size);

  DBG (31, "<< get scan mode\n");
  return (status);
}

static SANE_Status
define_scan_mode (int fd, u_char page, void *data)
{
  static u_char cmd[6];
  u_char pdata[36];
  size_t i;
  int status, pdatalen;
  DBG (31, ">> define scan mode\n");

  memset (cmd, 0, sizeof (cmd));
  memset (pdata, 0, sizeof (pdata));
  cmd[0] = 0xd6;
  cmd[1] = 0x10;
  cmd[4] = (page == TRANSPARENCY_UNIT) ? 0x0c
    : (page == TRANSPARENCY_UNIT_FB1200) ? 0x0c
    : (page == SCAN_CONTROL_CONDITIONS) ? 0x14
    : (page == SCAN_CONTROL_CON_FB1200) ? 0x17 : 0x24;

  memcpy (pdata + 4, data, (page == TRANSPARENCY_UNIT) ? 8
    : (page == TRANSPARENCY_UNIT_FB1200) ? 10
    : (page == SCAN_CONTROL_CONDITIONS) ? 16
    : (page == SCAN_CONTROL_CON_FB1200) ? 19 : 32);

  for (i = 0; i < sizeof (cmd); i++)
    DBG (31, "define scan mode: cmd[%d]='0x%0X'\n", (int) i,
    cmd[i]);

  for (i = 0; i < sizeof (pdata); i++)
    DBG (31, "define scan mode: pdata[%d]='0x%0X'\n", (int) i,
    pdata[i]);

  pdatalen = (page == TRANSPARENCY_UNIT) ? 12
    : (page == TRANSPARENCY_UNIT_FB1200) ? 14
    : (page == SCAN_CONTROL_CONDITIONS) ? 20
    : (page == SCAN_CONTROL_CON_FB1200) ? 23 : 36;

  status = sanei_scsi_cmd2 (fd, cmd, sizeof (cmd), pdata, pdatalen, NULL,
    NULL);
  DBG (31, "<< define scan mode\n");
  return (status);
}

static SANE_Status
get_density_curve (int fd, int component, void *buf, size_t *buf_size,
		   int transfer_data_type)
{
  static u_char cmd[10];
  int status;
  DBG (31, ">> get_density_curve\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0x28;
  cmd[2] = transfer_data_type;
  cmd[4] = component;
  cmd[5] = 0;
  cmd[6] = ((*buf_size) >> 16) & 0xff;
  cmd[7] = ((*buf_size) >> 8) & 0xff;
  cmd[8] = (*buf_size) & 0xff;
  status = sanei_scsi_cmd2 (fd, cmd, sizeof (cmd), NULL, 0, buf, buf_size);

  DBG (31, "<< get_density_curve\n");
  return (status);
}

#ifdef IMPLEMENT_ALL_SCANNER_SCSI_COMMANDS
static SANE_Status
get_density_curve_data_format (int fd, void *buf, size_t *buf_size)
{
  static u_char cmd[10];
  int status;
  DBG (31, ">> get_density_curve_data_format\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0x28;
  cmd[2] = 0x03;
  cmd[4] = 0xff;
  cmd[5] = 0;
  cmd[6] = 0;
  cmd[7] = 0;
  cmd[8] = 14;
  status = sanei_scsi_cmd2 (fd, cmd, sizeof (cmd), NULL, 0, buf, buf_size);

  DBG (31, "<< get_density_curve_data_format\n");
  return (status);
}
#endif

static SANE_Status
set_density_curve (int fd, int component, void *buf, size_t *buf_size,
		   int transfer_data_type)
{
  static u_char cmd[10];
  int status;
  DBG (31, ">> set_density_curve\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0x2a;
  cmd[2] = transfer_data_type;
  cmd[4] = component;
  cmd[5] = 0;
  cmd[6] = ((*buf_size) >> 16) & 0xff;
  cmd[7] = ((*buf_size) >> 8) & 0xff;
  cmd[8] = (*buf_size) & 0xff;

  status =
    sanei_scsi_cmd2 (fd, cmd, sizeof (cmd), buf, *buf_size, NULL, NULL);

  DBG (31, "<< set_density_curve\n");
  return (status);
}


/* static SANE_Status */
/* set_density_curve_data_format (int fd, void *buf, size_t *buf_size) */
/* { */
/*   static u_char cmd[10]; */
/*   int status, i; */
/*   DBG (31, ">> set_density_curve_data_format\n"); */

/*   memset (cmd, 0, sizeof (cmd)); */
/*   cmd[0] = 0x2a; */
/*   cmd[2] = 0x03; */
/*   cmd[4] = 0xff; */
/*   cmd[5] = 0; */
/*   cmd[6] = 0; */
/*   cmd[7] = 0; */
/*   cmd[8] = 14; */
/*   status = sanei_scsi_cmd (fd, cmd, sizeof (cmd), buf, buf_size); */

/*   DBG (31, "<< set_density_curve_data_format\n"); */
/*   return (status); */
/* } */

#ifdef IMPLEMENT_ALL_SCANNER_SCSI_COMMANDS
static SANE_Status
get_power_on_timer (int fd, void *buf, size_t *buf_size)
{
  static u_char cmd[10];
  int status;
  DBG (31, ">> get power on timer\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0xe3;
  cmd[6] = 1;
  cmd[7] = 0;
  cmd[8] = 0;
  status = sanei_scsi_cmd2 (fd, cmd, sizeof (cmd), NULL, 0, buf, buf_size);

  DBG (31, "<< get power on timer\n");
  return (status);
}
#endif

static SANE_Status
get_film_status (int fd, void *buf, size_t *buf_size)
{
  static u_char cmd[10];
  int status;
  DBG (31, ">> get film status\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0xe1;
  cmd[6] = 0;
  cmd[7] = 0;
  cmd[8] = 4;
  status = sanei_scsi_cmd2 (fd, cmd, sizeof (cmd), NULL, 0, buf, buf_size);

  DBG (31, "<< get film status\n");
  return (status);
}

static SANE_Status
get_data_status (int fd, void *buf, size_t *buf_size)
{
  static u_char cmd[10];
  int status;
  DBG (31, ">> get_data_status\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0x34;
  cmd[8] = 28;
  status = sanei_scsi_cmd2 (fd, cmd, sizeof (cmd), NULL, 0, buf, buf_size);

  DBG (31, "<< get_data_status\n");
  return (status);
}

/*************** modification for FB620S ***************/
static SANE_Status
reset_scanner (int fd)
{
  static u_char cmd[6];
  int status;
  DBG (31, ">> reset_scanner\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0xc1;
  status = sanei_scsi_cmd2 (fd, cmd, sizeof (cmd), NULL, 0, NULL, NULL);

  DBG (31, "<< reset_scanner \n");
  return (status);
}

static SANE_Status
execute_calibration (int fd)
{
  static u_char cmd[6];
  u_char data[2];
  int status;
  DBG (31, ">> execute_calibration\n");

  memset (cmd, 0, sizeof (cmd));
  memset (data, 0, sizeof (data));
  cmd[0] = 0xc2;
  cmd[4] = 2;
  status = sanei_scsi_cmd2 (fd, cmd, sizeof (cmd), data, sizeof (data),
			    NULL, NULL);

  DBG (31, "<< execute_calibration\n");
  return (status);
}

static SANE_Status
get_calibration_status (int fd, void *buf, size_t *buf_size)
{
  static u_char cmd[6];
  int status;
  DBG (31, ">> get_calibration_status\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0xc3;
  cmd[4] = *buf_size;
  status = sanei_scsi_cmd2 (fd, cmd, sizeof (cmd), NULL, 0, buf, buf_size);

  DBG (31, "<< get_calibration_status\n");
  return (status);
}

#if 0
static SANE_Status
get_switch_status (int fd, void *buf, size_t *buf_size)
{
  static u_char cmd[6];
  int status;
  DBG (31, ">> get_switch_status\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0xc4;
  cmd[4] = 2;
  status = sanei_scsi_cmd2 (fd, cmd, sizeof (cmd), NULL, 0, buf, buf_size);

  DBG (31, "<< get_switch_status\n");
  return (status);
}

static SANE_Status
wait_ready(int fd)
{
  SANE_Status status;
  int retry = 0;

  while ((status = test_unit_ready (fd)) != SANE_STATUS_GOOD)
    {
      DBG(5, "wait_ready failed (%d)\n", retry);
      if (retry++ > 15) 
	return SANE_STATUS_IO_ERROR;
      sleep(3);
    }
  return(status);
}
#endif

/*************** modification for FB1200S ***************/
static SANE_Status
cancel (int fd)
{
  static u_char cmd[10];
  int status;
  DBG (31, ">> cancel_FB1200S\n");

  memset (cmd, 0, sizeof (cmd));
  cmd[0] = 0xe4;
  status = sanei_scsi_cmd2 (fd, cmd, sizeof (cmd), NULL, 0, NULL, NULL);

  DBG (31, "<< cancel_FB1200S \n");
  return (status);
}

/**************************************************************************/
/* As long as we do not know how this scanner stores its density curves,
   we do the gamma correction with a 8 <--> 12 bit translation table
   stored in the CANON_Scanner structure. */

static SANE_Status
get_density_curve_fs2710 (SANE_Handle handle, int component, u_char *buf,
			  size_t *buf_size)
{
  CANON_Scanner *s = handle;
  int i;

  for (i = 0; i < 256; i++)
    *buf++ = s->gamma_map[component][i << 4];
  *buf_size = 256;
  return (SANE_STATUS_GOOD);
}

static SANE_Status
set_density_curve_fs2710 (SANE_Handle handle, int component, u_char *buf)
{
  CANON_Scanner *s = handle;
  int i, j, hi, lo;
  u_char *p;

  for (i = 1, hi = *buf++, p = &s->gamma_map[component][0]; i <= 256; i++)
    {
      lo = hi;
      hi = (i < 256) ? *buf++ : 2 * *(buf - 1) - *(buf - 2);
      if (hi > 255)
	hi = 255;
      for (j = 0; j < 16; j++)		/* do a linear interpolation */
	*p++ = (u_char) (lo + ((double) ((hi - lo) * j)) / 16.0 + 0.5);
    }
  return (SANE_STATUS_GOOD);
}

static SANE_Status
set_parameters_fs2710 (SANE_Handle handle)
{
  CANON_Scanner *s = handle;
  int i, j, invert, shadow[4], hilite[4];
  double x, b, c;

  shadow[1] = s->ShadowR << 4;
  shadow[2] = s->ShadowG << 4;
  shadow[3] = s->ShadowB << 4;
  hilite[1] = s->HiliteR << 4;
  hilite[2] = s->HiliteG << 4;
  hilite[3] = s->HiliteB << 4;
  c = ((double) s->contrast) / 128.0;
  b = ((double) (s->brightness - 128)) / 128.0;

  invert = strcmp (filmtype_list[1], s->val[OPT_NEGATIVE].s);

  for (i = 1; i < 4; i++)
    {
      for (j = 0; j < 4096; j++)
	{
	  if (j <= shadow[i])
	    s->gamma_map[i][j] = (u_char) ((s->brightness >= 128) ?
	      2 * s->brightness - 256 : 0);
	  else if (j < hilite[i])
	    {
	      x = ((double) (j - shadow[i]))
		/ ((double) (hilite[i] - shadow[i]));
	      /* first do the contrast correction */
	      x = (x <= 0.5) ? 0.5 * pow (2 * x, c)
		: 1.0 - 0.5 * pow (2 * (1.0 - x), c);
	      x = pow (x, 0.5);		/* default gamma correction */
	      x += b;			/* brightness correction */
	      s->gamma_map[i][j] = (u_char) MAX (0, MIN (255,
		(int) (255.0 * x)));
	    }
	  else
	    s->gamma_map[i][j] = (u_char) ((s->brightness >= 128) ?
	      255 : 2 * s->brightness);
	}
    }

  return (SANE_STATUS_GOOD);
}

/**************************************************************************/
