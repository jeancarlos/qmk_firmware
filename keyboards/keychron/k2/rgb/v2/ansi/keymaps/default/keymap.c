/*
Copyright 2022 Dimitris Mantzouranis <d3xter93@gmail.com>
Copyright 2022 Pablo Ramirez
Copyright 2023 Santanu Paik (https://github.com/ITNerd69)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include QMK_KEYBOARD_H

#ifdef BLUETOOTH_ITON_BT
#    include "connection.h"
#    include "dip_switch.h"
#    include "iton_bt.h"
#    include "usb_driver.h"
#    include "usb_endpoints.h"
#    ifdef ITON_BT_TIMING
#        include "usb_device_state.h"
#    endif

extern usb_endpoint_in_t usb_endpoints_in[USB_ENDPOINT_IN_COUNT];

static bool k2_mode_usb_idle(void) {
    if (!usb_endpoint_in_is_inactive(&usb_endpoints_in[USB_ENDPOINT_IN_KEYBOARD])) return false;
#    if defined(SHARED_EP_ENABLE)
    if (!usb_endpoint_in_is_inactive(&usb_endpoints_in[USB_ENDPOINT_IN_SHARED])) return false;
#    endif
    return true;
}

static bool k2_mode_matrix_clear(void) {
    for (uint8_t row = 0; row < MATRIX_ROWS; ++row) {
        if (matrix_get_row(row)) return false;
    }
    return true;
}
/* Make the next matrix scan emit presses for keys held during the transition. */
static void k2_mode_rearm_keys(void) {
    extern matrix_row_t matrix_previous[MATRIX_ROWS];
    for (uint8_t row = 0; row < MATRIX_ROWS; ++row) {
        // Fn is processed even while blocked: retain its release history.
        matrix_previous[row] &= row == 5 ? ((matrix_row_t)1 << 11) : 0;
    }
}
#    include "k2_bt_profile.inc"
#    include "k2_mode_transition.inc"
#endif

// Each layer gets a name for readability, which is then used in the keymap matrix below.
// The underscores don't mean anything - you can have a layer called STUFF or any other name.
// Layer names don't all need to be of the same length, obviously, and you can also skip them
// entirely and just use numbers.
enum layer_names {
    WIN_BASE = 0,
    WIN_FN   = 1,
    MAC_BASE = 2,
    MAC_FN   = 3,
};
#define KC_TASK LGUI(KC_TAB)        // Task viewer
#define KC_FLXP LGUI(KC_E)          // Windows file explorer

#define KC_MSSN LGUI(KC_F3)         // Mission Control
#define KC_FIND LALT(LGUI(KC_SPC))  // Finder
#define KC_MSCR LSFT(LGUI(KC_3))    // Mac screenshot
#define KC_MSNP LSFT(LGUI(KC_4))    // Mac snip tool


