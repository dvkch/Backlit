/*
 * ESC/I commands for Epson scanners
 *
 * Based on Kazuhiro Sasayama previous
 * Work on epson.[ch] file from the SANE package.
 * Please see those files for original copyrights.
 *
 * Copyright (C) 2006 Tower Technologies
 * Author: Alessandro Zummo <a.zummo@towertech.it>
 *
 * This file is part of the SANE package.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2.
 */

#define DEBUG_DECLARE_ONLY

#include "sane/config.h"

#include <byteorder.h>
#include <math.h>
#include <sys/types.h>

#include "epson2.h"
#include "epson2-io.h"
#include "epson2-commands.h"


/* ESC H, set zoom */
SANE_Status
esci_set_zoom(Epson_Scanner * s, unsigned char x, unsigned char y)
{
	SANE_Status status;
	unsigned char params[2];

	DBG(8, "%s: x = %d, y = %d\n", __func__, x, y);

	if (!s->hw->cmd->set_zoom) {
		DBG(1, "%s: not supported\n", __func__);
		return SANE_STATUS_GOOD;
	}

	params[0] = ESC;
	params[1] = s->hw->cmd->set_zoom;

	status = e2_cmd_simple(s, params, 2);
	if (status != SANE_STATUS_GOOD)
		return status;

	params[0] = x;
	params[1] = y;

	return e2_cmd_simple(s, params, 2);
}

/* ESC R */
SANE_Status
esci_set_resolution(Epson_Scanner * s, int x, int y)
{
	SANE_Status status;
	unsigned char params[4];

	DBG(8, "%s: x = %d, y = %d\n", __func__, x, y);

	if (!s->hw->cmd->set_resolution) {
		DBG(1, "%s: not supported\n", __func__);
		return SANE_STATUS_GOOD;
	}

	params[0] = ESC;
	params[1] = s->hw->cmd->set_resolution;

	status = e2_cmd_simple(s, params, 2);
	if (status != SANE_STATUS_GOOD)
		return status;

	params[0] = x;
	params[1] = x >> 8;
	params[2] = y;
	params[3] = y >> 8;

	return e2_cmd_simple(s, params, 4);
}

/*
 * Sends the "set scan area" command to the scanner with the currently selected
 * scan area. This scan area must be already corrected for "color shuffling" if
 * necessary.
 */

SANE_Status
esci_set_scan_area(Epson_Scanner * s, int x, int y, int width, int height)
{
	SANE_Status status;
	unsigned char params[8];

	DBG(8, "%s: x = %d, y = %d, w = %d, h = %d\n",
	    __func__, x, y, width, height);

	if (!s->hw->cmd->set_scan_area) {
		DBG(1, "%s: not supported\n", __func__);
		return SANE_STATUS_UNSUPPORTED;
	}

	/* verify the scan area */
	if (x < 0 || y < 0 || width <= 0 || height <= 0)
		return SANE_STATUS_INVAL;

	params[0] = ESC;
	params[1] = s->hw->cmd->set_scan_area;

	status = e2_cmd_simple(s, params, 2);
	if (status != SANE_STATUS_GOOD)
		return status;

	params[0] = x;
	params[1] = x >> 8;
	params[2] = y;
	params[3] = y >> 8;
	params[4] = width;
	params[5] = width >> 8;
	params[6] = height;
	params[7] = height >> 8;

	return e2_cmd_simple(s, params, 8);
}

static int
get_roundup_index(double frac[], int n)
{
	int i, index = -1;
	double max_val = 0.0;

	for (i = 0; i < n; i++) {

		if (frac[i] < 0)
			continue;

		if (max_val < frac[i]) {
			index = i;
			max_val = frac[i];
		}
	}

	return index;
}

static int
get_rounddown_index(double frac[], int n)
{
	int i, index = -1;
	double min_val = 1.0;

	for (i = 0; i < n; i++) {

		if (frac[i] > 0)
			continue;

		if (min_val > frac[i]) {
			index = i;
			min_val = frac[i];
		}
	}

	return index;
}

