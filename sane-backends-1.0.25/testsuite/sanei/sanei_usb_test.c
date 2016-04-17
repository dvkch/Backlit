#include "../../include/sane/config.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <stddef.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_MKDIR
#include <sys/stat.h>
#include <sys/types.h>
#endif

#include <assert.h>

#define BACKEND_NAME	sanei_usb

#include "../../include/sane/sane.h"
#include "../../include/sane/sanei.h"
#include "../../include/sane/saneopts.h"

#include "../../include/sane/sanei_backend.h"
#include "../../include/sane/sanei_usb.h"

#include "../../include/_stdint.h"

/*
 * In order to avoid modifying sanei_usb.c to allow for unit tests
 * we include it so we can use its private variables and structures
 * and still test the code.
 */
#include "../../sanei/sanei_usb.c"


/** test sanei_usb_init()
 * calls sanei_usb_init
 * @param expected expected use count
 * @return 1 on success, else 0
 */
static int
test_init (int expected)
{
  /* initialize USB */
  printf ("%s starting ...\n", __FUNCTION__);
  sanei_usb_init ();
  if (initialized == 0)
    {
      printf ("ERROR: sanei_usb not initialized!\n");
      return 0;
    }
  if (initialized != expected)
    {
      printf ("ERROR: incorrect use count, expected %d, got %d!\n", expected,
	      initialized);
      return 0;
    }

  printf ("sanei_usb initialized, use count is %d ...\n", initialized);
  printf ("%s success\n\n", __FUNCTION__);
  return 1;
}

/** test sanei_usb_exit()
 * calls sanei_usb_exit
 * @param expected use count after exit call
 * @return 1 on success, else 0
 */
static int
test_exit (int expected)
{
  printf ("%s starting ...\n", __FUNCTION__);

  /* end of USB use test */
  sanei_usb_exit ();
  if (initialized != expected)
    {
      printf ("ERROR: incorrect use count, expected %d, got %d!\n", expected,
	      initialized);
      return 0;
    }

  printf ("%s success\n\n", __FUNCTION__);
  return 1;
}


/** count detected devices
 * count all detected devices and check it against expected value
 * @param expected detected count
 * @return 1 on success, else 0
 */
static int
count_detected (int expected)
{
  int num = 0;
  int i;

  for (i = 0; i < device_number; i++)
    {
      if (devices[i].missing == 0 && devices[i].devname != NULL)
	{
	  num++;
	}
    }
  if (num != expected)
    {
      printf ("ERROR: %d detected devices, expected %d!\n", num, expected);
      return 0;
    }
  printf ("%d devices still detected.\n", num);
  return 1;
}

/** create mock device
 * create a mock device entry
 * @param device device pointer to fill with mock data
 * @return nothing
 */
static void
create_mock_device (char *devname, device_list_type * device)
{
  memset (device, 0, sizeof (device_list_type));
  device->devname = strdup (devname);
  device->vendor = 0xdead;
  device->product = 0xbeef;
#if defined(HAVE_LIBUSB) || defined(HAVE_LIBUSB_1_0)
  device->method = sanei_usb_method_libusb;
#endif
#ifdef HAVE_USBCALLS
  device->method = sanei_usb_method_usbcalls;
#endif
#if !defined(HAVE_LIBUSB) && !defined(HAVE_LIBUSB_1_0) && !defined(HAVE_USBCALLS)
  device->method == sanei_usb_method_scanner_driver;
#endif
}

/** test store_device
 * test store_device for corner cases not covered by the
 * other regular use by sanei_usb_scan_devices
 * the startiing situation is that the mock device has never
 * put into device list.
 * @return 1 on success, else 0
 */
