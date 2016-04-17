/** @file sanei_ir.c
 *
 * sanei_ir - functions for utilizing the infrared plane
 *
 * Copyright (C) 2012 Michael Rickmann <mrickma@gwdg.de>
 *
 * This file is part of the SANE package.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 *
 * The threshold yen, otsu and max_entropy routines have been
 * adapted from the FOURIER 0.8 library by M. Emre Celebi,
 * http://sourceforge.net/projects/fourier-ipal/ which is
 * licensed under the GNU General Public License version 2 or later.
*/

#include <stdlib.h>
#include <string.h>
#include <values.h>
#include <math.h>

#define BACKEND_NAME sanei_ir	/* name of this module for debugging */

#include "../include/sane/sane.h"
#include "../include/sane/sanei_debug.h"
#include "../include/sane/sanei_ir.h"
#include "../include/sane/sanei_magic.h"


double *
sanei_ir_create_norm_histo (const SANE_Parameters * params, const SANE_Uint *img_data);
double * sanei_ir_accumulate_norm_histo (double * histo_data);


/* Initialize sanei_ir
 */
void
sanei_ir_init (void)
{
  DBG_INIT ();
}


/* Create a normalized histogram of a grayscale image, internal
 */
double *
sanei_ir_create_norm_histo (const SANE_Parameters * params,
                       const SANE_Uint *img_data)
{
  int is, i;
  int num_pixels;
  int *histo_data;
  double *histo;
  double term;

  DBG (10, "sanei_ir_create_norm_histo\n");

  if ((params->format != SANE_FRAME_GRAY)
      && (params->format != SANE_FRAME_RED)
      && (params->format != SANE_FRAME_GREEN)
      && (params->format != SANE_FRAME_BLUE))
    {
      DBG (5, "sanei_ir_create_norm_histo: invalid format\n");
      return NULL;
    }

  /* Allocate storage for the histogram */
  histo_data = calloc (HISTOGRAM_SIZE, sizeof (int));
  histo = malloc (HISTOGRAM_SIZE * sizeof (double));
  if ((histo == NULL) || (histo_data == NULL))
    {
      DBG (5, "sanei_ir_create_norm_histo: no buffers\n");
      if (histo) free (histo);
      if (histo_data) free (histo_data);
      return NULL;
    }

  num_pixels = params->pixels_per_line * params->lines;

  DBG (1, "sanei_ir_create_norm_histo: %d pixels_per_line, %d lines => %d num_pixels\n", params->pixels_per_line, params->lines, num_pixels);
  DBG (1, "sanei_ir_create_norm_histo: histo_data[] with %d x %ld bytes\n", HISTOGRAM_SIZE, sizeof(int));
  /* Populate the histogram */
  is = 16 - HISTOGRAM_SHIFT; /* Number of data bits to ignore */
  DBG (1, "sanei_ir_create_norm_histo: depth %d, HISTOGRAM_SHIFT %d => ignore %d bits\n", params->depth, HISTOGRAM_SHIFT, is);
  for (i = num_pixels; i > 0; i--) {
      histo_data[*img_data++ >> is]++;
  }

  /* Calculate the normalized histogram */
  term = 1.0 / (double) num_pixels;
  for (i = 0; i < HISTOGRAM_SIZE; i++)
    histo[i] = term * (double) histo_data[i];

  free (histo_data);
  return histo;
}


/* Create the normalized histogram of a grayscale image
 */
SANE_Status
sanei_ir_create_norm_histogram (const SANE_Parameters * params,
                                const SANE_Uint *img_data,
                                double ** histogram)
{
  double *histo;

  DBG (10, "sanei_ir_create_norm_histogram\n");

  histo = sanei_ir_create_norm_histo (params, img_data);
  if (!histo)
    return SANE_STATUS_NO_MEM;

  *histogram = histo;
  return SANE_STATUS_GOOD;
}

/* Accumulate a normalized histogram, internal
 */
double *
sanei_ir_accumulate_norm_histo (double * histo_data)
{
  int i;
  double *accum_data;

  accum_data = malloc (HISTOGRAM_SIZE * sizeof (double));
  if (accum_data == NULL)
    {
      DBG (5, "sanei_ir_accumulate_norm_histo: Insufficient memory !\n");
      return NULL;
    }

  accum_data[0] = histo_data[0];
  for (i = 1; i < HISTOGRAM_SIZE; i++)
    accum_data[i] = accum_data[i - 1] + histo_data[i];

  return accum_data;
}

/* Implements Yen's thresholding method
 */
