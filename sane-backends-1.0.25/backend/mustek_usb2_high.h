/* sane - Scanner Access Now Easy.

   Copyright (C) 2005 Mustek.
   Originally maintained by Mustek
   Author:Jack Roy 2005.5.24

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
   If you do not wish that, delete this exception notice.

   This file implements a SANE backend for the Mustek BearPaw 2448 TA Pro 
   and similar USB2 scanners. */

#ifndef MUSTEK_USB2_HIGH_H
#define MUSTEK_USB2_HIGH_H

/* const use in structures*/

/*scan mode*/
typedef unsigned short SCANMODE, *LPSCANMODE;
#define SM_TEXT		0x00
#define SM_GRAY		0x01
#define SM_RGB24	0x02
#define SM_GRAY10	0x03
#define SM_RGB30	0x04
#define SM_GRAY12	0x05
#define SM_RGB36	0x06
#define SM_GRAY14	0x07
#define SM_RGB42	0x08
#define SM_GRAY16	0x09
#define SM_RGB48	0x0a

/*pixel flavor*/
typedef SANE_Byte PIXELFLAVOR, *LPPIXELFLAVOR;
#define PF_BlackIs0 0x00
#define PF_WhiteIs0 0x01

/*scan source*/
typedef SANE_Byte SCANSOURCE, *LPSCANSOURCE;
#define SS_Reflective	0x00
#define SS_Positive		0x01
#define SS_Negative		0x02
#define SS_ADF			0x03

/*RGB order*/
typedef unsigned short RGBORDER, *LPRGBORDER;
#define RO_RGB 0x00
#define RO_BGR 0x01

/* structures use in parameters of export function*/

typedef struct tagGAMMAINFO
{
  SCANMODE smScanMode;
  unsigned short wInputGammaBits;
  unsigned short wOutputGammaBits;
} GAMMAINFO, *LPGAMMAINFO;

typedef struct tagGETPARAMETERS
{
  unsigned short wSourceXDPI;
  unsigned short wSourceYDPI;
  unsigned int dwLineByteWidth;
  unsigned int dwLength;
} GETPARAMETERS, *LPGETPARAMETERS;

typedef struct tagFRAME
{
  unsigned short x1;
  unsigned short y1;
  unsigned short x2;
  unsigned short y2;
} FRAME, *LPFRAME;

typedef struct tagSETPARAMETERS
{
  FRAME fmArea;
  unsigned short wTargetDPI;
  SCANMODE smScanMode;
  unsigned short wLinearThreshold;	/*threshold for Line art mode */
  PIXELFLAVOR pfPixelFlavor;
  SCANSOURCE ssScanSource;
  unsigned short * pGammaTable;
} SETPARAMETERS, *LPSETPARAMETERS;

typedef struct tagIMAGEROWS
{
  RGBORDER roRgbOrder;
  unsigned short wWantedLineNum;
  unsigned short wXferedLineNum;
  SANE_Byte * pBuffer;
} IMAGEROWS, *LPIMAGEROWS;


/*Macro define*/

#define R_GAIN							0
#define G_GAIN                          0
#define B_GAIN                          0
#define R_OFFSET                        0
#define G_OFFSET                        0
#define B_OFFSET                        0
#define R_DIRECTION                     0
#define G_DIRECTION                     0
#define B_DIRECTION                     0

/* use for adjust AD's offset*/

/* for Reflective*/
#define REFL_DARK_MAX_LEVEL             20
#define REFL_DARK_MIN_LEVEL             10
#define REFL_WHITE_MAX_LEVEL			220
#define REFL_WHITE_MIN_LEVEL			210
#define REFL_MAX_LEVEL_RANGE            210
#define REFL_MIN_LEVEL_RANGE            190

