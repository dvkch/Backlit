/* sane - Scanner Access Now Easy.
   Copyright (C) 1997 Geoffrey T. Dairiki
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

   This file is part of a SANE backend for HP Scanners supporting
   HP Scanner Control Language (SCL).
*/

/*#define STUBS
extern int sanei_debug_hp;*/
#define DEBUG_DECLARE_ONLY
#include "../include/sane/config.h"

#include <stdlib.h>
#include <string.h>
#include "../include/lassert.h"
#include "hp-device.h"
#include "hp-accessor.h"
#include "hp-option.h"
#include "hp-scsi.h"
#include "hp-scl.h"

/* Mark an scl-command to be simulated */
SANE_Status
sanei_hp_device_simulate_set (const char *devname, HpScl scl, int flag)

{HpDeviceInfo *info;
 int inqid;

 info = sanei_hp_device_info_get ( devname );
 if (!info) return SANE_STATUS_INVAL;

 inqid = SCL_INQ_ID(scl)-HP_SCL_INQID_MIN;
 info->simulate.sclsimulate[inqid] = flag;

 DBG(3, "hp_device_simulate_set: %d set to %ssimulated\n",
     inqid+HP_SCL_INQID_MIN, flag ? "" : "not ");

 return SANE_STATUS_GOOD;
}

/* Clear all simulation flags */
void
sanei_hp_device_simulate_clear (const char *devname)

{HpDeviceInfo *info;

 info = sanei_hp_device_info_get ( devname );
 if (!info) return;

 memset (&(info->simulate.sclsimulate[0]), 0,
         sizeof (info->simulate.sclsimulate));

 info->simulate.gamma_simulate = 0;
}

/* Get simulate flag for an scl-command */
hp_bool_t
sanei_hp_device_simulate_get (const char *devname, HpScl scl)

{HpDeviceInfo *info;
 int inqid;

 info = sanei_hp_device_info_get ( devname );
 if (!info) return 0;

 inqid = SCL_INQ_ID(scl)-HP_SCL_INQID_MIN;
 return info->simulate.sclsimulate[inqid];
}

SANE_Status
sanei_hp_device_support_get (const char *devname, HpScl scl,
                             int *minval, int *maxval)

{HpDeviceInfo *info;
 HpSclSupport *sclsupport;
 int inqid;

/* #define HP_TEST_SIMULATE */
#ifdef HP_TEST_SIMULATE
  if (scl == SCL_BRIGHTNESS) return SANE_STATUS_UNSUPPORTED;
  if (scl == SCL_CONTRAST) return SANE_STATUS_UNSUPPORTED;
  if (scl == SCL_DOWNLOAD_TYPE)
  {
     *minval = 2; *maxval = 14;
     return SANE_STATUS_GOOD;
  }
#endif

 info = sanei_hp_device_info_get ( devname );
 if (!info) return SANE_STATUS_INVAL;

 inqid = SCL_INQ_ID(scl)-HP_SCL_INQID_MIN;
 sclsupport = &(info->sclsupport[inqid]);

 if ( !(sclsupport->checked) ) return SANE_STATUS_INVAL;
 if ( !(sclsupport->is_supported) ) return SANE_STATUS_UNSUPPORTED;

 if (minval) *minval = sclsupport->minval;
 if (maxval) *maxval = sclsupport->maxval;

 return SANE_STATUS_GOOD;
}

/* Update the list of supported commands */
SANE_Status
sanei_hp_device_support_probe (HpScsi scsi)

