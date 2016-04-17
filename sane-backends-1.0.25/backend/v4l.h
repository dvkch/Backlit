/* sane - Scanner Access Now Easy.
   Copyright (C) 1997 David Mosberger-Tang
   Updates and bugfixes (C) 2002. 2003 Henning Meier-Geinitz

   This file is part of the SANE package.

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
*/

#ifndef v4l_h
#define v4l_h

#ifndef __LINUX_VIDEODEV_H
/* Kernel interface */
/* Only the stuff we need. For more features, more defines are needed */

#define VID_TYPE_CAPTURE	1	/* Can capture */
#define VID_TYPE_TUNER		2	/* Can tune */
#define VID_TYPE_TELETEXT	4	/* Does teletext */
#define VID_TYPE_OVERLAY	8	/* Overlay onto frame buffer */
#define VID_TYPE_CHROMAKEY	16	/* Overlay by chromakey */
#define VID_TYPE_CLIPPING	32	/* Can clip */
#define VID_TYPE_FRAMERAM	64	/* Uses the frame buffer memory */
#define VID_TYPE_SCALES		128	/* Scalable */
#define VID_TYPE_MONOCHROME	256	/* Monochrome only */
#define VID_TYPE_SUBCAPTURE	512	/* Can capture subareas of the image */
#define VID_TYPE_MPEG_DECODER	1024	/* Can decode MPEG streams */
#define VID_TYPE_MPEG_ENCODER	2048	/* Can encode MPEG streams */
#define VID_TYPE_MJPEG_DECODER	4096	/* Can decode MJPEG streams */
#define VID_TYPE_MJPEG_ENCODER	8192	/* Can encode MJPEG streams */

struct video_capability
{
	char name[32];
	int type;
	int channels;	/* Num channels */
	int audios;	/* Num audio devices */
	int maxwidth;	/* Supported width */
	int maxheight;	/* And height */
	int minwidth;	/* Supported width */
	int minheight;	/* And height */
};

struct video_picture
{
	__u16	brightness;
	__u16	hue;
	__u16	colour;
	__u16	contrast;
	__u16	whiteness;	/* Black and white only */
	__u16	depth;		/* Capture depth */
	__u16   palette;	/* Palette in use */
#define VIDEO_PALETTE_GREY	1	/* Linear greyscale */
#define VIDEO_PALETTE_HI240	2	/* High 240 cube (BT848) */
#define VIDEO_PALETTE_RGB565	3	/* 565 16 bit RGB */
#define VIDEO_PALETTE_RGB24	4	/* 24bit RGB */
#define VIDEO_PALETTE_RGB32	5	/* 32bit RGB */	
#define VIDEO_PALETTE_RGB555	6	/* 555 15bit RGB */
#define VIDEO_PALETTE_YUV422	7	/* YUV422 capture */
#define VIDEO_PALETTE_YUYV	8
#define VIDEO_PALETTE_UYVY	9	/* The great thing about standards is ... */
#define VIDEO_PALETTE_YUV420	10
#define VIDEO_PALETTE_YUV411	11	/* YUV411 capture */
#define VIDEO_PALETTE_RAW	12	/* RAW capture (BT848) */
#define VIDEO_PALETTE_YUV422P	13	/* YUV 4:2:2 Planar */
#define VIDEO_PALETTE_YUV411P	14	/* YUV 4:1:1 Planar */
#define VIDEO_PALETTE_YUV420P	15	/* YUV 4:2:0 Planar */
#define VIDEO_PALETTE_YUV410P	16	/* YUV 4:1:0 Planar */
#define VIDEO_PALETTE_PLANAR	13	/* start of planar entries */
#define VIDEO_PALETTE_COMPONENT 7	/* start of component entries */
};

struct video_window
{
	__u32	x,y;			/* Position of window */
	__u32	width,height;		/* Its size */
	__u32	chromakey;
	__u32	flags;
	struct	video_clip *clips;	/* Set only */
	int	clipcount;
#define VIDEO_WINDOW_INTERLACE	1
#define VIDEO_WINDOW_CHROMAKEY	16	/* Overlay by chromakey */
#define VIDEO_CLIP_BITMAP	-1
/* bitmap is 1024x625, a '1' bit represents a clipped pixel */
#define VIDEO_CLIPMAP_SIZE	(128 * 625)
};

#define VIDEO_MAX_FRAME		32

struct video_mbuf
{
	int	size;		/* Total memory to map */
	int	frames;		/* Frames */
	int	offsets[VIDEO_MAX_FRAME];
};
	
struct video_mmap
{
	unsigned	int frame;		/* Frame (0 - n) for double buffer */
	int		height,width;
	unsigned	int format;		/* should be VIDEO_PALETTE_* */
};

struct video_channel
{
	int channel;
	char name[32];
	int tuners;
	__u32  flags;
#define VIDEO_VC_TUNER		1	/* Channel has a tuner */
#define VIDEO_VC_AUDIO		2	/* Channel has audio */
	__u16  type;
#define VIDEO_TYPE_TV		1
#define VIDEO_TYPE_CAMERA	2	
	__u16 norm;			/* Norm set by channel */
};

