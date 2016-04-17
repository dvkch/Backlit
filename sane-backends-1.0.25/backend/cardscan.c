/* sane - Scanner Access Now Easy.

   This file is part of the SANE package, and implements a SANE backend
   for various Corex Cardscan scanners.

   Copyright (C) 2007-2010 m. allan noah

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

   --------------------------------------------------------------------------

   This file implements a SANE backend for the Corex Cardscan 800C

   The source code is divided in sections which you can easily find by
   searching for the tag "@@".

   Section 1 - Init & static stuff
   Section 2 - sane_init, _get_devices, _open & friends
   Section 3 - sane_*_option functions
   Section 4 - sane_start, _get_param, _read & friends
   Section 5 - sane_close functions
   Section 6 - misc functions

   Changes:
      v0, 2007-05-09, MAN (SANE v1.0.19)
        - initial release
      v1, 2008-02-14, MAN
	- sanei_config_read has already cleaned string (#310597)
      v2, 2010-02-10, MAN
	- add lines_per_block config option
	- add has_cal_buffer config option
	- basic support for 600c
        - clean #include lines

##################################################
   DATA FROM TRACE OF WINDOWS DRIVER:

cmd packet format:
cmdcode cmdlenlow cmdlenhigh cmdpayloadbytes

resp packet format:
respcode paperfound resplenlow resplenhigh respayloadbytes

############ status read loop? ##################
>> 01 01 00 00
<< 81 00 07 00 00 09 0c 61 c2 7a 0a
>> 34 00 00
<< b4 00 00 00
>> 01 01 00 00
<< 81 00 07 00 00 09 0c 61 c2 7a 0a
>> 34 00 00
<< b4 00 00 00
>> 01 01 00 00
<< 81 00 07 00 00 09 0c 61 c2 7a 0a

############# scanner settings read? (0x04b8 is scan width) #############
>> 48 00 00
<< c8 00 0c 00 b8 04 60 00 00 80 00 00 00 58 ca 7d

############## color and gray calibration data read ############
>> 45 00 00
<< 0x2600 bytes, bbbBBBgggGGGrrrRRRxxxXXX

############ 34/b4 and 01/81 status loop til paper inserted ##############

>> 35 01 00 00
<< b5 01 01 00 00

always together? {
>> 14 05 00 80 1b 28 00 0f
<< 94 01 05 00 80 1b 28 00 0f
>> 22 01 00 00
<< a2 01 01 00 00
}

>> 1a 01 00 66
<< 9a 01 01 00 66

>> 19 03 00 51 62 49
<< 99 01 03 00 51 62 49

############# heat up lamp? #################
===========color===================
three times {
>> 18 07 00 00 01 60 00 61 00 07
<< 0x40 read and 0x03 read
the 3 byte drops from f4 f4 f4 to 17 10 08 etc.
}
===========gray===================
three times {
>> 12 06 00 00 01 60 00 61 00
<< 0x40 read and 0x01 read
}
the 1 byte drops from f4 to 02
==================================

>> 35 01 00 00
<< b5 01 01 00 00

>> 13 01 00 28
<< 93 01 01 00 28

===========color===================
three times {
>> 18 07 00 01 10 60 00 18 05 07
<< 0xe2c0 read
}

14/94 and 22/a2

many times {
>> 18 07 00 01 10 60 00 18 05 07
<< 0xe2c0 read
}
===========gray===================
two times {
>> 12 06 00 01 10 60 00 18 05
<< 0x4bc0 read
}

14/94 and 22/a2

many times {
>> 12 06 00 01 10 60 00 18 05
<< 0x4bc0 read
}
==================================

>> 35 01 00 ff
<< b5 00 01 00 ff

14/94 and 22/a2

########### discarge capacitor? ###########
four times {
>> 21 02 00 0a 00
<< a1 00 02 00 0a 00
}

>> 01 01 00 00
<< 81 00 07 00 00 09 0c 61 c2 7a 0a

>> 35 01 00 ff
<< b5 00 01 00 ff

>> 34 00 00
<< b4 00 00 00
#############################################

   SANE FLOW DIAGRAM

   - sane_init() : initialize backend
   . - sane_get_devices() : query list of scanner devices
   . - sane_open() : open a particular scanner device
   . . - sane_set_io_mode : set blocking mode
   . . - sane_get_select_fd : get scanner fd
   . .
   . . - sane_get_option_descriptor() : get option information
   . . - sane_control_option() : change option values
   . . - sane_get_parameters() : returns estimated scan parameters
   . . - (repeat previous 3 functions)
   . .
   . . - sane_start() : start image acquisition
   . .   - sane_get_parameters() : returns actual scan parameters
   . .   - sane_read() : read image data (from pipe)
   . . (sane_read called multiple times; after sane_read returns EOF, 
   . . loop may continue with sane_start which may return a 2nd page
   . . when doing duplex scans, or load the next page from the ADF)
   . .
   . . - sane_cancel() : cancel operation
   . - sane_close() : close opened scanner device
   - sane_exit() : terminate use of backend

*/

/*
 * @@ Section 1 - Init
 */

#include "../include/sane/config.h"

#include <string.h> /*memcpy...*/
#include <ctype.h> /*isspace*/

#include "../include/sane/sanei_backend.h"
#include "../include/sane/sanei_usb.h"
#include "../include/sane/saneopts.h"
#include "../include/sane/sanei_config.h"

#include "cardscan.h"

#define DEBUG 1
#define BUILD 2 

/* values for SANE_DEBUG_CARDSCAN env var:
 - errors           5
 - function trace  10
 - function detail 15
 - get/setopt cmds 20
 - usb cmd trace   25 
 - usb cmd detail  30
 - useless noise   35
*/

int global_has_cal_buffer = 1;
int global_lines_per_block = 16;

/* ------------------------------------------------------------------------- */
#define STRING_GRAYSCALE SANE_VALUE_SCAN_MODE_GRAY
#define STRING_COLOR SANE_VALUE_SCAN_MODE_COLOR

/*
 * used by attach* and sane_get_devices
 * a ptr to a null term array of ptrs to SANE_Device structs
 * a ptr to a single-linked list of scanner structs
 */
static const SANE_Device **sane_devArray = NULL;
static struct scanner *scanner_devList = NULL;

/*
 * @@ Section 2 - SANE & scanner init code
 */

