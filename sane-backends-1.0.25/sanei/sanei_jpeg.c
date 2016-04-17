/*
   Code for JPEG decompression by the Independent JPEG Group.  Please see
   README.jpeg in the top level directory.
 */

#include "../include/sane/config.h"

#ifdef HAVE_LIBJPEG

#include "../include/sane/sanei_jpeg.h"

typedef struct
  {
    struct djpeg_dest_struct pub;	/* public fields */

    /* Usually these two pointers point to the same place: */
    char *iobuffer;		/* fwrite's I/O buffer */
    JSAMPROW pixrow;		/* decompressor output buffer */
    size_t buffer_width;	/* width of I/O buffer */
    JDIMENSION samples_per_row;	/* JSAMPLEs per output row */
  }
ppm_dest_struct;

typedef ppm_dest_struct *ppm_dest_ptr;

/*
 * For 12-bit JPEG data, we either downscale the values to 8 bits
 * (to write standard byte-per-sample PPM/PGM files), or output
 * nonstandard word-per-sample PPM/PGM files.  Downscaling is done
 * if PPM_NORAWWORD is defined (this can be done in the Makefile
 * or in jconfig.h).
 * (When the core library supports data precision reduction, a cleaner
 * implementation will be to ask for that instead.)
 */

#if BITS_IN_JSAMPLE==8
#define PUTPPMSAMPLE(ptr,v)  *ptr++=(char) (v)
#define BYTESPERSAMPLE 1
#define PPM_MAXVAL 255
#else
#ifdef PPM_NORAWWORD
#define PUTPPMSAMPLE(ptr,v)  *ptr++=(char) ((v) >> (BITS_IN_JSAMPLE-8))
#define BYTESPERSAMPLE 1
#define PPM_MAXVAL 255
#else
/* The word-per-sample format always puts the LSB first. */
#define PUTPPMSAMPLE(ptr,v)			\
	{ register int val_=v;		\
		*ptr++=(char) (val_ & 0xFF);	\
		*ptr++=(char) ((val_ >> 8) & 0xFF);	\
	}
#define BYTESPERSAMPLE 2
#define PPM_MAXVAL ((1<<BITS_IN_JSAMPLE)-1)
#endif
#endif

METHODDEF (void)
sanei_jpeg_start_output_ppm (j_decompress_ptr cinfo, djpeg_dest_ptr dinfo)
{
  cinfo = cinfo;
  dinfo = dinfo;
  /* header image is supplied for us */

}

METHODDEF (void)
sanei_jpeg_finish_output_ppm (j_decompress_ptr cinfo, djpeg_dest_ptr dinfo)
{
  cinfo = cinfo;
  dinfo = dinfo;

  /* nothing to do */
}

/*
 * Write some pixel data.
 * In this module rows_supplied will always be 1.
 *
 * put_pixel_rows handles the "normal" 8-bit case where the decompressor
 * output buffer is physically the same as the fwrite buffer.
 */

METHODDEF (void)
sanei_jpeg_put_pixel_rows (j_decompress_ptr cinfo, djpeg_dest_ptr dinfo,
			   JDIMENSION rows_supplied, char *data)
{
  ppm_dest_ptr dest = (ppm_dest_ptr) dinfo;
  cinfo = cinfo;
  dinfo = dinfo;
  rows_supplied = rows_supplied;

  memcpy (data, dest->iobuffer, dest->buffer_width);
}


/*
 * This code is used when we have to copy the data and apply a pixel
 * format translation.  Typically this only happens in 12-bit mode.
 */

METHODDEF (void)
sanei_jpeg_copy_pixel_rows (j_decompress_ptr cinfo, djpeg_dest_ptr dinfo,
			    JDIMENSION rows_supplied, char *data)
{
  ppm_dest_ptr dest = (ppm_dest_ptr) dinfo;
  register char *bufferptr;
  register JSAMPROW ptr;
  register JDIMENSION col;

  cinfo = cinfo;
  dinfo = dinfo;
  rows_supplied = rows_supplied;

  ptr = dest->pub.buffer[0];
  bufferptr = dest->iobuffer;
  for (col = dest->samples_per_row; col > 0; col--)
    {
      PUTPPMSAMPLE (bufferptr, GETJSAMPLE (*ptr++));
    }
  memcpy (data, dest->iobuffer, dest->buffer_width);
}


/*
 * Write some pixel data when color quantization is in effect.
 * We have to demap the color index values to straight data.
 */