static int
test_store_device (void)
{
  int current_number;
  int expected;
  int i;
  int found;
  device_list_type mock;

  create_mock_device ("mock", &mock);

  /* first test store when there is no more room
   * to store device */
  current_number = device_number;
  device_number = MAX_DEVICES;
  /* give unused devices a name so strcmp() won't crash. */
  for (i = current_number; i < MAX_DEVICES; i++)
    devices[i].devname = "";

  store_device (mock);
  /* there should be no more devices */
  if (device_number > MAX_DEVICES)
    {
      printf ("ERROR: store past end of device list!\n");
      return 0;
    }
  /* walk device list to be sure mock device hasn't been stored */
  for (i = 0; i < MAX_DEVICES; i++)
    {
      if (devices[i].devname && !strcmp (devices[i].devname, mock.devname))
	{
	  printf
	    ("ERROR: device stored although there were no place for it!\n");
	  return 0;
	}
    }

  /* restore device_number */
  device_number = current_number;
  /* reset unused devnames to NULL */
  for (i = current_number; i < MAX_DEVICES; i++)
    devices[i].devname = NULL;
  expected = device_number + 1;

  /* store mock device */
  store_device (mock);
  found = 0;
  for (i = 0; i < MAX_DEVICES && !found; i++)
    {
      if (devices[i].devname && !strcmp (devices[i].devname, mock.devname))
	{
	  found = 1;
	}
    }
  if (device_number != expected || !found)
    {
      printf ("ERROR: mock device not stored !\n");
      return 0;
    }

  /* scan devices should mark it as missing, and device_number should decrease */
  sanei_usb_scan_devices ();
  found = 0;
  for (i = 0; i < MAX_DEVICES && !found; i++)
    {
      if (devices[i].devname
	  && devices[i].missing == 1
	  && !strcmp (devices[i].devname, mock.devname))
	{
	  found = 1;
	}
    }
  if (device_number != expected || !found)
    {
      printf ("ERROR: mock device still present !\n");
      return 0;
    }

  /* second scan devices should mark missing to 2 */
  sanei_usb_scan_devices ();
  found = 0;
  for (i = 0; i < MAX_DEVICES && !found; i++)
    {
      if (devices[i].devname
	  && devices[i].missing == 2
	  && !strcmp (devices[i].devname, mock.devname))
	{
	  found = 1;
	}
    }
  if (device_number != expected || !found)
    {
      printf ("ERROR: mock device slot not reusable !\n");
      return 0;
    }

  /* store mock device again, slot in devices should be reused
   * and device_number shouldn't change */
  create_mock_device ("mock2", &mock);
  store_device (mock);
  found = 0;
  for (i = 0; i < MAX_DEVICES && !found; i++)
    {
      if (devices[i].devname && !strcmp (devices[i].devname, mock.devname))
	{
	  found = 1;
	}
    }
  if (device_number != expected || !found)
    {
      printf ("ERROR: mock device not stored !\n");
      return 0;
    }

  /* last rescan to wipe mock device out */
  sanei_usb_scan_devices ();

  return 1;
}

/** return count of opened devices
 * @return count of opened devices
 */
static int
get_opened (void)
{
  int num = 0;
  int i;

  for (i = 0; i < device_number; i++)
    {
      if (devices[i].missing == 0 && devices[i].devname != NULL
	  && devices[i].open == SANE_TRUE)
	{
	  num++;
	}
    }
  return num;
}

/** count opened devices
 * count all opended devices and check it against expected value
 * @param expected use opened count
 * @return 1 on success, else 0
 */
static int
count_opened (int expected)
{
  int num = get_opened();

  if (num != expected)
    {
      printf ("ERROR: %d opened devices, expected %d!\n", num, expected);
      return 0;
    }
  printf ("%d devices still opened.\n", num);
  return 1;
}

/** open all devices
 * loop on all existing devices and open them
 * @param dn array to store opened device number
 * @param expected number of devices to be opened
 * @return 1 on success, else 0
 */
