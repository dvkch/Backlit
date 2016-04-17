/* @file plustek-pp_p9636.c
 * @brief here we have all functionality according to the p9636t
 *
 * based on sources acquired from Plustek Inc.
 * Copyright (C) 1998 Plustek Inc.
 * Copyright (C) 2000-2004 Gerhard Jaeger <gerhard@gjaeger.de>
 * also based on the work done by Rick Bronson
 *
 * History:
 * - 0.30 - initial version
 * - 0.31 - Added some comments
 * - 0.32 - minor bug-fixes
 *        - change p9636ReconnectScannerPath
 *        - moved function IOSetStartStopRegister into this file
 * - 0.33 - went back to original three calls to p9636ReconnectScannerPath
 *          to make sure that HP-printers will not start to print during
 *          scan-process
 *        - removed function p9636PositionLamp()
 * - 0.34 - no changes
 * - 0.35 - no changes
 * - 0.36 - changes, due to define renaming
 * - 0.37 - move p9636OpenScanPath, p9636CloseScanPath
 *          and p9636RegisterToScanner to io.c
 *        - removed skipping of the memory test for OP9636P
 *        - removed // comments
 * - 0.38 - added function p9636PutToIdleMode()
 *        - moved p9636ReadWriteTest to io.c
 *        - added function p9636Calibration
 *        - renamed function p9636SetP98001Init() to p9636InitP98001()
 * - 0.39 - no changes
 * - 0.40 - no changes
 * - 0.41 - no changes
 * - 0.42 - changed include names
 * - 0.43 - no changes
 * .
 * <hr>
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
 * As a special exception, the authors of SANE give permission for
 * additional uses of the libraries contained in this release of SANE.
 *
 * The exception is that, if you link a SANE library with other files
 * to produce an executable, this does not by itself cause the
 * resulting executable to be covered by the GNU General Public
 * License.  Your use of that executable is in no way restricted on
 * account of linking the SANE library code into it.
 *
 * This exception does not, however, invalidate any other reasons why
 * the executable file might be covered by the GNU General Public
 * License.
 *
 * If you submit changes to SANE to the maintainers to be included in
 * a subsequent release, you agree by submitting the changes that
 * those changes may be distributed with this exception intact.
 *
 * If you write modifications of your own for SANE, it is your choice
 * whether to permit this exception to apply to your modifications.
 * If you do not wish that, delete this exception notice.
 * <hr>
 */
#include "plustek-pp_scan.h"

/*************************** some definitions ********************************/

#define _NUM_OF_DACREGS_W8144    11

/*************************** some local vars *********************************/

static UShort P97ColorModeRegister[] = {
	0x022C, 0x2A39, 0x0A3A, 0x373B, 0x163C, 0x0E41, 0x9042, 0x0143,
	0x2744, 0x2745, 0x0146, 0x0347, 0x2748, 0x2F49, 0x094A, 0x034B,
	0x074C, 0x054D, 0x064E, 0x0850, 0x0D51, 0x0C52, 0x0B53, 0x00F0,

	0x022C, 0x3D39, 0x043A, 0x463B, 0x063C, 0x1F41, 0x8C42, 0x0143,
	0x1344, 0x1345, 0xF246, 0x0247, 0x1348, 0x1349, 0xFA4A, 0x004B,
	0x074C, 0x054D, 0x0E4E, 0x0150, 0x0651, 0x1252, 0x0B53, 0x00F0,

	0x002C, 0x1639, 0x033A, 0x1F3B, 0x073C, 0x0441, 0x1E42, 0x0143,
	0x1344, 0x1345, 0xF146, 0x0247, 0x1348, 0x1349, 0xF94A, 0x044B,
	0x074C, 0x054D, 0x034E, 0x0650, 0x0351, 0x0952, 0x0B53, 0x00F0,

	0x022C, 0x1839, 0x043A, 0x1D3B, 0x033C, 0x0C41, 0x8442, 0x0343,
	0x0A44, 0x0845, 0xFA46, 0x0447, 0x0A48, 0x0849, 0xF24A, 0x024B,
	0x034C, 0x024D, 0x0E4E, 0x0050, 0x0951, 0x0352, 0x0353, 0x00F0
};

static UShort P17ColorModeRegister[] = {
	0x022C, 0x2C39, 0x053A, 0x3c3B, 0x0e3C, 0x0E41, 0x9042, 0x0143,
	0x2744, 0x2745, 0x0146, 0x0247, 0x2748, 0x2F49, 0x094A, 0x054B,
	0x074C, 0x054D, 0x064E, 0x0850, 0x0D51, 0x0C52, 0x0B53, 0x00F0,

	0x022C, 0x3D39, 0x043A, 0x463B, 0x063C, 0x1F41, 0x8C42, 0x0143,
	0x1344, 0x1345, 0xF246, 0x0147, 0x1348, 0x1349, 0xFA4A, 0x004B,
	0x074C, 0x054D, 0x0E4E, 0x0150, 0x0651, 0x1252, 0x0B53, 0x00F0,

	0x002C, 0x1639, 0x023A, 0x1a3B, 0x053C, 0x0441, 0x1E42, 0x0143,
	0x1344, 0x1345, 0xF146, 0x0147, 0x1348, 0x1349, 0xF94A, 0x044B,
	0x074C, 0x054D, 0x034E, 0x0650, 0x0351, 0x0952, 0x0B53, 0x00F0,

	0x022C, 0x1839, 0x043A, 0x1D3B, 0x033C, 0x0C41, 0x8442, 0x0343,
	0x0A44, 0x0845, 0xFA46, 0x0347, 0x0A48, 0x0849, 0xF24A, 0x024B,
	0x034C, 0x024D, 0x0E4E, 0x0050, 0x0951, 0x0352, 0x0353, 0x00F0
};

static UShort P535ColorModeRegister[] = {
	0x022C, 0x2F39, 0x883A, 0x403B, 0x0F3C, 0x0E41, 0x9042, 0x0143,
	0x2744, 0x2745, 0x0146, 0x0147, 0x2748, 0x2F49, 0x094A, 0x034B,
	0x074C, 0x054D, 0x064E, 0x0850, 0x0D51, 0x0C52, 0x0B53, 0x00F0,

	0x022C, 0x3D39, 0x843A, 0x463B, 0x063C, 0x1F41, 0x8C42, 0x0143,
	0x1344, 0x1345, 0xF246, 0x0147, 0x1348, 0x1349, 0xFA4A, 0x014B,
	0x074C, 0x054D, 0x0E4E, 0x0150, 0x0651, 0x1252, 0x0B53, 0x00F0,

	0x002C, 0x1639, 0x833A, 0x1F3B, 0x073C, 0x0441, 0x1E42, 0x0143,
	0x1344, 0x1345, 0xF146, 0x0147, 0x1348, 0x1349, 0xF94A, 0x044B,
	0x074C, 0x054D, 0x034E, 0x0650, 0x0351, 0x0952, 0x0B53, 0x00F0,

	0x022C, 0x1639, 0x833A, 0x1D3B, 0x033C, 0x0C41, 0x8442, 0x0343,
	0x0A44, 0x0845, 0xFA46, 0x0347, 0x0A48, 0x0849, 0xF24A, 0x024B,
	0x034C, 0x024D, 0x0E4E, 0x0050, 0x0951, 0x0352, 0x0353, 0x00F0
};

