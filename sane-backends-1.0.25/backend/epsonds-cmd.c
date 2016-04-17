/*
 * epsonds-cmd.c - Epson ESC/I-2 routines.
 *
 * Copyright (C) 2015 Tower Technologies
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
#include <ctype.h>
#include <unistd.h>	     /* sleep */

#include "epsonds.h"
#include "epsonds-io.h"
#include "epsonds-cmd.h"
#include "epsonds-ops.h"

static SANE_Status
esci2_parse_block(char *buf, int len, void *userdata, SANE_Status (*cb)(void *userdata, char *token, int len))
{
	SANE_Status status = SANE_STATUS_GOOD;
	SANE_Status delayed_status = SANE_STATUS_GOOD;


	char *start = buf;
	char *end = (buf + len) - 1;

	/* 0  : #
	 * 1-3: param
	 * 4- : data
	*/

	while (1) {

		char param[4];

		while (*start != '#' && start < end)
			start++;

		if (*start != '#')
			break;

		param[0] = *++start;
		param[1] = *++start;
		param[2] = *++start;
		param[3] = '\0';

		if (strncmp("---", param, 3) == 0)
			break;

		/* ugly hack to skip over GMT in RESA */
		if (strncmp("GMT", param, 3) == 0 && *(start + 5) == 'h') {
			start = start + 4 + 0x100;
			continue;
		}

		/* find the end of the token */
		{
			int tlen;
			char *next = start;

			while (*next != '#' && *next != 0x00 && next < end)
				next++;

			tlen = next - start - 1;

			if (cb) {
				status = cb(userdata, start - 2, tlen);
				if (status != SANE_STATUS_GOOD) {
					delayed_status = status;
				}
			}

			start = next;
		}
	}

	if (delayed_status != SANE_STATUS_GOOD)
		return delayed_status;

	return status;
}

static SANE_Bool
esci2_check_header(const char *cmd, const char *buf, unsigned int *more)
{
	int err;

	*more = 0;

	if (strncmp(cmd, buf, 4) != 0) {

		if (strncmp("UNKN", buf, 4) == 0) {
			DBG(1, "UNKN reply code received\n");
		} else if (strncmp("INVD", buf, 4) == 0) {
			DBG(1, "INVD reply code received\n");
		} else {
			DBG(1, "%c%c%c%c, unexpected reply code\n", buf[0], buf[1], buf[2], buf[3]);
		}

		return 0;
	}

	/* INFOx0000100#.... */

	/* read the answer len */
	if (buf[4] != 'x') {
		DBG(1, "unknown type in header: %c\n", buf[4]);
		return 0;
	}

	err = sscanf(&buf[5], "%x#", more);
	if (err != 1) {
		DBG(1, "cannot decode length from header\n");
		return 0;
	}

	return 1;
}

static SANE_Status esci2_cmd(epsonds_scanner* s,
	char *cmd, size_t len,
	char *payload, size_t plen,
	void *userdata, SANE_Status (*cb)(void *userdata, char *token, int len))
{
	SANE_Status status;
	unsigned int more;
	char rbuf[64];

	DBG(8, "%s: %4s len %lu, payload len: %lu\n", __func__, cmd, len, plen);

	if (len < 12) {
		DBG(1, "%s: command is too short (%lu)\n", __func__, len);
		return SANE_STATUS_INVAL;
	}

	/* merge the payload size and send the RequestBlock */
	if (payload && plen) {

		sprintf(rbuf, "%4.4sx%07x", cmd, (unsigned int)plen);

		DBG(8, " %s (%lu)\n", rbuf, plen);

		eds_send(s, rbuf, 12, &status);

	} else {
		eds_send(s, cmd, len, &status);
	}

	if (status != SANE_STATUS_GOOD) {
		return status;
	}

	/* send ParameterBlock */
	if (payload && plen) {

		eds_send(s, payload, plen, &status);
		if (status != SANE_STATUS_GOOD) {
			return status;
		}
	}

	/* receive DataHeaderBlock */
	memset(rbuf, 0x00, sizeof(rbuf));

	eds_recv(s, rbuf, 64, &status);
	if (status != SANE_STATUS_GOOD) {
		return status;
	}

	/* rxbuf holds the DataHeaderBlock, which should be
	 * parsed to know if we need to read more data
	 */
	if (!esci2_check_header(cmd, rbuf, &more)) {
		return SANE_STATUS_IO_ERROR;
	}

	/* parse the received header block */
	if (cb) {
		status = esci2_parse_block(rbuf + 12, 64 - 12, userdata, cb);
		if (status != SANE_STATUS_GOOD && status != SANE_STATUS_DEVICE_BUSY) {
			DBG(1, "%s: %4s error while parsing received header\n", __func__, cmd);
		}
	}

	/* header valid, get the data block if present */
	if (more) {

		char *pbuf = malloc(more);
		if (pbuf) {

			ssize_t read = eds_recv(s, pbuf, more, &status);
			if (read != more) {
			}

			/* parse the received data block */
			if (cb) {
				status = esci2_parse_block(pbuf, more, userdata, cb);
				if (status != SANE_STATUS_GOOD) {
					DBG(1, "%s: %4s error while parsing received data block\n", __func__, cmd);
				}
			}

			free(pbuf);

		} else {
			return SANE_STATUS_NO_MEM;
		}
	}

	return status;
}

