/* sane - Scanner Access Now Easy.

   Copyright (C) 2005 Mustek.
   Originally maintained by Mustek
   Author:Roy 2005.5.24

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

#include "mustek_usb2_asic.h"

/* ---------------------- low level asic functions -------------------------- */

static SANE_Byte RegisterBankStatus = -1;

static STATUS
WriteIOControl (PAsic chip, unsigned short wValue, unsigned short wIndex, unsigned short wLength,
		SANE_Byte * lpbuf)
{
  STATUS status = STATUS_GOOD;

  status =
    sanei_usb_control_msg (chip->fd, 0x40, 0x01, wValue, wIndex, wLength,
			   lpbuf);
  if (status != STATUS_GOOD)
    {
      DBG (DBG_ERR, "WriteIOControl Error!\n");
      return status;
    }

  return STATUS_GOOD;
}

static STATUS
ReadIOControl (PAsic chip, unsigned short wValue, unsigned short wIndex, unsigned short wLength,
	       SANE_Byte * lpbuf)
{
  STATUS status = STATUS_GOOD;

  status =
    sanei_usb_control_msg (chip->fd, 0xc0, 0x01, wValue, wIndex, wLength,
			   lpbuf);
  if (status != STATUS_GOOD)
    {
      DBG (DBG_ERR, "WriteIOControl Error!\n");
      return status;
    }

  return status;
}

static STATUS
Mustek_ClearFIFO (PAsic chip)
{
  STATUS status = STATUS_GOOD;
  SANE_Byte buf[4];
  DBG (DBG_ASIC, "Mustek_ClearFIFO:Enter\n");

  buf[0] = 0;
  buf[1] = 0;
  buf[2] = 0;
  buf[3] = 0;
  status = WriteIOControl (chip, 0x05, 0, 4, (SANE_Byte *) (buf));
  if (status != STATUS_GOOD)
    return status;

  status = WriteIOControl (chip, 0xc0, 0, 4, (SANE_Byte *) (buf));
  if (status != STATUS_GOOD)
    return status;


  DBG (DBG_ASIC, "Mustek_ClearFIFO:Exit\n");
  return STATUS_GOOD;
}


static STATUS
Mustek_SendData (PAsic chip, unsigned short reg, SANE_Byte data)
{
  SANE_Byte buf[4];
  STATUS status = STATUS_GOOD;
  DBG (DBG_ASIC, "Mustek_SendData: Enter. reg=%x,data=%x\n", reg, data);


  if (reg <= 0xFF)
    {
      if (RegisterBankStatus != 0)
	{
	  DBG (DBG_ASIC, "RegisterBankStatus=%d\n", RegisterBankStatus);
	  buf[0] = ES01_5F_REGISTER_BANK_SELECT;
	  buf[1] = SELECT_REGISTER_BANK0;
	  buf[2] = ES01_5F_REGISTER_BANK_SELECT;
	  buf[3] = SELECT_REGISTER_BANK0;
	  WriteIOControl (chip, 0xb0, 0, 4, buf);
	  RegisterBankStatus = 0;
	  DBG (DBG_ASIC, "RegisterBankStatus=%d\n", RegisterBankStatus);
	}

    }
  else if (reg <= 0x1FF)
    {
      if (RegisterBankStatus != 1)
	{
	  DBG (DBG_ASIC, "RegisterBankStatus=%d\n", RegisterBankStatus);
	  buf[0] = ES01_5F_REGISTER_BANK_SELECT;
	  buf[1] = SELECT_REGISTER_BANK1;
	  buf[2] = ES01_5F_REGISTER_BANK_SELECT;
	  buf[3] = SELECT_REGISTER_BANK1;

	  WriteIOControl (chip, 0xb0, 0, 4, buf);
	  RegisterBankStatus = 1;
	}
    }
  else if (reg <= 0x2FF)
    {
      if (RegisterBankStatus != 2)
	{
	  DBG (DBG_ASIC, "RegisterBankStatus=%d\n", RegisterBankStatus);
	  buf[0] = ES01_5F_REGISTER_BANK_SELECT;
	  buf[1] = SELECT_REGISTER_BANK2;
	  buf[2] = ES01_5F_REGISTER_BANK_SELECT;
	  buf[3] = SELECT_REGISTER_BANK2;

	  WriteIOControl (chip, 0xb0, 0, 4, buf);
	  RegisterBankStatus = 2;
	}
    }

  buf[0] = LOBYTE (reg);
  buf[1] = data;
  buf[2] = LOBYTE (reg);
  buf[3] = data;
  status = WriteIOControl (chip, 0xb0, 0, 4, buf);
  if (status != STATUS_GOOD)
    DBG (DBG_ERR, ("Mustek_SendData: write error\n"));

  return status;
}

static STATUS
Mustek_ReceiveData (PAsic chip, SANE_Byte * reg)
{
  STATUS status = STATUS_GOOD;
  SANE_Byte buf[4];

  DBG (DBG_ASIC, "Mustek_ReceiveData\n");

  status = ReadIOControl (chip, 0x07, 0, 4, buf);

  *reg = buf[0];
  return status;
}

static STATUS
Mustek_WriteAddressLineForRegister (PAsic chip, SANE_Byte x)
{
  STATUS status = STATUS_GOOD;
  SANE_Byte buf[4];

  DBG (DBG_ASIC, "Mustek_WriteAddressLineForRegister: Enter\n");

  buf[0] = x;
  buf[1] = x;
  buf[2] = x;
  buf[3] = x;
  status = WriteIOControl (chip, 0x04, x, 4, buf);

  DBG (DBG_ASIC, "Mustek_WriteAddressLineForRegister: Exit\n");
  return status;
}


static STATUS
SetRWSize (PAsic chip, SANE_Byte ReadWrite, unsigned int size)
{
  STATUS status = STATUS_GOOD;
  DBG (DBG_ASIC, "SetRWSize: Enter\n");

  if (ReadWrite == 0)
    {				/*write */
      status = Mustek_SendData (chip, 0x7C, (SANE_Byte) (size));
      if (status != STATUS_GOOD)
	return status;
      status = Mustek_SendData (chip, 0x7D, (SANE_Byte) (size >> 8));
      if (status != STATUS_GOOD)
	return status;
      status = Mustek_SendData (chip, 0x7E, (SANE_Byte) (size >> 16));
      if (status != STATUS_GOOD)
	return status;
      status = Mustek_SendData (chip, 0x7F, (SANE_Byte) (size >> 24));
      if (status != STATUS_GOOD)
	return status;
    }
  else
    {				/* read */
      status = Mustek_SendData (chip, 0x7C, (SANE_Byte) (size >> 1));
      if (status != STATUS_GOOD)
	return status;
      status = Mustek_SendData (chip, 0x7D, (SANE_Byte) (size >> 9));
      if (status != STATUS_GOOD)
	return status;
      status = Mustek_SendData (chip, 0x7E, (SANE_Byte) (size >> 17));
      if (status != STATUS_GOOD)
	return status;
      status = Mustek_SendData (chip, 0x7F, (SANE_Byte) (size >> 25));
      if (status != STATUS_GOOD)
	return status;
    }

  DBG (DBG_ASIC, "SetRWSize: Exit\n");
  return STATUS_GOOD;
}

static STATUS
Mustek_DMARead (PAsic chip, unsigned int size, SANE_Byte * lpdata)
{
  STATUS status = STATUS_GOOD;
  unsigned int i, buf[1];
  unsigned int read_size;

  DBG (DBG_ASIC, "Mustek_DMARead: Enter\n");

  status = Mustek_ClearFIFO (chip);
  if (status != STATUS_GOOD)
    return status;

  buf[0] = read_size = 32 * 1024;
  for (i = 0; i < size / (read_size); i++)
    {
      SetRWSize (chip, 1, buf[0]);
      status = WriteIOControl (chip, 0x03, 0, 4, (SANE_Byte *) (buf));

      status =
	sanei_usb_read_bulk (chip->fd, lpdata + i * read_size,
			     (size_t *) buf);
      if (status != STATUS_GOOD)
	{
	  DBG (DBG_ERR, "Mustek_DMARead: read error\n");
	  return status;
	}
    }

  buf[0] = size - i * read_size;
  if (buf[0] > 0)
    {
      SetRWSize (chip, 1, buf[0]);
      status = WriteIOControl (chip, 0x03, 0, 4, (SANE_Byte *) (buf));

      status =
	sanei_usb_read_bulk (chip->fd, lpdata + i * read_size,
			     (size_t *) buf);
      if (status != STATUS_GOOD)
	{
	  DBG (DBG_ERR, "Mustek_DMARead: read error\n");
	  return status;
	}

      usleep (20000);
    }

  DBG (DBG_ASIC, "Mustek_DMARead: Exit\n");
  return STATUS_GOOD;
}

static STATUS
Mustek_DMAWrite (PAsic chip, unsigned int size, SANE_Byte * lpdata)
{
  STATUS status = STATUS_GOOD;
  unsigned int buf[1];
  unsigned int i;
  unsigned int write_size;

  DBG (DBG_ASIC, "Mustek_DMAWrite: Enter:size=%d\n", size);

  status = Mustek_ClearFIFO (chip);
  if (status != STATUS_GOOD)
    return status;

  buf[0] = write_size = 32 * 1024;
  for (i = 0; i < size / (write_size); i++)
    {
      SetRWSize (chip, 0, buf[0]);
      WriteIOControl (chip, 0x02, 0, 4, (SANE_Byte *) buf);

      status =
	sanei_usb_write_bulk (chip->fd, lpdata + i * write_size,
			      (size_t *) buf);
      if (status != STATUS_GOOD)
	{
	  DBG (DBG_ERR, "Mustek_DMAWrite: write error\n");
	  return status;
	}
    }


  buf[0] = size - i * write_size;
  if (buf[0] > 0)
    {
      SetRWSize (chip, 0, buf[0]);
      WriteIOControl (chip, 0x02, 0, 4, (SANE_Byte *) buf);

      status =
	sanei_usb_write_bulk (chip->fd, lpdata + i * write_size,
			      (size_t *) buf);
      if (status != STATUS_GOOD)
	{
	  DBG (DBG_ERR, "Mustek_DMAWrite: write error\n");
	  return status;
	}
    }

  Mustek_ClearFIFO (chip);

  DBG (DBG_ASIC, "Mustek_DMAWrite: Exit\n");
  return STATUS_GOOD;
}


static STATUS
Mustek_SendData2Byte (PAsic chip, unsigned short reg, SANE_Byte data)
{
  static SANE_Bool isTransfer = FALSE;
  static SANE_Byte BankBuf[4];
  static SANE_Byte DataBuf[4];

  if (reg <= 0xFF)
    {
      if (RegisterBankStatus != 0)
	{
	  DBG (DBG_ASIC, "RegisterBankStatus=%d\n", RegisterBankStatus);
	  BankBuf[0] = ES01_5F_REGISTER_BANK_SELECT;
	  BankBuf[1] = SELECT_REGISTER_BANK0;
	  BankBuf[2] = ES01_5F_REGISTER_BANK_SELECT;
	  BankBuf[3] = SELECT_REGISTER_BANK0;
	  WriteIOControl (chip, 0xb0, 0, 4, BankBuf);

	  RegisterBankStatus = 0;
	}
    }
  else if (reg <= 0x1FF)
    {
      if (RegisterBankStatus != 1)
	{
	  DBG (DBG_ASIC, "RegisterBankStatus=%d\n", RegisterBankStatus);
	  BankBuf[0] = ES01_5F_REGISTER_BANK_SELECT;
	  BankBuf[1] = SELECT_REGISTER_BANK1;
	  BankBuf[2] = ES01_5F_REGISTER_BANK_SELECT;

	  BankBuf[3] = SELECT_REGISTER_BANK1;
	  WriteIOControl (chip, 0xb0, 0, 4, BankBuf);
	  RegisterBankStatus = 1;
	}
    }
  else if (reg <= 0x2FF)
    {
      if (RegisterBankStatus != 2)
	{
	  DBG (DBG_ASIC, "RegisterBankStatus=%d\n", RegisterBankStatus);
	  BankBuf[0] = ES01_5F_REGISTER_BANK_SELECT;
	  BankBuf[1] = SELECT_REGISTER_BANK2;
	  BankBuf[2] = ES01_5F_REGISTER_BANK_SELECT;
	  BankBuf[3] = SELECT_REGISTER_BANK2;
	  WriteIOControl (chip, 0xb0, 0, 4, BankBuf);
	  RegisterBankStatus = 2;
	}
    }

  if (isTransfer == FALSE)
    {
      DataBuf[0] = LOBYTE (reg);
      DataBuf[1] = data;
      isTransfer = TRUE;
    }
  else
    {
      DataBuf[2] = LOBYTE (reg);
      DataBuf[3] = data;
      WriteIOControl (chip, 0xb0, 0, 4, DataBuf);
      isTransfer = FALSE;
    }

  return STATUS_GOOD;
}



/* ---------------------- asic motor functions ----------------------------- */

static STATUS
LLFRamAccess (PAsic chip, LLF_RAMACCESS * RamAccess)
{
  STATUS status = STATUS_GOOD;
  SANE_Byte a[2];

  DBG (DBG_ASIC, "LLFRamAccess:Enter\n");

  /*Set start address. Unit is a word. */
  Mustek_SendData (chip, ES01_A0_HostStartAddr0_7,
		   LOBYTE (RamAccess->LoStartAddress));

  if (RamAccess->IsOnChipGamma == ON_CHIP_FINAL_GAMMA)
    {				/* final gamma */
      Mustek_SendData (chip, ES01_A1_HostStartAddr8_15,
		       HIBYTE (RamAccess->LoStartAddress));
      Mustek_SendData (chip, ES01_A2_HostStartAddr16_21,
		       LOBYTE (RamAccess->HiStartAddress) | ACCESS_GAMMA_RAM);
    }
  else if (RamAccess->IsOnChipGamma == ON_CHIP_PRE_GAMMA)
    {				/* pre gamma */
      Mustek_SendData (chip, ES01_A1_HostStartAddr8_15,
		       HIBYTE (RamAccess->
			       LoStartAddress) | ES01_ACCESS_PRE_GAMMA);
      Mustek_SendData (chip, ES01_A2_HostStartAddr16_21,
		       LOBYTE (RamAccess->HiStartAddress) | ACCESS_GAMMA_RAM);
    }
  else
    {				/* dram */
      Mustek_SendData (chip, ES01_A1_HostStartAddr8_15,
		       HIBYTE (RamAccess->LoStartAddress));
      Mustek_SendData (chip, ES01_A2_HostStartAddr16_21,
		       LOBYTE (RamAccess->HiStartAddress) | ACCESS_DRAM);
    }

  /* set SDRAM delay time */
  Mustek_SendData (chip, ES01_79_AFEMCLK_SDRAMCLK_DELAY_CONTROL,
		   SDRAMCLK_DELAY_12_ns);

  /*Set end address. Unit is a word. */
  Mustek_SendData (chip, ES01_A3_HostEndAddr0_7, 0xff);
  Mustek_SendData (chip, ES01_A4_HostEndAddr8_15, 0xff);
  Mustek_SendData (chip, ES01_A5_HostEndAddr16_21, 0xff);
  Mustek_ClearFIFO (chip);

  if (RamAccess->ReadWrite == WRITE_RAM)
    {				/*Write RAM */
      Mustek_DMAWrite (chip, RamAccess->RwSize, RamAccess->BufferPtr);	/* read-write size must be even */

      /*steal read 2byte */
      usleep (20000);
      RamAccess->RwSize = 2;
      RamAccess->BufferPtr = (SANE_Byte *) a;
      RamAccess->ReadWrite = READ_RAM;
      LLFRamAccess (chip, RamAccess);
      DBG (DBG_ASIC, "end steal 2 byte!\n");
    }
  else
    {				/* Read RAM */
      Mustek_DMARead (chip, RamAccess->RwSize, RamAccess->BufferPtr);	/* read-write size must be even */
    }

  DBG (DBG_ASIC, "LLFRamAccess:Exit\n");
  return status;
}


static STATUS
LLFSetMotorCurrentAndPhase (PAsic chip,
			    LLF_MOTOR_CURRENT_AND_PHASE *
			    MotorCurrentAndPhase)
{
  STATUS status = STATUS_GOOD;
  SANE_Byte MotorPhase;

  DBG (DBG_ASIC, "LLFSetMotorCurrentAndPhase:Enter\n");

  if (MotorCurrentAndPhase->MotorDriverIs3967 == 1)
    {
      MotorPhase = 0xFE;
    }
  else
    {
      MotorPhase = 0xFF;
    }

  DBG (DBG_ASIC, "MotorPhase=0x%x\n", MotorPhase);
  Mustek_SendData (chip, ES02_50_MOTOR_CURRENT_CONTORL, 0x01);

  if (MotorCurrentAndPhase->FillPhase == 0)
    {
      Mustek_SendData (chip, ES01_AB_PWM_CURRENT_CONTROL, 0x00);

      Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
			    MotorCurrentAndPhase->MotorCurrentTableA[0]);
      Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
			    MotorCurrentAndPhase->MotorCurrentTableB[0]);
      Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
			    0x08 & MotorPhase);

      /*2 */
      Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
			    MotorCurrentAndPhase->MotorCurrentTableA[0]);
      Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
			    MotorCurrentAndPhase->MotorCurrentTableB[0]);
      Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
			    0x09 & MotorPhase);

      /*3 */
      Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
			    MotorCurrentAndPhase->MotorCurrentTableA[0]);
      Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
			    MotorCurrentAndPhase->MotorCurrentTableB[0]);
      Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
			    0x01 & MotorPhase);

      /*4 */
      Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
			    MotorCurrentAndPhase->MotorCurrentTableA[0]);
      Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
			    MotorCurrentAndPhase->MotorCurrentTableB[0]);
      Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
			    0x00 & MotorPhase);
    }
  else
    {
      if (MotorCurrentAndPhase->MoveType == _4_TABLE_SPACE_FOR_FULL_STEP)
	{			/* Full Step */
	  Mustek_SendData (chip, ES01_AB_PWM_CURRENT_CONTROL, 0x00);

	  /*1 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				MotorCurrentAndPhase->MotorCurrentTableA[0]);
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				MotorCurrentAndPhase->MotorCurrentTableB[0]);
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x08 & MotorPhase);

	  /*2 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				MotorCurrentAndPhase->MotorCurrentTableA[0]);
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				MotorCurrentAndPhase->MotorCurrentTableB[0]);
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x09 & MotorPhase);

	  /*3 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				MotorCurrentAndPhase->MotorCurrentTableA[0]);
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				MotorCurrentAndPhase->MotorCurrentTableB[0]);
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x01 & MotorPhase);

	  /*4 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				MotorCurrentAndPhase->MotorCurrentTableA[0]);
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				MotorCurrentAndPhase->MotorCurrentTableB[0]);
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x00 & MotorPhase);
	}
      else if (MotorCurrentAndPhase->MoveType ==
	       _8_TABLE_SPACE_FOR_1_DIV_2_STEP)
	{			/* Half Step */
	  Mustek_SendData (chip, ES01_AB_PWM_CURRENT_CONTROL, 0x01);

	  /*1 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				MotorCurrentAndPhase->MotorCurrentTableA[0]);
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				MotorCurrentAndPhase->MotorCurrentTableB[0]);
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x25 & MotorPhase);

	  /*2 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				MotorCurrentAndPhase->MotorCurrentTableA[0]);
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				MotorCurrentAndPhase->MotorCurrentTableB[0]);
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x07 & MotorPhase);

	  /*3 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				MotorCurrentAndPhase->MotorCurrentTableA[0]);
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				MotorCurrentAndPhase->MotorCurrentTableB[0]);
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x24 & MotorPhase);

	  /*4 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				MotorCurrentAndPhase->MotorCurrentTableA[0]);
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				MotorCurrentAndPhase->MotorCurrentTableB[0]);
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x30 & MotorPhase);

	  /*5 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				MotorCurrentAndPhase->MotorCurrentTableA[0]);
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				MotorCurrentAndPhase->MotorCurrentTableB[0]);
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x2c & MotorPhase);

	  /*6 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				MotorCurrentAndPhase->MotorCurrentTableA[0]);
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				MotorCurrentAndPhase->MotorCurrentTableB[0]);
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x0e & MotorPhase);

	  /*7 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				MotorCurrentAndPhase->MotorCurrentTableA[0]);
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				MotorCurrentAndPhase->MotorCurrentTableB[0]);
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x2d & MotorPhase);

	  /*8 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				MotorCurrentAndPhase->MotorCurrentTableA[0]);
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				MotorCurrentAndPhase->MotorCurrentTableB[0]);
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x39 & MotorPhase);
	}
      else if (MotorCurrentAndPhase->MoveType ==
	       _16_TABLE_SPACE_FOR_1_DIV_4_STEP)
	{			/* 1/4 step */
	  Mustek_SendData (chip, ES01_AB_PWM_CURRENT_CONTROL, 0x02);

	  /*1 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (0 *
								     3.141592654
								     * 90 /
								     4 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (0 *
								     3.141592654
								     * 90 /
								     4 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x08 & MotorPhase);

	  /*2 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (1 *
								     3.141592654
								     * 90 /
								     4 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (1 *
								     3.141592654
								     * 90 /
								     4 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x08 & MotorPhase);

	  /*3 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (2 *
								     3.141592654
								     * 90 /
								     4 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (2 *
								     3.141592654
								     * 90 /
								     4 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x08 & MotorPhase);

	  /*4 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (3 *
								     3.141592654
								     * 90 /
								     4 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (3 *
								     3.141592654
								     * 90 /
								     4 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x08 & MotorPhase);

	  /*5 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * cos (0 *
								     3.141592654
								     * 90 /
								     4 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * sin (0 *
								     3.141592654
								     * 90 /
								     4 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x09 & MotorPhase);

	  /*6 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * cos (1 *
								     3.141592654
								     * 90 /
								     4 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * sin (1 *
								     3.141592654
								     * 90 /
								     4 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x09 & MotorPhase);

	  /*7 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * cos (2 *
								     3.141592654
								     * 90 /
								     4 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * sin (2 *
								     3.141592654
								     * 90 /
								     4 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x09 & MotorPhase);

	  /*8 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * cos (3 *
								     3.141592654
								     * 90 /
								     4 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * sin (3 *
								     3.141592654
								     * 90 /
								     4 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x09 & MotorPhase);

	  /*9 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (0 *
								     3.141592654
								     * 90 /
								     4 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (0 *
								     3.141592654
								     * 90 /
								     4 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x01 & MotorPhase);

	  /*10 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (1 *
								     3.141592654
								     * 90 /
								     4 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (1 *
								     3.141592654
								     * 90 /
								     4 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x01 & MotorPhase);

	  /*11 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (2 *
								     3.141592654
								     * 90 /
								     4 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (2 *
								     3.141592654
								     * 90 /
								     4 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x01 & MotorPhase);

	  /*12 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (3 *
								     3.141592654
								     * 90 /
								     4 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (3 *
								     3.141592654
								     * 90 /
								     4 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x01 & MotorPhase);

	  /*13 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * cos (0 *
								     3.141592654
								     * 90 /
								     4 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * sin (0 *
								     3.141592654
								     * 90 /
								     4 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x00 & MotorPhase);

	  /*14 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * cos (1 *
								     3.141592654
								     * 90 /
								     4 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * sin (1 *
								     3.141592654
								     * 90 /
								     4 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x00 & MotorPhase);

	  /*15 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * cos (2 *
								     3.141592654
								     * 90 /
								     4 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * sin (2 *
								     3.141592654
								     * 90 /
								     4 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x00 & MotorPhase);

	  /*16 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * cos (3 *
								     3.141592654
								     * 90 /
								     4 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * sin (3 *
								     3.141592654
								     * 90 /
								     4 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x00 & MotorPhase);

	}
      else if (MotorCurrentAndPhase->MoveType ==
	       _32_TABLE_SPACE_FOR_1_DIV_8_STEP)
	{
	  Mustek_SendData (chip, ES01_AB_PWM_CURRENT_CONTROL, 0x03);

	  /*1 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (0 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (0 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x00 & MotorPhase);

	  /*2 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (1 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (1 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x00 & MotorPhase);

	  /*3 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (2 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (2 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x00 & MotorPhase);

	  /*4 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (3 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (3 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x00 & MotorPhase);

	  /*5 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (4 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (4 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x00 & MotorPhase);

	  /*6 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (5 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (5 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x00 & MotorPhase);

	  /*7 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (6 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (6 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x00 & MotorPhase);

	  /*8 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (7 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));

	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (7 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x00 & MotorPhase);

	  /*9 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (0 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (0 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x08 & MotorPhase);

	  /*10 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (1 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (1 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x08 & MotorPhase);

	  /*11 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (2 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (2 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x08 & MotorPhase);

	  /*12 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (3 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (3 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x08 & MotorPhase);

	  /*13 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (4 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (4 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x08 & MotorPhase);

	  /*14 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (5 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (5 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x08 & MotorPhase);

	  /*15 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (6 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (6 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x08 & MotorPhase);

	  /*16 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (7 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (7 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x08 & MotorPhase);

	  /*17 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (0 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (0 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x09 & MotorPhase);

	  /*18 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (1 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (1 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x09 & MotorPhase);

	  /*19 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (2 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (2 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x09 & MotorPhase);

	  /*20 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (3 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (3 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x09 & MotorPhase);

	  /*21 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (4 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (4 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x09 & MotorPhase);

	  /*22 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (5 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (5 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x09 & MotorPhase);

	  /*23 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (6 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (6 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x09 & MotorPhase);

	  /*24 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (7 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (7 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x09 & MotorPhase);

	  /*25 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (0 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (0 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x01 & MotorPhase);

	  /*26 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (1 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (1 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x01 & MotorPhase);

	  /*27 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (2 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (2 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x01 & MotorPhase);

	  /*28 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (3 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (3 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x01 & MotorPhase);

	  /*29 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (4 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (4 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x01 & MotorPhase);

	  /*30 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (5 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (5 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x01 & MotorPhase);

	  /*31 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (6 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (6 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x01 & MotorPhase);

	  /*32 */
	  Mustek_SendData2Byte (chip, ES02_52_MOTOR_CURRENT_TABLE_A,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableA[0] * sin (7 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_53_MOTOR_CURRENT_TABLE_B,
				(SANE_Byte) (MotorCurrentAndPhase->
					MotorCurrentTableB[0] * cos (7 *
								     3.141592654
								     * 90 /
								     8 /
								     180)));
	  Mustek_SendData2Byte (chip, ES02_51_MOTOR_PHASE_TABLE_1,
				0x01 & MotorPhase);

	}
    }

  if (MotorCurrentAndPhase->FillPhase != 0)
    {
      Mustek_SendData (chip, ES02_50_MOTOR_CURRENT_CONTORL,
		       0x00 | MotorCurrentAndPhase->MoveType);
    }
  else
    {
      Mustek_SendData (chip, ES02_50_MOTOR_CURRENT_CONTORL, 0x00);
    }

  DBG (DBG_ASIC, "LLFSetMotorCurrentAndPhase:Exit\n");
  return status;
}


