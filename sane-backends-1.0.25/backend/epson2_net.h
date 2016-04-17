#ifndef _EPSON2_NET_H_
#define _EPSON2_NET_H_

#include <sys/types.h>
#include "../include/sane/sane.h"

extern int sanei_epson_net_read(struct Epson_Scanner *s, unsigned char *buf, ssize_t buf_size,
				SANE_Status *status);
extern int sanei_epson_net_write(struct Epson_Scanner *s, unsigned int cmd, const unsigned char *buf,
				size_t buf_size, size_t reply_len,
				SANE_Status *status);
extern SANE_Status sanei_epson_net_lock(struct Epson_Scanner *s);
extern SANE_Status sanei_epson_net_unlock(struct Epson_Scanner *s);

#endif
