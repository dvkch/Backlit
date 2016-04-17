/* --------------------------------------------------------------------------------------------------------- */

/* umax.h - headerfile for SANE-backend for umax scanners
  
   (C) 1997-2002 Oliver Rauch

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
   If you do not wish that, delete this exception notice.
  
 */

/* --------------------------------------------------------------------------------------------------------- */


#ifndef umax_h
#define umax_h

#include "sys/types.h"

/* --------------------------------------------------------------------------------------------------------- */
/* COMPILER OPTIONS: */

#define UMAX_ENABLE_USB
#define UMAX_HIDE_UNUSED

/* #define SANE_UMAX_DEBUG_S12 */
/* #define UMAX_CALIBRATION_MODE_SELECTABLE */
                                         
/* --------------------------------------------------------------------------------------------------------- */

#define SANE_UMAX_SCSI_MAXQUEUE 		8

/* --------------------------------------------------------------------------------------------------------- */

#define SANE_UMAX_FIX_ROUND(val) ((SANE_Word) ((val) * (1 << SANE_FIXED_SCALE_SHIFT) + 1.0 / (1 << (SANE_FIXED_SCALE_SHIFT+1))))

/* --------------------------------------------------------------------------------------------------------- */

enum Umax_Option
{
    OPT_NUM_OPTS = 0,

    /* ------------------------------------------- */

    OPT_MODE_GROUP,
    OPT_MODE,
    OPT_SOURCE,
    OPT_X_RESOLUTION,
    OPT_Y_RESOLUTION,

    OPT_RESOLUTION_BIND,							  /* bind x and y resolution */
    OPT_NEGATIVE,

    /* ------------------------------------------- */

    OPT_GEOMETRY_GROUP,
    OPT_TL_X,										       /* top-left x */
    OPT_TL_Y,										       /* top-left y */
    OPT_BR_X,										   /* bottom-right x */
    OPT_BR_Y,										   /* bottom-right y */

    /* ------------------------------------------- */

    OPT_ENHANCEMENT_GROUP,

    OPT_BIT_DEPTH,								         /* output bit depth */
    OPT_QUALITY,								      /* quality calibration */
    OPT_DOR,									/* double optical resolution */
    OPT_WARMUP,
    OPT_RGB_BIND,					 /* use same rgb-values for each color in color-mode */

    OPT_BRIGHTNESS,
    OPT_CONTRAST,
    OPT_THRESHOLD,

    OPT_HIGHLIGHT,										/* highlight */
    OPT_HIGHLIGHT_R,
    OPT_HIGHLIGHT_G,
    OPT_HIGHLIGHT_B,

    OPT_SHADOW,										 	   /* shadow */
    OPT_SHADOW_R,
    OPT_SHADOW_G,
    OPT_SHADOW_B,

    OPT_ANALOG_GAMMA,									     /* analog gamma */
    OPT_ANALOG_GAMMA_R,
    OPT_ANALOG_GAMMA_G,
    OPT_ANALOG_GAMMA_B,

    OPT_CUSTOM_GAMMA,								  /* use custom gamma tables */
    				       /* The gamma vectors MUST appear in the order gray, red, green, blue. */
    OPT_GAMMA_VECTOR,
    OPT_GAMMA_VECTOR_R,
    OPT_GAMMA_VECTOR_G,
    OPT_GAMMA_VECTOR_B,

    OPT_HALFTONE_DIMENSION,
    OPT_HALFTONE_PATTERN,

    /* ------------------------------------------- */

    OPT_ADVANCED_GROUP,

    OPT_CAL_EXPOS_TIME,							    /* exposure time for calibration */
    OPT_CAL_EXPOS_TIME_R,
    OPT_CAL_EXPOS_TIME_G,
    OPT_CAL_EXPOS_TIME_B,
    OPT_SCAN_EXPOS_TIME,							   /* exposure time for scan */
    OPT_SCAN_EXPOS_TIME_R,
    OPT_SCAN_EXPOS_TIME_G,
    OPT_SCAN_EXPOS_TIME_B,