SANE_Status
sanei_ir_threshold_yen (const SANE_Parameters * params,
                         double * norm_histo, int *thresh)
{
  double *P1 = NULL;            /* cumulative normalized histogram */
  double *P1_sq = NULL;         /* cumulative normalized histogram */
  double *P2_sq = NULL;
  double crit, max_crit;
  int threshold, i;
  SANE_Status ret = SANE_STATUS_NO_MEM;

  DBG (10, "sanei_ir_threshold_yen\n");

  P1 = sanei_ir_accumulate_norm_histo (norm_histo);
  P1_sq = malloc (HISTOGRAM_SIZE * sizeof (double));
  P2_sq = malloc (HISTOGRAM_SIZE * sizeof (double));
  if (!P1 || !P1_sq || !P2_sq)
    {
      DBG (5, "sanei_ir_threshold_yen: no buffers\n");
      goto cleanup;
    }

  /* calculate cumulative squares */
  P1_sq[0] = norm_histo[0] * norm_histo[0];
  for (i = 1; i < HISTOGRAM_SIZE; i++)
      P1_sq[i] = P1_sq[i - 1] + norm_histo[i] * norm_histo[i];
  P2_sq[HISTOGRAM_SIZE - 1] = 0.0;
  for (i = HISTOGRAM_SIZE - 2; i >= 0; i--)
      P2_sq[i] = P2_sq[i + 1] + norm_histo[i + 1] * norm_histo[i + 1];

  /* Find the threshold that maximizes the criterion */
  threshold = INT_MIN;
  max_crit = DBL_MIN;
  for (i = 0; i < HISTOGRAM_SIZE; i++)
    {
      crit =
        -1.0 * SAFE_LOG (P1_sq[i] * P2_sq[i]) +
        2 * SAFE_LOG (P1[i] * (1.0 - P1[i]));
      if (crit > max_crit)
        {
          max_crit = crit;
          threshold = i;
        }
    }

  if (threshold == INT_MIN)
    {
      DBG (5, "sanei_ir_threshold_yen: no threshold found\n");
      ret = SANE_STATUS_INVAL;
    }
  else
    {
      ret = SANE_STATUS_GOOD;
      if (params->depth > 8)
        {
          i = 1 << (params->depth - HISTOGRAM_SHIFT);
          *thresh = threshold * i + i / 2;
        }
      else
        *thresh = threshold;
      DBG (10, "sanei_ir_threshold_yen: threshold %d\n", *thresh);
    }

  cleanup:
    if (P1)
      free (P1);
    if (P1_sq)
      free (P1_sq);
    if (P2_sq)
      free (P2_sq);
    return ret;
}


/* Implements Otsu's thresholding method
 */
SANE_Status
sanei_ir_threshold_otsu (const SANE_Parameters * params,
                          double * norm_histo, int *thresh)
{
  double *cnh = NULL;
  double *mean = NULL;
  double total_mean;
  double bcv, max_bcv;
  int first_bin, last_bin;
  int threshold, i;
  SANE_Status ret = SANE_STATUS_NO_MEM;

  DBG (10, "sanei_ir_threshold_otsu\n");

  cnh = sanei_ir_accumulate_norm_histo (norm_histo);
  mean = malloc (HISTOGRAM_SIZE * sizeof (double));
  if (!cnh || !mean)
    {
      DBG (5, "sanei_ir_threshold_otsu: no buffers\n");
      goto cleanup;
    }

  mean[0] = 0.0;
  for (i = 1; i < HISTOGRAM_SIZE; i++)
      mean[i] = mean[i - 1] + i * norm_histo[i];
  total_mean = mean[HISTOGRAM_SIZE - 1];

  first_bin = 0;
  for (i = 0; i < HISTOGRAM_SIZE; i++)
    if (cnh[i] != 0)
      {
        first_bin = i;
        break;
      }
  last_bin = HISTOGRAM_SIZE - 1;
  for (i = HISTOGRAM_SIZE - 1; i >= first_bin; i--)
    if (1.0 - cnh[i] != 0)
      {
        last_bin = i;
        break;
      }

  threshold = INT_MIN;
  max_bcv = 0.0;
  for (i = first_bin; i <= last_bin; i++)
    {
      bcv = total_mean * cnh[i] - mean[i];
      bcv *= bcv / (cnh[i] * (1.0 - cnh[i]));
      if (max_bcv < bcv)
        {
          max_bcv = bcv;
          threshold = i;
        }
    }

  if (threshold == INT_MIN)
    {
      DBG (5, "sanei_ir_threshold_otsu: no threshold found\n");
      ret = SANE_STATUS_INVAL;
    }
  else
    {
      ret = SANE_STATUS_GOOD;
      if (params->depth > 8)
        {
          i = 1 << (params->depth - HISTOGRAM_SHIFT);
          *thresh = threshold * i + i / 2;
        }
      else
        *thresh = threshold;
      DBG (10, "sanei_ir_threshold_otsu: threshold %d\n", *thresh);
    }
  cleanup:
    if (cnh)
      free (cnh);
    if (mean)
      free (mean);
    return ret;
}