const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

  /*
    *****************************************************************************************************************
    * ESC  * F1   * F2   * F3   * F4   * F5   * F6   * F7   * F8   * F9   * F10  * F11  * F12  * PSCR * DEL  * RMOD *
    *****************************************************************************************************************
    * ~    * 1    * 2    * 3    * 4    * 5    * 6    * 7    * 8    * 9    * 0    * *    * =    * BSPC        * PGUP *
    *****************************************************************************************************************
    * TAB     * Q    * W    * E    * R    * T    * Y    * U    * I    * O    * P    * [    * ]    * \        * PGDN *
    *****************************************************************************************************************
    * CAPS      * A    * S    * D    * F    * G    * H    * J    * K    * L    * ;    * '    * ENT           * HOME *
    *****************************************************************************************************************
    * LSFT         * Z    * X    * C    * V    * B    * N    * M    * ,    * .    * /    * RSFT       * UP   * END  *
    *****************************************************************************************************************
    * LCTL  * LGUI  * LALT  * SPC                                         * RALT * FN   * RCTL * LEFT * DOWN * RGHT *
    *****************************************************************************************************************
  */
  [WIN_BASE] = LAYOUT_ansi(
  /*  0           1           2           3           4           5           6           7           8           9           10          11          12          13          14          15       */
      KC_ESC,     KC_F1,      KC_F2,      KC_F3,      KC_F4,      KC_F5,      KC_F6,      KC_F7,      KC_F8,      KC_F9,      KC_F10,     KC_F11,     KC_F12,     KC_PSCR,    KC_DEL,     RM_NEXT  ,
      KC_GRV,     KC_1,       KC_2,       KC_3,       KC_4,       KC_5,       KC_6,       KC_7,       KC_8,       KC_9,       KC_0,       KC_MINS,    KC_EQL,     KC_BSPC,                KC_PGUP  ,
      KC_TAB,     KC_Q,       KC_W,       KC_E,       KC_R,       KC_T,       KC_Y,       KC_U,       KC_I,       KC_O,       KC_P,       KC_LBRC,    KC_RBRC,    KC_BSLS,                KC_PGDN  ,
      KC_CAPS,    KC_A,       KC_S,       KC_D,       KC_F,       KC_G,       KC_H,       KC_J,       KC_K,       KC_L,       KC_SCLN,    KC_QUOT,                KC_ENT,                 KC_HOME  ,
      KC_LSFT,                KC_Z,       KC_X,       KC_C,       KC_V,       KC_B,       KC_N,       KC_M,       KC_COMM,    KC_DOT,     KC_SLSH,                KC_RSFT,    KC_UP,      KC_END   ,
      KC_LCTL,    KC_LGUI,    KC_LALT,                                        KC_SPC,                                         KC_RALT,    MO(WIN_FN), KC_RCTL,    KC_LEFT,    KC_DOWN,    KC_RGHT
  ),

  /*
    *****************************************************************************************************************
    *RESET * BRID * BRIU * TASK * FLXP * RVAD * RVAI * MPRV * MPLY * MNXT * MUTE * VOLD * VOLU *      *  INS * RTOG *
    *****************************************************************************************************************
    *      *      *      *      *      *      *      *      *      *      *      *      *      *             *      *
    *****************************************************************************************************************
    *         *      *      *      *      *      *      *      *      *      *      *      *      *          *      *
    *****************************************************************************************************************
    *           *      *      *      *      *      *      *      *      *      *      *      *               *      *
    *****************************************************************************************************************
    *              *      *      *      *      *      *      *      *      *      *      *            * RSAI *      *
    *****************************************************************************************************************
    *       *       *       *                                             *      *      *      * RHUD * RSAD * RHUI *
    *****************************************************************************************************************
  */
  [WIN_FN] = LAYOUT_ansi(
  /*  0           1           2           3           4           5           6           7           8           9           10          11          12          13          14          15       */
      QK_BOOT,      KC_BRID,    KC_BRIU,    KC_TASK,    KC_FLXP,    RM_VALD,    RM_VALU,    KC_MPRV,    KC_MPLY,    KC_MNXT,    KC_MUTE,    KC_VOLD,    KC_VOLU,    _______,    KC_INS,     RM_TOGG  ,
      _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,                _______  ,
      _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,                _______  ,
      _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,                _______,                _______  ,
      _______,                _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,                _______,    RM_SATU,    _______  ,
      _______,    _______,    _______,                                        _______,                                        _______,    _______,    _______,    RM_HUED,    RM_SATD,    RM_HUEU
  ),

    /*
    *****************************************************************************************************************
    * ESC  * F1   * F2   * F3   * F4   * F5   * F6   * F7   * F8   * F9   * F10  * F11  * F12  * PSCR * DEL  * RMOD *
    *****************************************************************************************************************
    * ~    * 1    * 2    * 3    * 4    * 5    * 6    * 7    * 8    * 9    * 0    * *    * =    * BSPC        * PGUP *
    *****************************************************************************************************************
    * TAB     * Q    * W    * E    * R    * T    * Y    * U    * I    * O    * P    * [    * ]    * \        * PGDN *
    *****************************************************************************************************************
    * CAPS      * A    * S    * D    * F    * G    * H    * J    * K    * L    * ;    * '    * ENT           * HOME *
    *****************************************************************************************************************
    * LSFT         * Z    * X    * C    * V    * B    * N    * M    * ,    * .    * /    * RSFT       * UP   * END  *
    *****************************************************************************************************************
    * LCTL  * LALT  * LGUI  * SPC                                         * LGUI * FN   * RCTL * LEFT * DOWN * RGHT *
    *****************************************************************************************************************
  */
  [MAC_BASE] = LAYOUT_ansi(
  /*  0           1           2           3           4           5           6           7           8           9           10          11          12          13          14          15       */
      KC_ESC,     KC_F1,      KC_F2,      KC_F3,      KC_F4,      KC_F5,      KC_F6,      KC_F7,      KC_F8,      KC_F9,      KC_F10,     KC_F11,     KC_F12,     KC_MSCR,    KC_DEL,     RM_NEXT  ,
      KC_GRV,     KC_1,       KC_2,       KC_3,       KC_4,       KC_5,       KC_6,       KC_7,       KC_8,       KC_9,       KC_0,       KC_MINS,    KC_EQL,     KC_BSPC,                KC_PGUP  ,
      KC_TAB,     KC_Q,       KC_W,       KC_E,       KC_R,       KC_T,       KC_Y,       KC_U,       KC_I,       KC_O,       KC_P,       KC_LBRC,    KC_RBRC,    KC_BSLS,                KC_PGDN  ,
      KC_CAPS,    KC_A,       KC_S,       KC_D,       KC_F,       KC_G,       KC_H,       KC_J,       KC_K,       KC_L,       KC_SCLN,    KC_QUOT,                KC_ENT,                 KC_HOME  ,
      KC_LSFT,                KC_Z,       KC_X,       KC_C,       KC_V,       KC_B,       KC_N,       KC_M,       KC_COMM,    KC_DOT,     KC_SLSH,                KC_RSFT,    KC_UP,      KC_END   ,
      KC_LCTL,    KC_LALT,    KC_LGUI,                                        KC_SPC,                                         KC_RGUI,    MO(MAC_FN), KC_RCTL,    KC_LEFT,    KC_DOWN,    KC_RGHT
  ),

  /*
    *****************************************************************************************************************
    *RESET * BRID * BRIU * TASK * FLXP * RVAD * RVAI * MPRV * MPLY * MNXT * MUTE * VOLD * VOLU *      *  INS * RTOG *
    *****************************************************************************************************************
    *      *      *      *      *      *      *      *      *      *      *      *      *      *             *      *
    *****************************************************************************************************************
    *         *      *      *      *      *      *      *      *      *      *      *      *      *          *      *
    *****************************************************************************************************************
    *           *      *      *      *      *      *      *      *      *      *      *      *               *      *
    *****************************************************************************************************************
    *              *      *      *      *      *      *      *      *      *      *      *            * RSAI *      *
    *****************************************************************************************************************
    *       *       *       *                                             *      *      *      * RHUD * RSAD * RHUI *
    *****************************************************************************************************************
  */
  [MAC_FN] = LAYOUT_ansi(
  /*  0           1           2           3           4           5           6           7           8           9           10          11          12          13          14          15       */
      QK_BOOT,      KC_BRID,    KC_BRIU,    KC_MSSN,    KC_FIND,    RM_VALD,    RM_VALU,    KC_MPRV,    KC_MPLY,    KC_MNXT,    KC_MUTE,    KC_VOLD,    KC_VOLU,    KC_MSNP,    KC_INS,     RM_TOGG  ,
      _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,                _______  ,
      _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,                _______  ,
      _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,                _______,                _______  ,
      _______,                _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,                _______,    RM_SATU,    _______  ,
      _______,    _______,    _______,                                        _______,                                        _______,    _______,    _______,    RM_HUED,    RM_SATD,    RM_HUEU
  )
};

