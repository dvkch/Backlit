/*
 * epsonds-ops.h - Epson ESC/I-2 driver.
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

#define e2_model(s,m) e2_dev_model((s)->hw,(m))

extern void eds_dev_init(epsonds_device *dev);
extern SANE_Status eds_dev_post_init(struct epsonds_device *dev);

extern SANE_Status eds_add_resolution(epsonds_device *dev, int r);
extern SANE_Status eds_set_resolution_range(epsonds_device *dev, int min, int max);
extern void eds_set_fbf_area(epsonds_device *dev, int x, int y, int unit);
extern void eds_set_adf_area(epsonds_device *dev, int x, int y, int unit);
extern void eds_set_tpu_area(epsonds_device *dev, int x, int y, int unit);

extern SANE_Status eds_add_depth(epsonds_device *dev, SANE_Word depth);
extern SANE_Status eds_discover_capabilities(epsonds_scanner *s);
extern SANE_Status eds_set_extended_scanning_parameters(epsonds_scanner *s);
extern SANE_Status eds_set_scanning_parameters(epsonds_scanner *s);
extern void eds_setup_block_mode(epsonds_scanner *s);
extern SANE_Status eds_init_parameters(epsonds_scanner *s);

extern void eds_copy_image_from_ring(epsonds_scanner *s, SANE_Byte *data, SANE_Int max_length,
                   SANE_Int *length);

extern SANE_Status eds_ring_init(ring_buffer *ring, SANE_Int size);
extern SANE_Status eds_ring_write(ring_buffer *ring, SANE_Byte *buf, SANE_Int size);
extern SANE_Int eds_ring_read(ring_buffer *ring, SANE_Byte *buf, SANE_Int size);
extern SANE_Int eds_ring_skip(ring_buffer *ring, SANE_Int size);
extern SANE_Int eds_ring_avail(ring_buffer *ring);
extern void eds_ring_flush(ring_buffer *ring)    ;

