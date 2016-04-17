/*
 * epson2.c - SANE library for Epson scanners.
 *
 * Based on Kazuhiro Sasayama previous
 * Work on epson.[ch] file from the SANE package.
 * Please see those files for additional copyrights.
 *
 * Copyright (C) 2006-07 Tower Technologies
 * Author: Alessandro Zummo <a.zummo@towertech.it>
 *
 * This file is part of the SANE package.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2.
 */

/* some defines to make handling the TPU easier */
#define FILM_TYPE_NEGATIVE      (1L << 0)
#define FILM_TYPE_SLIDE         (1L << 1)

#define e2_model(s,m) e2_dev_model((s)->hw,(m))

extern int sanei_scsi_max_request_size;
extern int *gamma_params;

extern void e2_dev_init(Epson_Device *dev, const char *devname, int conntype);
extern SANE_Status e2_dev_post_init(struct Epson_Device *dev);
extern SANE_Bool e2_dev_model(Epson_Device *dev, const char *model);
extern void e2_set_cmd_level(SANE_Handle handle, unsigned char *level);
extern SANE_Status e2_set_model(Epson_Scanner *s, unsigned char *model, size_t len);
extern SANE_Status e2_add_resolution(Epson_Device *dev, int r);
extern void e2_set_fbf_area(Epson_Scanner *s, int x, int y, int unit);
extern void e2_set_adf_area(struct Epson_Scanner *s, int x, int y, int unit);
extern void e2_set_tpu_area(struct Epson_Scanner *s, int x, int y, int unit);
extern void e2_set_tpu2_area(struct Epson_Scanner *s, int x, int y, int unit);
extern void e2_add_depth(Epson_Device *dev, SANE_Word depth);
extern SANE_Status e2_discover_capabilities(Epson_Scanner *s);
extern SANE_Status e2_set_extended_scanning_parameters(Epson_Scanner *s);
extern SANE_Status e2_set_scanning_parameters(Epson_Scanner *s);
extern void e2_setup_block_mode(Epson_Scanner *s);
extern SANE_Status e2_init_parameters(Epson_Scanner *s);
extern void e2_wait_button(Epson_Scanner *s);
extern SANE_Status e2_check_warm_up(Epson_Scanner *s, SANE_Bool *wup);
extern SANE_Status e2_wait_warm_up(Epson_Scanner *s);
extern SANE_Status e2_check_adf(Epson_Scanner *s);
extern SANE_Status e2_start_std_scan(Epson_Scanner *s);
extern SANE_Status e2_start_ext_scan(Epson_Scanner *s);
extern void e2_scan_finish(Epson_Scanner *s);
extern void e2_copy_image_data(Epson_Scanner *s, SANE_Byte *data, SANE_Int max_length,
		   SANE_Int *length);
extern SANE_Status e2_ext_read(struct Epson_Scanner *s);
extern SANE_Status e2_block_read(struct Epson_Scanner *s);