static unsigned char
int2cpt(int val)
{
	if (val >= 0) {

		if (val > 127)
			val = 127;

		return (unsigned char) val;

	} else {

		val = -val;

		if (val > 127)
			val = 127;

		return (unsigned char) (0x80 | val);
	}
}

static void
round_cct(double org_cct[], int rnd_cct[])
{
	int loop = 0;
	int i, j, sum[3];
	double mult_cct[9], frac[9];

	for (i = 0; i < 9; i++) {
  		mult_cct[i] = org_cct[i] * 32;
		rnd_cct[i] = (int) floor(mult_cct[i] + 0.5);
	}
	
	do {
		for (i = 0; i < 3; i++) {

			int k = i * 3;

			if ((rnd_cct[k] == 11) &&
				(rnd_cct[k] == rnd_cct[k + 1]) &&
				(rnd_cct[k] == rnd_cct[k + 2])) {

				rnd_cct[k + i]--;
				mult_cct[k + i] = rnd_cct[k + i];
			}
		}

		for (i = 0; i < 3; i++) {

			int k = i * 3;

			for (sum[i] = j = 0; j < 3; j++)
				sum[i] += rnd_cct[k + j];
		}

		for (i = 0; i < 9; i++)
			frac[i] = mult_cct[i] - rnd_cct[i];

		for (i = 0; i < 3; i++) {

			int k = i * 3;

			if (sum[i] < 32) {

				int index = get_roundup_index(&frac[k], 3);
				if (index != -1) {
					rnd_cct[k + index]++;
					mult_cct[k + index] = rnd_cct[k + index];
					sum[i]++;
				}

			} else if (sum[i] > 32) {

				int index = get_rounddown_index(&frac[k], 3);
				if (index != -1) {
					rnd_cct[k + index]--;
					mult_cct[k + index] = rnd_cct[k + index];
					sum[i]--;
				}
			}
		}
	}

	while ((++loop < 2)
		&& ((sum[0] != 32) || (sum[1] != 32) || (sum[2] != 32)));
}

static void
profile_to_colorcoeff(double *profile, unsigned char *color_coeff)
{
	int cc_idx[] = { 4, 1, 7, 3, 0, 6, 5, 2, 8 };
	int i, color_table[9];

  	round_cct(profile, color_table);

  	for (i = 0; i < 9; i++)
  		color_coeff[i] = int2cpt(color_table[cc_idx[i]]);
}
                                            
                                            
/*
 * Sends the "set color correction coefficients" command with the
 * currently selected parameters to the scanner.
 */

SANE_Status
esci_set_color_correction_coefficients(Epson_Scanner * s, SANE_Word *table)
{
	SANE_Status status;
	unsigned char params[2];
	unsigned char data[9];
	double cct[9];

	DBG(8, "%s\n", __func__);
	if (!s->hw->cmd->set_color_correction_coefficients) {
		DBG(1, "%s: not supported\n", __func__);
		return SANE_STATUS_UNSUPPORTED;
	}

	params[0] = ESC;
	params[1] = s->hw->cmd->set_color_correction_coefficients;

	status = e2_cmd_simple(s, params, 2);
	if (status != SANE_STATUS_GOOD)
		return status;

	cct[0] = SANE_UNFIX(table[0]);
	cct[1] = SANE_UNFIX(table[1]);
	cct[2] = SANE_UNFIX(table[2]);
	cct[3] = SANE_UNFIX(table[3]);
	cct[4] = SANE_UNFIX(table[4]);
	cct[5] = SANE_UNFIX(table[5]);
	cct[6] = SANE_UNFIX(table[6]);
	cct[7] = SANE_UNFIX(table[7]);
	cct[8] = SANE_UNFIX(table[8]);

	profile_to_colorcoeff(cct, data);

	DBG(11, "%s: %d,%d,%d %d,%d,%d %d,%d,%d\n", __func__,
	    data[0] , data[1], data[2], data[3],
	    data[4], data[5], data[6], data[7], data[8]);

	return e2_cmd_simple(s, data, 9);
}

