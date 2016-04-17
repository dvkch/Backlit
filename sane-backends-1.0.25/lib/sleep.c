#include "../include/sane/config.h"

#ifndef HAVE_SLEEP

#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif

unsigned int sleep(unsigned int seconds)
{
#ifdef HAVE_WINDOWS_H
    Sleep(seconds*1000);
    return 0;
#else
    int rc = 0;

    /* WARNING: Not all platforms support usleep() for more than 1
     * second. Assuming if they do not have POSIX sleep then they
     * do not have POSIX usleep() either and are using our internal
     * version which can support it. If it fails, need to add an OS
     * specific replacement like Sleep for Windows.
     */
    if (usleep(seconds*1000000))
	rc = 1;
    return rc;
#endif

}

#endif
