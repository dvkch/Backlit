#include "../include/sane/config.h"

#ifndef HAVE_VSYSLOG

#include <stdio.h>
#include <stdarg.h>

void vsyslog(int priority, const char *format, va_list args)
{
  char buf[1024];
  vsnprintf(buf, sizeof(buf), format, args);
  syslog(priority, "%s", buf);
}

#endif /* !HAVE_VSYSLOG */