static UShort P518ColorModeRegister[] = {
	0x022C, 0x2F39, 0x883A, 0x403B, 0x0F3C, 0x0E41, 0x9042, 0x0143,
	0x2744, 0x2745, 0x0146, 0x0147, 0x2748, 0x2F49, 0x094A, 0x034B,
	0x074C, 0x054D, 0x064E, 0x0850, 0x0D51, 0x0C52, 0x0B53, 0x00F0,

	0x022C, 0x3D39, 0x843A, 0x463B, 0x063C, 0x1F41, 0x8C42, 0x0143,
	0x1344, 0x1345, 0xF246, 0x0147, 0x1348, 0x1349, 0xFA4A, 0x014B,
	0x074C, 0x054D, 0x0E4E, 0x0150, 0x0651, 0x1252, 0x0B53, 0x00F0,

	0x002C, 0x1639, 0x853A, 0x1F3B, 0x073C, 0x0441, 0x1E42, 0x0143,
	0x1344, 0x1345, 0xF146, 0x0147, 0x1348, 0x1349, 0xF94A, 0x044B,
	0x074C, 0x054D, 0x034E, 0x0650, 0x0351, 0x0952, 0x0B53, 0x00F0,

	0x022C, 0x1639, 0x833A, 0x1D3B, 0x033C, 0x0C41, 0x8442, 0x0343,
	0x0A44, 0x0845, 0xFA46, 0x0347, 0x0A48, 0x0849, 0xF24A, 0x024B,
	0x034C, 0x024D, 0x0E4E, 0x0050, 0x0951, 0x0352, 0x0353, 0x00F0
};

static UShort P56ColorModeRegister[] = {
	0x022C, 0x2F39, 0x043A, 0x363B, 0x033C, 0x0E41, 0x9042, 0x0143,
	0x2744, 0x2745, 0x0146, 0x0247, 0x2748, 0x2F49, 0x094A, 0x034B,
	0x074C, 0x054D, 0x064E, 0x0850, 0x0D51, 0x0C52, 0x0B53, 0x00F0,

	0x022C, 0x3D39, 0x033A, 0x443B, 0x033C, 0x1F41, 0x8C42, 0x0143,
	0x1344, 0x1345, 0xF246, 0x0247, 0x1348, 0x1349, 0xFA4A, 0x004B,
	0x074C, 0x054D, 0x0E4E, 0x0150, 0x0651, 0x1252, 0x0B53, 0x00F0,

	0x002C, 0x1439, 0x033A, 0x1D3B, 0x033C, 0x0441, 0x1E42, 0x0143,
	0x1344, 0x1345, 0xF146, 0x0247, 0x1348, 0x1349, 0xF94A, 0x044B,
	0x074C, 0x054D, 0x034E, 0x0650, 0x0351, 0x0952, 0x0B53, 0x00F0,

	0x022C, 0x1639, 0x033A, 0x1B3B, 0x023C, 0x0C41, 0x8442, 0x0343,
	0x0A44, 0x0845, 0xFA46, 0x0447, 0x0A48, 0x0849, 0xF24A, 0x024B,
	0x034C, 0x024D, 0x0E4E, 0x0050, 0x0951, 0x0352, 0x0353, 0x00F0
};

static UShort P539ColorModeRegister[] = {
	0x022C, 0x2F39, 0x883A, 0x403B, 0x0F3C, 0x0E41, 0x9042, 0x0143,
	0x2744, 0x2745, 0x0146, 0x0147, 0x2748, 0x2F49, 0x094A, 0x034B,
	0x074C, 0x054D, 0x064E, 0x0850, 0x0D51, 0x0C52, 0x0B53, 0x00F0,

	0x022C, 0x3D39, 0x843A, 0x463B, 0x063C, 0x1F41, 0x8C42, 0x0143,
	0x1344, 0x1345, 0xF246, 0x0147, 0x1348, 0x1349, 0xFA4A, 0x014B,
	0x074C, 0x054D, 0x0E4E, 0x0150, 0x0651, 0x1252, 0x0B53, 0x00F0,

	0x002C, 0x1639, 0x833A, 0x1F3B, 0x073C, 0x0441, 0x1E42, 0x0143,
	0x1344, 0x1345, 0xF146, 0x0147, 0x1348, 0x1349, 0xF94A, 0x044B,
	0x074C, 0x054D, 0x034E, 0x0650, 0x0351, 0x0952, 0x0B53, 0x00F0,

	0x022C, 0x1639, 0x833A, 0x1D3B, 0x033C, 0x0C41, 0x8442, 0x0343,
	0x0A44, 0x0845, 0xFA46, 0x0347, 0x0A48, 0x0849, 0xF24A, 0x024B,
	0x034C, 0x024D, 0x0E4E, 0x0050, 0x0951, 0x0352, 0x0353, 0x00F0
};

static RegDef WolfsonDAC8144[_NUM_OF_DACREGS_W8144] = {
    {0x01, 0x01}, {0x02, 0x04}, {0x03, 0x42}, {0x05, 0x10}, {0x20, 0xd0},
    {0x21, 0xd0}, {0x22, 0xd0}, {0x24, 0x00}, {0x25, 0x00},	{0x26, 0x00},
    {0x02, 0x04}
};

static RegDef ccdStop[] = {
    {0x41, 0xff}, {0x42, 0xff}, {0x4b, 0xff}, {0x4c, 0xff},
    {0x4d, 0xff}, {0x4e, 0xff}, {0x2a, 0x01}, {0x2b, 0x00},
    {0x2d, 0x00}, {0x1b, 0x19}, {0x14, 0xff}, {0x15, 0x00}
};

static DACTblDef shadingVar = {
    {{100, 100, 94}}, {{0x20, 0x20, 0x20}},
    {{0x10, 0x10, 0x10}}, {{0x00, 0x00, 0x00}}, {{0xd0, 0xd0, 0xd0}}, 0
};

/*************************** local functions *********************************/

/*.............................................................................
 * initialize the register values for the 98001 asic
 */
static void p9636InitializeAsicRegister( pScanData ps )
{
    memset( &ps->AsicReg, 0, sizeof(RegData));
    ps->AsicReg.RD_ScanControl = _SCAN_1ST_AVERAGE + _SCAN_BYTEMODE;
    ps->Scan.bFifoSelect       = ps->RegGFifoOffset;
}

