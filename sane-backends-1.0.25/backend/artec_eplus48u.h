#ifndef ARTEC48U_H
#define ARTEC48U_H

#include "../include/sane/sane.h"
#include "../include/sane/sanei.h"
#include "../include/sane/saneopts.h"
#include <sys/types.h>
#ifdef HAVE_SYS_IPC_H
#include <sys/ipc.h>
#endif
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "../include/sane/sanei_usb.h"
#include "../include/sane/sanei_thread.h"

#define _MAX_ID_LEN 20

/*Uncomment next line for button support. This
  actually isn't supported by the frontends. */
/*#define ARTEC48U_USE_BUTTONS 1*/

#define ARTEC48U_PACKET_SIZE 64
#define DECLARE_FUNCTION_NAME(name)     \
  IF_DBG ( static const char function_name[] = name; )

typedef SANE_Byte Artec48U_Packet[ARTEC48U_PACKET_SIZE];
#define XDBG(args)           do { IF_DBG ( DBG args ); } while (0)

/* calculate the minimum/maximum values */
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

/* return the lower/upper 8 bits of a 16 bit word */
#define HIBYTE(w) ((SANE_Byte)(((SANE_Word)(w) >> 8) & 0xFF))
#define LOBYTE(w) ((SANE_Byte)(w))


#define CHECK_DEV_NOT_NULL(dev, func_name)                              \
  do {                                                                  \
    if (!(dev))                                                         \
      {                                                                 \
        XDBG ((3, "%s: BUG: NULL device\n", (func_name)));              \
        return SANE_STATUS_INVAL;                                       \
      }                                                                 \
  } while (SANE_FALSE)

/** Check that the device is open.
 *
 * @param dev       Pointer to the device object (Artec48U_Device).
 * @param func_name Function name (for use in debug messages).
 */
#define CHECK_DEV_OPEN(dev, func_name)                                  \
  do {                                                                  \
    CHECK_DEV_NOT_NULL ((dev), (func_name));                            \
    if ((dev)->fd == -1)                                                \
      {                                                                 \
        XDBG ((3, "%s: BUG: device %p not open\n", (func_name), (void*)(dev)));\
        return SANE_STATUS_INVAL;                                       \
      }                                                                 \
  } while (SANE_FALSE)

#define CHECK_DEV_ACTIVE(dev,func_name)                                \
  do {                                                                  \
    CHECK_DEV_OPEN ((dev), (func_name));                                \
    if (!(dev)->active)                                                 \
      {                                                                 \
        XDBG ((3, "%s: BUG: device %p not active\n",                    \
               (func_name), (void*)(dev)));                                    \
        return SANE_STATUS_INVAL;                                       \
      }                                                                 \
  } while (SANE_FALSE)

typedef struct Artec48U_Device Artec48U_Device;
typedef struct Artec48U_Scan_Request Artec48U_Scan_Request;
typedef struct Artec48U_Scanner Artec48U_Scanner;
typedef struct Artec48U_Scan_Parameters Artec48U_Scan_Parameters;
typedef struct Artec48U_AFE_Parameters Artec48U_AFE_Parameters;
typedef struct Artec48U_Exposure_Parameters Artec48U_Exposure_Parameters;
typedef struct Artec48U_Line_Reader Artec48U_Line_Reader;
typedef struct Artec48U_Delay_Buffer Artec48U_Delay_Buffer;

enum artec_options
{
  OPT_NUM_OPTS = 0,
  OPT_MODE_GROUP,
  OPT_SCAN_MODE,
  OPT_BIT_DEPTH,
  OPT_BLACK_LEVEL,
  OPT_RESOLUTION,
  OPT_ENHANCEMENT_GROUP,
  OPT_BRIGHTNESS,
  OPT_CONTRAST,
  OPT_GAMMA,
  OPT_GAMMA_R,
  OPT_GAMMA_G,
  OPT_GAMMA_B,
  OPT_DEFAULT_ENHANCEMENTS,
  OPT_GEOMETRY_GROUP,
  OPT_TL_X,
  OPT_TL_Y,
  OPT_BR_X,
  OPT_BR_Y,
  OPT_CALIBRATION_GROUP,
  OPT_CALIBRATE,
  OPT_CALIBRATE_SHADING,
#ifdef ARTEC48U_USE_BUTTONS
  OPT_BUTTON_STATE,
#endif
  /* must come last: */
  NUM_OPTIONS
};

