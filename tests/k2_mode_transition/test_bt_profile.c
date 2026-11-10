// SPDX-License-Identifier: GPL-2.0-or-later
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

static uint32_t now, saved_profile;
static uint8_t sent_profile;
static unsigned switches, pairings;
static bool queue_ready = true, idle = true;
static bool transfer_completes = true;
static bool radio_reconnects_before_pair;
void iton_bt_enters_connection_state(void);
void iton_bt_profile_sent(uint8_t profile);

static uint32_t timer_read32(void) { return now; }
static uint32_t timer_elapsed32(uint32_t start) { return now - start; }
static bool iton_bt_switch_profile(uint8_t profile) {
    if (!queue_ready) return false;
    ++switches;
    sent_profile = profile;
    if (transfer_completes) iton_bt_profile_sent(profile);
    return true;
}
static bool iton_bt_enter_pairing_if(bool (*ready)(void)) {
    if (radio_reconnects_before_pair) iton_bt_enters_connection_state();
    if (!queue_ready || !ready()) return false;
    ++pairings;
    return true;
}
static bool iton_bt_is_idle(void) { return idle; }
static void eeconfig_update_user(uint32_t profile) { saved_profile = profile; }

#include "../../keyboards/keychron/k2/rgb/v2/ansi/keymaps/default/k2_bt_profile.inc"

