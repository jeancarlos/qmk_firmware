// Include the driver to exercise its static scanner with stubbed hardware.
#include "../sn32f2xx.c"
PWMDriver          PWMD1;
struct flash_state EFLD1;
struct led_config  g_led_config;
static int         enabled;
static int         last_channel;
static int         last_value;
static int         slot_pwm[16];
static int         frame_pwm[64][16];
void               pwmDisableChannel(PWMDriver *p, int c) {
    (void)p;
    (void)c;
}
void pwmEnableChannel(PWMDriver *p, int c, int v) {
    (void)p;
    slot_pwm[c] = v;
    last_channel = c;
    last_value   = v;
    enabled++;
}
void gpio_write_pin_high(int p) {
    memcpy(frame_pwm[p], slot_pwm, sizeof slot_pwm);
}
void gpio_write_pin_low(int p) {
    (void)p;
}
static void fixture_reset(void) {
    enabled         = 0;
    current_row     = 0;
    current_key_row = 0;
    for (int col = 0; col < 16; col++)
        chan_col_order[col] = col;
    memset(&g_led_config, 255, sizeof g_led_config);
    memset(led_state, 0, sizeof led_state);
    memset(frame_pwm, 0, sizeof frame_pwm);
    memset(slot_pwm, 0, sizeof slot_pwm);
}

int fixture_run(void) {
    fixture_reset();
    g_led_config.matrix_co[0][0] = 0;
    led_state[0].g               = 42;
    update_pwm_channels(&PWMD1);
    return enabled;
}

void fixture_render_key(uint8_t row, uint8_t col, uint8_t r, uint8_t g, uint8_t b) {
    fixture_reset();
    g_led_config.matrix_co[row][col] = 0;
    sn32f2xx_set_color(0, r, g, b);
    sn32f2xx_flush();
    for (int slot = 0; slot < SN32F2XX_RGB_MATRIX_ROWS_HW; slot++) {
        memset(slot_pwm, 0, sizeof slot_pwm);
        update_pwm_channels(&PWMD1);
    }
    sn32f2xx_set_color(0, 0, 0, 0);
    sn32f2xx_flush();
}

int fixture_pwm_at(int pin, int col) {
    return frame_pwm[pin][col];
}

int fixture_last_channel(void) {
    return last_channel;
}
int fixture_last_value(void) {
    return last_value;
}
void pwmEnablePeriodicNotification(PWMDriver *p) {
    (void)p;
}
void pwmDisablePeriodicNotification(PWMDriver *p) {
    (void)p;
}
void pwmEnablePeriodicNotificationI(PWMDriver *p) {
    (void)p;
}
void pwm_lld_change_counter(PWMDriver *p, int c) {
    (void)p;
    (void)c;
}
void pwmStart(PWMDriver *p, const PWMConfig *c) {
    (void)p;
    (void)c;
}
void gpio_set_pin_output_push_pull(int p) {
    (void)p;
}
void chSysLockFromISR(void) {}
void chSysUnlockFromISR(void) {}
