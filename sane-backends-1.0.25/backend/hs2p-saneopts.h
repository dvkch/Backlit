/* sane - Scanner Access Now Easy.
   Copyright (C) 2007 Jeremy Johnson
   This file is part of a SANE backend for Ricoh IS450
   and IS420 family of HS2P Scanners using the SCSI controller.
   
   This file is part of the SANE package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.

   As a special exception, the authors of SANE give permission for
   additional uses of the libraries contained in this release of SANE.

   The exception is that, if you link a SANE library with other files
   to produce an executable, this does not by itself cause the
   resulting executable to be covered by the GNU General Public
   License.  Your use of that executable is in no way restricted on
   account of linking the SANE library code into it.

   This exception does not, however, invalidate any other reasons why
   the executable file might be covered by the GNU General Public
   License.

   If you submit changes to SANE to the maintainers to be included in
   a subsequent release, you agree by submitting the changes that
   those changes may be distributed with this exception intact.

   If you write modifications of your own for SANE, it is your choice
   whether to permit this exception to apply to your modifications.
   If you do not wish that, delete this exception notice. */


#define SANE_NAME_INQUIRY  "inquiry"
#define SANE_TITLE_INQUIRY "Inquiry Data"
#define SANE_DESC_INQUIRY  "Displays scanner inquiry data"

#define SANE_TITLE_SCAN_MODE_GROUP "Scan Mode"
#define SANE_TITLE_GEOMETRY_GROUP "Geometry"
#define SANE_TITLE_FEEDER_GROUP "Feeder"

#define SANE_TITLE_ENHANCEMENT_GROUP "Enhancement"
#define SANE_TITLE_ICON_GROUP "Icon"
#define SANE_TITLE_BARCODE_GROUP "Barcode"

#define SANE_TITLE_MISCELLANEOUS_GROUP "Miscellaneous"

#define SANE_NAME_AUTOBORDER "autoborder"
#define SANE_TITLE_AUTOBORDER "Autoborder"
#define SANE_DESC_AUTOBORDER "Enable Automatic Border Detection"

#define SANE_NAME_COMPRESSION "compression"
#define SANE_TITLE_COMPRESSION "Data Compression"
#define SANE_DESC_COMPRESSION "Sets the compression mode of the scanner"

#define SANE_NAME_ROTATION "rotation"
#define SANE_TITLE_ROTATION "Page Rotation"
#define SANE_DESC_ROTATION "Sets the page rotation mode of the scanner"

#define SANE_NAME_DESKEW "deskew"
#define SANE_TITLE_DESKEW "Page Deskew"
#define SANE_DESC_DESKEW "Enable Deskew Mode"

#define SANE_NAME_TIMEOUT_ADF "timeout-adf"
#define SANE_TITLE_TIMEOUT_ADF "ADF Timeout"
#define SANE_DESC_TIMEOUT_ADF "Sets the timeout in seconds for the ADF"

#define SANE_NAME_TIMEOUT_MANUAL "timeout-manual"
#define SANE_TITLE_TIMEOUT_MANUAL "Manual Timeout"
#define SANE_DESC_TIMEOUT_MANUAL "Sets the timeout in seconds for manual feeder"

#define SANE_NAME_BATCH "batch"
#define SANE_TITLE_BATCH "Batch"
#define SANE_DESC_BATCH "Enable Batch Mode"

#define SANE_NAME_CHECK_ADF "check-adf"
#define SANE_TITLE_CHECK_ADF "Check ADF"
#define SANE_DESC_CHECK_ADF "Check ADF Status prior to starting scan"

#define SANE_NAME_PREFEED "prefeed"
#define SANE_TITLE_PREFEED "Prefeed"
#define SANE_DESC_PREFEED "Prefeed"

#define SANE_NAME_DUPLEX "duplex"
#define SANE_TITLE_DUPLEX "Duplex"
#define SANE_DESC_DUPLEX "Enable Duplex (Dual-Sided) Scanning"

#define SANE_NAME_ENDORSER  "endorser"
#define SANE_TITLE_ENDORSER "Endorser"
#define SANE_DESC_ENDORSER  "Print up to 19 character string on each sheet"

#define SANE_NAME_ENDORSER_STRING  "endorser-string"
#define SANE_TITLE_ENDORSER_STRING "Endorser String"
#define SANE_DESC_ENDORSER_STRING  "valid characters: [0-9][ :#`'-./][A-Z][a-z]"

#define SANE_NAME_BARCODE_SEARCH_COUNT "barcode-search-count"
#define SANE_TITLE_BARCODE_SEARCH_COUNT "Barcode Search Count"
#define SANE_DESC_BARCODE_SEARCH_COUNT "Number of barcodes to search for in the scanned image"

