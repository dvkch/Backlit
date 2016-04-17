/* sane - Scanner Access Now Easy.

   Copyright (C) 2000 Kazuya Fukuda

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
   If you do not wish that, delete this exception notice. */

#ifndef nec_h
#define nec_h 1

#include <sys/types.h>

/* default values for configurable options.
   Though these options are only meaningful if USE_FORK is defined,
   they are 
   DEFAULT_BUFFERS:      number of buffers allocated as shared memory
                         for the data transfer from reader_process to
                         read_data. The minimum value is 2
   DEFAULT_BUFSIZE:      default size of one buffer. Must be greater
                         than zero.
   DEFAULT_QUEUED_READS: number of read requests queued by 
                         sanei_scsi_req_enter. Since queued read requests
                         are currently only supported for Linux and 
                         DomainOS, this value should automatically be set
                         dependent on the target OS...
                         For Linux, 2 is the optimum; for DomainOS, I
                         don't have any recommendation; other OS
                         should use the value zero.
   
   The value for DEFAULT_BUFSIZE is probably too Linux-oriented...
*/

#define DEFAULT_BUFFERS 12
#define DEFAULT_BUFSIZE 128 * 1024
#define DEFAULT_QUEUED_READS 2

#define NEC_MAJOR	0
#define NEC_MINOR	12

typedef enum
  {
    OPT_NUM_OPTS = 0,

    OPT_MODE_GROUP,
    OPT_MODE,
    OPT_HALFTONE,
    OPT_PAPER,
    OPT_SCANSOURCE,
    OPT_GAMMA,
#ifdef USE_CUSTOM_GAMMA
    OPT_CUSTOM_GAMMA,
#endif
    OPT_RESOLUTION_GROUP,
#ifdef USE_RESOLUTION_LIST
    OPT_RESOLUTION_LIST,
#endif
    OPT_RESOLUTION,

    OPT_GEOMETRY_GROUP,
    OPT_TL_X,			/* top-left x */
    OPT_TL_Y,			/* top-left y */
    OPT_BR_X,			/* bottom-right x */
    OPT_BR_Y,			/* bottom-right y */

    OPT_ENHANCEMENT_GROUP,
    OPT_EDGE_EMPHASIS,
    OPT_OR,
    OPT_NR,
    OPT_EDGE,
    OPT_THRESHOLD,
#ifdef USE_COLOR_THRESHOLD
    OPT_THRESHOLD_R,
    OPT_THRESHOLD_G,
    OPT_THRESHOLD_B,
#endif
    OPT_LIGHTCOLOR,
    OPT_TINT,
    OPT_COLOR,
    OPT_PREVIEW,

#ifdef USE_CUSTOM_GAMMA 
    OPT_GAMMA_VECTOR,
    OPT_GAMMA_VECTOR_R,
    OPT_GAMMA_VECTOR_G,
    OPT_GAMMA_VECTOR_B,
#endif
    /* must come last: */
    NUM_OPTIONS
  }
NEC_Option;

#ifdef USE_FORK

/* status defines for a buffer: 
   buffer not used / read request queued / buffer contains data
*/
#define SHM_EMPTY 0
#define SHM_BUSY  1
#define SHM_FULL  2
typedef struct NEC_shmem_ctl
  {
    int shm_status;   /* can be SHM_EMPTY, SHM_BUSY, SHM_FULL */
    size_t used;      /* number of bytes successfully read from scanner */
    size_t nreq;      /* number of bytes requested from scanner */
    size_t start;    /* index of the begin of used area of the buffer */
    void *qid;
    SANE_Byte *buffer;
  }
NEC_shmem_ctl;

typedef struct NEC_rdr_ctl
  {
    int cancel;      /* 1 = flag for the reader process to cancel */
    int running; /* 1 indicates that the reader process is alive */
    SANE_Status status; /* return status of the reader process */
    NEC_shmem_ctl *buf_ctl;
  }
NEC_rdr_ctl;
#endif /* USE_FORK */

typedef enum 
  {
    /* PCIN500, PCINXXX are used as array indices, so the corresponding
       numbers should start at 0
    */
    unknown = -1,
    PCIN500,
    PCINXXX
  }
NEC_Model;