/*.............................................................................
 * 1) Setup the registers of asic.
 * 2) Determine which type of CCD we are using
 * 3) According to the CCD, prepare the CCD dependent veriables
 *    SONY CCD:
 *		The color exposure sequence: Red, Green (after 11 red lines),
 *		Blue (after 8 green lines)
 *	TOSHIBA CCD:
 *		The color exposure sequence: Red, Blue (after 11 red lines),
 *		Green (after 8 blue lines)
 */
static void p9636Init98001( pScanData ps, Bool shading )
{
    Byte   	 bData;
    UShort   w;
	ULong    dwID;
    DataType Data;
    pUShort  pw;

	DBG( DBG_LOW, "p9636InitP98001(%d)\n", shading );

    bData = IODataRegisterFromScanner( ps, ps->RegConfiguration );

    ps->Device.bCCDID = bData;
    ps->Device.bCCDID &= _P98_CCD_TYPE_ID;
	DBG( DBG_HIGH, "bData = 0x%04X, PCB-ID = 0x%02X\n", bData, (bData >> 4));
	
	/* encode the CCD-id into the flag parameter */
    dwID = (ULong)ps->Device.bCCDID;
    dwID = dwID << 16;
    ps->sCaps.dwFlag |= dwID;

    /* do the defaults here and the differences below */
    ps->Device.f0_8_16 = _FALSE;

    ps->Shade.pCcdDac->DarkCmpHi.Colors.Red   = 0x30;
    ps->Shade.pCcdDac->DarkCmpLo.Colors.Red   = 0x20;
    ps->Shade.pCcdDac->DarkCmpHi.Colors.Green = 0x30;
    ps->Shade.pCcdDac->DarkCmpLo.Colors.Green = 0x20;
    ps->Shade.pCcdDac->DarkCmpHi.Colors.Blue  = 0x30;
    ps->Shade.pCcdDac->DarkCmpLo.Colors.Blue  = 0x20;

    ps->Shade.pCcdDac->DarkOffSub.Colors.Red   = 0x10;
    ps->Shade.pCcdDac->DarkOffSub.Colors.Green = 0x10;
    ps->Shade.pCcdDac->DarkOffSub.Colors.Blue  = 0x10;

    ps->Shade.pCcdDac->DarkDAC.Colors.Red   = 0xd0;
    ps->Shade.pCcdDac->DarkDAC.Colors.Green = 0xd0;
    ps->Shade.pCcdDac->DarkDAC.Colors.Blue  = 0xd0;

	/*
	 * yes, I know this function is rather ugly to read, but depending
	 * on the scan-settings and the detected CCD-chip it prepares the
	 * settings for the DAC
	 */
    ps->Device.dwModelOriginY = 0x50;
    ps->Device.wNumDACRegs    = _NUM_OF_DACREGS_W8144;

    switch( ps->Device.bCCDID ) {

	case _CCD_3797:
		DBG( DBG_HIGH, "CCD-ID = 0x%02X = _CCD_3797\n", ps->Device.bCCDID );
        ps->Shade.pCcdDac->GainResize.Colors.Red   = 100;
        ps->Shade.pCcdDac->GainResize.Colors.Green = 100;
        ps->Shade.pCcdDac->GainResize.Colors.Blue  =  96;

	    if( ps->DataInf.dwScanFlag & SCANDEF_TPA ) {
			if (ps->DataInf.dwScanFlag & SCANDEF_Transparency) {

                ps->Shade.pCcdDac->GainResize.Colors.Red   = 130;
                ps->Shade.pCcdDac->GainResize.Colors.Green = 110;
                ps->Shade.pCcdDac->GainResize.Colors.Blue  = 112;

                ps->Shade.pCcdDac->DarkDAC.Colors.Red   = 0xcc;
                ps->Shade.pCcdDac->DarkDAC.Colors.Green = 0xcc;
                ps->Shade.pCcdDac->DarkDAC.Colors.Blue  = 0xcc;

			    ps->bsPreRedDAC = ps->bsPreGreenDAC = ps->bsPreBlueDAC = 0xcc;

			    ps->wsDACCompareHighRed   = 0x0B0;
			    ps->wsDACCompareLowRed 	  = 0x0A0;
			    ps->wsDACCompareHighGreen = 0x90;
			    ps->wsDACCompareLowGreen  = 0x80;
		    	ps->wsDACCompareHighBlue  = 0x90;
			    ps->wsDACCompareLowBlue   = 0x80;
		    	ps->wsDACOffsetRed =
				ps->wsDACOffsetGreen = ps->wsDACOffsetBlue = 0x30;
			} else {
                ps->Shade.pCcdDac->GainResize.Colors.Red   =  97;
                ps->Shade.pCcdDac->GainResize.Colors.Green =  82;
                ps->Shade.pCcdDac->GainResize.Colors.Blue  = 100;

                ps->Shade.pCcdDac->DarkDAC.Colors.Red   = 0x80;
                ps->Shade.pCcdDac->DarkDAC.Colors.Green = 0x80;
                ps->Shade.pCcdDac->DarkDAC.Colors.Blue  = 0x80;

		    	ps->bsPreRedDAC = ps->bsPreGreenDAC = ps->bsPreBlueDAC = 0x80;

			    ps->wsDACCompareHighRed   = 0x90;
			    ps->wsDACCompareLowRed    = 0x80;
		    	ps->wsDACCompareHighGreen = 0x1a0;
			    ps->wsDACCompareLowGreen  = 0x190;
			    ps->wsDACCompareHighBlue  = 0x260;
			    ps->wsDACCompareLowBlue   = 0x250;
		    	ps->wsDACOffsetRed =
				ps->wsDACOffsetGreen = ps->wsDACOffsetBlue = 0x20;
			}
		} else {
            ps->Shade.pCcdDac->DarkCmpHi.Colors.Red   = 0x50;
            ps->Shade.pCcdDac->DarkCmpLo.Colors.Red   = 0x40;
            ps->Shade.pCcdDac->DarkCmpHi.Colors.Green = 0x38;
            ps->Shade.pCcdDac->DarkCmpLo.Colors.Green = 0x28;
            ps->Shade.pCcdDac->DarkCmpHi.Colors.Blue  = 0x28;
            ps->Shade.pCcdDac->DarkCmpLo.Colors.Blue  = 0x18;

            ps->Shade.pCcdDac->DarkOffSub.Colors.Red   = 0x30;
            ps->Shade.pCcdDac->DarkOffSub.Colors.Green = 0x18;
            ps->Shade.pCcdDac->DarkOffSub.Colors.Blue  = 0x08;

            ps->Shade.pCcdDac->DarkDAC.Colors.Red   = 0xf0;
            ps->Shade.pCcdDac->DarkDAC.Colors.Blue  = 0xcc;
			if (ps->bSetScanModeFlag & _ScanMode_Mono) {
                ps->Shade.pCcdDac->DarkDAC.Colors.Green =
					((ps->bSetScanModeFlag & _ScanMode_AverageOut)? 0xa0:0x68);
			} else {
                ps->Shade.pCcdDac->DarkDAC.Colors.Green = 0xcc;
			}
	    }

	    if ((ps->bSetScanModeFlag & _ScanMode_Mono) ||
			(ps->DataInf.dwScanFlag & SCANDEF_Negative)) {
       		WolfsonDAC8144[3].bParam = 0x12;
		} else {
       		WolfsonDAC8144[3].bParam = 0x10;
		}

	    pw = P97ColorModeRegister;
	    break;

	case _CCD_3717:
		DBG( DBG_HIGH, "CCD-ID = 0x%02X = _CCD_3717\n", ps->Device.bCCDID );

        ps->Shade.pCcdDac->GainResize.Colors.Red   =  96;
        ps->Shade.pCcdDac->GainResize.Colors.Green =  97;
        ps->Shade.pCcdDac->GainResize.Colors.Blue  = 100;

        ps->Shade.pCcdDac->DarkCmpHi.Colors.Red   = 0x15;
        ps->Shade.pCcdDac->DarkCmpLo.Colors.Red   = 0x05;
        ps->Shade.pCcdDac->DarkCmpHi.Colors.Green = 0x10;
        ps->Shade.pCcdDac->DarkCmpLo.Colors.Green = 0x01;
        ps->Shade.pCcdDac->DarkCmpHi.Colors.Blue  = 0x10;
        ps->Shade.pCcdDac->DarkCmpLo.Colors.Blue  = 0x01;

        ps->Shade.pCcdDac->DarkOffSub.Colors.Red   = 0x28;
        ps->Shade.pCcdDac->DarkOffSub.Colors.Green = 0x14;
        ps->Shade.pCcdDac->DarkOffSub.Colors.Blue  = 0x10;

        ps->Shade.pCcdDac->DarkDAC.Colors.Green =
                             ((ps->bSetScanModeFlag&_ScanMode_Mono)?0xd0:0xd6);
        ps->Shade.pCcdDac->DarkDAC.Colors.Red   = 0xdd;
        ps->Shade.pCcdDac->DarkDAC.Colors.Blue  = 0xd9;
	    ps->Device.f0_8_16 	= _TRUE;
	    pw = P17ColorModeRegister;
	    break;

	case _CCD_535:
		DBG( DBG_HIGH, "CCD-ID = 0x%02X = _CCD_535\n", ps->Device.bCCDID );
        ps->Shade.pCcdDac->GainResize.Colors.Red   = 100;
        ps->Shade.pCcdDac->GainResize.Colors.Green = 100;
        ps->Shade.pCcdDac->GainResize.Colors.Blue  =  95;

        ps->Shade.pCcdDac->DarkOffSub.Colors.Red   = 0x2e;
        ps->Shade.pCcdDac->DarkOffSub.Colors.Green = 0x20;
        ps->Shade.pCcdDac->DarkOffSub.Colors.Blue  = 0x28;

		ps->wMinCmpDpi = 75;
	    ps->lpEppColorHomePos->wMaxSteps = 890;
	    ps->lpBppColorHomePos->wMaxSteps = 890;
	    ps->lpSppColorHomePos->wMaxSteps = 890;
/*
 * for less 75 Dpi Motor can't moveable in Some machine
 *	    ps->lpEppColorHomePos->bExposureTime = 48;
 */
	    ps->lpEppColorHomePos->bExposureTime 	   = 64;
	    ps->a_tabDiffParam[_ColorEpp60].bStepSpeed = 8;
		ps->a_ColorSettings [1].bExposureTime      = 64;
	    ps->a_tabDiffParam[_ColorEpp100_1400].bStepSpeed = 16;
		ps->lpBppColorHomePos->bExposureTime = 96;
	    ps->lpSppColorHomePos->bExposureTime = 96;

        ps->Shade.pCcdDac->DarkDAC.Colors.Red  = 0xf0;
        ps->Shade.pCcdDac->DarkDAC.Colors.Blue = 0xdf;

	    if (ps->bSetScanModeFlag & _ScanMode_Mono) {
            ps->Shade.pCcdDac->GainResize.Colors.Green = 110;

            ps->Shade.pCcdDac->DarkCmpHi.Colors.Green  = 0x30;
            ps->Shade.pCcdDac->DarkCmpLo.Colors.Green  = 0x20;
            ps->Shade.pCcdDac->DarkOffSub.Colors.Green = 0x20;
            ps->Shade.pCcdDac->DarkDAC.Colors.Green    = 0xf0;
	    } else {
			if (ps->bSetScanModeFlag & _ScanMode_AverageOut) {

                ps->Shade.pCcdDac->DarkDAC.Colors.Red   = 0xf6;
                ps->Shade.pCcdDac->DarkDAC.Colors.Green = 0xe5;
                ps->Shade.pCcdDac->DarkDAC.Colors.Blue  = 0xe4;
                ps->Shade.pCcdDac->DarkOffSub.Colors.Red   = 0x18;
                ps->Shade.pCcdDac->DarkOffSub.Colors.Green = 0;
                ps->Shade.pCcdDac->DarkOffSub.Colors.Blue  = 0;
			} else {
                ps->Shade.pCcdDac->DarkDAC.Colors.Red   = 0xf0;
                ps->Shade.pCcdDac->DarkDAC.Colors.Green = 0xe1;
                ps->Shade.pCcdDac->DarkDAC.Colors.Blue  = 0xdf;
			}
	    }
	    pw = P535ColorModeRegister;
	    break;

	case _CCD_2556:
		DBG( DBG_HIGH, "CCD-ID = 0x%02X = _CCD_2556\n", ps->Device.bCCDID );
        ps->Shade.pCcdDac->GainResize.Colors.Red   = 100;
        ps->Shade.pCcdDac->GainResize.Colors.Green = 100;
        ps->Shade.pCcdDac->GainResize.Colors.Blue  = 100;

        ps->Shade.pCcdDac->DarkCmpHi.Colors.Red   = 0x20;
        ps->Shade.pCcdDac->DarkCmpLo.Colors.Red   = 0x10;
        ps->Shade.pCcdDac->DarkCmpHi.Colors.Green = 0x20;
        ps->Shade.pCcdDac->DarkCmpLo.Colors.Green = 0x10;
        ps->Shade.pCcdDac->DarkCmpHi.Colors.Blue  = 0x20;
        ps->Shade.pCcdDac->DarkCmpLo.Colors.Blue  = 0x10;

        ps->Shade.pCcdDac->DarkOffSub.Colors.Red   = 0;
        ps->Shade.pCcdDac->DarkOffSub.Colors.Green = 0;
        ps->Shade.pCcdDac->DarkOffSub.Colors.Blue  = 0;

	    pw = P56ColorModeRegister;
	    break;

	case _CCD_518:
		DBG( DBG_HIGH, "CCD-ID = 0x%02X = _CCD_518\n", ps->Device.bCCDID );
        ps->Device.dwModelOriginY = 0x50-3;                 /* 0x50-13 */
        ps->Shade.pCcdDac->GainResize.Colors.Red   = 98;
        ps->Shade.pCcdDac->GainResize.Colors.Green = 98;
        ps->Shade.pCcdDac->GainResize.Colors.Blue  = 98;

        ps->Shade.pCcdDac->DarkDAC.Colors.Red   = 0xcc;
        ps->Shade.pCcdDac->DarkDAC.Colors.Green = 0xcc;
        ps->Shade.pCcdDac->DarkDAC.Colors.Blue  = 0xcc;

	    if (ps->bSetScanModeFlag & _ScanMode_Mono) {

            ps->Shade.pCcdDac->DarkCmpHi.Colors.Green = 0x30;
            ps->Shade.pCcdDac->DarkCmpLo.Colors.Green = 0x20;
	    }

	    if( ps->DataInf.dwScanFlag & SCANDEF_TPA ) {
			if( ps->DataInf.dwScanFlag & SCANDEF_Transparency ) {
                ps->Shade.pCcdDac->GainResize.Colors.Red   = 104;
                ps->Shade.pCcdDac->GainResize.Colors.Green =  92;
                ps->Shade.pCcdDac->GainResize.Colors.Blue  =  96;
			    ps->bsPreRedDAC = ps->bsPreGreenDAC = ps->bsPreBlueDAC = 0xcc;

                ps->Shade.pCcdDac->DarkCmpHi.Colors.Red   = 0x20;
                ps->Shade.pCcdDac->DarkCmpLo.Colors.Red   = 0x10;
                ps->Shade.pCcdDac->DarkCmpHi.Colors.Green = 0x20;
                ps->Shade.pCcdDac->DarkCmpLo.Colors.Green = 0x10;
                ps->Shade.pCcdDac->DarkCmpHi.Colors.Blue  = 0x20;
                ps->Shade.pCcdDac->DarkCmpLo.Colors.Blue  = 0x10;

                ps->Shade.pCcdDac->DarkOffSub.Colors.Red   = 0;
                ps->Shade.pCcdDac->DarkOffSub.Colors.Green = 0;
                ps->Shade.pCcdDac->DarkOffSub.Colors.Blue  = 0;

			    ps->wsDACCompareHighRed   = 0x80;     	/* 0x35 */
			    ps->wsDACCompareLowRed 	  = 0x70;	    /* 0x25 */
			    ps->wsDACCompareHighGreen = 0x80;   	/* 0x46 */
		    	ps->wsDACCompareLowGreen  = 0x70;    	/* 0x36 */
			    ps->wsDACCompareHighBlue  = 0x80;    	/* 0x54 */
			    ps->wsDACCompareLowBlue   = 0x70;     	/* 0x44 */
			    ps->wsDACOffsetRed =
				ps->wsDACOffsetGreen = ps->wsDACOffsetBlue = 0; /* 0x70 */
			} else {
                ps->Shade.pCcdDac->GainResize.Colors.Red   = 94;
                ps->Shade.pCcdDac->GainResize.Colors.Green = 80;
                ps->Shade.pCcdDac->GainResize.Colors.Blue  = 78;

                ps->Shade.pCcdDac->DarkDAC.Colors.Red   = 0x80;
                ps->Shade.pCcdDac->DarkDAC.Colors.Green = 0x80;
                ps->Shade.pCcdDac->DarkDAC.Colors.Blue  = 0x80;

			    ps->bsPreRedDAC = ps->bsPreGreenDAC = ps->bsPreBlueDAC = 0x80;

			    ps->wsDACCompareHighRed   = 0x90;
		    	ps->wsDACCompareLowRed    = 0x80;
			    ps->wsDACCompareHighGreen = 0x1a0;
			    ps->wsDACCompareLowGreen  = 0x190;
			    ps->wsDACCompareHighBlue  = 0x160;
			    ps->wsDACCompareLowBlue   = 0x150;
			    ps->wsDACOffsetRed 	 = 0xb0;	       /* 0x180 */
			    ps->wsDACOffsetGreen = 0xcc;	       /* 0x180 */
		    	ps->wsDACOffsetBlue  = 0xe2;	       /* 0x240 */
			}
		}

	    if ((ps->bSetScanModeFlag & _ScanMode_Mono) ||
								(ps->DataInf.dwScanFlag & SCANDEF_Negative)) {
       		WolfsonDAC8144[3].bParam = 0x12;
	    } else {
       		WolfsonDAC8144[3].bParam = 0x10;
		}
	    ps->Device.f0_8_16    = _TRUE;
	    pw = P518ColorModeRegister;
	    break;

	/*
	 * CCD_539
	 */
	default:
		DBG( DBG_HIGH, "CCD-ID = 0x%02X = _CCD_539\n", ps->Device.bCCDID );
        ps->Shade.pCcdDac->GainResize.Colors.Red   = 100;
        ps->Shade.pCcdDac->GainResize.Colors.Green = 100;
        ps->Shade.pCcdDac->GainResize.Colors.Blue  =  98;

        ps->Shade.pCcdDac->DarkDAC.Colors.Red   = 0xcc;
        ps->Shade.pCcdDac->DarkDAC.Colors.Green = 0xcc;
        ps->Shade.pCcdDac->DarkDAC.Colors.Blue  = 0xcc;

	    if( ps->DataInf.dwScanFlag & SCANDEF_TPA ) {
			if( ps->DataInf.dwScanFlag & SCANDEF_Transparency ) {

                ps->Shade.pCcdDac->GainResize.Colors.Red   = 80;
                ps->Shade.pCcdDac->GainResize.Colors.Green = 80;
                ps->Shade.pCcdDac->GainResize.Colors.Blue  = 80;
			    ps->bsPreRedDAC   = 0xcc;
                ps->bsPreGreenDAC = 0xcc;
                ps->bsPreBlueDAC  = 0xcc;
			    ps->wsDACCompareHighRed = 0x90;
			    ps->wsDACCompareLowRed  = 0x80;
			    ps->wsDACCompareHighGreen = 0x90;
			    ps->wsDACCompareLowGreen  = 0x80;
		    	ps->wsDACCompareHighBlue  = 0x90;
			    ps->wsDACCompareLowBlue   = 0x80;
			    ps->wsDACOffsetRed =
				ps->wsDACOffsetGreen = ps->wsDACOffsetBlue = 0x70;
			} else {
                ps->Shade.pCcdDac->GainResize.Colors.Red   = 80;
                ps->Shade.pCcdDac->GainResize.Colors.Green = 96;
                ps->Shade.pCcdDac->GainResize.Colors.Blue  = 95;

                ps->Shade.pCcdDac->DarkDAC.Colors.Red   = 0x90;
                ps->Shade.pCcdDac->DarkDAC.Colors.Green = 0x90;
                ps->Shade.pCcdDac->DarkDAC.Colors.Blue  = 0x90;

			    ps->bsPreRedDAC = ps->bsPreGreenDAC = ps->bsPreBlueDAC = 0x90;
			    ps->wsDACCompareHighRed   = 0xd0;	 	/* 0x90  */
			    ps->wsDACCompareLowRed 	  = 0xc0;		/* 0x80  */
			    ps->wsDACCompareHighGreen = 0x110;		/* 0x1a0 */
		    	ps->wsDACCompareLowGreen  = 0x100;		/* 0x190 */
			    ps->wsDACCompareHighBlue  = 0x130;		/* 0x260 */
			    ps->wsDACCompareLowBlue   = 0x120;		/* 0x250 */
			    ps->wsDACOffsetRed 		  = 0x70;		/* 0x70  */
		    	ps->wsDACOffsetGreen 	  = 0x70;		/* 0x180 */
			    ps->wsDACOffsetBlue 	  = 0x70;		/* 0x240 */
			}
		}

	    if (ps->bSetScanModeFlag & _ScanMode_Mono ||
								(ps->DataInf.dwScanFlag & SCANDEF_Negative)) {
       		WolfsonDAC8144[3].bParam = 0x12;
		} else {
       		WolfsonDAC8144[3].bParam = 0x10;
		}
	    ps->Device.f0_8_16 = _TRUE;
	    pw = P539ColorModeRegister;
        break;
    }

    if( ps->bSetScanModeFlag == _ScanMode_Color )
   		WolfsonDAC8144[2].bParam = 0x52;
    else
   		WolfsonDAC8144[2].bParam = 0x42;

    if( ps->bSetScanModeFlag == _ScanMode_Mono )
   		WolfsonDAC8144[0].bParam = 7;
    else
	    WolfsonDAC8144[0].bParam = 3;

    ps->OpenScanPath( ps );

	/*
	 * base init of the DAC
 	 */
    DBG( DBG_IO, "Programming DAC (%u regs)\n", ps->Device.wNumDACRegs );

    for( w = 0; w < ps->Device.wNumDACRegs; w++) {

        DBG( DBG_IO, "*[0x%02x] = 0x%02x\n",
                            WolfsonDAC8144[w].bReg, WolfsonDAC8144[w].bParam );
		IODataRegisterToDAC( ps,
                             WolfsonDAC8144[w].bReg, WolfsonDAC8144[w].bParam );
    }

    if( ps->bSetScanModeFlag & _ScanMode_Mono ) {
		ps->AsicReg.RD_Model1Control = _MOTOR_2916 + _BUTTON_MODE +
									   _CCD_SHIFT_GATE + _SCAN_GRAYTYPE;
	} else {
		ps->AsicReg.RD_Model1Control = _MOTOR_2916 +
                                       _BUTTON_MODE + _CCD_SHIFT_GATE;
	}
    IODataToRegister( ps, ps->RegModel1Control, ps->AsicReg.RD_Model1Control );

    /* Check: THIS IS 11 on the 98003 */
	IODataToRegister( ps, ps->RegWaitStateInsert, 6 );

	/*
	 * according to the scan mode, program the CCD
	 */
    pw = pw + (ULong)ps->bSetScanModeFlag * 24;
	DBG( DBG_LOW, "bSetScanModeFlag = %u\n", ps->bSetScanModeFlag );

    for( w = 24; w--; pw++) {
		Data.wValue = *pw;
		IODataToRegister( ps, Data.wOverlap.b1st, Data.wOverlap.b2nd );
    }

    ps->CloseScanPath( ps );
}

