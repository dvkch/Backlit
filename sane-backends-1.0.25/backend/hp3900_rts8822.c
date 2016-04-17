/* HP Scanjet 3900 series - RTS8822 Core

   Copyright (C) 2005-2013 Jonathan Bravo Lopez <jkdsoft@gmail.com>

   This file is part of the SANE package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

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


/*
 This code is still a bit ugly due to it's the result of applying
 reverse engineering techniques to windows driver. So at this
 moment what you see is exactly what windows driver does.
 And so many global vars exist that will be erased when driver
 is entirely debugged. There are some variables with unknown
 purpose. So they have no meaning name in form v+address. I
 hope to change their names when driver is debugged completely.
*/

#ifndef RTS8822_CORE

#define RTS8822_CORE

#define GetTickCount() (time(0) * 1000)
#define min(A,B) (((A)<(B)) ? (A) : (B))
#define max(A,B) (((A)>(B)) ? (A) : (B))
#define PIXEL_TO_MM(_pixel_, _dpi_) ((_pixel_) * 25.4 / (_dpi_))
#define MM_TO_PIXEL(_mm_, _dpi_) ((_mm_) * (_dpi_) / 25.4)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>		/* bzero()   */
#include <time.h>		/* clock()   */
#include <math.h>		/* truncf()  */
#include <ctype.h>		/* tolower() */
#include <unistd.h>		/* usleep()  */
#include <sys/types.h>

#include "hp3900_types.c"
#include "hp3900_debug.c"
#include "hp3900_config.c"
#include "hp3900_usb.c"

/*-------------------- Exported function headers --------------------*/

#ifdef developing
static SANE_Int hp4370_prueba (struct st_device *dev);
static void prueba (SANE_Byte * a);
void shadingtest1 (struct st_device *dev, SANE_Byte * Regs,
		   struct st_calibration *myCalib);
static SANE_Int Calib_test (struct st_device *dev, SANE_Byte * Regs,
			    struct st_calibration *myCalib,
			    struct st_scanparams *scancfg);
static SANE_Int Calib_BlackShading_jkd (struct st_device *dev,
					SANE_Byte * Regs,
					struct st_calibration *myCalib,
					struct st_scanparams *scancfg);
#endif

/*static void show_diff(struct st_device *dev, SANE_Byte *original);*/

/* functions to allocate and free space for a device */
static struct st_device *RTS_Alloc (void);
static void RTS_Free (struct st_device *dev);

/* Scanner level commands */
static SANE_Int RTS_Scanner_Init (struct st_device *dev);
static SANE_Int RTS_Scanner_SetParams (struct st_device *dev,
				       struct params *param);
static SANE_Int RTS_Scanner_StartScan (struct st_device *dev);
static void RTS_Scanner_StopScan (struct st_device *dev, SANE_Int wait);
static void RTS_Scanner_End (struct st_device *dev);

/* loading configuration functions */
static SANE_Int Load_Buttons (struct st_device *dev);
static SANE_Int Load_Chipset (struct st_device *dev);
static SANE_Int Load_Config (struct st_device *dev);
static SANE_Int Load_Constrains (struct st_device *dev);
static SANE_Int Load_Motor (struct st_device *dev);
static SANE_Int Load_MotorCurves (struct st_device *dev);
static SANE_Int Load_Motormoves (struct st_device *dev);
static SANE_Int Load_Scanmodes (struct st_device *dev);
static SANE_Int Load_Sensor (struct st_device *dev);
static SANE_Int Load_Timings (struct st_device *dev);

/* freeing configuration functions */
static void Free_Buttons (struct st_device *dev);
static void Free_Chipset (struct st_device *dev);
static void Free_Config (struct st_device *dev);
static void Free_Constrains (struct st_device *dev);
static void Free_Motor (struct st_device *dev);
static void Free_MotorCurves (struct st_device *dev);
static void Free_Motormoves (struct st_device *dev);
static void Free_Scanmodes (struct st_device *dev);
static void Free_Sensor (struct st_device *dev);
static void Free_Timings (struct st_device *dev);
static void Free_Vars (void);

/* Functions to manage data */
static SANE_Byte data_bitget (SANE_Byte * address, SANE_Int mask);
static void data_bitset (SANE_Byte * address, SANE_Int mask, SANE_Byte data);
static SANE_Int data_lsb_get (SANE_Byte * address, SANE_Int size);
static void data_lsb_set (SANE_Byte * address, SANE_Int data, SANE_Int size);
static void data_msb_set (SANE_Byte * address, SANE_Int data, SANE_Int size);
static void data_wide_bitset (SANE_Byte * address, SANE_Int mask,
			      SANE_Int data);
static SANE_Int data_swap_endianess (SANE_Int address, SANE_Int size);

static SANE_Int Device_get (SANE_Int product, SANE_Int vendor);

/* Chipset related commands */
static SANE_Int Chipset_ID (struct st_device *dev);
static SANE_Int Chipset_Name (struct st_device *dev, char *name,
			      SANE_Int size);
static SANE_Int Chipset_Reset (struct st_device *dev);

/* Initializing functions */
static SANE_Int Init_Registers (struct st_device *dev);
static SANE_Int Init_USBData (struct st_device *dev);
static SANE_Int Init_Vars (void);

/* scanmode functions */
static SANE_Int Scanmode_fitres (struct st_device *dev, SANE_Int scantype,
				 SANE_Int colormode, SANE_Int resolution);
static SANE_Int Scanmode_maxres (struct st_device *dev, SANE_Int scantype,
				 SANE_Int colormode);
static SANE_Int Scanmode_minres (struct st_device *dev, SANE_Int scantype,
				 SANE_Int colormode);

/* Chipset management useful commands*/
static SANE_Int RTS_USBType (struct st_device *dev);
static SANE_Byte RTS_Sensor_Type (USB_Handle usb_handle);
static void RTS_DebugInit (void);
static SANE_Int RTS_Enable_CCD (struct st_device *dev, SANE_Byte * Regs,
				SANE_Int channels);

/* DMA management commands */
static SANE_Int RTS_DMA_Cancel (struct st_device *dev);
static SANE_Int RTS_DMA_CheckType (struct st_device *dev, SANE_Byte * Regs);
static SANE_Int RTS_DMA_Enable_Read (struct st_device *dev, SANE_Int dmacs,
				     SANE_Int size, SANE_Int options);
static SANE_Int RTS_DMA_Enable_Write (struct st_device *dev, SANE_Int dmacs,
				      SANE_Int size, SANE_Int options);
static SANE_Int RTS_DMA_Read (struct st_device *dev, SANE_Int dmacs,
			      SANE_Int options, SANE_Int size,
			      SANE_Byte * buffer);
static SANE_Int RTS_DMA_Reset (struct st_device *dev);
static SANE_Int RTS_DMA_SetType (struct st_device *dev, SANE_Byte * Regs,
				 SANE_Byte ramtype);
static SANE_Int RTS_DMA_WaitReady (struct st_device *dev, SANE_Int msecs);
static SANE_Int RTS_DMA_Write (struct st_device *dev, SANE_Int dmacs,
			       SANE_Int options, SANE_Int size,
			       SANE_Byte * buffer);

/* EEPROM management commands */
static SANE_Int RTS_EEPROM_ReadByte (USB_Handle usb_handle, SANE_Int address,
				     SANE_Byte * data);
static SANE_Int RTS_EEPROM_ReadInteger (USB_Handle usb_handle,
					SANE_Int address, SANE_Int * data);
static SANE_Int RTS_EEPROM_ReadWord (USB_Handle usb_handle, SANE_Int address,
				     SANE_Int * data);
static SANE_Int RTS_EEPROM_WriteBuffer (USB_Handle usb_handle,
					SANE_Int address, SANE_Byte * data,
					SANE_Int size);
static SANE_Int RTS_EEPROM_WriteByte (USB_Handle usb_handle, SANE_Int address,
				      SANE_Byte data);
static SANE_Int RTS_EEPROM_WriteInteger (USB_Handle usb_handle,
					 SANE_Int address, SANE_Int data);
static SANE_Int RTS_EEPROM_WriteWord (USB_Handle usb_handle, SANE_Int address,
				      SANE_Int data);

static SANE_Int RTS_Execute (struct st_device *dev);
static SANE_Int RTS_Warm_Reset (struct st_device *dev);
static SANE_Byte RTS_IsExecuting (struct st_device *dev, SANE_Byte * Regs);

static SANE_Int RTS_GetScanmode (struct st_device *dev, SANE_Int scantype,
				 SANE_Int colormode, SANE_Int resolution);
static SANE_Int RTS_GetImage (struct st_device *dev, SANE_Byte * Regs,
			      struct st_scanparams *scancfg,
			      struct st_gain_offset *gain_offset,
			      SANE_Byte * buffer,
			      struct st_calibration *myCalib,
			      SANE_Int options, SANE_Int gainmode);
static SANE_Int RTS_GetImage_GetBuffer (struct st_device *dev, double dSize,
					SANE_Byte * buffer,
					double *transferred);
static SANE_Int RTS_GetImage_Read (struct st_device *dev, SANE_Byte * buffer,
				   struct st_scanparams *myvar,
				   struct st_hwdconfig *hwdcfg);

static SANE_Int RTS_isTmaAttached (struct st_device *dev);

/* functions to wait for a process tp finish */
static SANE_Int RTS_WaitInitEnd (struct st_device *dev, SANE_Int msecs);
static SANE_Int RTS_WaitScanEnd (struct st_device *dev, SANE_Int msecs);

/* functions to read/write control registers */
static SANE_Int RTS_ReadRegs (USB_Handle usb_handle, SANE_Byte * buffer);
static SANE_Int RTS_WriteRegs (USB_Handle usb_handle, SANE_Byte * buffer);

/* functions to manage the scan counter */
static SANE_Int RTS_ScanCounter_Inc (struct st_device *dev);
static SANE_Int RTS_ScanCounter_Get (struct st_device *dev);

/* functions to setup control registers */
static SANE_Int RTS_Setup (struct st_device *dev, SANE_Byte * Regs,
			   struct st_scanparams *myvar,
			   struct st_hwdconfig *hwdcfg,
			   struct st_gain_offset *gain_offset);
static void RTS_Setup_Arrangeline (struct st_device *dev,
				   struct st_hwdconfig *hwdcfg,
				   SANE_Int colormode);
static void RTS_Setup_Channels (struct st_device *dev, SANE_Byte * Regs,
				struct st_scanparams *scancfg,
				SANE_Int mycolormode);
static void RTS_Setup_Coords (SANE_Byte * Regs, SANE_Int iLeft, SANE_Int iTop,
			      SANE_Int width, SANE_Int height);
static SANE_Int RTS_Setup_Depth (SANE_Byte * Regs,
				 struct st_scanparams *scancfg,
				 SANE_Int mycolormode);
static void RTS_Setup_Exposure_Times (SANE_Byte * Regs,
				      struct st_scanparams *scancfg,
				      struct st_scanmode *sm);
static void RTS_Setup_GainOffset (SANE_Byte * Regs,
				  struct st_gain_offset *gain_offset);
static void RTS_Setup_Gamma (SANE_Byte * Regs, struct st_hwdconfig *lowcfg);
static SANE_Int RTS_Setup_Line_Distances (struct st_device *dev,
					  SANE_Byte * Regs,
					  struct st_scanparams *scancfg,
					  struct st_hwdconfig *hwdcfg,
					  SANE_Int mycolormode,
					  SANE_Int arrangeline);
static SANE_Int RTS_Setup_Motor (struct st_device *dev, SANE_Byte * Regs,
				 struct st_scanparams *myvar,
				 SANE_Int somevalue);
static void RTS_Setup_RefVoltages (struct st_device *dev, SANE_Byte * Regs);
static void RTS_Setup_SensorTiming (struct st_device *dev, SANE_Int mytiming,
				    SANE_Byte * Regs);
static void RTS_Setup_Shading (SANE_Byte * Regs,
			       struct st_scanparams *scancfg,
			       struct st_hwdconfig *hwdcfg,
			       SANE_Int bytes_per_line);

static SANE_Int Scan_Start (struct st_device *dev);

static void SetLock (USB_Handle usb_handle, SANE_Byte * Regs,
		     SANE_Byte Enable);
static SANE_Int fn3330 (struct st_device *dev, SANE_Byte * Regs,
			struct st_cal2 *calbuffers,
			SANE_Int sensorchannelcolor, SANE_Int * tablepos,
			SANE_Int data);
static SANE_Int fn3560 (USHORT * table, struct st_cal2 *calbuffers,
			SANE_Int * tablepos);
static SANE_Int fn3730 (struct st_device *dev, struct st_cal2 *calbuffers,
			SANE_Byte * Regs, USHORT * table,
			SANE_Int sensorchannelcolor, SANE_Int data);

static SANE_Int Reading_CreateBuffers (struct st_device *dev);
static SANE_Int Reading_DestroyBuffers (struct st_device *dev);
static SANE_Int Reading_BufferSize_Get (struct st_device *dev,
					SANE_Byte channels_per_dot,
					SANE_Int channel_size);
static SANE_Int Reading_BufferSize_Notify (struct st_device *dev,
					   SANE_Int data, SANE_Int size);
static SANE_Int Reading_Wait (struct st_device *dev,
			      SANE_Byte Channels_per_dot,
			      SANE_Byte Channel_size, SANE_Int size,
			      SANE_Int * last_amount, SANE_Int seconds,
			      SANE_Byte op);

static SANE_Int Read_Image (struct st_device *dev, SANE_Int buffer_size,
			    SANE_Byte * buffer, SANE_Int * transferred);
static SANE_Int Read_ResizeBlock (struct st_device *dev, SANE_Byte * buffer,
				  SANE_Int buffer_size,
				  SANE_Int * transferred);
static SANE_Int Read_Block (struct st_device *dev, SANE_Int buffer_size,
			    SANE_Byte * buffer, SANE_Int * transferred);
static SANE_Int Read_NonColor_Block (struct st_device *dev,
				     SANE_Byte * buffer, SANE_Int buffer_size,
				     SANE_Byte ColorMode,
				     SANE_Int * transferred);

/* Ref functions */
static SANE_Int Refs_Analyze_Pattern (struct st_scanparams *scancfg,
				      SANE_Byte * scanned_pattern,
				      SANE_Int * ler1, SANE_Int ler1order,
				      SANE_Int * ser1, SANE_Int ser1order);
static SANE_Int Refs_Counter_Inc (struct st_device *dev);
static SANE_Byte Refs_Counter_Load (struct st_device *dev);
static SANE_Int Refs_Counter_Save (struct st_device *dev, SANE_Byte data);
static SANE_Int Refs_Detect (struct st_device *dev, SANE_Byte * Regs,
			     SANE_Int resolution_x, SANE_Int resolution_y,
			     SANE_Int * x, SANE_Int * y);
static SANE_Int Refs_Load (struct st_device *dev, SANE_Int * x, SANE_Int * y);
static SANE_Int Refs_Save (struct st_device *dev, SANE_Int left_leading,
			   SANE_Int start_pos);
static SANE_Int Refs_Set (struct st_device *dev, SANE_Byte * Regs,
			  struct st_scanparams *myscan);

/* Coordinates' constrains functions */
static SANE_Int Constrains_Check (struct st_device *dev, SANE_Int Resolution,
				  SANE_Int scantype,
				  struct st_coords *mycoords);
static struct st_coords *Constrains_Get (struct st_device *dev,
					 SANE_Byte scantype);

/* Gain and offset functions */
static SANE_Int GainOffset_Clear (struct st_device *dev);
static SANE_Int GainOffset_Get (struct st_device *dev);
static SANE_Int GainOffset_Save (struct st_device *dev, SANE_Int * offset,
				 SANE_Byte * gain);
static SANE_Int GainOffset_Counter_Inc (struct st_device *dev,
					SANE_Int * arg1);
static SANE_Byte GainOffset_Counter_Load (struct st_device *dev);
static SANE_Int GainOffset_Counter_Save (struct st_device *dev,
					 SANE_Byte data);

/* Gamma functions*/
static SANE_Int Gamma_AllocTable (SANE_Byte * table);
static SANE_Int Gamma_Apply (struct st_device *dev, SANE_Byte * Regs,
			     struct st_scanparams *scancfg,
			     struct st_hwdconfig *hwdcfg,
			     struct st_gammatables *mygamma);
static void Gamma_FreeTables (void);
static SANE_Int Gamma_SendTables (struct st_device *dev, SANE_Byte * Regs,
				  SANE_Byte * gammatable, SANE_Int size);
static SANE_Int Gamma_GetTables (struct st_device *dev,
				 SANE_Byte * Gamma_buffer);

/* Lamp functions */
static SANE_Byte Lamp_GetGainMode (struct st_device *dev, SANE_Int resolution,
				   SANE_Byte scantype);
static void Lamp_SetGainMode (struct st_device *dev, SANE_Byte * Regs,
			      SANE_Int resolution, SANE_Byte gainmode);
static SANE_Int Lamp_PWM_DutyCycle_Get (struct st_device *dev,
					SANE_Int * data);
static SANE_Int Lamp_PWM_DutyCycle_Set (struct st_device *dev,
					SANE_Int duty_cycle);
static SANE_Int Lamp_PWM_Setup (struct st_device *dev, SANE_Int lamp);
static SANE_Int Lamp_PWM_use (struct st_device *dev, SANE_Int enable);
static SANE_Int Lamp_PWM_CheckStable (struct st_device *dev,
				      SANE_Int resolution, SANE_Int lamp);
static SANE_Int Lamp_PWM_Save (struct st_device *dev, SANE_Int fixedpwm);
static SANE_Int Lamp_PWM_SaveStatus (struct st_device *dev, SANE_Byte status);
static SANE_Int Lamp_Status_Get (struct st_device *dev, SANE_Byte * flb_lamp,
				 SANE_Byte * tma_lamp);
static SANE_Int Lamp_Status_Set (struct st_device *dev, SANE_Byte * Regs,
				 SANE_Int turn_on, SANE_Int lamp);
static SANE_Int Lamp_Status_Timer_Set (struct st_device *dev,
				       SANE_Int minutes);
static SANE_Int Lamp_Warmup (struct st_device *dev, SANE_Byte * Regs,
			     SANE_Int lamp, SANE_Int resolution);

/* Head related functions */
static SANE_Int Head_IsAtHome (struct st_device *dev, SANE_Byte * Regs);
static SANE_Int Head_ParkHome (struct st_device *dev, SANE_Int bWait,
			       SANE_Int movement);
static SANE_Int Head_Relocate (struct st_device *dev, SANE_Int speed,
			       SANE_Int direction, SANE_Int ypos);

/* Motor functions */
static SANE_Byte *Motor_AddStep (SANE_Byte * steps, SANE_Int * bwriten,
				 SANE_Int step);
static SANE_Int Motor_Change (struct st_device *dev, SANE_Byte * buffer,
			      SANE_Byte value);
static SANE_Int Motor_GetFromResolution (SANE_Int resolution);
static SANE_Int Motor_Move (struct st_device *dev, SANE_Byte * Regs,
			    struct st_motormove *mymotor,
			    struct st_motorpos *mtrpos);
static void Motor_Release (struct st_device *dev);
static SANE_Int Motor_Setup_Steps (struct st_device *dev, SANE_Byte * Regs,
				   SANE_Int mysetting);
static SANE_Int Motor_Curve_Equal (struct st_device *dev,
				   SANE_Int motorsetting, SANE_Int direction,
				   SANE_Int curve1, SANE_Int curve2);
static void Motor_Curve_Free (struct st_motorcurve **motorcurves,
			      SANE_Int * mtc_count);
static struct st_curve *Motor_Curve_Get (struct st_device *dev,
					 SANE_Int motorcurve,
					 SANE_Int direction, SANE_Int itype);
static struct st_motorcurve **Motor_Curve_Parse (SANE_Int * mtc_count,
						 SANE_Int * buffer);

/* Functions to arrange scanned lines */
static SANE_Int Arrange_Colour (struct st_device *dev, SANE_Byte * buffer,
				SANE_Int buffer_size, SANE_Int * transferred);
static SANE_Int Arrange_Compose (struct st_device *dev, SANE_Byte * buffer,
				 SANE_Int buffer_size,
				 SANE_Int * transferred);
static SANE_Int Arrange_NonColour (struct st_device *dev, SANE_Byte * buffer,
				   SANE_Int buffer_size,
				   SANE_Int * transferred);

/* Composing RGB triplet functions */
static void Triplet_Gray (SANE_Byte * pPointer1, SANE_Byte * pPointer2,
			  SANE_Byte * buffer, SANE_Int channels_count);
static void Triplet_Lineart (SANE_Byte * pPointer1, SANE_Byte * pPointer2,
			     SANE_Byte * buffer, SANE_Int channels_count);
static void Triplet_Compose_Order (struct st_device *dev, SANE_Byte * pRed,
				   SANE_Byte * pGreen, SANE_Byte * pBlue,
				   SANE_Byte * buffer, SANE_Int dots);
static void Triplet_Compose_HRes (SANE_Byte * pPointer1,
				  SANE_Byte * pPointer2,
				  SANE_Byte * pPointer3,
				  SANE_Byte * pPointer4,
				  SANE_Byte * pPointer5,
				  SANE_Byte * pPointer6, SANE_Byte * buffer,
				  SANE_Int Width);
static void Triplet_Compose_LRes (SANE_Byte * pRed, SANE_Byte * pGreen,
				  SANE_Byte * pBlue, SANE_Byte * buffer,
				  SANE_Int dots);
static void Triplet_Colour_Order (struct st_device *dev, SANE_Byte * pRed,
				  SANE_Byte * pGreen, SANE_Byte * pBlue,
				  SANE_Byte * buffer, SANE_Int Width);
static void Triplet_Colour_HRes (SANE_Byte * pRed1, SANE_Byte * pGreen1,
				 SANE_Byte * pBlue1, SANE_Byte * pRed2,
				 SANE_Byte * pGreen2, SANE_Byte * pBlue2,
				 SANE_Byte * buffer, SANE_Int Width);
static void Triplet_Colour_LRes (SANE_Int Width, SANE_Byte * Buffer,
				 SANE_Byte * pChannel1, SANE_Byte * pChannel2,
				 SANE_Byte * pChannel3);

/* Timing functions */
static SANE_Int Timing_SetLinearImageSensorClock (SANE_Byte * Regs,
						  struct st_cph *cph);

/* Functions used to resize retrieved image */
static SANE_Int Resize_Start (struct st_device *dev, SANE_Int * transferred);
static SANE_Int Resize_CreateBuffers (struct st_device *dev, SANE_Int size1,
				      SANE_Int size2, SANE_Int size3);
static SANE_Int Resize_DestroyBuffers (struct st_device *dev);
static SANE_Int Resize_Increase (SANE_Byte * to_buffer,
				 SANE_Int to_resolution, SANE_Int to_width,
				 SANE_Byte * from_buffer,
				 SANE_Int from_resolution,
				 SANE_Int from_width, SANE_Int myresize_mode);
static SANE_Int Resize_Decrease (SANE_Byte * to_buffer,
				 SANE_Int to_resolution, SANE_Int to_width,
				 SANE_Byte * from_buffer,
				 SANE_Int from_resolution,
				 SANE_Int from_width, SANE_Int myresize_mode);

/* Scanner buttons support */
static SANE_Int Buttons_Count (struct st_device *dev);
static SANE_Int Buttons_Enable (struct st_device *dev);
static SANE_Int Buttons_Order (struct st_device *dev, SANE_Int mask);
static SANE_Int Buttons_Status (struct st_device *dev);
static SANE_Int Buttons_Released (struct st_device *dev);

/* Calibration functions */
static SANE_Int Calib_CreateBuffers (struct st_device *dev,
				     struct st_calibration *buffer,
				     SANE_Int my14b4);
static SANE_Int Calib_CreateFixedBuffers (void);
static void Calib_FreeBuffers (struct st_calibration *caltables);
static void Calib_LoadCut (struct st_device *dev,
			   struct st_scanparams *scancfg, SANE_Int scantype,
			   struct st_calibration_config *calibcfg);
static SANE_Int Calib_AdcGain (struct st_device *dev,
			       struct st_calibration_config *calibcfg,
			       SANE_Int arg2, SANE_Int gainmode);
static SANE_Int Calib_AdcOffsetRT (struct st_device *dev,
				   struct st_calibration_config *calibcfg,
				   SANE_Int value);
static SANE_Int Calib_BlackShading (struct st_device *dev,
				    struct st_calibration_config *calibcfg,
				    struct st_calibration *myCalib,
				    SANE_Int gainmode);
static SANE_Int Calib_BWShading (struct st_calibration_config *calibcfg,
				 struct st_calibration *myCalib,
				 SANE_Int gainmode);
static SANE_Int Calib_WhiteShading_3 (struct st_device *dev,
				      struct st_calibration_config *calibcfg,
				      struct st_calibration *myCalib,
				      SANE_Int gainmode);
static void Calibrate_Free (struct st_cal2 *calbuffers);
static SANE_Int Calibrate_Malloc (struct st_cal2 *calbuffers,
				  SANE_Byte * Regs,
				  struct st_calibration *myCalib,
				  SANE_Int somelength);
static SANE_Int Calib_ReadTable (struct st_device *dev, SANE_Byte * table,
				 SANE_Int size, SANE_Int data);
static SANE_Int Calib_WriteTable (struct st_device *dev, SANE_Byte * table,
				  SANE_Int size, SANE_Int data);
static SANE_Int Calib_LoadConfig (struct st_device *dev,
				  struct st_calibration_config *calibcfg,
				  SANE_Int scantype, SANE_Int resolution,
				  SANE_Int bitmode);
static SANE_Int Calib_PAGain (struct st_device *dev,
			      struct st_calibration_config *calibcfg,
			      SANE_Int gainmode);
static SANE_Int Calibration (struct st_device *dev, SANE_Byte * Regs,
			     struct st_scanparams *scancfg,
			     struct st_calibration *myCalib, SANE_Int value);

/* function for white shading correction */
static SANE_Int WShading_Calibrate (struct st_device *dev, SANE_Byte * Regs,
				    struct st_calibration *myCalib,
				    struct st_scanparams *scancfg);
static void WShading_Emulate (SANE_Byte * buffer, SANE_Int * chnptr,
			      SANE_Int size, SANE_Int depth);

/* functions for shading calibration */
static SANE_Int Shading_apply (struct st_device *dev, SANE_Byte * Regs,
			       struct st_scanparams *myvar,
			       struct st_calibration *myCalib);
static SANE_Int Shading_black_apply (struct st_device *dev, SANE_Byte * Regs,
				     SANE_Int channels,
				     struct st_calibration *myCalib,
				     struct st_cal2 *calbuffers);
static SANE_Int Shading_white_apply (struct st_device *dev, SANE_Byte * Regs,
				     SANE_Int channels,
				     struct st_calibration *myCalib,
				     struct st_cal2 *calbuffers);

/* Spread-Spectrum Clock Generator functions */
static SANE_Int SSCG_Enable (struct st_device *dev);

static void Split_into_12bit_channels (SANE_Byte * destino,
				       SANE_Byte * fuente, SANE_Int size);
static SANE_Int Scan_Read_BufferA (struct st_device *dev,
				   SANE_Int buffer_size, SANE_Int arg2,
				   SANE_Byte * pBuffer,
				   SANE_Int * bytes_transfered);

static SANE_Int Bulk_Operation (struct st_device *dev, SANE_Byte op,
				SANE_Int buffer_size, SANE_Byte * buffer,
				SANE_Int * transfered);
static SANE_Int Get_PAG_Value (SANE_Byte scantype, SANE_Byte color);
static SANE_Int GetOneLineInfo (struct st_device *dev, SANE_Int resolution,
				SANE_Int * maximus, SANE_Int * minimus,
				double *average);

static SANE_Int Load_StripCoords (SANE_Int scantype, SANE_Int * ypos,
				  SANE_Int * xpos);

/*static SANE_Int Free_Fixed_CalBuffer(void);*/
static SANE_Int SetMultiExposure (struct st_device *dev, SANE_Byte * Regs);

static void Set_E950_Mode (struct st_device *dev, SANE_Byte mode);

static SANE_Int LoadImagingParams (struct st_device *dev, SANE_Int inifile);

static SANE_Int SetScanParams (struct st_device *dev, SANE_Byte * Regs,
			       struct st_scanparams *scancfg,
			       struct st_hwdconfig *hwdcfg);
static SANE_Int IsScannerLinked (struct st_device *dev);

static SANE_Int Read_FE3E (struct st_device *dev, SANE_Byte * destino);

static double get_shrd (double value, SANE_Int desp);
static char get_byte (double value);
/*static SANE_Int RTS8822_GetRegisters(SANE_Byte *buffer);*/

/* ----------------- Implementation ------------------*/

static void
RTS_Free (struct st_device *dev)
{
  /* this function frees space of devices's variable */

  if (dev != NULL)
    {
      /* next function shouldn't be necessary but I can NOT assure that other
         programmers will call Free_Config before this function */
      Free_Config (dev);

      if (dev->init_regs != NULL)
	free (dev->init_regs);

      if (dev->Resize != NULL)
	free (dev->Resize);

      if (dev->Reading != NULL)
	free (dev->Reading);

      if (dev->scanning != NULL)
	free (dev->scanning);

      if (dev->status != NULL)
	free (dev->status);

      free (dev);
    }
}

static struct st_device *
RTS_Alloc ()
{
  /* this function allocates space for device's variable */

  struct st_device *dev = NULL;

  dev = malloc (sizeof (struct st_device));
  if (dev != NULL)
    {
      SANE_Int rst = OK;

      bzero (dev, sizeof (struct st_device));

      /* initial registers */
      dev->init_regs = malloc (sizeof (SANE_Byte) * RT_BUFFER_LEN);
      if (dev->init_regs != NULL)
	bzero (dev->init_regs, sizeof (SANE_Byte) * RT_BUFFER_LEN);
      else
	rst = ERROR;

      if (rst == OK)
	{
	  dev->scanning = malloc (sizeof (struct st_scanning));
	  if (dev->scanning != NULL)
	    bzero (dev->scanning, sizeof (struct st_scanning));
	  else
	    rst = ERROR;
	}

      if (rst == OK)
	{
	  dev->Reading = malloc (sizeof (struct st_readimage));
	  if (dev->Reading != NULL)
	    bzero (dev->Reading, sizeof (struct st_readimage));
	  else
	    rst = ERROR;
	}

      if (rst == OK)
	{
	  dev->Resize = malloc (sizeof (struct st_resize));
	  if (dev->Resize != NULL)
	    bzero (dev->Resize, sizeof (struct st_resize));
	  else
	    rst = ERROR;
	}

      if (rst == OK)
	{
	  dev->status = malloc (sizeof (struct st_status));
	  if (dev->status != NULL)
	    bzero (dev->status, sizeof (struct st_status));
	  else
	    rst = ERROR;
	}

      /* if something fails, free space */
      if (rst != OK)
	{
	  RTS_Free (dev);
	  dev = NULL;
	}
    }

  return dev;
}

static void
RTS_Scanner_End (struct st_device *dev)
{
  Gamma_FreeTables ();
  Free_Config (dev);
  Free_Vars ();
}

static SANE_Int
Device_get (SANE_Int product, SANE_Int vendor)
{
  return cfg_device_get (product, vendor);
}

static SANE_Int
RTS_Scanner_Init (struct st_device *dev)
{
  SANE_Int rst;

  DBG (DBG_FNC, "> RTS_Scanner_Init:\n");
  DBG (DBG_FNC, "> Backend version: %s\n", BACKEND_VRSN);

  rst = ERROR;

  /* gets usb type of this scanner if it's not already set by user */
  if (RTS_Debug->usbtype == -1)
    RTS_Debug->usbtype = RTS_USBType (dev);

  if (RTS_Debug->usbtype != ERROR)
    {
      DBG (DBG_FNC, " -> Chipset model ID: %i\n", Chipset_ID (dev));

      Chipset_Reset (dev);

      if (Load_Config (dev) == OK)
	{
	  if (IsScannerLinked (dev) == OK)
	    {
	      Set_E950_Mode (dev, 0);
	      Gamma_AllocTable (NULL);
	      rst = OK;
	    }
	  else
	    Free_Config (dev);
	}
    }

  return rst;
}

static SANE_Int
RTS_WriteRegs (USB_Handle usb_handle, SANE_Byte * buffer)
{
  SANE_Int rst = ERROR;

  if (buffer != NULL)
    rst =
      Write_Buffer (usb_handle, 0xe800, buffer,
		    RT_BUFFER_LEN * sizeof (SANE_Byte));

  return rst;
}

static SANE_Int
RTS_ReadRegs (USB_Handle usb_handle, SANE_Byte * buffer)
{
  SANE_Int rst = ERROR;

  if (buffer != NULL)
    rst =
      Read_Buffer (usb_handle, 0xe800, buffer,
		   RT_BUFFER_LEN * sizeof (SANE_Byte));

  return rst;
}

static void
SetLock (USB_Handle usb_handle, SANE_Byte * Regs, SANE_Byte Enable)
{
  SANE_Byte lock;

  DBG (DBG_FNC, "+ SetLock(*Regs, Enable=%i):\n", Enable);

  if (Regs == NULL)
    {
      if (Read_Byte (usb_handle, 0xee00, &lock) != OK)
	lock = 0;
    }
  else
    lock = Regs[0x600];

  if (Enable == FALSE)
    lock &= 0xfb;
  else
    lock |= 4;

  if (Regs != NULL)
    Regs[0x600] = lock;

  Write_Byte (usb_handle, 0xee00, lock);

  DBG (DBG_FNC, "- SetLock\n");
}

static void
Set_E950_Mode (struct st_device *dev, SANE_Byte mode)
{
  SANE_Int data;

  DBG (DBG_FNC, "+ Set_E950_Mode(mode=%i):\n", mode);

  if (Read_Word (dev->usb_handle, 0xe950, &data) == OK)
    {
      data = (mode == 0) ? data & 0xffbf : data | 0x40;
      Write_Word (dev->usb_handle, 0xe950, data);
    }

  DBG (DBG_FNC, "- Set_E950_Mode\n");
}

static struct st_curve *
Motor_Curve_Get (struct st_device *dev, SANE_Int motorcurve,
		 SANE_Int direction, SANE_Int itype)
{
  struct st_curve *rst = NULL;

  if (dev != NULL)
    {
      if ((dev->mtrsetting != NULL) && (motorcurve < dev->mtrsetting_count))
	{
	  struct st_motorcurve *mtc = dev->mtrsetting[motorcurve];

	  if (mtc != NULL)
	    {
	      if ((mtc->curve != NULL) && (mtc->curve_count > 0))
		{
		  struct st_curve *crv;
		  SANE_Int a = 0;

		  while (a < mtc->curve_count)
		    {
		      /* get each curve */
		      crv = mtc->curve[a];
		      if (crv != NULL)
			{
			  /* check direction and type */
			  if ((crv->crv_speed == direction)
			      && (crv->crv_type == itype))
			    {
			      /* found ! */
			      rst = crv;
			      break;
			    }
			}
		      a++;
		    }
		}
	    }
	}
    }

  return rst;
}

static SANE_Int
Motor_Curve_Equal (struct st_device *dev, SANE_Int motorsetting,
		   SANE_Int direction, SANE_Int curve1, SANE_Int curve2)
{
  /* compares two curves of the same direction
     returns TRUE if both buffers are equal */

  SANE_Int rst = FALSE;
  struct st_curve *crv1 =
    Motor_Curve_Get (dev, motorsetting, direction, curve1);
  struct st_curve *crv2 =
    Motor_Curve_Get (dev, motorsetting, direction, curve2);

  if ((crv1 != NULL) && (crv2 != NULL))
    {
      if (crv1->step_count == crv2->step_count)
	{
	  rst = TRUE;

	  if (crv1->step_count > 0)
	    {
	      SANE_Int a = 0;

	      while ((a < crv1->step_count) && (rst == TRUE))
		{
		  rst = (crv1->step[a] == crv2->step[a]) ? TRUE : FALSE;
		  a++;
		}
	    }
	}
    }

  return rst;
}

static struct st_motorcurve **
Motor_Curve_Parse (SANE_Int * mtc_count, SANE_Int * buffer)
{
  /* this function parses motorcurve buffer to get all motor settings */
  struct st_motorcurve **rst = NULL;

  *mtc_count = 0;

  if (buffer != NULL)
    {
      /* phases:
         -1 : null phase
         0 :
         -3 : initial config
       */
      struct st_motorcurve *mtc = NULL;
      SANE_Int phase;

      phase = -1;
      while (*buffer != -1)
	{
	  if (*buffer == -2)
	    {
	      /* end of motorcurve */
	      /* complete any openned phase */
	      /* close phase */
	      phase = -1;
	    }
	  else
	    {
	      /* step */
	      if (phase == -1)
		{
		  /* new motorcurve */
		  phase = 0;
		  mtc =
		    (struct st_motorcurve *)
		    malloc (sizeof (struct st_motorcurve));
		  if (mtc != NULL)
		    {
		      *mtc_count += 1;
		      rst =
			realloc (rst,
				 sizeof (struct st_motorcurve **) *
				 *mtc_count);
		      if (rst != NULL)
			{
			  rst[*mtc_count - 1] = mtc;
			  memset (mtc, 0, sizeof (struct st_motorcurve));
			  phase = -3;	/* initial config */
			}
		      else
			{
			  /* memory error */
			  *mtc_count = 0;
			  break;
			}
		    }
		  else
		    break;	/* some error */
		}

	      if (mtc != NULL)
		{
		  switch (phase)
		    {
		    case -3:	/* initial config */
		      mtc->mri = *(buffer);
		      mtc->msi = *(buffer + 1);
		      mtc->skiplinecount = *(buffer + 2);
		      mtc->motorbackstep = *(buffer + 3);
		      buffer += 3;

		      phase = -4;
		      break;

		    case -4:
		      /**/
		      {
			/* create new step curve */
			struct st_curve *curve =
			  malloc (sizeof (struct st_curve));
			if (curve != NULL)
			  {
			    /* add to step curve list */
			    mtc->curve =
			      (struct st_curve **) realloc (mtc->curve,
							    sizeof (struct
								    st_curve
								    **) *
							    (mtc->
							     curve_count +
							     1));
			    if (mtc->curve != NULL)
			      {
				mtc->curve_count++;
				mtc->curve[mtc->curve_count - 1] = curve;

				memset (curve, 0, sizeof (struct st_curve));
				/* read crv speed and type */
				curve->crv_speed = *buffer;
				curve->crv_type = *(buffer + 1);
				buffer += 2;

				/* get length of step buffer */
				while (*(buffer + curve->step_count) != 0)
				  curve->step_count++;

				if (curve->step_count > 0)
				  {
				    /* allocate step buffer */
				    curve->step =
				      (SANE_Int *) malloc (sizeof (SANE_Int) *
							   curve->step_count);
				    if (curve->step != NULL)
				      {
					memcpy (curve->step, buffer,
						sizeof (SANE_Int) *
						curve->step_count);
					buffer += curve->step_count;
				      }
				    else
				      curve->step_count = 0;
				  }
			      }
			    else
			      {
				mtc->curve_count = 0;
				free (curve);
			      }
			  }
			else
			  break;
		      }
		      break;
		    }
		}
	    }
	  buffer++;
	}
    }

  return rst;
}

static void
Motor_Curve_Free (struct st_motorcurve **motorcurves, SANE_Int * mtc_count)
{
  if ((motorcurves != NULL) && (mtc_count != NULL))
    {
      struct st_motorcurve *mtc;
      struct st_curve *curve;

      while (*mtc_count > 0)
	{
	  mtc = motorcurves[*mtc_count - 1];
	  if (mtc != NULL)
	    {
	      if (mtc->curve != NULL)
		{
		  while (mtc->curve_count > 0)
		    {
		      curve = mtc->curve[mtc->curve_count - 1];
		      if (curve != NULL)
			{
			  if (curve->step != NULL)
			    free (curve->step);

			  free (curve);
			}
		      mtc->curve_count--;
		    }
		}
	      free (mtc);
	    }
	  *mtc_count -= 1;
	}

      free (motorcurves);
    }
}

static SANE_Byte
RTS_Sensor_Type (USB_Handle usb_handle)
{
  /*
     Returns sensor type
     01 = CCD
     00 = CIS
   */

  SANE_Int a, b, c;
  SANE_Byte rst;

  DBG (DBG_FNC, "+ RTS_Sensor_Type:\n");

  a = b = c = 0;

  /* Save data first */
  Read_Word (usb_handle, 0xe950, &a);
  Read_Word (usb_handle, 0xe956, &b);

  /* Enables GPIO 0xe950 writing directly 0x13ff */
  Write_Word (usb_handle, 0xe950, 0x13ff);
  /* Sets GPIO 0xe956 writing 0xfcf0 */
  Write_Word (usb_handle, 0xe956, 0xfcf0);
  /* Makes a sleep of 200 ms */
  usleep (1000 * 200);
  /* Get GPIO 0xe968 */
  Read_Word (usb_handle, 0xe968, &c);
  /* Restore data */
  Write_Word (usb_handle, 0xe950, a);
  Write_Word (usb_handle, 0xe956, b);

  rst = ((_B1 (c) & 1) == 0) ? CCD_SENSOR : CIS_SENSOR;

  DBG (DBG_FNC, "- RTS_Sensor_Type: %s\n",
       (rst == CCD_SENSOR) ? "CCD" : "CIS");

  return rst;
}

static void
Free_Scanmodes (struct st_device *dev)
{
  DBG (DBG_FNC, "> Free_Scanmodes\n");

  if (dev->scanmodes != NULL)
    {
      if (dev->scanmodes_count > 0)
	{
	  SANE_Int a;
	  for (a = 0; a < dev->scanmodes_count; a++)
	    if (dev->scanmodes[a] != NULL)
	      free (dev->scanmodes[a]);
	}

      free (dev->scanmodes);
      dev->scanmodes = NULL;
    }

  dev->scanmodes_count = 0;
}

static SANE_Int
Load_Scanmodes (struct st_device *dev)
{
  SANE_Int rst = OK;
  SANE_Int a, b;
  struct st_scanmode reg, *mode;

  DBG (DBG_FNC, "> Load_Scanmodes\n");

  if ((dev->scanmodes != NULL) || (dev->scanmodes_count > 0))
    Free_Scanmodes (dev);

  a = 0;
  while ((cfg_scanmode_get (dev->sensorcfg->type, a, &reg) == OK)
	 && (rst == OK))
    {
      mode = (struct st_scanmode *) malloc (sizeof (struct st_scanmode));
      if (mode != NULL)
	{
	  memcpy (mode, &reg, sizeof (struct st_scanmode));

	  for (b = 0; b < 3; b++)
	    {
	      if (mode->mexpt[b] == 0)
		{
		  mode->mexpt[b] = mode->ctpc;
		  if (mode->multiexposure != 1)
		    mode->expt[b] = mode->ctpc;
		}
	    }

	  mode->ctpc = ((mode->ctpc + 1) * mode->multiexposure) - 1;

	  dev->scanmodes =
	    (struct st_scanmode **) realloc (dev->scanmodes,
					     (dev->scanmodes_count +
					      1) *
					     sizeof (struct st_scanmode **));
	  if (dev->scanmodes != NULL)
	    {
	      dev->scanmodes[dev->scanmodes_count] = mode;
	      dev->scanmodes_count++;
	    }
	  else
	    rst = ERROR;
	}
      else
	rst = ERROR;

      a++;
    }

  if (rst == ERROR)
    Free_Scanmodes (dev);

  DBG (DBG_FNC, " -> Found %i scanmodes\n", dev->scanmodes_count);
  dbg_scanmodes (dev);

  return rst;
}

static void
Free_Config (struct st_device *dev)
{
  DBG (DBG_FNC, "+ Free_Config\n");

  /* free buttons */
  Free_Buttons (dev);

  /* free motor general configuration */
  Free_Motor (dev);

  /* free sensor main configuration */
  Free_Sensor (dev);

  /* free ccd sensor timing tables */
  Free_Timings (dev);

  /* free motor curves */
  Free_MotorCurves (dev);

  /* free motor movements */
  Free_Motormoves (dev);

  /* free scan modes */
  Free_Scanmodes (dev);

  /* free constrains */
  Free_Constrains (dev);

  /* free chipset configuration */
  Free_Chipset (dev);

  DBG (DBG_FNC, "- Free_Config\n");
}

static void
Free_Chipset (struct st_device *dev)
{
  DBG (DBG_FNC, "> Free_Chipset\n");

  if (dev->chipset != NULL)
    {
      if (dev->chipset->name != NULL)
	free (dev->chipset->name);

      free (dev->chipset);
      dev->chipset = NULL;
    }
}

static SANE_Int
Load_Chipset (struct st_device *dev)
{
  SANE_Int rst = ERROR;

  DBG (DBG_FNC, "> Load_Chipset\n");

  if (dev->chipset != NULL)
    Free_Chipset (dev);

  dev->chipset = malloc (sizeof (struct st_chip));
  if (dev->chipset != NULL)
    {
      SANE_Int model;

      bzero (dev->chipset, sizeof (struct st_chip));

      /* get chipset model of selected scanner */
      model = cfg_chipset_model_get (RTS_Debug->dev_model);

      /* get configuration for selected chipset */
      rst = cfg_chipset_get (model, dev->chipset);
    }

  /* if rst == ERROR may be related to allocating space for chipset name */

  return rst;
}

static SANE_Int
Load_Config (struct st_device *dev)
{
  DBG (DBG_FNC, "+ Load_Config\n");

  /* load chipset configuration */
  Load_Chipset (dev);

  /* load scanner's buttons */
  Load_Buttons (dev);

  /* load scanner area constrains */
  Load_Constrains (dev);

  /* load motor general configuration */
  Load_Motor (dev);

  /* load sensor main configuration */
  Load_Sensor (dev);

  if (dev->sensorcfg->type == -1)
    /* get sensor from gpio */
    dev->sensorcfg->type = RTS_Sensor_Type (dev->usb_handle);

  /* load ccd sensor timing tables */
  Load_Timings (dev);

  /* load motor curves */
  Load_MotorCurves (dev);

  /* load motor movements */
  Load_Motormoves (dev);

  /* load scan modes */
  Load_Scanmodes (dev);

  /* deprecated */
  if (dev->sensorcfg->type == CCD_SENSOR)
    /* ccd */ usbfile =
      (RTS_Debug->usbtype == USB20) ? T_RTINIFILE : T_USB1INIFILE;
  else				/* cis */
    usbfile = (RTS_Debug->usbtype == USB20) ? S_RTINIFILE : S_USB1INIFILE;

  scantype = ST_NORMAL;

  pwmlamplevel = get_value (SCAN_PARAM, PWMLAMPLEVEL, 1, usbfile);

  arrangeline2 = get_value (SCAN_PARAM, ARRANGELINE, FIX_BY_HARD, usbfile);

  shadingbase = get_value (TRUE_GRAY_PARAM, SHADINGBASE, 3, usbfile);
  shadingfact[0] = get_value (TRUE_GRAY_PARAM, SHADINGFACT1, 1, usbfile);
  shadingfact[1] = get_value (TRUE_GRAY_PARAM, SHADINGFACT2, 1, usbfile);
  shadingfact[2] = get_value (TRUE_GRAY_PARAM, SHADINGFACT3, 1, usbfile);

  LoadImagingParams (dev, usbfile);

  DBG (DBG_FNC, "- Load_Config\n");

  return OK;
}

static SANE_Int
LoadImagingParams (struct st_device *dev, SANE_Int inifile)
{
  DBG (DBG_FNC, "> LoadImagingParams(inifile='%i'):\n", inifile);

  scan.startpos = get_value (SCAN_PARAM, STARTPOS, 0, inifile);
  scan.leftleading = get_value (SCAN_PARAM, LEFTLEADING, 0, inifile);
  arrangeline = get_value (SCAN_PARAM, ARRANGELINE, FIX_BY_HARD, inifile);
  compression = get_value (SCAN_PARAM, COMPRESSION, 0, inifile);

  /* get default gain and offset values */
  cfg_gainoffset_get (dev->sensorcfg->type, default_gain_offset);

  linedarlampoff = get_value (CALI_PARAM, LINEDARLAMPOFF, 0, inifile);

  pixeldarklevel = get_value (CALI_PARAM, PIXELDARKLEVEL, 0x00ffff, inifile);

  binarythresholdh = get_value (PLATFORM, BINARYTHRESHOLDH, 0x80, inifile);
  binarythresholdl = get_value (PLATFORM, BINARYTHRESHOLDL, 0x7f, inifile);

  return OK;
}

static SANE_Int
Arrange_Colour (struct st_device *dev, SANE_Byte * buffer,
		SANE_Int buffer_size, SANE_Int * transferred)
{
  /*
     05F0FA78   04EC00D8  /CALL to Assumed StdFunc2 from hpgt3970.04EC00D3
     05F0FA7C   05D10048  |Arg1 = 05D10048
     05F0FA80   0000F906  \Arg2 = 0000F906
   */
  SANE_Int mydistance;
  SANE_Int Lines_Count;
  SANE_Int space;
  SANE_Int rst = OK;
  SANE_Int c;
  struct st_scanning *scn;

  DBG (DBG_FNC, "> Arrange_Colour(*buffer, buffer_size=%i, *transferred)\n",
       buffer_size);

  /* this is just to make code more legible */
  scn = dev->scanning;

  if (scn->imagebuffer == NULL)
    {
      if (dev->sensorcfg->type == CCD_SENSOR)
	mydistance =
	  (dev->sensorcfg->line_distance * scan2.resolution_y) /
	  dev->sensorcfg->resolution;
      else
	mydistance = 0;

      /*aafa */
      if (mydistance != 0)
	{
	  scn->bfsize =
	    (scn->arrange_hres ==
	     TRUE) ? scn->arrange_sensor_evenodd_dist : 0;
	  scn->bfsize = (scn->bfsize + (mydistance * 2) + 1) * line_size;
	}
      else
	scn->bfsize = line_size * 2;

      /*ab3c */
      space =
	(((scn->bfsize / line_size) * bytesperline) >
	 scn->bfsize) ? (scn->bfsize / line_size) *
	bytesperline : scn->bfsize;

      scn->imagebuffer = (SANE_Byte *) malloc (space * sizeof (SANE_Byte));
      if (scn->imagebuffer == NULL)
	return ERROR;

      scn->imagepointer = scn->imagebuffer;

      if (Read_Block (dev, scn->bfsize, scn->imagebuffer, transferred) != OK)
	return ERROR;

      scn->arrange_orderchannel = FALSE;
      scn->channel_size = (scan2.depth == 8) ? 1 : 2;

      /* Calculate channel displacements */
      for (c = CL_RED; c <= CL_BLUE; c++)
	{
	  if (mydistance == 0)
	    {
	      /*ab9b */
	      if (scn->arrange_hres == FALSE)
		{
		  if ((((dev->sensorcfg->line_distance * scan2.resolution_y) *
			2) / dev->sensorcfg->resolution) == 1)
		    scn->arrange_orderchannel = TRUE;

		  if (scn->arrange_orderchannel == TRUE)
		    scn->desp[c] =
		      ((dev->sensorcfg->rgb_order[c] / 2) * line_size) +
		      (scn->channel_size * c);
		  else
		    scn->desp[c] = scn->channel_size * c;
		}
	    }
	  else
	    {
	      /*ac32 */
	      scn->desp[c] =
		(dev->sensorcfg->rgb_order[c] * (mydistance * line_size)) +
		(scn->channel_size * c);

	      if (scn->arrange_hres == TRUE)
		{
		  scn->desp1[c] = scn->desp[c];
		  scn->desp2[c] =
		    ((scn->channel_size * 3) + scn->desp[c]) +
		    (scn->arrange_sensor_evenodd_dist * line_size);
		}
	    }
	}

      /*ace2 */
      for (c = CL_RED; c <= CL_BLUE; c++)
	{
	  if (scn->arrange_hres == TRUE)
	    {
	      scn->pColour2[c] = scn->imagebuffer + scn->desp2[c];
	      scn->pColour1[c] = scn->imagebuffer + scn->desp1[c];
	    }
	  else
	    scn->pColour[c] = scn->imagebuffer + scn->desp[c];
	}
    }

  /*ad91 */
  Lines_Count = buffer_size / line_size;
  while (Lines_Count > 0)
    {
      if (scn->arrange_orderchannel == FALSE)
	{
	  if (scn->arrange_hres == TRUE)
	    Triplet_Colour_HRes (scn->pColour1[CL_RED],
				 scn->pColour1[CL_GREEN],
				 scn->pColour1[CL_BLUE],
				 scn->pColour2[CL_RED],
				 scn->pColour2[CL_GREEN],
				 scn->pColour2[CL_BLUE], buffer,
				 line_size / (scn->channel_size * 3));
	  else
	    Triplet_Colour_LRes (line_size / (scn->channel_size * 3), buffer,
				 scn->pColour[CL_RED], scn->pColour[CL_GREEN],
				 scn->pColour[CL_BLUE]);
	}
      else
	Triplet_Colour_Order (dev, scn->pColour[CL_RED],
			      scn->pColour[CL_GREEN], scn->pColour[CL_BLUE],
			      buffer, line_size / (scn->channel_size * 3));

      scn->arrange_size -= bytesperline;
      if (scn->arrange_size < 0)
	v15bc--;

      buffer += line_size;

      Lines_Count--;
      if (Lines_Count == 0)
	{
	  if ((scn->arrange_size | v15bc) == 0)
	    return OK;
	}

      if (Read_Block (dev, line_size, scn->imagepointer, transferred) ==
	  ERROR)
	return ERROR;

      /* Update displacements */
      for (c = CL_RED; c <= CL_BLUE; c++)
	{
	  if (scn->arrange_hres == TRUE)
	    {
	      /*aeb7 */
	      scn->desp2[c] = (scn->desp2[c] + line_size) % scn->bfsize;
	      scn->desp1[c] = (scn->desp1[c] + line_size) % scn->bfsize;

	      scn->pColour2[c] = scn->imagebuffer + scn->desp2[c];
	      scn->pColour1[c] = scn->imagebuffer + scn->desp1[c];
	    }
	  else
	    {
	      /*af86 */
	      scn->desp[c] = (scn->desp[c] + line_size) % scn->bfsize;
	      scn->pColour[c] = scn->imagebuffer + scn->desp[c];
	    }
	}

      /*aff3 */
      scn->imagepointer += line_size;
      if (scn->imagepointer >= (scn->imagebuffer + scn->bfsize))
	scn->imagepointer = scn->imagebuffer;
    }

  return rst;
}

static SANE_Int
RTS_Scanner_SetParams (struct st_device *dev, struct params *param)
{
  SANE_Int rst = ERROR;

  DBG (DBG_FNC, "+ RTS_Scanner_SetParams:\n");
  DBG (DBG_FNC, "->  param->resolution_x=%i\n", param->resolution_x);
  DBG (DBG_FNC, "->  param->resolution_y=%i\n", param->resolution_y);
  DBG (DBG_FNC, "->  param->left        =%i\n", param->coords.left);
  DBG (DBG_FNC, "->  param->width       =%i\n", param->coords.width);
  DBG (DBG_FNC, "->  param->top         =%i\n", param->coords.top);
  DBG (DBG_FNC, "->  param->height      =%i\n", param->coords.height);
  DBG (DBG_FNC, "->  param->colormode   =%s\n",
       dbg_colour (param->colormode));
  DBG (DBG_FNC, "->  param->scantype    =%s\n",
       dbg_scantype (param->scantype));
  DBG (DBG_FNC, "->  param->depth       =%i\n", param->depth);
  DBG (DBG_FNC, "->  param->channel     =%i\n", param->channel);

  /* validate area size to scan */
  if ((param->coords.width != 0) && (param->coords.height != 0))
    {
      SANE_Byte mybuffer[1];
      struct st_hwdconfig hwdcfg;

      /* setting coordinates */
      memcpy (&scan.coord, &param->coords, sizeof (struct st_coords));

      /* setting resolution */
      scan.resolution_x = param->resolution_x;
      scan.resolution_y = param->resolution_y;

      /* setting colormode and depth */
      scan.colormode = param->colormode;
      scan.depth = (param->colormode == CM_LINEART) ? 8 : param->depth;

      /* setting color channel for non color scans */
      scan.channel = _B0 (param->channel);

      arrangeline = FIX_BY_HARD;
      if ((scan.resolution_x == 2400) || ((scan.resolution_x == 4800)))
	{
	  if (scan.colormode != CM_COLOR)
	    {
	      if (scan.colormode == CM_GRAY)
		{
		  if (scan.channel == 3)
		    arrangeline = FIX_BY_SOFT;
		}
	    }
	  else
	    arrangeline = FIX_BY_SOFT;
	}

      /* setting scan type */
      if ((param->scantype > 0) && (param->scantype < 4))
	scan.scantype = param->scantype;
      else
	scan.scantype = ST_NORMAL;

      /* setting scanner lamp */
      data_bitset (&dev->init_regs[0x146], 0x40,
		   ((dev->sensorcfg->type == CIS_SENSOR) ? 0 : 1));

      /* turn on appropiate lamp */
      if (scan.scantype == ST_NORMAL)
	Lamp_Status_Set (dev, NULL, TRUE, FLB_LAMP);
      else
	Lamp_Status_Set (dev, NULL, TRUE, TMA_LAMP);

      mybuffer[0] = 0;
      if (RTS_IsExecuting (dev, mybuffer) == FALSE)
	RTS_WriteRegs (dev->usb_handle, dev->init_regs);

      if (scan.depth == 16)
	compression = FALSE;

      /* resetting low level config */
      bzero (&hwdcfg, sizeof (struct st_hwdconfig));

      /* setting low level config */
      hwdcfg.scantype = scan.scantype;
      hwdcfg.calibrate = mybuffer[0];
      hwdcfg.arrangeline = arrangeline;	/*1 */
      hwdcfg.highresolution = (scan.resolution_x > 1200) ? TRUE : FALSE;
      hwdcfg.sensorevenodddistance = dev->sensorcfg->evenodd_distance;

      SetScanParams (dev, dev->init_regs, &scan, &hwdcfg);

      scan.shadinglength =
	(((scan.sensorresolution * 17) / 2) + 3) & 0xfffffffc;

      rst = OK;
    }

  DBG (DBG_FNC, "- RTS_Scanner_SetParams: %i\n", rst);

  return rst;
}

static SANE_Int
SetScanParams (struct st_device *dev, SANE_Byte * Regs,
	       struct st_scanparams *scancfg, struct st_hwdconfig *hwdcfg)
{
  struct st_coords mycoords;
  SANE_Int mycolormode;
  SANE_Int myvalue;
  SANE_Int mymode;
  SANE_Int channel_size;
  SANE_Int channel_count;
  SANE_Int dots_per_block;
  SANE_Int aditional_dots;

  DBG (DBG_FNC, "+ SetScanParams:\n");
  dbg_ScanParams (scancfg);
  dbg_hwdcfg (hwdcfg);

  bzero (&mycoords, sizeof (struct st_coords));
  /* Copy scancfg to scan2 */
  memcpy (&scan2, scancfg, sizeof (struct st_scanparams));

  mycolormode = scancfg->colormode;
  myvalue = scancfg->colormode;
  scantype = hwdcfg->scantype;

  if (scancfg->colormode == CM_LINEART)
    scan2.depth = 8;

  if ((scancfg->colormode != CM_COLOR) && (scancfg->channel == 3))	/*channel = 0x00 */
    {
      if (scancfg->colormode == CM_GRAY)
	{
	  mycolormode = (hwdcfg->arrangeline != FIX_BY_SOFT) ? 3 : CM_COLOR;
	}
      else
	mycolormode = 3;
      myvalue = mycolormode;
    }

  dev->Resize->resolution_x = scancfg->resolution_x;
  dev->Resize->resolution_y = scancfg->resolution_y;

  mymode = RTS_GetScanmode (dev, hwdcfg->scantype, myvalue, scancfg->resolution_x);	/*0x0b */
  if (mymode == -1)
    {
      /* Non supported resolution. We will resize image after scanning */
      SANE_Int fitres;

      fitres =
	Scanmode_fitres (dev, hwdcfg->scantype, scancfg->colormode,
			 scancfg->resolution_x);
      if (fitres != -1)
	{
	  /* supported resolution found */
	  dev->Resize->type = RSZ_DECREASE;
	}
      else
	{
	  dev->Resize->type = RSZ_INCREASE;
	  fitres =
	    Scanmode_maxres (dev, hwdcfg->scantype, scancfg->colormode);
	}

      scan2.resolution_x = fitres;
      scan2.resolution_y = fitres;

      mymode =
	RTS_GetScanmode (dev, hwdcfg->scantype, myvalue, scan2.resolution_x);
      if (mymode == -1)
	return ERROR;

      imageheight = scancfg->coord.height;
      dev->Resize->towidth = scancfg->coord.width;

      /* Calculate coords for new resolution */
      mycoords.left =
	(scan2.resolution_x * scancfg->coord.left) /
	dev->Resize->resolution_x;
      mycoords.width =
	(scan2.resolution_x * scancfg->coord.width) /
	dev->Resize->resolution_x;
      mycoords.top =
	(scan2.resolution_y * scancfg->coord.top) / dev->Resize->resolution_y;
      mycoords.height =
	((scan2.resolution_y * scancfg->coord.height) /
	 dev->Resize->resolution_y) + 2;

      switch (scan2.colormode)
	{
	case CM_GRAY:
	  if ((dev->scanmodes[mymode]->samplerate == PIXEL_RATE)
	      && (mycolormode != 3))
	    dev->Resize->towidth *= 2;

	  channel_size = (scan2.depth == 8) ? 1 : 2;
	  dev->Resize->mode = (scan2.depth == 8) ? RSZ_GRAYL : RSZ_GRAYH;
	  dev->Resize->bytesperline = dev->Resize->towidth * channel_size;
	  break;
	case CM_LINEART:
	  if (dev->scanmodes[mymode]->samplerate == PIXEL_RATE)
	    dev->Resize->towidth *= 2;

	  dev->Resize->mode = RSZ_LINEART;
	  dev->Resize->bytesperline = (dev->Resize->towidth + 7) / 8;
	  break;
	default:		/*CM_COLOR */
	  channel_count = 3;
	  channel_size = (scan2.depth == 8) ? 1 : 2;
	  dev->Resize->mode = (scan2.depth == 8) ? RSZ_COLOURL : RSZ_COLOURH;
	  dev->Resize->bytesperline =
	    scancfg->coord.width * (channel_count * channel_size);
	  break;
	}
    }
  else
    {
      /* Supported scanmode */
      dev->Resize->type = RSZ_NONE;
      scan2.resolution_x = scancfg->resolution_x;
      scan2.resolution_y = scancfg->resolution_y;
      mycoords.left = scancfg->coord.left;
      mycoords.top = scancfg->coord.top;
      mycoords.width = scancfg->coord.width;
      mycoords.height = scancfg->coord.height;
    }

  scancfg->timing = dev->scanmodes[mymode]->timing;

  scan2.sensorresolution = dev->timings[scancfg->timing]->sensorresolution;
  if ((scantype > 0) && (scantype < 5))
    scan2.shadinglength =
      (((scan2.sensorresolution * 17) / 2) + 3) & 0xfffffffc;

  scancfg->sensorresolution = scan2.sensorresolution;
  scancfg->shadinglength = scan2.shadinglength;

  dev->scanning->arrange_compression = ((mycolormode != CM_LINEART)
					&& (scan2.depth <=
					    8)) ? hwdcfg->compression : FALSE;

  if ((arrangeline2 == FIX_BY_HARD) || (mycolormode == CM_LINEART))
    arrangeline2 = mycolormode;	/*¿? */
  else if ((mycolormode == CM_GRAY) && (hwdcfg->highresolution == FALSE))
    arrangeline2 = 0;

  if (hwdcfg->highresolution == FALSE)
    {
      /* resolution < 1200 dpi */
      dev->scanning->arrange_hres = FALSE;
      dev->scanning->arrange_sensor_evenodd_dist = 0;
    }
  else
    {
      /* resolution > 1200 dpi */
      dev->scanning->arrange_hres = TRUE;
      dev->scanning->arrange_sensor_evenodd_dist =
	hwdcfg->sensorevenodddistance;
    }

  /* with must be adjusted to fit in the dots count per block */
  aditional_dots = 0;
  if (mycolormode != CM_LINEART)
    {
      dots_per_block = ((scan2.resolution_x > 2400)
			&& (scancfg->samplerate == PIXEL_RATE)) ? 8 : 4;

      /* fit width */
      if ((mycoords.width % dots_per_block) != 0)
	{
	  aditional_dots = dots_per_block - (mycoords.width % dots_per_block);
	  mycoords.width += aditional_dots;
	}
    }
  else
    {
      /* Lineart */
      dots_per_block = 32 - (mycoords.width & 0x1f);
      if (dots_per_block < 32)
	{
	  mycoords.width += dots_per_block;
	  aditional_dots = (dots_per_block / 8);
	}
    }

  DBG (DBG_FNC, " -> dots_per_block: %i\n", dots_per_block);
  DBG (DBG_FNC, " -> aditional_dots: %i\n", aditional_dots);

  if (mycolormode == CM_LINEART)
    {
      bytesperline =
	(dev->scanmodes[mymode]->samplerate ==
	 PIXEL_RATE) ? mycoords.width / 4 : mycoords.width / 8;
      imagewidth3 = bytesperline;
      lineart_width = bytesperline * 8;
      line_size = bytesperline - aditional_dots;
      dev->Resize->fromwidth = line_size * 8;
    }
  else
    {
      /*4510 */
      switch (mycolormode)
	{
	case CM_COLOR:
	  channel_count = 3;
	  break;
	case 3:
	  channel_count = 1;
	  break;
	case CM_GRAY:
	  channel_count = (dev->scanmodes[mymode]->samplerate == PIXEL_RATE) ? 2 : 1;	/*1 */
	  break;
	}

      channel_size = (scan2.depth == 8) ? 1 : 2;
      bytesperline = mycoords.width * (channel_count * channel_size);
      imagewidth3 = bytesperline / channel_count;
      lineart_width = imagewidth3 / channel_size;
      line_size =
	bytesperline - (aditional_dots * (channel_count * channel_size));
      dev->Resize->fromwidth = line_size / (channel_count * channel_size);
    }

  imagesize = mycoords.height * bytesperline;
  v15b4 = 0;
  dev->scanning->arrange_size = imagesize;
  v15bc = 0;

  /* set resolution ratio */
  data_bitset (&Regs[0xc0], 0x1f,
	       scancfg->sensorresolution / scancfg->resolution_x);

  scancfg->coord.left = mycoords.left;
  scancfg->coord.top = mycoords.top;
  scancfg->coord.width = mycoords.width;
  scancfg->coord.height = mycoords.height;
  scancfg->resolution_x = scan2.resolution_x;
  scancfg->resolution_y = scan2.resolution_y;

  myvalue =
    (dev->Resize->type == RSZ_NONE) ? line_size : dev->Resize->bytesperline;
  scancfg->bytesperline = bytesperline;

  scancfg->v157c = myvalue;

  if (scan.colormode != CM_COLOR)
    {
      if (mycolormode == CM_COLOR)
	scancfg->v157c = (scancfg->v157c / 3);
    }

  if (scan.colormode == CM_LINEART)
    {
      if (mycolormode == 3)
	{
	  scancfg->v157c = (scancfg->v157c + 7) / 8;
	  scancfg->bytesperline = (scancfg->bytesperline + 7) / 8;
	}
    }

  DBG (DBG_FNC, "- SetScanParams:\n");

  return OK;
}

static SANE_Int
GainOffset_Counter_Save (struct st_device *dev, SANE_Byte data)
{
  SANE_Int rst = OK;

  DBG (DBG_FNC, "> GainOffset_Counter_Save(data=%i):\n", data);

  /* check if chipset supports accessing eeprom */
  if ((dev->chipset->capabilities & CAP_EEPROM) != 0)
    {
      data = min (data, 0x0f);
      rst = RTS_EEPROM_WriteByte (dev->usb_handle, 0x0077, data);
    }

  return rst;
}

static SANE_Int
GainOffset_Counter_Inc (struct st_device *dev, SANE_Int * arg1)
{
  SANE_Byte count;
  SANE_Int rst;

  DBG (DBG_FNC, "+ GainOffset_Counter_Inc:\n");

  /* check if chipset supports accessing eeprom */
  if ((dev->chipset->capabilities & CAP_EEPROM) != 0)
    {
      count = GainOffset_Counter_Load (dev);
      if ((count >= 0x0f) || (GainOffset_Get (dev) != OK))
	{
	  offset[CL_BLUE] = offset[CL_GREEN] = offset[CL_RED] = 0;
	  gain[CL_BLUE] = gain[CL_GREEN] = gain[CL_RED] = 0;
	  count = 0;
	}
      else
	{
	  count++;
	  if (arg1 != NULL)
	    *arg1 = 1;
	}

      rst = GainOffset_Counter_Save (dev, count);
    }
  else
    rst = OK;

  DBG (DBG_FNC, "- GainOffset_Counter_Inc: %i\n", rst);

  return rst;
}

static SANE_Int
GainOffset_Get (struct st_device *dev)
{
  SANE_Int a, data, rst;
  SANE_Byte checksum;

  DBG (DBG_FNC, "+ GainOffset_Get:\n");

  checksum = 0;

  /* check if chipset supports accessing eeprom */
  if ((dev->chipset->capabilities & CAP_EEPROM) != 0)
    {
      /* get current checksum */
      if (RTS_EEPROM_ReadByte (dev->usb_handle, 0x76, &checksum) == OK)
	{
	  rst = OK;

	  /* read gain and offset values from EEPROM */
	  for (a = CL_RED; a <= CL_BLUE; a++)
	    {
	      if (RTS_EEPROM_ReadWord (dev->usb_handle, 0x70 + (2 * a), &data)
		  == ERROR)
		{
		  rst = ERROR;
		  break;
		}
	      else
		offset[a] = data;
	    }

	  /* check checksum */
	  checksum =
	    _B0 (checksum + offset[CL_GREEN] + offset[CL_BLUE] +
		 offset[CL_RED]);
	}
      else
	rst = ERROR;
    }
  else
    rst = ERROR;

  /* extract gain and offset values */
  if ((rst == OK) && (checksum == 0x5b))
    {
      for (a = CL_RED; a <= CL_BLUE; a++)
	{
	  gain[a] = (offset[a] >> 9) & 0x1f;
	  offset[a] &= 0x01ff;
	}
    }
  else
    {
      /* null values, let's reset them */
      for (a = CL_RED; a <= CL_BLUE; a++)
	{
	  gain[a] = 0;
	  offset[a] = 0;
	}

      rst = ERROR;
    }

  DBG (DBG_FNC,
       "->   Preview gainR=%i, gainG=%i, gainB=%i, offsetR=%i, offsetG=%i, offsetB=%i\n",
       gain[CL_RED], gain[CL_GREEN], gain[CL_BLUE], offset[CL_RED],
       offset[CL_GREEN], offset[CL_BLUE]);

  DBG (DBG_FNC, "- GainOffset_Get: %i\n", rst);

  return rst;
}

static SANE_Int
Scanmode_maxres (struct st_device *dev, SANE_Int scantype, SANE_Int colormode)
{
  /* returns position in scanmodes table where data fits with given arguments */
  SANE_Int rst = 0;
  SANE_Int a;
  struct st_scanmode *reg;

  for (a = 0; a < dev->scanmodes_count; a++)
    {
      reg = dev->scanmodes[a];
      if (reg != NULL)
	{
	  if ((reg->scantype == scantype) && (reg->colormode == colormode))
	    rst = max (rst, reg->resolution);	/* found ! */
	}
    }

  if (rst == 0)
    {
      /* There isn't any mode for these arguments.
         Most devices doesn't support specific setup to scan in lineart mode
         so they use gray colormode. Lets check this case */
      if (colormode == CM_LINEART)
	rst = Scanmode_maxres (dev, scantype, CM_GRAY);
    }

  DBG (DBG_FNC, "> Scanmode_maxres(scantype=%s, colormode=%s): %i\n",
       dbg_scantype (scantype), dbg_colour (colormode), rst);

  return rst;
}

static SANE_Int
Scanmode_minres (struct st_device *dev, SANE_Int scantype, SANE_Int colormode)
{
  /* returns position in scanmodes table where data fits with given arguments */
  SANE_Int rst, a;
  struct st_scanmode *reg;

  rst = Scanmode_maxres (dev, scantype, colormode);

  for (a = 0; a < dev->scanmodes_count; a++)
    {
      reg = dev->scanmodes[a];
      if (reg != NULL)
	{
	  if ((reg->scantype == scantype) && (reg->colormode == colormode))
	    rst = min (rst, reg->resolution);	/* found ! */
	}
    }

  if (rst == 0)
    {
      /* There isn't any mode for these arguments.
         Most devices doesn't support specific setup to scan in lineart mode
         so they use gray colormode. Lets check this case */
      if (colormode == CM_LINEART)
	rst = Scanmode_minres (dev, scantype, CM_GRAY);
    }

  DBG (DBG_FNC, "> Scanmode_minres(scantype=%s, colormode=%s): %i\n",
       dbg_scantype (scantype), dbg_colour (colormode), rst);

  return rst;
}

static SANE_Int
Scanmode_fitres (struct st_device *dev, SANE_Int scantype, SANE_Int colormode,
		 SANE_Int resolution)
{
  /* returns a supported resolution */
  SANE_Int rst;
  SANE_Int a, nullres;
  struct st_scanmode *reg;

  nullres = Scanmode_maxres (dev, scantype, colormode) + 1;
  rst = nullres;

  for (a = 0; a < dev->scanmodes_count; a++)
    {
      reg = dev->scanmodes[a];
      if (reg != NULL)
	{
	  if ((reg->scantype == scantype) && (reg->colormode == colormode))
	    {
	      if ((reg->resolution < rst) && (resolution <= reg->resolution))
		rst = reg->resolution;
	    }
	}
    }

  if (rst == nullres)
    {
      /* There isn't any mode for these arguments.
         Most devices doesn't support specific setup to scan in lineart mode
         so they use gray colormode. Lets check this case */
      if (colormode != CM_LINEART)
	{
	  /* at this point, given resolution is bigger than maximum supported resolution */
	  rst = -1;
	}
      else
	rst = Scanmode_minres (dev, scantype, CM_GRAY);
    }

  DBG (DBG_FNC,
       "> Scanmode_fitres(scantype=%s, colormode=%s, resolution=%i): %i\n",
       dbg_scantype (scantype), dbg_colour (colormode), resolution, rst);

  return rst;
}

static SANE_Int
RTS_GetScanmode (struct st_device *dev, SANE_Int scantype, SANE_Int colormode,
		 SANE_Int resolution)
{
  /* returns position in scanmodes table where data fits with given arguments */
  SANE_Int rst = -1;
  SANE_Int a;
  struct st_scanmode *reg;

  for (a = 0; a < dev->scanmodes_count; a++)
    {
      reg = dev->scanmodes[a];
      if (reg != NULL)
	{
	  if ((reg->scantype == scantype) && (reg->colormode == colormode)
	      && (reg->resolution == resolution))
	    {
	      /* found ! */
	      rst = a;
	      break;
	    }
	}
    }

  if (rst == -1)
    {
      /* There isn't any mode for these arguments.
         May be given resolution isn't supported by chipset.
         Most devices doesn't support specific setup to scan in lineart mode
         so they use gray colormode. Lets check this case */
      if ((colormode == CM_LINEART) || (colormode == 3))
	rst = RTS_GetScanmode (dev, scantype, CM_GRAY, resolution);
    }

  DBG (DBG_FNC,
       "> RTS_GetScanmode(scantype=%s, colormode=%s, resolution=%i): %i\n",
       dbg_scantype (scantype), dbg_colour (colormode), resolution, rst);

  return rst;
}

static void
Free_Motor (struct st_device *dev)
{
  /* this function releases space for stepper motor */

  DBG (DBG_FNC, "> Free_Motor\n");

  if (dev->motorcfg != NULL)
    {
      free (dev->motorcfg);
      dev->motorcfg = NULL;
    }
}

static SANE_Int
Load_Motor (struct st_device *dev)
{
  /* this function loads general configuration for motor */

  SANE_Int rst = ERROR;

  DBG (DBG_FNC, "> Load_Motor\n");

  if (dev->motorcfg != NULL)
    Free_Motor (dev);

  dev->motorcfg = malloc (sizeof (struct st_motorcfg));
  if (dev->motorcfg != NULL)
    {
      rst = cfg_motor_get (dev->motorcfg);
      dbg_motorcfg (dev->motorcfg);
    }

  return rst;
}

static void
Free_Sensor (struct st_device *dev)
{
  /* this function releases space for ccd sensor */

  DBG (DBG_FNC, "> Free_Sensor\n");

  if (dev->sensorcfg != NULL)
    {
      free (dev->sensorcfg);
      dev->sensorcfg = NULL;
    }
}

static void
Free_Buttons (struct st_device *dev)
{
  /* this function releases space for buttons */

  DBG (DBG_FNC, "> Free_Buttons\n");

  if (dev->buttons != NULL)
    {
      free (dev->buttons);
      dev->buttons = NULL;
    }
}

static SANE_Int
Load_Buttons (struct st_device *dev)
{
  /* this function loads configuration for ccd sensor */

  SANE_Int rst = ERROR;

  DBG (DBG_FNC, "> Load_Buttons\n");

  if (dev->buttons != NULL)
    Free_Buttons (dev);

  dev->buttons = malloc (sizeof (struct st_buttons));
  if (dev->buttons != NULL)
    {
      rst = cfg_buttons_get (dev->buttons);
      dbg_buttons (dev->buttons);
    }

  return rst;
}

static SANE_Int
Load_Sensor (struct st_device *dev)
{
  /* this function loads configuration for ccd sensor */

  SANE_Int rst = ERROR;

  DBG (DBG_FNC, "> Load_Sensor\n");

  if (dev->sensorcfg != NULL)
    Free_Sensor (dev);

  dev->sensorcfg = malloc (sizeof (struct st_sensorcfg));
  if (dev->sensorcfg != NULL)
    {
      rst = cfg_sensor_get (dev->sensorcfg);
      dbg_sensor (dev->sensorcfg);
    }

  return rst;
}

static void
Free_Timings (struct st_device *dev)
{
  /* this function frees all ccd sensor timing tables */
  DBG (DBG_FNC, "> Free_Timings\n");

  if (dev->timings != NULL)
    {
      if (dev->timings_count > 0)
	{
	  SANE_Int a;
	  for (a = 0; a < dev->timings_count; a++)
	    if (dev->timings[a] != NULL)
	      free (dev->timings[a]);

	  dev->timings_count = 0;
	}

      free (dev->timings);
      dev->timings = NULL;
    }
}

static SANE_Int
Load_Timings (struct st_device *dev)
{
  SANE_Int rst = OK;
  SANE_Int a;
  struct st_timing reg, *tmg;

  DBG (DBG_FNC, "> Load_Timings\n");

  if (dev->timings != NULL)
    Free_Timings (dev);

  a = 0;

  while ((cfg_timing_get (dev->sensorcfg->type, a, &reg) == OK)
	 && (rst == OK))
    {
      tmg = (struct st_timing *) malloc (sizeof (struct st_timing));
      if (tmg != NULL)
	{
	  memcpy (tmg, &reg, sizeof (struct st_timing));

	  dev->timings_count++;
	  dev->timings =
	    (struct st_timing **) realloc (dev->timings,
					   sizeof (struct st_timing **) *
					   dev->timings_count);
	  if (dev->timings == NULL)
	    {
	      rst = ERROR;
	      dev->timings_count = 0;
	    }
	  else
	    dev->timings[dev->timings_count - 1] = tmg;
	}
      else
	rst = ERROR;

      a++;
    }

  if (rst == ERROR)
    Free_Timings (dev);

  DBG (DBG_FNC, " -> Found %i timing registers\n", dev->timings_count);

  return rst;
}

static SANE_Int
IsScannerLinked (struct st_device *dev)
{
  SANE_Int var2;
  SANE_Byte lamp;

  DBG (DBG_FNC, "+ IsScannerLinked:\n");

  Read_FE3E (dev, &v1619);
  Init_USBData (dev);
  scan.scantype = ST_NORMAL;

  RTS_WaitInitEnd (dev, 0x30000);

  lamp = FLB_LAMP;

  /* Comprobar si es la primera conexión con el escaner */
  if (Read_Word (dev->usb_handle, 0xe829, &var2) == OK)
    {
      SANE_Int firstconnection;

#ifdef STANDALONE
      firstconnection = TRUE;
#else
      firstconnection = (var2 == 0) ? TRUE : FALSE;
#endif

      if (firstconnection == TRUE)
	{
	  /* primera conexión */
	  SANE_Byte flb_lamp, tma_lamp;

	  flb_lamp = 0;
	  tma_lamp = 0;
	  Lamp_Status_Get (dev, &flb_lamp, &tma_lamp);

	  if ((flb_lamp == 0) && (tma_lamp != 0))
	    lamp = TMA_LAMP;

	  /*Clear GainOffset count */
	  GainOffset_Clear (dev);
	  GainOffset_Counter_Save (dev, 0);

	  /* Clear AutoRef count */
	  Refs_Counter_Save (dev, 0);

	  Buttons_Enable (dev);
	  Lamp_Status_Timer_Set (dev, 13);
	}
      else
	lamp = (_B0 (var2) == 0) ? FLB_LAMP : TMA_LAMP;
    }

  if (RTS_Warm_Reset (dev) != OK)
    return ERROR;

  Head_ParkHome (dev, TRUE, dev->motorcfg->parkhomemotormove);

  Lamp_Status_Timer_Set (dev, 13);

  /* Use fixed pwm? */
  if (RTS_Debug->use_fixed_pwm != FALSE)
    {
      Lamp_PWM_Save (dev, cfg_fixedpwm_get (dev->sensorcfg->type, ST_NORMAL));
      /* Lets enable using fixed pwm */
      Lamp_PWM_SaveStatus (dev, TRUE);
    }

  Lamp_PWM_Setup (dev, lamp);

  DBG (DBG_FNC, "- IsScannerLinked:\n");

  return OK;
}

static SANE_Int
Lamp_PWM_SaveStatus (struct st_device *dev, SANE_Byte status)
{
  SANE_Byte mypwm;
  SANE_Int rst = OK;

  DBG (DBG_FNC, "+ Lamp_PWM_SaveStatus(status=%i):\n", status);

  /* check if chipset supports accessing eeprom */
  if ((dev->chipset->capabilities & CAP_EEPROM) != 0)
    {
      rst = ERROR;

      if (RTS_EEPROM_ReadByte (dev->usb_handle, 0x007b, &mypwm) == OK)
	{
	  mypwm = (status == FALSE) ? mypwm & 0x7f : mypwm | 0x80;

	  if (RTS_EEPROM_WriteByte (dev->usb_handle, 0x007b, mypwm) == OK)
	    rst = OK;
	}
    }

  DBG (DBG_FNC, "- Lamp_PWM_SaveStatus: %i\n", rst);

  return rst;
}

static SANE_Int
Lamp_PWM_Save (struct st_device *dev, SANE_Int fixedpwm)
{
  SANE_Int rst;

  DBG (DBG_FNC, "+ Lamp_PWM_Save(fixedpwm=%i):\n", fixedpwm);

  /* check if chipset supports accessing eeprom */
  if ((dev->chipset->capabilities & CAP_EEPROM) != 0)
    rst =
      RTS_EEPROM_WriteByte (dev->usb_handle, 0x7b, ((fixedpwm << 2) | 0x80));
  else
    rst = OK;

  DBG (DBG_FNC, "- Lamp_PWM_Save: %i\n", rst);

  return rst;
}

static SANE_Int
Lamp_PWM_Setup (struct st_device *dev, SANE_Int lamp)
{
  SANE_Int rst = OK;

  DBG (DBG_FNC, "+ Lamp_PWM_Setup(lamp=%s):\n",
       (lamp == FLB_LAMP) ? "FLB_LAMP" : "TMA_LAMP");

  if (Lamp_PWM_use (dev, 1) == OK)
    {
      SANE_Int fixedpwm, currentpwd;

      currentpwd = 0;
      fixedpwm =
	cfg_fixedpwm_get (dev->sensorcfg->type,
			  (lamp == FLB_LAMP) ? ST_NORMAL : ST_TA);

      if (Lamp_PWM_DutyCycle_Get (dev, &currentpwd) == OK)
	{
	  /* set duty cycle if current one is different */
	  if (currentpwd != fixedpwm)
	    rst = Lamp_PWM_DutyCycle_Set (dev, fixedpwm);
	}
      else
	rst = Lamp_PWM_DutyCycle_Set (dev, fixedpwm);
    }

  DBG (DBG_FNC, "- Lamp_PWM_Setup: %i\n", rst);

  return rst;
}

static SANE_Int
Lamp_PWM_DutyCycle_Get (struct st_device *dev, SANE_Int * data)
{
  SANE_Byte a;
  SANE_Int rst = ERROR;

  DBG (DBG_FNC, "+ Lamp_PWM_DutyCycle_Get:\n");

  if (Read_Byte (dev->usb_handle, 0xe948, &a) == OK)
    {
      *data = a & 0x3f;
      rst = OK;
    }

  DBG (DBG_FNC, "- Lamp_PWM_DutyCycle_Get = %i: %i\n", *data, rst);

  return rst;
}

static SANE_Int
Lamp_PWM_DutyCycle_Set (struct st_device *dev, SANE_Int duty_cycle)
{
  SANE_Byte *Regs;
  SANE_Int rst;

  DBG (DBG_FNC, "+ Lamp_PWM_DutyCycle_Set(duty_cycle=%i):\n", duty_cycle);

  rst = ERROR;

  Regs = (SANE_Byte *) malloc (RT_BUFFER_LEN * sizeof (SANE_Byte));
  if (Regs != NULL)
    {
      if (RTS_ReadRegs (dev->usb_handle, Regs) == OK)
	{
	  data_bitset (&Regs[0x148], 0x3f, duty_cycle);

	  if (pwmlamplevel == 0)
	    {
	      data_bitset (&Regs[0x148], 0x40, 0);
	      Regs[0x1e0] |= ((duty_cycle >> 1) & 0x40);
	    }

	  data_bitset (&dev->init_regs[0x148], 0x7f, Regs[0x148]);
	  data_bitset (&dev->init_regs[0x1e0], 0x3f, Regs[0x1e0]);

	  Write_Byte (dev->usb_handle, 0xe948, Regs[0x0148]);

	  rst = Write_Byte (dev->usb_handle, 0xe9e0, Regs[0x01e0]);
	}

      free (Regs);
    }

  DBG (DBG_FNC, "- Lamp_PWM_DutyCycle_Set: %i\n", rst);

  return rst;
}

static SANE_Int
Head_ParkHome (struct st_device *dev, SANE_Int bWait, SANE_Int movement)
{
  SANE_Int rst = ERROR;
  SANE_Byte *Regs;

  DBG (DBG_FNC, "+ Head_ParkHome(bWait=%i, movement=%i):\n", bWait, movement);

  Regs = (SANE_Byte *) malloc (RT_BUFFER_LEN * sizeof (SANE_Byte));
  if (Regs != NULL)
    {
      rst = OK;

      memcpy (Regs, dev->init_regs, RT_BUFFER_LEN * sizeof (SANE_Byte));

      /* Lets wait if it's required when is already executing */
      if (bWait != FALSE)
	{
	  if (RTS_WaitScanEnd (dev, 0x3a98) != OK)
	    {
	      DBG (DBG_FNC, " -> Head_ParkHome: RTS_WaitScanEnd Timeout\n");
	      rst = ERROR;	/* timeout */
	    }
	}
      else
	{
	  if (RTS_IsExecuting (dev, Regs) == FALSE)
	    {
	      DBG (DBG_FNC,
		   " -> Head_ParkHome: RTS_IsExecuting = 0, exiting function\n");
	      rst = ERROR;	/* if NOT executing */
	    }
	}

      /* Check if lamp is at home */
      if ((rst == OK) && (Head_IsAtHome (dev, Regs) == FALSE))
	{
	  struct st_motormove mymotor;
	  struct st_motorpos mtrpos;

	  DBG (DBG_FNC,
	       "->   Head_ParkHome: Lamp is not at home, lets move\n");

	  /* it isn't */
	  dev->status->parkhome = TRUE;

	  if ((movement != -1) && (movement < dev->motormove_count))
	    {
	      memcpy (&mymotor, dev->motormove[movement],
		      sizeof (struct st_motormove));
	    }
	  else
	    {
	      /* debug this code. Shouldn't have any relationship with offsets */
	      if (default_gain_offset->edcg2[CL_BLUE] < 4)
		mymotor.scanmotorsteptype =
		  default_gain_offset->edcg2[CL_BLUE];

	      mymotor.ctpc = default_gain_offset->odcg2[1];
	      mymotor.systemclock = default_gain_offset->edcg2[1];	/*? */
	    }

	  mtrpos.options = MTR_ENABLED | MTR_BACKWARD;
	  mtrpos.v12e448 = 0x01;
	  mtrpos.v12e44c = 0x00;
	  mtrpos.coord_y = 0x4e20;

	  Motor_Move (dev, Regs, &mymotor, &mtrpos);

	  /* Should we wait? */
	  if (bWait != FALSE)
	    rst = RTS_WaitScanEnd (dev, 15000);

	  dev->status->parkhome = FALSE;
	}

      free (Regs);
    }

  DBG (DBG_FNC, "- Head_ParkHome: %i:\n", rst);

  return rst;
}

static SANE_Int
Motor_Move (struct st_device *dev, SANE_Byte * Regs,
	    struct st_motormove *mymotor, struct st_motorpos *mtrpos)
{
  SANE_Byte *cpRegs;
  SANE_Int rst;

  DBG (DBG_FNC, "+ Motor_Move:\n");

  rst = ERROR;

  cpRegs = (SANE_Byte *) malloc (RT_BUFFER_LEN * sizeof (SANE_Byte));
  if (cpRegs != NULL)
    {
      SANE_Int data, v12dcf8, coord_y, step_type;

      memcpy (cpRegs, Regs, RT_BUFFER_LEN * sizeof (SANE_Byte));
      v12dcf8 = 0;

      /* resolution = 1 dpi */
      data_bitset (&cpRegs[0xc0], 0x1f, 1);	     /*---xxxxx*/

      /* set motor step type */
      data_bitset (&cpRegs[0xd9], 0x70, mymotor->scanmotorsteptype);	      /*-xxx----*/

      /* set motor direction (polarity) */
      data_bitset (&cpRegs[0xd9], 0x80, mtrpos->options >> 3);	/*e------- */

      /* next value doesn't seem to have any effect */
      data_bitset (&cpRegs[0xd9], 0x0f, mtrpos->options);		       /*----efgh*/

      /* 0 enable/1 disable motor */
      data_bitset (&cpRegs[0xdd], 0x80, mtrpos->options >> 4);	/*d------- */

      /* next value doesn't seem to have any effect */
      data_bitset (&cpRegs[0xdd], 0x40, mtrpos->options >> 4);		       /*-d------*/

      switch (mymotor->scanmotorsteptype)
	{
	case STT_OCT:
	  step_type = 8;
	  break;
	case STT_QUART:
	  step_type = 4;
	  break;
	case STT_HALF:
	  step_type = 2;
	  break;
	case STT_FULL:
	  step_type = 1;
	  break;
	default:
	  step_type = 0;
	  break;		/* shouldn't be used */
	}

      coord_y = (mtrpos->coord_y * step_type) & 0xffff;
      if (coord_y < 2)
	coord_y = 2;

      /* Sets dummyline to 1 */
      data_bitset (&cpRegs[0xd6], 0xf0, 1);

      /* set step_size - 1 */
      cpRegs[0xe0] = 0;

      cpRegs[0x01] &= 0xf9;
      cpRegs[0x01] |= (mtrpos->v12e448 & 1) << 2;

      /* set dummy scan */
      data_bitset (&cpRegs[0x01], 0x10, 1);	     /*---x----*/

      /* set samplerate */
      data_bitset (&cpRegs[0x1cf], 0x40, PIXEL_RATE);	       /*-x------*/

      /* unknown data */
      data_bitset (&cpRegs[0x1cf], 0x80, 1);	/*x------- */

      /* sets one chanel per color */
      data_bitset (&cpRegs[0x12], 0x3f, 0);	/* channel   */
      data_bitset (&cpRegs[0x12], 0xc0, 1);	/* 1 channel */

      /* timing cnpp */
      data_bitset (&cpRegs[0x96], 0x3f, 0x0b);		/*--001011*/

      /* set systemclock */
      data_bitset (&cpRegs[0x00], 0x0f, mymotor->systemclock);		/*----xxxx*/

      /* set last step of accurve.smearing table to 2 */
      data_lsb_set (&cpRegs[0xe4], 2, 3);

      /* set last step of deccurve.scanbufferfull table to 16 */
      data_lsb_set (&Regs[0xea], 0x10, 3);

      /* set last step of deccurve.normalscan table to 16 */
      data_lsb_set (&Regs[0xed], 0x10, 3);

      /* set last step of deccurve.smearing table to 16 */
      data_lsb_set (&Regs[0xf0], 0x10, 3);

      /* set last step of deccurve.parkhome table to 16 */
      data_lsb_set (&Regs[0xf3], 0x10, 3);

      /* set msi */
      cpRegs[0xda] = 2;
      cpRegs[0xdd] &= 0xfc;

      /* set if motor has motorcurves */
      data_bitset (&cpRegs[0xdf], 0x10,
		   ((mymotor->motorcurve != -1) ? 1 : 0));

      if (mymotor->motorcurve != -1)
	{
	  struct st_curve *crv;

	  /* set last step of accurve.normalscan table */
	  crv =
	    Motor_Curve_Get (dev, mymotor->motorcurve, ACC_CURVE,
			     CRV_NORMALSCAN);
	  if (crv != NULL)
	    data_lsb_set (&cpRegs[0xe1], crv->step[crv->step_count - 1], 3);

	  DBG (DBG_FNC, " -> Setting up stepper motor using motorcurve %i\n",
	       mymotor->motorcurve);
	  v12dcf8 = Motor_Setup_Steps (dev, cpRegs, mymotor->motorcurve);

	  /* set step_size - 1 */
	  cpRegs[0xe0] = 0;

	  crv =
	    Motor_Curve_Get (dev, mymotor->motorcurve, DEC_CURVE,
			     CRV_NORMALSCAN);
	  if (crv != NULL)
	    coord_y -= (v12dcf8 + crv->step_count);

	  /* set line exposure time */
	  data_lsb_set (&cpRegs[0x30], mymotor->ctpc, 3);

	  /* set last step of accurve.smearing table */
	  data_lsb_set (&cpRegs[0xe4], 0, 3);
	}
      else
	{
	  /* Setting some motor step */
	  SANE_Int some_step;

	  switch (Regs[0x00] & 0x0f)
	    {
	    case 0x00:
	      some_step = 0x00895440;
	      break;		/*  3 x 0x2DC6C0 */
	    case 0x08:
	    case 0x01:
	      some_step = 0x00b71b00;
	      break;		/*  4 x 0x2DC6C0 */
	    case 0x02:
	      some_step = 0x0112a880;
	      break;		/*  6 x 0x2DC6C0 */
	    case 0x0a:
	    case 0x03:
	      some_step = 0x016e3600;
	      break;		/*  8 x 0x2DC6C0 */
	    case 0x04:
	      some_step = 0x02255100;
	      break;		/* 12 x 0x2DC6C0 */
	    case 0x0c:
	      some_step = 0x02dc6c00;
	      break;		/* 16 x 0x2DC6C0 */
	    case 0x05:
	      some_step = 0x044aa200;
	      break;		/* 24 x 0x2DC6C0 */
	    case 0x0d:
	      some_step = 0x05b8d800;
	      break;		/* 32 x 0x2DC6C0 */

	    case 0x09:
	      some_step = 0x00f42400;
	      break;
	    case 0x0b:
	      some_step = 0x01e84800;
	      break;		/* = case 9 * 2 */
	    default:
	      some_step = 0x0478f7f8;
	      break;
	    }

	  /* divide by timing.cnpp */
	  some_step /= ((cpRegs[0x96] & 0x3f) + 1);
	  if (mymotor->ctpc > 0)
	    some_step /= mymotor->ctpc;

	  /* set line exposure time */
	  data_lsb_set (&cpRegs[0x30], some_step, 3);

	  /* set last step of accurve.normalscan table */
	  data_lsb_set (&cpRegs[0xe1], some_step, 3);
	}

      /* Setting coords */
      RTS_Setup_Coords (cpRegs, 100, coord_y - 1, 800, 1);

      /* enable head movement */
      data_bitset (&cpRegs[0xd8], 0x80, 1);

      /* release motor before executing */
      Motor_Release (dev);

      RTS_Warm_Reset (dev);

      /* action! */
      data = RTS_WriteRegs (dev->usb_handle, cpRegs);
      if (data == OK)
	RTS_Execute (dev);

      /* wait 10 seconds */
      RTS_WaitScanEnd (dev, 10000);

      rst = (data != OK) ? v12dcf8 : RTS_WaitScanEnd (dev, 20000);

      free (cpRegs);
    }

  DBG (DBG_FNC, "- Motor_Move: %i\n", rst);

  return rst;
}

static void
Free_Motormoves (struct st_device *dev)
{
  DBG (DBG_FNC, "> Free_Motormoves\n");

  if (dev->motormove != NULL)
    {
      SANE_Int a;
      struct st_motormove *ms;

      for (a = 0; a < dev->motormove_count; a++)
	{
	  ms = dev->motormove[a];
	  if (ms != NULL)
	    free (ms);
	}

      free (dev->motormove);
      dev->motormove = NULL;
    }

  dev->motormove_count = 0;
}

static void
Free_MotorCurves (struct st_device *dev)
{
  DBG (DBG_FNC, "> Free_MotorCurves\n");
  if (dev->mtrsetting != NULL)
    Motor_Curve_Free (dev->mtrsetting, &dev->mtrsetting_count);

  dev->mtrsetting = NULL;
  dev->mtrsetting_count = 0;
}

static SANE_Int
Load_MotorCurves (struct st_device *dev)
{
  SANE_Int rst = ERROR;
  SANE_Int *mtc = NULL;

  if (dev->mtrsetting != NULL)
    Free_MotorCurves (dev);

  DBG (DBG_FNC, "> Load_MotorCurves\n");

  /* get motor setttings buffer for this device */
  mtc = cfg_motorcurve_get ();
  if (mtc != NULL)
    {
      /* parse buffer to get all motorcurves */
      dev->mtrsetting = Motor_Curve_Parse (&dev->mtrsetting_count, mtc);
      if (dev->mtrsetting != NULL)
	rst = OK;
    }

  if (rst != ERROR)
    {
      DBG (DBG_FNC, " -> Found %i motor settings\n", dev->mtrsetting_count);
      dbg_motorcurves (dev);
    }
  else
    DBG (DBG_ERR, "- Load_MotorCurves error!!\n");

  return rst;
}

static SANE_Int
Load_Motormoves (struct st_device *dev)
{
  SANE_Int rst = OK;
  SANE_Int a;
  struct st_motormove reg, *mm;

  DBG (DBG_FNC, "> Load_Motormoves\n");

  /* if there is already any movement loaded let's free it */
  if (dev->motormove != NULL)
    Free_Motormoves (dev);

  a = 0;
  while ((cfg_motormove_get (dev->sensorcfg->type, a, &reg) != ERROR)
	 && (rst == OK))
    {
      dev->motormove_count++;
      dev->motormove =
	(struct st_motormove **) realloc (dev->motormove,
					  sizeof (struct st_motormove **) *
					  dev->motormove_count);
      if (dev->motormove != NULL)
	{
	  mm = (struct st_motormove *) malloc (sizeof (struct st_motormove));
	  if (mm != NULL)
	    {
	      memcpy (mm, &reg, sizeof (struct st_motormove));
	      dev->motormove[dev->motormove_count - 1] = mm;
	    }
	  else
	    rst = ERROR;
	}
      else
	rst = ERROR;

      a++;
    }

  if (rst == ERROR)
    Free_Motormoves (dev);

  DBG (DBG_FNC, " -> Found %i motormoves\n", dev->motormove_count);
  dbg_motormoves (dev);

  return rst;
}

static SANE_Byte *
Motor_AddStep (SANE_Byte * steps, SANE_Int * bwriten, SANE_Int step)
{
  steps = (SANE_Byte *) realloc (steps, sizeof (SANE_Byte) * (*bwriten + 3));
  if (steps != NULL)
    {
      data_msb_set (&steps[*bwriten], step, 3);
      *bwriten += 3;
    }
  else
    *bwriten = 0;

  return steps;
}

static SANE_Int
Motor_Setup_Steps (struct st_device *dev, SANE_Byte * Regs,
		   SANE_Int mysetting)
{
  SANE_Int varx10, cont, last_acc_step, varx20, stepbuffer_size,
    mystep, bwriten;
  SANE_Int myvar, var1, myvalor, mybwriten;
  struct st_curve *mycurve;
  SANE_Byte *steps;

  DBG (DBG_FNC, "+ Motor_Setup_Steps(*Regs, motorsetting=%i):\n", mysetting);

  varx10 = 0;
  cont = 0;
  varx20 = 0;
  stepbuffer_size = (v15f8 << 4) & 0xffff;
  steps = NULL;
  bwriten = 0;
  deccurvecount = 0;
  acccurvecount = 0;
  last_acc_step = 0;

  /* mycurve points to acc normalscan steps table */
  mycurve = Motor_Curve_Get (dev, mysetting, ACC_CURVE, CRV_NORMALSCAN);

  if (mycurve != NULL)
    {
      /* acccurvecount has the number of steps in acc normalscan table */
      acccurvecount = mycurve->step_count;

      /* get last acccurve step from acc.normalscan step table */
      last_acc_step = data_lsb_get (&Regs[0xe1], 3);

      /* sets pointer to acc.normalscan step table */
      data_wide_bitset (&Regs[0xf6], 0x3fff, stepbuffer_size);

      /* Separate each step in three bytes */
      if (mycurve->step_count > 0)
	for (cont = 0; cont < mycurve->step_count; cont++)
	  {
	    mystep = mycurve->step[cont];
	    if (mystep <= last_acc_step)
	      {
		acccurvecount = cont;
		break;
	      }
	    varx20 += mystep + 1;
	    steps = Motor_AddStep (steps, &bwriten, mystep);
	  }
    }

  if (acccurvecount == 0)
    {
      /* Write one step (last_acc_step + 0x01) to buffer */
      acccurvecount++;
      varx20 += (last_acc_step + 1) + 1;
      steps = Motor_AddStep (steps, &bwriten, last_acc_step + 1);
    }

  /* write another step (last_acc_step) */
  acccurvecount++;
  varx20 += last_acc_step + 1;
  steps = Motor_AddStep (steps, &bwriten, last_acc_step);

  /* get line exposure time */
  myvar = data_lsb_get (&Regs[0x30], 3) + 1;

  var1 = (varx20 + myvar - 1) / myvar;
  var1 = ((var1 * myvar) + mycurve->step[0] - varx20) - 0x0d;
  if (steps != NULL)
    data_msb_set (&steps[0], var1, 3);

  /* dec.scanbufferfull step table */
  /* set pointer to next table */
  stepbuffer_size += (acccurvecount * 3);
  data_wide_bitset (&Regs[0xf8], 0x3fff, stepbuffer_size);

  /* set last step of deccurve.scanbufferfull table */
  mycurve = Motor_Curve_Get (dev, mysetting, DEC_CURVE, CRV_BUFFERFULL);
  deccurvecount = mycurve->step_count;
  data_lsb_set (&Regs[0xea], mycurve->step[mycurve->step_count - 1], 3);

  /* write another step mycurve->step_count,cont,last_acc_step */
  deccurvecount++;
  steps = Motor_AddStep (steps, &bwriten, last_acc_step);

  /* Separate each step in three bytes */
  if (mycurve->step_count > 1)
    for (cont = 0; cont < (mycurve->step_count - 1); cont++)
      {
	mystep = mycurve->step[cont];
	if (mystep > last_acc_step)
	  steps = Motor_AddStep (steps, &bwriten, mystep);
	else
	  deccurvecount--;
      }

  myvalor = dev->mtrsetting[mysetting]->motorbackstep;
  if (myvalor > 0)
    {
      SANE_Int step_size = _B0 (Regs[0xe0]) + 1;

      myvalor = ((myvalor - deccurvecount) - acccurvecount) + 2;
      varx10 = myvalor;
      myvalor /= step_size;
      myvalor *= step_size;
      var1 = mycurve->step[mycurve->step_count - 1];	/* last deccurve step */
      if (last_acc_step >= var1)
	var1 = last_acc_step + 1;
      deccurvecount += (varx10 - myvalor);
      myvalor = varx10 - myvalor;
    }
  else
    myvalor = varx10;

  if (myvalor > 0)
    for (cont = myvalor; cont > 0; cont--)
      steps = Motor_AddStep (steps, &bwriten, var1 - 1);

  /* write another step , bwriten tiene 4b */
  steps = Motor_AddStep (steps, &bwriten, var1);

  /* acc.smearing step table */
  if (Motor_Curve_Get (dev, mysetting, ACC_CURVE, CRV_SMEARING) != NULL)
    {
      /* acc.smearing curve enabled */
      if (Motor_Curve_Equal
	  (dev, mysetting, ACC_CURVE, CRV_SMEARING, CRV_NORMALSCAN) == TRUE)
	{
	  /* acc.smearing pointer points to acc.normalscan table */
	  data_wide_bitset (&Regs[0xfa], 0x3fff,
			    data_lsb_get (&Regs[0xf6], 2));
	  /* last step of acc.smearing table is the same as acc.normalscan */
	  data_lsb_set (&Regs[0xe4], data_lsb_get (&Regs[0xe1], 3), 3);
	}
      else
	{
	  /* set pointer to next step table */
	  stepbuffer_size += (deccurvecount * 3);
	  data_wide_bitset (&Regs[0xfa], 0x3fff, stepbuffer_size);

	  /* set last step of acc.smearing table */
	  mycurve = Motor_Curve_Get (dev, mysetting, ACC_CURVE, CRV_SMEARING);
	  if (mycurve != NULL)
	    {
	      smearacccurvecount = mycurve->step_count;
	      data_lsb_set (&Regs[0xe4],
			    mycurve->step[mycurve->step_count - 1], 3);

	      /* generate acc.smearing table */
	      if (mycurve->step_count > 0)
		for (cont = 0; cont < mycurve->step_count; cont++)
		  steps =
		    Motor_AddStep (steps, &bwriten, mycurve->step[cont]);
	    }
	}
    }
  else
    {
      /* acc.smearing curve disabled */
      data_wide_bitset (&Regs[0xfa], 0x3fff, 0);
    }

  /* dec.smearing */
  if (Motor_Curve_Get (dev, mysetting, DEC_CURVE, CRV_SMEARING) != NULL)
    {
      /* dec.smearing curve enabled */
      if (Motor_Curve_Equal
	  (dev, mysetting, DEC_CURVE, CRV_SMEARING, CRV_BUFFERFULL) == TRUE)
	{
	  /* dec.smearing pointer points to dec.scanbufferfull table */
	  data_wide_bitset (&Regs[0x00fc], 0x3fff,
			    data_lsb_get (&Regs[0x00f8], 2));
	  /* last step of dec.smearing table is the same as dec.scanbufferfull */
	  data_lsb_set (&Regs[0x00f0], data_lsb_get (&Regs[0x00ea], 3), 3);
	}
      else
	{
	  /* set pointer to next step table */
	  if (mycurve != NULL)
	    stepbuffer_size += (mycurve->step_count * 3);
	  data_wide_bitset (&Regs[0xfc], 0x3fff, stepbuffer_size);

	  /* set last step of dec.smearing table */
	  mycurve = Motor_Curve_Get (dev, mysetting, DEC_CURVE, CRV_SMEARING);
	  if (mycurve != NULL)
	    {
	      smeardeccurvecount = mycurve->step_count;
	      data_lsb_set (&Regs[0xf0],
			    mycurve->step[mycurve->step_count - 1], 3);

	      /* generate dec.smearing table */
	      if (mycurve->step_count > 0)
		for (cont = 0; cont < mycurve->step_count; cont++)
		  steps =
		    Motor_AddStep (steps, &bwriten, mycurve->step[cont]);
	    }
	}
    }
  else
    {
      /* dec.smearing curve disabled */
      data_wide_bitset (&Regs[0x00fc], 0x3fff, 0);
    }

  /* dec.normalscan */
  if (Motor_Curve_Get (dev, mysetting, DEC_CURVE, CRV_NORMALSCAN) != NULL)
    {
      /* dec.normalscan enabled */
      if (Motor_Curve_Equal
	  (dev, mysetting, DEC_CURVE, CRV_NORMALSCAN, CRV_BUFFERFULL) == TRUE)
	{
	  /* dec.normalscan pointer points to dec.scanbufferfull table */
	  data_wide_bitset (&Regs[0xfe], 0x3fff,
			    data_lsb_get (&Regs[0xf8], 2));
	  /* last step of dec.normalscan table is the same as dec.scanbufferfull */
	  data_lsb_set (&Regs[0xed], data_lsb_get (&Regs[0xea], 3), 3);
	}
      else
	if (Motor_Curve_Equal
	    (dev, mysetting, DEC_CURVE, CRV_NORMALSCAN, CRV_SMEARING) == TRUE)
	{
	  /* dec.normalscan pointer points to dec.smearing table */
	  data_wide_bitset (&Regs[0xfe], 0x3fff,
			    data_lsb_get (&Regs[0xfc], 2));
	  /* last step of dec.normalscan table is the same as dec.smearing */
	  data_lsb_set (&Regs[0xed], data_lsb_get (&Regs[0xf0], 3), 3);
	}
      else
	{
	  /* set pointer to next step table */
	  if (mycurve != NULL)
	    stepbuffer_size += (mycurve->step_count * 3);
	  data_wide_bitset (&Regs[0xfe], 0x3fff, stepbuffer_size);

	  /* set last step of dec.normalscan table */
	  mycurve =
	    Motor_Curve_Get (dev, mysetting, DEC_CURVE, CRV_NORMALSCAN);
	  if (mycurve != NULL)
	    {
	      data_lsb_set (&Regs[0xed],
			    mycurve->step[mycurve->step_count - 1], 3);

	      /* generate dec.normalscan table */
	      if (mycurve->step_count > 0)
		for (cont = 0; cont < mycurve->step_count; cont++)
		  steps =
		    Motor_AddStep (steps, &bwriten, mycurve->step[cont]);
	    }
	}
    }
  else
    {
      /* dec.normalscan disabled */
      data_wide_bitset (&Regs[0xfe], 0x3fff, 0);
    }

  /* acc.parkhome */
  if (Motor_Curve_Get (dev, mysetting, ACC_CURVE, CRV_PARKHOME) != NULL)
    {
      /* parkhome curve enabled */

      if (Motor_Curve_Equal
	  (dev, mysetting, ACC_CURVE, CRV_PARKHOME, CRV_NORMALSCAN) == TRUE)
	{
	  /* acc.parkhome pointer points to acc.normalscan table */
	  data_wide_bitset (&Regs[0x100], 0x3fff,
			    data_lsb_get (&Regs[0xf6], 2));

	  /* last step of acc.parkhome table is the same as acc.normalscan */
	  data_lsb_set (&Regs[0xe7], data_lsb_get (&Regs[0xe1], 3), 3);
	}
      else
	if (Motor_Curve_Equal
	    (dev, mysetting, ACC_CURVE, CRV_PARKHOME, CRV_SMEARING) == TRUE)
	{
	  /* acc.parkhome pointer points to acc.smearing table */
	  data_wide_bitset (&Regs[0x100], 0x3fff,
			    data_lsb_get (&Regs[0xfa], 2));
	  /* last step of acc.parkhome table is the same as acc.smearing */
	  data_lsb_set (&Regs[0xe7], data_lsb_get (&Regs[0xe4], 3), 3);
	}
      else
	{
	  /* set pointer to next step table */
	  if (mycurve != NULL)
	    stepbuffer_size += (mycurve->step_count * 3);
	  data_wide_bitset (&Regs[0x100], 0x3fff, stepbuffer_size);

	  /* set last step of acc.parkhome table */
	  mycurve = Motor_Curve_Get (dev, mysetting, ACC_CURVE, CRV_PARKHOME);
	  if (mycurve != NULL)
	    {
	      data_lsb_set (&Regs[0xe7],
			    mycurve->step[mycurve->step_count - 1], 3);

	      /* generate acc.parkhome table */
	      if (mycurve->step_count > 0)
		for (cont = 0; cont < mycurve->step_count; cont++)
		  steps =
		    Motor_AddStep (steps, &bwriten, mycurve->step[cont]);
	    }
	}
    }
  else
    {
      /* parkhome curve is disabled */
      /* acc.parkhome pointer points to 0 */
      data_wide_bitset (&Regs[0x100], 0x3fff, 0);
      data_lsb_set (&Regs[0xe7], 16, 3);
    }

  /* dec.parkhome */
  if (Motor_Curve_Get (dev, mysetting, DEC_CURVE, CRV_PARKHOME) != NULL)
    {
      /* parkhome curve enabled */
      if (Motor_Curve_Equal
	  (dev, mysetting, DEC_CURVE, CRV_PARKHOME, CRV_BUFFERFULL) == TRUE)
	{
	  /* dec.parkhome pointer points to dec.scanbufferfull table */
	  data_wide_bitset (&Regs[0x102], 0x3fff,
			    data_lsb_get (&Regs[0xf8], 2));
	  /* last step of dec.parkhome table is the same as dec.scanbufferfull */
	  data_lsb_set (&Regs[0xf3], data_lsb_get (&Regs[0xe4], 3), 3);
	}
      else
	if (Motor_Curve_Equal
	    (dev, mysetting, DEC_CURVE, CRV_PARKHOME, CRV_SMEARING) == TRUE)
	{
	  /* dec.parkhome pointer points to dec.smearing table */
	  data_wide_bitset (&Regs[0x102], 0x3fff,
			    data_lsb_get (&Regs[0xfe], 2));
	  /* last step of dec.parkhome table is the same as dec.smearing */
	  data_lsb_set (&Regs[0xf3], data_lsb_get (&Regs[0xf0], 3), 3);
	}
      else
	if (Motor_Curve_Equal
	    (dev, mysetting, DEC_CURVE, CRV_PARKHOME, CRV_NORMALSCAN) == TRUE)
	{
	  /* dec.parkhome pointer points to dec.normalscan table */
	  data_wide_bitset (&Regs[0x102], 0x3fff,
			    data_lsb_get (&Regs[0xfe], 2));
	  /* last step of dec.parkhome table is the same as dec.normalscan */
	  data_lsb_set (&Regs[0xf3], data_lsb_get (&Regs[0xed], 3), 3);
	}
      else
	{
	  /* set pointer to next step table */
	  if (mycurve != NULL)
	    stepbuffer_size += (mycurve->step_count * 3);
	  data_wide_bitset (&Regs[0x102], 0x3fff, stepbuffer_size);

	  /* set last step of dec.parkhome table */
	  mycurve = Motor_Curve_Get (dev, mysetting, DEC_CURVE, CRV_PARKHOME);
	  if (mycurve != NULL)
	    {
	      data_lsb_set (&Regs[0xf3],
			    mycurve->step[mycurve->step_count - 1], 3);

	      /* generate dec.parkhome table */
	      if (mycurve->step_count > 0)
		for (cont = 0; cont < mycurve->step_count; cont++)
		  steps =
		    Motor_AddStep (steps, &bwriten, mycurve->step[cont]);
	    }
	}
    }
  else
    {
      /* parkhome curve is disabled */

      /* dec.parkhome pointer points to 0 */
      data_wide_bitset (&Regs[0x102], 0x3fff, 0);
      data_lsb_set (&Regs[0xf3], 16, 3);
    }

  mybwriten = bwriten & 0x8000000f;
  if (mybwriten < 0)
    mybwriten = ((mybwriten - 1) | 0xfffffff0) + 1;

  if (mybwriten != 0)
    bwriten = (((bwriten & 0xffff) >> 0x04) + 1) << 0x04;
  bwriten = bwriten & 0xffff;

  /* display table */
  DBG (DBG_FNC, " -> Direction Type           Offset Last step\n");
  DBG (DBG_FNC, " -> --------- -------------- ------ ---------\n");
  DBG (DBG_FNC, " -> ACC_CURVE CRV_NORMALSCAN %6i  %6i\n",
       data_lsb_get (&Regs[0x0f6], 2) & 0x3fff, data_lsb_get (&Regs[0x0e1],
							      3));
  DBG (DBG_FNC, " -> ACC_CURVE CRV_SMEARING   %6i  %6i\n",
       data_lsb_get (&Regs[0x0fa], 2) & 0x3fff, data_lsb_get (&Regs[0x0e4],
							      3));
  DBG (DBG_FNC, " -> ACC_CURVE CRV_PARKHOME   %6i  %6i\n",
       data_lsb_get (&Regs[0x100], 2) & 0x3fff, data_lsb_get (&Regs[0x0e7],
							      3));
  DBG (DBG_FNC, " -> DEC_CURVE CRV_NORMALSCAN %6i  %6i\n",
       data_lsb_get (&Regs[0x0fe], 2) & 0x3fff, data_lsb_get (&Regs[0x0ed],
							      3));
  DBG (DBG_FNC, " -> DEC_CURVE CRV_SMEARING   %6i  %6i\n",
       data_lsb_get (&Regs[0x0fc], 2) & 0x3fff, data_lsb_get (&Regs[0x0f0],
							      3));
  DBG (DBG_FNC, " -> DEC_CURVE CRV_PARKHOME   %6i  %6i\n",
       data_lsb_get (&Regs[0x102], 2) & 0x3fff, data_lsb_get (&Regs[0x0f3],
							      3));
  DBG (DBG_FNC, " -> DEC_CURVE CRV_BUFFERFULL %6i  %6i\n",
       data_lsb_get (&Regs[0x0f8], 2) & 0x3fff, data_lsb_get (&Regs[0x0ea],
							      3));

  RTS_Warm_Reset (dev);

  /* send motor steps */
  if (steps != NULL)
    {
      if (bwriten > 0)
	{
	  /* lock */
	  SetLock (dev->usb_handle, Regs, TRUE);

	  /* send steps */
	  RTS_DMA_Write (dev, 0x0000, v15f8, bwriten, steps);

	  /* unlock */
	  SetLock (dev->usb_handle, Regs, FALSE);
	}

      free (steps);
    }

  DBG (DBG_FNC, "- Motor_Setup_Steps: %i\n", acccurvecount);

  return acccurvecount;
}

static SANE_Int
Lamp_PWM_use (struct st_device *dev, SANE_Int enable)
{
  SANE_Byte a, b;
  SANE_Int rst = ERROR;

  DBG (DBG_FNC, "+ Lamp_PWM_use(enable=%i):\n", enable);

  if (Read_Byte (dev->usb_handle, 0xe948, &a) == OK)
    {
      if (Read_Byte (dev->usb_handle, 0xe9e0, &b) == OK)
	{
	  if (enable != 0)
	    {
	      if (pwmlamplevel != 0x00)
		{
		  b |= 0x80;
		  dev->init_regs[0x01e0] &= 0x3f;
		  dev->init_regs[0x01e0] |= 0x80;
		}
	      else
		{
		  a |= 0x40;
		  b &= 0x3f;
		  dev->init_regs[0x0148] |= 0x40;
		  dev->init_regs[0x01e0] &= 0x3f;
		}
	    }
	  else
	    {
	      b &= 0x7f;
	      a &= 0xbf;
	    }

	  if (Write_Byte (dev->usb_handle, 0xe948, a) == OK)
	    rst = Write_Byte (dev->usb_handle, 0xe9e0, b);
	}
    }

  DBG (DBG_FNC, "- Lamp_PWM_use: %i\n", rst);

  return rst;
}

static SANE_Int
SSCG_Enable (struct st_device *dev)
{
  SANE_Int rst;
  SANE_Int sscg;
  SANE_Byte data1, data2;
  SANE_Int enable, mode, clock;

  DBG (DBG_FNC, "+ SSCG_Enable:\n");

  rst = cfg_sscg_get (&enable, &mode, &clock);

  if (rst == OK)
    {
      if ((Read_Byte (dev->usb_handle, 0xfe3a, &data1) == OK)
	  && (Read_Byte (dev->usb_handle, 0xfe39, &data2) == OK))
	{
	  if (enable != FALSE)
	    {
	      /* clock values: 0=0.25%; 1=0.50%; 2=0.75%; 3=1.00% */
	      data2 = (mode == 0) ? data2 & 0x7f : data2 | 0x80;

	      sscg = (data1 & 0xf3) | (((clock & 0x03) | 0x04) << 0x02);
	      sscg = (sscg << 8) | data2;
	    }
	  else
	    sscg = ((data1 & 0xef) << 8) | _B0 (data2);

	  rst = Write_Word (dev->usb_handle, 0xfe39, sscg);
	}
      else
	rst = ERROR;
    }

  DBG (DBG_FNC, "- SSCG_Enable: %i\n", rst);

  return rst;
}

static void
RTS_Setup_RefVoltages (struct st_device *dev, SANE_Byte * Regs)
{
  /* this function sets top, midle and bottom reference voltages */

  DBG (DBG_FNC, "> RTS_Setup_RefVoltages\n");

  if (Regs != NULL)
    {
      SANE_Byte vrts, vrms, vrbs;

      cfg_refvoltages_get (dev->sensorcfg->type, &vrts, &vrms, &vrbs);

      /* Top Reference Voltage */
      data_bitset (&Regs[0x14], 0xe0, vrts);	/*xxx----- */

      /* Middle Reference Voltage */
      data_bitset (&Regs[0x15], 0xe0, vrms);	/*xxx----- */

      /* Bottom Reference Voltage */
      data_bitset (&Regs[0x16], 0xe0, vrbs);	/*xxx----- */
    }
}

static SANE_Int
Init_USBData (struct st_device *dev)
{
  SANE_Byte data;
  SANE_Byte *resource;

  DBG (DBG_FNC, "+ Init_USBData:\n");

  if (Read_Byte (dev->usb_handle, 0xf8ff, &data) != OK)
    return ERROR;

  data = (data | 1);
  if (Write_Byte (dev->usb_handle, 0xf8ff, data) != OK)
    return ERROR;

  if (SSCG_Enable (dev) != OK)
    return ERROR;

  Init_Registers (dev);

  /* Gamma table size = 0x100 */
  data_bitset (&dev->init_regs[0x1d0], 0x30, 0x00);	 /*--00----*/

  /* Set 3 channels_per_dot */
  data_bitset (&dev->init_regs[0x12], 0xc0, 0x03);	/*xx------ */

  /* systemclock */
  data_bitset (&dev->init_regs[0x00], 0x0f, 0x05);	 /*----xxxx*/

  /* timing cnpp */
  data_bitset (&dev->init_regs[0x96], 0x3f, 0x17);	 /*--xxxxxx*/

  /* set sensor_channel_color_order */
  data_bitset (&dev->init_regs[0x60a], 0x7f, 0x24);	 /*-xxxxxxx*/

  /* set crvs */
  data_bitset (&dev->init_regs[0x10], 0x1f, get_value (SCAN_PARAM, CRVS, 7, usbfile));	   /*---xxxxx*/

  /* set reference voltages */
  RTS_Setup_RefVoltages (dev, dev->init_regs);

  dev->init_regs[0x11] |= 0x10;

  data_bitset (&dev->init_regs[0x26], 0x70, 5);	     /*-101----*/

  dev->init_regs[0x185] = 0x88;
  dev->init_regs[0x186] = 0x88;

  /* SDRAM clock */
  data = get_value (SCAN_PARAM, MCLKIOC, 8, usbfile);
  data_bitset (&dev->init_regs[0x187], 0x0f, 0x08);	 /*----xxxx*/
  data_bitset (&dev->init_regs[0x187], 0xf0, data);	/*xxxx---- */

  data--;

  if (data < 7)
    {
      switch (data)
	{
	case 0:
	  data |= 0xc0;
	  break;
	case 1:
	  data |= 0xa0;
	  break;
	case 2:
	  data |= 0xe0;
	  break;
	case 3:
	  data |= 0x90;
	  break;
	case 4:
	  data |= 0xd0;
	  break;
	case 5:
	  data |= 0xb0;
	  break;
	case 6:
	  data = (data & 0x0f);
	  break;
	}
      dev->init_regs[0x187] = _B0 (data);
    }

  data_bitset (&dev->init_regs[0x26], 0x0f, 0);	     /*----0000*/

  dev->init_regs[0x27] &= 0x3f;
  dev->init_regs[0x29] = 0x24;
  dev->init_regs[0x2a] = 0x10;
  dev->init_regs[0x150] = 0xff;
  dev->init_regs[0x151] = 0x13;
  dev->init_regs[0x156] = 0xf0;
  dev->init_regs[0x157] = 0xfd;

  if (dev->motorcfg->changemotorcurrent != FALSE)
    Motor_Change (dev, dev->init_regs, 3);

  dev->init_regs[0xde] = 0;
  data_bitset (&dev->init_regs[0xdf], 0x0f, 0);

  /* loads motor resource for this dev */
  resource = cfg_motor_resource_get (&data);
  if ((resource != NULL) && (data > 1))
    memcpy (&dev->init_regs[0x104], resource, data);

  /* this bit is set but I don't know its purpose */
  dev->init_regs[0x01] |= 0x40;	      /*-1------*/

  dev->init_regs[0x124] = 0x94;

  /* release motor */
  Motor_Release (dev);

  DBG (DBG_FNC, "- Init_USBData:\n");

  return OK;
}

static SANE_Int
Init_Registers (struct st_device *dev)
{
  SANE_Int rst = OK;

  DBG (DBG_FNC, "+ Init_Registers:\n");

  /* Lee dev->init_regs */
  bzero (dev->init_regs, RT_BUFFER_LEN);
  RTS_ReadRegs (dev->usb_handle, dev->init_regs);
  Read_FE3E (dev, &v1619);

  if (dev->sensorcfg->type == CCD_SENSOR)
    {
      /* CCD sensor */
      data_bitset (&dev->init_regs[0x11], 0xc0, 0);	/*xx------ */
      data_bitset (&dev->init_regs[0x146], 0x80, 1);	/*x------- */
      data_bitset (&dev->init_regs[0x146], 0x40, 1);		/*-x------*/

    }
  else
    {
      /* CIS sensor */
      data_bitset (&dev->init_regs[0x146], 0x80, 0);	/*0------- */
      data_bitset (&dev->init_regs[0x146], 0x40, 0);		/*-0------*/
      data_bitset (&dev->init_regs[0x11], 0xc0, 2);	/*xx------ */
      data_bitset (&dev->init_regs[0xae], 0x3f, 0x14);		/*--xxxxxx*/
      data_bitset (&dev->init_regs[0xaf], 0x07, 1);		/*-----xxx*/

      dev->init_regs[0x9c] = dev->init_regs[0xa2] = dev->init_regs[0xa8] =
	(RTS_Debug->dev_model != UA4900) ? 1 : 0;
      dev->init_regs[0x9d] = dev->init_regs[0xa3] = dev->init_regs[0xa9] = 0;
      dev->init_regs[0x9e] = dev->init_regs[0xa4] = dev->init_regs[0xaa] = 0;
      dev->init_regs[0x9f] = dev->init_regs[0xa5] = dev->init_regs[0xab] = 0;
      dev->init_regs[0xa0] = dev->init_regs[0xa6] = dev->init_regs[0xac] = 0;
      dev->init_regs[0xa1] = dev->init_regs[0xa7] = dev->init_regs[0xad] =
	(RTS_Debug->dev_model != UA4900) ? 0x80 : 0;
    }

  /* disable CCD channels */
  data_bitset (&dev->init_regs[0x10], 0xe0, 0);	/*xxx----- */
  data_bitset (&dev->init_regs[0x13], 0x80, 0);	/*x------- */

  /* enable timer to switch off lamp */
  data_bitset (&dev->init_regs[0x146], 0x10, 1);	 /*---x----*/

  /* set time to switch off lamp */
  dev->init_regs[0x147] = 0xff;

  /* set last acccurve step */
  data_lsb_set (&dev->init_regs[0xe1], 0x2af8, 3);

  /* set msi 0x02 */
  dev->init_regs[0xda] = 0x02;
  data_bitset (&dev->init_regs[0xdd], 0x03, 0);		/*------xx*/

  /* set binary threshold high and low in little endian */
  data_lsb_set (&dev->init_regs[0x19e], binarythresholdl, 2);
  data_lsb_set (&dev->init_regs[0x1a0], binarythresholdh, 2);


  data_bitset (&dev->init_regs[0x01], 0x08, 0);		/*----x---*/
  data_bitset (&dev->init_regs[0x16f], 0x40, 0);	/*-x------*/
  dev->init_regs[0x0bf] = (dev->init_regs[0x00bf] & 0xe0) | 0x20;
  dev->init_regs[0x163] = (dev->init_regs[0x0163] & 0x3f) | 0x40;

  data_bitset (&dev->init_regs[0xd6], 0x0f, 8);		/*----xxxx*/
  data_bitset (&dev->init_regs[0x164], 0x80, 1);	/*x------- */

  dev->init_regs[0x0bc] = 0x00;
  dev->init_regs[0x0bd] = 0x00;

  dev->init_regs[0x165] = (dev->init_regs[0x0165] & 0x3f) | 0x80;
  dev->init_regs[0x0ed] = 0x10;
  dev->init_regs[0x0be] = 0x00;
  dev->init_regs[0x0d5] = 0x00;

  dev->init_regs[0xee] = 0x00;
  dev->init_regs[0xef] = 0x00;
  dev->init_regs[0xde] = 0xff;

  /* set bit[4] has_curves = 0 | bit[0..3] unknown = 0 */
  data_bitset (&dev->init_regs[0xdf], 0x10, 0);		/*---x----*/
  data_bitset (&dev->init_regs[0xdf], 0x0f, 0);		/*----xxxx*/

  /* Set motor type */
  data_bitset (&dev->init_regs[0xd7], 0x80, dev->motorcfg->type);	/*x------- */

  if (dev->motorcfg->type == MT_ONCHIP_PWM)
    {
      data_bitset (&dev->init_regs[0x14e], 0x10, 1);	      /*---x----*/

      /* set motorpwmfrequency */
      data_bitset (&dev->init_regs[0xd7], 0x3f, dev->motorcfg->pwmfrequency);	       /*--xxxxxx*/
    }

  dev->init_regs[0x600] &= 0xfb;
  dev->init_regs[0x1d8] |= 0x08;

  v160c_block_size = 0x04;
  mem_total = 0x80000;

  /* check and setup installed ram */
  RTS_DMA_CheckType (dev, dev->init_regs);
  rst = RTS_DMA_WaitReady (dev, 1500);

  DBG (DBG_FNC, "- Init_Registers: %i\n", rst);

  return rst;
}

static SANE_Int
Read_FE3E (struct st_device *dev, SANE_Byte * destino)
{
  SANE_Int rst;

  DBG (DBG_FNC, "+ Read_FE3E:\n");

  rst = ERROR;
  if (destino != NULL)
    {
      SANE_Byte data;
      if (Read_Byte (dev->usb_handle, 0xfe3e, &data) == 0)
	{
	  *destino = data;
	  rst = OK;
	  DBG (DBG_FNC, " -> %02x\n", _B0 (data));
	}
    }

  DBG (DBG_FNC, "- Read_FE3E: %i\n", rst);

  return rst;
}

static SANE_Int
Head_IsAtHome (struct st_device *dev, SANE_Byte * Regs)
{
  SANE_Int rst;

  DBG (DBG_FNC, "+ Head_IsAtHome:\n");

  /* if returns TRUE, lamp is at home. Otherwise it returns FALSE */
  rst = 0;

  if (Regs != NULL)
    {
      SANE_Byte data;
      if (Read_Byte (dev->usb_handle, 0xe96f, &data) == OK)
	{
	  Regs[0x16f] = _B0 (data);
	  rst = (data >> 6) & 1;
	}
    }

  rst = (rst == 1) ? TRUE : FALSE;

  DBG (DBG_FNC, "- Head_IsAtHome: %s\n", (rst == TRUE) ? "Yes" : "No");

  return rst;
}

static SANE_Byte
RTS_IsExecuting (struct st_device *dev, SANE_Byte * Regs)
{
  SANE_Byte rst;

  DBG (DBG_FNC, "+ RTS_IsExecuting:\n");

  rst = 0;

  if (Regs != NULL)
    {
      SANE_Byte data;
      if (Read_Byte (dev->usb_handle, 0xe800, &data) == OK)
	{
	  Regs[0x00] = data;
	  rst = (data >> 7) & 1;
	}
    }

  DBG (DBG_FNC, "- RTS_IsExecuting: %i\n", rst);

  return rst;
}

static SANE_Int
RTS_WaitScanEnd (struct st_device *dev, SANE_Int msecs)
{
  SANE_Byte data;
  SANE_Int rst;

  DBG (DBG_FNC, "+ RTS_WaitScanEnd(msecs=%i):\n", msecs);

  /* returns 0 if ok or timeout
     returns -1 if fails */

  rst = ERROR;

  if (Read_Byte (dev->usb_handle, 0xe800, &data) == OK)
    {
      long ticks = GetTickCount () + msecs;
      rst = OK;
      while (((data & 0x80) != 0) && (ticks > GetTickCount ()) && (rst == OK))
	{
	  rst = Read_Byte (dev->usb_handle, 0xe800, &data);
	}
    }

  DBG (DBG_FNC, "- RTS_WaitScanEnd: Ending with rst=%i\n", rst);

  return rst;
}

static SANE_Int
RTS_Enable_CCD (struct st_device *dev, SANE_Byte * Regs, SANE_Int channels)
{
  SANE_Int rst = ERROR;

  DBG (DBG_FNC, "+ RTS_Enable_CCD(*Regs, arg2=%i):\n", channels);

  if (Read_Buffer (dev->usb_handle, 0xe810, &Regs[0x10], 4) == OK)
    {
      data_bitset (&Regs[0x10], 0xe0, channels);	/*xxx----- */
      data_bitset (&Regs[0x13], 0x80, channels >> 3);	/*x------- */

      Write_Buffer (dev->usb_handle, 0xe810, &Regs[0x10], 4);
      rst = OK;
    }

  DBG (DBG_FNC, "- RTS_Enable_CCD: %i\n", rst);

  return rst;
}

static SANE_Int
RTS_Warm_Reset (struct st_device *dev)
{
  SANE_Byte data;
  SANE_Int rst;

  DBG (DBG_FNC, "+ RTS_Warm_Reset:\n");

  rst = ERROR;
  if (Read_Byte (dev->usb_handle, 0xe800, &data) == OK)
    {
      data = (data & 0x3f) | 0x40;	/*01------ */
      if (Write_Byte (dev->usb_handle, 0xe800, data) == OK)
	{
	  data &= 0xbf;		      /*-0------*/
	  rst = Write_Byte (dev->usb_handle, 0xe800, data);
	}
    }

  DBG (DBG_FNC, "- RTS_Warm_Reset: %i\n", rst);

  return rst;
}

static SANE_Int
Lamp_Status_Timer_Set (struct st_device *dev, SANE_Int minutes)
{
  SANE_Byte MyBuffer[2];
  SANE_Int rst;

  DBG (DBG_FNC, "+ Lamp_Status_Timer_Set(minutes=%i):\n", minutes);

  MyBuffer[0] = dev->init_regs[0x0146] & 0xef;
  MyBuffer[1] = dev->init_regs[0x0147];

  if (minutes > 0)
    {
      double rst, op2;

      minutes = _B0 (minutes);
      op2 = 2.682163611980331;
      MyBuffer[0x00] |= 0x10;
      rst = (minutes * op2);
      MyBuffer[0x01] = (SANE_Byte) floor (rst);
    }

  dev->init_regs[0x147] = MyBuffer[1];
  dev->init_regs[0x146] =
    (dev->init_regs[0x146] & 0xef) | (MyBuffer[0] & 0x10);

  rst =
    Write_Word (dev->usb_handle, 0xe946,
		(SANE_Int) ((MyBuffer[1] << 8) + MyBuffer[0]));

  DBG (DBG_FNC, "- Lamp_Status_Timer_Set: %i\n", rst);

  return rst;
}

static SANE_Int
Buttons_Enable (struct st_device *dev)
{
  SANE_Int data, rst;

  DBG (DBG_FNC, "+ Buttons_Enable:\n");

  if (Read_Word (dev->usb_handle, 0xe958, &data) == OK)
    {
      data |= 0x0f;
      rst = Write_Word (dev->usb_handle, 0xe958, data);
    }
  else
    rst = ERROR;

  DBG (DBG_FNC, "- Buttons_Enable: %i\n", rst);

  return rst;
}

static SANE_Int
Buttons_Count (struct st_device *dev)
{
  SANE_Int rst = 0;

  /* This chipset supports up to six buttons */

  if (dev->buttons != NULL)
    rst = dev->buttons->count;

  DBG (DBG_FNC, "> Buttons_Count: %i\n", rst);

  return rst;
}

static SANE_Int
Buttons_Status (struct st_device *dev)
{
  SANE_Int rst = -1;
  SANE_Byte data;

  DBG (DBG_FNC, "+ Buttons_Status\n");

  /* Each bit is 1 if button is not pressed, and 0 if it is pressed
     This chipset supports up to six buttons */

  if (Read_Byte (dev->usb_handle, 0xe968, &data) == OK)
    rst = _B0 (data);

  DBG (DBG_FNC, "- Buttons_Status: %i\n", rst);

  return rst;
}

static SANE_Int
Buttons_Released (struct st_device *dev)
{
  SANE_Int rst = -1;
  SANE_Byte data;

  DBG (DBG_FNC, "+ Buttons_Released\n");

  /* Each bit is 1 if button is released, until reading this register. Then
     entire register is cleared. This chipset supports up to six buttons */

  if (Read_Byte (dev->usb_handle, 0xe96a, &data) == OK)
    rst = _B0 (data);

  DBG (DBG_FNC, "- Buttons_Released: %i\n", rst);

  return rst;
}

static SANE_Int
Buttons_Order (struct st_device *dev, SANE_Int mask)
{
  /* this is a way to order each button according to its bit in register 0xe968 */
  SANE_Int rst = -1;

  if (dev->buttons != NULL)
    {
      SANE_Int a;

      for (a = 0; a < 6; a++)
	{
	  if (dev->buttons->mask[a] == mask)
	    {
	      rst = a;
	      break;
	    }
	}
    }

  return rst;
}

static SANE_Int
GainOffset_Clear (struct st_device *dev)
{
  SANE_Int rst = OK;

  DBG (DBG_FNC, "+ GainOffset_Clear:\n");

  /* clear offsets */
  offset[CL_RED] = offset[CL_GREEN] = offset[CL_BLUE] = 0;

  /* save offsets */
  /* check if chipset supports accessing eeprom */
  if ((dev->chipset->capabilities & CAP_EEPROM) != 0)
    {
      SANE_Int a;

      for (a = CL_RED; a <= CL_BLUE; a++)
	RTS_EEPROM_WriteWord (dev->usb_handle, 0x70 + (2 * a), 0);

      /* update checksum */
      rst = RTS_EEPROM_WriteByte (dev->usb_handle, 0x76, 0);
    }

  DBG (DBG_FNC, "- GainOffset_Clear: %i\n", rst);

  return rst;
}

static SANE_Int
Lamp_Status_Get (struct st_device *dev, SANE_Byte * flb_lamp,
		 SANE_Byte * tma_lamp)
{
  /* The only reason that I think windows driver uses two variables to get
     which lamp is switched on instead of one variable is that some chipset
     model could have both lamps switched on, so I'm maintaining such var */

  SANE_Int rst = ERROR;		/* default */

  DBG (DBG_FNC, "+ Lamp_Status_Get:\n");

  if ((flb_lamp != NULL) && (tma_lamp != NULL))
    {
      SANE_Int data1;
      SANE_Byte data2;

      if (Read_Byte (dev->usb_handle, 0xe946, &data2) == OK)
	{
	  if (Read_Word (dev->usb_handle, 0xe954, &data1) == OK)
	    {
	      rst = OK;

	      *flb_lamp = 0;
	      *tma_lamp = 0;

	      switch (dev->chipset->model)
		{
		case RTS8822BL_03A:
		  *flb_lamp = ((data2 & 0x40) != 0) ? 1 : 0;
		  *tma_lamp = (((data2 & 0x20) != 0)
			       && ((data1 & 0x10) == 1)) ? 1 : 0;
		  break;
		default:
		  if ((_B1 (data1) & 0x10) == 0)
		    *flb_lamp = (data2 >> 6) & 1;
		  else
		    *tma_lamp = (data2 >> 6) & 1;
		  break;
		}
	    }
	}
    }

  DBG (DBG_FNC, "- Lamp_Status_Get: rst=%i flb=%i tma=%i\n", rst,
       _B0 (*flb_lamp), _B0 (*tma_lamp));

  return rst;
}

static SANE_Int
RTS_DMA_WaitReady (struct st_device *dev, SANE_Int msecs)
{
  SANE_Byte data;
  SANE_Int rst;
  long mytime;

  DBG (DBG_FNC, "+ RTS_DMA_WaitReady(msecs=%i):\n", msecs);

  rst = OK;
  mytime = GetTickCount () + msecs;

  while ((mytime > GetTickCount ()) && (rst == OK))
    {
      if (Read_Byte (dev->usb_handle, 0xef09, &data) == OK)
	{
	  if ((data & 1) == 0)
	    usleep (1000 * 100);
	  else
	    break;
	}
      else
	rst = ERROR;
    }

  DBG (DBG_FNC, "- RTS_DMA_WaitReady: %i\n", rst);

  return rst;
}

static SANE_Int
RTS_WaitInitEnd (struct st_device *dev, SANE_Int msecs)
{
  SANE_Byte data;
  SANE_Int rst;
  long mytime;

  DBG (DBG_FNC, "+ RTS_WaitInitEnd(msecs=%i):\n", msecs);

  rst = OK;
  mytime = GetTickCount () + msecs;

  while ((mytime > GetTickCount ()) && (rst == OK))
    {
      if (Read_Byte (dev->usb_handle, 0xf910, &data) == OK)
	{
	  if ((data & 8) == 0)
	    usleep (1000 * 100);
	  else
	    break;
	}
      else
	rst = ERROR;
    }

  DBG (DBG_FNC, "- RTS_WaitInitEnd: %i\n", rst);

  return rst;
}

static SANE_Int
Motor_Change (struct st_device *dev, SANE_Byte * buffer, SANE_Byte value)
{
  SANE_Int data, rst;

  DBG (DBG_FNC, "+ Motor_Change(*buffer, value=%i):\n", value);

  if (Read_Word (dev->usb_handle, 0xe954, &data) == OK)
    {
      data &= 0xcf;	      /*--00----*/
      value--;
      switch (value)
	{
	case 2:
	  data |= 0x30;
	  break;				     /*--11----*/
	case 1:
	  data |= 0x20;
	  break;				     /*--10----*/
	case 0:
	  data |= 0x10;
	  break;				     /*--01----*/
	}

      buffer[0x154] = _B0 (data);

      rst = Write_Byte (dev->usb_handle, 0xe954, buffer[0x154]);
    }
  else
    rst = ERROR;

  DBG (DBG_FNC, "- Motor_Change: %i\n", rst);

  return rst;
}

static SANE_Int
RTS_DMA_Read (struct st_device *dev, SANE_Int dmacs, SANE_Int options,
	      SANE_Int size, SANE_Byte * buffer)
{
  SANE_Int rst = ERROR;

  DBG (DBG_FNC,
       "+ RTS_DMA_Read(dmacs=%04x, options=%04x, size=%i., *buffer):\n",
       dmacs, options, size);

  /* is there any buffer to send? */
  if ((buffer != NULL) && (size > 0))
    {
      /* reset dma */
      if (RTS_DMA_Reset (dev) == OK)
	{
	  /* prepare dma to read */
	  if (RTS_DMA_Enable_Read (dev, dmacs, size, options) == OK)
	    {
	      SANE_Int transferred;

	      rst =
		Bulk_Operation (dev, BLK_READ, size, buffer, &transferred);
	    }
	}
    }

  DBG (DBG_FNC, "- RTS_DMA_Read(): %i\n", rst);

  return rst;
}

static SANE_Int
RTS_DMA_Write (struct st_device *dev, SANE_Int dmacs, SANE_Int options,
	       SANE_Int size, SANE_Byte * buffer)
{
  SANE_Int rst = ERROR;

  DBG (DBG_FNC,
       "+ RTS_DMA_Write(dmacs=%04x, options=%04x, size=%i., *buffer):\n",
       dmacs, options, size);

  /* is there any buffer to send? */
  if ((buffer != NULL) && (size > 0))
    {
      /* reset dma */
      if (RTS_DMA_Reset (dev) == OK)
	{
	  /* prepare dma to write */
	  if (RTS_DMA_Enable_Write (dev, dmacs, size, options) == OK)
	    {
	      SANE_Int transferred;
	      SANE_Byte *check_buffer;

	      check_buffer = (SANE_Byte *) malloc (size);
	      if (check_buffer != NULL)
		{
		  /* if some transfer fails we try again until ten times */
		  SANE_Int a;
		  for (a = 10; a > 0; a--)
		    {
		      /* send buffer */
		      Bulk_Operation (dev, BLK_WRITE, size, buffer,
				      &transferred);

		      /* prepare dma to read */
		      if (RTS_DMA_Enable_Read (dev, dmacs, size, options) ==
			  OK)
			{
			  SANE_Int b = 0, diff = FALSE;

			  /* read buffer */
			  Bulk_Operation (dev, BLK_READ, size, check_buffer,
					  &transferred);

			  /* check buffers */
			  while ((b < size) && (diff == FALSE))
			    {
			      if (buffer[b] == check_buffer[b])
				b++;
			      else
				diff = TRUE;
			    }

			  /* if buffers are equal we can break loop */
			  if (diff == TRUE)
			    {
			      /* cancel dma */
			      RTS_DMA_Cancel (dev);

			      /* prepare dma to write buffer again */
			      if (RTS_DMA_Enable_Write
				  (dev, dmacs, size, options) != OK)
				break;
			    }
			  else
			    {
			      /* everything went ok */
			      rst = OK;
			      break;
			    }
			}
		      else
			break;
		    }

		  /* free check buffer */
		  free (check_buffer);
		}
	      else
		{
		  /* for some reason it's not posible to allocate space to check
		     sent buffer so we just write data */
		  Bulk_Operation (dev, BLK_WRITE, size, buffer, &transferred);
		  rst = OK;
		}
	    }
	}
    }

  DBG (DBG_FNC, "- RTS_DMA_Write(): %i\n", rst);

  return rst;
}

static SANE_Int
RTS_DMA_CheckType (struct st_device *dev, SANE_Byte * Regs)
{
  /* This function tries to detect what kind of RAM supports chipset */
  /* Returns a value between 0 and 4. -1 means error */

  SANE_Int rst = -1;

  DBG (DBG_FNC, "+ RTS_DMA_CheckType(*Regs):\n");

  if (Regs != NULL)
    {
      SANE_Byte *out_buffer;

      /* define buffer to send */
      out_buffer = (SANE_Byte *) malloc (sizeof (SANE_Byte) * 2072);
      if (out_buffer != NULL)
	{
	  SANE_Byte *in_buffer;

	  /* define incoming buffer */
	  in_buffer = (SANE_Byte *) malloc (sizeof (SANE_Byte) * 2072);
	  if (in_buffer != NULL)
	    {
	      SANE_Int a, b, diff;

	      /* fill outgoing buffer with a known pattern */
	      b = 0;
	      for (a = 0; a < 2072; a++)
		{
		  out_buffer[a] = b;
		  b++;
		  if (b == 0x61)
		    b = 0;
		}

	      /* let's send buffer with different ram types and compare
	         incoming buffer until getting the right type */

	      for (a = 4; a >= 0; a--)
		{
		  /* set ram type */
		  if (RTS_DMA_SetType (dev, Regs, a) != OK)
		    break;

		  /* wait 1500 miliseconds */
		  if (RTS_DMA_WaitReady (dev, 1500) == OK)
		    {
		      /* reset dma */
		      RTS_DMA_Reset (dev);

		      /* write buffer */
		      RTS_DMA_Write (dev, 0x0004, 0x102, 2072, out_buffer);

		      /* now read buffer */
		      RTS_DMA_Read (dev, 0x0004, 0x102, 2072, in_buffer);

		      /* check buffers */
		      b = 0;
		      diff = FALSE;
		      while ((b < 2072) && (diff == FALSE))
			{
			  if (out_buffer[b] == in_buffer[b])
			    b++;
			  else
			    diff = TRUE;
			}

		      /* if buffers are equal */
		      if (diff == FALSE)
			{
			  SANE_Int data = 0;

			  /* buffers are equal so we've found the right ram type */
			  memset (out_buffer, 0, 0x20);
			  for (b = 0; b < 0x20; b += 2)
			    out_buffer[b] = b;

			  /* write buffer */
			  if (RTS_DMA_Write
			      (dev, 0x0004, 0x0000, 0x20, out_buffer) == OK)
			    {
			      SANE_Int c = 0;
			      diff = TRUE;

			      do
				{
				  c++;
				  for (b = 1; b < 0x20; b += 2)
				    out_buffer[b] = c;

				  if (RTS_DMA_Write
				      (dev, 0x0004, (_B0 (c) << 0x11) >> 0x04,
				       0x20, out_buffer) == OK)
				    {
				      if (RTS_DMA_Read
					  (dev, 0x0004, 0x0000, 0x20,
					   in_buffer) == OK)
					{
					  b = 0;
					  diff = FALSE;
					  while ((b < 0x20)
						 && (diff == FALSE))
					    {
					      if (out_buffer[b] ==
						  in_buffer[b])
						b++;
					      else
						diff = TRUE;
					    }

					  if (diff == FALSE)
					    data = c << 7;
					}
				    }
				}
			      while ((c < 0x80) && (diff == TRUE));
			    }

			  switch (data)
			    {
			    case 16384:
			      Regs[0x708] &= 0x1f;
			      Regs[0x708] |= 0x80;
			      break;
			    case 8192:
			      Regs[0x708] &= 0x1f;
			      Regs[0x708] |= 0x60;
			      break;
			    case 4096:
			      Regs[0x708] &= 0x1f;
			      Regs[0x708] |= 0x40;
			      break;
			    case 2048:
			      Regs[0x708] &= 0x1f;
			      Regs[0x708] |= 0x20;
			      break;
			    case 1024:
			      Regs[0x708] &= 0x1f;
			      data = 0x200;
			      break;
			    case 128:
			      Regs[0x708] &= 0x1f;
			      break;
			    }

			  DBG (DBG_FNC, " -> data1 = 0x%08x\n",
			       (data * 4) * 1024);
			  DBG (DBG_FNC, " -> data2 = 0x%08x\n", data * 1024);
			  DBG (DBG_FNC, " -> type  = 0x%04x\n",
			       Regs[0x708] >> 5);

			  RTS_DMA_SetType (dev, Regs, Regs[0x708] >> 5);

			  rst = OK;
			  break;
			}
		    }
		  else
		    break;
		}

	      free (in_buffer);
	    }

	  free (out_buffer);
	}
    }

  DBG (DBG_FNC, "- RTS_DMA_CheckType(): %i\n", rst);

  return rst;
}

static SANE_Int
RTS_DMA_SetType (struct st_device *dev, SANE_Byte * Regs, SANE_Byte ramtype)
{
  SANE_Int rst = ERROR;

  DBG (DBG_FNC, "+ RTS_DMA_SetType(*Regs, ramtype=%i):\n", ramtype);

  if (Regs != NULL)
    {
      data_bitset (&Regs[0x708], 0x08, 0);	    /*----0---*/

      if (Write_Byte (dev->usb_handle, 0xef08, Regs[0x708]) == OK)
	{
	  data_bitset (&Regs[0x708], 0xe0, ramtype);

	  if (Write_Byte (dev->usb_handle, 0xef08, Regs[0x708]) == OK)
	    {
	      data_bitset (&Regs[0x708], 0x08, 1);		    /*----1---*/
	      rst = Write_Byte (dev->usb_handle, 0xef08, Regs[0x708]);
	    }
	}
    }

  DBG (DBG_FNC, "- RTS_DMA_SetType: %i\n", rst);

  return rst;
}

static void
Motor_Release (struct st_device *dev)
{
  SANE_Byte data = 0;

  DBG (DBG_FNC, "+ Motor_Release:\n");

  if (Read_Byte (dev->usb_handle, 0xe8d9, &data) == OK)
    {
      data |= 4;
      Write_Byte (dev->usb_handle, 0xe8d9, data);
    }

  DBG (DBG_FNC, "- Motor_Release:\n");
}

static SANE_Byte
GainOffset_Counter_Load (struct st_device *dev)
{
  SANE_Byte data = 0x0f;

  DBG (DBG_FNC, "+ GainOffset_Counter_Load:\n");

  /* check if chipset supports accessing eeprom */
  if ((dev->chipset->capabilities & CAP_EEPROM) != 0)
    if (RTS_EEPROM_ReadByte (dev->usb_handle, 0x77, &data) != OK)
      data = 0x0f;

  DBG (DBG_FNC, "- GainOffset_Counter_Load: %i\n", _B0 (data));

  return data;
}

static SANE_Int
RTS_Execute (struct st_device *dev)
{
  SANE_Byte e813, e800;
  SANE_Int ret;

  DBG (DBG_FNC, "+ RTS_Execute:\n");

  e813 = 0;
  e800 = 0;
  ret = ERROR;

  if (Read_Byte (dev->usb_handle, 0xe800, &e800) == OK)
    {
      if (Read_Byte (dev->usb_handle, 0xe813, &e813) == OK)
	{
	  e813 &= 0xbf;
	  if (Write_Byte (dev->usb_handle, 0xe813, e813) == OK)
	    {
	      e800 |= 0x40;
	      if (Write_Byte (dev->usb_handle, 0xe800, e800) == OK)
		{
		  e813 |= 0x40;
		  if (Write_Byte (dev->usb_handle, 0xe813, e813) == OK)
		    {
		      e800 &= 0xbf;
		      if (Write_Byte (dev->usb_handle, 0xe800, e800) == OK)
			{
			  usleep (1000 * 100);
			  e800 |= 0x80;
			  ret = Write_Byte (dev->usb_handle, 0xe800, e800);
			}
		    }
		}
	    }
	}
    }

  DBG (DBG_FNC, "- RTS_Execute: %i\n", ret);

  return ret;
}

static SANE_Int
RTS_isTmaAttached (struct st_device *dev)
{
  SANE_Int rst;

  DBG (DBG_FNC, "+ RTS_isTmaAttached:\n");

  /* returns 0 if Tma is attached. Otherwise 1 */
  if (Read_Word (dev->usb_handle, 0xe968, &rst) == OK)
    {
      rst = ((_B1 (rst) & 2) != 0) ? FALSE : TRUE;
    }
  else
    rst = TRUE;

  DBG (DBG_FNC, "- RTS_isTmaAttached: %s\n", (rst == TRUE) ? "Yes" : "No");

  return rst;
}

static SANE_Int
Gamma_AllocTable (SANE_Byte * table)
{
  SANE_Int C;
  SANE_Int rst = OK;

  hp_gamma->depth = 8;

  for (C = 0; C < 3; C++)
    if (hp_gamma->table[C] == NULL)
      hp_gamma->table[C] = malloc (sizeof (SANE_Byte) * 256);

  if ((hp_gamma->table[CL_RED] != NULL) &&
      (hp_gamma->table[CL_GREEN] != NULL) &&
      (hp_gamma->table[CL_BLUE] != NULL))
    {
      /* All tables allocated */
      for (C = 0; C < 256; C++)
	{
	  if ((table != NULL) && (RTS_Debug->EnableGamma == TRUE))
	    {
	      /* fill gamma tables with user defined values */
	      hp_gamma->table[CL_RED][C] = table[C];
	      hp_gamma->table[CL_GREEN][C] = table[0x100 + C];
	      hp_gamma->table[CL_BLUE][C] = table[0x200 + C];
	    }
	  else
	    {
	      hp_gamma->table[CL_RED][C] = C;
	      hp_gamma->table[CL_GREEN][C] = C;
	      hp_gamma->table[CL_BLUE][C] = C;
	    }
	}

      /* Locate threshold of bw */
      for (C = 0; C < 256; C++)
	if (hp_gamma->table[CL_RED][C] != 0)
	  break;

      bw_threshold = C - 1;
    }
  else
    {
      /* Some alloc failed */
      rst = ERROR;

      Gamma_FreeTables ();
    }

  DBG (DBG_FNC, "> Gamma_AllocTable: %i >> bw_threshold = %i\n", rst,
       bw_threshold);

  return rst;
}

static SANE_Int
Gamma_Apply (struct st_device *dev, SANE_Byte * Regs,
	     struct st_scanparams *scancfg, struct st_hwdconfig *hwdcfg,
	     struct st_gammatables *mygamma)
{
  SANE_Int rst = OK;

  DBG (DBG_FNC, "+ Gamma_Apply(*Regs, *scancfg, *hwdcfg, *mygamma):\n");
  dbg_ScanParams (scancfg);

  if (hwdcfg->use_gamma_tables == FALSE)
    {
      DBG (DBG_FNC, "-> Gamma tables are not used\n");

      v1600 = NULL;
      v1604 = NULL;
      v1608 = NULL;
    }
  else
    {
      /*390b */
      SANE_Int table_size, buffersize, c;
      SANE_Byte channels, *gammabuffer;

      DBG (DBG_FNC, "-> Using gamma tables\n");

      /* get channels count */
      channels = 3;		/* default */

      if (scancfg->colormode != CM_COLOR)
	{
	  if (scancfg->channel != 3)
	    {
	      if (scancfg->colormode != 3)
		channels = (scancfg->samplerate == PIXEL_RATE) ? 2 : 1;
	    }
	}

      /* get size for gamma tables */
      switch (mygamma->depth & 0x0c)
	{
	case 0:
	  table_size = 0x100 + (mygamma->depth & 1);
	  break;
	case 4:
	  table_size = 0x400 + (mygamma->depth & 1);
	  break;
	case 8:
	  table_size = 0x1000 + (mygamma->depth & 1);
	  break;
	default:
	  table_size = 2;
	  break;
	}

      /* allocate space for gamma buffer */
      buffersize = table_size * channels;
      gammabuffer = (SANE_Byte *) malloc (buffersize * sizeof (SANE_Byte));
      if (gammabuffer != NULL)
	{
	  /* update gamma pointers for each channel */
	  v1600 = (SANE_Byte *) & mygamma->table[CL_RED];
	  v1604 = (SANE_Byte *) & mygamma->table[CL_GREEN];
	  v1608 = (SANE_Byte *) & mygamma->table[CL_BLUE];

	  /* include gamma tables into one buffer */
	  for (c = 0; c < channels; c++)
	    memcpy (gammabuffer + (c * table_size), mygamma->table[c],
		    table_size);

	  /* send gamma buffer to scanner */
	  Write_Byte (dev->usb_handle, 0xee0b, Regs[0x060b] & 0xaf);
	  rst = Gamma_SendTables (dev, Regs, gammabuffer, buffersize);
	  Write_Byte (dev->usb_handle, 0xee0b, Regs[0x060b]);

	  /* free gamma buffer */
	  free (gammabuffer);
	}
      else
	rst = ERROR;
    }

  return rst;
}

static SANE_Int
Refs_Analyze_Pattern (struct st_scanparams *scancfg,
		      SANE_Byte * scanned_pattern, SANE_Int * ler1,
		      SANE_Int ler1order, SANE_Int * ser1, SANE_Int ser1order)
{
  SANE_Int buffersize, xpos, ypos, coord, cnt, chn_size, dist, rst;
  double *color_sum, *color_dif, diff_max;
  SANE_Int vector[3];

  DBG (DBG_FNC,
       "+ Refs_Analyze_Pattern(depth=%i, width=%i, height=%i, *scanned_pattern, *ler1, ler1order=%i, *ser1, ser1order=%i)\n",
       scancfg->depth, scancfg->coord.width, scancfg->coord.height, ler1order,
       ser1order);

  rst = ERROR;			/* by default */
  dist = 5;			/* distance to compare */
  chn_size = (scancfg->depth > 8) ? 2 : 1;
  buffersize = max (scancfg->coord.width, scancfg->coord.height);

  color_sum = (double *) malloc (sizeof (double) * buffersize);
  if (color_sum != NULL)
    {
      color_dif = (double *) malloc (sizeof (double) * buffersize);
      if (color_dif != NULL)
	{
			/*-------- 1st SER -------- */
	  coord = 1;

	  if ((scancfg->coord.width - dist) > 1)
	    {
	      /* clear buffers */
	      bzero (color_sum, sizeof (double) * buffersize);
	      bzero (color_dif, sizeof (double) * buffersize);

	      for (xpos = 0; xpos < scancfg->coord.width; xpos++)
		{
		  for (ypos = 0; ypos < 20; ypos++)
		    color_sum[xpos] +=
		      data_lsb_get (scanned_pattern +
				    (scancfg->coord.width * ypos) + xpos,
				    chn_size);
		}

	      diff_max =
		(ser1order !=
		 0) ? color_sum[0] - color_sum[1] : color_sum[1] -
		color_sum[0];
	      color_dif[0] = diff_max;
	      cnt = 1;

	      do
		{
		  color_dif[cnt] =
		    (ser1order !=
		     0) ? color_sum[cnt] - color_sum[cnt +
						     dist] : color_sum[cnt +
								       dist] -
		    color_sum[cnt];

		  if ((color_dif[cnt] >= 0) && (color_dif[cnt] > diff_max))
		    {
		      /*d4df */
		      diff_max = color_dif[cnt];
		      if (abs (color_dif[cnt] - color_dif[cnt - 1]) >
			  abs (color_dif[coord] - color_dif[coord - 1]))
			coord = cnt;
		    }

		  cnt++;
		}
	      while (cnt < (scancfg->coord.width - dist));
	    }

	  vector[0] = coord + dist;

			/*-------- 1st LER -------- */
	  coord = 1;

	  if ((scancfg->coord.height - dist) > 1)
	    {
	      /* clear buffers */
	      bzero (color_sum, sizeof (double) * buffersize);
	      bzero (color_dif, sizeof (double) * buffersize);

	      for (ypos = 0; ypos < scancfg->coord.height; ypos++)
		{
		  for (xpos = vector[0]; xpos < scancfg->coord.width - dist;
		       xpos++)
		    color_sum[ypos] +=
		      data_lsb_get (scanned_pattern +
				    (scancfg->coord.width * ypos) + xpos,
				    chn_size);
		}

	      diff_max =
		(ler1order !=
		 0) ? color_sum[0] - color_sum[1] : color_sum[1] -
		color_sum[0];
	      color_dif[0] = diff_max;

	      cnt = 1;

	      do
		{
		  color_dif[cnt] =
		    (ler1order !=
		     0) ? color_sum[cnt] - color_sum[cnt +
						     dist] : color_sum[cnt +
								       dist] -
		    color_sum[cnt];

		  if ((color_dif[cnt] >= 0) && (color_dif[cnt] > diff_max))
		    {
		      diff_max = color_dif[cnt];
		      if (abs (color_dif[cnt] - color_dif[cnt - 1]) >
			  abs (color_dif[coord] - color_dif[coord - 1]))
			coord = cnt;
		    }

		  cnt++;
		}
	      while (cnt < (scancfg->coord.height - dist));
	    }

	  vector[1] = coord + dist;

			/*-------- 1st LER -------- */
	  if ((scancfg->coord.width - dist) > 1)
	    {
	      /* clear buffers */
	      bzero (color_sum, sizeof (double) * buffersize);
	      bzero (color_dif, sizeof (double) * buffersize);

	      for (xpos = 0; xpos < scancfg->coord.width; xpos++)
		{
		  for (ypos = coord + 4; ypos < scancfg->coord.height; ypos++)
		    color_sum[xpos] +=
		      data_lsb_get (scanned_pattern +
				    (scancfg->coord.width * ypos) + xpos,
				    chn_size);
		}

	      diff_max =
		(ser1order !=
		 0) ? color_sum[0] - color_sum[1] : color_sum[1] -
		color_sum[0];
	      color_dif[0] = diff_max;
	      cnt = 1;

	      do
		{
		  color_dif[cnt] =
		    (ser1order !=
		     0) ? color_sum[cnt] - color_sum[cnt +
						     dist] : color_sum[cnt +
								       dist] -
		    color_sum[cnt];

		  if ((color_dif[cnt] >= 0) && (color_dif[cnt] > diff_max))
		    {
		      diff_max = color_dif[cnt];
		      if (abs (color_dif[cnt] - color_dif[cnt - 1]) >
			  abs (color_dif[coord] - color_dif[coord - 1]))
			coord = cnt;
		    }

		  cnt++;
		}
	      while (cnt < (scancfg->coord.width - dist));
	    }

	  vector[2] = coord + dist;

	  /* save image */
	  if (RTS_Debug->SaveCalibFile != FALSE)
	    dbg_autoref (scancfg, scanned_pattern, vector[0], vector[2],
			 vector[1]);

	  /* assign values detected */
	  if (ser1 != NULL)
	    *ser1 = vector[2];

	  if (ler1 != NULL)
	    *ler1 = vector[1];

	  /* show values */
	  DBG (DBG_FNC, " -> Vectors found: x1=%i, x2=%i, y=%i\n", vector[0],
	       vector[2], vector[1]);

	  rst = OK;

	  free (color_dif);
	}

      free (color_sum);
    }

  DBG (DBG_FNC, "- Refs_Analyze_Pattern: %i\n", rst);

  return rst;
}

static double
get_shrd (double value, SANE_Int desp)
{
  if (desp <= 0x40)
    return value / pow (2, desp);
  else
    return 0;
}

static char
get_byte (double value)
{
  unsigned int data;
  double temp;

  if (value > 0xffffffff)
    {
      temp = floor (get_shrd (value, 0x20));
      temp *= pow (2, 32);
      value -= temp;
    }

  data = (unsigned int) value;

  data = _B0 (data);

  return data;
}

static SANE_Int
Timing_SetLinearImageSensorClock (SANE_Byte * Regs, struct st_cph *cph)
{
  SANE_Int rst = ERROR;

  DBG (DBG_FNC,
       "+ Timing_SetLinearImageSensorClock(SANE_Byte *Regs, struct st_cph *cph)\n");

  dbg_sensorclock (cph);

  if ((Regs != NULL) && (cph != NULL))
    {
      Regs[0x00] = get_byte (cph->p1);
      Regs[0x01] = get_byte (get_shrd (cph->p1, 0x08));
      Regs[0x02] = get_byte (get_shrd (cph->p1, 0x10));
      Regs[0x03] = get_byte (get_shrd (cph->p1, 0x18));

      Regs[0x04] &= 0x80;
      Regs[0x04] |= ((get_byte (get_shrd (cph->p1, 0x20))) & 0x0f);
      Regs[0x04] |= ((cph->ps & 1) << 6);
      Regs[0x04] |= ((cph->ge & 1) << 5);
      Regs[0x04] |= ((cph->go & 1) << 4);

      Regs[0x05] = get_byte (cph->p2);
      Regs[0x06] = get_byte (get_shrd (cph->p2, 0x08));
      Regs[0x07] = get_byte (get_shrd (cph->p2, 0x10));
      Regs[0x08] = get_byte (get_shrd (cph->p2, 0x18));
      Regs[0x09] &= 0xf0;
      Regs[0x09] |= ((get_byte (get_shrd (cph->p2, 0x20))) & 0x0f);

      rst = OK;
    }

  DBG (DBG_FNC, "- Timing_SetLinearImageSensorClock: %i\n", rst);

  return rst;
}

static void
RTS_Setup_SensorTiming (struct st_device *dev, SANE_Int mytiming,
			SANE_Byte * Regs)
{
  DBG (DBG_FNC, "+ RTS_Setup_SensorTiming(mytiming=%i, *Regs):\n", mytiming);

  if ((Regs != NULL) && (mytiming < dev->timings_count))
    {
      struct st_timing *mt = dev->timings[mytiming];

      if (mt != NULL)
	{
	  dbg_timing (mt);

	  /* Correlated-Double-Sample 1 & 2 */
	  data_bitset (&Regs[0x92], 0x3f, mt->cdss[0]);
	  data_bitset (&Regs[0x93], 0x3f, mt->cdsc[0]);
	  data_bitset (&Regs[0x94], 0x3f, mt->cdss[1]);
	  data_bitset (&Regs[0x95], 0x3f, mt->cdsc[1]);

	  data_bitset (&Regs[0x96], 0x3f, mt->cnpp);

	  /* Linear image sensor transfer gates */
	  data_bitset (&Regs[0x45], 0x80, mt->cvtrp[0]);
	  data_bitset (&Regs[0x45], 0x40, mt->cvtrp[1]);
	  data_bitset (&Regs[0x45], 0x20, mt->cvtrp[2]);

	  data_bitset (&Regs[0x45], 0x1f, mt->cvtrfpw);
	  data_bitset (&Regs[0x46], 0x1f, mt->cvtrbpw);

	  data_lsb_set (&Regs[0x47], mt->cvtrw, 1);

	  data_lsb_set (&Regs[0x84], mt->cphbp2s, 3);
	  data_lsb_set (&Regs[0x87], mt->cphbp2e, 3);

	  data_lsb_set (&Regs[0x8a], mt->clamps, 3);
	  data_lsb_set (&Regs[0x8d], mt->clampe, 3);

	  if (dev->chipset->model == RTS8822L_02A)
	    {
	      if (mt->clampe == -1)
		data_lsb_set (&Regs[0x8d], mt->cphbp2e, 3);
	    }

	  Regs[0x97] = get_byte (mt->adcclkp[0]);
	  Regs[0x98] = get_byte (get_shrd (mt->adcclkp[0], 0x08));
	  Regs[0x99] = get_byte (get_shrd (mt->adcclkp[0], 0x10));
	  Regs[0x9a] = get_byte (get_shrd (mt->adcclkp[0], 0x18));
	  Regs[0x9b] &= 0xf0;
	  Regs[0x9b] |= ((get_byte (get_shrd (mt->adcclkp[0], 0x20))) & 0x0f);

	  Regs[0xc1] = get_byte (mt->adcclkp[1]);
	  Regs[0xc2] = get_byte (get_shrd (mt->adcclkp[1], 0x08));
	  Regs[0xc3] = get_byte (get_shrd (mt->adcclkp[1], 0x10));
	  Regs[0xc4] = get_byte (get_shrd (mt->adcclkp[1], 0x18));
	  Regs[0xc5] &= 0xe0;
	  Regs[0xc5] |= ((get_byte (get_shrd (mt->adcclkp[1], 0x20))) & 0x0f);

	  /* bit(4) = bit(0) */
	  Regs[0xc5] |= ((mt->adcclkp2e & 1) << 4);

	  Timing_SetLinearImageSensorClock (&Regs[0x48], &mt->cph[0]);
	  Timing_SetLinearImageSensorClock (&Regs[0x52], &mt->cph[1]);
	  Timing_SetLinearImageSensorClock (&Regs[0x5c], &mt->cph[2]);
	  Timing_SetLinearImageSensorClock (&Regs[0x66], &mt->cph[3]);
	  Timing_SetLinearImageSensorClock (&Regs[0x70], &mt->cph[4]);
	  Timing_SetLinearImageSensorClock (&Regs[0x7a], &mt->cph[5]);
	}
    }
}

static SANE_Int
Motor_GetFromResolution (SANE_Int resolution)
{
  SANE_Int ret;

  ret = 3;
  if (RTS_Debug->usbtype != USB11)
    {
      if (scan.scantype != ST_NORMAL)
	{
	  /* scantype is ST_NEG or ST_TA */
	  if (resolution >= 600)
	    ret = 0;
	}
      else if (resolution >= 1200)
	ret = 0;
    }
  else if (resolution >= 600)
    ret = 0;

  DBG (DBG_FNC, "> Motor_GetFromResolution(resolution=%i): %i\n", resolution,
       ret);

  return ret;
}

static SANE_Int
SetMultiExposure (struct st_device *dev, SANE_Byte * Regs)
{
  SANE_Int iValue, myctpc;

  DBG (DBG_FNC, "> SetMultiExposure:\n");

  /* set motor has no curves */
  data_bitset (&Regs[0xdf], 0x10, 0);	   /*---0----*/

  /* select case systemclock */
  switch (Regs[0x00] & 0x0f)
    {
    case 0x00:
      iValue = 0x00895440;
      break;			/*  3 x 0x2DC6C0 */
    case 0x08:
    case 0x01:
      iValue = 0x00b71b00;
      break;			/*  4 x 0x2DC6C0 */
    case 0x02:
      iValue = 0x0112a880;
      break;			/*  6 x 0x2DC6C0 */
    case 0x0a:
    case 0x03:
      iValue = 0x016e3600;
      break;			/*  8 x 0x2DC6C0 */
    case 0x04:
      iValue = 0x02255100;
      break;			/* 12 x 0x2DC6C0 */
    case 0x0c:
      iValue = 0x02dc6c00;
      break;			/* 16 x 0x2DC6C0 */
    case 0x05:
      iValue = 0x044aa200;
      break;			/* 24 x 0x2DC6C0 */
    case 0x0d:
      iValue = 0x05b8d800;
      break;			/* 32 x 0x2DC6C0 */

    case 0x09:
      iValue = 0x00f42400;
      break;
    case 0x0b:
      iValue = 0x01e84800;
      break;			/* = case 9 * 2 */
    default:
      iValue = 0x0478f7f8;
      break;
    }

  /* divide by timing.cnpp */
  iValue /= ((Regs[0x96] & 0x3f) + 1);
  iValue /= dev->motorcfg->basespeedpps;

  /* get line exposure time */
  myctpc = data_lsb_get (&Regs[0x30], 3) + 1;

  DBG (DBG_FNC, "CTPC -- SetMultiExposure -- 1 =%i\n", myctpc);

  /* if last step of accurve.normalscan table is lower than iValue ... */
  if (data_lsb_get (&Regs[0xe1], 3) < iValue)
    {
      SANE_Int traget;
      SANE_Int step_size = _B0 (Regs[0xe0]) + 1;

      /* set exposure time [RED] if zero */
      if (data_lsb_get (&Regs[0x36], 3) == 0)
	data_lsb_set (&Regs[0x36], myctpc - 1, 3);

      /* set exposure time [GREEN] if zero */
      if (data_lsb_get (&Regs[0x3c], 3) == 0)
	data_lsb_set (&Regs[0x3c], myctpc - 1, 3);

      /* set exposure time [BLUE] if zero */
      if (data_lsb_get (&Regs[0x42], 3) == 0)
	data_lsb_set (&Regs[0x42], myctpc - 1, 3);

      iValue = (iValue + 1) * step_size;

      /* update line exposure time */
      traget = (((myctpc + iValue - 1) / myctpc) * myctpc);
      data_lsb_set (&Regs[0x30], traget - 1, 3);

      traget = (traget / step_size) - 1;
      data_lsb_set (&Regs[0x00e1], traget, 3);
    }

  /* 8300 */
  return OK;
}

static SANE_Int
data_lsb_get (SANE_Byte * address, SANE_Int size)
{
  SANE_Int ret = 0;
  if ((address != NULL) && (size > 0) && (size < 5))
    {
      SANE_Int a;
      SANE_Byte b;
      size--;
      for (a = size; a >= 0; a--)
	{
	  b = address[a];
	  ret = (ret << 8) + b;
	}
    }
  return ret;
}

static SANE_Byte
data_bitget (SANE_Byte * address, SANE_Int mask)
{
  SANE_Int desp = 0;

  if (mask & 1);
  else if (mask & 2)
    desp = 1;
  else if (mask & 4)
    desp = 2;
  else if (mask & 8)
    desp = 3;
  else if (mask & 16)
    desp = 4;
  else if (mask & 32)
    desp = 5;
  else if (mask & 64)
    desp = 6;
  else if (mask & 128)
    desp = 7;

  return (*address & mask) >> desp;
}

static void
data_bitset (SANE_Byte * address, SANE_Int mask, SANE_Byte data)
{
  /* This function fills mask bits of just a byte with bits given in data */
  if (mask & 1);
  else if (mask & 2)
    data <<= 1;
  else if (mask & 4)
    data <<= 2;
  else if (mask & 8)
    data <<= 3;
  else if (mask & 16)
    data <<= 4;
  else if (mask & 32)
    data <<= 5;
  else if (mask & 64)
    data <<= 6;
  else if (mask & 128)
    data <<= 7;

  *address = (*address & (0xff - mask)) | (data & mask);
}

static void
data_wide_bitset (SANE_Byte * address, SANE_Int mask, SANE_Int data)
{
  /* Setting bytes bit per bit
     mask is 4 bytes size
     Example:
     data  = 0111010111
     mask  = 00000000 11111111 11000000 00000000
     rst   = 00000000 01110101 11000000 00000000 */

  SANE_Int mymask, started = FALSE;

  if ((address != NULL) && (mask != 0))
    {
      while (mask != 0)
	{
	  mymask = _B0 (mask);

	  if (started == FALSE)
	    {
	      if (mymask != 0)
		{
		  SANE_Int a, myvalue;

		  for (a = 0; a < 8; a++)
		    if ((mymask & (1 << a)) != 0)
		      break;

		  myvalue = _B0 (data << a);
		  myvalue >>= a;
		  data_bitset (address, mymask, myvalue);
		  data >>= (8 - a);
		  started = TRUE;
		}
	    }
	  else
	    {
	      data_bitset (address, mymask, _B0 (data));
	      data >>= 8;
	    }

	  address++;
	  mask >>= 8;
	}
    }
}


static void
data_lsb_set (SANE_Byte * address, SANE_Int data, SANE_Int size)
{
  if ((address != NULL) && (size > 0) && (size < 5))
    {
      SANE_Int a;
      for (a = 0; a < size; a++)
	{
	  address[a] = _B0 (data);
	  data >>= 8;
	}
    }
}

static void
data_msb_set (SANE_Byte * address, SANE_Int data, SANE_Int size)
{
  if ((address != NULL) && (size > 0) && (size < 5))
    {
      SANE_Int a;

      for (a = size - 1; a >= 0; a--)
	{
	  address[a] = _B0 (data);
	  data >>= 8;
	}
    }
}

static SANE_Int
data_swap_endianess (SANE_Int address, SANE_Int size)
{
  SANE_Int rst = 0;

  if ((size > 0) && (size < 5))
    {
      SANE_Int a;

      for (a = 0; a < size; a++)
	{
	  rst = (rst << 8) | _B0 (address);
	  address >>= 8;
	}
    }

  return rst;
}

static void
Lamp_SetGainMode (struct st_device *dev, SANE_Byte * Regs,
		  SANE_Int resolution, SANE_Byte gainmode)
{
  DBG (DBG_FNC, "> Lamp_SetGainMode(*Regs, resolution=%i, gainmode=%i):\n",
       resolution, gainmode);

  if (dev->chipset->model == RTS8822L_02A)
    {
      /* hp4370 */
      SANE_Int data1, data2;

      data1 = data_lsb_get (&Regs[0x154], 2) & 0xfe7f;
      data2 = data_lsb_get (&Regs[0x156], 2);

      switch (resolution)
	{
	case 4800:
	  data2 |= 0x40;
	  data1 &= 0xffbf;
	  break;
	case 100:
	case 150:
	case 200:
	case 300:
	case 600:
	case 1200:
	case 2400:
	  data1 |= 0x40;
	  data2 &= 0xffbf;
	  break;
	}

      data_lsb_set (&Regs[0x154], data1, 2);
      data_lsb_set (&Regs[0x156], data2, 2);
    }
  else
    {
      /* hp3970 hp4070 ua4900 */
      SANE_Int data;

      data = data_lsb_get (&Regs[0x154], 2) & 0xfe7f;
      data = (gainmode == FALSE) ? data | 0x0040 : data & 0xffbf;

      switch (resolution)
	{
	case 100:
	case 200:
	case 300:
	case 600:
	  data |= 0x0100;
	  break;
	case 2400:
	  data |= 0x0180;
	  break;
	case 1200:
	  if (dev->sensorcfg->type == CIS_SENSOR)
	    data |= 0x80;
	  else if (dev->sensorcfg->type == CCD_SENSOR)
	    data |= 0x0180;
	  break;
	}

      data_lsb_set (&Regs[0x0154], data, 2);
    }
}

static SANE_Int
RTS_Scanner_StartScan (struct st_device *dev)
{
  SANE_Int rst = ERROR;		/* default */
  SANE_Int data;

  DBG (DBG_FNC, "+ RTS_Scanner_StartScan():\n");

  v14b4 = 1;			/* TEMPORAL */
  data = 0;
  Lamp_PWM_DutyCycle_Get (dev, &data);
  data = _B0 (data);

  DBG (DBG_FNC, "->   Pwm used = %i\n", data);

  /*
     windows driver saves pwm used, in file usbfile
     Section [SCAN_PARAM], field PwmUsed
   */

  dev->status->cancel = FALSE;

  if (Scan_Start (dev) == OK)
    {
      SANE_Int transferred;

      rst = OK;

      if (dev->scanning->imagebuffer != NULL)
	{
	  free (dev->scanning->imagebuffer);
	  dev->scanning->imagebuffer = NULL;
	}

      SetLock (dev->usb_handle, NULL, (scan2.depth == 16) ? FALSE : TRUE);

      /* Reservamos los buffers necesarios para leer la imagen */
      Reading_CreateBuffers (dev);

      if (dev->Resize->type != RSZ_NONE)
	Resize_Start (dev, &transferred);	/* 6729 */

      RTS_ScanCounter_Inc (dev);
    }

  DBG (DBG_FNC, "- RTS_Scanner_StartScan: %i\n", rst);

  return rst;
}

static void
Triplet_Gray (SANE_Byte * pPointer1, SANE_Byte * pPointer2,
	      SANE_Byte * buffer, SANE_Int channels_count)
{
  /*
     pPointer1 = FAB8
     pPointer2 = FABC
     buffer    = FAC0
     channels_count = FAC4
   */

  SANE_Int value;
  SANE_Int channel_size;

  DBG (DBG_FNC,
       "> Triplet_Gray(*pPointer1, *pPointer2, *buffer, channels_count=%i)\n",
       channels_count);

  channel_size = (scan2.depth > 8) ? 2 : 1;
  channels_count = channels_count / 2;

  while (channels_count > 0)
    {
      value = data_lsb_get (pPointer1, channel_size);
      data_lsb_set (buffer, value, channel_size);

      value = data_lsb_get (pPointer2, channel_size);
      data_lsb_set (buffer + channel_size, value, channel_size);

      pPointer1 += 2 * channel_size;
      pPointer2 += 2 * channel_size;
      buffer += 2 * channel_size;

      channels_count--;
    }
}

static void
Triplet_Lineart (SANE_Byte * pPointer1, SANE_Byte * pPointer2,
		 SANE_Byte * buffer, SANE_Int channels_count)
{
  /* Composing colour in lineart mode */

  SANE_Int dots_count = 0;
  SANE_Int channel;
  SANE_Byte mask;
  SANE_Byte value;
  SANE_Int C;

  DBG (DBG_FNC,
       "> Triplet_Lineart(*pPointer1, *pPointer2, *buffer, channels_count=%i)\n",
       channels_count);

  if (channels_count > 0)
    {
      dots_count = (channels_count + 1) / 2;
      while (dots_count > 0)
	{
	  mask = 0x80;
	  channel = 2;
	  do
	    {
	      value = 0;
	      for (C = 4; C > 0; C--)
		{
		  value =
		    (value << 2) +
		    (((*pPointer2 & mask) << 1) | (*pPointer1 & mask));
		  mask = mask >> 1;
		}
	      *buffer = value;
	      buffer++;
	      channel--;
	    }
	  while (channel > 0);
	  pPointer2 += 2;
	  pPointer1 += 2;
	  dots_count--;
	}
    }
}

static SANE_Int
Arrange_NonColour (struct st_device *dev, SANE_Byte * buffer,
		   SANE_Int buffer_size, SANE_Int * transferred)
{
  /*
     buffer : fadc
     buffer_size : fae0
   */

  SANE_Int lines_count = 0;	/* ebp */
  SANE_Int channels_count = 0;	/* fadc pisa buffer */
  SANE_Int rst = ERROR;
  struct st_scanning *scn;

  DBG (DBG_FNC,
       "+ Arrange_NonColour(*buffer, buffer_size=%i, *transferred):\n",
       buffer_size);

  /* this is just to make code more legible */
  scn = dev->scanning;

  if (scn->imagebuffer == NULL)
    {
      if ((scn->arrange_hres == TRUE) || (scan2.colormode == CM_LINEART))
	{
	  scn->bfsize = (scn->arrange_sensor_evenodd_dist + 1) * line_size;
	  scn->imagebuffer =
	    (SANE_Byte *) malloc (scn->bfsize * sizeof (SANE_Byte));
	  if (scn->imagebuffer != NULL)
	    {
	      if (Read_Block (dev, scn->bfsize, scn->imagebuffer, transferred)
		  == OK)
		{
		  scn->channel_size = (scan2.depth == 8) ? 1 : 2;
		  scn->desp1[CL_RED] = 0;
		  scn->desp2[CL_RED] =
		    scn->channel_size +
		    (scn->arrange_sensor_evenodd_dist * line_size);
		  scn->pColour2[CL_RED] =
		    scn->imagebuffer + scn->desp2[CL_RED];
		  scn->pColour1[CL_RED] =
		    scn->imagebuffer + scn->desp1[CL_RED];
		  rst = OK;
		}
	    }
	}
    }
  else
    rst = OK;

  /* b0f4 */
  if (rst == OK)
    {
      scn->imagepointer = scn->imagebuffer;
      lines_count = buffer_size / line_size;
      channels_count = line_size / scn->channel_size;
      while (lines_count > 0)
	{
	  if (scan2.colormode == CM_LINEART)
	    Triplet_Lineart (scn->pColour1[CL_RED], scn->pColour2[CL_RED],
			     buffer, channels_count);
	  else
	    Triplet_Gray (scn->pColour1[CL_RED], scn->pColour2[CL_RED],
			  buffer, channels_count);

	  buffer += line_size;
	  scn->arrange_size -= bytesperline;

	  lines_count--;
	  if (lines_count == 0)
	    {
	      if ((scn->arrange_size | v15bc) == 0)
		break;
	    }

	  rst = Read_Block (dev, line_size, scn->imagepointer, transferred);
	  if (rst != OK)
	    break;

	  if (scn->arrange_hres == TRUE)
	    {
	      scn->desp2[CL_RED] =
		(line_size + scn->desp2[CL_RED]) % scn->bfsize;
	      scn->desp1[CL_RED] =
		(line_size + scn->desp1[CL_RED]) % scn->bfsize;

	      scn->pColour2[CL_RED] = scn->imagebuffer + scn->desp2[CL_RED];
	      scn->pColour1[CL_RED] = scn->imagebuffer + scn->desp1[CL_RED];
	    }

	  /* b21d */
	  scn->imagepointer += line_size;
	  if (scn->imagepointer >= (scn->imagebuffer + scn->bfsize))
	    scn->imagepointer = scn->imagebuffer;
	}
    }

  /* 2246 */

  DBG (DBG_FNC, "- Arrange_NonColour(*transferred=%i): %i\n", *transferred,
       rst);

  return rst;
}

static SANE_Int
Resize_Decrease (SANE_Byte * to_buffer, SANE_Int to_resolution,
		 SANE_Int to_width, SANE_Byte * from_buffer,
		 SANE_Int from_resolution, SANE_Int from_width,
		 SANE_Int myresize_mode)
{
  /*
     to_buffer = FAC8 = 0x236200
     to_resolution      = FACC = 0x4b
     to_width    = FAD0 = 0x352
     from_buffer = FAD4 = 0x235460
     from_resolution      = FAD8 = 0x64
     from_width    = FADC = 0x46d
     myresize_mode   = FAE0 = 1
   */

  SANE_Int rst = ERROR;
  SANE_Int channels = 0;	/* fac8 */
  SANE_Int depth = 0;		/* fae0 */
  SANE_Int color[3] = { 0, 0, 0 };	/* fab8 | fabc | fac0 */
  SANE_Int to_pos = 0;		/* fad4 */
  SANE_Int rescont = 0;
  SANE_Int from_pos = 0;	/* fab4 */
  SANE_Int C;
  SANE_Int smres = 0;		/* fab0 */
  SANE_Int value;
  SANE_Int channel_size;

  to_resolution = to_resolution & 0xffff;
  from_resolution = from_resolution & 0xffff;

  DBG (DBG_FNC,
       "+ Resize_Decrease(*to_buffer, to_resolution=%i, to_width=%i, *from_buffer, from_resolution=%i, from_width=%i, myresize_mode=%i):\n",
       to_resolution, to_width, from_resolution, from_width, myresize_mode);

  if (myresize_mode != RSZ_LINEART)
    {
      switch (myresize_mode)
	{
	case RSZ_GRAYL:
	  channels = 1;
	  depth = 8;
	  break;
	case RSZ_COLOURL:
	  channels = 3;
	  depth = 8;
	  break;
	case RSZ_COLOURH:
	  channels = 3;
	  depth = 16;
	  break;
	case RSZ_GRAYH:
	  channels = 1;
	  depth = 16;
	  break;
	}

      channel_size = (depth > 8) ? 2 : 1;
      to_pos = 0;
      rescont = 0;

      while (to_pos < to_width)
	{
	  from_pos++;
	  if (from_pos > from_width)
	    from_buffer -= (((depth + 7) / 8) * channels);

	  rescont += to_resolution;
	  if (rescont < from_resolution)
	    {
	      /* Adds 3 color channel values */
	      for (C = 0; C < channels; C++)
		{
		  color[C] +=
		    data_lsb_get (from_buffer, channel_size) * to_resolution;
		  from_buffer += channel_size;
		}
	    }
	  else
	    {
	      /* fc3c */
	      to_pos++;
	      smres = to_resolution - (rescont - from_resolution);
	      for (C = 0; C < channels; C++)
		{
		  value =
		    ((data_lsb_get (from_buffer, channel_size) * smres) +
		     color[C]) / from_resolution;
		  data_lsb_set (to_buffer, value, channel_size);
		  color[C] =
		    data_lsb_get (from_buffer,
				  channel_size) * (rescont - from_resolution);

		  to_buffer += channel_size;
		  from_buffer += channel_size;
		}
	      rescont -= from_resolution;
	    }
	}

      rst = OK;
    }
  else
    {
      /* fd60 */
      SANE_Int bit, pos, desp, rescont2;

      *to_buffer = 0;
      bit = 0;
      pos = 0;
      desp = 0;
      rescont = 0;
      rescont2 = 0;
      if (to_width > 0)
	{
	  do
	    {
	      if (bit == 8)
		{
		  /* fda6 */
		  bit = 0;
		  to_buffer++;
		  *to_buffer = 0;
		}

	      rescont += to_resolution;
	      if (rescont < from_resolution)
		{
		  if ((*from_buffer & (0x80 >> desp)) != 0)
		    rescont2 += to_resolution;
		}
	      else
		{
		  /*fdd5 */
		  pos++;
		  rescont -= from_resolution;
		  if ((*from_buffer & (0x80 >> desp)) != 0)
		    /*fdee */
		    rescont2 += (to_resolution - rescont);
		  if (rescont2 > (to_resolution / 2))
		    /* fe00 */
		    *to_buffer = _B0 (*to_buffer | (0x80 >> bit));
		  rescont2 =
		    ((*from_buffer & (0x80 >> desp)) != 0) ? rescont : 0;
		  bit++;
		}

	      /* fe2f */
	      desp++;
	      if (desp == 8)
		{
		  desp = 0;
		  from_buffer++;
		}
	    }
	  while (pos < to_width);
	}
      else
	rst = OK;
    }

  DBG (DBG_FNC, "- Resize_Decrease: %i\n", rst);

  return rst;
}

static SANE_Int
Resize_Increase (SANE_Byte * to_buffer, SANE_Int to_resolution,
		 SANE_Int to_width, SANE_Byte * from_buffer,
		 SANE_Int from_resolution, SANE_Int from_width,
		 SANE_Int myresize_mode)
{
  /*

     to_buffer       = FAC8 = 0x2353f0
     to_resolution   = FACC = 0x4b
     to_width        = FAD0 = 0x352
     from_buffer     = FAD4 = 0x234650
     from_resolution = FAD8 = 0x64
     from_width      = FADC = 0x46d
     myresize_mode   = FAE0 = 1
   */

  SANE_Int rst = ERROR;
  SANE_Int desp;		/* fac0 */
  SANE_Byte *myp2;		/* faac */
  SANE_Int mywidth;		/* fab4 fab8 */
  SANE_Int mychannels;		/* fabc */
  SANE_Int channels = 0;	/* faa4 */
  SANE_Int depth = 0;		/* faa8 */
  SANE_Int pos = 0;		/* fae0 */
  SANE_Int rescount;
  SANE_Int val6 = 0;
  SANE_Int val7 = 0;
  SANE_Int value;
  /**/
    DBG (DBG_FNC,
	 "+ Resize_Increase(*to_buffer, to_resolution=%i, to_width=%i, *from_buffer, from_resolution=%i, from_width=%i, myresize_mode=%i):\n",
	 to_resolution, to_width, from_resolution, from_width, myresize_mode);

  if (myresize_mode != RSZ_LINEART)
    {
      switch (myresize_mode)
	{
	case RSZ_GRAYL:
	  channels = 1;
	  depth = 8;
	  break;
	case RSZ_COLOURL:
	  channels = 3;
	  depth = 8;
	  break;
	case RSZ_COLOURH:
	  channels = 3;
	  depth = 16;
	  break;
	case RSZ_GRAYH:
	  channels = 1;
	  depth = 16;
	  break;
	}

      if (channels > 0)
	{
	  SANE_Byte channel_size;
	  SANE_Byte *p_dst;	/* fac8 */
	  SANE_Byte *p_src;	/* fad4 */

	  desp = to_buffer - from_buffer;
	  myp2 = from_buffer;
	  channel_size = (depth == 8) ? 1 : 2;

	  for (mychannels = 0; mychannels < channels; mychannels++)
	    {
	      pos = 0;
	      rescount = (from_resolution / 2) + to_resolution;

	      p_src = myp2;
	      p_dst = myp2 + desp;

	      /* f938 */
	      val7 = data_lsb_get (p_src, channel_size);

	      if (to_width > 0)
		{
		  for (mywidth = 0; mywidth < to_width; mywidth++)
		    {
		      if (rescount >= to_resolution)
			{
			  rescount -= to_resolution;
			  val6 = val7;
			  pos++;
			  if (pos < from_width)
			    {
			      p_src += (channels * channel_size);
			      val7 = data_lsb_get (p_src, channel_size);
			    }
			}

		      /*f9a5 */
		      data_lsb_set (p_dst,
				    ((((to_resolution - rescount) * val6) +
				      (val7 * rescount)) / to_resolution),
				    channel_size);
		      rescount += from_resolution;
		      p_dst += (channels * channel_size);
		    }
		}

	      myp2 += channel_size;
	    }

	  rst = OK;
	}
      else
	rst = OK;
    }
  else
    {
      /* RSZ_LINEART mode */
      /* fa02 */
      /*
         to_buffer = FAC8 = 0x2353f0
         to_resolution      = FACC = 0x4b
         to_width    = FAD0 = 0x352
         from_buffer = FAD4 = 0x234650
         from_resolution      = FAD8 = 0x64
         from_width    = FADC = 0x46d
         myresize_mode   = FAE0 = 1
       */
      SANE_Int myres2;		/* fac8 */
      SANE_Int sres;
      SANE_Int lfae0;
      SANE_Int lfad8;
      SANE_Int myres;
      SANE_Int cont = 1;
      SANE_Int someval;
      SANE_Int bit;		/*lfaa8 */

      myres2 = from_resolution;
      sres = (myres2 / 2) + to_resolution;
      value = _B0 (*from_buffer);
      bit = 0;
      lfae0 = 0;
      lfad8 = value >> 7;
      someval = lfad8;
      *to_buffer = 0;

      if (to_width > 0)
	{
	  myres = to_resolution;
	  to_resolution = myres / 2;
	  do
	    {
	      if (sres >= myres)
		{
		  sres -= myres;
		  lfae0++;
		  cont++;
		  lfad8 = someval;
		  if (lfae0 < from_width)
		    {
		      if (cont == 8)
			{
			  cont = 0;
			  from_buffer++;
			}
		      bit = (((0x80 >> cont) & *from_buffer) != 0) ? 1 : 0;
		    }
		}
	      /*faa6 */
	      if ((((myres - sres) * lfad8) + (bit * sres)) > to_resolution)
		*to_buffer |= (0x80 >> bit);

	      bit++;
	      if (bit == 8)
		{
		  bit = 0;
		  to_buffer++;
		  *to_buffer = 0;
		}
	      to_width--;
	      sres += myres2;
	    }
	  while (to_width > 0);
	  rst = OK;
	}
    }

  DBG (DBG_FNC, "- Resize_Increase: %i\n", rst);

  return rst;
}

static SANE_Int
Resize_Start (struct st_device *dev, SANE_Int * transferred)
{
  SANE_Int rst = ERROR;
  struct st_resize *rz = dev->Resize;

  DBG (DBG_FNC, "+ Resize_Start(*transferred):\n");

  if (Resize_CreateBuffers
      (dev, line_size, rz->bytesperline, rz->bytesperline) == ERROR)
    return ERROR;

  if (arrangeline2 == FIX_BY_SOFT)
    {
      /* fee0 */
      if (scan2.colormode == CM_COLOR)
	rst = Arrange_Colour (dev, rz->v3624, line_size, transferred);
      else
	rst = Arrange_NonColour (dev, rz->v3624, line_size, transferred);
    }
  else
    rst = Read_Block (dev, line_size, rz->v3624, transferred);	/* ff03 */

  /* Redimensionado */
  switch (rz->type)
    {
    case RSZ_DECREASE:
      /* ff1b */
      Resize_Decrease (rz->v3628, rz->resolution_x, rz->towidth, rz->v3624,
		       scan2.resolution_x, rz->fromwidth, rz->mode);
      break;
    case RSZ_INCREASE:
      /* ff69 */
      rz->rescount = 0;
      Resize_Increase (rz->v3628, rz->resolution_x, rz->towidth, rz->v3624,
		       scan2.resolution_x, rz->fromwidth, rz->mode);
      if (arrangeline2 == FIX_BY_SOFT)
	{
	  /* ffb1 */
	  if (scan2.colormode == CM_COLOR)
	    rst = Arrange_Colour (dev, rz->v3624, line_size, transferred);
	  else
	    rst = Arrange_NonColour (dev, rz->v3624, line_size, transferred);
	}
      else
	rst = Read_Block (dev, line_size, rz->v3624, transferred);	/* ffe0 */

      /* fff2 */
      Resize_Increase (rz->v362c, rz->resolution_x, rz->towidth, rz->v3624,
		       scan2.resolution_x, rz->fromwidth, rz->mode);
      break;
    }

  /* 002a */

  DBG (DBG_FNC, "- Resize_Start(*transferred=%i): %i\n", *transferred, rst);

  return rst;
}

static SANE_Int
Resize_CreateBuffers (struct st_device *dev, SANE_Int size1, SANE_Int size2,
		      SANE_Int size3)
{
  SANE_Int rst = ERROR;
  struct st_resize *rz = dev->Resize;

  rz->v3624 = (SANE_Byte *) malloc ((size1 + 0x40) * sizeof (SANE_Byte));
  rz->v3628 = (SANE_Byte *) malloc ((size2 + 0x40) * sizeof (SANE_Byte));
  rz->v362c = (SANE_Byte *) malloc ((size3 + 0x40) * sizeof (SANE_Byte));

  if ((rz->v3624 == NULL) || (rz->v3628 == NULL) || (rz->v362c == NULL))
    Resize_DestroyBuffers (dev);
  else
    rst = OK;

  DBG (DBG_FNC, "> Resize_CreateBuffers(size1=%i, size2=%i, size3=%i): %i\n",
       size1, size2, size3, rst);

  return rst;
}

static SANE_Int
Resize_DestroyBuffers (struct st_device *dev)
{
  struct st_resize *rz = dev->Resize;

  if (rz->v3624 != NULL)
    free (rz->v3624);

  if (rz->v3628 != NULL)
    free (rz->v3628);

  if (rz->v362c != NULL)
    free (rz->v362c);

  rz->v3624 = NULL;
  rz->v3628 = NULL;
  rz->v362c = NULL;

  return OK;
}

static SANE_Int
Reading_DestroyBuffers (struct st_device *dev)
{
  DBG (DBG_FNC, "> Reading_DestroyBuffers():\n");

  if (dev->Reading->DMABuffer != NULL)
    free (dev->Reading->DMABuffer);

  if (dev->scanning->imagebuffer != NULL)
    {
      free (dev->scanning->imagebuffer);
      dev->scanning->imagebuffer = NULL;
    }

  bzero (dev->Reading, sizeof (struct st_readimage));

  return OK;
}

static SANE_Int
Gamma_SendTables (struct st_device *dev, SANE_Byte * Regs,
		  SANE_Byte * gammatable, SANE_Int size)
{
  SANE_Int rst = ERROR;

  DBG (DBG_FNC, "+ Gamma_SendTables(*Regs, *gammatable, size=%i):\n", size);

  if ((gammatable != NULL) && (size > 0))
    {
      SANE_Int transferred;
      SANE_Int first_table;
      SANE_Int cont = 0;
      SANE_Int retry = TRUE;
      SANE_Byte *mybuffer;

      /* lock */
      SetLock (dev->usb_handle, Regs, TRUE);

      first_table = (data_lsb_get (&Regs[0x1b4], 2) & 0x3fff) >> 4;

      mybuffer = (SANE_Byte *) malloc (sizeof (SANE_Byte) * size);
      if (mybuffer != NULL)
	{
	  /* Try to send buffer during 10 seconds */
	  long tick = GetTickCount () + 10000;
	  while ((retry == TRUE) && (tick > GetTickCount ()))
	    {
	      retry = FALSE;

	      /* Operation type 0x14 */
	      if (IWrite_Word (dev->usb_handle, 0x0000, 0x0014, 0x0800) == OK)
		{
		  /* Send size to write */
		  if (RTS_DMA_Enable_Write (dev, 0x0000, size, first_table) ==
		      OK)
		    {
		      /* Send data */
		      if (Bulk_Operation
			  (dev, BLK_WRITE, size, gammatable,
			   &transferred) == OK)
			{
			  /* Send size to read */
			  if (RTS_DMA_Enable_Read
			      (dev, 0x0000, size, first_table) == OK)
			    {
			      /* Retrieve data */
			      if (Bulk_Operation
				  (dev, BLK_READ, size, mybuffer,
				   &transferred) == OK)
				{
				  /* Check data */
				  while ((cont < size) && (retry == FALSE))
				    {
				      if (mybuffer[cont] != gammatable[cont])
					retry = TRUE;
				      cont++;
				    }

				  if (retry == FALSE)
				    rst = OK;
				}
			    }
			}
		    }
		}
	    }

	  free (mybuffer);
	}

      /* unlock */
      SetLock (dev->usb_handle, Regs, FALSE);
    }

  DBG (DBG_FNC, "- Gamma_SendTables: %i\n", rst);

  return rst;
}

static SANE_Int
Gamma_GetTables (struct st_device *dev, SANE_Byte * Gamma_buffer)
{
  SANE_Int rst = ERROR;

  DBG (DBG_FNC, "+ Gamma_GetTables(SANE_Byte *Gamma_buffer):\n");

  if (Gamma_buffer == NULL)
    return ERROR;

  /* Operation type 0x14 */
  if (IWrite_Word (dev->usb_handle, 0x0000, 0x0014, 0x0800) == 0x00)
    {
      SANE_Int size = 768;

      if (RTS_DMA_Enable_Read (dev, 0x0000, size, 0) == OK)
	{
	  SANE_Int transferred = 0;
	  usleep (1000 * 500);

	  /* Read buffer */
	  rst =
	    Bulk_Operation (dev, BLK_READ, size, Gamma_buffer, &transferred);
	}
    }

  DBG (DBG_FNC, "- Gamma_GetTables: %i\n", rst);

  return rst;
}

static void
Gamma_FreeTables ()
{
  SANE_Int c;

  DBG (DBG_FNC, "> Gamma_FreeTables()\n");

  for (c = 0; c < 3; c++)
    {
      if (hp_gamma->table[c] != NULL)
	{
	  free (hp_gamma->table[c]);
	  hp_gamma->table[c] = NULL;
	}
    }
  use_gamma_tables = FALSE;
}

static void
RTS_Scanner_StopScan (struct st_device *dev, SANE_Int wait)
{
  SANE_Byte data;

  DBG (DBG_FNC, "+ RTS_Scanner_StopScan():\n");

  data = 0;

  Reading_DestroyBuffers (dev);
  Resize_DestroyBuffers (dev);

  RTS_DMA_Reset (dev);

  data_bitset (&dev->init_regs[0x60b], 0x10, 0);
  data_bitset (&dev->init_regs[0x60a], 0x40, 0);

  if (Write_Buffer (dev->usb_handle, 0xee0a, &dev->init_regs[0x60a], 2) == OK)
    Motor_Change (dev, dev->init_regs, 3);

  usleep (1000 * 200);

  if (wait == FALSE)
    {
      Read_Byte (dev->usb_handle, 0xe801, &data);
      if ((data & 0x02) == 0)
	{
	  if (Head_IsAtHome (dev, dev->init_regs) == FALSE)
	    {
	      /* clear execution bit */
	      data_bitset (&dev->init_regs[0x00], 0x80, 0);

	      Write_Byte (dev->usb_handle, 0x00, dev->init_regs[0x00]);
	      Head_ParkHome (dev, TRUE, dev->motorcfg->parkhomemotormove);
	    }
	}
    }
  else
    {
      /*66a1 */
      /* clear execution bit */
      data_bitset (&dev->init_regs[0x00], 0x80, 0);

      Write_Byte (dev->usb_handle, 0x00, dev->init_regs[0x00]);
      if (Head_IsAtHome (dev, dev->init_regs) == FALSE)
	Head_ParkHome (dev, TRUE, dev->motorcfg->parkhomemotormove);
    }

  /*66e0 */
  RTS_Enable_CCD (dev, dev->init_regs, 0);

  Lamp_Status_Timer_Set (dev, 13);

  DBG (DBG_FNC, "- RTS_Scanner_StopScan()\n");
}

static SANE_Int
Reading_CreateBuffers (struct st_device *dev)
{
  SANE_Byte data;
  SANE_Int mybytesperline;
  SANE_Int mybuffersize, a, b;

  DBG (DBG_FNC, "+ Reading_CreateBuffers():\n");

  data = 0;

  /* Gets BinarythresholdH */
  if (Read_Byte (dev->usb_handle, 0xe9a1, &data) == OK)
    binarythresholdh = data;

  mybytesperline =
    (scan2.depth == 12) ? (bytesperline * 3) / 4 : bytesperline;

  dev->Reading->Max_Size = 0xfc00;
  dev->Reading->DMAAmount = 0;

  a = (RTS_Debug->dmabuffersize / 63);
  b = (((RTS_Debug->dmabuffersize - a) / 2) + a) >> 0x0f;
  mybuffersize = ((b << 6) - b) << 10;
  if (mybuffersize < 0x1f800)
    mybuffersize = 0x1f800;

  dev->Reading->DMABufferSize = mybuffersize;	/*3FFC00 4193280 */

  do
    {
      dev->Reading->DMABuffer =
	(SANE_Byte *) malloc (dev->Reading->DMABufferSize *
			      sizeof (SANE_Byte));
      if (dev->Reading->DMABuffer != NULL)
	break;
      dev->Reading->DMABufferSize -= dev->Reading->Max_Size;
    }
  while (dev->Reading->DMABufferSize >= dev->Reading->Max_Size);

  /* 6003 */
  dev->Reading->Starting = TRUE;

  dev->Reading->Size4Lines = (mybytesperline > dev->Reading->Max_Size) ?
    mybytesperline : (dev->Reading->Max_Size / mybytesperline) *
    mybytesperline;

  dev->Reading->ImageSize = imagesize;
  read_v15b4 = v15b4;

  DBG (DBG_FNC, "- Reading_CreateBuffers():\n");

  return OK;
}

static SANE_Int
RTS_ScanCounter_Inc (struct st_device *dev)
{
  /* Keep a count of the number of scans done by this scanner */

  SANE_Int idata;

  DBG (DBG_FNC, "+ RTS_ScanCounter_Inc():\n");

  /* check if chipset supports accessing eeprom */
  if ((dev->chipset->capabilities & CAP_EEPROM) != 0)
    {
      SANE_Byte cdata = 0;
      SANE_Byte somebuffer[26];

      switch (dev->chipset->model)
	{
	case RTS8822L_02A:
	case RTS8822BL_03A:
	  /* value is 4 bytes size starting from address 0x21 in msb format */
	  if (RTS_EEPROM_ReadInteger (dev->usb_handle, 0x21, &idata) == OK)
	    {
	      idata = data_swap_endianess (idata, 4) + 1;
	      idata = data_swap_endianess (idata, 4);
	      RTS_EEPROM_WriteInteger (dev->usb_handle, 0x21, idata);
	    }
	  break;
	default:
	  /* value is 4 bytes size starting from address 0x21 in lsb format */
	  bzero (&somebuffer, sizeof (somebuffer));
	  somebuffer[4] = 0x0c;

	  RTS_EEPROM_ReadInteger (dev->usb_handle, 0x21, &idata);
	  data_lsb_set (&somebuffer[0], idata + 1, 4);

	  RTS_EEPROM_ReadByte (dev->usb_handle, 0x003a, &cdata);
	  somebuffer[25] = cdata;
	  RTS_EEPROM_WriteBuffer (dev->usb_handle, 0x21, somebuffer, 0x1a);
	  break;
	}
    }

  DBG (DBG_FNC, "- RTS_ScanCounter_Inc()\n");

  return OK;
}

static SANE_Int
RTS_ScanCounter_Get (struct st_device *dev)
{
  /* Returns the number of scans done by this scanner */

  SANE_Int idata = 0;

  DBG (DBG_FNC, "+ RTS_ScanCounter_Get():\n");

  /* check if chipset supports accessing eeprom */
  if ((dev->chipset->capabilities & CAP_EEPROM) != 0)
    {
      RTS_EEPROM_ReadInteger (dev->usb_handle, 0x21, &idata);

      switch (dev->chipset->model)
	{
	case RTS8822L_02A:
	case RTS8822BL_03A:
	  /* value is 4 bytes size starting from address 0x21 in msb format */
	  idata = data_swap_endianess (idata, 4);
	  break;
	default:		/* RTS8822L_01H */
	  /* value is 4 bytes size starting from address 0x21 in lsb format */
	  idata &= 0xffffffff;
	  break;
	}
    }

  DBG (DBG_FNC, "- RTS_ScanCounter_Get(): %i\n", idata);

  return idata;
}

static SANE_Int
Read_Image (struct st_device *dev, SANE_Int buffer_size, SANE_Byte * buffer,
	    SANE_Int * transferred)
{
  SANE_Int rst;
  SANE_Byte mycolormode;

  DBG (DBG_FNC, "+ Read_Image(buffer_size=%i, *buffer, *transferred):\n",
       buffer_size);

  *transferred = 0;
  mycolormode = scan2.colormode;
  rst = ERROR;
  if ((scan2.colormode != CM_COLOR) && (scan2.channel == 3))
    mycolormode = 3;

  if (dev->Resize->type == RSZ_NONE)
    {
      if (arrangeline == FIX_BY_SOFT)
	{
	  switch (mycolormode)
	    {
	    case CM_COLOR:
	      rst = Arrange_Colour (dev, buffer, buffer_size, transferred);
	      break;
	    case 3:
	      rst = Arrange_Compose (dev, buffer, buffer_size, transferred);
	      break;
	    default:
	      rst = Arrange_NonColour (dev, buffer, buffer_size, transferred);
	      break;
	    }
	}
      else
	rst = Read_Block (dev, buffer_size, buffer, transferred);	/*00fe */
    }
  else
    rst = Read_ResizeBlock (dev, buffer, buffer_size, transferred);	/*010d */

  DBG (DBG_FNC, "- Read_Image(*transferred=%i): %i\n", *transferred, rst);

  return rst;
}

static SANE_Int
Arrange_Compose (struct st_device *dev, SANE_Byte * buffer,
		 SANE_Int buffer_size, SANE_Int * transferred)
{
  /*
     fnb250

     0600FA7C   05E10048 buffer
     0600FA80   0000F906 buffer_size
   */
  SANE_Byte *mybuffer = buffer;	/* fa7c */
  SANE_Int mydistance;		/*ebp */
  SANE_Int mydots;		/*fa74 */
  SANE_Int channel_size;
  SANE_Int c;
  struct st_scanning *scn;

  /*mywidth = fa70 */

  DBG (DBG_FNC, "+ Arrange_Compose(*buffer, buffer_size=%i, *transferred):\n",
       buffer_size);

  channel_size = (scan2.depth == 8) ? 1 : 2;

  /* this is just to make code more legible */
  scn = dev->scanning;

  if (scn->imagebuffer == NULL)
    {
      if (dev->sensorcfg->type == CCD_SENSOR)
	mydistance =
	  (dev->sensorcfg->line_distance * scan2.resolution_y) /
	  dev->sensorcfg->resolution;
      else
	mydistance = 0;

      if (mydistance != 0)
	{
	  scn->bfsize =
	    (scn->arrange_hres ==
	     TRUE) ? scn->arrange_sensor_evenodd_dist : 0;
	  scn->bfsize = line_size * (scn->bfsize + (mydistance * 2) + 1);
	}
      else
	scn->bfsize = line_size * 2;

      /*b2f0 */
      scn->imagebuffer =
	(SANE_Byte *) malloc (scn->bfsize * sizeof (SANE_Byte));
      if (scn->imagebuffer == NULL)
	return ERROR;

      scn->imagepointer = scn->imagebuffer;
      if (Read_Block (dev, scn->bfsize, scn->imagebuffer, transferred) ==
	  ERROR)
	return ERROR;

      /* Calculate channel displacements */
      scn->arrange_orderchannel = FALSE;
      for (c = CL_RED; c <= CL_BLUE; c++)
	{
	  if (mydistance == 0)
	    {
	      /*b34e */
	      if (scn->arrange_hres == FALSE)
		{
		  if ((((dev->sensorcfg->line_distance * scan2.resolution_y) *
			2) / dev->sensorcfg->resolution) == 1)
		    scn->arrange_orderchannel = TRUE;

		  if (scn->arrange_orderchannel == TRUE)
		    scn->desp[c] =
		      ((dev->sensorcfg->rgb_order[c] / 2) * line_size) +
		      (channel_size * c);
		  else
		    scn->desp[c] = channel_size * c;
		}
	    }
	  else
	    {
	      /*b3e3 */
	      scn->desp[c] =
		(dev->sensorcfg->rgb_order[c] * (mydistance * line_size)) +
		(channel_size * c);

	      if (scn->arrange_hres == TRUE)
		{
		  /*b43b */
		  scn->desp1[c] = scn->desp[c];
		  scn->desp2[c] =
		    ((channel_size * 3) + scn->desp1[c]) +
		    (scn->arrange_sensor_evenodd_dist * line_size);
		};
	    }
	}

      for (c = CL_RED; c <= CL_BLUE; c++)
	{
	  if (scn->arrange_hres == TRUE)
	    {
	      scn->pColour2[c] = scn->imagebuffer + scn->desp2[c];
	      scn->pColour1[c] = scn->imagebuffer + scn->desp1[c];
	    }
	  else
	    scn->pColour[c] = scn->imagebuffer + scn->desp[c];
	}
    }

  /*b545 */
  buffer_size /= line_size;
  mydots = line_size / (channel_size * 3);

  while (buffer_size > 0)
    {
      if (scn->arrange_orderchannel == FALSE)
	{
	  /*b5aa */
	  if (scn->arrange_hres == TRUE)
	    Triplet_Compose_HRes (scn->pColour1[CL_RED],
				  scn->pColour1[CL_GREEN],
				  scn->pColour1[CL_BLUE],
				  scn->pColour2[CL_RED],
				  scn->pColour2[CL_GREEN],
				  scn->pColour2[CL_BLUE], mybuffer, mydots);
	  else
	    Triplet_Compose_LRes (scn->pColour[CL_RED],
				  scn->pColour[CL_GREEN],
				  scn->pColour[CL_BLUE], mybuffer, mydots);
	}
      else
	Triplet_Compose_Order (dev, scn->pColour[CL_RED],
			       scn->pColour[CL_GREEN], scn->pColour[CL_BLUE],
			       mybuffer, mydots);

      /*b5f8 */
      mybuffer += line_size;
      scn->arrange_size -= bytesperline;
      if (scn->arrange_size < 0)
	v15bc--;

      buffer_size--;
      if (buffer_size == 0)
	{
	  if ((scn->arrange_size | v15bc) == 0)
	    return OK;
	}

      /*b63f */
      if (Read_Block (dev, line_size, scn->imagepointer, transferred) ==
	  ERROR)
	return ERROR;

      for (c = CL_RED; c <= CL_BLUE; c++)
	{
	  if (scn->arrange_hres == TRUE)
	    {
	      /*b663 */
	      scn->desp2[c] = (scn->desp2[c] + line_size) % scn->bfsize;
	      scn->desp1[c] = (scn->desp1[c] + line_size) % scn->bfsize;

	      scn->pColour2[c] = scn->imagebuffer + scn->desp2[c];
	      scn->pColour1[c] = scn->imagebuffer + scn->desp1[c];
	    }
	  else
	    {
	      /*b74a */
	      scn->desp[c] = (scn->desp[c] + line_size) % scn->bfsize;
	      scn->pColour[c] = scn->imagebuffer + scn->desp[c];
	    }
	}

      /*b7be */
      scn->imagepointer += line_size;
      if (scn->imagepointer >= (scn->imagebuffer + scn->bfsize))
	scn->imagepointer = scn->imagebuffer;
    }

  return OK;
}

static void
Triplet_Compose_HRes (SANE_Byte * pRed1, SANE_Byte * pGreen1,
		      SANE_Byte * pBlue1, SANE_Byte * pRed2,
		      SANE_Byte * pGreen2, SANE_Byte * pBlue2,
		      SANE_Byte * buffer, SANE_Int Width)
{
  SANE_Int Value;
  SANE_Int Channel_size;
  SANE_Int max_value;

  DBG (DBG_FNC,
       "> Triplet_Compose_HRes(*pRed1, *pGreen1, *pBlue1, *pRed2 *pGreen2, *pBlue2, *buffer, Width=%i):\n",
       Width);

  Width /= 2;
  Channel_size = (scan2.depth > 8) ? 2 : 1;
  max_value = (1 << scan2.depth) - 1;

  while (Width > 0)
    {
      Value =
	data_lsb_get (pRed1, Channel_size) + data_lsb_get (pGreen1,
							   Channel_size) +
	data_lsb_get (pBlue1, Channel_size);

      Value = min (Value, max_value);

      if (v1600 != NULL)
	{
	  if (scan2.depth > 8)
	    Value = *(v1600 + (Value >> 8)) | _B0 (Value);
	  else
	    Value = *(v1600 + Value);
	}

      data_lsb_set (buffer, Value, Channel_size);
      buffer += Channel_size;

      Value =
	data_lsb_get (pRed2, Channel_size) + data_lsb_get (pGreen2,
							   Channel_size) +
	data_lsb_get (pBlue2, Channel_size);

      Value = min (Value, max_value);

      if (v1600 != NULL)
	{
	  if (scan2.depth > 8)
	    Value = *(v1600 + (Value >> 8)) | _B0 (Value);
	  else
	    Value = *(v1600 + Value);
	}

      data_lsb_set (buffer, Value, Channel_size);
      buffer += Channel_size;

      pRed1 += 6 * Channel_size;
      pGreen1 += 6 * Channel_size;
      pBlue1 += 6 * Channel_size;

      pRed2 += 6 * Channel_size;
      pGreen2 += 6 * Channel_size;
      pBlue2 += 6 * Channel_size;

      Width--;
    }
}

static void
Triplet_Compose_Order (struct st_device *dev, SANE_Byte * pRed,
		       SANE_Byte * pGreen, SANE_Byte * pBlue,
		       SANE_Byte * buffer, SANE_Int dots)
{
  SANE_Int Value;

  DBG (DBG_FNC,
       "> Triplet_Compose_Order(*pRed, *pGreen, *pBlue, *buffer, dots=%i):\n",
       dots);

  if (scan2.depth > 8)
    {
      /* c0fe */
      dots = dots / 2;
      while (dots > 0)
	{
	  Value =
	    min (data_lsb_get (pRed, 2) + data_lsb_get (pGreen, 2) +
		 data_lsb_get (pBlue, 2), 0xffff);

	  if (v1600 != NULL)
	    Value = (*(v1600 + (Value >> 8)) << 8) | _B0 (Value);

	  data_lsb_set (buffer, Value, 2);

	  buffer += 2;
	  pRed += 6;
	  pGreen += 6;
	  pBlue += 6;
	  dots--;
	}
    }
  else
    {
      SANE_Byte *myp1, *myp2, *myp3;

      if (dev->sensorcfg->rgb_order[CL_RED] == 1)
	{
	  myp1 = pRed;
	  myp2 = pGreen;
	  myp3 = pBlue;
	}
      else if (dev->sensorcfg->rgb_order[CL_GREEN] == 1)
	{
	  myp1 = pGreen;
	  myp2 = pRed;
	  myp3 = pBlue;
	}
      else
	{
	  myp1 = pBlue;
	  myp2 = pRed;
	  myp3 = pGreen;
	}

      while (dots > 0)
	{
	  Value =
	    min (((*myp1 + *(line_size + myp1)) / 2) + *myp2 + *myp3, 0xff);

	  *buffer = (v1600 == NULL) ? _B0 (Value) : *(v1600 + Value);

	  buffer++;
	  myp1 += 3;
	  myp2 += 3;
	  myp3 += 3;
	  dots--;
	}
    }
}

static void
Triplet_Compose_LRes (SANE_Byte * pRed, SANE_Byte * pGreen, SANE_Byte * pBlue,
		      SANE_Byte * buffer, SANE_Int dots)
{
  SANE_Int Value;
  SANE_Int Channel_size;
  SANE_Int max_value;

  DBG (DBG_FNC,
       "> Triplet_Compose_LRes(*pRed, *pGreen, *pBlue, *buffer, dots=%i):\n",
       dots);

  Channel_size = (scan2.depth > 8) ? 2 : 1;
  max_value = (1 << scan2.depth) - 1;

  /*bf59 */
  while (dots > 0)
    {
      Value =
	data_lsb_get (pRed, Channel_size) + data_lsb_get (pGreen,
							  Channel_size) +
	data_lsb_get (pBlue, Channel_size);

      Value = min (Value, max_value);

      if (v1600 != NULL)
	{
	  if (scan2.depth > 8)
	    Value = (*(v1600 + (Value >> 8)) << 8) | _B0 (Value);
	  else
	    Value = _B0 (*(v1600 + Value));
	}

      data_lsb_set (buffer, Value, Channel_size);

      buffer += Channel_size;
      pRed += Channel_size * 3;
      pGreen += Channel_size * 3;
      pBlue += Channel_size * 3;
      dots--;
    }
}

static void
Triplet_Colour_Order (struct st_device *dev, SANE_Byte * pRed,
		      SANE_Byte * pGreen, SANE_Byte * pBlue,
		      SANE_Byte * buffer, SANE_Int Width)
{
  SANE_Int Value;

  DBG (DBG_FNC,
       "> Triplet_Colour_Order(*pRed, *pGreen, *pBlue, *buffer, Width=%i):\n",
       Width);

  if (scan2.depth > 8)
    {
      Width = Width / 2;
      while (Width > 0)
	{
	  Value = data_lsb_get (pRed, 2);
	  data_lsb_set (buffer, Value, 2);

	  Value = data_lsb_get (pGreen, 2);
	  data_lsb_set (buffer + 2, Value, 2);

	  Value = data_lsb_get (pBlue, 2);
	  data_lsb_set (buffer + 4, Value, 2);

	  pRed += 6;
	  pGreen += 6;
	  pBlue += 6;
	  buffer += 6;
	  Width--;
	}
    }
  else
    {
      SANE_Int Colour;

      if (dev->sensorcfg->rgb_order[CL_RED] == 1)
	Colour = CL_RED;
      else if (dev->sensorcfg->rgb_order[CL_GREEN] == 1)
	Colour = CL_GREEN;
      else
	Colour = CL_BLUE;

      while (Width > 0)
	{
	  switch (Colour)
	    {
	    case CL_RED:
	      *buffer = (*pRed + *(pRed + line_size)) / 2;
	      *(buffer + 1) = *pGreen;
	      *(buffer + 2) = *pBlue;
	      break;
	    case CL_GREEN:
	      *buffer = *pRed;
	      *(buffer + 1) = ((*pGreen + *(pGreen + line_size)) / 2);
	      *(buffer + 2) = *pBlue;
	      break;
	    case CL_BLUE:
	      *buffer = *pRed;
	      *(buffer + 1) = *pGreen;
	      *(buffer + 2) = ((*pBlue + *(pBlue + line_size)) / 2);
	      break;
	    }

	  pRed += 3;
	  pGreen += 3;
	  pBlue += 3;
	  buffer += 3;

	  Width--;
	}
    }
}

static void
Triplet_Colour_HRes (SANE_Byte * pRed1, SANE_Byte * pGreen1,
		     SANE_Byte * pBlue1, SANE_Byte * pRed2,
		     SANE_Byte * pGreen2, SANE_Byte * pBlue2,
		     SANE_Byte * buffer, SANE_Int Width)
{
  SANE_Int Value;
  SANE_Int channel_size;
  SANE_Int c;
  SANE_Byte *pPointers[6];

  pPointers[0] = pRed1;
  pPointers[1] = pGreen1;
  pPointers[2] = pBlue1;

  pPointers[3] = pRed2;
  pPointers[4] = pGreen2;
  pPointers[5] = pBlue2;

  DBG (DBG_FNC,
       "> Triplet_Colour_HRes(*pRed1, *pGreen1, *pBlue1, *pRed2, *pGreen2, *pBlue2, *buffer, Width=%i):\n",
       Width);

  channel_size = (scan2.depth > 8) ? 2 : 1;

  Width = Width / 2;
  while (Width > 0)
    {
      for (c = 0; c < 6; c++)
	{
	  Value = data_lsb_get (pPointers[c], channel_size);
	  data_lsb_set (buffer, Value, channel_size);

	  pPointers[c] += (6 * channel_size);
	  buffer += (channel_size);
	}
      Width--;
    }
}

static void
Triplet_Colour_LRes (SANE_Int Width, SANE_Byte * Buffer,
		     SANE_Byte * pChannel1, SANE_Byte * pChannel2,
		     SANE_Byte * pChannel3)
{
  /*
     05F0FA4C   04EBAE4A  /CALL to Assumed StdFunc6 from hpgt3970.04EBAE45
     05F0FA50   00234FF8  |Arg1 = 00234FF8 pChannel3
     05F0FA54   002359EF  |Arg2 = 002359EF pChannel2
     05F0FA58   002363E6  |Arg3 = 002363E6 pChannel1
     05F0FA5C   05D10048  |Arg4 = 05D10048 Buffer
     05F0FA60   00000352  |Arg5 = 00000352 Width
   */

  /* Esta funcion une los tres canales de color en un triplete
     Inicialmente cada color está separado en 3 buffers apuntados
     por pChannel1 ,2 y 3
   */
  SANE_Int Value;
  SANE_Int channel_size;
  SANE_Int c;
  SANE_Byte *pChannels[3];

  pChannels[0] = pChannel3;
  pChannels[1] = pChannel2;
  pChannels[2] = pChannel1;

  DBG (DBG_FNC, "> Triplet_Colour_LRes(Width=%i, *Buffer2, *p1, *p2, *p3):\n",
       Width);

  channel_size = (scan2.depth > 8) ? 2 : 1;
  while (Width > 0)
    {
      /* ba74 */
      for (c = 0; c < 3; c++)
	{
	  Value = data_lsb_get (pChannels[c], channel_size);
	  data_lsb_set (Buffer, Value, channel_size);

	  pChannels[c] += channel_size;
	  Buffer += channel_size;
	}
      Width--;
    }
}

static SANE_Int
Read_ResizeBlock (struct st_device *dev, SANE_Byte * buffer,
		  SANE_Int buffer_size, SANE_Int * transferred)
{
  /*The Beach
     buffer      = FA7C   05E30048
     buffer_size = FA80   0000F906
   */

  SANE_Int rst = ERROR;		/* fa68 */
  SANE_Int lfa54;
  SANE_Int lfa58;
  SANE_Byte *pP1;		/* fa5c */
  SANE_Byte *pP2;		/* fa60 */
  SANE_Int bOk;
  struct st_resize *rz = dev->Resize;

  /* fa74 = Resize->resolution_y */
  /* fa70 = Resize->resolution_x */
  /* fa64 = scan2.resolution_y  */
  /* fa6c = scan2.resolution_x  */

  DBG (DBG_FNC,
       "+ Read_ResizeBlock(*buffer, buffer_size=%i, *transferred):\n",
       buffer_size);

  if (rz->type == RSZ_DECREASE)
    {
      lfa58 = 0;
      do
	{
	  bOk = 1;
	  if (arrangeline2 == FIX_BY_SOFT)
	    {
	      if (scan2.colormode == CM_COLOR)
		rst = Arrange_Colour (dev, rz->v3624, line_size, transferred);
	      else
		rst =
		  Arrange_NonColour (dev, rz->v3624, line_size, transferred);
	    }
	  else
	    rst = Read_Block (dev, line_size, rz->v3624, transferred);

	  /*f2df */
	  Resize_Decrease (rz->v362c, rz->resolution_x, rz->towidth,
			   rz->v3624, scan2.resolution_x, rz->fromwidth,
			   rz->mode);
	  rz->rescount += rz->resolution_y;

	  if (rz->rescount > scan2.resolution_y)
	    {
	      /*f331 */
	      rz->rescount -= scan2.resolution_y;
	      if (scan2.depth == 8)
		{
		  /* f345 */
		  pP1 = rz->v3628;
		  pP2 = rz->v362c;
		  if (rz->mode == RSZ_LINEART)
		    {
		      /* f36b */
		      SANE_Int bit = 0;
		      SANE_Byte *pP3 = rz->v362c;
		      SANE_Int value;

		      *buffer = 0;
		      lfa54 = 0;
		      while (lfa54 < rz->towidth)
			{
			  if (bit == 8)
			    {
			      buffer++;
			      *buffer = 0;
			      pP1++;
			      bit = 0;
			      pP3++;
			    }

			  value =
			    ((*pP1 & (0x80 >> bit)) != 0) ? rz->rescount : 0;

			  if ((*pP3 & (0x80 >> bit)) != 0)
			    value += (scan2.resolution_y - rz->rescount);

			  if (value > rz->resolution_y)
			    *buffer |= (0x80 >> bit);

			  bit++;
			  lfa54++;
			}
		    }
		  else
		    {
		      /* f414 */
		      lfa54 = 0;
		      while (lfa54 < rz->bytesperline)
			{
			  *buffer =
			    _B0 ((((scan2.resolution_y -
				    rz->rescount) * *pP2) +
				  (*pP1 * rz->rescount)) /
				 scan2.resolution_y);
			  pP1++;
			  pP2++;
			  buffer++;
			  lfa54++;
			}
		    }
		}
	      else
		{
		  /* f47d */
		  lfa54 = 0;
		  pP1 = rz->v3628;
		  pP2 = rz->v362c;

		  if ((rz->bytesperline & 0xfffffffe) > 0)
		    {
		      SANE_Int value;
		      do
			{
			  value =
			    (((scan2.resolution_y -
			       rz->rescount) * data_lsb_get (pP2,
							     2)) +
			     (data_lsb_get (pP1, 2) * rz->rescount)) /
			    scan2.resolution_y;
			  data_lsb_set (buffer, value, 2);

			  buffer += 2;
			  pP1 += 2;
			  pP2 += 2;
			  lfa54++;
			}
		      while (lfa54 < (rz->bytesperline / 2));
		    }
		}
	    }
	  else
	    bOk = 0;
	  /* f4fd f502 */
	  pP1 = rz->v3628;
	  /* swap pointers */
	  rz->v3628 = rz->v362c;
	  rz->v362c = pP1;
	}
      while (bOk == 0);
    }
  else
    {
      /*f530 */
      SANE_Int lfa68;
      SANE_Int transferred;
      SANE_Int channel_size;

      rz->rescount += scan2.resolution_y;
      lfa58 = 0;
      if (rz->rescount > rz->resolution_y)
	{
	  lfa68 = 1;
	  rz->rescount -= rz->resolution_y;
	}
      else
	lfa68 = 0;

      pP1 = rz->v3628;
      pP2 = rz->v362c;

      if (rz->mode == RSZ_LINEART)
	{
	  /*f592 */
	  *buffer = 0;

	  if (rz->towidth > 0)
	    {
	      SANE_Int mask, mres;
	      /* lfa60 = rz->resolution_y     */
	      /* lfa7c = rz->resolution_y / 2 */

	      for (lfa54 = 0; lfa54 < rz->towidth; lfa54++)
		{
		  mask = 0x80 >> lfa58;

		  mres = ((mask & *pP1) != 0) ? rz->rescount : 0;

		  if ((mask & *pP2) != 0)
		    mres += (rz->resolution_y - rz->rescount);

		  if (mres > (rz->resolution_y / 2))
		    *buffer = *buffer | mask;

		  lfa58++;
		  if (lfa58 == 8)
		    {
		      lfa58 = 0;
		      buffer++;
		      pP1++;
		      pP2++;
		      *buffer = 0;
		    }
		}
	    }
	}
      else
	{
	  /*f633 */
	  channel_size = (scan2.depth > 8) ? 2 : 1;

	  if (rz->rescount < scan2.resolution_y)
	    {
	      if (rz->bytesperline != 0)
		{
		  SANE_Int value;

		  for (lfa54 = 0; lfa54 < rz->bytesperline; lfa54++)
		    {
		      value =
			(((scan2.resolution_y -
			   rz->rescount) * data_lsb_get (pP2,
							 channel_size)) +
			 (rz->rescount * data_lsb_get (pP1, channel_size))) /
			scan2.resolution_y;
		      data_lsb_set (buffer, value, channel_size);

		      pP1 += channel_size;
		      pP2 += channel_size;
		      buffer += channel_size;
		    }
		}
	    }
	  else
	    memcpy (buffer, rz->v3628, rz->bytesperline);	/*f6a8 */
	}

      /*f736 */
      if (lfa68 != 0)
	{
	  SANE_Byte *temp;

	  if (arrangeline2 == FIX_BY_SOFT)
	    {
	      /*f74b */
	      if (scan2.colormode == CM_COLOR)
		rst =
		  Arrange_Colour (dev, rz->v3624, line_size, &transferred);
	      else
		rst =
		  Arrange_NonColour (dev, rz->v3624, line_size, &transferred);
	    }
	  else
	    rst = Read_Block (dev, line_size, rz->v3624, &transferred);	/*f77a */

	  /*f78c */
	  /* swap buffers */
	  temp = rz->v3628;
	  rz->v3628 = rz->v362c;
	  rz->v362c = temp;

	  Resize_Increase (temp, rz->resolution_x, rz->towidth, rz->v3624,
			   scan2.resolution_x, rz->fromwidth, rz->mode);
	}
      else
	rst = OK;
    }

  DBG (DBG_FNC, "- Read_ResizeBlock(*transferred=%i): %i\n", *transferred,
       rst);

  return rst;
}

static void
Split_into_12bit_channels (SANE_Byte * destino, SANE_Byte * fuente,
			   SANE_Int size)
{
  /*
     Each letter represents a bit
     abcdefgh 12345678 lmnopqrs << before splitting
     [efgh1234 0000abcd] [lmnopqrs 00005678]  << after splitting, in memory
     [0000abcd efgh1234] [00005678 lmnopqrs]  << resulting channels
   */

  DBG (DBG_FNC, "> Split_into_12bit_channels(*destino, *fuente, size=%i\n",
       size);

  if ((destino != NULL) && (fuente != NULL))
    {
      if ((size - (size & 0x03)) != 0)
	{
	  SANE_Int C;

	  C = (size - (size & 0x03) + 3) / 4;
	  do
	    {
	      *destino = _B0 ((*(fuente + 1) >> 4) + (*fuente << 4));
	      *(destino + 1) = _B0 (*fuente >> 4);
	      *(destino + 2) = _B0 (*(fuente + 2));
	      *(destino + 3) = *(fuente + 1) & 0x0f;
	      destino += 4;
	      fuente += 3;
	      C--;
	    }
	  while (C > 0);
	}

       /**/ if ((size & 0x03) != 0)
	{
	  *destino = _B0 ((*(fuente + 1) >> 4) + (*fuente << 4));
	  *(destino + 1) = _B0 (*fuente >> 4);
	}
    }
}

static SANE_Int
Read_NonColor_Block (struct st_device *dev, SANE_Byte * buffer,
		     SANE_Int buffer_size, SANE_Byte ColorMode,
		     SANE_Int * transferred)
{
  /* FA50   05DA0048 buffer
     FA54   0000F906 buffer_size
     FA58   00       ColorMode
   */

  SANE_Int rst = OK;
  SANE_Int lfa38 = 0;
  SANE_Byte *gamma = v1600;
  SANE_Int block_bytes_per_line;
  SANE_Int mysize;
  SANE_Byte *mybuffer;

  DBG (DBG_FNC,
       "+ Read_NonColor_Block(*buffer, buffer_size=%i, ColorMode=%s):\n",
       buffer_size, dbg_colour (ColorMode));

  if (ColorMode != CM_GRAY)
    {
      /* Lineart mode */
      if ((lineart_width & 7) != 0)
	lfa38 = 8 - (lineart_width & 7);
      block_bytes_per_line = (lineart_width + 7) / 8;
    }
  else
    block_bytes_per_line = line_size;
  /*61b2 */

  mysize = (buffer_size / block_bytes_per_line) * bytesperline;
  mybuffer = (SANE_Byte *) malloc (mysize * sizeof (SANE_Byte));	/*fa40 */

  if (mybuffer != NULL)
    {
      SANE_Int LinesCount;
      SANE_Int mysize4lines;
      SANE_Byte *pBuffer = buffer;
      SANE_Byte *pImage = NULL;	/* fa30 */
      SANE_Int puntero;
      SANE_Int value;

      do
	{
	  mysize4lines =
	    (mysize <=
	     dev->Reading->Size4Lines) ? mysize : dev->Reading->Size4Lines;
	  LinesCount = mysize4lines / bytesperline;

	  if (ColorMode == CM_GRAY)
	    {
	      if (scan2.depth == 12)
		{
		  /* 633b */
		  /*GRAY Bit mode 12 */
		  rst =
		    Scan_Read_BufferA (dev, (mysize4lines * 3) / 4, 0,
				       mybuffer, transferred);
		  if (rst == OK)
		    {
		      pImage = mybuffer;
		      pBuffer += LinesCount * block_bytes_per_line;
		      while (LinesCount > 0)
			{
			  Split_into_12bit_channels (mybuffer, pImage,
						     line_size);
			  pImage += (bytesperline * 3) / 4;
			  LinesCount--;
			}
		    }
		  else
		    break;
		}
	      else
		{
		  /* grayscale 8 and 16 bits */

		  SANE_Int channel_size;

		  rst =
		    Scan_Read_BufferA (dev, mysize4lines, 0, mybuffer,
				       transferred);

		  if (rst == OK)
		    {
		      channel_size = (scan2.depth > 8) ? 2 : 1;

		      pImage = mybuffer;

		      /* No gamma tables */
		      while (LinesCount > 0)
			{
			  if (line_size > 0)
			    {
			      puntero = 0;
			      do
				{
				  value =
				    data_lsb_get (pImage + puntero,
						  channel_size);

				  if (gamma != NULL)
				    value +=
				      *gamma << (8 * (channel_size - 1));

				  data_lsb_set (pBuffer, value, channel_size);

				  pBuffer += channel_size;
				  puntero += channel_size;
				}
			      while (puntero < line_size);
			    }
			  pImage += bytesperline;
			  LinesCount--;
			}
		    }
		  else
		    break;
		}
	    }
	  else
	    {
	      /*6429 */
	      /* LINEART */
	      SANE_Int desp;
	      rst =
		Scan_Read_BufferA (dev, mysize4lines, 0, mybuffer,
				   transferred);
	      if (rst == OK)
		{
		  pImage = mybuffer;
		  while (LinesCount > 0)
		    {
		      if (lineart_width > 0)
			{
			  desp = 0;
			  do
			    {
			      if ((desp % 7) == 0)
				*pBuffer = 0;

			      /* making a byte bit per bit */
			      *pBuffer = *pBuffer << 1;

			      /* bit 1 if data is under thresholdh value */
			      if (*(pImage + desp) >= binarythresholdh)	/* binarythresholdh = 0x0c */
				*pBuffer = *pBuffer | 1;

			      desp++;
			      if ((desp % 7) == 0)
				pBuffer++;

			    }
			  while (desp < lineart_width);
			}

		      if (lfa38 != 0)
			{
			  *pBuffer = (*pBuffer << lfa38);
			  pBuffer++;
			}
		      /* 64b0 */
		      pImage += bytesperline;
		      LinesCount--;
		    }
		}
	      else
		break;
	    }
	  /* 64c0 */
	  mysize -= mysize4lines;
	}
      while ((mysize > 0) && (dev->status->cancel == FALSE));

      free (mybuffer);
    }
  else
    rst = ERROR;

  DBG (DBG_FNC, "- Read_NonColor_Block(*transferred=%i): %i\n", *transferred,
       rst);

  return rst;
}

static SANE_Int
Read_Block (struct st_device *dev, SANE_Int buffer_size, SANE_Byte * buffer,
	    SANE_Int * transferred)
{
  /*
     SANE_Int buffer_size          fa80
     SANE_Byte *buffer    fa7c
   */
/*
scan2:
04F0155C  01 08 00 02 03 00 58 02  ..X
04F01564  58 02 58 02 C5 00 00 00  XXÅ...
04F0156C  B4 07 00 00 8B 01 00 00  ´..‹..
04F01574  10 06 00 00 EC 13 00 00  ..ì..
04F0157C  B2 07 00 00 B4 07 00 00  ²..´..
04F01584  CF 08 00 00              Ï..

arrangeline2 = 1
*/
  SANE_Int rst, LinesCount;
  SANE_Int mysize;
  SANE_Byte *readbuffer = NULL;
  SANE_Byte *pImage = NULL;

  DBG (DBG_FNC, "+ Read_Block(buffer_size=%i, *buffer):\n", buffer_size);

  rst = ERROR;
  *transferred = 0;

  if ((scan2.colormode != CM_COLOR) && (scan2.channel == 3)
      && (arrangeline2 != FIX_BY_SOFT))
    {
      /*6510 */
      return Read_NonColor_Block (dev, buffer, buffer_size, scan2.colormode,
				  transferred);
    }

  /*6544 */
  mysize = (buffer_size / line_size) * bytesperline;
  readbuffer = (SANE_Byte *) malloc (mysize * sizeof (SANE_Byte));
  pImage = buffer;

  if (readbuffer != NULL)
    {
      do
	{
	  buffer_size =
	    (dev->Reading->Size4Lines <
	     mysize) ? dev->Reading->Size4Lines : mysize;
	  LinesCount = buffer_size / bytesperline;

	  if (scan2.depth == 12)
	    {
	      rst =
		Scan_Read_BufferA (dev, buffer_size, 0, readbuffer,
				   transferred);
	      if (rst == OK)
		{
		  if (LinesCount > 0)
		    {
		      SANE_Byte *destino, *fuente;
		      destino = buffer;
		      fuente = readbuffer;
		      do
			{
			  Split_into_12bit_channels (destino, fuente,
						     line_size);
			  destino += line_size;
			  fuente += (bytesperline * 3) / 4;
			  LinesCount--;
			}
		      while (LinesCount > 0);
		    }
		}
	      else
		break;
	    }
	  else
	    {
	      /*65d9 */
	      rst =
		Scan_Read_BufferA (dev, buffer_size, 0, readbuffer,
				   transferred);
	      if (rst == OK)
		{
		  memcpy (pImage, readbuffer, *transferred);

		  /* apply white shading correction */
		  if ((RTS_Debug->wshading == TRUE)
		      && (scan2.scantype == ST_NORMAL))
		    WShading_Emulate (pImage, &wshading->ptr, *transferred,
				      scan2.depth);

		  pImage += *transferred;
		}
	      else
		break;
	    }
	  /*6629 */
	  mysize -= buffer_size;
	}
      while ((mysize > 0) && (dev->status->cancel == FALSE));

      free (readbuffer);
    }

  DBG (DBG_FNC, "- Read_Block(*transferred=%i): %i\n", *transferred, rst);

  return rst;
}

static SANE_Int
Scan_Read_BufferA (struct st_device *dev, SANE_Int buffer_size, SANE_Int arg2,
		   SANE_Byte * pBuffer, SANE_Int * bytes_transfered)
{
  SANE_Int rst = OK;
  SANE_Byte *ptBuffer = NULL;
  SANE_Byte *ptImg = NULL;
  struct st_readimage *rd = dev->Reading;

  DBG (DBG_FNC,
       "+ Scan_Read_BufferA(buffer_size=%i, arg2, *pBuffer, *bytes_transfered):\n",
       buffer_size);

  arg2 = arg2;			/* silence gcc */
  *bytes_transfered = 0;

  if (pBuffer != NULL)
    {
      ptBuffer = pBuffer;

      while ((buffer_size > 0) && (rst == OK)
	     && (dev->status->cancel == FALSE))
	{
	  /* Check if we've already started */
	  if (rd->Starting == TRUE)
	    {
	      /* Get channels per dot and channel's size in bytes */
	      SANE_Byte data;

	      rd->Channels_per_dot = 1;
	      if (Read_Byte (dev->usb_handle, 0xe812, &data) == OK)
		{
		  data = data >> 6;
		  if (data != 0)
		    rd->Channels_per_dot = data;
		}

	      rd->Channel_size = 1;
	      if (Read_Byte (dev->usb_handle, 0xee0b, &data) == OK)
		if (((data & 0x40) != 0) && ((data & 0x08) == 0))
		  rd->Channel_size = 2;

	      rd->RDStart = rd->DMABuffer;
	      rd->RDSize = 0;
	      rd->DMAAmount = 0;
	      rd->Starting = FALSE;
	    }

	  /* Is there any data to read from scanner? */
	  if ((rd->ImageSize > 0) && (rd->RDSize == 0))
	    {
	      /* Try to read from scanner all possible data to fill DMABuffer */
	      if (rd->RDSize < rd->DMABufferSize)
		{
		  SANE_Int iAmount, dofree;

		  /* Check if we have already notify buffer size */
		  if (rd->DMAAmount <= 0)
		    {
		      /* Initially I suppose that I can read all image */
		      iAmount = min (rd->ImageSize, rd->Max_Size);
		      rd->DMAAmount =
			((RTS_Debug->dmasetlength * 2) / iAmount) * iAmount;
		      rd->DMAAmount = min (rd->DMAAmount, rd->ImageSize);
		      Reading_BufferSize_Notify (dev, 0, rd->DMAAmount);
		      iAmount = min (iAmount, rd->DMABufferSize - rd->RDSize);
		    }
		  else
		    {
		      iAmount = min (rd->DMAAmount, rd->ImageSize);
		      iAmount = min (iAmount, rd->Max_Size);
		    }

		  /* Allocate buffer to read image if it's necessary */
		  if ((rd->RDSize == 0) && (iAmount <= buffer_size))
		    {
		      ptImg = ptBuffer;
		      dofree = FALSE;
		    }
		  else
		    {
		      ptImg =
			(SANE_Byte *) malloc (iAmount * sizeof (SANE_Byte));
		      dofree = TRUE;
		    }

		  if (ptImg != NULL)
		    {
		      /* We must wait for scanner to get data */
		      SANE_Int opStatus, sc;

		      sc = (iAmount < rd->Max_Size) ? TRUE : FALSE;
		      opStatus = Reading_Wait (dev, rd->Channels_per_dot,
					       rd->Channel_size,
					       iAmount,
					       &rd->Bytes_Available, 10, sc);

		      /* If something fails, perhaps we can read some bytes... */
		      if (opStatus != OK)
			{
			  if (rd->Bytes_Available > 0)
			    iAmount = rd->Bytes_Available;
			  else
			    rst = ERROR;
			}

		      if (rst == OK)
			{
			  /* Try to read from scanner */
			  SANE_Int transferred = 0;
			  opStatus =
			    Bulk_Operation (dev, BLK_READ, iAmount, ptImg,
					    &transferred);

			  DBG (DBG_FNC,
			       "> Scan_Read_BufferA: Bulk read %i bytes\n",
			       transferred);

			  /*if something fails may be we can read some bytes */
			  iAmount = (SANE_Int) transferred;
			  if (iAmount != 0)
			    {
			      /* Lets copy data into DMABuffer if it's necessary */
			      if (ptImg != ptBuffer)
				{
				  SANE_Byte *ptDMABuffer;

				  ptDMABuffer = rd->RDStart + rd->RDSize;
				  if ((ptDMABuffer - rd->DMABuffer) >=
				      rd->DMABufferSize)
				    ptDMABuffer -= rd->DMABufferSize;

				  if ((ptDMABuffer + iAmount) >=
				      (rd->DMABuffer + rd->DMABufferSize))
				    {
				      SANE_Int rest =
					iAmount - (rd->DMABufferSize -
						   (ptDMABuffer -
						    rd->DMABuffer));
				      memcpy (ptDMABuffer, ptImg,
					      iAmount - rest);
				      memcpy (rd->DMABuffer,
					      ptImg + (iAmount - rest), rest);
				    }
				  else
				    memcpy (ptDMABuffer, ptImg, iAmount);
				  rd->RDSize += iAmount;
				}
			      else
				{
				  *bytes_transfered += iAmount;
				  buffer_size -= iAmount;
				}

			      rd->DMAAmount -= iAmount;
			      rd->ImageSize -= iAmount;
			    }
			  else
			    rst = ERROR;
			}

		      /* Lets free buffer */
		      if (dofree == TRUE)
			{
			  free (ptImg);
			  ptImg = NULL;
			}
		    }
		  else
		    rst = ERROR;
		}
	    }

	  /* is there any data read from scanner? */
	  if (rd->RDSize > 0)
	    {
	      /* Add to the given buffer so many bytes as posible */
	      SANE_Int iAmount;

	      iAmount = min (buffer_size, rd->RDSize);
	      if ((rd->RDStart + iAmount) >=
		  (rd->DMABuffer + rd->DMABufferSize))
		{
		  SANE_Int rest =
		    rd->DMABufferSize - (rd->RDStart - rd->DMABuffer);
		  memcpy (ptBuffer, rd->RDStart, rest);
		  memcpy (ptBuffer + rest, rd->DMABuffer, iAmount - rest);
		  rd->RDStart = rd->DMABuffer + (iAmount - rest);
		}
	      else
		{
		  memcpy (ptBuffer, rd->RDStart, iAmount);
		  rd->RDStart += iAmount;
		}

	      ptBuffer += iAmount;
	      rd->RDSize -= iAmount;
	      buffer_size -= iAmount;
	      *bytes_transfered += iAmount;

	      /* if there isn't any data in DMABuffer we can point RDStart
	         to the begining of DMABuffer */
	      if (rd->RDSize == 0)
		rd->RDStart = rd->DMABuffer;
	    }

	  /* in case of all data is read we return OK with bytes_transfered = 0 */
	  if ((*bytes_transfered == 0)
	      || ((rd->RDSize == 0) && (rd->ImageSize == 0)))
	    break;
	}

      if (rst == ERROR)
	RTS_DMA_Cancel (dev);
    }

  DBG (DBG_FNC, "->   *bytes_transfered=%i\n", *bytes_transfered);
  DBG (DBG_FNC, "->   Reading->ImageSize=%i\n", rd->ImageSize);
  DBG (DBG_FNC, "->   Reading->DMAAmount=%i\n", rd->DMAAmount);
  DBG (DBG_FNC, "->   Reading->RDSize   =%i\n", rd->RDSize);

  DBG (DBG_FNC, "- Scan_Read_BufferA: %i\n", rst);

  return rst;
}

static SANE_Int
Reading_BufferSize_Get (struct st_device *dev, SANE_Byte channels_per_dot,
			SANE_Int channel_size)
{
  /* returns the ammount of bytes in scanner's buffer ready to be read */

  SANE_Int rst;

  DBG (DBG_FNC,
       "+ Reading_BufferSize_Get(channels_per_dot=%i, channel_size=%i):\n",
       channels_per_dot, channel_size);

  rst = 0;

  if (channel_size > 0)
    {
      SANE_Int myAmount;

      if (channels_per_dot < 1)
	{
	  /* read channels per dot from registers */
	  if (Read_Byte (dev->usb_handle, 0xe812, &channels_per_dot) == OK)
	    channels_per_dot = _B0 (channels_per_dot >> 6);

	  if (channels_per_dot == 0)
	    channels_per_dot++;
	}

      if (Read_Integer (dev->usb_handle, 0xef16, &myAmount) == OK)
	rst = ((channels_per_dot * 32) / channel_size) * myAmount;
    }

  DBG (DBG_FNC, "- Reading_BufferSize_Get: %i bytes\n", rst);

  return rst;
}

static SANE_Int
Lamp_Warmup (struct st_device *dev, SANE_Byte * Regs, SANE_Int lamp,
	     SANE_Int resolution)
{
  SANE_Int rst = OK;

  DBG (DBG_FNC, "+ Lamp_Warmup(*Regs, lamp=%i, resolution=%i)\n", lamp,
       resolution);

  if (Regs != NULL)
    {
      SANE_Byte flb_lamp, tma_lamp;
      SANE_Int overdrivetime;

      Lamp_Status_Get (dev, &flb_lamp, &tma_lamp);

      /* ensure that selected lamp is switched on */
      if (lamp == FLB_LAMP)
	{
	  overdrivetime = RTS_Debug->overdrive_flb;

	  if (flb_lamp == 0)
	    {
	      /* FLB-Lamp is turned off, lets turn on */
	      Lamp_Status_Set (dev, Regs, TRUE, FLB_LAMP);
	      waitforpwm = TRUE;
	    }
	}
      else
	{
	  /* is tma device attached to scanner ? */
	  if (RTS_isTmaAttached (dev) == TRUE)
	    {
	      overdrivetime = RTS_Debug->overdrive_ta;

	      if (tma_lamp == 0)
		{
		  /* tma lamp is turned off */
		  Lamp_Status_Set (dev, Regs, FALSE, TMA_LAMP);
		  waitforpwm = TRUE;
		}
	    }
	  else
	    rst = ERROR;
	}

      /* perform warmup process */
      if (rst == OK)
	{
	  Lamp_PWM_Setup (dev, lamp);

	  if (waitforpwm == TRUE)
	    {
	      /*Lamp_PWM_DutyCycle_Set(dev, (lamp == TMA_LAMP)? 0x0e : 0x00); */

	      if (RTS_Debug->warmup == TRUE)
		{
		  long ticks = GetTickCount () + overdrivetime;

		  DBG (DBG_VRB, "- Lamp Warmup process. Please wait...\n");

		  dev->status->warmup = TRUE;

		  while (GetTickCount () <= ticks)
		    usleep (1000 * 200);

		  Lamp_PWM_CheckStable (dev, resolution, lamp);

		}
	      else
		DBG (DBG_VRB, "- Lamp Warmup process disabled.\n");
	    }

	  /*Lamp_PWM_Setup(dev, lamp);

	     if (waitforpwm == TRUE)
	     {
	     if (RTS_Debug->warmup == TRUE)
	     Lamp_PWM_CheckStable(dev, resolution, lamp);

	     waitforpwm = FALSE;
	     } */
	}

    }
  else
    rst = ERROR;

  dev->status->warmup = FALSE;

  DBG (DBG_FNC, "- Lamp_Warmup: %i\n", rst);

  return rst;
}

static SANE_Int
Scan_Start (struct st_device *dev)
{
  SANE_Int rst;

  DBG (DBG_FNC, "+ Scan_Start:\n");

  rst = ERROR;
  if (RTS_Enable_CCD (dev, dev->init_regs, 0x0f) == OK)
    {
      SANE_Byte Regs[RT_BUFFER_LEN], mlock;
      SANE_Int ypos, xpos, runb1;
      struct st_scanparams scancfg;
      struct st_hwdconfig hwdcfg;
      struct st_calibration myCalib;
      long tick;

      memcpy (&Regs, dev->init_regs, RT_BUFFER_LEN * sizeof (SANE_Byte));
      memcpy (&scancfg, &scan, sizeof (struct st_scanparams));

      dbg_ScanParams (&scancfg);

      /* reserva buffer 6 dwords en fa84-fa9f */
      bzero (&hwdcfg, sizeof (struct st_hwdconfig));

      /* wait till lamp is at home (should use timeout
         windows driver doesn't use it)
       */
      tick = GetTickCount () + 10000;
      while ((Head_IsAtHome (dev, Regs) == FALSE)
	     && (tick > GetTickCount ()));

      if (v14b4 != 0)
	{
	  SANE_Int lfaa0 = 0;

	  if (GainOffset_Counter_Inc (dev, &lfaa0) != OK)
	    return 0x02;
	}

      tick = GetTickCount ();

      /* set margin references */
      Refs_Set (dev, Regs, &scancfg);

      /* locate head to right position */
      Load_StripCoords (scantype, &ypos, &xpos);
      if (ypos != 0)
	Head_Relocate (dev, dev->motorcfg->parkhomemotormove, MTR_FORWARD,
		       ypos);

      /* perform lamp warmup */
      if (Lamp_Warmup
	  (dev, Regs, (scancfg.scantype == ST_NORMAL) ? FLB_LAMP : TMA_LAMP,
	   scan.resolution_x) == ERROR)
	return ERROR;

      /* Calibration process */

      /*592c */
      if (Calib_CreateBuffers (dev, &myCalib, v14b4) != OK)
	return ERROR;

      /*5947 */

/*
if (Calib_BlackShading_jkd(dev, Regs, &myCalib, &scancfg) == OK)
	Head_ParkHome(dev, TRUE, dev->motorcfg->parkhomemotormove);
*/

/*
if (Calib_test(dev, Regs, &myCalib, &scancfg) == OK )
	Head_ParkHome(dev, TRUE, dev->motorcfg->parkhomemotormove);
*/

/* Calibrate White shading correction */
      if ((RTS_Debug->wshading == TRUE) && (scan.scantype == ST_NORMAL))
	if (WShading_Calibrate (dev, Regs, &myCalib, &scancfg) == OK)
	  Head_ParkHome (dev, TRUE, dev->motorcfg->parkhomemotormove);

      hwdcfg.calibrate = RTS_Debug->calibrate;

      if (RTS_Debug->calibrate != 0)
	{
	  /* Let's calibrate */
	  if ((scancfg.colormode != CM_COLOR) && (scancfg.channel == 3))
	    scancfg.colormode = CM_COLOR;

	  hwdcfg.arrangeline = 0;

	  if (scan.scantype == ST_NORMAL)
	    {
	      /* Calibration for reflective type */

	      /*59e3 */
	      memcpy (&Regs, dev->init_regs,
		      RT_BUFFER_LEN * sizeof (SANE_Byte));

	      if (Calibration (dev, Regs, &scancfg, &myCalib, 0) != OK)
		{
		  if (v14b4 == 0)
		    Calib_FreeBuffers (&myCalib);
		  return ERROR;
		}
	    }
	  else
	    {
	      /*59ed */
	      /* Calibration for negative/slide type */

	    }

	  /*5af1 */
	  if (RTS_Debug->ScanWhiteBoard != FALSE)
	    {
	      Head_ParkHome (dev, TRUE, dev->motorcfg->basespeedmotormove);
	      scan.ler = 1;
	    }

	  scancfg.colormode = scan.colormode;
	}
      else
	{
	  /*5b1e */
	  /*Don't calibrate */
	  if (scan.scantype == ST_NORMAL)
	    {
	      Lamp_Status_Set (dev, NULL, TRUE, FLB_LAMP);
	    }
	  else
	    {
	      if ((scan.scantype == ST_TA) || (scan.scantype == ST_NEG))
		{
		  /*SANE_Int ta_y_start; */
		  Lamp_Status_Set (dev, NULL, FALSE, TMA_LAMP);
		  /*ta_y_start =
		     get_value(SCAN_PARAM, TA_Y_START, 0x2508, usbfile);
		     ta_y_start += (((((scan.coord.top * 3) * 5) * 5) * 32) / scancfg.resolution_x);
		     if (ta_y_start >= 500)
		     {
		     Head_Relocate(dev, dev->motorcfg->highspeedmotormove, MTR_FORWARD, ta_y_start);
		     scancfg.coord.top = 1;
		     scan.ler = 1;
		     } else
		     {
		     / *5ba9* /
		     if (ta_y_start > 0)
		     {
		     Head_Relocate(dev, dev->motorcfg->basespeedmotormove, MTR_FORWARD, ta_y_start);
		     scancfg.coord.top = 1;
		     scan.ler = 1;
		     }
		     } */
		}
	    }
	}

      /*5bd0 */
      usleep (1000 * 200);

      hwdcfg.scantype = scan.scantype;
      hwdcfg.motor_direction = MTR_FORWARD;

      /* Set Origin */
      if ((scan.scantype >= ST_NORMAL) || (scan.scantype <= ST_NEG))
	{
	  scancfg.coord.left += scan.ser;
	  scancfg.coord.top += scan.ler;
	}

      hwdcfg.sensorevenodddistance = dev->sensorcfg->evenodd_distance;
      hwdcfg.highresolution = (scancfg.resolution_x <= 1200) ? FALSE : TRUE;

      /*5c55 */
      /*
         if (RTS_Debug->calibrate == FALSE)
         {
         SANE_Int mytop = (((scancfg.coord.top * 5) * 5) * 16) / scancfg.resolution_y;
         if ((scancfg.resolution_y <= 150)&&(mytop < 300))
         {
         scancfg.coord.top = scancfg.resolution_y / 4;
         } else
         {
         if (mytop < 100)
         scancfg.coord.top = scancfg.resolution_y / 12;
         }
         }
       */

      /*5cd9 */
      if (compression != FALSE)
	hwdcfg.compression = TRUE;

      /* setting arrangeline option */
      hwdcfg.arrangeline = arrangeline;
      if (scancfg.resolution_x == 2400)
	{
	  /* 5cfa */
	  if (scancfg.colormode != CM_COLOR)
	    {
	      if ((scancfg.colormode == CM_GRAY) && (scancfg.channel == 3))
		hwdcfg.arrangeline = FIX_BY_SOFT;
	    }
	  else
	    hwdcfg.arrangeline = FIX_BY_SOFT;
	}

      /*5d12 */
      if (dev->sensorcfg->type == CCD_SENSOR)
	{
	  /*5d3a */
	  scancfg.coord.left += 24;
	  switch (scancfg.resolution_x)
	    {
	    case 1200:
	      scancfg.coord.left -= 63;
	      break;
	    case 2400:
	      scancfg.coord.left -= 127;
	      break;
	    }
	}
      else
	{
	  /*5d5a */
	  /* CIS sensor */
	  /*5d6d */
	  scancfg.coord.left += 50;
	  switch (scancfg.resolution_x)
	    {
	    case 1200:
	      scancfg.coord.left -= 63;
	      break;
	    case 2400:
	      scancfg.coord.left -= 127;
	      break;
	    }
	}

      /* 5d92 */
      DBG (DBG_FNC, " ->Scan_Start xStart=%i, xExtent=%i\n",
	   scancfg.coord.left, scancfg.coord.width);

      runb1 = 1;
      if (scan.scantype == ST_NORMAL)
	{
	  /*5db7 */
	  if ((scancfg.resolution_x == 1200)
	      || (scancfg.resolution_x == 2400))
	    {
	      /*5e41 */
	      if ((scancfg.resolution_y / 10) > scancfg.coord.top)
		runb1 = 0;
	    }
	  else
	    {
	      if ((scancfg.resolution_x == 600)
		  && (RTS_Debug->usbtype == USB11)
		  && (scancfg.colormode == CM_COLOR))
		{
		  /*5ded */
		  if ((scancfg.resolution_y / 10) > scancfg.coord.top)
		    runb1 = 0;
		}
	      else
		{
		  if ((scancfg.resolution_x == 600)
		      || (scancfg.resolution_x == 300))
		    {
		      /*5e11 */
		      if (scancfg.resolution_y > scancfg.coord.top)
			runb1 = 0;
		    }
		  else
		    runb1 = 0;
		}
	    }
	}
      else
	{
	  /*5e7c *//* entra aquí */
	  if ((scancfg.resolution_y / 10) > scancfg.coord.top)
	    runb1 = 0;
	}
      /*5eb1 */
      if (runb1 == 1)		/*entra */
	{
	  SANE_Int val1 = scancfg.coord.top - (scancfg.resolution_y / 10);
	  scancfg.coord.top -= val1;
	  Head_Relocate (dev, dev->motorcfg->highspeedmotormove, MTR_FORWARD, (dev->motorcfg->resolution / scancfg.resolution_y) * val1);	/*x168 */
	}

      /*5efe */
      if (RTS_Debug->calibrate != FALSE)
	{
	  if (use_gamma_tables != FALSE)
	    {
	      hwdcfg.use_gamma_tables = TRUE;
	      hp_gamma->depth = 0;
	    }

	  /*5f24 */
	  hwdcfg.white_shading = TRUE;
	  hwdcfg.black_shading = TRUE;
	  hwdcfg.unk3 = 0;
	  RTS_Setup (dev, Regs, &scancfg, &hwdcfg, &calibdata->gain_offset);

	  myCalib.shading_type = 0;
	  myCalib.shadinglength =
	    min (myCalib.shadinglength, scan.shadinglength);

	  if (scancfg.colormode != CM_COLOR)
	    {
	      if ((scancfg.channel > 0) && (scancfg.channel < 3))
		myCalib.WRef[0] = myCalib.WRef[scancfg.channel];
	    }

	  RTS_WriteRegs (dev->usb_handle, Regs);

	  /* apply gamma if required */
	  Gamma_Apply (dev, Regs, &scancfg, &hwdcfg, hp_gamma);

	  Shading_apply (dev, Regs, &scancfg, &myCalib);

	  /* Save to file? */
	  if (RTS_Debug->DumpShadingData != FALSE)
	    dump_shading (&myCalib);	/*5ff9 */
	}
      else
	RTS_Setup (dev, Regs, &scancfg, &hwdcfg, default_gain_offset);

      /*602a */
      RTS_Debug->calibrate = hwdcfg.calibrate;
      binarythresholdh = bw_threshold;
      binarythresholdl = bw_threshold;
      DBG (DBG_FNC, ">  bw threshold -- hi=%i, lo=%i\n", binarythresholdh,
	   binarythresholdl);

      /* set threshold high */
      data_lsb_set (&Regs[0x1a0], binarythresholdh, 2);

      /* set threshold low */
      data_lsb_set (&Regs[0x19e], binarythresholdl, 2);

      /* if has motorcurves... */
      if ((Regs[0xdf] & 0x10) != 0)
	data_bitset (&Regs[0x01], 0x02, 1);

      /* Set MLOCK */
      mlock = get_value (SCAN_PARAM, MLOCK, 0, usbfile) & 1;
      data_bitset (&Regs[0x00], 0x10, mlock);	       /*---x----*/

      if (dev->motorcfg->changemotorcurrent != FALSE)
	Motor_Change (dev, Regs,
		      Motor_GetFromResolution (scancfg.resolution_x));

      /* set gain control mode */
      Lamp_SetGainMode (dev, Regs, scancfg.resolution_x,
			Lamp_GetGainMode (dev, scancfg.resolution_x,
					  scan.scantype));

      RTS_WaitScanEnd (dev, 15000);
      if (v14b4 == 0)
	Calib_FreeBuffers (&myCalib);

      /* release motor */
      Motor_Release (dev);


#ifdef developing
/*	prueba(Regs);
	dbg_registers(Regs);*/
      /*WShading_Calibrate(dev, Regs, &myCalib, &scancfg); */
      /*shadingtest1(dev, Regs, &myCalib); */
#endif

      if (RTS_Warm_Reset (dev) == OK)
	{
	  RTS_WriteRegs (dev->usb_handle, Regs);
	  usleep (1000 * 500);

	  if (RTS_Execute (dev) == OK)
	    {
	      Lamp_Status_Timer_Set (dev, 0);

	      /* Let scanner some time to store some data */
	      if ((dev->chipset->model == RTS8822L_02A)
		  && (scancfg.resolution_x > 2400))
		usleep (1000 * 5000);

	      rst = OK;
	    }
	}
    }

  DBG (DBG_FNC, "- Scan_Start: %i\n", rst);

  return rst;
}

static SANE_Int
RTS_Setup_Motor (struct st_device *dev, SANE_Byte * Regs,
		 struct st_scanparams *scancfg, SANE_Int somevalue)
{
  SANE_Int rst = ERROR;		/* default */

  DBG (DBG_FNC, "+ RTS_Setup_Motor(*Regs, *scancfg, somevalue=%i):\n",
       somevalue);
  dbg_ScanParams (scancfg);

  if ((Regs != NULL) && (scancfg != NULL))
    {
      SANE_Int colormode, mymode;

      colormode = ((scancfg->colormode != CM_COLOR)
		   && (scancfg->channel == 3)) ? 3 : scancfg->colormode;
      mymode =
	RTS_GetScanmode (dev, scantype, colormode, scancfg->resolution_x);

      if (mymode != -1)
	{
	  SANE_Int mbs[2] = { 0 };	/* motor back steps */
	  SANE_Int step_size, step_type, dummyline, myvalue, lf02c;
	  struct st_scanmode *sm;

	  sm = dev->scanmodes[mymode];

	  /* set motor step type */
	  data_bitset (&Regs[0xd9], 0x70, sm->scanmotorsteptype);	       /*-xxx----*/

	  /* set motor direction (polarity) */
	  data_bitset (&Regs[0xd9], 0x80, somevalue >> 3);	/*e------- */

	  /* next value doesn't seem to have any effect */
	  data_bitset (&Regs[0xd9], 0x0f, somevalue);			       /*----efgh*/

	  /* 0 enable/1 disable motor */
	  data_bitset (&Regs[0xdd], 0x80, somevalue >> 4);	/*d------- */

	  /* next value doesn't seem to have any effect */
	  data_bitset (&Regs[0xdd], 0x40, somevalue >> 4);		       /*-d------*/

	  switch (sm->scanmotorsteptype)
	    {
	    case STT_OCT:
	      step_type = 8;
	      break;
	    case STT_QUART:
	      step_type = 4;
	      break;
	    case STT_HALF:
	      step_type = 2;
	      break;
	    default:
	      step_type = 1;
	      break;		/* STT_FULL */
	    }

	  /* set dummy lines */
	  dummyline = sm->dummyline;
	  if (dummyline == 0)
	    dummyline++;

	  data_bitset (&Regs[0xd6], 0xf0, dummyline);	/*xxxx---- */

	  /* Set if motor has curves */
	  data_bitset (&Regs[0xdf], 0x10, ((sm->motorcurve != -1) ? 1 : 0));		 /*---x----*/

	  /* set last step of deccurve.scanbufferfull table to 16 */
	  data_lsb_set (&Regs[0xea], 0x10, 3);

	  /* set last step of deccurve.normalscan table to 16 */
	  data_lsb_set (&Regs[0xed], 0x10, 3);

	  /* set last step of deccurve.smearing table to 16 */
	  data_lsb_set (&Regs[0xf0], 0x10, 3);

	  /* set last step of deccurve.parkhome table to 16 */
	  data_lsb_set (&Regs[0xf3], 0x10, 3);

	  /* set step size */
	  step_size =
	    _B0 ((dev->motorcfg->resolution * step_type) /
		 (dummyline * scancfg->resolution_y));
	  data_lsb_set (&Regs[0xe0], step_size - 1, 1);

	  /* set line exposure time */
	  myvalue = data_lsb_get (&Regs[0x30], 3);
	  myvalue += ((myvalue + 1) % step_size);
	  data_lsb_set (&Regs[0x30], myvalue, 3);

	  /* set last step of accurve.normalscan table */
	  myvalue = ((myvalue + 1) / step_size) - 1;
	  data_lsb_set (&Regs[0xe1], myvalue, 3);

	  /* 42b30eb */
	  lf02c = 0;
	  if (sm->motorcurve != -1)
	    {
	      if (sm->motorcurve < dev->mtrsetting_count)
		{
		  struct st_motorcurve *ms = dev->mtrsetting[sm->motorcurve];
		  ms->motorbackstep = sm->motorbackstep;
		}

	      DBG (DBG_FNC, " -> Setting up step motor using motorcurve %i\n",
		   sm->motorcurve);
	      lf02c = Motor_Setup_Steps (dev, Regs, sm->motorcurve);

	      /* set motor back steps */
	      mbs[1] = sm->motorbackstep;
	      if (mbs[1] >= (smeardeccurvecount + smearacccurvecount))
		mbs[0] =
		  mbs[1] - (smeardeccurvecount + smearacccurvecount) + 2;
	      else
		mbs[0] = 0;

	      if (mbs[1] >= (deccurvecount + acccurvecount))
		mbs[1] -= (deccurvecount + acccurvecount) + 2;
	      else
		mbs[1] = 0;
	    }
	  else
	    {
	      /* this scanner hasn't got any motorcurve */

	      /* set last step of accurve.smearing table (same as accurve.normalscan) */
	      data_lsb_set (&Regs[0xe4], myvalue, 3);

	      /* set last step of accurve.parkhome table (same as accurve.normalscan) */
	      data_lsb_set (&Regs[0xe7], myvalue, 3);

	      /* both motorbacksteps are equal */
	      mbs[0] = sm->motorbackstep;
	      mbs[1] = sm->motorbackstep;
	    }

	  /* show msi and motorbacksteps */
	  DBG (DBG_FNC, " -> msi            = %i\n", sm->msi);
	  DBG (DBG_FNC, " -> motorbackstep1 = %i\n", mbs[0]);
	  DBG (DBG_FNC, " -> motorbackstep2 = %i\n", mbs[1]);

	  /* set msi */
	  data_bitset (&Regs[0xda], 0xff, _B0 (sm->msi));	/*xxxxxxxx */
	  data_bitset (&Regs[0xdd], 0x03, _B1 (sm->msi));	      /*------xx*/

	  /* set motorbackstep (a) */
	  data_bitset (&Regs[0xdb], 0xff, _B0 (mbs[0]));	/*xxxxxxxx */
	  data_bitset (&Regs[0xdd], 0x0c, _B1 (mbs[0]));	      /*----xx--*/

	  /* set motorbackstep (b) */
	  data_bitset (&Regs[0xdc], 0xff, _B0 (mbs[1]));	/*xxxxxxxx */
	  data_bitset (&Regs[0xdd], 0x30, _B1 (mbs[1]));	      /*--xx----*/

	  /* 328b */

	  /* get dummy lines count */
	  dummyline = data_bitget (&Regs[0xd6], 0xf0);

	  myvalue = scancfg->coord.top * (dummyline * step_size);

	  if (lf02c >= myvalue)
	    scancfg->coord.top = 1;
	  else
	    scancfg->coord.top -= (lf02c / (dummyline * step_size)) - 1;

	  rst = lf02c;		/* Result from Motor_Setup_Steps */
	}
    }

  DBG (DBG_FNC, "- RTS_Setup_Motor: %i\n", rst);

  return rst;
}

static void
RTS_Setup_Exposure_Times (SANE_Byte * Regs, struct st_scanparams *scancfg,
			  struct st_scanmode *sm)
{
  DBG (DBG_FNC, "> RTS_Setup_Exposure_Times\n");

  if ((sm != NULL) && (Regs != NULL) && (scancfg != NULL))
    {
      SANE_Int myexpt[3], linexpt, a;

      /* calculate line exposure time */
      linexpt = sm->ctpc + 1;
      if (RTS_Debug->usbtype == USB11)
	linexpt *= sm->multiexposureforfullspeed;

      if (scancfg->depth > 8)
	linexpt *= sm->multiexposurefor16bitmode;

      linexpt--;

      /* generate exposure times for each channel color */
      for (a = CL_RED; a <= CL_BLUE; a++)
	{
	  if ((linexpt > sm->mexpt[a]) && (sm->expt[a] == 0))
	    sm->expt[a] = sm->mexpt[a];

	  myexpt[a] = (sm->expt[a] == 0) ? sm->mexpt[a] : sm->expt[a];
	}

      /* save exposure times */
      DBG (DBG_FNC, "-> Exposure times : %04x, %04x, %04x\n", sm->expt[0],
	   sm->expt[1], sm->expt[2]);
      data_lsb_set (&Regs[0x36], sm->expt[CL_RED], 3);
      data_lsb_set (&Regs[0x3c], sm->expt[CL_GREEN], 3);
      data_lsb_set (&Regs[0x42], sm->expt[CL_BLUE], 3);

      /* save maximum exposure times */
      DBG (DBG_FNC, "-> Maximum exposure times: %04x, %04x, %04x\n",
	   sm->mexpt[0], sm->mexpt[1], sm->mexpt[2]);
      data_lsb_set (&Regs[0x33], sm->mexpt[CL_RED], 3);
      data_lsb_set (&Regs[0x39], sm->mexpt[CL_GREEN], 3);
      data_lsb_set (&Regs[0x3f], sm->mexpt[CL_BLUE], 3);

      /* save line exposure time */
      data_lsb_set (&Regs[0x30], linexpt, 3);

      /* scancfg->expt = lowest value */
      scancfg->expt = min (min (myexpt[1], myexpt[2]), myexpt[0]);
    }
}

static SANE_Int
RTS_Setup_Line_Distances (struct st_device *dev, SANE_Byte * Regs,
			  struct st_scanparams *scancfg,
			  struct st_hwdconfig *hwdcfg, SANE_Int mycolormode,
			  SANE_Int arrangeline)
{
  SANE_Int iLineDistance = 0;

  if (arrangeline == FIX_BY_HARD)
    {
      /* we don't need to arrange retrieved line */
      SANE_Int mylinedistance, myevenodddist;

      mylinedistance =
	(dev->sensorcfg->line_distance * scancfg->resolution_y) /
	dev->sensorcfg->resolution;

      if (hwdcfg->highresolution == TRUE)
	myevenodddist =
	  (hwdcfg->sensorevenodddistance * scancfg->resolution_y) /
	  dev->sensorcfg->resolution;
      else
	myevenodddist = 0;

      data_bitset (&Regs[0x149], 0x3f, myevenodddist);
      data_bitset (&Regs[0x14a], 0x3f, mylinedistance);
      data_bitset (&Regs[0x14b], 0x3f, mylinedistance + myevenodddist);
      data_bitset (&Regs[0x14c], 0x3f, mylinedistance * 2);
      data_bitset (&Regs[0x14d], 0x3f, (mylinedistance * 2) + myevenodddist);
    }
  else
    {
      /* arrange retrieved line */
      data_bitset (&Regs[0x149], 0x3f, 0);
      data_bitset (&Regs[0x14a], 0x3f, 0);
      data_bitset (&Regs[0x14b], 0x3f, 0);
      data_bitset (&Regs[0x14c], 0x3f, 0);
      data_bitset (&Regs[0x14d], 0x3f, 0);

      if (arrangeline == FIX_BY_SOFT)
	{
	  if (hwdcfg->highresolution == FALSE)
	    {
	      if (mycolormode == CM_COLOR)
		{
		  iLineDistance =
		    (dev->sensorcfg->line_distance * scan2.resolution_y) * 2;
		  iLineDistance =
		    (iLineDistance / dev->sensorcfg->resolution) + 1;
		  if (iLineDistance < 2)
		    iLineDistance = 2;
		}
	    }
	  else
	    {
	      /* bcc */
	      if (mycolormode == CM_COLOR)
		iLineDistance =
		  ((dev->sensorcfg->line_distance * 2) +
		   hwdcfg->sensorevenodddistance) * scan2.resolution_y;
	      else
		iLineDistance =
		  dev->sensorcfg->line_distance * scan2.resolution_y;

	      iLineDistance =
		(iLineDistance / dev->sensorcfg->resolution) + 1;
	      if (iLineDistance < 2)
		iLineDistance = 2;
	    }

	  /* c25 */
	  iLineDistance &= 0xffff;
	  v15b4 = (iLineDistance > 0) ? 1 : 0;
	  imagesize += iLineDistance * bytesperline;
	}
    }

  DBG (DBG_FNC,
       "> RTS_Setup_Line_Distances(*Regs, *scancfg, *hwdcfg, mycolormode=%i, arrangeline=%i): %i\n",
       mycolormode, arrangeline, iLineDistance);

  return iLineDistance;
}

static SANE_Int
RTS_Setup_Depth (SANE_Byte * Regs, struct st_scanparams *scancfg,
		 SANE_Int mycolormode)
{
  /* channels_per_line = channels_per_dot * scan.width
     bytes_per_line = channels_per_line * bits_per_channel
   */

  SANE_Int bytes_per_line = 0;

  if ((scancfg != NULL) && (Regs != NULL))
    {
      SANE_Int channels_per_line =
	data_bitget (&Regs[0x12], 0xc0) * scancfg->coord.width;

      bytes_per_line = channels_per_line;

      /* set bits per channel in shading correction's register (0x1cf) */
      if (mycolormode == CM_LINEART)
	{
	  /* lineart mode */
	  bytes_per_line = (bytes_per_line + 7) / 8;
	  data_bitset (&Regs[0x1cf], 0x30, 3);		    /*--11----*/
	}
      else
	{
	  /*f0c */
	  switch (scancfg->depth)
	    {
	    case 16:
	      /* 16 bits per channel */
	      bytes_per_line *= 2;
	      data_bitset (&Regs[0x1cf], 0x30, 2);			    /*--10----*/
	      break;
	    case 12:
	      /* 12 bits per channel */
	      bytes_per_line *= 2;
	      data_bitset (&Regs[0x1cf], 0x30, 1);			    /*--01----*/
	      break;
	    default:
	      /* 8 bits per channel */
	      data_bitset (&Regs[0x1cf], 0x30, 0);			    /*--00----*/
	      break;
	    }
	}
    }

  return bytes_per_line;
}

static void
RTS_Setup_Shading (SANE_Byte * Regs, struct st_scanparams *scancfg,
		   struct st_hwdconfig *hwdcfg, SANE_Int bytes_per_line)
{
  DBG (DBG_FNC,
       "> RTS_Setup_Shading(*Regs, *scancfg, *hwdcfg, bytes_per_line=%i)\n",
       bytes_per_line);

  if ((Regs != NULL) && (hwdcfg != NULL))
    {
      SANE_Int dots_count, myvalue, myvalue2, mem_available, resolution_ratio,
	sensor_line_distance;
      SANE_Int channels, table_size;

      resolution_ratio = Regs[0x0c0] & 0x1f;

      /* 50de */
      data_bitset (&Regs[0x1bf], 0x18, hwdcfg->unk3);	       /*---xx---*/

      /* Enable black shading correction ? */
      data_bitset (&Regs[0x1cf], 0x08, hwdcfg->black_shading);		/*----x---*/

      /* Enable white shading correction ? */
      data_bitset (&Regs[0x1cf], 0x04, hwdcfg->white_shading);		/*-----x--*/

      if ((hwdcfg->white_shading != FALSE) && (hwdcfg->black_shading != FALSE)
	  && (hwdcfg->unk3 != 0))
	data_bitset (&Regs[0x1cf], 0x04, 0);		    /*-----x--*/

      table_size = 0;

      /* if hwdcfg->black_shading */
      if ((Regs[0x1cf] & 8) != 0)
	table_size = (resolution_ratio * scancfg->coord.width) * 2;	/* black shading buffer size? */

      /* if hwdcfg->white_shading */
      if ((Regs[0x1cf] & 4) != 0)
	table_size += (resolution_ratio * scancfg->coord.width) * 2;	/* white shading buffer size? */

      /* Regs 0x1ba, 0x1bb, 0x1bd, 0x1c0 seem to be 4 pointers
         to some buffer related to shading correction */

      Regs[0x1ba] = 0x00;
      table_size = (table_size + v160c_block_size - 1) / v160c_block_size;
      table_size = ((table_size + 15) / 16) + 16;

      Regs[0x1bf] &= 0xfe;
      Regs[0x1bb] = _B0 (table_size);
      Regs[0x1bc] = _B1 (table_size);
      Regs[0x1bf] |= _B2 (table_size) & 1;	    /*-------x*/

      Regs[0x1bf] &= 0xf9;
      Regs[0x1bd] = _B0 (table_size * 2);
      Regs[0x1be] = _B1 (table_size * 2);
      Regs[0x1bf] |= (_B2 (table_size * 2) & 3) << 1;	       /*-----xx-*/

      data_wide_bitset (&Regs[0x1c0], 0xfffff, table_size * 3);

      mem_available = mem_total - ((table_size * 3) * 16);
      sensor_line_distance = Regs[0x14a] & 0x3f;

      /* select case channels_per_dot */
      channels = data_lsb_get (&Regs[0x12], 1) >> 6;

      switch (channels)
	{
	case 3:		/* 3 channels per dot */
	  /* 528d */
	  dots_count = bytes_per_line / 3;	/* 882 */
	  myvalue =
	    (((sensor_line_distance + 1) * dots_count) + v160c_block_size -
	     1) / v160c_block_size;
	  myvalue2 = myvalue;
	  mem_available = (mem_available - (myvalue * 3) + 2) / 3;

	  myvalue += (table_size * 3) * 8;
	  myvalue = ((myvalue * 2) + mem_available);

	  data_bitset (&Regs[0x1c2], 0xf0, _B2 ((myvalue / 16) + 1));	/* 4 higher bits   xxxx---- */
	  data_wide_bitset (&Regs[0x1c3], 0xffff, (myvalue / 16) + 1);	/* 16 lower bits */

	  myvalue = myvalue + myvalue2 + mem_available;
	  data_wide_bitset (&Regs[0x1c5], 0xfffff, (myvalue / 16) + 1);
	  break;
	case 2:		/* 2 channels per dot */
	  dots_count = bytes_per_line / 2;
	  myvalue =
	    (((sensor_line_distance + 1) * dots_count) + v160c_block_size -
	     1) / v160c_block_size;
	  mem_available = ((mem_available - myvalue) + 1) / 2;
	  myvalue += (((table_size * 3) + mem_available) / 16) + 1;

	  data_bitset (&Regs[0x1c2], 0xf0, _B2 (myvalue));	/* 4 higher bits   xxxx---- */
	  data_wide_bitset (&Regs[0x1c3], 0xffff, myvalue);	/* 16 lower bits */
	  break;
	default:
	  dots_count = bytes_per_line;
	  break;
	}

      Regs[0x01c7] &= 0x0f;
      Regs[0x01c8] = _B0 ((mem_total - 1) / 16);
      Regs[0x01c9] = _B1 ((mem_total - 1) / 16);
      Regs[0x01c7] |= (_B2 ((mem_total - 1) / 16) & 0x0f) << 4;

      mem_available -= (dots_count + v160c_block_size - 1) / v160c_block_size;
      mem_available /= 16;
      Regs[0x0712] &= 0x0f;
      Regs[0x0710] = _B0 (mem_available);
      Regs[0x0711] = _B1 (mem_available);
      Regs[0x0712] |= _B0 (_B2 (mem_available) << 4);	/*xxxx---- */

      Regs[0x0713] = 0x00;
      Regs[0x0714] = 0x10;
      Regs[0x0715] &= 0xf0;
    }
}

static void
RTS_Setup_Arrangeline (struct st_device *dev, struct st_hwdconfig *hwdcfg,
		       SANE_Int colormode)
{
  dev->scanning->arrange_compression =
    (colormode == CM_LINEART) ? FALSE : hwdcfg->compression;

  if ((colormode == CM_LINEART)
      || ((colormode == CM_GRAY) && (hwdcfg->highresolution == FALSE)))
    arrangeline2 = 0;
  else
    arrangeline2 = hwdcfg->arrangeline;

  dev->scanning->arrange_hres = hwdcfg->highresolution;
  dev->scanning->arrange_sensor_evenodd_dist =
    (hwdcfg->highresolution == FALSE) ? 0 : hwdcfg->sensorevenodddistance;
}

static void
RTS_Setup_Channels (struct st_device *dev, SANE_Byte * Regs,
		    struct st_scanparams *scancfg, SANE_Int mycolormode)
{
  DBG (DBG_FNC, "> RTS_Setup_Channels(colormode=%i)\n", mycolormode);

  if ((scancfg != NULL) && (Regs != NULL))
    {
      if ((mycolormode != CM_COLOR) && (mycolormode != 3))
	{
	  /* CM_GRAY || CM_LINEART */
	  if (scancfg->samplerate == LINE_RATE)
	    {
	      /* Setting channels_per_dot to 1 */
	      data_bitset (&Regs[0x12], 0xc0, 1);	/*01------ */

	      /* setting one rgb_channel_order */
	      data_bitset (&Regs[0x12], 0x03, dev->sensorcfg->rgb_order[scancfg->channel]);		     /*------xx*/

	      /* set sensor_channel_color_order */
	      data_bitset (&Regs[0x60a], 0x3f, 6);		    /*--xxxxxx*/

	      /* set samplerate */
	      data_bitset (&Regs[0x1cf], 0x40, PIXEL_RATE);		     /*-x------*/

	      /* set unknown data */
	      data_bitset (&Regs[0x1cf], 0x80, 1);	/*x------- */

	      if (scancfg->channel == dev->sensorcfg->rgb_order[1])
		{
		  /* mexpts[CL_RED] = mexpts[CL_GREEN] */
		  data_lsb_set (&Regs[0x33], data_lsb_get (&Regs[0x39], 3),
				3);

		  /* expts[CL_RED] = expts[CL_GREEN] */
		  data_lsb_set (&Regs[0x36], data_lsb_get (&Regs[0x3c], 3),
				3);
		}
	      else if (scancfg->channel == dev->sensorcfg->rgb_order[2])
		{
		  /* mexpts[CL_RED] = mexpts[CL_BLUE] */
		  data_lsb_set (&Regs[0x33], data_lsb_get (&Regs[0x3f], 3),
				3);

		  /* expts[CL_RED] = expts[CL_BLUE] */
		  data_lsb_set (&Regs[0x36], data_lsb_get (&Regs[0x42], 3),
				3);
		}
	    }
	  else
	    {
	      /* e01 */
	      /* setting channels_per_dot to 2 */
	      data_bitset (&Regs[0x12], 0xc0, 2);

	      /* set two channel color order */
	      data_bitset (&Regs[0x12], 0x03, dev->sensorcfg->channel_gray[0]);			 /*------xx*/
	      data_bitset (&Regs[0x12], 0x0c, dev->sensorcfg->channel_gray[1]);			 /*----xx--*/

	      /* set samplerate */
	      data_bitset (&Regs[0x1cf], 0x40, LINE_RATE);

	      /* set unknown data */
	      data_bitset (&Regs[0x1cf], 0x80, 1);
	    }
	}
      else
	{
	  /* CM_COLOR || 3 */
	  /* e42 */

	  /* setting channels_per_dot to 3 */
	  data_bitset (&Regs[0x12], 0xc0, 3);

	  /* setting samplerate */
	  data_bitset (&Regs[0x1cf], 0x40, scancfg->samplerate);

	  /* set unknown data */
	  data_bitset (&Regs[0x1cf], 0x80, 0);

	  /* set sensor chanel_color_order */
	  data_bitset (&Regs[0x60a], 0x03, dev->sensorcfg->channel_color[2]);		   /*------xx*/
	  data_bitset (&Regs[0x60a], 0x0c, dev->sensorcfg->channel_color[1]);		   /*----xx--*/
	  data_bitset (&Regs[0x60a], 0x30, dev->sensorcfg->channel_color[0]);		   /*--xx----*/

	  /* set rgb_channel_order */
	  data_bitset (&Regs[0x12], 0x03, dev->sensorcfg->rgb_order[0]);	      /*------xx*/
	  data_bitset (&Regs[0x12], 0x0c, dev->sensorcfg->rgb_order[1]);	      /*----xx--*/
	  data_bitset (&Regs[0x12], 0x30, dev->sensorcfg->rgb_order[2]);	      /*--xx----*/
	}
    }
}

static SANE_Int
RTS_Setup (struct st_device *dev, SANE_Byte * Regs,
	   struct st_scanparams *scancfg, struct st_hwdconfig *hwdcfg,
	   struct st_gain_offset *gain_offset)
{
  SANE_Int rst = ERROR;		/* default */
  SANE_Int lSMode;
  SANE_Byte mycolormode;

  DBG (DBG_FNC, "+ RTS_Setup:\n");
  dbg_ScanParams (scancfg);
  dbg_hwdcfg (hwdcfg);

  mycolormode = scancfg->colormode;
  if (scancfg->colormode != CM_COLOR)
    {
      if (scancfg->colormode == CM_LINEART)
	scancfg->depth = 8;

      if (scancfg->channel == 3)
	{
	  if (scancfg->colormode == CM_GRAY)
	    mycolormode = (hwdcfg->arrangeline != FIX_BY_SOFT) ? 3 : CM_COLOR;
	  else
	    mycolormode = 3;
	}
    }

  /* 42b47d6 */
  memcpy (&scan2, scancfg, sizeof (struct st_scanparams));

  scantype = hwdcfg->scantype;
  lSMode =
    RTS_GetScanmode (dev, scantype, mycolormode, scancfg->resolution_x);
  if (lSMode >= 0)
    {
      struct st_scanmode *sm = dev->scanmodes[lSMode];

      if (sm != NULL)
	{
	  SANE_Int dummyline, iLineDistance, resolution_ratio, bytes_per_line;
	  struct st_coords rts_coords;

	  iLineDistance = 0;

	  scancfg->timing = sm->timing;
	  scancfg->sensorresolution =
	    dev->timings[scancfg->timing]->sensorresolution;
	  scancfg->shadinglength =
	    (((scancfg->sensorresolution * 17) / 2) + 3) & 0xfffffffc;
	  scancfg->samplerate = sm->samplerate;

	  hwdcfg->motorplus = sm->motorplus;

	  /* set systemclock */
	  data_bitset (&Regs[0x00], 0x0f, sm->systemclock);

	  /* setting exposure times */
	  RTS_Setup_Exposure_Times (Regs, scancfg, sm);

	  /* setting arranges */
	  RTS_Setup_Arrangeline (dev, hwdcfg, mycolormode);

	  /* set up line distances */
	  iLineDistance =
	    RTS_Setup_Line_Distances (dev, Regs, scancfg, hwdcfg, mycolormode,
				      arrangeline);

	  /* 4c67 */

	  /* setup channel colors */
	  RTS_Setup_Channels (dev, Regs, scancfg, mycolormode);

	  /* setup depth */
	  bytes_per_line = RTS_Setup_Depth (Regs, scancfg, mycolormode);

	  /* f61 */

	  /* Set resolution ratio */
	  resolution_ratio =
	    (scancfg->sensorresolution / scancfg->resolution_x) & 0x1f;
	  data_bitset (&Regs[0xc0], 0x1f, resolution_ratio);

	  /* set sensor timing values */
	  RTS_Setup_SensorTiming (dev, scancfg->timing, Regs);

	  data_bitset (&Regs[0xd8], 0x40, ((scantype == ST_NORMAL) ? 0 : 1));		  /*-x------*/

	  /* Use static head ? */
	  data_bitset (&Regs[0xd8], 0x80, ((hwdcfg->static_head == FALSE) ? 1 : 0));	/*x------- */

	  /* Setting up gamma */
	  RTS_Setup_Gamma (Regs, hwdcfg);

	  /* setup shading correction */
	  RTS_Setup_Shading (Regs, scancfg, hwdcfg, bytes_per_line);

	  /* setup stepper motor */
	  hwdcfg->startpos =
	    RTS_Setup_Motor (dev, Regs, scancfg,
			     hwdcfg->motor_direction | MTR_ENABLED);

	  /* set coordinates */
	  dummyline = data_bitget (&Regs[0xd6], 0xf0);

	  if (scancfg->coord.left == 0)
	    scancfg->coord.left++;
	  if (scancfg->coord.top == 0)
	    scancfg->coord.top++;

	  rts_coords.left = scancfg->coord.left * resolution_ratio;
	  rts_coords.width = scancfg->coord.width * resolution_ratio;
	  rts_coords.top = scancfg->coord.top * dummyline;
	  rts_coords.height =
	    ((Regs[0x14d] & 0x3f) + scancfg->coord.height +
	     iLineDistance) * dummyline;

	  if ((rts_coords.left & 1) == 0)
	    rts_coords.left++;

	  RTS_Setup_Coords (Regs, rts_coords.left, rts_coords.top,
			    rts_coords.width, rts_coords.height);

	  data_bitset (&Regs[0x01], 0x06, 0);		   /*-----xx-*/

	  /* dummy_scan? */
	  data_bitset (&Regs[0x01], 0x10, hwdcfg->dummy_scan);		    /*---x----*/

	  data_bitset (&Regs[0x163], 0xc0, 1);	/*xx------ */

	  if (dev->scanning->arrange_compression != FALSE)
	    {
	      Regs[0x60b] &= 0x8f;
	      data_bitset (&Regs[0x60b], 0x10, 1);			 /*-001----*/
	    }
	  else
	    data_bitset (&Regs[0x60b], 0x7f, 0);		   /*-0000000*/

	  if (mycolormode == 3)
	    {
	      SANE_Int channels_per_line;

	      /* Set channels_per_line = channels_per_dot * scan_width */
	      channels_per_line =
		data_bitget (&Regs[0x12], 0xc0) * scancfg->coord.width;
	      data_wide_bitset (&Regs[0x060c], 0x3ffff, channels_per_line);

	      /* Sets 16 bits per channel */
	      data_bitset (&Regs[0x1cf], 0x30, 2);		    /*--10----*/

	      Regs[0x60b] |= 0x40;
	      if (v1619 == 0x21)
		{
		  dev->scanning->arrange_compression = FALSE;
		  data_bitset (&Regs[0x60b], 0x10, 0);			    /*---0----*/
		}

	      switch (scancfg->depth)
		{
		case 8:
		case 16:
		  Regs[0x060b] &= 0xf3;
		  break;
		case 12:
		  Regs[0x060b] = (Regs[0x060b] & 0xfb) | 0x08;
		  break;
		}

	      if (scancfg->colormode == CM_LINEART)
		data_bitset (&Regs[0x60b], 0x0c, 0);

	      /* disable gamma correction ¿? */
	      data_bitset (&Regs[0x1d0], 0x40, 0);
	    }

	  /* 5683 */
	  /* Set calibration table */
	  RTS_Setup_GainOffset (Regs, gain_offset);

	  rst = OK;
	}
    }

  DBG (DBG_FNC, "- RTS_Setup: %i\n", rst);

  return rst;
}

static void
RTS_Setup_Coords (SANE_Byte * Regs, SANE_Int iLeft, SANE_Int iTop,
		  SANE_Int width, SANE_Int height)
{
  DBG (DBG_FNC,
       "> RTS_Setup_Coords(*Regs, iLeft=%i, iTop=%i, width=%i, height=%i)\n",
       iLeft, iTop, width, height);

  if (Regs != NULL)
    {
      /* Set Left coord */
      data_lsb_set (&Regs[0xb0], iLeft, 2);

      /* Set Right coord */
      data_lsb_set (&Regs[0xb2], iLeft + width, 2);

      /* Set Top coord */
      data_lsb_set (&Regs[0xd0], iTop, 2);
      data_bitset (&Regs[0xd4], 0x0f, _B2 (iTop));

      /* Set Down coord */
      data_lsb_set (&Regs[0xd2], iTop + height, 2);
      data_bitset (&Regs[0xd4], 0xf0, _B2 (iTop + height));
    }
}

static void
RTS_Setup_GainOffset (SANE_Byte * Regs, struct st_gain_offset *gain_offset)
{
  SANE_Byte fake[] =
    { 0x19, 0x15, 0x19, 0x64, 0x64, 0x64, 0x74, 0xc0, 0x74, 0xc0, 0x6d,
    0xc0, 0x6d, 0xc0, 0x5f, 0xc0, 0x5f, 0xc0
  };

  DBG (DBG_FNC, "> RTS_Setup_GainOffset(*Regs, *gain_offset)\n");
  dbg_calibtable (gain_offset);

  if ((Regs != NULL) && (gain_offset != NULL))
    {
      if (RTS_Debug->calibrate == FALSE)
	{
	  data_bitset (&Regs[0x13], 0x03, gain_offset->pag[CL_RED]);		    /*------xx*/
	  data_bitset (&Regs[0x13], 0x0c, gain_offset->pag[CL_GREEN]);		    /*----xx--*/
	  data_bitset (&Regs[0x13], 0x30, gain_offset->pag[CL_BLUE]);		    /*--xx----*/

	  memcpy (&Regs[0x14], &fake, 18);
	}
      else
	{
	  SANE_Int a;

	  for (a = CL_RED; a <= CL_BLUE; a++)
	    {
	      /* Offsets */
	      Regs[0x1a + (a * 4)] = _B0 (gain_offset->edcg1[a]);
	      Regs[0x1b + (a * 4)] =
		((gain_offset->edcg1[a] >> 1) & 0x80) | (gain_offset->
							 edcg2[a] & 0x7f);
	      Regs[0x1c + (a * 4)] = _B0 (gain_offset->odcg1[a]);
	      Regs[0x1d + (a * 4)] =
		((gain_offset->odcg1[a] >> 1) & 0x80) | (gain_offset->
							 odcg2[a] & 0x7f);

	      /* Variable Gain Amplifier */
	      data_bitset (&Regs[0x14 + a], 0x1f, gain_offset->vgag1[a]);
	      data_bitset (&Regs[0x17 + a], 0x1f, gain_offset->vgag2[a]);
	    }

	  data_bitset (&Regs[0x13], 0x03, gain_offset->pag[CL_RED]);		    /*------xx*/
	  data_bitset (&Regs[0x13], 0x0c, gain_offset->pag[CL_GREEN]);		    /*----xx--*/
	  data_bitset (&Regs[0x13], 0x30, gain_offset->pag[CL_BLUE]);		    /*--xx----*/
	}
    }
}

static void
Calibrate_Free (struct st_cal2 *calbuffers)
{
  DBG (DBG_FNC, "> Calibrate_Free(*calbuffers)\n");

  if (calbuffers != NULL)
    {
      SANE_Int c;

      if (calbuffers->table2 != NULL)
	{
	  free (calbuffers->table2);
	  calbuffers->table2 = NULL;
	}

      for (c = 0; c < 4; c++)
	{
	  if (calbuffers->tables[c] != NULL)
	    {
	      free (calbuffers->tables[c]);
	      calbuffers->tables[c] = NULL;
	    }
	}

      calbuffers->shadinglength1 = 0;
      calbuffers->tables_size = 0;
      calbuffers->shadinglength3 = 0;
    }
}

static SANE_Int
Calibrate_Malloc (struct st_cal2 *calbuffers, SANE_Byte * Regs,
		  struct st_calibration *myCalib, SANE_Int somelength)
{
  SANE_Int myshadinglength, pos;
  SANE_Int rst;

  if ((calbuffers != NULL) && (Regs != NULL) && (myCalib != NULL))
    {
      if ((Regs[0x1bf] & 0x18) == 0)
	{
	  if ((((Regs[0x1cf] >> 1) & Regs[0x1cf]) & 0x04) != 0)
	    calbuffers->table_count = 2;
	  else
	    calbuffers->table_count = 4;
	}
      else
	calbuffers->table_count = 4;

      /*365d */
      myshadinglength = myCalib->shadinglength * 2;
      calbuffers->shadinglength1 = min (myshadinglength, somelength);

      if ((myshadinglength % somelength) != 0)
	calbuffers->tables_size =
	  (myshadinglength >= somelength) ? somelength * 2 : somelength;
      else
	calbuffers->tables_size = somelength;

      if (myshadinglength >= somelength)
	{
	  calbuffers->shadinglength1 =
	    (myshadinglength % calbuffers->shadinglength1) +
	    calbuffers->shadinglength1;
	  calbuffers->shadinglength3 =
	    ((myCalib->shadinglength * 2) / somelength) - 1;
	}
      else
	calbuffers->shadinglength3 = 0;

      calbuffers->shadinglength3 =
	(somelength / 16) * calbuffers->shadinglength3;

      rst = OK;
      for (pos = 0; pos < calbuffers->table_count; pos++)
	{
	  calbuffers->tables[pos] =
	    (USHORT *) malloc (calbuffers->tables_size * sizeof (USHORT));
	  if (calbuffers->tables[pos] == NULL)
	    {
	      rst = ERROR;
	      break;
	    }
	}

      if (rst == OK)
	{
	  calbuffers->table2 =
	    (USHORT *) malloc (calbuffers->tables_size * sizeof (USHORT));
	  if (calbuffers->table2 == NULL)
	    rst = ERROR;
	}

      if (rst != OK)
	Calibrate_Free (calbuffers);
    }
  else
    rst = ERROR;

  DBG (DBG_FNC,
       "> Calibrate_Malloc(*calbuffers, *Regs, *myCalib, somelength=%i): %i\n",
       somelength, rst);

  return rst;
}

static SANE_Int
fn3560 (USHORT * table, struct st_cal2 *calbuffers, SANE_Int * tablepos)
{
  /*05FEF974   001F99B0  |table = 001F99B0
     05FEF978   05FEFA08  |calbuffers->tables[0] = 05FEFA08
     05FEF97C   000000A0  |calbuffers->shadinglength3 = 000000A0
     05FEF980   00000348  |calbuffers->shadinglength1 = 00000348
     05FEF984   04F01502  |calbuffers->table_count = 04F01502
     05FEF988   05FEF998  \Arg6 = 05FEF998
   */

  if (table != NULL)
    {
      SANE_Int pos[4] = { 0, 0, 0, 0 };	/*f960 f964 f968 f96c */
      SANE_Int usetable = 0;
      SANE_Int a;

      SANE_Int mylength3 = calbuffers->shadinglength1;	/*f97c */
      SANE_Byte *pPointer =
	(SANE_Byte *) (table + (calbuffers->shadinglength3 * 16));

      DBG (DBG_FNC, "> fn3560(*table, *calbuffers, *tablepos)\n");

      if (mylength3 > 0)
	{
	  do
	    {
	      if (calbuffers->tables[usetable] != NULL)
		{
		  if (mylength3 <= 16)
		    {
		      if (mylength3 > 0)
			{
			  do
			    {
			      *(calbuffers->tables[usetable] +
				pos[usetable]) = _B0 (*pPointer);
			      pPointer++;
			      pos[usetable]++;
			      mylength3--;
			    }
			  while (mylength3 > 0);
			}
		      break;
		    }

		  for (a = 0; a < 16; a++)
		    {
		      *(calbuffers->tables[usetable] + pos[usetable]) =
			_B0 (*pPointer);
		      pPointer++;
		      pos[usetable]++;
		    }
		}

	      mylength3 -= 16;
	      usetable++;
	      if (usetable == calbuffers->table_count)
		usetable = 0;
	    }
	  while (mylength3 > 0);
	}

      /*35f8 */
      if (calbuffers->table_count > 0)
	{
	  /* Return position of each table */
	  memcpy (tablepos, pos, sizeof (SANE_Int) * 4);
	}
    }

  return OK;
}

static SANE_Int
Calib_WriteTable (struct st_device *dev, SANE_Byte * table, SANE_Int size,
		  SANE_Int data)
{
  SANE_Int rst = ERROR;

  DBG (DBG_FNC, "+ Calib_WriteTable(*table, size=%i):\n", size);

  if ((table != NULL) && (size > 0))
    {
      SANE_Int transferred;

      if (RTS_DMA_Reset (dev) == OK)
	{
	  /* Send size to write */
	  if (RTS_DMA_Enable_Write (dev, 0x0004, size, data) == OK)
	    /* Send data */
	    rst = Bulk_Operation (dev, BLK_WRITE, size, table, &transferred);
	}
    }

  DBG (DBG_FNC, "- Calib_WriteTable: %i\n", rst);

  return rst;
}

static SANE_Int
Calib_ReadTable (struct st_device *dev, SANE_Byte * table, SANE_Int size,
		 SANE_Int data)
{
  SANE_Int rst = ERROR;

  DBG (DBG_FNC, "+ Calib_ReadTable(*table, size=%i):\n", size);

  if ((table != NULL) && (size > 0))
    {
      SANE_Int transferred;

      if (RTS_DMA_Reset (dev) == OK)
	{
	  /* Send size to read */
	  if (RTS_DMA_Enable_Read (dev, 0x0004, size, data) == OK)
	    /* Retrieve data */
	    rst = Bulk_Operation (dev, BLK_READ, size, table, &transferred);
	}
    }

  DBG (DBG_FNC, "- Calib_ReadTable: %i\n", rst);

  return rst;
}

static SANE_Int
fn3330 (struct st_device *dev, SANE_Byte * Regs, struct st_cal2 *calbuffers,
	SANE_Int sensorchannelcolor, SANE_Int * tablepos, SANE_Int data)
{
  /*05EEF968   04F0F7F8  |Regs = 04F0F7F8
     05EEF96C   02DEC838  |calbuffers->table2 = 02DEC838
     05EEF970   05EEFA08  |calbuffers->tables[] = 05EEFA08
     05EEF974   00000000  |sensorchannelcolor = 00000000
     05EEF978   000000A0  |calbuffers->shadinglength3 = 000000A0
     05EEF97C   00000400  |calbuffers->tables_size = 00000400
     05EEF980   05EEF998  |&pos = 05EEF998
     05EEF984   00221502  |calbuffers->table_count = 00221502
     05EEF988   00000000  \data = 00000000
   */

  SANE_Int table_count = calbuffers->table_count;	/*f960 */
  SANE_Int schcolor = _B0 (sensorchannelcolor);
  SANE_Int a = 0;
  SANE_Int tablelength = calbuffers->shadinglength3 / table_count;	/*f954 */
  SANE_Int val_color = 0;	/*f974 */
  SANE_Int val_lineart = 0;	/*f978 */
  SANE_Int val_gray = 0;	/*ebx */
  SANE_Int value4 = 0;		/*ebp */
  SANE_Int size;
  SANE_Int rst = OK;

  DBG (DBG_FNC,
       "+ fn3330(*Regs, *calbuffers, sensorchannelcolor=%i, *tablepos, data=%i):\n",
       sensorchannelcolor, data);

  if (calbuffers->table_count > 0)
    {
      do
	{
	  if (calbuffers->table_count == 2)
	    {
	      /*338c */
	      if (a != 0)
		{
		  /*3394 */
		  if (_B0 (data) == 0)
		    {
		      val_color = 0x100000;
		      val_lineart = 0x100000;
		      val_gray = 0x200000;
		    }
		  else
		    {
		      /*343a */
		      val_color = 0x300000;
		      val_lineart = 0x300000;
		      val_gray = 0;
		    }
		}
	      else
		{
		  /*33be */
		  if (_B0 (data) == 0)
		    {
		      val_color = 0;
		      val_lineart = 0;
		      val_gray = 0x300000;
		    }
		  else
		    {
		      /*342a */
		      val_color = 0x200000;
		      val_lineart = 0x200000;
		      val_gray = 0x100000;
		    }
		}
	    }
	  else
	    {
	      /*33d5 */
	      switch (a)
		{
		case 0:
		  val_color = 0;
		  val_lineart = 0;
		  val_gray = 0x300000;
		  break;
		case 1:
		  val_color = 0x200000;
		  val_lineart = 0x200000;
		  val_gray = 0x100000;
		  break;
		case 2:
		  val_color = 0x100000;
		  val_lineart = 0x100000;
		  val_gray = 0x200000;
		  break;
		case 3:
		  val_color = 0x300000;
		  val_lineart = 0x300000;
		  val_gray = 0;
		  break;
		}
	    }

	  /*3449 */
	  switch (schcolor)
	    {
	    case CM_LINEART:
	      size =
		(((Regs[0x1bf] >> 1) & 3) << 0x10) | (Regs[0x1be] << 0x08) |
		Regs[0x1bd];
	      value4 = (tablelength + size) | val_lineart;
	      break;
	    case CM_GRAY:
	      size =
		((Regs[0x1bf] & 1) << 0x10) | (Regs[0x1bc] << 0x08) |
		Regs[0x1bb];
	      value4 = (tablelength + size) | val_gray;
	      break;
	    default:
	      size = _B0 (Regs[0x1ba]);
	      value4 = (tablelength + size) | val_color;
	      break;
	    }

	  if (Calib_ReadTable
	      (dev, (SANE_Byte *) calbuffers->table2, calbuffers->tables_size,
	       value4) != OK)
	    {
	      rst = ERROR;
	      break;
	    }

	  memcpy (calbuffers->tables[a], calbuffers->table2, tablepos[a]);

	  if (tablepos[a + 1] == 0)
	    break;

	  a++;
	}
      while (a < calbuffers->table_count);
    }

  DBG (DBG_FNC, "- fn3330: %i\n", rst);

  return rst;
}

static SANE_Int
fn3730 (struct st_device *dev, struct st_cal2 *calbuffers, SANE_Byte * Regs,
	USHORT * table, SANE_Int sensorchannelcolor, SANE_Int data)
{
  /*05FEF9AC   |calbuffers         = 05FEF9F8
     05FEF9B0   |Regs               = 04EFF7F8
     05FEF9B4   |table              = 001F99B0
     05FEF9B8   |sensorchannelcolor = 00000000
     05FEF9BC   |data               = 00000000
   */

  SANE_Int pos[4] = { 0, 0, 0, 0 };	/*f998 f99c f9a0 f9a4 */
  SANE_Int rst;

  DBG (DBG_FNC,
       "+ fn3730(*calbuffers, *Regs, *table, sensorchannelcolor=%i, data=%i):\n",
       sensorchannelcolor, data);

  fn3560 (table, calbuffers, pos);
  rst = fn3330 (dev, Regs, calbuffers, sensorchannelcolor, pos, data);

  DBG (DBG_FNC, "- fn3730: %i\n", rst);

  return rst;
}

static SANE_Int
Shading_white_apply (struct st_device *dev, SANE_Byte * Regs,
		     SANE_Int channels, struct st_calibration *myCalib,
		     struct st_cal2 *calbuffers)
{
  SANE_Int rst = OK;

  DBG (DBG_FNC, "+ Shading_white_apply(channels=%i)\n", channels);

  /*3e7f */
  Calibrate_Malloc (calbuffers, Regs, myCalib,
		    (RTS_Debug->usbtype == USB20) ? 0x200 : 0x40);

  if (channels > 0)
    {
      /*int a; */
      SANE_Int chnl;
      SANE_Int pos;		/*fa2c */
      SANE_Int transferred;

      rst = ERROR;

      for (chnl = 0; chnl < channels; chnl++)
	{
	  /*for (a = 0; a < myCalib->shadinglength; a++)
	     myCalib->black_shading[chnl][a] = 0x2000; */
	  /* 11 tries */
	  for (pos = 0; pos <= 10; pos++)
	    {
	      /* Send size to write */
	      if (RTS_DMA_Enable_Write
		  (dev, dev->sensorcfg->channel_color[chnl] | 0x14,
		   myCalib->shadinglength, 0) == OK)
		/* Send data */
		Bulk_Operation (dev, BLK_WRITE,
				myCalib->shadinglength * sizeof (USHORT),
				(SANE_Byte *) & myCalib->
				white_shading[chnl][myCalib->first_position -
						    1], &transferred);

	      /*3df7 */
	      if (fn3730
		  (dev, calbuffers, Regs,
		   &myCalib->white_shading[chnl][myCalib->first_position - 1],
		   dev->sensorcfg->channel_color[chnl], 1) == OK)
		{
		  rst = OK;
		  break;
		}

	      RTS_DMA_Cancel (dev);
	    }
	}
    }

  Calibrate_Free (calbuffers);

  DBG (DBG_FNC, "- Shading_white_apply: %i\n", rst);

  return OK;
}

static SANE_Int
Shading_black_apply (struct st_device *dev, SANE_Byte * Regs,
		     SANE_Int channels, struct st_calibration *myCalib,
		     struct st_cal2 *calbuffers)
{
  SANE_Int rst = OK;

  DBG (DBG_FNC, "+ Shading_black_apply(channels=%i)\n", channels);

  /* 3d79 */
  Calibrate_Malloc (calbuffers, Regs, myCalib,
		    (RTS_Debug->usbtype == USB20) ? 0x200 : 0x40);

  if (channels > 0)
    {
      /*int a; */
      SANE_Int chnl;
      SANE_Int pos;		/*fa2c */
      SANE_Int transferred;

      rst = ERROR;

      for (chnl = 0; chnl < channels; chnl++)
	{
	  /* 11 tries */
	  /*for (a = 0; a < myCalib->shadinglength; a++)
	     myCalib->black_shading[chnl][a] = 0x2000; */

	  for (pos = 0; pos <= 10; pos++)
	    {
	      /* Send size to write */
	      if (RTS_DMA_Enable_Write
		  (dev, dev->sensorcfg->channel_color[chnl] | 0x10,
		   myCalib->shadinglength, 0) == OK)
		/* Send data */
		Bulk_Operation (dev, BLK_WRITE,
				myCalib->shadinglength * sizeof (USHORT),
				(SANE_Byte *) & myCalib->
				black_shading[chnl][myCalib->first_position -
						    1], &transferred);

	      /*3df7 */
	      if (fn3730
		  (dev, calbuffers, Regs,
		   &myCalib->black_shading[chnl][myCalib->first_position - 1],
		   dev->sensorcfg->channel_color[chnl], 0) == OK)
		{
		  rst = OK;
		  break;
		}

	      RTS_DMA_Cancel (dev);
	    }
	}
    }

  /*3e62 */
  Calibrate_Free (calbuffers);

  DBG (DBG_FNC, "- Shading_black_apply: %i\n", rst);

  return OK;
}

static SANE_Int
Shading_apply (struct st_device *dev, SANE_Byte * Regs,
	       struct st_scanparams *myvar, struct st_calibration *myCalib)
{
  /*
     Regs f1bc
     myvar     f020
     hwdcfg  e838
     arg4      e81c
     myCalib   e820
   */

  SANE_Int rst;			/* lf9e0 */
  SANE_Int myfact;		/* e820 */
  SANE_Int shadata;
  SANE_Byte channels;		/* f9d4 */
  SANE_Int myShadingBase;	/* e818 */

  char lf9d1;
  char lf9d0;

  DBG (DBG_FNC, "+ Shading_apply(*Regs, *myvar, *mygamma, *myCalib):\n");
  dbg_ScanParams (myvar);

  lf9d0 = (Regs[0x60b] >> 6) & 1;
  lf9d1 = (Regs[0x60b] >> 4) & 1;
  Regs[0x060b] &= 0xaf;
  rst = Write_Byte (dev->usb_handle, 0xee0b, Regs[0x060b]);
  if (rst == OK)
    {
      SANE_Byte colormode = myvar->colormode;	/*fa24 */
      SANE_Int le7cc, le7d8;
      struct st_cal2 calbuffers;	/* f9f8 */

      if (colormode != CM_COLOR)
	{
	  if (myvar->channel != 3)
	    {
	      if (colormode != 3)
		channels = (myvar->samplerate == PIXEL_RATE) ? 2 : 1;
	      else
		channels = 3;
	    }
	  else
	    {
	      colormode = 3;
	      channels = 3;
	    }
	}
      else
	channels = 3;

      /*
         White shading formula :    2000H x Target / (Wn-Dn) = White Gain data ----- for 8 times system
         White shading formula :    4000H x Target / (Wn-Dn) = White Gain data ----- for 4 times system
         For example : Target = 3FFFH   Wn = 2FFFH     Dn = 0040H  and 8 times system operation
         then   White Gain = 2000H x 3FFFH / (2FFFH-0040H) = 2AE4H (1.34033 times)
       */
      /* 3aad */
      if (colormode == 3)
	{
	  /*
	     SANE_Int pos;
	     SANE_Int colour;

	     myShadingBase = shadingbase;

	     for (colour = 0; colour < channels; colour++)
	     {
	     if (myCalib->white_shading[colour] != NULL)
	     {
	     myfact = shadingfact[colour];
	     if (myCalib->shadinglength > 0)
	     {
	     for (pos = myCalib->first_position - 1; pos < myCalib->shadinglength; pos++)
	     myCalib->white_shading[colour][pos] = (myCalib->white_shading[colour][pos] * myfact) / myShadingBase;
	     }
	     } else break;
	     }
	   */
	}

      /* 3b3b */
      if (myCalib->shading_enabled != FALSE)
	{
	  /* 3b46 */
	  SANE_Int colour, pos;
	  le7cc = shadingbase;
	  le7d8 = shadingbase;

	  DBG (DBG_FNC, "-> Shading type: %i\n", myCalib->shading_type);

	  for (colour = 0; colour < channels; colour++)
	    {
	      if (colormode == 3)
		le7cc = shadingfact[colour];

	      myShadingBase = ((Regs[0x1cf] & 2) != 0) ? 0x2000 : 0x4000;

	      myfact = myCalib->WRef[colour] * myShadingBase;

	      if (myCalib->shading_type == 2)
		{
		  /*3bd8 */
		  if ((myCalib->black_shading[colour] != NULL)
		      && (myCalib->white_shading[colour] != NULL))
		    {
		      for (pos = myCalib->first_position - 1;
			   pos < myCalib->shadinglength; pos++)
			{
			  if (myCalib->white_shading[colour][pos] == 0)
			    shadata = myShadingBase;
			  else
			    shadata =
			      myfact / myCalib->white_shading[colour][pos];

			  shadata = min ((shadata * le7cc) / le7d8, 0xff00);
			  myCalib->black_shading[colour][pos] &= 0xff;
			  myCalib->black_shading[colour][pos] |=
			    shadata & 0xff00;
			}
		    }
		  else
		    break;
		}
	      else
		{
		  /*3c63 */
		  if (myCalib->shading_type == 3)
		    {
		      /*3c68 */
		      if (myCalib->black_shading[colour] != NULL)
			{
			  for (pos = myCalib->first_position - 1;
			       pos < myCalib->shadinglength; pos++)
			    {
			      if (myCalib->black_shading[colour][pos] == 0)
				shadata = myShadingBase;
			      else
				shadata =
				  myfact /
				  myCalib->black_shading[colour][pos];

			      shadata =
				min ((shadata * le7cc) / le7d8, 0xffc0);
			      myCalib->black_shading[colour][pos] &= 0x3f;
			      myCalib->black_shading[colour][pos] |=
				shadata & 0xffc0;
			    }
			}
		      else
			break;
		    }
		  else
		    {
		      /*3ce3 */
		      if (myCalib->white_shading[colour] != NULL)
			{
			  for (pos = 0; pos < myCalib->shadinglength; pos++)
			    {
			      if (myCalib->white_shading[colour][pos] == 0)
				shadata = myShadingBase;
			      else
				shadata =
				  myfact /
				  myCalib->white_shading[colour][pos];

			      shadata =
				min ((shadata * le7cc) / le7d8, 0xffff);
			      myCalib->white_shading[colour][pos] = shadata;
			    }
			}
		      else
			break;
		    }
		}
	    }
	}

      /*3d4c */
      bzero (&calbuffers, sizeof (struct st_cal2));

      /* If black shading correction is enabled ... */
      if ((Regs[0x1cf] & 8) != 0)
	Shading_black_apply (dev, Regs, channels, myCalib, &calbuffers);

      /*3e6e */

      /* If white shading correction is enabled ... */
      if ((Regs[0x1cf] & 4) != 0)
	Shading_white_apply (dev, Regs, channels, myCalib, &calbuffers);

      /* 3f74 */
      if (rst == 0)
	{
	  data_bitset (&Regs[0x60b], 0x40, lf9d0);		/*-x------*/
	  data_bitset (&Regs[0x60b], 0x10, lf9d1);		/*---x----*/

	  rst = Write_Byte (dev->usb_handle, 0xee0b, Regs[0x060b]);
	}
    }
  /*3fb5 */

  DBG (DBG_FNC, "- Shading_apply: %i\n", rst);

  return rst;
}

static SANE_Int
Bulk_Operation (struct st_device *dev, SANE_Byte op, SANE_Int buffer_size,
		SANE_Byte * buffer, SANE_Int * transfered)
{
  SANE_Int iTransferSize, iBytesToTransfer, iPos, rst, iBytesTransfered;

  DBG (DBG_FNC, "+ Bulk_Operation(op=%s, buffer_size=%i, buffer):\n",
       ((op & 0x01) != 0) ? "READ" : "WRITE", buffer_size);

  iBytesToTransfer = buffer_size;
  iPos = 0;
  rst = OK;
  iBytesTransfered = 0;

  if (transfered != NULL)
    *transfered = 0;

  iTransferSize = min (buffer_size, RTS_Debug->dmatransfersize);

  if (op != 0)
    {
      /* Lectura */
      do
	{
	  iTransferSize = min (iTransferSize, iBytesToTransfer);

	  iBytesTransfered =
	    Read_Bulk (dev->usb_handle, &buffer[iPos], iTransferSize);
	  if (iBytesTransfered < 0)
	    {
	      rst = ERROR;
	      break;
	    }
	  else
	    {
	      if (transfered != NULL)
		*transfered += iBytesTransfered;
	    }
	  iPos += iTransferSize;
	  iBytesToTransfer -= iTransferSize;
	}
      while (iBytesToTransfer > 0);
    }
  else
    {
      /* Escritura */
      do
	{
	  iTransferSize = min (iTransferSize, iBytesToTransfer);

	  if (Write_Bulk (dev->usb_handle, &buffer[iPos], iTransferSize) !=
	      OK)
	    {
	      rst = ERROR;
	      break;
	    }
	  else
	    {
	      if (transfered != NULL)
		*transfered += iTransferSize;
	    }
	  iPos += iTransferSize;
	  iBytesToTransfer -= iTransferSize;
	}
      while (iBytesToTransfer > 0);
    }

  DBG (DBG_FNC, "- Bulk_Operation: %i\n", rst);

  return rst;
}

static SANE_Int
Reading_BufferSize_Notify (struct st_device *dev, SANE_Int data,
			   SANE_Int size)
{
  SANE_Int rst;

  DBG (DBG_FNC, "+ Reading_BufferSize_Notify(data=%i, size=%i):\n", data,
       size);

  rst = RTS_DMA_Enable_Read (dev, 0x0008, size, data);

  DBG (DBG_FNC, "- Reading_BufferSize_Notify: %i\n", rst);

  return rst;
}

static SANE_Int
Reading_Wait (struct st_device *dev, SANE_Byte Channels_per_dot,
	      SANE_Byte Channel_size, SANE_Int size, SANE_Int * last_amount,
	      SANE_Int seconds, SANE_Byte op)
{
  SANE_Int rst;
  SANE_Byte cTimeout, executing;
  SANE_Int lastAmount, myAmount;
  long tick;

  DBG (DBG_FNC,
       "+ Reading_Wait(Channels_per_dot=%i, Channel_size=%i, size=%i, *last_amount, seconds=%i, op=%i):\n",
       Channels_per_dot, Channel_size, size, seconds, op);

  rst = OK;
  cTimeout = FALSE;
  lastAmount = 0;

  myAmount = Reading_BufferSize_Get (dev, Channels_per_dot, Channel_size);
  if (myAmount < size)
    {
      /* Wait until scanner fills its buffer */
      if (seconds == 0)
	seconds = 10;
      tick = GetTickCount () + (seconds * 1000);

      while (cTimeout == FALSE)
	{
	  myAmount =
	    Reading_BufferSize_Get (dev, Channels_per_dot, Channel_size);

	  /* check special case */
	  if (op == TRUE)
	    {
	      if (((myAmount + 0x450) > size)
		  || (RTS_IsExecuting (dev, &executing) == FALSE))
		break;
	    }

	  if (myAmount < size)
	    {
	      /* Check timeout */
	      if (myAmount == lastAmount)
		{
		  /* we are in timeout? */
		  if (tick < GetTickCount ())
		    {
		      /* TIMEOUT */
		      rst = ERROR;
		      cTimeout = TRUE;
		    }
		  else
		    usleep (100 * 1000);
		}
	      else
		{
		  /* Amount increased, update tick */
		  lastAmount = myAmount;
		  tick = GetTickCount () + (seconds * 1000);
		}
	    }
	  else
	    {
	      lastAmount = myAmount;
	      break;		/* buffer full */
	    }
	}
    }

  if (last_amount != NULL)
    *last_amount = myAmount;

  DBG (DBG_FNC, "- Reading_Wait: %i , last_amount=%i\n", rst, myAmount);

  return rst;
}

static SANE_Int
RTS_GetImage_GetBuffer (struct st_device *dev, double dSize,
			char unsigned *buffer, double *transferred)
{
  SANE_Int rst = ERROR;
  SANE_Int itransferred;
  double dtransferred = 0;

  DBG (DBG_FNC, "+ RTS_GetImage_GetBuffer(dSize=%f, buffer, transferred):\n",
       dSize);

  rst = OK;
  dSize /= 2;

  if (dSize > 0)
    {
      SANE_Int myLength;
      SANE_Int iPos = 0;

      do
	{
	  itransferred = 0;
	  myLength =
	    (dSize <=
	     RTS_Debug->dmasetlength) ? dSize : RTS_Debug->dmasetlength;

	  if (myLength > 0x1ffe0)
	    myLength = 0x1ffe0;

	  rst = ERROR;
	  if (Reading_Wait (dev, 0, 1, myLength * 2, NULL, 5, FALSE) == OK)
	    {
	      if (Reading_BufferSize_Notify (dev, 0, myLength * 2) == OK)
		rst =
		  Bulk_Operation (dev, BLK_READ, myLength * 2, &buffer[iPos],
				  &itransferred);
	    }

	  if (rst != OK)
	    break;

	  iPos += itransferred;
	  dSize -= itransferred;
	  dtransferred += itransferred * 2;
	}
      while (dSize > 0);
    }

  /* Return bytes transferred */
  if (transferred != NULL)
    *transferred = dtransferred;

  if (rst != OK)
    RTS_DMA_Cancel (dev);

  DBG (DBG_FNC, "- RTS_GetImage_GetBuffer: %i\n", rst);

  return rst;
}

static SANE_Int
RTS_GetImage_Read (struct st_device *dev, SANE_Byte * buffer,
		   struct st_scanparams *scancfg, struct st_hwdconfig *hwdcfg)
{
  /*buffer   f80c = esp+14
     scancfg    f850 = esp+18
     hwdcfg faac = */

  SANE_Int rst = ERROR;

  DBG (DBG_FNC, "+ RTS_GetImage_Read(buffer, scancfg, hwdcfg):\n");

  if (buffer != NULL)
    {
      double dSize = scancfg->bytesperline * scancfg->coord.height;
      SANE_Byte exfn;

      if (scancfg->depth == 12)
	dSize = (dSize * 3) / 4;

      /*3ff6 */
      exfn = 1;
      if (hwdcfg != NULL)
	if (hwdcfg->compression != FALSE)
	  exfn = 0;

      if (exfn != 0)
	{
	  double transferred;
	  rst = RTS_GetImage_GetBuffer (dev, dSize, buffer, &transferred);
	}

      if (rst == OK)
	RTS_WaitScanEnd (dev, 1500);
    }

  DBG (DBG_FNC, "- RTS_GetImage_Read: %i\n", rst);

  return rst;
}

static SANE_Int
RTS_GetImage (struct st_device *dev, SANE_Byte * Regs,
	      struct st_scanparams *scancfg,
	      struct st_gain_offset *gain_offset, SANE_Byte * buffer,
	      struct st_calibration *myCalib, SANE_Int options,
	      SANE_Int gaincontrol)
{
  /* 42b8e10 */

  SANE_Int rst = ERROR;		/* default */

  DBG (DBG_FNC,
       "+ RTS_GetImage(*Regs, *scancfg, *gain_offset, *buffer, myCalib, options=0x%08x, gaincontrol=%i):\n",
       options, gaincontrol);
  dbg_ScanParams (scancfg);

  /* validate arguments */
  if ((Regs != NULL) && (scancfg != NULL))
    {
      if ((scancfg->coord.width != 0) && (scancfg->coord.height != 0))
	{
	  struct st_scanparams *myscancfg;

	  /* let's make a copy of scan config */
	  myscancfg =
	    (struct st_scanparams *) malloc (sizeof (struct st_scanparams));
	  if (myscancfg != NULL)
	    {
	      struct st_hwdconfig *hwdcfg;

	      memcpy (myscancfg, scancfg, sizeof (struct st_scanparams));

	      /* Allocate space for low level config */
	      hwdcfg =
		(struct st_hwdconfig *) malloc (sizeof (struct st_hwdconfig));
	      if (hwdcfg != NULL)
		{
		  bzero (hwdcfg, sizeof (struct st_hwdconfig));

		  if (((options & 2) != 0) || ((_B1 (options) & 1) != 0))
		    {
		      /* switch off lamp */
		      data_bitset (&Regs[0x146], 0x40, 0);

		      Write_Byte (dev->usb_handle, 0xe946, Regs[0x146]);
		      usleep (1000 * ((v14b4 == 0) ? 500 : 300));
		    }

		  hwdcfg->scantype = scan.scantype;
		  hwdcfg->use_gamma_tables =
		    ((options & OP_USE_GAMMA) != 0) ? 1 : 0;
		  hwdcfg->white_shading =
		    ((options & OP_WHITE_SHAD) != 0) ? 1 : 0;
		  hwdcfg->black_shading =
		    ((options & OP_BLACK_SHAD) != 0) ? 1 : 0;
		  hwdcfg->motor_direction =
		    ((options & OP_BACKWARD) !=
		     0) ? MTR_BACKWARD : MTR_FORWARD;
		  hwdcfg->compression =
		    ((options & OP_COMPRESSION) != 0) ? 1 : 0;
		  hwdcfg->static_head =
		    ((options & OP_STATIC_HEAD) != 0) ? 1 : 0;
		  hwdcfg->dummy_scan = (buffer == NULL) ? TRUE : FALSE;
		  hwdcfg->arrangeline = 0;
		  hwdcfg->highresolution =
		    (myscancfg->resolution_x > 1200) ? TRUE : FALSE;
		  hwdcfg->unk3 = 0;

		  /* Set Left coord */
		  myscancfg->coord.left +=
		    ((dev->sensorcfg->type == CCD_SENSOR) ? 24 : 50);

		  switch (myscancfg->resolution_x)
		    {
		    case 1200:
		      myscancfg->coord.left -= 63;
		      break;
		    case 2400:
		      myscancfg->coord.left -= 126;
		      break;
		    }

		  if (myscancfg->coord.left < 0)
		    myscancfg->coord.left = 0;

		  RTS_Setup (dev, Regs, myscancfg, hwdcfg, gain_offset);

		  /* Setting exposure time */
		  switch (scan.scantype)
		    {
		    case ST_NORMAL:
		      if (scan.resolution_x == 100)
			{
			  SANE_Int iValue;
			  SANE_Byte *myRegs;

			  myRegs =
			    (SANE_Byte *) malloc (RT_BUFFER_LEN *
						  sizeof (SANE_Byte));
			  if (myRegs != NULL)
			    {
			      bzero (myRegs,
				     RT_BUFFER_LEN * sizeof (SANE_Byte));
			      RTS_Setup (dev, myRegs, &scan, hwdcfg,
					 gain_offset);

			      iValue = data_lsb_get (&myRegs[0x30], 3);
			      data_lsb_set (&Regs[0x30], iValue, 3);

			      /*Copy myregisters mexpts to Regs mexpts */
			      iValue = data_lsb_get (&myRegs[0x33], 3);
			      data_lsb_set (&Regs[0x33], iValue, 3);

			      iValue = data_lsb_get (&myRegs[0x39], 3);
			      data_lsb_set (&Regs[0x39], iValue, 3);

			      iValue = data_lsb_get (&myRegs[0x3f], 3);
			      data_lsb_set (&Regs[0x3f], iValue, 3);

			      free (myRegs);
			    }
			}
		      break;
		    case ST_NEG:
		      {
			SANE_Int myvalue;

			/* Setting exposure times for Negative scans */
			data_lsb_set (&Regs[0x30], myscancfg->expt, 3);
			data_lsb_set (&Regs[0x33], myscancfg->expt, 3);
			data_lsb_set (&Regs[0x39], myscancfg->expt, 3);
			data_lsb_set (&Regs[0x3f], myscancfg->expt, 3);

			data_lsb_set (&Regs[0x36], 0, 3);
			data_lsb_set (&Regs[0x3c], 0, 3);
			data_lsb_set (&Regs[0x42], 0, 3);

			myvalue =
			  ((myscancfg->expt +
			    1) / (data_lsb_get (&Regs[0xe0], 1) + 1)) - 1;
			data_lsb_set (&Regs[0xe1], myvalue, 3);
		      }
		      break;
		    }

		  /* 91a0 */
		  if (myscancfg->resolution_y > 600)
		    {
		      options |= 0x20000000;
		      if (options != 0)	/* Always true ... */
			SetMultiExposure (dev, Regs);
		      else
			myscancfg->coord.top += hwdcfg->startpos;
		    }
		  else
		    SetMultiExposure (dev, Regs);

		  /* 91e2 */
		  RTS_WriteRegs (dev->usb_handle, Regs);
		  if (myCalib != NULL)
		    Shading_apply (dev, Regs, myscancfg, myCalib);

		  if (dev->motorcfg->changemotorcurrent != FALSE)
		    Motor_Change (dev, Regs,
				  Motor_GetFromResolution (myscancfg->
							   resolution_x));

		  /* mlock = 0 */
		  data_bitset (&Regs[0x00], 0x10, 0);

		  data_wide_bitset (&Regs[0xde], 0xfff, 0);

		  /* release motor */
		  Motor_Release (dev);

		  if (RTS_Warm_Reset (dev) == OK)
		    {
		      rst = OK;

		      SetLock (dev->usb_handle, Regs,
			       (myscancfg->depth == 16) ? FALSE : TRUE);

		      /* set gain control */
		      Lamp_SetGainMode (dev, Regs, myscancfg->resolution_x,
					gaincontrol);

		      /* send registers to scanner */
		      if (RTS_WriteRegs (dev->usb_handle, Regs) == OK)
			{
			  /* execute! */
			  if (RTS_Execute (dev) == OK)
			    RTS_GetImage_Read (dev, buffer, myscancfg, hwdcfg);	/*92e7 */
			}

		      /*92fc */
		      SetLock (dev->usb_handle, Regs, FALSE);

		      if ((options & 0x200) != 0)
			{
			  /* switch on lamp */
			  data_bitset (&Regs[0x146], 0x40, 1);

			  Write_Byte (dev->usb_handle, 0xe946, Regs[0x146]);
			  /* Wait 3 seconds */
			  usleep (1000 * 3000);
			}

		      /*9351 */
		      if (dev->motorcfg->changemotorcurrent == TRUE)
			Motor_Change (dev, dev->init_regs, 3);
		    }

		  /* free low level configuration */
		  free (hwdcfg);
		}

	      /* free scanning configuration */
	      free (myscancfg);
	    }
	}
    }

  DBG (DBG_FNC, "- RTS_GetImage: %i\n", rst);

  return rst;
}

static SANE_Int
Refs_Detect (struct st_device *dev, SANE_Byte * Regs, SANE_Int resolution_x,
	     SANE_Int resolution_y, SANE_Int * x, SANE_Int * y)
{
  SANE_Int rst = ERROR;		/* default */

  DBG (DBG_FNC, "+ Refs_Detect(*Regs, resolution_x=%i, resolution_y=%i):\n",
       resolution_x, resolution_y);

  if ((x != NULL) && (y != NULL))
    {
      SANE_Byte *image;
      struct st_scanparams scancfg;

      *x = *y = 0;		/* default */

      /* set configuration to scan a little area at the top-left corner */
      bzero (&scancfg, sizeof (struct st_scanparams));
      scancfg.depth = 8;
      scancfg.colormode = CM_GRAY;
      scancfg.channel = CL_RED;
      scancfg.resolution_x = resolution_x;
      scancfg.resolution_y = resolution_y;
      scancfg.coord.left = 4;
      scancfg.coord.width = (resolution_x * 3) / 10;
      scancfg.coord.top = 1;
      scancfg.coord.height = (resolution_y * 4) / 10;
      scancfg.shadinglength = (resolution_x * 17) / 2;
      scancfg.bytesperline = scancfg.coord.width;

      /* allocate space to store image */
      image =
	(SANE_Byte *) malloc ((scancfg.coord.height * scancfg.coord.width) *
			      sizeof (SANE_Byte));
      if (image != NULL)
	{
	  struct st_gain_offset gain_offset;
	  SANE_Int gaincontrol, pwmlamplevel_backup, C;

	  gaincontrol = 0;
	  if (RTS_Debug->use_fixed_pwm == FALSE)
	    {
	      /* 3877 */
	      gaincontrol = Lamp_GetGainMode (dev, resolution_x, ST_NORMAL);	/* scan.scantype */
	      pwmlamplevel = 0;
	      Lamp_PWM_use (dev, 1);
	      Lamp_PWM_DutyCycle_Set (dev, (gaincontrol == 0) ? 0x12 : 0x26);

	      /* Enciende flb lamp */
	      Lamp_Status_Set (dev, NULL, TRUE, FLB_LAMP);
	      usleep (1000 * 2000);
	    }

	  /* 38d6 */
	  pwmlamplevel_backup = pwmlamplevel;
	  pwmlamplevel = 0;
	  Lamp_PWM_use (dev, 1);

	  bzero (&gain_offset, sizeof (struct st_gain_offset));
	  for (C = CL_RED; C <= CL_BLUE; C++)
	    {
	      gain_offset.pag[C] = 3;
	      gain_offset.vgag1[C] = 4;
	      gain_offset.vgag2[C] = 4;
	    }

	  /* perform lamp warmup */
	  Lamp_Warmup (dev, Regs, FLB_LAMP, resolution_x);

	  /* retrieve image from scanner */
	  if (RTS_GetImage
	      (dev, Regs, &scancfg, &gain_offset, image, 0, 0x20000000,
	       gaincontrol) == OK)
	    {
	      SANE_Int ser1, ler1;

	      /* same image to disk if required by user */
	      if (RTS_Debug->SaveCalibFile != FALSE)
		{
		  dbg_tiff_save ("pre-autoref.tiff",
				 scancfg.coord.width,
				 scancfg.coord.height,
				 scancfg.depth,
				 CM_GRAY,
				 scancfg.resolution_x,
				 scancfg.resolution_y,
				 image,
				 scancfg.coord.height * scancfg.coord.width);
		}

	      /* calculate reference position */
	      if (Refs_Analyze_Pattern (&scancfg, image, &ler1, 1, &ser1, 0)
		  == OK)
		{
		  *y = scancfg.coord.top + ler1;
		  *x = scancfg.coord.left + ser1;

		  rst = OK;
		}
	    }

	  free (image);

	  pwmlamplevel = pwmlamplevel_backup;
	}

      DBG (DBG_FNC, " -> Detected refs: x=%i , y=%i\n", *x, *y);
    }

  DBG (DBG_FNC, "- Refs_Detect: %i\n", rst);

  return rst;
}

static SANE_Int
Refs_Set (struct st_device *dev, SANE_Byte * Regs,
	  struct st_scanparams *scancfg)
{
  SANE_Int rst;
  SANE_Int y, x;
  struct st_autoref refcfg;

  DBG (DBG_FNC, "+ Refs_Set(*Regs, *scancfg):\n");
  dbg_ScanParams (scancfg);

  rst = OK;

  /* get fixed references for given resolution */
  cfg_vrefs_get (dev->sensorcfg->type, scancfg->resolution_x, &scan.ler,
		 &scan.ser);
  scan.leftleading = scan.ser;
  scan.startpos = scan.ler;

  /* get auto reference configuration */
  cfg_autoref_get (&refcfg);

  if (refcfg.type != REF_NONE)
    {
      /* if reference counter is == 0 perform auto detection */
      if (Refs_Counter_Load (dev) == 0)
	{
	  DBG (DBG_FNC,
	       " -> Refs_Set - Autodetection mandatory (counter == 0)\n");

	  refcfg.type = REF_AUTODETECT;
	}

      switch (refcfg.type)
	{
	case REF_AUTODETECT:
	  /* try to autodetect references scanning a little area */
	  if (Refs_Detect
	      (dev, Regs, refcfg.resolution, refcfg.resolution, &x, &y) == OK)
	    Refs_Save (dev, x, y);
	  else
	    rst = ERROR;

	  Head_ParkHome (dev, TRUE, dev->motorcfg->parkhomemotormove);
	  break;

	case REF_TAKEFROMSCANNER:
	  /* Try to get values from scanner */
	  if (Refs_Load (dev, &x, &y) == ERROR)
	    {
	      if (Refs_Detect
		  (dev, Regs, refcfg.resolution, refcfg.resolution, &x,
		   &y) == OK)
		Refs_Save (dev, x, y);
	      else
		rst = ERROR;

	      Head_ParkHome (dev, TRUE, dev->motorcfg->parkhomemotormove);
	    }
	  break;
	}

      if (rst == OK)
	{
	  /* values are based on resolution given by refcfg.resolution.

	     offset_x and y are based on 2400 dpi so convert values to that dpi
	     before adding offsets and then return to resolution given by user */

	  x *= (2400 / refcfg.resolution);
	  y *= (2400 / refcfg.resolution);

	  scan.leftleading = x;
	  scan.startpos = y;
	  scan.ser = ((x + refcfg.offset_x) * scancfg->resolution_x) / 2400;
	  scan.ler = ((y + refcfg.offset_y) * scancfg->resolution_y) / 2400;

	  DBG (DBG_FNC,
	       " -> After SEROffset and LEROffset, xoffset = %i, yoffset =%i\n",
	       scan.ser, scan.ler);
	}

      /* increase refs counter */
      Refs_Counter_Inc (dev);
    }

  DBG (DBG_FNC, "- Refs_Set: %i\n", rst);

  return rst;
}

static SANE_Int
Lamp_Status_Set (struct st_device *dev, SANE_Byte * Regs, SANE_Int turn_on,
		 SANE_Int lamp)
{
  SANE_Int rst = ERROR;		/* default */
  SANE_Byte freevar = FALSE;

  DBG (DBG_FNC, "+ Lamp_Status_Set(*Regs, turn_on=%i->%s, lamp=%s)\n",
       turn_on,
       ((((lamp - 1) | turn_on) & 1) == 1) ? "Yes" : "No",
       (lamp == FLB_LAMP) ? "FLB_LAMP" : "TMA_LAMP");

  if (Regs == NULL)
    {
      Regs = (SANE_Byte *) malloc (RT_BUFFER_LEN * sizeof (SANE_Byte));

      if (Regs != NULL)
	freevar = TRUE;
    }

  if (Regs != NULL)
    {
      RTS_ReadRegs (dev->usb_handle, Regs);

      /* next op depends on chipset */
      switch (dev->chipset->model)
	{
	case RTS8822BL_03A:
	  /* register 0xe946 has 2 bits and each one referres one lamp
	     0x40: FLB_LAMP | 0x20 : TMA_LAMP
	     if both were enabled both lamps would be switched on */
	  data_bitset (&Regs[0x146], 0x20, ((lamp == TMA_LAMP) && (turn_on == TRUE)) ? 1 : 0);	/* TMA */
	  data_bitset (&Regs[0x146], 0x40, ((lamp == FLB_LAMP) && (turn_on == TRUE)) ? 1 : 0);	/* FLB */

	  data_bitset (&Regs[0x155], 0x10, (lamp != FLB_LAMP) ? 1 : 0);
	  break;
	default:
	  /* the other chipsets only use one bit to indicate when a lamp is
	     switched on or not being bit 0x10 in 0xe955 who decides which lamp
	     is affected */
	  /* switch on lamp? yes if TMA_LAMP, else whatever turn_on says */
	  data_bitset (&Regs[0x146], 0x40, ((lamp - 1) | turn_on));
	  /* what lamp must be switched on? */
	  if ((Regs[0x146] & 0x40) != 0)
	    data_bitset (&Regs[0x155], 0x10, (lamp != FLB_LAMP) ? 1 : 0);
	  break;
	}

      /*42b8cd1 */
      /* switch on/off lamp */
      /*dev->init_regs[0x0146] = (dev->init_regs[0x146] & 0xbf) | (Regs[0x146] & 0x40); */
      dev->init_regs[0x0146] = (dev->init_regs[0x146] & 0x9f) | (Regs[0x146] & 0x60);		/*-xx-----*/

      /* Which lamp */
      dev->init_regs[0x0155] = Regs[0x0155];
      Write_Byte (dev->usb_handle, 0xe946, Regs[0x0146]);
      usleep (1000 * 200);
      Write_Buffer (dev->usb_handle, 0xe954, &Regs[0x0154], 2);
    }

  if (freevar != FALSE)
    {
      free (Regs);
      Regs = NULL;
    }

  DBG (DBG_FNC, "- Lamp_Status_Set: %i\n", rst);

  return rst;
}

static SANE_Int
Get_PAG_Value (SANE_Byte scantype, SANE_Byte color)
{
  SANE_Int rst, iType, iColor;

  switch (scantype)
    {
    case ST_NEG:
      iType = CALIBNEGATIVEFILM;
      break;
    case ST_TA:
      iType = CALIBTRANSPARENT;
      break;
    case ST_NORMAL:
      iType = CALIBREFLECTIVE;
      break;
    default:
      iType = CALIBREFLECTIVE;
      break;
    }

  switch (color)
    {
    case CL_BLUE:
      iColor = PAGB;
      break;
    case CL_GREEN:
      iColor = PAGG;
      break;
    case CL_RED:
      iColor = PAGR;
      break;
    default:
      iColor = PAGR;
      break;
    }

  rst = get_value (iType, iColor, 1, FITCALIBRATE);

  DBG (DBG_FNC, "> Get_PAG_Value(scantype=%s, color=%i): %i\n",
       dbg_scantype (scantype), color, rst);

  return rst;
}

static SANE_Byte
Lamp_GetGainMode (struct st_device *dev, SANE_Int resolution,
		  SANE_Byte scantype)
{
  SANE_Byte ret;
  SANE_Int mygain, iValue;

  switch (scantype)
    {
    case ST_TA:
      ret = 0;
      iValue = DPIGAINCONTROL_TA600;
      break;
    case ST_NEG:
      ret = 1;
      iValue = DPIGAINCONTROL_NEG600;
      break;
    default:			/* Reflective */
      ret = 1;
      iValue = DPIGAINCONTROL600;
      break;
    }

  mygain = get_value (SCAN_PARAM, iValue, ret, usbfile);
  ret = 0;

/*

*/
  if (scantype == ST_NORMAL)
    {
      if (dev->chipset->model == RTS8822L_02A)
	{
	  switch (resolution)
	    {
	    case 100:
	    case 150:
	    case 300:
	    case 600:
	    case 1200:
	    case 2400:
	    case 4800:
	      ret = ((RTS_Debug->usbtype != USB11) && (mygain != 0)) ? 1 : 0;
	      break;
	    }
	}
      else
	{
	  switch (resolution)
	    {
	    case 100:
	    case 200:
	    case 300:
	    case 600:
	      if (RTS_Debug->usbtype != USB11)
		ret = (mygain != 0) ? 1 : 0;
	      else
		ret = (resolution == 100) ? 1 : 0;
	      break;
	    case 1200:
	    case 2400:
	      ret = 0;
	      break;
	    }
	}
    }
  else if (scantype == ST_TA)
    {
      switch (resolution)
	{
	  /*hp3970 */
	case 100:
	case 200:
	  /*common */
	case 300:
	case 600:
	case 1200:
	case 2400:
	  /*hp4370 */
	case 150:
	case 4800:
	  ret = ((RTS_Debug->usbtype != USB11) && (mygain != 0)) ? 1 : 0;
	  break;
	}
    }
  else
    {
      /* ST_NEG */
      switch (resolution)
	{
	case 100:
	case 200:
	case 300:
	case 600:
	  ret = ((RTS_Debug->usbtype != USB11) && (mygain != 0)) ? 1 : 0;
	  break;
	case 1200:
	case 2400:
	case 4800:		/*hp4370 */
	  ret = 0;
	  break;
	}
    }

  DBG (DBG_FNC, "> Lamp_GetGainMode(resolution=%i, scantype=%s): %i\n",
       resolution, dbg_scantype (scantype), ret);

  return ret;
}

static SANE_Int
GetOneLineInfo (struct st_device *dev, SANE_Int resolution,
		SANE_Int * maximus, SANE_Int * minimus, double *average)
{
  SANE_Int rst = ERROR;

  DBG (DBG_FNC,
       "+ GetOneLineInfo(resolution=%i, *maximus, *minimus, *average):\n",
       resolution);

  /* Check parameters */
  if ((maximus != NULL) && (minimus != NULL) && (average != NULL))
    {
      SANE_Byte *Regs, *image;
      SANE_Int a, gainmode;
      struct st_gain_offset gain_offset;
      struct st_scanparams scancfg;

      Regs = (SANE_Byte *) malloc (RT_BUFFER_LEN * sizeof (SANE_Byte));
      if (Regs != NULL)
	{
	  /* Copy scanner registers */
	  memcpy (Regs, dev->init_regs, RT_BUFFER_LEN * sizeof (SANE_Byte));

	  /* Setting some registers */
	  for (a = 0x192; a <= 0x19d; a++)
	    Regs[a] = 0;

	  /* Create calibration table */
	  for (a = CL_RED; a <= CL_BLUE; a++)
	    {
	      gain_offset.edcg1[a] = 256;
	      gain_offset.edcg2[a] = 0;
	      gain_offset.odcg1[a] = 256;
	      gain_offset.odcg2[a] = 0;
	      gain_offset.vgag1[a] = 4;
	      gain_offset.vgag2[a] = 4;
	      gain_offset.pag[a] = Get_PAG_Value (scan.scantype, a);
	    }

	  RTS_GetScanmode (dev, scantype, 0, resolution);

	  /* Setting scanning params */
	  memset (&scancfg, 0, sizeof (struct st_scanparams));
	  scancfg.colormode = CM_COLOR;
	  scancfg.resolution_x = resolution;
	  scancfg.resolution_y = resolution;
	  scancfg.coord.left = 100;
	  scancfg.coord.width = (resolution * 8.5) - 100;
	  scancfg.coord.top = 1;
	  scancfg.coord.height = 1;
	  scancfg.depth = 8;
	  scancfg.shadinglength = resolution * 8.5;
	  scancfg.v157c = scancfg.coord.width * 3;
	  scancfg.bytesperline = scancfg.v157c;

	  /* Reserve buffer for line */
	  image =
	    (SANE_Byte *) malloc (((scancfg.coord.width * 0x21) * 3) *
				  sizeof (SANE_Byte));
	  if (image != NULL)
	    {
	      gainmode =
		Lamp_GetGainMode (dev, resolution & 0xffff, scan.scantype);
	      if (RTS_GetImage
		  (dev, Regs, &scancfg, &gain_offset, image, 0,
		   OP_STATIC_HEAD, gainmode) != ERROR)
		{
		  /* Read all image to take max min and average colours */
		  SANE_Byte *pointer1 = image;
		  SANE_Byte *pointer2;
		  SANE_Byte *pointer3;
		  SANE_Int cmin[3];	/* min values */
		  SANE_Int cmax[3];	/* max values */
		  double cave[3];	/* average values */
		  SANE_Int mysize;

		  if (scancfg.colormode != CM_GRAY)
		    {
		      pointer2 = image;
		      pointer3 = image;
		    }
		  else
		    {
		      pointer2 = image + 1;
		      pointer3 = image + 2;
		    }

		  for (a = CL_RED; a <= CL_BLUE; a++)
		    {
		      cmin[a] = 255;
		      cmax[a] = 0;
		      cave[a] = 0;
		    }

		  if (scancfg.coord.height > 0)
		    {
		      SANE_Int y, x;
		      SANE_Byte *mypointer;
		      SANE_Byte color;
		      SANE_Int desp[3];

		      desp[CL_RED] = pointer1 - pointer3;
		      desp[CL_GREEN] = pointer2 - pointer3;
		      desp[CL_BLUE] = 0;

		      for (y = 0; y < scancfg.coord.height; y++)
			{
			  if (scancfg.coord.width > 0)
			    {
			      mypointer = pointer3;

			      for (x = 0; x < scancfg.coord.width; x++)
				{
				  for (a = CL_RED; a <= CL_BLUE; a++)
				    {
				      /* Take colour values */
				      color = *(mypointer + desp[a]);

				      /* Take max values for each color */
				      cmax[a] = max (cmax[a], color);

				      /* Take min values for each color */
				      cmin[a] = min (cmin[a], color);

				      /* Average */
				      cave[a] += color;
				    }

				  mypointer += 3;
				}
			    }

			  /* point to the pixel that is below */
			  pointer1 += scancfg.coord.width * 3;
			  pointer2 += scancfg.coord.width * 3;
			  pointer3 += scancfg.coord.width * 3;
			}
		    }

		  mysize = scancfg.coord.height * scancfg.coord.width;
		  if (mysize < 1)
		    mysize = 1;

		  for (a = CL_RED; a <= CL_BLUE; a++)
		    {
		      maximus[a] = cmax[a];
		      minimus[a] = cmin[a];
		      average[a] = cave[a] / mysize;
		    }

		  DBG (DBG_FNC, " -> GetOneLineInfo: max r=%3i g=%3i b=%3i\n",
		       maximus[CL_RED], maximus[CL_GREEN], maximus[CL_BLUE]);
		  DBG (DBG_FNC, " ->                 min r=%3i g=%3i b=%3i\n",
		       minimus[CL_RED], minimus[CL_GREEN], minimus[CL_BLUE]);
		  DBG (DBG_FNC,
		       " ->                 avg r=%3.0f g=%3.0f b=%3.0f\n",
		       average[CL_RED], average[CL_GREEN], average[CL_BLUE]);

		  rst = OK;
		}

	      free (image);
	    }

	  free (Regs);
	}
    }

  DBG (DBG_FNC, "- GetOneLineInfo: %i\n", rst);

  return OK;
}

static SANE_Int
Lamp_PWM_CheckStable (struct st_device *dev, SANE_Int resolution,
		      SANE_Int lamp)
{
  struct st_checkstable check;
  SANE_Int rst;

  DBG (DBG_FNC, "+ Lamp_PWM_CheckStable(resolution=%i, lamp=%i):\n",
       resolution, lamp);

  rst = cfg_checkstable_get (lamp, &check);

  if (rst == OK)
    {
      SANE_Int maximus[3] = { 0 };
      SANE_Int minimus[3] = { 0 };
      double average[3] = { 0 };
      SANE_Int maxbigger;
      SANE_Int last_colour = 0;

      double diff = check.diff * 0.01;
      long tottime = GetTickCount () + check.tottime;

      while (GetTickCount () <= tottime)
	{
	  rst = GetOneLineInfo (dev, resolution, maximus, minimus, average);
	  if (rst == OK)
	    {
	      /* Takes maximal colour value */
	      maxbigger =
		max (maximus[CL_GREEN],
		     max (maximus[CL_BLUE], maximus[CL_RED]));

	      /*breaks when colour intensity increases 'diff' or lower */
	      if (abs (maxbigger - last_colour) < diff)
		{
		  DBG (DBG_FNC, " -> PWM is ready\n");
		  break;
		}

	      last_colour = maxbigger;
	    }

	  usleep (1000 * check.interval);
	}

    }

  DBG (DBG_FNC, "- Lamp_PWM_CheckStable: %i\n", rst);

  return OK;
}

static SANE_Byte
Refs_Counter_Load (struct st_device *dev)
{
  SANE_Byte data = 15;

  DBG (DBG_FNC, "+ Refs_Counter_Load:\n");

  /* check if chipset supports accessing eeprom */
  if ((dev->chipset->capabilities & CAP_EEPROM) != 0)
    if (RTS_EEPROM_ReadByte (dev->usb_handle, 0x78, &data) != OK)
      data = 15;

  DBG (DBG_FNC, "- Refs_Counter_Load: %i\n", _B0 (data));

  return data;
}

static SANE_Int
Refs_Counter_Save (struct st_device *dev, SANE_Byte data)
{
  SANE_Int rst = OK;

  DBG (DBG_FNC, "+ Refs_Counter_Save(data=%i):\n", data);

  /* check if chipset supports accessing eeprom */
  if ((dev->chipset->capabilities & CAP_EEPROM) != 0)
    {
      if (data > 15)
	data = 15;

      rst = RTS_EEPROM_WriteByte (dev->usb_handle, 0x78, data);
    }

  DBG (DBG_FNC, "- Refs_Counter_Save: %i\n", rst);

  return rst;
}

static SANE_Int
Refs_Counter_Inc (struct st_device *dev)
{
  SANE_Byte data;

  DBG (DBG_FNC, "+ Refs_Counter_Inc:\n");

  data = Refs_Counter_Load (dev) + 1;

  if (data >= 15)
    data = 0;

  Refs_Counter_Save (dev, data);

  DBG (DBG_FNC, "- Refs_Counter_Inc() : Count=%i\n", data);

  return OK;
}

static SANE_Int
Load_StripCoords (SANE_Int scantype, SANE_Int * ypos, SANE_Int * xpos)
{
  SANE_Int iType;

  switch (scantype)
    {
    case 3:
      iType = CALIBNEGATIVEFILM;
      break;
    case 2:
      iType = CALIBTRANSPARENT;
      break;
    default:
      iType = CALIBREFLECTIVE;
      break;
    }

  *xpos = get_value (iType, WSTRIPXPOS, 0, FITCALIBRATE);
  *ypos = get_value (iType, WSTRIPYPOS, 0, FITCALIBRATE);

  DBG (DBG_FNC, "> Load_StripCoords(scantype=%s): ypos=%i, xpos=%i\n",
       dbg_scantype (scantype), *ypos, *xpos);

  return OK;
}

static SANE_Int
Head_Relocate (struct st_device *dev, SANE_Int speed, SANE_Int direction,
	       SANE_Int ypos)
{
  SANE_Int rst;
  SANE_Byte *Regs;

  DBG (DBG_FNC, "+ Head_Relocate(speed=%i, direction=%i, ypos=%i):\n", speed,
       direction, ypos);

  rst = ERROR;

  Regs = (SANE_Byte *) malloc (RT_BUFFER_LEN * sizeof (SANE_Byte));
  if (Regs != NULL)
    {
      struct st_motormove mymotor;
      struct st_motorpos mtrpos;

      bzero (&mymotor, sizeof (struct st_motormove));
      memcpy (Regs, dev->init_regs, RT_BUFFER_LEN * sizeof (SANE_Byte));

      if (speed < dev->motormove_count)
	memcpy (&mymotor, dev->motormove[speed],
		sizeof (struct st_motormove));

      /*83fe */
      mtrpos.coord_y = ypos;
      mtrpos.options =
	MTR_ENABLED | ((direction == MTR_BACKWARD) ? MTR_BACKWARD :
		       MTR_FORWARD);
      mtrpos.v12e448 = 0;
      mtrpos.v12e44c = 1;

      Motor_Move (dev, Regs, &mymotor, &mtrpos);

      /* waits 15 seconds */
      RTS_WaitScanEnd (dev, 15000);

      free (Regs);
      rst = OK;
    }

  DBG (DBG_FNC, "- Head_Relocate: %i\n", rst);

  return rst;
}

static SANE_Int
Calib_CreateFixedBuffers ()
{
  SANE_Byte channel;
  SANE_Int ret;

  DBG (DBG_FNC, "> Calib_CreateFixedBuffers()\n");

  ret = OK;
  channel = 0;

  while ((channel < 3) && (ret == OK))
    {
      /* First table */
      if (fixed_black_shading[channel] == NULL)
	fixed_black_shading[channel] =
	  (USHORT *) malloc (0x7f8 * sizeof (USHORT));

      if (fixed_black_shading[channel] != NULL)
	bzero (fixed_black_shading[channel], 0x7f8 * sizeof (USHORT));
      else
	ret = ERROR;

      /* Second table */
      if (fixed_white_shading[channel] == NULL)
	fixed_white_shading[channel] =
	  (USHORT *) malloc (0x7f8 * sizeof (USHORT));

      if (fixed_white_shading[channel] != NULL)
	bzero (fixed_white_shading[channel], 0x7f8 * sizeof (USHORT));
      else
	ret = ERROR;

      channel++;
    }

  return ret;
}

static SANE_Int
Calib_CreateBuffers (struct st_device *dev, struct st_calibration *buffer,
		     SANE_Int my14b4)
{
  SANE_Int ebp, ret, channel;

  ret = ERROR;
  dev = dev;

  buffer->shadinglength = scan.coord.width;
  ebp = 0x14;

  if (my14b4 != 0)
    {
      /* 673d */
      if (Calib_CreateFixedBuffers () == OK)
	{
	  for (channel = 0; channel < 3; channel++)
	    {
	      buffer->white_shading[channel] = fixed_white_shading[channel];
	      buffer->black_shading[channel] = fixed_black_shading[channel];
	    }
	  ret = OK;
	}
    }
  else
    {
      /* 677f */
      SANE_Int pos;
      channel = 0;
      while ((channel < 3) && (ret == OK))
	{
	  buffer->black_shading[channel] =
	    (USHORT *) malloc (ebp +
			       (buffer->shadinglength * sizeof (USHORT)));
	  buffer->white_shading[channel] =
	    (USHORT *) malloc (ebp +
			       (buffer->shadinglength * sizeof (USHORT)));
	  if ((buffer->black_shading[channel] != NULL)
	      && (buffer->white_shading[channel] != NULL))
	    {
	      for (pos = 0; pos < buffer->shadinglength; pos++)
		{
		  buffer->black_shading[channel][pos] = 0x00;
		  buffer->white_shading[channel][pos] = 0x4000;
		}
	      ret = OK;
	    }
	  else
	    Calib_FreeBuffers (buffer);

	  channel++;
	}
    }

  DBG (DBG_FNC, "> Calib_CreateBuffers: *buffer, my14b4=%i): %i\n", my14b4,
       ret);

  return ret;
}

static void
Calib_FreeBuffers (struct st_calibration *caltables)
{
  DBG (DBG_FNC, "> Calib_FreeBuffers(*caltables)\n");

  if (caltables != NULL)
    {
      SANE_Int channel;

      for (channel = 0; channel < 3; channel++)
	{
	  if (caltables->black_shading[channel] != NULL)
	    {
	      free (caltables->black_shading[channel]);
	      caltables->black_shading[channel] = NULL;
	    }

	  if (caltables->white_shading[channel] != NULL)
	    {
	      free (caltables->white_shading[channel]);
	      caltables->white_shading[channel] = NULL;
	    }
	}
    }
}

static SANE_Int
Calib_LoadConfig (struct st_device *dev,
		  struct st_calibration_config *calibcfg, SANE_Int scantype,
		  SANE_Int resolution, SANE_Int bitmode)
{
  SANE_Int section, a;
  struct st_autoref refcfg;

  DBG (DBG_FNC,
       "> Calib_LoadConfig(*calibcfg, scantype=%s, resolution=%i, bitmode=%i)\n",
       dbg_scantype (scantype), resolution, bitmode);

  switch (scantype)
    {
    case ST_NEG:
      section = CALIBNEGATIVEFILM;
      break;
    case ST_TA:
      section = CALIBTRANSPARENT;
      break;
    default:
      section = CALIBREFLECTIVE;
      break;
    }

  calibcfg->WStripXPos = get_value (section, WSTRIPXPOS, 0, FITCALIBRATE);
  calibcfg->WStripYPos = get_value (section, WSTRIPYPOS, 0, FITCALIBRATE);
  calibcfg->BStripXPos = get_value (section, BSTRIPXPOS, 0, FITCALIBRATE);
  calibcfg->BStripYPos = get_value (section, WSTRIPYPOS, 0, FITCALIBRATE);

  /* get calibration wrefs */
  cfg_wrefs_get (dev->sensorcfg->type, bitmode, resolution, scantype,
		 &calibcfg->WRef[CL_RED], &calibcfg->WRef[CL_GREEN],
		 &calibcfg->WRef[CL_BLUE]);

  /* 4913 */

  for (a = CL_RED; a <= CL_BLUE; a++)
    {
      WRef[a] = _B0 (calibcfg->WRef[a]);

      calibcfg->BRef[a] = get_value (section, BREFR + a, 10, FITCALIBRATE);
      calibcfg->OffsetEven1[a] =
	get_value (section, OFFSETEVEN1R + a, 256, FITCALIBRATE);
      calibcfg->OffsetEven2[a] =
	get_value (section, OFFSETEVEN2R + a, 0, FITCALIBRATE);
      calibcfg->OffsetOdd1[a] =
	get_value (section, OFFSETODD1R + a, 256, FITCALIBRATE);
      calibcfg->OffsetOdd2[a] =
	get_value (section, OFFSETODD2R + a, 0, FITCALIBRATE);
    }

  calibcfg->RefBitDepth =
    _B0 (get_value (section, REFBITDEPTH, 8, FITCALIBRATE));
  calibcfg->CalibOffset10n =
    _B0 (get_value (section, CALIBOFFSET10N, 3, FITCALIBRATE));
  calibcfg->CalibOffset20n =
    _B0 (get_value (section, CALIBOFFSET20N, 0, FITCALIBRATE));
  calibcfg->OffsetHeight =
    get_value (section, OFFSETHEIGHT, 10, FITCALIBRATE);

  /* 4ae9 */

  /* get left coordinate and length to calibrate offset */
  cfg_offset_get (dev->sensorcfg->type, resolution, scantype,
		  &calibcfg->OffsetPixelStart, &calibcfg->OffsetNPixel);

  /*4c49 */
  calibcfg->OffsetNSigma = get_value (section, OFFSETNSIGMA, 2, FITCALIBRATE);
  calibcfg->OffsetTargetMax =
    get_value (section, OFFSETTARGETMAX, 0x32, FITCALIBRATE) * 0.01;
  calibcfg->OffsetTargetMin =
    get_value (section, OFFSETTARGETMIN, 2, FITCALIBRATE) * 0.01;
  calibcfg->OffsetBoundaryRatio1 =
    get_value (section, OFFSETBOUNDARYRATIO1, 0x64, FITCALIBRATE) * 0.01;
  calibcfg->OffsetBoundaryRatio2 =
    get_value (section, OFFSETBOUNDARYRATIO2, 0x64, FITCALIBRATE) * 0.01;

  calibcfg->OffsetAvgRatio1 =
    get_value (section, OFFSETAVGRATIO1, 0x64, FITCALIBRATE) * 0.01;
  calibcfg->OffsetAvgRatio2 =
    get_value (section, OFFSETAVGRATIO2, 0x64, FITCALIBRATE) * 0.01;
  calibcfg->AdcOffQuickWay =
    get_value (section, ADCOFFQUICKWAY, 1, FITCALIBRATE);
  calibcfg->AdcOffPredictStart =
    get_value (section, ADCOFFPREDICTSTART, 0xc8, FITCALIBRATE);
  calibcfg->AdcOffPredictEnd =
    get_value (section, ADCOFFPREDICTEND, 0x1f4, FITCALIBRATE);
  calibcfg->AdcOffEvenOdd =
    get_value (section, ADCOFFEVENODD, 1, FITCALIBRATE);
  calibcfg->OffsetTuneStep1 =
    _B0 (get_value (section, OFFSETTUNESTEP1, 1, FITCALIBRATE));
  calibcfg->OffsetTuneStep2 =
    _B0 (get_value (section, OFFSETTUNESTEP2, 1, FITCALIBRATE));
  calibcfg->CalibGain10n = get_value (section, CALIBGAIN10N, 1, FITCALIBRATE);
  calibcfg->CalibGain20n = get_value (section, CALIBGAIN20N, 0, FITCALIBRATE);
  calibcfg->CalibPAGOn = get_value (section, CALIBPAGON, 0, FITCALIBRATE);

  for (a = CL_RED; a <= CL_BLUE; a++)
    {
      calibcfg->OffsetAvgTarget[a] =
	_B0 (get_value (section, OFFSETAVGTARGETR + a, 0x0d, FITCALIBRATE));
      calibcfg->PAG[a] = get_value (section, PAGR + a, 3, FITCALIBRATE);
      calibcfg->Gain1[a] = get_value (section, GAIN1R + a, 4, FITCALIBRATE);
      calibcfg->Gain2[a] = get_value (section, GAIN2R + a, 4, FITCALIBRATE);
      calibcfg->WShadingPreDiff[a] =
	get_value (section, WSHADINGPREDIFFR + a, -1, FITCALIBRATE);
      calibcfg->BShadingPreDiff[a] =
	get_value (section, BSHADINGPREDIFFR + a, 2, FITCALIBRATE);
    }

  calibcfg->GainHeight = get_value (section, GAINHEIGHT, 0x1e, FITCALIBRATE);
  calibcfg->GainTargetFactor =
    get_value (section, GAINTARGETFACTOR, 0x5a, FITCALIBRATE) * 0.01;
  calibcfg->TotShading = get_value (section, TOTSHADING, 0, FITCALIBRATE);

  /* White shading */
  calibcfg->WShadingOn = get_value (section, WSHADINGON, 3, FITCALIBRATE);
  calibcfg->WShadingHeight =
    get_value (section, WSHADINGHEIGHT, 0x18, FITCALIBRATE);

  /* Black shading */
  calibcfg->BShadingOn = get_value (section, BSHADINGON, 2, FITCALIBRATE);
  calibcfg->BShadingHeight =
    get_value (section, BSHADINGHEIGHT, 0x1e, FITCALIBRATE);

  calibcfg->BShadingDefCutOff =
    get_value (section, BSHADINGDEFCUTOFF, 0, FITCALIBRATE);

  refcfg.extern_boundary = 0;
  cfg_autoref_get (&refcfg);
  calibcfg->ExternBoundary = refcfg.extern_boundary * 0.01;

  calibcfg->EffectivePixel =
    cfg_effectivepixel_get (dev->sensorcfg->type, resolution);

  return OK;
}

static SANE_Int
Calib_AdcGain (struct st_device *dev, struct st_calibration_config *calibcfg,
	       SANE_Int arg2, SANE_Int gaincontrol)
{
  /*
     0606F8E0   04F60738  |Arg1 = 04F60738
     0606F8E4   0606F90C  |Arg2 = 0606F90C calibcfg
     0606F8E8   00000001  |Arg3 = 00000001 arg2
     0606F8EC   00000001  \Arg4 = 00000001 gaincontrol
   */

  SANE_Int rst = ERROR;
  SANE_Byte *myRegs;		/*f1c0 */

  DBG (DBG_FNC, "+ Calib_AdcGain(*calibcfg, arg2=%i, gaincontrol=%i)\n", arg2,
       gaincontrol);

  myRegs = (SANE_Byte *) malloc (sizeof (SANE_Byte) * RT_BUFFER_LEN);
  if (myRegs != NULL)
    {
      struct st_scanparams *scancfg;	/*f17c */
      SANE_Int bytes_per_line, bytes_to_next_colour, bytes_per_pixel;

      /* get register values to perform adc gain calibration */
      memcpy (myRegs, &calibdata->Regs, sizeof (SANE_Byte) * RT_BUFFER_LEN);

      scancfg =
	(struct st_scanparams *) malloc (sizeof (struct st_scanparams));
      if (scancfg != NULL)
	{
	  SANE_Byte *image, *pgain, *pcalgain;

	  /* get proper scan configuration */
	  memcpy (scancfg, &calibdata->scancfg,
		  sizeof (struct st_scanparams));

	  /* set gain control type */
	  Lamp_SetGainMode (dev, myRegs, scancfg->resolution_x, gaincontrol);

	  /* 8-bit depth */
	  scancfg->depth = 8;

	  /* set coordinates */
	  if ((scan.scantype > 0) && (scan.scantype < 4))
	    scancfg->coord.left += scan.ser;

	  if ((scancfg->coord.width & 1) == 0)
	    scancfg->coord.width++;

	  scancfg->coord.top = 1;
	  scancfg->coord.height = calibcfg->OffsetHeight;

	  /* three more values to read image data after getting image from scanner */
	  switch (scancfg->colormode)
	    {
	    case CM_GRAY:
	    case CM_LINEART:
	      bytes_to_next_colour = 0;
	      bytes_per_pixel = 1;
	      bytes_per_line = scancfg->coord.width;
	      break;
	    default:		/* CM_COLOR */
	      /* c027 */
	      bytes_to_next_colour = 1;
	      bytes_per_line = scancfg->coord.width * 3;
	      if (scancfg->samplerate == LINE_RATE)
		{
		  bytes_to_next_colour = scancfg->coord.width;
		  bytes_per_pixel = 1;
		}
	      else
		bytes_per_pixel = 3;
	      break;
	    }

	  /*7fc7 */
	  scancfg->v157c = bytes_per_line;
	  scancfg->bytesperline = bytes_per_line;

	  /* select type of gain parameters to set */
	  if (arg2 != 0)
	    {
	      pgain = calibdata->gain_offset.vgag1;
	      pcalgain = calibcfg->Gain1;
	    }
	  else
	    {
	      /*7ff2 */
	      pgain = calibdata->gain_offset.vgag2;
	      pcalgain = calibcfg->Gain2;
	    }

	  /*8002 */
	  /* Allocate space for image  | size = 132912 */
	  image =
	    (SANE_Byte *) malloc (sizeof (SANE_Byte) *
				  ((scancfg->coord.height +
				    16) * bytes_per_line));
	  if (image != NULL)
	    {
	      /* Lets read image */
	      if (RTS_GetImage
		  (dev, myRegs, scancfg, &calibdata->gain_offset, image, NULL,
		   OP_STATIC_HEAD, gaincontrol) == OK)
		{
		  SANE_Int a;
		  SANE_Int vmin[3], vmax[3];
		  double dval[3] = { 0.0 };	/*f1a8 f1b0 f1b8 */
		  SANE_Byte *pimage = image;

		  /* initialize values */
		  for (a = CL_RED; a <= CL_BLUE; a++)
		    {
		      calibcfg->unk1[a] = 0;
		      calibcfg->unk2[a] = 0xff;

		      vmin[a] = 0xff;
		      vmax[a] = 0;
		    }

		  /* process image data */
		  if (scancfg->coord.width > 0)
		    {
		      /*8104 */
		      SANE_Int pos, myheight /*f164 */ ;
		      SANE_Int chn_sum[3];

		      for (pos = scancfg->coord.width; pos > 0; pos--)
			{
			  chn_sum[CL_RED] = chn_sum[CL_GREEN] =
			    chn_sum[CL_BLUE] = 0;

			  if (scancfg->coord.height > 0)
			    for (myheight = 0;
				 myheight < scancfg->coord.height; myheight++)
			      for (a = CL_RED; a <= CL_BLUE; a++)
				chn_sum[a] +=
				  *(pimage + (bytes_per_line * myheight) +
				    (bytes_to_next_colour * a));

			  /*816e */
			  for (a = CL_RED; a <= CL_BLUE; a++)
			    {
			      vmin[a] =
				min (vmin[a],
				     chn_sum[a] / scancfg->coord.height);
			      vmax[a] =
				max (vmax[a],
				     chn_sum[a] / scancfg->coord.height);

			      calibcfg->unk1[a] =
				max (calibcfg->unk1[a], vmax[a]);
			      calibcfg->unk2[a] =
				min (calibcfg->unk1[a], vmin[a]);

			      dval[a] += vmax[a] & 0xffff;
			    }

			  pimage += bytes_per_pixel;
			}
		    }

		  /*82b0 */
		  dval[CL_RED] /= scancfg->coord.width;
		  dval[CL_GREEN] /= scancfg->coord.width;
		  dval[CL_BLUE] /= scancfg->coord.width;

		  DBG (DBG_FNC, " -> adcgain (av/l): r=%f, g=%f, b=%f\n",
		       dval[CL_RED], dval[CL_GREEN], dval[CL_BLUE]);
		  DBG (DBG_FNC, " ->         (max ): R=%i, G=%i, B=%i\n",
		       calibcfg->unk1[CL_RED], calibcfg->unk1[CL_GREEN],
		       calibcfg->unk1[CL_BLUE]);
		  DBG (DBG_FNC, " ->         (min ): r=%i, g=%i, b=%i\n",
		       calibcfg->unk2[CL_RED], calibcfg->unk2[CL_GREEN],
		       calibcfg->unk2[CL_BLUE]);

		  if (scancfg->colormode == CM_COLOR)
		    {
		      /*8353 */
		      double dvalue;
		      SANE_Int ival;

		      for (a = CL_RED; a <= CL_BLUE; a++)
			{
			  dvalue =
			    ((((calibcfg->WRef[a] * (1 << scancfg->depth)) *
			       calibcfg->GainTargetFactor) * 0.00390625) /
			     dval[a]) * ((44 - pgain[a]) / 40);
			  if (dvalue > 0.9090909090909091)
			    {
			      /*83d7 */
			      dvalue = min (44 - (40 / dvalue), 31);
			      ival = dvalue;
			      pgain[a] = _B0 (ival);
			      pcalgain[a] = _B0 (ival);
			    }
			  else
			    {
			      pgain[a] = 0;
			      pcalgain[a] = 0;
			    }
			}
		    }
		  else
		    {
		      /*843c */
		      /*falta codigo */
		      double dvalue;
		      SANE_Int ival;

		      dvalue =
			((44 -
			  pgain[CL_RED]) / 40) * ((((1 << scancfg->depth) *
						    calibcfg->WRef[scancfg->
								   channel]) *
						   0.9) * 0.00390625) /
			17.08509389671362;

		      for (a = CL_RED; a <= CL_BLUE; a++)
			{
			  if (dvalue > 0.9090909090909091)
			    {
			      dvalue = min (44 - (40 / dvalue), 31);
			      ival = dvalue;
			      pgain[a] = _B0 (ival);
			      pcalgain[a] = _B0 (ival);
			    }
			  else
			    {
			      /*84e3 */
			      pgain[a] = 0;
			      pcalgain[a] = 0;
			    }
			}
		    }

		  /*84fa */
		  /* Save buffer */
		  if (RTS_Debug->SaveCalibFile != FALSE)
		    {
		      dbg_tiff_save ("adcgain.tiff",
				     scancfg->coord.width,
				     scancfg->coord.height,
				     scancfg->depth,
				     CM_COLOR,
				     scancfg->resolution_x,
				     scancfg->resolution_y,
				     image,
				     (scancfg->coord.height +
				      16) * bytes_per_line);
		    }

		  /* check if peak values are above offset average target + 5 */
		  for (a = CL_RED; a <= CL_BLUE; a++)
		    if (calibcfg->unk1[a] >= calibcfg->OffsetAvgTarget[a] + 5)
		      {
			rst = OK;
			break;
		      }
		}

	      free (image);
	    }

	  free (scancfg);
	}

      free (myRegs);
    }

  /* v14b8 = (rst == OK)? 0: 1; */

  /* show */
  dbg_calibtable (&calibdata->gain_offset);

  DBG (DBG_FNC, "- Calib_AdcGain: %i\n", rst);

  return rst;
}

static SANE_Int
GainOffset_Save (struct st_device *dev, SANE_Int * offset, SANE_Byte * gain)
{
  SANE_Int rst = OK;

  DBG (DBG_FNC, "+ GainOffset_Save(*offset, *gain):\n");

  /* check if chipset supports accessing eeprom */
  if ((dev->chipset->capabilities & CAP_EEPROM) != 0)
    {
      if ((offset != NULL) && (gain != NULL))
	{
	  SANE_Int a, crc, value;

	  crc = 0x5B;
	  for (a = CL_RED; a <= CL_BLUE; a++)
	    {
	      value = (*gain << 9) | *offset;
	      crc = _B0 (abs (crc - _B0 (value)));
	      rst =
		RTS_EEPROM_WriteWord (dev->usb_handle, 0x70 + (a * 2), value);
	    }

	  if (rst == OK)
	    rst = RTS_EEPROM_WriteByte (dev->usb_handle, 0x76, crc);
	}
      else
	rst = ERROR;
    }

  DBG (DBG_FNC, "- GainOffset_Save: %i\n", rst);

  return rst;
}

static SANE_Int
Calib_PAGain (struct st_device *dev, struct st_calibration_config *calibcfg,
	      SANE_Int gainmode)
{
  SANE_Byte *Regs;
  struct st_scanparams *scancfg;
  SANE_Int channel_size;
  SANE_Int bytes_to_next_colour = 0;
  SANE_Int bytes_per_pixel = 0;
  SANE_Int length = 0;
  SANE_Byte *image;
  double rst;
  SANE_Int ret = ERROR;

  DBG (DBG_FNC, "+ Calib_PAGain(*calibcfg, gainmode=%i)\n", gainmode);

  Regs = (SANE_Byte *) malloc (RT_BUFFER_LEN * sizeof (SANE_Byte));
  if (Regs != NULL)
    {
      scancfg =
	(struct st_scanparams *) malloc (sizeof (struct st_scanparams));
      if (scancfg != NULL)
	{
	  memcpy (Regs, &calibdata->Regs, RT_BUFFER_LEN * sizeof (SANE_Byte));
	  memcpy (scancfg, &calibdata->scancfg,
		  sizeof (struct st_scanparams));

	  if (scan.scantype == ST_NORMAL)
	    {
	      /* bfa5 */
	      scancfg->coord.left = scan.ser;
	      scancfg->coord.width = (scancfg->sensorresolution * 17) / 2;
	    }
	  else
	    {
	      scancfg->coord.left = scan.ser + v0750;
	      scancfg->coord.width = (scancfg->sensorresolution * 3) / 2;
	    }

	  /* bfca */
	  if ((scancfg->coord.width & 1) == 1)
	    scancfg->coord.width++;

	  scancfg->coord.top = 1;
	  scancfg->coord.height = calibcfg->OffsetHeight;

	  channel_size = (scancfg->depth > 8) ? 2 : 1;

	  switch (scancfg->colormode)
	    {
	    case CM_GRAY:
	    case CM_LINEART:
	      bytes_to_next_colour = 0;
	      bytes_per_pixel = 1;
	      length = channel_size * scancfg->coord.width;
	      break;
	    default:		/* CM_COLOR */
	      /* c027 */
	      bytes_to_next_colour = 1;
	      length = (channel_size * scancfg->coord.width) * 3;
	      if (scancfg->samplerate == LINE_RATE)
		{
		  bytes_to_next_colour = scancfg->coord.width;
		  bytes_per_pixel = 1;
		}
	      else
		bytes_per_pixel = 3;
	      break;
	    }

	  /* c070 */
	  scancfg->v157c = length;

	  image =
	    (SANE_Byte *) malloc ((scancfg->coord.height * length) *
				  sizeof (SANE_Byte));
	  if (image != NULL)
	    {
	      ret =
		RTS_GetImage (dev, Regs, scancfg, &calibdata->gain_offset,
			      image, 0, OP_STATIC_HEAD, gainmode);
	      if (ret == OK)
		{
		  /* 429c105 */
		  SANE_Int a;
		  SANE_Byte *ptr[3];
		  SANE_Int vmin[3] = { 255, 255, 255 };	/* f16c|f16e|f170 */
		  SANE_Int vmax[3] = { 0, 0, 0 };	/* f164|f166|f168 */
		  SANE_Int total[3];

		  ptr[CL_RED] = image;
		  ptr[CL_GREEN] = image + bytes_to_next_colour;
		  ptr[CL_BLUE] = image + (bytes_to_next_colour * 2);

		  if (scancfg->coord.width > 0)
		    {
		      SANE_Int pos, b;

		      for (pos = 0; pos < scancfg->coord.width; pos++)
			{
			  total[CL_BLUE] = 0;
			  total[CL_GREEN] = 0;
			  total[CL_RED] = 0;

			  for (a = 0; a < scancfg->coord.height; a++)
			    {
			      for (b = CL_RED; b <= CL_BLUE; b++)
				total[b] +=
				  *(ptr[b] +
				    ((scancfg->coord.height - a) * length));
			    }

			  /* c1a5 */
			  for (a = CL_RED; a <= CL_BLUE; a++)
			    {
			      total[a] /= scancfg->coord.height;
			      vmin[a] = min (vmin[a], total[a]);
			      vmax[a] = max (vmax[a], total[a]);

			      ptr[a] += bytes_per_pixel;
			    }
			}
		    }

		  /* 429c234 */
		  for (a = CL_RED; a <= CL_BLUE; a++)
		    {
		      rst =
			(calibcfg->WRef[a] * calibcfg->GainTargetFactor) /
			vmax[a];
		      if (rst <= 1.5)
			{
			  if (rst <= 1.286)
			    {
			      if (rst <= 1.125)
				calibdata->gain_offset.pag[a] = 0;
			      else
				calibdata->gain_offset.pag[a] = 1;
			    }
			  else
			    calibdata->gain_offset.pag[a] = 2;
			}
		      else
			calibdata->gain_offset.pag[a] = 3;
		    }
		}
	      free (image);
	    }
	  free (scancfg);
	}
      free (Regs);
    }

  DBG (DBG_FNC, "- Calib_PAGain: %i\n", ret);

  return ret;
}

static SANE_Int
Chipset_ID (struct st_device *dev)
{
  SANE_Int ret;

  if (Read_Word (dev->usb_handle, 0xfe3c, &ret) == OK)
    ret = _B0 (ret);
  else
    ret = 0;

  DBG (DBG_FNC, "> Chipset_ID(): %i\n", ret);

  return ret;
}

static SANE_Int
Chipset_Name (struct st_device *dev, char *name, SANE_Int size)
{
  SANE_Int rst = ERROR;

  if (name != NULL)
    {
      strncpy (name, dev->chipset->name, size);
      rst = OK;
    }

  return rst;
}

static SANE_Int
Refs_Load (struct st_device *dev, SANE_Int * x, SANE_Int * y)
{
  SANE_Int ret = OK;

  DBG (DBG_FNC, "+ Refs_Load:\n");

  *y = *x = 0;

  /* check if chipset supports accessing eeprom */
  if ((dev->chipset->capabilities & CAP_EEPROM) != 0)
    {
      SANE_Int data;

      ret = ERROR;

      if (RTS_EEPROM_ReadWord (dev->usb_handle, 0x6a, &data) == OK)
	{
	  *x = data;
	  if (RTS_EEPROM_ReadWord (dev->usb_handle, 0x6c, &data) == OK)
	    {
	      *y = data;
	      if (RTS_EEPROM_ReadWord (dev->usb_handle, 0x6e, &data) == OK)
		{
		  if ((_B0 (*y + *x + data)) == 0x5a)
		    ret = OK;
		}
	    }
	}
    }

  DBG (DBG_FNC, "- Refs_Load(y=%i, x=%i) : %i\n", *y, *x, ret);

  return ret;
}

static SANE_Int
Refs_Save (struct st_device *dev, SANE_Int left_leading, SANE_Int start_pos)
{
  SANE_Int ret = OK;

  DBG (DBG_FNC, "+ Refs_Save(left_leading=%i, start_pos=%i)\n", left_leading,
       start_pos);

  /* check if chipset supports accessing eeprom */
  if ((dev->chipset->capabilities & CAP_EEPROM) != 0)
    {
      ret = ERROR;

      if (RTS_EEPROM_WriteWord (dev->usb_handle, 0x6a, left_leading) == OK)
	{
	  if (RTS_EEPROM_WriteWord (dev->usb_handle, 0x6c, start_pos) == OK)
	    {
	      SANE_Byte data = _B0 (0x5a - (start_pos + left_leading));
	      ret = RTS_EEPROM_WriteByte (dev->usb_handle, 0x6e, data);
	    }
	}
    }

  DBG (DBG_FNC, "- Refs_Save: %i\n", ret);

  return ret;
}

static SANE_Int
Calib_AdcOffsetRT (struct st_device *dev,
		   struct st_calibration_config *calibcfg, SANE_Int value)
{
/*
05EFF8E4   04F10738  |Arg1 = 04F10738
05EFF8E8   05EFF90C  |Arg2 = 05EFF90C calibcfg
05EFF8EC   00000001  \Arg3 = 00000001 value
*/
  SANE_Byte Regs[RT_BUFFER_LEN];	/*f1c4 */
  SANE_Int channels_per_dot;	/*f108 */
  struct st_scanparams scancfg;	/*f18c */
  SANE_Int *pedcg;		/*f114 */
  SANE_Int *podcg;		/*f118 */
  SANE_Int *poffseteven;	/*f130 */
  SANE_Int *poffsetodd;		/*f128 */
  SANE_Int channel;
  SANE_Int avgtarget[3];	/*f1b8 f1bc f1c0 */
  SANE_Byte *scanbuffer;	/*f0f8 */
  SANE_Int scan_options;	/*f12c */
  SANE_Int highresolution;	/*f144 */
  double dbValues[3] = { 0, 0, 0 };	/*f148 f14c f150 */
  SANE_Int do_loop;		/*f0ec */
  SANE_Int gainmode;
  SANE_Byte *ptr;		/*f0f4 */
  SANE_Byte *mvgag;		/*f0e4 *//*f10c */
  USHORT wvalues[9];		/*0856 0858 085a 085c 085e 0860 0862 0864 0866 */
  SANE_Int imgcount = 0;
  /* myoffsetnpixel = f120 */
  /* desp f0e8 & f140 */

  DBG (DBG_FNC, "+ Calib_AdcOffsetRT(*calibcfg, value=%i)\n", value);

  memcpy (&Regs, &calibdata->Regs, RT_BUFFER_LEN * sizeof (SANE_Byte));
  memcpy (&scancfg, &calibdata->scancfg, sizeof (struct st_scanparams));

  channels_per_dot = (calibdata->scancfg.colormode == CM_COLOR) ? 3 : 1;

  if (value != 0)
    {
      pedcg = &calibdata->gain_offset.edcg1[CL_RED];
      podcg = &calibdata->gain_offset.odcg1[CL_RED];
      poffseteven = &calibcfg->OffsetEven1[CL_RED];
      poffsetodd = &calibcfg->OffsetOdd1[CL_RED];
    }
  else
    {
      /*c37c */
      pedcg = &calibdata->gain_offset.edcg2[CL_RED];
      podcg = &calibdata->gain_offset.odcg2[CL_RED];
      poffseteven = &calibcfg->OffsetEven2[CL_RED];
      poffsetodd = &calibcfg->OffsetOdd2[CL_RED];
    }

  /*c3a4 */
  scancfg.coord.left = calibcfg->OffsetPixelStart;

  if (channels_per_dot > 0)
    {
      for (channel = 0; channel < channels_per_dot; channel++)
	{
	  avgtarget[channel] = calibcfg->OffsetAvgTarget[channel] << 8;
	  if (avgtarget[channel] == 0)
	    avgtarget[channel] = 0x80;
	}
    }

  /* set image coordinates to scan */
  scancfg.coord.width = calibcfg->OffsetNPixel;
  if ((scancfg.coord.width & 1) == 0)
    scancfg.coord.width++;

  scancfg.bytesperline = channels_per_dot * scancfg.coord.width;
  scancfg.coord.top = 1;
  scancfg.coord.height = calibcfg->OffsetHeight;
  scancfg.depth = 8;

  /* allocate memory to store image */
  scanbuffer =
    (SANE_Byte *) malloc ((scancfg.bytesperline * calibcfg->OffsetHeight) *
			  sizeof (SANE_Byte));
  if (scanbuffer == NULL)
    return ERROR;

  /*42ac477 */
  scan_options = (linedarlampoff == 1) ? 1 : 0x101;
  highresolution = (scancfg.sensorresolution >= 1200) ? TRUE : FALSE;

  do
    {
      if (channels_per_dot > 0)
	{
	  for (channel = 0; channel < channels_per_dot; channel++)
	    dbValues[channel] =
	      (40 / (44 - calibdata->gain_offset.vgag2[channel])) * (40 /
								     (44 -
								      calibdata->
								      gain_offset.
								      vgag1
								      [channel]));
	}

      /*429c50f */
      /* Get Image */
      gainmode = Lamp_GetGainMode (dev, scancfg.resolution_x, scan.scantype);
      if (RTS_GetImage
	  (dev, Regs, &scancfg, &calibdata->gain_offset, scanbuffer, 0,
	   scan_options, gainmode) != OK)
	{
	  free (scanbuffer);
	  return ERROR;
	}

      /*429c55f */
      /* Save retrieved image */
      if (RTS_Debug->SaveCalibFile != FALSE)
	{
	  char fname[30];

	  imgcount++;
	  if (snprintf (fname, 30, "adcoffset_rt%i.tiff", imgcount) > 0)
	    dbg_tiff_save (fname,
			   scancfg.coord.width,
			   scancfg.coord.height,
			   scancfg.depth,
			   CM_COLOR,
			   scancfg.resolution_x,
			   scancfg.resolution_y,
			   scanbuffer,
			   scancfg.bytesperline * calibcfg->OffsetHeight);
	}

      /*429c5a5 */
      do_loop = FALSE;

      if (highresolution == TRUE)
	{
	  /* f0fc = f0e4 = channel */
	  SANE_Int lf104;
	  SANE_Int *mydcg;	/*f0f4 */
	  USHORT *mywvalue;	/*ebp */

	  SANE_Byte is_ready[6];	/*f174 f178 f17c f180 f184 f18c */
	  SANE_Byte *ptr_ready;	/*f11c */
	  SANE_Int colour;

	  for (channel = 0; channel < 6; channel++)
	    is_ready[channel] = FALSE;

	  if (channels_per_dot <= 0)
	    break;

	  ptr = scanbuffer;
	  mvgag = (SANE_Byte *) calibdata->gain_offset.vgag1;

	  for (channel = 0; channel < channels_per_dot; channel++)
	    {
	      for (lf104 = 0; lf104 < 2; lf104++)
		{
		  if (lf104 == 0)
		    {
		      mywvalue = &wvalues[channel];
		      mydcg = pedcg;
		      ptr_ready = &is_ready[0];
		    }
		  else
		    {
		      /*1645 */
		      mywvalue = &wvalues[3];
		      mydcg = podcg;
		      ptr_ready = &is_ready[3];
		    }

		  /*1658 */
		  if (ptr_ready[channel] == FALSE)
		    {
		      colour = 0;
		      if (lf104 < calibcfg->OffsetNPixel)
			{
			  SANE_Int dot;

			  for (dot = 0;
			       dot < (calibcfg->OffsetNPixel - lf104 + 1) / 2;
			       dot++)
			    colour +=
			      scanbuffer[mvgag[(lf104 * channels_per_dot)] +
					 (dot * (channels_per_dot * 2))];
			}

		      /*c6b2 */
		      colour = colour << 8;
		      if (colour == 0)
			{
			  /*c6b9 */
			  if (mydcg[channel] != 0x1ff)
			    {
			      /*c6d5 */
			      mydcg[channel] = 0x1ff;
			      do_loop = TRUE;
			    }
			  else
			    ptr_ready[channel] = TRUE;
			}
		      else
			{
			  SANE_Int myesi;
			  SANE_Int d;

			  /*c6e8 */
			  if (*mywvalue == 0)
			    mywvalue += 2;

			  colour /= (calibcfg->OffsetNPixel / 2);
			  if (colour >= avgtarget[channel])
			    {
			      colour -= avgtarget[channel];
			      myesi = 0;
			    }
			  else
			    {
			      colour = avgtarget[channel] - colour;
			      myesi = 1;
			    }

			  d = mydcg[channel];
			  if (d < 0x100)
			    d = 0xff - d;

			  if (myesi != 0)
			    {
			      /*c76e */
			      if ((d + colour) > 0x1ff)
				{
				  if (*mvgag > 0)
				    {
				      *mvgag = *mvgag - 1;
				      do_loop = TRUE;
				    }
				  else
				    ptr_ready[channel] = TRUE;	/*c7a0 */
				}
			      else
				d += colour;
			    }
			  else
			    {
			      /*c7ad */
			      if (colour > d)
				{
				  if (*mvgag > 0)
				    {
				      *mvgag = *mvgag - 1;
				      do_loop = TRUE;
				    }
				  else
				    ptr_ready[channel] = TRUE;
				}
			      else
				d -= colour;
			    }

			  /*c7dd */
			  mydcg[channel] = (d < 0x100) ? 0x100 - d : d;
			}

		      dbg_calibtable (&calibdata->gain_offset);
		    }
		}

	      /*c804 */
	      mvgag++;
	    }
	}
      else
	{
	  /* Low resolution */

	  SANE_Byte is_ready[3];
	  SANE_Int colour;

	  /*429c845 */
	  for (channel = 0; channel < channels_per_dot; channel++)
	    is_ready[channel] = FALSE;

	  if (channels_per_dot <= 0)
	    break;

	  ptr = scanbuffer;
	  mvgag = (SANE_Byte *) calibdata->gain_offset.vgag1;

	  for (channel = 0; channel < channels_per_dot; channel++)
	    {
	      if (is_ready[channel] == FALSE)
		{
		  colour = 0;
		  if (calibcfg->OffsetNPixel > 0)
		    {
		      SANE_Int dot;

		      /* Take one channel colour values from offsetnpixel pixels */
		      for (dot = 0; dot < calibcfg->OffsetNPixel; dot++)
			colour += *(ptr + (dot * channels_per_dot));
		    }

		  colour <<= 8;
		  if (colour == 0)
		    {
		      if (pedcg[channel] != 0x1ff)
			{
			  do_loop = TRUE;
			  podcg[channel] = 0x1ff;
			  pedcg[channel] = 0x1ff;
			}
		      else
			is_ready[channel] = TRUE;
		    }
		  else
		    {
		      /*c8f7 */
		      SANE_Int myesi;
		      SANE_Int op1, op2, op3;

		      /* Get colour average */
		      colour /= calibcfg->OffsetNPixel;

		      /* get absolute difference with avgtarget */
		      myesi = (colour > avgtarget[channel]) ? 0 : 1;
		      colour = abs (avgtarget[channel] - colour);

		      if (scancfg.resolution_x > 600)
			{
			  /*c923 */
			  if (wvalues[channel + 3] == 0)
			    wvalues[channel + 3]++;

			  if (wvalues[channel] == 0)
			    wvalues[channel]++;

			  op3 = max (wvalues[channel], wvalues[channel + 3]);
			}
		      else
			{
			  if (wvalues[channel + 6] == 0)
			    wvalues[channel + 6]++;

			  op3 = wvalues[channel + 6];
			}

		      /*c9d3 */
		      op1 = (SANE_Int) (colour / (dbValues[channel] * op3));
		      op2 =
			(pedcg[channel] <
			 0x100) ? pedcg[channel] - 0xff : pedcg[channel];

		      if (myesi != 0)
			{
			  /*c9f5 */
			  if (((op2 + op1) & 0xffff) > 0x1ff)
			    {
			      if (*mvgag != 0)
				{
				  do_loop = TRUE;
				  *mvgag = *mvgag - 1;
				}
			      else
				is_ready[channel] = TRUE;
			    }
			  else
			    op2 += op1;
			}
		      else
			{
			  /*ca31 */
			  if (op1 > op2)
			    {
			      if (*mvgag > 0)
				{
				  do_loop = TRUE;
				  *mvgag = *mvgag - 1;
				}
			      else
				is_ready[channel] = TRUE;
			    }
			  else
			    op2 -= op1;
			}

		      /*ca54 */
		      if (op2 < 0x100)
			op2 = 0x100 - op2;

		      pedcg[channel] = op2;
		      podcg[channel] = op2;
		    }
		}
	      /*ca6f */
	      ptr++;
	      mvgag++;
	      dbg_calibtable (&calibdata->gain_offset);
	    }
	}
    }
  while (do_loop != FALSE);

  /*429cad1 */
  for (channel = 0; channel < 3; channel++)
    {
      poffseteven[channel] =
	(pedcg[channel] < 0x100) ? 0xff - pedcg[channel] : pedcg[channel];
      poffsetodd[channel] =
	(podcg[channel] < 0x100) ? 0xff - podcg[channel] : podcg[channel];
    }

  free (scanbuffer);

  return OK;
}

static void
Calib_LoadCut (struct st_device *dev, struct st_scanparams *scancfg,
	       SANE_Int scantype, struct st_calibration_config *calibcfg)
{
  double mylong;		/*ee78 */
  double mylong2;
  /**/ SANE_Int channel[3];
  SANE_Int a;

  cfg_shading_cut_get (dev->sensorcfg->type, scancfg->depth,
		       scancfg->resolution_x, scantype, &channel[0],
		       &channel[1], &channel[2]);

  mylong = 1 << scancfg->depth;

  for (a = CL_RED; a <= CL_BLUE; a++)
    {
      mylong2 = channel[a];
      calibcfg->ShadingCut[a] = (mylong * mylong2) * 0.000390625;
    }
}

static SANE_Int
Calib_BWShading (struct st_calibration_config *calibcfg,
		 struct st_calibration *myCalib, SANE_Int gainmode)
{
  /*
     0603F8E4   0603F90C  |Arg1 = 0603F90C calibcfg
     0603F8E8   0603FAAC  |Arg2 = 0603FAAC myCalib
     0603F8EC   00000001  \Arg3 = 00000001 gainmode
   */

  /*falta codigo */

  /*silence gcc */
  calibcfg = calibcfg;
  myCalib = myCalib;
  gainmode = gainmode;

  return OK;
}

static SANE_Int
Calib_WhiteShading_3 (struct st_device *dev,
		      struct st_calibration_config *calibcfg,
		      struct st_calibration *myCalib, SANE_Int gainmode)
{
/*
05EDF8E0   04F00738  |Arg1 = 04F00738
05EDF8E4   05EDF90C  |Arg2 = 05EDF90C calibcfg
05EDF8E8   05EDFAAC  |Arg3 = 05EDFAAC myCalib
05EDF8EC   00000001  \Arg4 = 00000001 gainmode
*/
  SANE_Byte *myRegs;		/*f1bc */
  struct st_scanparams scancfg;	/*f170 */
  SANE_Int myWidth;		/*f14c */
  SANE_Int lf168, bytes_per_pixel;
  SANE_Int bytes_per_line;
  /**/ SANE_Int a;
  double lf1a4[3];
  SANE_Int otherheight;		/*f150 */
  SANE_Int otherheight2;
  SANE_Int lf12c;
  SANE_Int lf130;
  double *buffer1;		/*f138 */
  double *buffer2;		/*f144 */
  SANE_Byte *scanbuffer;	/*f164 */
  SANE_Byte *ptr;		/*f148 */
  SANE_Int position;		/*f140 */
  SANE_Int lf13c, myHeight;
  SANE_Int myESI, myEDI;
  SANE_Int channel;		/*f134 */
  double myst;
  double sumatorio;
  SANE_Int rst;

  DBG (DBG_FNC, "> Calib_WhiteShading3(*calibcfg, *myCalib, gainmode=%i)\n",
       gainmode);

  myRegs = (SANE_Byte *) malloc (RT_BUFFER_LEN * sizeof (SANE_Byte));
  memcpy (myRegs, &calibdata->Regs, RT_BUFFER_LEN * sizeof (SANE_Byte));
  memcpy (&scancfg, &calibdata->scancfg, sizeof (struct st_scanparams));

  Lamp_SetGainMode (dev, myRegs, scancfg.resolution_x, gainmode);

  rst = OK;
  scancfg.resolution_y = 200;
  switch (scan.scantype)
    {
    case ST_NORMAL:
      /*a184 */
      scancfg.coord.left += scan.ser;
      scancfg.coord.width &= 0xffff;
      break;
    case ST_TA:
    case ST_NEG:
      scancfg.coord.left += scan.ser;
      break;
    }

  /*a11b */
  if ((scancfg.coord.width & 1) != 0)
    scancfg.coord.width++;

  scancfg.coord.top = 1;
  scancfg.coord.height = calibcfg->WShadingHeight;

  switch (scancfg.colormode)
    {
    case CM_GRAY:
    case CM_LINEART:
      myWidth = scancfg.coord.width;
      lf168 = 0;
      bytes_per_line = ((scancfg.depth + 7) / 8) * myWidth;
      bytes_per_pixel = 1;
      break;
    default:			/* CM_COLOR */
      myWidth = scancfg.coord.width * 3;
      bytes_per_line = ((scancfg.depth + 7) / 8) * myWidth;
      lf168 = (scancfg.samplerate == LINE_RATE) ? scancfg.coord.width : 1;
      bytes_per_pixel = (scancfg.samplerate == PIXEL_RATE) ? 3 : 1;
      break;
    }

  /*a1e8 */
  scancfg.v157c = bytes_per_line;
  scancfg.bytesperline = bytes_per_line;

  for (a = 0; a < 3; a++)
    lf1a4[a] = (calibcfg->WRef[a] * (1 << scancfg.depth)) >> 8;

  /* debug this code because if it's correct, lf130 and lf12c are always 2 */
  otherheight = calibcfg->WShadingHeight - 3;
  otherheight -= (otherheight - 4);
  otherheight2 = otherheight / 2;
  otherheight -= otherheight2;
  lf130 = otherheight2;
  lf12c = otherheight;

  buffer1 = (double *) malloc (otherheight * sizeof (double));
  if (buffer1 == NULL)
    return ERROR;

  buffer2 = (double *) malloc (otherheight * sizeof (double));
  if (buffer2 == NULL)
    {
      free (buffer1);
      return ERROR;
    }

  scanbuffer =
    (SANE_Byte *) malloc (((scancfg.coord.height + 16) * bytes_per_line) *
			  sizeof (SANE_Byte));
  if (scanbuffer == NULL)
    {
      free (buffer1);
      free (buffer2);
      return ERROR;
    }

  /* Scan image */
  myCalib->shading_enabled = FALSE;
  rst =
    RTS_GetImage (dev, myRegs, &scancfg, &calibdata->gain_offset, scanbuffer,
		  myCalib, 0x20000080, gainmode);

  for (a = 0; a < 3; a++)
    myCalib->WRef[a] *= ((1 << scancfg.depth) >> 8);

  if (rst == ERROR)
    {
      free (buffer1);
      free (buffer2);
      free (scanbuffer);
      return ERROR;
    }

  if (scancfg.depth > 8)
    {
      /*a6d9 */
      position = 0;
      sumatorio = 0;
      if (myWidth > 0)
	{
	  do
	    {
	      switch (scancfg.colormode)
		{
		case CM_GRAY:
		case CM_LINEART:
		  channel = 0;
		  lf13c = position;
		  break;
		default:	/*CM_COLOR */
		  if (scancfg.samplerate == PIXEL_RATE)
		    {
		      /* pixel rate */
		      channel = position % bytes_per_pixel;
		      lf13c = position / bytes_per_pixel;
		    }
		  else
		    {
		      /* line rate */
		      channel = position / lf168;
		      lf13c = position % lf168;
		    }
		  break;
		}

	      /*a743 */
	      if (lf130 > 0)
		bzero (buffer1, lf130 * sizeof (double));

	      /*a761 */
	      if (lf12c > 0)
		{
		  for (a = 0; a < lf12c; a++)
		    buffer2[a] = (1 << scancfg.depth) - 1.0;
		}

	      /*a78f */
	      myESI = 0;
	      myEDI = 0;
	      ptr = scanbuffer + (position * 2);
	      myHeight = 0;

	      if (otherheight > 0)
		{
		  do
		    {
		      myst = 0;
		      for (a = 0; a < 4; a++)
			myst += data_lsb_get (ptr + (a * (myWidth * 2)), 2);

		      myEDI = 0;
		      myst = myst * 0.25;
		      if (myHeight < (otherheight - 4))
			{
			  if (myst < buffer2[myESI])
			    {
			      buffer2[myESI] = myst;
			      if (lf12c > 0)
				{
				  for (a = 0; a < lf12c; a++)
				    if (buffer2[myESI] < buffer2[a])
				      myESI = a;
				}
			    }

			  /*a820 */
			  if (myst >= buffer1[myEDI])
			    {
			      buffer1[myEDI] = myst;
			      if (lf130 > 0)
				{
				  for (a = 0; a < lf130; a++)
				    if (buffer1[myEDI] >= buffer1[a])
				      myEDI = a;
				}
			    }
			  sumatorio += myst;
			}
		      else
			{
			  /*a853 */
			  if (myHeight == (otherheight - 4))
			    {
			      if (lf12c > 0)
				{
				  for (a = 0; a < lf12c; a++)
				    if (buffer2[myESI] >= buffer2[a])
				      myESI = a;
				}

			      if (lf130 > 0)
				{
				  for (a = 0; a < lf130; a++)
				    if (buffer1[myEDI] < buffer1[a])
				      myEDI = a;
				}
			    }

			  /*a895 */
			  if (myst >= buffer2[myESI])
			    {
			      /*a89c */
			      sumatorio -= buffer2[myESI];
			      sumatorio += myst;
			      buffer2[myESI] = myst;
			      if (lf12c > 0)
				{
				  for (a = 0; a < lf12c; a++)
				    if (buffer2[myESI] >= buffer2[a])
				      myESI = a;
				}
			    }
			  else
			    {
			      if (myst < buffer1[myEDI])
				{
				  sumatorio -= buffer1[myEDI];
				  sumatorio += myst;
				  buffer1[myEDI] = myst;

				  if (lf130 > 0)
				    {
				      for (a = 0; a < lf130; a++)
					if (buffer1[myEDI] < buffer1[a])
					  myEDI = a;
				    }
				}
			    }
			}

		      /*a901 */
		      ptr += (myWidth * 2);
		      myHeight++;
		    }
		  while (myHeight < otherheight);
		}

	      /*a924 */
	      scancfg.ser = 0;
	      scancfg.startpos = otherheight - 4;

	      sumatorio = sumatorio / scancfg.startpos;
	      if (myCalib->shading_enabled != FALSE)
		{
		  /*a94a */
		  myCalib->white_shading[channel][lf13c] =
		    (unsigned short) sumatorio;
		}
	      else
		{
		  /*a967 */
		  if ((scancfg.colormode != CM_GRAY)
		      && (scancfg.colormode != CM_LINEART))
		    sumatorio /= lf1a4[channel];
		  else
		    sumatorio /= lf1a4[scancfg.channel];

		  sumatorio = min (sumatorio * 0x4000, 65535);

		  if (myRegs[0x1bf] != 0x18)
		    myCalib->black_shading[channel][lf13c] |=
		      (0x140 -
		       ((((myRegs[0x1bf] >> 3) & 3) *
			 3) << 6)) & ((int) sumatorio);
		  else
		    myCalib->white_shading[channel][lf13c] =
		      (unsigned short) sumatorio;
		}

	      /*a9fd */
	      position++;
	    }
	  while (position < myWidth);
	}
    }
  else
    {
      /*a6d9 */
      position = 0;
      sumatorio = 0;
      if (myWidth > 0)
	{
	  do
	    {
	      switch (scancfg.colormode)
		{
		case CM_GRAY:
		case CM_LINEART:
		  channel = 0;
		  lf13c = position;
		  break;
		default:	/*CM_COLOR */
		  if (scancfg.samplerate == PIXEL_RATE)
		    {
		      channel = position % bytes_per_pixel;
		      lf13c = position / bytes_per_pixel;
		    }
		  else
		    {
		      channel = position / lf168;
		      lf13c = position % lf168;
		    }
		  break;
		}

	      /*a743 */
	      if (lf130 > 0)
		bzero (buffer1, lf130 * sizeof (double));

	      /*a761 */
	      if (lf12c > 0)
		{
		  for (a = 0; a < lf12c; a++)
		    buffer2[a] = (1 << scancfg.depth) - 1.0;
		}

	      /*a78f */
	      myESI = 0;
	      myEDI = 0;
	      ptr = scanbuffer + position;
	      myHeight = 0;

	      if (otherheight > 0)
		{
		  do
		    {
		      myst = 0;
		      for (a = 0; a < 4; a++)
			myst += *(ptr + (a * myWidth));

		      myEDI = 0;
		      myst *= 0.25;
		      if (myHeight < (otherheight - 4))
			{
			  if (myst < buffer2[myESI])
			    {
			      buffer2[myESI] = myst;
			      if (lf12c > 0)
				{
				  for (a = 0; a < lf12c; a++)
				    if (buffer2[myESI] < buffer2[a])
				      myESI = a;
				}
			    }
			  /*a820 */
			  if (myst >= buffer1[myEDI])
			    {
			      buffer1[myEDI] = myst;
			      if (lf130 > 0)
				{
				  for (a = 0; a < lf130; a++)
				    if (buffer1[myEDI] >= buffer1[a])
				      myEDI = a;
				}
			    }
			  sumatorio += myst;
			}
		      else
			{
			  /*a853 */
			  if (myHeight == (otherheight - 4))
			    {
			      if (lf12c > 0)
				{
				  for (a = 0; a < lf12c; a++)
				    if (buffer2[myESI] >= buffer2[a])
				      myESI = a;
				}

			      if (lf130 > 0)
				{
				  for (a = 0; a < lf130; a++)
				    if (buffer1[myEDI] < buffer1[a])
				      myEDI = a;
				}
			    }

			  /*a895 */
			  if (myst >= buffer2[myESI])
			    {
			      /*a89c */
			      sumatorio -= buffer2[myESI];
			      sumatorio += myst;
			      buffer2[myESI] = myst;
			      if (lf12c > 0)
				{
				  for (a = 0; a < lf12c; a++)
				    if (buffer2[myESI] >= buffer2[a])
				      myESI = a;
				}
			    }
			  else
			    {
			      if (myst < buffer1[myEDI])
				{
				  sumatorio -= buffer1[myEDI];
				  sumatorio += myst;
				  buffer1[myEDI] = myst;

				  if (lf130 > 0)
				    {
				      for (a = 0; a < lf130; a++)
					if (buffer1[myEDI] < buffer1[a])
					  myEDI = a;
				    }
				}
			    }
			}
		      /*a901 */
		      ptr += myWidth;
		      myHeight++;
		    }
		  while (myHeight < otherheight);
		}
	      /*a924 */
	      scancfg.ser = 0;
	      scancfg.startpos = otherheight - 4;

	      sumatorio /= scancfg.startpos;
	      if (myCalib->shading_enabled != FALSE)
		{
		  /*a94a */
		  myCalib->white_shading[channel][lf13c] =
		    (unsigned short) sumatorio;
		}
	      else
		{
		  /*a967 */
		  if ((scancfg.colormode != CM_GRAY)
		      && (scancfg.colormode != CM_LINEART))
		    sumatorio /= lf1a4[channel];
		  else
		    sumatorio /= lf1a4[scancfg.channel];

		  sumatorio = min (sumatorio * 0x4000, 65535);

		  if (myRegs[0x1bf] != 0x18)
		    myCalib->black_shading[channel][lf13c] |=
		      (0x140 -
		       ((((myRegs[0x1bf] >> 3) & 0x03) *
			 3) << 6)) & ((int) sumatorio);
		  else
		    myCalib->white_shading[channel][lf13c] =
		      (unsigned short) sumatorio;
		}
	      /*a9fd */
	      position++;
	    }
	  while (position < myWidth);
	}
    }

  /*aa12 */
  if (RTS_Debug->SaveCalibFile != FALSE)
    {
      dbg_tiff_save ("whiteshading3.tiff",
		     scancfg.coord.width,
		     scancfg.coord.height,
		     scancfg.depth,
		     CM_COLOR,
		     scancfg.resolution_x,
		     scancfg.resolution_y,
		     scanbuffer,
		     (scancfg.coord.height + 16) * bytes_per_line);
    }

  free (buffer1);
  free (buffer2);
  free (scanbuffer);

  return OK;
}

static SANE_Int
Calib_BlackShading (struct st_device *dev,
		    struct st_calibration_config *calibcfg,
		    struct st_calibration *myCalib, SANE_Int gainmode)
{
  /*
     gainmode  f8ec
     myCalib   f8e8
     calibcfg f8e4
   */
  SANE_Byte *myRegs;		/*f1bc */
  struct st_scanparams scancfg;	/*f020 */
  double shadingprediff[6];	/*f08c f094 f09c f0a4 f0ac f0b4 */
  double mylong;		/*f018 */
  double maxvalue;		/*eff8 */
  double sumatorio = 0.0;
  double myst;
  SANE_Int rst;
  SANE_Int a;
  SANE_Int mheight;		/*efe0 */
  SANE_Int current_line;	/*efe4 */
  SANE_Int bytes_per_line;	/*efd8 */
  SANE_Int position;		/*lefcc */
  SANE_Int leff0, lf010, lefd0;
  SANE_Byte *buffer;		/*efd4 */
  SANE_Byte buff2[256];		/*F0BC */
  SANE_Int buff3[0x8000];
  SANE_Byte *ptr;		/*f008 */
  SANE_Int my14b4;		/*f008 pisa ptr */
  SANE_Int biggest;		/*bx */
  SANE_Int lowest;		/*dx */
  SANE_Int lefdc;
  SANE_Int channel;
  SANE_Int smvalues[3];		/*f04c f04e f050 */
  double dbvalue[6];		/*lf05c lf060, lf064 lf068, lf06c lf070,
				   lf074 lf078, lf07c lf080, lf084 lf088 */

  DBG (DBG_FNC, "> Calib_BlackShading(*calibcfg, *myCalib, gainmode=%i)\n",
       gainmode);

  rst = OK;
  myRegs = (SANE_Byte *) malloc (RT_BUFFER_LEN * sizeof (SANE_Byte));
  memcpy (myRegs, &calibdata->Regs, RT_BUFFER_LEN * sizeof (SANE_Byte));
  memcpy (&scancfg, &calibdata->scancfg, sizeof (struct st_scanparams));

  Lamp_SetGainMode (dev, myRegs, scancfg.resolution_x, gainmode);

  for (a = CL_RED; a <= CL_BLUE; a++)
    shadingprediff[a + 3] = calibcfg->BShadingPreDiff[a];

  if ((scan.scantype >= ST_NORMAL) && (scan.scantype <= ST_NEG))
    scancfg.coord.left += scan.ser;

  if ((scancfg.coord.width & 1) != 0)
    scancfg.coord.width++;

  scancfg.coord.top = 1;
  scancfg.depth = 8;
  scancfg.coord.height = calibcfg->BShadingHeight;

  if (scancfg.colormode != CM_COLOR)
    {
      bytes_per_line = scancfg.coord.width;
      leff0 = 0;
      lf010 = 1;
    }
  else
    {
      /*876c */
      bytes_per_line = scancfg.coord.width * 3;
      if (scancfg.samplerate == LINE_RATE)
	{
	  leff0 = scancfg.coord.width;
	  lf010 = 1;
	}
      else
	{
	  leff0 = 1;
	  lf010 = 3;
	}
    }

  scancfg.v157c = bytes_per_line;
  scancfg.bytesperline = bytes_per_line;

  mylong = 1 << (16 - scancfg.depth);
  if ((myRegs[0x1bf] & 0x18) != 0)
    mylong /= 1 << (((myRegs[0x1bf] >> 5) & 3) + 4);

  lefd0 =
    ((((myRegs[0x1bf] >> 3) & 2) << 8) -
     ((((myRegs[0x1bf] >> 3) & 1) * 3) << 6)) - 1;

  if (scancfg.depth >= 8)
    maxvalue = ((1 << (scancfg.depth - 8)) << 8) - 1;
  else
    maxvalue = (256 / (1 << (8 - scancfg.depth))) - 1;

  Calib_LoadCut (dev, &scancfg, scan.scantype, calibcfg);
  for (a = CL_RED; a <= CL_BLUE; a++)
    shadingprediff[a] = calibcfg->ShadingCut[a];

  if (calibcfg->BShadingOn == -1)
    {
      SANE_Int b, d;
      double e;

      for (a = CL_RED; a <= CL_BLUE; a++)
	{
	  myst = max (shadingprediff[a], 0);
	  myst = (maxvalue >= myst) ? shadingprediff[a] : maxvalue;
	  shadingprediff[a] = max (myst, 0);
	}

      b = 0;

      while (b < bytes_per_line)
	{
	  if (scancfg.colormode != CM_COLOR)
	    {
	      channel = 0;
	      d = b;
	    }
	  else
	    {
	      if (scancfg.samplerate == PIXEL_RATE)
		{
		  channel = (b % lf010) & 0xffff;
		  d = (b / lf010) & 0xffff;
		}
	      else
		{
		  channel = (b / leff0) & 0xffff;
		  d = (b % leff0) & 0xffff;
		}
	    }
	  /*89d0 */
	  e = min (lefd0, mylong * shadingprediff[channel]);
	  myCalib->black_shading[channel][d] |= (unsigned short) e & 0xffff;
	  b++;
	}

      return OK;
    }

  /* Allocate buffer to read image */
  mheight = scancfg.coord.height;
  buffer =
    (SANE_Byte *) malloc (((scancfg.coord.height + 16) * bytes_per_line) *
			  sizeof (SANE_Byte));
  if (buffer == NULL)
    return ERROR;

  /* Turn off lamp */
  Lamp_Status_Set (dev, NULL, FALSE, FLB_LAMP);
  usleep (200 * 1000);

  /* Scan image */
  myCalib->shading_enabled = FALSE;
  rst =
    RTS_GetImage (dev, myRegs, &scancfg, &calibdata->gain_offset, buffer,
		  myCalib, 0x101, gainmode);
  if (rst == ERROR)
    {
      /*8ac2 */
      free (buffer);
      memcpy (&calibdata->Regs, myRegs, RT_BUFFER_LEN * sizeof (SANE_Byte));
      free (myRegs);
      return ERROR;
    }

  /* myRegs isn't going to be used anymore */
  free (myRegs);
  myRegs = NULL;

  /* Turn on lamp again */
  if (scan.scantype != ST_NORMAL)
    {
      Lamp_Status_Set (dev, NULL, FALSE, TMA_LAMP);
      usleep (1000 * 1000);
    }
  else
    Lamp_Status_Set (dev, NULL, TRUE, FLB_LAMP);

  /* Save buffer */
  if (RTS_Debug->SaveCalibFile != FALSE)
    {
      dbg_tiff_save ("blackshading.tiff",
		     scancfg.coord.width,
		     scancfg.coord.height,
		     scancfg.depth,
		     CM_COLOR,
		     scancfg.resolution_x,
		     scancfg.resolution_y,
		     buffer, (scancfg.coord.height + 16) * bytes_per_line);
    }

  if (scancfg.depth > 8)
    {
      /*8bb2 */
      bzero (&dbvalue, 6 * sizeof (double));
      position = 0;

      if (bytes_per_line > 0)
	{
	  do
	    {
	      bzero (&buff3, 0x8000 * sizeof (SANE_Int));
	      sumatorio = 0;
	      ptr = buffer + position;
	      current_line = 0;
	      biggest = 0;
	      lowest = (int) maxvalue;
	      /* Toma los valores de una columna */
	      if (mheight > 0)
		{
		  SANE_Int value;
		  do
		    {
		      value = data_lsb_get (ptr, 2);
		      biggest = max (biggest, value);
		      if (current_line < mheight)
			{
			  sumatorio += value;
			  lowest = min (lowest, value);
			  biggest = max (biggest, value);

			  buff3[value]++;
			}
		      else
			{
			  /*8cab */
			  if (value > lowest)
			    {
			      buff3[lowest]--;
			      buff3[value]++;
			      sumatorio += value;
			      sumatorio -= lowest;

			      if (buff3[lowest] != 0)
				{
				  do
				    {
				      lowest++;
				    }
				  while (buff3[lowest] == 0);
				}
			    }
			}
		      /*8d0b */
		      ptr += (bytes_per_line * 2);
		      current_line++;
		    }
		  while (current_line < mheight);
		}
	      /*8d27 */
	      sumatorio /= mheight;

	      if (scancfg.colormode != CM_COLOR)
		{
		  channel = 0;
		  lefdc = position;
		}
	      else
		{
		  /*8d5f */
		  if (scancfg.samplerate == PIXEL_RATE)
		    {
		      channel = position % lf010;
		      lefdc = (position / lf010) & 0xffff;
		    }
		  else
		    {
		      channel = position / leff0;
		      lefdc = position % leff0;
		    }
		}

	      dbvalue[channel] += sumatorio;
	      if ((scancfg.colormode == CM_GRAY)
		  || (scancfg.colormode == CM_LINEART))
		sumatorio += shadingprediff[scancfg.channel];
	      else
		sumatorio += shadingprediff[channel];

	      myst = min (max (0, sumatorio), maxvalue);

	      dbvalue[channel + 3] = myst;

	      if ((calibcfg->BShadingOn == 1) || (calibcfg->BShadingOn == 2))
		{
		  if (calibcfg->BShadingOn == 2)
		    {
		      myst -=
			calibcfg->BRef[channel] * (1 << (scancfg.depth - 8));
		      myst = max (myst, 0);
		    }
		  /*8e6d */
		  myst *= mylong;
		  myCalib->black_shading[channel][lefdc] = min (myst, lefd0);
		}

	      position++;
	    }
	  while (position < bytes_per_line);
	}
    }
  else
    {
      /*8eb6 */
      bzero (&dbvalue, 6 * sizeof (double));
      position = 0;

      if (bytes_per_line > 0)
	{
	  do
	    {
	      bzero (&buff2, 256 * sizeof (SANE_Byte));
	      sumatorio = 0;
	      /* ptr points to the next position of the first line */
	      ptr = buffer + position;
	      biggest = 0;
	      lowest = (int) maxvalue;
	      current_line = 0;
	      /* Toma los valores de una columna */
	      if (mheight > 0)
		{
		  my14b4 = v14b4;
		  do
		    {
		      biggest = max (biggest, *ptr);

		      if (my14b4 == 0)
			{
			  /*8fd7 */
			  if (current_line < mheight)
			    {
			      sumatorio += *ptr;

			      lowest = min (lowest, *ptr);
			      biggest = max (biggest, *ptr);

			      buff2[*ptr]++;
			    }
			  else
			    {
			      /*9011 */
			      if (*ptr > lowest)
				{
				  buff2[lowest]--;
				  buff2[*ptr]++;
				  sumatorio += *ptr;
				  sumatorio -= lowest;

				  if (buff2[lowest] != 0)
				    {
				      do
					{
					  lowest++;
					}
				      while (buff2[lowest] == 0);
				    }
				}
			    }
			}
		      else
			sumatorio += *ptr;
		      /*9067 */
		      /* Point to the next pixel under current line */
		      ptr += bytes_per_line;
		      current_line++;
		    }
		  while (current_line < mheight);
		}

	      /*908a */
	      /* Calculates average of each column */
	      sumatorio = sumatorio / mheight;

	      if (scancfg.colormode != CM_COLOR)
		{
		  channel = 0;
		  lefdc = position;
		}
	      else
		{
		  /*90c5 */
		  if (scancfg.samplerate == PIXEL_RATE)
		    {
		      channel = position % lf010;
		      lefdc = (position / lf010) & 0xffff;
		    }
		  else
		    {
		      /*90fb */
		      channel = position / leff0;
		      lefdc = position % leff0;
		    }
		}

	      /*911f */
	      dbvalue[channel] += sumatorio;
	      if ((scancfg.colormode == CM_GRAY)
		  || (scancfg.colormode == CM_LINEART))
		sumatorio += shadingprediff[scancfg.channel];
	      else
		sumatorio += shadingprediff[channel];

	      /*9151 */
	      myst = min (max (0, sumatorio), maxvalue);

	      /*9198 */
	      if (position >= 3)
		{
		  double myst2;

		  myst -= dbvalue[channel + 3];
		  myst2 = myst;
		  myst = min (myst, shadingprediff[channel + 3]);

		  my14b4 = -shadingprediff[channel + 3];
		  if (myst >= my14b4)
		    myst = min (myst2, shadingprediff[channel + 3]);
		  else
		    myst = my14b4;

		  myst += dbvalue[channel + 3];
		}

	      /*9203 */
	      dbvalue[channel + 3] = myst;

	      switch (calibcfg->BShadingOn)
		{
		case 1:
		  myCalib->black_shading[channel][lefdc] |=
		    (unsigned short) (((int) myst & 0xff) << 8) & 0xffff;
		  break;
		case 2:
		  /*9268 */
		  my14b4 =
		    calibcfg->BRef[channel] / (1 << (8 - scancfg.depth));
		  myst -= my14b4;
		  myst = max (myst, 0);
		  myst *= mylong;
		  myCalib->black_shading[channel][lefdc] = min (myst, lefd0);
		  break;
		}

	      /*92d8 */
	      position++;
	    }
	  while (position < bytes_per_line);
	}
    }

  /*9306 */
  if (calibcfg->BShadingOn == -2)
    {
      for (a = 0; a < 3; a++)
	{
	  dbvalue[a] =
	    (dbvalue[a] / scancfg.coord.width) + calibcfg->ShadingCut[a];
	  if (dbvalue[a] < 0)
	    dbvalue[a] = 0;
	  smvalues[a] = min ((int) (dbvalue[a] + 0.5) & 0xffff, maxvalue);
	}

      if (scancfg.coord.width > 0)
	{
	  SANE_Int b, c;

	  for (c = 0; c < scancfg.coord.width; c++)
	    for (b = 0; b < 3; b++)
	      myCalib->black_shading[b][c] |=
		(unsigned short) min (smvalues[b] * mylong, lefd0);
	}
    }
  /*9425 */
  free (buffer);

  return OK;
}

static SANE_Int
Calibration (struct st_device *dev, SANE_Byte * Regs,
	     struct st_scanparams *scancfg, struct st_calibration *myCalib,
	     SANE_Int value)
{
  /*//SANE_Int Calibration([fa20]char *Regs, [fa24]struct st_scanparams *scancfg, [fa28]struct st_calibration myCalib, [fa2c]SANE_Int value)
   */

  struct st_calibration_config calibcfg;	/* f90c */
  SANE_Int a;
  SANE_Byte gainmode;
  SANE_Int lf900;

  DBG (DBG_FNC, "> Calibration\n");
  dbg_ScanParams (scancfg);

  value = value;		/*silence gcc */

  memcpy (&calibdata->Regs, Regs, sizeof (SANE_Byte) * RT_BUFFER_LEN);

  /*4213be8 */
  memset (&calibcfg, 0x30, sizeof (struct st_calibration_config));
  Calib_LoadConfig (dev, &calibcfg, scan.scantype, scancfg->resolution_x,
		    scancfg->depth);

  bzero (&calibdata->gain_offset, sizeof (struct st_gain_offset));	/*[42b3654] */
  for (a = CL_RED; a <= CL_BLUE; a++)
    {
      myCalib->WRef[a] = calibcfg.WRef[a];

      calibdata->gain_offset.edcg1[a] = 256;
      calibdata->gain_offset.odcg1[a] = 256;
      calibdata->gain_offset.vgag1[a] = 4;
      calibdata->gain_offset.vgag2[a] = 4;

      /*3654|3656|3658
         365a|365c|365e
         3660|3662|3664
         3666|3668|366a
         366c|366d|366e
         366f|3670|3671
         3672|3673|3674 */
    }

  memcpy (&calibdata->scancfg, scancfg, sizeof (struct st_scanparams));
  gainmode = Lamp_GetGainMode (dev, scancfg->resolution_x, scan.scantype);	/* [lf904] = 1 */

  /* 3cf3 */
  myCalib->first_position = 1;
  myCalib->shading_type = 0;
  if (calibdata->scancfg.colormode == CM_LINEART)
    {
      calibdata->scancfg.colormode = CM_GRAY;
      calibcfg.GainTargetFactor = 1.3;
    }

  lf900 = OK;
  if (calibcfg.CalibPAGOn != 0)
    {
      if (Calib_PAGain (dev, &calibcfg, gainmode) != 0)
	lf900 = ERROR;
    /*ERROR*/}
  else
    {
      /*3da7 */
      if ((calibdata->scancfg.colormode != CM_GRAY)
	  && (calibdata->scancfg.colormode != CM_LINEART))
	{
	  for (a = CL_RED; a <= CL_BLUE; a++)
	    calibdata->gain_offset.pag[a] = calibcfg.PAG[a];
	}
      else
	{
	  /* 3dd3 */
	  /* Default PAGain */
	  if (calibdata->scancfg.channel > 2)
	    calibdata->scancfg.channel = 0;

	  for (a = CL_RED; a <= CL_BLUE; a++)
	    calibdata->gain_offset.pag[a] =
	      calibcfg.PAG[calibdata->scancfg.channel];
	}
    }

  /* 3e01 */
  if (calibcfg.CalibOffset10n != 0)	  /*==2*/
    {
      /*v14b4=1  offset[CL_RED]=0x174  offset[CL_GREEN]=0x16d  offset[CL_BLUE]=0x160 */
      if ((v14b4 != 0) && (offset[CL_RED] != 0) && (offset[CL_GREEN] != 0)
	  && (offset[CL_BLUE] != 0))
	{
	  for (a = CL_RED; a <= CL_BLUE; a++)
	    {
	      calibdata->gain_offset.edcg1[a] = offset[a];
	      calibdata->gain_offset.odcg1[a] = offset[a];
	    }
	}
      else
	{
	  /* 3e84 */
	  if ((calibcfg.CalibOffset10n > 0) && (calibcfg.CalibOffset10n < 4))
	    {
	      /*if (calibcfg.CalibOffset10n != 0) */
	      if (calibcfg.CalibOffset10n == 3)
		{
		  lf900 = Calib_AdcOffsetRT (dev, &calibcfg, 1);
		}
	      else
		{
		  /* 3eb2 */
		  /*falta codigo */
		}
	    }
	}
    }
  else
    {
      /* 3faf */
      for (a = CL_RED; a <= CL_BLUE; a++)
	{
	  calibdata->gain_offset.edcg1[a] =
	    abs (calibcfg.OffsetEven1[a] - 0x100);
	  calibdata->gain_offset.odcg1[a] =
	    abs (calibcfg.OffsetOdd1[a] - 0x100);
	}
    }

  /* 3f13 3f0b */
  if ((gainmode != 0) && (calibcfg.CalibGain10n != 0))
    {
      /*gain[CL_RED]=0x17 gain[CL_GREEN]=0x12 gain[CL_BLUE]=0x17 */
      if ((v14b4 != 0) && (gain[CL_RED] != 0) && (gain[CL_GREEN] != 0)
	  && (gain[CL_BLUE] != 0))
	{
	  for (a = CL_RED; a <= CL_BLUE; a++)
	    calibdata->gain_offset.vgag1[a] = gain[a];
	}
      else
	{
	  /*4025 */
	  lf900 = Calib_AdcGain (dev, &calibcfg, 1, gainmode);

	  if ((v14b4 != 0) && (lf900 == OK))
	    GainOffset_Save (dev, &calibdata->gain_offset.edcg1[0],
			     &calibdata->gain_offset.vgag1[0]);
	}
    }
  else
    {
      /*4089 */
      for (a = CL_RED; a <= CL_BLUE; a++)
	calibdata->gain_offset.vgag1[a] = calibcfg.Gain1[a];
    }

  /*40a5 */
  if ((gainmode != 0) && (calibcfg.CalibOffset20n != 0))
    {
      switch (calibcfg.CalibOffset20n)
	{
	case 3:
	  lf900 = Calib_AdcOffsetRT (dev, &calibcfg, 2);
	  break;
	}
      /*4140 */
      /*falta codigo */
    }
  else
    {
      /*4162 */
      for (a = CL_RED; a <= CL_BLUE; a++)
	{
	  calibdata->gain_offset.edcg2[a] =
	    abs (calibcfg.OffsetEven2[a] - 0x40);
	  calibdata->gain_offset.odcg2[a] =
	    abs (calibcfg.OffsetOdd2[a] - 0x40);
	}
    }

  /*41d6 */
  if ((gainmode != 0) && (calibcfg.CalibGain20n != 0))
    {
      lf900 = Calib_AdcGain (dev, &calibcfg, 0, gainmode);
    }
  else
    {
      /*423c */
      for (a = CL_RED; a <= CL_BLUE; a++)
	calibdata->gain_offset.vgag2[a] = calibcfg.Gain2[a];
    }

  /*4258 */
  if (calibcfg.TotShading != 0)
    {
      lf900 = Calib_BWShading (&calibcfg, myCalib, gainmode);
      /*falta codigo */
    }
  else
    {
      /*428f */
      if (gainmode != 0)
	{
	  if (calibcfg.BShadingOn != 0)
	    lf900 = Calib_BlackShading (dev, &calibcfg, myCalib, gainmode);

	  /*42fd */
	  if ((lf900 != ERROR) && (calibcfg.WShadingOn != 0))
	    {
	      switch (calibcfg.WShadingOn)
		{
		default:
		  break;
		case 3:
		  lf900 =
		    Calib_WhiteShading_3 (dev, &calibcfg, myCalib, gainmode);
		  break;
		case 2:
		  break;
		}
	    }
	  else
	    myCalib->shading_enabled = FALSE;
	}
      else
	myCalib->shading_enabled = FALSE;
    }

  /*43ca */
  memcpy (&myCalib->gain_offset, &calibdata->gain_offset,
	  sizeof (struct st_gain_offset));
  memcpy (&mitabla2, &calibdata->gain_offset, sizeof (struct st_gain_offset));

  /*4424 */
  /* Park home after calibration */
  if (get_value (SCANINFO, PARKHOMEAFTERCALIB, TRUE, FITCALIBRATE) == FALSE)
    scan.ler -= calibcfg.WShadingHeight;
  else
    Head_ParkHome (dev, TRUE, dev->motorcfg->parkhomemotormove);

  return OK;
}

/*static void show_diff(struct st_device *dev, SANE_Byte *original)
{
	SANE_Byte *buffer = (SANE_Byte *)malloc(RT_BUFFER_LEN * sizeof(SANE_Byte));
	SANE_Int a;

	if ((buffer == NULL)||(original == NULL))
		return;

	if (RTS_ReadRegs(dev->usb_handle, buffer) != OK)
	{
		free(buffer);
		return;
	}

	for (a = 0; a < RT_BUFFER_LEN; a++)
	{
		if ((original[a] & 0xff) != (buffer[a] & 0xff))
		{
			printf("%5i: %i -> %i\n", a, original[a] & 0xff, buffer[a] & 0xff);
			original[a] = buffer[a] & 0xff;
		}
	}

	free(buffer);
} */

static SANE_Int
Load_Constrains (struct st_device *dev)
{
  SANE_Int rst = ERROR;

  if (dev->constrains != NULL)
    Free_Constrains (dev);

  DBG (DBG_FNC, "> Load_Constrains\n");

  dev->constrains =
    (struct st_constrains *) malloc (sizeof (struct st_constrains));
  if (dev->constrains != NULL)
    {
      cfg_constrains_get (dev->constrains);
      rst = OK;
    }

  return rst;
}

static SANE_Int
Constrains_Check (struct st_device *dev, SANE_Int Resolution,
		  SANE_Int scantype, struct st_coords *mycoords)
{
  /*
     Constrains: 
     100 dpi   850 x  1170   | 164 x 327
     300 dpi  2550 x  3510
     600 dpi  5100 x  7020
     1200 dpi 10200 x 14040
   */

  SANE_Int rst = ERROR;

  if (dev->constrains != NULL)
    {
      struct st_coords coords;
      struct st_coords *mc;

      if ((scantype < ST_NORMAL) || (scantype > ST_NEG))
	scantype = ST_NORMAL;

      switch (scantype)
	{
	case ST_TA:
	  mc = &dev->constrains->slide;
	  break;
	case ST_NEG:
	  mc = &dev->constrains->negative;
	  break;
	default:
	  mc = &dev->constrains->reflective;
	  break;
	}

      coords.left = MM_TO_PIXEL (mc->left, Resolution);
      coords.width = MM_TO_PIXEL (mc->width, Resolution);
      coords.top = MM_TO_PIXEL (mc->top, Resolution);
      coords.height = MM_TO_PIXEL (mc->height, Resolution);

      /* Check left and top */
      if (mycoords->left < 0)
	mycoords->left = 0;

      mycoords->left += coords.left;

      if (mycoords->top < 0)
	mycoords->top = 0;

      mycoords->top += coords.top;

      /* Check width and height */
      if ((mycoords->width < 0) || (mycoords->width > coords.width))
	mycoords->width = coords.width;

      if ((mycoords->height < 0) || (mycoords->height > coords.height))
	mycoords->height = coords.height;

      rst = OK;
    }

  DBG (DBG_FNC,
       "> Constrains_Check: Source=%s, Res=%i, LW=(%i,%i), TH=(%i,%i): %i\n",
       dbg_scantype (scantype), Resolution, mycoords->left, mycoords->width,
       mycoords->top, mycoords->height, rst);

  return rst;
}

static struct st_coords *
Constrains_Get (struct st_device *dev, SANE_Byte scantype)
{
  static struct st_coords *rst = NULL;

  if (dev->constrains != NULL)
    {
      switch (scantype)
	{
	case ST_TA:
	  rst = &dev->constrains->slide;
	  break;
	case ST_NEG:
	  rst = &dev->constrains->negative;
	  break;
	default:
	  rst = &dev->constrains->reflective;
	  break;
	}
    }

  return rst;
}

static void
Free_Constrains (struct st_device *dev)
{
  DBG (DBG_FNC, "> Free_Constrains\n");

  if (dev->constrains != NULL)
    {
      free (dev->constrains);
      dev->constrains = NULL;
    }
}

static void
RTS_DebugInit ()
{
  /* Default vaules */
  RTS_Debug->dev_model = HP3970;

  RTS_Debug->DumpShadingData = FALSE;
  RTS_Debug->SaveCalibFile = FALSE;
  RTS_Debug->ScanWhiteBoard = FALSE;
  RTS_Debug->EnableGamma = TRUE;
  RTS_Debug->use_fixed_pwm = TRUE;
  RTS_Debug->dmatransfersize = 0x80000;
  RTS_Debug->dmasetlength = 0x7c0000;
  RTS_Debug->dmabuffersize = 0x400000;
  RTS_Debug->usbtype = -1;

  /* Lamp settings */
  RTS_Debug->overdrive_flb = 10000;	/* msecs */
  RTS_Debug->overdrive_ta = 10000;	/* msecs */

  RTS_Debug->warmup = TRUE;

  /* Calibration settings */
  RTS_Debug->calibrate = FALSE;
  RTS_Debug->wshading = TRUE;
}

static void
RTS_Setup_Gamma (SANE_Byte * Regs, struct st_hwdconfig *hwdcfg)
{
  DBG (DBG_FNC, "> RTS_Setup_Gamma(*Regs, *hwdcfg)\n");

  if ((hwdcfg != NULL) && (Regs != NULL))
    {
      if (hwdcfg->use_gamma_tables != FALSE)
	{
	  SANE_Int table_size;

	  /* set set table size */
	  data_bitset (&Regs[0x1d0], 0x0f, hwdcfg->gamma_tablesize);

	  /* enable gamma correction */
	  data_bitset (&Regs[0x1d0], 0x40, 1);


	  switch (Regs[0x1d0] & 0x0c)
	    {
	    case 0:
	      table_size = (Regs[0x1d0] & 1) | 0x0100;
	      break;
	    case 4:
	      table_size = (Regs[0x1d0] & 1) | 0x0400;
	      break;
	    case 8:
	      table_size = (Regs[0x1d0] & 1) | 0x1000;
	      break;
	    default:
	      table_size = hwdcfg->startpos & 0xffff;
	      break;
	    }

	  /* 5073 */
	  /* points to red gamma table */
	  data_wide_bitset (&Regs[0x1b4], 0x3fff, 0);

	  /* points to green gamma table */
	  data_wide_bitset (&Regs[0x1b6], 0x3fff, table_size);

	  /* points to blue gamma table */
	  data_wide_bitset (&Regs[0x1b8], 0x3fff, table_size * 2);

	  v15f8 = (((table_size * 3) + 15) / 16) & 0xffff;
	}
      else
	{
	  /* disable gamma correction */
	  data_bitset (&Regs[0x1d0], 0x40, 0);
	  v15f8 = 0;
	}
    }
}

static SANE_Int
RTS_USBType (struct st_device *dev)
{
  /* Gets USB type of this scanner */

  SANE_Int rst = ERROR;
  SANE_Byte data;

  DBG (DBG_FNC, "+ RTS_USBType\n");

  if (Read_Byte (dev->usb_handle, 0xfe11, &data) == OK)
    rst = (data & 1);

  DBG (DBG_FNC, "- RTS_USBType(void): %s\n",
       (rst == USB11) ? "USB1.1" : "USB2.0");

  return rst;
}

static SANE_Int
Init_Vars (void)
{
  SANE_Int rst = OK;

  hp_gamma = malloc (sizeof (struct st_gammatables));
  if (hp_gamma != NULL)
    bzero (hp_gamma, sizeof (struct st_gammatables));
  else
    rst = ERROR;

  if (rst == OK)
    {
      RTS_Debug = malloc (sizeof (struct st_debug_opts));
      if (RTS_Debug != NULL)
	bzero (RTS_Debug, sizeof (struct st_debug_opts));
      else
	rst = ERROR;
    }

  if (rst == OK)
    {
      default_gain_offset = malloc (sizeof (struct st_gain_offset));
      if (default_gain_offset != NULL)
	bzero (default_gain_offset, sizeof (struct st_gain_offset));
      else
	rst = ERROR;
    }

  if (rst == OK)
    {
      calibdata = malloc (sizeof (struct st_calibration_data));
      if (calibdata != NULL)
	bzero (calibdata, sizeof (struct st_calibration_data));
      else
	rst = ERROR;
    }

  if (rst == OK)
    {
      wshading = malloc (sizeof (struct st_shading));
      if (wshading != NULL)
	bzero (wshading, sizeof (struct st_shading));
      else
	rst = ERROR;
    }

  waitforpwm = TRUE;

  use_gamma_tables = TRUE;

  if (rst == OK)
    RTS_DebugInit ();
  else
    Free_Vars ();

  return rst;
}

static void
Free_Vars (void)
{
  if (RTS_Debug != NULL)
    {
      free (RTS_Debug);
      RTS_Debug = NULL;
    }

  if (hp_gamma != NULL)
    {
      free (hp_gamma);
      hp_gamma = NULL;
    }

  if (calibdata != NULL)
    {
      free (calibdata);
      calibdata = NULL;
    }

  if (wshading != NULL)
    {
      if (wshading->rates != NULL)
	free (wshading->rates);

      free (wshading);
      wshading = NULL;
    }

  if (default_gain_offset != NULL)
    {
      free (default_gain_offset);
      default_gain_offset = NULL;
    }

}

static SANE_Int
Chipset_Reset (struct st_device *dev)
{
  SANE_Int rst;

  DBG (DBG_FNC, "+ Chipset_Reset:\n");

  /* I've found two ways to reset chipset. Next one will stay commented
     rst = ERROR;
     if (Read_Byte(dev->usb_handle, 0xe800, &data) == OK)
     {
     data |= 0x20;
     if (Write_Byte(dev->usb_handle, 0xe800, data) == OK)
     {
     data &= 0xdf;
     rst = Write_Byte(dev->usb_handle, 0xe800, data);
     }
     }
   */

  rst = IWrite_Buffer (dev->usb_handle, 0x0000, NULL, 0, 0x0801);

  DBG (DBG_FNC, "- Chipset_Reset: %i\n", rst);

  return rst;
}

static SANE_Int
RTS_DMA_Enable_Read (struct st_device *dev, SANE_Int dmacs, SANE_Int size,
		     SANE_Int options)
{
  SANE_Int rst = ERROR;
  SANE_Byte buffer[6];

  DBG (DBG_FNC,
       "+ RTS_DMA_Enable_Read(dmacs=0x%04x, size=%i, options=0x%06x)\n",
       dmacs, size, options);

  data_msb_set (&buffer[0], options, 3);

  /* buffer size divided by 2 (words count) */
  data_lsb_set (&buffer[3], size / 2, 3);

  rst = IWrite_Buffer (dev->usb_handle, dmacs, buffer, 6, 0x0400);

  DBG (DBG_FNC, "- RTS_DMA_Enable_Read: %i\n", rst);

  return rst;
}

static SANE_Int
RTS_DMA_Enable_Write (struct st_device *dev, SANE_Int dmacs, SANE_Int size,
		      SANE_Int options)
{
  SANE_Int rst = ERROR;
  SANE_Byte buffer[6];

  DBG (DBG_FNC,
       "+ RTS_DMA_Enable_Write(dmacs=0x%04x, size=%i, options=0x%06x)\n",
       dmacs, size, options);

  data_msb_set (&buffer[0], options, 3);

  /* buffer size divided by 2 (words count) */
  data_lsb_set (&buffer[3], size / 2, 3);

  rst = IWrite_Buffer (dev->usb_handle, dmacs, buffer, 6, 0x0401);

  DBG (DBG_FNC, "- RTS_DMA_Enable_Write: %i\n", rst);

  return rst;
}

static SANE_Int
RTS_DMA_Cancel (struct st_device *dev)
{
  SANE_Int rst;

  DBG (DBG_FNC, "+ RTS_DMA_Cancel:\n");

  rst = IWrite_Word (dev->usb_handle, 0x0000, 0, 0x0600);

  DBG (DBG_FNC, "- RTS_DMA_Cancel: %i\n", rst);

  return rst;
}

static SANE_Int
RTS_DMA_Reset (struct st_device *dev)
{
  SANE_Int rst;

  DBG (DBG_FNC, "+ RTS_DMA_Reset:\n");

  rst = IWrite_Word (dev->usb_handle, 0x0000, 0x0000, 0x0800);

  DBG (DBG_FNC, "- RTS_DMA_Reset: %i\n", rst);

  return rst;
}

static SANE_Int
RTS_EEPROM_WriteByte (USB_Handle usb_handle, SANE_Int address, SANE_Byte data)
{
  SANE_Int rst;

  DBG (DBG_FNC, "+ RTS_EEPROM_WriteByte(address=%04x, data=%i):\n", address,
       data);

  rst = IWrite_Byte (usb_handle, address, data, 0x200, 0x200);

  DBG (DBG_FNC, "- RTS_EEPROM_WriteByte: %i\n", rst);

  return rst;
}

static SANE_Int
RTS_EEPROM_ReadWord (USB_Handle usb_handle, SANE_Int address, SANE_Int * data)
{
  SANE_Int rst;

  DBG (DBG_FNC, "+ RTS_EEPROM_ReadWord(address=%04x, data):\n", address);

  rst = IRead_Word (usb_handle, address, data, 0x200);

  DBG (DBG_FNC, "- RTS_EEPROM_ReadWord: %i\n", rst);

  return rst;
}

static SANE_Int
RTS_EEPROM_ReadByte (USB_Handle usb_handle, SANE_Int address,
		     SANE_Byte * data)
{
  SANE_Int rst;

  DBG (DBG_FNC, "+ RTS_EEPROM_ReadByte(address=%04x, data):\n", address);

  rst = IRead_Byte (usb_handle, address, data, 0x200);

  DBG (DBG_FNC, "- RTS_EEPROM_ReadByte: %i\n", rst);

  return rst;
}

static SANE_Int
RTS_EEPROM_WriteWord (USB_Handle usb_handle, SANE_Int address, SANE_Int data)
{
  SANE_Int rst;

  DBG (DBG_FNC, "+ RTS_EEPROM_WriteWord(address=%04x, data=%i):\n", address,
       data);

  rst = IWrite_Word (usb_handle, address, data, 0x0200);

  DBG (DBG_FNC, "- RTS_EEPROM_WriteWord: %i\n", rst);

  return rst;
}

static SANE_Int
RTS_EEPROM_ReadInteger (USB_Handle usb_handle, SANE_Int address,
			SANE_Int * data)
{
  SANE_Int rst;

  DBG (DBG_FNC, "+ RTS_EEPROM_ReadInteger(address=%04x, data):\n", address);

  rst = IRead_Integer (usb_handle, address, data, 0x200);

  DBG (DBG_FNC, "- RTS_EEPROM_ReadInteger: %i\n", rst);

  return rst;
}

static SANE_Int
RTS_EEPROM_WriteInteger (USB_Handle usb_handle, SANE_Int address,
			 SANE_Int data)
{
  SANE_Int rst;

  DBG (DBG_FNC, "+ RTS_EEPROM_WriteInteger(address=%04x, data):\n", address);

  rst = IWrite_Integer (usb_handle, address, data, 0x200);

  DBG (DBG_FNC, "- RTS_EEPROM_WriteInteger: %i\n", rst);

  return rst;
}

static SANE_Int
RTS_EEPROM_WriteBuffer (USB_Handle usb_handle, SANE_Int address,
			SANE_Byte * data, SANE_Int size)
{
  SANE_Int rst;

  DBG (DBG_FNC, "+ RTS_EEPROM_WriteBuffer(address=%04x, data, size=%i):\n",
       address, size);

  rst = IWrite_Buffer (usb_handle, address, data, size, 0x200);

  DBG (DBG_FNC, "- RTS_EEPROM_WriteBuffer: %i\n", rst);

  return rst;
}

static void
WShading_Emulate (SANE_Byte * buffer, SANE_Int * chnptr, SANE_Int size,
		  SANE_Int depth)
{
  if ((wshading->rates != NULL) && (chnptr != NULL))
    {
      if (*chnptr < wshading->count)
	{
	  double maxvalue, chncolor;
	  SANE_Int chnsize;
	  SANE_Int pos;
	  SANE_Int icolor;

	  maxvalue = (1 << depth) - 1;
	  chnsize = (depth > 8) ? 2 : 1;

	  pos = 0;
	  while (pos < size)
	    {
	      /* get channel color */
	      chncolor = data_lsb_get (buffer + pos, chnsize);

	      /* apply shading coeficient */
	      chncolor *= wshading->rates[*chnptr];

	      /* care about limits */
	      chncolor = min (chncolor, maxvalue);

	      /* save color */
	      icolor = chncolor;
	      data_lsb_set (buffer + pos, icolor, chnsize);

	      *chnptr = *chnptr + 1;
	      if (*chnptr >= wshading->count)
		*chnptr = 0;

	      pos += chnsize;
	    }
	}
    }
}

static SANE_Int
WShading_Calibrate (struct st_device *dev, SANE_Byte * Regs,
		    struct st_calibration *myCalib,
		    struct st_scanparams *scancfg)
{
  struct st_calibration_config *calibcfg;
  struct st_gain_offset myCalibTable;
  struct st_scanparams *myscancfg;
  SANE_Byte *myRegs;		/*f1bc */
  SANE_Int bytes_per_line;
  /**/ SANE_Int x, y, a, C;
  SANE_Byte *pattern;		/*f164 */
  double sumatorio;
  SANE_Int gainmode;
  SANE_Int rst;
  SANE_Byte *avg_colors;

  DBG (DBG_FNC, "> WShading_Calibrate(*myCalib)\n");

  bzero (&myCalibTable, sizeof (struct st_gain_offset));
  for (C = CL_RED; C <= CL_BLUE; C++)
    {
      myCalibTable.pag[C] = 3;
      myCalibTable.vgag1[C] = 4;
      myCalibTable.vgag2[C] = 4;
    }

  calibcfg =
    (struct st_calibration_config *)
    malloc (sizeof (struct st_calibration_config));
  memset (calibcfg, 0x30, sizeof (struct st_calibration_config));

  myscancfg = (struct st_scanparams *) malloc (sizeof (struct st_scanparams));
  memcpy (myscancfg, scancfg, sizeof (struct st_scanparams));

  myRegs = (SANE_Byte *) malloc (RT_BUFFER_LEN * sizeof (SANE_Byte));
  memcpy (myRegs, Regs, RT_BUFFER_LEN * sizeof (SANE_Byte));

  Calib_LoadConfig (dev, calibcfg, scan.scantype, myscancfg->resolution_x,
		    myscancfg->depth);
  gainmode = Lamp_GetGainMode (dev, myscancfg->resolution_x, scan.scantype);

  Lamp_SetGainMode (dev, myRegs, myscancfg->resolution_x, gainmode);

  rst = OK;

  switch (scan.scantype)
    {
    case ST_NORMAL:
      /*a184 */
      myscancfg->coord.left += scan.ser;
      myscancfg->coord.width &= 0xffff;
      break;
    case ST_TA:
    case ST_NEG:
      myscancfg->coord.left += scan.ser;
      break;
    }

  /*a11b */
  if ((myscancfg->coord.width & 1) != 0)
    myscancfg->coord.width++;

  myscancfg->coord.top = 1;
  myscancfg->coord.height = calibcfg->WShadingHeight;

  myscancfg->sensorresolution = 0;

  bytes_per_line =
    myscancfg->coord.width * (((myscancfg->colormode == CM_COLOR) ? 3 : 1) *
			      ((myscancfg->depth > 8) ? 2 : 1));

  /*a1e8 */
  myscancfg->v157c = bytes_per_line;
  myscancfg->bytesperline = bytes_per_line;

  /* allocate space for pattern */
  pattern =
    (SANE_Byte *) malloc (((myscancfg->coord.height) * bytes_per_line) *
			  sizeof (SANE_Byte));
  if (pattern == NULL)
    return ERROR;

  /* Scan image */
  myCalib->shading_enabled = FALSE;
  rst =
    RTS_GetImage (dev, myRegs, myscancfg, &myCalibTable, pattern, myCalib,
		  0x20000000, gainmode);

  if (rst != ERROR)
    {
      SANE_Int chn;
      double colors[3] = { 0, 0, 0 };
      double prueba;
      SANE_Int data;
      SANE_Int bytes_per_channel;

      bytes_per_channel = (myscancfg->depth > 8) ? 2 : 1;

      avg_colors = (SANE_Byte *) malloc (sizeof (SANE_Byte) * bytes_per_line);

      if (avg_colors != NULL)
	{
	  wshading->ptr = 0;
	  wshading->count = bytes_per_line / bytes_per_channel;

	  if (wshading->rates != NULL)
	    {
	      free (wshading->rates);
	      wshading->rates = NULL;
	    }
	  wshading->rates =
	    (double *) malloc (sizeof (double) * wshading->count);

	  chn = 0;
	  for (x = 0; x < wshading->count; x++)
	    {
	      sumatorio = 0;

	      for (y = 0; y < myscancfg->coord.height; y++)
		{
		  data =
		    data_lsb_get (pattern +
				  ((x * bytes_per_channel) +
				   (bytes_per_line * y)), bytes_per_channel);
		  sumatorio += data;
		}

	      sumatorio /= myscancfg->coord.height;
	      a = sumatorio;
	      colors[chn] = max (colors[chn], sumatorio);
	      chn++;
	      if (chn > 2)
		chn = 0;

	      data_lsb_set (avg_colors + (x * bytes_per_channel), a,
			    bytes_per_channel);
	    }

	  DBG (DBG_FNC, " -> max colors RGB= %f %f %f\n", colors[0],
	       colors[1], colors[2]);

	  chn = 0;
	  for (x = 0; x < wshading->count; x++)
	    {
	      data =
		data_lsb_get (avg_colors + (x * bytes_per_channel),
			      bytes_per_channel);
	      prueba = data;
	      *(wshading->rates + x) = colors[chn] / prueba;
	      chn++;
	      if (chn > 2)
		chn = 0;
	    }
	}

      if (RTS_Debug->SaveCalibFile != FALSE)
	{
	  dbg_tiff_save ("whiteshading_jkd.tiff",
			 myscancfg->coord.width,
			 myscancfg->coord.height,
			 myscancfg->depth,
			 CM_COLOR,
			 scancfg->resolution_x,
			 scancfg->resolution_y,
			 pattern, (myscancfg->coord.height) * bytes_per_line);
	}

#ifdef developing
      {
	FILE *archivo;
	char texto[1024];

	/* apply correction to the pattern to see the result */
	chn = 0;
	for (x = 0; x < myscancfg->coord.height * wshading->count; x++)
	  {
	    data =
	      data_lsb_get (pattern + (x * bytes_per_channel),
			    bytes_per_channel);
	    sumatorio = data;
	    sumatorio *= wshading->rates[chn];
	    if (sumatorio > ((1 << myscancfg->depth) - 1))
	      sumatorio = (1 << myscancfg->depth) - 1;

	    a = sumatorio;
	    data_lsb_set (pattern + (x * bytes_per_channel), a,
			  bytes_per_channel);

	    chn++;
	    if (chn == wshading->count)
	      chn = 0;
	  }

	/* save corrected pattern */
	dbg_tiff_save ("onwhiteshading_jkd.tiff",
		       myscancfg->coord.width,
		       myscancfg->coord.height,
		       myscancfg->depth,
		       CM_COLOR,
		       scancfg->resolution_x,
		       scancfg->resolution_y,
		       pattern, (myscancfg->coord.height) * bytes_per_line);

	/* export coefficients */
	archivo = fopen ("wShading.txt", "w");
	for (x = 0; x < wshading->count; x++)
	  {
	    snprintf (texto, 1024, "%f", wshading->rates[x]);
	    fprintf (archivo, "%s\n", texto);
	  }

	fclose (archivo);
      }
#endif
    }

  free (pattern);

  return OK;
}

#ifdef developing
static SANE_Int
motor_pos (struct st_device *dev, SANE_Byte * Regs,
	   struct st_calibration *myCalib, struct st_scanparams *scancfg)
{
  struct st_calibration_config *calibcfg;
  struct st_gain_offset myCalibTable;
  struct st_scanparams *myscancfg;
  SANE_Byte *myRegs;		/*f1bc */
  SANE_Int bytes_per_line;
  /**/ SANE_Int a, C;
  SANE_Byte *scanbuffer;	/*f164 */
  SANE_Int gainmode;
  SANE_Int rst;

  DBG (DBG_FNC, "> Calib_test(*myCalib)\n");

  bzero (&myCalibTable, sizeof (struct st_gain_offset));

  calibcfg =
    (struct st_calibration_config *)
    malloc (sizeof (struct st_calibration_config));
  memset (calibcfg, 0x30, sizeof (struct st_calibration_config));

  myscancfg = (struct st_scanparams *) malloc (sizeof (struct st_scanparams));
  memcpy (myscancfg, scancfg, sizeof (struct st_scanparams));

  myRegs = (SANE_Byte *) malloc (RT_BUFFER_LEN * sizeof (SANE_Byte));
  memcpy (myRegs, Regs, RT_BUFFER_LEN * sizeof (SANE_Byte));

  Calib_LoadConfig (dev, calibcfg, scan.scantype, myscancfg->resolution_x,
		    myscancfg->depth);
  gainmode = Lamp_GetGainMode (dev, myscancfg->resolution_x, scan.scantype);

  Lamp_SetGainMode (dev, myRegs, myscancfg->resolution_x, gainmode);

  rst = OK;

  switch (scan.scantype)
    {
    case ST_NORMAL:
      /*a184 */
      myscancfg->coord.left += scan.ser;
      myscancfg->coord.width &= 0xffff;
      break;
    case ST_TA:
    case ST_NEG:
      myscancfg->coord.left += scan.ser;
      break;
    }

  /*a11b */
  if ((myscancfg->coord.width & 1) != 0)
    myscancfg->coord.width++;

  myscancfg->coord.top = 100;
  myscancfg->coord.height = 30;

  bytes_per_line = myscancfg->coord.width * 3;

  /*a1e8 */
  myscancfg->v157c = bytes_per_line;
  myscancfg->bytesperline = bytes_per_line;

  scanbuffer =
    (SANE_Byte *) malloc (((myscancfg->coord.height + 16) * bytes_per_line) *
			  sizeof (SANE_Byte));
  if (scanbuffer == NULL)
    return ERROR;

  /* Scan image */
  myCalib->shading_enabled = FALSE;
  /*Head_Relocate(dev, dev->motorcfg->highspeedmotormove, MTR_FORWARD, 5500); */

  for (a = 0; a < 10; a++)
    {
      for (C = CL_RED; C <= CL_BLUE; C++)
	{
	  myCalibTable.pag[C] = 3;
	  myCalibTable.vgag1[C] = 4;
	  myCalibTable.vgag2[C] = 4;
	  myCalibTable.edcg1[C] = a * 20;
	}

      dbg_ScanParams (myscancfg);

      Head_Relocate (dev, dev->motorcfg->highspeedmotormove, MTR_FORWARD,
		     5000);
      rst =
	RTS_GetImage (dev, myRegs, myscancfg, &myCalibTable, scanbuffer,
		      myCalib, 0x20000000, gainmode);
      Head_ParkHome (dev, TRUE, dev->motorcfg->parkhomemotormove);

      if (rst != ERROR)
	{
	  char name[30];
	  snprintf (name, 30, "calibtest-%i.tiff", a);
	  dbg_tiff_save (name,
			 myscancfg->coord.width,
			 myscancfg->coord.height,
			 myscancfg->depth,
			 CM_COLOR,
			 myscancfg->resolution_x,
			 myscancfg->resolution_y,
			 scanbuffer,
			 (myscancfg->coord.height + 16) * bytes_per_line);
	}
    }

  free (scanbuffer);

  exit (0);
  return OK;
}

static SANE_Int
hp4370_prueba (struct st_device *dev)
{
  SANE_Int rst;
  SANE_Int data = 0x0530, a;
  SANE_Int transferred;
  SANE_Byte buffer[512];

  for (a = 0; a < 256; a++)
    data_lsb_set (buffer + (a * 2), 0x9d7, 2);

  rst = IWrite_Word (dev->usb_handle, 0x0000, data, 0x0800);
  RTS_DMA_Enable_Write (dev, 0x4, 512, 0);
  Bulk_Operation (dev, BLK_WRITE, 512, buffer, &transferred);

  return rst;
}

static SANE_Int
Calib_BlackShading_jkd (struct st_device *dev, SANE_Byte * Regs,
			struct st_calibration *myCalib,
			struct st_scanparams *scancfg)
{
  struct st_calibration_config *calibcfg;
  struct st_gain_offset myCalibTable;
  struct st_scanparams *myscancfg;
  SANE_Byte *myRegs;		/*f1bc */
  SANE_Int bytes_per_line;
  /**/ SANE_Int x, y, a, C;
  SANE_Byte *scanbuffer;	/*f164 */
  double sumatorio;
  SANE_Int gainmode;
  SANE_Int rst;

  DBG (DBG_FNC, "> Calib_BlackShading_jkd(*myCalib)\n");

  bzero (&myCalibTable, sizeof (struct st_gain_offset));
  for (C = CL_RED; C <= CL_BLUE; C++)
    {
      myCalibTable.pag[C] = 3;
      myCalibTable.vgag1[C] = 4;
      myCalibTable.vgag2[C] = 4;
    }

  calibcfg =
    (struct st_calibration_config *)
    malloc (sizeof (struct st_calibration_config));
  memset (calibcfg, 0x30, sizeof (struct st_calibration_config));

  myscancfg = (struct st_scanparams *) malloc (sizeof (struct st_scanparams));
  memcpy (myscancfg, scancfg, sizeof (struct st_scanparams));

  myRegs = (SANE_Byte *) malloc (RT_BUFFER_LEN * sizeof (SANE_Byte));
  memcpy (myRegs, Regs, RT_BUFFER_LEN * sizeof (SANE_Byte));

  Calib_LoadConfig (dev, calibcfg, scan.scantype, myscancfg->resolution_x,
		    myscancfg->depth);
  gainmode = Lamp_GetGainMode (dev, myscancfg->resolution_x, scan.scantype);

  Lamp_SetGainMode (dev, myRegs, myscancfg->resolution_x, gainmode);

  rst = OK;

  switch (scan.scantype)
    {
    case ST_NORMAL:
      /*a184 */
      myscancfg->coord.left += scan.ser;
      myscancfg->coord.width &= 0xffff;
      break;
    case ST_TA:
    case ST_NEG:
      myscancfg->coord.left += scan.ser;
      break;
    }

  /*a11b */
  if ((myscancfg->coord.width & 1) != 0)
    myscancfg->coord.width++;

  myscancfg->coord.top = 1;
  myscancfg->coord.height = calibcfg->BShadingHeight;

  bytes_per_line = myscancfg->coord.width * 3;

  /*a1e8 */
  myscancfg->v157c = bytes_per_line;
  myscancfg->bytesperline = bytes_per_line;

  scanbuffer =
    (SANE_Byte *) malloc (((myscancfg->coord.height + 16) * bytes_per_line) *
			  sizeof (SANE_Byte));
  if (scanbuffer == NULL)
    return ERROR;

  /* Turn off lamp */
  Lamp_Status_Set (dev, NULL, FALSE, FLB_LAMP);
  usleep (200 * 1000);

  /* Scan image */
  myCalib->shading_enabled = FALSE;
  rst =
    RTS_GetImage (dev, myRegs, myscancfg, &myCalibTable, scanbuffer, myCalib,
		  0x101, gainmode);

  /* Turn on lamp again */
  if (scan.scantype != ST_NORMAL)
    {
      Lamp_Status_Set (dev, NULL, FALSE, TMA_LAMP);
      usleep (1000 * 1000);
    }
  else
    Lamp_Status_Set (dev, NULL, TRUE, FLB_LAMP);

  if (rst != ERROR)
    {
      jkd_black = (SANE_Byte *) malloc (bytes_per_line);

      if (jkd_black != NULL)
	{
	  jkd_blackbpl = bytes_per_line;

	  for (x = 0; x < bytes_per_line; x++)
	    {
	      sumatorio = 0;

	      for (y = 0; y < myscancfg->coord.height + 16; y++)
		sumatorio += scanbuffer[x + (bytes_per_line * y)];

	      sumatorio /= myscancfg->coord.height + 16;
	      a = sumatorio;
	      *(jkd_black + x) = _B0 (a);
	    }
	}

      /*if (RTS_Debug->SaveCalibFile != FALSE) */
      {
	dbg_tiff_save ("blackshading_jkd.tiff",
		       myscancfg->coord.width,
		       myscancfg->coord.height,
		       myscancfg->depth,
		       CM_COLOR,
		       myscancfg->resolution_x,
		       myscancfg->resolution_y,
		       scanbuffer,
		       (myscancfg->coord.height + 16) * bytes_per_line);
      }
    }

  free (scanbuffer);

  return OK;
}

static SANE_Int
Calib_test (struct st_device *dev, SANE_Byte * Regs,
	    struct st_calibration *myCalib, struct st_scanparams *scancfg)
{
  struct st_calibration_config *calibcfg;
  struct st_gain_offset myCalibTable;
  struct st_scanparams *myscancfg;
  SANE_Byte *myRegs;		/*f1bc */
  SANE_Int bytes_per_line;
  /**/ SANE_Int a, C;
  SANE_Byte *scanbuffer;	/*f164 */
  SANE_Int gainmode;
  SANE_Int rst;

  DBG (DBG_FNC, "> Calib_test(*myCalib)\n");

  bzero (&myCalibTable, sizeof (struct st_gain_offset));

  calibcfg =
    (struct st_calibration_config *)
    malloc (sizeof (struct st_calibration_config));
  memset (calibcfg, 0x30, sizeof (struct st_calibration_config));

  myscancfg = (struct st_scanparams *) malloc (sizeof (struct st_scanparams));
  memcpy (myscancfg, scancfg, sizeof (struct st_scanparams));

  myRegs = (SANE_Byte *) malloc (RT_BUFFER_LEN * sizeof (SANE_Byte));
  memcpy (myRegs, Regs, RT_BUFFER_LEN * sizeof (SANE_Byte));

  Calib_LoadConfig (dev, calibcfg, scan.scantype, myscancfg->resolution_x,
		    myscancfg->depth);
  gainmode = Lamp_GetGainMode (dev, myscancfg->resolution_x, scan.scantype);

  Lamp_SetGainMode (dev, myRegs, myscancfg->resolution_x, gainmode);

  rst = OK;

  switch (scan.scantype)
    {
    case ST_NORMAL:
      /*a184 */
      myscancfg->coord.left += scan.ser;
      myscancfg->coord.width &= 0xffff;
      break;
    case ST_TA:
    case ST_NEG:
      myscancfg->coord.left += scan.ser;
      break;
    }

  /*a11b */
  if ((myscancfg->coord.width & 1) != 0)
    myscancfg->coord.width++;

  myscancfg->coord.top = 100;
  myscancfg->coord.height = 30;

  bytes_per_line = myscancfg->coord.width * 3;

  /*a1e8 */
  myscancfg->v157c = bytes_per_line;
  myscancfg->bytesperline = bytes_per_line;

  scanbuffer =
    (SANE_Byte *) malloc (((myscancfg->coord.height + 16) * bytes_per_line) *
			  sizeof (SANE_Byte));
  if (scanbuffer == NULL)
    return ERROR;

  /* Scan image */
  myCalib->shading_enabled = FALSE;
  /*Head_Relocate(dev, dev->motorcfg->highspeedmotormove, MTR_FORWARD, 5500); */

  for (a = 0; a < 10; a++)
    {
      for (C = CL_RED; C <= CL_BLUE; C++)
	{
	  myCalibTable.pag[C] = 3;
	  myCalibTable.vgag1[C] = 4;
	  myCalibTable.vgag2[C] = 4;
	  myCalibTable.edcg1[C] = a * 20;
	}

      Head_Relocate (dev, dev->motorcfg->highspeedmotormove, MTR_FORWARD,
		     5000);
      rst =
	RTS_GetImage (dev, myRegs, myscancfg, &myCalibTable, scanbuffer,
		      myCalib, 0x20000000, gainmode);
      Head_ParkHome (dev, TRUE, dev->motorcfg->parkhomemotormove);

      if (rst != ERROR)
	{
	  char name[30];
	  snprintf (name, 30, "calibtest-%i.tiff", a);
	  dbg_tiff_save (name,
			 myscancfg->coord.width,
			 myscancfg->coord.height,
			 myscancfg->depth,
			 CM_COLOR,
			 myscancfg->resolution_x,
			 myscancfg->resolution_y,
			 scanbuffer,
			 (myscancfg->coord.height + 16) * bytes_per_line);
	}
    }

  free (scanbuffer);

  exit (0);
  return OK;
}

static void
prueba (SANE_Byte * a)
{
  /* SANE_Byte p[] = {}; */
  /*int z = 69; */

  /*(a + 11) = 0x0; */
  /*a[1] = a[1] | 0x40; */

  /*memcpy(a, &p, sizeof(p)); */

  /*memcpy(a + 0x12, p, 10); */
  /*a[0x146] &= 0xdf; */

}

void
shadingtest1 (struct st_device *dev, SANE_Byte * Regs,
	      struct st_calibration *myCalib)
{
  USHORT *buffer;
  int a;
  int bit[2];

  DBG (DBG_FNC, "+ shadingtest1(*Regs, *myCalib):\n");

  if ((Regs == NULL) || (myCalib == NULL))
    return;

  RTS_DMA_Reset (dev);

  bit[0] = (Regs[0x60b] >> 6) & 1;
  bit[1] = (Regs[0x60b] >> 4) & 1;
  Regs[0x060b] &= 0xaf;

  Write_Byte (dev->usb_handle, 0xee0b, Regs[0x060b]);

  Regs[0x1cf] = 0;		/* reset config. By default black shading disabled and pixel-rate */
  /*Regs[0x1cf] |= 2;  shadingbase 0x2000 */
  Regs[0x1cf] |= 4;		/* White shading enabled */
  Regs[0x1cf] |= 0x20;		/* 16 bits per channel */

  Write_Byte (dev->usb_handle, 0xe9cf, Regs[0x01cf]);

  buffer = (USHORT *) malloc (sizeof (USHORT) * myCalib->shadinglength);

  DBG (DBG_FNC, " -> shading length = %i\n", myCalib->shadinglength);

  /* fill buffer */
  for (a = 0; a < myCalib->shadinglength; a++)
    buffer[a] = RTS_Debug->shd + (a * 500);

  for (a = 0; a < 3; a++)
    {
      RTS_DMA_Write (dev, a | 0x14, 0,
		     sizeof (USHORT) * myCalib->shadinglength,
		     (SANE_Byte *) buffer);
    }

  data_bitset (&Regs[0x60b], 0x40, bit[0]);	 /*-x------*/
  data_bitset (&Regs[0x60b], 0x10, bit[1]);	 /*---x----*/

  Write_Byte (dev->usb_handle, 0xee0b, Regs[0x060b]);

  DBG (DBG_FNC, "- shadingtest1\n");
}

#endif

#endif /* RTS8822_CORE */
