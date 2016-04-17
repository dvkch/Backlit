/* sane - Scanner Access Now Easy.

   Copyright (C) 1997, 1998, 2002, 2013 Franck Schnefra, Michel Roelofs,
   Emmanuel Blot, Mikko Tyolajarvi, David Mosberger-Tang, Wolfgang Goeller,
   Petter Reinholdtsen, Gary Plewa, Sebastien Sable, Max Ushakov,
   Andrew Goodbody, Oliver Schwartz and Kevin Charter

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

   This file is a component of the implementation of a backend for many
   of the AGFA SnapScan and Acer Vuego/Prisa flatbed scanners. */


/* $Id$
   SnapScan backend data sources (implementation) */

/**************************************************************************************
If you get confused from all the structs (like I did when I first saw them),
think of it as "C++ in C". If you're accustomed to OO and UML maybe the
following diagram helps you to make sense of it:

                       ------------------------
                       !        Source        !
                       ------------------------
                       !pss: SnapScan_Scanner*!
                       ------------------------   +psub
                       !init() = 0            !-----------------
                       !remaining() = 0       !                !
                       !bytesPerLine()        !                !
                       !pixelsPerLine()       !                !
                       !get() = 0             !                !{TransformerSource forwards
                       !done() = 0            !                ! function calls to corres-
                       ------------------------                ! ponding functions in psub}
                                   ^                           !
                                  /_\                          !
                                   !                           !
       --------------------------------------------------     /\
       !               !                !               !     \/
-------------    -------------    -------------    -------------------
!SCSISource !    ! FDSource  !    !BufSource  !    !TransformerSource!
=============    =============    =============    ===================
!remaining()!    !remaining()!    !remaining()!    !init()           !
!get()      !    !get()      !    !get()      !    !remaining()      !
!done()     !    !done()     !    !done()     !    !bytesPerLine()   !
!init()     !    !init()     !    !init()     !    !pixelsPerLine()  !
-------------    -------------    -------------    !get()            !
                                                   !done()           !
                                                   -------------------
                                                            ^
                                                           /_\
                                                            !
                                       ------------------------------------
                                       !                 !                !
                              ----------------    -------------    -------------
                              !   Expander   !    ! RGBRouter !    !  Inverter !
                              ================    =============    =============
                              !remaining()   !    !remaining()!    !remaining()!
                              !bytesPerLine()!    !get()      !    !get()      !
                              !get()         !    !done()     !    !done()     !
                              !done()        !    !init()     !    !init()     !
                              !init()        !    -------------    -------------
                              ----------------
All instances of the descendants of TransformerSource can be chained together. For
color scanning, a typical source chain would consist of an RGBRouter sitting on top
of a SCSISource. In the get() method, RGBRouter will then call the get() method of
the subsource, process the data and return it.

I hope this makes sense to you (and I got the right idea of the original author's
intention).
***********************************************************************************/

#ifndef __FUNCTION__
#define __FUNCTION__ "(undef)"
#endif

static SANE_Status Source_init (Source *pself,
                                SnapScan_Scanner *pss,
                                SourceRemaining remaining,
                                SourceBytesPerLine bytesPerLine,
                                SourcePixelsPerLine pixelsPerLine,
                                SourceGet get,
                                SourceDone done)
{
    pself->pss = pss;
    pself->remaining = remaining;
    pself->bytesPerLine = bytesPerLine;
    pself->pixelsPerLine = pixelsPerLine;
    pself->get = get;
    pself->done = done;
    return SANE_STATUS_GOOD;
}

/* these are defaults, normally used only by base sources */

static SANE_Int Source_bytesPerLine (Source *pself)
{
    return pself->pss->bytes_per_line;
}

static SANE_Int Source_pixelsPerLine (Source *pself)
{
    return pself->pss->pixels_per_line;
}

/**********************************************************************/

/* the base sources */
typedef enum
{
    SCSI_SRC,
    FD_SRC,
    BUF_SRC
} BaseSourceType;


typedef struct
{
    SOURCE_GUTS;
    SANE_Int scsi_buf_pos;    /* current position in scsi buffer */
    SANE_Int scsi_buf_max;    /* data limit */
    SANE_Int absolute_max;    /* largest possible data read */
} SCSISource;

static SANE_Int SCSISource_remaining (Source *pself)
{
    SCSISource *ps = (SCSISource *) pself;
    return ps->pss->bytes_remaining + (ps->scsi_buf_max - ps->scsi_buf_pos);
}

static SANE_Status SCSISource_get (Source *pself,
                                   SANE_Byte *pbuf,
                                   SANE_Int *plen)
{
    SCSISource *ps = (SCSISource *) pself;
    SANE_Status status = SANE_STATUS_GOOD;
    SANE_Int remaining = *plen;
    char* me = "SCSISource_get";

    DBG (DL_CALL_TRACE, "%s\n", me);
    while (remaining > 0
           && pself->remaining(pself) > 0
           && status == SANE_STATUS_GOOD
           && !cancelRead)
    {
        SANE_Int ndata = ps->scsi_buf_max - ps->scsi_buf_pos;
        DBG (DL_DATA_TRACE, "%s: ndata %d; remaining %d\n", me, ndata, remaining);
        if (ndata == 0)
        {
            ps->pss->expected_read_bytes = MIN((size_t)ps->absolute_max,
                                           ps->pss->bytes_remaining);
            ps->scsi_buf_pos = 0;
            ps->scsi_buf_max = 0;
            status = scsi_read (ps->pss, READ_IMAGE);
            if (status != SANE_STATUS_GOOD)
                break;
            ps->scsi_buf_max = ps->pss->read_bytes;
            ndata = ps->pss->read_bytes;
            ps->pss->bytes_remaining -= ps->pss->read_bytes;
            DBG (DL_DATA_TRACE, "%s: pos: %d; max: %d; expected: %lu; read: %lu\n",
                me, ps->scsi_buf_pos, ps->scsi_buf_max, (u_long) ps->pss->expected_read_bytes,
                (u_long) ps->pss->read_bytes);
        }
        ndata = MIN(ndata, remaining);
        memcpy (pbuf, ps->pss->buf + ps->scsi_buf_pos, (size_t)ndata);
        pbuf += ndata;
        ps->scsi_buf_pos += ndata;
        remaining -= ndata;
    }
    *plen -= remaining;
    return status;
}