static int
test_open_all (SANE_Int * dn, int expected)
{
  int opened = 0;
  int i;
  int last;
  SANE_Status status;

  /* loop on detected devices and open them */
  last = -1;
  for (i = 0; i < device_number; i++)
    {
      if (devices[i].missing == 0 && devices[i].devname != NULL)
	{
	  /* open device */
	  status = sanei_usb_open (devices[i].devname, dn + opened);
	  if (status == SANE_STATUS_GOOD)
	    {
	      opened++;
	      last = i;
	    }
	  else
	    {
              if (status == SANE_STATUS_ACCESS_DENIED ||
                  status == SANE_STATUS_DEVICE_BUSY)
                {
                  expected--;
                }
              else
                {
	          printf ("ERROR: couldn't open device %s!\n",
                          devices[i].devname);
	          return 0;
                }
	    }
	}
    }
  printf ("opened %d devices\n", opened);

  /* try to reopen an opened device when there is one */
  if (last >= 0)
    {
      status = sanei_usb_open (devices[last].devname, dn + opened);
      if (status == SANE_STATUS_GOOD)
	{
	  printf ("ERROR: unexpected success when opening %s twice!\n",
		  devices[last].devname);
	  return 0;
	}
    }

  /* there should be as many opened devices than detected devices */
  return count_opened (expected);
}

/** test opening invalid device
 * try to open an non existing device
 * @return 1 on success, else 0
 */
static int
test_open_invalid (void)
{
  SANE_Status status;
  SANE_Int dn;

  status = sanei_usb_open ("invalid device", &dn);
  if (status == SANE_STATUS_GOOD)
    {
      printf ("ERROR: unexpected success opening invalid device!\n");
      return 0;
    }
  return 1;
}

/** close all devices
 * loop on all opened devices and close them
 * @param dn array of opened device number
 * @param expected number of devices to be closed
 * @return 1 on success, else 0
 */
static int
test_close_all (SANE_Int * dn, int expected)
{
  int closed = 0;
  int i;

  /* loop on detected devices and open them */
  for (i = 0; i < expected; i++)
    {
      /* close device */
      sanei_usb_close (dn[i]);
      closed++;
    }
  printf ("closed %d devices\n", closed);

  /* there should be any more opened devices */
  return count_opened (0);
}


/** claim all open devices
 * loop on all opened devices and claim interface 0
 * @param dn array of opened device number
 * @param expected number of devices to be claimed
 * @return 1 on success, else 0
 */
static int
test_claim_all (SANE_Int * dn, int expected)
{
  int claimed = 0;
  int i;
  SANE_Status status;
  device_list_type mock;

  claimed = 0;
  for (i = 0; i < expected; i++)
    {
      status = sanei_usb_claim_interface (dn[i], devices[dn[i]].interface_nr);
      if (status != SANE_STATUS_GOOD)
	{
	  printf ("ERROR: couldn't claim interface 0 on device %d!\n", dn[i]);
	}
      else
	{
	  claimed++;
	}
    }
  if (claimed != expected)
    {
      printf ("ERROR: expected %d claimed interfaces, got %d!\n", expected,
	      claimed);
      return 0;
    }
  printf ("%d devices claimed...\n\n", claimed);

  /* try to claim invalid device entry */
  status = sanei_usb_claim_interface (device_number, 0);
  if (status == SANE_STATUS_GOOD)
    {
      printf ("ERROR: could claim interface 0 on invalid device!\n");
      return 0;
    }

  /* create a mock device and make it missing by rescanning */
  create_mock_device ("mock", &mock);
  store_device (mock);
  sanei_usb_scan_devices ();

  /* try to claim interface on missing device */
  status = sanei_usb_claim_interface (device_number - 1, 0);
  if (status == SANE_STATUS_GOOD)
    {
      printf ("ERROR: could claim interface 0 on invalid device!\n");
      return 0;
    }

  /* remove mock device */
  device_number--;
  free (devices[device_number].devname);
  devices[device_number].devname = NULL;

  return 1;
}


/** release all claimed devices
 * loop on all opened devices and claim interface 0
 * @param dn array of opened device number
 * @param expected number of devices to be claimed
 * @return 1 on success, else 0
 */
