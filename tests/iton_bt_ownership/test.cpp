// SPDX-License-Identifier: GPL-2.0-or-later
#include <gtest/gtest.h>
#include <cstdint>
#include <algorithm>
extern "C" {
void iton_bt_init(void);
void iton_bt_task(void);
bool iton_bt_is_idle(void);
void iton_bt_send(uint8_t cmd, uint8_t *data, uint8_t len);
void iton_bt_send_consumer(uint16_t usage);
void iton_bt_send_system(uint16_t usage);
bool iton_bt_send2(uint8_t cmd, uint8_t b1, uint8_t b2);
void test_reset(void);
void advance_time(uint32_t ms);
unsigned test_queue_capacity(void);
void test_falling_edge(void);
void test_rising_edge(void);
bool test_overlapping_start(void);
bool test_stopped_tx(void);
bool test_sent_release(void);
bool test_receive_preserves_active_send(void);
bool test_send_preserves_active_receive(void);
void test_receive_notification(bool pending);
bool test_connected(void);
void test_enable_pending_drain(void);
void test_short_notification(void);
void test_oversized_notification(void);
void test_complete_transfer(void);
bool test_output_high(void);
void test_full_receive(void);
bool test_overflow(void);
bool test_rx_collision(void);
unsigned test_sends(void);
uint8_t test_sent_byte(unsigned index);
void test_set_input_high(void);
unsigned test_receive_count(void);
bool test_receiving(void);
void test_receive_ack_notification(uint8_t type, uint8_t value);
void test_receive_led(uint8_t value);
uint8_t bluetooth_keyboard_leds(void);
void bluetooth_send_system(uint16_t usage);
void test_send_sixth_key(uint8_t key);
void test_send_keyboard_pair(uint8_t first, uint8_t sixth);
uint8_t test_last_sixth_key(void);
void test_tx_residue(void);
void test_spi_busy(bool busy);
void test_spi_irq_pending(bool pending);
void test_defer_fifo_reset(void);
void test_complete_fifo_reset(void);
}

static const unsigned short_frames = test_queue_capacity() / 4;
static const unsigned paired_report_blockers = (test_queue_capacity() - 27) / 4 + 1;
static const unsigned keyboard_frames = test_queue_capacity() / 10;