static SANE_Status SCSISource_done (Source *pself)
{
    DBG(DL_MINOR_INFO, "SCSISource_done\n");
    UNREFERENCED_PARAMETER(pself);
    return SANE_STATUS_GOOD;
}

static SANE_Status SCSISource_init (SCSISource *pself, SnapScan_Scanner *pss)
{
    SANE_Status status = Source_init ((Source *) pself, pss,
                                      SCSISource_remaining,
                                      Source_bytesPerLine,
                                      Source_pixelsPerLine,
                                      SCSISource_get,
                                      SCSISource_done);
    if (status == SANE_STATUS_GOOD)
    {
        pself->scsi_buf_max = 0;
        pself->scsi_buf_pos = 0;
        pself->absolute_max =
            (pss->phys_buf_sz/pss->bytes_per_line)*pss->bytes_per_line;
    }
    return status;
}

/* File sources */

typedef struct
{
    SOURCE_GUTS;
    int fd;
    SANE_Int bytes_remaining;
} FDSource;

static SANE_Int FDSource_remaining (Source *pself)
{
    FDSource *ps = (FDSource *) pself;    
    return ps->bytes_remaining;
}

static SANE_Status FDSource_get (Source *pself, SANE_Byte *pbuf, SANE_Int *plen)
{
    SANE_Status status = SANE_STATUS_GOOD;
    FDSource *ps = (FDSource *) pself;
    SANE_Int remaining = *plen;

    while (remaining > 0
           && pself->remaining(pself) > 0
           && status == SANE_STATUS_GOOD)
    {
        SANE_Int bytes_read = read (ps->fd, pbuf, remaining);
        if (bytes_read == -1)
        {
            if (errno == EAGAIN)
            {
                /* No data currently available */
                break;
            }
            /* It's an IO error */
            DBG (DL_MAJOR_ERROR, "%s: read failed: %s\n",
                     __FUNCTION__, strerror(errno));
            status = SANE_STATUS_IO_ERROR;
        }
        else if (bytes_read == 0)
        {
            /* EOF of current reading */
            DBG(DL_DATA_TRACE, "%s: EOF\n",__FUNCTION__);
            break;
        }
        ps->bytes_remaining -= bytes_read;
        remaining -= bytes_read;
        pbuf += bytes_read;
    }
    *plen -= remaining;
    return status;
}

static SANE_Status FDSource_done (Source *pself)
{
    close(((FDSource *) pself)->fd);
    return SANE_STATUS_GOOD;
}

static SANE_Status FDSource_init (FDSource *pself,
                                  SnapScan_Scanner *pss,
                                  int fd)
{
    SANE_Status status = Source_init ((Source *) pself,
                                      pss,
                                      FDSource_remaining,
                                      Source_bytesPerLine,
                                      Source_pixelsPerLine,
                                      FDSource_get,
                                      FDSource_done);
    if (status == SANE_STATUS_GOOD)
    {
        pself->fd = fd;
        pself->bytes_remaining = pss->bytes_per_line * (pss->lines + pss->chroma);
    }
    return status;
}


/* buffer sources simply read from a pre-filled buffer; we have these
   so that we can include source chain processing overhead in the
   measure_transfer_rate() function */

typedef struct
{
    SOURCE_GUTS;
    SANE_Byte *buf;
    SANE_Int buf_size;
    SANE_Int buf_pos;
} BufSource;

static SANE_Int BufSource_remaining (Source *pself)
{
    BufSource *ps = (BufSource *) pself;
    return  ps->buf_size - ps->buf_pos;
}

static SANE_Status BufSource_get (Source *pself,
                                  SANE_Byte *pbuf,
                                  SANE_Int *plen)
{
    BufSource *ps = (BufSource *) pself;
    SANE_Status status = SANE_STATUS_GOOD;
    SANE_Int to_move = MIN(*plen, pself->remaining(pself));
    if (to_move == 0)
    {
        status = SANE_STATUS_EOF;
    }
    else
    {
        memcpy (pbuf, ps->buf + ps->buf_pos, to_move);
        ps->buf_pos += to_move;
        *plen = to_move;
    }
    return status;
}

static SANE_Status BufSource_done (Source *pself)
{
    UNREFERENCED_PARAMETER(pself);
    return SANE_STATUS_GOOD;
}

static SANE_Status BufSource_init (BufSource *pself,
                                   SnapScan_Scanner *pss,
                                     SANE_Byte *buf,
                                   SANE_Int buf_size)
{
    SANE_Status status = Source_init ((Source *) pself,
                                      pss,
                                      BufSource_remaining,
                                      Source_bytesPerLine,
                                      Source_pixelsPerLine,
                                      BufSource_get,
                                      BufSource_done);
    DBG(DL_DATA_TRACE, "BufSource_init: buf_size=%d\n", buf_size);
    if (status == SANE_STATUS_GOOD)
    {
        pself->buf = buf;
        pself->buf_size = buf_size;
        pself->buf_pos = 0;
    }
    return status;
}

/* base source creation */

