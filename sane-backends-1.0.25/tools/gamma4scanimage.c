/* --------------------------------------------------------------------------------------------------------- */
/* sane - Scanner Access Now Easy.

   gamma4scanimage

   (C) 2002 Oliver Rauch

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

*/
/* --------------------------------------------------------------------------------------------------------- */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
 double gamma  = 1.0;
 int maxin     = 16383; /* 14 bit gamma input */
 int maxout    = 255; /* 8 bit gamma output */
 int shadow    = 0;
 int highlight = maxin;
 int in, out;
 float f;

  if ( (argc==1) || (argc>6) )
  {
    printf("Usage: %s gamma [shadow [highlight [maxin [maxout]]]]\n", argv[0]);
    exit(-1);
  }

  if (argc>1)
  {
    gamma     = atof(argv[1]);
  }

  if (argc>2)
  {
    shadow    = atoi(argv[2]);
  }

  if (argc>3)
  {
    highlight = atoi(argv[3]);
  }

  if (argc>4)
  {
    maxin     = atoi(argv[4]);
  }

  if (argc>5)
  {
    maxout    = atoi(argv[5]);
  }

  if (shadow < 0)
  {
    printf("%s error: shadow=%d < 0\n", argv[0], shadow);
    exit(-1);
  }

  if (highlight < 0)
  {
    printf("%s error: highlight=%d < 0\n", argv[0], highlight);
    exit(-1);
  }

  if (maxin < 0)
  {
    printf("%s error: maxin=%d < 0\n", argv[0], maxin);
    exit(-1);
  }

  if (maxout < 0)
  {
    printf("%s error: maxout=%d < 0\n", argv[0], maxout);
    exit(-1);
  }

  if (shadow >= highlight)
  {
    printf("%s error: shadow=%d >= highlight=%d\n", argv[0], shadow, highlight);
    exit(-1);
  }

  if (highlight > maxin)
  {
    printf("%s error: highlight=%d > maxin=%d\n", argv[0], highlight, maxin);
    exit(-1);
  }

  if ((gamma < 0.1) || (gamma > 5))
  {
    printf("%s error: gamma=%f out of range [0.1;5]\n", argv[0], gamma);
    exit(-1);
  }

  f = (highlight - shadow) / 255.0 + shadow;

  printf("[%d]%d-", 0, 0);

  if (shadow > 0)
  {
    printf("[%d]%d-", shadow, 0);
  }

  while (f < highlight)
  {
    in = (int) f;
    out = maxout * pow((double) (in - shadow)/(highlight-shadow), (1.0/gamma));
    printf("[%d]%d-", in, out);
    f *= 1.5;
  }

  if (f > highlight)
  {
    printf("[%d]%d-", highlight, maxout);
  }

  printf("[%d]%d", maxin, maxout);

 return 0;
}
