/** 
 * Description of the Primax PagePartner model
 */
static P5_Model pagepartner_model = {
  "Primax PagePartner",
  "Primax",
  "PagePartner",
  SANE_I18N ("sheetfed scanner"),

  {300, 200, 150, 100, 0},
  /* 500 seems also possible */
  {600, 400, 300, 200, 150, 100, 0},

  300,
  600,
  100,
  100,
  16,

  SANE_FIX (0.0),
  SANE_FIX (0.0),
  SANE_FIX (215.9),
  SANE_FIX (300.0),
};

#ifdef HAVE_LINUX_PPDEV_H
static char *
addr_name (uint16_t addr)
{
  switch (addr)
    {
    case DATA:
      return "DATA";
      break;
    case STATUS:
      return "STATUS";
      break;
    case CONTROL:
      return "CONTROL";
      break;
    case EPPADR:
      return "EPPADR";
      break;
    case EPPDATA:
      return "EPPDATA";
      break;
    default:
      return "*ERROR*";
    }
}
#endif

/** @brief low level hardware access functions
 * @{
 */

static uint8_t
inb (int fd, uint16_t addr)
{
#ifdef HAVE_LINUX_PPDEV_H
  uint8_t val = 0xff;
  int rc, mode = 0xff;

  switch (addr)
    {
    case DATA:
      rc = ioctl (fd, PPRDATA, &val);
      break;
    case STATUS:
      rc = ioctl (fd, PPRSTATUS, &val);
      break;
    case CONTROL:
      rc = ioctl (fd, PPRCONTROL, &val);
      break;
    case EPPDATA:
      mode = 1;			/* data_reverse */
      rc = ioctl (fd, PPDATADIR, &mode);
      mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
      rc = ioctl (fd, PPSETMODE, &mode);
#ifdef PPSETFLAGS
      mode = PP_FASTREAD;
      rc = ioctl (fd, PPSETFLAGS, &mode);
#endif
      rc = read (fd, &val, 1);
      break;
    default:
      DBG (DBG_error, "inb(%s) escaped ppdev\n", addr_name (addr));
      return 0xFF;
    }
  if (rc < 0)
    {
      DBG (DBG_error, "ppdev ioctl returned <%s>\n", strerror (errno));
    }
  return val;
#else
  if(fd && addr)
    return 0;
  return 0;
#endif
}

static void
outb (int fd, uint16_t addr, uint8_t value)
{
#ifdef HAVE_LINUX_PPDEV_H
  int rc = 0, mode = 0xff;

  switch (addr)
    {
    case DATA:
      rc = ioctl (fd, PPWDATA, &value);
      break;
    case CONTROL:
      mode = value & 0x20;
      rc = ioctl (fd, PPDATADIR, &mode);
      if (!rc)
	{
	  value = value & 0xDF;
	  rc = ioctl (fd, PPWCONTROL, &value);
	}
      break;
    case EPPDATA:
      mode = 0;			/* data forward */
      rc = ioctl (fd, PPDATADIR, &mode);
      mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
      rc = ioctl (fd, PPSETMODE, &mode);
      rc = write (fd, &value, 1);
      break;
    case EPPADR:
      mode = 0;			/* data forward */
      rc = ioctl (fd, PPDATADIR, &mode);
      mode = IEEE1284_MODE_EPP | IEEE1284_ADDR;
      rc = ioctl (fd, PPSETMODE, &mode);
      rc = write (fd, &value, 1);
      break;
    default:
      DBG (DBG_error, "outb(%s,0x%02x) escaped ppdev\n", addr_name (addr),
	   value);
      break;
    }
  if (rc < 0)
    {
      DBG (DBG_error, "ppdev ioctl returned <%s>\n", strerror (errno));
    }
#else
  if(fd && addr && value)
    return;
#endif /* HAVE_LINUX_PPDEV_H */
}

static void
write_reg (int fd, uint8_t index, uint8_t value)
{
  uint8_t idx;

  /* both nibbles hold the same value */
  idx = index & 0x0F;
  DBG (DBG_io2, "write_reg(REG%X,0x%x)\n", idx, value);
  idx = idx << 4 | idx;
  outb (fd, EPPADR, idx);
  outb (fd, EPPDATA, value);
}

static uint8_t
read_reg (int fd, uint8_t index)
{
  uint8_t idx;

  /* both nibbles hold the same value */
  idx = index & 0x0F;
  idx = idx << 4 | idx;
  outb (fd, EPPADR, idx);
  return inb (fd, EPPDATA);
}

#ifdef HAVE_LINUX_PPDEV_H
static int
read_data (int fd, uint8_t * data, int length)
{
  int mode, rc, nb;
  unsigned char bval;

  bval = REG8;
  mode = IEEE1284_MODE_EPP | IEEE1284_ADDR;
  rc = ioctl (fd, PPSETMODE, &mode);
  rc = write (fd, &bval, 1);

  mode = 1;			/* data_reverse */
  rc = ioctl (fd, PPDATADIR, &mode);
#ifdef PPSETFLAGS
  mode = PP_FASTREAD;
  rc = ioctl (fd, PPSETFLAGS, &mode);
#endif
  mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
  rc = ioctl (fd, PPSETMODE, &mode);
  nb = 0;
  while (nb < length)
    {
      rc = read (fd, data + nb, length - nb);
      if (rc < 0)
	{
	  DBG (DBG_error, "memtest: error reading data back!\n");
	  return 0;
	}
      else
	{
	  nb += rc;
	}
    }

  return 1;
}

static void
index_write_data (int fd, uint8_t index, uint8_t * data, int length)
{
  int mode, rc;
  unsigned char bval;

  bval = index;
  mode = IEEE1284_MODE_EPP | IEEE1284_ADDR;
  rc = ioctl (fd, PPSETMODE, &mode);
  rc = write (fd, &bval, 1);

  mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
  rc = ioctl (fd, PPSETMODE, &mode);
  mode = 0;			/* data forward */
  rc = ioctl (fd, PPDATADIR, &mode);
  rc = write (fd, data, length);
  return;
}