static SANE_Status create_base_source (SnapScan_Scanner *pss,
                                       BaseSourceType st,
                                       Source **pps)
{
    SANE_Status status = SANE_STATUS_GOOD;
    *pps = NULL;
    switch (st)
    {
    case SCSI_SRC:
        *pps = (Source *) malloc(sizeof(SCSISource));
        if (*pps == NULL)
        {
            DBG (DL_MAJOR_ERROR, "failed to allocate SCSISource");
            status = SANE_STATUS_NO_MEM;
        }
        else
        {
            status = SCSISource_init ((SCSISource *) *pps, pss);
        }
    break;
    case FD_SRC:
        *pps = (Source *) malloc(sizeof(FDSource));
        if (*pps == NULL)
        {
            DBG (DL_MAJOR_ERROR, "failed to allocate FDSource");
            status = SANE_STATUS_NO_MEM;
        }
        else
        {
            status = FDSource_init ((FDSource *) *pps, pss, pss->rpipe[0]);
        }
    break;
    case BUF_SRC:
        *pps = (Source *) malloc(sizeof(BufSource));
        if (*pps == NULL)
        {
            DBG (DL_MAJOR_ERROR, "failed to allocate BufSource");
            status = SANE_STATUS_NO_MEM;
        }
        else
        {
            status = BufSource_init ((BufSource *) *pps,
                                     pss,
                                     pss->buf,
                                     pss->read_bytes);
        }
    break;
    default:
        DBG (DL_MAJOR_ERROR, "illegal base source type %d", st);
        break;
    }
    return status;
}

/**********************************************************************/

/* The transformer sources */

#define TX_SOURCE_GUTS \
        SOURCE_GUTS;\
        Source *psub    /* sub-source */

typedef struct
{
    TX_SOURCE_GUTS;
} TxSource;

static SANE_Int TxSource_remaining (Source *pself)
{
    TxSource *ps = (TxSource *) pself;
    return ps->psub->remaining(ps->psub);
}

static SANE_Int TxSource_bytesPerLine (Source *pself)
{
    TxSource *ps = (TxSource *) pself;
    return ps->psub->bytesPerLine(ps->psub);
}

static SANE_Int TxSource_pixelsPerLine (Source *pself)
{
    TxSource *ps = (TxSource *) pself;
    return ps->psub->pixelsPerLine(ps->psub);
}

static SANE_Status TxSource_get (Source *pself, SANE_Byte *pbuf, SANE_Int *plen)
{
    TxSource *ps = (TxSource *) pself;
    return ps->psub->get(ps->psub, pbuf, plen);
}

static SANE_Status TxSource_done (Source *pself)
{
    TxSource *ps = (TxSource *) pself;
    SANE_Status status = ps->psub->done(ps->psub);
    free(ps->psub);
    ps->psub = NULL;
    return status;
}

static SANE_Status TxSource_init (TxSource *pself,
                                  SnapScan_Scanner *pss,
                                  SourceRemaining remaining,
                                  SourceBytesPerLine bytesPerLine,
                                  SourcePixelsPerLine pixelsPerLine,
                                  SourceGet get,
                                  SourceDone done,
                                  Source *psub)
{
    SANE_Status status = Source_init((Source *) pself,
                                     pss,
                                     remaining,
                                     bytesPerLine,
                                     pixelsPerLine,
                                     get,
                                     done);
    if (status == SANE_STATUS_GOOD)
        pself->psub = psub;
    return status;
}

/* The expander makes three-channel, one-bit, raw scanner data into
   8-bit data. It is used to support the bilevel colour scanning mode */

typedef struct
{
    TX_SOURCE_GUTS;
    SANE_Byte *ch_buf;            /* channel buffer */
    SANE_Int   ch_size;            /* channel buffer size = #bytes in a channel */
    SANE_Int   ch_ndata;        /* actual #bytes in channel buffer */
    SANE_Int   ch_pos;            /* position in buffer */
    SANE_Int   bit;                /* current bit */
    SANE_Int   last_bit;        /* current last bit (counting down) */
    SANE_Int   last_last_bit;    /* last bit in the last byte of the channel */
} Expander;

static SANE_Int Expander_remaining (Source *pself)
{
    Expander *ps = (Expander *) pself;
    SANE_Int sub_remaining = TxSource_remaining(pself);
    SANE_Int sub_bits_per_channel = TxSource_pixelsPerLine(pself);
    SANE_Int whole_channels = sub_remaining/ps->ch_size;
    SANE_Int result = whole_channels*sub_bits_per_channel;

    if (ps->ch_pos < ps->ch_size)
    {
        SANE_Int bits_covered = MAX((ps->ch_pos - 1)*8, 0) + 7 - ps->bit;
        result += sub_bits_per_channel - bits_covered;
    }

    return result;
}

static SANE_Int Expander_bytesPerLine (Source *pself)
{
    return TxSource_pixelsPerLine(pself)*3;
}

static SANE_Status Expander_get (Source *pself, SANE_Byte *pbuf, SANE_Int *plen)
{
    Expander *ps = (Expander *) pself;
    SANE_Status status = SANE_STATUS_GOOD;
    SANE_Int remaining = *plen;

    while (remaining > 0
           &&
           pself->remaining(pself) > 0 &&
           !cancelRead)
    {
        if (ps->ch_pos == ps->ch_ndata)
        {
            /* we need more data; try to get the remainder of the current
               channel, or else the next channel */
            SANE_Int ndata = ps->ch_size - ps->ch_ndata;
            if (ndata == 0)
            {
                ps->ch_ndata = 0;
                ps->ch_pos = 0;
                ndata = ps->ch_size;
            }
            status = TxSource_get(pself, ps->ch_buf + ps->ch_pos, &ndata);
            if (status != SANE_STATUS_GOOD)
                break;
            if (ndata == 0)
                break;
            ps->ch_ndata += ndata;
            if (ps->ch_pos == (ps->ch_size - 1))
                ps->last_bit = ps->last_last_bit;
            else
                ps->last_bit = 0;
            ps->bit = 7;
        }
        *pbuf = ((ps->ch_buf[ps->ch_pos] >> ps->bit) & 0x01) ? 0xFF : 0x00;
        pbuf++;
        remaining--;

        if (ps->bit == ps->last_bit)
        {
            ps->bit = 7;
            ps->ch_pos++;
            if (ps->ch_pos == (ps->ch_size - 1))
                ps->last_bit = ps->last_last_bit;
            else
                ps->last_bit = 0;
        }
        else
        {
            ps->bit--;
        }
    }

    *plen -= remaining;
    return status;
}

static SANE_Status Expander_done (Source *pself)
{
    Expander *ps = (Expander *) pself;
    SANE_Status status = TxSource_done(pself);
    free(ps->ch_buf);
    ps->ch_buf = NULL;
    ps->ch_size = 0;
    ps->ch_pos = 0;
    return status;
}

