#include "hdlc.h"
#include <string.h>
#include <sys/crc.h>
#include <stdio.h>

#define LOG_LEVEL CONFIG_HDLC_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(hdlc);

void hdlc_init(struct hdlc_data *hdlc, hdlc_callback_t callback)
{
	memset(hdlc, 0, sizeof(*hdlc));
	hdlc->callback = callback;
}

#define FCS_RESIDUE 0xf0b8

static void validate_packet(struct hdlc_data *hdlc)
{
	size_t num_bits = hdlc->num_bits;

	if (num_bits <= 16 + 6) {
		return;
	}

	num_bits -= 16 + 6;

	if ((num_bits % 8) != 0) {
		return;
	}

	size_t num_bytes = num_bits / 8;

	uint16_t crc = crc16_ccitt(0xffff, hdlc->buffer, num_bytes + 2);
	if (crc != FCS_RESIDUE) {
		return;
	}

	LOG_DBG("bits: %u", num_bits);
	LOG_HEXDUMP_DBG(hdlc->buffer, num_bytes, "packet:");

	if (hdlc->callback) {
		hdlc->callback(NULL, num_bits / 8);
	}
}

static inline void handle_initial_zero(struct hdlc_data *hdlc, bool bit)
{
	if (bit) {
		hdlc->num_ones = 0;
	} else {
		hdlc->state = HDLC_STATE_ONES;
	}
}

static inline void handle_ones(struct hdlc_data *hdlc, bool bit)
{
	if (hdlc->num_ones == 6) {
		hdlc->state = HDLC_STATE_FINAL_ZERO;
	}
}

static inline void handle_final_zero(struct hdlc_data *hdlc, bool bit)
{
	if (bit) {
		hdlc->state = HDLC_STATE_INITIAL_ZERO;
		hdlc->num_ones = 0;
	} else {
		hdlc->state = HDLC_STATE_DATA;
		hdlc->num_bits = 0;
	}
}

static inline void handle_data(struct hdlc_data *hdlc, bool bit)
{
	size_t byte_idx = hdlc->num_bits / 8;

	if (byte_idx >= ARRAY_SIZE(hdlc->buffer)) {
		/* Message too large, reset the decoder. */
		hdlc->num_ones = 0;
		hdlc->state = HDLC_STATE_INITIAL_ZERO;
	}

	uint8_t bit_offset = hdlc->num_bits % 8;
	uint8_t bit_val = bit ? 0x80 : 0x00;

	if (bit_offset == 0) {
		hdlc->buffer[byte_idx] = bit_val;
	} else {
		hdlc->buffer[byte_idx] = (hdlc->buffer[byte_idx] >> 1) | bit_val;
	}

	hdlc->num_bits++;

	if (hdlc->num_ones == 5) {
		hdlc->state = HDLC_STATE_SKIP_ZERO;
	}
}

static inline void handle_skip_zero(struct hdlc_data *hdlc, bool bit)
{
	if (bit) {
		hdlc->state = HDLC_STATE_PACKET_END;
	} else {
		hdlc->state = HDLC_STATE_DATA;
	}
}

static inline void handle_packet_end(struct hdlc_data *hdlc, bool bit)
{
	if (bit) {
		/* too many ones... */
		hdlc->state = HDLC_STATE_INITIAL_ZERO;
		hdlc->num_ones = 0;
	} else {
		validate_packet(hdlc);
		hdlc->state = HDLC_STATE_DATA;
		hdlc->num_bits = 0;
	}
}

void hdlc_input(struct hdlc_data *hdlc, bool raw_bit)
{
	if (hdlc->num_ones > 6) {
		hdlc->num_ones = 0;
		hdlc->state = HDLC_STATE_INITIAL_ZERO;
		return;
	}

	/* NRZI decoding, 0 is represented as level change in the bitstream. */
	bool bit = raw_bit == hdlc->last_bit;
	hdlc->last_bit = raw_bit;

	if (bit) {
		hdlc->num_ones++;
	} else {
		hdlc->num_ones = 0;
	}

	switch (hdlc->state) {
	default:
	case HDLC_STATE_INITIAL_ZERO:
		handle_initial_zero(hdlc, bit);
		break;
	case HDLC_STATE_ONES:
		handle_ones(hdlc, bit);
		break;
	case HDLC_STATE_FINAL_ZERO:
		handle_final_zero(hdlc, bit);
		break;
	case HDLC_STATE_DATA:
		handle_data(hdlc, bit);
		break;
	case HDLC_STATE_SKIP_ZERO:
		handle_skip_zero(hdlc, bit);
		break;
	case HDLC_STATE_PACKET_END:
		handle_packet_end(hdlc, bit);
		break;
	}
}