static void
write_data (int fd, uint8_t * data, int length)
{
  index_write_data (fd, REG8, data, length);
}

static void
write_reg2 (int fd, uint8_t index, uint16_t value)
{
  uint8_t data2[2];

  data2[0] = value & 0xff;
  data2[1] = value >> 8;
  index_write_data (fd, index, data2, 2);
}
#else

static int
read_data (int fd, uint8_t * data, int length)
{ 
  if(fd && data && length)
    return -1;
  return -1;
}

static void
write_data (int fd, uint8_t * data, int length)
{
  if(fd && data && length)
    return;
}

static void
write_reg2 (int fd, uint8_t index, uint16_t value)
{
  if(fd && index && value)
    return;
}
#endif

/**
 * @}
 */


/** @brief This function checks a memory buffer.
 * This function writes at the given memory address then read it back
 * to check the scanner is correctly working.
 * @param fd file descriptor used to access hardware
 * @param addr address where to write and read
 * @return SANE_TRUE on succes, SANE_FALSE otherwise
 */
static int
memtest (int fd, uint16_t addr)
{
  uint8_t sent[256];
  uint8_t back[256];
  int i;

  write_reg2 (fd, REG1, addr);
  for (i = 0; i < 256; i++)
    {
      sent[i] = (uint8_t) i;
      back[i] = 0;
    }
  write_data (fd, sent, 256);
  read_data (fd, back, 256);

  /* check if data read back is the same that the one sent */
  for (i = 0; i < 256; i++)
    {
      if (back[i] != sent[i])
	{
	  return SANE_FALSE;
	}
    }

  return SANE_TRUE;
}


#define INB(k,y,z) val=inb(k,y); if(val!=z) { DBG(DBG_error,"expected 0x%02x, got 0x%02x\n",z, val); return SANE_FALSE; }

/** @brief connect to scanner
 * This function sends the connect sequence for the scanner.
 * @param fd filedescriptor of the parallel port communication channel
 * @return SANE_TRUE in case of success, SANE_FALSE otherwise
 */
static int
connect (int fd)
{
  uint8_t val;

  inb (fd, CONTROL);
  outb (fd, CONTROL, 0x04);
  outb (fd, DATA, 0x02);
  INB (fd, DATA, 0x02);
  outb (fd, DATA, 0x03);
  INB (fd, DATA, 0x03);
  outb (fd, DATA, 0x03);
  outb (fd, DATA, 0x83);
  outb (fd, DATA, 0x03);
  outb (fd, DATA, 0x83);
  INB (fd, DATA, 0x83);
  outb (fd, DATA, 0x82);
  INB (fd, DATA, 0x82);
  outb (fd, DATA, 0x02);
  outb (fd, DATA, 0x82);
  outb (fd, DATA, 0x02);
  outb (fd, DATA, 0x82);
  INB (fd, DATA, 0x82);
  outb (fd, DATA, 0x82);
  INB (fd, DATA, 0x82);
  outb (fd, DATA, 0x02);
  outb (fd, DATA, 0x82);
  outb (fd, DATA, 0x02);
  outb (fd, DATA, 0x82);
  INB (fd, DATA, 0x82);
  outb (fd, DATA, 0x83);
  INB (fd, DATA, 0x83);
  outb (fd, DATA, 0x03);
  outb (fd, DATA, 0x83);
  outb (fd, DATA, 0x03);
  outb (fd, DATA, 0x83);
  INB (fd, DATA, 0x83);
  outb (fd, DATA, 0x82);
  INB (fd, DATA, 0x82);
  outb (fd, DATA, 0x02);
  outb (fd, DATA, 0x82);
  outb (fd, DATA, 0x02);
  outb (fd, DATA, 0x82);
  INB (fd, DATA, 0x82);
  outb (fd, DATA, 0x83);
  INB (fd, DATA, 0x83);
  outb (fd, DATA, 0x03);
  outb (fd, DATA, 0x83);
  outb (fd, DATA, 0x03);
  outb (fd, DATA, 0x83);
  INB (fd, DATA, 0x83);
  outb (fd, DATA, 0x83);
  INB (fd, DATA, 0x83);
  outb (fd, DATA, 0x03);
  outb (fd, DATA, 0x83);
  outb (fd, DATA, 0x03);
  outb (fd, DATA, 0x83);
  INB (fd, DATA, 0x83);
  outb (fd, DATA, 0x82);
  INB (fd, DATA, 0x82);
  outb (fd, DATA, 0x02);
  outb (fd, DATA, 0x82);
  outb (fd, DATA, 0x02);
  outb (fd, DATA, 0x82);
  outb (fd, DATA, 0xFF);
  DBG (DBG_info, "connect() OK...\n");
  return SANE_TRUE;
}

static int
disconnect (int fd)
{
  uint8_t val;

  outb (fd, CONTROL, 0x04);
  outb (fd, DATA, 0x00);
  INB (fd, DATA, 0x00);
  outb (fd, DATA, 0x01);
  INB (fd, DATA, 0x01);
  outb (fd, DATA, 0x01);
  outb (fd, DATA, 0x81);
  outb (fd, DATA, 0x01);
  outb (fd, DATA, 0x81);
  INB (fd, DATA, 0x81);
  outb (fd, DATA, 0x80);
  INB (fd, DATA, 0x80);
  outb (fd, DATA, 0x00);
  outb (fd, DATA, 0x80);
  outb (fd, DATA, 0x00);
  outb (fd, DATA, 0x80);
  INB (fd, DATA, 0x80);
  outb (fd, DATA, 0x80);
  INB (fd, DATA, 0x80);
  outb (fd, DATA, 0x00);
  outb (fd, DATA, 0x80);
  outb (fd, DATA, 0x00);
  outb (fd, DATA, 0x80);
  INB (fd, DATA, 0x80);
  outb (fd, DATA, 0x81);
  INB (fd, DATA, 0x81);
  outb (fd, DATA, 0x01);
  outb (fd, DATA, 0x81);
  outb (fd, DATA, 0x01);
  outb (fd, DATA, 0x81);
  INB (fd, DATA, 0x81);
  outb (fd, DATA, 0x80);
  INB (fd, DATA, 0x80);
  outb (fd, DATA, 0x00);
  outb (fd, DATA, 0x80);
  outb (fd, DATA, 0x00);
  outb (fd, DATA, 0x80);
  INB (fd, DATA, 0x80);
  outb (fd, DATA, 0x00);
  outb (fd, DATA, 0x80);
  outb (fd, DATA, 0x00);
  outb (fd, DATA, 0x80);
  INB (fd, DATA, 0x80);
  outb (fd, DATA, 0x00);
  outb (fd, DATA, 0x80);
  outb (fd, DATA, 0x00);
  outb (fd, DATA, 0x80);
  INB (fd, DATA, 0x80);
  outb (fd, DATA, 0x00);
  outb (fd, DATA, 0x80);
  outb (fd, DATA, 0x00);
  outb (fd, DATA, 0x80);
  inb (fd, CONTROL);
  outb (fd, CONTROL, 0x0C);

  return SANE_STATUS_GOOD;
}

