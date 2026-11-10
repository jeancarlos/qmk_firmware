#include "gtest/gtest.h"
#include <cstdint>

extern "C" {
int fixture_run(void);
int fixture_last_channel(void);
int fixture_last_value(void);
void fixture_render_key(uint8_t row, uint8_t col, uint8_t r, uint8_t g, uint8_t b);
int fixture_pwm_at(int pin, int col);
}

TEST(Sn32Rgb, EmptyMatrixPositionsDoNotEnablePwm) {
    EXPECT_EQ(fixture_run(), 1);
    EXPECT_EQ(fixture_last_channel(), 0);
    EXPECT_EQ(fixture_last_value(), 42);
}


// GPIO banks A, B, C, D are numbered consecutively in the hardware stub.
TEST(Sn32Rgb, K2RoutesEachPrimaryToItsOwnRow) {
    const int red[]   = {35, 38, 41, 44, 29, 30}; // C3, C6, C9, C12, B13, B14
    const int green[] = {32, 36, 39, 42, 45, 51}; // C0, C4, C7, C10, C13, D3
    const int blue[]  = {33, 37, 40, 43, 46, 31}; // C1, C5, C8, C11, C14, B15
    for (uint8_t row = 0; row < 6; ++row) {
        SCOPED_TRACE(row);
        fixture_render_key(row, 1, 64, 128, 255);
        for (uint8_t other = 0; other < 6; ++other) {
            EXPECT_EQ(fixture_pwm_at(red[other], 1), other == row ? 64 : 0);
            EXPECT_EQ(fixture_pwm_at(green[other], 1), other == row ? 128 : 0);
            EXPECT_EQ(fixture_pwm_at(blue[other], 1), other == row ? 255 : 0);
        }
    }
}

TEST(Sn32Rgb, K2WhiteProfileIndicatorsUseAllThreePrimaries) {
    for (uint8_t col = 1; col <= 3; ++col) {
        SCOPED_TRACE(col);
        fixture_render_key(1, col, 255, 255, 255);
        EXPECT_EQ(fixture_pwm_at(38, col), 255); // C6
        EXPECT_EQ(fixture_pwm_at(36, col), 255); // C4
        EXPECT_EQ(fixture_pwm_at(37, col), 255); // C5
    }
}