TEST(ItonOwnership, IdleFifoByteWithBusyFlagDoesNotDisplaceKeyboardRelease) {
    test_reset();
    iton_bt_init();
    test_send_keyboard_pair(4, 0);
    ASSERT_EQ(test_sends(), 1u);
    ASSERT_EQ(test_sent_byte(0), 0xa1);
    ASSERT_EQ(test_sent_byte(3), 4);
    test_complete_transfer();
    test_tx_residue();
    test_spi_busy(true);
    test_send_keyboard_pair(0, 0);
    ASSERT_EQ(test_sends(), 2u);
    ASSERT_EQ(test_sent_byte(0), 0xa1);
    for (unsigned i = 1; i < 9; ++i) EXPECT_EQ(test_sent_byte(i), 0);
}
TEST(ItonOwnership, IdleSlaveWithBusyFlagSendsQueuedFrame) {
    test_reset();
    iton_bt_init();
    test_spi_busy(true);
    ASSERT_TRUE(iton_bt_send2(0xa6, 0x51, 0x81));
    ASSERT_EQ(test_sends(), 1u);
    EXPECT_EQ(test_sent_byte(0), 0xa6);
    EXPECT_EQ(test_sent_byte(1), 0x51);
    EXPECT_EQ(test_sent_byte(2), 0x81);
}
TEST(ItonOwnership, PendingSpiInterruptAndUnfinishedResetPreserveQueuedFrame) {
    test_reset();
    iton_bt_init();
    test_tx_residue();
    test_spi_irq_pending(true);
    ASSERT_TRUE(iton_bt_send2(0xa6, 0x51, 0x81));
    ASSERT_EQ(test_sends(), 0u);
    test_spi_irq_pending(false);
    test_defer_fifo_reset();
    iton_bt_task();
    ASSERT_EQ(test_sends(), 0u);
    test_complete_fifo_reset();
    iton_bt_task();
    ASSERT_EQ(test_sends(), 1u);
    EXPECT_EQ(test_sent_byte(0), 0xa6);
    EXPECT_EQ(test_sent_byte(1), 0x51);
    EXPECT_EQ(test_sent_byte(2), 0x81);
}
TEST(ItonOwnership, IncomingRequestWaitsForFifoReset) {
    test_reset();
    iton_bt_init();
    test_tx_residue();
    test_defer_fifo_reset();
    ASSERT_TRUE(iton_bt_send2(0xa6, 0x51, 0x81));
    ASSERT_EQ(test_sends(), 0u);
    test_rising_edge();
    ASSERT_EQ(test_receive_count(), 0u);
    test_complete_fifo_reset();
    iton_bt_task();
    ASSERT_EQ(test_receive_count(), 1u);
    ASSERT_EQ(test_sends(), 0u);
    test_falling_edge();
    iton_bt_task();
    ASSERT_EQ(test_sends(), 1u);
    EXPECT_EQ(test_sent_byte(0), 0xa6);
}
TEST(ItonOwnership, SimultaneousOrdinaryAndSixthKeyChangesSendBothStates) {
    test_reset();
    iton_bt_init();
    test_rising_edge();
    test_send_keyboard_pair(4, 42);
    test_falling_edge();
    iton_bt_task();
    ASSERT_EQ(test_sends(), 1u);
    EXPECT_EQ(test_sent_byte(0), 0xa1);
    EXPECT_EQ(test_sent_byte(3), 4);
    test_complete_transfer();
    iton_bt_task();
    ASSERT_EQ(test_sends(), 2u);
    EXPECT_EQ(test_sent_byte(0), 0xa2);
    EXPECT_EQ(test_sent_byte(6), 4);
}
TEST(ItonOwnership, SixthKeyReleaseClearsEntireBitmap) {
    test_reset();
    iton_bt_init();
    test_send_keyboard_pair(4, 42);
    test_complete_transfer();
    iton_bt_task();
    test_complete_transfer();
    test_send_keyboard_pair(4, 0);
    test_complete_transfer();
    iton_bt_task();
    ASSERT_EQ(test_sent_byte(0), 0xa2);
    for (unsigned i = 1; i < 16; ++i) EXPECT_EQ(test_sent_byte(i), 0);
}
TEST(ItonOwnership, UnsupportedSixthKeyClearsPreviouslyRepresentedKey) {
    test_reset();
    iton_bt_init();
    test_send_keyboard_pair(4, 42);
    test_complete_transfer();
    iton_bt_task();
    test_complete_transfer();
    test_send_keyboard_pair(4, 120);
    test_complete_transfer();
    iton_bt_task();
    ASSERT_EQ(test_sent_byte(0), 0xa2);
    for (unsigned i = 1; i < 16; ++i) EXPECT_EQ(test_sent_byte(i), 0);
    EXPECT_EQ(test_last_sixth_key(), 0);
}
TEST(ItonOwnership, RejectedSixthKeyDoesNotUpdateSentState) {
    test_reset();
    iton_bt_init();
    test_rising_edge();
    for (uint8_t i = 1; i <= short_frames - 2; ++i) iton_bt_send2(0xa6, 0x51, i);
    test_send_sixth_key(42);
    EXPECT_TRUE(test_overflow());
    EXPECT_EQ(test_last_sixth_key(), 0);
    test_falling_edge();
    for (uint8_t i = 0; i < 5; ++i) {
        iton_bt_task();
        test_complete_transfer();
    }
    test_send_sixth_key(42);
    EXPECT_EQ(test_last_sixth_key(), 42);
}
TEST(ItonOwnership, RejectedPairedStateDoesNotEnqueueHalfAReport) {
    test_reset();
    iton_bt_init();
    test_rising_edge();
    for (uint8_t i = 1; i <= paired_report_blockers; ++i) ASSERT_TRUE(iton_bt_send2(0xa6, 0x51, i));
    test_send_keyboard_pair(4, 42);
    EXPECT_TRUE(test_overflow());
    EXPECT_EQ(test_last_sixth_key(), 0);
    test_falling_edge();
    for (uint8_t i = 1; i <= paired_report_blockers; ++i) {
        iton_bt_task();
        ASSERT_EQ(test_sends(), i);
        EXPECT_EQ(test_sent_byte(0), 0xa6);
        EXPECT_EQ(test_sent_byte(2), i);
        test_complete_transfer();
    }
    iton_bt_task();
    ASSERT_EQ(test_sends(), paired_report_blockers + 1);
    EXPECT_EQ(test_sent_byte(0), 0xa1);
    EXPECT_EQ(test_sent_byte(3), 4);
    test_complete_transfer();
    iton_bt_task();
    ASSERT_EQ(test_sends(), paired_report_blockers + 2);
    EXPECT_EQ(test_sent_byte(0), 0xa2);
    EXPECT_EQ(test_sent_byte(6), 4);
}
TEST(ItonOwnership, RetainsKeyboardClickWhenPairedReportDoesNotFit) {
    test_reset();
    iton_bt_init();
    test_rising_edge();
    for (uint8_t i = 1; i <= paired_report_blockers; ++i) ASSERT_TRUE(iton_bt_send2(0xa6, 0x51, i));
    test_send_keyboard_pair(4, 42);
    test_send_keyboard_pair(0, 0);
    test_falling_edge();
    for (uint8_t i = 1; i <= paired_report_blockers; ++i) {
        iton_bt_task(); ASSERT_EQ(test_sent_byte(0), 0xa6); test_complete_transfer();
    }
    iton_bt_task(); ASSERT_EQ(test_sent_byte(0), 0xa1); EXPECT_EQ(test_sent_byte(3), 4); test_complete_transfer();
    iton_bt_task(); ASSERT_EQ(test_sent_byte(0), 0xa2); EXPECT_EQ(test_sent_byte(6), 4); test_complete_transfer();
    iton_bt_task(); ASSERT_EQ(test_sent_byte(0), 0xa1); EXPECT_EQ(test_sent_byte(3), 0); test_complete_transfer();
    iton_bt_task(); ASSERT_EQ(test_sent_byte(0), 0xa2); EXPECT_EQ(test_sent_byte(6), 0); test_complete_transfer();
    EXPECT_TRUE(iton_bt_is_idle());
}
TEST(ItonOwnership, HighInputAtStartupArmsReceiveAndThenSends) {
    test_reset();
    test_set_input_high();
    iton_bt_init();
    iton_bt_send_consumer(1);
    EXPECT_EQ(test_receive_count(), 1u);
    EXPECT_EQ(test_sends(), 0u);
    test_falling_edge();
    iton_bt_task();
    EXPECT_EQ(test_sends(), 1u);
}