METHODDEF (void)
sanei_jpeg_put_demapped_rgb (j_decompress_ptr cinfo, djpeg_dest_ptr dinfo,
			     JDIMENSION rows_supplied, char *data)
{

  ppm_dest_ptr dest = (ppm_dest_ptr) dinfo;
  register char *bufferptr;
  register int pixval;
  register JSAMPROW ptr;
  register JSAMPROW color_map0 = cinfo->colormap[0];
  register JSAMPROW color_map1 = cinfo->colormap[1];
  register JSAMPROW color_map2 = cinfo->colormap[2];
  register JDIMENSION col;

  rows_supplied = rows_supplied;

  ptr = dest->pub.buffer[0];
  bufferptr = dest->iobuffer;
  for (col = cinfo->output_width; col > 0; col--)
    {
      pixval = GETJSAMPLE (*ptr++);
      PUTPPMSAMPLE (bufferptr, GETJSAMPLE (color_map0[pixval]));
      PUTPPMSAMPLE (bufferptr, GETJSAMPLE (color_map1[pixval]));
      PUTPPMSAMPLE (bufferptr, GETJSAMPLE (color_map2[pixval]));
    }
  memcpy (data, dest->iobuffer, dest->buffer_width);
}


METHODDEF (void)
sanei_jpeg_put_demapped_gray (j_decompress_ptr cinfo, djpeg_dest_ptr dinfo,
			      JDIMENSION rows_supplied, char *data)
{
  ppm_dest_ptr dest = (ppm_dest_ptr) dinfo;
  register char *bufferptr;
  register JSAMPROW ptr;
  register JSAMPROW color_map = cinfo->colormap[0];
  register JDIMENSION col;

  rows_supplied = rows_supplied;

  ptr = dest->pub.buffer[0];
  bufferptr = dest->iobuffer;
  for (col = cinfo->output_width; col > 0; col--)
    {
      PUTPPMSAMPLE (bufferptr,
		    GETJSAMPLE (color_map[GETJSAMPLE (*ptr++)]));
    }
  memcpy (data, dest->iobuffer, dest->buffer_width);
}

GLOBAL (djpeg_dest_ptr)
sanei_jpeg_jinit_write_ppm (j_decompress_ptr cinfo)
{

  ppm_dest_ptr dest;

  /* Create module interface object, fill in method pointers */
  dest = (ppm_dest_ptr)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
				SIZEOF (ppm_dest_struct));
  dest->pub.start_output = sanei_jpeg_start_output_ppm;
  dest->pub.finish_output = sanei_jpeg_finish_output_ppm;

  /* Calculate output image dimensions so we can allocate space */
  jpeg_calc_output_dimensions (cinfo);

  /* Create physical I/O buffer.  Note we make this near on a PC. */
  dest->samples_per_row = cinfo->output_width * cinfo->out_color_components;
  dest->buffer_width = dest->samples_per_row * (BYTESPERSAMPLE * SIZEOF (char));
  dest->iobuffer = (char *) (*cinfo->mem->alloc_small)
    ((j_common_ptr) cinfo, JPOOL_IMAGE, dest->buffer_width);

  if (cinfo->quantize_colors || BITS_IN_JSAMPLE != 8 ||
      SIZEOF (JSAMPLE) != SIZEOF (char))
    {
      /* When quantizing, we need an output buffer for clrmap indexes
       * that's separate from the physical I/O buffer.  We also need a
       * separate buffer if pixel format translation must take place.
       */
      dest->pub.buffer = (*cinfo->mem->alloc_sarray)
	((j_common_ptr) cinfo, JPOOL_IMAGE,
	 cinfo->output_width * cinfo->output_components,
	 (JDIMENSION) 1);
      dest->pub.buffer_height = 1;
      if (!cinfo->quantize_colors)
	dest->pub.put_pixel_rows = sanei_jpeg_copy_pixel_rows;
      else if (cinfo->out_color_space == JCS_GRAYSCALE)
	dest->pub.put_pixel_rows = sanei_jpeg_put_demapped_gray;
      else
	dest->pub.put_pixel_rows = sanei_jpeg_put_demapped_rgb;
    }
  else
    {
      /* We will fwrite() directly from decompressor output buffer. */
      /* Synthesize a JSAMPARRAY pointer structure */
      /* Cast here implies near->far pointer conversion on PCs */
      dest->pixrow = (JSAMPROW) dest->iobuffer;
      dest->pub.buffer = &dest->pixrow;
      dest->pub.buffer_height = 1;
      dest->pub.put_pixel_rows = sanei_jpeg_put_pixel_rows;
    }

  return (djpeg_dest_ptr) dest;
}

#endif
