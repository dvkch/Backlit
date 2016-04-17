/* Create SANE/tiff headers TIFF interfacing routines for SANE
   Copyright (C) 2000 Peter Kirchgessner
   Copyright (C) 2002 Oliver Rauch: added tiff ICC profile

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* Changes:
   2000-11-19, PK: Color TIFF-header: write 3 values for bits per sample
   2001-12-16, PK: Write fill order tag for b/w-images
   2002-08-27, OR: Added tiff tag for ICC profile
*/
#ifdef _AIX
# include "../include/lalloca.h"	/* MUST come first for AIX! */
#endif

#include <stdlib.h>
#include <stdio.h>

#include "../include/sane/config.h"
#include "../include/sane/sane.h"

#include "stiff.h"

typedef struct {
    int tag, typ, nvals, val;
} IFD_ENTRY;


typedef struct {
    int maxtags;
    int ntags;
    IFD_ENTRY *ifde;
} IFD;

#define IFDE_TYP_BYTE     (1)
#define IFDE_TYP_ASCII    (2)
#define IFDE_TYP_SHORT    (3)
#define IFDE_TYP_LONG     (4)
#define IFDE_TYP_RATIONAL (5)

static IFD *
create_ifd (void)

{   IFD *ifd;
    int maxtags = 10;

    ifd = (IFD *)malloc (sizeof (IFD));
    if (ifd == NULL) return NULL;

    ifd->ifde = (IFD_ENTRY *)malloc (maxtags * sizeof (IFD_ENTRY));
    if (ifd->ifde == NULL)
    {
        free (ifd);
        return NULL;
    }
    ifd->ntags = 0;
    ifd->maxtags = maxtags;

    return ifd;
}

static void
free_ifd (IFD *ifd)

{
    if (ifd == NULL) return;
    if (ifd->ifde != NULL)
    {
        free (ifd->ifde);
        ifd->ifde = NULL;
    }
    free (ifd);
    ifd = NULL;
}

static void
add_ifd_entry (IFD *ifd, int tag, int typ, int nvals, int val)

{   IFD_ENTRY *ifde;
    int add_entries = 10;

    if (ifd == NULL) return;
    if (ifd->ntags == ifd->maxtags)
    {
        ifde = (IFD_ENTRY *)realloc (ifd->ifde,
                                     (ifd->maxtags+add_entries)*sizeof (IFD_ENTRY));
        if (ifde == NULL) return;
        ifd->ifde = ifde;
        ifd->maxtags += add_entries;
    }
    ifde = &(ifd->ifde[ifd->ntags]);
    ifde->tag = tag;
    ifde->typ = typ;
    ifde->nvals = nvals;
    ifde->val = val;
    (ifd->ntags)++;
}

static void
write_i2 (FILE *fptr, int val, int motorola)
{
    if (motorola)
    {
        putc ((val >> 8) & 0xff, fptr);
        putc (val & 0xff, fptr);
    }
    else
    {
        putc (val & 0xff, fptr);
        putc ((val >> 8) & 0xff, fptr);
    }
}


static void
write_i4 (FILE *fptr, int val, int motorola)
{
    if (motorola)
    {
        putc ((val >> 24) & 0xff, fptr);
        putc ((val >> 16) & 0xff, fptr);
        putc ((val >> 8) & 0xff, fptr);
        putc (val & 0xff, fptr);
    }
    else
    {
        putc (val & 0xff, fptr);
        putc ((val >> 8) & 0xff, fptr);
        putc ((val >> 16) & 0xff, fptr);
        putc ((val >> 24) & 0xff, fptr);
    }
}