#if SANE_UNUSED
static STATUS
LLFStopMotorMove (PAsic chip)
{
  STATUS status = STATUS_GOOD;
  DBG (DBG_ASIC, "LLFStopMotorMove:Enter\n");

  Mustek_SendData (chip, ES01_F4_ActiveTriger, ACTION_TRIGER_DISABLE);

  Asic_WaitUnitReady (chip);

  DBG (DBG_ASIC, "LLFStopMotorMove:Exit\n");
  return status;
}
#endif

static STATUS
LLFSetMotorTable (PAsic chip, LLF_SETMOTORTABLE * LLF_SetMotorTable)
{
  STATUS status = STATUS_GOOD;
  LLF_RAMACCESS RamAccess;

  DBG (DBG_ASIC, "LLFSetMotorTable:Enter\n");
  if (LLF_SetMotorTable->MotorTablePtr != NULL)
    {
      RamAccess.ReadWrite = WRITE_RAM;
      RamAccess.IsOnChipGamma = EXTERNAL_RAM;
      RamAccess.DramDelayTime = SDRAMCLK_DELAY_12_ns;

      RamAccess.LoStartAddress = 0;
      RamAccess.LoStartAddress |= LLF_SetMotorTable->MotorTableAddress;
      RamAccess.LoStartAddress <<= TABLE_OFFSET_BASE;
      RamAccess.LoStartAddress |= 0x3000;

      RamAccess.HiStartAddress = 0;
      RamAccess.HiStartAddress |= LLF_SetMotorTable->MotorTableAddress;
      RamAccess.HiStartAddress >>= (16 - TABLE_OFFSET_BASE);

      RamAccess.RwSize = 512 * 2 * 8;	/* BYTE */
      RamAccess.BufferPtr = (SANE_Byte *) LLF_SetMotorTable->MotorTablePtr;

      LLFRamAccess (chip, &RamAccess);

      /* tell scan chip the motor table address, unit is 2^14 words */
      Mustek_SendData (chip, ES01_9D_MotorTableAddrA14_A21,
		       LLF_SetMotorTable->MotorTableAddress);
    }

  DBG (DBG_ASIC, "LLFSetMotorTable:Exit\n");
  return status;
}

static STATUS
LLFMotorMove (PAsic chip, LLF_MOTORMOVE * LLF_MotorMove)
{
  STATUS status = STATUS_GOOD;
  unsigned int motor_steps;
  SANE_Byte temp_motor_action;
  SANE_Byte temp_status;

  DBG (DBG_ASIC, "LLFMotorMove:Enter\n");

  Mustek_SendData (chip, ES01_F4_ActiveTriger, ACTION_TRIGER_DISABLE);

  status = Asic_WaitUnitReady (chip);

  DBG (DBG_ASIC, "Set start/end pixel\n");

  Mustek_SendData (chip, ES01_B8_ChannelRedExpStartPixelLSB, LOBYTE (100));
  Mustek_SendData (chip, ES01_B9_ChannelRedExpStartPixelMSB, HIBYTE (100));
  Mustek_SendData (chip, ES01_BA_ChannelRedExpEndPixelLSB, LOBYTE (101));
  Mustek_SendData (chip, ES01_BB_ChannelRedExpEndPixelMSB, HIBYTE (101));

  Mustek_SendData (chip, ES01_BC_ChannelGreenExpStartPixelLSB, LOBYTE (100));
  Mustek_SendData (chip, ES01_BD_ChannelGreenExpStartPixelMSB, HIBYTE (100));
  Mustek_SendData (chip, ES01_BE_ChannelGreenExpEndPixelLSB, LOBYTE (101));
  Mustek_SendData (chip, ES01_BF_ChannelGreenExpEndPixelMSB, HIBYTE (101));

  Mustek_SendData (chip, ES01_C0_ChannelBlueExpStartPixelLSB, LOBYTE (100));
  Mustek_SendData (chip, ES01_C1_ChannelBlueExpStartPixelMSB, HIBYTE (100));
  Mustek_SendData (chip, ES01_C2_ChannelBlueExpEndPixelLSB, LOBYTE (101));
  Mustek_SendData (chip, ES01_C3_ChannelBlueExpEndPixelMSB, HIBYTE (101));

  /*set motor accelerate steps MAX 511 steps */
  Mustek_SendData (chip, ES01_E0_MotorAccStep0_7,
		   LOBYTE (LLF_MotorMove->AccStep));
  Mustek_SendData (chip, ES01_E1_MotorAccStep8_8,
		   HIBYTE (LLF_MotorMove->AccStep));
  DBG (DBG_ASIC, "AccStep=%d\n", LLF_MotorMove->AccStep);

  Mustek_SendData (chip, ES01_E2_MotorStepOfMaxSpeed0_7,
		   LOBYTE (LLF_MotorMove->FixMoveSteps));
  Mustek_SendData (chip, ES01_E3_MotorStepOfMaxSpeed8_15,
		   HIBYTE (LLF_MotorMove->FixMoveSteps));
  Mustek_SendData (chip, ES01_E4_MotorStepOfMaxSpeed16_19, 0);
  DBG (DBG_ASIC, "FixMoveSteps=%d\n", LLF_MotorMove->FixMoveSteps);

  /*set motor decelerate steps MAX 255 steps */
  Mustek_SendData (chip, ES01_E5_MotorDecStep, LLF_MotorMove->DecStep);
  DBG (DBG_ASIC, "DecStep=%d\n", LLF_MotorMove->DecStep);

  /*set motor uniform speed only for uniform speed 
     //only used for UNIFORM_MOTOR_AND_SCAN_SPEED_ENABLE
     //If you use acc mode, this two reg are not used. */
  Mustek_SendData (chip, ES01_FD_MotorFixedspeedLSB,
		   LOBYTE (LLF_MotorMove->FixMoveSpeed));
  Mustek_SendData (chip, ES01_FE_MotorFixedspeedMSB,
		   HIBYTE (LLF_MotorMove->FixMoveSpeed));
  DBG (DBG_ASIC, "FixMoveSpeed=%d\n", LLF_MotorMove->FixMoveSpeed);

  /*Set motor type */
  Mustek_SendData (chip, ES01_A6_MotorOption, LLF_MotorMove->MotorSelect |
		   LLF_MotorMove->HomeSensorSelect | LLF_MotorMove->
		   MotorMoveUnit);

  /*Set motor speed unit, for all motor mode,
     //inclue uniform, acc, motor speed of scan */
  Mustek_SendData (chip, ES01_F6_MorotControl1,
		   LLF_MotorMove->MotorSpeedUnit | LLF_MotorMove->
		   MotorSyncUnit);

  /* below is setting action register */
  if (LLF_MotorMove->ActionType == ACTION_TYPE_BACKTOHOME)
    {
      DBG (DBG_ASIC, "ACTION_TYPE_BACKTOHOME\n");

      temp_motor_action = MOTOR_BACK_HOME_AFTER_SCAN_ENABLE;
      motor_steps = 30000 * 2;
    }
  else
    {
      DBG (DBG_ASIC, "Forward or Backward\n");
      temp_motor_action = MOTOR_MOVE_TO_FIRST_LINE_ENABLE;
      motor_steps = LLF_MotorMove->FixMoveSteps;

      if (LLF_MotorMove->ActionType == ACTION_TYPE_BACKWARD)
	{
	  DBG (DBG_ASIC, "ACTION_TYPE_BACKWARD\n");
	  temp_motor_action =
	    temp_motor_action | INVERT_MOTOR_DIRECTION_ENABLE;
	}
    }

  if (LLF_MotorMove->ActionType == ACTION_TYPE_TEST_MODE)
    {
      DBG (DBG_ASIC, "ACTION_TYPE_TEST_MODE\n");
      temp_motor_action = temp_motor_action |
	MOTOR_MOVE_TO_FIRST_LINE_ENABLE |
	MOTOR_BACK_HOME_AFTER_SCAN_ENABLE | MOTOR_TEST_LOOP_ENABLE;
    }

  Mustek_SendData (chip, ES01_94_PowerSaveControl,
		   0x27 | LLF_MotorMove->Lamp0PwmFreq | LLF_MotorMove->
		   Lamp1PwmFreq);

  /* fix speed move steps */
  Mustek_SendData (chip, ES01_E2_MotorStepOfMaxSpeed0_7,
		   LOBYTE (motor_steps));
  Mustek_SendData (chip, ES01_E3_MotorStepOfMaxSpeed8_15,
		   HIBYTE (motor_steps));
  Mustek_SendData (chip, ES01_E4_MotorStepOfMaxSpeed16_19,
		   (SANE_Byte) ((motor_steps & 0x00ff0000) >> 16));
  DBG (DBG_ASIC, "motor_steps=%d\n", motor_steps);
  DBG (DBG_ASIC, "LOBYTE(motor_steps)=%d\n", LOBYTE (motor_steps));
  DBG (DBG_ASIC, "HIBYTE(motor_steps)=%d\n", HIBYTE (motor_steps));
  DBG (DBG_ASIC, "(SANE_Byte)((motor_steps & 0x00ff0000) >> 16)=%d\n",
       (SANE_Byte) ((motor_steps & 0x00ff0000) >> 16));

  if (LLF_MotorMove->ActionMode == ACTION_MODE_UNIFORM_SPEED_MOVE)
    {
      temp_motor_action =
	temp_motor_action | UNIFORM_MOTOR_AND_SCAN_SPEED_ENABLE;
    }

  Mustek_SendData (chip, ES01_F3_ActionOption, SCAN_DISABLE |
		   SCAN_BACK_TRACKING_DISABLE | temp_motor_action);
  Mustek_SendData (chip, ES01_F4_ActiveTriger, ACTION_TRIGER_ENABLE);

  temp_status = 0;
  if (LLF_MotorMove->WaitOrNoWait == 1)
    {
      if (LLF_MotorMove->ActionType == ACTION_TYPE_BACKTOHOME)
	{
	  DBG (DBG_ASIC, "ACTION_TYPE_BACKTOHOME\n");
	  Asic_WaitCarriageHome (chip, FALSE);
	}
      else
	{
	  Asic_WaitUnitReady (chip);
	}
    }

  DBG (DBG_ASIC, "LLFMotorMove:Exit\n");
  return status;
}

static STATUS
SetMotorStepTable (PAsic chip, LLF_MOTORMOVE * MotorStepsTable, unsigned short wStartY,
		   unsigned int dwScanImageSteps, unsigned short wYResolution)
{
  STATUS status = STATUS_GOOD;
  unsigned short wAccSteps = 511;
  unsigned short wForwardSteps = 20;
  SANE_Byte bDecSteps = 255;
  unsigned short wMotorSycnPixelNumber = 0;
  unsigned short wScanAccSteps = 511;
  SANE_Byte bScanDecSteps = 255;
  unsigned short wFixScanSteps = 20;
  unsigned short wScanBackTrackingSteps = 40;
  unsigned short wScanRestartSteps = 40;
  unsigned short wScanBackHomeExtSteps = 100;
  unsigned int dwTotalMotorSteps;

  DBG (DBG_ASIC, "SetMotorStepTable:Enter\n");

  dwTotalMotorSteps = dwScanImageSteps;

  switch (wYResolution)
    {
    case 2400:
    case 1200:
      wScanAccSteps = 100;
      bScanDecSteps = 10;
      wScanBackTrackingSteps = 10;
      wScanRestartSteps = 10;
      break;
    case 600:
    case 300:
      wScanAccSteps = 300;
      bScanDecSteps = 40;
      break;
    case 150:
      wScanAccSteps = 300;
      bScanDecSteps = 40;
      break;
    case 100:
    case 75:
    case 50:
      wScanAccSteps = 300;
      bScanDecSteps = 40;
      break;
    }

  if (wStartY < (wAccSteps + wForwardSteps + bDecSteps + wScanAccSteps))	/*not including T0,T1 steps */
    {
      wAccSteps = 1;
      bDecSteps = 1;
      wFixScanSteps = (wStartY - wScanAccSteps) > 0 ?
	(wStartY - wScanAccSteps) : 0;
      wForwardSteps = 0;

      chip->isMotorGoToFirstLine = MOTOR_MOVE_TO_FIRST_LINE_DISABLE;
    }
  else
    {
      wForwardSteps =
	(wStartY - wAccSteps - (unsigned short) bDecSteps - wScanAccSteps -
	 wFixScanSteps) >
	0 ? (wStartY - wAccSteps - (unsigned short) bDecSteps - wScanAccSteps -
	     wFixScanSteps) : 0;

      chip->isMotorGoToFirstLine = MOTOR_MOVE_TO_FIRST_LINE_ENABLE;
    }

  dwTotalMotorSteps += wAccSteps;
  dwTotalMotorSteps += wForwardSteps;
  dwTotalMotorSteps += bDecSteps;
  dwTotalMotorSteps += wScanAccSteps;
  dwTotalMotorSteps += wFixScanSteps;
  dwTotalMotorSteps += bScanDecSteps;
  dwTotalMotorSteps += 2;


  MotorStepsTable->AccStep = wAccSteps;
  MotorStepsTable->DecStep = bDecSteps;
  MotorStepsTable->wForwardSteps = wForwardSteps;
  MotorStepsTable->wScanAccSteps = wScanAccSteps;
  MotorStepsTable->bScanDecSteps = bScanDecSteps;
  MotorStepsTable->wFixScanSteps = wFixScanSteps;
  MotorStepsTable->MotorSyncUnit = (SANE_Byte) wMotorSycnPixelNumber;
  MotorStepsTable->wScanBackHomeExtSteps = wScanBackHomeExtSteps;
  MotorStepsTable->wScanRestartSteps = wScanRestartSteps;
  MotorStepsTable->wScanBackTrackingSteps = wScanBackTrackingSteps;

  /*state 1 */
  Mustek_SendData (chip, ES01_E0_MotorAccStep0_7, LOBYTE (wAccSteps));
  Mustek_SendData (chip, ES01_E1_MotorAccStep8_8, HIBYTE (wAccSteps));
  /*state 2 */
  Mustek_SendData (chip, ES01_E2_MotorStepOfMaxSpeed0_7,
		   LOBYTE (wForwardSteps));
  Mustek_SendData (chip, ES01_E3_MotorStepOfMaxSpeed8_15,
		   HIBYTE (wForwardSteps));
  Mustek_SendData (chip, ES01_E4_MotorStepOfMaxSpeed16_19, 0);
  /*state 3 */
  Mustek_SendData (chip, ES01_E5_MotorDecStep, bDecSteps);
  /*state 4 */
  Mustek_SendData (chip, ES01_AE_MotorSyncPixelNumberM16LSB,
		   LOBYTE (wMotorSycnPixelNumber));
  Mustek_SendData (chip, ES01_AF_MotorSyncPixelNumberM16MSB,
		   HIBYTE (wMotorSycnPixelNumber));
  /*state 5 */
  Mustek_SendData (chip, ES01_EC_ScanAccStep0_7, LOBYTE (wScanAccSteps));
  Mustek_SendData (chip, ES01_ED_ScanAccStep8_8, HIBYTE (wScanAccSteps));
  /*state 6 */
  Mustek_SendData (chip, ES01_EE_FixScanStepLSB, LOBYTE (wFixScanSteps));
  Mustek_SendData (chip, ES01_8A_FixScanStepMSB, HIBYTE (wFixScanSteps));
  /*state 8 */
  Mustek_SendData (chip, ES01_EF_ScanDecStep, bScanDecSteps);
  /*state 10 */
  Mustek_SendData (chip, ES01_E6_ScanBackTrackingStepLSB,
		   LOBYTE (wScanBackTrackingSteps));
  Mustek_SendData (chip, ES01_E7_ScanBackTrackingStepMSB,
		   HIBYTE (wScanBackTrackingSteps));
  /*state 15 */
  Mustek_SendData (chip, ES01_E8_ScanRestartStepLSB,
		   LOBYTE (wScanRestartSteps));
  Mustek_SendData (chip, ES01_E9_ScanRestartStepMSB,
		   HIBYTE (wScanRestartSteps));
  /*state 19 */
  Mustek_SendData (chip, ES01_EA_ScanBackHomeExtStepLSB,
		   LOBYTE (wScanBackHomeExtSteps));
  Mustek_SendData (chip, ES01_EB_ScanBackHomeExtStepMSB,
		   HIBYTE (wScanBackHomeExtSteps));

  /*total motor steps */
  Mustek_SendData (chip, ES01_F0_ScanImageStep0_7,
		   LOBYTE (dwTotalMotorSteps));
  Mustek_SendData (chip, ES01_F1_ScanImageStep8_15,
		   HIBYTE (dwTotalMotorSteps));
  Mustek_SendData (chip, ES01_F2_ScanImageStep16_19,
		   (SANE_Byte) ((dwTotalMotorSteps & 0x00ff0000) >> 16));

  DBG (DBG_ASIC, "SetMotorStepTable:Exit\n");
  return status;
}

static STATUS
CalculateMotorTable (LLF_CALCULATEMOTORTABLE * lpCalculateMotorTable,
		     unsigned short wYResolution)
{
  STATUS status = STATUS_GOOD;
  unsigned short i;
  unsigned short wEndSpeed, wStartSpeed;
  unsigned short wScanAccSteps;
  SANE_Byte bScanDecSteps;
  double PI = 3.1415926;
  double x = PI / 2;
  long double y;
  unsigned short *lpMotorTable;

  DBG (DBG_ASIC, "CalculateMotorTable:Enter\n");

  wStartSpeed = lpCalculateMotorTable->StartSpeed;
  wEndSpeed = lpCalculateMotorTable->EndSpeed;
  wScanAccSteps = lpCalculateMotorTable->AccStepBeforeScan;
  bScanDecSteps = lpCalculateMotorTable->DecStepAfterScan;
  lpMotorTable = lpCalculateMotorTable->lpMotorTable;

  /*Motor T0 & T6 Acc Table */
  for (i = 0; i < 512; i++)
    {
      y = (6000 - 3500);
      y *= (pow (0.09, (x * i) / 512) - pow (0.09, (x * 511) / 512));
      y += 4500;
      *((unsigned short *) lpMotorTable + i) = (unsigned short) y;	/*T0 */
      *((unsigned short *) lpMotorTable + i + 512 * 6) = (unsigned short) y;	/*T6 */
    }

  /*Motor T1 & T7 Dec Table */
  for (i = 0; i < 256; i++)
    {
      y = (6000 - 3500);
      y *= pow (0.3, (x * i) / 256);
      y = 6000 - y;
      *((unsigned short *) lpMotorTable + i + 512) = (unsigned short) y;	/*T1 */
      *((unsigned short *) lpMotorTable + i + 512 * 7) = (unsigned short) y;	/*T7 */
    }

  switch (wYResolution)
    {
    case 2400:
    case 1200:
    case 600:
    case 300:
    case 150:
    case 100:
    case 75:
    case 50:
      for (i = 0; i < wScanAccSteps; i++)
	{
	  y = (wStartSpeed - wEndSpeed);
	  y *=
	    (pow (0.09, (x * i) / wScanAccSteps) -
	     pow (0.09, (x * (wScanAccSteps - 1)) / wScanAccSteps));
	  y += wEndSpeed;
	  *((unsigned short *) lpMotorTable + i + 512 * 2) = (unsigned short) y;	/*T2 */
	  *((unsigned short *) lpMotorTable + i + 512 * 4) = (unsigned short) y;	/*T4 */
	}
      for (i = wScanAccSteps; i < 512; i++)
	{
	  *((unsigned short *) lpMotorTable + i + 512 * 2) = (unsigned short) wEndSpeed;	/*T2 */
	  *((unsigned short *) lpMotorTable + i + 512 * 4) = (unsigned short) wEndSpeed;	/*T4 */
	}


      for (i = 0; i < (unsigned short) bScanDecSteps; i++)
	{
	  y = (wStartSpeed - wEndSpeed);
	  y *= pow (0.3, (x * i) / bScanDecSteps);
	  y = wStartSpeed - y;
	  *((unsigned short *) lpMotorTable + i + 512 * 3) = (unsigned short) (y);	/*T3 */
	  *((unsigned short *) lpMotorTable + i + 512 * 5) = (unsigned short) (y);	/*T5 */
	}
      for (i = bScanDecSteps; i < 256; i++)
	{
	  *((unsigned short *) lpMotorTable + i + 512 * 3) = (unsigned short) wStartSpeed;	/*T3 */
	  *((unsigned short *) lpMotorTable + i + 512 * 5) = (unsigned short) wStartSpeed;	/*T5 */
	}
      break;
    }

  DBG (DBG_ASIC, "CalculateMotorTable:Exit\n");
  return status;
}