/** Artec48U analog front-end (AFE) parameters.
 */
struct Artec48U_AFE_Parameters
{
  SANE_Byte r_offset;	/**< Red channel offset */
  SANE_Byte r_pga;	/**< Red channel PGA gain */
  SANE_Byte g_offset;	/**< Green channel offset (also used for mono) */
  SANE_Byte g_pga;	/**< Green channel PGA gain (also used for mono) */
  SANE_Byte b_offset;	/**< Blue channel offset */
  SANE_Byte b_pga;	/**< Blue channel PGA gain */
};

/** TV9693 exposure time parameters.
 */
struct Artec48U_Exposure_Parameters
{
  SANE_Int r_time;     /**< Red exposure time */
  SANE_Int g_time;     /**< Red exposure time */
  SANE_Int b_time;     /**< Red exposure time */
};

struct Artec48U_Device
{
  Artec48U_Device *next;
  /** Device file descriptor. */
  int fd;
  /** Device activation flag. */
  SANE_Bool active;
  SANE_String_Const name;
  SANE_Device sane;		 /** Scanner model data. */
  SANE_String_Const firmware_path;
  double gamma_master;
  double gamma_r;
  double gamma_g;
  double gamma_b;
  Artec48U_Exposure_Parameters exp_params;
  Artec48U_AFE_Parameters afe_params;
  Artec48U_AFE_Parameters artec_48u_afe_params;
  Artec48U_Exposure_Parameters artec_48u_exposure_params;

  SANE_Int optical_xdpi;
  SANE_Int optical_ydpi;
  SANE_Int base_ydpi;
  SANE_Int xdpi_offset;		/* in optical_xdpi units */
  SANE_Int ydpi_offset;		/* in optical_ydpi units */
  SANE_Int x_size;		/* in optical_xdpi units */
  SANE_Int y_size;		/* in optical_ydpi units */
/* the number of lines, that we move forward before we start reading the
   shading lines */
  int shading_offset;
/* the number of lines we read for the black shading buffer */
  int shading_lines_b;
/* the number of lines we read for the white shading buffer */
  int shading_lines_w;

  SANE_Fixed x_offset, y_offset;
  SANE_Bool read_active;
  SANE_Byte *read_buffer;
  size_t requested_buffer_size;
  size_t read_pos;
  size_t read_bytes_in_buffer;
  size_t read_bytes_left;
  unsigned int is_epro;
  unsigned int epro_mult;
};

/** Scan parameters for artec48u_device_setup_scan().
 *
 * These parameters describe a low-level scan request; many such requests are
 * executed during calibration, and they need to have parameters separate from
 * the main request (Artec48U_Scan_Request).  E.g., on the BearPaw 2400 TA the
 * scan to find the home position is always done at 300dpi 8-bit mono with
 * fixed width and height, regardless of the high-level scan parameters.
 */
struct Artec48U_Scan_Parameters
{
  SANE_Int xdpi;	/**< Horizontal resolution */
  SANE_Int ydpi;	/**< Vertical resolution */
  SANE_Int depth;	/**< Number of bits per channel */
  SANE_Bool color;	/**< Color mode flag */

  SANE_Int pixel_xs;		/**< Logical width in pixels */
  SANE_Int pixel_ys;		/**< Logical height in pixels */
  SANE_Int scan_xs;		/**< Physical width in pixels */
  SANE_Int scan_ys;		/**< Physical height in pixels */
  SANE_Int scan_bpl;		/**< Number of bytes per scan line */
  SANE_Bool lineart;		/**<Lineart is not really supported by device*/
};


/** Parameters for the high-level scan request.
 *
 * These parameters describe the scan request sent by the SANE frontend.
 */
struct Artec48U_Scan_Request
{
  SANE_Fixed x0;	/**< Left boundary  */
  SANE_Fixed y0;	/**< Top boundary */
  SANE_Fixed xs;	/**< Width */
  SANE_Fixed ys;	/**< Height */
  SANE_Int xdpi;	/**< Horizontal resolution */
  SANE_Int ydpi;	/**< Vertical resolution */
  SANE_Int depth;	/**< Number of bits per channel */
  SANE_Bool color;	/**< Color mode flag */
};
/** Scan action code (purpose of the scan).
 *
 * The scan action code affects various scanning mode fields in the setup
 * command.
 */