static SANE_Status Expander_init (Expander *pself,
                  SnapScan_Scanner *pss,
                                  Source *psub)
{
    SANE_Status status = TxSource_init((TxSource *) pself,
                                       pss,
                                       Expander_remaining,
                                       Expander_bytesPerLine,
                                       TxSource_pixelsPerLine,
                                       Expander_get,
                                       Expander_done,
                                       psub);
    if (status == SANE_STATUS_GOOD)
    {
        pself->ch_size = TxSource_bytesPerLine((Source *) pself)/3;
        pself->ch_buf = (SANE_Byte *) malloc(pself->ch_size);
        if (pself->ch_buf == NULL)
        {
            DBG (DL_MAJOR_ERROR,
                 "%s: couldn't allocate channel buffer.\n",
                 __FUNCTION__);
            status = SANE_STATUS_NO_MEM;
        }
        else
        {
            pself->ch_ndata = 0;
            pself->ch_pos = 0;
            pself->last_last_bit = pself->pixelsPerLine((Source *) pself)%8;
            if (pself->last_last_bit == 0)
                pself->last_last_bit = 7;
            pself->last_last_bit = 7 - pself->last_last_bit;
            pself->bit = 7;
            if (pself->ch_size > 1)
                pself->last_bit = 0;
            else
                pself->last_bit = pself->last_last_bit;
        }
    }
    return status;
}

static SANE_Status create_Expander (SnapScan_Scanner *pss,
                                    Source *psub,
                                    Source **pps)
{
    SANE_Status status = SANE_STATUS_GOOD;
    *pps = (Source *) malloc(sizeof(Expander));
    if (*pps == NULL)
    {
        DBG (DL_MAJOR_ERROR,
             "%s: failed to allocate Expander.\n",
             __FUNCTION__);
        status = SANE_STATUS_NO_MEM;
    }
    else
    {
        status = Expander_init ((Expander *) *pps, pss, psub);
    }
    return status;
}

/* 
   This filter implements a fix for scanners that have some columns
   of pixels offset. Currently it only shifts every other column
   starting with the first one down ch_offset pixels.
   
   The Deinterlacer detects if data is in SANE RGB frame format (3 bytes/pixel)
   or in Grayscale (1 byte/pixel).
   
   The first ch_offset lines of data in the output are fudged so that even indexed
   add odd indexed pixels will have the same value. This is necessary because
   the real pixel values of the columns that are shifted down are not 
   in the data for the first ch_offset lines. A better way to handle this would be to
   scan in ch_offset extra lines of data, but I haven't figured out how to do this 
   yet.

*/

typedef struct
{
    TX_SOURCE_GUTS;
    SANE_Byte *ch_buf;            /* channel buffer */
    SANE_Int   ch_size;           /* channel buffer size */
    SANE_Int   ch_line_size;      /* size of one line */
    SANE_Int   ch_ndata;          /* actual #bytes in channel buffer */
    SANE_Int   ch_pos;            /* position in buffer */
    SANE_Int   ch_bytes_per_pixel;
    SANE_Bool  ch_lineart;
    SANE_Int   ch_offset;         /* The number of lines to be shifted */
    SANE_Bool  ch_past_init;      /* flag indicating if we have enough data to shift pixels down */
    SANE_Bool  ch_shift_even;     /* flag indicating wether even or odd pixels are shifted */
} Deinterlacer;

static SANE_Int Deinterlacer_remaining (Source *pself)
{
    Deinterlacer *ps = (Deinterlacer *) pself;
    SANE_Int result = TxSource_remaining(pself);
    result += ps->ch_ndata - ps->ch_pos;
    return result;
}