static STATUS
LLFCalculateMotorTable (LLF_CALCULATEMOTORTABLE * LLF_CalculateMotorTable)
{
  STATUS status = STATUS_GOOD;
  unsigned short i;
  double PI = 3.1415926535;
  double x;

  DBG (DBG_ASIC, "LLF_CALCULATEMOTORTABLE:Enter\n");

  x = PI / 2;

  for (i = 0; i < 512; i++)
    {
      /* befor scan acc table */
      *(LLF_CalculateMotorTable->lpMotorTable + i) =
	(unsigned short) ((LLF_CalculateMotorTable->StartSpeed -
		 LLF_CalculateMotorTable->EndSpeed) * pow (0.09,
							   (x * i) / 512) +
		LLF_CalculateMotorTable->EndSpeed);
      *(LLF_CalculateMotorTable->lpMotorTable + i + 512 * 2) =
	(unsigned short) ((LLF_CalculateMotorTable->StartSpeed -
		 LLF_CalculateMotorTable->EndSpeed) * pow (0.09,
							   (x * i) / 512) +
		LLF_CalculateMotorTable->EndSpeed);
      *(LLF_CalculateMotorTable->lpMotorTable + i + 512 * 4) =
	(unsigned short) ((LLF_CalculateMotorTable->StartSpeed -
		 LLF_CalculateMotorTable->EndSpeed) * pow (0.09,
							   (x * i) / 512) +
		LLF_CalculateMotorTable->EndSpeed);
      *(LLF_CalculateMotorTable->lpMotorTable + i + 512 * 6) =
	(unsigned short) ((LLF_CalculateMotorTable->StartSpeed -
		 LLF_CalculateMotorTable->EndSpeed) * pow (0.09,
							   (x * i) / 512) +
		LLF_CalculateMotorTable->EndSpeed);
    }

  for (i = 0; i < 255; i++)
    {
      *(LLF_CalculateMotorTable->lpMotorTable + i + 512) =
	(unsigned short) (LLF_CalculateMotorTable->StartSpeed -
		(LLF_CalculateMotorTable->StartSpeed -
		 LLF_CalculateMotorTable->EndSpeed) * pow (0.3,
							   (x * i) / 256));
      *(LLF_CalculateMotorTable->lpMotorTable + i + 512 * 3) =
	(unsigned short) (LLF_CalculateMotorTable->StartSpeed -
		(LLF_CalculateMotorTable->StartSpeed -
		 LLF_CalculateMotorTable->EndSpeed) * pow (0.3,
							   (x * i) / 256));
      *(LLF_CalculateMotorTable->lpMotorTable + i + 512 * 5) =
	(unsigned short) (LLF_CalculateMotorTable->StartSpeed -
		(LLF_CalculateMotorTable->StartSpeed -
		 LLF_CalculateMotorTable->EndSpeed) * pow (0.3,
							   (x * i) / 256));
      *(LLF_CalculateMotorTable->lpMotorTable + i + 512 * 7) =
	(unsigned short) (LLF_CalculateMotorTable->StartSpeed -
		(LLF_CalculateMotorTable->StartSpeed -
		 LLF_CalculateMotorTable->EndSpeed) * pow (0.3,
							   (x * i) / 256));
    }

  for (i = 0; i < 512; i++)
    {				/* back acc table */
      *(LLF_CalculateMotorTable->lpMotorTable + i) =
	(unsigned short) ((LLF_CalculateMotorTable->StartSpeed -
		 LLF_CalculateMotorTable->EndSpeed) * pow (0.09,
							   (x * i) / 512) +
		LLF_CalculateMotorTable->EndSpeed);
      *(LLF_CalculateMotorTable->lpMotorTable + i + 512 * 6) =
	(unsigned short) ((LLF_CalculateMotorTable->StartSpeed -
		 LLF_CalculateMotorTable->EndSpeed) * pow (0.09,
							   (x * i) / 512) +
		LLF_CalculateMotorTable->EndSpeed);
    }

  if (LLF_CalculateMotorTable->AccStepBeforeScan == 0)
    {
    }
  else
    {
      for (i = 0; i < LLF_CalculateMotorTable->AccStepBeforeScan; i++)
	{
	  *(LLF_CalculateMotorTable->lpMotorTable + i + 512 * 2) =
	    (unsigned short) ((LLF_CalculateMotorTable->StartSpeed -
		     LLF_CalculateMotorTable->EndSpeed) * (pow (0.09,
								(x * i) /
								LLF_CalculateMotorTable->
								AccStepBeforeScan)
							   - pow (0.09,
								  (x *
								   (LLF_CalculateMotorTable->
								    AccStepBeforeScan
								    -
								    1)) /
								  LLF_CalculateMotorTable->
								  AccStepBeforeScan))
		    + LLF_CalculateMotorTable->EndSpeed);
	}
    }

  DBG (DBG_ASIC, "LLF_CALCULATEMOTORTABLE:Exit\n");
  return status;
}


static STATUS
SetMotorCurrent (PAsic chip, unsigned short dwMotorSpeed,
		 LLF_MOTOR_CURRENT_AND_PHASE * CurrentPhase)
{
  STATUS status = STATUS_GOOD;
  DBG (DBG_ASIC, "SetMotorCurrent:Enter\n");

  chip = chip;

  if (dwMotorSpeed < 2000)
    {
      CurrentPhase->MotorCurrentTableA[0] = 255;
      CurrentPhase->MotorCurrentTableB[0] = 255;
    }
  else if (dwMotorSpeed < 3500)
    {
      CurrentPhase->MotorCurrentTableA[0] = 200;
      CurrentPhase->MotorCurrentTableB[0] = 200;
    }
  else if (dwMotorSpeed < 5000)
    {
      CurrentPhase->MotorCurrentTableA[0] = 160;
      CurrentPhase->MotorCurrentTableB[0] = 160;
    }
  else if (dwMotorSpeed < 10000)
    {
      CurrentPhase->MotorCurrentTableA[0] = 70;
      CurrentPhase->MotorCurrentTableB[0] = 70;
    }
  else if (dwMotorSpeed < 17000)
    {
      CurrentPhase->MotorCurrentTableA[0] = 60;
      CurrentPhase->MotorCurrentTableB[0] = 60;
    }
  else if (dwMotorSpeed < 25000)
    {
      CurrentPhase->MotorCurrentTableA[0] = 50;
      CurrentPhase->MotorCurrentTableB[0] = 50;
    }
  else
    {
      CurrentPhase->MotorCurrentTableA[0] = 50;
      CurrentPhase->MotorCurrentTableB[0] = 50;
    }

  DBG (DBG_ASIC, "SetMotorCurrent:Exit\n");
  return status;
}


static STATUS
MotorBackHome (PAsic chip, SANE_Byte WaitOrNoWait)
{
  STATUS status = STATUS_GOOD;
  unsigned short BackHomeMotorTable[512 * 8];
  LLF_CALCULATEMOTORTABLE CalMotorTable;
  LLF_MOTOR_CURRENT_AND_PHASE CurrentPhase;
  LLF_SETMOTORTABLE LLF_SetMotorTable;
  LLF_MOTORMOVE MotorMove;

  DBG (DBG_ASIC, "MotorBackHome:Enter\n");

  CalMotorTable.StartSpeed = 5000;
  CalMotorTable.EndSpeed = 1200;
  CalMotorTable.AccStepBeforeScan = 511;
  CalMotorTable.DecStepAfterScan = 255;
  CalMotorTable.lpMotorTable = BackHomeMotorTable;
  LLFCalculateMotorTable (&CalMotorTable);


  CurrentPhase.MotorCurrentTableA[0] = 220;
  CurrentPhase.MotorCurrentTableB[0] = 220;
  CurrentPhase.MoveType = _4_TABLE_SPACE_FOR_FULL_STEP;
  LLFSetMotorCurrentAndPhase (chip, &CurrentPhase);

  LLF_SetMotorTable.MotorTableAddress = 0;
  LLF_SetMotorTable.MotorTablePtr = BackHomeMotorTable;
  LLFSetMotorTable (chip, &LLF_SetMotorTable);

  MotorMove.MotorSelect = MOTOR_0_ENABLE | MOTOR_1_DISABLE;
  MotorMove.MotorMoveUnit = ES03_TABLE_DEFINE;
  MotorMove.MotorSpeedUnit = SPEED_UNIT_1_PIXEL_TIME;
  MotorMove.MotorSyncUnit = MOTOR_SYNC_UNIT_1_PIXEL_TIME;
  MotorMove.HomeSensorSelect = HOME_SENSOR_0_ENABLE;
  MotorMove.ActionMode = ACTION_MODE_ACCDEC_MOVE;
  MotorMove.ActionType = ACTION_TYPE_BACKTOHOME;

  MotorMove.AccStep = 511;
  MotorMove.DecStep = 255;
  MotorMove.FixMoveSteps = 0;
  MotorMove.FixMoveSpeed = 3000;
  MotorMove.WaitOrNoWait = WaitOrNoWait;
  LLFMotorMove (chip, &MotorMove);

  DBG (DBG_ASIC, "MotorBackHome:Exit\n");
  return status;
}


static STATUS
LLFSetRamAddress (PAsic chip, unsigned int dwStartAddr, unsigned int dwEndAddr,
		  SANE_Byte byAccessTarget)
{
  STATUS status = STATUS_GOOD;
  SANE_Byte * pStartAddr = (SANE_Byte *) & dwStartAddr;
  SANE_Byte * pEndAddr = (SANE_Byte *) & dwEndAddr;

  DBG (DBG_ASIC, "LLFSetRamAddress:Enter\n");

  /*Set start address. */
  Mustek_SendData (chip, ES01_A0_HostStartAddr0_7, *(pStartAddr));
  Mustek_SendData (chip, ES01_A1_HostStartAddr8_15, *(pStartAddr + 1));
  if (byAccessTarget == ACCESS_DRAM)
    Mustek_SendData (chip, ES01_A2_HostStartAddr16_21,
		     *(pStartAddr + 2) | ACCESS_DRAM);
  else
    Mustek_SendData (chip, ES01_A2_HostStartAddr16_21,
		     *(pStartAddr + 2) | ACCESS_GAMMA_RAM);

  /*Set end address. */
  Mustek_SendData (chip, ES01_A3_HostEndAddr0_7, *(pEndAddr));
  Mustek_SendData (chip, ES01_A4_HostEndAddr8_15, *(pEndAddr + 1));
  Mustek_SendData (chip, ES01_A5_HostEndAddr16_21, *(pEndAddr + 2));

  Mustek_ClearFIFO (chip);

  DBG (DBG_ASIC, "LLFSetRamAddress:Exit\n");
  return status;
}



/* ---------------------- medium level asic functions ---------------------- */

static STATUS
InitTiming (PAsic chip)
{
  STATUS status = STATUS_GOOD;
  DBG (DBG_ASIC, "InitTiming:Enter\n");

  chip->Timing.AFE_ADCCLK_Timing = 1010580480;
  chip->Timing.AFE_ADCVS_Timing = 12582912;
  chip->Timing.AFE_ADCRS_Timing = 3072;
  chip->Timing.AFE_ChannelA_LatchPos = 3080;
  chip->Timing.AFE_ChannelB_LatchPos = 3602;
  chip->Timing.AFE_ChannelC_LatchPos = 5634;
  chip->Timing.AFE_ChannelD_LatchPos = 1546;
  chip->Timing.AFE_Secondary_FF_LatchPos = 12;

  /* Sensor */
  chip->Timing.CCD_DummyCycleTiming = 0;
  chip->Timing.PHTG_PluseWidth = 12;
  chip->Timing.PHTG_WaitWidth = 1;
  chip->Timing.PHTG_TimingAdj = 1;
  chip->Timing.PHTG_TimingSetup = 0;
  chip->Timing.ChannelR_StartPixel = 100;
  chip->Timing.ChannelR_EndPixel = 200;
  chip->Timing.ChannelG_StartPixel = 100;
  chip->Timing.ChannelG_EndPixel = 200;
  chip->Timing.ChannelB_StartPixel = 100;
  chip->Timing.ChannelB_EndPixel = 200;

  /*1200dpi Timing */
  chip->Timing.CCD_PH2_Timing_1200 = 1048320;
  chip->Timing.CCD_PHRS_Timing_1200 = 983040;
  chip->Timing.CCD_PHCP_Timing_1200 = 61440;
  chip->Timing.CCD_PH1_Timing_1200 = 4293918720u;
  chip->Timing.DE_CCD_SETUP_REGISTER_1200 = 32;
  chip->Timing.wCCDPixelNumber_1200 = 11250;

  /*600dpi Timing */
  chip->Timing.CCD_PH2_Timing_600 = 1048320;
  chip->Timing.CCD_PHRS_Timing_600 = 983040;
  chip->Timing.CCD_PHCP_Timing_600 = 61440;
  chip->Timing.CCD_PH1_Timing_600 = 4293918720u;
  chip->Timing.DE_CCD_SETUP_REGISTER_600 = 0;
  chip->Timing.wCCDPixelNumber_600 = 7500;

  DBG (DBG_ASIC, "InitTiming:Exit\n");
  return status;
}

static STATUS
OpenScanChip (PAsic chip)
{
  STATUS status = STATUS_GOOD;
  SANE_Byte x[4];

  DBG (DBG_ASIC, "OpenScanChip:Enter\n");

  x[0] = 0x64;
  x[1] = 0x64;
  x[2] = 0x64;
  x[3] = 0x64;
  status = WriteIOControl (chip, 0x90, 0, 4, x);
  if (status != STATUS_GOOD)
    return status;

  x[0] = 0x65;
  x[1] = 0x65;
  x[2] = 0x65;
  x[3] = 0x65;
  status = WriteIOControl (chip, 0x90, 0, 4, x);
  if (status != STATUS_GOOD)
    return status;

  x[0] = 0x44;
  x[1] = 0x44;
  x[2] = 0x44;
  x[3] = 0x44;
  status = WriteIOControl (chip, 0x90, 0, 4, x);
  if (status != STATUS_GOOD)
    return status;

  x[0] = 0x45;
  x[1] = 0x45;
  x[2] = 0x45;
  x[3] = 0x45;
  status = WriteIOControl (chip, 0x90, 0, 4, x);

  DBG (DBG_ASIC, "OpenScanChip: Exit\n");
  return status;
}


static STATUS
CloseScanChip (PAsic chip)
{
  STATUS status = STATUS_GOOD;
  SANE_Byte x[4];

  DBG (DBG_ASIC, "CloseScanChip:Enter\n");

  x[0] = 0x64;
  x[1] = 0x64;
  x[2] = 0x64;
  x[3] = 0x64;
  status = WriteIOControl (chip, 0x90, 0, 4, x);
  if (status != STATUS_GOOD)
    return status;

  x[0] = 0x65;
  x[1] = 0x65;
  x[2] = 0x65;
  x[3] = 0x65;
  status = WriteIOControl (chip, 0x90, 0, 4, x);
  if (status != STATUS_GOOD)
    return status;

  x[0] = 0x16;
  x[1] = 0x16;
  x[2] = 0x16;
  x[3] = 0x16;
  status = WriteIOControl (chip, 0x90, 0, 4, x);
  if (status != STATUS_GOOD)
    return status;

  x[0] = 0x17;
  x[1] = 0x17;
  x[2] = 0x17;
  x[3] = 0x17;
  status = WriteIOControl (chip, 0x90, 0, 4, x);

  DBG (DBG_ASIC, "CloseScanChip: Exit\n");
  return status;
}


static STATUS
SafeInitialChip (PAsic chip)
{
  STATUS status = STATUS_GOOD;

  DBG (DBG_ASIC, "SafeInitialChip:Enter\n");

  Mustek_SendData (chip, ES01_F3_ActionOption, 0);
  Mustek_SendData (chip, ES01_86_DisableAllClockWhenIdle,
		   CLOSE_ALL_CLOCK_DISABLE);
  Mustek_SendData (chip, ES01_F4_ActiveTriger, ACTION_TRIGER_DISABLE);

  status = Asic_WaitUnitReady (chip);

  DBG (DBG_ASIC, "isFirstOpenChip=%d\n", chip->isFirstOpenChip);
  if (chip->isFirstOpenChip)
    {
      DBG (DBG_ASIC, "isFirstOpenChip=%d\n", chip->isFirstOpenChip);
      status = DRAM_Test (chip);
      if (status != STATUS_GOOD)
	{
	  DBG (DBG_ASIC, "DRAM_Test: Error\n");
	  return status;
	}
      chip->isFirstOpenChip = FALSE;
    }

  DBG (DBG_ASIC, "SafeInitialChip: exit\n");
  return status;
}


static STATUS
DRAM_Test (PAsic chip)
{
  STATUS status = STATUS_GOOD;
  unsigned char *temps;
  unsigned int i;

  DBG (DBG_ASIC, "DRAM_Test:Enter\n");

  temps = (unsigned char *) malloc (64);

  for (i = 0; i < 64; i++)
    {
      *(temps + i) = i;
    }

  /*set start address */
  status = Mustek_SendData (chip, ES01_A0_HostStartAddr0_7, 0x00);
  if (status != STATUS_GOOD)
    {
      free (temps);
      return status;
    }

  status = Mustek_SendData (chip, ES01_A1_HostStartAddr8_15, 0x00);
  if (status != STATUS_GOOD)
    {
      free (temps);
      return status;
    }

  status =
    Mustek_SendData (chip, ES01_A2_HostStartAddr16_21, 0x00 | ACCESS_DRAM);
  if (status != STATUS_GOOD)
    {
      free (temps);
      return status;
    }

  Mustek_SendData (chip, ES01_79_AFEMCLK_SDRAMCLK_DELAY_CONTROL,
		   SDRAMCLK_DELAY_12_ns);
  status = Mustek_SendData (chip, ES01_A3_HostEndAddr0_7, 0xff);
  if (status != STATUS_GOOD)
    {
      free (temps);
      return status;
    }

  status = Mustek_SendData (chip, ES01_A4_HostEndAddr8_15, 0xff);
  if (status != STATUS_GOOD)
    {
      free (temps);
      return status;
    }

  status = Mustek_SendData (chip, ES01_A5_HostEndAddr16_21, 0xff);
  if (status != STATUS_GOOD)
    {
      free (temps);
      return status;
    }

  status = Mustek_DMAWrite (chip, 64, (SANE_Byte *) (temps));
  if (status != STATUS_GOOD)
    {
      DBG (DBG_ASIC, "Mustek_DMAWrite error\n");
      free (temps);
      return status;
    }

  status = Mustek_SendData (chip, ES01_A0_HostStartAddr0_7, 0x00);
  if (status != STATUS_GOOD)
    {
      free (temps);
      return status;
    }

  status = Mustek_SendData (chip, ES01_A1_HostStartAddr8_15, 0x00);
  if (status != STATUS_GOOD)
    {
      free (temps);
      return status;
    }

  status =
    Mustek_SendData (chip, ES01_A2_HostStartAddr16_21, 0x00 | ACCESS_DRAM);
  if (status != STATUS_GOOD)
    {
      free (temps);
      return status;
    }

  /*set end address */
  status = Mustek_SendData (chip, ES01_A3_HostEndAddr0_7, 0xff);
  if (status != STATUS_GOOD)
    {
      free (temps);
      return status;
    }

  status = Mustek_SendData (chip, ES01_A4_HostEndAddr8_15, 0xff);
  if (status != STATUS_GOOD)
    {
      free (temps);
      return status;
    }

  status = Mustek_SendData (chip, ES01_A5_HostEndAddr16_21, 0xff);
  if (status != STATUS_GOOD)
    {
      free (temps);
      return status;
    }

  memset (temps, 0, 64);

  status = Mustek_DMARead (chip, 64, (SANE_Byte *) (temps));
  if (status != STATUS_GOOD)
    {
      free (temps);
      return status;
    }

  for (i = 0; i < 60; i += 10)
    {
      DBG (DBG_ASIC, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
	   *(temps + i), *(temps + i + 1), *(temps + i + 2), *(temps + i + 3),
	   *(temps + i + 4), *(temps + i + 5), *(temps + i + 6),
	   *(temps + i + 7), *(temps + i + 8), *(temps + i + 9));
    }

  for (i = 0; i < 64; i++)
    {
      if (*(temps + i) != i)
	{
	  DBG (DBG_ERR, "DRAM Test error...(No.=%d)\n", i + 1);
	  return STATUS_IO_ERROR;
	}
    }

  free (temps);

  DBG (DBG_ASIC, "DRAM_Text: Exit\n");
  return status;
}

#if SANE_UNUSED
static STATUS
SetPowerSave (PAsic chip)
{
  STATUS status = STATUS_GOOD;
  DBG (DBG_ASIC, "SetPowerSave:Enter\n");

  if (chip->firmwarestate < FS_OPENED)
    OpenScanChip (chip);

  if (chip->firmwarestate > FS_OPENED)
    Asic_ScanStop (chip);

  Mustek_SendData (chip, ES01_94_PowerSaveControl, 0x10);

  chip->firmwarestate = FS_OPENED;
  DBG (DBG_ASIC, "SetPowerSave:Exit\n");
  return status;
}
#endif

static STATUS
SetLineTimeAndExposure (PAsic chip)
{
  STATUS status = STATUS_GOOD;
  DBG (DBG_ASIC, "SetLineTimeAndExposure:Enter\n");

  if (chip->firmwarestate < FS_OPENED)
    OpenScanChip (chip);

  Mustek_SendData (chip, ES01_C4_MultiTGTimesRed, 0);
  Mustek_SendData (chip, ES01_C5_MultiTGTimesGreen, 0);
  Mustek_SendData (chip, ES01_C6_MultiTGTimesBlue, 0);

  Mustek_SendData (chip, ES01_C7_MultiTGDummyPixelNumberLSB, 0);
  Mustek_SendData (chip, ES01_C8_MultiTGDummyPixelNumberMSB, 0);

  Mustek_SendData (chip, ES01_C9_CCDDummyPixelNumberLSB, 0);
  Mustek_SendData (chip, ES01_CA_CCDDummyPixelNumberMSB, 0);

  Mustek_SendData (chip, ES01_CB_CCDDummyCycleNumber, 0);


  chip->firmwarestate = FS_OPENED;

  DBG (DBG_ASIC, "SetLineTimeAndExposure:Exit\n");
  return status;
}





