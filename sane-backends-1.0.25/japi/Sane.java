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

//
//	Sane.java - Java version of SANE API.
//
//	Written: 10/9/97 - JSF
//

public class Sane
{
	//
	//	Public constants:
	//
public static int FIXED_SCALE_SHIFT = 16;
	//
	//	Sane status values:
	//
public static int STATUS_GOOD = 0;	// everything A-OK
public static int STATUS_UNSUPPORTED = 1;// operation is not supported
public static int STATUS_CANCELLED = 2;	// operation was cancelled
public static int STATUS_DEVICE_BUSY = 3;// device is busy; try again later
public static int STATUS_INVAL = 4;	// data is invalid (includes no 
					//   dev at open)
public static int STATUS_EOF = 5;	// no more data available (end-of-file)
public static int STATUS_JAMMED = 6;	// document feeder jammed
public static int STATUS_NO_DOCS = 7;	// document feeder out of documents
public static int STATUS_COVER_OPEN = 8;// scanner cover is open
public static int STATUS_IO_ERROR = 9;	// error during device I/O
public static int STATUS_NO_MEM = 10;	// out of memory
					// access to resource has been denied
public static int STATUS_ACCESS_DENIED = 11;
	//
	//	Initialize when class is loaded.
	//
static	{
	System.loadLibrary("sanej");
	}
	//
	//	Private methods:
	//
				// Get list of devices.
private native int getDevicesNative(
				SaneDevice[] deviceList, boolean localOnly);
				// Get option descriptor.
private native void getOptionNative(int handle, int option, SaneOption opt);
	//
	//	Public methods:
	//
public Sane()
	{  }
public int fix(double v)
	{ return (int) ((v) * (1 << FIXED_SCALE_SHIFT)); }
public double unfix(int v)
	{ return (double)(v) / (1 << FIXED_SCALE_SHIFT); }
public int versionMajor(int code)
	{ return ((code) >> 24) &   0xff; }
public int versionMinor(int code)
	{ return ((code) >> 16) &   0xff; }
public int versionBuild(int code)
	{ return ((code) >>  0) & 0xffff; }
	//
	//	SANE interface.
	//
				// Initialize, and return STATUS_
public native int init(int[] versionCode);
public native void exit();	// All done.
				// Get list of devices.
public int getDevices(SaneDevice[] deviceList, boolean localOnly)
	{
				// Create objects first.
	for (int i = 0; i < deviceList.length - 1; i++)
		deviceList[i] = new SaneDevice();
	return getDevicesNative(deviceList, localOnly);
	}
				// Open a device.
public native int open(String deviceName, int[] handle);
				// Close a device.
public native void close(int handle);
				// Get option descriptor.
public SaneOption getOptionDescriptor(int handle, int option)
	{
	SaneOption opt = new SaneOption();
	opt.name = null;
	getOptionNative(handle, option, opt);
	if (opt.name == null)	// Error?
		return (null);
	return (opt);
	}
				// Get each type of option:
public native int getControlOption(int handle, int option, int [] value,
							int [] info);
public native int getControlOption(int handle, int option, byte [] value,
							int [] info);
				// Set each type of option (SET_VALUE or
				//  SET_AUTO):
public native int setControlOption(int handle, int option,
				int action, int value, int [] info);
public native int setControlOption(int handle, int option,
				int action, String value, int [] info);
public native int getParameters(int handle, SaneParameters params);
public native int start(int handle);
public native int read(int handle, byte [] data, 
					int maxLength, int [] length);
public native void cancel(int handle);
public native String strstatus(int status);
}
