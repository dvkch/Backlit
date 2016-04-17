/*
 * epsonds-cmd.h - Epson ESC/I-2 routines.
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

#ifndef epsonds_cmd_h
#define epsonds_cmd_h

SANE_Status esci2_info(epsonds_scanner *s);
SANE_Status esci2_fin(epsonds_scanner *s);
SANE_Status esci2_can(epsonds_scanner *s);
SANE_Status esci2_capa(epsonds_scanner *s);
SANE_Status esci2_resa(epsonds_scanner *s);
SANE_Status esci2_stat(epsonds_scanner *s);
SANE_Status esci2_para(epsonds_scanner *s, char *parameters);
SANE_Status esci2_mech(epsonds_scanner *s, char *parameters);
SANE_Status esci2_trdt(epsonds_scanner *s);
SANE_Status esci2_img(struct epsonds_scanner *s, SANE_Int *length) ;

#endif