static STATUS
CCDTiming (PAsic chip)
{
  STATUS status = STATUS_GOOD;
  unsigned int dwPH1, dwPH2, dwPHRS, dwPHCP;

  DBG (DBG_ASIC, "CCDTiming:Enter\n");
  DBG (DBG_ASIC, "Dpi=%d\n", chip->Scan.Dpi);

  if (chip->firmwarestate < FS_OPENED)
    OpenScanChip (chip);

  Mustek_SendData (chip, ES01_82_AFE_ADCCLK_TIMING_ADJ_BYTE0,
		   (SANE_Byte) (chip->Timing.AFE_ADCCLK_Timing));
  Mustek_SendData (chip, ES01_83_AFE_ADCCLK_TIMING_ADJ_BYTE1,
		   (SANE_Byte) (chip->Timing.AFE_ADCCLK_Timing >> 8));
  Mustek_SendData (chip, ES01_84_AFE_ADCCLK_TIMING_ADJ_BYTE2,
		   (SANE_Byte) (chip->Timing.AFE_ADCCLK_Timing >> 16));
  Mustek_SendData (chip, ES01_85_AFE_ADCCLK_TIMING_ADJ_BYTE3,
		   (SANE_Byte) (chip->Timing.AFE_ADCCLK_Timing >> 24));

  Mustek_SendData (chip, ES01_1F0_AFERS_TIMING_ADJ_B0,
		   (SANE_Byte) (chip->Timing.AFE_ADCRS_Timing));
  Mustek_SendData (chip, ES01_1F1_AFERS_TIMING_ADJ_B1,
		   (SANE_Byte) (chip->Timing.AFE_ADCRS_Timing >> 8));
  Mustek_SendData (chip, ES01_1F2_AFERS_TIMING_ADJ_B2,
		   (SANE_Byte) (chip->Timing.AFE_ADCRS_Timing >> 16));
  Mustek_SendData (chip, ES01_1F3_AFERS_TIMING_ADJ_B3,
		   (SANE_Byte) (chip->Timing.AFE_ADCRS_Timing >> 24));

  Mustek_SendData (chip, ES01_1EC_AFEVS_TIMING_ADJ_B0,
		   (SANE_Byte) (chip->Timing.AFE_ADCVS_Timing));
  Mustek_SendData (chip, ES01_1ED_AFEVS_TIMING_ADJ_B1,
		   (SANE_Byte) (chip->Timing.AFE_ADCVS_Timing >> 8));
  Mustek_SendData (chip, ES01_1EE_AFEVS_TIMING_ADJ_B2,
		   (SANE_Byte) (chip->Timing.AFE_ADCVS_Timing >> 16));
  Mustek_SendData (chip, ES01_1EF_AFEVS_TIMING_ADJ_B3,
		   (SANE_Byte) (chip->Timing.AFE_ADCVS_Timing >> 24));

  Mustek_SendData (chip, ES01_160_CHANNEL_A_LATCH_POSITION_HB,
		   HIBYTE (chip->Timing.AFE_ChannelA_LatchPos));
  Mustek_SendData (chip, ES01_161_CHANNEL_A_LATCH_POSITION_LB,
		   LOBYTE (chip->Timing.AFE_ChannelA_LatchPos));

  Mustek_SendData (chip, ES01_162_CHANNEL_B_LATCH_POSITION_HB,
		   HIBYTE (chip->Timing.AFE_ChannelB_LatchPos));
  Mustek_SendData (chip, ES01_163_CHANNEL_B_LATCH_POSITION_LB,
		   LOBYTE (chip->Timing.AFE_ChannelB_LatchPos));

  Mustek_SendData (chip, ES01_164_CHANNEL_C_LATCH_POSITION_HB,
		   HIBYTE (chip->Timing.AFE_ChannelC_LatchPos));
  Mustek_SendData (chip, ES01_165_CHANNEL_C_LATCH_POSITION_LB,
		   LOBYTE (chip->Timing.AFE_ChannelC_LatchPos));

  Mustek_SendData (chip, ES01_166_CHANNEL_D_LATCH_POSITION_HB,
		   HIBYTE (chip->Timing.AFE_ChannelD_LatchPos));
  Mustek_SendData (chip, ES01_167_CHANNEL_D_LATCH_POSITION_LB,
		   LOBYTE (chip->Timing.AFE_ChannelD_LatchPos));

  Mustek_SendData (chip, ES01_168_SECONDARY_FF_LATCH_POSITION,
		   chip->Timing.AFE_Secondary_FF_LatchPos);

  Mustek_SendData (chip, ES01_1D0_DUMMY_CYCLE_TIMING_B0,
		   (SANE_Byte) (chip->Timing.CCD_DummyCycleTiming));
  Mustek_SendData (chip, ES01_1D1_DUMMY_CYCLE_TIMING_B1,
		   (SANE_Byte) (chip->Timing.CCD_DummyCycleTiming >> 8));
  Mustek_SendData (chip, ES01_1D2_DUMMY_CYCLE_TIMING_B2,
		   (SANE_Byte) (chip->Timing.CCD_DummyCycleTiming >> 16));
  Mustek_SendData (chip, ES01_1D3_DUMMY_CYCLE_TIMING_B3,
		   (SANE_Byte) (chip->Timing.CCD_DummyCycleTiming >> 24));

  if (chip->Scan.Dpi >= 1200)
    {
      dwPH1 = chip->Timing.CCD_PH1_Timing_1200;
      dwPH2 = chip->Timing.CCD_PH2_Timing_1200;
      dwPHRS = chip->Timing.CCD_PHRS_Timing_1200;
      dwPHCP = chip->Timing.CCD_PHCP_Timing_1200;
    }
  else
    {
      dwPH1 = chip->Timing.CCD_PH1_Timing_600;
      dwPH2 = chip->Timing.CCD_PH2_Timing_600;
      dwPHRS = chip->Timing.CCD_PHRS_Timing_600;
      dwPHCP = chip->Timing.CCD_PHCP_Timing_600;
    }

  Mustek_SendData (chip, ES01_1D4_PH1_TIMING_ADJ_B0, (SANE_Byte) (dwPH1));
  Mustek_SendData (chip, ES01_1D5_PH1_TIMING_ADJ_B1, (SANE_Byte) (dwPH1 >> 8));
  Mustek_SendData (chip, ES01_1D6_PH1_TIMING_ADJ_B2, (SANE_Byte) (dwPH1 >> 16));
  Mustek_SendData (chip, ES01_1D7_PH1_TIMING_ADJ_B3, (SANE_Byte) (dwPH1 >> 24));

  /* set ccd ph1 ph2 rs cp */
  Mustek_SendData (chip, ES01_D0_PH1_0, 0);
  Mustek_SendData (chip, ES01_D1_PH2_0, 4);
  Mustek_SendData (chip, ES01_D4_PHRS_0, 0);
  Mustek_SendData (chip, ES01_D5_PHCP_0, 0);

  Mustek_SendData (chip, ES01_1D8_PH2_TIMING_ADJ_B0, (SANE_Byte) (dwPH2));
  Mustek_SendData (chip, ES01_1D9_PH2_TIMING_ADJ_B1, (SANE_Byte) (dwPH2 >> 8));
  Mustek_SendData (chip, ES01_1DA_PH2_TIMING_ADJ_B2, (SANE_Byte) (dwPH2 >> 16));
  Mustek_SendData (chip, ES01_1DB_PH2_TIMING_ADJ_B3, (SANE_Byte) (dwPH2 >> 24));

  Mustek_SendData (chip, ES01_1E4_PHRS_TIMING_ADJ_B0, (SANE_Byte) (dwPHRS));
  Mustek_SendData (chip, ES01_1E5_PHRS_TIMING_ADJ_B1, (SANE_Byte) (dwPHRS >> 8));
  Mustek_SendData (chip, ES01_1E6_PHRS_TIMING_ADJ_B2, (SANE_Byte) (dwPHRS >> 16));
  Mustek_SendData (chip, ES01_1E7_PHRS_TIMING_ADJ_B3, (SANE_Byte) (dwPHRS >> 24));

  Mustek_SendData (chip, ES01_1E8_PHCP_TIMING_ADJ_B0, (SANE_Byte) (dwPHCP));
  Mustek_SendData (chip, ES01_1E9_PHCP_TIMING_ADJ_B1, (SANE_Byte) (dwPHCP >> 8));
  Mustek_SendData (chip, ES01_1EA_PHCP_TIMING_ADJ_B2, (SANE_Byte) (dwPHCP >> 16));
  Mustek_SendData (chip, ES01_1EB_PHCP_TIMING_ADJ_B3, (SANE_Byte) (dwPHCP >> 24));

  chip->firmwarestate = FS_OPENED;
  DBG (DBG_ASIC, "CCDTiming:Exit\n");
  return status;
}

static STATUS
IsCarriageHome (PAsic chip, SANE_Bool * LampHome, SANE_Bool * TAHome)
{
  STATUS status = STATUS_GOOD;
  SANE_Byte temp;

  DBG (DBG_ASIC, "IsCarriageHome:Enter\n");

  status = GetChipStatus (chip, 0, &temp);
  if (status != STATUS_GOOD)
    {
      DBG (DBG_ASIC, "IsCarriageHome:Error!\n");
      return status;
    }

  if ((temp & SENSOR0_DETECTED) == SENSOR0_DETECTED)
    *LampHome = TRUE;
  else
    {
      *LampHome = FALSE;
    }

  *TAHome = TRUE;

  DBG (DBG_ASIC, "LampHome=%d\n", *LampHome);

  DBG (DBG_ASIC, "IsCarriageHome:Exit\n");
  return status;
}


static STATUS
GetChipStatus (PAsic chip, SANE_Byte Selector, SANE_Byte * ChipStatus)
{
  STATUS status = STATUS_GOOD;
  DBG (DBG_ASIC, "GetChipStatus:Enter\n");

  status = Mustek_SendData (chip, ES01_8B_Status, Selector);
  if (status != STATUS_GOOD)
    return status;

  status = Mustek_WriteAddressLineForRegister (chip, ES01_8B_Status);
  if (status != STATUS_GOOD)
    return status;

  *ChipStatus = ES01_8B_Status;
  status = Mustek_ReceiveData (chip, ChipStatus);
  if (status != STATUS_GOOD)
    return status;

  DBG (DBG_ASIC, "GetChipStatus:Exit\n");
  return status;
}

static STATUS
SetAFEGainOffset (PAsic chip)
{
  STATUS status = STATUS_GOOD;
  int i = 0;

  DBG (DBG_ASIC, "SetAFEGainOffset:Enter\n");

  if (chip->AD.DirectionR)
    {				/* Negative */
      Mustek_SendData (chip, ES01_60_AFE_AUTO_GAIN_OFFSET_RED_LB,
		       (chip->AD.GainR << 1) | 0x01);
      Mustek_SendData (chip, ES01_61_AFE_AUTO_GAIN_OFFSET_RED_HB,
		       chip->AD.OffsetR);
    }
  else
    {				/* Postive */
      Mustek_SendData (chip, ES01_60_AFE_AUTO_GAIN_OFFSET_RED_LB,
		       (chip->AD.GainR << 1));
      Mustek_SendData (chip, ES01_61_AFE_AUTO_GAIN_OFFSET_RED_HB,
		       chip->AD.OffsetR);
    }

  if (chip->AD.DirectionG)
    {
      Mustek_SendData (chip, ES01_62_AFE_AUTO_GAIN_OFFSET_GREEN_LB,
		       (chip->AD.GainG << 1) | 0x01);
      Mustek_SendData (chip, ES01_63_AFE_AUTO_GAIN_OFFSET_GREEN_HB,
		       chip->AD.OffsetG);
    }
  else
    {
      Mustek_SendData (chip, ES01_62_AFE_AUTO_GAIN_OFFSET_GREEN_LB,
		       (chip->AD.GainG << 1));

      Mustek_SendData (chip, ES01_63_AFE_AUTO_GAIN_OFFSET_GREEN_HB,
		       chip->AD.OffsetG);
    }

  if (chip->AD.DirectionB)
    {
      Mustek_SendData (chip, ES01_64_AFE_AUTO_GAIN_OFFSET_BLUE_LB,
		       (chip->AD.GainB << 1) | 0x01);
      Mustek_SendData (chip, ES01_65_AFE_AUTO_GAIN_OFFSET_BLUE_HB,
		       chip->AD.OffsetB);
    }
  else
    {
      Mustek_SendData (chip, ES01_64_AFE_AUTO_GAIN_OFFSET_BLUE_LB,
		       (chip->AD.GainB << 1));
      Mustek_SendData (chip, ES01_65_AFE_AUTO_GAIN_OFFSET_BLUE_HB,
		       chip->AD.OffsetB);
    }


  Mustek_SendData (chip, ES01_2A0_AFE_GAIN_OFFSET_CONTROL, 0x01);

  for (i = 0; i < 4; i++)
    {

      if (chip->AD.DirectionR == 0)
	{
	  Mustek_SendData (chip, ES01_2A1_AFE_AUTO_CONFIG_GAIN,
			   (SANE_Byte) (chip->AD.GainR << 1));
	  Mustek_SendData (chip, ES01_2A2_AFE_AUTO_CONFIG_OFFSET,
			   (SANE_Byte) (chip->AD.OffsetR));
	}
      else
	{
	  Mustek_SendData (chip, ES01_2A1_AFE_AUTO_CONFIG_GAIN,
			   (SANE_Byte) (chip->AD.GainR << 1) | 0x01);
	  Mustek_SendData (chip, ES01_2A2_AFE_AUTO_CONFIG_OFFSET,
			   (SANE_Byte) (chip->AD.OffsetR));
	}
    }

  for (i = 0; i < 4; i++)
    {
      if (chip->AD.DirectionG == 0)
	{
	  Mustek_SendData (chip, ES01_2A1_AFE_AUTO_CONFIG_GAIN,
			   (SANE_Byte) (chip->AD.GainG << 1));
	  Mustek_SendData (chip, ES01_2A2_AFE_AUTO_CONFIG_OFFSET,
			   (SANE_Byte) (chip->AD.OffsetG));
	}
      else
	{
	  Mustek_SendData (chip, ES01_2A1_AFE_AUTO_CONFIG_GAIN,
			   (SANE_Byte) (chip->AD.GainG << 1) | 0x01);
	  Mustek_SendData (chip, ES01_2A2_AFE_AUTO_CONFIG_OFFSET,
			   (SANE_Byte) (chip->AD.OffsetG));
	}
    }

  for (i = 0; i < 4; i++)
    {
      if (chip->AD.DirectionB == 0)
	{
	  Mustek_SendData (chip, ES01_2A1_AFE_AUTO_CONFIG_GAIN,
			   (SANE_Byte) (chip->AD.GainB << 1));
	  Mustek_SendData (chip, ES01_2A2_AFE_AUTO_CONFIG_OFFSET,
			   (SANE_Byte) (chip->AD.OffsetB));
	}
      else
	{
	  Mustek_SendData (chip, ES01_2A1_AFE_AUTO_CONFIG_GAIN,
			   (SANE_Byte) (chip->AD.GainB << 1) | 0x01);
	  Mustek_SendData (chip, ES01_2A2_AFE_AUTO_CONFIG_OFFSET,
			   (SANE_Byte) (chip->AD.OffsetB));
	}
    }

  for (i = 0; i < 36; i++)
    {
      Mustek_SendData (chip, ES01_2A1_AFE_AUTO_CONFIG_GAIN, 0);
      Mustek_SendData (chip, ES01_2A2_AFE_AUTO_CONFIG_OFFSET, 0);
    }

  Mustek_SendData (chip, ES01_2A0_AFE_GAIN_OFFSET_CONTROL, 0x00);

  /* Set to AFE */
  Mustek_SendData (chip, ES01_04_ADAFEPGACH1, chip->AD.GainR);
  Mustek_SendData (chip, ES01_06_ADAFEPGACH2, chip->AD.GainG);
  Mustek_SendData (chip, ES01_08_ADAFEPGACH3, chip->AD.GainB);

  if (chip->AD.DirectionR)
    Mustek_SendData (chip, ES01_0B_AD9826OffsetRedN, chip->AD.OffsetR);
  else
    Mustek_SendData (chip, ES01_0A_AD9826OffsetRedP, chip->AD.OffsetR);

  if (chip->AD.DirectionG)
    Mustek_SendData (chip, ES01_0D_AD9826OffsetGreenN, chip->AD.OffsetG);
  else
    Mustek_SendData (chip, ES01_0C_AD9826OffsetGreenP, chip->AD.OffsetG);

  if (chip->AD.DirectionB)
    Mustek_SendData (chip, ES01_0F_AD9826OffsetBlueN, chip->AD.OffsetB);
  else
    Mustek_SendData (chip, ES01_0E_AD9826OffsetBlueP, chip->AD.OffsetB);


  LLFSetRamAddress (chip, 0x0, PackAreaStartAddress - (512 * 8 - 1),
		    ACCESS_DRAM);

  Mustek_SendData (chip, ES01_F3_ActionOption,
		   MOTOR_MOVE_TO_FIRST_LINE_DISABLE |
		   MOTOR_BACK_HOME_AFTER_SCAN_DISABLE |
		   SCAN_ENABLE |
		   SCAN_BACK_TRACKING_DISABLE |
		   INVERT_MOTOR_DIRECTION_DISABLE |
		   UNIFORM_MOTOR_AND_SCAN_SPEED_ENABLE |
		   ES01_STATIC_SCAN_DISABLE | MOTOR_TEST_LOOP_DISABLE);

  Mustek_SendData (chip, ES01_9A_AFEControl,
		   AD9826_AFE | AUTO_CHANGE_AFE_GAIN_OFFSET_DISABLE);

  Mustek_SendData (chip, ES01_00_ADAFEConfiguration, 0x70);
  Mustek_SendData (chip, ES01_02_ADAFEMuxConfig, 0x80);

  DBG (DBG_ASIC, "SetAFEGainOffset:Exit\n");
  return status;
}

static STATUS
SetLEDTime (PAsic chip)
{
  STATUS status = STATUS_GOOD;
  DBG (DBG_ASIC, "SetLEDTime:Enter\n");

  Mustek_SendData (chip, ES01_B8_ChannelRedExpStartPixelLSB,
		   LOBYTE (chip->Timing.ChannelR_StartPixel));
  Mustek_SendData (chip, ES01_B9_ChannelRedExpStartPixelMSB,
		   HIBYTE (chip->Timing.ChannelR_StartPixel));
  Mustek_SendData (chip, ES01_BA_ChannelRedExpEndPixelLSB,
		   LOBYTE (chip->Timing.ChannelR_EndPixel));
  Mustek_SendData (chip, ES01_BB_ChannelRedExpEndPixelMSB,
		   HIBYTE (chip->Timing.ChannelR_EndPixel));

  Mustek_SendData (chip, ES01_BC_ChannelGreenExpStartPixelLSB,
		   LOBYTE (chip->Timing.ChannelG_StartPixel));
  Mustek_SendData (chip, ES01_BD_ChannelGreenExpStartPixelMSB,
		   HIBYTE (chip->Timing.ChannelG_StartPixel));
  Mustek_SendData (chip, ES01_BE_ChannelGreenExpEndPixelLSB,
		   LOBYTE (chip->Timing.ChannelG_EndPixel));
  Mustek_SendData (chip, ES01_BF_ChannelGreenExpEndPixelMSB,
		   HIBYTE (chip->Timing.ChannelG_EndPixel));

  Mustek_SendData (chip, ES01_C0_ChannelBlueExpStartPixelLSB,
		   LOBYTE (chip->Timing.ChannelB_StartPixel));
  Mustek_SendData (chip, ES01_C1_ChannelBlueExpStartPixelMSB,
		   HIBYTE (chip->Timing.ChannelB_StartPixel));
  Mustek_SendData (chip, ES01_C2_ChannelBlueExpEndPixelLSB,
		   LOBYTE (chip->Timing.ChannelB_EndPixel));
  Mustek_SendData (chip, ES01_C3_ChannelBlueExpEndPixelMSB,
		   HIBYTE (chip->Timing.ChannelB_EndPixel));

  DBG (DBG_ASIC, "SetLEDTime:Exit\n");
  return status;
}

static STATUS
SetScanMode (PAsic chip, SANE_Byte bScanBits)
{
  STATUS status = STATUS_GOOD;
  SANE_Byte temp_f5_register = 0;
  SANE_Byte GrayBWChannel;

  DBG (DBG_ASIC, "SetScanMode():Enter; set f5 register\n");

  if (bScanBits >= 24)
    {
      temp_f5_register |= COLOR_ES02;
    }
  else
    {
      temp_f5_register |= GRAY_ES02;
    }

  if ((bScanBits == 8) || (bScanBits == 24))
    {
      temp_f5_register |= _8_BITS_ES02;
    }
  else if (bScanBits == 1)
    {
      temp_f5_register |= _1_BIT_ES02;
    }
  else
    {
      temp_f5_register |= _16_BITS_ES02;
    }

  if (bScanBits < 24)
    {
      GrayBWChannel = 1;
    }
  else
    {
      GrayBWChannel = 4;
    }

  if (GrayBWChannel == 0)
    {
      temp_f5_register |= GRAY_RED_ES02;
    }
  else if (GrayBWChannel == 1)
    {
      temp_f5_register |= GRAY_GREEN_ES02;
    }
  else if (GrayBWChannel == 2)
    {
      temp_f5_register |= GRAY_BLUE_ES02;
    }
  else
    {
      temp_f5_register |= GRAY_GREEN_BLUE_ES02;
    }

  status = Mustek_SendData (chip, ES01_F5_ScanDataFormat, temp_f5_register);

  DBG (DBG_ASIC, "F5_ScanDataFormat=0x%x\n", temp_f5_register);
  DBG (DBG_ASIC, "SetScanMode():Exit\n");
  return status;
}