#define VIDIOCGCAP		_IOR('v',1,struct video_capability)	/* Get capabilities */
#define VIDIOCGCHAN		_IOWR('v',2,struct video_channel)	/* Get channel info (sources) */
#define VIDIOCSCHAN		_IOW('v',3,struct video_channel)	/* Set channel 	*/
#define VIDIOCGTUNER		_IOWR('v',4,struct video_tuner)		/* Get tuner abilities */
#define VIDIOCSTUNER		_IOW('v',5,struct video_tuner)		/* Tune the tuner for the current channel */
#define VIDIOCGPICT		_IOR('v',6,struct video_picture)	/* Get picture properties */
#define VIDIOCSPICT		_IOW('v',7,struct video_picture)	/* Set picture properties */
#define VIDIOCCAPTURE		_IOW('v',8,int)				/* Start, end capture */
#define VIDIOCGWIN		_IOR('v',9, struct video_window)	/* Get the video overlay window */
#define VIDIOCSWIN		_IOW('v',10, struct video_window)	/* Set the video overlay window - passes clip list for hardware smarts , chromakey etc */
#define VIDIOCGFBUF		_IOR('v',11, struct video_buffer)	/* Get frame buffer */
#define VIDIOCSFBUF		_IOW('v',12, struct video_buffer)	/* Set frame buffer - root only */
#define VIDIOCKEY		_IOR('v',13, struct video_key)		/* Video key event - to dev 255 is to all - cuts capture on all DMA windows with this key (0xFFFFFFFF == all) */
#define VIDIOCGFREQ		_IOR('v',14, unsigned long)		/* Set tuner */
#define VIDIOCSFREQ		_IOW('v',15, unsigned long)		/* Set tuner */
#define VIDIOCGAUDIO		_IOR('v',16, struct video_audio)	/* Get audio info */
#define VIDIOCSAUDIO		_IOW('v',17, struct video_audio)	/* Audio source, mute etc */
#define VIDIOCSYNC		_IOW('v',18, int)			/* Sync with mmap grabbing */
#define VIDIOCMCAPTURE		_IOW('v',19, struct video_mmap)		/* Grab frames */
#define VIDIOCGMBUF		_IOR('v',20, struct video_mbuf)		/* Memory map buffer info */
#define VIDIOCGUNIT		_IOR('v',21, struct video_unit)		/* Get attached units */
#define VIDIOCGCAPTURE		_IOR('v',22, struct video_capture)	/* Get subcapture */
#define VIDIOCSCAPTURE		_IOW('v',23, struct video_capture)	/* Set subcapture */
#define VIDIOCSPLAYMODE		_IOW('v',24, struct video_play_mode)	/* Set output video mode/feature */
#define VIDIOCSWRITEMODE	_IOW('v',25, int)			/* Set write mode */
#define VIDIOCGPLAYINFO		_IOR('v',26, struct video_info)		/* Get current playback info from hardware */
#define VIDIOCSMICROCODE	_IOW('v',27, struct video_code)		/* Load microcode into hardware */
#define	VIDIOCGVBIFMT		_IOR('v',28, struct vbi_format)		/* Get VBI information */
#define	VIDIOCSVBIFMT		_IOW('v',29, struct vbi_format)		/* Set VBI information */


/* end of kernel interface */
#endif /* !__LINUX_VIDEODEV_H */

#include <../include/sane/sane.h>

#define MAX_CHANNELS 32

typedef enum
{
  V4L_RES_LOW = 0,
  V4L_RES_HIGH
}
V4L_Resolution;

typedef enum
{
  OPT_NUM_OPTS = 0,

  OPT_MODE_GROUP,
  OPT_MODE,
  OPT_CHANNEL,

  OPT_GEOMETRY_GROUP,
  OPT_TL_X,			/* top-left x */
  OPT_TL_Y,			/* top-left y */
  OPT_BR_X,			/* bottom-right x */
  OPT_BR_Y,			/* bottom-right y */

  OPT_ENHANCEMENT_GROUP,
  OPT_BRIGHTNESS,
  OPT_HUE,
  OPT_COLOR,
  OPT_CONTRAST,
  OPT_WHITE_LEVEL,

  /* must come last: */
  NUM_OPTIONS
}
V4L_Option;

typedef struct V4L_Device
{
  struct V4L_Device *next;
  SANE_Device sane;
}
V4L_Device;

typedef struct V4L_Scanner
{
  struct V4L_Scanner *next;

  SANE_Option_Descriptor opt[NUM_OPTIONS];
  Option_Value val[NUM_OPTIONS];
  V4L_Resolution resolution;
  SANE_Parameters params;
  SANE_String_Const devicename;	/* Name of the Device */
  int fd;			/* Filedescriptor */
  SANE_Int user_corner;		/* bitmask of user-selected coordinates */
  SANE_Bool scanning;
  SANE_Bool deliver_eof;
  SANE_Bool is_mmap;		/* Do we use mmap ? */
  /* state for reading a frame: */
  size_t num_bytes;		/* # of bytes read so far */
  size_t bytes_per_frame;	/* total number of bytes in frame */
  struct video_capability capability;
  struct video_picture pict;
  struct video_window window;
  struct video_mbuf mbuf;
  struct video_mmap mmap;
  SANE_String_Const channel[MAX_CHANNELS];
  SANE_Int buffercount;
}
V4L_Scanner;

#endif /* v4l_h */
