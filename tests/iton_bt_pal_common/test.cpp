// SPDX-License-Identifier: GPL-2.0-or-later
#include <gtest/gtest.h>
#include "config.h"
extern "C" {
void     iton_bt_init(void);
unsigned test_callback_registered(void);
unsigned test_event_enabled(void);
}
TEST(ItonPal, ConfiguresEventsOnlyWhenCallbacksEnabled) {
    iton_bt_init();
    EXPECT_EQ(test_callback_registered(), PAL_USE_CALLBACKS ? 1u : 0u);
    EXPECT_EQ(test_event_enabled(), PAL_USE_CALLBACKS ? 1u : 0u);
}