SANE_Status
esci_set_gamma_table(Epson_Scanner * s)
{
	SANE_Status status;
	unsigned char params[2];
	unsigned char gamma[257];
	int n;
	int table;

/*	static const char gamma_cmds[] = { 'M', 'R', 'G', 'B' }; */
	static const char gamma_cmds[] = { 'R', 'G', 'B' };

	DBG(8, "%s\n", __func__);
	if (!s->hw->cmd->set_gamma_table)
		return SANE_STATUS_UNSUPPORTED;

	params[0] = ESC;
	params[1] = s->hw->cmd->set_gamma_table;

	/* Print the gamma tables before sending them to the scanner */

	if (DBG_LEVEL >= 16) {
		int c, i, j;

		for (c = 0; c < 3; c++) {
			for (i = 0; i < 256; i += 16) {
				char gammaValues[16 * 3 + 1], newValue[4];

				gammaValues[0] = '\0';

				for (j = 0; j < 16; j++) {
					sprintf(newValue, " %02x",
						s->gamma_table[c][i + j]);
					strcat(gammaValues, newValue);
				}

				DBG(16, "gamma table[%d][%d] %s\n", c, i,
				    gammaValues);
			}
		}
	}

	for (table = 0; table < 3; table++) {
		gamma[0] = gamma_cmds[table];

		for (n = 0; n < 256; ++n)
			gamma[n + 1] = s->gamma_table[table][n];

		status = e2_cmd_simple(s, params, 2);
		if (status != SANE_STATUS_GOOD)
			return status;

		status = e2_cmd_simple(s, gamma, 257);
		if (status != SANE_STATUS_GOOD)
			return status;
	}

	return status;
}

/* ESC F - Request Status
 * -> ESC f
 * <- Information block
 */

SANE_Status
esci_request_status(SANE_Handle handle, unsigned char *scanner_status)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;
	SANE_Status status;
	unsigned char params[2];

	DBG(8, "%s\n", __func__);

	if (s->hw->cmd->request_status == 0)
		return SANE_STATUS_UNSUPPORTED;

	params[0] = ESC;
	params[1] = s->hw->cmd->request_status;

	e2_send(s, params, 2, 4, &status);
	if (status != SANE_STATUS_GOOD)
		return status;

	status = e2_recv_info_block(s, params, 4, NULL);
	if (status != SANE_STATUS_GOOD)
		return status;

	if (scanner_status)
		*scanner_status = params[0];

	DBG(1, "status: %02x\n", params[0]);

	if (params[0] & STATUS_NOT_READY)
		DBG(1, " scanner in use on another interface\n");
	else
		DBG(1, " ready\n");

	if (params[0] & STATUS_FER)
		DBG(1, " system error\n");

	if (params[0] & STATUS_OPTION)
		DBG(1, " option equipment is installed\n");
	else
		DBG(1, " no option equipment installed\n");

	if (params[0] & STATUS_EXT_COMMANDS)
		DBG(1, " support extended commands\n");
	else
		DBG(1, " does NOT support extended commands\n");

	if (params[0] & STATUS_RESERVED)
		DBG(0,
		    " a reserved bit is set, please contact the author.\n");

	return status;
}

/* extended commands */

/* FS I, Request Extended Identity
 * -> FS I
 * <- Extended identity data (80)
 *
 * Request the properties of the scanner.
 */