static int
test_release_all (SANE_Int * dn, int expected)
{
  int released = 0;
  int i;
  SANE_Status status;
  device_list_type mock;

  released = 0;
  for (i = 0; i < expected; i++)
    {
      status =
	sanei_usb_release_interface (dn[i], devices[dn[i]].interface_nr);
      if (status != SANE_STATUS_GOOD)
	{
	  printf ("ERROR: couldn't release interface 0 on device %d!\n",
		  dn[i]);
	}
      else
	{
	  released++;
	}
    }
  if (released != expected)
    {
      printf ("ERROR: expected %d released interfaces, got %d!\n", expected,
	      released);
      return 0;
    }
  printf ("%d devices released...\n\n", released);

  /* try to release invalid device entry */
  status = sanei_usb_release_interface (device_number, 0);
  if (status == SANE_STATUS_GOOD)
    {
      printf ("ERROR: could release interface 0 on invalid device!\n");
      return 0;
    }

  /* create a mock device and make it missing by rescanning */
  create_mock_device ("mock", &mock);
  store_device (mock);
  sanei_usb_scan_devices ();

  /* try to claim interface on missing device */
  status = sanei_usb_release_interface (device_number - 1, 0);
  if (status == SANE_STATUS_GOOD)
    {
      printf ("ERROR: could release interface 0 on invalid device!\n");
      return 0;
    }

  /* remove mock device */
  device_number--;
  free (devices[device_number].devname);
  devices[device_number].devname = NULL;

  return 1;
}

/** get id for all devices names
 * loop on all existing devices and get vendor
 * and product id by name.
 * @param expected count
 * @return 1 on success, else 0
 */
static int
test_vendor_by_devname (void)
{
  int i;
  SANE_Status status;
  SANE_Word vendor, product;
  device_list_type mock;

  /* loop on detected devices and open them */
  for (i = 0; i < device_number; i++)
    {
      if (devices[i].missing == 0 && devices[i].devname != NULL)
	{
	  /* get device id */
	  status = sanei_usb_get_vendor_product_byname (devices[i].devname,
							&vendor, &product);
	  if (status != SANE_STATUS_GOOD)
	    {
	      printf ("ERROR: couldn't query device %s!\n",
		      devices[i].devname);
	      return 0;
	    }
	  if (vendor == 0 || product == 0)
	    {
	      printf ("ERROR: incomplete device id for %s!\n",
		      devices[i].devname);
	      return 0;
	    }
	  printf ("%s is %04x:%04x\n", devices[i].devname, vendor, product);
	}
    }

  /* add mock device */
  create_mock_device ("mock", &mock);
  store_device (mock);
  status = sanei_usb_get_vendor_product_byname ("mock", &vendor, &product);
  if (status != SANE_STATUS_GOOD)
    {
      printf ("ERROR: getting vendor for mock devname!\n");
      return 0;
    }
  if (vendor != mock.vendor || product != mock.product)
    {
      printf ("ERROR: wrong vendor/product for mock devname!\n");
      return 0;
    }
  /* remove mock device */
  device_number--;
  free (devices[device_number].devname);
  devices[device_number].devname = NULL;

  /* try go get id for an invalid devname */
  status = sanei_usb_get_vendor_product_byname ("invalid devname",
						&vendor, &product);
  if (status == SANE_STATUS_GOOD)
    {
      printf ("ERROR: unexpected success getting id for invalid devname!\n");
      return 0;
    }

  printf ("\n");
  return 1;
}

/** get vendor for all devices id
 * loop on all existing devices and get vendor
 * and product id.
 * @param expected count
 * @return 1 on success, else 0
 */