TEST(ItonOwnership, AcknowledgementIsQueuedUntilReceiveEnds) {
    const uint8_t notifications[][3] = {
        {0x51, 0x76, 0x50},
        {0x51, 0x78, 0x51},
        {0x5a, 0x0b, 0x60},
        {0x5a, 0x08, 0x00},
    };
    for (const auto &notification : notifications) {
        SCOPED_TRACE(notification[1]);
        test_reset();
        iton_bt_init();
        test_receive_ack_notification(notification[0], notification[1]);
        EXPECT_EQ(test_sends(), 0u);
        iton_bt_task();
        ASSERT_EQ(test_sends(), 1u);
        EXPECT_EQ(test_sent_byte(0), 0xa6);
        EXPECT_EQ(test_sent_byte(1), 0x51);
        EXPECT_EQ(test_sent_byte(2), notification[2]);
    }
}

TEST(ItonOwnership, AckSurvivesSaturatedReportQueue) {
    test_reset();
    iton_bt_init();
    test_rising_edge();
    for (unsigned i = 0; i <= short_frames; ++i) iton_bt_send_consumer(0x123);
    test_falling_edge();
    test_receive_ack_notification(0x51, 0x76);
    iton_bt_task();
    ASSERT_EQ(test_sends(), 1u);
    EXPECT_EQ(test_sent_byte(0), 0xa6);
    EXPECT_EQ(test_sent_byte(2), 0x50);
    test_complete_transfer();
    iton_bt_task();
    EXPECT_EQ(test_sent_byte(0), 0xa3);
}

TEST(ItonOwnership, MissingTransmitCompletionRetriesOnceThenUnblocksRelease) {
    test_reset();
    iton_bt_init();
    test_send_keyboard_pair(4, 0);
    test_send_keyboard_pair(0, 0);
    advance_time(99);
    iton_bt_task();
    EXPECT_TRUE(test_output_high());
    EXPECT_FALSE(test_stopped_tx());
    advance_time(1);
    iton_bt_task();
    EXPECT_FALSE(test_output_high());
    EXPECT_TRUE(test_stopped_tx());
    advance_time(2);
    iton_bt_task();
    ASSERT_EQ(test_sends(), 2u);
    EXPECT_EQ(test_sent_byte(3), 4);
    advance_time(100);
    iton_bt_task();
    EXPECT_FALSE(test_output_high());
    advance_time(2);
    iton_bt_task();
    ASSERT_EQ(test_sends(), 3u);
    EXPECT_EQ(test_sent_byte(3), 0);
    test_complete_transfer();
    EXPECT_TRUE(iton_bt_is_idle());
}

