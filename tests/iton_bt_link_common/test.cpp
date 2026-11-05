// SPDX-License-Identifier: GPL-2.0-or-later
#include <gtest/gtest.h>
#include "config.h"
#include <cstdint>
extern "C" {
void        test_set_driver_state(void);
extern bool iton_bt_is_connected;
#ifdef ITON_BT_ENABLE_LED_STATE
extern uint8_t iton_bt_led_state;
#endif
}
TEST(ItonLinkage, DriverStateIsSharedAcrossTranslationUnits) {
    test_set_driver_state();
    EXPECT_TRUE(iton_bt_is_connected);
#ifdef ITON_BT_ENABLE_LED_STATE
    EXPECT_EQ(iton_bt_led_state, 0x42);
#endif
}
