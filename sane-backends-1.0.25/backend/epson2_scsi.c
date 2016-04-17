#undef BACKEND_NAME
#define BACKEND_NAME epson2_scsi

#include "../include/sane/config.h"
#include "../include/sane/sanei_debug.h"
#include "../include/sane/sanei_scsi.h"
#include "epson2_scsi.h"

#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef NEED_SYS_TYPES_H
#include <sys/types.h>
#endif

#include <string.h> /* for memset and memcpy */
#include <stdio.h>

/* sense handler for the sanei_scsi_xxx comands */
SANE_Status
sanei_epson2_scsi_sense_handler(int scsi_fd,
	unsigned char *result, void *arg)
{
	/* to get rid of warnings */
	scsi_fd = scsi_fd;
	arg = arg;

	if (result[0] && result[0] != 0x70) {
		DBG(2, "%s: sense code = 0x%02x\n",
			__FUNCTION__, result[0]);
		return SANE_STATUS_IO_ERROR;
	} else {
		return SANE_STATUS_GOOD;
	}
}

SANE_Status
sanei_epson2_scsi_inquiry(int fd, void *buf, size_t *buf_size)
{
	unsigned char cmd[6];
	int status;

	memset(cmd, 0, 6);
	cmd[0] = INQUIRY_COMMAND;
	cmd[4] = *buf_size > 255 ? 255 : *buf_size;
	status = sanei_scsi_cmd(fd, cmd, sizeof cmd, buf, buf_size);

	return status;
}

SANE_Status
sanei_epson2_scsi_test_unit_ready(int fd)
{
	unsigned char cmd[6];

	memset(cmd, 0, 6);
	cmd[0] = TEST_UNIT_READY_COMMAND;

	return sanei_scsi_cmd2(fd, cmd, sizeof(cmd), NULL, 0, NULL, NULL);
}

int
sanei_epson2_scsi_read(int fd, void *buf, size_t buf_size,
		      SANE_Status *status)
{
	unsigned char cmd[6];

	memset(cmd, 0, 6);
	cmd[0] = READ_6_COMMAND;
	cmd[2] = buf_size >> 16;
	cmd[3] = buf_size >> 8;
	cmd[4] = buf_size;

	*status = sanei_scsi_cmd2(fd, cmd, sizeof(cmd), NULL, 0, buf, &buf_size);
	if (*status == SANE_STATUS_GOOD)
		return buf_size;

	return 0;
}

int
sanei_epson2_scsi_write(int fd, const void *buf, size_t buf_size,
		       SANE_Status *status)
{
	unsigned char cmd[6];

	memset(cmd, 0, sizeof(cmd));
	cmd[0] = WRITE_6_COMMAND;
	cmd[2] = buf_size >> 16;
	cmd[3] = buf_size >> 8;
	cmd[4] = buf_size;

	*status = sanei_scsi_cmd2(fd, cmd, sizeof(cmd), buf, buf_size, NULL, NULL);
	if (*status == SANE_STATUS_GOOD)
		return buf_size;

	return 0;
}
