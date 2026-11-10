// SPDX-License-Identifier: GPL-2.0-or-later
#include <gtest/gtest.h>
#include <cstddef>
#include <cstdint>
extern "C" {
void     iton_bt_send(uint8_t cmd, uint8_t *data, uint8_t len);
void     test_reset_sends(void);
unsigned test_sends(void);
void     test_advance_send(void);
size_t   test_sent_length(void);
uint8_t  test_sent_byte(size_t index);
void     test_keyboard_reset(void);
uint8_t  test_keyboard_cached_key(void);
void     test_keyboard_send(uint8_t sixth, uint8_t mods, uint8_t first);
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

TEST(ItonBounds, SixthKeyWithinBitmapIsEncoded) {
    test_keyboard_reset();
    test_keyboard_send(0x77, 0, 0);
    EXPECT_EQ(test_sends(), 1u);
    EXPECT_EQ(test_sent_byte(0), 0xa1);
    test_advance_send();
    EXPECT_EQ(test_sends(), 2u);
    EXPECT_EQ(test_sent_length(), 16u);
    EXPECT_EQ(test_sent_byte(0), 0xa2);
    EXPECT_EQ(test_sent_byte(15), 0x80);
    EXPECT_EQ(test_keyboard_cached_key(), 0x77);
}

TEST(ItonBounds, UnsupportedSixthKeysClearPriorBitmapAndFallBack) {
    test_keyboard_reset();
    test_keyboard_send(0x77, 0, 0);
    for (uint8_t key : {uint8_t{0x78}, uint8_t{0xff}}) {
        test_reset_sends();
        test_keyboard_send(key, 0x02, 0x04);
        EXPECT_EQ(test_sends(), 1u);
        EXPECT_EQ(test_sent_length(), 9u);
        EXPECT_EQ(test_sent_byte(0), 0xa1);
        EXPECT_EQ(test_sent_byte(1), 0x02);
        EXPECT_EQ(test_sent_byte(3), 0x04);
        EXPECT_EQ(test_sent_byte(8), key);
        EXPECT_EQ(test_keyboard_cached_key(), 0);

        test_reset_sends();
        test_keyboard_send(key, 0x04, 0x05);
        EXPECT_EQ(test_sends(), 1u);
        EXPECT_EQ(test_sent_byte(0), 0xa1);
        EXPECT_EQ(test_sent_byte(1), 0x04);
        EXPECT_EQ(test_sent_byte(3), 0x05);
        EXPECT_EQ(test_keyboard_cached_key(), 0);
    }
    test_reset_sends();
    test_keyboard_send(0x76, 0, 0);
    EXPECT_EQ(test_sent_byte(0), 0xa1);
    test_advance_send();
    EXPECT_EQ(test_sent_byte(0), 0xa2);
    EXPECT_EQ(test_sent_byte(15), 0x40);
    EXPECT_EQ(test_keyboard_cached_key(), 0x76);
}
