#pragma once
#include <stdint.h>
#include <stdbool.h>
#define COL2ROW 0
#define ROW2COL 1
#define MATRIX_ROWS 6
#define MATRIX_COLS 16
#define DIODE_DIRECTION COL2ROW
#define SN32F2 1
#define SN32F240B 1
#define SN32F2XX_RGB_MATRIX_ROW_PINS {0}
#define SN32F2XX_RGB_MATRIX_COL_PINS {0}
#define SN32F2XX_LED_COUNT 84
#define PWM_OUTPUT_ACTIVE_HIGH 1
#define PWM_OUTPUT_ACTIVE_LOW 0
#define PWM_CHANNELS 24
#define PWM_OUTPUT_DISABLED 0
#define RGB_MATRIX_MAXIMUM_BRIGHTNESS 255
#define RGB_MATRIX_HUE_STEP 1
#define RGB_MATRIX_SAT_STEP 1
#define RGB_MATRIX_VAL_STEP 1
#define RGB_MATRIX_SPD_STEP 1
#define RGB_MATRIX_LED_PROCESS_LIMIT 1
#define PAL_PORT(x) ((x) / 16)
#define PAL_PAD(x) ((x) % 16)
#define PAL_IOPORTS_WIDTH 16
#define A0 0
#define B0 16
#define C15 47
#define FLASH_PGM 1
typedef int pin_t;
typedef struct {
    int unused;
} PWMDriver;
typedef struct {
    int   mode;
    void *callback;
    int   pfpamsk;
} PWMChannelConfig;
typedef struct {
    int frequency;
    int period;
    void (*callback)(PWMDriver *);
    PWMChannelConfig channels[24];
    int              cr2;
} PWMConfig;
extern PWMDriver PWMD1;
extern struct flash_state {
    int state;
} EFLD1;
void pwmDisableChannel(PWMDriver *, int);
void pwmEnableChannel(PWMDriver *, int, int);
void pwmEnablePeriodicNotification(PWMDriver *);
void pwmDisablePeriodicNotification(PWMDriver *);
void pwmEnablePeriodicNotificationI(PWMDriver *);
void pwm_lld_change_counter(PWMDriver *, int);
void pwmStart(PWMDriver *, const PWMConfig *);
void gpio_write_pin_high(int);
void gpio_write_pin_low(int);
void gpio_set_pin_output_push_pull(int);
void chSysLockFromISR(void);
void chSysUnlockFromISR(void);