typedef enum Artec48U_Scan_Action
{
  SA_CALIBRATE_SCAN_WHITE,	/**< Scan white shading buffer           */
  SA_CALIBRATE_SCAN_BLACK,	/**< Scan black shading buffer           */
  SA_CALIBRATE_SCAN_OFFSET_1,	/**< First scan to determin offset       */
  SA_CALIBRATE_SCAN_OFFSET_2,	/**< Second scan to determine offset     */
  SA_CALIBRATE_SCAN_EXPOSURE_1,	  /**< First scan to determin offset       */
  SA_CALIBRATE_SCAN_EXPOSURE_2,	  /**< Second scan to determine offset     */
  SA_SCAN			/**< Normal scan */
}
Artec48U_Scan_Action;


struct Artec48U_Delay_Buffer
{
  SANE_Int line_count;
  SANE_Int read_index;
  SANE_Int write_index;
  unsigned int **lines;
  SANE_Byte *mem_block;
};

struct Artec48U_Line_Reader
{
  Artec48U_Device *dev;			  /**< Low-level interface object */
  Artec48U_Scan_Parameters params;	  /**< Scan parameters */

  /** Number of pixels in the returned scanlines */
  SANE_Int pixels_per_line;

  SANE_Byte *pixel_buffer;

  Artec48U_Delay_Buffer r_delay;
  Artec48U_Delay_Buffer g_delay;
  Artec48U_Delay_Buffer b_delay;
  SANE_Bool delays_initialized;

    SANE_Status (*read) (Artec48U_Line_Reader * reader,
			 unsigned int **buffer_pointers_return);
};

#ifndef SANE_OPTION
typedef union
{
  SANE_Word w;
  SANE_Word *wa;		/* word array */
  SANE_String s;
}
Option_Value;
#endif

struct Artec48U_Scanner
{
  Artec48U_Scanner *next;
  Artec48U_Scan_Parameters params;
  Artec48U_Scan_Request request;
  Artec48U_Device *dev;
  Artec48U_Line_Reader *reader;
  FILE *pipe_handle;
  SANE_Pid reader_pid;
  int pipe;
  int reader_pipe;
  SANE_Option_Descriptor opt[NUM_OPTIONS];
  Option_Value val[NUM_OPTIONS];
  SANE_Status exit_code;
  SANE_Parameters sane_params;
  SANE_Bool scanning;
  SANE_Bool eof;
  SANE_Bool calibrated;
  SANE_Word gamma_array[4][65536];
  SANE_Word contrast_array[65536];
  SANE_Word brightness_array[65536];
  SANE_Byte *line_buffer;
  SANE_Byte *lineart_buffer;
  SANE_Word lines_to_read;
  unsigned int temp_shading_buffer[3][10240]; /*epro*/
  unsigned int *buffer_pointers[3];
  unsigned char *shading_buffer_w;
  unsigned char *shading_buffer_b;
  unsigned int *shading_buffer_white[3];
  unsigned int *shading_buffer_black[3];
  unsigned long byte_cnt;
};


/** Create a new Artec48U_Device object.
 *
 * The newly created device object is in the closed state.
 *
 * @param dev_return Returned pointer to the created device object.
 *
 * @return
 * - #SANE_STATUS_GOOD   - the device object was created.
 * - #SANE_STATUS_NO_MEM - not enough system resources to create the object.
 */
static SANE_Status artec48u_device_new (Artec48U_Device ** dev_return);

/** Destroy the device object and release all associated resources.
 *
 * If the device was active, it will be deactivated; if the device was open, it
 * will be closed.
 *
 * @param dev Device object.
 *
 * @return
 * - #SANE_STATUS_GOOD  - success.
 */
static SANE_Status artec48u_device_free (Artec48U_Device * dev);

/** Open the scanner device.
 *
 * This function opens the device special file @a dev_name and tries to detect
 * the device model by its USB ID.
 *
 * If the device is detected successfully (its USB ID is found in the supported
 * device list), this function sets the appropriate model parameters.
 *
 * If the USB ID is not recognized, the device remains unconfigured; an attempt
 * to activate it will fail unless artec48u_device_set_model() is used to force
 * the parameter set.  Note that the open is considered to be successful in
 * this case.
 *
 * @param dev Device object.
 * @param dev_name Scanner device name.
 *
 * @return
 * - #SANE_STATUS_GOOD - the device was opened successfully (it still may be
 *   unconfigured).
 */
