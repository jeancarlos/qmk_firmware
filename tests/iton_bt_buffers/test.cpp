// SPDX-License-Identifier: GPL-2.0-or-later
#include <gtest/gtest.h>
#include <cstdint>

extern "C" {
void iton_bt_init(void);
void iton_bt_send(uint8_t cmd, uint8_t *data, uint8_t len);
void test_reset(void);
void test_begin_receive(void);
bool test_receive_buffer_is_unchanged(void);
bool test_send_buffer_is_unchanged(void);
}

TEST(ItonBuffers, ReceiveDoesNotOverwritePendingSend) {
    test_reset();
    iton_bt_init();
    uint8_t payload[] = {0x12, 0x34};
    iton_bt_send(0xa3, payload, sizeof(payload));
    test_begin_receive();
    EXPECT_TRUE(test_send_buffer_is_unchanged());
}

TEST(ItonBuffers, SendDoesNotOverwritePendingReceive) {
    test_reset();
    iton_bt_init();
    test_begin_receive();
    uint8_t payload[] = {0x12, 0x34};
    iton_bt_send(0xa3, payload, sizeof(payload));
    EXPECT_TRUE(test_receive_buffer_is_unchanged());
}