static SANE_Status Deinterlacer_get (Source *pself, SANE_Byte *pbuf, SANE_Int *plen)
{
    Deinterlacer *ps = (Deinterlacer *) pself;
    SANE_Status status = SANE_STATUS_GOOD;
    SANE_Int remaining = *plen;
    SANE_Int org_len = *plen;
    char *me = "Deinterlacer_get";
    
    DBG(DL_DATA_TRACE, "%s: remaining=%d, pself->remaining=%d, ch_ndata=%d, ch_pos=%d\n",
            me, remaining, pself->remaining(pself), ps->ch_ndata, ps->ch_pos);

    while (remaining > 0
           &&
           pself->remaining(pself) > 0 &&
           !cancelRead)
    {
        if (ps->ch_pos % (ps->ch_line_size) == ps->ch_ndata % (ps->ch_line_size) )
        {
            /* we need more data; try to get the remainder of the current
               line, or else the next line */
            SANE_Int ndata = (ps->ch_line_size) - ps->ch_ndata % (ps->ch_line_size);
            if (ps->ch_pos >= ps->ch_size)
            {
	        /* wrap to the beginning of the buffer if we need to */
                ps->ch_ndata = 0;
                ps->ch_pos = 0;
                ndata = ps->ch_line_size;
            }
            status = TxSource_get(pself, ps->ch_buf + ps->ch_pos, &ndata);
            if (status != SANE_STATUS_GOOD)
                break;
            if (ndata == 0)
                break;
            ps->ch_ndata += ndata;
        }
        /* Handle special lineart mode: Valid pixels need to be masked */
        if (ps->ch_lineart)
        {
            if (ps->ch_past_init)
            {
                if (ps->ch_shift_even)
                {
                    /* Even columns need to be shifted, i.e. bits 1,3,5,7 -> 0xaa */
                    /* use valid pixels from this line and shifted pixels from ch_size lines back */
                    *pbuf = (ps->ch_buf[ps->ch_pos] & 0x55) |
                            (ps->ch_buf[(ps->ch_pos + (ps->ch_line_size)) % ps->ch_size] & 0xaa);
                }
                else
                {
                    /* Odd columns need to be shifted, i.e. bits 0,2,4,6 -> 0x55 */
                    *pbuf = (ps->ch_buf[ps->ch_pos] & 0xaa) |
                            (ps->ch_buf[(ps->ch_pos + (ps->ch_line_size)) % ps->ch_size] & 0x55);
                }
            }
            else
            {
                /* not enough data. duplicate pixel values from previous column */
                if (ps->ch_shift_even)
                {
                    /* bits 0,2,4,6 contain valid data -> 0x55 */
                    SANE_Byte valid_pixel = ps->ch_buf[ps->ch_pos] & 0x55;
                    *pbuf = valid_pixel | (valid_pixel >> 1);
                }
                else
                {

                    /* bits 1,3,5,7 contain valid data -> 0xaa */
                    SANE_Byte valid_pixel = ps->ch_buf[ps->ch_pos] & 0xaa;
                    *pbuf = valid_pixel | (valid_pixel << 1);
                }
            }
        }
        else /* colour / grayscale mode */
        {
            if ((ps->ch_shift_even && ((ps->ch_pos/ps->ch_bytes_per_pixel) % 2 == 0)) ||
                (!ps->ch_shift_even && ((ps->ch_pos/ps->ch_bytes_per_pixel) % 2 == 1)))
            {
                /* the even indexed pixels need to be shifted down */
                if (ps->ch_past_init){
                    /* We need to use data 4 lines back */
                    /* So we just go one forward and it will wrap around to 4 back. */
                    *pbuf = ps->ch_buf[(ps->ch_pos + (ps->ch_line_size)) % ps->ch_size];
                }else{
                    /* Use data from the next pixel for even indexed pixels
                      if we are on the first few lines. 
                      TODO: also we will overread the buffer if the buffer read ended 
                      on the first pixel. */
                    if (ps->ch_pos % (ps->ch_line_size) == 0 )
                        *pbuf = ps->ch_buf[ps->ch_pos+ps->ch_bytes_per_pixel];
                    else
                        *pbuf = ps->ch_buf[ps->ch_pos-ps->ch_bytes_per_pixel];
                }
            }else{
                /* odd indexed pixels are okay */
                *pbuf = ps->ch_buf[ps->ch_pos];
            }
        }
	/* set the flag so we know we have enough data to start shifting columns */
	if (ps->ch_pos >= ps->ch_line_size * ps->ch_offset)
	   ps->ch_past_init = SANE_TRUE;

        pbuf++;
        remaining--;
        ps->ch_pos++;
    }

    *plen -= remaining;
    
    DBG(DL_DATA_TRACE,
        "%s: Request=%d, remaining=%d, read=%d, TXSource_rem=%d, bytes_rem=%lu\n",
        me,
        org_len,
        pself->remaining(pself),
        *plen,
        TxSource_remaining(pself),
        (u_long) ps->pss->bytes_remaining);    
    return status;
}

static SANE_Status Deinterlacer_done (Source *pself)
{
    Deinterlacer *ps = (Deinterlacer *) pself;
    SANE_Status status = TxSource_done(pself);
    free(ps->ch_buf);
    ps->ch_buf = NULL;
    ps->ch_size = 0;
    ps->ch_line_size = 0;
    ps->ch_pos = 0;
    return status;
}

static SANE_Status Deinterlacer_init (Deinterlacer *pself,
                  SnapScan_Scanner *pss,
                                  Source *psub)
{
    SANE_Status status = TxSource_init((TxSource *) pself,
                                       pss,
                                       Deinterlacer_remaining,
                                       TxSource_bytesPerLine,
                                       TxSource_pixelsPerLine,
                                       Deinterlacer_get,
                                       Deinterlacer_done,
                                       psub);
    if (status == SANE_STATUS_GOOD)
    {
        pself->ch_shift_even = SANE_TRUE;
        switch (pss->pdev->model)
        {
        case PERFECTION3490:
            pself->ch_offset = 8;
            if ((actual_mode(pss) == MD_GREYSCALE) || (actual_mode(pss) == MD_LINEART))
                 pself->ch_shift_even = SANE_FALSE;
            break;
        case PERFECTION2480:
        default:
            pself->ch_offset = 4;
            break;
        }
        pself->ch_line_size = TxSource_bytesPerLine((Source *) pself);
        /* We need at least ch_offset+1 lines of buffer in order 
           to shift up ch_offset pixels. */
        pself->ch_size = pself->ch_line_size * (pself->ch_offset + 1);
        pself->ch_buf = (SANE_Byte *) malloc(pself->ch_size);
        if (pself->ch_buf == NULL)
        {
            DBG (DL_MAJOR_ERROR,
                 "%s: couldn't allocate channel buffer.\n",
                 __FUNCTION__);
            status = SANE_STATUS_NO_MEM;
        }
        else
        {
            pself->ch_ndata = 0;
            pself->ch_pos = 0;
            pself->ch_past_init = SANE_FALSE;
	    if ((actual_mode(pss) == MD_GREYSCALE) || (actual_mode(pss) == MD_LINEART))
	    	pself->ch_bytes_per_pixel = 1;
	    else
	    	pself->ch_bytes_per_pixel = 3;
	    if (pss->bpp_scan == 16)
		pself->ch_bytes_per_pixel *= 2;
        }
        pself->ch_lineart = (actual_mode(pss) == MD_LINEART);
    }
    return status;
}

static SANE_Status create_Deinterlacer (SnapScan_Scanner *pss,
                                    Source *psub,
                                    Source **pps)
{
    SANE_Status status = SANE_STATUS_GOOD;
    *pps = (Source *) malloc(sizeof(Deinterlacer));
    if (*pps == NULL)
    {
        DBG (DL_MAJOR_ERROR,
             "%s: failed to allocate Deinterlacer.\n",
             __FUNCTION__);
        status = SANE_STATUS_NO_MEM;
    }
    else
    {
        status = Deinterlacer_init ((Deinterlacer *) *pps, pss, psub);
    }
    return status;
}

/* ----------------------------------------------------- */

/* the RGB router assumes 8-bit RGB data arranged in contiguous
   channels, possibly with R-G and R-B offsets, and rearranges the
   data into SANE RGB frame format */

