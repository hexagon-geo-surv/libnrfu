// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (C) 2022 Leica Geosystems AG
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <nrfu.h>

#include "serial.h"
#include "toolbox.h"

#define SLIP_BYTE_END		0xC0	/* indicates end of packet */
#define SLIP_BYTE_ESC		0xDB	/* indicates byte stuffing */
#define SLIP_BYTE_ESC_END	0xDC	/* ESC ESC_END means END data byte */
#define SLIP_BYTE_ESC_ESC	0xDD	/* ESC ESC_ESC means ESC data byte */

int error_level = NRFU_LOG_LEVEL_ERROR;

#define dfu_log(level, fmt, arg...) \
	do { \
		if (error_level >= (level)) \
			fprintf(stderr, fmt, ## arg); \
	} while (0)

struct nrfu_data_t {
	int serial_fd;
	uint16_t mtu;
	uint16_t receipt_notify_n;
};

struct object_select_response_t {
	uint32_t max_size;
	uint32_t offset;
	uint32_t crc;
};

struct dfu_msg_t {
	union {
		struct {
			uint8_t op_code;
			uint8_t resp_op_code;
			uint8_t res_code;
			uint8_t payload[];
		} response;
		struct {
			uint8_t op_code;
			uint8_t payload[];
		} command;
		uint8_t data[128];
	};
	size_t payload_length;
};

enum dfu_opcode {
	DFU_OPCODE_OBJECT_CREATE	= 0x01,
	DFU_OPCODE_SET_PRN		= 0x02,
	DFU_OPCODE_GET_CRC		= 0x03,
	DFU_OPCODE_SET_EXECUTE		= 0x04,
	DFU_OPCODE_OBJECT_SELECT	= 0x06,
	DFU_OPCODE_GET_MTU		= 0x07,
	DFU_OPCODE_WRITE_OBJECT		= 0x08,
	DFU_OPCODE_PING			= 0x09,
	DFU_OPCODE_RESPONSE		= 0x60,
};

enum dfu_rescode {
	DFU_RESCODE_SUCCESS		= 0x01,
};

enum dfu_object_type {
	DFU_OBJECT_TYPE_COMMAND		= 0x01,
	DFU_OBJECT_TYPE_DATA		= 0x02,
};

static int slip_sendc(struct nrfu_data_t *p, uint8_t c)
{
	uint8_t to_send[2];
	size_t length = 0;

	switch (c) {
	case SLIP_BYTE_END:
		to_send[length++] = SLIP_BYTE_ESC;
		to_send[length++] = SLIP_BYTE_ESC_END;
		break;

	case SLIP_BYTE_ESC:
		to_send[length++] = SLIP_BYTE_ESC;
		to_send[length++] = SLIP_BYTE_ESC_ESC;
		break;

	default:
		to_send[length++] = c;
	}
	return serial_send(p->serial_fd, to_send, length);
}

static int dfu_send_msg(struct nrfu_data_t *p, struct dfu_msg_t *msg)
{
	int ret;
	uint8_t *data;
	uint8_t end_byte = SLIP_BYTE_END;
	int i = 2;

	if (!p || !msg)
		return -1;

	dfu_log(NRFU_LOG_LEVEL_DEBUG, "--> ");
	dfu_log(NRFU_LOG_LEVEL_DEBUG, "0x%02x ", msg->command.op_code);

	ret = slip_sendc(p, msg->command.op_code);
	if (ret < 0) {
		dfu_log(NRFU_LOG_LEVEL_ERROR, "Failed to send \"0x%02x\"\n", msg->command.op_code);
		return ret;
	}

	for (data = msg->command.payload; data < &msg->command.payload[msg->payload_length]; data++) {
		dfu_log(NRFU_LOG_LEVEL_DEBUG, "0x%02x ", *data);
		if (i > 0 && !(i % 16))
			dfu_log(NRFU_LOG_LEVEL_DEBUG, "\n");
		i++;

		ret = slip_sendc(p, *data);
		if (ret < 0) {
			dfu_log(NRFU_LOG_LEVEL_ERROR, "\n");
			dfu_log(NRFU_LOG_LEVEL_ERROR, "Failed to send \"0x%02x\"\n", *data);
			break;
		}
	}

	if (ret < 0)
		return ret;
	dfu_log(NRFU_LOG_LEVEL_DEBUG, "\n");
	return serial_send(p->serial_fd, &end_byte, 1);
}

static int dfu_get_response(struct nrfu_data_t *p, enum dfu_opcode opcode, struct dfu_msg_t *msg)
{
	size_t resp_length;
	int i;

	if (!p || !msg)
		return -1;

	msg->payload_length = 0;
	resp_length = serial_receive(p->serial_fd, msg->data, sizeof(msg->data), SLIP_BYTE_END);

	dfu_log(NRFU_LOG_LEVEL_DEBUG, "<-- ");
	for (i = 0; i < resp_length; i++) {
		dfu_log(NRFU_LOG_LEVEL_DEBUG, "0x%02x ", msg->data[i]);
		if (i > 0 && !(i % 16))
			dfu_log(NRFU_LOG_LEVEL_DEBUG, "\n");
	}
	dfu_log(NRFU_LOG_LEVEL_DEBUG, "\n");

	if (resp_length < 3) {
		dfu_log(NRFU_LOG_LEVEL_ERROR, "Response too short: %zu\n", resp_length);
		return -1;
	}

	if (msg->response.op_code != DFU_OPCODE_RESPONSE) {
		dfu_log(NRFU_LOG_LEVEL_ERROR, "No response: 0x%02x\n", msg->response.op_code);
		return -1;
	}

	if (msg->response.resp_op_code != opcode) {
		dfu_log(NRFU_LOG_LEVEL_ERROR, "Unexpected OP_CODE: 0x%02x (expected 0x%02x)\n", msg->response.resp_op_code, opcode);
		return -1;
	}

	if (msg->response.res_code == DFU_RESCODE_SUCCESS) {
		msg->payload_length = resp_length - 3;
		return 0;
	}

	dfu_log(NRFU_LOG_LEVEL_ERROR, "Response Error! Received:\n");
	for (i = 0; i < resp_length - 2; i++)
		dfu_log(NRFU_LOG_LEVEL_ERROR, "0x%02x ", msg->data[i]);
	dfu_log(NRFU_LOG_LEVEL_ERROR, "\n");
	return -1;
}

static int send_ping(struct nrfu_data_t *p)
{
	struct dfu_msg_t msg;

	msg.command.op_code = DFU_OPCODE_PING;
	msg.payload_length = 0;
	msg.command.payload[msg.payload_length++] = 0x01;

	dfu_log(NRFU_LOG_LEVEL_INFO, "Sending ping...\n");
	if (dfu_send_msg(p, &msg) < 0) {
		dfu_log(NRFU_LOG_LEVEL_ERROR, "Failed to send ping!\n");
		return -1;
	}

	if (dfu_get_response(p, DFU_OPCODE_PING, &msg) < 0) {
		dfu_log(NRFU_LOG_LEVEL_ERROR, "Failed to receive ping!\n");
		return -1;
	}

	dfu_log(NRFU_LOG_LEVEL_INFO, "[OK]\n");
	return 0;
}

static int set_receipt_notify(struct nrfu_data_t *p)
{
	struct dfu_msg_t msg;

	msg.command.op_code = DFU_OPCODE_SET_PRN;
	msg.payload_length = 0;
	msg.payload_length += uint16_encode(p->receipt_notify_n, &msg.command.payload[msg.payload_length]);

	dfu_log(NRFU_LOG_LEVEL_INFO, "Setting receipt notify to %u...\n", p->receipt_notify_n);
	if (dfu_send_msg(p, &msg) < 0) {
		dfu_log(NRFU_LOG_LEVEL_ERROR, "Failed to set receipt notify to %u!\n", p->receipt_notify_n);
		return -1;
	}

	if (dfu_get_response(p, DFU_OPCODE_SET_PRN, &msg) < 0) {
		dfu_log(NRFU_LOG_LEVEL_ERROR, "Failed to receive response for receipt notify!\n");
		return -1;
	}

	dfu_log(NRFU_LOG_LEVEL_INFO, "[OK]\n");
	return 0;
}

static int get_mtu(struct nrfu_data_t *p)
{
	struct dfu_msg_t msg;

	if (!p)
		return -1;

	msg.command.op_code = DFU_OPCODE_GET_MTU;
	msg.payload_length = 0;

	dfu_log(NRFU_LOG_LEVEL_INFO, "Getting MTU...\n");
	if (dfu_send_msg(p, &msg) < 0) {
		dfu_log(NRFU_LOG_LEVEL_ERROR, "Failed to send command GET_MTU!\n");
		return -1;
	}

	if (dfu_get_response(p, DFU_OPCODE_GET_MTU, &msg) < 0) {
		dfu_log(NRFU_LOG_LEVEL_ERROR, "Failed to receive MTU!\n");
		return -1;
	}

	if (msg.payload_length < sizeof(p->mtu)) {
		dfu_log(NRFU_LOG_LEVEL_ERROR, "Response too short for MTU!\n");
		dfu_log(NRFU_LOG_LEVEL_ERROR, "Received: %lu Expected: %lu!\n", msg.payload_length, sizeof(p->mtu));
		return -1;
	}

	p->mtu = uint16_decode(msg.response.payload);

	dfu_log(NRFU_LOG_LEVEL_INFO, "[OK]: MTU is %u\n", p->mtu);
	return 0;
}

static int object_select(struct nrfu_data_t *p, enum dfu_object_type type, struct object_select_response_t *resp)
{
	struct dfu_msg_t msg;

	if (!resp)
		return -1;

	msg.command.op_code = DFU_OPCODE_OBJECT_SELECT;
	msg.payload_length = 0;
	msg.command.payload[msg.payload_length++] = type;

	dfu_log(NRFU_LOG_LEVEL_INFO, "Selecting object type %s...\n",
		type == DFU_OBJECT_TYPE_COMMAND ? "COMMAND" : "DATA");
	if (dfu_send_msg(p, &msg) < 0) {
		dfu_log(NRFU_LOG_LEVEL_ERROR, "Failed to send command OBJ_SELECT!\n");
		return -1;
	}

	if (dfu_get_response(p, DFU_OPCODE_OBJECT_SELECT, &msg) < 0) {
		dfu_log(NRFU_LOG_LEVEL_ERROR, "Failed to receive OBJ_SELECT!\n");
		return -1;
	}

	if (msg.payload_length < sizeof(*resp)) {
		dfu_log(NRFU_LOG_LEVEL_ERROR, "Response too short for OBJ_SELECT!\n");
		dfu_log(NRFU_LOG_LEVEL_ERROR, "Received: %lu Expected: %lu!\n",
			msg.payload_length, sizeof(*resp));
		return -1;
	}

	resp->max_size = uint32_decode(&msg.response.payload[0]);
		resp->offset = uint32_decode(&msg.response.payload[4]);
		resp->crc = uint32_decode(&msg.response.payload[8]);

	dfu_log(NRFU_LOG_LEVEL_INFO, "[OK]: [0x%x, 0x%x, 0x%x]\n",
		resp->max_size, resp->offset, resp->crc);
	return 0;
}

static int object_create(struct nrfu_data_t *p, enum dfu_object_type type, uint32_t size)
{
	struct dfu_msg_t msg;

	msg.command.op_code = DFU_OPCODE_OBJECT_CREATE;
	msg.payload_length = 0;

	msg.command.payload[msg.payload_length++] = type;
	msg.payload_length += uint32_encode(size, &msg.command.payload[msg.payload_length]);

	dfu_log(NRFU_LOG_LEVEL_INFO, "Creating object type %s, size 0x%x...\n",
		type == DFU_OBJECT_TYPE_COMMAND ? "COMMAND" : "DATA", size);
	if (dfu_send_msg(p, &msg) < 0) {
		dfu_log(NRFU_LOG_LEVEL_ERROR, "Failed to send command OBJ_CREATE!\n");
		return -1;
	}

	if (dfu_get_response(p, DFU_OPCODE_OBJECT_CREATE, &msg) < 0) {
		dfu_log(NRFU_LOG_LEVEL_ERROR,
			"Failed to create object of type 0x%02x with size %u!\n", type, size);
		return -1;
	}

	dfu_log(NRFU_LOG_LEVEL_INFO, "[OK]\n");
	return 0;
}

static int get_crc(struct nrfu_data_t *p, uint32_t *offset, uint32_t *crc)
{
	struct dfu_msg_t msg;

	if (!crc || !offset)
		return -1;

	msg.command.op_code = DFU_OPCODE_GET_CRC;
	msg.payload_length = 0;

	dfu_log(NRFU_LOG_LEVEL_INFO, "Fetching CRC...\n");
	if (dfu_send_msg(p, &msg) < 0) {
		dfu_log(NRFU_LOG_LEVEL_ERROR, "Failed to send command GET_CRC!\n");
		return -1;
	}

	if (dfu_get_response(p, DFU_OPCODE_GET_CRC, &msg) < 0) {
		dfu_log(NRFU_LOG_LEVEL_ERROR, "Failed to receive CRC!\n");
		return -1;
	}

	if (msg.payload_length < sizeof(*offset) + sizeof(*crc)) {
		dfu_log(NRFU_LOG_LEVEL_ERROR, "Response too short for GET_CRC!\n");
		dfu_log(NRFU_LOG_LEVEL_ERROR, "Received: %lu Expected: %lu!\n",
			msg.payload_length, sizeof(*offset) + sizeof(*crc));
		return -1;
	}

	*offset = uint32_decode(&msg.response.payload[0]);
	*crc = uint32_decode(&msg.response.payload[sizeof(*offset)]);

	dfu_log(NRFU_LOG_LEVEL_INFO, "[OK]: [0x%x, 0x%x ]\n", *offset, *crc);
	return 0;
}

static int stream_data(struct nrfu_data_t *p, FILE *fp, long file_size, uint32_t *crc,
			   uint32_t start_offset)
{
	int chunk_size;
	struct dfu_msg_t msg;
	uint32_t offset = 0;
	uint32_t offset_target, crc_target;

	if (!p || !fp || !crc)
		return -1;

	msg.command.op_code = DFU_OPCODE_WRITE_OBJECT;
	msg.payload_length = 0;

	chunk_size = ((p->mtu - 1) / 2) - 1;
	if (chunk_size > sizeof(msg.data) - 1) {
		dfu_log(NRFU_LOG_LEVEL_ERROR, "Message buffer too small for chunk size: %zu vs. %d\n",
			sizeof(msg.data), chunk_size);
		return -1;
	}

	dfu_log(NRFU_LOG_LEVEL_INFO, "Streaming file of size 0x%lx with chunk size 0x%x...",
		file_size, chunk_size);

	while (offset < file_size) {
		msg.payload_length = fread(msg.command.payload, 1, chunk_size, fp);
		if (ferror(fp)) {
			dfu_log(NRFU_LOG_LEVEL_ERROR, "Failed to read file\n");
			return -1;
		}

		offset += msg.payload_length;
		*crc = crc32_compute(msg.command.payload, msg.payload_length, *crc);

		if (dfu_send_msg(p, &msg) < 0) {
			dfu_log(NRFU_LOG_LEVEL_ERROR, "Failed to send data!\n");
			return -1;
		}
	}

	dfu_log(NRFU_LOG_LEVEL_INFO, "[OK]\n");

	if (get_crc(p, &offset_target, &crc_target) < 0)
		return -1;

	if (*crc != crc_target) {
		dfu_log(NRFU_LOG_LEVEL_ERROR, "CRC validation failed.");
		dfu_log(NRFU_LOG_LEVEL_ERROR, "Expected: 0x%08x Received 0x%08x\n", *crc, crc_target);
		return -1;
	}

	offset += start_offset;
	if (offset != offset_target) {
		dfu_log(NRFU_LOG_LEVEL_ERROR, "Offset validation failed.");
		dfu_log(NRFU_LOG_LEVEL_ERROR, "Expected: 0x%08x Received 0x%08x\n",
			offset, offset_target);
		return -1;
	}

	return 0;
}

static int set_execute(struct nrfu_data_t *p)
{
	struct dfu_msg_t msg;

	msg.command.op_code = DFU_OPCODE_SET_EXECUTE;
	msg.payload_length = 0;

	dfu_log(NRFU_LOG_LEVEL_INFO, "Setting Execute...");
	if (dfu_send_msg(p, &msg) < 0) {
		dfu_log(NRFU_LOG_LEVEL_ERROR, "Failed to send command SET_EXECUTE!\n");
		return -1;
	}

	if (dfu_get_response(p, DFU_OPCODE_SET_EXECUTE, &msg) < 0) {
		dfu_log(NRFU_LOG_LEVEL_ERROR, "Failed to set execute!\n");
		return -1;
	}

	dfu_log(NRFU_LOG_LEVEL_INFO, "[OK]\n");
	return 0;
}

static int send_init_packet(struct nrfu_data_t *p, const char *init_packet)
{
	FILE *fp;
	long file_size;
	struct object_select_response_t obj_sel_resp;
	int ret = -1;
	uint32_t crc = 0;

	if (!init_packet)
		return -1;

	dfu_log(NRFU_LOG_LEVEL_INFO, "Opening %s...\n", init_packet);
	fp = fopen(init_packet, "rb");
	if (!fp) {
		dfu_log(NRFU_LOG_LEVEL_ERROR, "Failed to open %s\n", init_packet);
		return -1;
	}
	dfu_log(NRFU_LOG_LEVEL_INFO, "[OK]\n");

	fseek(fp, 0, SEEK_END);
	file_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (object_select(p, DFU_OBJECT_TYPE_COMMAND, &obj_sel_resp) < 0)
		goto out;

	if (file_size > obj_sel_resp.max_size) {
		dfu_log(NRFU_LOG_LEVEL_ERROR, "Init command too long: %ld (max. %d)\n", file_size, obj_sel_resp.max_size);
		goto out;
	}

	if (obj_sel_resp.offset != 0)
		dfu_log(NRFU_LOG_LEVEL_INFO, "Offset at 0x%x\n", obj_sel_resp.offset);

	if (object_create(p, DFU_OBJECT_TYPE_COMMAND, file_size) < 0)
		goto out;

	if (stream_data(p, fp, file_size, &crc, 0) < 0)
		goto out;

	if (set_execute(p) < 0)
		goto out;

	ret = 0;
out:
	fclose(fp);
	if (ret)
		dfu_log(NRFU_LOG_LEVEL_ERROR, "Failed to send init-packet \"%s\"\n", init_packet);
	return ret;
}

static int send_firmware(struct nrfu_data_t *p, const char *firmware)
{
	FILE *fp;
	uint32_t file_size, obj_offset;
	struct object_select_response_t obj_sel_resp;
	int ret = -1;
	uint32_t crc = 0;

	if (!firmware)
		return -1;

	dfu_log(NRFU_LOG_LEVEL_INFO, "open %s\n", firmware);
	fp = fopen(firmware, "rb");
	if (!fp) {
		dfu_log(NRFU_LOG_LEVEL_ERROR, "Failed to open %s\n", firmware);
		return -1;
	}
	dfu_log(NRFU_LOG_LEVEL_INFO, "[OK]\n");

	fseek(fp, 0, SEEK_END);
	file_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (object_select(p, DFU_OBJECT_TYPE_DATA, &obj_sel_resp) < 0)
		goto out;

	if (obj_sel_resp.offset != 0) {
		dfu_log(NRFU_LOG_LEVEL_INFO, "Offset at 0x%x\n", obj_sel_resp.offset);
		obj_sel_resp.offset = 0;
	}

	for (obj_offset = obj_sel_resp.offset; obj_offset < file_size; obj_offset += obj_sel_resp.max_size) {
		uint32_t obj_size = obj_sel_resp.max_size;

		if (file_size - obj_offset < obj_sel_resp.max_size)
			obj_size = file_size - obj_offset;

		if (object_create(p, DFU_OBJECT_TYPE_DATA, obj_size) < 0)
			goto out;

		fseek(fp, obj_offset, SEEK_SET);
		if (stream_data(p, fp, obj_size, &crc, obj_offset) < 0)
			goto out;

		if (set_execute(p) < 0)
			goto out;
	}

	ret = 0;
out:
	fclose(fp);
	if (ret)
		dfu_log(NRFU_LOG_LEVEL_ERROR, "Failed to send firmware \"%s\"\n", firmware);
	return ret;
}

int nrfu_update(const char *devname, const char *init_packet, const char *firmware, enum nrfu_log_level log_level)
{
	struct nrfu_data_t priv;
	int ret = -1;

	if (!devname || !init_packet || !firmware)
		return -1;

	error_level = log_level;

	priv.serial_fd = serial_init(devname);
	if (priv.serial_fd < 0) {
		dfu_log(NRFU_LOG_LEVEL_ERROR, "Failed to initialize \"%s\"!\n", devname);
		goto err_out;
	}

	priv.receipt_notify_n = 0;

	if (send_ping(&priv) < 0)
		goto err_out;

	if (set_receipt_notify(&priv) < 0)
		goto err_out;

	if (get_mtu(&priv) < 0)
		goto err_out;

	if (send_init_packet(&priv, init_packet) < 0)
		goto err_out;

	if (send_firmware(&priv, firmware) < 0)
		goto err_out;

	ret = 0;
err_out:
	if (priv.serial_fd >= 0)
		close(priv.serial_fd);

	return ret;
}