TEST(ItonOwnership, MissingReleaseCompletionRetriesConsumerAndSystemRelease) {
    for (bool consumer : {true, false}) {
        test_reset();
        iton_bt_init();
        auto send = consumer ? iton_bt_send_consumer : iton_bt_send_system;
        send(0x81);
        test_complete_transfer();
        send(0);
        ASSERT_EQ(test_sends(), 2u);
        advance_time(100);
        iton_bt_task();
        EXPECT_FALSE(test_output_high());
        advance_time(2);
        iton_bt_task();
        ASSERT_EQ(test_sends(), 3u);
        EXPECT_EQ(test_sent_byte(0), consumer ? 0xa3 : 0xa4);
        EXPECT_EQ(test_sent_byte(1), 0);
        if (consumer) EXPECT_EQ(test_sent_byte(2), 0);
        test_complete_transfer();
        EXPECT_TRUE(iton_bt_is_idle());
    }
}

TEST(ItonOwnership, TimedOutReceiveWaitsForPeerToReleaseInt) {
    test_reset();
    iton_bt_init();
    test_rising_edge();
    test_send_keyboard_pair(4, 0);
    advance_time(100);
    iton_bt_task();
    iton_bt_task();
    EXPECT_EQ(test_receive_count(), 1u);
    EXPECT_FALSE(test_receiving());
    EXPECT_EQ(test_sends(), 0u);
    test_falling_edge();
    iton_bt_task();
    ASSERT_EQ(test_sends(), 1u);
    EXPECT_EQ(test_sent_byte(3), 4);
    EXPECT_FALSE(test_overlapping_start());
}
TEST(ItonOwnership, QueueOverflowIsReportedAndExistingFramesRemainOrdered) {
    test_reset();
    iton_bt_init();
    test_rising_edge();
    for (uint8_t i = 1; i <= short_frames; ++i) EXPECT_TRUE(iton_bt_send2(0xa6, 0x51, i));
    EXPECT_TRUE(iton_bt_send2(0xa6, 0x51, short_frames + 1));
    EXPECT_FALSE(iton_bt_send2(0xa6, 0x51, short_frames + 2));
    EXPECT_TRUE(test_overflow());
    test_falling_edge();
    for (uint8_t i = 1; i <= short_frames + 1; ++i) {
        iton_bt_task();
        ASSERT_EQ(test_sends(), i);
        EXPECT_EQ(test_sent_byte(0), 0xa6);
        EXPECT_EQ(test_sent_byte(2), i);
        test_complete_transfer();
    }
    iton_bt_task();
    EXPECT_EQ(test_sends(), short_frames + 1);
}

TEST(ItonOwnership, FullQueueAndDeferredFrameEventuallySendConsumerRelease) {
    test_reset();
    iton_bt_init();
    test_rising_edge();
    for (uint16_t i = 1; i <= short_frames + 1; ++i) iton_bt_send_consumer(i);
    iton_bt_send_consumer(0);
    EXPECT_TRUE(test_overflow());
    test_falling_edge();
    for (uint8_t i = 1; i <= short_frames + 1; ++i) {
        iton_bt_task();
        ASSERT_EQ(test_sends(), i);
        EXPECT_EQ(test_sent_byte(0), 0xa3);
        EXPECT_EQ(test_sent_byte(2), i);
        test_complete_transfer();
    }
    iton_bt_task();
    ASSERT_EQ(test_sends(), short_frames + 2);
    EXPECT_EQ(test_sent_byte(0), 0xa3);
    EXPECT_EQ(test_sent_byte(1), 0);
    EXPECT_EQ(test_sent_byte(2), 0);
}

