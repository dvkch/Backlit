/*
 * cdjpeg.h
 *
 * Copyright (C) 1994-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains common declarations for the sample applications
 * cjpeg and djpeg.  It is NOT used by the core JPEG library.
 */

#define JPEG_CJPEG_DJPEG	/* define proper options in jconfig.h */
#define JPEG_INTERNAL_OPTIONS	/* cjpeg.c,djpeg.c need to see xxx_SUPPORTED */
#include "sane/sanei_jinclude.h"
#include "jpeglib.h"
#include "jerror.h"		/* get library error codes too */
#include "sane/sanei_cderror.h"	/* get application-specific error codes */

/*
 * Object interface for djpeg's output file encoding modules
 */

typedef struct djpeg_dest_struct * djpeg_dest_ptr;

struct djpeg_dest_struct {
  /* start_output is called after jpeg_start_decompress finishes.
   * The color map will be ready at this time, if one is needed.
   */
  JMETHOD(void, start_output, (j_decompress_ptr cinfo,
			       djpeg_dest_ptr dinfo));
  /* Emit the specified number of pixel rows from the buffer. */
  JMETHOD(void, put_pixel_rows, (j_decompress_ptr cinfo,
				 djpeg_dest_ptr dinfo,
				 JDIMENSION rows_supplied,
				 char *data));
  /* Finish up at the end of the image. */
  JMETHOD(void, finish_output, (j_decompress_ptr cinfo,
				djpeg_dest_ptr dinfo));

  /* Target file spec; filled in by djpeg.c after object is created. */
  FILE * output_file;

  /* Output pixel-row buffer.  Created by module init or start_output.
   * Width is cinfo->output_width * cinfo->output_components;
   * height is buffer_height.
   */
  JSAMPARRAY buffer;
  JDIMENSION buffer_height;
};

/* Module selection routines for I/O modules. */

EXTERN(djpeg_dest_ptr) sanei_jpeg_jinit_write_ppm JPP((j_decompress_ptr cinfo));

/* miscellaneous useful macros */