static int
test_vendor_by_id (void)
{
  int i;
  SANE_Status status;
  SANE_Word vendor, product;
  device_list_type mock;

  /* loop on detected devices and open them */
  for (i = 0; i < device_number; i++)
    {
      if (devices[i].missing == 0 && devices[i].devname != NULL)
	{
	  /* get device id */
	  status = sanei_usb_get_vendor_product (i, &vendor, &product);
	  if (status != SANE_STATUS_GOOD)
	    {
	      printf ("ERROR: couldn't query device %d!\n", i);
	      return 0;
	    }
	  if (vendor == 0 || product == 0)
	    {
	      printf ("ERROR: incomplete device id for %d!\n", i);
	      return 0;
	    }
	  printf ("%d is %04x:%04x\n", i, vendor, product);
	}
    }

  /* add mock device */
  create_mock_device ("mock", &mock);
  store_device (mock);
  status =
    sanei_usb_get_vendor_product (device_number - 1, &vendor, &product);
  if (status != SANE_STATUS_GOOD)
    {
      printf ("ERROR: getting vendor for mock devname!\n");
      return 0;
    }
  if (vendor != mock.vendor || product != mock.product)
    {
      printf ("ERROR: wrong vendor/product for mock devname!\n");
      return 0;
    }
  /* remove mock device */
  device_number--;
  free (devices[device_number].devname);
  devices[device_number].devname = NULL;

  /* try go get id for an invalid id */
  status =
    sanei_usb_get_vendor_product (device_number + 1, &vendor, &product);
  if (status == SANE_STATUS_GOOD)
    {
      printf
	("ERROR: unexpected success getting vendor for invalid devname!\n");
      return 0;
    }

  printf ("\n");
  return 1;
}

/** test timeout functions : libusb only
 * @return 1 on success, else 0
 */
static int
test_timeout (void)
{
#if defined(HAVE_LIBUSB) || defined(HAVE_LIBUSB_1_0)
  int timeout = libusb_timeout;

  sanei_usb_set_timeout (5000);
  if (libusb_timeout != 5000)
    {
      printf ("ERROR: failed to set timeout\n");
      return 1;
    }
  sanei_usb_set_timeout (timeout);
#endif
  return 1;
}

/** test device scanning
 * call sanei_usb_scan_devices, since it has no return code, no real
 * assert can be done, but at least we can test it doesn't break
 * other functions or don't leak memory
 * @return always 1
 */
static int
test_scan_devices (int detected, int opened)
{
  int rc;

  printf ("rescanning for devices ...\n");
  sanei_usb_scan_devices ();
  rc = count_detected (detected);
  if (!rc)
    {
      printf ("ERROR: scanning devices change detected count!\n");
      return 0;
    }
  rc = count_opened (opened);
  if (!rc)
    {
      printf ("ERROR: scanning devices change opened count!\n");
      return 0;
    }
  printf ("\n");
  return 1;
}


/**
 * flag for dummy attach
 */
static int dummy_flag;

/**
 * expected device name during attach
 */
static char *expected_device;

/** dummy attach function
 * dummy attach function
 * @return resturn SANE_STATUS_GOOD
 */
static SANE_Status
dummy_attach (const char *dev)
{
  dummy_flag = (strcmp (expected_device, dev) == 0);
  if (dummy_flag)
    {
      printf ("success attaching to %s...\n", dev);
    }
  else
    {
      printf ("failed attaching to %s...\n", dev);
    }
  return SANE_STATUS_GOOD;
}

/** test attaching usb device
 * create a mock device and attach to it, checking
 * if it is ok
 * @return 1 on success, else 0
 */
static int
test_attach (void)
{
  device_list_type mock;

  /* add mock device and try ot attach to it */
  dummy_flag = 0;
  create_mock_device ("mock", &mock);
  expected_device = mock.devname;
  store_device (mock);
  sanei_usb_attach_matching_devices ("usb 0xdead 0xbeef", dummy_attach);

  /* flag must be set */
  if (dummy_flag != 1)
    {
      printf ("ERROR: couldn't attach to 'usb xdead 0xbeef' device!\n");
      return 0;
    }

  /* attach by devname */
  dummy_flag = 0;
  sanei_usb_attach_matching_devices (mock.devname, dummy_attach);
  /* flag must be set */
  if (dummy_flag != 1)
    {
      printf ("ERROR: couldn't attach to 'mock' device!\n");
      return 0;
    }

  /* attach to bogus device */
  dummy_flag = 0;
  sanei_usb_attach_matching_devices ("usb 0x0001 0x0001", dummy_attach);

  /* flag must not be set */
  if (dummy_flag != 0)
    {
      printf ("ERROR: shouldn't be attached to bogus device!\n");
      return 0;
    }

  /* attach by bogus devname */
  sanei_usb_attach_matching_devices ("bogus", dummy_attach);

  /* flag must not be set */
  if (dummy_flag != 0)
    {
      printf ("ERROR: shouldn't be attached to bogus device!\n");
      return 0;
    }

  /* remove mock device */
  device_number--;
  free (devices[device_number].devname);
  devices[device_number].devname = NULL;
  dummy_flag = 0;

  return 1;
}