static void
setadresses (int fd, uint16_t start, uint16_t end)
{
  write_reg (fd, REG3, start & 0xff);
  write_reg (fd, REG4, start >> 8);
  write_reg (fd, REG5, end & 0xff);
  write_reg (fd, REG6, end >> 8);
  DBG (DBG_io, "setadresses(0x%x,0x%x); OK...\n", start, end);
}

#ifdef HAVE_LINUX_PPDEV_H
/** @brief open parallel port device
 * opens parallel port's low level device in EPP mode
 * @param devicename nam of the real device or the special value 'auto' 
 * @return file descriptor in cas of successn -1 otherwise
 */
static int
open_pp (const char *devicename)
{
  int fd, rc, mode = 0;
  char *name;

  DBG (DBG_proc, "open_pp: start, devicename=%s\n", devicename);
  /* TODO improve auto device finding */
  if (strncmp (devicename, "auto", 4) == 0)
    {
      name = strdup("/dev/parport0");
    }
  else
    {
      name = strdup(devicename);
    }

  /* open device */
  fd = open (name, O_RDWR);
  if (fd < 0)
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
	  DBG (DBG_error, "open_pp: no %s device ...\n", name);
	  break;
	case EACCES:
	  DBG (DBG_error,
	       "open_pp: current user cannot use existing %s device ...\n",
	       name);
	  break;
	default:
	  DBG (DBG_error, "open_pp: %s while opening %s\n", strerror (errno),
	       name);
	}
      return -1;
    }
  free(name);

  /* claim device and set it to EPP */
  rc = ioctl (fd, PPCLAIM);
  rc = ioctl (fd, PPGETMODES, &mode);
  if (mode & PARPORT_MODE_PCSPP)
    DBG (DBG_io, "PARPORT_MODE_PCSPP\n");
  if (mode & PARPORT_MODE_TRISTATE)
    DBG (DBG_io, "PARPORT_MODE_TRISTATE\n");
  if (mode & PARPORT_MODE_EPP)
    DBG (DBG_io, "PARPORT_MODE_EPP\n");
  if (mode & PARPORT_MODE_ECP)
    DBG (DBG_io, "PARPORT_MODE_ECP\n");
  if (mode & PARPORT_MODE_COMPAT)
    DBG (DBG_io, "PARPORT_MODE_COMPAT\n");
  if (mode & PARPORT_MODE_DMA)
    DBG (DBG_io, "PARPORT_MODE_DMA\n");
  if (mode & PARPORT_MODE_EPP)
    {
      mode = IEEE1284_MODE_EPP;
    }
  else
    {
      /* 
         if (mode & PARPORT_MODE_ECP)
         {
         mode = IEEE1284_MODE_ECP;
         }
         else
       */
      {
	mode = -1;
      }
    }
  if (mode == -1)
    {
      DBG (DBG_error, "open_pp: no EPP mode, giving up ...\n");
      rc = ioctl (fd, PPRELEASE);
      close (fd);
      return -1;
    }
  rc = ioctl (fd, PPNEGOT, &mode);
  rc = ioctl (fd, PPSETMODE, &mode);
  DBG (DBG_proc, "open_pp: exit\n");
  return fd;
}

/** close low level device
 * release and close low level hardware device 
 */
static void
close_pp (int fd)
{
  int mode = IEEE1284_MODE_COMPAT;

  if (fd > 2)
    {
      ioctl (fd, PPNEGOT, &mode);
      ioctl (fd, PPRELEASE);
      close (fd);
    }
}

#else /*  HAVE_LINUX_PPDEV_H */

static int
open_pp (const char *devicename)
{
  if(devicename)
    return -1;
  return -1;
}

static void
close_pp (int fd)
{
  if(fd)
    return;
}
#endif /*  HAVE_LINUX_PPDEV_H */

/** @brief test if a document is inserted
 * Test if a document is inserted by reading register E
 * @param fd file descriptor to access scanner
 * @return SANE_STATUS_NO_DOCS if no document or SANE_STATUS_GOOD
 * if something is present.
 */
static SANE_Status
test_document (int fd)
{
  int detector;

  /* check for document presence 0xC6: present, 0xC3 no document */
  detector = read_reg (fd, REGE);
  DBG (DBG_io, "test_document: detector=0x%02X\n", detector);

  /* document inserted */
  if (detector & 0x04)
    return SANE_STATUS_GOOD;

  return SANE_STATUS_NO_DOCS;
}

/**
 * return the amount of scanned data available
 * @param fd file descriptor to access scanner
 * @return avaible byte number 
 */
static int
available_bytes (int fd)
{
  int counter;

  /* read the number of 256 bytes block of scanned data */
  counter = read_reg (fd, REG9);
  DBG (DBG_io, "available_bytes: available_bytes=0x%02X\n", counter);
  return 256 * counter;
}