SANE_Status
esci_request_extended_identity(SANE_Handle handle, unsigned char *buf)
{
	unsigned char model[17];
	Epson_Scanner *s = (Epson_Scanner *) handle;
	SANE_Status status;
	unsigned char params[2];

	DBG(8, "%s\n", __func__);

	if (buf == NULL)
		return SANE_STATUS_INVAL;

	if (s->hw->cmd->request_extended_identity == 0)
		return SANE_STATUS_UNSUPPORTED;

	params[0] = FS;
	params[1] = s->hw->cmd->request_extended_identity;

	status = e2_txrx(s, params, 2, buf, 80);
	if (status != SANE_STATUS_GOOD)
		return status;

	DBG(1, " command level   : %c%c\n", buf[0], buf[1]);
	DBG(1, " basic resolution: %lu\n", (unsigned long) le32atoh(&buf[4]));
	DBG(1, " min resolution  : %lu\n", (unsigned long) le32atoh(&buf[8]));
	DBG(1, " max resolution  : %lu\n", (unsigned long) le32atoh(&buf[12]));
	DBG(1, " max pixel num   : %lu\n", (unsigned long) le32atoh(&buf[16]));
	DBG(1, " scan area       : %lux%lu\n",
	    (unsigned long) le32atoh(&buf[20]), (unsigned long) le32atoh(&buf[24]));

	DBG(1, " adf area        : %lux%lu\n",
	    (unsigned long) le32atoh(&buf[28]), (unsigned long) le32atoh(&buf[32]));

	DBG(1, " tpu area        : %lux%lu\n",
	    (unsigned long) le32atoh(&buf[36]), (unsigned long) le32atoh(&buf[40]));

	DBG(1, " capabilities (1): 0x%02x\n", buf[44]);
	DBG(1, " capabilities (2): 0x%02x\n", buf[45]);
	DBG(1, " input depth     : %d\n", buf[66]);
	DBG(1, " max output depth: %d\n", buf[67]);
	DBG(1, " rom version     : %c%c%c%c\n",
	    buf[62], buf[63], buf[64], buf[65]);

	memcpy(model, &buf[46], 16);
	model[16] = '\0';
	DBG(1, " model name      : %s\n", model);

	DBG(1, "options:\n");

	if (le32atoh(&buf[28]) > 0)
		DBG(1, " ADF detected\n");

	if (le32atoh(&buf[36]) > 0)
		DBG(1, " TPU detected\n");

	if (buf[44])
		DBG(1, "capabilities (1):\n");

	if (buf[44] & EXT_IDTY_CAP1_DLF)
		DBG(1, " main lamp change is supported\n");

	if (buf[44] & EXT_IDTY_CAP1_NOTFBF)
		DBG(1, " the device is NOT flatbed\n");

	if (buf[44] & EXT_IDTY_CAP1_ADFT)
		DBG(1, " page type ADF is installed\n");

	if (buf[44] & EXT_IDTY_CAP1_ADFS)
		DBG(1, " ADF is duplex capable\n");

	if (buf[44] & EXT_IDTY_CAP1_ADFO)
		DBG(1, " page type ADF loads from the first sheet\n");

	if (buf[44] & EXT_IDTY_CAP1_LID)
		DBG(1, " lid type option is installed\n");

	if (buf[44] & EXT_IDTY_CAP1_TPIR)
		DBG(1, " infrared scanning is supported\n");

	if (buf[44] & EXT_IDTY_CAP1_PB)
		DBG(1, " push button is supported\n");


	if (buf[45])
		DBG(1, "capabilities (2):\n");

	if (buf[45] & EXT_IDTY_CAP2_AFF)
		DBG(1, " ADF has auto form feed\n");

	if (buf[45] & EXT_IDTY_CAP2_DFD)
		DBG(1, " ADF has double feed detection\n");

	if (buf[45] & EXT_IDTY_CAP2_ADFAS)
		DBG(1, " ADF has auto scan\n");

	return SANE_STATUS_GOOD;
}