static STATUS
SetPackAddress (PAsic chip, unsigned short wXResolution, unsigned short wWidth, unsigned short wX,
		double XRatioAdderDouble, double XRatioTypeDouble,
		SANE_Byte byClear_Pulse_Width, unsigned short * PValidPixelNumber)
{
  STATUS status = STATUS_GOOD;

  unsigned short LineTotalOverlapPixel;
  SANE_Byte OverLapPixel;
  SANE_Byte TotalLineShift;
  SANE_Byte InvalidPixelNumberBackup;
  unsigned short SegmentTotalPixel;
  unsigned int dwLineTotalPixel;
  unsigned short ValidPixelNumber = *PValidPixelNumber;

  unsigned int FinalLinePixelPerSegment;
  SANE_Byte InValidPixelNumber;
  unsigned int CISPackAreaStartAddress;
  SANE_Byte PackAreaUseLine;

  unsigned int MaxPixelHW;
  int i;

  DBG (DBG_ASIC, "SetPackAddress:Enter\n");

  LineTotalOverlapPixel = 0;
  OverLapPixel = 0;
  TotalLineShift = 1;
  PackAreaUseLine = TotalLineShift + 1;

  if (wXResolution > (SENSOR_DPI / 2))
    {
      ValidPixelNumber = ValidPixelNumberFor1200DPI;
      OverLapPixel = OverLapPixelNumber1200;
    }
  else
    {
      ValidPixelNumber = ValidPixelNumberFor600DPI;
      OverLapPixel = OverLapPixelNumber600;
    }

  ValidPixelNumber = (unsigned short) ((wWidth + 10 + 15) * XRatioAdderDouble);
  ValidPixelNumber >>= 4;
  ValidPixelNumber <<= 4;

  ValidPixelNumber += (OverLapPixel * 2);

  for (i = 0; i < 16; i++)
    {
      Mustek_SendData (chip, ES01_2B0_SEGMENT0_OVERLAP_SEGMENT1 + i,
		       OverLapPixel);
      Mustek_SendData (chip, ES01_2C0_VALID_PIXEL_PARAMETER_OF_SEGMENT1 + i,
		       0);
    }
  LineTotalOverlapPixel = OverLapPixel * 16;

  FinalLinePixelPerSegment = ValidPixelNumber + OverLapPixel * 2;

  if ((FinalLinePixelPerSegment % 8) > 0)
    {
      InValidPixelNumber = (SANE_Byte) (8 - (FinalLinePixelPerSegment % 8));
    }
  else
    {
      InValidPixelNumber = 0;
    }

  InvalidPixelNumberBackup = InValidPixelNumber;

  Mustek_SendData (chip, ES01_1B0_SEGMENT_PIXEL_NUMBER_LB,
		   LOBYTE (ValidPixelNumber));
  Mustek_SendData (chip, ES01_1B1_SEGMENT_PIXEL_NUMBER_HB,
		   HIBYTE (ValidPixelNumber));

  SegmentTotalPixel =
    ValidPixelNumber + OverLapPixel * 2 + InValidPixelNumber;

  Mustek_SendData (chip, ES01_169_NUMBER_OF_SEGMENT_PIXEL_LB,
		   LOBYTE (ValidPixelNumber));
  Mustek_SendData (chip, ES01_16A_NUMBER_OF_SEGMENT_PIXEL_HB,
		   HIBYTE (ValidPixelNumber));
  Mustek_SendData (chip, ES01_16B_BETWEEN_SEGMENT_INVALID_PIXEL, 0);

  Mustek_SendData (chip, ES01_B6_LineWidthPixelLSB,
		   LOBYTE (ValidPixelNumber));
  Mustek_SendData (chip, ES01_B7_LineWidthPixelMSB,
		   HIBYTE (ValidPixelNumber));

  Mustek_SendData (chip, ES01_19A_CHANNEL_LINE_GAP_LB,
		   LOBYTE (ValidPixelNumber));
  Mustek_SendData (chip, ES01_19B_CHANNEL_LINE_GAP_HB,
		   HIBYTE (ValidPixelNumber));
  DBG (DBG_ASIC, "ValidPixelNumber=%d\n", ValidPixelNumber);

  for (i = 0; i < 36; i++)
    {
      Mustek_SendData (chip, 0x270 + i, 0);
    }

  Mustek_SendData (chip, 0x270,
		   (SANE_Byte) ((SegmentTotalPixel * (PackAreaUseLine) * 1)));
  Mustek_SendData (chip, 0x271,
		   (SANE_Byte) ((SegmentTotalPixel * (PackAreaUseLine) * 1) >> 8));
  Mustek_SendData (chip, 0x272,
		   (SANE_Byte) ((SegmentTotalPixel * (PackAreaUseLine) *
			    1) >> 16));

  Mustek_SendData (chip, 0x27C,
		   (SANE_Byte) ((SegmentTotalPixel * (PackAreaUseLine) * 2)));
  Mustek_SendData (chip, 0x27D,
		   (SANE_Byte) ((SegmentTotalPixel * (PackAreaUseLine) * 2) >> 8));
  Mustek_SendData (chip, 0x27E,
		   (SANE_Byte) ((SegmentTotalPixel * (PackAreaUseLine) *
			    2) >> 16));

  Mustek_SendData (chip, 0x288,
		   (SANE_Byte) ((SegmentTotalPixel * (PackAreaUseLine) * 3)));
  Mustek_SendData (chip, 0x289,
		   (SANE_Byte) ((SegmentTotalPixel * (PackAreaUseLine) * 3) >> 8));
  Mustek_SendData (chip, 0x28A,
		   (SANE_Byte) ((SegmentTotalPixel * (PackAreaUseLine) *
			    3) >> 16));
  DBG (DBG_ASIC, "channel gap=%d\n", SegmentTotalPixel * (PackAreaUseLine));


  Mustek_SendData (chip, ES01_B4_StartPixelLSB, LOBYTE (wX + 0));
  Mustek_SendData (chip, ES01_B5_StartPixelMSB, HIBYTE (wX + 0));


  dwLineTotalPixel = ValidPixelNumber;

  Mustek_SendData (chip, ES01_1B9_LINE_PIXEL_NUMBER_LB,
		   LOBYTE (XRatioTypeDouble * (dwLineTotalPixel - 1)));
  Mustek_SendData (chip, ES01_1BA_LINE_PIXEL_NUMBER_HB,
		   HIBYTE (XRatioTypeDouble * (dwLineTotalPixel - 1)));

  /* final start read out pixel */
  Mustek_SendData (chip, ES01_1F4_START_READ_OUT_PIXEL_LB, LOBYTE (0));
  Mustek_SendData (chip, ES01_1F5_START_READ_OUT_PIXEL_HB, HIBYTE (0));

  MaxPixelHW = (dwLineTotalPixel + InValidPixelNumber) - 10;

  if (wWidth > MaxPixelHW)
    {
      DBG (DBG_ERR, "read out pixel over max pixel! image will shift!!!\n");
    }

  /* final read pixel width */
  Mustek_SendData (chip, ES01_1F6_READ_OUT_PIXEL_LENGTH_LB,
		   LOBYTE (wWidth + 9));
  Mustek_SendData (chip, ES01_1F7_READ_OUT_PIXEL_LENGTH_HB,
		   HIBYTE (wWidth + 9));

  /* data output sequence */
  Mustek_SendData (chip, ES01_1F8_PACK_CHANNEL_SELECT_B0, 0);
  Mustek_SendData (chip, ES01_1F9_PACK_CHANNEL_SELECT_B1, 0);
  Mustek_SendData (chip, ES01_1FA_PACK_CHANNEL_SELECT_B2, 0x18);

  Mustek_SendData (chip, ES01_1FB_PACK_CHANNEL_SIZE_B0,
		   (SANE_Byte) ((SegmentTotalPixel * PackAreaUseLine)));
  Mustek_SendData (chip, ES01_1FC_PACK_CHANNEL_SIZE_B1,
		   (SANE_Byte) ((SegmentTotalPixel * PackAreaUseLine) >> 8));
  Mustek_SendData (chip, ES01_1FD_PACK_CHANNEL_SIZE_B2,
		   (SANE_Byte) ((SegmentTotalPixel * PackAreaUseLine) >> 16));

  Mustek_SendData (chip, ES01_16C_LINE_SHIFT_OUT_TIMES_DIRECTION, 0x01);
  Mustek_SendData (chip, ES01_1CE_LINE_SEGMENT_NUMBER, 0x00);
  Mustek_SendData (chip, ES01_D8_PHTG_EDGE_TIMING_ADJUST, 0x17);

  Mustek_SendData (chip, ES01_D9_CLEAR_PULSE_WIDTH, byClear_Pulse_Width);

  Mustek_SendData (chip, ES01_DA_CLEAR_SIGNAL_INVERTING_OUTPUT, 0x54 | 0x01);
  Mustek_SendData (chip, ES01_CD_TG_R_CONTROL, 0x3C);
  Mustek_SendData (chip, ES01_CE_TG_G_CONTROL, 0);
  Mustek_SendData (chip, ES01_CF_TG_B_CONTROL, 0x3C);


  /* set pack area address */

  CISPackAreaStartAddress = PackAreaStartAddress;
  DBG (DBG_ASIC, "CISPackAreaStartAddress=%d\n", CISPackAreaStartAddress);

  /* cycle 1 */
  Mustek_SendData (chip, ES01_16D_EXPOSURE_CYCLE1_SEGMENT1_START_ADDR_BYTE0,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0)));
  Mustek_SendData (chip, ES01_16E_EXPOSURE_CYCLE1_SEGMENT1_START_ADDR_BYTE1,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0) >> 8));
  Mustek_SendData (chip, ES01_16F_EXPOSURE_CYCLE1_SEGMENT1_START_ADDR_BYTE2,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0) >> 16));

  Mustek_SendData (chip, ES01_170_EXPOSURE_CYCLE1_SEGMENT2_START_ADDR_BYTE0,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0xC0000)));
  Mustek_SendData (chip, ES01_171_EXPOSURE_CYCLE1_SEGMENT2_START_ADDR_BYTE1,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0xC0000) >> 8));
  Mustek_SendData (chip, ES01_172_EXPOSURE_CYCLE1_SEGMENT2_START_ADDR_BYTE2,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0xC0000) >> 16));

  Mustek_SendData (chip, ES01_173_EXPOSURE_CYCLE1_SEGMENT3_START_ADDR_BYTE0,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0xC0000)));
  Mustek_SendData (chip, ES01_174_EXPOSURE_CYCLE1_SEGMENT3_START_ADDR_BYTE1,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0xC0000) >> 8));
  Mustek_SendData (chip, ES01_175_EXPOSURE_CYCLE1_SEGMENT3_START_ADDR_BYTE2,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0xC0000) >> 16));

  Mustek_SendData (chip, ES01_176_EXPOSURE_CYCLE1_SEGMENT4_START_ADDR_BYTE0,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0xC0000)));
  Mustek_SendData (chip, ES01_177_EXPOSURE_CYCLE1_SEGMENT4_START_ADDR_BYTE1,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0xC0000) >> 8));
  Mustek_SendData (chip, ES01_178_EXPOSURE_CYCLE1_SEGMENT4_START_ADDR_BYTE2,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0xC0000) >> 16));

  /* cycle 2 */
  Mustek_SendData (chip, ES01_179_EXPOSURE_CYCLE2_SEGMENT1_START_ADDR_BYTE0,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0xC0000)));
  Mustek_SendData (chip, ES01_17A_EXPOSURE_CYCLE2_SEGMENT1_START_ADDR_BYTE1,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0xC0000) >> 8));
  Mustek_SendData (chip, ES01_17B_EXPOSURE_CYCLE2_SEGMENT1_START_ADDR_BYTE2,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0xC0000) >> 16));

  Mustek_SendData (chip, ES01_17C_EXPOSURE_CYCLE2_SEGMENT2_START_ADDR_BYTE0,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0xC0000)));
  Mustek_SendData (chip, ES01_17D_EXPOSURE_CYCLE2_SEGMENT2_START_ADDR_BYTE1,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0xC0000) >> 8));
  Mustek_SendData (chip, ES01_17E_EXPOSURE_CYCLE2_SEGMENT2_START_ADDR_BYTE2,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0xC0000) >> 16));

  Mustek_SendData (chip, ES01_17F_EXPOSURE_CYCLE2_SEGMENT3_START_ADDR_BYTE0,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0xC0000)));
  Mustek_SendData (chip, ES01_180_EXPOSURE_CYCLE2_SEGMENT3_START_ADDR_BYTE1,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0xC0000) >> 8));
  Mustek_SendData (chip, ES01_181_EXPOSURE_CYCLE2_SEGMENT3_START_ADDR_BYTE2,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0xC0000) >> 16));

  Mustek_SendData (chip, ES01_182_EXPOSURE_CYCLE2_SEGMENT4_START_ADDR_BYTE0,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0xC0000)));
  Mustek_SendData (chip, ES01_183_EXPOSURE_CYCLE2_SEGMENT4_START_ADDR_BYTE1,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0xC0000) >> 8));
  Mustek_SendData (chip, ES01_184_EXPOSURE_CYCLE2_SEGMENT4_START_ADDR_BYTE2,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0xC0000) >> 16));

  /* cycle 3 */
  Mustek_SendData (chip, ES01_185_EXPOSURE_CYCLE3_SEGMENT1_START_ADDR_BYTE0,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0xC0000)));

  Mustek_SendData (chip, ES01_186_EXPOSURE_CYCLE3_SEGMENT1_START_ADDR_BYTE1,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0xC0000) >> 8));
  Mustek_SendData (chip, ES01_187_EXPOSURE_CYCLE3_SEGMENT1_START_ADDR_BYTE2,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0xC0000) >> 16));

  Mustek_SendData (chip, ES01_188_EXPOSURE_CYCLE3_SEGMENT2_START_ADDR_BYTE0,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0xC0000)));
  Mustek_SendData (chip, ES01_189_EXPOSURE_CYCLE3_SEGMENT2_START_ADDR_BYTE1,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0xC0000) >> 8));
  Mustek_SendData (chip, ES01_18A_EXPOSURE_CYCLE3_SEGMENT2_START_ADDR_BYTE2,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0xC0000) >> 16));

  Mustek_SendData (chip, ES01_18B_EXPOSURE_CYCLE3_SEGMENT3_START_ADDR_BYTE0,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0xC0000)));
  Mustek_SendData (chip, ES01_18C_EXPOSURE_CYCLE3_SEGMENT3_START_ADDR_BYTE1,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0xC0000) >> 8));
  Mustek_SendData (chip, ES01_18D_EXPOSURE_CYCLE3_SEGMENT3_START_ADDR_BYTE2,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0xC0000) >> 16));

  Mustek_SendData (chip, ES01_18E_EXPOSURE_CYCLE3_SEGMENT4_START_ADDR_BYTE0,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0xC0000)));
  Mustek_SendData (chip, ES01_18F_EXPOSURE_CYCLE3_SEGMENT4_START_ADDR_BYTE1,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0xC0000) >> 8));
  Mustek_SendData (chip, ES01_190_EXPOSURE_CYCLE3_SEGMENT4_START_ADDR_BYTE2,
		   (SANE_Byte) ((CISPackAreaStartAddress + 0xC0000) >> 16));
  DBG (DBG_ASIC, "set CISPackAreaStartAddress ok\n");

  Mustek_SendData (chip, 0x260, InValidPixelNumber);
  Mustek_SendData (chip, 0x261, InValidPixelNumber << 4);
  Mustek_SendData (chip, 0x262, InValidPixelNumber);
  Mustek_SendData (chip, 0x263, 0);
  DBG (DBG_ASIC, "InValidPixelNumber=%d\n", InValidPixelNumber);

  Mustek_SendData (chip, 0x264, 0);
  Mustek_SendData (chip, 0x265, 0);
  Mustek_SendData (chip, 0x266, 0);
  Mustek_SendData (chip, 0x267, 0);

  Mustek_SendData (chip, 0x268, 0);
  Mustek_SendData (chip, 0x269, 0);
  Mustek_SendData (chip, 0x26A, 0);
  Mustek_SendData (chip, 0x26B, 0);

  Mustek_SendData (chip, 0x26C, 0);
  Mustek_SendData (chip, 0x26D, 0);
  Mustek_SendData (chip, 0x26E, 0);
  Mustek_SendData (chip, 0x26F, 0);
  DBG (DBG_ASIC, "Set Invalid Pixel ok\n");


  /* Pack Start Address */
  Mustek_SendData (chip, ES01_19E_PACK_AREA_R_START_ADDR_BYTE0,
		   (SANE_Byte) ((CISPackAreaStartAddress +
			    (SegmentTotalPixel * (PackAreaUseLine * 0)))));
  Mustek_SendData (chip, ES01_19F_PACK_AREA_R_START_ADDR_BYTE1,
		   (SANE_Byte) ((CISPackAreaStartAddress +
			    (SegmentTotalPixel *
			     (PackAreaUseLine * 0))) >> 8));
  Mustek_SendData (chip, ES01_1A0_PACK_AREA_R_START_ADDR_BYTE2,
		   (SANE_Byte) ((CISPackAreaStartAddress +
			    (SegmentTotalPixel *
			     (PackAreaUseLine * 0))) >> 16));


  Mustek_SendData (chip, ES01_1A1_PACK_AREA_G_START_ADDR_BYTE0,
		   (SANE_Byte) ((CISPackAreaStartAddress +
			    (SegmentTotalPixel * (PackAreaUseLine * 1)))));
  Mustek_SendData (chip, ES01_1A2_PACK_AREA_G_START_ADDR_BYTE1,
		   (SANE_Byte) ((CISPackAreaStartAddress +
			    (SegmentTotalPixel *
			     (PackAreaUseLine * 1))) >> 8));
  Mustek_SendData (chip, ES01_1A3_PACK_AREA_G_START_ADDR_BYTE2,
		   (SANE_Byte) ((CISPackAreaStartAddress +
			    (SegmentTotalPixel *
			     (PackAreaUseLine * 1))) >> 16));

  Mustek_SendData (chip, ES01_1A4_PACK_AREA_B_START_ADDR_BYTE0,
		   (SANE_Byte) ((CISPackAreaStartAddress +
			    (SegmentTotalPixel * (PackAreaUseLine * 2)))));
  Mustek_SendData (chip, ES01_1A5_PACK_AREA_B_START_ADDR_BYTE1,
		   (SANE_Byte) ((CISPackAreaStartAddress +
			    (SegmentTotalPixel *
			     (PackAreaUseLine * 2))) >> 8));
  Mustek_SendData (chip, ES01_1A6_PACK_AREA_B_START_ADDR_BYTE2,
		   (SANE_Byte) ((CISPackAreaStartAddress +
			    (SegmentTotalPixel *
			     (PackAreaUseLine * 2))) >> 16));

  /* Pack End Address */
  Mustek_SendData (chip, ES01_1A7_PACK_AREA_R_END_ADDR_BYTE0,
		   (SANE_Byte) ((CISPackAreaStartAddress +
			    (SegmentTotalPixel * (PackAreaUseLine * 1) -
			     1))));
  Mustek_SendData (chip, ES01_1A8_PACK_AREA_R_END_ADDR_BYTE1,
		   (SANE_Byte) ((CISPackAreaStartAddress +
			    (SegmentTotalPixel * (PackAreaUseLine * 1) -
			     1)) >> 8));
  Mustek_SendData (chip, ES01_1A9_PACK_AREA_R_END_ADDR_BYTE2,
		   (SANE_Byte) ((CISPackAreaStartAddress +
			    (SegmentTotalPixel * (PackAreaUseLine * 1) -
			     1)) >> 16));

  Mustek_SendData (chip, ES01_1AA_PACK_AREA_G_END_ADDR_BYTE0,
		   (SANE_Byte) ((CISPackAreaStartAddress +
			    (SegmentTotalPixel * (PackAreaUseLine * 2) -
			     1))));
  Mustek_SendData (chip, ES01_1AB_PACK_AREA_G_END_ADDR_BYTE1,
		   (SANE_Byte) ((CISPackAreaStartAddress +
			    (SegmentTotalPixel * (PackAreaUseLine * 2) -
			     1)) >> 8));
  Mustek_SendData (chip, ES01_1AC_PACK_AREA_G_END_ADDR_BYTE2,
		   (SANE_Byte) ((CISPackAreaStartAddress +
			    (SegmentTotalPixel * (PackAreaUseLine * 2) -
			     1)) >> 16));

  Mustek_SendData (chip, ES01_1AD_PACK_AREA_B_END_ADDR_BYTE0,
		   (SANE_Byte) ((CISPackAreaStartAddress +
			    (SegmentTotalPixel * (PackAreaUseLine * 3) -
			     1))));
  Mustek_SendData (chip, ES01_1AE_PACK_AREA_B_END_ADDR_BYTE1,
		   (SANE_Byte) ((CISPackAreaStartAddress +
			    (SegmentTotalPixel * (PackAreaUseLine * 3) -
			     1)) >> 8));
  Mustek_SendData (chip, ES01_1AF_PACK_AREA_B_END_ADDR_BYTE2,
		   (SANE_Byte) ((CISPackAreaStartAddress +
			    (SegmentTotalPixel * (PackAreaUseLine * 3) -
			     1)) >> 16));
  DBG (DBG_ASIC,
       "CISPackAreaStartAddress + (SegmentTotalPixel*(PackAreaUseLine*1))=%d\n",
       (CISPackAreaStartAddress +
	(SegmentTotalPixel * (PackAreaUseLine * 1))));

  Mustek_SendData (chip, ES01_19C_MAX_PACK_LINE, PackAreaUseLine);

  status =
    Mustek_SendData (chip, ES01_19D_PACK_THRESHOLD_LINE, TotalLineShift);
  DBG (DBG_ASIC, "PackAreaUseLine=%d,TotalLineShift=%d\n", PackAreaUseLine,
       TotalLineShift);

  *PValidPixelNumber = ValidPixelNumber;

  DBG (DBG_ASIC, "SetPackAddress:Enter\n");
  return status;
}

static STATUS
SetExtraSetting (PAsic chip, unsigned short wXResolution, unsigned short wCCD_PixelNumber,
		 SANE_Bool isCaribrate)
{
  STATUS status = STATUS_GOOD;
  SANE_Byte byPHTG_PulseWidth, byPHTG_WaitWidth;
  SANE_Byte temp_ff_register = 0;
  SANE_Byte bThreshold = 128;

  DBG (DBG_ASIC, "SetExtraSetting:Enter\n");

  Mustek_SendData (chip, ES01_B8_ChannelRedExpStartPixelLSB,
		   LOBYTE (chip->Timing.ChannelR_StartPixel));
  Mustek_SendData (chip, ES01_B9_ChannelRedExpStartPixelMSB,
		   HIBYTE (chip->Timing.ChannelR_StartPixel));
  Mustek_SendData (chip, ES01_BA_ChannelRedExpEndPixelLSB,
		   LOBYTE (chip->Timing.ChannelR_EndPixel));
  Mustek_SendData (chip, ES01_BB_ChannelRedExpEndPixelMSB,
		   HIBYTE (chip->Timing.ChannelR_EndPixel));

  Mustek_SendData (chip, ES01_BC_ChannelGreenExpStartPixelLSB,
		   LOBYTE (chip->Timing.ChannelG_StartPixel));
  Mustek_SendData (chip, ES01_BD_ChannelGreenExpStartPixelMSB,
		   HIBYTE (chip->Timing.ChannelG_StartPixel));
  Mustek_SendData (chip, ES01_BE_ChannelGreenExpEndPixelLSB,
		   LOBYTE (chip->Timing.ChannelG_EndPixel));
  Mustek_SendData (chip, ES01_BF_ChannelGreenExpEndPixelMSB,
		   HIBYTE (chip->Timing.ChannelG_EndPixel));

  Mustek_SendData (chip, ES01_C0_ChannelBlueExpStartPixelLSB,
		   LOBYTE (chip->Timing.ChannelB_StartPixel));
  Mustek_SendData (chip, ES01_C1_ChannelBlueExpStartPixelMSB,
		   HIBYTE (chip->Timing.ChannelB_StartPixel));
  Mustek_SendData (chip, ES01_C2_ChannelBlueExpEndPixelLSB,
		   LOBYTE (chip->Timing.ChannelB_EndPixel));
  Mustek_SendData (chip, ES01_C3_ChannelBlueExpEndPixelMSB,
		   HIBYTE (chip->Timing.ChannelB_EndPixel));

  byPHTG_PulseWidth = chip->Timing.PHTG_PluseWidth;
  byPHTG_WaitWidth = chip->Timing.PHTG_WaitWidth;
  Mustek_SendData (chip, ES01_B2_PHTGPulseWidth, byPHTG_PulseWidth);
  Mustek_SendData (chip, ES01_B3_PHTGWaitWidth, byPHTG_WaitWidth);

  Mustek_SendData (chip, ES01_CC_PHTGTimingAdjust,
		   chip->Timing.PHTG_TimingAdj);
  Mustek_SendData (chip, ES01_D0_PH1_0, chip->Timing.PHTG_TimingSetup);

  DBG (DBG_ASIC, "ChannelR_StartPixel=%d,ChannelR_EndPixel=%d\n",
       chip->Timing.ChannelR_StartPixel, chip->Timing.ChannelR_EndPixel);

  if (wXResolution == 1200)
    {
      Mustek_SendData (chip, ES01_DE_CCD_SETUP_REGISTER,
		       chip->Timing.DE_CCD_SETUP_REGISTER_1200);
    }
  else
    {
      Mustek_SendData (chip, ES01_DE_CCD_SETUP_REGISTER,
		       chip->Timing.DE_CCD_SETUP_REGISTER_600);
    }

  if (isCaribrate == TRUE)
    {
      temp_ff_register |= BYPASS_DARK_SHADING_ENABLE;
      temp_ff_register |= BYPASS_WHITE_SHADING_ENABLE;
    }
  else				/*Setwindow */
    {
      temp_ff_register |= BYPASS_DARK_SHADING_DISABLE;
      temp_ff_register |= BYPASS_WHITE_SHADING_DISABLE;
    }

  temp_ff_register |= BYPASS_PRE_GAMMA_ENABLE;

  temp_ff_register |= BYPASS_CONVOLUTION_ENABLE;


  temp_ff_register |= BYPASS_MATRIX_ENABLE;

  temp_ff_register |= BYPASS_GAMMA_ENABLE;

  if (isCaribrate == TRUE)
    {
      Mustek_SendData (chip, ES01_FF_SCAN_IMAGE_OPTION, 0xfc | (0x00 & 0x03));
      DBG (DBG_ASIC, "FF_SCAN_IMAGE_OPTION=0x%x\n", 0xfc | (0x00 & 0x03));
    }
  else				/*Setwindow */
    {
      Mustek_SendData (chip, ES01_FF_SCAN_IMAGE_OPTION,
		       temp_ff_register | (0x00 & 0x03));
      DBG (DBG_ASIC, "FF_SCAN_IMAGE_OPTION=0x%x\n",
	   temp_ff_register | (0x00 & 0x03));
    }

  /*  pixel process time */
  Mustek_SendData (chip, ES01_B0_CCDPixelLSB, LOBYTE (wCCD_PixelNumber));
  Mustek_SendData (chip, ES01_B1_CCDPixelMSB, HIBYTE (wCCD_PixelNumber));
  Mustek_SendData (chip, ES01_DF_ICG_CONTROL, 0x17);
  DBG (DBG_ASIC, "wCCD_PixelNumber=%d\n", wCCD_PixelNumber);

  Mustek_SendData (chip, ES01_88_LINE_ART_THRESHOLD_HIGH_VALUE, bThreshold);
  Mustek_SendData (chip, ES01_89_LINE_ART_THRESHOLD_LOW_VALUE,
		   bThreshold - 1);
  DBG (DBG_ASIC, "bThreshold=%d\n", bThreshold);

  usleep (50000);

  DBG (DBG_ASIC, "SetExtraSetting:Exit\n");
  return status;
}


/* ---------------------- high level asic functions ------------------------ */


/* HOLD: We don't want to have global vid/pids */
static unsigned short ProductID = 0x0409;
static unsigned short VendorID = 0x055f;

static SANE_String_Const device_name;

