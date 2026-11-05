// Include the driver to exercise its static scanner with stubbed hardware.
#include "../sn32f2xx.c"
PWMDriver          PWMD1;
struct flash_state EFLD1;
struct led_config  g_led_config;
static int         enabled;
static int         last_channel;
static int         last_value;
void               pwmDisableChannel(PWMDriver *p, int c) {
    (void)p;
    (void)c;
}
void pwmEnableChannel(PWMDriver *p, int c, int v) {
    (void)p;
    last_channel = c;
    last_value   = v;
    enabled++;
}
void gpio_write_pin_high(int p) {
    (void)p;
}
void gpio_write_pin_low(int p) {
    (void)p;
}
int fixture_run(void) {
    enabled         = 0;
    current_row     = 0;
    current_key_row = 0;
    for (int col = 0; col < 16; col++)
        chan_col_order[col] = col;
    memset(&g_led_config, 255, sizeof g_led_config);
    g_led_config.matrix_co[0][0] = 0;
    led_state[0].g               = 42;
    update_pwm_channels(&PWMD1);
    return enabled;
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