/*.............................................................................
 * call the InitAsic function
 * set LineControl and stop motor
 */
static void p9636SetupScannerVariables( pScanData ps )
{
	ps->ReInitAsic( ps, _FALSE );

    IOCmdRegisterToScanner(ps, ps->RegLineControl, ps-> AsicReg.RD_LineControl);

    memset( ps->a_nbNewAdrPointer, 0, _SCANSTATE_BYTES);
    IOSetToMotorRegister( ps );
}

/*.............................................................................
 * set all necessary register contents
 */
static void p9636SetGeneralRegister( pScanData ps )
{
	DBG( DBG_LOW, "p9636SetGeneralRegister()\n" );

	ps->AsicReg.RD_StepControl   = _MOTOR0_SCANSTATE + _MOTOR0_ONESTEP;
	ps->AsicReg.RD_ModeControl   = _ModeScan + _ModeFifoRSel;
	ps->AsicReg.RD_Motor1Control = _MotorOn  + _MotorDirForward;
	ps->AsicReg.RD_Motor0Control = ps->bHpMotor | (_MotorOn+_MotorDirForward);
	ps->AsicReg.RD_XStepTime 	 = ps->bStepSpeed;

    if( COLOR_BW == ps->DataInf.wPhyDataType ) {

		ps->AsicReg.RD_ScanControl = _SCAN_BITMODE;

		if( !(ps->DataInf.dwScanFlag & SCANDEF_Inverse))
			ps->AsicReg.RD_ScanControl |= _P98_SCANDATA_INVERT;

    } else {

		if (COLOR_TRUE48 == ps->DataInf.wPhyDataType) {
			ps->AsicReg.RD_ScanControl = _SCAN_12BITMODE;

		    if (!(ps->DataInf.dwScanFlag & SCANDEF_RightAlign))
				ps->AsicReg.RD_ScanControl |= _BITALIGN_LEFT;
		} else
			ps->AsicReg.RD_ScanControl = _SCAN_BYTEMODE;

		if( ps->DataInf.dwScanFlag & SCANDEF_Inverse )
			ps->AsicReg.RD_ScanControl |= _P98_SCANDATA_INVERT;
    }

    ps->AsicReg.RD_ScanControl |= _SCAN_1ST_AVERAGE;
    IOSelectLampSource( ps );
}