static SANE_Status
attach_one_scanner (SANE_String_Const devname)
{
  DBG (DBG_ASIC, "attach_one_scanner: enter\n");
  DBG (DBG_INFO, "attach_one_scanner: devname = %s\n", devname);
  device_name = devname;
  return SANE_STATUS_GOOD;
}

static STATUS
Asic_Open (PAsic chip, SANE_Byte *pDeviceName)
{
  STATUS status;
  SANE_Status sane_status;

  DBG (DBG_ASIC, "Asic_Open: Enter\n");

  device_name = NULL;

  if (chip->firmwarestate > FS_OPENED)
    {
      DBG (DBG_ASIC, "chip has been opened. fd=%d\n", chip->fd);
      return STATUS_INVAL;
    }

  /* init usb */
  sanei_usb_init ();

  /* find scanner */
  sane_status =
    sanei_usb_find_devices (VendorID, ProductID, attach_one_scanner);
  if (sane_status != SANE_STATUS_GOOD)
    {
      DBG (DBG_ERR, "Asic_Open: sanei_usb_find_devices failed: %s\n",
	   sane_strstatus (sane_status));
      return STATUS_INVAL;
    }
  /* open usb */
  if (device_name == NULL)
    {
      DBG (DBG_ERR, "Asic_Open: no scanner found\n");
      return STATUS_INVAL;
    }
  sane_status = sanei_usb_open (device_name, &chip->fd);
  if (sane_status != SANE_STATUS_GOOD)
    {
      DBG (DBG_ERR, "Asic_Open: sanei_usb_open of %s failed: %s\n",
	   device_name, sane_strstatus (sane_status));
      return STATUS_INVAL;
    }

  /* open scanner chip */
  status = OpenScanChip (chip);
  if (status != STATUS_GOOD)
    {
      sanei_usb_close (chip->fd);
      DBG (DBG_ASIC, "Asic_Open: OpenScanChip error\n");
      return status;
    }

  Mustek_SendData (chip, ES01_94_PowerSaveControl, 0x27);
  Mustek_SendData (chip, ES01_86_DisableAllClockWhenIdle,
		   CLOSE_ALL_CLOCK_DISABLE);
  Mustek_SendData (chip, ES01_79_AFEMCLK_SDRAMCLK_DELAY_CONTROL,
		   SDRAMCLK_DELAY_12_ns);

  /* SDRAM initial sequence */
  Mustek_SendData (chip, ES01_87_SDRAM_Timing, 0xf1);
  Mustek_SendData (chip, ES01_87_SDRAM_Timing, 0xa5);
  Mustek_SendData (chip, ES01_87_SDRAM_Timing, 0x91);
  Mustek_SendData (chip, ES01_87_SDRAM_Timing, 0x81);
  Mustek_SendData (chip, ES01_87_SDRAM_Timing, 0xf0);


  chip->firmwarestate = FS_OPENED;
  Asic_WaitUnitReady (chip);
  DBG (DBG_ASIC, "Asic_WaitUnitReady\n");
  status = SafeInitialChip (chip);
  if (status != STATUS_GOOD)
    {
      DBG (DBG_ERR, "Asic_Open: SafeInitialChip error\n");
      return status;
    }

  pDeviceName = (SANE_Byte *) strdup (device_name);
  if (!pDeviceName)
    {
      DBG (DBG_ERR, "Asic_Open: not enough memory\n");
      return STATUS_INVAL;
    }
  DBG (DBG_INFO, "Asic_Open: device %s successfully opened\n", pDeviceName);
  DBG (DBG_ASIC, "Asic_Open: Exit\n");
  return status;
}


static STATUS
Asic_Close (PAsic chip)
{
  STATUS status;
  DBG (DBG_ASIC, "Asic_Close: Enter\n");

  if (chip->firmwarestate < FS_OPENED)
    {
      DBG (DBG_ASIC, "Asic_Close: Scanner is not opened\n");
      return STATUS_GOOD;
    }

  if (chip->firmwarestate > FS_OPENED)
    {
      DBG (DBG_ASIC,
	   "Asic_Close: Scanner is scanning, try to stop scanning\n");
      Asic_ScanStop (chip);
    }

  Mustek_SendData (chip, ES01_86_DisableAllClockWhenIdle,
		   CLOSE_ALL_CLOCK_ENABLE);

  status = CloseScanChip (chip);
  if (status != STATUS_GOOD)
    {
      DBG (DBG_ERR, "Asic_Close: CloseScanChip error\n");
      return status;
    }

  sanei_usb_close (chip->fd);
  chip->firmwarestate = FS_ATTACHED;

  DBG (DBG_ASIC, "Asic_Close: Exit\n");
  return status;
}

static STATUS
Asic_TurnLamp (PAsic chip, SANE_Bool isLampOn)
{
  STATUS status = STATUS_GOOD;
  SANE_Byte PWM;

  DBG (DBG_ASIC, "Asic_TurnLamp: Enter\n");

  if (chip->firmwarestate < FS_OPENED)
    {
      DBG (DBG_ERR, "Asic_TurnLamp: Scanner is not opened\n");
      return STATUS_INVAL;
    }

  if (chip->firmwarestate > FS_OPENED)
    {
      Mustek_SendData (chip, ES01_F4_ActiveTriger, ACTION_TRIGER_DISABLE);
    }

  if (isLampOn)
    {
      PWM = LAMP0_PWM_DEFAULT;
    }
  else
    {
      PWM = 0;
    }
  Mustek_SendData (chip, ES01_99_LAMP_PWM_FREQ_CONTROL, 1);
  Mustek_SendData (chip, ES01_90_Lamp0PWM, PWM);
  DBG (DBG_ASIC, "Lamp0 PWM = %d\n", PWM);

  chip->firmwarestate = FS_OPENED;

  DBG (DBG_ASIC, "Asic_TurnLamp: Exit\n");
  return status;
}


static STATUS
Asic_TurnTA (PAsic chip, SANE_Bool isTAOn)
{
  SANE_Byte PWM;

  DBG (DBG_ASIC, "Asic_TurnTA: Enter\n");

  if (chip->firmwarestate < FS_OPENED)
    {
      DBG (DBG_ERR, "Asic_TurnTA: Scanner is not opened\n");
      return STATUS_INVAL;
    }

  if (chip->firmwarestate > FS_OPENED)
    Mustek_SendData (chip, ES01_F4_ActiveTriger, ACTION_TRIGER_DISABLE);

  if (isTAOn)
    {
      PWM = LAMP1_PWM_DEFAULT;
    }
  else
    {
      PWM = 0;

    }
  Mustek_SendData (chip, ES01_99_LAMP_PWM_FREQ_CONTROL, 1);
  Mustek_SendData (chip, ES01_91_Lamp1PWM, PWM);
  DBG (DBG_ASIC, "Lamp1 PWM = %d\n", PWM);

  chip->firmwarestate = FS_OPENED;
  DBG (DBG_ASIC, "Asic_TurnTA: Exit\n");
  return STATUS_GOOD;
}

static STATUS
Asic_WaitUnitReady (PAsic chip)
{
  STATUS status = STATUS_GOOD;
  SANE_Byte temp_status;
  int i = 0;

  DBG (DBG_ASIC, "Asic_WaitUnitReady:Enter\n");

  if (chip->firmwarestate < FS_OPENED)
    {
      DBG (DBG_ERR, "Asic_WaitUnitReady: Scanner has not been opened\n");

      return STATUS_INVAL;
    }


  do
    {
      status = GetChipStatus (chip, 1, &temp_status);
      if (status != STATUS_GOOD)
	{
	  DBG (DBG_ASIC, "WaitChipIdle:Error!\n");
	  return status;
	}
      i++;
      usleep (100000);
    }
  while (((temp_status & 0x1f) != 0) && i < 300);
  DBG (DBG_ASIC, "Wait %d s\n", (unsigned short) (i * 0.1));

  Mustek_SendData (chip, ES01_F4_ActiveTriger, ACTION_TRIGER_DISABLE);
  chip->motorstate = MS_STILL;

  DBG (DBG_ASIC, "Asic_WaitUnitReady: Exit\n");
  return status;
}

#if SANE_UNUSED
static STATUS
Asic_Release (PAsic chip)
{
  STATUS status = STATUS_GOOD;
  DBG (DBG_ASIC, "Asic_Release()\n");

  if (chip->firmwarestate > FS_ATTACHED)
    status = Asic_Close (chip);

  chip->firmwarestate = FS_NULL;

  DBG (DBG_ASIC, "Asic_Release: Exit\n");
  return status;
}
#endif

static STATUS
Asic_Initialize (PAsic chip)
{
  STATUS status = STATUS_GOOD;
  DBG (DBG_ASIC, "Asic_Initialize:Enter\n");


  chip->motorstate = MS_STILL;
  chip->dwBytesCountPerRow = 0;
  chip->lpGammaTable = NULL;
  DBG (DBG_ASIC, "isFirstOpenChip=%d\n", chip->isFirstOpenChip);

  chip->isFirstOpenChip = TRUE;
  DBG (DBG_ASIC, "isFirstOpenChip=%d\n", chip->isFirstOpenChip);

  chip->SWWidth = 0;
  chip->TA_Status = TA_UNKNOW;
  chip->lpShadingTable = NULL;
  chip->isMotorMove = MOTOR_0_ENABLE;

  Asic_Reset (chip);
  InitTiming (chip);

  chip->isUniformSpeedToScan = UNIFORM_MOTOR_AND_SCAN_SPEED_DISABLE;
  chip->isMotorGoToFirstLine = MOTOR_MOVE_TO_FIRST_LINE_ENABLE;

  chip->UsbHost = HT_USB10;

  DBG (DBG_ASIC, "Asic_Initialize: Exit\n");
  return status;
}

static STATUS
Asic_SetWindow (PAsic chip, SANE_Byte bScanBits,
		unsigned short wXResolution, unsigned short wYResolution,
		unsigned short wX, unsigned short wY, unsigned short wWidth, unsigned short wLength)
{
  STATUS status = STATUS_GOOD;

  unsigned short ValidPixelNumber;

  unsigned short wPerLineNeedBufferSize = 0;
  unsigned short BytePerPixel = 0;
  unsigned int dwTotal_PerLineNeedBufferSize = 0;
  unsigned int dwTotalLineTheBufferNeed = 0;
  unsigned short dwTotal_CCDResolution = 1200;
  unsigned short wThinkCCDResolution = 0;
  unsigned short wCCD_PixelNumber = 0;
  unsigned int dwLineWidthPixel = 0;
  unsigned short wNowMotorDPI;
  unsigned short XRatioTypeWord;
  double XRatioTypeDouble;
  double XRatioAdderDouble;

  LLF_MOTORMOVE *lpMotorStepsTable =
    (LLF_MOTORMOVE *) malloc (sizeof (LLF_MOTORMOVE));
  SANE_Byte byDummyCycleNum = 0;
  unsigned short Total_MotorDPI;

  unsigned short wMultiMotorStep = 1;
  SANE_Byte bMotorMoveType = _MOTOR_MOVE_TYPE;

  SANE_Byte byClear_Pulse_Width = 0;

  unsigned int dwLinePixelReport;
  SANE_Byte byPHTG_PulseWidth, byPHTG_WaitWidth;

  unsigned short StartSpeed, EndSpeed;
  LLF_CALCULATEMOTORTABLE CalMotorTable;
  LLF_MOTOR_CURRENT_AND_PHASE CurrentPhase;
  LLF_RAMACCESS RamAccess;
  unsigned int dwStartAddr, dwEndAddr, dwTableBaseAddr, dwShadingTableAddr;

  SANE_Byte isMotorMoveToFirstLine = chip->isMotorGoToFirstLine;
  SANE_Byte isUniformSpeedToScan = chip->isUniformSpeedToScan;
  SANE_Byte isScanBackTracking = SCAN_BACK_TRACKING_ENABLE;
  unsigned short * lpMotorTable;
  unsigned int RealTableSize;
  double dbXRatioAdderDouble;
  unsigned short wFullBank;

  DBG (DBG_ASIC, "Asic_SetWindow: Enter\n");
  DBG (DBG_ASIC,
       "bScanBits=%d,wXResolution=%d,wYResolution=%d,wX=%d,wY=%d,wWidth=%d,wLength=%d\n",
       bScanBits, wXResolution, wYResolution, wX, wY, wWidth, wLength);

  if (chip->firmwarestate != FS_OPENED)
    {
      DBG (DBG_ERR, "Asic_SetWindow: Scanner is not opened\n");
      return STATUS_INVAL;
    }

  Mustek_SendData (chip, ES01_F3_ActionOption, 0);
  Mustek_SendData (chip, ES01_86_DisableAllClockWhenIdle,
		   CLOSE_ALL_CLOCK_DISABLE);
  Mustek_SendData (chip, ES01_F4_ActiveTriger, ACTION_TRIGER_DISABLE);

  status = Asic_WaitUnitReady (chip);

  /* dummy clock mode */
  Mustek_SendData (chip, 0x1CD, 0);

  /* LED Flash */
  Mustek_SendData (chip, ES01_94_PowerSaveControl, 0x27 | 64 | 128);

  /* calculate byte per line */
  if (bScanBits > 24)
    {
      wPerLineNeedBufferSize = wWidth * 6;
      BytePerPixel = 6;
      chip->dwBytesCountPerRow = (unsigned int) (wWidth) * 6;
    }
  else if (bScanBits == 24)
    {
      wPerLineNeedBufferSize = wWidth * 3;
      BytePerPixel = 3;
      chip->dwBytesCountPerRow = (unsigned int) (wWidth) * 3;
    }
  else if ((bScanBits > 8) && (bScanBits <= 16))
    {
      wPerLineNeedBufferSize = wWidth * 2;
      BytePerPixel = 2;
      chip->dwBytesCountPerRow = (unsigned int) (wWidth) * 2;
    }
  else if ((bScanBits == 8))
    {
      wPerLineNeedBufferSize = wWidth;
      BytePerPixel = 1;
      chip->dwBytesCountPerRow = (unsigned int) (wWidth);
    }
  else if ((bScanBits < 8))
    {
      wPerLineNeedBufferSize = wWidth >> 3;
      BytePerPixel = 1;
      chip->dwBytesCountPerRow = (unsigned int) (wWidth);
    }
  DBG (DBG_ASIC, "dwBytesCountPerRow = %d\n", chip->dwBytesCountPerRow);

  byDummyCycleNum = 0;
  if (chip->lsLightSource == LS_REFLECTIVE)
    {
      if (chip->UsbHost == HT_USB10)
	{
	  switch (wYResolution)
	    {
	    case 2400:
	    case 1200:
	      if (chip->dwBytesCountPerRow > 22000)
		byDummyCycleNum = 4;
	      else if (chip->dwBytesCountPerRow > 15000)
		byDummyCycleNum = 3;
	      else if (chip->dwBytesCountPerRow > 10000)
		byDummyCycleNum = 2;
	      else if (chip->dwBytesCountPerRow > 5000)
		byDummyCycleNum = 1;
	      break;
	    case 600:
	    case 300:
	    case 150:
	    case 100:
	      if (chip->dwBytesCountPerRow > 21000)
		byDummyCycleNum = 7;
	      else if (chip->dwBytesCountPerRow > 18000)
		byDummyCycleNum = 6;
	      else if (chip->dwBytesCountPerRow > 15000)
		byDummyCycleNum = 5;
	      else if (chip->dwBytesCountPerRow > 12000)
		byDummyCycleNum = 4;
	      else if (chip->dwBytesCountPerRow > 9000)
		byDummyCycleNum = 3;
	      else if (chip->dwBytesCountPerRow > 6000)
		byDummyCycleNum = 2;
	      else if (chip->dwBytesCountPerRow > 3000)
		byDummyCycleNum = 1;
	      break;
	    case 75:
	    case 50:
	      byDummyCycleNum = 1;
	      break;
	    default:
	      byDummyCycleNum = 0;
	      break;
	    }
	}
      else
	{
	  switch (wYResolution)
	    {
	    case 2400:
	    case 1200:
	    case 75:
	    case 50:
	      byDummyCycleNum = 1;
	      break;
	    default:
	      byDummyCycleNum = 0;
	      break;
	    }
	}
    }

  dwTotal_PerLineNeedBufferSize = wPerLineNeedBufferSize;
  dwTotalLineTheBufferNeed = wLength;

  chip->Scan.Dpi = wXResolution;
  CCDTiming (chip);

  dwTotal_CCDResolution = SENSOR_DPI;

  if (chip->lsLightSource == LS_REFLECTIVE)
    {
      if (wXResolution > (dwTotal_CCDResolution / 2))
	{			/* full ccd resolution 1200dpi */
	  wThinkCCDResolution = dwTotal_CCDResolution;
	  Mustek_SendData (chip, ES01_98_GPIOControl8_15, 0x01);
	  Mustek_SendData (chip, ES01_96_GPIOValue8_15, 0x01);

	  wCCD_PixelNumber = chip->Timing.wCCDPixelNumber_1200;
	}
      else
	{			/*600dpi */
	  wThinkCCDResolution = dwTotal_CCDResolution / 2;
	  Mustek_SendData (chip, ES01_98_GPIOControl8_15, 0x01);
	  Mustek_SendData (chip, ES01_96_GPIOValue8_15, 0x00);
	  wCCD_PixelNumber = chip->Timing.wCCDPixelNumber_600;
	}
    }
  else
    {
      if (wXResolution > (dwTotal_CCDResolution / 2))
	{
	  wThinkCCDResolution = dwTotal_CCDResolution;
	  Mustek_SendData (chip, ES01_98_GPIOControl8_15, 0x01);
	  Mustek_SendData (chip, ES01_96_GPIOValue8_15, 0x01);
	  wCCD_PixelNumber = TA_IMAGE_PIXELNUMBER;
	}
      else
	{
	  wThinkCCDResolution = dwTotal_CCDResolution / 2;
	  Mustek_SendData (chip, ES01_98_GPIOControl8_15, 0x01);
	  Mustek_SendData (chip, ES01_96_GPIOValue8_15, 0x00);
	  wCCD_PixelNumber = TA_IMAGE_PIXELNUMBER;
	}
    }

  dwLineWidthPixel = wWidth;

  SetLineTimeAndExposure (chip);

  Mustek_SendData (chip, ES01_CB_CCDDummyCycleNumber, byDummyCycleNum);


  SetLEDTime (chip);

  /* calculate Y ratio */
  Total_MotorDPI = 1200;
  wNowMotorDPI = Total_MotorDPI;


  /* SetADConverter */
  Mustek_SendData (chip, ES01_74_HARDWARE_SETTING,
		   MOTOR1_SERIAL_INTERFACE_G10_8_ENABLE | LED_OUT_G11_DISABLE
		   | SLAVE_SERIAL_INTERFACE_G15_14_DISABLE |
		   SHUTTLE_CCD_DISABLE);

  /* set AFE */
  Mustek_SendData (chip, ES01_9A_AFEControl,
		   AD9826_AFE | AUTO_CHANGE_AFE_GAIN_OFFSET_DISABLE);
  SetAFEGainOffset (chip);

  Mustek_SendData (chip, ES01_F7_DigitalControl, DIGITAL_REDUCE_DISABLE);

  /* calculate X ratio */
  XRatioTypeDouble = wXResolution;
  XRatioTypeDouble /= wThinkCCDResolution;
  XRatioAdderDouble = 1 / XRatioTypeDouble;
  XRatioTypeWord = (unsigned short) (XRatioTypeDouble * 32768);	/* 32768 = 2^15 */

  XRatioAdderDouble = (double) (XRatioTypeWord) / 32768;
  XRatioAdderDouble = 1 / XRatioAdderDouble;

  /* 0x8000 = 1.00000 -> get all pixel */
  Mustek_SendData (chip, ES01_9E_HorizontalRatio1to15LSB,
		   LOBYTE (XRatioTypeWord));
  Mustek_SendData (chip, ES01_9F_HorizontalRatio1to15MSB,
		   HIBYTE (XRatioTypeWord));

  /* SetMotorType */
  Mustek_SendData (chip, ES01_A6_MotorOption, MOTOR_0_ENABLE |
		   MOTOR_1_DISABLE |
		   HOME_SENSOR_0_ENABLE | ES03_TABLE_DEFINE);

  Mustek_SendData (chip, ES01_F6_MorotControl1, SPEED_UNIT_1_PIXEL_TIME |
		   MOTOR_SYNC_UNIT_1_PIXEL_TIME);

  /* SetScanStepTable */

  /*set Motor move type */
  if (wYResolution >= 1200)
    bMotorMoveType = _8_TABLE_SPACE_FOR_1_DIV_2_STEP;

  switch (bMotorMoveType)
    {
    case _4_TABLE_SPACE_FOR_FULL_STEP:
      wMultiMotorStep = 1;
      break;

    case _8_TABLE_SPACE_FOR_1_DIV_2_STEP:
      wMultiMotorStep = 2;
      break;

    case _16_TABLE_SPACE_FOR_1_DIV_4_STEP:
      wMultiMotorStep = 4;
      break;

    case _32_TABLE_SPACE_FOR_1_DIV_8_STEP:
      wMultiMotorStep = 8;
      break;
    }
  wY *= wMultiMotorStep;

  SetScanMode (chip, bScanBits);



  /*set white shading int and dec */
  if (chip->lsLightSource == LS_REFLECTIVE)
    Mustek_SendData (chip, ES01_F8_WHITE_SHADING_DATA_FORMAT,
		     ES01_SHADING_3_INT_13_DEC);
  else
    Mustek_SendData (chip, ES01_F8_WHITE_SHADING_DATA_FORMAT,
		     ES01_SHADING_4_INT_12_DEC);

  SetPackAddress (chip, wXResolution, wWidth, wX, XRatioAdderDouble,
		  XRatioTypeDouble, byClear_Pulse_Width, &ValidPixelNumber);
  SetExtraSetting (chip, wXResolution, wCCD_PixelNumber, FALSE);

  /* calculate line time  */
  byPHTG_PulseWidth = chip->Timing.PHTG_PluseWidth;
  byPHTG_WaitWidth = chip->Timing.PHTG_WaitWidth;
  dwLinePixelReport = ((byClear_Pulse_Width + 1) * 2 +
		       (byPHTG_PulseWidth + 1) * (1) +
		       (byPHTG_WaitWidth + 1) * (1) +
		       (wCCD_PixelNumber + 1)) * (byDummyCycleNum + 1);

  DBG (DBG_ASIC, "Motor Time = %d\n",
       (dwLinePixelReport * wYResolution / wNowMotorDPI) / wMultiMotorStep);
  if ((dwLinePixelReport * wYResolution / wNowMotorDPI) / wMultiMotorStep >
      64000)
    {
      DBG (DBG_ASIC, "Motor Time Over Flow !!!\n");
    }

  EndSpeed =
    (unsigned short) ((dwLinePixelReport * wYResolution / wNowMotorDPI) /
	    wMultiMotorStep);
  SetMotorStepTable (chip, lpMotorStepsTable, wY, dwTotalLineTheBufferNeed * wNowMotorDPI / wYResolution * wMultiMotorStep, wYResolution);	/*modified by Chester 92/04/08 */


  if (EndSpeed >= 20000)
    {
      Asic_MotorMove (chip, 1, wY / wMultiMotorStep);
      isUniformSpeedToScan = UNIFORM_MOTOR_AND_SCAN_SPEED_ENABLE;
      isScanBackTracking = SCAN_BACK_TRACKING_DISABLE;
      isMotorMoveToFirstLine = MOTOR_MOVE_TO_FIRST_LINE_DISABLE;
    }

  Mustek_SendData (chip, ES01_F3_ActionOption, isMotorMoveToFirstLine |
		   MOTOR_BACK_HOME_AFTER_SCAN_DISABLE |
		   SCAN_ENABLE |
		   isScanBackTracking |
		   INVERT_MOTOR_DIRECTION_DISABLE |
		   isUniformSpeedToScan |
		   ES01_STATIC_SCAN_DISABLE | MOTOR_TEST_LOOP_DISABLE);

  if (EndSpeed > 8000)
    {
      StartSpeed = EndSpeed;
    }
  else
    {
      if (EndSpeed <= 1000)
	StartSpeed = EndSpeed + 4500;
      else
	StartSpeed = EndSpeed + 3500;
    }

  Mustek_SendData (chip, ES01_FD_MotorFixedspeedLSB, LOBYTE (EndSpeed));
  Mustek_SendData (chip, ES01_FE_MotorFixedspeedMSB, HIBYTE (EndSpeed));

  lpMotorTable = (unsigned short *) malloc (512 * 8 * 2);
  memset (lpMotorTable, 0, 512 * 8 * sizeof (unsigned short));

  CalMotorTable.StartSpeed = StartSpeed;
  CalMotorTable.EndSpeed = EndSpeed;
  CalMotorTable.AccStepBeforeScan = lpMotorStepsTable->wScanAccSteps;
  CalMotorTable.DecStepAfterScan = lpMotorStepsTable->bScanDecSteps;
  CalMotorTable.lpMotorTable = lpMotorTable;

  CalculateMotorTable (&CalMotorTable, wYResolution);

  CurrentPhase.MotorDriverIs3967 = 0;
  CurrentPhase.FillPhase = 1;
  CurrentPhase.MoveType = bMotorMoveType;
  SetMotorCurrent (chip, EndSpeed, &CurrentPhase);
  LLFSetMotorCurrentAndPhase (chip, &CurrentPhase);

  DBG (DBG_ASIC,
       "EndSpeed = %d, BytesCountPerRow=%d, MotorCurrentTable=%d, LinePixelReport=%d\n",
       EndSpeed, chip->dwBytesCountPerRow, CurrentPhase.MotorCurrentTableA[0],
       dwLinePixelReport);

  /* write motor table */
  RealTableSize = 512 * 8;
  dwTableBaseAddr = PackAreaStartAddress - RealTableSize;

  dwStartAddr = dwTableBaseAddr;

  RamAccess.ReadWrite = WRITE_RAM;
  RamAccess.IsOnChipGamma = EXTERNAL_RAM;
  RamAccess.DramDelayTime = SDRAMCLK_DELAY_12_ns;
  RamAccess.LoStartAddress = (unsigned short) (dwStartAddr);
  RamAccess.HiStartAddress = (unsigned short) (dwStartAddr >> 16);
  RamAccess.RwSize = RealTableSize * 2;
  RamAccess.BufferPtr = (SANE_Byte *) lpMotorTable;
  LLFRamAccess (chip, &RamAccess);

  Mustek_SendData (chip, ES01_DB_PH_RESET_EDGE_TIMING_ADJUST, 0x00);

  Mustek_SendData (chip, ES01_DC_CLEAR_EDGE_TO_PH_TG_EDGE_WIDTH, 0);

  Mustek_SendData (chip, ES01_9D_MotorTableAddrA14_A21,
		   (SANE_Byte) (dwTableBaseAddr >> TABLE_OFFSET_BASE));


  /* set address and shading table */
  /*set image buffer range and write shading table */
  RealTableSize =
    (ACC_DEC_STEP_TABLE_SIZE * NUM_OF_ACC_DEC_STEP_TABLE) +
    ShadingTableSize (ValidPixelNumber);
  dwShadingTableAddr =
    PackAreaStartAddress -
    (((RealTableSize +
       (TABLE_BASE_SIZE - 1)) >> TABLE_OFFSET_BASE) << TABLE_OFFSET_BASE);
  dwEndAddr = dwShadingTableAddr - 1;
  dwStartAddr = dwShadingTableAddr;

  if (wXResolution > 600)
    dbXRatioAdderDouble = 1200 / wXResolution;
  else
    dbXRatioAdderDouble = 600 / wXResolution;

  RamAccess.ReadWrite = WRITE_RAM;
  RamAccess.IsOnChipGamma = EXTERNAL_RAM;
  RamAccess.DramDelayTime = SDRAMCLK_DELAY_12_ns;
  RamAccess.LoStartAddress = (unsigned short) (dwStartAddr);
  RamAccess.HiStartAddress = (unsigned short) (dwStartAddr >> 16);
  RamAccess.RwSize =
    ShadingTableSize ((int) ((wWidth + 4) * dbXRatioAdderDouble)) *
    sizeof (unsigned short);
  RamAccess.BufferPtr = (SANE_Byte *) chip->lpShadingTable;
  LLFRamAccess (chip, &RamAccess);

  /*tell scan chip the shading table address, unit is 2^15 bytes(2^14 word) */
  Mustek_SendData (chip, ES01_9B_ShadingTableAddrA14_A21,
		   (SANE_Byte) (dwShadingTableAddr >> TABLE_OFFSET_BASE));

  /*empty bank */
  Mustek_SendData (chip, ES01_FB_BufferEmptySize16WordLSB,
		   LOBYTE (WaitBufferOneLineSize >> (7 - 3)));
  Mustek_SendData (chip, ES01_FC_BufferEmptySize16WordMSB,
		   HIBYTE (WaitBufferOneLineSize >> (7 - 3)));

  /*full bank */
  wFullBank =
    (unsigned short) ((dwEndAddr -
	     (((dwLineWidthPixel * BytePerPixel) / 2) * 3 * 1)) / BANK_SIZE);
  Mustek_SendData (chip, ES01_F9_BufferFullSize16WordLSB, LOBYTE (wFullBank));
  Mustek_SendData (chip, ES01_FA_BufferFullSize16WordMSB, HIBYTE (wFullBank));


  /* set buffer address */
  LLFSetRamAddress (chip, 0x0, dwEndAddr, ACCESS_DRAM);

  Mustek_SendData (chip, ES01_00_ADAFEConfiguration, 0x70);
  Mustek_SendData (chip, ES01_02_ADAFEMuxConfig, 0x80);

  free (lpMotorTable);
  free (lpMotorStepsTable);
  chip->firmwarestate = FS_OPENED;

  DBG (DBG_ASIC, "Asic_SetWindow: Exit\n");
  return status;
}