#define SANE_NAME_BARCODE_HMIN "barcode-hmin"
#define SANE_TITLE_BARCODE_HMIN "Barcode Minimum Height"
#define SANE_DESC_BARCODE_HMIN "Sets the Barcode Minimun Height (larger values increase recognition speed)"

#define SANE_NAME_BARCODE_SEARCH_MODE "barcode-search-mode"
#define SANE_TITLE_BARCODE_SEARCH_MODE "Barcode Search Mode"
#define SANE_DESC_BARCODE_SEARCH_MODE "Chooses the orientation of barcodes to be searched"

#define SANE_NAME_BARCODE_SEARCH_TIMEOUT "barcode-search-timeout"
#define SANE_TITLE_BARCODE_SEARCH_TIMEOUT "Barcode Search Timeout"
#define SANE_DESC_BARCODE_SEARCH_TIMEOUT "Sets the timeout for barcode searching"

#define SANE_NAME_BARCODE_SEARCH_BAR "barcode-search-bar"
#define SANE_TITLE_BARCODE_SEARCH_BAR "Barcode Search Bar"
#define SANE_DESC_BARCODE_SEARCH_BAR "Specifies the barcode type to search for"

#define SANE_NAME_SECTION "section"
#define SANE_TITLE_SECTION "Image/Barcode Search Sections"
#define SANE_DESC_SECTION "Specifies an image section and/or a barcode search region"

#define SANE_NAME_BARCODE_RELMAX "barcode-relmax"
#define SANE_TITLE_BARCODE_RELMAX "Barcode RelMax"
#define SANE_DESC_BARCODE_RELMAX "Specifies the maximum relation from the widest to the smallest bar"

#define SANE_NAME_BARCODE_BARMIN "barcode-barmin"
#define SANE_TITLE_BARCODE_BARMIN "Barcode Bar Minimum"
#define SANE_DESC_BARCODE_BARMIN "Specifies the minimum number of bars in Bar/Patch code"

#define SANE_NAME_BARCODE_BARMAX "barcode-barmax"
#define SANE_TITLE_BARCODE_BARMAX "Barcode Bar Maximum"
#define SANE_DESC_BARCODE_BARMAX "Specifies the maximum number of bars in a Bar/Patch code"

#define SANE_NAME_BARCODE_CONTRAST "barcode-contrast"
#define SANE_TITLE_BARCODE_CONTRAST "Barcode Contrast"
#define SANE_DESC_BARCODE_CONTRAST "Specifies the image contrast used in decoding.  Use higher values when " \
"there are more white pixels in the code"

#define SANE_NAME_BARCODE_PATCHMODE "barcode-patchmode"
#define SANE_TITLE_BARCODE_PATCHMODE "Barcode Patch Mode"
#define SANE_DESC_BARCODE_PATCHMODE "Controls Patch Code detection."

#define SANE_NAME_SCAN_WAIT_MODE "scan-wait-mode"
#define SANE_TITLE_SCAN_WAIT_MODE "Scan Wait Mode "
#define SANE_DESC_SCAN_WAIT_MODE "Enables the scanner's start button"

#define SANE_NAME_ACE_FUNCTION "ace-function"
#define SANE_TITLE_ACE_FUNCTION "ACE Function"
#define SANE_DESC_ACE_FUNCTION "ACE Function"

#define SANE_NAME_ACE_SENSITIVITY "ace-sensitivity"
#define SANE_TITLE_ACE_SENSITIVITY "ACE Sensitivity"
#define SANE_DESC_ACE_SENSITIVITY "ACE Sensitivity"

#define SANE_NAME_ICON_WIDTH "icon-width"
#define SANE_TITLE_ICON_WIDTH "Icon Width"
#define SANE_DESC_ICON_WIDTH "Width of icon (thumbnail) image in pixels"

#define SANE_NAME_ICON_LENGTH "icon-length"
#define SANE_TITLE_ICON_LENGTH "Icon Length"
#define SANE_DESC_ICON_LENGTH "Length of icon (thumbnail) image in pixels"

#define SANE_NAME_ORIENTATION "orientation"
#define SANE_TITLE_ORIENTATION "Paper Orientation"
#define SANE_DESC_ORIENTATION "[Portrait]/Landscape" \

#define SANE_NAME_PAPER_SIZE "paper-size"
#define SANE_TITLE_PAPER_SIZE "Paper Size"
#define SANE_DESC_PAPER_SIZE "Specify the scan window geometry by specifying the paper size " \
"of the documents to be scanned"