/*.............................................................................
 * tell the scanner/ASIC where to start scanning and how many pixels
 */
static void p9636SetStartStopRegister( pScanData ps )
{
	ps->AsicReg.RD_Origin = (UShort)(ps->dwOffset70 + ps->Device.DataOriginX +
									 ps->DataInf.crImage.x);

	DBG( DBG_LOW, "p9636SetStartStopRegister()\n" );

    if (ps->bSetScanModeFlag & _ScanMode_AverageOut )

		ps->AsicReg.RD_Origin = ps->AsicReg.RD_Origin >> 1;

    if (ps->DataInf.wPhyDataType < COLOR_256GRAY) {
		ps->AsicReg.RD_Pixels = (UShort)ps->DataInf.dwAsicBytesPerLine;
	} else {
		ps->AsicReg.RD_Pixels = (UShort)ps->DataInf.dwAsicPixelsPerPlane;
	}

	DBG( DBG_LOW, "RD_Origin = %u, RD_Pixels = %u\n",
					ps->AsicReg.RD_Origin, ps->AsicReg.RD_Pixels );
}

/*.............................................................................
 *
 */
static void p9636SetupScanningCondition( pScanData ps )
{
	ULong dw;

	IORegisterDirectToScanner( ps, ps->RegInitDataFifo );

	ps->InitialSetCurrentSpeed( ps );

    if (ps->DataInf.wPhyDataType > COLOR_TRUE24) {
		if (ps->DataInf.dwAsicBytesPerPlane < 1024)
		    ps->Scan.dwMinReadFifo = 1024;
		else
		    ps->Scan.dwMinReadFifo = ps->DataInf.dwAsicBytesPerPlane;
	} else {
		if (ps->DataInf.dwAsicBytesPerPlane * 2 < 1024)
		    ps->Scan.dwMinReadFifo = 1024;
		else
		    ps->Scan.dwMinReadFifo = ps->DataInf.dwAsicBytesPerPlane * 2;
	}

    p9636SetGeneralRegister( ps );

	IORegisterDirectToScanner( ps, ps->RegInitDataFifo );

	ps->SetupMotorRunTable( ps );

    ps->AsicReg.RD_Dpi = ps->DataInf.xyPhyDpi.x;

	p9636SetStartStopRegister( ps );
	IOSetToMotorRegister  ( ps );

	ps->bCurrentLineCount = 0;
    IOCmdRegisterToScanner(ps, ps->RegScanControl, ps->AsicReg.RD_ScanControl);

	IOPutOnAllRegisters( ps );

	ps->OpenScanPath( ps );

	ps->AsicReg.RD_ModeControl &= ~_ModeIdle;
    IODataToRegister( ps, ps->RegModeControl, ps->AsicReg.RD_ModeControl);

	ps->AsicReg.RD_ModeControl = _ModeScan + _ModeFifoRSel;
    IODataToRegister( ps, ps->RegModeControl, ps->AsicReg.RD_ModeControl);

	IORegisterToScanner( ps, ps->RegInitDataFifo );

	ps->CloseScanPath( ps );

    if (ps->DataInf.wPhyDataType >= COLOR_TRUE24) {

		dw = ps->DataInf.dwAsicPixelsPerPlane;
		ps->dwMaxReadFifoData = _SIZE_COLORFIFO -
							    ps->DataInf.dwAsicBytesPerPlane * 64UL /
			    				(ULong)ps->bCurrentSpeed - dw;
    } else {
		dw = ps->DataInf.dwAsicBytesPerPlane;
		ps->dwMaxReadFifoData = _SIZE_GRAYFIFO -
			   					ps->DataInf.dwAsicBytesPerPlane * 64UL /
							    (ULong)ps->bCurrentSpeed - dw;
    }

    if ((dw = dw * 4UL) > ps->dwMaxReadFifoData) {
		ps->dwSizeMustProcess = ps->dwMaxReadFifoData;
    } else {
		ps->dwSizeMustProcess = dw;
	}

    if (ps->DataInf.wPhyDataType >= COLOR_TRUE24) {

		if (ps->DataInf.xyPhyDpi.y <= 150) {
		    dw = ps->DataInf.dwAsicPixelsPerPlane;
		} else {
		    if (ps->DataInf.xyPhyDpi.y <= 300) {
				dw = ps->DataInf.dwAsicPixelsPerPlane * 2;
		    } else {
				if (ps->DataInf.xyPhyDpi.y <= 600) {
		    		dw = ps->DataInf.dwAsicPixelsPerPlane * 4;
				} else {
				    dw = ps->DataInf.dwAsicPixelsPerPlane * 8;
				}
			}
		}

		if (ps->Device.f0_8_16 && (ps->DataInf.xyPhyDpi.y >= 150))
		    dw <<= 1;

		ps->dwSizeMustProcess  += dw;
		ps->Scan.dwMinReadFifo += dw;
		ps->dwMaxReadFifoData  += dw;
    }
}

