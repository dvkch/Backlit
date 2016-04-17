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

/**
 **	ScanIt.java - Do the actual scanning for SANE.
 **
 **	Written: 11/3/97 - JSF
 **/

import java.util.Vector;
import java.util.Enumeration;
import java.awt.image.ImageProducer;
import java.awt.image.ImageConsumer;
import java.awt.image.ColorModel;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.io.BufferedWriter;
import java.io.IOException;

/*
 *	This class uses SANE to scan an image.
 */
public class ScanIt implements ImageProducer
    {
					// # lines we incr. image height.
    private static final int STRIP_HEIGHT = 256;
    private Sane sane;
    private int handle = 0;		// SANE device handle.
    private Vector consumers = new Vector();
					// File to write to.
    private OutputStream outputFile = null;
    private SaneParameters parms = new SaneParameters();
    private ColorModel cm;		// RGB color model.
    private int width, height;		// Dimensions.
    private int x, y;			// Position.
    private int image[] = null;		// Image that we build as we scan.
    private int offset;			// Offset within image in pixels if
					//   doing separate frames, bytes
					//   (3/word) if RBG.

	/*
	 *	Tell consumers our status.  The scan is also terminated de-
	 *	pending on the status.
	 */
    private void tellStatus(int s)
	{
	Enumeration next = consumers.elements();
	while (next.hasMoreElements())
		{
		ImageConsumer ic = (ImageConsumer) next.nextElement();
		ic.imageComplete(s);
		}
					// Done?  Stop scan.
	if (s == ImageConsumer.STATICIMAGEDONE ||
	    s == ImageConsumer.IMAGEERROR)
		sane.cancel(handle);
	}

	/*
	 *	Tell consumers the image size.
	 */
    private void tellDimensions(int w, int h)
	{
	System.out.println("tellDimensions:  " + w + ", " + h);
	Enumeration next = consumers.elements();
	while (next.hasMoreElements())
		{
		ImageConsumer ic = (ImageConsumer) next.nextElement();
		ic.setDimensions(w, h);
		}
	}

	/*
	 *	Send pixels to the clients.
	 */
    private void tellPixels(int x, int y, int w, int h)
	{
/*
	System.out.println("image length=" + image.length);
	System.out.println("width=" + width);
	System.out.println("tellPixels:  x="+x +" y="+y + " w="+w
					+ " h="+h);
 */
	Enumeration next = consumers.elements();
	while (next.hasMoreElements())
		{
		ImageConsumer ic = (ImageConsumer) next.nextElement();
		ic.setPixels(x, y, w, h, cm, image, 0, width);
		}
	}

	/*
	 *	Construct.
	 */
    public ScanIt(Sane s, int hndl)
	{
	sane = s;
	handle = hndl;
	}

	/*
	 *	Add a consumer.
	 */
    public synchronized void addConsumer(ImageConsumer ic)
	{
	if (consumers.contains(ic))
		return;			// Already here.
	consumers.addElement(ic);
	}

	/*
	 *	Is a consumer in the list?
	 */
    public synchronized boolean isConsumer(ImageConsumer ic)
	{ return consumers.contains(ic); }

	/*
	 *	Remove consumer.
	 */
    public synchronized void removeConsumer(ImageConsumer ic)
	{ consumers.removeElement(ic); }
    	
	/*
	 *	Add a consumer and start scanning.
	 */
    public void startProduction(ImageConsumer ic)
	{
	System.out.println("In startProduction()");
	addConsumer(ic);
	scan();
	}

	/*
	 *	Set file to write to.
	 */
    public void setOutputFile(OutputStream o)
	{ outputFile = o; }

	/*
	 *	Ignore this:
	 */
    public void requestTopDownLeftRightResend(ImageConsumer ic)
	{  }

	/*
	 *	Go to next line in image, reallocating if necessary.
	 */
    private void nextLine()
	{
	x = 0;
	++y;
	if (y >= height || image == null)
		{			// Got to reallocate.
		int oldSize = image == null ? 0 : width*height;
		height += STRIP_HEIGHT;	// Add more lines.
		int newSize = width*height;
		int[] newImage = new int[newSize];
		int i;
		if (oldSize != 0)	// Copy old data.
			for (i = 0; i < oldSize; i++)
				newImage[i] = image[i];
					// Fill new pixels with 0's, setting
					//   alpha channel.
		for (i = oldSize; i < newSize; i++)
			newImage[i] = (255 << 24);
		image = newImage;
		System.out.println("nextLine:  newSize="+newSize);
					// Tell clients.
		tellDimensions(width, height);
		}
	}

	/*
	 *	Process a buffer of data.
	 */
    private boolean process(byte[] data, int readLen)
	{
	int prevY = y > 0 ? y : 0;	// Save current Y-coord.
	int i;
	switch (parms.format)
		{
	case SaneParameters.FRAME_RED:
	case SaneParameters.FRAME_GREEN:
	case SaneParameters.FRAME_BLUE:
		System.out.println("Process RED, GREEN or BLUE");
		int cindex = 2 - (parms.format - SaneParameters.FRAME_RED);
					// Single frame.
		for (i = 0; i < readLen; ++i)
			{		// Doing a single color frame.
			image[offset + i] |= 
				(((int) data[i]) & 0xff) << (8*cindex);
			++x;
			if (x >= width)
				nextLine();
			}
		break;
	case SaneParameters.FRAME_RGB:
		for (i = 0; i < readLen; ++i)
			{
			int b = 2 - (offset + i)%3;
			image[(offset + i)/3] |= 
				(((int) data[i]) & 0xff) << (8*b);
			if (b == 0)
				{
				++x;
				if (x >= width)
					nextLine();
				}
			}
		break;
	case SaneParameters.FRAME_GRAY:
		System.out.println("Process GREY");
					// Single frame.
		for (i = 0; i < readLen; ++i)
			{
			int v = ((int) data[i]) & 0xff;
			image[offset + i] |= (v<<16) | (v<<8) | (v);
			++x;
			if (x >= width)
				nextLine();
			}
		break;
		}
	offset += readLen;		// Update where we are.
					// Show it.
	System.out.println("PrevY = " + prevY + ", y = " + y);
//	tellPixels(0, prevY, width, y - prevY);	
	tellPixels(0, 0, width, height);
	return true;
	}

	/*
	 *	Start scanning.
	 */
    public void scan()
	{
	int dataLen = 32*1024;
	byte [] data = new byte[dataLen];
	int [] readLen = new int[1];
	int frameCnt = 0;
					// For now, use default RGB model.
	cm = ColorModel.getRGBdefault();
	int status;
	image = null;
	do				// Do each frame.
		{
		frameCnt++;
		x = 0;			// Init. position.
		y = -1;
		offset = 0;
		System.out.println("Reading frame #" + frameCnt);
		status = sane.start(handle);
		if (status != Sane.STATUS_GOOD)
			{
			System.out.println("start() failed.  Status= " 
								+ status);
			tellStatus(ImageConsumer.IMAGEERROR);
			return;
			}
		status = sane.getParameters(handle, parms);
		if (status != Sane.STATUS_GOOD)
			{
			System.out.println("getParameters() failed.  Status= " 
								+ status);
			tellStatus(ImageConsumer.IMAGEERROR);
			return;	//++++cleanup.
			}
		if (frameCnt == 1)	// First time?
			{
			width = parms.pixelsPerLine;
			if (parms.lines >= 0)
				height = parms.lines - STRIP_HEIGHT + 1;
			else		// Hand-scanner.
				height = 0;
			nextLine();	// Allocate image.
			}
		while ((status = sane.read(handle, data, dataLen, readLen))
							== Sane.STATUS_GOOD)
			{
			System.out.println("Read " + readLen[0] + " bytes.");
			if (!process(data, readLen[0]))
				{
				tellStatus(ImageConsumer.IMAGEERROR);
				return;
				}
			}
		if (status != Sane.STATUS_EOF)
			{
			System.out.println("read() failed.  Status= "
								+ status);
			tellStatus(ImageConsumer.IMAGEERROR);
			return;
			}
		}
	while (!parms.lastFrame);
	height = y;			// For now, send whole image here.
	tellDimensions(width, height);
	tellPixels(0, 0, width, height);
	if (outputFile != null)		// Write to file.
		{
		try
			{
			write(outputFile);
			}
		catch (IOException e)
			{	//+++++++++++++++
			System.out.println("I/O error writing file.");
			}	
		outputFile = null;	// Clear for next time.
		}
	tellStatus(ImageConsumer.STATICIMAGEDONE);
	image = null;			// Allow buffer to be freed.
	}

	/*
	 *	Write ppm/pnm output for last scan to a file.
	 */
    private void write(OutputStream out) throws IOException
	{
	PrintWriter pout = new PrintWriter(out);
	BufferedWriter bout = new BufferedWriter(pout);
	int len = width*height;		// Get # of pixels.
	int i;
	switch (parms.format)
		{
	case SaneParameters.FRAME_RED:
	case SaneParameters.FRAME_GREEN:
	case SaneParameters.FRAME_BLUE:
	case SaneParameters.FRAME_RGB:
		pout.print("P6\n# SANE data follows\n" + 
			width + ' ' + height + "\n255\n");
		for (i = 0; i < len; i++)
			{
			int pix = image[i];
			bout.write((pix >> 16) & 0xff);
			bout.write((pix >> 8) & 0xff);
			bout.write(pix & 0xff);
			}
		break;
	case SaneParameters.FRAME_GRAY:
		pout.print("P5\n# SANE data follows\n" +
			width + ' ' + height + "\n255\n");
		for (i = 0; i < len; i++)
			{
			int pix = image[i];
			bout.write(pix & 0xff);
			}
		break;
		}

	bout.flush();			// Flush output.
	pout.flush();
	}
    }