static SANE_Status esci2_cmd_simple(epsonds_scanner* s, char *cmd, SANE_Status (*cb)(void *userdata, char *token, int len))
{
	return esci2_cmd(s, cmd, 12, NULL, 0, s, cb);
}

SANE_Status esci2_fin(epsonds_scanner *s)
{
	SANE_Status status;

	DBG(5, "%s\n", __func__);

	status = esci2_cmd_simple(s, "FIN x0000000", NULL);
	s->locked = 0;
	return status;
}

SANE_Status esci2_can(epsonds_scanner *s)
{
	return esci2_cmd_simple(s, "CAN x0000000", NULL);
}

static int decode_value(char *buf, int len)
{
	char tmp[10];

	memcpy(tmp, buf, len);
	tmp[len] = '\0';

	if (buf[0] == 'd' && len == 4) {
		return strtol(buf + 1, NULL, 10);
	} else if (buf[0] == 'i' && len == 8) {
		return strtol(buf + 1, NULL, 10);
	} else if (buf[0] == 'x' && len == 8) {
		return strtol(buf + 1, NULL, 16);
	} else if (buf[0] == 'h' && len == 4) {
		return strtol(buf + 1, NULL, 16);
	}

	return -1;
}

/* h000 */
static char *decode_binary(char *buf)
{
	char tmp[6];
	int hl;

	memcpy(tmp, buf, 4);
	tmp[4] = '\0';

	if (buf[0] != 'h')
		return NULL;

	hl = strtol(tmp + 1, NULL, 16);
	if (hl) {

		char *v = malloc(hl + 1);
		memcpy(v, buf + 4, hl);
		v[hl] = '\0';

		return v;
	}

	return NULL;
}

static char *decode_string(char *buf)
{
	char *p, *s = decode_binary(buf);
	if (s == NULL)
		return NULL;

	/* trim white space at the end */
	p = s + strlen(s);
	while (*--p == ' ')
		*p = '\0';

	return s;
}

static void debug_token(int level, const char *func, char *token, int len)
{
	char *tdata = malloc(len + 1);
	memcpy(tdata, token + 3, len);
	tdata[len] = '\0';

	DBG(level, "%s: %3.3s / %s / %d\n", func, token, tdata, len);

	free(tdata);
}