static SANE_Status
build_correction (P5_Device * dev, unsigned int dpi, unsigned int mode,
		  unsigned int start, unsigned int width)
{
  unsigned int i, j, shift, step;

  DBG (DBG_proc, "build_correction: start=%d, width=%d\n", start, width);
  DBG (DBG_trace, "build_correction: dpi=%d, mode=%d\n", dpi, mode);

  /* loop on calibration data to find the matching one */
  j = 0;
  while (dev->calibration_data[j]->dpi != dpi)
    {
      j++;
      if (j > MAX_RESOLUTIONS)
	{
	  DBG (DBG_error, "build_correction: couldn't find calibration!\n");
	  return SANE_STATUS_INVAL;
	}
    }

  if (dev->gain != NULL)
    {
      free (dev->gain);
      dev->gain = NULL;
    }
  if (dev->offset != NULL)
    {
      free (dev->offset);
      dev->offset = NULL;
    }
  dev->gain = (float *) malloc (width * sizeof (float));
  if (dev->gain == NULL)
    {
      DBG (DBG_error,
	   "build_correction: failed to allocate memory for gain!\n");
      return SANE_STATUS_NO_MEM;
    }
  dev->offset = (uint8_t *) malloc (width);
  if (dev->offset == NULL)
    {
      DBG (DBG_error,
	   "build_correction: failed to allocate memory for offset!\n");
      return SANE_STATUS_NO_MEM;
    }

  /* compute starting point of calibration data to use */
  shift = start;
  step = 1;
  if (mode == MODE_GRAY)
    {
      /* we use green data */
      shift += 1;
      step = 3;
    }
  for (i = 0; i < width; i += step)
    {
      if (dev->calibration_data[j]->white_data[shift + i] -
	  dev->calibration_data[0]->black_data[shift + i] > BLACK_LEVEL)
	{
	  dev->gain[i] =
	    WHITE_TARGET /
	    ((float)
	     (dev->calibration_data[j]->white_data[shift + i] -
	      dev->calibration_data[j]->black_data[shift + i]));
	  dev->offset[i] = dev->calibration_data[j]->black_data[shift + i];
	}
      else
	{
	  dev->gain[i] = 1.0;
	  dev->offset[i] = 0;
	}
    }
  return SANE_STATUS_GOOD;
  DBG (DBG_proc, "build_correction: end\n");
}

/** @brief start up a real scan
 * This function starts the scan with the given parameters.
 * @param dev device describing hardware
 * @param mode color, gray level or lineart.
 * @param dpi desired scan resolution.
 * @param startx coordinate of the first pixel to scan in 
 * scan's resolution coordinate
 * @param width width of the scanned area
 * scanner's physical scan aread.
 * @return SANE_STATUS_GOOD if scan is successfully started
 */
static SANE_Status
start_scan (P5_Device * dev, int mode, unsigned int dpi, unsigned int startx,
	    unsigned int width)
{
  uint8_t reg0=0;
  uint8_t reg2=0;
  uint8_t regF=0;
  uint16_t addr=0;
  uint16_t start, end;
  unsigned int xdpi;

  DBG (DBG_proc, "start_scan: start \n");
  DBG (DBG_io, "start_scan: startx=%d, width=%d, dpi=%d\n", startx, width,
       dpi);

	/** @brief register values
	 * - reg2 : reg2 seems related to x dpi and provides only 100,
	 *   150, 200 and 300 resolutions.
	 * - regF : lower nibble gives y dpi resolution ranging from 150
	 *   to 1200 dpi.
	 */
  xdpi = dpi;
  switch (dpi)
    {
    case 100:
      reg2 = 0x90;
      regF = 0xA2;
      break;
    case 150:
      reg2 = 0x10;
      regF = 0xA4;
      break;
    case 200:
      reg2 = 0x80;
      regF = 0xA6;
      break;
    case 300:
      reg2 = 0x00;
      regF = 0xA8;
      break;
    case 400:
      reg2 = 0x80;		/* xdpi=200 */
      regF = 0xAA;
      xdpi = 200;
      break;
    case 500:
      reg2 = 0x00;
      regF = 0xAC;
      xdpi = 300;
      break;
    case 600:
      reg2 = 0x00;
      regF = 0xAE;
      xdpi = 300;
      break;
    }

  switch (mode)
    {
    case MODE_COLOR:
      reg0 = 0x00;
      addr = 0x0100;
      break;
    case MODE_GRAY:
      /* green channel only */
      reg0 = 0x20;
      addr = 0x0100;
      break;
    case MODE_LINEART:
      reg0 = 0x40;
      addr = 0x0908;
      break;
    }

  write_reg (dev->fd, REG1, 0x01);
  write_reg (dev->fd, REG7, 0x00);
  write_reg (dev->fd, REG0, reg0);
  write_reg (dev->fd, REG1, 0x00);
  write_reg (dev->fd, REGF, regF);
  /* the memory addr used to test need not to be related
   * to resolution, 0x0100 could be always used */
  /* TODO get rid of it */
  memtest (dev->fd, addr);

  /* handle case where dpi>xdpi */
  start = startx;
  if (dpi > xdpi)
    {
      width = (width * xdpi) / dpi;
      start = (startx * xdpi) / dpi;
    }

  /* compute and set start addr */
  if (mode == MODE_COLOR)
    {
      start = start * 3;
      width = width * 3;
    }
  end = start + width + 1;

  /* build calibration data for the scan */
  if (dev->calibrated)
    {
      build_correction (dev, xdpi, mode, start, width);
    }

  setadresses (dev->fd, start, end);

  write_reg (dev->fd, REG1, addr >> 8);
  write_reg (dev->fd, REG2, reg2);
  regF = (regF & 0x0F) | 0x80;
  write_reg (dev->fd, REGF, regF);
  write_reg (dev->fd, REG0, reg0);
  if (mode == MODE_LINEART)
    {
      write_reg (dev->fd, 0x07, 0x04);
    }
  else
    {
      write_reg (dev->fd, 0x07, 0x00);
    }
  write_reg (dev->fd, REG1, addr >> 8);
  write_reg2 (dev->fd, REG1, addr);
  write_reg (dev->fd, REGF, regF | 0x01);
  write_reg (dev->fd, REG0, reg0 | 0x0C);
  if (mode == MODE_LINEART)
    {
      write_reg (dev->fd, REG1, 0x19);
    }
  else
    {
      write_reg (dev->fd, REG1, 0x11);
    }
  DBG (DBG_proc, "start_scan: exit\n");
  return SANE_STATUS_GOOD;
}