static void
write_ifd (FILE *fptr, IFD *ifd, int motorola)
{int k;
    IFD_ENTRY *ifde;

    if (!ifd) return;

    if (motorola) putc ('M', fptr), putc ('M', fptr);
    else putc ('I', fptr), putc ('I', fptr);

    write_i2 (fptr, 42, motorola);  /* Magic */
    write_i4 (fptr, 8, motorola);   /* Offset to first IFD */
    write_i2 (fptr, ifd->ntags, motorola);

    for (k = 0; k < ifd->ntags; k++)
    {
        ifde = &(ifd->ifde[k]);
        write_i2 (fptr, ifde->tag, motorola);
        write_i2 (fptr, ifde->typ, motorola);
        write_i4 (fptr, ifde->nvals, motorola);
        if ((ifde->typ == IFDE_TYP_SHORT) && (ifde->nvals == 1))
        {
            write_i2 (fptr, ifde->val, motorola);
            write_i2 (fptr, 0, motorola);
        }
        else
        {
            write_i4 (fptr, ifde->val, motorola);
        }
    }
    write_i4 (fptr, 0, motorola); /* End of IFD chain */
}


static void
write_tiff_bw_header (FILE *fptr, int width, int height, int resolution)
{IFD *ifd;
    int header_size = 8, ifd_size;
    int strip_offset, data_offset, data_size;
    int strip_bytecount;
    int ntags;
    int motorola;

    ifd = create_ifd ();

    strip_bytecount = ((width+7)/8) * height;

    /* the following values must be known in advance */
    ntags = 12;
    data_size = 0;
    if (resolution > 0)
    {
        ntags += 3;
        data_size += 2*4 + 2*4;
    }

    ifd_size = 2 + ntags*12 + 4;
    data_offset = header_size + ifd_size;
    strip_offset = data_offset + data_size;

    /* New subfile type */
    add_ifd_entry (ifd, 254, IFDE_TYP_LONG, 1, 0);
    /* image width */
    add_ifd_entry (ifd, 256, (width > 0xffff) ? IFDE_TYP_LONG : IFDE_TYP_SHORT,
                   1, width);
    /* image length */
    add_ifd_entry (ifd, 257, (height > 0xffff) ? IFDE_TYP_LONG : IFDE_TYP_SHORT,
                   1, height);
    /* bits per sample */
    add_ifd_entry (ifd, 258, IFDE_TYP_SHORT, 1, 1);
    /* compression (uncompressed) */
    add_ifd_entry (ifd, 259, IFDE_TYP_SHORT, 1, 1);
    /* photometric interpretation */
    add_ifd_entry (ifd, 262, IFDE_TYP_SHORT, 1, 0);
    /* fill order */
    add_ifd_entry (ifd, 266, IFDE_TYP_SHORT, 1, 1);
    /* strip offset */
    add_ifd_entry (ifd, 273, IFDE_TYP_LONG, 1, strip_offset);
    /* orientation */
    add_ifd_entry (ifd, 274, IFDE_TYP_SHORT, 1, 1);
    /* samples per pixel */
    add_ifd_entry (ifd, 277, IFDE_TYP_SHORT, 1, 1);
    /* rows per strip */
    add_ifd_entry (ifd, 278, IFDE_TYP_LONG, 1, height);
    /* strip bytecount */
    add_ifd_entry (ifd, 279, IFDE_TYP_LONG, 1, strip_bytecount);
    if (resolution > 0)
    {
        /* x resolution */
        add_ifd_entry (ifd, 282, IFDE_TYP_RATIONAL, 1, data_offset);
        data_offset += 2*4;
        /* y resolution */
        add_ifd_entry (ifd, 283, IFDE_TYP_RATIONAL, 1, data_offset);
        data_offset += 2*4;
    }
    if (resolution > 0)
    {
        /* resolution unit (dpi) */
        add_ifd_entry (ifd, 296, IFDE_TYP_SHORT, 1, 2);
    }

    /* I prefer motorola format. Its human readable. */
    motorola = 1;
    write_ifd (fptr, ifd, motorola);

    /* Write x/y resolution */
    if (resolution > 0)
    {
        write_i4 (fptr, resolution, motorola);
        write_i4 (fptr, 1, motorola);
        write_i4 (fptr, resolution, motorola);
        write_i4 (fptr, 1, motorola);
    }

    free_ifd (ifd);
}

