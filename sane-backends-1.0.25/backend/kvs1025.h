/*
   Copyright (C) 2008, Panasonic Russia Ltd.
*/
/* sane - Scanner Access Now Easy.
   Panasonic KV-S1020C / KV-S1025C USB scanners.
*/

#ifndef __KVS1025_H
#define __KVS1025_H

/* SANE backend name */
#ifdef BACKEND_NAME
#undef BACKEND_NAME
#endif

#define BACKEND_NAME          kvs1025

/* Build version */
#define V_BUILD            5

/* Paper range supported -- MAX scanner limits */
#define KV_MAX_X_RANGE     216
#define KV_MAX_Y_RANGE     2540

/* Round ULX, ULY, Width and Height to 16 Pixels */
#define KV_PIXEL_ROUND  19200
/* (XR * W / 1200) % 16 == 0 i.e. (XR * W) % 19200 == 0 */

/* MAX IULs per LINE */
#define KV_PIXEL_MAX    14064
/* Max 14064 pixels per line, 1/1200 inch each */

#define MM_PER_INCH         25.4
#define mmToIlu(mm) (((mm) * 1200) / MM_PER_INCH)
#define iluToMm(ilu) (((ilu) * MM_PER_INCH) / 1200)

/* Vendor defined options */
#define SANE_NAME_DUPLEX            "duplex"
#define SANE_NAME_PAPER_SIZE        "paper-size"
#define SANE_NAME_AUTOSEP           "autoseparation"
#define SANE_NAME_LANDSCAPE         "landscape"
#define SANE_NAME_INVERSE           "inverse"
#define SANE_NAME_MIRROR            "mirror"
#define SANE_NAME_LONGPAPER         "longpaper"
#define SANE_NAME_LENGTHCTL         "length-control"
#define SANE_NAME_MANUALFEED        "manual-feed"
#define SANE_NAME_FEED_TIMEOUT	    "feed-timeout"
#define SANE_NAME_DBLFEED           "double-feed"

#define SANE_TITLE_DUPLEX           SANE_I18N("Duplex")
#define SANE_TITLE_PAPER_SIZE       SANE_I18N("Paper size")
#define SANE_TITLE_AUTOSEP          SANE_I18N("Automatic separation")
#define SANE_TITLE_LANDSCAPE        SANE_I18N("Landscape")
#define SANE_TITLE_INVERSE          SANE_I18N("Inverse Image")
#define SANE_TITLE_MIRROR           SANE_I18N("Mirror image")
#define SANE_TITLE_LONGPAPER        SANE_I18N("Long paper mode")
#define SANE_TITLE_LENGTHCTL        SANE_I18N("Length control mode")
#define SANE_TITLE_MANUALFEED       SANE_I18N("Manual feed mode")
#define SANE_TITLE_FEED_TIMEOUT	    SANE_I18N("Manual feed timeout")
#define SANE_TITLE_DBLFEED          SANE_I18N("Double feed detection")

#define SANE_DESC_DUPLEX \
SANE_I18N("Enable Duplex (Dual-Sided) Scanning")
#define SANE_DESC_PAPER_SIZE \
SANE_I18N("Physical size of the paper in the ADF");
#define SANE_DESC_AUTOSEP \
SANE_I18N("Automatic separation")

#define SIDE_FRONT      0x00
#define SIDE_BACK       0x80

/* Debug levels.
 * Should be common to all backends. */

#define DBG_error0  0
#define DBG_error   1
#define DBG_sense   2
#define DBG_warning 3
#define DBG_inquiry 4
#define DBG_info    5
#define DBG_info2   6
#define DBG_proc    7
#define DBG_read    8
#define DBG_sane_init   10
#define DBG_sane_proc   11
#define DBG_sane_info   12
#define DBG_sane_option 13
#define DBG_shortread   101

/* Prototypes of SANE backend functions, see kvs1025.c */

SANE_Status sane_init (SANE_Int * version_code,
		       SANE_Auth_Callback /* __sane_unused__ authorize */ );

void sane_exit (void);

SANE_Status sane_get_devices (const SANE_Device *** device_list,
			      SANE_Bool /*__sane_unused__ local_only*/ );

SANE_Status sane_open (SANE_String_Const devicename, SANE_Handle * handle);

void sane_close (SANE_Handle handle);

const SANE_Option_Descriptor *sane_get_option_descriptor (SANE_Handle
							  handle,
							  SANE_Int option);

SANE_Status sane_control_option (SANE_Handle handle, SANE_Int option,
				 SANE_Action action, void *val,
				 SANE_Int * info);
SANE_Status sane_get_parameters (SANE_Handle handle,
				 SANE_Parameters * params);

SANE_Status sane_start (SANE_Handle handle);

SANE_Status sane_read (SANE_Handle handle, SANE_Byte * buf,
		       SANE_Int max_len, SANE_Int * len);

void sane_cancel (SANE_Handle handle);

SANE_Status sane_set_io_mode (SANE_Handle h, SANE_Bool m);

SANE_Status sane_get_select_fd (SANE_Handle h, SANE_Int * fd);

SANE_String_Const sane_strstatus (SANE_Status status);

#endif /* #ifndef __KVS1025_H */
