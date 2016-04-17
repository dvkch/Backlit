/** @file sanei_ir.h
 *
 * This file provides an interface to the
 * sanei_ir functions for utilizing the infrared plane
 *
 * Copyright (C) 2012 Michael Rickmann <mrickma@gwdg.de>
 *
 * This file is part of the SANE package.
 *
 * Essentially three things have to be done:
 * - 1) reduce red spectral overlap from the infrared (ired) plane
 * - 2) find the dirt
 * - 3) replace the dirt
 *
 * - 1) is mainly adressed by sanei_ir_spectral_clean
 * - 2) by sanei_ir_filter_madmean
 * - 3) by sanei_ir_dilate_mean
 */


#ifndef SANEI_IR_H
#define SANEI_IR_H

#include <stdint.h>

#define SAMPLE_SIZE	40000				/**< maximal for random sampling */

#define HISTOGRAM_SHIFT	8				/**< standard histogram size */
#define HISTOGRAM_SIZE	(1 << HISTOGRAM_SHIFT)

#define SAFE_LOG(x) ( ((x) > 0.0) ? log ((x)) : (0.0) )	/**< define log (0) = 0 */

#define MAD_WIN2_SIZE(x) ( (((x) * 4) / 3) | 1 )	/**< MAD filter: 2nd window size */

typedef uint16_t SANE_Uint;

/**
 * @brief Pointer to access values of different bit depths
 */
typedef union
{
  uint8_t *b8;			/**< <= 8 bits */
  uint16_t *b16;		/**< > 8, <= 16 bits */
}
SANEI_IR_bufptr;


/** Initialize sanei_ir.
 *
 * Call this before any other sanei_ir function.
 */
extern void sanei_ir_init (void);

/**
 * @brief Create the normalized histogram of a grayscale image
 *
 * @param[in] params describes image
 * @param[in] img_data image pointer { grayscale }
 * @param[out] histogram an array of double with histogram
 *
 * @return
 * - SANE_STATUS_GOOD - success
 * - SANE_STATUS_NO_MEM - if out of memory
 *
 * @note
 * histogram has to be freed by calling routine
 */
extern SANE_Status
sanei_ir_create_norm_histogram (const SANE_Parameters * params,
                           const SANE_Uint *img_data,
                           double ** histogram);

/**
 * @brief Implements Yen's thresholding method
 *
 * @param[in] params describes image
 * @param[in] norm_histo points to a normalized histogram
 * @param[out] thresh found threshold
 *
 * @return
 * - SANE_STATUS_GOOD - success
 * - SANE_STATUS_NO_MEM - if out of memory
 *
 * -# Yen J.C., Chang F.J., and Chang S. (1995) "A New Criterion
 *       for Automatic Multilevel Thresholding" IEEE Trans. on Image
 *       Processing, 4(3): 370-378
 * -# Sezgin M. and Sankur B. (2004) "Survey over Image Thresholding
 *       Techniques and Quantitative Performance Evaluation" Journal of
 *       Electronic Imaging, 13(1): 146-165
 * -# M. Emre Celebi, 06.15.2007, fourier_0.8,
 *       http://sourceforge.net/projects/fourier-ipal/
 * -# ImageJ Multithresholder plugin,
 *       http://rsbweb.nih.gov/ij/plugins/download/AutoThresholder.java
 */
extern SANE_Status
sanei_ir_threshold_yen (const SANE_Parameters * params,
                         double * norm_histo, int *thresh);

/**
 * @brief Implements Otsu's thresholding method
 *
 * @param[in] params describes image
 * @param[in] norm_histo points to a normalized histogram
 * @param[out] thresh found threshold
 *
 * @return
 * - SANE_STATUS_GOOD - success
 * - SANE_STATUS_NO_MEM - if out of memory
 *
 * -# Otsu N. (1979) "A Threshold Selection Method from Gray Level Histograms"
 *      IEEE Trans. on Systems, Man and Cybernetics, 9(1): 62-66
 * -# M. Emre Celebi, 06.15.2007, fourier_0.8
 *      http://sourceforge.net/projects/fourier-ipal/
 */
extern SANE_Status
sanei_ir_threshold_otsu (const SANE_Parameters * params,
                          double * norm_histo, int *thresh);

/**
 * @brief Implements a Maximum Entropy thresholding method
 *
 * @param[in] params describes image
 * @param[in] norm_histo points to a normalized histogram
 * @param[out] thresh found threshold
 *
 * @return
 * - SANE_STATUS_GOOD - success
 * - SANE_STATUS_NO_MEM - if out of memory
 *
 * -# Kapur J.N., Sahoo P.K., and Wong A.K.C. (1985) "A New Method for
 *      Gray-Level Picture Thresholding Using the Entropy of the Histogram"
 *      Graphical Models and Image Processing, 29(3): 273-285
 * -# M. Emre Celebi, 06.15.2007, fourier_0.8
 *      http://sourceforge.net/projects/fourier-ipal/
 * -# ImageJ Multithresholder plugin,
 *       http://rsbweb.nih.gov/ij/plugins/download/AutoThresholder.java
 */
