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
 **	ImageCanvas.java - A canvas that displays an image.
 **
 **	Written: 11/4/97 - JSF
 **/

import java.awt.Canvas;
import java.awt.Dimension;
import java.awt.Graphics;
import java.awt.Image;
import java.awt.image.ImageProducer;

/*
 *	Here's a canvas that just shows an image.
 */

public class ImageCanvas extends Canvas
    {
    Image image = null;			// The image to display.
    ImageCanvasClient client = null;	// 1 client for now.

	/*
	 *	Create with empty image.
	 */
    ImageCanvas()
	{
	}

	/*
	 *	Create image from an image producer.
	 */
    void setImage(ImageProducer iprod, ImageCanvasClient c)
	{
	client = c;
	image = createImage(iprod);
	repaint();
	}

	/*
	 *	Paint.
	 */
    public void paint(Graphics g)
	{
	System.out.println("In ImageCanvas.paint()");
	Dimension dim = getSize();
	g.drawRect(0, 0, dim.width - 1, dim.height - 1);
	if (image != null)
		{
		g.drawImage(image, 1, 1, dim.width - 2, dim.height - 2, this);
		}
	}

	/*
	 *	Image has been updated.
	 */
    public boolean imageUpdate(Image theimg, int infoflags,
				int x, int y, int w, int h)
	{
	System.out.println("In imageUpdate()");
	boolean done = ((infoflags & (ERROR | FRAMEBITS | ALLBITS)) != 0);
//	repaint();
	paint(getGraphics());
	if (done && client != null)	// Tell client when done.
		{
		client.imageDone((infoflags & ERROR) != 0);
		client = null;
		}
	return (!done);
	}
    }