bool dip_switch_update_user(uint8_t index, bool active) {
  switch(index) {
    case 0: // OS switch
      if (active) { // Mac/iOS mode
        layer_move(MAC_BASE);
      }
      else { // Windows/Android mode
        layer_move(WIN_BASE);
      }
      break;
#ifdef BLUETOOTH_ITON_BT
    case 1: // Connection switch
      // The V1.13 firmware selects Bluetooth when D5 is high. DIP callbacks
      // invert that level and run even before the SPI driver is initialized.
      k2_mode_request(active ? CONNECTION_HOST_USB : CONNECTION_HOST_BLUETOOTH);
      break;
#endif
  }
  return true;
}

#ifdef BLUETOOTH_ITON_BT
static bool k2_bt_shortcuts_active(void) {
    return connection_get_host() == CONNECTION_HOST_BLUETOOTH &&
           k2_mode_desired != CONNECTION_HOST_USB && !k2_mode_blocks_keys();
}

static bool k2_fn_active(void) {
    return layer_state_is(WIN_FN) || layer_state_is(MAC_FN);
}

bool pre_process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (keycode >= KC_1 && keycode <= KC_3 &&
        k2_bt_profile_key(keycode - KC_1, record->event.pressed, k2_bt_shortcuts_active() && k2_fn_active())) {
        return false;
    }
    // Keep the bootloader shortcut available if the transport stalls.
    return !k2_mode_blocks_keys() || keycode == MO(WIN_FN) || keycode == MO(MAC_FN) || keycode == QK_BOOT;
}
void housekeeping_task_user(void) {
    k2_mode_task();
    k2_bt_profile_task(k2_bt_shortcuts_active(), k2_fn_active());
#    ifdef ITON_BT_TIMING
    static uint32_t last_log;
    if (connection_get_host() == CONNECTION_HOST_USB && usb_device_state_get_configure_state() == USB_DEVICE_STATE_CONFIGURED && timer_elapsed32(last_log) >= 1000) {
        last_log = timer_read32();
        iton_bt_log_timing();
    }
#    endif
}
#    ifdef RGB_MATRIX_STATUS
bool rgb_matrix_render_status_kb(void) {
    static uint8_t last_led = K2_BT_NORMAL_RGB;
    uint8_t led = K2_BT_NORMAL_RGB;
    if (connection_get_host() == CONNECTION_HOST_BLUETOOTH || k2_mode_desired == CONNECTION_HOST_BLUETOOTH) {
        led = k2_bt_status_led();
    }
    if (led == K2_BT_NORMAL_RGB) {
        last_led = led;
        return false;
    }
    if (led != last_led) {
        rgb_matrix_set_color_all(0, 0, 0);
        if (led > 0) {
            const uint8_t white = RGB_MATRIX_MAXIMUM_BRIGHTNESS;
            rgb_matrix_set_color(g_led_config.matrix_co[1][led], white, white, white);
        }
        rgb_matrix_update_pwm_buffers();
        last_led = led;
    }
    return true;
}
#    endif

void keyboard_post_init_user(void) {
    k2_bt_load_profile(eeconfig_read_user());
    gpio_set_pin_input(D5);
    dip_switch_read(true);
    housekeeping_task_user();
}
#endif
