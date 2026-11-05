#include "gtest/gtest.h"

extern "C" {
int fixture_run(void);
int fixture_last_channel(void);
int fixture_last_value(void);
}

TEST(Sn32Rgb, EmptyMatrixPositionsDoNotEnablePwm) {
    EXPECT_EQ(fixture_run(), 1);
    EXPECT_EQ(fixture_last_channel(), 0);
    EXPECT_EQ(fixture_last_value(), 42);
}
