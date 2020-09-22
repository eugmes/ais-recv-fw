#include <ztest.h>
#include <hdlc.h>

static const uint8_t bitstream[] = {
#include <bitstream.inc>
};

static unsigned int packet_count;

void test_callback(const struct hdlc_data *hdlc, const uint8_t *buf, size_t len)
{
	packet_count++;
}

static void test_hdlc(void)
{
	struct hdlc_data hdlc;
	hdlc_init(&hdlc, &test_callback);

	for (size_t i = 0; i < ARRAY_SIZE(bitstream); i++) {
		uint8_t byte = bitstream[i];

		for (int bit = 0; bit < 8; bit++) {
			hdlc_input(&hdlc, (byte & 1) != 0);
			byte >>= 1;
		}
	}

	zassert_equal(packet_count, 167, NULL);
}

void test_main(void)
{
	ztest_test_suite(hdlc_tests,
		ztest_unit_test(test_hdlc)
	);

	ztest_run_test_suite(hdlc_tests);
}