{HpDeviceInfo *info;
 HpSclSupport *sclsupport;
 SANE_Status status;
 int k, val, inqid;
 static HpScl sclprobe[] =  /* The commands that should be probed */
 {
   SCL_AUTO_BKGRND,
   SCL_COMPRESSION,
   SCL_DOWNLOAD_TYPE,
   SCL_X_SCALE,
   SCL_Y_SCALE,
   SCL_DATA_WIDTH,
   SCL_INVERSE_IMAGE,
   SCL_BW_DITHER,
   SCL_CONTRAST,
   SCL_BRIGHTNESS,
#ifdef SCL_SHARPENING
   SCL_SHARPENING,
#endif
   SCL_MIRROR_IMAGE,
   SCL_X_RESOLUTION,
   SCL_Y_RESOLUTION,
   SCL_OUTPUT_DATA_TYPE,
   SCL_PRELOAD_ADF,
   SCL_MEDIA,
   SCL_X_EXTENT,
   SCL_Y_EXTENT,
   SCL_X_POS,
   SCL_Y_POS,
   SCL_SPEED,
   SCL_FILTER,
   SCL_TONE_MAP,
   SCL_MATRIX,
   SCL_UNLOAD,
   SCL_CHANGE_DOC,
   SCL_ADF_BFEED
 };
 enum hp_device_compat_e compat;

 DBG(1, "hp_device_support_probe: Check supported commands for %s\n",
     sanei_hp_scsi_devicename (scsi) );

 info = sanei_hp_device_info_get ( sanei_hp_scsi_devicename (scsi) );
 assert (info);

 memset (&(info->sclsupport[0]), 0, sizeof (info->sclsupport));

 for (k = 0; k < (int)(sizeof (sclprobe) / sizeof (sclprobe[0])); k++)
 {
   inqid = SCL_INQ_ID(sclprobe[k])-HP_SCL_INQID_MIN;
   sclsupport = &(info->sclsupport[inqid]);
   status = sanei_hp_scl_inquire (scsi, sclprobe[k], &val,
                                  &(sclsupport->minval),
                                  &(sclsupport->maxval));
   sclsupport->is_supported = (status == SANE_STATUS_GOOD);
   sclsupport->checked = 1;

   /* The OfficeJets seem to ignore brightness and contrast settings,
    * so we'll pretend they're not supported at all. */
   if (((sclprobe[k]==SCL_BRIGHTNESS) || (sclprobe[k]==SCL_CONTRAST)) &&
       (sanei_hp_device_probe (&compat, scsi) == SANE_STATUS_GOOD) &&
       (compat & HP_COMPAT_OJ_1150C)) {
	 sclsupport->is_supported=0;
   }

   if (sclsupport->is_supported)
   {
     DBG(1, "hp_device_support_probe: %d supported (%d..%d, %d)\n",
         inqid+HP_SCL_INQID_MIN, sclsupport->minval, sclsupport->maxval, val);
   }
   else
   {
     DBG(1, "hp_device_support_probe: %d not supported\n",
         inqid+HP_SCL_INQID_MIN);
   }
 }

 return SANE_STATUS_GOOD;
}