/* FS F, request scanner status */
SANE_Status
esci_request_scanner_status(SANE_Handle handle, unsigned char *buf)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;
	SANE_Status status;
	unsigned char params[2];

	DBG(8, "%s\n", __func__);

	if (!s->hw->extended_commands)
		return SANE_STATUS_UNSUPPORTED;

	if (buf == NULL)
		return SANE_STATUS_INVAL;

	params[0] = FS;
	params[1] = 'F';

	status = e2_txrx(s, params, 2, buf, 16);
	if (status != SANE_STATUS_GOOD)
		return status;

	DBG(1, "global status   : 0x%02x\n", buf[0]);

	if (buf[0] & FSF_STATUS_MAIN_FER)
		DBG(1, " system error\n");

	if (buf[0] & FSF_STATUS_MAIN_NR)
		DBG(1, " not ready\n");

	if (buf[0] & FSF_STATUS_MAIN_WU)
		DBG(1, " scanner is warming up\n");

	if (buf[0] & FSF_STATUS_MAIN_CWU)
		DBG(1, " warmup can be cancelled\n");


	DBG(1, "adf status      : 0x%02x\n", buf[1]);

	if (buf[1] & FSF_STATUS_ADF_IST)
		DBG(11, " installed\n");
	else
		DBG(11, " not installed\n");

	if (buf[1] & FSF_STATUS_ADF_EN)
		DBG(11, " enabled\n");
	else
		DBG(11, " not enabled\n");

	if (buf[1] & FSF_STATUS_ADF_ERR)
		DBG(1, " error\n");

	if (buf[1] & FSF_STATUS_ADF_PE)
		DBG(1, " paper empty\n");

	if (buf[1] & FSF_STATUS_ADF_PJ)
		DBG(1, " paper jam\n");

	if (buf[1] & FSF_STATUS_ADF_OPN)
		DBG(1, " cover open\n");

	if (buf[1] & FSF_STATUS_ADF_PAG)
		DBG(1, " duplex capable\n");


	DBG(1, "tpu status      : 0x%02x\n", buf[2]);

	if (buf[2] & FSF_STATUS_TPU_IST)
		DBG(11, " installed\n");
	else
		DBG(11, " not installed\n");

	if (buf[2] & FSF_STATUS_TPU_EN)
		DBG(11, " enabled\n");
	else
		DBG(11, " not enabled\n");

	if (buf[2] & FSF_STATUS_TPU_ERR)
		DBG(1, " error\n");

	if (buf[1] & FSF_STATUS_TPU_OPN)
		DBG(1, " cover open\n");


	DBG(1, "device type     : 0x%02x\n", buf[3] & 0xC0);
	DBG(1, "main body status: 0x%02x\n", buf[3] & 0x3F);

	if (buf[3] & FSF_STATUS_MAIN2_PE)
		DBG(1, " paper empty\n");

	if (buf[3] & FSF_STATUS_MAIN2_PJ)
		DBG(1, " paper jam\n");

	if (buf[3] & FSF_STATUS_MAIN2_OPN)
		DBG(1, " cover open\n");

	return SANE_STATUS_GOOD;
}

SANE_Status
esci_set_scanning_parameter(SANE_Handle handle, unsigned char *buf)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;
	SANE_Status status;
	unsigned char params[2];

	DBG(8, "%s\n", __func__);

	if (buf == NULL)
		return SANE_STATUS_INVAL;

	params[0] = FS;
	params[1] = 'W';

	DBG(10, "resolution of main scan     : %lu\n", (unsigned long) le32atoh(&buf[0]));
	DBG(10, "resolution of sub scan      : %lu\n", (unsigned long) le32atoh(&buf[4]));
	DBG(10, "offset length of main scan  : %lu\n", (unsigned long) le32atoh(&buf[8]));
	DBG(10, "offset length of sub scan   : %lu\n", (unsigned long) le32atoh(&buf[12]));
	DBG(10, "scanning length of main scan: %lu\n", (unsigned long) le32atoh(&buf[16]));
	DBG(10, "scanning length of sub scan : %lu\n", (unsigned long) le32atoh(&buf[20]));
	DBG(10, "scanning color              : %d\n", buf[24]);
	DBG(10, "data format                 : %d\n", buf[25]);
	DBG(10, "option control              : %d\n", buf[26]);
	DBG(10, "scanning mode               : %d\n", buf[27]);
	DBG(10, "block line number           : %d\n", buf[28]);
	DBG(10, "gamma correction            : %d\n", buf[29]);
	DBG(10, "brightness                  : %d\n", buf[30]);
	DBG(10, "color correction            : %d\n", buf[31]);
	DBG(10, "halftone processing         : %d\n", buf[32]);
	DBG(10, "threshold                   : %d\n", buf[33]);
	DBG(10, "auto area segmentation      : %d\n", buf[34]);
	DBG(10, "sharpness control           : %d\n", buf[35]);
	DBG(10, "mirroring                   : %d\n", buf[36]);
	DBG(10, "film type                   : %d\n", buf[37]);
	DBG(10, "main lamp lighting mode     : %d\n", buf[38]);

	status = e2_cmd_simple(s, params, 2);
	if (status != SANE_STATUS_GOOD)
		return status;

	status = e2_cmd_simple(s, buf, 64);
	if (status != SANE_STATUS_GOOD) {
		DBG(1, "%s: invalid scanning parameters\n", __func__);
		return status;
	}

	return SANE_STATUS_GOOD;
}

