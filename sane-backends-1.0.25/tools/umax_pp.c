/*
*  This program is free software; you can redistribute it and/or
*  modify it under the terms of the GNU General Public License
*  as published by the Free Software Foundation; either version
*  2 of the License, or (at your option) any later version.
*/

/* For putenv */
#define _XOPEN_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#define __MAIN__

#include "../backend/umax_pp_low.h"
#include "../backend/umax_pp_mid.h"

static void
Usage (char *name)
{
  fprintf (stderr,
	   "%s [-c color_mode] [-x coord] [-y coord] [-w width] [-h height] [-g gain] [-z offset] [-d dpi] [-t level] [-s] [-p] [-l 0|1] [-a ioport_addr] [-r]\n",
	   name);
}


int
main (int argc, char **argv)
{
  char dbgstr[80];
  int probe = 0;
  int port = 0;
  char *name = NULL;
  int scan = 0;
  int lamp = -1;
  int i, fd;
  int found;
  int recover = 0;
  int trace = 0;
  int maxw, maxh;

/* scanning parameters : defaults to preview (75 dpi color, full scan area) */
  int gain = 0x0;
  int offset = 0x646;
  int dpi = 75;
  int x = 0, y = 0;
  int width = -1, height = -1;
  int color = RGB_MODE;

  char **ports;
  int rc;


  /* option parsing */
  /*
     -c --color   : color mode: RGB, BW, BW12, RGB12
     -x           : x coordinate
     -y           : y coordinate
     -w --witdh   : scan width
     -h --height  : scan height
     -f --file    : session file
     -p --probe   : probe scanner
     -s --scan    : scan
     -t --trace   : execution trace
     -l --lamp    : turn lamp on/off 1/0
     -d --dpi     : set scan resolution
     -g --gain   : set RVB gain
     -z --offset: set offset
     -a --addr    : io port address
     -n --name    : ppdev device name
     -r           : recover from previous failed scan
     -m --model   : model revision
   */


  i = 1;
  trace = 0;
  sanei_umax_pp_setauto (1);
  while (i < argc)
    {
      found = 0;

      if ((strcmp (argv[i], "-p") == 0) || (strcmp (argv[i], "--probe") == 0))
	{
	  probe = 1;
	  found = 1;
	}

      if ((strcmp (argv[i], "-c") == 0) || (strcmp (argv[i], "--color") == 0))
	{
	  if (i == (argc - 1))
	    {
	      Usage (argv[0]);
	      fprintf (stderr, "expected color mode value\n");
	      return 0;
	    }
	  color = 0;
	  i++;
	  found = 1;
	  if (strcmp (argv[i], "RGB") == 0)
	    color = RGB_MODE;
	  if (strcmp (argv[i], "RGB12") == 0)
	    color = RGB12_MODE;
	  if (strcmp (argv[i], "BW") == 0)
	    color = BW_MODE;
	  if (strcmp (argv[i], "BW12") == 0)
	    color = BW12_MODE;
	  if (color == 0)
	    {
	      fprintf (stderr, "unexpected color mode value <%s>\n", argv[i]);
	      fprintf (stderr, "Must be RGB, RGB12, BW, or BW12\n");
	      return 0;
	    }
	}


      if (strcmp (argv[i], "-x") == 0)
	{
	  if (i == (argc - 1))
	    {
	      Usage (argv[0]);
	      fprintf (stderr, "expected x value\n");
	      return 0;
	    }
	  x = atoi (argv[i + 1]);
	  i++;
	  found = 1;
	}

      if (strcmp (argv[i], "-y") == 0)
	{
	  if (i == (argc - 1))
	    {
	      Usage (argv[0]);
	      fprintf (stderr, "expected y value\n");
	      return 0;
	    }
	  y = atoi (argv[i + 1]);
	  i++;
	  found = 1;
	}

      if ((strcmp (argv[i], "-w") == 0) || (strcmp (argv[i], "--witdh") == 0))
	{
	  if (i == (argc - 1))
	    {
	      Usage (argv[0]);
	      fprintf (stderr, "expected width value\n");
	      return 0;
	    }
	  width = atoi (argv[i + 1]);
	  i++;
	  found = 1;
	}

      if ((strcmp (argv[i], "-h") == 0)
	  || (strcmp (argv[i], "--height") == 0))
	{
	  if (i == (argc - 1))
	    {
	      Usage (argv[0]);
	      fprintf (stderr, "expected height value\n");
	      return 0;
	    }
	  height = atoi (argv[i + 1]);
	  i++;
	  found = 1;
	}

      if ((strcmp (argv[i], "-t") == 0) || (strcmp (argv[i], "--trace") == 0))
	{
	  if (i == (argc - 1))
	    {
	      Usage (argv[0]);
	      fprintf (stderr, "expected trace value\n");
	      return 0;
	    }
	  trace = atoi (argv[i + 1]);
	  i++;
	  found = 1;
	}


      if ((strcmp (argv[i], "-r") == 0)
	  || (strcmp (argv[i], "--recover") == 0))
	{
	  recover = 1;
	  found = 1;
	}

      if ((strcmp (argv[i], "-s") == 0) || (strcmp (argv[i], "--scan") == 0))
	{
	  scan = 1;
	  /* we have to probe again if we scan */
	  probe = 1;
	  found = 1;
	}

      if ((strcmp (argv[i], "-d") == 0) || (strcmp (argv[i], "--dpi") == 0))
	{
	  if (i == (argc - 1))
	    {
	      Usage (argv[0]);
	      fprintf (stderr, "expected dpi value\n");
	      return 0;
	    }
	  dpi = atoi (argv[i + 1]);
	  if ((dpi < 75) || (dpi > 1200))
	    {
	      fprintf (stderr, "dpi value has to be between 75 and 1200\n");
	      return 0;
	    }
	  if ((dpi != 75)
	      && (dpi != 150)
	      && (dpi != 300) && (dpi != 600) && (dpi != 1200))
	    {
	      fprintf (stderr,
		       "dpi value has to be 75, 150, 300, 600 or 1200\n");
	      return 0;
	    }
	  i++;
	  found = 1;
	}

      if ((strcmp (argv[i], "-g") == 0)
	  || (strcmp (argv[i], "--gain") == 0))
	{
	  if (i == (argc - 1))
	    {
	      Usage (argv[0]);
	      fprintf (stderr, "expected hex gain value ( ex: A59 )\n");
	      return 0;
	    }
	  i++;
	  found = 1;
	  if (strlen (argv[i]) != 3)
	    {
	      Usage (argv[0]);
	      fprintf (stderr, "expected hex gain value ( ex: A59 )\n");
	      return 0;
	    }
	  gain = strtol (argv[i], NULL, 16);
	  sanei_umax_pp_setauto (0);
	}

      if ((strcmp (argv[i], "-z") == 0)
	  || (strcmp (argv[i], "--offset") == 0))
	{
	  if (i == (argc - 1))
	    {
	      Usage (argv[0]);
	      fprintf (stderr, "expected hex offset value ( ex: A59 )\n");
	      return 0;
	    }
	  i++;
	  found = 1;
	  if (strlen (argv[i]) != 3)
	    {
	      Usage (argv[0]);
	      fprintf (stderr, "expected hex offset value ( ex: A59 )\n");
	      return 0;
	    }
	  offset = strtol (argv[i], NULL, 16);
	}

      if ((strcmp (argv[i], "-n") == 0) || (strcmp (argv[i], "--name") == 0))
	{
	  if (i == (argc - 1))
	    {
	      Usage (argv[0]);
	      fprintf (stderr,
		       "expected device name ( ex: /dev/parport0 )\n");
	      return 0;
	    }
	  i++;
	  found = 1;
	  name = argv[i];
	}
      if ((strcmp (argv[i], "-a") == 0) || (strcmp (argv[i], "--addr") == 0))
	{
	  if (i == (argc - 1))
	    {
	      Usage (argv[0]);
	      fprintf (stderr, "expected hex io port value ( ex: 3BC )\n");
	      return 0;
	    }
	  i++;
	  found = 1;
	  if ((strlen (argv[i]) < 3) || (strlen (argv[i]) > 4))
	    {
	      Usage (argv[0]);
	      fprintf (stderr, "expected hex io port value ( ex: 378 )\n");
	      return 0;
	    }
	  port = strtol (argv[i], NULL, 16);
	}


      if ((strcmp (argv[i], "-l") == 0) || (strcmp (argv[i], "--lamp") == 0))
	{
	  if (i == (argc - 1))
	    {
	      Usage (argv[0]);
	      fprintf (stderr, "expected lamp value\n");
	      return 0;
	    }
	  lamp = atoi (argv[i + 1]);
	  i++;
	  found = 1;
	}

      if (!found)
	{
	  Usage (argv[0]);
	  fprintf (stderr, "unexpected argument <%s>\n", argv[i]);
	  return 0;
	}

      /* next arg */
      i++;
    }

  /* since we use DBG, we have to set env var */
  /* according to the required trace level    */
  if (trace)
    {
      sprintf (dbgstr, "SANE_DEBUG_UMAX_PP_LOW=%d", trace);
      putenv (dbgstr);
    }

  /* no address or device given */
  rc = 0;
  if ((name == NULL) && (port == 0))
    {
      /* safe tests: user parallel port devices */
      ports = sanei_parport_find_device ();
      if (ports != NULL)
	{
	  i = 0;
	  rc = 0;
	  while ((ports[i] != NULL) && (rc != 1))
	    {
	      rc = sanei_umax_pp_initPort (port, ports[i]);
	      i++;
	    }
	}

      /* try for direct hardware access */
      if (rc != 1)
	{
	  ports = sanei_parport_find_port ();
	  i = 0;
	  rc = 0;
	  while ((ports[i] != NULL) && (rc != 1))
	    {
	      rc = sanei_umax_pp_initPort (strtol (ports[i], NULL, 16), NULL);
	      i++;
	    }
	}
      if (rc != 1)
	{
	  fprintf (stderr, "failed to detect a valid device or port!\n");
	  return 0;
	}
    }
  else
    {
      if (sanei_umax_pp_initPort (port, name) != 1)
	{
	  if (port)
	    fprintf (stderr, "failed to gain direct acces to port 0x%X!\n",
		     port);
	  else
	    fprintf (stderr, "failed to gain acces to device %s!\n", name);
	  return 0;
	}
    }
  if (trace)
    {
      printf
	("UMAX 610P/1220P/2000P scanning program version 6.4 starting ...\n");
#ifdef HAVE_LINUX_PPDEV_H
      printf ("ppdev character device built-in.\n");
#endif
#ifdef ENABLE_PARPORT_DIRECTIO
      printf ("direct hardware access built-in.\n");
#endif
    }


  /* scanning is the default behaviour */
  if ((!scan) && (lamp < 0) && (!probe))
    scan = 1;


  /* probe scanner */
  if ((probe) || (lamp >= 0))
    {
      printf ("Probing scanner ....\n");
      if (sanei_umax_pp_probeScanner (recover) != 1)
	{
	  if (recover)
	    {
	      sanei_umax_pp_initTransport (recover);
	      sanei_umax_pp_endSession ();
	      if (sanei_umax_pp_probeScanner (recover) != 1)
		{
		  printf ("Recover failed ....\n");
		  return 0;
		}
	      printf ("Recover done !\n");
	    }
	  else
	    return 0;
	}

      /* could be written better .... but it is only test */
      sanei_umax_pp_endSession ();

      /* init transport layer */
      if (sanei_umax_pp_initTransport (0) != 1)
	{
	  printf ("initTransport() failed (%s:%d)\n", __FILE__, __LINE__);
	  return 0;
	}
      i = sanei_umax_pp_checkModel ();
      if (i < 600)
	{
	  sanei_umax_pp_endSession ();
	  printf ("checkModel() failed (%s:%d)\n", __FILE__, __LINE__);
	  return 0;
	}
      printf ("UMAX Astra %dP detected \n", i);

      /* free scanner if a scan is planned */
      if (scan)
	sanei_umax_pp_endSession ();
      printf ("Done ....\n");
    }

  /* lamp on/off: must come after probing (610p handling) */
  if (lamp >= 0)
    {
      /* init transport layer */
      if (trace)
	printf ("Tryning to set lamp %s\n", lamp ? "on" : "off");
      if (sanei_umax_pp_initTransport (recover) != 1)
	{
	  printf ("initTransport() failed (%s:%d)\n", __FILE__, __LINE__);
	  return 0;
	}
      else
	{
	  if (trace)
	    printf ("initTransport passed...\n");
	}
      if (sanei_umax_pp_setLamp (lamp) == 0)
	{
	  fprintf (stderr, "Setting lamp state failed!\n");
	  return 0;
	}
      else
	{
	  if (trace)
	    printf ("sanei_umax_pp_setLamp passed...\n");
	}
    }

  /* scan */
  if (scan)
    {
      printf ("Scanning ....\n");
      if (sanei_umax_pp_getastra () < 1210)
	{
	  maxw = 2550;
	  maxh = 3500;
	}
      else
	{
	  maxw = 5100;
	  maxh = 7000;
	}
      if (width < 0)
	width = maxw;
      if (height < 0)
	height = maxh;

      if ((width < 1) || (width > maxw))
	{
	  fprintf (stderr, "width must be between 1 and %d\n", maxw);
	  return 0;
	}
      if (x + width > maxw)
	{
	  fprintf (stderr,
		   "Right side of scan area exceed physical limits (x+witdh>%d)\n",
		   maxw);
	  return 0;
	}
      if (y < 0 || y > maxh)
	{
	  fprintf (stderr, "y must be between 0 and %d\n", maxh - 1);
	  return 0;
	}
      if (x < 0 || x > maxw)
	{
	  fprintf (stderr, "x must be between 0 and %d\n", maxw - 1);
	  return 0;
	}
      if ((height < 1) || (height > maxh))
	{
	  fprintf (stderr, "height must be between 1 and %d\n", maxh);
	  return 0;
	}
      if (y + height > maxh)
	{
	  fprintf (stderr,
		   "Bottom side of scan area exceed physical limits (y+height>%d)\n",
		   maxh);
	  return 0;
	}

      /* init transport layer */
      /* 0: failed
         1: success
         2: retry
       */
      do
	{
	  i = sanei_umax_pp_initTransport (recover);
	}
      while (i == 2);
      if (i != 1)
	{
	  printf ("initTransport() failed (%s:%d)\n", __FILE__, __LINE__);
	  return 0;
	}
      /* init scanner */
      if (sanei_umax_pp_initScanner (recover) == 0)
	{
	  sanei_umax_pp_endSession ();
	  return 0;
	}

      /* set x origin left to right */
      x = sanei_umax_pp_getLeft () + (maxw - x) - width;

      /* scan */
      if (sanei_umax_pp_scan
	  (x, y, width, height, dpi, color, gain, offset) != 1)
	{
	  sanei_umax_pp_endSession ();
	  return 0;
	}

      /* wait for head parking */
      sanei_umax_pp_parkWait ();
      printf ("Done ....\n");
    }
  sanei_umax_pp_endSession ();
#ifdef HAVE_LINUX_PPDEV_H
  fd = sanei_umax_pp_getparport ();
  if (fd > 0)
    {
      close (fd);
      sanei_umax_pp_setparport (0);
    }
#endif

  return 1;
}