/*.............................................................................
 * switch the scanner into idle mode
 */
static void p9636PutToIdleMode( pScanData ps )
{
    int i;

	DBG( DBG_LOW, "Putting Scanner (ASIC 98001) into Idle-Mode\n" );

    /*
     * turn off motor
   	 */
    IOCmdRegisterToScanner( ps, ps->RegMotor0Control, 0x00 );
    IOCmdRegisterToScanner( ps, ps->RegLineControl,ps->AsicReg.RD_LineControl);

    IOCmdRegisterToScanner( ps, ps->RegModeControl,
                	                            (_ModeIdle + _ModeFifoClose));

    ps->OpenScanPath(ps);

    DBG( DBG_IO, "CCD-Stop\n" );

    for( i = 0; i < 12; i++ ) {

        DBG(DBG_IO, "*[0x%02x] = 0x%02x\n",ccdStop[i].bReg, ccdStop[i].bParam);
		IODataToRegister( ps, ccdStop[i].bReg, ccdStop[i].bParam );
	}

    IODataRegisterToDAC( ps, 0x01, 0x00 );	/* Close ADC */

	ps->CloseScanPath(ps);
}

/*.............................................................................
 * do all the preliminary stuff here (calibrate the scanner and move the
 * sensor to it´s start position, also setup the driver for the
 * current run)
 */