static SANE_Status info_cb(void *userdata, char *token, int len)
{
	epsonds_scanner *s = (epsonds_scanner *)userdata;
	char *value;

	if (DBG_LEVEL >= 11) {
		debug_token(DBG_LEVEL, __func__, token, len);
	}

	/* pointer to the token's value */
	value = token + 3;

	/* nrd / nrdBUSY */

	if (strncmp("nrd", token, 3) == 0) {
		if (strncmp("BUSY", value, 4) == 0) {
			return SANE_STATUS_DEVICE_BUSY;
		}
	}

	if (strncmp("PRD", token, 3) == 0) {
		free(s->hw->model);
		s->hw->model = decode_string(value);
		s->hw->sane.model = s->hw->model;
		DBG(1, " product: %s\n", s->hw->model);
		/* we will free the string later */
	}

	if (strncmp("VER", token, 3) == 0) {
		char *v = decode_string(value);
		DBG(1, " version: %s\n", v);
		free(v);
	}

	if (strncmp("S/N", token, 3) == 0) {
		char *v = decode_string(value);
		DBG(1, "  serial: %s\n", v);
		free(v);
	}

	if (strncmp("ADF", token, 3) == 0) {

		s->hw->has_adf = 1;

		if (len == 8) {

			if (strncmp("TYPEPAGE", value, len) == 0) {
				DBG(1, "     ADF: page type\n");
			}

			if (strncmp("TYPEFEED", value, len) == 0) {
				DBG(1, "     ADF: sheet feed type\n");
			}

			if (strncmp("DPLX1SCN", value, len) == 0) {
				DBG(1, "     ADF: duplex single pass\n");
				s->hw->adf_singlepass = 1;
			}

			if (strncmp("DPLX2SCN", value, len) == 0) {
				DBG(1, "     ADF: duplex double pass\n");
				s->hw->adf_singlepass = 0;
			}

			if (strncmp("FORDPF1N", value, len) == 0) {
				DBG(1, "     ADF: order is 1 to N\n");
			}

			if (strncmp("FORDPFN1", value, len) == 0) {
				DBG(1, "     ADF: order is N to 1\n");
			}

			if (strncmp("ALGNLEFT", value, len) == 0) {
				DBG(1, "     ADF: left aligned\n");
				s->hw->adf_alignment = 0;
			}

			if (strncmp("ALGNCNTR", value, len) == 0) {
				DBG(1, "     ADF: center aligned\n");
				s->hw->adf_alignment = 1;
			}

			if (strncmp("ALGNRIGT", value, len) == 0) {
				DBG(1, "     ADF: right aligned (not supported!)\n");
				s->hw->adf_alignment = 2;
			}
		}

		if (len == 4) {

			if (strncmp("PREF", value, len) == 0) {
				DBG(1, "     ADF: auto pre-feed\n");
			}

			if (strncmp("ASCN", value, len) == 0) {
				DBG(1, "     ADF: auto scan\n");
			}

			if (strncmp("RCVR", value, len) == 0) {
				DBG(1, "     ADF: auto recovery\n");
			}
		}

		if (len == 20) {

			/* ADFAREAi0000850i0001400 */

			if (strncmp("AREA", value, 4) == 0) {

				int min = decode_value(value + 4, 8);
				int max = decode_value(value + 4 + 8, 8);

				DBG(1, "     ADF: area %dx%d @ 100dpi\n", min, max);
			}

			if (strncmp("AMIN", value, 4) == 0) {

				int min = decode_value(value + 4, 8);
				int max = decode_value(value + 4 + 8, 8);

				DBG(1, "     ADF: min %dx%d @ 100dpi\n", min, max);
			}

			if (strncmp("AMAX", value, 4) == 0) {

				int min = decode_value(value + 4, 8);
				int max = decode_value(value + 4 + 8, 8);

				DBG(1, "     ADF: max %dx%d @ 100dpi\n", min, max);

				eds_set_adf_area(s->hw,	min, max, 100);
			}
		}

		if (len == 12) {

			/* RESOi0000600 */

			if (strncmp("RESO", value, 4) == 0) {

				int res = decode_value(value + 4, 8);

				DBG(1, "     ADF: basic resolution is %d dpi\n", res);
			}

			/* OVSNd025d035 */

			if (strncmp("OVSN", value, 4) == 0) {

				int x = decode_value(value + 4, 4);
				int y = decode_value(value + 4 + 4, 4);

				DBG(1, "     ADF: overscan %dx%d @ 100dpi\n", x, y);
			}
		}
	}

	if (strncmp("FB ", token, 3) == 0) {

		s->hw->has_fb = 1;

		if (len == 20) {

			/* AREAi0000850i0001400 */
			if (strncmp("AREA", value, 4) == 0) {

				int min = decode_value(value + 4, 8);
				int max = decode_value(value + 4 + 8, 8);

				DBG(1, "      FB: area %dx%d @ 100dpi\n", min, max);

				eds_set_fbf_area(s->hw,	min, max, 100);
			}
		}

		if (len == 8) {

			if (strncmp("ALGNLEFT", value, len) == 0) {
				DBG(1, "      FB: left aligned\n");
				s->hw->fbf_alignment = 0;
			}

			if (strncmp("ALGNCNTR", value, len) == 0) {
				DBG(1, "      FB: center aligned\n");
				s->hw->fbf_alignment = 1;
			}

			if (strncmp("ALGNRIGT", value, len) == 0) {
				DBG(1, "      FB: right aligned (not supported!)\n");
				s->hw->fbf_alignment = 2;
			}
		}

		if (len == 12) {

			/* RESOi0000600 */

			if (strncmp("RESO", value, 4) == 0) {

				int res = decode_value(value + 4, 8);

				DBG(1, "      FB: basic resolution is %d dpi\n", res);
			}

			/* OVSNd025d035 */

			if (strncmp("OVSN", value, 4) == 0) {

				int x = decode_value(value + 4, 4);
				int y = decode_value(value + 4 + 4, 4);

				DBG(1, "      FB: overscan %dx%d @ 100dpi\n", x, y);
			}
		}

		if (len == 4) {

			if (strncmp("DETX", value, len) == 0) {
				DBG(1, "      FB: paper width detection\n");
			}

			if (strncmp("DETY", value, len) == 0) {
				DBG(1, "      FB: paper height detection\n");
			}
		}
	}

	return SANE_STATUS_GOOD;
}

