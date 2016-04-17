#include "../include/sane/config.h"

#ifndef HAVE_INET_PTON

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

int 
inet_pton (int af, const char *src, void *dst)
{

  if (af == AF_INET)
    {

#if defined(HAVE_INET_ATON)
      int result;
      struct in_addr in;

      result = inet_aton (src, &in);
      if (result)
	{
	  memcpy (dst, &in.s_addr, sizeof (in.s_addr));
	  return 1;
	}
      else
	return 0;

#elif defined(HAVE_INET_ADDR)

# if !defined(INADDR_NONE)
#  define INADDR_NONE -1
# endif /* !defined(INADDR_NONE) */
      u_int32_t in;

      in = inet_addr (src);
      if (in != INADDR_NONE)
	{
	  memcpy (dst, &in, sizeof (in));
	  return 1;
	}
      else
	return 0;

#endif /* defined(HAVE_INET_ATON) */
    }
  return -1;
}

#endif /* !HAVE_INET_PTON */