typedef struct NEC_Info
  {
    SANE_Range res_range;
    SANE_Range tl_x_ranges[3]; /* normal / FSU / ADF */
    SANE_Range br_x_ranges[3]; /* normal / FSU / ADF */
    SANE_Range tl_y_ranges[3]; /* normal / FSU / ADF */
    SANE_Range br_y_ranges[3]; /* normal / FSU / ADF */
    SANE_Range threshold_range;
    SANE_Range tint_range;
    SANE_Range color_range;

    SANE_Int res_default;
    SANE_Int x_default;
    SANE_Int y_default;
    SANE_Int bmu;
    SANE_Int mud;
    SANE_Int adf_fsu_installed;
    SANE_String_Const scansources[5];
    size_t buffers;
    size_t bufsize;
    int wanted_bufsize;
    size_t queued_reads;
  }
NEC_Info;

typedef struct NEC_Sense_Data
  {
    NEC_Model model;
    /* flag, if conditions like "paper jam" or "cover open" 
       are considered as an error. Should be 0 for attach, else
       a frontend might refuse to start, if the scanner returns
       these errors.
    */
    int complain_on_adf_error;
    /* Linux returns only 16 bytes of sense data... */
    u_char sb[16]; 
  }
NEC_Sense_Data;

typedef struct NEC_Device
  {
    struct NEC_Device *next;
    SANE_Device sane;
    NEC_Info info;
    /* xxx now part of sense data NEC_Model model; */
    NEC_Sense_Data sensedat;
  }
NEC_Device;

typedef struct NEC_New_Device 
  {
    struct NEC_Device *dev;
    struct NEC_New_Device *next;
  }
NEC_New_Device;

typedef struct NEC_Scanner
  {
    struct NEC_Scanner *next;
    int fd;
    NEC_Device *dev;
    SANE_Option_Descriptor opt[NUM_OPTIONS];
    Option_Value val[NUM_OPTIONS];
    SANE_Parameters params;

    int    get_params_called;
    SANE_Byte *buffer;    /* for color data re-ordering */
    SANE_Int buf_used;
    SANE_Int buf_pos;
    SANE_Int modes;
    SANE_Int res;
    SANE_Int ulx;
    SANE_Int uly;
    SANE_Int width;
    SANE_Int length;
    SANE_Int threshold;
    SANE_Int image_composition;
    SANE_Int bpp;
    SANE_Int halftone;
    SANE_Bool reverse;
    SANE_Bool or;
    SANE_Bool nr;
    SANE_Int gamma;
    SANE_Int edge;
    SANE_Int lightcolor;
    SANE_Int adf_fsu_mode; /* mode selected by user */
    SANE_Int adf_scan; /* flag, if the actual scan is an ADF scan */

    SANE_Int tint;
    SANE_Int color;

    size_t bytes_to_read;
    size_t max_lines_to_read;
    size_t unscanned_lines;
    SANE_Bool scanning;
    SANE_Bool busy;
    SANE_Bool cancel;
#ifdef USE_CUSTOM_GAMMA
    SANE_Int gamma_table[4][256];
#endif
#ifdef USE_FORK
    pid_t reader_pid;
    NEC_rdr_ctl   *rdr_ctl;
    int shmid;
    size_t read_buff; /* index of the buffer actually used by read_data */
#endif /* USE_FORK */
  }
NEC_Scanner;

typedef struct NEC_Send
{
    SANE_Int dtc;
    SANE_Int dtq;
    SANE_Int length;
    SANE_Byte *data;
}
NEC_Send;

typedef struct WPDH
{
    u_char wpdh[6];
    u_char wdl[2];
} 
WPDH;

typedef struct WDB
{
    SANE_Byte wid;
    SANE_Byte autobit;
    SANE_Byte x_res[2];
    SANE_Byte y_res[2];

    SANE_Byte x_ul[4];
    SANE_Byte y_ul[4];
    SANE_Byte width[4];
    SANE_Byte length[4];

    SANE_Byte brightness;
    SANE_Byte threshold;
    SANE_Byte contrast;
    SANE_Byte image_composition;
    SANE_Byte bpp;

    SANE_Byte ht_pattern[2];
    SANE_Byte rif_padding;
    SANE_Byte bit_ordering[2];
    SANE_Byte compression_type;
    SANE_Byte compression_argument;
    SANE_Byte reserved[6];
}
WDB;