/* Implements a Maximum Entropy thresholding method
 */
SANE_Status
sanei_ir_threshold_maxentropy (const SANE_Parameters * params,
                               double * norm_histo, int *thresh)
{
 int ih, it;
 int threshold;
 int first_bin;
 int last_bin;
 double tot_ent, max_ent;       /* entropies */
 double ent_back, ent_obj;
 double *P1;                    /* cumulative normalized histogram */
 double *P2;
 SANE_Status ret = SANE_STATUS_NO_MEM;

 DBG (10, "sanei_ir_threshold_maxentropy\n");

 /* Calculate the cumulative normalized histogram */
 P1 = sanei_ir_accumulate_norm_histo (norm_histo);
 P2 = malloc (HISTOGRAM_SIZE * sizeof (double));
 if (!P1 || !P2)
   {
     DBG (5, "sanei_ir_threshold_maxentropy: no buffers\n");
     goto cleanup;
   }

 for ( ih = 0; ih < HISTOGRAM_SIZE; ih++ )
   P2[ih] = 1.0 - P1[ih];

 first_bin = 0;
 for ( ih = 0; ih < HISTOGRAM_SIZE; ih++ )
   if (P1[ih] != 0)
    {
     first_bin = ih;
     break;
    }
 last_bin = HISTOGRAM_SIZE - 1;
 for ( ih = HISTOGRAM_SIZE - 1; ih >= first_bin; ih-- )
   if (P2[ih] != 0)
    {
     last_bin = ih;
     break;
    }

 /* Calculate the total entropy each gray-level
  * and find the threshold that maximizes it
  */
 threshold = INT_MIN;
 max_ent = DBL_MIN;
 for ( it = first_bin; it <= last_bin; it++ )
  {
   /* Entropy of the background pixels */
   ent_back = 0.0;
   for ( ih = 0; ih <= it; ih++ )
     if (norm_histo[ih] != 0)
       ent_back -= ( norm_histo[ih] / P1[it] ) * log ( norm_histo[ih] / P1[it] );

   /* Entropy of the object pixels */
   ent_obj = 0.0;
   for ( ih = it + 1; ih < HISTOGRAM_SIZE; ih++ )
     if (norm_histo[ih] != 0)
       ent_obj -= ( norm_histo[ih] / P2[it] ) * log ( norm_histo[ih] / P2[it] );

   /* Total entropy */
   tot_ent = ent_back + ent_obj;

   if ( max_ent < tot_ent )
    {
     max_ent = tot_ent;
     threshold = it;
    }
  }

 if (threshold == INT_MIN)
   {
     DBG (5, "sanei_ir_threshold_maxentropy: no threshold found\n");
     ret = SANE_STATUS_INVAL;
   }
 else
   {
     ret = SANE_STATUS_GOOD;
     if (params->depth > 8)
       {
         it = 1 << (params->depth - HISTOGRAM_SHIFT);
         *thresh = threshold * it + it / 2;
       }
     else
       *thresh = threshold;
     DBG (10, "sanei_ir_threshold_maxentropy: threshold %d\n", *thresh);
 }

 cleanup:
   if (P1)
     free (P1);
   if (P2)
     free (P2);
   return ret;
}

/* Generate gray scale luminance image from separate R, G, B images
 */
SANE_Status
sanei_ir_RGB_luminance (SANE_Parameters * params, const SANE_Uint **in_img,
                      SANE_Uint **out_img)
{
  SANE_Uint *outi;
  int itop, i;

  if ((params->depth < 8) || (params->depth > 16) ||
      (params->format != SANE_FRAME_GRAY))
    {
      DBG (5, "sanei_ir_RGB_luminance: invalid format\n");
      return SANE_STATUS_UNSUPPORTED;
    }

  itop = params->pixels_per_line * params->lines;
  outi = malloc (itop * sizeof(SANE_Uint));
  if (!outi)
    {
      DBG (5, "sanei_ir_RGB_luminance: can not allocate out_img\n");
      return SANE_STATUS_NO_MEM;
    }

  for (i = itop; i > 0; i--)
      *(outi++) = (218 * (int) *(in_img[0]++) +
                   732 * (int) *(in_img[1]++) +
                   74 * (int) *(in_img[2]++)) >> 10;
  *out_img = outi;
  return SANE_STATUS_GOOD;
}

