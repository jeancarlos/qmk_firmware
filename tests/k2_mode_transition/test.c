#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

enum { CONNECTION_HOST_NONE = 1, CONNECTION_HOST_USB = 2, CONNECTION_HOST_BLUETOOTH = 3 };
static bool idle = true, usb_idle = false, allow_command = true, matrix_empty = false;
static uint16_t now;
static uint8_t k2_bt_profile = 2, sent_profile;
static inline uint16_t timer_read(void) { return now; }
static inline uint16_t timer_elapsed(uint16_t start) { return now - start; }
static bool k2_mode_usb_idle(void) { return usb_idle; }
static bool k2_mode_matrix_clear(void) { return matrix_empty; }
static unsigned events, selections, commands;
static unsigned rearmed;
static void k2_mode_rearm_keys(void) { ++rearmed; }
static uint8_t selected, command_target;
static uint8_t connection_get_host(void) { return selected; }
static void connection_set_host_noeeprom(uint8_t host) { selected = host; ++selections; events = events * 10 + 1; }
static bool iton_bt_is_idle(void) { return idle; }
static bool iton_bt_mode_usb(void) { if (!allow_command) return false; command_target = CONNECTION_HOST_USB; ++commands; events = events * 10 + 3; return true; }
static bool k2_bt_select_profile(uint8_t profile) { sent_profile = profile; if (!allow_command) return false; command_target = CONNECTION_HOST_BLUETOOTH; ++commands; events = events * 10 + 3; return true; }
#include "../../keyboards/keychron/k2/rgb/v2/ansi/keymaps/default/k2_mode_transition.inc"
static void host_task_clear_old(void) { events = events * 10 + 2; }
int main(void) {
    /* A stalled Bluetooth module must not block wired typing at startup. */
    selected = CONNECTION_HOST_NONE;
    idle = false; usb_idle = true; matrix_empty = true;
    k2_mode_request(CONNECTION_HOST_USB);
    k2_mode_task();
    assert(k2_mode_blocks_keys()); /* host_task has not selected USB yet */
    host_task_clear_old();
    k2_mode_task();
    assert(selected == CONNECTION_HOST_USB && !k2_mode_blocks_keys());
    assert(rearmed == 1); /* Recover presses consumed before USB was selected. */
    matrix_empty = false; /* A key is held on the USB host. */
    k2_mode_request(CONNECTION_HOST_BLUETOOTH);
    k2_mode_task();
    assert(!k2_mode_blocks_keys()); /* Its release must reach USB. */
    matrix_empty = true;
    k2_mode_task();
    assert(k2_mode_blocks_keys());
    k2_mode_request(CONNECTION_HOST_USB); /* Cancel while the module is stalled. */
    k2_mode_task();
    assert(!k2_mode_blocks_keys() && rearmed == 2);
    k2_mode_phase = K2_MODE_START;
    k2_mode_first = false;
    k2_mode_startup_usb = false;
    k2_mode_startup_usb_eligible = false;
    k2_mode_desired = CONNECTION_HOST_NONE;
    selected = 0; events = selections = commands = rearmed = 0;
    idle = true; usb_idle = false; matrix_empty = false;
    k2_mode_request(CONNECTION_HOST_USB);
    assert(!k2_mode_blocks_keys()); /* allow held keys to release on the old host */
    k2_mode_task();
    assert(selections == 0 && commands == 0);
    matrix_empty = true;
    k2_mode_task(); /* host selection only */
    assert(k2_mode_blocks_keys());
    assert(selected == CONNECTION_HOST_USB && selections == 1 && commands == 0);
    host_task_clear_old();
    idle = false;
    k2_mode_task(); /* old Bluetooth releases still queued */
    assert(commands == 0 && k2_mode_blocks_keys());
    idle = true; allow_command = true;
    k2_mode_task(); /* USB release is still queued */
    assert(commands == 0 && k2_mode_blocks_keys());
    usb_idle = true; allow_command = false;
    k2_mode_task(); /* command rejected: retry later */
    assert(commands == 0 && k2_mode_blocks_keys());
    allow_command = true;
    k2_mode_task();
    assert(events == 123 && command_target == CONNECTION_HOST_USB);
    k2_mode_request(CONNECTION_HOST_BLUETOOTH); /* switch moved during transition */
    idle = false; k2_mode_task(); assert(k2_mode_blocks_keys());
    idle = true; k2_mode_task(); assert(k2_mode_blocks_keys() && rearmed == 0);
    assert(selected == CONNECTION_HOST_BLUETOOTH && commands == 1);
    host_task_clear_old(); k2_mode_task();
    assert(commands == 2 && command_target == CONNECTION_HOST_BLUETOOTH);
    assert(sent_profile == k2_bt_profile);
    idle = false; k2_mode_task(); assert(k2_mode_blocks_keys());
    idle = true; k2_mode_task(); assert(!k2_mode_blocks_keys() && rearmed == 1);
    k2_mode_request(CONNECTION_HOST_USB);
    k2_mode_task(); host_task_clear_old();
    idle = false; k2_mode_task();
    k2_mode_request(CONNECTION_HOST_BLUETOOTH); /* cancel while transfer is stuck */
    k2_mode_task();
    assert(selected == CONNECTION_HOST_BLUETOOTH);
    host_task_clear_old(); k2_mode_task();
    assert(!k2_mode_blocks_keys() && rearmed == 2);

    /* Once the mode command is accepted, do not pretend it was cancelled. */
    idle = true;
    k2_mode_request(CONNECTION_HOST_USB);
    k2_mode_task(); host_task_clear_old(); k2_mode_task();
    assert(command_target == CONNECTION_HOST_USB && commands == 3);
    idle = false;
    k2_mode_request(CONNECTION_HOST_BLUETOOTH);
    k2_mode_task();
    assert(k2_mode_blocks_keys() && selected == CONNECTION_HOST_USB && commands == 3);
    idle = true; k2_mode_task();
    assert(k2_mode_blocks_keys() && selected == CONNECTION_HOST_BLUETOOTH);
    host_task_clear_old(); k2_mode_task();
    assert(command_target == CONNECTION_HOST_BLUETOOTH && commands == 4);
    k2_mode_task();
    assert(!k2_mode_blocks_keys() && selected == CONNECTION_HOST_BLUETOOTH);

    k2_mode_request(CONNECTION_HOST_USB);
    k2_mode_task(); host_task_clear_old();
    idle = false;
    k2_mode_request(CONNECTION_HOST_BLUETOOTH); k2_mode_task();
    assert(selected == CONNECTION_HOST_BLUETOOTH);
    k2_mode_request(CONNECTION_HOST_USB); /* switch changes during rollback */
    host_task_clear_old(); k2_mode_task();
    assert(k2_mode_blocks_keys() && selected == CONNECTION_HOST_USB);
    idle = true; host_task_clear_old(); k2_mode_task();
    assert(command_target == CONNECTION_HOST_USB);
    k2_mode_task(); assert(!k2_mode_blocks_keys());

    /* An initial Bluetooth host must be released before USB can accept keys. */
    k2_mode_phase = K2_MODE_START; k2_mode_first = true;
    k2_mode_startup_usb = false; k2_mode_startup_usb_eligible = false;
    selected = CONNECTION_HOST_BLUETOOTH; idle = false;
    now = UINT16_MAX - 99; /* Timeout must also work across timer wrap. */
    k2_mode_request(CONNECTION_HOST_USB); k2_mode_task();
    host_task_clear_old(); k2_mode_task();
    assert(k2_mode_blocks_keys());

    /* A wedged radio must not prevent typing on the already selected USB host. */
    unsigned before_rearm = rearmed;
    unsigned before_command = commands;
    now += K2_USB_RECOVERY_MS - 1;
    k2_mode_task();
    assert(k2_mode_blocks_keys() && rearmed == before_rearm);
    ++now;
    k2_mode_task();
    assert(selected == CONNECTION_HOST_USB && !k2_mode_blocks_keys());
    assert(rearmed == before_rearm + 1 && commands == before_command);
    k2_mode_task();
    assert(rearmed == before_rearm + 1); /* No duplicate presses. */
    idle = true;
    k2_mode_task();
    assert(!k2_mode_blocks_keys() && command_target == CONNECTION_HOST_USB);
    k2_mode_task();
    assert(k2_mode_phase == K2_MODE_START && rearmed == before_rearm + 1);

    /* An unfinished Bluetooth mode command cannot strand the physical cable request. */
    k2_mode_request(CONNECTION_HOST_BLUETOOTH);
    k2_mode_task(); host_task_clear_old(); k2_mode_task();
    assert(command_target == CONNECTION_HOST_BLUETOOTH && k2_mode_blocks_keys());
    idle = false;
    k2_mode_request(CONNECTION_HOST_USB);
    k2_mode_task();
    assert(selected == CONNECTION_HOST_BLUETOOTH && k2_mode_blocks_keys());
    now += 1000;
    k2_mode_task();
    assert(selected == CONNECTION_HOST_USB && k2_mode_blocks_keys());
    host_task_clear_old(); k2_mode_task();
    assert(!k2_mode_blocks_keys() && command_target == CONNECTION_HOST_BLUETOOTH);

    return 0;
}
