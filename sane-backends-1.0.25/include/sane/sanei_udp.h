/* sane - Scanner Access Now Easy.
 * Copyright (C) 2007 Tower Technologies
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
 * Header file for UDP/IP communications. 
 */

#ifndef sanei_udp_h
#define sanei_udp_h

#include <sane/sane.h>

#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <netinet/in.h>
#include <netdb.h>
#endif

extern SANE_Status sanei_udp_open(const char *host, int port, int *fdp);
extern SANE_Status sanei_udp_open_broadcast(int *fdp);
extern void sanei_udp_close(int fd);
extern void sanei_udp_set_nonblock(int fd, SANE_Bool nonblock);
extern ssize_t sanei_udp_write(int fd, const u_char * buf, int count);
extern ssize_t sanei_udp_read(int fd, u_char * buf, int count);
extern ssize_t sanei_udp_write_broadcast(int fd, int port, const u_char * buf, int count);
extern ssize_t sanei_udp_recvfrom(int fd, u_char * buf, int count, char **fromp);

#endif /* sanei_udp_h */