/*
 * Called by SANE initially.
 * 
 * From the SANE spec:
 * This function must be called before any other SANE function can be
 * called. The behavior of a SANE backend is undefined if this
 * function is not called first. The version code of the backend is
 * returned in the value pointed to by version_code. If that pointer
 * is NULL, no version code is returned. Argument authorize is either
 * a pointer to a function that is invoked when the backend requires
 * authentication for a specific resource or NULL if the frontend does
 * not support authentication.
 */
SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
    authorize = authorize;        /* get rid of compiler warning */
  
    DBG_INIT ();
    DBG (10, "sane_init: start\n");
  
    sanei_usb_init();
  
    if (version_code)
      *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, BUILD);
  
    DBG (5, "sane_init: cardscan backend %d.%d.%d, from %s\n",
      SANE_CURRENT_MAJOR, V_MINOR, BUILD, PACKAGE_STRING);
  
    DBG (10, "sane_init: finish\n");
  
    return SANE_STATUS_GOOD;
}

/*
 * Called by SANE to find out about supported devices.
 * 
 * From the SANE spec:
 * This function can be used to query the list of devices that are
 * available. If the function executes successfully, it stores a
 * pointer to a NULL terminated array of pointers to SANE_Device
 * structures in *device_list. The returned list is guaranteed to
 * remain unchanged and valid until (a) another call to this function
 * is performed or (b) a call to sane_exit() is performed. This
 * function can be called repeatedly to detect when new devices become
 * available. If argument local_only is true, only local devices are
 * returned (devices directly attached to the machine that SANE is
 * running on). If it is false, the device list includes all remote
 * devices that are accessible to the SANE library.
 * 
 * SANE does not require that this function is called before a
 * sane_open() call is performed. A device name may be specified
 * explicitly by a user which would make it unnecessary and
 * undesirable to call this function first.
 *
 * Read the config file, find scanners with help from sanei_*
 * store in global device structs
 */
SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
    struct scanner *dev;
    char line[PATH_MAX];
    const char *lp;
    FILE *fp;
    int num_devices=0;
    int i=0;
  
    local_only = local_only;        /* get rid of compiler warning */
  
    DBG (10, "sane_get_devices: start\n");
 
    global_has_cal_buffer = 1;
    global_lines_per_block = 16;

    fp = sanei_config_open (CONFIG_FILE);
  
    if (fp) {
  
        DBG (15, "sane_get_devices: reading config file %s\n", CONFIG_FILE);
  
        while (sanei_config_read (line, PATH_MAX, fp)) {
      
            lp = line;

            /* ignore comments */
            if (*lp == '#')
                continue;
      
            /* skip empty lines */
            if (*lp == 0)
                continue;
      
            if ((strncmp ("usb", lp, 3) == 0) && isspace (lp[3])) {
                DBG (15, "sane_get_devices: looking for '%s'\n", lp);
                sanei_usb_attach_matching_devices(lp, attach_one);
            }

            else if (!strncmp(lp, "has_cal_buffer", 14) && isspace (lp[14])) {
    
                int buf;
                lp += 14;
                lp = sanei_config_skip_whitespace (lp);
                buf = atoi (lp);
    
                if(buf){
                  global_has_cal_buffer = 1;
                }
                else{
                  global_has_cal_buffer = 0;
                }
    
                DBG (15, "sane_get_devices: setting \"has_cal_buffer\" to %d\n",
                  global_has_cal_buffer);
            }

            else if (!strncmp(lp, "lines_per_block", 15) && isspace (lp[15])) {
    
                int buf;
                lp += 15;
                lp = sanei_config_skip_whitespace (lp);
                buf = atoi (lp);
    
                if(buf < 1 || buf > 32){
                  DBG (15, 
                    "sane_get_devices: \"lines_per_block\"=%d\n out of range",
                    buf
                  );
                  continue;
                }

                DBG (15, "sane_get_devices: \"lines_per_block\" is %d\n", buf);
                global_lines_per_block = buf;
            }

            else{
                DBG (5, "sane_get_devices: config line \"%s\" ignored.\n", lp);
            }
        }
        fclose (fp);
    }
  
    else {
        DBG (5, "sane_get_devices: no config file '%s', using defaults\n",
          CONFIG_FILE);
  
        DBG (15, "sane_get_devices: looking for 'usb 0x08F0 0x0005'\n");
        sanei_usb_attach_matching_devices("usb 0x08F0 0x0005", attach_one);
    }
  
    for (dev = scanner_devList; dev; dev=dev->next) {
        DBG (15, "sane_get_devices: found scanner %s\n",dev->device_name);
        num_devices++;
    }
  
    DBG (15, "sane_get_devices: found %d scanner(s)\n",num_devices);
  
    sane_devArray = calloc (num_devices + 1, sizeof (SANE_Device*));
    if (!sane_devArray)
        return SANE_STATUS_NO_MEM;
  
    for (dev = scanner_devList; dev; dev=dev->next) {
        sane_devArray[i++] = (SANE_Device *)&dev->sane;
    }
  
    sane_devArray[i] = 0;
  
    *device_list = sane_devArray;
  
    DBG (10, "sane_get_devices: finish\n");
  
    return SANE_STATUS_GOOD;
}

/* callback used by sane_get_devices
 * build the scanner struct and link to global list 
 * unless struct is already loaded, then pretend 
 */