SANE_Status
sanei_hp_device_probe_model (enum hp_device_compat_e *compat, HpScsi scsi,
                             int *model_num, const char **model_name)
{
  static struct {
      HpScl		cmd;
      int               model_num;
      const char *	model;
      enum hp_device_compat_e	flag;
  }	probes[] = {
      { SCL_HP_MODEL_1, 1, "ScanJet Plus",             HP_COMPAT_PLUS },
      { SCL_HP_MODEL_2, 2, "ScanJet IIc",              HP_COMPAT_2C },
      { SCL_HP_MODEL_3, 3, "ScanJet IIp",              HP_COMPAT_2P },
      { SCL_HP_MODEL_4, 4, "ScanJet IIcx",             HP_COMPAT_2CX },
      { SCL_HP_MODEL_5, 5, "ScanJet 3c/4c/6100C",      HP_COMPAT_4C },
      { SCL_HP_MODEL_6, 6, "ScanJet 3p",               HP_COMPAT_3P },
      { SCL_HP_MODEL_8, 8, "ScanJet 4p",               HP_COMPAT_4P },
      { SCL_HP_MODEL_9, 9, "ScanJet 5p/4100C/5100C",   HP_COMPAT_5P },
      { SCL_HP_MODEL_10,10,"PhotoSmart Photo Scanner", HP_COMPAT_PS },
      { SCL_HP_MODEL_11,11,"OfficeJet 1150C",          HP_COMPAT_OJ_1150C },
      { SCL_HP_MODEL_12,12,"OfficeJet 1170C or later", HP_COMPAT_OJ_1170C },
      { SCL_HP_MODEL_14,14,"ScanJet 62x0C",            HP_COMPAT_6200C },
      { SCL_HP_MODEL_16,15,"ScanJet 5200C",            HP_COMPAT_5200C },
      { SCL_HP_MODEL_17,17,"ScanJet 63x0C",            HP_COMPAT_6300C }
  };
  int		i;
  char		buf[8];
  size_t	len;
  SANE_Status	status;
  static char	*last_device = NULL;
  static enum hp_device_compat_e last_compat;
  static int    last_model_num = -1;
  static const char *last_model_name = "Model Unknown";

  assert(scsi);
  DBG(1, "probe_scanner: Probing %s\n", sanei_hp_scsi_devicename (scsi));

  if (last_device != NULL)  /* Look if we already probed the device */
  {
    if (strcmp (last_device, sanei_hp_scsi_devicename (scsi)) == 0)
    {
      DBG(3, "probe_scanner: use cached compatibility flags\n");
      *compat = last_compat;
      if (model_num) *model_num = last_model_num;
      if (model_name) *model_name = last_model_name;
      return SANE_STATUS_GOOD;
    }
    sanei_hp_free (last_device);
    last_device = NULL;
  }
  *compat = 0;
  last_model_num = -1;
  last_model_name = "Model Unknown";
  for (i = 0; i < (int)(sizeof(probes)/sizeof(probes[0])); i++)
    {
      DBG(1,"probing %s\n",probes[i].model);

      len = sizeof(buf);
      if (!FAILED( status = sanei_hp_scl_upload(scsi, probes[i].cmd,
					  buf, sizeof(buf)) ))
	{
	  DBG(1, "probe_scanner: %s compatible (%5s)\n", probes[i].model, buf);
          last_model_name = probes[i].model;
          /* Some scanners have different responses */
          if (probes[i].model_num == 9)
          {
            if (strncmp (buf, "5110A", 5) == 0)
              last_model_name = "ScanJet 5p";
            else if (strncmp (buf, "5190A", 5) == 0)
              last_model_name = "ScanJet 5100C";
            else if (strncmp (buf, "6290A", 5) == 0)
              last_model_name = "ScanJet 4100C";
          }
	  *compat |= probes[i].flag;
          last_model_num = probes[i].model_num;
	}
      else if (!UNSUPPORTED( status ))
	  return status;	/* SCL inquiry failed */
    }
  /* Save values for next call */
  last_device = sanei_hp_strdup (sanei_hp_scsi_devicename (scsi));
  last_compat = *compat;
  if (model_num) *model_num = last_model_num;
  if (model_name) *model_name = last_model_name;

  return SANE_STATUS_GOOD;
}

SANE_Status
sanei_hp_device_probe (enum hp_device_compat_e *compat, HpScsi scsi)
{
  return sanei_hp_device_probe_model (compat, scsi, 0, 0);
}

hp_bool_t
sanei_hp_device_compat (HpDevice this, enum hp_device_compat_e which)
{
  return (this->compat & which) != 0;
}

static SANE_Status
hp_nonscsi_device_new (HpDevice * newp, const char * devname, HpConnect connect)
{
  HpDevice	this;
  HpScsi	scsi;
  SANE_Status	status;
  const char *  model_name = "ScanJet";

  if (FAILED( sanei_hp_nonscsi_new(&scsi, devname, connect) ))
  {
    DBG(1, "%s: Can't open nonscsi device\n", devname);
    return SANE_STATUS_INVAL;	/* Can't open device */
  }

  /* reset scanner; returns all parameters to defaults */
  if (FAILED( sanei_hp_scl_reset(scsi) ))
    {
      DBG(1, "hp_nonscsi_device_new: SCL reset failed\n");
      sanei_hp_scsi_destroy(scsi,1);
      return SANE_STATUS_IO_ERROR;
    }

  /* Things seem okay, allocate new device */
  this = sanei_hp_allocz(sizeof(*this));
  this->data = sanei_hp_data_new();

  if (!this || !this->data)
      return SANE_STATUS_NO_MEM;

  this->sanedev.name = sanei_hp_strdup(devname);
  if (!this->sanedev.name)
      return SANE_STATUS_NO_MEM;
  this->sanedev.vendor = "Hewlett-Packard";
  this->sanedev.type   = "flatbed scanner";

  status = sanei_hp_device_probe_model (&(this->compat), scsi, 0, &model_name);
  if (!FAILED(status))
  {
      sanei_hp_device_support_probe (scsi);
      status = sanei_hp_optset_new(&(this->options), scsi, this);
  }
  sanei_hp_scsi_destroy(scsi,1);

  if (!model_name) model_name = "ScanJet";
  this->sanedev.model = sanei_hp_strdup (model_name);
  if (!this->sanedev.model)
      return SANE_STATUS_NO_MEM;

  if (FAILED(status))
    {
      DBG(1, "hp_nonscsi_device_new: %s: probe failed (%s)\n",
	  devname, sane_strstatus(status));
      sanei_hp_data_destroy(this->data);
      sanei_hp_free((void *)this->sanedev.name);
      sanei_hp_free((void *)this->sanedev.model);
      sanei_hp_free(this);
      return status;
    }

  DBG(1, "hp_nonscsi_device_new: %s: found HP ScanJet model %s\n",
      devname, this->sanedev.model);

  *newp = this;
  return SANE_STATUS_GOOD;
}

