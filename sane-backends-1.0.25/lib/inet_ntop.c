#include "../include/sane/config.h"

#ifndef HAVE_INET_NTOP

#include <string.h>
#include <sys/types.h>
#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif


const char *
inet_ntop (int af, const void *src, char *dst, size_t cnt)
{
  struct in_addr in;
  char *text_addr;

#ifdef HAVE_INET_NTOA
  if (af == AF_INET)
    {
      memcpy (&in.s_addr, src, sizeof (in.s_addr));
      text_addr = inet_ntoa (in);
      if (text_addr && dst)
	{
	  strncpy (dst, text_addr, cnt);
	  return dst;
	}
      else
	return 0;
    }
#endif /* HAVE_INET_NTOA */
  return 0;
}

#endif /* !HAVE_INET_NTOP */
