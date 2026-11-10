// SPDX-License-Identifier: GPL-2.0-or-later
enum { K2_BT_DISCONNECTED, K2_BT_RECONNECTING, K2_BT_PAIRING, K2_BT_CONNECTED };
enum { K2_BT_PROFILES = 3, K2_BT_AUTO_PAIR_MS = 2000, K2_BT_PAIR_HOLD_MS = 3000, K2_BT_BLINK_MS = 250 };
enum { K2_BT_NORMAL_RGB = UINT8_MAX };
enum { K2_BT_KNOWN_SHIFT = 8, K2_BT_KNOWN_MASK = (1 << K2_BT_PROFILES) - 1, K2_BT_CONFIG_VERSION = 1 << 16 };

static uint8_t k2_bt_profile;
static uint8_t k2_bt_request = K2_BT_PROFILES;
static volatile uint8_t k2_bt_hold = K2_BT_PROFILES;
static uint8_t k2_bt_consumed_keys;
static uint32_t k2_bt_hold_since;
static uint32_t k2_bt_saved_config;

// The ITON notifications can run in interrupt context.
static volatile bool k2_bt_auto_pair;
static volatile uint8_t k2_bt_auto_profile = K2_BT_PROFILES;
static volatile uint8_t k2_bt_sent_profile;
static volatile uint32_t k2_bt_auto_pair_since;
static volatile uint8_t k2_bt_known_profiles;
static volatile uint8_t k2_bt_state;
static volatile uint32_t k2_bt_pairing_since;

void iton_bt_connection_successful(void) {
    k2_bt_auto_pair = false;
    if (k2_bt_state != K2_BT_CONNECTED) k2_bt_known_profiles |= 1U << k2_bt_sent_profile;
    k2_bt_state = K2_BT_CONNECTED;
}
void iton_bt_disconnected(void) { k2_bt_state = K2_BT_DISCONNECTED; }
void iton_bt_enters_connection_state(void) {
    k2_bt_state = K2_BT_RECONNECTING;
}

void iton_bt_entered_pairing(void) {
    k2_bt_auto_pair = false;
    if (k2_bt_hold == k2_bt_sent_profile) k2_bt_hold = K2_BT_PROFILES;
    if (k2_bt_state != K2_BT_PAIRING) k2_bt_pairing_since = timer_read32();
    k2_bt_state = K2_BT_PAIRING;
}

void iton_bt_profile_sent(uint8_t profile) {
    if (profile >= K2_BT_PROFILES) return;
    // A pairing notice belongs to the channel that was selected when it arrived.
    if (profile != k2_bt_sent_profile && k2_bt_state == K2_BT_PAIRING) {
        k2_bt_state = K2_BT_DISCONNECTED;
    }
    k2_bt_sent_profile = profile;
    if (profile == k2_bt_auto_profile) {
        k2_bt_auto_pair_since = timer_read32();
        k2_bt_auto_pair = !(k2_bt_known_profiles & (1U << profile));
        k2_bt_auto_profile = K2_BT_PROFILES;
    }
}

static bool k2_bt_select_profile(uint8_t profile) {
    if (!iton_bt_switch_profile(profile)) return false;
    // Wait for radio status before changing the displayed link state.
    k2_bt_profile = profile;
    return true;
}

static uint32_t k2_bt_profile_config(void) {
    return K2_BT_CONFIG_VERSION | k2_bt_profile | ((uint32_t)k2_bt_known_profiles << K2_BT_KNOWN_SHIFT);
}

static void k2_bt_load_profile(uint32_t config) {
    uint8_t profile = config & UINT8_MAX;
    if (profile >= K2_BT_PROFILES) {
        profile = 0;
        config = 0;
    }
    k2_bt_profile = profile;
    k2_bt_sent_profile = k2_bt_profile;
    // Older settings may have recorded a reconnect attempt as a successful bond.
    k2_bt_known_profiles = config & K2_BT_CONFIG_VERSION ? (config >> K2_BT_KNOWN_SHIFT) & K2_BT_KNOWN_MASK : 0;
    k2_bt_saved_config = k2_bt_profile_config();
}

static bool k2_bt_profile_key(uint8_t profile, bool pressed, bool shortcut) {
    uint8_t key = 1U << profile;
    if (!pressed) {
        if (!(k2_bt_consumed_keys & key)) return false;
        k2_bt_consumed_keys &= ~key;
        if (k2_bt_hold == profile) k2_bt_hold = K2_BT_PROFILES;
        return true;
    }
    if (k2_bt_consumed_keys & key) return true;
    if (!shortcut) return false;

    k2_bt_consumed_keys |= key;
    k2_bt_request = profile;
    k2_bt_hold = profile;
    k2_bt_hold_since = timer_read32();
    return true;
}

static bool k2_bt_pair_due(void) {
    bool auto_due = k2_bt_auto_pair && k2_bt_state == K2_BT_DISCONNECTED && timer_elapsed32(k2_bt_auto_pair_since) >= K2_BT_AUTO_PAIR_MS;
    return auto_due || (k2_bt_hold < K2_BT_PROFILES && timer_elapsed32(k2_bt_hold_since) >= K2_BT_PAIR_HOLD_MS);
}

static void k2_bt_profile_task(bool bluetooth, bool fn) {
    uint32_t config = k2_bt_profile_config();
    // Keep flash programming out of an active SPI frame.
    if (config != k2_bt_saved_config && iton_bt_is_idle()) {
        eeconfig_update_user(config);
        k2_bt_saved_config = config;
    }

    if (!bluetooth) {
        k2_bt_request = K2_BT_PROFILES;
        k2_bt_auto_pair = false;
        k2_bt_auto_profile = K2_BT_PROFILES;
    }
    if (!bluetooth || !fn) k2_bt_hold = K2_BT_PROFILES;

    if (k2_bt_request < K2_BT_PROFILES) {
        k2_bt_auto_pair = false;
        k2_bt_auto_profile = k2_bt_known_profiles & (1U << k2_bt_request) ? K2_BT_PROFILES : k2_bt_request;
        if (!k2_bt_select_profile(k2_bt_request)) {
            k2_bt_auto_profile = K2_BT_PROFILES;
            return;
        }
        k2_bt_request = K2_BT_PROFILES;
    }

    if (k2_bt_pair_due() && iton_bt_is_idle() && iton_bt_enter_pairing_if(k2_bt_pair_due)) {
        k2_bt_auto_pair = false;
        k2_bt_hold = K2_BT_PROFILES;
    }
}

#ifdef RGB_MATRIX_STATUS
static uint8_t k2_bt_status_led(void) {
    uint8_t state = k2_bt_state;
    if (state == K2_BT_CONNECTED) return K2_BT_NORMAL_RGB;
    if (state == K2_BT_PAIRING && (timer_elapsed32(k2_bt_pairing_since) / K2_BT_BLINK_MS) % 2) return 0;
    return k2_bt_sent_profile + 1;
}
#endif
