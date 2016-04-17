/*
   Copyright (C) 2008, Panasonic Russia Ltd.
*/
/* sane - Scanner Access Now Easy.
   Panasonic KV-S1020C / KV-S1025C USB scanners.
*/

#ifndef KVS1025_CMDS_H
#define KVS1025_CMDS_H

/* Commands supported by the KV-S1020C / KV-S1025C scanner. */
#define SCSI_TEST_UNIT_READY        0x00
#define SCSI_INQUIRY                0x12
#define SCSI_SET_WINDOW             0x24
#define SCSI_SCAN                   0x1B
#define SCSI_SEND_10                0x2A
#define SCSI_READ_10                0x28
#define SCSI_REQUEST_SENSE          0x03
#define SCSI_GET_BUFFER_STATUS      0x34
#define SCSI_SET_TIMEOUT	    0xE1

typedef enum
{
  KV_CMD_NONE = 0,
  KV_CMD_IN = 0x81,		/* scanner to pc */
  KV_CMD_OUT = 0x02		/* pc to scanner */
} KV_CMD_DIRECTION;		/* equals to endpoint address */

typedef struct
{
  KV_CMD_DIRECTION direction;
  unsigned char cdb[12];
  int cdb_size;
  int data_size;
  void *data;
} KV_CMD_HEADER, *PKV_CMD_HEADER;

#define KV_CMD_TIMEOUT          10000

static inline int
getbitfield (unsigned char *pageaddr, int mask, int shift)
{
  return ((*pageaddr >> shift) & mask);
}

/* defines for request sense return block */
#define get_RS_information_valid(b)       getbitfield(b + 0x00, 1, 7)
#define get_RS_error_code(b)              getbitfield(b + 0x00, 0x7f, 0)
#define get_RS_filemark(b)                getbitfield(b + 0x02, 1, 7)
#define get_RS_EOM(b)                     getbitfield(b + 0x02, 1, 6)
#define get_RS_ILI(b)                     getbitfield(b + 0x02, 1, 5)
#define get_RS_sense_key(b)               getbitfield(b + 0x02, 0x0f, 0)
#define get_RS_information(b)             getnbyte(b+0x03, 4)
#define get_RS_additional_length(b)       b[0x07]
#define get_RS_ASC(b)                     b[0x0c]
#define get_RS_ASCQ(b)                    b[0x0d]
#define get_RS_SKSV(b)                    getbitfield(b+0x0f,1,7)

typedef enum
{
  KV_SUCCESS = 0,
  KV_FAILED = 1,
  KV_CHK_CONDITION = 2
} KV_STATUS;

typedef struct
{
  KV_STATUS status;
  unsigned char reserved[16];
  unsigned char sense[18];
} KV_CMD_RESPONSE, *PKV_CMD_RESPONSE;


#endif /*#ifndef KVS1025_CMDS_H */