int main(void) {
    k2_bt_load_profile(2 | (7U << K2_BT_KNOWN_SHIFT));
    assert(k2_bt_profile == 2 && k2_bt_known_profiles == 0);
    k2_bt_load_profile(UINT32_MAX);
    assert(k2_bt_status_led() == 1);
    assert(!k2_bt_profile_key(1, true, false));
    assert(!k2_bt_profile_key(1, false, false));

    /* Short chord selects the profile and never enters pairing. */
    queue_ready = false;
    assert(k2_bt_profile_key(1, true, true));
    k2_bt_profile_task(true, true);
    assert(!switches);
    assert(k2_bt_profile_key(1, false, false));
    queue_ready = true;
    k2_bt_profile_task(true, false);
    assert(switches == 1 && sent_profile == 1 && saved_profile == 0);
    idle = false;
    k2_bt_profile_task(true, false);
    assert(saved_profile == 0);
    idle = true;
    k2_bt_profile_task(true, false);
    assert(saved_profile == (K2_BT_CONFIG_VERSION | 1));
    assert(k2_bt_status_led() == 2);

    /* Holding an already connected profile still allows pairing. */
    iton_bt_connection_successful();
    assert(k2_bt_status_led() == K2_BT_NORMAL_RGB);
    now = UINT32_MAX - 999;
    assert(k2_bt_profile_key(1, true, true));
    k2_bt_profile_task(true, true);
    iton_bt_enters_connection_state();
    iton_bt_connection_successful();
    now += K2_BT_PAIR_HOLD_MS - 1;
    k2_bt_profile_task(true, true);
    assert(switches == 2 && !pairings);
    ++now;
    idle = false;
    k2_bt_profile_task(true, true);
    assert(!pairings);
    idle = true;
    k2_bt_profile_task(true, true);
    k2_bt_profile_task(true, true);
    assert(pairings == 1 && k2_bt_status_led() == K2_BT_NORMAL_RGB);
    now = 2000;
    iton_bt_entered_pairing();
    assert(k2_bt_status_led() == 2);
    now += K2_BT_BLINK_MS;
    assert(k2_bt_status_led() == 0);
    iton_bt_disconnected();
    assert(k2_bt_status_led() == 2);
    assert(k2_bt_profile_key(1, false, false));

    /* Releasing Fn first cancels the hold even if the number stays down. */
    assert(k2_bt_profile_key(2, true, true));
    k2_bt_profile_task(true, false);
    iton_bt_enters_connection_state();
    now += K2_BT_PAIR_HOLD_MS;
    k2_bt_profile_task(true, true);
    assert(switches == 3 && sent_profile == 2 && pairings == 1);
    assert(k2_bt_status_led() == 3);
    assert(k2_bt_profile_key(2, false, false));
    iton_bt_connection_successful();
    iton_bt_enters_connection_state();
    assert(k2_bt_status_led() == 3);

    /* An unsent request cannot follow the keyboard out of Bluetooth mode. */
    assert(k2_bt_profile_key(0, true, true));
    k2_bt_profile_task(false, true);
    k2_bt_profile_task(true, true);
    assert(switches == 3 && k2_bt_status_led() == 3);
    assert(k2_bt_profile_key(0, false, false));

    /* Channel 1 can be selected and paired after returning to Bluetooth. */
    assert(k2_bt_profile_key(0, true, true));
    k2_bt_profile_task(true, true);
    now += K2_BT_PAIR_HOLD_MS;
    k2_bt_profile_task(true, true);
    assert(switches == 4 && sent_profile == 0 && (saved_profile & UINT8_MAX) == 0 && pairings == 2);
    assert(k2_bt_profile_key(0, false, false));

    /* Queuing a channel change does not end the active link. */
    iton_bt_connection_successful();
    assert(k2_bt_profile_key(1, true, true));
    k2_bt_profile_task(true, true);
    assert(sent_profile == 1 && k2_bt_status_led() == K2_BT_NORMAL_RGB);
    assert(k2_bt_profile_key(1, false, false));
    iton_bt_disconnected();
    assert(k2_bt_status_led() == 2);

    /* An autonomous pairing notice consumes the original hold. */
    assert(k2_bt_profile_key(2, true, true));
    k2_bt_profile_task(true, true);
    iton_bt_entered_pairing();
    iton_bt_connection_successful();
    now += K2_BT_PAIR_HOLD_MS;
    k2_bt_profile_task(true, true);
    assert(pairings == 2 && k2_bt_status_led() == K2_BT_NORMAL_RGB);
    assert(k2_bt_profile_key(2, false, false));

    /* A known channel can disconnect or time out without entering pairing. */
    assert(k2_bt_profile_key(1, true, true));
    k2_bt_profile_task(true, true);
    assert(k2_bt_profile_key(1, false, false));
    iton_bt_enters_connection_state();
    iton_bt_disconnected();
    now += 3000;
    k2_bt_profile_task(true, false);
    assert(pairings == 2 && k2_bt_status_led() == 2);

    /* A short tap pairs an unknown channel only after the old link ends. */
    k2_bt_load_profile(K2_BT_CONFIG_VERSION | 1 | (2U << K2_BT_KNOWN_SHIFT));
    iton_bt_connection_successful();
    assert(k2_bt_profile_key(0, true, true));
    k2_bt_profile_task(true, true);
    assert(k2_bt_profile_key(0, false, false));
    now += 3000;
    k2_bt_profile_task(true, false);
    assert(pairings == 2 && k2_bt_status_led() == K2_BT_NORMAL_RGB);

    transfer_completes = false;
    assert(k2_bt_profile_key(0, true, true));
    k2_bt_profile_task(true, true);
    assert(k2_bt_profile_key(0, false, false));
    iton_bt_disconnected();
    now += 3000;
    k2_bt_profile_task(true, false);
    assert(pairings == 2);

    transfer_completes = true;
    assert(k2_bt_profile_key(0, true, true));
    k2_bt_profile_task(true, true);
    assert(k2_bt_profile_key(0, false, false));
    k2_bt_profile_task(true, false);
    assert(pairings == 2 && k2_bt_status_led() == 1);
    now += 3000;
    k2_bt_profile_task(true, false);
    assert(pairings == 3);
    iton_bt_entered_pairing();
    iton_bt_connection_successful();
    now += 3000;
    k2_bt_profile_task(true, false);
    assert(pairings == 3 && k2_bt_status_led() == K2_BT_NORMAL_RGB);
    k2_bt_load_profile(saved_profile);
    assert(k2_bt_known_profiles == 3 && k2_bt_profile == 0);

    assert(k2_bt_profile_key(2, true, true));
    k2_bt_profile_task(true, true);
    assert(k2_bt_profile_key(2, false, false));
    iton_bt_disconnected();
    now += K2_BT_AUTO_PAIR_MS;
    radio_reconnects_before_pair = true;
    k2_bt_profile_task(true, false);
    assert(pairings == 3);

    /* An unsuccessful reconnect to an unknown channel can still fall back to pairing. */
    radio_reconnects_before_pair = false;
    now += K2_BT_AUTO_PAIR_MS;
    k2_bt_profile_task(true, false);
    assert(pairings == 3 && k2_bt_status_led() == 3);
    iton_bt_disconnected();
    k2_bt_profile_task(true, false);
    assert(pairings == 4);

    /* Changing channels while pairing does not carry the blink to the new channel. */
    iton_bt_entered_pairing();
    assert(k2_bt_profile_key(0, true, true));
    k2_bt_profile_task(true, true);
    assert(k2_bt_profile_key(0, false, false));
    now += K2_BT_BLINK_MS;
    assert(k2_bt_status_led() == 1);
    iton_bt_connection_successful();
    assert(k2_bt_status_led() == K2_BT_NORMAL_RGB);

    /* A status from the previous channel cannot cancel an unsent selection. */
    k2_bt_load_profile(K2_BT_CONFIG_VERSION | 1 | (2U << K2_BT_KNOWN_SHIFT));
    transfer_completes = false;
    assert(k2_bt_profile_key(2, true, true));
    k2_bt_profile_task(true, true);
    assert(k2_bt_profile_key(2, false, false));
    iton_bt_enters_connection_state();
    iton_bt_profile_sent(2);
    iton_bt_disconnected();
    now += K2_BT_AUTO_PAIR_MS;
    k2_bt_profile_task(true, false);
    assert(pairings == 5);
    return 0;
}