static void
write_tiff_grey_header (FILE *fptr, int width, int height, int depth,
                        int resolution, const char *icc_profile)
{IFD *ifd;
    int header_size = 8, ifd_size;
    int strip_offset, data_offset, data_size;
    int strip_bytecount;
    int ntags;
    int motorola, bps, maxsamplevalue;
    FILE *icc_file = 0;
    int icc_len = -1;

    if (icc_profile)
    {
      icc_file = fopen(icc_profile, "r");

      if (!icc_file)
      {
        fprintf(stderr, "Could not open ICC profile %s\n", icc_profile);
      }
      else
      {
        icc_len = 16777216 * fgetc(icc_file) + 65536 * fgetc(icc_file) + 256 * fgetc(icc_file) + fgetc(icc_file);
        rewind(icc_file);
      }
    }

    ifd = create_ifd ();

    bps = (depth <= 8) ? 1 : 2;  /* Bytes per sample */
    maxsamplevalue = (depth <= 8) ? 255 : 65535;
    strip_bytecount = width * height * bps;

    /* the following values must be known in advance */
    ntags = 13;
    data_size = 0;
    if (resolution > 0)
    {
        ntags += 3;
        data_size += 2*4 + 2*4;
    }

    if (icc_len > 0) /* if icc profile exists add memory for tag */
    {
        ntags += 1;
        data_size += icc_len;
    }

    ifd_size = 2 + ntags*12 + 4;
    data_offset = header_size + ifd_size;
    strip_offset = data_offset + data_size;

    /* New subfile type */
    add_ifd_entry (ifd, 254, IFDE_TYP_LONG, 1, 0);
    /* image width */
    add_ifd_entry (ifd, 256, (width > 0xffff) ? IFDE_TYP_LONG : IFDE_TYP_SHORT,
                   1, width);
    /* image length */
    add_ifd_entry (ifd, 257, (height > 0xffff) ? IFDE_TYP_LONG : IFDE_TYP_SHORT,
                   1, height);
    /* bits per sample */
    add_ifd_entry (ifd, 258, IFDE_TYP_SHORT, 1, depth);
    /* compression (uncompressed) */
    add_ifd_entry (ifd, 259, IFDE_TYP_SHORT, 1, 1);
    /* photometric interpretation */
    add_ifd_entry (ifd, 262, IFDE_TYP_SHORT, 1, 1);
    /* strip offset */
    add_ifd_entry (ifd, 273, IFDE_TYP_LONG, 1, strip_offset);
    /* orientation */
    add_ifd_entry (ifd, 274, IFDE_TYP_SHORT, 1, 1);
    /* samples per pixel */
    add_ifd_entry (ifd, 277, IFDE_TYP_SHORT, 1, 1);
    /* rows per strip */
    add_ifd_entry (ifd, 278, IFDE_TYP_LONG, 1, height);
    /* strip bytecount */
    add_ifd_entry (ifd, 279, IFDE_TYP_LONG, 1, strip_bytecount);
    /* min sample value */
    add_ifd_entry (ifd, 280, IFDE_TYP_SHORT, 1, 0);
    /* max sample value */
    add_ifd_entry (ifd, 281, IFDE_TYP_SHORT, 1, maxsamplevalue);
    if (resolution > 0)
    {
        /* x resolution */
        add_ifd_entry (ifd, 282, IFDE_TYP_RATIONAL, 1, data_offset);
        data_offset += 2*4;
        /* y resolution */
        add_ifd_entry (ifd, 283, IFDE_TYP_RATIONAL, 1, data_offset);
        data_offset += 2*4;
    }
    if (resolution > 0)
    {
        /* resolution unit (dpi) */
        add_ifd_entry (ifd, 296, IFDE_TYP_SHORT, 1, 2);
    }

    if (icc_len > 0) /* add ICC-profile TAG */
    {
      add_ifd_entry(ifd, 34675, 7, icc_len, data_offset);
      data_offset += icc_len;
    }

    /* I prefer motorola format. Its human readable. But for 16 bit, */
    /* the image format is defined by SANE to be the native byte order */
    if (bps == 1)
    {
        motorola = 1;
    }
    else
    {int check = 1;
        motorola = ((*((char *)&check)) == 0);
    }

    write_ifd (fptr, ifd, motorola);

    /* Write x/y resolution */
    if (resolution > 0)
    {
        write_i4 (fptr, resolution, motorola);
        write_i4 (fptr, 1, motorola);
        write_i4 (fptr, resolution, motorola);
        write_i4 (fptr, 1, motorola);
    }

    /* Write ICC profile */
    if (icc_len > 0)
    {
      int i;
      for (i=0; i<icc_len; i++)
      {
        if (!feof(icc_file))
        {
          fputc(fgetc(icc_file), fptr);
        }
        else
        {
          fprintf(stderr, "ICC profile %s is too short\n", icc_profile);
          break;
        }
      }
    }

    if (icc_file)
    {
      fclose(icc_file);
    }

    free_ifd (ifd);
}