static SANE_Status
attach_one (const char *device_name)
{
    struct scanner *s;
    int ret, i;
    SANE_Word vid, pid;
  
    DBG (10, "attach_one: start '%s'\n", device_name);
  
    for (s = scanner_devList; s; s = s->next) {
        if (strcmp (s->sane.name, device_name) == 0) {
            DBG (10, "attach_one: already attached!\n");
            return SANE_STATUS_GOOD;
        }
    }
  
    /* build a scanner struct to hold it */
    DBG (15, "attach_one: init struct\n");
  
    if ((s = calloc (sizeof (*s), 1)) == NULL)
        return SANE_STATUS_NO_MEM;
  
    /* copy the device name */
    s->device_name = strdup (device_name);
    if (!s->device_name){
        free (s);
        return SANE_STATUS_NO_MEM;
    }
  
    /* connect the fd */
    DBG (15, "attach_one: connect fd\n");
  
    s->fd = -1;
    ret = connect_fd(s);
    if(ret != SANE_STATUS_GOOD){
        free (s->device_name);
        free (s);
        return ret;
    }
  
    /* clean up the scanner struct based on model */
    /* this is the only piece of model specific code */
    sanei_usb_get_vendor_product(s->fd,&vid,&pid);
  
    if(vid == 0x08f0){
        s->vendor_name = "CardScan";
        if(pid == 0x0005){
            s->product_name = "800c";
        }
        else if(pid == 0x0002){
            s->product_name = "600c";
        }
        else{
            DBG (5, "Unknown product, using default settings\n");
            s->product_name = "Unknown";
        }
    }
    else{
        DBG (5, "Unknown vendor/product, using default settings\n");
        s->vendor_name = "Unknown";
        s->product_name = "Unknown";
    }
  
    DBG (15, "attach_one: Found %s scanner %s at %s\n",
      s->vendor_name, s->product_name, s->device_name);
 
    /*copy config file settings*/
    s->has_cal_buffer = global_has_cal_buffer;
    s->lines_per_block = global_lines_per_block;
    s->color_block_size = s->lines_per_block * PIXELS_PER_LINE * 3;
    s->gray_block_size = s->lines_per_block * PIXELS_PER_LINE;

    /* try to get calibration */
    if(s->has_cal_buffer){
      DBG (15, "attach_one: scanner calibration\n");
    
      ret = load_calibration(s);
      if (ret != SANE_STATUS_GOOD) {
          DBG (5, "sane_start: ERROR: cannot calibrate, incompatible?\n");
          free (s->device_name);
          free (s);
          return ret;
      }
    }
    else{
      DBG (15, "attach_one: skipping calibration\n");
    }
  
    /* set SANE option 'values' to good defaults */
    DBG (15, "attach_one: init options\n");
  
    /* go ahead and setup the first opt, because 
     * frontend may call control_option on it 
     * before calling get_option_descriptor 
     */
    memset (s->opt, 0, sizeof (s->opt));
    for (i = 0; i < NUM_OPTIONS; ++i) {
        s->opt[i].name = "filler";
        s->opt[i].size = sizeof (SANE_Word);
        s->opt[i].cap = SANE_CAP_INACTIVE;
    }
  
    s->opt[OPT_NUM_OPTS].name = SANE_NAME_NUM_OPTIONS;
    s->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
    s->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
    s->opt[OPT_NUM_OPTS].type = SANE_TYPE_INT;
    s->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
  
    DBG (15, "attach_one: init settings\n");
  
    /* we close the connection, so that another backend can talk to scanner */
    disconnect_fd(s);
  
    /* load info into sane_device struct */
    s->sane.name = s->device_name;
    s->sane.vendor = s->vendor_name;
    s->sane.model = s->product_name;
    s->sane.type = "scanner";
  
    s->next = scanner_devList;
    scanner_devList = s;
  
    DBG (10, "attach_one: finish\n");
  
    return SANE_STATUS_GOOD;
}

/*
 * connect the fd in the scanner struct
 */
static SANE_Status
connect_fd (struct scanner *s)
{
    SANE_Status ret;
  
    DBG (10, "connect_fd: start\n");
  
    if(s->fd > -1){
        DBG (5, "connect_fd: already open\n");
        ret = SANE_STATUS_GOOD;
    }
    else {
        DBG (15, "connect_fd: opening USB device\n");
        ret = sanei_usb_open (s->device_name, &(s->fd));
    }
  
    if(ret != SANE_STATUS_GOOD){
        DBG (5, "connect_fd: could not open device: %d\n", ret);
    }
  
    DBG (10, "connect_fd: finish\n");
  
    return ret;
}

static SANE_Status
load_calibration(struct scanner *s)
{
    SANE_Status ret=SANE_STATUS_GOOD;
    unsigned char cmd[] = {0x45, 0x00, 0x00};
    unsigned char * buf;
    size_t bytes = HEADER_SIZE + CAL_COLOR_SIZE*2 + CAL_GRAY_SIZE*2;
    int j;
  
    DBG (10, "load_calibration: start\n");
  
    buf = malloc(bytes);
    if(!buf){
      DBG(5, "load_calibration: not enough mem for buffer: %ld\n",(long)bytes);
      return SANE_STATUS_NO_MEM;
    }
  
    ret = do_cmd(
      s, 0,
      cmd, sizeof(cmd),
      NULL, 0,
      buf, &bytes
    );
  
    if (ret == SANE_STATUS_GOOD) {
        DBG(15, "load_calibration: got GOOD\n");
    
        /*
         * color cal data comes from scaner like:
         * bbbbbbbBBBBBBBgggggggGGGGGGGrrrrrrrRRRRRRR
         * where b=darkblue, B=lightblue, etc
         * reorder the data into two buffers
         * bbbbbbbgggggggrrrrrrr and BBBBBBBGGGGGGGRRRRRRR
         */
    
        /*dark/light blue*/
        memcpy(s->cal_color_b, buf+HEADER_SIZE, PIXELS_PER_LINE);
        memcpy(s->cal_color_w,
          buf+HEADER_SIZE+PIXELS_PER_LINE, PIXELS_PER_LINE);
    
        /*dark/light green*/
        memcpy(s->cal_color_b+PIXELS_PER_LINE,
          buf+HEADER_SIZE+(PIXELS_PER_LINE*2), PIXELS_PER_LINE);
        memcpy(s->cal_color_w+PIXELS_PER_LINE,
          buf+HEADER_SIZE+(PIXELS_PER_LINE*3), PIXELS_PER_LINE);
    
        /*dark/light red*/
        memcpy(s->cal_color_b+(PIXELS_PER_LINE*2),
          buf+HEADER_SIZE+(PIXELS_PER_LINE*4), PIXELS_PER_LINE);
        memcpy(s->cal_color_w+(PIXELS_PER_LINE*2),
          buf+HEADER_SIZE+(PIXELS_PER_LINE*5), PIXELS_PER_LINE);
    
        /* then slide the light data down using the dark offset */
        for(j=0;j<CAL_COLOR_SIZE;j++){
            s->cal_color_w[j] -= s->cal_color_b[j];
        }
    
        /*dark/light gray*/
        memcpy(s->cal_gray_b,
          buf+HEADER_SIZE+(CAL_COLOR_SIZE*2), PIXELS_PER_LINE);
        memcpy(s->cal_gray_w,
          buf+HEADER_SIZE+(CAL_COLOR_SIZE*2)+PIXELS_PER_LINE, PIXELS_PER_LINE);
    
        /* then slide the light data down using the dark offset */
        for(j=0;j<CAL_GRAY_SIZE;j++){
            s->cal_gray_w[j] -= s->cal_gray_b[j];
        }
    
        hexdump(35, "cal_color_b:", s->cal_color_b, CAL_COLOR_SIZE);
        hexdump(35, "cal_color_w:", s->cal_color_w, CAL_COLOR_SIZE);
        hexdump(35, "cal_gray_b:", s->cal_gray_b, CAL_GRAY_SIZE);
        hexdump(35, "cal_gray_w:", s->cal_gray_w, CAL_GRAY_SIZE);
    }
    else {
        DBG(5, "load_calibration: error reading data block status = %d\n", ret);
    }
  
    DBG (10, "load_calibration: finish\n");
  
    return ret;
}