#define SANE_NAME_PADDING "padding"
#define SANE_TITLE_PADDING "Padding"
#define SANE_DESC_PADDING "Pad if media length is less than requested"

#define SANE_NAME_AUTO_SIZE "auto-size"
#define SANE_TITLE_AUTO_SIZE "Auto Size"
#define SANE_DESC_AUTO_SIZE "Automatic Paper Size Determination"

#define SANE_NAME_BINARYFILTER "binary-filter"
#define SANE_TITLE_BINARYFILTER "Binary Filter"
#define SANE_DESC_BINARYFILTER "Binary Filter"

#define SANE_NAME_SMOOTHING "smoothing"
#define SANE_TITLE_SMOOTHING "Smoothing"
#define SANE_DESC_SMOOTHING "Binary Smoothing Filter"

#define SANE_NAME_NOISEREMOVAL "noise-removal"
#define SANE_TITLE_NOISEREMOVAL "Noise Removal"
#define SANE_DESC_NOISEREMOVAL "Binary Noise Removal Filter"

#define SANE_NAME_NOISEMATRIX "noise-removal-matrix"
#define SANE_TITLE_NOISEMATRIX "Noise Removal Matrix"
#define SANE_DESC_NOISEMATRIX "Noise Removal Matrix"

#define SANE_NAME_GRAYFILTER "gray-filter"
#define SANE_TITLE_GRAYFILTER "Gray Filter"
#define SANE_DESC_GRAYFILTER "Gray Filter"

#define SANE_NAME_HALFTONE_CODE "halftone-type"
#define SANE_TITLE_HALFTONE_CODE "Halftone Type"
#define SANE_DESC_HALFTONE_CODE  "Dither or Error Diffusion"

/*
#define SANE_NAME_HALFTONE_PATTERN "pattern"
#define SANE_TITLE_HALFTONE_PATTERN "Pattern"
#define SANE_DESC_HALFTONE_PATTERN  "10 built-in halftone patterns + 2 user patterns"
*/

#define SANE_NAME_ERRORDIFFUSION "error-diffusion"
#define SANE_TITLE_ERRORDIFFUSION "Error Diffusion"
#define SANE_DESC_ERRORDIFFUSION  "Useful for documents with both text and images"

/*
#define SANE_NAME_HALFTONE "halftone"
#define SANE_TITLE_HALFTONE "Halftone"
#define SANE_DESC_HALFTONE "Choose a dither pattern or error diffusion"

#define SANE_NAME_NEGATIVE "negative image"
#define SANE_TITLE_NEGATIVE "Negative Image"
#define SANE_DESC_NEGATIVE "Reverse Image Format"

#define SANE_NAME_BRIGHTNESS "brightness"
#define SANE_TITLE_BRIGHTNESS "Brightness"
#define SANE_DESC_BRIGHTNESS "Brightness"

#define SANE_NAME_THRESHOLD "threshold"
#define SANE_TITLE_THRESHOLD "Threshold"
#define SANE_DESC_THRESHOLD "Threshold"
*/

#define SANE_NAME_GAMMA "gamma"
#define SANE_TITLE_GAMMA "Gamma"
#define SANE_DESC_GAMMA "Gamma Correction"

#define SANE_NAME_AUTOSEP "auto-separation"
#define SANE_TITLE_AUTOSEP "Automatic Separation"
#define SANE_DESC_AUTOSEP "Automatic Separation"

#define SANE_NAME_AUTOBIN "auto-binarization"
#define SANE_TITLE_AUTOBIN "Automatic Binarization"
#define SANE_DESC_AUTOBIN "Automatic Binarization"

#define SANE_NAME_WHITE_BALANCE "white-balance"
#define SANE_TITLE_WHITE_BALANCE "White Balance"
#define SANE_DESC_WHITE_BALANCE  "White Balance"

#define SANE_NAME_PADDING_TYPE "padding-type"
#define SANE_TITLE_PADDING_TYPE "Padding Type"
#define SANE_DESC_PADDING_TYPE  "Padding Type"

#define SANE_NAME_BITORDER "bit-order"
#define SANE_TITLE_BITORDER "Bit Order"
#define SANE_DESC_BITORDER  "Bit Order"

#define SANE_NAME_SELF_DIAGNOSTICS "self-diagnostics"
#define SANE_TITLE_SELF_DIAGNOSTICS "Self Diagnostics"
#define SANE_DESC_SELF_DIAGNOSTICS "Self Diagnostics"

