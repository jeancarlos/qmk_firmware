// SPDX-License-Identifier: GPL-2.0-or-later
// Called once per main loop, after QMK host_task().
enum { K2_MODE_START, K2_MODE_WAIT_RELEASE, K2_MODE_WAIT_COMMAND, K2_MODE_WAIT_ROLLBACK };
static uint8_t k2_mode_desired = CONNECTION_HOST_NONE;
static uint8_t k2_mode_target = CONNECTION_HOST_NONE;
static uint8_t k2_mode_source = CONNECTION_HOST_NONE;
static uint8_t k2_mode_phase;

static void k2_mode_request(uint8_t host) {
    k2_mode_desired = host;
}

static bool k2_mode_blocks_keys(void) {
    return k2_mode_phase != K2_MODE_START;
}

static void k2_mode_task(void) {
    if (k2_mode_phase == K2_MODE_START) {
        if (k2_mode_desired == CONNECTION_HOST_NONE || !k2_mode_matrix_clear()) return;
        k2_mode_source = connection_get_host();
        k2_mode_target = k2_mode_desired;
        // On the next loop host_task clears the old host before changing its driver.
        connection_set_host_noeeprom(k2_mode_target);
        k2_mode_phase = K2_MODE_WAIT_RELEASE;
        return;
    }
    if (k2_mode_phase == K2_MODE_WAIT_ROLLBACK) {
        if (k2_mode_desired != CONNECTION_HOST_NONE && k2_mode_desired != k2_mode_source) {
            k2_mode_target = k2_mode_desired;
            connection_set_host_noeeprom(k2_mode_target);
            k2_mode_phase = K2_MODE_WAIT_RELEASE;
            return;
        }
        if (k2_mode_desired == k2_mode_source) k2_mode_desired = CONNECTION_HOST_NONE;
        k2_mode_phase = K2_MODE_START;
        k2_mode_rearm_keys();
        return;
    }
    if (k2_mode_phase == K2_MODE_WAIT_RELEASE && k2_mode_source != CONNECTION_HOST_NONE &&
        k2_mode_desired == k2_mode_source && k2_mode_desired != k2_mode_target) {
        connection_set_host_noeeprom(k2_mode_source);
        k2_mode_phase = K2_MODE_WAIT_ROLLBACK;
        return;
    }
    if (!iton_bt_is_idle() || !k2_mode_usb_idle()) return;
    if (k2_mode_phase == K2_MODE_WAIT_RELEASE) {
        bool accepted = k2_mode_target == CONNECTION_HOST_USB ? iton_bt_mode_usb() : iton_bt_switch_profile(0);
        if (accepted) k2_mode_phase = K2_MODE_WAIT_COMMAND;
        return;
    }
    if (k2_mode_desired != CONNECTION_HOST_NONE && k2_mode_desired != k2_mode_target) {
        k2_mode_source = k2_mode_target;
        k2_mode_target = k2_mode_desired;
        connection_set_host_noeeprom(k2_mode_target);
        k2_mode_phase = K2_MODE_WAIT_RELEASE;
        return;
    }
    k2_mode_desired = CONNECTION_HOST_NONE;
    k2_mode_phase = K2_MODE_START;
    k2_mode_rearm_keys();
}