/* Convert image from >8 bit depth to an 8 bit image
 */
SANE_Status
sanei_ir_to_8bit (SANE_Parameters * params, const SANE_Uint *in_img,
                 SANE_Parameters * out_params, SANE_Uint **out_img)
{
  SANE_Uint *outi;
  size_t ssize;
  int i, is;

  if ((params->depth < 8) || (params->depth > 16))
    {
      DBG (5, "sanei_ir_to_8bit: invalid format\n");
      return SANE_STATUS_UNSUPPORTED;
    }
  ssize = params->pixels_per_line * params->lines;
  if (params->format == SANE_FRAME_RGB)
    ssize *= 3;
  outi = malloc (ssize * sizeof(SANE_Uint));
  if (!outi)
    {
      DBG (5, "sanei_ir_to_8bit: can not allocate out_img\n");
      return SANE_STATUS_NO_MEM;
    }

  if (out_params)
    {
      memmove (out_params, params, sizeof(SANE_Parameters));
      out_params->bytes_per_line = out_params->pixels_per_line;
      if (params->format == SANE_FRAME_RGB)
        out_params->bytes_per_line *= 3;
      out_params->depth = 8;
    }

  memmove (outi, in_img, ssize * sizeof(SANE_Uint));
  is = params->depth - 8;
  for (i = ssize; i > 0; i--) {
    *outi = *outi >> is, outi += 2;
  }

  *out_img = outi;
  return SANE_STATUS_GOOD;
}

/* allocate and initialize logarithmic lookup table
 */
SANE_Status
sanei_ir_ln_table (int len, double **lut_ln)
{
  double *llut;
  int i;

  DBG (10, "sanei_ir_ln_table\n");

  llut = malloc (len * sizeof (double));
  if (!llut)
    {
      DBG (5, "sanei_ir_ln_table: no table\n");
      return SANE_STATUS_NO_MEM;
    }
  llut[0] = 0;
  llut[1] = 0;
  for (i = 2; i < len; i++)
    llut[i] = log ((double) i);

  *lut_ln = llut;
  return SANE_STATUS_GOOD;
}


/* Reduce red spectral overlap from an infrared image plane
 */
SANE_Status
sanei_ir_spectral_clean (const SANE_Parameters * params, double *lut_ln,
			const SANE_Uint *red_data,
			SANE_Uint *ir_data)
{
  const SANE_Uint *rptr;
  SANE_Uint *iptr;
  SANE_Int depth;
  double *llut;
  double rval, rsum, rrsum;
  double risum, rfac, radd;
  double *norm_histo;
  int64_t isum;
  int *calc_buf, *calc_ptr;
  int ival, imin, imax;
  int itop, len, ssize;
  int thresh_low, thresh;
  int irand, i;
  SANE_Status status;

  DBG (10, "sanei_ir_spectral_clean\n");

  itop = params->pixels_per_line * params->lines;
  calc_buf = malloc (itop * sizeof (int));		/* could save this */
  if (!calc_buf)
    {
      DBG (5, "sanei_ir_spectral_clean: no buffer\n");
      return SANE_STATUS_NO_MEM;
    }

  depth = params->depth;
  len = 1 << depth;
  if (lut_ln)
    llut = lut_ln;
  else
    {
      status = sanei_ir_ln_table (len, &llut);
      if (status != SANE_STATUS_GOOD) {
        free (calc_buf);
        return status;
      }
    }

  /* determine not transparent areas to exclude them later
   * TODO: this has not been tested for negatives
   */
  thresh_low = INT_MAX;
  status =
      sanei_ir_create_norm_histogram (params, ir_data, &norm_histo);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (5, "sanei_ir_spectral_clean: no buffer\n");
      free (calc_buf);
      return SANE_STATUS_NO_MEM;
    }

  /* TODO: remember only needed if cropping is not ok */
  status = sanei_ir_threshold_maxentropy (params, norm_histo, &thresh);
  if (status == SANE_STATUS_GOOD)
    thresh_low = thresh;
  status = sanei_ir_threshold_otsu (params, norm_histo, &thresh);
  if ((status == SANE_STATUS_GOOD) && (thresh < thresh_low))
    thresh_low = thresh;
  status = sanei_ir_threshold_yen (params, norm_histo, &thresh);
  if ((status == SANE_STATUS_GOOD) && (thresh < thresh_low))
    thresh_low = thresh;
  if (thresh_low == INT_MAX)
    thresh_low = 0;
  else
    thresh_low /= 2;
  DBG (10, "sanei_ir_spectral_clean: low threshold %d\n", thresh_low);

  /* calculate linear regression ired (red) from randomly chosen points */
  ssize = itop / 2;
  if (SAMPLE_SIZE < ssize)
    ssize = SAMPLE_SIZE;
  isum = 0;
  rsum = rrsum = risum = 0.0;
  i = ssize;
  while (i > 0)
    {
      irand = rand () % itop;
      rval = llut[red_data[irand]];
      ival = ir_data[irand];
      if (ival > thresh_low)
        {
          isum += ival;
          rsum += rval;
          rrsum += rval * rval;
          risum += rval * (double) ival;
          i--;
        }
    }

  /* "a" in ired = b + a * ln (red) */
  rfac =
    ((double) ssize * risum -
    rsum * (double) isum) / ((double) ssize * rrsum - rsum * rsum);
    radd = ((double) isum - rfac * rsum) / (double) ssize;      /* "b" unused */

  DBG (10, "sanei_ir_spectral_clean: n = %d, ired(red) = %f * ln(red) + %f\n",
            ssize, rfac, radd);

  /* now calculate ired' = ired - a  * ln (red) */
  imin = INT_MAX;
  imax = INT_MIN;
  rptr = red_data;
  iptr = ir_data;
  calc_ptr = calc_buf;
    for (i = itop; i > 0; i--)
      {
	ival = *iptr++ - (int) (rfac * llut[*rptr++] + 0.5);
	if (ival > imax)
	  imax = ival;
	if (ival < imin)
	  imin = ival;
	*calc_ptr++ = ival;
      }

  /* scale the result back into the ired image */
  calc_ptr = calc_buf;
  iptr = ir_data;
  rfac = (double) (len - 1) / (double) (imax - imin);
    for (i = itop; i > 0; i--)
      *iptr++ = (double) (*calc_ptr++ - imin) * rfac;

  if (!lut_ln)
    free (llut);
  free (calc_buf);
  free (norm_histo);
  return SANE_STATUS_GOOD;
}


