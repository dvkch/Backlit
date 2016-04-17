/* HP Scanjet 3900 series - USB layer

   Copyright (C) 2005-2008 Jonathan Bravo Lopez <jkdsoft@gmail.com>

   This file is part of the SANE package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

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

#ifndef USBLAYER

#define USBLAYER

#define TIMEOUT      1000
#define BLK_READ_EP  0x81
#define BLK_WRITE_EP 0x02

SANE_Int dataline_count = 0;

/* USB layer commands */
static SANE_Int usb_ctl_write (USB_Handle usb_handle, SANE_Int address,
			       SANE_Byte * buffer, SANE_Int size,
			       SANE_Int index);
static SANE_Int usb_ctl_read (USB_Handle usb_handle, SANE_Int address,
			      SANE_Byte * buffer, SANE_Int size,
			      SANE_Int index);

/* Higher level commands*/

static SANE_Int IRead_Byte (USB_Handle usb_handle, SANE_Int address,
			    SANE_Byte * data, SANE_Int index);
static SANE_Int IRead_Word (USB_Handle usb_handle, SANE_Int address,
			    SANE_Int * data, SANE_Int index);
static SANE_Int IRead_Integer (USB_Handle usb_handle, SANE_Int address,
			       SANE_Int * data, SANE_Int index);
static SANE_Int IRead_Buffer (USB_Handle usb_handle, SANE_Int address,
			      SANE_Byte * buffer, SANE_Int size,
			      SANE_Int index);
static SANE_Int IWrite_Byte (USB_Handle usb_handle, SANE_Int address,
			     SANE_Byte data, SANE_Int index1,
			     SANE_Int index2);
static SANE_Int IWrite_Word (USB_Handle usb_handle, SANE_Int address,
			     SANE_Int data, SANE_Int index);
static SANE_Int IWrite_Integer (USB_Handle usb_handle, SANE_Int address,
				SANE_Int data, SANE_Int index);
static SANE_Int IWrite_Buffer (USB_Handle usb_handle, SANE_Int address,
			       SANE_Byte * buffer, SANE_Int size,
			       SANE_Int index);

static SANE_Int Read_Byte (USB_Handle usb_handle, SANE_Int address,
			   SANE_Byte * data);
static SANE_Int Read_Word (USB_Handle usb_handle, SANE_Int address,
			   SANE_Int * data);
static SANE_Int Read_Integer (USB_Handle usb_handle, SANE_Int address,
			      SANE_Int * data);
static SANE_Int Read_Buffer (USB_Handle usb_handle, SANE_Int address,
			     SANE_Byte * buffer, SANE_Int size);
static SANE_Int Read_Bulk (USB_Handle usb_handle, SANE_Byte * buffer,
			   size_t size);
static SANE_Int Write_Byte (USB_Handle usb_handle, SANE_Int address,
			    SANE_Byte data);
static SANE_Int Write_Word (USB_Handle usb_handle, SANE_Int address,
			    SANE_Int data);
/*static SANE_Int  Write_Integer  (USB_Handle usb_handle, SANE_Int address, SANE_Int data);*/
static SANE_Int Write_Buffer (USB_Handle usb_handle, SANE_Int address,
			      SANE_Byte * buffer, SANE_Int size);
static SANE_Int Write_Bulk (USB_Handle usb_handle, SANE_Byte * buffer,
			    SANE_Int size);

static SANE_Int show_buffer (SANE_Int level, SANE_Byte * buffer,
			     SANE_Int size);

/* Implementation */

static SANE_Int
IWrite_Byte (USB_Handle usb_handle, SANE_Int address, SANE_Byte data,
	     SANE_Int index1, SANE_Int index2)
{
  SANE_Int rst = ERROR;
  SANE_Byte buffer[2] = { 0x00, 0x00 };

  if (usb_ctl_read (usb_handle, address + 1, buffer, 0x02, index1) == 2)
    {
      buffer[1] = (buffer[0] & 0xff);
      buffer[0] = (data & 0xff);

      if (usb_ctl_write (usb_handle, address, buffer, 0x02, index2) == 2)
	rst = OK;
    }

  return rst;
}