#define SANE_NAME_OPTICAL_ADJUSTMENT "optical-adjustment"
#define SANE_TITLE_OPTICAL_ADJUSTMENT "Optical Adjustment"
#define SANE_DESC_OPTICAL_ADJUSTMENT "Optical Adjustment"

typedef enum
{
  OPT_NUM_OPTS = 0,

  OPT_MODE_GROUP,
  OPT_INQUIRY,			/* inquiry string */
  OPT_PREVIEW,
  OPT_SCAN_MODE,		/* scan mode */
  OPT_RESOLUTION,
  OPT_X_RESOLUTION,
  OPT_Y_RESOLUTION,
  OPT_COMPRESSION,		/* hardware compression */

  OPT_GEOMETRY_GROUP,
  /*OPT_AUTOBORDER,       automatic border detection */
  /*OPT_ROTATION,         hardware rotation */
  /*OPT_DESKEW,           hardware deskew */
  OPT_PAGE_ORIENTATION,		/* portrait, landscape */
  OPT_PAPER_SIZE,		/* paper size */
  OPT_TL_X,			/* top-left x */
  OPT_TL_Y,			/* top-left y */
  OPT_BR_X,			/* bottom-right x */
  OPT_BR_Y,			/* bottom-right y */
  OPT_PADDING,			/* Pad to requested length */
  OPT_AUTO_SIZE,		/* Automatic Size Recognition */

  OPT_FEEDER_GROUP,
  OPT_SCAN_SOURCE,		/* scan source (eg. Flatbed, ADF) */
  OPT_DUPLEX,			/* scan both sides of the page */
  OPT_SCAN_WAIT_MODE,		/* Enables the scanner's Start Button */
  OPT_PREFEED,
  OPT_ENDORSER,			/* Endorser (off,on) */
  OPT_ENDORSER_STRING,		/* Endorser String */
  /*OPT_BATCH,              scan in batch mode */
  /*OPT_TIMEOUT_MANUAL,     timeout in seconds with manual feed */
  /*OPT_TIMEOUT_ADF,        timeout in seconds with ADF */
  /*OPT_CHECK_ADF,          check for page in ADF before scanning */

  OPT_ENHANCEMENT_GROUP,
  /* OPT_ACE_FUNCTION,  
     OPT_ACE_SENSITIVITY, */
  OPT_BRIGHTNESS,		/* Brightness */
  OPT_THRESHOLD,		/* Threshold */
  OPT_CONTRAST,			/* Contrast */
  OPT_NEGATIVE,			/* Negative (reverse image) */
  OPT_GAMMA,			/* Gamma Correction */
  OPT_CUSTOM_GAMMA,
  OPT_GAMMA_VECTOR_GRAY,
  OPT_HALFTONE_CODE,		/* Halftone Code    */
  OPT_HALFTONE_PATTERN,		/* Halftone Pattern */
  OPT_GRAYFILTER,		/* MRIF */
  OPT_SMOOTHING,		/* Smoothing */
  OPT_NOISEREMOVAL,		/* Noise Removal */
  OPT_AUTOSEP,			/* Auto Separation */
  OPT_AUTOBIN,			/* Auto Binarization */
  OPT_WHITE_BALANCE,

  OPT_MISCELLANEOUS_GROUP,
  OPT_PADDING_TYPE,
  /*OPT_BITORDER,      */
  OPT_SELF_DIAGNOSTICS,
  OPT_OPTICAL_ADJUSTMENT,
  /*
     OPT_PARITION_FUNCTION
     OPT_SECTION
   */

  OPT_DATA_GROUP,
  OPT_UPDATE,
  OPT_NREGX_ADF,
  OPT_NREGY_ADF,
  OPT_NREGX_BOOK,
  OPT_NREGY_BOOK,
  OPT_NSCANS_ADF,
  OPT_NSCANS_BOOK,
  OPT_LAMP_TIME,
  OPT_EO_ODD,
  OPT_EO_EVEN,
  OPT_BLACK_LEVEL_ODD,
  OPT_BLACK_LEVEL_EVEN,
  OPT_WHITE_LEVEL_ODD,
  OPT_WHITE_LEVEL_EVEN,
  OPT_DENSITY,
  OPT_FIRST_ADJ_WHITE_ODD,
  OPT_FIRST_ADJ_WHITE_EVEN,
  OPT_NREGX_REVERSE,
  OPT_NREGY_REVERSE,
  OPT_NSCANS_REVERSE_ADF,
  OPT_REVERSE_TIME,
  OPT_NCHARS,

  NUM_OPTIONS			/* must come last: */
} HS2P_Option;
