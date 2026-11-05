// SPDX-License-Identifier: GPL-2.0-or-later
#include <gtest/gtest.h>
#include "config.h"
extern "C" {
void     iton_bt_init(void);
void     test_reset(void);
unsigned test_callback_registered(void);
unsigned test_event_enabled(void);
#if PAL_USE_CALLBACKS
void     test_prepare_edge_on_enable(void);
unsigned test_receive_count(void);
bool     test_receive_was_ready(void);
#endif
}
#if PAL_USE_CALLBACKS
TEST(ItonPal, SpiIsReadyWhenEventFiresDuringInitialization) {
    test_reset();
    test_prepare_edge_on_enable();
    iton_bt_init();
    EXPECT_EQ(test_receive_count(), 1u);
    EXPECT_TRUE(test_receive_was_ready());
}
#endif
TEST(ItonPal, ConfiguresEventsOnlyWhenCallbacksEnabled) {
    test_reset();
    iton_bt_init();
    EXPECT_EQ(test_callback_registered(), PAL_USE_CALLBACKS ? 1u : 0u);
    EXPECT_EQ(test_event_enabled(), PAL_USE_CALLBACKS ? 1u : 0u);
}
