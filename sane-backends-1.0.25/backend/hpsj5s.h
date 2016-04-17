#ifndef __HPSJ5S_MIDDLE_LEVEL_API_HEADER__
#define __HPSJ5S_MIDDLE_LEVEL_API_HEADER__


#include <ieee1284.h>

/*Scanner hardware registers*/
#define REGISTER_FUNCTION_CODE		0x70	/*Here goes function code */
#define REGISTER_FUNCTION_PARAMETER	0x60	/*Here goes function param */

#define ADDRESS_RESULT			0x20	/*Here we get result */

/*Scanner functions (not all - some of them I cann't identify)*/
#define FUNCTION_SETUP_HARDWARE		0xA0

/*Scanner hardware control flags:*/
/*Set this flag and non-zero speed to start rotation*/
#define FLAGS_HW_MOTOR_READY		0x1
/*Set this flag to turn on lamp*/
#define FLAGS_HW_LAMP_ON		0x2
/*Set this flag to turn indicator lamp off*/
#define FLAGS_HW_INDICATOR_OFF		0x4


/*
        Types:
*/
/*Color modes we support: 1-bit Drawing, 2-bit Halftone, 8-bit Gray Scale, 24-bt True Color*/
typedef enum
{ Drawing, Halftone, GrayScale, TrueColor }
enumColorDepth;

/*Middle-level API:*/

static int OpenScanner (const char *scanner_path);

static void CloseScanner (int handle);

static int DetectScanner (void);

static void StandByScanner (void);

static void SwitchHardwareState (SANE_Byte mask, SANE_Byte invert_mask);

static int CheckPaperPresent (void);

static int ReleasePaper (void);

static int PaperFeed (SANE_Word wLinesToFeed);

static void TransferScanParameters (enumColorDepth enColor,
				    SANE_Word wResolution,
				    SANE_Word wCorrectedLength);

static void TurnOnPaperPulling (enumColorDepth enColor,
				SANE_Word wResolution);

static void TurnOffPaperPulling (void);

static SANE_Byte GetCalibration (void);

static void CalibrateScanElements (void);

/*Internal-use functions:*/

static int OutputCheck (void);
static int InputCheck (void);
static int CallCheck (void);
static void LoadingPaletteToScanner (void);

/*Low level warappers:*/

static void WriteAddress (SANE_Byte Address);

static void WriteData (SANE_Byte Data);

static void WriteScannerRegister (SANE_Byte Address, SANE_Byte Data);

static void CallFunctionWithParameter (SANE_Byte Function,
				       SANE_Byte Parameter);

static SANE_Byte CallFunctionWithRetVal (SANE_Byte Function);

static SANE_Byte ReadDataByte (void);

static void ReadDataBlock (SANE_Byte * Buffer, int lenght);

/*Daisy chaining API: (should be moved to ieee1284 library in future)*/

/*Deselect all devices in chain on this port.*/
static void daisy_deselect_all (struct parport *port);

/*Select device with number 'daisy' in 'mode'.*/
static int daisy_select (struct parport *port, int daisy, int mode);

/*Setup address for device in chain on this port*/
static int assign_addr (struct parport *port, int daisy);

/* Send a daisy-chain-style CPP command packet. */
static int cpp_daisy (struct parport *port, int cmd);

#endif