TEST(ItonOwnership, SaturatedConsumerClickPreservesPressAndRelease) {
    test_reset();
    iton_bt_init();
    test_rising_edge();
    for (uint8_t i = 1; i <= short_frames + 1; ++i) ASSERT_TRUE(iton_bt_send2(0xa6, 0x51, i));
    iton_bt_send_consumer(0x00e9);
    iton_bt_send_system(0);
    iton_bt_send_consumer(0);
    iton_bt_send_consumer(0x00ea); /* must not pass an already deferred release */
    iton_bt_send_consumer(0);
    test_send_keyboard_pair(4, 0);
    test_falling_edge();
    for (uint8_t i = 1; i <= short_frames + 1; ++i) {
        iton_bt_task(); ASSERT_EQ(test_sent_byte(0), 0xa6); test_complete_transfer();
    }
    iton_bt_task(); ASSERT_EQ(test_sent_byte(0), 0xa3); EXPECT_EQ(test_sent_byte(2), 0xe9); test_complete_transfer();
    iton_bt_task(); ASSERT_EQ(test_sent_byte(0), 0xa4); test_complete_transfer();
    iton_bt_task(); ASSERT_EQ(test_sent_byte(0), 0xa3); EXPECT_EQ(test_sent_byte(2), 0); test_complete_transfer();
    iton_bt_task(); ASSERT_EQ(test_sent_byte(0), 0xa1); EXPECT_EQ(test_sent_byte(3), 4); test_complete_transfer();
    EXPECT_TRUE(iton_bt_is_idle());
}

TEST(ItonOwnership, RejectedConsumerReleasePrecedesLaterControl) {
    test_reset();
    iton_bt_init();
    test_rising_edge();
    for (uint16_t i = 1; i <= short_frames + 1; ++i) iton_bt_send_consumer(i);
    iton_bt_send_consumer(0);
    EXPECT_FALSE(iton_bt_send2(0xa6, 0x51, 99));
    test_falling_edge();
    for (uint8_t i = 1; i <= short_frames + 1; ++i) {
        iton_bt_task();
        EXPECT_EQ(test_sent_byte(2), i);
        test_complete_transfer();
    }
    iton_bt_task();
    ASSERT_EQ(test_sends(), short_frames + 2);
    EXPECT_EQ(test_sent_byte(0), 0xa3);
    EXPECT_EQ(test_sent_byte(2), 0);
    test_complete_transfer();
    EXPECT_TRUE(iton_bt_send2(0xa6, 0x51, 99));
    EXPECT_EQ(test_sent_byte(0), 0xa6);
    EXPECT_EQ(test_sent_byte(2), 99);
}

TEST(ItonOwnership, FullQueueRetainsLatestConsumerRelease) {
    test_reset();
    iton_bt_init();
    test_rising_edge();
    for (uint16_t i = 1; i <= short_frames; ++i) iton_bt_send_consumer(i);
    iton_bt_send_consumer(0);
    test_falling_edge();
    for (uint8_t i = 1; i <= short_frames + 1; ++i) {
        iton_bt_task();
        ASSERT_EQ(test_sends(), i);
        EXPECT_EQ(test_sent_byte(0), 0xa3);
        EXPECT_EQ(test_sent_byte(2), i == short_frames + 1 ? 0 : i);
        test_complete_transfer();
    }
}

TEST(ItonOwnership, FullMixedQueueDefersReleaseWithoutReorderingControl) {
    test_reset();
    iton_bt_init();
    test_rising_edge();
    iton_bt_send_consumer(1);
    for (uint8_t i = 1; i < short_frames; ++i) iton_bt_send2(0xa6, 0x51, i);
    iton_bt_send_consumer(0);
    EXPECT_FALSE(test_overflow());
    test_falling_edge();
    iton_bt_task();
    EXPECT_EQ(test_sent_byte(0), 0xa3);
    EXPECT_EQ(test_sent_byte(2), 1);
    test_complete_transfer();
    for (uint8_t i = 1; i < short_frames; ++i) {
        iton_bt_task();
        ASSERT_EQ(test_sent_byte(0), 0xa6);
        EXPECT_EQ(test_sent_byte(2), i);
        test_complete_transfer();
    }
    iton_bt_task();
    ASSERT_EQ(test_sends(), short_frames + 1);
    EXPECT_EQ(test_sent_byte(0), 0xa3);
    EXPECT_EQ(test_sent_byte(2), 0);
}

