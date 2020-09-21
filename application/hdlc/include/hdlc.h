#ifndef APPLICATION_LIB_INCLUDE_HDLC_H
#define APPLICATION_LIB_INCLUDE_HDLC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

enum hdlc_state {
	/** Expeciting initial zero of frame delimeter */
	HDLC_STATE_INITIAL_ZERO,
	/** Expecting ones in frame delimiter. */
	HDLC_STATE_ONES,
	/** Expecting final zero in frame delimeter. */
	HDLC_STATE_FINAL_ZERO,
	/** Receiving packet data. */
	HDLC_STATE_DATA,
	/** Expecting a zero to skip. */
	HDLC_STATE_SKIP_ZERO,
	/** Expecting zero in frame delimeter at the end of a packet. */
	HDLC_STATE_PACKET_END,
};

typedef void (*hdlc_callback_t)(const uint8_t *buf, size_t len);

// FIXME use something sensible here
#define HDLC_BUFFER_SIZE 128

struct hdlc_data {
	enum hdlc_state state;
	bool last_bit;
	uint8_t num_ones;
	uint16_t num_bits;
	hdlc_callback_t callback;
	uint8_t buffer[HDLC_BUFFER_SIZE];
};

void hdlc_init(struct hdlc_data *hdlc, hdlc_callback_t callback);
void hdlc_input(struct hdlc_data *hdlc, bool raw_bit);

#endif
