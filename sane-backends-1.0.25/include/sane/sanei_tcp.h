/* sane - Scanner Access Now Easy.
 * Copyright (C) 2006 Tower Technologies
 * Author: Alessandro Zummo <a.zummo@towertech.it>
 * This file is part of the SANE package.
 *
 * This file is in the public domain.  You may use and modify it as
 * you see fit, as long as this copyright message is included and
 * that there is an indication as to what modifications have been
 * made (if any).
 *
 * SANE is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Header file for TCP/IP communications. 
 */

#ifndef sanei_tcp_h
#define sanei_tcp_h

#include <sane/sane.h>

#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <netinet/in.h>
#include <netdb.h>
#endif
#include <sys/types.h>

extern SANE_Status sanei_tcp_open(const char *host, int port, int *fdp);
extern void sanei_tcp_close(int fd);
extern ssize_t sanei_tcp_write(int fd, const u_char * buf, int count);
extern ssize_t sanei_tcp_read(int fd, u_char * buf, int count);

#endif /* sanei_tcp_h */