/* FS S */

SANE_Status
esci_get_scanning_parameter(SANE_Handle handle, unsigned char *buf)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;
	SANE_Status status;
	unsigned char params[2];

	DBG(8, "%s\n", __func__);

	if (buf == NULL)
		return SANE_STATUS_INVAL;

	params[0] = FS;
	params[1] = 'S';

	status = e2_txrx(s, params, 2, buf, 64);
	if (status != SANE_STATUS_GOOD)
		return status;

	DBG(10, "resolution of main scan     : %lu\n",
	    (u_long) le32atoh(&buf[0]));
	DBG(10, "resolution of sub scan      : %lu\n",
	    (u_long) le32atoh(&buf[4]));
	DBG(10, "offset length of main scan  : %lu\n",
	    (u_long) le32atoh(&buf[8]));
	DBG(10, "offset length of sub scan   : %lu\n",
	    (u_long) le32atoh(&buf[12]));
	DBG(10, "scanning length of main scan: %lu\n",
	    (u_long) le32atoh(&buf[16]));
	DBG(10, "scanning length of sub scan : %lu\n",
	    (u_long) le32atoh(&buf[20]));
	DBG(10, "scanning color              : %d\n", buf[24]);
	DBG(10, "data format                 : %d\n", buf[25]);
	DBG(10, "option control              : %d\n", buf[26]);
	DBG(10, "scanning mode               : %d\n", buf[27]);
	DBG(10, "block line number           : %d\n", buf[28]);
	DBG(10, "gamma correction            : %d\n", buf[29]);
	DBG(10, "brightness                  : %d\n", buf[30]);
	DBG(10, "color correction            : %d\n", buf[31]);
	DBG(10, "halftone processing         : %d\n", buf[32]);
	DBG(10, "threshold                   : %d\n", buf[33]);
	DBG(10, "auto area segmentation      : %d\n", buf[34]);
	DBG(10, "sharpness control           : %d\n", buf[35]);
	DBG(10, "mirroring                   : %d\n", buf[36]);
	DBG(10, "film type                   : %d\n", buf[37]);
	DBG(10, "main lamp lighting mode     : %d\n", buf[38]);

	return SANE_STATUS_GOOD;
}

/* ESC # */

SANE_Status
esci_enable_infrared(SANE_Handle handle)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;
	SANE_Status status;
	int i;
	unsigned char params[2];
	unsigned char buf[64];

	unsigned char seq[32] = {
		0xCA, 0xFB, 0x77, 0x71, 0x20, 0x16, 0xDA, 0x09,
		0x5F, 0x57, 0x09, 0x12, 0x04, 0x83, 0x76, 0x77,
		0x3C, 0x73, 0x9C, 0xBE, 0x7A, 0xE0, 0x52, 0xE2,
		0x90, 0x0D, 0xFF, 0x9A, 0xEF, 0x4C, 0x2C, 0x81
	};

	DBG(8, "%s\n", __func__);

	status = esci_get_scanning_parameter(handle, buf);
	if (status != SANE_STATUS_GOOD)
		return status;

	for (i = 0; i < 32; i++) {
		buf[i] = seq[i] ^ buf[i];
	}

	params[0] = ESC;
	params[1] = '#';

	status = e2_cmd_simple(s, params, 2);
	if (status != SANE_STATUS_GOOD)
		return status;

	status = e2_cmd_simple(s, buf, 32);
	if (status != SANE_STATUS_GOOD)
		return status;

	return SANE_STATUS_GOOD;
}