/* Hopefully fast mean filter
 * JV: what does this do? Remove local mean?
 */
SANE_Status
sanei_ir_filter_mean (const SANE_Parameters * params,
		      const SANE_Uint *in_img, SANE_Uint *out_img,
		      int win_rows, int win_cols)
{
  const SANE_Uint *src;
  SANE_Uint *dest;
  int num_cols, num_rows;
  int itop, iadd, isub;
  int ndiv, the_sum;
  int nrow, ncol;
  int hwr, hwc;
  int *sum;
  int i, j;

  DBG (10, "sanei_ir_filter_mean, window: %d x%d\n", win_rows, win_cols);

  if (((win_rows & 1) == 0) || ((win_cols & 1) == 0))
    {
      DBG (5, "sanei_ir_filter_mean: window even sized\n");
      return SANE_STATUS_INVAL;
    }

  num_cols = params->pixels_per_line;
  num_rows = params->lines;

  sum = malloc (num_cols * sizeof (int));
  if (!sum)
    {
      DBG (5, "sanei_ir_filter_mean: no buffer for sums\n");
      return SANE_STATUS_NO_MEM;
    }
  dest = out_img;

  hwr = win_rows / 2;		/* half window sizes */
  hwc = win_cols / 2;

  /* pre-pre calculation */
  for (j = 0; j < num_cols; j++)
    {
        sum[j] = 0;
	src = in_img + j;
	for (i = 0; i < hwr; i++)
	  {
	    sum[j] += *src;
	    src += num_cols;
	  }
    }

  itop = num_rows * num_cols;
  iadd = hwr * num_cols;
  isub = (hwr - win_rows) * num_cols;
  nrow = hwr;

      for (i = 0; i < num_rows; i++)
	{
	  /* update row sums if possible */
	  if (isub >= 0)	/* subtract old row */
	    {
	      nrow--;
	      src = in_img + isub;
	      for (j = 0; j < num_cols; j++)
		sum[j] -= *src++;
	    }
	  isub += num_cols;

	  if (iadd < itop)	/* add new row */
	    {
	      nrow++;
	      src = in_img + iadd;
	      for (j = 0; j < num_cols; j++)
		sum[j] += *src++;
	    }
	  iadd += num_cols;

	  /* now we do the image columns using only the precalculated sums */

	  the_sum = 0;		/* precalculation */
	  for (j = 0; j < hwc; j++)
	    the_sum += sum[j];
	  ncol = hwc;

	  /* at the left margin, real index hwc lower */
	  for (j = hwc; j < win_cols; j++)
	    {
	      ncol++;
	      the_sum += sum[j];
	      *dest++ = the_sum / (ncol * nrow);
	    }

	  ndiv = ncol * nrow;
	  /* in the middle, real index hwc + 1 higher */
	  for (j = 0; j < num_cols - win_cols; j++)
	    {
	      the_sum -= sum[j];
	      the_sum += sum[j + win_cols];
	      *dest++ = the_sum / ndiv;
	    }

	  /* at the right margin, real index hwc + 1 higher */
	  for (j = num_cols - win_cols; j < num_cols - hwc - 1; j++)
	    {
	      ncol--;
	      the_sum -= sum[j];	/* j - hwc - 1 */
	      *dest++ = the_sum / (ncol * nrow);
	    }
	}
  free (sum);
  return SANE_STATUS_GOOD;
}