SANE_Status esci2_info(epsonds_scanner *s)
{
	SANE_Status status;
	int i = 4;

	DBG(1, "= gathering device information\n");

	do {
		status = esci2_cmd_simple(s, "INFOx0000000", &info_cb);
		if (status == SANE_STATUS_DEVICE_BUSY) {
			sleep(2);
		}

		i--;

	} while (status == SANE_STATUS_DEVICE_BUSY && i);

	return status;
}

/* CAPA */

static SANE_Status capa_cb(void *userdata, char *token, int len)
{
	epsonds_scanner *s = (epsonds_scanner *)userdata;

	char *value = token + 3;

	if (DBG_LEVEL >= 11) {
		debug_token(DBG_LEVEL, __func__, token, len);
	}

	if (len == 4) {

		if (strncmp("ADFDPLX", token, 3 + 4) == 0) {
			DBG(1, "     ADF: duplex\n");
			s->hw->adf_is_duplex = 1;
		}

		if (strncmp("ADFSKEW", token, 3 + 4) == 0) {
			DBG(1, "     ADF: skew correction\n");
			s->hw->adf_has_skew = 1;
		}

		if (strncmp("ADFOVSN", token, 3 + 4) == 0) {
			DBG(1, "     ADF: overscan\n");
		}

		if (strncmp("ADFPEDT", token, 3 + 4) == 0) {
			DBG(1, "     ADF: paper end detection\n");
		}

		if (strncmp("ADFLOAD", token, 3 + 4) == 0) {
			DBG(1, "     ADF: paper load\n");
			s->hw->adf_has_load = 1;
		}

		if (strncmp("ADFEJCT", token, 3 + 4) == 0) {
			DBG(1, "     ADF: paper eject\n");
			s->hw->adf_has_eject = 1;
		}

		if (strncmp("ADFCRP ", token, 3 + 4) == 0) {
			DBG(1, "     ADF: image cropping\n");
		}

		if (strncmp("ADFFAST", token, 3 + 4) == 0) {
			DBG(1, "     ADF: fast mode available\n");
		}

		if (strncmp("ADFDFL1", token, 3 + 4) == 0) {
			DBG(1, "     ADF: double feed detection\n");
			s->hw->adf_has_dfd = 1;
		}
	}

	if (len == 8 && strncmp("ADFDFL1DFL2", token, 3 + 4) == 0) {
		DBG(1, "     ADF: double feed detection (high sensitivity)\n");
		s->hw->adf_has_dfd = 2;
	}

	if (strncmp("FMT", token, 3) == 0) {

		/* a bit ugly... */

		if (len >= 8) {
			if (strncmp("RAW ", value + 4, 4) == 0) {
				s->hw->has_raw = 1;
			}
		}

		if (len >= 12) {
			if (strncmp("RAW ", value + 8, 4) == 0) {
				s->hw->has_raw = 1;
			}
		}
	}

	/* RSMRANGi0000050i0000600 */

	if (strncmp("RSMRANG", token, 3 + 4) == 0) {

		char *p = token + 3 + 4;

		if (p[0] == 'i') {

			int min = decode_value(p, 8);
			int max = decode_value(p + 8, 8);

			eds_set_resolution_range(s->hw, min, max);

			DBG(1, "resolution min/max %d/%d\n", min, max);
		}
	}

	/* RSMLISTi0000300i0000600 */

	if (strncmp("RSMLIST", token, 3 + 4) == 0) {

		char *p = token + 3 + 4;

		if (p[0] == 'i') {

			int i;
			int count = (len - 4) / 8;

			for (i = 0; i < count; i++) {

				eds_add_resolution(s->hw, decode_value(p, 8));
				p += 8;
			}
		}
	}

	return SANE_STATUS_GOOD;
}