typedef struct
{
    TX_SOURCE_GUTS;
    SANE_Byte *cbuf;            /* circular line buffer */
    SANE_Byte *xbuf;            /* single line buffer */
    SANE_Int pos;                    /* current position in xbuf */
    SANE_Int cb_size;            /* size of the circular buffer */
    SANE_Int cb_line_size;/* size of a line in the circular buffer */
    SANE_Int cb_start;        /* start of valid data in the circular buffer */
    SANE_Int cb_finish;        /* finish of valid data, for next read */
    SANE_Int ch_offset[3];/* offset in cbuf */
    SANE_Int round_req;
    SANE_Int round_read;
} RGBRouter;

static void put_int16r (int n, u_char *p)
{
	p[0] = (n & 0x00ff);
	p[1] = (n & 0xff00) >> 8;
}


static SANE_Int RGBRouter_remaining (Source *pself)
{
    RGBRouter *ps = (RGBRouter *) pself;
   SANE_Int remaining;
    if (ps->round_req == ps->cb_size)
        remaining = TxSource_remaining(pself) - ps->cb_size + ps->cb_line_size;
    else
      remaining = TxSource_remaining(pself) + ps->cb_line_size - ps->pos;
   return (remaining);
}

static SANE_Status RGBRouter_get (Source *pself,
                                  SANE_Byte *pbuf,
                                  SANE_Int *plen)
{
    RGBRouter *ps = (RGBRouter *) pself;
    SANE_Status status = SANE_STATUS_GOOD;
    SANE_Int remaining = *plen;
    SANE_Byte *s;
    SANE_Int i, t;
    SANE_Int r, g, b;
    SANE_Int run_req;
    SANE_Int org_len = *plen;
    char *me = "RGBRouter_get";

    while (remaining > 0  &&  pself->remaining(pself) > 0 && !cancelRead)
    {
        DBG(DL_DATA_TRACE, "%s: remaining=%d, pself->remaining=%d, round_req=%d, cb_size=%d\n",
            me, remaining, pself->remaining(pself), ps->round_req, ps->cb_size);
        /* Check if there is no valid data left from previous get */
        if (ps->pos >= ps->cb_line_size)
        {
            /* Try to get more data. either one line or
               full buffer (first time) */
            do
            {
                run_req = ps->round_req - ps->round_read;
                status = TxSource_get (pself,
                                       ps->cbuf + ps->cb_start + ps->round_read,
                                       &run_req);
                if (status != SANE_STATUS_GOOD || run_req==0)
                {
                    *plen -= remaining;
                    if ( *plen > 0 )
                        DBG(DL_DATA_TRACE, "%s: request=%d, read=%d\n",
                            me, org_len, *plen);
                    return status;
                }
                ps->round_read += run_req;
            }
            while ((ps->round_req > ps->round_read) && !cancelRead);

            /* route RGB */
            ps->cb_start = (ps->cb_start + ps->round_read)%ps->cb_size;
            s = ps->xbuf;
            r = (ps->cb_start + ps->ch_offset[0])%ps->cb_size;
            g = (ps->cb_start + ps->ch_offset[1])%ps->cb_size;
            b = (ps->cb_start + ps->ch_offset[2])%ps->cb_size;
            for (i = 0;  i < ps->cb_line_size/3;  i++)
            {
                if (pself->pss->bpp_scan == 8)
                {
                    *s++ = ps->cbuf[r++];
                    *s++ = ps->cbuf[g++];
                    *s++ = ps->cbuf[b++];
                }
                else if (pself->pss->pdev->model == SCANWIT2720S)
                {
                    t = (((ps->cbuf[r+1] << 8) | ps->cbuf[r]) & 0xfff) << 4;
                    put_int16r (t, s);
                    s += 2;
                    r += 2;
                    t = (((ps->cbuf[g+1] << 8) | ps->cbuf[g]) & 0xfff) << 4;
                    put_int16r (t, s);
                    s += 2;
                    g += 2;
                    t = (((ps->cbuf[b+1] << 8) | ps->cbuf[b]) & 0xfff) << 4;
                    put_int16r (t, s);
                    s += 2;
                    b += 2;
                    i++;
                }
                else
                {
                    *s++ = ps->cbuf[r++];
                    *s++ = ps->cbuf[r++];
                    *s++ = ps->cbuf[g++];
                    *s++ = ps->cbuf[g++];
                    *s++ = ps->cbuf[b++];
                    *s++ = ps->cbuf[b++];
                    i++;
                }
            }

            /* end of reading & offsetiing whole line data;
               reset valid position */
            ps->pos = 0;

            /* prepare for next round */
            ps->round_req = ps->cb_line_size;
            ps->round_read =0;
        }

        /* Repack the whole scan line and copy to caller's buffer */
        while (remaining > 0  &&  ps->pos < ps->cb_line_size)
        {
            *pbuf++ = ps->xbuf[ps->pos++];
            remaining--;
        }
    }
    *plen -= remaining;
    DBG(DL_DATA_TRACE,
        "%s: Request=%d, remaining=%d, read=%d, TXSource_rem=%d, bytes_rem=%lu\n",
        me,
        org_len,
        pself->remaining(pself),
        *plen,
        TxSource_remaining(pself),
        (u_long) ps->pss->bytes_remaining);
    return status;
}

static SANE_Status RGBRouter_done (Source *pself)
{
    RGBRouter *ps = (RGBRouter *) pself;
    SANE_Status status = TxSource_done(pself);

    free(ps->cbuf);
    free(ps->xbuf);
    ps->cbuf = NULL;
    ps->cb_start = -1;
    ps->pos = 0;
    return status;
}