/* Find noise by adaptive thresholding
 */
SANE_Status
sanei_ir_filter_madmean (const SANE_Parameters * params,
			 const SANE_Uint *in_img,
			 SANE_Uint ** out_img, int win_size,
			 int a_val, int b_val)
{
  SANE_Uint *delta_ij, *delta_ptr;
  SANE_Uint *mad_ij;
  const SANE_Uint *mad_ptr;
  SANE_Uint *out_ij, *dest8;
  double ab_term;
  int num_rows, num_cols;
  int threshold, itop;
  size_t size;
  int ival, i;
  int depth;
  SANE_Status ret = SANE_STATUS_NO_MEM;

  DBG (10, "sanei_ir_filter_madmean\n");

  depth = params->depth;
  if (depth != 8)
    {
      a_val = a_val << (depth - 8);
      b_val = b_val << (depth - 8);
    }
  num_cols = params->pixels_per_line;
  num_rows = params->lines;
  itop = num_rows * num_cols;
  size = itop * sizeof (SANE_Uint);
  out_ij = malloc (size);
  delta_ij = malloc (size);
  mad_ij = malloc (size);

  if (out_ij && delta_ij && mad_ij)
    {
      /* get the differences to the local mean */
      mad_ptr = in_img;
      if (sanei_ir_filter_mean (params, mad_ptr, delta_ij, win_size, win_size)
	  == SANE_STATUS_GOOD)
	{
	  delta_ptr = delta_ij;
	    for (i = 0; i < itop; i++)
	      {
		ival = *mad_ptr++ - *delta_ptr;
		*delta_ptr++ = abs (ival);
	      }
	  /* make the second filtering window a bit larger */
	  win_size = MAD_WIN2_SIZE(win_size);
	  /* and get the local mean differences */
	  if (sanei_ir_filter_mean
	      (params, delta_ij, mad_ij, win_size,
	       win_size) == SANE_STATUS_GOOD)
	    {
	      mad_ptr = mad_ij;
	      delta_ptr = delta_ij;
	      dest8 = out_ij;
	      /* construct the noise map */
	      ab_term = (b_val - a_val) / (double) b_val;
		for (i = 0; i < itop; i++)
		  {
		    /* by calculating the threshold */
		    ival = *mad_ptr++;
		    if (ival >= b_val)	/* outlier */
		      threshold = a_val;
		    else
		      threshold = a_val + (double) ival *ab_term;
		    /* above threshold is noise, indicated by 0 */
		    if (*delta_ptr++ >= threshold)
		      *dest8++ = 0;
		    else
		      *dest8++ = 255;
		  }
	      *out_img = out_ij;
	      ret = SANE_STATUS_GOOD;
	    }
	}
    }
  else
    DBG (5, "sanei_ir_filter_madmean: Cannot allocate buffers\n");

  free (mad_ij);
  free (delta_ij);
  return ret;
}


/* Add dark pixels to mask from static threshold
 */
void
sanei_ir_add_threshold (const SANE_Parameters * params,
			const SANE_Uint *in_img,
			SANE_Uint * mask_img, int threshold)
{
  const SANE_Uint *in_ptr;
  SANE_Uint *mask_ptr;
  int itop, i;

  DBG (10, "sanei_ir_add_threshold\n");

  itop = params->pixels_per_line * params->lines;
  in_ptr = in_img;
  mask_ptr = mask_img;

    for (i = itop; i > 0; i--)
      {
	if (*in_ptr++ <= threshold)
	  *mask_ptr = 0;
	mask_ptr++;
      }
}


/* Calculate minimal Manhattan distances for an image mask
 */