static STATUS
Asic_Reset (PAsic chip)
{
  STATUS status = STATUS_GOOD;
  DBG (DBG_ASIC, "Asic_Reset: Enter\n");

  chip->lsLightSource = LS_REFLECTIVE;

  chip->dwBytesCountPerRow = 0;
  chip->AD.DirectionR = 0;
  chip->AD.DirectionG = 0;
  chip->AD.DirectionB = 0;
  chip->AD.GainR = 0;
  chip->AD.GainG = 0;
  chip->AD.GainB = 0;

  chip->AD.OffsetR = 0;
  chip->AD.OffsetG = 0;
  chip->AD.OffsetB = 0;

  chip->Scan.TotalMotorSteps = 60000;
  chip->Scan.StartLine = 0;
  chip->Scan.StartPixel = 0;

  DBG (DBG_ASIC, "Asic_Reset: Exit\n");
  return status;
}

static STATUS
Asic_SetSource (PAsic chip, LIGHTSOURCE lsLightSource)
{
  STATUS status = STATUS_GOOD;
  DBG (DBG_ASIC, "Asic_SetSource: Enter\n");

  chip->lsLightSource = lsLightSource;
  switch (chip->lsLightSource)
    {
    case 1:
      DBG (DBG_ASIC, "Asic_SetSource: Source is Reflect\n");
      break;
    case 2:
      DBG (DBG_ASIC, "Asic_SetSource: Source is Postion\n");
      break;
    case 4:
      DBG (DBG_ASIC, "Asic_SetSource: Source is Negtive\n");
      break;
    default:
      DBG (DBG_ASIC, "Asic_SetSource: Source error\n");
    }

  DBG (DBG_ASIC, "Asic_SetSource: Exit\n");
  return status;
}

static STATUS
Asic_ScanStart (PAsic chip)
{
  STATUS status = STATUS_GOOD;
  DBG (DBG_ASIC, "Asic_ScanStart: Enter\n");

  if (chip->firmwarestate != FS_OPENED)
    {
      DBG (DBG_ERR, "Asic_ScanStart: Scanner is not opened\n");
      return STATUS_INVAL;
    }

  Mustek_SendData (chip, ES01_8B_Status, 0x1c | 0x20);
  Mustek_WriteAddressLineForRegister (chip, 0x8B);
  Mustek_ClearFIFO (chip);
  Mustek_SendData (chip, ES01_F4_ActiveTriger, ACTION_TRIGER_ENABLE);


  chip->firmwarestate = FS_SCANNING;

  DBG (DBG_ASIC, "Asic_ScanStart: Exit\n");
  return status;
}

static STATUS
Asic_ScanStop (PAsic chip)
{
  STATUS status = STATUS_GOOD;
  SANE_Byte temps[2];
  SANE_Byte buf[4];

  DBG (DBG_ASIC, "Asic_ScanStop: Enter\n");

  if (chip->firmwarestate < FS_SCANNING)
    {
      return status;
    }

  usleep (100 * 1000);

  buf[0] = 0x02;		/*stop */
  buf[1] = 0x02;
  buf[2] = 0x02;
  buf[3] = 0x02;
  status = WriteIOControl (chip, 0xc0, 0, 4, buf);
  if (status != STATUS_GOOD)
    {
      DBG (DBG_ERR, "Asic_ScanStop: Stop scan error\n");
      return status;
    }

  buf[0] = 0x00;		/*clear */
  buf[1] = 0x00;
  buf[2] = 0x00;
  buf[3] = 0x00;
  status = WriteIOControl (chip, 0xc0, 0, 4, buf);
  if (status != STATUS_GOOD)
    {
      DBG (DBG_ERR, "Asic_ScanStop: Clear scan error\n");
      return status;
    }

  status = Mustek_DMARead (chip, 2, temps);
  if (status != STATUS_GOOD)
    {
      DBG (DBG_ERR, "Asic_ScanStop: DMAReadGeneralMode error\n");
      return status;
    }

  Mustek_SendData (chip, ES01_F3_ActionOption, 0);
  Mustek_SendData (chip, ES01_86_DisableAllClockWhenIdle,
		   CLOSE_ALL_CLOCK_DISABLE);
  Mustek_SendData (chip, ES01_F4_ActiveTriger, ACTION_TRIGER_DISABLE);
  Mustek_ClearFIFO (chip);

  chip->firmwarestate = FS_OPENED;
  DBG (DBG_ASIC, "Asic_ScanStop: Exit\n");
  return status;
}

static STATUS
Asic_ReadImage (PAsic chip, SANE_Byte * pBuffer, unsigned short LinesCount)
{
  STATUS status = STATUS_GOOD;
  unsigned int dwXferBytes;

  DBG (DBG_ASIC, "Asic_ReadImage: Enter : LinesCount = %d\n", LinesCount);

  if (chip->firmwarestate != FS_SCANNING)
    {
      DBG (DBG_ERR, "Asic_ReadImage: Scanner is not scanning\n");
      return STATUS_INVAL;
    }

  dwXferBytes = (unsigned int) (LinesCount) * chip->dwBytesCountPerRow;
  DBG (DBG_ASIC, "Asic_ReadImage: chip->dwBytesCountPerRow = %d\n",
       chip->dwBytesCountPerRow);

  /* HOLD: an unsigned long can't be < 0
  if (dwXferBytes < 0)
    {
      DBG (DBG_ASIC, "Asic_ReadImage: dwXferBytes <0\n");
      return STATUS_INVAL;
    }
  */
  if (dwXferBytes == 0)

    {
      DBG (DBG_ASIC, "Asic_ReadImage: dwXferBytes == 0\n");
      return STATUS_GOOD;
    }

  status = Mustek_DMARead (chip, dwXferBytes, pBuffer);

  DBG (DBG_ASIC, "Asic_ReadImage: Exit\n");
  return status;
}


#if SANE_UNUSED
static STATUS
Asic_CheckFunctionKey (PAsic chip, SANE_Byte * key)
{
  STATUS status = STATUS_GOOD;
  SANE_Byte bBuffer_1 = 0xff;
  SANE_Byte bBuffer_2 = 0xff;

  DBG (DBG_ASIC, "Asic_CheckFunctionKey: Enter\n");

  if (chip->firmwarestate != FS_OPENED)
    {
      DBG (DBG_ERR, "Asic_CheckFunctionKey: Scanner is not Opened\n");
      return STATUS_INVAL;
    }

  Mustek_SendData (chip, ES01_97_GPIOControl0_7, 0x00);
  Mustek_SendData (chip, ES01_95_GPIOValue0_7, 0x17);
  Mustek_SendData (chip, ES01_98_GPIOControl8_15, 0x00);
  Mustek_SendData (chip, ES01_96_GPIOValue8_15, 0x08);

  GetChipStatus (chip, 0x02, &bBuffer_1);
  GetChipStatus (chip, 0x03, &bBuffer_2);


  if (((0xff - bBuffer_1) & 0x10) == 0x10)
    *key = 0x01;

  if (((0xff - bBuffer_1) & 0x01) == 0x01)
    *key = 0x02;
  if (((0xff - bBuffer_1) & 0x04) == 0x04)
    *key = 0x04;
  if (((0xff - bBuffer_2) & 0x08) == 0x08)
    *key = 0x08;
  if (((0xff - bBuffer_1) & 0x02) == 0x02)
    *key = 0x10;


  DBG (DBG_ASIC, "CheckFunctionKey=%d\n", *key);
  DBG (DBG_ASIC, "Asic_CheckFunctionKey: Exit\n");
  return status;
}
#endif

static STATUS
Asic_IsTAConnected (PAsic chip, SANE_Bool * hasTA)
{
  SANE_Byte bBuffer_1 = 0xff;

  DBG (DBG_ASIC, "Asic_IsTAConnected: Enter\n");

  Mustek_SendData (chip, ES01_97_GPIOControl0_7, 0x00);
  Mustek_SendData (chip, ES01_95_GPIOValue0_7, 0x00);

  Mustek_SendData (chip, ES01_98_GPIOControl8_15, 0x00);
  Mustek_SendData (chip, ES01_96_GPIOValue8_15, 0x00);


  GetChipStatus (chip, 0x02, &bBuffer_1);

  if (((0xff - bBuffer_1) & 0x08) == 0x08)
    *hasTA = TRUE;
  else
    *hasTA = FALSE;

  DBG (DBG_ASIC, "hasTA=%d\n", *hasTA);
  DBG (DBG_ASIC, "Asic_IsTAConnected():Exit\n");
  return STATUS_GOOD;
}

#if SANE_UNUSED
static STATUS
Asic_DownloadGammaTable (PAsic chip, void * lpBuffer)
{
  STATUS status = STATUS_GOOD;
  DBG (DBG_ASIC, "Asic_DownloadGammaTable()\n");

  chip->lpGammaTable = lpBuffer;

  DBG (DBG_ASIC, "Asic_DownloadGammaTable: Exit\n");
  return status;
}
#endif

static STATUS
Asic_ReadCalibrationData (PAsic chip, void * pBuffer,
			  unsigned int dwXferBytes, SANE_Byte bScanBits)
{
  STATUS status = STATUS_GOOD;
  SANE_Byte * pCalBuffer;
  unsigned int dwTotalReadData;
  unsigned int dwReadImageData;

  DBG (DBG_ASIC, "Asic_ReadCalibrationData: Enter\n");

  if (chip->firmwarestate != FS_SCANNING)
    {
      DBG (DBG_ERR, "Asic_ReadCalibrationData: Scanner is not scanning\n");
      return STATUS_INVAL;
    }

  if (bScanBits == 24)
    {
      unsigned int i;
      pCalBuffer = (SANE_Byte *) malloc (dwXferBytes);
      if (pCalBuffer == NULL)
	{
	  DBG (DBG_ERR, 
		   "Asic_ReadCalibrationData: Can't malloc bCalBuffer memory\n");
	  return STATUS_MEM_ERROR;
	}

      for (dwTotalReadData = 0; dwTotalReadData < dwXferBytes;)
	{
	  dwReadImageData = (dwXferBytes - dwTotalReadData) < 65536 ?
	    (dwXferBytes - dwTotalReadData) : 65536;

	  Mustek_DMARead (chip, dwReadImageData,
			  (SANE_Byte *) (pCalBuffer + dwTotalReadData));
	  dwTotalReadData += dwReadImageData;
	}

      dwXferBytes /= 3;
      for (i = 0; i < dwXferBytes; i++)

	{
	  *((SANE_Byte *) pBuffer + i) = *(pCalBuffer + i * 3);
	  *((SANE_Byte *) pBuffer + dwXferBytes + i) = *(pCalBuffer + i * 3 + 1);
	  *((SANE_Byte *) pBuffer + dwXferBytes * 2 + i) =
	    *(pCalBuffer + i * 3 + 2);
	}
      free (pCalBuffer);
    }
  else if (bScanBits == 8)
    {
      for (dwTotalReadData = 0; dwTotalReadData < dwXferBytes;)
	{
	  dwReadImageData = (dwXferBytes - dwTotalReadData) < 65536 ?
	    (dwXferBytes - dwTotalReadData) : 65536;

	  Mustek_DMARead (chip, dwReadImageData,
			  (SANE_Byte *) pBuffer + dwTotalReadData);
	  dwTotalReadData += dwReadImageData;
	}
    }

  DBG (DBG_ASIC, "Asic_ReadCalibrationData: Exit\n");
  return status;
}

static STATUS
Asic_SetMotorType (PAsic chip, SANE_Bool isMotorMove, SANE_Bool isUniformSpeed)
{
  STATUS status = STATUS_GOOD;
  isUniformSpeed = isUniformSpeed;
  DBG (DBG_ASIC, "Asic_SetMotorType:Enter\n");

  if (isMotorMove)
    {
      chip->isMotorMove = MOTOR_0_ENABLE;
    }
  else
    chip->isMotorMove = MOTOR_0_DISABLE;

  DBG (DBG_ASIC, "isMotorMove=%d\n", chip->isMotorMove);
  DBG (DBG_ASIC, "Asic_SetMotorType: Exit\n");
  return status;
}

static STATUS
Asic_MotorMove (PAsic chip, SANE_Bool isForward, unsigned int dwTotalSteps)
{
  STATUS status = STATUS_GOOD;
  unsigned short *NormalMoveMotorTable;
  LLF_CALCULATEMOTORTABLE CalMotorTable;
  LLF_MOTOR_CURRENT_AND_PHASE CurrentPhase;
  LLF_SETMOTORTABLE LLF_SetMotorTable;
  LLF_MOTORMOVE MotorMove;

  DBG (DBG_ASIC, "Asic_MotorMove:Enter\n");

  NormalMoveMotorTable = (unsigned short *) malloc (512 * 8 * 2);

  CalMotorTable.StartSpeed = 5000;
  CalMotorTable.EndSpeed = 1800;
  CalMotorTable.AccStepBeforeScan = 511;
  CalMotorTable.lpMotorTable = NormalMoveMotorTable;
  LLFCalculateMotorTable (&CalMotorTable);

  CurrentPhase.MotorDriverIs3967 = 0;
  CurrentPhase.MotorCurrentTableA[0] = 200;
  CurrentPhase.MotorCurrentTableB[0] = 200;
  CurrentPhase.MoveType = _4_TABLE_SPACE_FOR_FULL_STEP;
  LLFSetMotorCurrentAndPhase (chip, &CurrentPhase);

  LLF_SetMotorTable.MotorTableAddress = 0;
  LLF_SetMotorTable.MotorTablePtr = NormalMoveMotorTable;
  LLFSetMotorTable (chip, &LLF_SetMotorTable);

  free (NormalMoveMotorTable);

  MotorMove.MotorSelect = MOTOR_0_ENABLE | MOTOR_1_DISABLE;
  MotorMove.MotorMoveUnit = ES03_TABLE_DEFINE;
  MotorMove.MotorSpeedUnit = SPEED_UNIT_1_PIXEL_TIME;
  MotorMove.MotorSyncUnit = MOTOR_SYNC_UNIT_1_PIXEL_TIME;
  MotorMove.HomeSensorSelect = HOME_SENSOR_0_ENABLE;
  MotorMove.ActionMode = ACTION_MODE_ACCDEC_MOVE;
  MotorMove.ActionType = isForward;

  if (dwTotalSteps > 1000)
    {
      MotorMove.AccStep = 511;
      MotorMove.DecStep = 255;
      MotorMove.FixMoveSteps = dwTotalSteps - (511 + 255);
    }
  else
    {

      MotorMove.ActionMode = ACTION_MODE_UNIFORM_SPEED_MOVE;
      MotorMove.AccStep = 1;
      MotorMove.DecStep = 1;
      MotorMove.FixMoveSteps = dwTotalSteps - 2;
    }

  MotorMove.FixMoveSpeed = 7000;
  MotorMove.WaitOrNoWait = TRUE;

  LLFMotorMove (chip, &MotorMove);

  DBG (DBG_ASIC, "Asic_MotorMove: Exit\n");
  return status;
}

static STATUS
Asic_CarriageHome (PAsic chip, SANE_Bool isTA)
{
  STATUS status = STATUS_GOOD;
  SANE_Bool LampHome, TAHome;
  isTA = isTA;

  DBG (DBG_ASIC, "Asic_CarriageHome:Enter\n");

  status = IsCarriageHome (chip, &LampHome, &TAHome);
  if (!LampHome)
    {
      status = MotorBackHome (chip, TRUE);
    }

  DBG (DBG_ASIC, "Asic_CarriageHome: Exit\n");
  return status;
}

static STATUS
Asic_SetShadingTable (PAsic chip, unsigned short * lpWhiteShading,
		      unsigned short * lpDarkShading,
		      unsigned short wXResolution, unsigned short wWidth, unsigned short wX)
{
  STATUS status = STATUS_GOOD;
  unsigned short i, j, n;
  unsigned short wValidPixelNumber;
  double dbXRatioAdderDouble;
  unsigned int wShadingTableSize;

  wX = wX;
  DBG (DBG_ASIC, "Asic_SetShadingTable:Enter\n");

  if (chip->firmwarestate < FS_OPENED)

    OpenScanChip (chip);
  if (chip->firmwarestate == FS_SCANNING)
    Mustek_SendData (chip, ES01_F4_ActiveTriger, ACTION_TRIGER_DISABLE);


  if (wXResolution > 600)
    dbXRatioAdderDouble = 1200 / wXResolution;
  else
    dbXRatioAdderDouble = 600 / wXResolution;

  wValidPixelNumber = (unsigned short) ((wWidth + 4) * dbXRatioAdderDouble);
  DBG (DBG_ASIC, "wValidPixelNumber = %d\n", wValidPixelNumber);

  /* clear old Shading table, if it has. */
  /*  first 4 element and lastest 5 of Shading table can't been used */
  wShadingTableSize = (ShadingTableSize (wValidPixelNumber)) * sizeof (unsigned short);
  if (chip->lpShadingTable != NULL)
    {
      free (chip->lpShadingTable);

    }

  DBG (DBG_ASIC, "Alloc a new shading table= %d Byte!\n", wShadingTableSize);
  chip->lpShadingTable = (SANE_Byte *) malloc (wShadingTableSize);
  if (chip->lpShadingTable == NULL)
    {
      DBG (DBG_ASIC, "lpShadingTable == NULL\n");
      return STATUS_MEM_ERROR;
    }

  n = 0;
  for (i = 0; i <= (wValidPixelNumber / 40); i++)
    {
      if (i < (wValidPixelNumber / 40))
	{
	  for (j = 0; j < 40; j++)
	    {
	      *((unsigned short *) chip->lpShadingTable + i * 256 + j * 6) =
		*((unsigned short *) lpDarkShading + n * 3);
	      *((unsigned short *) chip->lpShadingTable + i * 256 + j * 6 + 2) =
		*((unsigned short *) lpDarkShading + n * 3 + 1);
	      *((unsigned short *) chip->lpShadingTable + i * 256 + j * 6 + 4) =
		*((unsigned short *) lpDarkShading + n * 3 + 2);

	      *((unsigned short *) chip->lpShadingTable + i * 256 + j * 6 + 1) =
		*((unsigned short *) lpWhiteShading + n * 3);
	      *((unsigned short *) chip->lpShadingTable + i * 256 + j * 6 + 3) =
		*((unsigned short *) lpWhiteShading + n * 3 + 1);
	      *((unsigned short *) chip->lpShadingTable + i * 256 + j * 6 + 5) =
		*((unsigned short *) lpWhiteShading + n * 3 + 2);
	      if ((j % (unsigned short) dbXRatioAdderDouble) ==
		  (dbXRatioAdderDouble - 1))
		n++;

	      if (i == 0 && j < 4 * dbXRatioAdderDouble)
		n = 0;
	    }
	}
      else
	{
	  for (j = 0; j < (wValidPixelNumber % 40); j++)
	    {
	      *((unsigned short *) chip->lpShadingTable + i * 256 + j * 6) =
		*((unsigned short *) lpDarkShading + (n) * 3);
	      *((unsigned short *) chip->lpShadingTable + i * 256 + j * 6 + 2) =
		*((unsigned short *) lpDarkShading + (n) * 3 + 1);
	      *((unsigned short *) chip->lpShadingTable + i * 256 + j * 6 + 4) =
		*((unsigned short *) lpDarkShading + (n) * 3 + 2);

	      *((unsigned short *) chip->lpShadingTable + i * 256 + j * 6 + 1) =
		*((unsigned short *) lpWhiteShading + (n) * 3);
	      *((unsigned short *) chip->lpShadingTable + i * 256 + j * 6 + 3) =
		*((unsigned short *) lpWhiteShading + (n) * 3 + 1);
	      *((unsigned short *) chip->lpShadingTable + i * 256 + j * 6 + 5) =
		*((unsigned short *) lpWhiteShading + (n) * 3 + 2);

	      if ((j % (unsigned short) dbXRatioAdderDouble) ==
		  (dbXRatioAdderDouble - 1))
		n++;

	      if (i == 0 && j < 4 * dbXRatioAdderDouble)
		n = 0;
	    }
	}
    }

  DBG (DBG_ASIC, "Asic_SetShadingTable: Exit\n");
  return status;
}