SANE_Status
esci_request_command_parameter(SANE_Handle handle, unsigned char *buf)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;
	SANE_Status status;
	unsigned char params[2];

	DBG(8, "%s\n", __func__);

	if (s->hw->cmd->request_condition == 0)
		return SANE_STATUS_UNSUPPORTED;

	params[0] = ESC;
	params[1] = s->hw->cmd->request_condition;

	status = e2_cmd_info_block(s, params, 2, 45, &buf, NULL);
	if (status != SANE_STATUS_GOOD)
		return status;

	DBG(1, "scanning parameters:\n");
	DBG(1, "color                  : %d\n", buf[1]);
	DBG(1, "resolution             : %dx%d\n",
	    buf[4] << 8 | buf[3], buf[6] << 8 | buf[5]);
	DBG(1, "halftone               : %d\n", buf[19]);
	DBG(1, "brightness             : %d\n", buf[21]);
	DBG(1, "color correction       : %d\n", buf[28]);
	DBG(1, "gamma                  : %d\n", buf[23]);
	DBG(1, "sharpness              : %d\n", buf[30]);
	DBG(1, "threshold              : %d\n", buf[38]);
	DBG(1, "data format            : %d\n", buf[17]);
	DBG(1, "mirroring              : %d\n", buf[34]);
	DBG(1, "option unit control    : %d\n", buf[42]);
	DBG(1, "film type              : %d\n", buf[44]);
	DBG(1, "auto area segmentation : %d\n", buf[36]);
	DBG(1, "line counter           : %d\n", buf[40]);
	DBG(1, "scanning mode          : %d\n", buf[32]);
	DBG(1, "zoom                   : %d,%d\n", buf[26], buf[25]);
	DBG(1, "scan area              : %d,%d %d,%d\n",
	    buf[9] << 8 | buf[8], buf[11] << 8 | buf[10],
	    buf[13] << 8 | buf[12], buf[15] << 8 | buf[14]);
	return status;
}

/* ESC q - Request Focus Position 
 * -> ESC q
 * <- Information block
 * <- Focus position status (2)
 *	0 - Error status
 *	1 - Focus position
 */

SANE_Status
esci_request_focus_position(SANE_Handle handle, unsigned char *position)
{
	SANE_Status status;
	unsigned char *buf;
	Epson_Scanner *s = (Epson_Scanner *) handle;

	unsigned char params[2];

	DBG(8, "%s\n", __func__);

	if (s->hw->cmd->request_focus_position == 0)
		return SANE_STATUS_UNSUPPORTED;

	params[0] = ESC;
	params[1] = s->hw->cmd->request_focus_position;

	status = e2_cmd_info_block(s, params, 2, 2, &buf, NULL);
	if (status != SANE_STATUS_GOOD)
		return status;

	if (buf[0] & 0x01)
		DBG(1, "autofocus error\n");

	*position = buf[1];
	DBG(8, " focus position = 0x%x\n", buf[1]);

	free(buf);

	return status;
}

/* ESC ! - Request Push Button Status
 * -> ESC !
 * <- Information block
 * <- Push button status (1)
 */

SANE_Status
esci_request_push_button_status(SANE_Handle handle, unsigned char *bstatus)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;
	SANE_Status status;
	unsigned char params[2];
	unsigned char *buf;

	DBG(8, "%s\n", __func__);

	if (s->hw->cmd->request_push_button_status == 0) {
		DBG(1, "push button status unsupported\n");
		return SANE_STATUS_UNSUPPORTED;
	}

	params[0] = ESC;
	params[1] = s->hw->cmd->request_push_button_status;

	status = e2_cmd_info_block(s, params, 2, 1, &buf, NULL);
	if (status != SANE_STATUS_GOOD)
		return status;

	DBG(1, "push button status = %d\n", buf[0]);
	*bstatus = buf[0];

	free(buf);

	return status;
}


/*
 * Request Identity information from scanner and fill in information
 * into dev and/or scanner structures.
 * XXX information should be parsed separately.
 */
SANE_Status
esci_request_identity(SANE_Handle handle, unsigned char **buf, size_t *len)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;
	unsigned char params[2];

	DBG(8, "%s\n", __func__);

	if (!s->hw->cmd->request_identity)
		return SANE_STATUS_INVAL;

	params[0] = ESC;
	params[1] = s->hw->cmd->request_identity;

	return e2_cmd_info_block(s, params, 2, 0, buf, len);
}


/*
 * Request information from scanner
 */