static SANE_Status RGBRouter_init (RGBRouter *pself,
                           SnapScan_Scanner *pss,
                                   Source *psub)
{
    SANE_Status status = TxSource_init((TxSource *) pself,
                                       pss,
                                       RGBRouter_remaining,
                                       TxSource_bytesPerLine,
                                       TxSource_pixelsPerLine,
                                       RGBRouter_get,
                                       RGBRouter_done,
                                       psub);
    if (status == SANE_STATUS_GOOD)
    {
        SANE_Int lines_in_buffer = 0;

        /* Size the buffer to accomodate the necessary number of scan
           lines to cater for the offset between R, G and B */
        lines_in_buffer = pss->chroma + 1;
        pself->cb_line_size = pself->bytesPerLine((Source *) pself);
        pself->cb_size = pself->cb_line_size*lines_in_buffer;
        pself->pos = pself->cb_line_size;

        pself->round_req = pself->cb_size;
        pself->round_read = 0;

        pself->cbuf = (SANE_Byte *) malloc(pself->cb_size);
        pself->xbuf = (SANE_Byte *) malloc(pself->cb_line_size);
        if (pself->cbuf == NULL  ||  pself->xbuf == NULL)
        {
            DBG (DL_MAJOR_ERROR,
                 "%s: failed to allocate circular buffer.\n",
                 __FUNCTION__);
            status = SANE_STATUS_NO_MEM;
        }
        else
        {
            SANE_Int ch;

            pself->cb_start = 0;
            for (ch = 0;  ch < 3;  ch++)
            {
                pself->ch_offset[ch] =
                        pss->chroma_offset[ch] * pself->cb_line_size
                        + ch * (pself->cb_line_size / 3);
            }
        }
        DBG(DL_MINOR_INFO, "RGBRouter_init: buf_size: %d x %d = %d\n",
            pself->cb_line_size, lines_in_buffer, pself->cb_size);
        DBG(DL_MINOR_INFO, "RGBRouter_init: buf offset R:%d G:%d B:%d\n",
            pself->ch_offset[0], pself->ch_offset[1],pself->ch_offset[2]);
    }
    return status;
}

static SANE_Status create_RGBRouter (SnapScan_Scanner *pss,
                                     Source *psub,
                                     Source **pps)
{
    static char me[] = "create_RGBRouter";
    SANE_Status status = SANE_STATUS_GOOD;

    DBG (DL_CALL_TRACE, "%s\n", me);
    *pps = (Source *) malloc(sizeof(RGBRouter));
    if (*pps == NULL)
    {
        DBG (DL_MAJOR_ERROR, "%s: failed to allocate RGBRouter.\n",
             __FUNCTION__);
        status = SANE_STATUS_NO_MEM;
    }
    else
    {
        status = RGBRouter_init ((RGBRouter *) *pps, pss, psub);
    }
    return status;
}

/* An Inverter is used to invert the bits in a lineart image */

typedef struct
{
    TX_SOURCE_GUTS;
} Inverter;

static SANE_Status Inverter_get (Source *pself, SANE_Byte *pbuf, SANE_Int *plen)
{
    SANE_Status status = TxSource_get (pself, pbuf, plen);
    if (status == SANE_STATUS_GOOD)
    {
        int i;
        for (i = 0;  i < *plen;  i++)
            pbuf[i] ^= 0xFF;
    }
    return status;
}

static SANE_Status Inverter_init (Inverter *pself,
                                  SnapScan_Scanner *pss,
                                  Source *psub)
{
    return  TxSource_init ((TxSource *) pself,
                           pss,
                           TxSource_remaining,
                           TxSource_bytesPerLine,
                           TxSource_pixelsPerLine,
                           Inverter_get,
                           TxSource_done,
                           psub);
}

static SANE_Status create_Inverter (SnapScan_Scanner *pss,
                                    Source *psub,
                                    Source **pps)
{
    SANE_Status status = SANE_STATUS_GOOD;
    *pps = (Source *) malloc(sizeof(Inverter));
    if (*pps == NULL)
    {
        DBG (DL_MAJOR_ERROR, "%s: failed to allocate Inverter.\n",
                 __FUNCTION__);
        status = SANE_STATUS_NO_MEM;
    }
    else
    {
        status = Inverter_init ((Inverter *) *pps, pss, psub);
    }
    return status;
}

/* Source chain creation */

static SANE_Status create_source_chain (SnapScan_Scanner *pss,
                                        BaseSourceType bst,
                                        Source **pps)
{
   static char me[] = "create_source_chain";
    SANE_Status status = create_base_source (pss, bst, pps);

   DBG (DL_CALL_TRACE, "%s\n", me);
    if (status == SANE_STATUS_GOOD)
    {
        SnapScan_Mode mode = actual_mode(pss);
        switch (mode)
        {
        case MD_COLOUR:
            status = create_RGBRouter (pss, *pps, pps);
            /* We only have the interlace probelms on 
               some scanners like the Epson Perfection 2480/2580 
               at 2400 dpi. */
            if (status == SANE_STATUS_GOOD && 
                ((pss->pdev->model == PERFECTION2480 && pss->res == 2400) ||
                (pss->pdev->model == PERFECTION3490 && pss->res == 3200) ||
                (pss->pdev->model == PRISA5000E && pss->res == 1200)))
                status = create_Deinterlacer (pss, *pps, pps);
            break;
        case MD_BILEVELCOLOUR:
            status = create_Expander (pss, *pps, pps);
            if (status == SANE_STATUS_GOOD)
                status = create_RGBRouter (pss, *pps, pps);
            if (status == SANE_STATUS_GOOD && 
                ((pss->pdev->model == PERFECTION2480 && pss->res == 2400) ||
                (pss->pdev->model == PERFECTION3490 && pss->res == 3200) ||
                (pss->pdev->model == PRISA5000E && pss->res == 1200)))
                status = create_Deinterlacer (pss, *pps, pps);
            break;
        case MD_GREYSCALE:
            if ((pss->pdev->model == PERFECTION2480 && pss->res == 2400) ||
                (pss->pdev->model == PERFECTION3490 && pss->res == 3200) ||
                (pss->pdev->model == PRISA5000E && pss->res == 1200))
                status = create_Deinterlacer (pss, *pps, pps);
            break;
        case MD_LINEART:
            /* The SnapScan creates a negative image by
               default... so for the user interface to make sense,
               the internal meaning of "negative" is reversed */
            if (pss->negative == SANE_FALSE)
                status = create_Inverter (pss, *pps, pps);
            if (pss->pdev->model == PERFECTION3490 && pss->res == 3200)
                status = create_Deinterlacer (pss, *pps, pps);
            break;
        default:
            DBG (DL_MAJOR_ERROR, "%s: bad mode value %d (internal error)\n",
                 __FUNCTION__, mode);
            status = SANE_STATUS_INVAL;
            break;
        }
    }
    return status;
}