static int p9636Calibration( pScanData ps )
{
	DBG( DBG_LOW, "p9636Calibration()\n" );

    ps->Scan.bFifoSelect = ps->RegGFifoOffset;

	/*
	 * wait for shading to be done
	 */
	_ASSERT(ps->WaitForShading);
	if( !ps->WaitForShading( ps ))
		return _E_TIMEOUT;

	IOCmdRegisterToScanner( ps, ps->RegLineControl, _DEFAULT_LINESCANTIME );

	/*
	 * move sensor and setup scanner for grabbing the picture
	 */
	_ASSERT(ps->WaitForPositionY);
	ps->WaitForPositionY( ps );

	IOCmdRegisterToScanner( ps, ps->RegLineControl,
								ps->AsicReg.RD_LineControl );

	ps->fDoFilter     = ps->fFilterFirstLine = _FALSE;
	ps->dwDivFilter   = ps->dwMul = 53;
	ps->bOffsetFilter = 12;

	if (COLOR_256GRAY == ps->DataInf.wPhyDataType) {
		ps->fDoFilter  = _TRUE;
		ps->pFilterBuf = ps->pGet1 = ps->pProcessingBuf;
		ps->pGet2      = ps->pGet1 + 5120;
		ps->pGet3      = ps->pGet2 + 5120;
		ps->pEndBuf    = ps->pGet3 + 5120;

		ps->fFilterFirstLine = _TRUE;
		ps->dwLinesFilter 	 = ps->DataInf.dwAppLinesPerArea;
	}

    ps->bCurrentLineCount = 0x3f;
	_DODELAY(1);

	return _OK;
}

/************************ exported functions *********************************/

/*.............................................................................
 * initialize the register values and function calls for the 98001 asic
 */
