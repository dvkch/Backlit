/* sane - Scanner Access Now Easy.
   Copyright (C) 2002 Henning Meier-Geinitz <henning@meier-geinitz.de>
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

   This file implements test picture functions for the test backend.
*/

#define BUFFER_SIZE (64 * 1024)

static SANE_Bool little_endian (void);

static SANE_Status
init_picture_buffer (Test_Device * test_device, SANE_Byte ** buffer,
		     size_t * buffer_size)
{
  SANE_Word pattern_size = 0, pattern_distance = 0;
  SANE_Word line_count, b_size;
  SANE_Word lines = 0;
  SANE_Word bpl = test_device->bytes_per_line;
  SANE_Word ppl = test_device->pixels_per_line;
  SANE_Byte *b;
  SANE_Bool is_little_endian = little_endian ();

  if (test_device->val[opt_invert_endianess].w)
    is_little_endian ^= 1;

  DBG (2, "(child) init_picture_buffer test_device=%p, buffer=%p, "
       "buffer_size=%p\n",(void*)test_device,(void*)buffer,(void*)buffer_size);

  if (strcmp (test_device->val[opt_test_picture].s, "Solid black") == 0
      || strcmp (test_device->val[opt_test_picture].s, "Solid white") == 0)
    {
      SANE_Byte pattern = 0;

      b_size = BUFFER_SIZE;
      if (buffer_size)
	*buffer_size = b_size;

      b = malloc (b_size);
      if (!b)
	{
	  DBG (1, "(child) init_picture_buffer: couldn't malloc buffer\n");
	  return SANE_STATUS_NO_MEM;
	}

      if (buffer)
	*buffer = b;

      if (strcmp (test_device->val[opt_test_picture].s, "Solid black") == 0)
	{
	  DBG (3, "(child) init_picture_buffer: drawing solid black test "
	       "picture %d bytes\n", b_size);
	  if (test_device->params.format == SANE_FRAME_GRAY
	      && test_device->params.depth == 1)
	    pattern = 0xff;
	  else
	    pattern = 0x00;
	}
      else
	{
	  DBG (3, "(child) init_picture_buffer: drawing solid white test "
	       "picture %d bytes\n", b_size);
	  if (test_device->params.format == SANE_FRAME_GRAY
	      && test_device->params.depth == 1)
	    pattern = 0x00;
	  else
	    pattern = 0xff;
	}
      memset (b, pattern, b_size);
      return SANE_STATUS_GOOD;
    }

  /* Grid */
  if (strcmp (test_device->val[opt_test_picture].s, "Grid") == 0)
    {
      double p_size = (10.0 * SANE_UNFIX (test_device->val[opt_resolution].w)
		       / MM_PER_INCH);
      SANE_Word increment = 1;
      if (test_device->params.format == SANE_FRAME_RGB)
	increment *= 3;
      if (test_device->params.depth == 16)
	increment *= 2;

      lines = 2 * p_size + 0.5;
      b_size = lines * bpl;
      if (buffer_size)
	*buffer_size = b_size;
      b = malloc (b_size);
      if (!b)
	{
	  DBG (1, "(child) init_picture_buffer: couldn't malloc buffer\n");
	  return SANE_STATUS_NO_MEM;
	}
      if (buffer)
	*buffer = b;
      DBG (3, "(child) init_picture_buffer: drawing grid test picture "
	   "%d bytes, %d bpl, %d ppl, %d lines\n", b_size, bpl, ppl, lines);

      for (line_count = 0; line_count < lines; line_count++)
	{
	  SANE_Word x = 0;

	  for (x = 0; x < bpl; x += increment)
	    {
	      SANE_Word x1;
	      SANE_Byte color = 0;

	      if (test_device->params.depth == 1)
		{
		  if (test_device->params.format == SANE_FRAME_GRAY ||
		      (test_device->params.format >= SANE_FRAME_RED &&
		       test_device->params.format <= SANE_FRAME_BLUE))
		    {
		      SANE_Byte value = 0;
		      for (x1 = 0; x1 < 8; x1++)
			{
			  SANE_Word xfull = x * 8 + (7 - x1);

			  if (xfull < ppl)
			    {
			      if ((((SANE_Word) (xfull / p_size)) % 2)
				  ^ !(line_count >
				      (SANE_Word) (p_size + 0.5)))
				color = 0x0;
			      else
				color = 0x1;
			    }
			  else
			    color = (rand ()) & 0x01;
			  value |= (color << x1);
			}
		      b[line_count * bpl + x] = value;
		    }
		  else		/* SANE_FRAME_RGB */
		    {
		      SANE_Byte value = 0;
		      for (x1 = 0; x1 < 8; x1++)
			{
			  SANE_Word xfull = x * 8 / 3 + (7 - x1);

			  if (xfull < ppl)
			    {
			      if (((SANE_Word) (xfull / p_size) % 2)
				  ^ (line_count > (SANE_Word) (p_size + 0.5)))
				color = 0x0;
			      else
				color = 0x1;
			    }
			  else
			    color = (rand ()) & 0x01;
			  value |= (color << x1);
			}
		      for (x1 = 0; x1 < increment; x1++)
			b[line_count * bpl + x + x1] = value;
		    }
		}
	      else		/* depth = 8, 16 */
		{
		  if (x / increment < ppl)
		    if ((((SANE_Int) (x / increment / p_size)) % 2)
			^ (line_count > (SANE_Int) (p_size + 0.5)))
		      color = 0x00;
		    else
		      color = 0xff;
		  else
		    color = (rand ()) & 0xff;

		  for (x1 = 0; x1 < increment; x1++)
		    b[line_count * bpl + x + x1] = color;
		}
	    }
	}
      return SANE_STATUS_GOOD;
    }

  /* Color patterns */
  if (test_device->params.format == SANE_FRAME_GRAY
      && test_device->params.depth == 1)
    {
      /* 1 bit black/white */
      pattern_size = 16;
      pattern_distance = 0;
      lines = 2 * (pattern_size + pattern_distance);
      b_size = lines * bpl;

      if (buffer_size)
	*buffer_size = b_size;
      b = malloc (b_size);
      if (!b)
	{
	  DBG (1, "(child) init_picture_buffer: couldn't malloc buffer\n");
	  return SANE_STATUS_NO_MEM;
	}
      if (buffer)
	*buffer = b;
      DBG (3, "(child) init_picture_buffer: drawing b/w test picture "
	   "%d bytes, %d bpl, %d lines\n", b_size, bpl, lines);
      memset (b, 255, b_size);
      for (line_count = 0; line_count < lines; line_count++)
	{
	  SANE_Word x = 0;

	  if (line_count >= lines / 2)
	    x += (pattern_size + pattern_distance) / 8;

	  while (x < bpl)
	    {
	      SANE_Word width;

	      width = pattern_size / 8;
	      if (x + width >= bpl)
		width = bpl - x;
	      memset (b + line_count * bpl + x, 0x00, width);
	      x += (pattern_size + pattern_distance) * 2 / 8;
	    }
	}
    }
  else if (test_device->params.format == SANE_FRAME_GRAY
	   && test_device->params.depth == 8)
    {
      /* 8 bit gray */
      pattern_size = 4;
      pattern_distance = 1;
      lines = 2 * (pattern_size + pattern_distance);
      b_size = lines * bpl;

      if (buffer_size)
	*buffer_size = b_size;
      b = malloc (b_size);
      if (!b)
	{
	  DBG (1, "(child) init_picture_buffer: couldn't malloc buffer\n");
	  return SANE_STATUS_NO_MEM;
	}
      if (buffer)
	*buffer = b;
      DBG (3, "(child) init_picture_buffer: drawing 8 bit gray test picture "
	   "%d bytes, %d bpl, %d lines\n", b_size, bpl, lines);
      memset (b, 0x55, b_size);
      for (line_count = 0; line_count < lines; line_count++)
	{
	  SANE_Word x = pattern_distance;

	  if (line_count % (pattern_size + pattern_distance)
	      < pattern_distance)
	    continue;

	  while (x < bpl)
	    {
	      SANE_Word width;
	      SANE_Byte color;

	      width = pattern_size;
	      if (x + width >= bpl)
		width = bpl - x;
	      if (line_count > (pattern_size + pattern_distance))
		color =
		  0xff - ((x / (pattern_size + pattern_distance)) & 0xff);
	      else
		color = (x / (pattern_size + pattern_distance)) & 0xff;
	      memset (b + line_count * bpl + x, color, width);
	      x += (pattern_size + pattern_distance);
	    }
	}

    }
  else if (test_device->params.format == SANE_FRAME_GRAY
	   && test_device->params.depth == 16)
    {
      /* 16 bit gray */
      pattern_size = 256;
      pattern_distance = 4;
      lines = 1 * (pattern_size + pattern_distance);
      b_size = lines * bpl;

      if (buffer_size)
	*buffer_size = b_size;
      b = malloc (b_size);
      if (!b)
	{
	  DBG (1, "(child) init_picture_buffer: couldn't malloc buffer\n");
	  return SANE_STATUS_NO_MEM;
	}
      if (buffer)
	*buffer = b;
      DBG (3, "(child) init_picture_buffer: drawing 16 bit gray test picture "
	   "%d bytes, %d bpl, %d lines\n", b_size, bpl, lines);
      memset (b, 0x55, b_size);
      for (line_count = 0; line_count < lines; line_count++)
	{
	  SANE_Word x = pattern_distance * 2;

	  if (line_count % (pattern_size + pattern_distance)
	      < pattern_distance)
	    continue;

	  while (x < bpl)
	    {
	      SANE_Word width;
	      SANE_Word x1;
	      SANE_Byte pattern_lo, pattern_hi;

	      width = pattern_size * 2;
	      if (x + width >= bpl)
		width = bpl - x;
	      pattern_lo =
		((line_count - pattern_distance)
		 % (pattern_size + pattern_distance)) & 0xff;
	      for (x1 = 0; x1 < width; x1 += 2)
		{
		  pattern_hi = (x1 / 2) & 0xff;
		  if (is_little_endian)
		    {
		      b[line_count * bpl + x + x1 + 0] = pattern_lo;
		      b[line_count * bpl + x + x1 + 1] = pattern_hi;
		    }
		  else
		    {
		      b[line_count * bpl + x + x1 + 0] = pattern_hi;
		      b[line_count * bpl + x + x1 + 1] = pattern_lo;
		    }
		}
	      x += ((pattern_size + pattern_distance) * 2);
	    }
	}

    }
  else if (test_device->params.format == SANE_FRAME_RGB
	   && test_device->params.depth == 1)
    {
      /* 1 bit color */
      pattern_size = 16;
      pattern_distance = 0;
      lines = 2 * (pattern_size + pattern_distance);
      b_size = lines * bpl;

      if (buffer_size)
	*buffer_size = b_size;
      b = malloc (b_size);
      if (!b)
	{
	  DBG (1, "(child) init_picture_buffer: couldn't malloc buffer\n");
	  return SANE_STATUS_NO_MEM;
	}
      if (buffer)
	*buffer = b;
      DBG (3, "(child) init_picture_buffer: drawing color lineart test "
	   "picture %d bytes, %d bpl, %d lines\n", b_size, bpl, lines);
      memset (b, 0x55, b_size);

      for (line_count = 0; line_count < lines; line_count++)
	{
	  SANE_Word x = 0;
	  SANE_Byte color = 0, color_r = 0, color_g = 0, color_b = 0;

	  if (line_count >= lines / 2)
	    color = 7;
	  while (x < bpl)
	    {
	      SANE_Word width;
	      SANE_Word x2 = 0;

	      width = pattern_size / 8 * 3;
	      if (x + width >= bpl)
		width = bpl - x;

	      color_b = (color & 1) * 0xff;
	      color_g = ((color >> 1) & 1) * 0xff;
	      color_r = ((color >> 2) & 1) * 0xff;

	      for (x2 = 0; x2 < width; x2 += 3)
		{
		  b[line_count * bpl + x + x2 + 0] = color_r;
		  b[line_count * bpl + x + x2 + 1] = color_g;
		  b[line_count * bpl + x + x2 + 2] = color_b;
		}
	      if (line_count < lines / 2)
		{
		  ++color;
		  if (color >= 8)
		    color = 0;
		}
	      else
		{
		  if (color == 0)
		    color = 8;
		  --color;
		}
	      x += ((pattern_size + pattern_distance) / 8 * 3);
	    }
	}
    }
  else if ((test_device->params.format == SANE_FRAME_RED
	    || test_device->params.format == SANE_FRAME_GREEN
	    || test_device->params.format == SANE_FRAME_BLUE)
	   && test_device->params.depth == 1)
    {
      /* 1 bit color three-pass */
      pattern_size = 16;
      pattern_distance = 0;
      lines = 2 * (pattern_size + pattern_distance);
      b_size = lines * bpl;

      if (buffer_size)
	*buffer_size = b_size;
      b = malloc (b_size);
      if (!b)
	{
	  DBG (1, "(child) init_picture_buffer: couldn't malloc buffer\n");
	  return SANE_STATUS_NO_MEM;
	}
      if (buffer)
	*buffer = b;
      DBG (3, "(child) init_picture_buffer: drawing color lineart three-pass "
	   "test picture %d bytes, %d bpl, %d lines\n", b_size, bpl, lines);
      memset (b, 0x55, b_size);

      for (line_count = 0; line_count < lines; line_count++)
	{
	  SANE_Word x = 0;
	  SANE_Byte color = 0, color_r = 0, color_g = 0, color_b = 0;

	  if (line_count >= lines / 2)
	    color = 7;
	  while (x < bpl)
	    {
	      SANE_Word width;
	      SANE_Word x2 = 0;

	      width = pattern_size / 8;
	      if (x + width >= bpl)
		width = bpl - x;

	      color_b = (color & 1) * 0xff;
	      color_g = ((color >> 1) & 1) * 0xff;
	      color_r = ((color >> 2) & 1) * 0xff;

	      for (x2 = 0; x2 < width; x2++)
		{
		  if (test_device->params.format == SANE_FRAME_RED)
		    b[line_count * bpl + x + x2] = color_r;
		  else if (test_device->params.format == SANE_FRAME_GREEN)
		    b[line_count * bpl + x + x2] = color_g;
		  else
		    b[line_count * bpl + x + x2] = color_b;
		}
	      if (line_count < lines / 2)
		{
		  ++color;
		  if (color >= 8)
		    color = 0;
		}
	      else
		{
		  if (color == 0)
		    color = 8;
		  --color;
		}
	      x += (pattern_size + pattern_distance) / 8;
	    }
	}
    }
  else if (test_device->params.format == SANE_FRAME_RGB
	   && test_device->params.depth == 8)
    {
      /* 8 bit color */
      pattern_size = 4;
      pattern_distance = 1;
      lines = 6 * (pattern_size + pattern_distance);
      b_size = lines * bpl;

      if (buffer_size)
	*buffer_size = b_size;
      b = malloc (b_size);
      if (!b)
	{
	  DBG (1, "(child) init_picture_buffer: couldn't malloc buffer\n");
	  return SANE_STATUS_NO_MEM;
	}
      if (buffer)
	*buffer = b;
      DBG (3, "(child) init_picture_buffer: drawing 8 bit color test picture "
	   "%d bytes, %d bpl, %d lines\n", b_size, bpl, lines);
      memset (b, 0x55, b_size);
      for (line_count = 0; line_count < lines; line_count++)
	{
	  SANE_Word x = pattern_distance * 3;

	  if (line_count % (pattern_size + pattern_distance)
	      < pattern_distance)
	    continue;

	  while (x < bpl)
	    {
	      SANE_Word width;
	      SANE_Byte color = 0, color_r = 0, color_g = 0, color_b = 0;
	      SANE_Word x1;

	      width = pattern_size * 3;
	      if (x + width >= bpl)
		width = bpl - x;

	      if ((line_count / (pattern_size + pattern_distance)) & 1)
		color =
		  0xff - ((x / ((pattern_size + pattern_distance) * 3))
			  & 0xff);
	      else
		color = (x / ((pattern_size + pattern_distance) * 3)) & 0xff;

	      if (line_count / (pattern_size + pattern_distance) < 2)
		{
		  color_r = color;
		  color_g = 0;
		  color_b = 0;
		}
	      else if (line_count / (pattern_size + pattern_distance) < 4)
		{
		  color_r = 0;
		  color_g = color;
		  color_b = 0;
		}
	      else
		{
		  color_r = 0;
		  color_g = 0;
		  color_b = color;
		}

	      for (x1 = 0; x1 < width; x1 += 3)
		{
		  b[line_count * bpl + x + x1 + 0] = color_r;
		  b[line_count * bpl + x + x1 + 1] = color_g;
		  b[line_count * bpl + x + x1 + 2] = color_b;
		}

	      x += ((pattern_size + pattern_distance) * 3);
	    }
	}

    }
  else if ((test_device->params.format == SANE_FRAME_RED
	    || test_device->params.format == SANE_FRAME_GREEN
	    || test_device->params.format == SANE_FRAME_BLUE)
	   && test_device->params.depth == 8)
    {
      /* 8 bit color three-pass */
      pattern_size = 4;
      pattern_distance = 1;
      lines = 6 * (pattern_size + pattern_distance);
      b_size = lines * bpl;

      if (buffer_size)
	*buffer_size = b_size;
      b = malloc (b_size);
      if (!b)
	{
	  DBG (1, "(child) init_picture_buffer: couldn't malloc buffer\n");
	  return SANE_STATUS_NO_MEM;
	}
      if (buffer)
	*buffer = b;
      DBG (3, "(child) init_picture_buffer: drawing 8 bit color three-pass "
	   "test picture %d bytes, %d bpl, %d lines\n", b_size, bpl, lines);
      memset (b, 0x55, b_size);
      for (line_count = 0; line_count < lines; line_count++)
	{
	  SANE_Word x = pattern_distance;

	  if (line_count % (pattern_size + pattern_distance)
	      < pattern_distance)
	    continue;

	  while (x < bpl)
	    {
	      SANE_Word width;
	      SANE_Byte color = 0;

	      width = pattern_size;
	      if (x + width >= bpl)
		width = bpl - x;

	      if ((line_count / (pattern_size + pattern_distance)) & 1)
		color =
		  0xff - (x / ((pattern_size + pattern_distance)) & 0xff);
	      else
		color = (x / (pattern_size + pattern_distance)) & 0xff;

	      if (line_count / (pattern_size + pattern_distance) < 2)
		{
		  if (test_device->params.format != SANE_FRAME_RED)
		    color = 0x00;
		}
	      else if (line_count / (pattern_size + pattern_distance) < 4)
		{
		  if (test_device->params.format != SANE_FRAME_GREEN)
		    color = 0x00;
		}
	      else
		{
		  if (test_device->params.format != SANE_FRAME_BLUE)
		    color = 0x00;
		}
	      memset (b + line_count * bpl + x, color, width);

	      x += (pattern_size + pattern_distance);
	    }
	}
    }
  else if (test_device->params.format == SANE_FRAME_RGB
	   && test_device->params.depth == 16)
    {
      /* 16 bit color */
      pattern_size = 256;
      pattern_distance = 4;
      lines = pattern_size + pattern_distance;
      b_size = lines * bpl;

      if (buffer_size)
	*buffer_size = b_size;
      b = malloc (b_size);
      if (!b)
	{
	  DBG (1, "(child) init_picture_buffer: couldn't malloc buffer\n");
	  return SANE_STATUS_NO_MEM;
	}
      if (buffer)
	*buffer = b;
      DBG (3,
	   "(child) init_picture_buffer: drawing 16 bit color test picture "
	   "%d bytes, %d bpl, %d lines\n", b_size, bpl, lines);
      memset (b, 0x55, b_size);
      for (line_count = 0; line_count < lines; line_count++)
	{
	  SANE_Word x = pattern_distance * 2 * 3;

	  if (line_count % (pattern_size + pattern_distance)
	      < pattern_distance)
	    continue;

	  while (x < bpl)
	    {
	      SANE_Word width;
	      SANE_Word x1;
	      SANE_Byte color_hi = 0, color_lo = 0;
	      SANE_Byte color_hi_r = 0, color_lo_r = 0;
	      SANE_Byte color_hi_g = 0, color_lo_g = 0;
	      SANE_Byte color_hi_b = 0, color_lo_b = 0;

	      width = pattern_size * 2 * 3;
	      if (x + width >= bpl)
		width = bpl - x;


	      for (x1 = 0; x1 < width; x1 += 6)
		{
		  color_lo =
		    ((line_count + pattern_size)
		     % (pattern_size + pattern_distance)) & 0xff;
		  color_hi = (x1 / 6) & 0xff;
		  if (((x / ((pattern_size + pattern_distance) * 6)) % 3) ==
		      0)
		    {
		      color_lo_r = color_lo;
		      color_hi_r = color_hi;
		      color_lo_g = 0;
		      color_hi_g = 0;
		      color_lo_b = 0;
		      color_hi_b = 0;
		    }
		  else if (((x / ((pattern_size + pattern_distance) * 6)) % 3)
			   == 1)
		    {
		      color_lo_r = 0;
		      color_hi_r = 0;
		      color_lo_g = color_lo;
		      color_hi_g = color_hi;
		      color_lo_b = 0;
		      color_hi_b = 0;
		    }
		  else
		    {
		      color_lo_r = 0;
		      color_hi_r = 0;
		      color_lo_g = 0;
		      color_hi_g = 0;
		      color_lo_b = color_lo;
		      color_hi_b = color_hi;
		    }
		  if (is_little_endian)
		    {
		      b[line_count * bpl + x + x1 + 0] = color_lo_r;
		      b[line_count * bpl + x + x1 + 1] = color_hi_r;
		      b[line_count * bpl + x + x1 + 2] = color_lo_g;
		      b[line_count * bpl + x + x1 + 3] = color_hi_g;
		      b[line_count * bpl + x + x1 + 4] = color_lo_b;
		      b[line_count * bpl + x + x1 + 5] = color_hi_b;
		    }
		  else
		    {
		      b[line_count * bpl + x + x1 + 0] = color_hi_r;
		      b[line_count * bpl + x + x1 + 1] = color_lo_r;
		      b[line_count * bpl + x + x1 + 2] = color_hi_g;
		      b[line_count * bpl + x + x1 + 3] = color_lo_g;
		      b[line_count * bpl + x + x1 + 4] = color_hi_b;
		      b[line_count * bpl + x + x1 + 5] = color_lo_b;
		    }
		}
	      x += ((pattern_size + pattern_distance) * 2 * 3);
	    }
	}

    }
  else if ((test_device->params.format == SANE_FRAME_RED
	    || test_device->params.format == SANE_FRAME_GREEN
	    || test_device->params.format == SANE_FRAME_BLUE)
	   && test_device->params.depth == 16)
    {
      /* 16 bit color three-pass */
      pattern_size = 256;
      pattern_distance = 4;
      lines = pattern_size + pattern_distance;
      b_size = lines * bpl;

      if (buffer_size)
	*buffer_size = b_size;
      b = malloc (b_size);
      if (!b)
	{
	  DBG (1, "(child) init_picture_buffer: couldn't malloc buffer\n");
	  return SANE_STATUS_NO_MEM;
	}
      if (buffer)
	*buffer = b;
      DBG (3, "(child) init_picture_buffer: drawing 16 bit color three-pass "
	   "test picture %d bytes, %d bpl, %d lines\n", b_size, bpl, lines);
      memset (b, 0x55, b_size);
      for (line_count = 0; line_count < lines; line_count++)
	{
	  SANE_Word x = pattern_distance * 2;

	  if (line_count % (pattern_size + pattern_distance)
	      < pattern_distance)
	    continue;

	  while (x < bpl)
	    {
	      SANE_Word width;
	      SANE_Word x1;
	      SANE_Byte color_hi = 0, color_lo = 0;

	      width = pattern_size * 2;
	      if (x + width >= bpl)
		width = bpl - x;


	      for (x1 = 0; x1 < width; x1 += 2)
		{
		  color_lo =
		    ((line_count + pattern_size)
		     % (pattern_size + pattern_distance)) & 0xff;
		  color_hi = (x1 / 2) & 0xff;
		  if (((x / ((pattern_size + pattern_distance) * 2)) % 3) ==
		      0)
		    {
		      if (test_device->params.format != SANE_FRAME_RED)
			{
			  color_lo = 0x00;
			  color_hi = 0x00;
			}
		    }
		  else if (((x / ((pattern_size + pattern_distance) * 2)) % 3)
			   == 1)
		    {
		      if (test_device->params.format != SANE_FRAME_GREEN)
			{
			  color_lo = 0x00;
			  color_hi = 0x00;
			}
		    }
		  else
		    {
		      if (test_device->params.format != SANE_FRAME_BLUE)
			{
			  color_lo = 0x00;
			  color_hi = 0x00;
			}
		    }

		  if (is_little_endian)
		    {
		      b[line_count * bpl + x + x1 + 0] = color_lo;
		      b[line_count * bpl + x + x1 + 1] = color_hi;
		    }
		  else
		    {
		      b[line_count * bpl + x + x1 + 0] = color_hi;
		      b[line_count * bpl + x + x1 + 1] = color_lo;
		    }

		}
	      x += ((pattern_size + pattern_distance) * 2);
	    }
	}

    }
  else				/* Huh? */
    {
      DBG (1, "(child) init_picture_buffer: unknown mode\n");
      return SANE_STATUS_INVAL;
    }

  return SANE_STATUS_GOOD;
}
