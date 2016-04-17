#include "../include/sane/config.h"

#ifndef HAVE_SYSLOG

#include <stdio.h>

void syslog(int priority, const char *format, va_list args)
{
    printf("%d ", priority);
    printf(format, args);
}

#endif