extern SANE_Status
sanei_ir_threshold_maxentropy (const SANE_Parameters * params,
                               double * norm_histo, int *thresh);

/**
 * @brief  Generate gray scale luminance image from separate R, G, B images
 *
 * @param      params points to image description
 * @param[in]  in_img pointer to at least 3 planes of image data
 * @param[out] out_img newly allocated image
 *
 * @return
 * - SANE_STATUS_GOOD - success
 * - SANE_STATUS_NO_MEM - if out of memory
 * - SANE_STATUS_UNSUPPORTED - wrong input bit depth
 *
 * @note out_img has to be freed by the calling routine.
 * @note on input params describe a single color plane,
 *       on output params are updated if image depth is scaled
 */
SANE_Status
sanei_ir_RGB_luminance (SANE_Parameters * params, const SANE_Uint **in_img,
                       SANE_Uint **out_img);

/**
 * @brief Convert image from >8 bit depth to an 8 bit image.
 *
 * @param[in]  params pimage description
 * @param[in]  in_img points to input image data
 * @param[out] out_params if != NULL
 *                        receives description of new image
 * @param[out] out_img newly allocated 8-bit image
 *
 * @return
 * - SANE_STATUS_GOOD - success
 * - SANE_STATUS_NO_MEM - if out of memory
 * - SANE_STATUS_UNSUPPORTED - wrong input bit depth
 *
 * @note
 * out_img has to be freed by the calling routine,
 */

extern SANE_Status
sanei_ir_to_8bit (SANE_Parameters * params, const SANE_Uint *in_img,
                 SANE_Parameters * out_params, SANE_Uint **out_img);

/**
 * @brief Allocate and initialize logarithmic lookup table
 *
 * @param[in]  length of table, usually 1 << depth
 * @param[out] lut_ln adress of pointer to allocated table
 *
 * @return
 * - SANE_STATUS_GOOD - success
 * - SANE_STATUS_NO_MEM - if out of memory
 *
 * @note natural logarithms are provided
 */
SANE_Status sanei_ir_ln_table (int len, double **lut_ln);

/**
 * @brief Reduces red spectral overlap from an infrared image plane
 *
 * @param[in]  params pointer to image description
 * @param[in]  lut_ln pointer lookup table
 *             if NULL it is dynamically handled
 * @param[in]  red_data pointer to red image plane
 * @param      ired_data pointer to ired image plane
 *
 * @return
 * - SANE_STATUS_GOOD - success
 * - SANE_STATUS_NO_MEM - if out of memory
 *
 * This routine is based on the observation that the relation beween the infrared value
 * ired and the red value red of an image point can be described by ired = b + a * ln (red).
 * First points are randomly sampled to calculate the linear regression coefficent a.
 * Then ired' = ired - a  * ln (red) is calculated for each pixel. Finally, the ir' image
 * is scaled between 0 and maximal value. For the logarithms a lookup table is used.
 * Negative films show very little spectral overlap but positive film usually has to be
 * cleaned. As we do a statistical measure of the film here dark margins and lumps of
 * dirt have to be excluded.
 *
 * @note original ired data are replaced by the cleaned ones
*/
extern SANE_Status
sanei_ir_spectral_clean (const SANE_Parameters * params, double *lut_ln,
			const SANE_Uint *red_data,
			SANE_Uint *ir_data);

/**
 * @brief Optimized mean filter
 *
 * @param[in]  params pointer to image description
 * @param[in]  in_img Pointer to grey scale image data
 * @param[out] out_img Pointer to grey scale image data
 * @param[in]  win_rows Height of filtering window, odd
 * @param[in]  win_cols Width of filtering window, odd
 *
 * @return
 * - SANE_STATUS_GOOD - success
 * - SANE_STATUS_NO_MEM - if out of memory
 * - SANE_STATUS_INVAL - wrong window size
 *
 * @note At the image margins the size of the filtering window
 *       is adapted. So there is no need to pad the image.
 * @note Memory for the output image has to be allocated before
 */
extern SANE_Status
sanei_ir_filter_mean (const SANE_Parameters * params,
		      const SANE_Uint *in_img, SANE_Uint *out_img,
		      int win_rows, int win_cols);


/**
 * @brief Find noise by adaptive thresholding
 *
 * @param[in]  params pointer to image description
 * @param[in]  in_img pointer to grey scale image
 * @param[out] out_img address of pointer to newly allocated binary image
 * @param[in] win_size Size of filtering window
 * @param[in]  a_val Parameter, below is definetly clean
 * @param[in]  b_val Parameter, above is definetly noisy
 *
 * @return
 * - SANE_STATUS_GOOD - success
 * - SANE_STATUS_NO_MEM - if out of memory
 *
 * This routine follows the concept of Crnojevic's MAD (median of the absolute deviations
 * from the median) filter. The first median filter step is replaced with a mean filter.
 * The dirty pixels which we wish to remove are always darker than the real signal. But
 * at high resolutions the scanner may generate some noise and the ired cleaning step can
 * reverse things. So a maximum filter will not do.
 * The second median is replaced by a mean filter to reduce computation time. Inspite of
 * these changes Crnojevic's recommendations for the choice of the parameters "a" and "b"
 * are still valid when scaled to the color depth.
 *
 * @reco Crnojevic recommends 10 < a_val < 30 and 50 < b_val < 100 for 8 bit color depth
 *
 * @note a_val, b_val are scaled by the routine according to bit depth
 * @note "0" in the mask output is regarded "dirty", 255 "clean"
 *
 * -# Crnojevic V. (2005) "Impulse Noise Filter with Adaptive Mad-Based Threshold"
 *      Proc. of the IEEE Int. Conf. on Image Processing, 3: 337-340
 */
