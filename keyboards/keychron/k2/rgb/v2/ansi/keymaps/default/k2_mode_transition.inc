// SPDX-License-Identifier: GPL-2.0-or-later
// Called once per main loop, after QMK host_task().
enum { K2_MODE_START, K2_MODE_WAIT_RELEASE, K2_MODE_WAIT_COMMAND, K2_MODE_WAIT_ROLLBACK };
enum { K2_USB_RECOVERY_MS = 250 };
static uint8_t k2_mode_desired = CONNECTION_HOST_NONE;
static uint8_t k2_mode_target = CONNECTION_HOST_NONE;
static uint8_t k2_mode_source = CONNECTION_HOST_NONE;
static uint8_t k2_mode_phase;
static uint16_t k2_mode_wait_since;
static bool k2_mode_first = true;
static bool k2_mode_startup_usb;
static bool k2_mode_startup_usb_eligible;

static void k2_mode_request(uint8_t host) {
    k2_mode_desired = host;
}

static bool k2_mode_blocks_keys(void) {
    return k2_mode_phase != K2_MODE_START && !k2_mode_startup_usb;
}

static void k2_mode_select(uint8_t host) {
    k2_mode_target = host;
    connection_set_host_noeeprom(host);
    k2_mode_phase = K2_MODE_WAIT_RELEASE;
}

static void k2_mode_task(void) {
    if (k2_mode_phase == K2_MODE_START) {
        if (k2_mode_desired == CONNECTION_HOST_NONE || !k2_mode_matrix_clear()) return;
        k2_mode_source = connection_get_host();
        // On the next loop host_task clears the old host before changing its driver.
        k2_mode_wait_since = timer_read();
        k2_mode_select(k2_mode_desired);
        return;
    }
    // host_task has now selected the initial USB host. Keep wired typing
    // available while the Bluetooth module starts, but finish any held key
    // before blocking events for a requested switch away from USB.
    if (k2_mode_first) {
        k2_mode_first = false;
        k2_mode_startup_usb_eligible = k2_mode_target == CONNECTION_HOST_USB && k2_mode_source != CONNECTION_HOST_BLUETOOTH;
    }
    if (k2_mode_startup_usb_eligible && !k2_mode_startup_usb && k2_mode_phase == K2_MODE_WAIT_RELEASE && k2_mode_desired == CONNECTION_HOST_USB) {
        k2_mode_startup_usb = true;
        k2_mode_rearm_keys();
    }
    if (k2_mode_startup_usb && k2_mode_desired != CONNECTION_HOST_USB) {
        if (!k2_mode_matrix_clear()) return;
        k2_mode_startup_usb = false;
    }
    if (k2_mode_phase == K2_MODE_WAIT_ROLLBACK) {
        if (k2_mode_desired != CONNECTION_HOST_NONE && k2_mode_desired != k2_mode_source) {
            k2_mode_wait_since = timer_read();
            k2_mode_select(k2_mode_desired);
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
    bool usb_idle = k2_mode_usb_idle();
    if (k2_mode_desired == CONNECTION_HOST_USB && timer_elapsed(k2_mode_wait_since) >= K2_USB_RECOVERY_MS) {
        if (k2_mode_target != CONNECTION_HOST_USB) {
            k2_mode_source = k2_mode_target;
            k2_mode_select(CONNECTION_HOST_USB);
            return; // Give host_task one loop to clear the old host and select USB.
        }
        // The radio still owns its queue. Let USB type while its mode command waits.
        if (usb_idle && !k2_mode_startup_usb) {
            k2_mode_startup_usb = true;
            k2_mode_rearm_keys();
        }
    }
    if (!iton_bt_is_idle() || !usb_idle) return;
    if (k2_mode_phase == K2_MODE_WAIT_RELEASE) {
        bool accepted = k2_mode_target == CONNECTION_HOST_USB ? iton_bt_mode_usb() : k2_bt_select_profile(k2_bt_profile);
        if (accepted) {
            k2_mode_startup_usb_eligible = false;
            k2_mode_phase = K2_MODE_WAIT_COMMAND;
            k2_mode_wait_since = timer_read();
        }
        return;
    }
    if (k2_mode_desired != CONNECTION_HOST_NONE && k2_mode_desired != k2_mode_target) {
        k2_mode_source = k2_mode_target;
        k2_mode_wait_since = timer_read();
        k2_mode_select(k2_mode_desired);
        return;
    }
    k2_mode_desired = CONNECTION_HOST_NONE;
    k2_mode_phase = K2_MODE_START;
    if (!k2_mode_startup_usb) k2_mode_rearm_keys();
    k2_mode_startup_usb = false;
}