static SANE_Int
IWrite_Word (USB_Handle usb_handle, SANE_Int address, SANE_Int data,
	     SANE_Int index)
{
  SANE_Int rst = ERROR;
  SANE_Byte buffer[2];

  buffer[0] = (data & 0xff);
  buffer[1] = ((data >> 8) & 0xff);

  if (usb_ctl_write (usb_handle, address, buffer, 0x02, index) == 2)
    rst = OK;

  return rst;
}

static SANE_Int
IWrite_Integer (USB_Handle usb_handle, SANE_Int address, SANE_Int data,
		SANE_Int index)
{
  SANE_Int rst = ERROR;
  SANE_Byte buffer[4];

  buffer[0] = (data & 0xff);
  buffer[1] = ((data >> 8) & 0xff);
  buffer[2] = ((data >> 16) & 0xff);
  buffer[3] = ((data >> 24) & 0xff);

  if (usb_ctl_write (usb_handle, address, buffer, 0x04, index) == 4)
    rst = OK;

  return rst;
}

static SANE_Int
IWrite_Buffer (USB_Handle usb_handle, SANE_Int address, SANE_Byte * buffer,
	       SANE_Int size, SANE_Int index)
{
  SANE_Int ret = ERROR;

  if (!((buffer == NULL) && (size > 0)))
    if (usb_ctl_write (usb_handle, address, buffer, size, index) == size)
      ret = OK;

  return ret;
}

static SANE_Int
IRead_Byte (USB_Handle usb_handle, SANE_Int address, SANE_Byte * data,
	    SANE_Int index)
{
  SANE_Byte buffer[2] = { 0x00, 0x00 };
  SANE_Int ret = ERROR;

  if (data != NULL)
    if (usb_ctl_read (usb_handle, address, buffer, 0x02, index) == 2)
      {
	*data = (SANE_Byte) (buffer[0] & 0xff);
	ret = OK;
      }

  return ret;
}

static SANE_Int
IRead_Word (USB_Handle usb_handle, SANE_Int address, SANE_Int * data,
	    SANE_Int index)
{
  SANE_Byte buffer[2] = { 0x00, 0x00 };
  SANE_Int ret = ERROR;

  if (data != NULL)
    if (usb_ctl_read (usb_handle, address, buffer, 0x02, index) == 2)
      {
	*data = ((buffer[1] << 8) & 0xffff) + (buffer[0] & 0xff);
	ret = OK;
      }

  return ret;
}

static SANE_Int
IRead_Integer (USB_Handle usb_handle, SANE_Int address, SANE_Int * data,
	       SANE_Int index)
{
  SANE_Byte buffer[4] = { 0x00, 0x00, 0x00, 0x00 };
  SANE_Int ret = ERROR;

  if (data != NULL)
    {
      *data = 0;
      if (usb_ctl_read (usb_handle, address, buffer, 0x04, index) == 4)
	{
	  SANE_Int C;
	  for (C = 3; C >= 0; C--)
	    *data = ((*data << 8) + (buffer[C] & 0xff)) & 0xffffffff;
	  ret = OK;
	}
    }

  return ret;
}

static SANE_Int
IRead_Buffer (USB_Handle usb_handle, SANE_Int address, SANE_Byte * buffer,
	      SANE_Int size, SANE_Int index)
{
  SANE_Int ret = ERROR;

  if (buffer != NULL)
    if (usb_ctl_read (usb_handle, address, buffer, size, index) == size)
      ret = OK;

  return ret;
}

static SANE_Int
Write_Byte (USB_Handle usb_handle, SANE_Int address, SANE_Byte data)
{
  return IWrite_Byte (usb_handle, address, data, 0x100, 0);
}