/*
 * From the SANE spec:
 * This function is used to establish a connection to a particular
 * device. The name of the device to be opened is passed in argument
 * name. If the call completes successfully, a handle for the device
 * is returned in *h. As a special case, specifying a zero-length
 * string as the device requests opening the first available device
 * (if there is such a device).
 */
SANE_Status
sane_open (SANE_String_Const name, SANE_Handle * handle)
{
    struct scanner *dev = NULL;
    struct scanner *s = NULL;
    SANE_Status ret;
   
    DBG (10, "sane_open: start\n");
  
    if(name[0] == 0){
        if(scanner_devList){
            DBG (15, "sane_open: no device requested, using first\n");
            s = scanner_devList;
        }
        else{
            DBG (15, "sane_open: no device requested, none found\n");
        }
    }
    else{
        DBG (15, "sane_open: device %s requested, attaching\n", name);

        ret = attach_one(name);
        if(ret){
            DBG (5, "sane_open: attach error %d\n",ret);
            return ret;
        }

        for (dev = scanner_devList; dev; dev = dev->next) {
            if (strcmp (dev->sane.name, name) == 0) {
                s = dev;
                break;
            }
        }
    }
  
    if (!s) {
        DBG (5, "sane_open: no device found\n");
        return SANE_STATUS_INVAL;
    }
  
    DBG (15, "sane_open: device %s found\n", s->sane.name);
  
    *handle = s;
  
    /* connect the fd so we can talk to scanner */
    ret = connect_fd(s);
    if(ret != SANE_STATUS_GOOD){
        return ret;
    }
  
    DBG (10, "sane_open: finish\n");
  
    return SANE_STATUS_GOOD;
}

/*
 * @@ Section 3 - SANE Options functions
 */

/*
 * Returns the options we know.
 *
 * From the SANE spec:
 * This function is used to access option descriptors. The function
 * returns the option descriptor for option number n of the device
 * represented by handle h. Option number 0 is guaranteed to be a
 * valid option. Its value is an integer that specifies the number of
 * options that are available for device handle h (the count includes
 * option 0). If n is not a valid option index, the function returns
 * NULL. The returned option descriptor is guaranteed to remain valid
 * (and at the returned address) until the device is closed.
 */
const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  struct scanner *s = handle;
  int i;
  SANE_Option_Descriptor *opt = &s->opt[option];

  DBG (20, "sane_get_option_descriptor: %d\n", option);

  if ((unsigned) option >= NUM_OPTIONS)
    return NULL;

  /* "Mode" group -------------------------------------------------------- */
  if(option==OPT_MODE_GROUP){
    opt->title = "Scan Mode";
    opt->desc = "";
    opt->type = SANE_TYPE_GROUP;
    opt->constraint_type = SANE_CONSTRAINT_NONE;
  }

  /* scan mode */
  else if(option==OPT_MODE){
    i=0;
    s->mode_list[i++]=STRING_GRAYSCALE;
    s->mode_list[i++]=STRING_COLOR;
    s->mode_list[i]=NULL;
  
    opt->name = SANE_NAME_SCAN_MODE;
    opt->title = SANE_TITLE_SCAN_MODE;
    opt->desc = SANE_DESC_SCAN_MODE;
    opt->type = SANE_TYPE_STRING;
    opt->constraint_type = SANE_CONSTRAINT_STRING_LIST;
    opt->constraint.string_list = s->mode_list;
    opt->size = maxStringSize (opt->constraint.string_list);
    opt->cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  }

  return opt;
}

/**
 * Gets or sets an option value.
 * 
 * From the SANE spec:
 * This function is used to set or inquire the current value of option
 * number n of the device represented by handle h. The manner in which
 * the option is controlled is specified by parameter action. The
 * possible values of this parameter are described in more detail
 * below.  The value of the option is passed through argument val. It
 * is a pointer to the memory that holds the option value. The memory
 * area pointed to by v must be big enough to hold the entire option
 * value (determined by member size in the corresponding option
 * descriptor).
 * 
 * The only exception to this rule is that when setting the value of a
 * string option, the string pointed to by argument v may be shorter
 * since the backend will stop reading the option value upon
 * encountering the first NUL terminator in the string. If argument i
 * is not NULL, the value of *i will be set to provide details on how
 * well the request has been met.
 */
SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
                     SANE_Action action, void *val, SANE_Int * info)
{
  struct scanner *s = (struct scanner *) handle;
  SANE_Int dummy = 0;

  /* Make sure that all those statements involving *info cannot break (better
   * than having to do "if (info) ..." everywhere!)
   */
  if (info == 0)
    info = &dummy;

  if (option >= NUM_OPTIONS) {
    DBG (5, "sane_control_option: %d too big\n", option);
    return SANE_STATUS_INVAL;
  }

  if (!SANE_OPTION_IS_ACTIVE (s->opt[option].cap)) {
    DBG (5, "sane_control_option: %d inactive\n", option);
    return SANE_STATUS_INVAL;
  }

  /*
   * SANE_ACTION_GET_VALUE: We have to find out the current setting and
   * return it in a human-readable form (often, text).
   */
  if (action == SANE_ACTION_GET_VALUE) {
      SANE_Word * val_p = (SANE_Word *) val;

      DBG (20, "sane_control_option: get value for '%s' (%d)\n", s->opt[option].name,option);

      switch (option) {

        case OPT_NUM_OPTS:
          *val_p = NUM_OPTIONS;
          return SANE_STATUS_GOOD;

        case OPT_MODE:
          if(s->mode == MODE_GRAYSCALE){
            strcpy (val, STRING_GRAYSCALE);
          }
          else if(s->mode == MODE_COLOR){
            strcpy (val, STRING_COLOR);
          }
          return SANE_STATUS_GOOD;
      }
  }
  else if (action == SANE_ACTION_SET_VALUE) {
      int tmp;
      SANE_Word val_c;
      SANE_Status status;

      DBG (20, "sane_control_option: set value for '%s' (%d)\n", s->opt[option].name,option);

      if ( s->started ) {
        DBG (5, "sane_control_option: cant set, device busy\n");
        return SANE_STATUS_DEVICE_BUSY;
      }

      if (!SANE_OPTION_IS_SETTABLE (s->opt[option].cap)) {
        DBG (5, "sane_control_option: not settable\n");
        return SANE_STATUS_INVAL;
      }

      status = sanei_constrain_value (s->opt + option, val, info);
      if (status != SANE_STATUS_GOOD) {
        DBG (5, "sane_control_option: bad value\n");
        return status;
      }

      /* may have been changed by constrain, so dont copy until now */
      val_c = *(SANE_Word *)val;

      /*
       * Note - for those options which can assume one of a list of
       * valid values, we can safely assume that they will have
       * exactly one of those values because that's what
       * sanei_constrain_value does. Hence no "else: invalid" branches
       * below.
       */
      switch (option) {
 
        /* Mode Group */
        case OPT_MODE:
          if (!strcmp (val, STRING_GRAYSCALE)) {
            tmp = MODE_GRAYSCALE;
          }
          else{
            tmp = MODE_COLOR;
          }

          if (tmp == s->mode)
              return SANE_STATUS_GOOD;

          s->mode = tmp;
          *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
          return SANE_STATUS_GOOD;

      }                       /* switch */
  }                           /* else */

  return SANE_STATUS_INVAL;
}

/*
 * @@ Section 4 - SANE scanning functions
 */
/*
 * Called by SANE to retrieve information about the type of data
 * that the current scan will return.
 *
 * From the SANE spec:
 * This function is used to obtain the current scan parameters. The
 * returned parameters are guaranteed to be accurate between the time
 * a scan has been started (sane_start() has been called) and the
 * completion of that request. Outside of that window, the returned
 * values are best-effort estimates of what the parameters will be
 * when sane_start() gets invoked.
 * 
 * Calling this function before a scan has actually started allows,
 * for example, to get an estimate of how big the scanned image will
 * be. The parameters passed to this function are the handle h of the
 * device for which the parameters should be obtained and a pointer p
 * to a parameter structure.
 */
SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  struct scanner *s = (struct scanner *) handle;

  DBG (10, "sane_get_parameters: start\n");

  params->pixels_per_line = PIXELS_PER_LINE;
  params->lines = -1;
  params->last_frame = 1;

  if (s->mode == MODE_COLOR) {
    params->format = SANE_FRAME_RGB;
    params->depth = 8;
    params->bytes_per_line = params->pixels_per_line * 3;
  }
  else if (s->mode == MODE_GRAYSCALE) {
    params->format = SANE_FRAME_GRAY;
    params->depth = 8;
    params->bytes_per_line = params->pixels_per_line;
  }

  DBG (15, "\tdepth %d\n", params->depth);
  DBG (15, "\tlines %d\n", params->lines);
  DBG (15, "\tpixels_per_line %d\n", params->pixels_per_line);
  DBG (15, "\tbytes_per_line %d\n", params->bytes_per_line);

  DBG (10, "sane_get_parameters: finish\n");

  return SANE_STATUS_GOOD;
}

/*
 * Called by SANE when a page acquisition operation is to be started.
 */
SANE_Status
sane_start (SANE_Handle handle)
{
    struct scanner *s = handle;
    SANE_Status ret;
  
    DBG (10, "sane_start: start\n");
  
    /* first page of batch */
    if(s->started){
        DBG(5,"sane_start: previous transfer not finished?");
        sane_cancel((SANE_Handle)s);
        return SANE_STATUS_CANCELLED;
    }
  
    /* set clean defaults */
    s->started=1;
    s->bytes_rx=0;
    s->bytes_tx=0;
    s->paperless_lines=0;
  
    /* heat up the lamp */ 
    if(s->mode == MODE_COLOR){
        ret = heat_lamp_color(s);
    }
    else{
        ret = heat_lamp_gray(s);
    }
  
    if (ret != SANE_STATUS_GOOD) {
        DBG (5, "sane_start: ERROR: failed to heat lamp\n");
        sane_cancel((SANE_Handle)s);
        return ret;
    }
  
    DBG (10, "sane_start: finish\n");
  
    return SANE_STATUS_GOOD;
}

static SANE_Status
heat_lamp_gray(struct scanner *s)
{
    SANE_Status ret = SANE_STATUS_GOOD;
    SANE_Status ret2 = SANE_STATUS_GOOD;
    unsigned char cmd[] =
      {0x12, 0x06, 0x00, 0x00, 0x01, 0x60, 0x00, 0x61, 0x00};
    size_t bytes = HEADER_SIZE + 1;
    unsigned char * buf;
    int i;
  
    DBG (10, "heat_lamp_gray: start\n");
  
    buf = malloc(bytes);
    if(!buf){
        DBG(5, "heat_lamp_gray: not enough mem for buffer: %lu\n",
          (long unsigned)bytes);
        return SANE_STATUS_NO_MEM;
    }

    for(i=0;i<10;i++){
    
        ret2 = do_cmd(
          s, 0,
          cmd, sizeof(cmd),
          NULL, 0,
          buf, &bytes
        );
      
        if (ret2 != SANE_STATUS_GOOD) {
            DBG(5, "heat_lamp_gray: %d error\n",i);
            ret = ret2;
            break;
        }
    
        if(!buf[1]){
            DBG(5, "heat_lamp_gray: %d got no docs\n",i);
            ret = SANE_STATUS_NO_DOCS;
            break;
        }
    
        DBG(15, "heat_lamp_gray: %d got: %d %d\n",i,
          buf[HEADER_SIZE],s->cal_gray_b[0]);

        if(buf[HEADER_SIZE] < 0x20){
            DBG(15, "heat_lamp_gray: hot\n");
            ret = SANE_STATUS_GOOD;
            break;
        }
        else{
            DBG(15, "heat_lamp_gray: cold\n");
            ret = SANE_STATUS_DEVICE_BUSY;
        }
    }
  
    free(buf);
  
    DBG (10, "heat_lamp_gray: finish %d\n",ret);
  
    return ret;
}