extern SANE_Status
sanei_ir_filter_madmean (const SANE_Parameters * params,
			 const SANE_Uint *in_img,
			 SANE_Uint ** out_img, int win_size,
			 int a_val, int b_val);


/**
 * @brief Add dark pixels to mask from static threshold
 *
 * @param[in]  params pointer to image description
 * @param[in]  in_img pointer to grey scale image
 * @param      mask_img pointer to binary image (0, 255)
 * @param[in]  threshold below which the pixel is set 0
 */
void
sanei_ir_add_threshold (const SANE_Parameters * params,
			const SANE_Uint *in_img,
			SANE_Uint * mask_img, int threshold);


/**
 * @brief Calculates minimal Manhattan distances for an image mask
 *
 * @param[in]  params pointer to image description
 * @param[in]  mask_img pointer to binary image (0, 255)
 * @param[out] dist_map integer pointer to map of closest distances
 * @param[out] idx_map integer pointer to indices of closest pixels
 * @param[in]  erode == 0: closest pixel has value 0, != 0: is 255
 *
 * manhattan_dist takes a mask image consisting of 0 or 255  values. Given that
 * a 0 represents a dirty pixel and erode != 0, manhattan_dist will calculate the
 * shortest distance to a clean (255) pixel and record which pixel that was so
 * that the clean parts of the image can be dilated into the dirty ones. Thresholding
 * can be done on the distance. Conversely, if erode == 0 the distance of a clean
 * pixel to the closest dirty one is calculated which can be used to dilate the mask.
 *
 * @ref extended and C version of
 *      http://ostermiller.org/dilate_and_erode.html
 */
void
sanei_ir_manhattan_dist (const SANE_Parameters * params,
			const SANE_Uint * mask_img, unsigned int *dist_map,
			unsigned int *idx_map, unsigned int erode);


/**
 * @brief Dilate or erode a mask image
 *
 * @param[in]  params pointer to image description
 * @param      mask_img pointer to binary image (0, 255)
 * @param      dist_map integer pointer to map of closest distances
 * @param      idx_map integer pointer to indices of closest pixels
 * @param[in]  by number of pixels, > 0 dilate, < 0 erode
 *
 * @note by > 0 will enlarge the 0 valued area
 */
void
sanei_ir_dilate (const SANE_Parameters * params, SANE_Uint * mask_img,
		unsigned int *dist_map, unsigned int *idx_map, int by);

/**
 * @brief Suggest cropping for dark margins of positive film
 *
 * @param[in]  params pointer to image description
 * @param[in]  dist_map integer pointer to map of closest distances
 * @param[in]  inner crop within (!=0) or outside (==0) the image's edges
 * @param[out] edges pointer to array holding top, bottom, left
 *             and right edges
 *
 * The distance map as calculated by sanei_ir_manhattan_dist contains
 * distances to the next clean pixel. Dark margins are detected as dirt.
 * So the first/last rows/columns tell us how to crop. This is rather
 * fast if the distance map has been calculated anyhow.
 */
void
sanei_ir_find_crop (const SANE_Parameters * params,
                    unsigned int * dist_map, int inner, int * edges);

/**
 * @brief Dilate clean image parts into dirty ones and smooth int inner,
 *
 * @param[in] params pointer to image description
 * @param     in_img array of pointers to color planes of image
 * @param[in] mask_img pointer to dirt mask image
 * @param[in] dist_max threshold up to which dilation is done
 * @param[in] expand the dirt mask before replacing the pixels
 * @param[in] win_size size of adaptive mean filtering window
 * @param[in] smooth triangular filter whole image for grain removal
 * @param[in] inner find crop within or outside the image's edges
 * @param[out] crop array of 4 integers, if non-NULL, top, bottom,
 *             left and right values for cropping are returned.
 *
 * @return
 * - SANE_STATUS_GOOD - success
 * - SANE_STATUS_NO_MEM - if out of memory
 *
 * The main purpose of this routine is to replace dirty pixels.
 * As spin-off it obtains half of what is needed for film grain
 * smoothening and most of how to crop positive film.
 * To speed things up these functions are also implemented.
 */
SANE_Status
sanei_ir_dilate_mean (const SANE_Parameters * params,
                      SANE_Uint **in_img,
                      SANE_Uint *mask_img,
                      int dist_max, int expand, int win_size,
                      SANE_Bool smooth, int inner,
                      int *crop);


#endif /* not SANEI_IR_H */