static SANE_Status artec48u_device_open (Artec48U_Device * dev);

/** Close the scanner device.
 *
 * @param dev Device object.
 */
static SANE_Status artec48u_device_close (Artec48U_Device * dev);

/** Activate the device.
 *
 * The device must be activated before performing any I/O operations with it.
 * All device model parameters must be configured before activation; it is
 * impossible to change them after the device is active.
 *
 * This function might need to acquire resources (it calls
 * Artec48U_Command_Set::activate).  These resources will be released when
 * artec48u_device_deactivate() is called.
 *
 * @param dev Device object.
 *
 * @return
 * - #SANE_STATUS_GOOD  - device activated successfully.
 * - #SANE_STATUS_INVAL - invalid request (attempt to activate a closed or
 *   unconfigured device).
 */
static SANE_Status artec48u_device_activate (Artec48U_Device * dev);

/** Deactivate the device.
 *
 * This function reverses the action of artec48u_device_activate().
 *
 * @param dev Device object.
 *
 * @return
 * - #SANE_STATUS_GOOD  - device deactivated successfully.
 * - #SANE_STATUS_INVAL - invalid request (the device was not activated).
 */
static SANE_Status artec48u_device_deactivate (Artec48U_Device * dev);

/** Write a data block to the TV9693 memory.
 *
 * @param dev  Device object.
 * @param addr Start address in the TV9693 memory.
 * @param size Size of the data block in bytes.
 * @param data Data block to write.
 *
 * @return
 * - #SANE_STATUS_GOOD     - success.
 * - #SANE_STATUS_IO_ERROR - a communication error occured.
 *
 * @warning
 * @a size must be a multiple of 64 (at least with TV9693), otherwise the
 * scanner (and possibly the entire USB bus) will lock up.
 */
static SANE_Status
artec48u_device_memory_write (Artec48U_Device * dev, SANE_Word addr,
			      SANE_Word size, SANE_Byte * data);

/** Read a data block from the TV9693 memory.
 *
 * @param dev  Device object.
 * @param addr Start address in the TV9693 memory.
 * @param size Size of the data block in bytes.
 * @param data Buffer for the read data.
 *
 * @return
 * - #SANE_STATUS_GOOD     - success.
 * - #SANE_STATUS_IO_ERROR - a communication error occured.
 *
 * @warning
 * @a size must be a multiple of 64 (at least with TV9693), otherwise the
 * scanner (and possibly the entire USB bus) will lock up.
 */
static SANE_Status
artec48u_device_memory_read (Artec48U_Device * dev, SANE_Word addr,
			     SANE_Word size, SANE_Byte * data);

/** Execute a control command.
 *
 * @param dev Device object.
 * @param cmd Command packet.
 * @param res Result packet (may point to the same buffer as @a cmd).
 *
 * @return
 * - #SANE_STATUS_GOOD     - success.
 * - #SANE_STATUS_IO_ERROR - a communication error occured.
 */
static SANE_Status
artec48u_device_req (Artec48U_Device * dev, Artec48U_Packet cmd,
		     Artec48U_Packet res);

/** Execute a "small" control command.
 *
 * @param dev Device object.
 * @param cmd Command packet; only first 8 bytes are used.
 * @param res Result packet (may point to the same buffer as @a cmd).
 *
 * @return
 * - #SANE_STATUS_GOOD     - success.
 * - #SANE_STATUS_IO_ERROR - a communication error occured.
 */
static SANE_Status
artec48u_device_small_req (Artec48U_Device * dev, Artec48U_Packet cmd,
			   Artec48U_Packet res);

/** Read raw data from the bulk-in scanner pipe.
 *
 * @param dev Device object.
 * @param buffer Buffer for the read data.
 * @param size Pointer to the variable which must be set to the requested data
 * size before call.  After completion this variable will hold the number of
 * bytes actually read.
 *
 * @return
 * - #SANE_STATUS_GOOD - success.
 * - #SANE_STATUS_IO_ERROR - a communication error occured.
 */
static SANE_Status
artec48u_device_read_raw (Artec48U_Device * dev, SANE_Byte * buffer,
			  size_t * size);

static SANE_Status
artec48u_device_set_read_buffer_size (Artec48U_Device * dev,
				      size_t buffer_size);