int
main (int argc, char **argv)
{
  int detected, opened, i;
  SANE_Int dn[MAX_DEVICES];

#ifdef HAVE_LIBUSB
  printf ("\n%s built with old libusb\n\n", argv[0]);
#endif
#ifdef HAVE_LIBUSB_1_0
  printf ("\n%s built with libusb-1.0\n\n", argv[0]);
#endif
#ifdef HAVE_USBCALLS
  printf ("\n%s built with usbcalls\n\n", argv[0]);
#endif
#if !defined(HAVE_LIBUSB) && !defined(HAVE_LIBUSB_1_0) && !defined(HAVE_USBCALLS)
  printf ("\n%s relying on deprecated scanner kernel module\n", argv[0]);
#endif

  /* start sanei_usb */
  assert (test_init (1));

  /* test timeout function */
  assert (test_timeout ());

  /* count available devices */
  detected = 0;
  for (i = 0; i < device_number; i++)
    {
      if (devices[i].missing == 0 && devices[i].devname != NULL)
	{
	  detected++;
	}
    }
  printf ("%d devices found.\n", detected);

  /* rescan devices : detected count shouldn't change */
  assert (test_scan_devices (detected, 0));

  /* test corner cases with mock device */
  assert (test_store_device ());

  /* get vendor/product id for all available devices devname */
  assert (test_vendor_by_devname ());

  /* get vendor/product id for all available devices id */
  assert (test_vendor_by_id ());

  /* open all available devices */
  assert (test_open_all (dn, detected));

  opened = get_opened();

  /* rescan devices : detected and opened count shouldn't change */
  assert (test_scan_devices (detected, opened));

  /* try to open an inexisting device */
  assert (test_open_invalid ());

  /* increase sanei _sub use count */
  assert (test_init (2));

  /* there should be still as many detected devices */
  assert (count_detected (detected));

  /* there should be still as many opened devices */
  assert (count_opened (opened));

  assert (test_exit (1));

  /* there should be still as many opened devices */
  assert (count_opened (opened));

  /* count devices again , sanei_usb_exit() shouldn't have
   * change the count */
  assert (count_detected (detected));

  /* claim all available devices */
  assert (test_claim_all (dn, opened));

  /* then release them all */
  assert (test_release_all (dn, opened));

  /* close all opened devices */
  assert (test_close_all (dn, opened));

  /* check there is no opened device */
  assert (count_opened (0));

  /* finally free resources */
  assert (test_exit (0));

  /* check there is no more devices */
  assert (count_detected (0));

  /* test attach matching device with a mock */
  assert (test_attach ());

  /* try to call sanei_usb_exit() when it not initialized */
  assert (test_exit (0));

  /* scan devices when sanei usb is not initialized */
  assert (test_scan_devices (0, 0));

  /* we re start use of sanei usb so we check we have left it
   * it he correct state after "closing" it. */
  printf ("\n============================================================\n");
  printf ("restart use of sanei usb after having freed all resources...\n\n");
  assert (test_init (1));

  /* we should have the same initial count of detected devices */
  assert (count_detected (detected));

  /* finally free resources */
  assert (test_exit (0));

  /* all the tests are OK ! */
  return 0;
}

/* vim: set sw=2 cino=>2se-1sn-1s{s^-1st0(0u0 smarttab expandtab: */