TEST(ItonOwnership, LaterControlCannotPassDeferredRelease) {
    test_reset();
    iton_bt_init();
    test_rising_edge();
    for (uint8_t i = 1; i <= short_frames; ++i) ASSERT_TRUE(iton_bt_send2(0xa6, 0x51, i));
    iton_bt_send_consumer(0);
    EXPECT_FALSE(iton_bt_send2(0xa6, 0x51, short_frames + 1));
    EXPECT_TRUE(test_overflow());
    test_falling_edge();
    for (uint8_t i = 1; i <= short_frames; ++i) {
        iton_bt_task();
        EXPECT_EQ(test_sent_byte(0), 0xa6);
        EXPECT_EQ(test_sent_byte(2), i);
        test_complete_transfer();
    }
    iton_bt_task();
    ASSERT_EQ(test_sends(), short_frames + 1);
    EXPECT_EQ(test_sent_byte(0), 0xa3);
    EXPECT_EQ(test_sent_byte(2), 0);
}

TEST(ItonOwnership, SaturationEventuallySendsDeferredReportsInOrder) {
    uint8_t order[] = {0xa1, 0xa3, 0xa4};
    do {
        test_reset();
        iton_bt_init();
        test_rising_edge();
        for (uint8_t i = 1; i <= short_frames + 1; ++i) ASSERT_TRUE(iton_bt_send2(0xa6, 0x51, i));
        for (uint8_t cmd : order) {
            if (cmd == 0xa1) test_send_keyboard_pair(4, 0);
            if (cmd == 0xa3) iton_bt_send_consumer(0);
            if (cmd == 0xa4) iton_bt_send_system(0);
        }
        test_falling_edge();
        for (uint8_t i = 0; i < short_frames + 1; ++i) { iton_bt_task(); test_complete_transfer(); }
        for (uint8_t cmd : order) {
            iton_bt_task();
            EXPECT_EQ(test_sent_byte(0), cmd);
            test_complete_transfer();
        }
        EXPECT_TRUE(iton_bt_is_idle());
    } while (std::next_permutation(order, order + 3));
}

TEST(ItonOwnership, DeferredKeyboardAndConsumerReleaseMaintainOrder) {
    for (bool consumer_first : {false, true}) {
        test_reset();
        iton_bt_init();
        test_rising_edge();
        for (uint8_t i = 1; i <= short_frames + 1; ++i) ASSERT_TRUE(iton_bt_send2(0xa6, 0x51, i));
        if (consumer_first) iton_bt_send_consumer(0);
        test_send_keyboard_pair(4, 0);
        if (!consumer_first) iton_bt_send_consumer(0);
        test_falling_edge();
        for (uint8_t i = 0; i < short_frames + 1; ++i) { iton_bt_task(); test_complete_transfer(); }
        iton_bt_task();
        EXPECT_EQ(test_sent_byte(0), consumer_first ? 0xa3 : 0xa1);
        test_complete_transfer();
        iton_bt_task();
        EXPECT_EQ(test_sent_byte(0), consumer_first ? 0xa1 : 0xa3);
        test_complete_transfer();
        EXPECT_TRUE(iton_bt_is_idle());
    }
}

TEST(ItonOwnership, SaturationEventuallyClearsSixthKey) {
    test_reset();
    iton_bt_init();
    test_send_keyboard_pair(4, 42);
    test_complete_transfer();
    iton_bt_task();
    test_complete_transfer();
    test_rising_edge();
    for (uint8_t i = 1; i <= short_frames + 1; ++i) ASSERT_TRUE(iton_bt_send2(0xa6, 0x51, i));
    test_send_keyboard_pair(0, 0);
    test_falling_edge();
    for (uint8_t i = 0; i < short_frames + 1; ++i) { iton_bt_task(); test_complete_transfer(); }
    iton_bt_task();
    EXPECT_EQ(test_sent_byte(0), 0xa1);
    EXPECT_EQ(test_sent_byte(3), 0);
    test_complete_transfer();
    iton_bt_task();
    EXPECT_EQ(test_sent_byte(0), 0xa2);
    for (uint8_t i = 1; i < 16; ++i) EXPECT_EQ(test_sent_byte(i), 0);
}