/** read a line of scan data
 * @param dev device to read
 * @param data pointer where to store data
 * @param length total bytes to read on one line
 * @param ltr total number of lines to read
 * @param retry signals that the function must read as much lines it can
 * @param x2 tells that lines must be enlarged by a 2 factor
 * @param mode COLOR_MODE if color mode
 * @returns number of data lines read, -1 in case of error
 */
static int
read_line (P5_Device * dev, uint8_t * data, size_t length, int ltr,
	   SANE_Bool retry, SANE_Bool x2, int mode, SANE_Bool correction)
{
  uint8_t counter, read, cnt;
  uint8_t inbuffer[MAX_SENSOR_PIXELS * 2 * 3 + 2];
  unsigned int i, factor;
  float val;

  DBG (DBG_proc, "read_line: trying to read %d lines of %lu bytes\n", ltr,
       (unsigned long)length);

  counter = read_reg (dev->fd, REG9);
  DBG (DBG_io, "read_line: %d bytes available\n", counter * 256);
  read = 0;
  if (x2 == SANE_FALSE)
    {
      factor = 1;
    }
  else
    {
      factor = 2;
    }

  /* in retry mode we read until not enough data, but in no retry
   * read only one line , counter give us 256 bytes block available
   * and we want an number multiple of color channels */
  cnt = (255 + length / factor) / 256;
  while ((counter > cnt && retry == 1) || (counter > cnt && read == 0))
    {
      /* read data from scanner, first and last byte aren't picture data */
      read_data (dev->fd, inbuffer, length / factor + 2);

      /* image correction */
      if (correction == SANE_TRUE)
	{
	  for (i = 0; i < length / factor; i++)
	    {
	      val = inbuffer[i + 1] - dev->offset[i];
	      if (val > 0)
		{
		  val = val * dev->gain[i];
		  if (val < 255)
		    inbuffer[i + 1] = val;
		  else
		    inbuffer[i + 1] = 255;
		}
	      else
		{
		  inbuffer[i + 1] = 0;
		}
	    }
	}

      /* handle horizontal data doubling */
      if (x2 == SANE_FALSE)
	{
	  memcpy (data + read * length, inbuffer + 1, length);
	}
      else
	{
	  if (mode == MODE_COLOR)
	    {
	      for (i = 0; i < length / factor; i += 3)
		{
		  data[read * length + i * factor] = inbuffer[i + 1];
		  data[read * length + i * factor + 1] = inbuffer[i + 2];
		  data[read * length + i * factor + 2] = inbuffer[i + 3];
		  data[read * length + i * factor + 3] = inbuffer[i + 1];
		  data[read * length + i * factor + 4] = inbuffer[i + 2];
		  data[read * length + i * factor + 5] = inbuffer[i + 3];
		}
	    }
	  else
	    {
	      for (i = 0; i < length / factor; i++)
		{
		  data[read * length + i * factor] = inbuffer[i + 1];
		  data[read * length + i * factor + 1] = inbuffer[i + 1];
		}
	    }
	}
      read++;
      if (retry == SANE_TRUE)
	{
	  read_reg (dev->fd, REGF);
	  read_reg (dev->fd, REGA);
	  read_reg (dev->fd, REG9);
	  counter = read_reg (dev->fd, REG9);
	  read_reg (dev->fd, REGA);
	  if (read >= ltr)
	    {
	      DBG (DBG_io, "read_line returning %d lines\n", read);
	      return read;
	    }
	  counter = read_reg (dev->fd, REG9);
	}
    }
  read_reg (dev->fd, REGF);
  read_reg (dev->fd, REGA);
  read_reg (dev->fd, REG9);
  counter = read_reg (dev->fd, REG9);
  read_reg (dev->fd, REGA);
  DBG (DBG_io, "read_line returning %d lines\n", read);
  return read;
}


static SANE_Status
eject (int fd)
{
  int detector;

  DBG (DBG_proc, "eject: start ...\n");

  do
    {
      write_reg2 (fd, REG1, 0x1110);
      detector = read_reg (fd, REGE);
      detector = read_reg (fd, REGE);
    }
  while ((detector & 0x04) != 0);
  write_reg (fd, REG0, 0x00);
  write_reg (fd, REG1, 0x00);
  write_reg (fd, REGF, 0x82);
  write_reg (fd, REG7, 0x00);

  DBG (DBG_proc, "eject: end.\n");
  return SANE_STATUS_GOOD;
}

/** @brief wait for document to be present in feeder
 * Polls document sensor until something is present. Give up after 20 seconds
 * @param fd file descriptor of the physical device
 */
/* static int
wait_document (int fd, uint8_t detector)
{
  int count = 0;
  uint8_t val;

  write_reg (fd, REG1, 0x00);
  write_reg (fd, REG7, 0x00);
  detector = read_reg (fd, REGE);
  while (detector == 0xc3 && count < 20)
    {
      sleep (1);
      count++;
      detector = read_reg (fd, REGE);
    }
  setadresses (fd, 0x002d, 0x09c7);
  write_reg (fd, REG1, 0x00);
  write_reg (fd, REG2, 0x90);
  write_reg (fd, REGF, 0x82);
  write_reg (fd, REG0, 0x00);
  val = inb (fd, STATUS) & 0xf8;
  if (val != 0xf8)
    {
      DBG (DBG_error, "wait_document: unexpected STATUS value 0x%02x instead of 0xf8", val);
    }
  if (count >= 20)
    {
      DBG (DBG_error, "wait_document: failed to detect document!\n");
      return 0;
    }
  return 1;
} */

/** @brief move at 150 dpi
 * move the paper at 150 dpi motor speed by the amount specified
 * @params dev pointer to the device structure
 */