static SANE_Status
artec48u_device_read_prepare (Artec48U_Device * dev, size_t expected_count);

static SANE_Status
artec48u_device_read (Artec48U_Device * dev, SANE_Byte * buffer,
		      size_t * size);

static SANE_Status artec48u_device_read_finish (Artec48U_Device * dev);


/**
 * Create a new Artec48U_Line_Reader object.
 *
 * @param dev           The low-level scanner interface object.
 * @param params        Scan parameters prepared by artec48u_device_setup_scan().
 * @param reader_return Location for the returned object.
 *
 * @return
 * - SANE_STATUS_GOOD   - on success
 * - SANE_STATUS_NO_MEM - cannot allocate memory for object or buffers
 * - other error values - failure of some internal functions
 */
static SANE_Status
artec48u_line_reader_new (Artec48U_Device * dev,
			  Artec48U_Scan_Parameters * params,
			  Artec48U_Line_Reader ** reader_return);

/**
 * Destroy the Artec48U_Line_Reader object.
 *
 * @param reader  The Artec48U_Line_Reader object to destroy.
 */
static SANE_Status artec48u_line_reader_free (Artec48U_Line_Reader * reader);

/**
 * Read a scanline from the Artec48U_Line_Reader object.
 *
 * @param reader      The Artec48U_Line_Reader object.
 * @param buffer_pointers_return Array of pointers to image lines (1 or 3
 * elements)
 *
 * This function reads a full scanline from the device, unpacks it to internal
 * buffers and returns pointer to these buffers in @a
 * buffer_pointers_return[i].  For monochrome scan, only @a
 * buffer_pointers_return[0] is filled; for color scan, elements 0, 1, 2 are
 * filled with pointers to red, green, and blue data.  The returned pointers
 * are valid until the next call to artec48u_line_reader_read(), or until @a
 * reader is destroyed.
 *
 * @return
 * - SANE_STATUS_GOOD  - read completed successfully
 * - other error value - an error occured
 */
static SANE_Status
artec48u_line_reader_read (Artec48U_Line_Reader * reader,
			   unsigned int **buffer_pointers_return);

static SANE_Status
artec48u_download_firmware (Artec48U_Device * dev,
			    SANE_Byte * data, SANE_Word size);

static SANE_Status
artec48u_is_moving (Artec48U_Device * dev, SANE_Bool * moving);

static SANE_Status artec48u_carriage_home (Artec48U_Device * dev);

static SANE_Status artec48u_stop_scan (Artec48U_Device * dev);

static SANE_Status
artec48u_setup_scan (Artec48U_Scanner * s,
		     Artec48U_Scan_Request * request,
		     Artec48U_Scan_Action action,
		     SANE_Bool calculate_only,
		     Artec48U_Scan_Parameters * params);

static SANE_Status
artec48u_scanner_new (Artec48U_Device * dev,
		      Artec48U_Scanner ** scanner_return);

static SANE_Status artec48u_scanner_free (Artec48U_Scanner * scanner);

static SANE_Status
artec48u_scanner_start_scan (Artec48U_Scanner * scanner,
			     Artec48U_Scan_Request * request,
			     Artec48U_Scan_Parameters * params);

static SANE_Status
artec48u_scanner_read_line (Artec48U_Scanner * scanner,
			    unsigned int **buffer_pointers,
			    SANE_Bool shading);

static SANE_Status artec48u_scanner_stop_scan (Artec48U_Scanner * scanner);

static SANE_Status
artec48u_calculate_shading_buffer (Artec48U_Scanner * s, int start, int end,
				   int resolution, SANE_Bool color);

static SANE_Status download_firmware_file (Artec48U_Device * chip);

static SANE_Status
artec48u_generic_set_exposure_time (Artec48U_Device * dev,
				    Artec48U_Exposure_Parameters * params);

static SANE_Status
artec48u_generic_set_afe (Artec48U_Device * dev,
			  Artec48U_AFE_Parameters * params);

static SANE_Status artec48u_generic_start_scan (Artec48U_Device * dev);

static SANE_Status
artec48u_generic_read_scanned_data (Artec48U_Device * dev, SANE_Bool * ready);

static SANE_Status init_options (Artec48U_Scanner * s);

static SANE_Status load_calibration_data (Artec48U_Scanner * s);

static SANE_Status save_calibration_data (Artec48U_Scanner * s);
#endif