static SANE_Status
heat_lamp_color(struct scanner *s)
{
    SANE_Status ret = SANE_STATUS_GOOD;
    SANE_Status ret2 = SANE_STATUS_GOOD;
    unsigned char cmd[] =
      {0x18, 0x07, 0x00, 0x00, 0x01, 0x60, 0x00, 0x61, 0x00, 0x07};
    size_t bytes = HEADER_SIZE + 3;
    unsigned char * buf;
    int i;
  
    DBG (10, "heat_lamp_color: start\n");
  
    buf = malloc(bytes);
    if(!buf){
        DBG(5, "heat_lamp_color: not enough mem for buffer: %lu\n",
          (long unsigned)bytes);
        return SANE_STATUS_NO_MEM;
    }

    for(i=0;i<10;i++){
    
        ret2 = do_cmd(
          s, 0,
          cmd, sizeof(cmd),
          NULL, 0,
          buf, &bytes
        );
      
        if (ret2 != SANE_STATUS_GOOD) {
            DBG(5, "heat_lamp_color: %d error\n",i);
            ret = ret2;
            break;
        }
    
        if(!buf[1]){
            DBG(5, "heat_lamp_color: %d got no docs\n",i);
            ret = SANE_STATUS_NO_DOCS;
            break;
        }
    
        DBG(15, "heat_lamp_color: %d got: %d,%d,%d %d,%d,%d\n",i,
          buf[HEADER_SIZE],buf[HEADER_SIZE+1],buf[HEADER_SIZE+2],
          s->cal_color_b[0],s->cal_color_b[1],s->cal_color_b[2]);

        if(buf[HEADER_SIZE] < 0x20
         && buf[HEADER_SIZE+1] < 0x20
         && buf[HEADER_SIZE+2] < 0x20){
            DBG(15, "heat_lamp_color: hot\n");
            ret = SANE_STATUS_GOOD;
            break;
        }
        else{
            DBG(15, "heat_lamp_color: cold\n");
            ret = SANE_STATUS_DEVICE_BUSY;
        }
    }
  
    free(buf);
  
    DBG (10, "heat_lamp_color: finish %d\n",ret);
  
    return ret;
}

/*
 * Called by SANE to read data.
 * 
 * From the SANE spec:
 * This function is used to read image data from the device
 * represented by handle h.  Argument buf is a pointer to a memory
 * area that is at least maxlen bytes long.  The number of bytes
 * returned is stored in *len. A backend must set this to zero when
 * the call fails (i.e., when a status other than SANE_STATUS_GOOD is
 * returned).
 * 
 * When the call succeeds, the number of bytes returned can be
 * anywhere in the range from 0 to maxlen bytes.
 */
SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * buf, SANE_Int max_len, SANE_Int * len)
{
    struct scanner *s = (struct scanner *) handle;
    SANE_Status ret=SANE_STATUS_GOOD;
  
    DBG (10, "sane_read: start\n");
  
    *len = 0;
  
    /* cancelled? */
    if(!s->started){
        DBG (5, "sane_read: call sane_start first\n");
        return SANE_STATUS_CANCELLED;
    }
  
    /* have sent all of current buffer */
    if(s->bytes_tx == s->bytes_rx){
  
        /* at end of data, stop */
        if(s->paperless_lines >= MAX_PAPERLESS_LINES){
            DBG (15, "sane_read: returning eof\n");
            power_down(s);
            return SANE_STATUS_EOF;
        }
  
        /* more to get, reset and go */
        s->bytes_tx = 0;
        s->bytes_rx = 0;
  
        if(s->mode == MODE_COLOR){
            ret = read_from_scanner_color(s);
        }
        else{
            ret = read_from_scanner_gray(s);
        }
  
        if(ret){
            DBG(5,"sane_read: returning %d\n",ret);
            return ret;
        }
    }
  
    /* data in current buffer, send some of it */
    *len = s->bytes_rx - s->bytes_tx;
    if(*len > max_len){
        *len = max_len;
    }
  
    memcpy(buf,s->buffer+s->bytes_tx,*len);
    s->bytes_tx += *len;
  
    DBG (10, "sane_read: %d,%d,%d finish\n", *len,s->bytes_rx,s->bytes_tx);
  
    return ret;
}

static SANE_Status
read_from_scanner_gray(struct scanner *s)
{
    SANE_Status ret=SANE_STATUS_GOOD;
    /*cmd    len-le16    move  lines  ???   ???   ???   ???*/
    unsigned char cmd[] =
      {0x12, 0x06, 0x00, 0x01, 0x01, 0x60, 0x00, 0x18, 0x05};
    size_t bytes = HEADER_SIZE + s->gray_block_size;
    unsigned char * buf;
    int i,j;
  
    DBG (10, "read_from_scanner_gray: start\n");
 
    cmd[4] = s->lines_per_block;

    buf = malloc(bytes);
    if(!buf){
        DBG(5, "read_from_scanner_gray: not enough mem for buffer: %lu\n",
          (long unsigned)bytes);
        return SANE_STATUS_NO_MEM;
    }
  
    ret = do_cmd(
      s, 0,
      cmd, sizeof(cmd),
      NULL, 0,
      buf, &bytes
    );
  
    if (ret == SANE_STATUS_GOOD) {

        DBG(15, "read_from_scanner_gray: got GOOD\n");

        if(!buf[1]){
          s->paperless_lines += s->lines_per_block;
        }
    
        s->bytes_rx = s->gray_block_size;
    
        /*memcpy(s->buffer,buf+HEADER_SIZE,s->gray_block_size);*/
  
        /* reorder the gray data into the struct's buffer */
        for(i=0;i<s->gray_block_size;i+=PIXELS_PER_LINE){
            for(j=0;j<PIXELS_PER_LINE;j++){
      
                unsigned char byte = buf[ HEADER_SIZE + i + j ];
                unsigned char bcal = s->cal_gray_b[j];
                unsigned char wcal = s->cal_gray_w[j];
        
                byte = (byte <= bcal)?0:(byte-bcal);
                byte = (byte >= wcal)?255:(byte*255/wcal);
                s->buffer[i+j] = byte;
            }
        }
    }
    else {
        DBG(5, "read_from_scanner_gray: error reading status = %d\n", ret);
    }
  
    free(buf);
  
    DBG (10, "read_from_scanner_gray: finish\n");
  
    return ret;
}