static SANE_Status
move (P5_Device * dev)
{
  int skip, done, read, count;
  SANE_Status status = SANE_STATUS_GOOD;
  unsigned char buffer[256];

  DBG (DBG_proc, "move: start\n");

  /* compute number of lines to skip */
  skip = dev->ystart;

  /* works, but remains to be explained ... */
  if (dev->ydpi > 300)
    skip = skip / 2;

  DBG (DBG_io, "move: skipping %d lines at %d dpi\n", skip, dev->ydpi);

  /* we do a real scan of small width, discarding data */
  done = 0;
  status = start_scan (dev, MODE_GRAY, dev->ydpi, 0, 256);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (DBG_error, "move: failed to start scan\n");
      return SANE_STATUS_INVAL;
    }

  do
    {
      /* test if document left the feeder */
      status = test_document (dev->fd);
      if (status == SANE_STATUS_NO_DOCS)
	{
	  DBG (DBG_info,
	       "move: document was shorter than the required move\n");
	  return SANE_STATUS_INVAL;
	}
      /* test if data is available */
      count = available_bytes (dev->fd);
      if (count)
	{
	  read =
	    read_line (dev, buffer, 256, 1, SANE_FALSE, SANE_FALSE,
		       MODE_GRAY, SANE_FALSE);
	  if (read == -1)
	    {
	      DBG (DBG_error, "move: failed to read data\n");
	      return SANE_STATUS_INVAL;
	    }
	  done += read;
	}
    }
  while (done < skip);

  /* reset scanner */
  write_reg2 (dev->fd, REG1, 0x1110);
  count = read_reg (dev->fd, REGE);
  count = read_reg (dev->fd, REGE);
  write_reg (dev->fd, REG0, 0x00);
  write_reg (dev->fd, REG1, 0x00);
  write_reg (dev->fd, REGF, 0x82);
  write_reg (dev->fd, REG7, 0x00);

  DBG (DBG_proc, "move: exit\n");
  return status;
}

/** clean up calibration data
 * @param dev device to clean up
 */
static void
cleanup_calibration (P5_Device * dev)
{
  int i;

  for (i = 0; i < MAX_RESOLUTIONS * 2; i++)
    {
      if (dev->calibration_data[i] != NULL)
	{
	  free (dev->calibration_data[i]);
	  dev->calibration_data[i] = NULL;
	}
    }
  dev->calibrated = SANE_FALSE;
}

/** detect a black scan line
 * parses the given buffer and retrun SANE_TRUE if the line is an
 * acceptable black line for calibration
 * @param buffer data line to parse
 * @param pixels number of pixels
 * @param mode MODE_COLOR or MODE_GRAY
 * @returns SANE_TRUE if it is considered as a white line
 */
static SANE_Bool
is_black_line (uint8_t * buffer, unsigned int pixels, int mode)
{
  unsigned int i, start, end, count, width;

  /* compute width in bytes */
  if (mode == MODE_COLOR)
    {
      width = pixels * 3;
    }
  else
    {
      width = pixels;
    }

  /* we allow the calibration target to be narrower than full width, ie
   * black margin at both ends of the line */
  start = (5 * width) / 100;
  end = (95 * width) / 100;
  count = 0;

  /* count number of black bytes */
  for (i = start; i < end; i++)
    {
      if (buffer[i] > BLACK_LEVEL)
	{
	  count++;
	}
    }

  /* we allow 3% black pixels maximum */
  if (count > (3 * width) / 100)
    {
      DBG (DBG_io, "is_black_line=SANE_FALSE\n");
      return SANE_FALSE;
    }

  DBG (DBG_io, "is_black_line=SANE_TRUE\n");
  return SANE_TRUE;
}

/** detect a white scan line
 * parses the given buffer and retrun SANE_TRUE if the line is an
 * acceptable white line for calibration
 * @param buffer data line to parse
 * @param pixels number of pixels
 * @param mode MODE_COLOR or MODE_GRAY
 * @returns SANE_TRUE if it is considered as a white line
 */
static SANE_Bool
is_white_line (uint8_t * buffer, unsigned int pixels, int mode)
{
  unsigned int i, start, end, count, width;

  /* compute width in bytes */
  if (mode == MODE_COLOR)
    {
      width = pixels * 3;
    }
  else
    {
      width = pixels;
    }

  /* we allow the calibration target to be narrower than full width, ie
   * black margin at both ends of the line */
  start = (5 * width) / 100;
  end = (95 * width) / 100;
  count = 0;

  /* count number of black bytes */
  for (i = start; i < end; i++)
    {
      if (buffer[i] < BLACK_LEVEL)
	{
	  count++;
	}
    }

  /* we allow 3% black pixels maximum */
  if (count > (3 * width) / 100)
    {
      DBG (DBG_io, "is_white_line=SANE_FALSE\n");
      return SANE_FALSE;
    }

  DBG (DBG_io, "is_white_line=SANE_TRUE\n");
  return SANE_TRUE;
}

/* ------------------------------------------------------------------------- */
/* writes gray data to a pnm file */
/*
static void
write_gray_data (unsigned char *image, char *name, SANE_Int width,
		 SANE_Int height)
{
  FILE *fdbg = NULL;

  fdbg = fopen (name, "wb");
  if (fdbg == NULL)
    return;
  fprintf (fdbg, "P5\n%d %d\n255\n", width, height);
  fwrite (image, width, height, fdbg);
  fclose (fdbg);
}
*/

/* ------------------------------------------------------------------------- */
/* writes rgb data to a pnm file */
static void
write_rgb_data (char *name, unsigned char *image, SANE_Int width,
		SANE_Int height)
{
  FILE *fdbg = NULL;

  fdbg = fopen (name, "wb");
  if (fdbg == NULL)
    return;
  fprintf (fdbg, "P6\n%d %d\n255\n", width, height);
  fwrite (image, width * 3, height, fdbg);
  fclose (fdbg);
}

/** give calibration file name
 * computes the calibration file name to use based on the
 * backend name and device
 */
