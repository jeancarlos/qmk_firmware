// SPDX-License-Identifier: GPL-2.0-or-later
#include <gtest/gtest.h>

#include "../../keyboards/keychron/k2/rgb/v2/bluetooth_protocol.h"

TEST(K2V2BluetoothProtocol, HostFramesAndInvalidHost) {
    for (uint8_t host = 1; host <= 3; ++host) {
        uint8_t frame[3] = {};
        EXPECT_TRUE(k2_bt_host_frame(host, frame));
        EXPECT_EQ(frame[0], 0xa6);
        EXPECT_EQ(frame[1], 0x51);
        EXPECT_EQ(frame[2], 0x80 + host);
    }
    for (uint8_t host : {0, 4}) {
        uint8_t frame[3] = {0x12, 0x34, 0x56};
        EXPECT_FALSE(k2_bt_host_frame(host, frame));
        EXPECT_EQ(frame[0], 0x12);
        EXPECT_EQ(frame[1], 0x34);
        EXPECT_EQ(frame[2], 0x56);
    }
}

TEST(K2V2BluetoothProtocol, WiredAndOsFrames) {
    uint8_t frame[3] = {};
    k2_bt_wired_frame(frame);
    EXPECT_EQ(frame[0], 0xa6);
    EXPECT_EQ(frame[1], 0x58);
    EXPECT_EQ(frame[2], 0x01);
    k2_bt_os_frame(true, frame);
    EXPECT_EQ(frame[0], 0xa6);
    EXPECT_EQ(frame[1], 0x51);
    EXPECT_EQ(frame[2], 0x74);
    k2_bt_os_frame(false, frame);
    EXPECT_EQ(frame[0], 0xa6);
    EXPECT_EQ(frame[1], 0x51);
    EXPECT_EQ(frame[2], 0x75);
}

TEST(K2V2BluetoothProtocol, DecodesLedAndStatusFrames) {
    for (uint8_t value : {0x00, 0xff}) {
        const uint8_t frame[] = {0xb1, value};
        k2_bt_event_t event{};
        EXPECT_TRUE(k2_bt_decode(frame, sizeof(frame), &event));
        EXPECT_EQ(event.type, K2_BT_EVENT_LED);
        EXPECT_EQ(event.value, value & 7);
    }
    for (uint8_t status : {0x76, 0x77, 0x78, 0x79}) {
        const uint8_t frame[] = {0xb6, 0x51, status};
        k2_bt_event_t event{};
        EXPECT_TRUE(k2_bt_decode(frame, sizeof(frame), &event));
        EXPECT_EQ(event.type, K2_BT_EVENT_STATUS);
        EXPECT_EQ(event.value, status);
    }
}

TEST(K2V2BluetoothProtocol, RejectsMalformedFramesWithoutChangingOutput) {
    const uint8_t empty[]      = {0};
    const uint8_t led[]        = {0xb1, 0x07};
    const uint8_t status[]     = {0xb6, 0x51, 0x76};
    const uint8_t bad_prefix[] = {0xb7, 0x51, 0x76};
    const uint8_t bad_middle[] = {0xb6, 0x52, 0x76};
    const uint8_t bad_status[] = {0xb6, 0x51, 0x75};
    const uint8_t oversized[]  = {0xb6, 0x51, 0x76, 0};
    k2_bt_event_t event{K2_BT_EVENT_STATUS, 0xa5};
    auto          reject = [&](const uint8_t *data, size_t size) {
        EXPECT_FALSE(k2_bt_decode(data, size, &event));
        EXPECT_EQ(event.type, K2_BT_EVENT_STATUS);
        EXPECT_EQ(event.value, 0xa5);
    };
    reject(nullptr, 2);
    reject(empty, 0);
    reject(led, 1);
    reject(status, 2);
    reject(bad_prefix, sizeof(bad_prefix));
    reject(bad_middle, sizeof(bad_middle));
    reject(bad_status, sizeof(bad_status));
    reject(oversized, sizeof(oversized));
    EXPECT_FALSE(k2_bt_decode(led, sizeof(led), nullptr));
}