/*for Transparent*/
#define TRAN_DARK_MAX_LEVEL             20
#define TRAN_DARK_MIN_LEVEL             10
#define TRAN_WHITE_MAX_LEVEL            220
#define TRAN_WHITE_MIN_LEVEL            210
#define TRAN_MAX_LEVEL_RANGE            210
#define TRAN_MIN_LEVEL_RANGE            190


/* in 600 dpi*/
#define FIND_LEFT_TOP_WIDTH_IN_DIP          512
#define FIND_LEFT_TOP_HEIGHT_IN_DIP         180
#define FIND_LEFT_TOP_CALIBRATE_RESOLUTION  600

#define TA_FIND_LEFT_TOP_WIDTH_IN_DIP       2668
#define TA_FIND_LEFT_TOP_HEIGHT_IN_DIP      300

#define TA_MOTOR_BACK_STEP_AFTER_FIND_BOUNDARY             150
#define TA_MOTOR_FORWARD_STEP_AFTER_READ_WHITE_DATA        1100

/*must be 8x*/
#define LINE_CALIBRATION__16BITS_HEIGHT				40

/* the length from block bar to start Calibration position*/
#define BEFORE_SCANNING_MOTOR_FORWARD_PIXEL 40

#define PRE_MOVE_MOTOR_LENGTH_IN_DPI    1450

/* if the motor is 1/8 step, setup MOTOR_STEP_MULTI as 8
 if the motor is 1/4 step, setup MOTOR_STEP_MULTI as 4
 if the motor is full step, setup MOTOR_STEP_MULTI as 1
#define MOTOR_EIGHTH_STEP*/
#ifdef MOTOR_EIGHTH_STEP
#define MOTOR_STEP_MULTI                8
#define GPIO_95_Config                  0x68
#else
#define MOTOR_STEP_MULTI                4

#define GPIO_95_Config                  0x60
#endif

#define TRAN_START_POS					4550

/* in 300dpi*/
#define MAX_SCANNING_WIDTH               2550	/*just for A4 */
#define MAX_SCANNING_HEIGHT              3540	/*just for A4 */

#define INIFILENAME                         "./msam.ini"

/*enable gamma*/
#define ENABLE_GAMMA

/*save debug image*/
/*#define DEBUG_SAVE_IMAGE*/

/*type define*/
typedef unsigned char SCANTYPE;
#define ST_Reflective	0x00
#define ST_Transparent	0x01

typedef enum tagCOLORMODE
{
  CM_RGB48 = 0,
  CM_RGB42 = 1,
  CM_RGB36 = 2,
  CM_RGB30 = 3,
  CM_RGB24 = 4,
  CM_GRAY16 = 5,
  CM_GRAY14 = 6,
  CM_GRAY12 = 7,
  CM_GRAY10 = 8,
  CM_GRAY8 = 9,
  CM_TEXT = 10,
  CM_RGB48ext = 11,
  CM_RGB42ext = 12,
  CM_RGB36ext = 13,
  CM_RGB30ext = 14,
  CM_RGB24ext = 15,
  CM_GRAY16ext = 16,
  CM_GRAY14ext = 17,
  CM_GRAY12ext = 18,
  CM_GRAY10ext = 19,
  CM_GRAY8ext = 20,
  CM_TEXText = 21
} COLORMODE, *PCOLORMODE;

typedef struct tagTARGETIMAGE
{
  SANE_Bool isOptimalSpeed;
  COLORMODE cmColorMode;
  unsigned short wDpi;
  unsigned short wX;
  unsigned short wY;
  unsigned short wWidth;
  unsigned short wHeight;
  SANE_Byte bScanSource;
} TARGETIMAGE, *PTARGETIMAGE;

typedef struct tagSUGGESTSETTING
{
  COLORMODE cmScanMode;
  unsigned short wXDpi;
  unsigned short wYDpi;
  unsigned short wX;
  unsigned short wY;
  unsigned short wWidth;
  unsigned short wHeight;
  unsigned int dwBytesPerRow;
} SUGGESTSETTING, *PSUGGESTSETTING;



#endif