void
sanei_ir_manhattan_dist (const SANE_Parameters * params,
			const SANE_Uint * mask_img, unsigned int *dist_map,
			unsigned int *idx_map, unsigned int erode)
{
  const SANE_Uint *mask;
  unsigned int *index, *manhattan;
  int rows, cols, itop;
  int i, j;

  DBG (10, "sanei_ir_manhattan_dist\n");

  if (erode != 0)
    erode = 255;

  /* initialize maps */
  cols = params->pixels_per_line;
  rows = params->lines;
  itop = rows * cols;
  mask = mask_img;
  manhattan = dist_map;
  index = idx_map;
  for (i = 0; i < itop; i++)
    {
      *manhattan++ = *mask++;
      *index++ = i;
    }

  /* traverse from top left to bottom right */
  manhattan = dist_map;
  index = idx_map;
  for (i = 0; i < rows; i++)
    for (j = 0; j < cols; j++)
      {
	if (*manhattan == erode)
	  {
	    /* take original, distance = 0, index stays the same */
	    *manhattan = 0;
	  }
	else
	  {
	    /* assume maximal distance to clean pixel */
	    *manhattan = cols + rows;
	    /* or one further away than pixel to the top */
	    if (i > 0)
	      if (manhattan[-cols] + 1 < *manhattan)
		{
		  *manhattan = manhattan[-cols] + 1;
		  *index = index[-cols];	/* index follows */
		}
	    /* or one further away than pixel to the left */
	    if (j > 0)
	      {
		if (manhattan[-1] + 1 < *manhattan)
		  {
		    *manhattan = manhattan[-1] + 1;
		    *index = index[-1];	/* index follows */
		  }
		if (manhattan[-1] + 1 == *manhattan)
		  if (rand () % 2 == 0)	/* chose index */
		    *index = index[-1];
	      }
	  }
	manhattan++;
	index++;
      }

  /* traverse from bottom right to top left */
  manhattan = dist_map + itop - 1;
  index = idx_map + itop - 1;
  for (i = rows - 1; i >= 0; i--)
    for (j = cols - 1; j >= 0; j--)
      {
	if (i < rows - 1)
	  {
	    /* either what we had on the first pass
	       or one more than the pixel to the bottm */
	    if (manhattan[+cols] + 1 < *manhattan)
	      {
		*manhattan = manhattan[+cols] + 1;
		*index = index[+cols];	/* index follows */
	      }
	    if (manhattan[+cols] + 1 == *manhattan)
	      if (rand () % 2 == 0)	/* chose index */
		*index = index[+cols];
	  }
	if (j < cols - 1)
	  {
	    /* or one more than pixel to the right */
	    if (manhattan[1] + 1 < *manhattan)
	      {
		*manhattan = manhattan[1] + 1;
		*index = index[1];	/* index follows */
	      }
	    if (manhattan[1] + 1 == *manhattan)
	      if (rand () % 2 == 0)	/* chose index */
		*index = index[1];
	  }
	manhattan--;
	index--;
      }
}


/* dilate or erode a mask image */

void
sanei_ir_dilate (const SANE_Parameters *params, SANE_Uint *mask_img,
		unsigned int *dist_map, unsigned int *idx_map, int by)
{
  SANE_Uint *mask;
  unsigned int *manhattan;
  unsigned int erode;
  unsigned int thresh;
  int i, itop;

  DBG (10, "sanei_ir_dilate\n");

  if (by == 0)
    return;
  if (by > 0)
    {
      erode = 0;
      thresh = by;
    }
  else
    {
      erode = 1;
      thresh = -by;
    }

  itop = params->pixels_per_line * params->lines;
  mask = mask_img;
  sanei_ir_manhattan_dist (params, mask_img, dist_map, idx_map, erode);

  manhattan = dist_map;
  for (i = 0; i < itop; i++)
    {
      if (*manhattan++ <= thresh)
	*mask++ = 0;
      else
	*mask++ = 255;
    }

  return;
}


/* Suggest cropping for dark margins of positive film
 */