TEST(ItonOwnership, SaturationResynchronizesLatestKeyboardState) {
    test_reset();
    iton_bt_init();
    test_rising_edge();
    for (uint8_t i = 1; i <= short_frames + 1; ++i) ASSERT_TRUE(iton_bt_send2(0xa6, 0x51, i));
    test_send_keyboard_pair(4, 42);
    test_send_keyboard_pair(0, 0);
    test_falling_edge();
    for (uint8_t i = 1; i <= short_frames + 1; ++i) {
        iton_bt_task();
        EXPECT_EQ(test_sent_byte(0), 0xa6);
        EXPECT_EQ(test_sent_byte(2), i);
        test_complete_transfer();
    }
    iton_bt_task();
    ASSERT_EQ(test_sends(), short_frames + 2);
    EXPECT_EQ(test_sent_byte(0), 0xa1);
    EXPECT_EQ(test_sent_byte(3), 0);
    test_complete_transfer();
    iton_bt_task();
    EXPECT_TRUE(iton_bt_is_idle());
}

TEST(ItonOwnership, KeyboardBurstRetainsEachPressAndReleaseWhileRadioIsBusy) {
    test_reset();
    iton_bt_init();
    test_rising_edge();
    const uint8_t reports[] = {4, 0, 5, 0, 6, 0, 7, 0, 8, 0, 9, 0};
    for (uint8_t key : reports) test_send_keyboard_pair(key, 0);
    test_falling_edge();
    for (uint8_t key : reports) {
        iton_bt_task();
        ASSERT_EQ(test_sent_byte(0), 0xa1);
        ASSERT_EQ(test_sent_byte(3), key);
        test_complete_transfer();
    }
    EXPECT_EQ(test_sends(), sizeof(reports));
    EXPECT_FALSE(test_overflow());
    EXPECT_TRUE(iton_bt_is_idle());
}

TEST(ItonOwnership, FullQueueRetainsLatestKeyboardRelease) {
    test_reset();
    iton_bt_init();
    test_rising_edge();
    uint8_t pressed[8] = {0, 0, 4};
    uint8_t released[8] = {0};
    for (unsigned i = 0; i < keyboard_frames; ++i) iton_bt_send(0xa1, pressed, sizeof(pressed));
    iton_bt_send(0xa1, released, sizeof(released));
    test_falling_edge();
    for (unsigned i = 0; i < keyboard_frames; ++i) {
        iton_bt_task();
        EXPECT_EQ(test_sent_byte(3), 4);
        test_complete_transfer();
    }
    iton_bt_task();
    EXPECT_EQ(test_sent_byte(0), 0xa1);
    EXPECT_EQ(test_sent_byte(3), 0);
}

TEST(ItonOwnership, FullQueueDefersReleaseAcrossRingBoundary) {
    test_reset();
    iton_bt_init();
    for (uint8_t i = 0; i < 5; ++i) {
        iton_bt_send_consumer(i + 1);
        test_complete_transfer();
    }
    test_rising_edge();
    for (uint8_t i = 1; i <= short_frames; ++i) iton_bt_send_consumer(i);
    iton_bt_send_consumer(0);
    test_falling_edge();
    for (uint8_t i = 1; i <= short_frames + 1; ++i) {
        iton_bt_task();
        EXPECT_EQ(test_sent_byte(0), 0xa3);
        EXPECT_EQ(test_sent_byte(2), i == short_frames + 1 ? 0 : i);
        test_complete_transfer();
    }
}
TEST(ItonOwnership, IdleRequiresQueueTransferAndInputToBeClear) {
    test_reset();
    iton_bt_init();
    EXPECT_TRUE(iton_bt_is_idle());
    iton_bt_send_consumer(1);
    EXPECT_FALSE(iton_bt_is_idle()); // hardware transfer still active
    test_complete_transfer();
    EXPECT_TRUE(iton_bt_is_idle());
    test_rising_edge();
    EXPECT_FALSE(iton_bt_is_idle()); // receive in progress
    iton_bt_send_consumer(0);
    EXPECT_FALSE(iton_bt_is_idle()); // receive and queued report
    test_falling_edge();
    EXPECT_FALSE(iton_bt_is_idle()); // queued report has not started
    iton_bt_task();
    EXPECT_FALSE(iton_bt_is_idle()); // sending is not delivery
    test_complete_transfer();
    EXPECT_TRUE(iton_bt_is_idle());
}

TEST(ItonOwnership, TransmitCompletionLowersHandshake) {
    test_reset();
    iton_bt_init();
    uint8_t payload = 0;
    iton_bt_send(0xa3, &payload, 1);
    EXPECT_TRUE(test_output_high());
    test_complete_transfer();
    EXPECT_FALSE(test_output_high());
}