static char *
calibration_file (const char *devicename)
{
  char *ptr = NULL;
  char tmp_str[PATH_MAX];

  ptr = getenv ("HOME");
  if (ptr != NULL)
    {
      sprintf (tmp_str, "%s/.sane/p5-%s.cal", ptr, devicename);
    }
  else
    {
      ptr = getenv ("TMPDIR");
      if (ptr != NULL)
	{
	  sprintf (tmp_str, "%s/p5-%s.cal", ptr, devicename);
	}
      else
	{
	  sprintf (tmp_str, "/tmp/p5-%s.cal", devicename);
	}
    }
  DBG (DBG_trace, "calibration_file: using >%s< for calibration file name\n",
       tmp_str);
  return strdup (tmp_str);
}

/** restore calibration data
 * restore calibration data by loading previously saved calibration data
 * @param dev device to restore
 * @return SANE_STATUS_GOOD on success, otherwise error code
 */
static SANE_Status
restore_calibration (P5_Device * dev)
{
  SANE_Status status = SANE_STATUS_GOOD;
  char *fname = NULL;
  FILE *fcalib = NULL;
  size_t size;
  int i;

  DBG (DBG_proc, "restore_calibration: start\n");
  cleanup_calibration (dev);
  fname = calibration_file (dev->model->name);
  fcalib = fopen (fname, "rb");
  if (fcalib == NULL)
    {
      DBG (DBG_error, "restore_calibration: failed to open %s!\n", fname);
      free (fname);
      return SANE_STATUS_IO_ERROR;
    }

  /* loop filling calibration data until EOF reached */
  i = 0;
  while (!feof (fcalib) && (i < 2 * MAX_RESOLUTIONS))
    {
      dev->calibration_data[i] = malloc (sizeof (P5_Calibration_Data));
      if (dev->calibration_data[i] == NULL)
	{
	  cleanup_calibration (dev);
	  free (fname);
	  fclose (fcalib);
	  DBG (DBG_error,
	       "restore_calibration: failed to allocate memory for calibration\n");
	  return SANE_STATUS_NO_MEM;
	}
      size =
	fread (dev->calibration_data[i], 1, sizeof (P5_Calibration_Data),
	       fcalib);
      if (feof (fcalib))
	{
	  free (dev->calibration_data[i]);
	  dev->calibration_data[i] = NULL;
	}
      else if (size != sizeof (P5_Calibration_Data))
	{
	  cleanup_calibration (dev);
	  free (fname);
	  fclose (fcalib);
	  DBG (DBG_error, "restore_calibration: failed to read from file\n");
	  return SANE_STATUS_IO_ERROR;
	}
      DBG (DBG_trace,
	   "restore_calibration: read 1 calibration structure from file\n");
      i++;
    }

  dev->calibrated = SANE_TRUE;
  fclose (fcalib);
  free (fname);

  DBG (DBG_proc, "restore_calibration: end\n");
  return status;
}

/** save calibration data
 * save calibration data from memory to file
 * @param dev device calibration to save
 * @return SANE_STATUS_GOOD on success, otherwise error code
 */
static SANE_Status
save_calibration (P5_Device * dev)
{
  SANE_Status status = SANE_STATUS_GOOD;
  char *fname = NULL;
  FILE *fcalib = NULL;
  int i;
  size_t size;

  DBG (DBG_proc, "save_calibration: start\n");
  fname = calibration_file (dev->model->name);
  fcalib = fopen (fname, "wb");
  if (fcalib == NULL)
    {
      DBG (DBG_error, "save_calibration: failed to open %s!\n", fname);
      free (fname);
      return SANE_STATUS_IO_ERROR;
    }

  /* loop filling calibration data until EOF reached */
  i = 0;
  while (dev->calibration_data[i] != NULL && (i < 2 * MAX_RESOLUTIONS))
    {
      size =
	fwrite (dev->calibration_data[i], sizeof (P5_Calibration_Data), 1,
		fcalib);
      if (size != sizeof (P5_Calibration_Data))
	{
	  free (fname);
	  fclose (fcalib);
	  DBG (DBG_error, "save_calibration: failed to write to file\n");
	  return SANE_STATUS_IO_ERROR;
	}
      DBG (DBG_trace,
	   "save_calibration: wrote 1 calibration structure to file\n");
      i++;
    }

  fclose (fcalib);
  free (fname);

  DBG (DBG_proc, "save_calibration: end\n");
  return status;
}

/** calibrate scanner
 * calibrates scanner by scanning a white sheet to get
 * reference data. The black reference data is extracted from the lines
 * that precede the physical document.
 * Calibration is done at 300 color, then data is built for other modes
 * and resolutions.
 * @param dev device to calibrate
 */