void
sanei_ir_find_crop (const SANE_Parameters * params,
                    unsigned int * dist_map, int inner, int * edges)
{
  int width = params->pixels_per_line;
  int height = params->lines;
  uint64_t sum_x, sum_y, n;
  int64_t sum_xx, sum_xy;
  double a, b, mami;
  unsigned int *src;
  int off1, off2, inc, wh, i, j;

  DBG (10, "sanei_ir_find_crop\n");

  /* loop through top, bottom, left, right */
  for (j = 0; j < 4; j++)
    {
      if (j < 2)        /* top, bottom */
        {
          off1 = width / 8;     /* only middle 3/4 */
          off2 = width - off1;
          n = width - 2 * off1;
          src = dist_map + off1;        /* first row */
          inc = 1;
          wh = width;
          if (j == 1)                   /* last row */
            src += (height - 1) * width;
        }
      else              /* left, right */
        {
          off1 = height / 8;     /* only middle 3/4 */
          off2 = height - off1;
          n = height - 2 * off1;
          src = dist_map + (off1 * width);      /* first column */
          inc = width;
          wh = height;
          if (j == 3)
            src += width - 1;                   /* last column */
        }

      /* calculate linear regression */
      sum_x = 0; sum_y = 0;
      sum_xx = 0; sum_xy = 0;
      for (i = off1; i < off2; i++)
        {
          sum_x += i;
          sum_y += *src;
          sum_xx += i * i;
          sum_xy += i * (*src);
          src += inc;
        }
      b = ((double) n * (double) sum_xy - (double) sum_x * (double) sum_y)
          / ((double) n * (double) sum_xx - (double) sum_x * (double) sum_x);
      a = ((double) sum_y - b * (double) sum_x) / (double) n;

      DBG (10, "sanei_ir_find_crop: y = %f + %f * x\n", a, b);

      /* take maximal/minimal value from either side */
      mami = a + b * (wh - 1);
      if (inner)
        {
          if (a > mami)
            mami = a;
        }
      else
        {
          if (a < mami)
            mami = a;
        }
      edges[j] = mami + 0.5;
    }
  edges[1] = height - edges[1];
  edges[3] = width - edges[3];

  DBG (10, "sanei_ir_find_crop: would crop at top: %d, bot: %d, left %d, right %d\n",
      edges[0], edges[1], edges[2], edges[3]);

  return;
}


/* Dilate clean image parts into dirty ones and smooth
 */
SANE_Status
sanei_ir_dilate_mean (const SANE_Parameters * params,
                      SANE_Uint **in_img,
                      SANE_Uint * mask_img,
                      int dist_max, int expand, int win_size,
                      SANE_Bool smooth, int inner,
                      int *crop)
{
  SANE_Uint *color;
  SANE_Uint *plane;
  unsigned int *dist_map, *manhattan;
  unsigned int *idx_map, *index;
  int dist;
  int rows, cols;
  int k, i, itop;
  SANE_Status ret = SANE_STATUS_NO_MEM;

  DBG (10, "sanei_ir_dilate_mean(): dist max = %d, expand = %d, win size = %d, smooth = %d, inner = %d\n",
    dist_max, expand, win_size, smooth, inner);

  cols = params->pixels_per_line;
  rows = params->lines;
  itop = rows * cols;
  idx_map = malloc (itop * sizeof (unsigned int));
  dist_map = malloc (itop * sizeof (unsigned int));
  plane = malloc (itop * sizeof (SANE_Uint));

  if (!idx_map || !dist_map || !plane)
    DBG (5, "sanei_ir_dilate_mean: Cannot allocate buffers\n");
  else
    {
      /* expand dirty regions into their half dirty surround*/
      if (expand > 0)
	sanei_ir_dilate (params, mask_img, dist_map, idx_map, expand);
      /* for dirty pixels determine the closest clean ones */
      sanei_ir_manhattan_dist (params, mask_img, dist_map, idx_map, 1);

      /* use the distance map to find how to crop dark edges */
      if (crop)
        sanei_ir_find_crop (params, dist_map, inner, crop);

      /* replace dirty pixels */
      for (k = 0; k < 3; k++)
	{
	  manhattan = dist_map;
	  index = idx_map;
	  color = in_img[k];
	  /* first replacement */
	    for (i = 0; i < itop; i++)
	      {
		dist = *manhattan++;
		if ((dist != 0) && (dist <= dist_max))
		  color[i] = color[index[i]];
	      }
          /* adapt pixels to their new surround and
           * smooth the whole image or the replaced pixels only */
	  ret =
	    sanei_ir_filter_mean (params, color, plane, win_size, win_size);
	  if (ret != SANE_STATUS_GOOD)
	    break;
	  else
	    if (smooth)
              {
                /* a second mean results in triangular blur */
                DBG (10, "sanei_ir_dilate_mean(): smoothing whole image\n");
                ret =
                  sanei_ir_filter_mean (params, plane, color, win_size,
                                        win_size);
                if (ret != SANE_STATUS_GOOD)
                  break;
              }
            else
              {
                /* replace with smoothened pixels only */
                DBG (10, "sanei_ir_dilate_mean(): smoothing replaced pixels only\n");
                manhattan = dist_map;
                  for (i = 0; i < itop; i++)
                    {
                      dist = *manhattan++;
                      if ((dist != 0) && (dist <= dist_max))
                        color[i] = plane[i];
                    }
              }
      }
    }
  free (plane);
  free (dist_map);
  free (idx_map);

  return ret;
}