static void
write_tiff_color_header (FILE *fptr, int width, int height, int depth,
                         int resolution, const char *icc_profile)
{IFD *ifd;
    int header_size = 8, ifd_size;
    int strip_offset, data_offset, data_size;
    int strip_bytecount;
    int ntags;
    int motorola, bps, maxsamplevalue;
    FILE *icc_file = 0;
    int icc_len = -1;

    if (icc_profile)
    {
      icc_file = fopen(icc_profile, "r");

      if (!icc_file)
      {
        fprintf(stderr, "Could not open ICC profile %s\n", icc_profile);
      }
      else
      {
        icc_len = 16777216 * fgetc(icc_file) + 65536 * fgetc(icc_file) + 256 * fgetc(icc_file) + fgetc(icc_file);
        rewind(icc_file);
      }
    }


    ifd = create_ifd ();

    bps = (depth <= 8) ? 1 : 2;  /* Bytes per sample */
    maxsamplevalue = (depth <= 8) ? 255 : 65535;
    strip_bytecount = width * height * 3 * bps;

    /* the following values must be known in advance */
    ntags = 13;
    data_size = 3*2 + 3*2 + 3*2;

    if (resolution > 0)
    {
        ntags += 3;
        data_size += 2*4 + 2*4;
    }

    if (icc_len > 0) /* if icc profile exists add memory for tag */
    {
        ntags += 1;
        data_size += icc_len;
    }


    ifd_size = 2 + ntags*12 + 4;
    data_offset = header_size + ifd_size;
    strip_offset = data_offset + data_size;

    /* New subfile type */
    add_ifd_entry (ifd, 254, IFDE_TYP_LONG, 1, 0);
    /* image width */
    add_ifd_entry (ifd, 256, (width > 0xffff) ? IFDE_TYP_LONG : IFDE_TYP_SHORT,
                   1, width);
    /* image length */
    add_ifd_entry (ifd, 257, (height > 0xffff) ? IFDE_TYP_LONG : IFDE_TYP_SHORT,
                   1, height);
    /* bits per sample */
    add_ifd_entry (ifd, 258, IFDE_TYP_SHORT, 3, data_offset);
    data_offset += 3*2;
    /* compression (uncompressed) */
    add_ifd_entry (ifd, 259, IFDE_TYP_SHORT, 1, 1);
    /* photometric interpretation */
    add_ifd_entry (ifd, 262, IFDE_TYP_SHORT, 1, 2);
    /* strip offset */
    add_ifd_entry (ifd, 273, IFDE_TYP_LONG, 1, strip_offset);
    /* orientation */
    add_ifd_entry (ifd, 274, IFDE_TYP_SHORT, 1, 1);
    /* samples per pixel */
    add_ifd_entry (ifd, 277, IFDE_TYP_SHORT, 1, 3);
    /* rows per strip */
    add_ifd_entry (ifd, 278, IFDE_TYP_LONG, 1, height);
    /* strip bytecount */
    add_ifd_entry (ifd, 279, IFDE_TYP_LONG, 1, strip_bytecount);
    /* min sample value */
    add_ifd_entry (ifd, 280, IFDE_TYP_SHORT, 3, data_offset);
    data_offset += 3*2;
    /* max sample value */
    add_ifd_entry (ifd, 281, IFDE_TYP_SHORT, 3, data_offset);
    data_offset += 3*2;

    if (resolution > 0)
    {
        /* x resolution */
        add_ifd_entry (ifd, 282, IFDE_TYP_RATIONAL, 1, data_offset);
        data_offset += 2*4;
        /* y resolution */
        add_ifd_entry (ifd, 283, IFDE_TYP_RATIONAL, 1, data_offset);
        data_offset += 2*4;
    }

    if (resolution > 0)
    {
        /* resolution unit (dpi) */
        add_ifd_entry (ifd, 296, IFDE_TYP_SHORT, 1, 2);
    }

    if (icc_len > 0) /* add ICC-profile TAG */
    {
      add_ifd_entry(ifd, 34675, 7, icc_len, data_offset);
      data_offset += icc_len;
    }


    /* I prefer motorola format. Its human readable. But for 16 bit, */
    /* the image format is defined by SANE to be the native byte order */
    if (bps == 1)
    {
        motorola = 1;
    }
    else
    {int check = 1;
        motorola = ((*((char *)&check)) == 0);
    }

    write_ifd (fptr, ifd, motorola);

    /* Write bits per sample value values */
    write_i2 (fptr, depth, motorola);
    write_i2 (fptr, depth, motorola);
    write_i2 (fptr, depth, motorola);

    /* Write min sample value values */
    write_i2 (fptr, 0, motorola);
    write_i2 (fptr, 0, motorola);
    write_i2 (fptr, 0, motorola);

    /* Write max sample value values */
    write_i2 (fptr, maxsamplevalue, motorola);
    write_i2 (fptr, maxsamplevalue, motorola);
    write_i2 (fptr, maxsamplevalue, motorola);

    /* Write x/y resolution */
    if (resolution > 0)
    {
        write_i4 (fptr, resolution, motorola);
        write_i4 (fptr, 1, motorola);
        write_i4 (fptr, resolution, motorola);
        write_i4 (fptr, 1, motorola);
    }

    /* Write ICC profile */
    if (icc_len > 0)
    {
      int i;
      for (i=0; i<icc_len; i++)
      {
        if (!feof(icc_file))
        {
          fputc(fgetc(icc_file), fptr);
        }
        else
        {
          fprintf(stderr, "ICC profile %s is too short\n", icc_profile);
          break;
        }
      }
    }

    if (icc_file)
    {
      fclose(icc_file);
    }

    free_ifd (ifd);
}


void
sanei_write_tiff_header (SANE_Frame format, int width, int height, int depth,
			 int resolution, const char *icc_profile, FILE *ofp)
{
#ifdef __EMX__	/* OS2 - write in binary mode. */
    _fsetmode(ofp, "b");
#endif
    switch (format)
    {
    case SANE_FRAME_RED:
    case SANE_FRAME_GREEN:
    case SANE_FRAME_BLUE:
    case SANE_FRAME_RGB:
        write_tiff_color_header (ofp, width, height, depth, resolution, icc_profile);
        break;

    default:
        if (depth == 1)
            write_tiff_bw_header (ofp, width, height, resolution);
        else
            write_tiff_grey_header (ofp, width, height, depth, resolution, icc_profile);
        break;
    }
}