static SANE_Status
sheetfed_calibration (P5_Device * dev)
{
  uint8_t buffer[MAX_SENSOR_PIXELS * 3];
  uint16_t white_data[MAX_SENSOR_PIXELS * 3];
  uint16_t black_data[MAX_SENSOR_PIXELS * 3];
  unsigned int i, j, k, dpi, pixels, read, black, white;
  float coeff;
  unsigned int red, green, blue;
  int line;
  SANE_Status status;
  char title[40];

  FILE *dbg = fopen ("debug.pnm", "wb");
  fprintf (dbg, "P6\n%d %d\n255\n", MAX_SENSOR_PIXELS,
	   CALIBRATION_SKIP_LINES * 4);

  DBG (DBG_proc, "sheetfed_calibration: start\n");

  /* check calibration target has been loaded in ADF */
  status = test_document (dev->fd);
  if (status == SANE_STATUS_NO_DOCS)
    {
      DBG (DBG_error,
	   "sheetfed_calibration: no calibration target present!\n");
      return SANE_STATUS_NO_DOCS;
    }

  /* clean up calibration data */
  cleanup_calibration (dev);

  /*  a RGB scan to get reference data */
  /* initialize calibration slot for the resolution */
  i = 0;
  dpi = dev->model->max_xdpi;
  pixels = MAX_SENSOR_PIXELS;
  dev->calibration_data[i] =
    (P5_Calibration_Data *) malloc (sizeof (P5_Calibration_Data));
  if (dev->calibration_data[i] == NULL)
    {
      cleanup_calibration (dev);
      DBG (DBG_error,
	   "sheetfed_calibration: failed to allocate memory for calibration\n");
      return SANE_STATUS_NO_MEM;
    }
  dev->calibration_data[i]->dpi = dpi;

  /* start scan */
  status = start_scan (dev, MODE_COLOR, dpi, 0, pixels);
  if (status != SANE_STATUS_GOOD)
    {
      cleanup_calibration (dev);
      DBG (DBG_error,
	   "sheetfed_calibration: failed to start scan at %d dpi\n", dpi);
      return SANE_STATUS_INVAL;
    }

  white = 0;
  black = 0;
  read = 0;
  for (j = 0; j < pixels * 3; j++)
    {
      black_data[j] = 0;
      white_data[j] = 0;
    }

  /* read lines and gather black and white ones until enough for sensor's
   * native resolution */
  do
    {
      status = test_document (dev->fd);
      if (status == SANE_STATUS_NO_DOCS && (white < 10 || black < 10))
	{
	  cleanup_calibration (dev);
	  DBG (DBG_error,
	       "sheetfed_calibration: calibration sheet too short!\n");
	  return SANE_STATUS_INVAL;
	}
      memset (buffer, 0x00, MAX_SENSOR_PIXELS * 3);
      line =
	read_line (dev, buffer, pixels * 3, 1, SANE_FALSE, SANE_FALSE,
		   MODE_COLOR, SANE_FALSE);
      if (line == -1)
	{
	  DBG (DBG_error, "sheetfed_calibration: failed to read data\n");
	  return SANE_STATUS_INVAL;
	}

      /* if a data line has been read, add it to reference data */
      if (line)
	{
	  read++;
	  fwrite (buffer, pixels * 3, 1, dbg);
	  if (is_white_line (buffer, pixels, MODE_COLOR) && white < 256)
	    {
	      white++;
	      /* first calibration lines are skipped */
	      for (j = 0; j < pixels * 3 && read > CALIBRATION_SKIP_LINES;
		   j++)
		{
		  white_data[j] += buffer[j];
		}
	    }
	  if (is_black_line (buffer, pixels, MODE_COLOR) && black < 256)
	    {
	      black++;
	      for (j = 0; j < pixels * 3; j++)
		{
		  black_data[j] += buffer[j];
		}
	    }
	}
    }
  while (test_document (dev->fd) != SANE_STATUS_NO_DOCS);
  DBG (DBG_trace, "sheetfed_calibration: white lines=%d, black lines=%d\n",
       white, black);

  /* average pixels and store in per dpi calibration data */
  for (j = 0; j < pixels * 3; j++)
    {
      dev->calibration_data[i]->white_data[j] = white_data[j] / white;
      dev->calibration_data[i]->black_data[j] = black_data[j] / black;
    }

  /* we average red, green and blue offset on the full sensor */
  red = 0;
  green = 0;
  blue = 0;
  for (j = 0; j < pixels * 3; j += 3)
    {
      red += dev->calibration_data[i]->black_data[j];
      green += dev->calibration_data[i]->black_data[j + 1];
      blue += dev->calibration_data[i]->black_data[j + 2];
    }
  for (j = 0; j < pixels * 3; j += 3)
    {
      dev->calibration_data[i]->black_data[j] = red / pixels;
      dev->calibration_data[i]->black_data[j + 1] = green / pixels;
      dev->calibration_data[i]->black_data[j + 2] = blue / pixels;
    }

  /* trace calibration data for debug */
  if (DBG_LEVEL > DBG_data)
    {
      sprintf (title, "calibration-white-%d.pnm",
	       dev->calibration_data[i]->dpi);
      write_rgb_data (title, dev->calibration_data[i]->white_data, pixels, 1);
      sprintf (title, "calibration-black-%d.pnm",
	       dev->calibration_data[i]->dpi);
      write_rgb_data (title, dev->calibration_data[i]->black_data, pixels, 1);
    }

  /* loop on all remaining resolution and compute calibration data from it */
  for (i = 1; i < MAX_RESOLUTIONS && dev->model->xdpi_values[i] > 0; i++)
    {
      dev->calibration_data[i] =
	(P5_Calibration_Data *) malloc (sizeof (P5_Calibration_Data));
      if (dev->calibration_data[i] == NULL)
	{
	  cleanup_calibration (dev);
	  DBG (DBG_error,
	       "sheetfed_calibration: failed to allocate memory for calibration\n");
	  return SANE_STATUS_INVAL;
	}
      dev->calibration_data[i]->dpi = dev->model->xdpi_values[i];

      coeff = ((float) dev->model->xdpi_values[i]) / (float) dpi;

      /* generate data by decimation */
      for (j = 0; j < pixels / coeff; j++)
	{
	  k = j * coeff;
	  dev->calibration_data[i]->white_data[j] =
	    dev->calibration_data[0]->white_data[k];
	  dev->calibration_data[i]->white_data[j + 1] =
	    dev->calibration_data[0]->white_data[k + 1];
	  dev->calibration_data[i]->white_data[j + 2] =
	    dev->calibration_data[0]->white_data[k + 2];
	  dev->calibration_data[i]->black_data[j] =
	    dev->calibration_data[0]->black_data[k];
	  dev->calibration_data[i]->black_data[j + 1] =
	    dev->calibration_data[0]->black_data[k + 1];
	  dev->calibration_data[i]->black_data[j + 2] =
	    dev->calibration_data[0]->black_data[k + 2];
	}
    }

  fclose (dbg);
  dev->calibrated = SANE_TRUE;

  /* eject calibration target */
  eject (dev->fd);

  DBG (DBG_proc, "sheetfed_calibration: end\n");
  return SANE_STATUS_GOOD;
}

/* vim: set sw=2 cino=>2se-1sn-1s{s^-1st0(0u0 smarttab expandtab: */
