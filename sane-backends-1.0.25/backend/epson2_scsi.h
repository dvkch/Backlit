#ifndef _EPSON2_SCSI_H_
#define _EPSON2_SCSI_H_

#include <sys/types.h>
#include "../include/sane/sane.h"

#define TEST_UNIT_READY_COMMAND		(0x00)
#define READ_6_COMMAND			(0x08)
#define WRITE_6_COMMAND			(0x0a)
#define INQUIRY_COMMAND			(0x12)
#define TYPE_PROCESSOR			(0x03)

#define INQUIRY_BUF_SIZE		(36)

SANE_Status sanei_epson2_scsi_sense_handler(int scsi_fd, unsigned char *result,
					   void *arg);
SANE_Status sanei_epson2_scsi_inquiry(int fd, void *buf,
				     size_t *buf_size);
int sanei_epson2_scsi_read(int fd, void *buf, size_t buf_size,
			  SANE_Status *status);
int sanei_epson2_scsi_write(int fd, const void *buf, size_t buf_size,
			   SANE_Status *status);
SANE_Status sanei_epson2_scsi_test_unit_ready(int fd);
#endif