    OPT_DISABLE_PRE_FOCUS,
    OPT_MANUAL_PRE_FOCUS,
    OPT_FIX_FOCUS_POSITION,
    OPT_LENS_CALIBRATION_DOC_POS,
    OPT_HOLDER_FOCUS_POS_0MM,

    OPT_CAL_LAMP_DEN,
    OPT_SCAN_LAMP_DEN,

    OPT_SELECT_EXPOSURE_TIME,
    OPT_SELECT_CAL_EXPOSURE_TIME,
    OPT_SELECT_LAMP_DENSITY,

    OPT_LAMP_ON,
    OPT_LAMP_OFF,
    OPT_LAMP_OFF_AT_EXIT,

    OPT_BATCH_SCAN_START,							/* start batch scan function */
    OPT_BATCH_SCAN_LOOP,							 /* loop batch scan function */
    OPT_BATCH_SCAN_END,								  /* end batch scan function */
    OPT_BATCH_NEXT_TL_Y,						      /* batch scan function next y position */

#ifdef UMAX_CALIBRATION_MODE_SELECTABLE
    OPT_CALIB_MODE,
#endif

    OPT_PREVIEW,					/* preview, sets preview-bit and bind x/y-resolution */

    /* must come last: */
    NUM_OPTIONS
};


/* -------------------------------------------------------------------------------------------------------- */


/* LIST OF AVAILABLE SCANNERS, THE VALUES LISTED HERE ARE THE SAME FOR DIFFERENT APPLICATIONS
   THAT USE THE SAME DEVICE */

/* Umax_Device contains values relevant for the device that are not intersting for the sane interface */