SANE_Status esci2_capa(epsonds_scanner *s)
{
	return esci2_cmd_simple(s, "CAPAx0000000", &capa_cb);
}

/* STAT */

static SANE_Status stat_cb(void *userdata, char *token, int len)
{
/*
	epsonds_scanner *s = (epsonds_scanner *)userdata;
	char *value = token + 3;
*/
	userdata = userdata;

	if (DBG_LEVEL >= 11) {
		debug_token(DBG_LEVEL, __func__, token, len);
	}

	return SANE_STATUS_GOOD;
}

SANE_Status esci2_stat(epsonds_scanner *s)
{
	return esci2_cmd_simple(s, "STATx0000000", &stat_cb);
}

/* RESA */

static SANE_Status resa_cb(void *userdata, char *token, int len)
{
	/* epsonds_scanner *s = (epsonds_scanner *)userdata; */

	userdata = userdata;

	if (DBG_LEVEL >= 11) {
		debug_token(DBG_LEVEL, __func__, token, len);
	}

	return SANE_STATUS_GOOD;
}

SANE_Status esci2_resa(epsonds_scanner *s)
{
	return esci2_cmd_simple(s, "RESAx0000000", &resa_cb);
}

/* PARA */

static SANE_Status para_cb(void *userdata, char *token, int len)
{
	if (DBG_LEVEL >= 11) {
		debug_token(DBG_LEVEL, __func__, token, len);
	}

	userdata = userdata;

	if (strncmp("par", token, 3) == 0) {
		if (strncmp("FAIL", token + 3, 4) == 0) {
			DBG(1, "%s: parameter setting failed\n", __func__);
			return SANE_STATUS_INVAL;
		}
	}

	return SANE_STATUS_GOOD;
}

SANE_Status esci2_para(epsonds_scanner *s, char *parameters)
{
	DBG(8, "%s: %s\n", __func__, parameters);
	return esci2_cmd(s, "PARAx0000000", 12, parameters, strlen(parameters), NULL, &para_cb);
}

SANE_Status esci2_mech(epsonds_scanner *s, char *parameters)
{
	DBG(8, "%s: %s\n", __func__, parameters);
	return esci2_cmd(s, "MECHx0000000", 12, parameters, strlen(parameters), NULL, &para_cb);
}

SANE_Status esci2_trdt(epsonds_scanner *s)
{
	return esci2_cmd_simple(s, "TRDTx0000000", NULL);
}


