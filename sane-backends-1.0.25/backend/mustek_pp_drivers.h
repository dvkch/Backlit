
#ifndef MUSTEK_PP_DRIVERS_H
#define MUSTEK_PP_DRIVERS_H

#include "mustek_pp.h"
#include "mustek_pp_decl.h"

static Mustek_pp_Functions Mustek_pp_Drivers[] = {
	{
		"debug", "Jochen Eisinger", "0.11-devel",
		debug_drv_init,
		debug_drv_capabilities,
		debug_drv_open,
		debug_drv_setup,
		debug_drv_config,
		debug_drv_close,
		debug_drv_start,
		debug_drv_read,
		debug_drv_stop
	},
	{
		"cis600", "Eddy De Greef", "0.13-beta",
		cis600_drv_init,
		cis_drv_capabilities,
		cis_drv_open,
		cis_drv_setup,
		cis_drv_config,
		cis_drv_close,
		cis_drv_start,
		cis_drv_read,
		cis_drv_stop
	},
	{
		"cis1200", "Eddy De Greef", "0.13-beta",
		cis1200_drv_init,
		cis_drv_capabilities,
		cis_drv_open,
		cis_drv_setup,
		cis_drv_config,
		cis_drv_close,
		cis_drv_start,
		cis_drv_read,
		cis_drv_stop
	},
	{
		"cis1200+", "Eddy De Greef", "0.13-beta",
		cis1200p_drv_init,
		cis_drv_capabilities,
		cis_drv_open,
		cis_drv_setup,
		cis_drv_config,
		cis_drv_close,
		cis_drv_start,
		cis_drv_read,
		cis_drv_stop
	},
	{
		"ccd300", "Jochen Eisinger", "0.11-devel",
		ccd300_init,
		ccd300_capabilities,
		ccd300_open,
		ccd300_setup,
		ccd300_config,
		ccd300_close,
		ccd300_start,
		ccd300_read,
		ccd300_stop
	}

};

#endif