static SANE_Status
read_from_scanner_color(struct scanner *s)
{
    SANE_Status ret=SANE_STATUS_GOOD;
    unsigned char cmd[] =
     {0x18, 0x07, 0x00, 0x01, 0x01, 0x60, 0x00, 0x18, 0x05, 0x07};
    size_t bytes = HEADER_SIZE + s->color_block_size;
    unsigned char * buf;
    int i,j,k;
  
    DBG (10, "read_from_scanner_color: start\n");
  
    cmd[4] = s->lines_per_block;

    buf = malloc(bytes);
    if(!buf){
        DBG(5, "read_from_scanner_color: not enough mem for buffer: %lu\n",
          (long unsigned)bytes);
        return SANE_STATUS_NO_MEM;
    }
  
    ret = do_cmd(
      s, 0,
      cmd, sizeof(cmd),
      NULL, 0,
      buf, &bytes
    );
  
    if (ret == SANE_STATUS_GOOD) {

        DBG(15, "read_from_scanner_color: got GOOD\n");

        if(!buf[1]){
          s->paperless_lines += s->lines_per_block;
        }
    
        s->bytes_rx = s->color_block_size;
    
        /*memcpy(s->buffer,buf+HEADER_SIZE,s->color_block_size);*/
  
        /* reorder the color data into the struct's buffer */
        for(i=0;i<s->color_block_size;i+=PIXELS_PER_LINE*3){
            for(j=0;j<PIXELS_PER_LINE;j++){
                for(k=0;k<3;k++){
      
                    int offset = PIXELS_PER_LINE*(2-k) + j;
                    unsigned char byte = buf[ HEADER_SIZE + i + offset ];
                    unsigned char bcal = s->cal_color_b[offset];
                    unsigned char wcal = s->cal_color_w[offset];
            
                    byte = (byte <= bcal)?0:(byte-bcal);
                    byte = (byte >= wcal)?255:(byte*255/wcal);
                    s->buffer[i+j*3+k] = byte;
                }
            }
        }
    }
    else {
        DBG(5, "read_from_scanner_color: error reading status = %d\n", ret);
    }
  
    free(buf);
  
    DBG (10, "read_from_scanner_color: finish\n");
  
    return ret;
}

/*
 * @@ Section 4 - SANE cleanup functions
 */
/*
 * Cancels a scan. 
 *
 * From the SANE spec:
 * This function is used to immediately or as quickly as possible
 * cancel the currently pending operation of the device represented by
 * handle h.  This function can be called at any time (as long as
 * handle h is a valid handle) but usually affects long-running
 * operations only (such as image is acquisition). It is safe to call
 * this function asynchronously (e.g., from within a signal handler).
 * It is important to note that completion of this operaton does not
 * imply that the currently pending operation has been cancelled. It
 * only guarantees that cancellation has been initiated. Cancellation
 * completes only when the cancelled call returns (typically with a
 * status value of SANE_STATUS_CANCELLED).  Since the SANE API does
 * not require any other operations to be re-entrant, this implies
 * that a frontend must not call any other operation until the
 * cancelled operation has returned.
 */
void
sane_cancel (SANE_Handle handle)
{
  struct scanner * s = (struct scanner *) handle;
  DBG (10, "sane_cancel: start\n");
  s->started = 0;
  DBG (10, "sane_cancel: finish\n");
}

static SANE_Status
power_down(struct scanner *s)
{
    SANE_Status ret = SANE_STATUS_GOOD;

    unsigned char cmd[] = {0x21, 0x02, 0x00, 0x0a, 0x00};
    unsigned char buf[6];
    size_t bytes = sizeof(buf);
    int i;
  
    DBG (10, "power_down: start\n");

    for(i=0;i<5;i++){
        ret = do_cmd(
          s, 0,
          cmd, sizeof(cmd),
          NULL, 0,
          buf, &bytes
        );

        if(ret != SANE_STATUS_GOOD){
            break;
        }
    }

#if 0
    unsigned char cmd[] = {0x35, 0x01, 0x00, 0xff};
    unsigned char buf[5];
    size_t bytes = sizeof(buf);
  
    DBG (10, "power_down: start\n");
  
    ret = do_cmd(
      s, 0,
      cmd, sizeof(cmd),
      NULL, 0,
      buf, &bytes
    );
#endif
      
    DBG (10, "power_down: finish %d\n",ret);
  
    return ret;
}

/*
 * Ends use of the scanner.
 * 
 * From the SANE spec:
 * This function terminates the association between the device handle
 * passed in argument h and the device it represents. If the device is
 * presently active, a call to sane_cancel() is performed first. After
 * this function returns, handle h must not be used anymore.
 */
void
sane_close (SANE_Handle handle)
{
  DBG (10, "sane_close: start\n");

  sane_cancel(handle);
  disconnect_fd((struct scanner *) handle);

  DBG (10, "sane_close: finish\n");
}

static SANE_Status
disconnect_fd (struct scanner *s)
{
  DBG (10, "disconnect_fd: start\n");

  if(s->fd > -1){
    DBG (15, "disconnecting usb device\n");
    sanei_usb_close (s->fd);
    s->fd = -1;
  }

  DBG (10, "disconnect_fd: finish\n");

  return SANE_STATUS_GOOD;
}

/*
 * Terminates the backend.
 * 
 * From the SANE spec:
 * This function must be called to terminate use of a backend. The
 * function will first close all device handles that still might be
 * open (it is recommended to close device handles explicitly through
 * a call to sane_close(), but backends are required to release all
 * resources upon a call to this function). After this function
 * returns, no function other than sane_init() may be called
 * (regardless of the status value returned by sane_exit(). Neglecting
 * to call this function may result in some resources not being
 * released properly.
 */
