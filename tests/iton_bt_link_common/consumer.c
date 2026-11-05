// SPDX-License-Identifier: GPL-2.0-or-later
#include "config.h"
#include "../../drivers/bluetooth/iton_bt.h"

void test_set_driver_state(void) {
    iton_bt_is_connected = true;
#ifdef ITON_BT_ENABLE_LED_STATE
    iton_bt_led_state = 0x42;
#endif
}