SANE_Status
esci_request_identity2(SANE_Handle handle, unsigned char **buf)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;
	SANE_Status status;
	size_t len;
	unsigned char params[2];

	DBG(8, "%s\n", __func__);

	if (s->hw->cmd->request_identity2 == 0)
		return SANE_STATUS_UNSUPPORTED;

	params[0] = ESC;
	params[1] = s->hw->cmd->request_identity2;

	status = e2_cmd_info_block(s, params, 2, 0, buf, &len);
	if (status != SANE_STATUS_GOOD)
		return status;

	return status;
}

/* Send the "initialize scanner" command to the device and reset it */

SANE_Status
esci_reset(Epson_Scanner * s)
{
	SANE_Status status;
	unsigned char params[2];

	DBG(8, "%s\n", __func__);

	if (!s->hw->cmd->initialize_scanner)
		return SANE_STATUS_GOOD;

	params[0] = ESC;
	params[1] = s->hw->cmd->initialize_scanner;

	if (s->fd == -1)
		return SANE_STATUS_GOOD;

	status = e2_cmd_simple(s, params, 2);

	return status;
}

SANE_Status
esci_feed(Epson_Scanner * s)
{
	unsigned char params[1];

	DBG(8, "%s\n", __func__);

	if (!s->hw->cmd->feed)
		return SANE_STATUS_UNSUPPORTED;

	params[0] = s->hw->cmd->feed;

	return e2_cmd_simple(s, params, 1);
}


/*
 * Eject the current page from the ADF. The scanner is opened prior to
 * sending the command and closed afterwards.
 */

SANE_Status
esci_eject(Epson_Scanner * s)
{
	unsigned char params[1];

	DBG(8, "%s\n", __func__);

	if (!s->hw->cmd->eject)
		return SANE_STATUS_UNSUPPORTED;

	if (s->fd == -1)
		return SANE_STATUS_GOOD;

	params[0] = s->hw->cmd->eject;

	return e2_cmd_simple(s, params, 1);
}

SANE_Status
esci_request_extended_status(SANE_Handle handle, unsigned char **data,
			size_t * data_len)
{
	Epson_Scanner *s = (Epson_Scanner *) handle;
	SANE_Status status = SANE_STATUS_GOOD;
	unsigned char params[2];
	unsigned char *buf;
	size_t buf_len;

	DBG(8, "%s\n", __func__);

	if (s->hw->cmd->request_extended_status == 0)
		return SANE_STATUS_UNSUPPORTED;

	params[0] = ESC;
	params[1] = s->hw->cmd->request_extended_status;

	/* This command returns 33 bytes of data on old scanners
	 * and 42 (CMD_SIZE_EXT_STATUS) on new ones.
	 */
	status = e2_cmd_info_block(s, params, 2, CMD_SIZE_EXT_STATUS,
				       &buf, &buf_len);
	if (status != SANE_STATUS_GOOD)
		return status;

	switch (buf_len) {
	case 33:
	case 42:
		break;
	default:
		DBG(1, "%s: unknown reply length (%lu)\n", __func__,
			(unsigned long) buf_len);
		break;
	}

	DBG(4, "main = %02x, ADF = %02x, TPU = %02x, main 2 = %02x\n",
		buf[0], buf[1], buf[6], buf[11]);

	if (buf[0] & EXT_STATUS_FER)
		DBG(1, "system error\n");

	if (buf[0] & EXT_STATUS_WU)
		DBG(1, "scanner is warming up\n");

	if (buf[1] & EXT_STATUS_ERR)
		DBG(1, "ADF: other error\n");

	if (buf[1] & EXT_STATUS_PE)
		DBG(1, "ADF: no paper\n");

	if (buf[1] & EXT_STATUS_PJ)
		DBG(1, "ADF: paper jam\n");

	if (buf[1] & EXT_STATUS_OPN)
		DBG(1, "ADF: cover open\n");

	if (buf[6] & EXT_STATUS_ERR)
		DBG(1, "TPU: other error\n");

	/* give back a pointer to the payload
	 * if the user requested it, otherwise
	 * free it.
	 */

	if (data)
		*data = buf;
	else
		free(buf);

	if (data_len)
		*data_len = buf_len;

	return status;
}