void
sane_exit (void)
{
  struct scanner *dev, *next;

  DBG (10, "sane_exit: start\n");

  for (dev = scanner_devList; dev; dev = next) {
      disconnect_fd(dev);
      next = dev->next;
      free (dev->device_name);
      free (dev);
  }

  if (sane_devArray)
    free (sane_devArray);

  scanner_devList = NULL;
  sane_devArray = NULL;

  DBG (10, "sane_exit: finish\n");
}


/*
 * @@ Section 5 - misc helper functions
 */
/*
 * take a bunch of pointers, send commands to scanner
 */
static SANE_Status
do_cmd(struct scanner *s, int shortTime,
 unsigned char * cmdBuff, size_t cmdLen,
 unsigned char * outBuff, size_t outLen,
 unsigned char * inBuff, size_t * inLen
)
{
    /* sanei_usb overwrites the transfer size, so make some local copies */
    size_t loc_cmdLen = cmdLen;
    size_t loc_outLen = outLen;
    size_t loc_inLen = *inLen;

    int cmdTime = USB_COMMAND_TIME;
    int outTime = USB_DATA_TIME;
    int inTime = USB_DATA_TIME;

    int ret = 0;

    DBG (10, "do_cmd: start\n");

    if(shortTime){
        cmdTime /= 20;
        outTime /= 20;
        inTime /= 20;
    }

    /* change timeout */
    sanei_usb_set_timeout(cmdTime);

    /* write the command out */
    DBG(25, "cmd: writing %ld bytes, timeout %d\n", (long)cmdLen, cmdTime);
    hexdump(30, "cmd: >>", cmdBuff, cmdLen);
    ret = sanei_usb_write_bulk(s->fd, cmdBuff, &cmdLen);
    DBG(25, "cmd: wrote %ld bytes, retVal %d\n", (long)cmdLen, ret);

    if(ret == SANE_STATUS_EOF){
        DBG(5,"cmd: got EOF, returning IO_ERROR\n");
        return SANE_STATUS_IO_ERROR;
    }
    if(ret != SANE_STATUS_GOOD){
        DBG(5,"cmd: return error '%s'\n",sane_strstatus(ret));
        return ret;
    }
    if(loc_cmdLen != cmdLen){
        DBG(5,"cmd: wrong size %ld/%ld\n", (long)loc_cmdLen, (long)cmdLen);
        return SANE_STATUS_IO_ERROR;
    }

    /* this command has a write component, and a place to get it */
    if(outBuff && outLen && outTime){

        /* change timeout */
        sanei_usb_set_timeout(outTime);

        DBG(25, "out: writing %ld bytes, timeout %d\n", (long)outLen, outTime);
        hexdump(30, "out: >>", outBuff, outLen);
        ret = sanei_usb_write_bulk(s->fd, outBuff, &outLen);
        DBG(25, "out: wrote %ld bytes, retVal %d\n", (long)outLen, ret);

        if(ret == SANE_STATUS_EOF){
            DBG(5,"out: got EOF, returning IO_ERROR\n");
            return SANE_STATUS_IO_ERROR;
        }
        if(ret != SANE_STATUS_GOOD){
            DBG(5,"out: return error '%s'\n",sane_strstatus(ret));
            return ret;
        }
        if(loc_outLen != outLen){
            DBG(5,"out: wrong size %ld/%ld\n", (long)loc_outLen, (long)outLen);
            return SANE_STATUS_IO_ERROR;
        }
    }

    /* this command has a read component, and a place to put it */
    if(inBuff && inLen && inTime){

        memset(inBuff,0,*inLen);

        /* change timeout */
        sanei_usb_set_timeout(inTime);

        DBG(25, "in: reading %ld bytes, timeout %d\n", (long)*inLen, inTime);
        ret = sanei_usb_read_bulk(s->fd, inBuff, inLen);
        DBG(25, "in: retVal %d\n", ret);

        if(ret == SANE_STATUS_EOF){
            DBG(5,"in: got EOF, continuing\n");
        }
        else if(ret != SANE_STATUS_GOOD){
            DBG(5,"in: return error '%s'\n",sane_strstatus(ret));
            return ret;
        }

        DBG(25, "in: read %ld bytes\n", (long)*inLen);
        if(*inLen){
            hexdump(30, "in: <<", inBuff, *inLen);
        }

        if(loc_inLen != *inLen){
            ret = SANE_STATUS_EOF;
            DBG(5,"in: short read %ld/%ld\n", (long)loc_inLen, (long)*inLen);
        }
    }

    DBG (10, "do_cmd: finish\n");

    return ret;
}

/**
 * Convenience method to determine longest string size in a list.
 */
static size_t
maxStringSize (const SANE_String_Const strings[])
{
  size_t size, max_size = 0;
  int i;

  for (i = 0; strings[i]; ++i) {
    size = strlen (strings[i]) + 1;
    if (size > max_size)
      max_size = size;
  }

  return max_size;
}

/**
 * Prints a hex dump of the given buffer onto the debug output stream.
 */
static void
hexdump (int level, char *comment, unsigned char *p, int l)
{
  int i;
  char line[128];
  char *ptr;

  if(DBG_LEVEL < level)
    return;

  DBG (level, "%s\n", comment);
  ptr = line;
  for (i = 0; i < l; i++, p++)
    {
      if ((i % 16) == 0)
        {
          if (ptr != line)
            {
              *ptr = '\0';
              DBG (level, "%s\n", line);
              ptr = line;
            }
          sprintf (ptr, "%3.3x:", i);
          ptr += 4;
        }
      sprintf (ptr, " %2.2x", *p);
      ptr += 3;
    }
  *ptr = '\0';
  DBG (level, "%s\n", line);
}

/**
 * An advanced method we don't support but have to define.
 */
SANE_Status
sane_set_io_mode (SANE_Handle h, SANE_Bool non_blocking)
{
  DBG (10, "sane_set_io_mode\n");
  DBG (15, "%d %p\n", non_blocking, h);
  return SANE_STATUS_UNSUPPORTED;
}

/**
 * An advanced method we don't support but have to define.
 */
SANE_Status
sane_get_select_fd (SANE_Handle h, SANE_Int *fdp)
{
  DBG (10, "sane_get_select_fd\n");
  DBG (15, "%p %d\n", h, *fdp);
  return SANE_STATUS_UNSUPPORTED;
}