_LOC int P9636InitAsic( pScanData ps )
{
	int result;

	DBG( DBG_LOW, "P9636InitAsic()\n" );

    /*
     * preset the asic shadow registers
     */
    p9636InitializeAsicRegister( ps );

	ps->IO.bOpenCount = 0;

	/*
	 * setup the register values
	 */
	ps->RegSwitchBus 			= 0;
  	ps->RegEPPEnable 			= 1;
	ps->RegECPEnable 			= 2;
	ps->RegReadDataMode 		= 3;
	ps->RegWriteDataMode 		= 4;
	ps->RegInitDataFifo 		= 5;
	ps->RegForceStep 			= 6;
	ps->RegInitScanState 		= 7;
	ps->RegRefreshScanState 	= 8;
	ps->RegWaitStateInsert 		= 0x0a;
	ps->RegRFifoOffset 			= 0x0a;
	ps->RegGFifoOffset 			= 0x0b;
	ps->RegBFifoOffset 			= 0x0c;
	ps->RegBitDepth 			= 0x13;
	ps->RegStepControl 			= 0x14;
	ps->RegMotor0Control 		= 0x15;
	ps->RegXStepTime 			= 0x16;
	ps->RegGetScanState 		= 0x17;
	ps->RegAsicID 				= 0x18;
	ps->RegMemoryLow 			= 0x19;
	ps->RegMemoryHigh 			= 0x1a;
	ps->RegModeControl 			= 0x1b;
	ps->RegLineControl 			= 0x1c;
	ps->RegScanControl 			= 0x1d;
	ps->RegConfiguration 		= 0x1e;
	ps->RegModelControl 		= 0x1f;
	ps->RegModel1Control 		= 0x20;
	ps->RegDpiLow 				= 0x21;
	ps->RegDpiHigh 				= 0x22;
	ps->RegScanPosLow 			= 0x23;
	ps->RegScanPosHigh 			= 0x24;
	ps->RegWidthPixelsLow 		= 0x25;
	ps->RegWidthPixelsHigh 		= 0x26;
	ps->RegThresholdLow 		= 0x27;
	ps->RegThresholdHigh 		= 0x28;
	ps->RegThresholdGapControl 	= 0x29;
	ps->RegADCAddress 			= 0x2a;
	ps->RegADCData 				= 0x2b;
	ps->RegADCPixelOffset 		= 0x2c;
	ps->RegADCSerialOutStr 		= 0x2d;
	ps->RegResetConfig 			= 0x2e;
	ps->RegLensPosition			= 0x2f;
	ps->RegStatus 				= 0x30;
	ps->RegScanStateControl 	= 0x31;
	ps->RegRedChDarkOffsetLow 	= 0x33;
	ps->RegRedChDarkOffsetHigh 	= 0x34;
	ps->RegGreenChDarkOffsetLow = 0x35;
	ps->RegGreenChDarkOffsetHigh= 0x36;
	ps->RegBlueChDarkOffsetLow 	= 0x37;
	ps->RegBlueChDarkOffsetHigh = 0x38;
	ps->RegResetPulse0 			= 0x39;
	ps->RegResetPulse1 			= 0x3a;
	ps->RegCCDClampTiming0 		= 0x3b;
	ps->RegCCDClampTiming1 		= 0x3c;
	ps->RegVSMPTiming0 			= 0x41;
	ps->RegVSMPTiming1 			= 0x42;
	ps->RegCCDQ1Timing0 		= 0x43;
	ps->RegCCDQ1Timing1 		= 0x44;
	ps->RegCCDQ1Timing2 		= 0x45;
	ps->RegCCDQ1Timing3 		= 0x46;
	ps->RegCCDQ2Timing0 		= 0x47;
	ps->RegCCDQ2Timing1 		= 0x48;
	ps->RegCCDQ2Timing2 		= 0x49;
	ps->RegCCDQ2Timing3 		= 0x4a;
	ps->RegADCclockTiming0 		= 0x4b;
	ps->RegADCclockTiming1		= 0x4c;
	ps->RegADCclockTiming2 		= 0x4d;
	ps->RegADCclockTiming3 		= 0x4e;
	ps->RegADCDVTiming0 		= 0x50;
	ps->RegADCDVTiming1 		= 0x51;
	ps->RegADCDVTiming2 		= 0x52;
	ps->RegADCDVTiming3 		= 0x53;

	/*
	 * setup function calls
	 */
	ps->SetupScannerVariables  = p9636SetupScannerVariables;
	ps->SetupScanningCondition = p9636SetupScanningCondition;
	ps->ReInitAsic			   = p9636Init98001;
    ps->PutToIdleMode          = p9636PutToIdleMode;
    ps->Calibration            = p9636Calibration;

	/*
	 * setup misc
	 */
	ps->CtrlReadHighNibble  = _CTRL_GENSIGNAL + _CTRL_AUTOLF + _CTRL_STROBE;
	ps->CtrlReadLowNibble   = _CTRL_GENSIGNAL + _CTRL_AUTOLF;

	ps->f97003 				= _FALSE;
    ps->IO.useEPPCmdMode    = _FALSE;
    ps->Scan.fRefreshState  = _TRUE;
	ps->wMinCmpDpi   		= 60;
	ps->Scan.fMotorBackward = _FALSE;

	IOSetXStepLineScanTime( ps, _DEFAULT_LINESCANTIME );

    ps->Shade.pCcdDac = &shadingVar;
	ps->bFastMoveFlag = _FastMove_Low_C75_G150;
	ps->dwOffset70 	  = _P98_OFFSET70;

	/*
	 * initialize the other modules and set some
	 * function pointer
	 */
	result = DacInitialize( ps );
	if( _OK != result )
		return result;

	result = ImageInitialize( ps );
	if( _OK != result )
		return result;

	result = IOFuncInitialize( ps );
	if( _OK != result )
		return result;

	result = IOInitialize( ps );
	if( _OK != result )
		return result;

	result = MotorInitialize( ps );
	if( _OK != result )
		return result;

	/*
	 * in debug version, check all function pointers
	 */
#ifdef DEBUG
	if(	_FALSE == MiscAllPointersSet( ps ))
		return _E_INTERNAL;
#endif

	DBG( DBG_LOW, "0x%02x\n", ps->sCaps.AsicID );

    if( _FALSE == ps->OpenScanPath( ps )) {
    	DBG( DBG_LOW, "P9636InitAsic() failed.\n" );
        return _E_NO_DEV;
    }

    /*get CCD ID */
    ps->Device.bCCDID = IODataFromRegister( ps, ps->RegConfiguration );
    ps->Device.bCCDID &= _P98_CCD_TYPE_ID;
	DBG( DBG_HIGH, "CCID = 0x%02X\n", ps->Device.bCCDID);

    ps->CloseScanPath( ps );

    /* as the program parts concerning some CCDs don't handle TPA stuff,
     * I assume that these devices won't have TPA functionality
     */
    switch( ps->Device.bCCDID ) {

	case _CCD_3717:
	case _CCD_535:
	case _CCD_2556:
		DBG( DBG_HIGH, "Seems we have a 9636P\n" );
		ps->sCaps.Model = MODEL_OP_9636PP;
        ps->sCaps.dwFlag &= ~SFLAG_TPA;
        break;
	}

	DBG( DBG_LOW, "P9636InitAsic() done.\n" );
	return _OK;
}

/* END PLUSTEK-PP_P9636.C ...................................................*/