typedef struct Umax_Device
{
  struct Umax_Device	*next;
  SANE_Device		sane;

  int connection_type;
#define SANE_UMAX_UNKNOWN 0
#define SANE_UMAX_SCSI 1
#define SANE_UMAX_USB 2

  SANE_Range		x_dpi_range;
  SANE_Range		y_dpi_range;
  SANE_Range		x_range;
  SANE_Range		y_range;
  SANE_Range		analog_gamma_range;
  unsigned		flags;

  unsigned char		*buffer[SANE_UMAX_SCSI_MAXQUEUE];		    /* buffer used for scsi-transfer */
  void                  *queue_id[SANE_UMAX_SCSI_MAXQUEUE];				    /* scsi queue id */
  size_t		length_queued[SANE_UMAX_SCSI_MAXQUEUE];			    /* length of queued data */
  size_t		length_read[SANE_UMAX_SCSI_MAXQUEUE];			  /* length of returned data */
  unsigned int		bufsize;
  unsigned int		row_bufsize;
  unsigned int		request_scsi_maxqueue;
  unsigned int		request_preview_lines;
  unsigned int		request_scan_lines;
  unsigned int		handle_bad_sense_error;
  unsigned int		execute_request_sense;
  unsigned int		force_preview_bit_rgb;
  unsigned int		scsi_buffer_size_min;
  unsigned int		scsi_buffer_size_max;
  unsigned int		scsi_maxqueue;

  unsigned char		*pixelbuffer;					   /* buffer used for pixel ordering */
  unsigned int		pixelline_max;				/* number of lines that fit into pixelbuffer */
  unsigned int		pixelline_ready[3];			    /* finished absolute line for each color */
  unsigned int		pixelline_next[3];				/* next line to write for each color */
  unsigned int		pixelline_del[3];			   /* next line to delete in opt_res for col */
  unsigned int		pixelline_optic[3];			   /* scanned line in opt_res for each color */
  unsigned int		pixelline_opt_res;			    /* number of scanned line in optical res */
  unsigned int		pixelline_read;					       /* number of read pixel-lines */
  unsigned int		pixelline_written;				    /* number of written pixel-lines */
  unsigned int		CCD_distance;				/* color line distance in optical resolution */
  unsigned int		CCD_color[9];							      /* color order */
										/* 0 / 1 2 / 3 4 5 / 6 7 / 8 */
#  define CCD_color_red   0
#  define CCD_color_green 1
#  define CCD_color_blue  2

  char			*devicename;					       /* name of the scanner device */
  int			sfd;					   /* output file descriptor, scanner device */

  char			vendor[9];							     /* will be UMAX */
  char			product[17];					      /* e.g. "SuperVista_S12" or so */
  char			version[5];								/* e.g. V1.3 */

  int			three_pass;					  /* used in RGB-mode if 3-pass => 1 */
  int			three_pass_color;			  /* select color for scanning in 3pass mode */
  unsigned int		row_len;					    /* len of one scan-line in bytes */
  unsigned int		lines_max;					  /* maximum number of lines to scan */
  unsigned int		max_value;							/* used for pnm-file */

  /* data defined by inquiry */
  int			inquiry_len;					   /* length of inquiry return block */
  int			inquiry_wdb_len;				/* length of window descriptor block */
  int			inquiry_vidmem;			     			     /* size of video memory */
  int			inquiry_optical_res;			     		       /* optical resolution */
  int			inquiry_x_res;						     /* maximum x-resolution */
  int			inquiry_y_res;						     /* maximum y-resolution */
  int			inquiry_dor_optical_res;			  /* optical resolution for dor mode */
  int			inquiry_dor_x_res;				/* maximum x-resolution for dor mode */
  int			inquiry_dor_y_res;				/* maximum y-resolution for dor mode */
  double		inquiry_fb_width;					  /* flatbed width in inches */
  double		inquiry_fb_length;					 /* flatbed length in inches */
  double		inquiry_uta_width;				     /* transparency width in inches */
  double		inquiry_uta_length;				    /* transparency length in inches */
  double		inquiry_uta_x_off;				  /* transparency x offset in inches */
  double		inquiry_uta_y_off;				  /* transparency y offset in inches */
  double		inquiry_dor_width;				/* double resolution width in inches */
  double		inquiry_dor_length;			       /* double resolution length in inches */
  double		inquiry_dor_x_off;			     /* double resolution x offset in inches */
  double		inquiry_dor_y_off;			     /* double resolution y offset in inches */

  int			inquiry_exposure_adj;				/* 1 if exposure adjust is supported */
  int			inquiry_exposure_time_step_unit;		  /* exposure time unit in micro sec */ 
  int			inquiry_exposure_time_max;				    /* exposure time maximum */
  int			inquiry_exposure_time_l_min;			/* exposure tine minimum for lineart */
  int			inquiry_exposure_time_l_fb_def;		/* exposure time default for lineart/flatbed */
  int			inquiry_exposure_time_l_uta_def;	    /* exposure time default for lineart/uta */
  int			inquiry_exposure_time_h_min;		       /* exposure tine minimum for halftone */
  int			inquiry_exposure_time_h_fb_def;	       /* exposure time default for halftone/flatbed */
  int			inquiry_exposure_time_h_uta_def;	   /* exposure time default for halftone/uta */
  int			inquiry_exposure_time_g_min;		      /* exposure tine minimum for grayscale */
  int			inquiry_exposure_time_g_fb_def;	      /* exposure time default for grayscale/flatbed */
  int			inquiry_exposure_time_g_uta_def;	  /* exposure time default for grayscale/uta */
  int			inquiry_exposure_time_c_min;			  /* exposure tine minimum for color */
  int			inquiry_exposure_time_c_fb_def_r;     /* exposure time default for color red/flatbed */
  int			inquiry_exposure_time_c_fb_def_g;   /* exposure time default for color green/flatbed */
  int			inquiry_exposure_time_c_fb_def_b;    /* exposure time default for color blue/flatbed */
  int			inquiry_exposure_time_c_uta_def_r;        /* exposure time default for color red/uta */
  int			inquiry_exposure_time_c_uta_def_g;      /* exposure time default for color green/uta */
  int			inquiry_exposure_time_c_uta_def_b;       /* exposure time default for color blue/uta */

  int			inquiry_max_warmup_time;			  /* maximum lamp warmup time in sec */
  int			inquiry_cbhs;						    /* 50, 255, 255+autoexp. */
  int			inquiry_cbhs_min;					   /* minimum value for cbhs */
  int			inquiry_cbhs_max;					   /* maximum value for cbhs */
  int			inquiry_contrast_min;					      /* minimum value for c */
  int			inquiry_contrast_max;					      /* maximum value for c */
  int			inquiry_brightness_min;					      /* minimum value for b */
  int			inquiry_brightness_max;					      /* maximum value for b */
  int			inquiry_threshold_min;					      /* minimum value for t */
  int			inquiry_threshold_max;					      /* maximum value for t */
  int			inquiry_highlight_min;					      /* minimum value for h */
  int			inquiry_highlight_max;					      /* maximum value for h */
  int			inquiry_shadow_min;					      /* minimum value for s */
  int			inquiry_shadow_max;					      /* maximum value for s */

  int			inquiry_quality_ctrl;						    /* 1 = supported */
  int			inquiry_batch_scan;						    /* 1 = supported */
  int			inquiry_preview;						    /* 1 = supported */
  int			inquiry_lamp_ctrl;						    /* 1 = supported */
  int			inquiry_transavail;						/* 1 = uta available */
  int			inquiry_adfmode;						/* 1 = adf available */
  int			inquiry_uta;							/* 1 = uta supported */
  int			inquiry_adf;							/* 1 = adf supported */
  int			inquiry_dor;							/* 1 = dor supported */
  int			inquiry_reverse;				      /* 1 = 1 bit reverse supported */
  int			inquiry_reverse_multi;				  /* 1 = multi bit reverse supported */
  int			inquiry_analog_gamma;				       /* 1 = analog gamma supported */
  int			inquiry_lineart_order;				     /* 1 = LSB first, 0 = MSB first */

  int			inquiry_lens_cal_in_doc_pos;		/* 1 = lens calibration in doc pos supported */
  int			inquiry_manual_focus;				       /* 1 = manual focus supported */
  int			inquiry_sel_uta_lens_cal_pos;   /* 1 = selection of lens calib pos for uta supported */

  int			inquiry_gamma_dwload;				     /* 1 = gamma download supported */
  int			inquiry_gamma_DCF;				      /* gamma download curve format */

  int			inquiry_one_pass_color;					     /* 1 = 1 pass supported */
  int			inquiry_three_pass_color;				     /* 1 = 3 pass supported */
  int			inquiry_color;						 /* 1 = color mode supported */
  int			inquiry_gray;					     /* 1 = grayscale mode supported */
  int			inquiry_halftone;				      /* 1 = halftone mode supported */
  int			inquiry_lineart;				       /* 1 = lineart mode supported */

  int			inquiry_calibration;			   /* 1 = calibration mode control supported */
  int			inquiry_highlight;					  /* 1 = highlight supported */
  int			inquiry_shadow;						     /* 1 = shadow supported */
  int			inquiry_GIB;							 /* gamma input bits */
  int			inquiry_GOB;						        /* gamma output bits */
  int			inquiry_max_calib_lines;				/* maximum calibration lines */
  int			inquiry_color_order;					   /* color ordering support */
  int			inquiry_CCD_line_distance;				      /* color line distance */
  int			inquiry_fb_uta_color_arrangement;		    /* line arrangement for fb & uta */
  int			inquiry_adf_color_arrangement;				 /* line arrangement for adf */

  unsigned int		relevant_optical_res;			     		       /* optical resolution */
  unsigned int		relevant_max_x_res;					     /* maximum x-resolution */
  unsigned int		relevant_max_y_res;					     /* maximum y-resolution */

  /* selected data */

  int			use_exposure_time_min;					   /*  exposure tine minimum */
  int			use_exposure_time_def_r;				    /* exposure time default */
  int			use_exposure_time_def_g;				    /* exposure time default */
  int			use_exposure_time_def_b;				    /* exposure time default */

  int			wdb_len;						   /* use this length of WDB */
  unsigned int		width_in_pixels;				 /* thats the wanted width in pixels */
  unsigned int		length_in_pixels;				/* thats the wanted length in pixels */
  unsigned int		scanwidth;		       /* thats the width in pixels at x_coordinate_base dpi */
  unsigned int		scanlength;		      /* thats the length in pixels at y_coordinate_base dpi */
  unsigned int		bytes_per_color;					     /* bytes per each color */

  unsigned int		x_resolution;					     /* scan-resolution for x in dpi */
  unsigned int		y_resolution;					     /* scan-resolution for y in dpi */
  double		scale_x;					  /* x-scaling of optical resolution */
  double		scale_y;					  /* y-scaling of optical resolution */
  int			upper_left_x;			     /* thats the left edge in points at 1200pt/inch */
  int			upper_left_y;			      /* thats the top edge in points at 1200pt/inch */

  unsigned int		x_coordinate_base;			      /* x base in pixels/inch, normaly 1200 */
  unsigned int		y_coordinate_base;			      /* y base in pixels/inch, normaly 1200 */

  unsigned int		bits_per_pixel;						 /* number of bits per pixel */
  int			bits_per_pixel_code;				/* 1 = 24bpp, 4 = 30 bpp, 8 = 36 bpp */
  int			gamma_input_bits_code;				/* 1 = 24bpp, 4 = 30 bpp, 8 = 36 bpp */
  int			set_auto;					    /* 0 or 1, don't know what it is */
  int			preview;							     /* 1 if preview */
  int			batch_scan;					  /* 1 = batch scan, 0 = normal scan */
  int			batch_end;						  /* 1 = reposition scanhead */
  int			batch_next_tl_y;			  /* top left y position for next batch scan */
  int			quality;					/* 1 = quality_calibration, 0 = fast */
  int			reverse;					      /* 1: exchange black and white */
  int			reverse_multi;						   /* 1: invert color values */
  int			WD_speed;				       /* is a combination of slow and smear */
  int			slow;							       /* 1: slow scan speed */
  int			smear;				       /* 1: don't care about image smearing problem */
  int			dor;								/* double resolution */
  int			cbhs_range;					       /* 50,255 or 255+autoexposure */
  int			fix_focus_position;					       /* fix focus position */
  int			lens_cal_in_doc_pos;			    /* lens calibration in document position */
  int			disable_pre_focus;						/* disable pre focus */
  int			holder_focus_pos_0mm;			    /* 0.6mm <-> 0.0mm holder focus position */
  int			manual_focus;					       /* automatic <-> manual focus */
  int			warmup;								 /* 1=set warmup-bit */
  int			module;							  /* flatbed or transparency */
  int			adf;							       /* 1 if ADF turned on */
  int			uta;							       /* 1 if UTA turned on */
  int			calibration;			      /* calibration :0=ignore, 1=driver, 2=by image */
  int			low_byte_first;			 /* 10 bit mode: 0=high byte frist, 1=low byte frist */
  int			colormode;				     /* LINEART, HALFTONE, GRAYSCALE or RGB  */
#  define LINEART             1
#  define HALFTONE            2
#  define GRAYSCALE           3
#  define RGB_LINEART         4
#  define RGB_HALFTONE        5
#  define RGB                 6

  int			exposure_time_calibration_r;			/* red exposure time for calibration */
  int			exposure_time_calibration_g;		      /* green exposure time for calibration */
  int			exposure_time_calibration_b;		       /* blue exposure time for calibration */
  int			exposure_time_scan_r;				       /* red exposure time for scan */
  int			exposure_time_scan_g;				     /* green exposure time for scan */
  int			exposure_time_scan_b;				       /* bue exposure time for scan */

  int			c_density;					    /* next calibration lamp density */
  int			s_density;						   /* next scan lamp density */

  int			threshold;						/* (128) 0-255, lineart mode */
  int			brightness;				    /* (128) cbhs_range 0-255, halftone mode */
  int			contrast;				    /* (128) cbhs_range 0-255, halftone-mode */
  int			highlight_r;					/* (255) cbhs_range 1-255, each mode */
  int			highlight_g;					/* (255) cbhs_range 1-255, each mode */
  int			highlight_b;					/* (255) cbhs_range 1-255, each mode */
  int			shadow_r;					  /* (0) cbhs_range 0-254, each mode */
  int			shadow_g;					  /* (0) cbhs_range 0-254, each mode */
  int			shadow_b;					  /* (0) cbhs_range 0-254, each mode */
  int			halftone;						  /* halftone pattern select */

  int			digital_gamma_r;				    /* gamma-select for red and gray */
  int			digital_gamma_g;				     /* gamma-select value for green */
  int			digital_gamma_b;				      /* gamma-select value for blue */

  int			analog_gamma_r;						/* analog gamma red and gray */
  int			analog_gamma_g;						       /* analog gamma green */
  int			analog_gamma_b;							/* analog gamma blue */

  int			calib_lines;						/* request calibration lines */

  int			do_calibration;					      /* 1: do calibration by driver */
  int			do_color_ordering;				 /* 1: order line-mode to pixel-mode */

  int			button0_pressed;			 /* scan-button 0 on scanner is pressed => 1 */
  int			button1_pressed;			 /* scan-button 1 on scanner is pressed => 1 */
  int			button2_pressed;			 /* scan-button 2 on scanner is pressed => 1 */

  int			calibration_area;		      /* define calibration area if no area is given */
  int                   calibration_width_offset;  /* some scanners do calibrate with some additional pixels */
  int                   calibration_width_offset_batch;			      /* the same for batch scanning */
  int                   calibration_bytespp;		   /* correction of bytespp if driver knows about it */
  int                   exposure_time_rgb_bind;		  /* exposure time can not be defined for each color */
  int			invert_shading_data;	     /* invert shading data before sending it to the scanner */
  int                   common_xy_resolutions;			/* do not allow different x and y resolution */
  int			pause_for_color_calibration;	/* pause between start_scan and do_calibration in ms */
  int			pause_for_gray_calibration;	/* pause between start_scan and do_calibration in ms */
  int			pause_after_calibration;	 /* pause between do_calibration and read data in ms */
  int			pause_after_reposition;				    /* pause for repositioning in ms */
  int			pause_for_moving;	       /* pause for moving scanhead over full scanarea in ms */
  int			lamp_control_available;		       /* is set when scanner supportes lamp control */
  int			gamma_lsb_padded;                              /* 16 bit gamma data is padded to lsb */
  int			force_quality_calibration;			   /* always set quality calibration */
} Umax_Device;


/* --------------------------------------------------------------------------------------------------------- */


/* LIST OF OPEND DEVICES, A DEVICE MAY BE OPEND TWICE, ALL VALUES LISTED HERE MAY BE
   DIFFERENT FOR DIFFERENT APPLICATIONS */

/* Umax_Scanner contains values relevant for the sane interface */

typedef struct Umax_Scanner
{
  struct Umax_Scanner		*next;
  Umax_Device			*device;

  SANE_Option_Descriptor	opt[NUM_OPTIONS];
  Option_Value			val[NUM_OPTIONS];
  SANE_Int			*gamma_table[4];
  SANE_Int			halftone_pattern[64];
  SANE_Range			gamma_range;
  unsigned int			gamma_length;
  SANE_Range			output_range;
  unsigned int			output_bytes;
  SANE_Range			exposure_time_range;

  int				scanning;
  SANE_Parameters		params;

  SANE_Pid			reader_pid;
  int				pipe_read_fd;
  int				pipe_write_fd;
} Umax_Scanner;


/* --------------------------------------------------------------------------------------------------------- */


#endif /* umax-sane_h */