static STATUS
Asic_WaitCarriageHome (PAsic chip, SANE_Bool isTA)
{
  STATUS status = STATUS_GOOD;
  SANE_Bool LampHome, TAHome;
  int i;

  isTA = isTA;

  DBG (DBG_ASIC, "Asic_WaitCarriageHome:Enter\n");

  for (i = 0; i < 100; i++)
    {
      status = IsCarriageHome (chip, &LampHome, &TAHome);
      if (LampHome)
	break;
      usleep (300000);
    }
  if (i == 100)
    status = STATUS_DEVICE_BUSY;
  DBG (DBG_ASIC, "Wait %d s\n", (unsigned short) (i * 0.3));

  Mustek_SendData (chip, ES01_F4_ActiveTriger, ACTION_TRIGER_DISABLE);
  chip->firmwarestate = FS_OPENED;
  chip->motorstate = MS_STILL;

  DBG (DBG_ASIC, "Asic_WaitCarriageHome: Exit\n");
  return status;
}

static STATUS
Asic_SetCalibrate (PAsic chip, SANE_Byte bScanBits, unsigned short wXResolution,
		   unsigned short wYResolution, unsigned short wX, unsigned short wY,
		   unsigned short wWidth, unsigned short wLength, SANE_Bool isShading)
{
  STATUS status = STATUS_GOOD;
  unsigned short ValidPixelNumber;

  unsigned short wPerLineNeedBufferSize = 0;
  unsigned short BytePerPixel = 0;
  unsigned int dwTotal_PerLineNeedBufferSize = 0;
  unsigned int dwTotalLineTheBufferNeed = 0;
  unsigned short dwTotal_CCDResolution = 0;
  unsigned short wThinkCCDResolution = 0;
  unsigned short wCCD_PixelNumber = 0;
  unsigned short wScanAccSteps = 1;
  SANE_Byte byScanDecSteps = 1;
  unsigned int dwLineWidthPixel = 0;

  unsigned short wNowMotorDPI;
  unsigned short XRatioTypeWord;
  double XRatioTypeDouble;
  double XRatioAdderDouble;

  LLF_MOTORMOVE *lpMotorStepsTable =
    (LLF_MOTORMOVE *) malloc (sizeof (LLF_MOTORMOVE));

  SANE_Byte byDummyCycleNum = 1;
  unsigned short Total_MotorDPI;
  unsigned short BeforeScanFixSpeedStep = 0;
  unsigned short BackTrackFixSpeedStep = 20;
  unsigned short wMultiMotorStep = 1;
  SANE_Byte bMotorMoveType = _MOTOR_MOVE_TYPE;
  SANE_Byte isMotorMoveToFirstLine = MOTOR_MOVE_TO_FIRST_LINE_DISABLE;
  SANE_Byte isUniformSpeedToScan = UNIFORM_MOTOR_AND_SCAN_SPEED_ENABLE;
  SANE_Byte isScanBackTracking = SCAN_BACK_TRACKING_DISABLE;
  unsigned int TotalStep = 0;
  unsigned short StartSpeed, EndSpeed;
  LLF_CALCULATEMOTORTABLE CalMotorTable;
  LLF_MOTOR_CURRENT_AND_PHASE CurrentPhase;
  LLF_RAMACCESS RamAccess;
  unsigned int dwStartAddr, dwEndAddr, dwTableBaseAddr;
  SANE_Byte byClear_Pulse_Width = 0;
  unsigned int dwLinePixelReport = 0;
  SANE_Byte byPHTG_PulseWidth, byPHTG_WaitWidth;
  unsigned short * lpMotorTable = (unsigned short *) malloc (512 * 8 * 2);
  unsigned int RealTableSize;
  unsigned short wFullBank;

  DBG (DBG_ASIC, "Asic_SetCalibrate: Enter\n");
  DBG (DBG_ASIC,
       "bScanBits=%d,wXResolution=%d, wYResolution=%d,	wX=%d, wY=%d, wWidth=%d, wLength=%d\n",
       bScanBits, wXResolution, wYResolution, wX, wY, wWidth, wLength);

  if (chip->firmwarestate != FS_OPENED)
    {
      DBG (DBG_ERR, "Asic_SetCalibrate: Scanner is not opened\n");
      return STATUS_INVAL;
    }

  if (lpMotorStepsTable == NULL)
    {
      DBG (DBG_ERR, "Asic_SetCalibrate: insufficiency memory!\n");
      return STATUS_INVAL;
    }
  DBG (DBG_ASIC, "malloc LLF_MOTORMOVE =%ld Byte\n", (long int) (sizeof (LLF_MOTORMOVE)));

  Mustek_SendData (chip, ES01_F3_ActionOption, 0);
  Mustek_SendData (chip, ES01_86_DisableAllClockWhenIdle,
		   CLOSE_ALL_CLOCK_DISABLE);
  Mustek_SendData (chip, ES01_F4_ActiveTriger, ACTION_TRIGER_DISABLE);

  status = Asic_WaitUnitReady (chip);

  Mustek_SendData (chip, 0x1CD, 0);

  Mustek_SendData (chip, ES01_94_PowerSaveControl, 0x27 | 64 | 128);

  if (bScanBits > 24)
    {
      wPerLineNeedBufferSize = wWidth * 6;
      BytePerPixel = 6;
      chip->dwBytesCountPerRow = (unsigned int) (wWidth) * 6;
    }
  else if (bScanBits == 24)
    {
      wPerLineNeedBufferSize = wWidth * 3;
      chip->dwCalibrationBytesCountPerRow = wWidth * 3;
      BytePerPixel = 3;
      chip->dwBytesCountPerRow = (unsigned int) (wWidth) * 3;
    }
  else if ((bScanBits > 8) && (bScanBits <= 16))
    {
      wPerLineNeedBufferSize = wWidth * 2;
      BytePerPixel = 2;
      chip->dwBytesCountPerRow = (unsigned int) (wWidth) * 2;
    }
  else if ((bScanBits == 8))
    {
      wPerLineNeedBufferSize = wWidth;
      BytePerPixel = 1;
      chip->dwBytesCountPerRow = (unsigned int) (wWidth);
    }
  else if ((bScanBits < 8))
    {
      wPerLineNeedBufferSize = wWidth >> 3;
      BytePerPixel = 1;
      chip->dwBytesCountPerRow = (unsigned int) (wWidth);
    }
  DBG (DBG_ASIC,
       "wPerLineNeedBufferSize=%d,BytePerPixel=%d,dwBytesCountPerRow=%d\n",
       wPerLineNeedBufferSize, BytePerPixel, chip->dwBytesCountPerRow);


  dwTotal_PerLineNeedBufferSize = wPerLineNeedBufferSize;
  dwTotalLineTheBufferNeed = wLength;
  DBG (DBG_ASIC, "wPerLineNeedBufferSize=%d,wLength=%d\n",
       wPerLineNeedBufferSize, wLength);


  chip->Scan.Dpi = wXResolution;
  CCDTiming (chip);

  dwTotal_CCDResolution = SENSOR_DPI;
  if (chip->lsLightSource == LS_REFLECTIVE)
    {
      if (wXResolution > (dwTotal_CCDResolution / 2))
	{
	  wThinkCCDResolution = dwTotal_CCDResolution;
	  Mustek_SendData (chip, ES01_98_GPIOControl8_15, 0x01);
	  Mustek_SendData (chip, ES01_96_GPIOValue8_15, 0x01);
	  wCCD_PixelNumber = chip->Timing.wCCDPixelNumber_1200;
	}
      else
	{
	  wThinkCCDResolution = dwTotal_CCDResolution / 2;
	  Mustek_SendData (chip, ES01_98_GPIOControl8_15, 0x01);
	  Mustek_SendData (chip, ES01_96_GPIOValue8_15, 0x00);
	  wCCD_PixelNumber = chip->Timing.wCCDPixelNumber_600;
	}
    }
  else
    {
      if (wXResolution > (dwTotal_CCDResolution / 2))
	{
	  wThinkCCDResolution = dwTotal_CCDResolution;
	  Mustek_SendData (chip, ES01_98_GPIOControl8_15, 0x01);
	  Mustek_SendData (chip, ES01_96_GPIOValue8_15, 0x01);
	  wCCD_PixelNumber = TA_CAL_PIXELNUMBER;
	}
      else
	{
	  wThinkCCDResolution = dwTotal_CCDResolution / 2;
	  Mustek_SendData (chip, ES01_98_GPIOControl8_15, 0x01);
	  Mustek_SendData (chip, ES01_96_GPIOValue8_15, 0x00);
	  wCCD_PixelNumber = TA_CAL_PIXELNUMBER;
	}
    }
  DBG (DBG_ASIC, "wThinkCCDResolution=%d,wCCD_PixelNumber=%d\n",
       wThinkCCDResolution, wCCD_PixelNumber);

  dwLineWidthPixel = wWidth;

  if (isShading)
    wYResolution = 600;
  DBG (DBG_ASIC, "dwLineWidthPixel=%d,wYResolution=%d\n", dwLineWidthPixel,
       wYResolution);

  SetLineTimeAndExposure (chip);
  if (wYResolution == 600)
    {
      Mustek_SendData (chip, ES01_CB_CCDDummyCycleNumber, byDummyCycleNum);
      DBG (DBG_ASIC, "Find Boundary CCDDummyCycleNumber == %d\n",
	   byDummyCycleNum);
    }

  SetLEDTime (chip);

  /* calculate Y ratio */
  Total_MotorDPI = 1200;

  wNowMotorDPI = Total_MotorDPI;
  DBG (DBG_ASIC, "wNowMotorDPI=%d\n", wNowMotorDPI);

  /* SetADConverter */
  Mustek_SendData (chip, ES01_74_HARDWARE_SETTING,
		   MOTOR1_SERIAL_INTERFACE_G10_8_ENABLE | LED_OUT_G11_DISABLE
		   | SLAVE_SERIAL_INTERFACE_G15_14_DISABLE |
		   SHUTTLE_CCD_DISABLE);

  /* set AFE */
  Mustek_SendData (chip, ES01_9A_AFEControl,
		   AD9826_AFE | AUTO_CHANGE_AFE_GAIN_OFFSET_DISABLE);
  Mustek_SendData (chip, ES01_F7_DigitalControl, DIGITAL_REDUCE_DISABLE);

  XRatioTypeDouble = wXResolution;
  XRatioTypeDouble /= wThinkCCDResolution;
  XRatioAdderDouble = 1 / XRatioTypeDouble;
  XRatioTypeWord = (unsigned short) (XRatioTypeDouble * 32768);

  XRatioAdderDouble = (double) (XRatioTypeWord) / 32768;
  XRatioAdderDouble = 1 / XRatioAdderDouble;

  Mustek_SendData (chip, ES01_9E_HorizontalRatio1to15LSB,
		   LOBYTE (XRatioTypeWord));
  Mustek_SendData (chip, ES01_9F_HorizontalRatio1to15MSB,
		   HIBYTE (XRatioTypeWord));
  DBG (DBG_ASIC,
       "XRatioTypeDouble=%.2f,XRatioAdderDouble=%.2f,XRatioTypeWord=%d\n",
       XRatioTypeDouble, XRatioAdderDouble, XRatioTypeWord);

  if (chip->isMotorMove == MOTOR_0_ENABLE)
    {
      Mustek_SendData (chip, ES01_A6_MotorOption, MOTOR_0_ENABLE |
		       MOTOR_1_DISABLE |
		       HOME_SENSOR_0_ENABLE | ES03_TABLE_DEFINE);
    }
  else
    {
      Mustek_SendData (chip, ES01_A6_MotorOption, MOTOR_0_DISABLE |
		       MOTOR_1_DISABLE |
		       HOME_SENSOR_0_ENABLE | ES03_TABLE_DEFINE);
    }
  DBG (DBG_ASIC, "isMotorMove=%d\n", chip->isMotorMove);

  Mustek_SendData (chip, ES01_F6_MorotControl1, SPEED_UNIT_1_PIXEL_TIME |
		   MOTOR_SYNC_UNIT_1_PIXEL_TIME);

  wScanAccSteps = 1;
  byScanDecSteps = 1;
  DBG (DBG_ASIC, "wScanAccSteps=%d,byScanDecSteps=%d\n", wScanAccSteps,
       byScanDecSteps);

  Mustek_SendData (chip, ES01_AE_MotorSyncPixelNumberM16LSB, LOBYTE (0));
  Mustek_SendData (chip, ES01_AF_MotorSyncPixelNumberM16MSB, HIBYTE (0));
  DBG (DBG_ASIC, "MotorSyncPixelNumber=%d\n", 0);

  Mustek_SendData (chip, ES01_EC_ScanAccStep0_7, LOBYTE (wScanAccSteps));
  Mustek_SendData (chip, ES01_ED_ScanAccStep8_8, HIBYTE (wScanAccSteps));
  DBG (DBG_ASIC, "wScanAccSteps=%d\n", wScanAccSteps);

  DBG (DBG_ASIC, "BeforeScanFixSpeedStep=%d,BackTrackFixSpeedStep=%d\n",
       BeforeScanFixSpeedStep, BackTrackFixSpeedStep);

  Mustek_SendData (chip, ES01_EE_FixScanStepLSB,
		   LOBYTE (BeforeScanFixSpeedStep));
  Mustek_SendData (chip, ES01_8A_FixScanStepMSB,
		   HIBYTE (BeforeScanFixSpeedStep));
  DBG (DBG_ASIC, "BeforeScanFixSpeedStep=%d\n", BeforeScanFixSpeedStep);

  Mustek_SendData (chip, ES01_EF_ScanDecStep, byScanDecSteps);
  DBG (DBG_ASIC, "byScanDecSteps=%d\n", byScanDecSteps);

  Mustek_SendData (chip, ES01_E6_ScanBackTrackingStepLSB,
		   LOBYTE (BackTrackFixSpeedStep));
  Mustek_SendData (chip, ES01_E7_ScanBackTrackingStepMSB,
		   HIBYTE (BackTrackFixSpeedStep));
  DBG (DBG_ASIC, "BackTrackFixSpeedStep=%d\n", BackTrackFixSpeedStep);

  Mustek_SendData (chip, ES01_E8_ScanRestartStepLSB,
		   LOBYTE (BackTrackFixSpeedStep));
  Mustek_SendData (chip, ES01_E9_ScanRestartStepMSB,
		   HIBYTE (BackTrackFixSpeedStep));
  DBG (DBG_ASIC, "BackTrackFixSpeedStep=%d\n", BackTrackFixSpeedStep);

  switch (bMotorMoveType)
    {
    case _4_TABLE_SPACE_FOR_FULL_STEP:
      wMultiMotorStep = 1;
      break;

    case _8_TABLE_SPACE_FOR_1_DIV_2_STEP:
      wMultiMotorStep = 2;
      break;

    case _16_TABLE_SPACE_FOR_1_DIV_4_STEP:
      wMultiMotorStep = 4;
      break;

    case _32_TABLE_SPACE_FOR_1_DIV_8_STEP:
      wMultiMotorStep = 8;
      break;
    }
  DBG (DBG_ASIC, "wMultiMotorStep=%d\n", wMultiMotorStep);

  TotalStep = wScanAccSteps + BeforeScanFixSpeedStep +
    (dwTotalLineTheBufferNeed * wNowMotorDPI / wYResolution) + byScanDecSteps;
  DBG (DBG_ASIC, "TotalStep=%d\n", TotalStep);

  Mustek_SendData (chip, ES01_F0_ScanImageStep0_7, (SANE_Byte) (TotalStep));
  Mustek_SendData (chip, ES01_F1_ScanImageStep8_15, (SANE_Byte) (TotalStep >> 8));
  Mustek_SendData (chip, ES01_F2_ScanImageStep16_19,
		   (SANE_Byte) (TotalStep >> 16));

  SetScanMode (chip, bScanBits);

  DBG (DBG_ASIC,
       "isMotorMoveToFirstLine=%d,isUniformSpeedToScan=%d,isScanBackTracking=%d\n",
       isMotorMoveToFirstLine, isUniformSpeedToScan, isScanBackTracking);


  Mustek_SendData (chip, ES01_F3_ActionOption, isMotorMoveToFirstLine |
		   MOTOR_BACK_HOME_AFTER_SCAN_DISABLE |
		   SCAN_ENABLE |
		   isScanBackTracking |
		   INVERT_MOTOR_DIRECTION_DISABLE |
		   isUniformSpeedToScan |
		   ES01_STATIC_SCAN_DISABLE | MOTOR_TEST_LOOP_DISABLE);

  if (chip->lsLightSource == LS_REFLECTIVE)
    Mustek_SendData (chip, ES01_F8_WHITE_SHADING_DATA_FORMAT,
		     ES01_SHADING_3_INT_13_DEC);
  else
    Mustek_SendData (chip, ES01_F8_WHITE_SHADING_DATA_FORMAT,
		     ES01_SHADING_4_INT_12_DEC);

  SetPackAddress (chip, wXResolution, wWidth, wX, XRatioAdderDouble,
		  XRatioTypeDouble, byClear_Pulse_Width, &ValidPixelNumber);
  SetExtraSetting (chip, wXResolution, wCCD_PixelNumber, TRUE);

  byPHTG_PulseWidth = chip->Timing.PHTG_PluseWidth;
  byPHTG_WaitWidth = chip->Timing.PHTG_WaitWidth;
  dwLinePixelReport = ((byClear_Pulse_Width + 1) * 2 +
		       (byPHTG_PulseWidth + 1) * (1) +
		       (byPHTG_WaitWidth + 1) * (1) +
		       (wCCD_PixelNumber + 1)) * (byDummyCycleNum + 1);

  DBG (DBG_ASIC, "Motor Time = %d\n",
       (dwLinePixelReport * wYResolution / wNowMotorDPI));
  if ((dwLinePixelReport * wYResolution / wNowMotorDPI) > 64000)
    {
      DBG (DBG_ASIC, "Motor Time Over Flow !!!\n");
    }

  EndSpeed = (unsigned short) (dwLinePixelReport / (wNowMotorDPI / wYResolution));

  if (wXResolution > 600)
    {
      StartSpeed = EndSpeed;
    }
  else
    {
      StartSpeed = EndSpeed + 3500;
    }
  DBG (DBG_ASIC, "StartSpeed =%d, EndSpeed = %d\n", StartSpeed, EndSpeed);


  Mustek_SendData (chip, ES01_FD_MotorFixedspeedLSB, LOBYTE (EndSpeed));
  Mustek_SendData (chip, ES01_FE_MotorFixedspeedMSB, HIBYTE (EndSpeed));
  memset (lpMotorTable, 0, 512 * 8 * sizeof (unsigned short));

  CalMotorTable.StartSpeed = StartSpeed;
  CalMotorTable.EndSpeed = EndSpeed;
  CalMotorTable.AccStepBeforeScan = wScanAccSteps;
  CalMotorTable.lpMotorTable = lpMotorTable;

  LLFCalculateMotorTable (&CalMotorTable);

  CurrentPhase.MotorDriverIs3967 = 0;
  CurrentPhase.FillPhase = 1;
  CurrentPhase.MoveType = bMotorMoveType;
  CurrentPhase.MotorCurrentTableA[0] = 200;
  CurrentPhase.MotorCurrentTableB[0] = 200;
  LLFSetMotorCurrentAndPhase (chip, &CurrentPhase);

  RealTableSize = 512 * 8;
  dwTableBaseAddr = PackAreaStartAddress - RealTableSize;

  dwStartAddr = dwTableBaseAddr;

  RamAccess.ReadWrite = WRITE_RAM;
  RamAccess.IsOnChipGamma = EXTERNAL_RAM;
  RamAccess.DramDelayTime = SDRAMCLK_DELAY_12_ns;
  RamAccess.LoStartAddress = (unsigned short) (dwStartAddr);
  RamAccess.HiStartAddress = (unsigned short) (dwStartAddr >> 16);
  RamAccess.RwSize = RealTableSize * 2;
  RamAccess.BufferPtr = (SANE_Byte *) lpMotorTable;
  LLFRamAccess (chip, &RamAccess);

  Mustek_SendData (chip, ES01_9D_MotorTableAddrA14_A21,
		   (SANE_Byte) (dwTableBaseAddr >> TABLE_OFFSET_BASE));

  dwEndAddr = PackAreaStartAddress - (512 * 8) - 1;

  Mustek_SendData (chip, ES01_FB_BufferEmptySize16WordLSB,
		   LOBYTE (WaitBufferOneLineSize >> (7 - 3)));
  Mustek_SendData (chip, ES01_FC_BufferEmptySize16WordMSB,
		   HIBYTE (WaitBufferOneLineSize >> (7 - 3)));

  wFullBank =
    (unsigned short) ((dwEndAddr -
	     (((dwLineWidthPixel * BytePerPixel) / 2) * 3 * 1)) / BANK_SIZE);
  Mustek_SendData (chip, ES01_F9_BufferFullSize16WordLSB, LOBYTE (wFullBank));
  Mustek_SendData (chip, ES01_FA_BufferFullSize16WordMSB, HIBYTE (wFullBank));

  Mustek_SendData (chip, ES01_DB_PH_RESET_EDGE_TIMING_ADJUST, 0x00);

  LLFSetRamAddress (chip, 0x0, dwEndAddr, ACCESS_DRAM);

  Mustek_SendData (chip, ES01_DC_CLEAR_EDGE_TO_PH_TG_EDGE_WIDTH, 0);

  Mustek_SendData (chip, ES01_00_ADAFEConfiguration, 0x70);
  Mustek_SendData (chip, ES01_02_ADAFEMuxConfig, 0x80);


  free (lpMotorTable);
  free (lpMotorStepsTable);

  DBG (DBG_ASIC, "Asic_SetCalibrate: Exit\n");
  return status;
}


static STATUS
Asic_SetAFEGainOffset (PAsic chip)
{
  STATUS status = STATUS_GOOD;
  DBG (DBG_ASIC, "Asic_SetAFEGainOffset:Enter\n");

  status = SetAFEGainOffset (chip);

  DBG (DBG_ASIC, "Asic_SetAFEGainOffset: Exit\n");
  return status;
}
