// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Candidate V1.13 frame subset; transport and hardware behavior are unverified.
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    K2_BT_EVENT_LED,
    K2_BT_EVENT_STATUS,
} k2_bt_event_type_t;

typedef struct {
    k2_bt_event_type_t type;
    uint8_t            value;
} k2_bt_event_t;

bool k2_bt_host_frame(uint8_t host, uint8_t out[3]);
void k2_bt_wired_frame(uint8_t out[3]);
void k2_bt_os_frame(bool mac, uint8_t out[3]);
bool k2_bt_decode(const uint8_t *data, size_t length, k2_bt_event_t *out);

#ifdef __cplusplus
}
#endif
