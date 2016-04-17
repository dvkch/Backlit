/*
 * Prototypes for Epson ESC/I commands
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

/* simple scanner commands, ESC <x> */

#define esci_set_focus_position(s,v)		e2_esc_cmd( s,(s)->hw->cmd->set_focus_position, v)
#define esci_set_color_mode(s,v)		e2_esc_cmd( s,(s)->hw->cmd->set_color_mode, v)
#define esci_set_data_format(s,v)		e2_esc_cmd( s,(s)->hw->cmd->set_data_format, v)
#define esci_set_halftoning(s,v)		e2_esc_cmd( s,(s)->hw->cmd->set_halftoning, v)
#define esci_set_gamma(s,v)			e2_esc_cmd( s,(s)->hw->cmd->set_gamma, v)
#define esci_set_color_correction(s,v)		e2_esc_cmd( s,(s)->hw->cmd->set_color_correction, v)
#define esci_set_lcount(s,v)			e2_esc_cmd( s,(s)->hw->cmd->set_lcount, v)
#define esci_set_bright(s,v)			e2_esc_cmd( s,(s)->hw->cmd->set_bright, v)
#define esci_mirror_image(s,v)			e2_esc_cmd( s,(s)->hw->cmd->mirror_image, v)
#define esci_set_speed(s,v)			e2_esc_cmd( s,(s)->hw->cmd->set_speed, v)
#define esci_set_sharpness(s,v)			e2_esc_cmd( s,(s)->hw->cmd->set_outline_emphasis, v)
#define esci_set_auto_area_segmentation(s,v)	e2_esc_cmd( s,(s)->hw->cmd->control_auto_area_segmentation, v)
#define esci_set_film_type(s,v)			e2_esc_cmd( s,(s)->hw->cmd->set_film_type, v)
#define esci_set_exposure_time(s,v)		e2_esc_cmd( s,(s)->hw->cmd->set_exposure_time, v)
#define esci_set_bay(s,v)			e2_esc_cmd( s,(s)->hw->cmd->set_bay, v)
#define esci_set_threshold(s,v)			e2_esc_cmd( s,(s)->hw->cmd->set_threshold, v)
#define esci_control_extension(s,v)		e2_esc_cmd( s,(s)->hw->cmd->control_an_extension, v)

SANE_Status esci_set_zoom(Epson_Scanner * s, unsigned char x, unsigned char y);
SANE_Status esci_set_resolution(Epson_Scanner * s, int x, int y);
SANE_Status esci_set_scan_area(Epson_Scanner * s, int x, int y, int width,
			  int height);
SANE_Status esci_set_color_correction_coefficients(Epson_Scanner * s, SANE_Word *table);
SANE_Status esci_set_gamma_table(Epson_Scanner * s);

SANE_Status esci_request_status(SANE_Handle handle, unsigned char *scanner_status);
SANE_Status esci_request_extended_identity(SANE_Handle handle, unsigned char *buf);
SANE_Status esci_request_scanner_status(SANE_Handle handle, unsigned char *buf);
SANE_Status esci_set_scanning_parameter(SANE_Handle handle, unsigned char *buf);
SANE_Status esci_get_scanning_parameter(SANE_Handle handle, unsigned char *buf);
SANE_Status esci_request_command_parameter(SANE_Handle handle, unsigned char *buf);
SANE_Status esci_request_focus_position(SANE_Handle handle,
				   unsigned char *position);
SANE_Status esci_request_push_button_status(SANE_Handle handle,
				       unsigned char *bstatus);
SANE_Status esci_request_identity(SANE_Handle handle, unsigned char **buf, size_t *len);

SANE_Status esci_request_identity2(SANE_Handle handle, unsigned char **buf);
SANE_Status esci_reset(Epson_Scanner * s);
SANE_Status esci_feed(Epson_Scanner * s);
SANE_Status esci_eject(Epson_Scanner * s);
SANE_Status esci_request_extended_status(SANE_Handle handle, unsigned char **data,
				    size_t * data_len);
SANE_Status esci_enable_infrared(SANE_Handle handle);