static SANE_Int
Write_Word (USB_Handle usb_handle, SANE_Int address, SANE_Int data)
{
  return IWrite_Word (usb_handle, address, data, 0);
}

/*static SANE_Int Write_Integer(USB_Handle usb_handle, SANE_Int address, SANE_Int data)
{
	return IWrite_Integer(usb_handle, address, data, 0);
}*/

static SANE_Int
Write_Buffer (USB_Handle usb_handle, SANE_Int address, SANE_Byte * buffer,
	      SANE_Int size)
{
  return IWrite_Buffer (usb_handle, address, buffer, size, 0);
}

static SANE_Int
Read_Byte (USB_Handle usb_handle, SANE_Int address, SANE_Byte * data)
{
  return IRead_Byte (usb_handle, address, data, 0x100);
}

static SANE_Int
Read_Word (USB_Handle usb_handle, SANE_Int address, SANE_Int * data)
{
  return IRead_Word (usb_handle, address, data, 0x100);
}

static SANE_Int
Read_Integer (USB_Handle usb_handle, SANE_Int address, SANE_Int * data)
{
  return IRead_Integer (usb_handle, address, data, 0x100);
}

static SANE_Int
Read_Buffer (USB_Handle usb_handle, SANE_Int address, SANE_Byte * buffer,
	     SANE_Int size)
{
  return IRead_Buffer (usb_handle, address, buffer, size, 0x100);
}

static SANE_Int
Write_Bulk (USB_Handle usb_handle, SANE_Byte * buffer, SANE_Int size)
{
  SANE_Int rst = ERROR;

  if (buffer != NULL)
    {
      dataline_count++;
      DBG (DBG_CTL, "%06i BLK DO: %i. bytes\n", dataline_count, size);
      show_buffer (4, buffer, size);

#ifdef STANDALONE
      if (usb_handle != NULL)
	if (usb_bulk_write
	    (usb_handle, BLK_WRITE_EP, (char *) buffer, size,
	     TIMEOUT) == size)
	  rst = OK;
#else
      if (usb_handle != -1)
	{
	  size_t mysize = size;
	  if (sanei_usb_write_bulk (usb_handle, buffer, &mysize) ==
	      SANE_STATUS_GOOD)
	    rst = OK;
	}
#endif
    }

  if (rst != OK)
    DBG (DBG_CTL, "             : Write_Bulk error\n");

  return rst;
}

static SANE_Int
Read_Bulk (USB_Handle usb_handle, SANE_Byte * buffer, size_t size)
{
  SANE_Int rst = ERROR;

  if (buffer != NULL)
    {
      dataline_count++;
      DBG (DBG_CTL, "%06i BLK DI: Buffer length = %lu. bytes\n",
	   dataline_count, (u_long) size);

#ifdef STANDALONE
      if (usb_handle != NULL)
	rst =
	  usb_bulk_read (usb_handle, BLK_READ_EP, (char *) buffer, size,
			 TIMEOUT);
#else
      if (usb_handle != -1)
	if (sanei_usb_read_bulk (usb_handle, buffer, &size) ==
	    SANE_STATUS_GOOD)
	  rst = size;
#endif
    }

  if (rst < 0)
    DBG (DBG_CTL, "             : Read_Bulk error\n");
  else
    show_buffer (4, buffer, rst);

  return rst;
}