SANE_Status
sanei_hp_device_new (HpDevice * newp, const char * devname)
{
  HpDevice	this;
  HpScsi	scsi;
  HpConnect     connect;
  SANE_Status	status;
  char *	str;

  DBG(3, "sanei_hp_device_new: %s\n", devname);

  connect = sanei_hp_get_connect (devname);
  if ( connect != HP_CONNECT_SCSI )
    return hp_nonscsi_device_new (newp, devname, connect);

  if (FAILED( sanei_hp_scsi_new(&scsi, devname) ))
    {
      DBG(1, "%s: Can't open scsi device\n", devname);
      return SANE_STATUS_INVAL;	/* Can't open device */
    }

  if (sanei_hp_scsi_inq(scsi)[0] != 0x03
      || memcmp(sanei_hp_scsi_vendor(scsi), "HP      ", 8) != 0)
    {
      DBG(1, "%s: does not seem to be an HP scanner\n", devname);
      sanei_hp_scsi_destroy(scsi,1);
      return SANE_STATUS_INVAL;
    }

  /* reset scanner; returns all parameters to defaults */
  if (FAILED( sanei_hp_scl_reset(scsi) ))
    {
      DBG(1, "sanei_hp_device_new: SCL reset failed\n");
      sanei_hp_scsi_destroy(scsi,1);
      return SANE_STATUS_IO_ERROR;
    }

  /* Things seem okay, allocate new device */
  this = sanei_hp_allocz(sizeof(*this));
  this->data = sanei_hp_data_new();

  if (!this || !this->data)
      return SANE_STATUS_NO_MEM;

  this->sanedev.name = sanei_hp_strdup(devname);
  str = sanei_hp_strdup(sanei_hp_scsi_model(scsi));
  if (!this->sanedev.name || !str)
      return SANE_STATUS_NO_MEM;
  this->sanedev.model = str;
  if ((str = strchr(str, ' ')) != 0)
      *str = '\0';
  this->sanedev.vendor = "Hewlett-Packard";
  this->sanedev.type   = "flatbed scanner";

  status = sanei_hp_device_probe(&(this->compat), scsi);
  if (!FAILED(status))
  {
      sanei_hp_device_support_probe (scsi);
      status = sanei_hp_optset_new(&this->options, scsi, this);
  }
  sanei_hp_scsi_destroy(scsi,1);

  if (FAILED(status))
    {
      DBG(1, "sanei_hp_device_new: %s: probe failed (%s)\n",
	  devname, sane_strstatus(status));
      sanei_hp_data_destroy(this->data);
      sanei_hp_free((void *)this->sanedev.name);
      sanei_hp_free((void *)this->sanedev.model);
      sanei_hp_free(this);
      return status;
    }

  DBG(1, "sanei_hp_device_new: %s: found HP ScanJet model %s\n",
      devname, this->sanedev.model);

  *newp = this;
  return SANE_STATUS_GOOD;
}

const SANE_Device *
sanei_hp_device_sanedevice (HpDevice this)
{
  return &this->sanedev;
}

