// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// The Bluetooth build needs room for PAL callbacks and transport buffers.
// This changes the wear-leveling format and is scoped to this static keymap.
#if defined(BLUETOOTH_ITON_BT) && !defined(WEAR_LEVELING_LOGICAL_SIZE)
#    define WEAR_LEVELING_LOGICAL_SIZE 512
#endif

#ifdef BLUETOOTH_ITON_BT
#    define RGB_MATRIX_STATUS
#endif