/* "extension" of the window descriptor block for the PC-IN500 */
typedef struct XWDBX500
  {
    SANE_Byte data_length;
    SANE_Byte control;
    SANE_Byte format;
    SANE_Byte gamma;
    SANE_Byte tint;
    SANE_Byte color;
    SANE_Byte reserved1;
    SANE_Byte reserved2;
  }
WDBX500;

typedef struct window_param
{
    WPDH wpdh;
    WDB wdb;
    WDBX500 wdbx500;
}
window_param;

typedef struct mode_sense_param
{
    SANE_Byte mode_data_length;
    SANE_Byte mode_param_header2;
    SANE_Byte mode_param_header3;
    SANE_Byte mode_desciptor_length;
    SANE_Byte page_code;
    SANE_Byte page_length; /* 6 */
    SANE_Byte bmu;
    SANE_Byte res2;
    SANE_Byte mud[2];
    SANE_Byte res3;
    SANE_Byte res4;
}
mode_sense_param;

typedef struct mode_sense_subdevice
{
    SANE_Byte mode_data_length;
    SANE_Byte mode_param_header2;
    SANE_Byte mode_param_header3;
    SANE_Byte mode_desciptor_length;
    SANE_Byte res1[5];
    SANE_Byte blocklength[3];
    SANE_Byte page_code;
    SANE_Byte page_length; /* 0x1a */
    SANE_Byte a_mode_type;
    SANE_Byte f_mode_type;
    SANE_Byte res2;
    SANE_Byte max_x[4];
    SANE_Byte max_y[4];
    SANE_Byte res3[2];
    SANE_Byte x_basic_resolution[2];
    SANE_Byte y_basic_resolution[2];
    SANE_Byte x_max_resolution[2];
    SANE_Byte y_max_resolution[2];
    SANE_Byte x_min_resolution[2];
    SANE_Byte y_min_resolution[2];
    SANE_Byte res4;
}
mode_sense_subdevice;

typedef struct mode_select_param
{
    SANE_Byte mode_param_header1;
    SANE_Byte mode_param_header2;
    SANE_Byte mode_param_header3;
    SANE_Byte mode_param_header4;
    SANE_Byte page_code;
    SANE_Byte page_length; /* 6 */
    SANE_Byte res1;
    SANE_Byte res2;
    SANE_Byte mud[2];
    SANE_Byte res3;
    SANE_Byte res4;
}
mode_select_param;

typedef struct mode_select_subdevice
{
    SANE_Byte mode_param_header1;
    SANE_Byte mode_param_header2;
    SANE_Byte mode_param_header3;
    SANE_Byte mode_param_header4;
    SANE_Byte page_code;
    SANE_Byte page_length; /*  0x1A */
    SANE_Byte a_mode;
    SANE_Byte f_mode;
    SANE_Byte res[24];
}
mode_select_subdevice;

typedef struct buffer_status
{
    SANE_Byte data_length[3];
    SANE_Byte block;
    SANE_Byte window_id;
    SANE_Byte reserved;
    SANE_Byte bsa[3];      /* buffer space available */ 
    SANE_Byte fdb[3];      /* filled data buffer */
}
buffer_status;

/* SCSI commands */
#define TEST_UNIT_READY        0x00
#define REQUEST_SENSE          0x03
#define INQUIRY                0x12
#define MODE_SELECT6           0x15
#define RESERVE_UNIT           0x16
#define RELEASE_UNIT           0x17
#define MODE_SENSE6            0x1a
#define SCAN                   0x1b
#define SEND_DIAGNOSTIC        0x1d
#define SET_WINDOW             0x24
#define GET_WINDOW             0x25
#define READ                   0x28
#define SEND                   0x2a
#define GET_DATA_BUFFER_STATUS 0x34

#define SENSE_LEN              18
#define INQUIRY_LEN            36
#define MODEPARAM_LEN          12
#define MODE_SUBDEV_LEN        32
#define WINDOW_LEN             76
#define BUFFERSTATUS_LEN       12

#endif /* not nec_h */
