// SPDX-License-Identifier: GPL-2.0-or-later
#include <gtest/gtest.h>
#include <cstddef>
#include <cstdint>
extern "C" {
void     iton_bt_send(uint8_t cmd, uint8_t *data, uint8_t len);
void     test_reset_sends(void);
unsigned test_sends(void);
size_t   test_sent_length(void);
uint8_t  test_sent_byte(size_t index);
}
TEST(ItonBounds, RejectsOverlongPayload) {
    uint8_t data[16] = {};
    test_reset_sends();
    iton_bt_send(0xa2, data, sizeof(data));
    EXPECT_EQ(test_sends(), 0u);
}
TEST(ItonBounds, SendsLargestFittingPayload) {
    uint8_t data[15] = {};
    data[14]         = 0x7f;
    test_reset_sends();
    iton_bt_send(0xa2, data, sizeof(data));
    EXPECT_EQ(test_sends(), 1u);
    EXPECT_EQ(test_sent_length(), 16u);
    EXPECT_EQ(test_sent_byte(0), 0xa2);
    EXPECT_EQ(test_sent_byte(15), 0x7f);
}
