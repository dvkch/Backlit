#ifndef __KVS20XX_CMD_H
#define __KVS20XX_CMD_H

/*
   Copyright (C) 2008, Panasonic Russia Ltd.
   Copyright (C) 2010, m. allan noah
*/
/*
   Panasonic KV-S20xx USB-SCSI scanners.
*/

#define COMMAND_BLOCK	1
#define DATA_BLOCK	2
#define RESPONSE_BLOCK	3

#define COMMAND_CODE	0x9000
#define DATA_CODE	0xb000
#define RESPONSE_CODE	0xa000
#define STATUS_SIZE 4

struct bulk_header
{
  u32 length;
  u16 type;
  u16 code;
  u32 transaction_id;
};

#define TEST_UNIT_READY        0x00
#define INQUIRY                0x12
#define SET_WINDOW             0x24
#define SCAN                   0x1B
#define SEND_10                0x2A
#define READ_10                0x28
#define REQUEST_SENSE          0x03
#define GET_BUFFER_STATUS      0x34
#define SET_TIMEOUT	    0xE1
#define GET_ADJUST_DATA	    0xE0
#define GOOD 0
#define CHECK_CONDITION 2

typedef enum
{
  CMD_NONE = 0,
  CMD_IN = 0x81,		/* scanner to pc */
  CMD_OUT = 0x02		/* pc to scanner */
} CMD_DIRECTION;		/* equals to endpoint address */

#define RESPONSE_SIZE	0x12
#define MAX_CMD_SIZE	12
struct cmd
{
  unsigned char cmd[MAX_CMD_SIZE];
  int cmd_size;
  void *data;
  int data_size;
  int dir;
};
struct response
{
  int status;
  unsigned char data[RESPONSE_SIZE];
};

#define END_OF_MEDIUM			(1<<6)
#define INCORRECT_LENGTH_INDICATOR	(1<<5)
static const struct
{
  unsigned sense, asc, ascq;
  SANE_Status st;
} s_errors[] =
{
  {
  0, 0, 0, SANE_STATUS_GOOD},
  {
  2, 0, 0, SANE_STATUS_DEVICE_BUSY},
  {
  2, 4, 1, SANE_STATUS_DEVICE_BUSY},
  {
  2, 4, 0x80, SANE_STATUS_COVER_OPEN},
  {
  2, 4, 0x81, SANE_STATUS_COVER_OPEN},
  {
  2, 4, 0x82, SANE_STATUS_COVER_OPEN},
  {
  2, 4, 0x83, SANE_STATUS_COVER_OPEN},
  {
  2, 4, 0x84, SANE_STATUS_COVER_OPEN},
  {
  2, 0x80, 1, SANE_STATUS_CANCELLED},
  {
  2, 0x80, 2, SANE_STATUS_CANCELLED},
  {
  3, 0x3a, 0, SANE_STATUS_NO_DOCS},
  {
  3, 0x80, 1, SANE_STATUS_JAMMED},
  {
  3, 0x80, 2, SANE_STATUS_JAMMED},
  {
  3, 0x80, 3, SANE_STATUS_JAMMED},
  {
  3, 0x80, 4, SANE_STATUS_JAMMED},
  {
  3, 0x80, 5, SANE_STATUS_JAMMED},
  {
  3, 0x80, 6, SANE_STATUS_JAMMED},
  {
  3, 0x80, 7, SANE_STATUS_JAMMED},
  {
  3, 0x80, 8, SANE_STATUS_JAMMED},
  {
3, 0x80, 9, SANE_STATUS_JAMMED},};

SANE_Status kvs20xx_scan (struct scanner *s);
SANE_Status kvs20xx_test_unit_ready (struct scanner *s);
SANE_Status kvs20xx_set_timeout (struct scanner *s, int timeout);
SANE_Status kvs20xx_set_window (struct scanner *s, int wnd_id);
SANE_Status kvs20xx_reset_window (struct scanner *s);
SANE_Status kvs20xx_read_picture_element (struct scanner *s, unsigned side,
					  SANE_Parameters * p);
SANE_Status kvs20xx_read_image_data (struct scanner *s, unsigned page,
				     unsigned side, void *buf,
				     unsigned max_size, unsigned *size);
SANE_Status kvs20xx_document_exist (struct scanner *s);
SANE_Status get_adjust_data (struct scanner *s, unsigned *dummy_length);
SANE_Status kvs20xx_sense_handler (int fd, u_char * sense_buffer, void *arg);

#endif /*__KVS20XX_CMD_H*/
