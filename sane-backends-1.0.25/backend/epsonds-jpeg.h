/*
 * epsonds.c - Epson ESC/I-2 driver, JPEG support.
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

#define UNUSEDARG __attribute__ ((unused))

SANE_Status eds_jpeg_start(epsonds_scanner *s);
void eds_jpeg_finish(epsonds_scanner *s);
SANE_Status eds_jpeg_read_header(epsonds_scanner *s);
void eds_jpeg_read(SANE_Handle handle, SANE_Byte *data, SANE_Int max_length, SANE_Int *length);