/*
 * $Log$
 * Revision 1.21  2005/12/02 19:12:54  oliver-guest
 * Another fix for lineart mode for the Epson 3490 @ 3200 DPI
 *
 * Revision 1.20  2005/11/28 19:28:29  oliver-guest
 * Fix for lineart mode of Epson 3490 @ 3200 DPI
 *
 * Revision 1.19  2005/11/25 17:24:48  oliver-guest
 * Fix for Epson 3490 @ 3200 DPI for grayscale and lineart mode
 *
 * Revision 1.18  2005/11/17 23:47:11  oliver-guest
 * Revert previous 'fix', disable 2400 dpi for Epson 3490, use 1600 dpi instead
 *
 * Revision 1.17  2005/11/17 23:32:23  oliver-guest
 * Fixes for Epson 3490 @ 2400 DPI
 *
 * Revision 1.16  2005/11/10 19:42:02  oliver-guest
 * Added deinterlacing for Epson 3490
 *
 * Revision 1.15  2005/10/31 21:08:47  oliver-guest
 * Distinguish between Benq 5000/5000E/5000U
 *
 * Revision 1.14  2005/10/13 22:43:30  oliver-guest
 * Fixes for 16 bit scan mode from Simon Munton
 *
 * Revision 1.13  2005/10/11 18:47:07  oliver-guest
 * Fixes for Epson 3490 and 16 bit scan mode
 *
 * Revision 1.12  2004/11/14 19:26:38  oliver-guest
 * Applied patch from Julien Blache to change ch_past_init from SANE_Int to SANE_Bool
 *
 * Revision 1.11  2004/11/09 23:17:38  oliver-guest
 * First implementation of deinterlacer for Epson scanners at high resolutions (thanks to Brad Johnson)
 *
 * Revision 1.10  2004/10/03 17:34:36  hmg-guest
 * 64 bit platform fixes (bug #300799).
 *
 * Revision 1.9  2004/04/09 16:18:37  oliver-guest
 * Fix initialization of FDSource.bytes_remaining
 *
 * Revision 1.8  2004/04/09 11:59:02  oliver-guest
 * Fixes for pthread implementation
 *
 * Revision 1.7  2004/04/08 21:53:10  oliver-guest
 * Use sanei_thread in snapscan backend
 *
 * Revision 1.6  2001/12/17 22:51:49  oliverschwartz
 * Update to snapscan-20011212 (snapscan 1.4.3)
 *
 * Revision 1.18  2001/12/12 19:44:59  oliverschwartz
 * Clean up CVS log
 *
 * Revision 1.17  2001/11/27 23:16:17  oliverschwartz
 * - Fix color alignment for SnapScan 600
 * - Added documentation in snapscan-sources.c
 * - Guard against TL_X < BR_X and TL_Y < BR_Y
 *
 * Revision 1.16  2001/10/08 18:22:02  oliverschwartz
 * - Disable quality calibration for Acer Vuego 310F
 * - Use sanei_scsi_max_request_size as scanner buffer size
 *   for SCSI devices
 * - Added new devices to snapscan.desc
 *
 * Revision 1.15  2001/09/28 15:56:51  oliverschwartz
 * - fix hanging for SNAPSCAN300 / VUEGO 310
 *
 * Revision 1.14  2001/09/28 13:39:16  oliverschwartz
 * - Added "Snapscan 300" ID string
 * - cleanup
 * - more debugging messages in snapscan-sources.c
 *
 * Revision 1.13  2001/09/18 15:01:07  oliverschwartz
 * - Read scanner id string again after firmware upload
 *   to indentify correct model
 * - Make firmware upload work for AGFA scanners
 * - Change copyright notice
 *
 * Revision 1.12  2001/09/09 18:06:32  oliverschwartz
 * add changes from Acer (new models; automatic firmware upload for USB scanners); fix distorted colour scans after greyscale scans (call set_window only in sane_start); code cleanup
 *
 * Revision 1.11  2001/04/13 13:12:18  oliverschwartz
 * use absolute_max as expected_read_bytes for PRISA620S
 *
 * Revision 1.10  2001/04/10 11:04:31  sable
 * Adding support for snapscan e40 an e50 thanks to Giuseppe Tanzilli
 *
 * Revision 1.9  2001/03/17 22:53:21  sable
 * Applying Mikael Magnusson patch concerning Gamma correction
 * Support for 1212U_2
 *
 * Revision 1.8  2000/11/28 03:55:07  cbagwell
 * Reverting a fix to RGBRouter_remaining to original fix.  This allows
 * most scanners to scan at 600 dpi by ignoring insufficent data in
 * the RGB circular buffer and always returning size = 1 in those cases.
 * This should probably be fixed at a higher level.
 *
 * Revision 1.7  2000/11/20 01:02:42  cbagwell
 * Updates so that USB will continue reading when it receives an EAGAIN error.
 * Also, changed RGBRouter_remaining to not be able to return a negative
 * value.
 *
 * Revision 1.6  2000/11/04 01:53:58  cbagwell
 * Commiting some needed USB updates.  Added extra test logic to detect
 * bad bytes_expected values.  Just to help debug faster on scanners
 * that tickle the bug.
 *
 * Revision 1.5  2000/10/30 22:32:20  sable
 * Support for vuego310s vuego610s and 1236s
 *
 * Revision 1.4  2000/10/28 14:16:10  sable
 * Bug correction for SnapScan310
 *
 * Revision 1.3  2000/10/28 14:06:35  sable
 * Add support for Acer300f
 *
 * Revision 1.2  2000/10/13 03:50:27  cbagwell
 * Updating to source from SANE 1.0.3.  Calling this versin 1.1
 * */
