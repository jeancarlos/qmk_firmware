// SPDX-License-Identifier: GPL-2.0-or-later
#include "bluetooth_protocol.h"

bool k2_bt_host_frame(uint8_t host, uint8_t out[3]) {
    if (host < 1 || host > 3) {
        return false;
    }
    out[0] = 0xa6;
    out[1] = 0x51;
    out[2] = 0x80 + host;
    return true;
}

void k2_bt_wired_frame(uint8_t out[3]) {
    out[0] = 0xa6;
    out[1] = 0x58;
    out[2] = 0x01;
}

void k2_bt_os_frame(bool mac, uint8_t out[3]) {
    out[0] = 0xa6;
    out[1] = 0x51;
    out[2] = mac ? 0x74 : 0x75;
}

bool k2_bt_decode(const uint8_t *data, size_t length, k2_bt_event_t *out) {
    if (!data || !out) {
        return false;
    }
    if (length == 2 && data[0] == 0xb1) {
        out->type  = K2_BT_EVENT_LED;
        out->value = data[1] & 7;
        return true;
    }
    if (length == 3 && data[0] == 0xb6 && data[1] == 0x51 && data[2] >= 0x76 && data[2] <= 0x79) {
        out->type  = K2_BT_EVENT_STATUS;
        out->value = data[2];
        return true;
    }
    return false;
}