static SANE_Status img_cb(void *userdata, char *token, int len)
{
	struct epsonds_scanner *s = userdata;

	if (DBG_LEVEL >= 11) {
		debug_token(DBG_LEVEL, __func__, token, len);
	}

	/* psti0000256i0000000i0000945 / 24 */

	/* integer comparison first so it's faster */
	if (len == 24 && strncmp("pst", token, 3) == 0) {

		s->dummy = decode_value(token + 3 + 8, 8);

		DBG(10, "%s: pst width: %d, height: %d, dummy: %d\n",
			__func__,
			decode_value(token + 3, 8),
			decode_value(token + 3 + 8 + 8, 8),
			s->dummy);

		return SANE_STATUS_GOOD;
	}

	if (len == 16 && strncmp("pen", token, 3) == 0) {
		DBG(10, "%s: page end\n", __func__);
		s->eof = 1;
		return SANE_STATUS_EOF;
	}

	/* typIMGA or typIMGB */
	if (len == 4 && strncmp("typ", token, 3) == 0) {

		if (token[6] == 'B')
			s->backside = 1;
		else
			s->backside = 0;

		return SANE_STATUS_GOOD;
	}

	if (strncmp("err", token, 3) == 0) {

		char *option = token + 3;	/* ADF, TPU, FB */
		char *cause = token + 3 + 4;	/* OPN, PJ, PE, ERR, LTF, LOCK, DFED, DTCL, AUT, PERM */

		s->scanning = 0;

		DBG(1, "%s: error on option %3.3s, cause %4.4s\n",
			__func__, option, cause);

		if (cause[0] == 'P' && cause[1] == 'J')
			return SANE_STATUS_JAMMED;

		if (cause[0] == 'P' && cause[1] == 'E')
			return SANE_STATUS_NO_DOCS;

		if (cause[0] == 'O' && cause[1] == 'P' && cause[2] == 'N')
			return SANE_STATUS_COVER_OPEN;

		return SANE_STATUS_IO_ERROR;
	}

	if (len == 4 && strncmp("atnCAN ", token, 3 + 4) == 0) {
		DBG(1, "%s: cancel request\n", __func__);
		s->canceling = 1;
		s->scanning = 0;
		return SANE_STATUS_CANCELLED;
	}

	if (len == 4 && strncmp("lftd000", token, 3 + 4) == 0) {
		s->scanning = 0;
	}

	return SANE_STATUS_GOOD;
}


SANE_Status
esci2_img(struct epsonds_scanner *s, SANE_Int *length)
{
	SANE_Status status = SANE_STATUS_GOOD;
	SANE_Status parse_status;
	unsigned int more;
	ssize_t read;

	*length = 0;

	if (s->canceling)
		return SANE_STATUS_CANCELLED;

	/* request image data */
	eds_send(s, "IMG x0000000", 12, &status);
	if (status != SANE_STATUS_GOOD) {
		return status;
	}

	/* receive DataHeaderBlock */
	memset(s->buf, 0x00, 64);
	eds_recv(s, s->buf, 64, &status);
	if (status != SANE_STATUS_GOOD) {
		return status;
	}

	/* check if we need to read any image data */
	more = 0;
	if (!esci2_check_header("IMG ", (char *)s->buf, &more)) {
		return SANE_STATUS_IO_ERROR;
	}

	/* this handles eof and errors */
	parse_status = esci2_parse_block((char *)s->buf + 12, 64 - 12, s, &img_cb);

	/* no more data? return using the status of the esci2_parse_block
	 * call, which might hold other error conditions.
	 */
	if (!more) {
		return parse_status;
	}

	/* ALWAYS read image data */
	read = eds_recv(s, s->buf, more, &status);
	if (status != SANE_STATUS_GOOD) {
		return status;
	}

	if (read != more) {
		return SANE_STATUS_IO_ERROR;
	}

	/* handle esci2_parse_block errors */
	if (parse_status != SANE_STATUS_GOOD) {
		return parse_status;
	}

	DBG(15, "%s: read %lu bytes, status: %d\n", __func__, (unsigned long) read, status);

	*length = read;

	if (s->canceling) {
		return SANE_STATUS_CANCELLED;
	}

	return SANE_STATUS_GOOD;
}
