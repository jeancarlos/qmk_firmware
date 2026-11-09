// SPDX-License-Identifier: GPL-2.0-or-later
#include <array>
#include "gtest/gtest.h"
#include "backing_mocks.hpp"

TEST(WearLevelingK2, SettingsSurviveReinitializationAndLogConsolidation) {
    auto& store = MockBackingStore::Instance();
    store.reset_instance();
    ASSERT_NE(wear_leveling_init(), WEAR_LEVELING_FAILED);

    std::array<uint8_t, WEAR_LEVELING_LOGICAL_SIZE> expected{};
    for (unsigned i = 0; i < 600; ++i) {
        const uint32_t address = (i * 37) % expected.size();
        expected[address] = static_cast<uint8_t>(i + 1);
        ASSERT_NE(wear_leveling_write(address, &expected[address], 1), WEAR_LEVELING_FAILED);
        if (i % 97 == 0) ASSERT_NE(wear_leveling_init(), WEAR_LEVELING_FAILED);
    }
    ASSERT_GT(store.erasure_count(), 0u);
    ASSERT_NE(wear_leveling_init(), WEAR_LEVELING_FAILED);
    std::array<uint8_t, WEAR_LEVELING_LOGICAL_SIZE> actual{};
    ASSERT_EQ(wear_leveling_read(0, actual.data(), actual.size()), WEAR_LEVELING_SUCCESS);
    EXPECT_EQ(actual, expected);
}
