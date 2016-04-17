/* sane - Scanner Access Now Easy.
   Copyright (C) 1997 Jeffrey S. Freedman
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
   If you do not wish that, delete this exception notice.  */

class Test
{
public static void main(String[] args)
	{
	Sane sane = new Sane();
	int version[] = new int[1];	// Array to get version #.
	int status = sane.init(version);
	if (status != Sane.STATUS_GOOD)
		{
		System.out.println("getDevices() failed.  Status= " + status);
		return;
		}
	System.out.println("VersionMajor =" + sane.versionMajor(version[0]));
	System.out.println("VersionMinor =" + sane.versionMinor(version[0]));
	System.out.println("VersionBuild =" + sane.versionBuild(version[0]));
					// Get list of devices.
					// Allocate room for 50.
	SaneDevice devList[] = new SaneDevice[50];
	status = sane.getDevices(devList, false);
	if (status != Sane.STATUS_GOOD)
		{
		System.out.println("getDevices() failed.  Status= " + status);
		return;
		}
	for (int i = 0; i < 50 && devList[i] != null; i++)
		{
		System.out.println("Device '" + devList[i].name + "' is a " +
			devList[i].vendor + " " + devList[i].model + " " +
			devList[i].type);
		}
	int handle[] = new int[1];
	status = sane.open(devList[0].name, handle);
	if (status != Sane.STATUS_GOOD)
		{
		System.out.println("open() failed.  Status= " + status);
		return;
		}
	System.out.println("Open handle=" + handle[0]);
					// Get # of device options.
	int numDevOptions[] = new int[1];
	status = sane.getControlOption(handle[0], 0, numDevOptions, null);
	if (status != Sane.STATUS_GOOD)
		{
		System.out.println("controlOption() failed.  Status= " 
								+ status);
		return;
		}
	System.out.println("Number of device options=" + numDevOptions[0]);
	for (int i = 0; i < numDevOptions[0]; i++)
		{
		SaneOption opt = sane.getOptionDescriptor(handle[0], i);
		if (opt == null)
			{
			System.out.println("getOptionDescriptor() failed for "
						+ i);
			continue;
			}
		System.out.println("Option name:  " + opt.name);
		System.out.println("Option title: " + opt.title);
		System.out.println("Option desc:  " + opt.desc);
		switch (opt.constraintType)
			{
		case SaneOption.CONSTRAINT_RANGE:
			System.out.println("Range:  " + 
				opt.rangeConstraint.min + ", " +
				opt.rangeConstraint.max + ", " +
				opt.rangeConstraint.quant);
			break;
		case SaneOption.CONSTRAINT_WORD_LIST:
			System.out.print("Word list: ");
			for (int j = 0; j < opt.wordListConstraint[0]; j++)
				System.out.print(opt.wordListConstraint[j]);
			System.out.println();
			break;
		case SaneOption.CONSTRAINT_STRING_LIST:
			for (int j = 0; opt.stringListConstraint[j] != null;
									j++)
				System.out.print("String constraint: " +
						opt.stringListConstraint[j]);
			break;
		default:
			System.out.println("No constraint.");
			break;
			}
		}
	status = sane.setControlOption(handle[0], 2, 
			SaneOption.ACTION_SET_VALUE, "../test1.pnm", null);
	if (status != Sane.STATUS_GOOD)
		{
		System.out.println("setControlOption() failed.  Status= " 
								+ status);
		}
	//
	//	Main scanning loop.
	//
	SaneParameters parm = new SaneParameters();
	int dataLen = 32*1024;
	byte [] data = new byte[dataLen];
	int [] readLen = new int[1];
	int frameCnt = 0;
	do
		{
		frameCnt++;
		System.out.println("Reading frame #" + frameCnt);
		status = sane.start(handle[0]);
		if (status != Sane.STATUS_GOOD)
			{
			System.out.println("start() failed.  Status= " 
								+ status);
			return;
			}
		status = sane.getParameters(handle[0], parm);
		if (status != Sane.STATUS_GOOD)
			{
			System.out.println("getParameters() failed.  Status= " 
								+ status);
			return;	//++++cleanup.
			}
		while ((status = sane.read(handle[0], data, dataLen, readLen))
							== Sane.STATUS_GOOD)
			{
			System.out.println("Read " + readLen[0] + " bytes.");
			// ++++++++Process data.
			}
		if (status != Sane.STATUS_EOF)
			{
			System.out.println("read() failed.  Status= "
								+ status);
			}
		}
	while (!parm.lastFrame);

	sane.close(handle[0]);
	}
}