TEST(ItonOwnership, FullReceiveWithUnknownBoundaryIsNotDispatched) {
    test_reset();
    iton_bt_init();
    test_full_receive();
    EXPECT_FALSE(test_connected());
}
TEST(ItonOwnership, FallingReceiveSignalDoesNotStopTransmit) {
    test_reset();
    iton_bt_init();
    uint8_t payload = 0;
    iton_bt_send(0xa3, &payload, 1);
    test_falling_edge();
    EXPECT_FALSE(test_stopped_tx());
}

TEST(ItonOwnership, ReceiveDoesNotStartDuringTransmit) {
    test_reset();
    iton_bt_init();
    uint8_t payload = 0;
    iton_bt_send(0xa3, &payload, 1);
    test_rising_edge();
    EXPECT_FALSE(test_overlapping_start());
    EXPECT_TRUE(test_rx_collision());
}

TEST(ItonOwnership, ReceiveStillRequestedAtTransmitCompletionIsArmed) {
    test_reset();
    iton_bt_init();
    iton_bt_send_consumer(1);
    test_rising_edge();
    EXPECT_EQ(test_receive_count(), 0u);
    test_complete_transfer();
    EXPECT_EQ(test_receive_count(), 1u);
    EXPECT_FALSE(test_overlapping_start());
    EXPECT_FALSE(test_output_high());
}

TEST(ItonOwnership, EndedReceiveRequestIsNotArmedAfterTransmit) {
    test_reset();
    iton_bt_init();
    iton_bt_send_consumer(1);
    test_rising_edge();
    test_falling_edge();
    test_complete_transfer();
    EXPECT_EQ(test_receive_count(), 0u);
    EXPECT_TRUE(test_rx_collision());
}

TEST(ItonOwnership, IncomingBytesDoNotOverwriteTransmitBuffer) {
    test_reset();
    iton_bt_init();
    EXPECT_TRUE(test_receive_preserves_active_send());
}

TEST(ItonOwnership, TransmitDoesNotStartDuringReceive) {
    test_reset();
    iton_bt_init();
    EXPECT_TRUE(test_send_preserves_active_receive());
    EXPECT_FALSE(test_overlapping_start());
}

TEST(ItonOwnership, ConsumerReleaseIsRetainedUntilReceiveEnds) {
    test_reset();
    iton_bt_init();
    test_rising_edge();
    iton_bt_send_consumer(0);
    EXPECT_FALSE(test_overlapping_start());
    test_falling_edge();
    iton_bt_task();
    EXPECT_TRUE(test_sent_release());
    EXPECT_FALSE(test_stopped_tx());
}

TEST(ItonOwnership, OldStopLosesPendingByteControl) {
    test_reset();
    iton_bt_init();
    test_receive_notification(true);
    EXPECT_FALSE(test_connected());
}

TEST(ItonOwnership, NotificationIsRecognizedWhenAllBytesHaveReachedBuffer) {
    test_reset();
    iton_bt_init();
    test_receive_notification(false);
    EXPECT_TRUE(test_connected());
}

TEST(ItonOwnership, ShortNotificationDoesNotReusePreviousPayload) {
    test_reset();
    iton_bt_init();
    test_short_notification();
    EXPECT_FALSE(test_connected());
}

TEST(ItonOwnership, PendingFinalByteIsRecognizedWhenStopDrainsFifo) {
    test_reset();
    iton_bt_init();
    test_enable_pending_drain();
    test_receive_notification(true);
    EXPECT_TRUE(test_connected());
}

TEST(ItonOwnership, OversizedNotificationDoesNotSetConnected) {
    test_reset();
    iton_bt_init();
    test_oversized_notification();
    EXPECT_FALSE(test_connected());
    iton_bt_task();
    EXPECT_EQ(test_sends(), 0u);
}

TEST(ItonOwnership, BluetoothLedStateIsVisibleThroughHostInterface) {
    test_reset();
    iton_bt_init();
    test_receive_led(2);
    EXPECT_EQ(bluetooth_keyboard_leds(), 2);
}

TEST(ItonOwnership, BluetoothSystemUsageReachesTransport) {
    test_reset();
    iton_bt_init();
    bluetooth_send_system(0x81);
    ASSERT_EQ(test_sends(), 1u);
    EXPECT_EQ(test_sent_byte(0), 0xa4);
    EXPECT_EQ(test_sent_byte(1), 0x81);
}