static SANE_Int
usb_ctl_write (USB_Handle usb_handle, SANE_Int address, SANE_Byte * buffer,
	       SANE_Int size, SANE_Int index)
{
  SANE_Int rst = ERROR;

  dataline_count++;
  DBG (DBG_CTL, "%06i CTL DO: 40 04 %04x %04x %04x\n",
       dataline_count, address & 0xffff, index, size);
  show_buffer (DBG_CTL, buffer, size);

#ifdef STANDALONE
  if (usb_handle != NULL)
    rst = usb_control_msg (usb_handle, 0x40,	/* Request type */
			   0x04,	/* Request      */
			   address,	/* Value        */
			   index,	/* Index        */
			   (char *) buffer,	/* Buffer       */
			   size,	/* Size         */
			   TIMEOUT);
#else
  if (usb_handle != -1)
    {
      if (sanei_usb_control_msg (usb_handle, 0x40,	/* Request type */
				 0x04,	/* Request      */
				 address,	/* Value        */
				 index,	/* Index        */
				 size,	/* Size         */
				 buffer)	/* Buffer       */
	  == SANE_STATUS_GOOD)
	rst = size;
      else
	rst = -1;
    }
#endif

  if (rst < 0)
    DBG (DBG_CTL, "             : Error, returned %i\n", rst);

  return rst;
}

static SANE_Int
usb_ctl_read (USB_Handle usb_handle, SANE_Int address, SANE_Byte * buffer,
	      SANE_Int size, SANE_Int index)
{
  SANE_Int rst;

  rst = ERROR;

  dataline_count++;
  DBG (DBG_CTL, "%06i CTL DI: c0 04 %04x %04x %04x\n",
       dataline_count, address & 0xffff, index, size);

#ifdef STANDALONE
  if (usb_handle != NULL)
    rst = usb_control_msg (usb_handle, 0xc0,	/* Request type */
			   0x04,	/* Request      */
			   address,	/* Value        */
			   index,	/* Index        */
			   (char *) buffer,	/* Buffer       */
			   size,	/* Size         */
			   TIMEOUT);
#else
  if (usb_handle != -1)
    {
      if (sanei_usb_control_msg (usb_handle, 0xc0,	/* Request type */
				 0x04,	/* Request      */
				 address,	/* Value        */
				 index,	/* Index        */
				 size,	/* Size         */
				 buffer)	/* Buffer       */
	  == SANE_STATUS_GOOD)
	rst = size;
      else
	rst = -1;
    }
#endif

  if (rst < 0)
    DBG (DBG_CTL, "             : Error, returned %i\n", rst);
  else
    show_buffer (DBG_CTL, buffer, rst);

  return rst;
}

static SANE_Int
show_buffer (SANE_Int level, SANE_Byte * buffer, SANE_Int size)
{
  if (DBG_LEVEL >= level)
    {
      char *sline = NULL;
      char *sdata = NULL;
      SANE_Int cont, data, offset = 0, col = 0;

      if ((size > 0) && (buffer != NULL))
	{
	  sline = (char *) malloc (256);
	  if (sline != NULL)
	    {
	      sdata = (char *) malloc (256);
	      if (sdata != NULL)
		{
		  bzero (sline, 256);
		  for (cont = 0; cont < size; cont++)
		    {
		      if (col == 0)
			{
			  if (cont == 0)
			    snprintf (sline, 255, "           BF: ");
			  else
			    snprintf (sline, 255, "               ");
			}
		      data = (buffer[cont] & 0xff);
		      snprintf (sdata, 255, "%02x ", data);
		      sline = strcat (sline, sdata);
		      col++;
		      offset++;
		      if (col == 8)
			{
			  col = 0;
			  snprintf (sdata, 255, " : %i\n", offset - 8);
			  sline = strcat (sline, sdata);
			  DBG (level, "%s", sline);
			  bzero (sline, 256);
			}
		    }
		  if (col > 0)
		    {
		      for (cont = col; cont < 8; cont++)
			{
			  snprintf (sdata, 255, "-- ");
			  sline = strcat (sline, sdata);
			  offset++;
			}
		      snprintf (sdata, 255, " : %i\n", offset - 8);
		      sline = strcat (sline, sdata);
		      DBG (level, "%s", sline);
		      bzero (sline, 256);
		    }
		  free (sdata);
		}
	      free (sline);
	    }
	}
      else
	DBG (level, "           BF: Empty buffer\n");
    }
  return OK;
}
#endif /*USBLAYER*/
