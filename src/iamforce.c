#define _GNU_SOURCE
#include <stdint.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdint.h>
#include <alsa/asoundlib.h>
#include <dlfcn.h>
#include <libgen.h>
#include <regex.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdarg.h>
#include <time.h>

#include "iamforce.h"
#include "tkgl_mpcmapper.h"

// Global status / Rest status
// Initial status
IAMForceStatus_t IAMForceStatus = {
    .pad_layout = IAMFORCE_LAYOUT_PAD_BANK_A,
    .force_mode = MPC_FORCE_MODE_NONE,
    // .launch_mode_layout = IAMFORCE_LAYOUT_PAD_BANK_A,
    // .stepseq_mode_layout = IAMFORCE_LAYOUT_PAD_BANK_A,
    // .note_mode_layout = IAMFORCE_LAYOUT_PAD_BANK_C,
    .mode_buttons = MODE_BUTTONS_BOTTOM_LOCK,
    .tap_status = false,
    .last_button_down = 0,
    .started_button_down = {0, 0},
    .project_loaded = false,
};

// Default length of messages
uint_fast8_t SOURCE_MESSAGE_LENGTH[8] = {
    3, // source_button on
    3, // source_button off
    3, // source_led
    3, // source_pad_note_on
    3, // source_pad_note_off
    3, // source_pad_aftertouch
    7, // source_pad_sysex
    1  // source_unkown
};

// FORCE starts from top-left, MPC start from BOTTOM-left
// These matrix are MPC => Force (not the other way around)
// Also, the numbers are those of a NOTE NUMBER, not the pad sysex!
// Pad sysex will be deduced at runtime.
static MPCControlToForce_t MPCPadToForce[IAMFORCE_LAYOUT_N][16] = {
    // Pad bank A
    {
        [0 ... 15].note_number = 0xff,
        // 0x01st line
        [0x00].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 0,
        [0x01].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 1,
        [0x02].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 2,
        [0x03].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 3,
        // 0x02d line
        [0x04].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 8,
        [0x05].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 9,
        [0x06].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 10,
        [0x07].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 11,
        // 0x03rd line
        [0x08].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 16,
        [0x09].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 17,
        [0x0a].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 18,
        [0x0b].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 19,
        // 0x04th line
        [0x0c].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 24,
        [0x0d].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 25,
        [0x0e].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 26,
        [0x0f].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 27},
    // Pad bank B
    {
        [0 ... 15].note_number = 0xff,
        // 0x01st line
        [0x00].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 4,
        [0x01].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 5,
        [0x02].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 6,
        [0x03].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 7,
        // 0x02d line
        [0x04].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 12,
        [0x05].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 13,
        [0x06].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 14,
        [0x07].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 15,
        // 0x03rd line
        [0x08].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 20,
        [0x09].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 21,
        [0x0a].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 22,
        [0x0b].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 23,
        // 0x04th line
        [0x0c].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 28,
        [0x0d].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 29,
        [0x0e].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 30,
        [0x0f].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 31},
    // Pad bank C
    {
        [0 ... 15].note_number = 0xff,
        // First line (the top line)
        [0x00].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 32,
        [0x01].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 33,
        [0x02].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 34,
        [0x03].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 35,
        // 0x02d line
        [0x04].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 40,
        [0x05].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 41,
        [0x06].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 42,
        [0x07].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 43,
        // 0x03rd line
        [0x08].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 48,
        [0x09].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 49,
        [0x0a].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 50,
        [0x0b].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 51,
        // 0x04th line
        [0x0c].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 56,
        [0x0d].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 57,
        [0x0e].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 58,
        [0x0f].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 59},
    // Pad bank D
    {
        [0 ... 15].note_number = 0xff,
        // 1st line
        [0x00].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 36,
        [0x01].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 37,
        [0x02].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 38,
        [0x03].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 39,
        // 0x02d line
        [0x04].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 44,
        [0x05].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 45,
        [0x06].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 46,
        [0x07].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 47,
        // 0x03rd line
        [0x08].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 52,
        [0x09].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 53,
        [0x0a].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 54,
        [0x0b].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 55,
        // 0x04th line
        [0x0c].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 60,
        [0x0d].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 61,
        [0x0e].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 62,
        [0x0f].note_number = FORCE_PAD_FLAG + FORCEPADS_TABLE_IDX_OFFSET + 63},
    // PAD LAYOUT MODE, this is 100% custom here (because of the reverse relationship)
    // Note that we could somehow link the Force button numbers to their controls,
    // but not sure it's even needed.
    {
        [0 ... 15].note_number = 0xff,
        // 1st line
        [0x00].callback = cb_mode_e,
        [0x01].callback = cb_mode_e,
        [0x02].callback = cb_mode_e,
        [0x03].callback = cb_mode_e,
        // 0x2d line
        [0x04].callback = cb_mode_e,
        [0x05].callback = cb_mode_e,
        [0x06].callback = cb_mode_e,
        [0x07].callback = cb_mode_e,
        // 3rd line
        [0x08].callback = cb_mode_e,
        [0x09].callback = cb_mode_e,
        [0x0a].callback = cb_mode_e,
        [0x0b].callback = cb_mode_e,
        // 4th line
        [0x0c].callback = cb_mode_e,
        [0x0d].callback = cb_mode_e,
        [0x0e].callback = cb_mode_e,
        [0x0f].callback = cb_mode_e},
    // IAMFORCE_LAYOUT_PAD_MUTE
    {
        [0 ... 15].note_number = 0xff,
        [0x00].note_number = FORCE_BT_MUTE,
        [0x00].color = 0x2f1900, // ...the mute yellow!
        [0x01].note_number = FORCE_BT_SOLO,
        [0x01].color = 0x00007E,
        [0x02].note_number = FORCE_BT_REC_ARM,
        [0x02].color = 0x7E0000,
        [0x03].note_number = FORCE_BT_CLIP_STOP,
        [0x03].color = 0x007E00,
        [0x04].note_number = FORCE_BT_LEFT,
        [0x04].color = COLOR_GREY,
        [0x05].note_number = FORCE_BT_ASSIGN_A,
        [0x05].color = COLOR_ORANGE,
        [0x06].note_number = FORCE_BT_ASSIGN_B,
        [0x06].color = COLOR_RED,
        [0x07].note_number = FORCE_BT_RIGHT,
        [0x07].color = COLOR_GREY,
        [0x08].note_number = FORCE_BT_MUTE_PAD1,
        [0x09].note_number = FORCE_BT_MUTE_PAD2,
        [0x0a].note_number = FORCE_BT_MUTE_PAD3,
        [0x0b].note_number = FORCE_BT_MUTE_PAD4,
        [0x0c].note_number = FORCE_BT_MUTE_PAD5,
        [0x0d].note_number = FORCE_BT_MUTE_PAD6,
        [0x0e].note_number = FORCE_BT_MUTE_PAD7,
        [0x0f].note_number = FORCE_BT_MUTE_PAD8},
    // IAMFORCE_LAYOUT_PAD_COLS
    {
        [0 ... 15].note_number = 0xff,
        [0x00].note_number = FORCE_BT_UNSET,
        [0x01].note_number = FORCE_BT_UP,
        [0x01].color = COLOR_GREY,
        [0x02].note_number = FORCE_BT_UNSET,
        [0x03].note_number = FORCE_BT_UNSET,
        [0x04].note_number = FORCE_BT_LEFT,
        [0x04].color = COLOR_GREY,
        [0x05].note_number = FORCE_BT_DOWN,
        [0x05].color = COLOR_GREY,
        [0x06].note_number = FORCE_BT_UNSET,
        [0x07].note_number = FORCE_BT_RIGHT,
        [0x07].color = COLOR_GREY,
        [0x08].note_number = FORCE_BT_COLUMN_PAD1,
        [0x09].note_number = FORCE_BT_COLUMN_PAD2,
        [0x0a].note_number = FORCE_BT_COLUMN_PAD3,
        [0x0b].note_number = FORCE_BT_COLUMN_PAD4,
        [0x0c].note_number = FORCE_BT_COLUMN_PAD5,
        [0x0d].note_number = FORCE_BT_COLUMN_PAD6,
        [0x0e].note_number = FORCE_BT_COLUMN_PAD7,
        [0x0f].note_number = FORCE_BT_COLUMN_PAD8},
    // SCENE MODE
    {
        [0 ... 15].note_number = 0xff,
        // 1st line
        [0x00].note_number = FORCE_BT_STOP_ALL,
        [0x00].color = COLOR_FIRE,
        [0x01].note_number = FORCE_BT_UP,
        [0x01].color = COLOR_GREY,
        [0x02].note_number = FORCE_BT_LAUNCH_1,
        [0x02].color = 0x007F00,
        [0x03].note_number = FORCE_BT_LAUNCH_5,
        [0x03].color = 0x004700,
        // 2d line
        [0x04].note_number = FORCE_BT_UNSET,
        [0x05].note_number = FORCE_BT_DOWN,
        [0x05].color = COLOR_GREY,
        [0x06].note_number = FORCE_BT_LAUNCH_2,
        [0x06].color = 0x007F00,
        [0x07].note_number = FORCE_BT_LAUNCH_6,
        [0x07].color = 0x004700,
        // 3rd line
        [0x08].note_number = FORCE_BT_UNSET,
        [0x09].note_number = FORCE_BT_UNSET,
        [0x0a].note_number = FORCE_BT_LAUNCH_3,
        [0x0a].color = 0x007F00,
        [0x0b].note_number = FORCE_BT_LAUNCH_7,
        [0x0b].color = 0x004700,
        // Last line
        [0x0c].note_number = FORCE_BT_UNSET,
        [0x0d].note_number = FORCE_BT_UNSET,
        [0x0e].note_number = FORCE_BT_LAUNCH_4,
        [0x0e].color = 0x007F00,
        [0x0f].note_number = FORCE_BT_LAUNCH_8,
        [0x0f].color = 0x004700},

    // IAMFORCE_LAYOUT_PAD_XFDR
    {
        [0 ... 15].note_number = 0xff,
        [0x00].callback = cb_xfader,
        [0x01].callback = cb_xfader,
        [0x02].callback = cb_xfader,
        [0x03].callback = cb_xfader,
        [0x04].callback = cb_xfader,
        [0x05].callback = cb_xfader,
        [0x06].callback = cb_xfader,
        [0x07].callback = cb_xfader,
        [0x08].callback = cb_xfader,
        [0x09].callback = cb_xfader,
        [0x0a].callback = cb_xfader,
        [0x0b].callback = cb_xfader,
        [0x0c].callback = cb_xfader,
        [0x0d].callback = cb_xfader,
        [0x0e].callback = cb_xfader,
        [0x0f].callback = cb_xfader}};

// Straight buttons mapping.
static MPCControlToForce_t MPCButtonToForce[128] = {
    // Default value (C99 extension)
    [0 ... 127].note_number = FORCE_BT_UNSET,

    // Global stuff
    [LIVEII_BT_ENCODER].note_number = FORCE_BT_ENCODER,
    [LIVEII_BT_SHIFT].note_number = FORCE_BT_SHIFT,
    [LIVEII_BT_SHIFT].callback = cb_shift,
    [LIVEII_BT_TAP_TEMPO].note_number = FORCE_BT_TAP_TEMPO,
    [LIVEII_BT_TAP_TEMPO].callback = cb_tap_tempo,
    [LIVEII_BT_QLINK_SELECT].note_number = FORCE_BT_KNOBS,
    [LIVEII_BT_PLUS].note_number = FORCE_BT_PLUS,
    [LIVEII_BT_MINUS].note_number = FORCE_BT_MINUS,

    // Transport / mode buttons
    [LIVEII_BT_MENU].note_number = FORCE_BT_MENU,
    [LIVEII_BT_MAIN].note_number = FORCE_BT_MATRIX,
    [LIVEII_BT_MIX].note_number = FORCE_BT_MIXER,
    [LIVEII_BT_MUTE].note_number = FORCE_BT_CLIP,
    [LIVEII_BT_NEXT_SEQ].note_number = FORCE_BT_NAVIGATE,
    [LIVEII_BT_REC].note_number = FORCE_BT_REC,
    [LIVEII_BT_OVERDUB].note_number = FORCE_BT_ARP,
    [LIVEII_BT_STOP].note_number = FORCE_BT_STOP,
    [LIVEII_BT_PLAY].callback = cb_play,
    [LIVEII_BT_PLAY].note_number = FORCE_BT_PLAY,
    [LIVEII_BT_PLAY_START].callback = cb_play,
    [LIVEII_BT_PLAY_START].note_number = FORCE_BT_PLAY,

    // Edition buttons. These are double-function buttons so we use a callbacks here
    [LIVEII_BT_BANK_A].callback = cb_edit_button,
    [LIVEII_BT_BANK_B].callback = cb_edit_button,
    [LIVEII_BT_BANK_C].callback = cb_edit_button,
    [LIVEII_BT_BANK_D].callback = cb_edit_button,
    [LIVEII_BT_NOTE_REPEAT].callback = cb_edit_button,
    [LIVEII_BT_FULL_LEVEL].callback = cb_edit_button,
    [LIVEII_BT_FULL_LEVEL].note_number = FORCE_BT_NOTE,
    [LIVEII_BT_16_LEVEL].callback = cb_edit_button,
    [LIVEII_BT_16_LEVEL].note_number = FORCE_BT_STEP_SEQ,
    [LIVEII_BT_ERASE].callback = cb_edit_button,
    [LIVEII_BT_ERASE].note_number = FORCE_BT_LAUNCH,

    // Upper right zone
    [LIVEII_BT_UNDO].note_number = FORCE_BT_UNDO,
    [LIVEII_BT_TC].note_number = FORCE_BT_LOAD,
    [LIVEII_BT_COPY].note_number = FORCE_BT_SAVE,
    [LIVEII_BT_STEP_SEQ].note_number = FORCE_BT_MASTER};

// Ok, now we need a data structure to store the other way around.
// The thing is, ONE Force pad/button can be mapped to several MPC pads.
// (although the general case is 1 == 1).
// We could use malloc() and a linked list, but we find it more convenient
// to handle that in a static array. The memory footprint would be
// something like 128 * 8 bytes (roughly) = 1Kb which is not such a big deal.
// static ForceControlToMPC_t ForcePadToMPC[CONTROL_TABLE_SIZE];
// Here we also set some default values for the ForceControlToMPC_t structure
// because they are a divergence of our MPC -> Force mapping model.
// that is: ONE Force control could update DIFFERENT MPC controls.
static ForceControlToMPC_t ForceControlToMPC[CONTROL_TABLE_SIZE] = {
    [0 ... CONTROL_TABLE_SIZE - 1] = {
        .note_number = 0xff,
        .color = COLOR_BLACK,
        .callback = NULL,
        .next_control = NULL},
    // // XXX Should I update colors as well?
    // [FORCE_BT_NOTE].callback = cb_edit_button,
    // [FORCE_BT_NOTE].note_number = LIVEII_BT_FULL_LEVEL,
    // [FORCE_BT_STEP_SEQ].callback = cb_edit_button,
    // [FORCE_BT_STEP_SEQ].note_number = LIVEII_BT_16_LEVEL,
    // [FORCE_BT_LAUNCH].callback = cb_edit_button,
    // [FORCE_BT_LAUNCH].note_number = LIVEII_BT_ERASE,
};
// [FORCE_BT_MUTE].callback = cb_edit_button,
// [FORCE_BT_SOLO].callback
// [FORCE_BT_REC_ARM].callback = cb_edit_button,
// [FORCE_BT_CLIP_STOP].callback = cb_edit_button};

static uint8_t ForceControlToMPCExtraNext = 0x80;                             // Next available index
static uint8_t ForceControlToMPCExtraMax = 0x80 + FORCEPADS_TABLE_IDX_OFFSET; // Max index

// These are the matrices where actual RGB values are stored
// They are initialized at start time and populated in the Write function
// And they are given with their index
// Use IAMFORCE_LAYOUT_PAD_* variables to address a specific array
PadColor_t MPCPadValues[IAMFORCE_LAYOUT_N][16];

void invertMPCToForceMapping()
{
    // Various variables helping us
    size_t MPCPadToForce_n = sizeof(MPCPadToForce) / sizeof(MPCPadToForce[0]);
    size_t MPCButtonToForce_n = sizeof(MPCButtonToForce) / sizeof(MPCButtonToForce[0]);
    ForceControlToMPC_t _empty = {
        .note_number = 0xff,
        .bank = 0xff,
        .color = COLOR_WHITE,
        .callback = NULL,
        .next_control = NULL};
    uint8_t mpc_note_number = 0xFF;

    // Initialize global mapping tables to 0 (just in case)
    for (int i = 0; i < CONTROL_TABLE_SIZE; i++)
    {
        ForceControlToMPC[i] = _empty;
    }

    // Iterate over all MPCPadToForce_a arrays
    for (uint8_t i = 0; i < MPCPadToForce_n; i++)
    {
        // Iterate over all elements of the mapping
        for (uint8_t j = 0; j < 16; j++)
        {
            // Get the current element and tweak values if necessary
            MPCControlToForce_t *mapping_p = &MPCPadToForce[i][j];
            if (mapping_p->note_number == 0xFF)
                continue;
            if (mapping_p->callback == NULL)
                mapping_p->callback = cb_default;
            mpc_note_number = getMPCPadNoteNumber(j);

            // Handle multiple controls mapping
            // We use a 16 bits integer here because MPCPadToForce can have
            // more than 256 elements (with the extra elements)
            ForceControlToMPC_t *reverse_mapping_p = &ForceControlToMPC[mapping_p->note_number];
            while (reverse_mapping_p->next_control != NULL)
            {
                reverse_mapping_p = &ForceControlToMPC[ForceControlToMPCExtraNext];
                if (ForceControlToMPCExtraNext < ForceControlToMPCExtraMax)
                    ForceControlToMPCExtraNext++;
                else
                    LOG_ERROR("Can't increment ForceControlToMPCExtraNext, you have too many buttons with multiple combinations");
            }

            // Save mapping
            if (reverse_mapping_p->callback == NULL)
            {
                // XXX TODO: don't override default (already set) values
                reverse_mapping_p->note_number = mpc_note_number;
                reverse_mapping_p->bank = i;
                reverse_mapping_p->color = mapping_p->color;
                reverse_mapping_p->callback = mapping_p->callback;
                reverse_mapping_p->next_control = NULL;
            }
            else
                LOG_DEBUG("Don't override existing mapping for %02x.%02x", i, j);
        }
    }

    // Now we do the same with MPCButtonToForce!
    for (mpc_note_number = 0; mpc_note_number < MPCButtonToForce_n; mpc_note_number++)
    {
        // Get the current element and tweak values if necessary
        MPCControlToForce_t *mapping_p = &MPCButtonToForce[mpc_note_number];
        if (mapping_p->note_number == 0xFF && mapping_p->callback == NULL)
            continue;
        if (mapping_p->callback == NULL)
            mapping_p->callback = cb_default;

        // Handle multiple controls mapping
        // We use a 16 bits integer here because MPCPadToForce can have
        // more than 256 elements (with the extra elements)
        ForceControlToMPC_t *reverse_mapping_p = &ForceControlToMPC[mapping_p->note_number];
        while (reverse_mapping_p->next_control != NULL)
        {
            reverse_mapping_p = &ForceControlToMPC[ForceControlToMPCExtraNext];
            if (ForceControlToMPCExtraNext < ForceControlToMPCExtraMax)
                ForceControlToMPCExtraNext++;
            else
                LOG_ERROR("Can't increment ForceControlToMPCExtraNext, you have too many buttons with multiple combinations");
        }

        // Save mapping
        if (reverse_mapping_p->callback == NULL)
        {
            LOG_DEBUG("   => Mapping Force note %02X to callback %p", mapping_p->note_number, mapping_p->callback);
            // XXX TODO: don't override default (already set) values
            reverse_mapping_p->note_number = mpc_note_number;
            reverse_mapping_p->bank = IAMFORCE_LAYOUT_NONE;
            reverse_mapping_p->callback = mapping_p->callback;
            reverse_mapping_p->next_control = NULL;
        }
        else
            LOG_DEBUG("Don't override existing mapping for Force note number %02x", mapping_p->note_number);
    }
}

/**************************************************************************
 *                                                                        *
 *  MPC Pads management                                                   *
 *                                                                        *
 **************************************************************************/

void _setLayoutPadMode()
{
    // Special case with IAMFORCE_LAYOUT_PAD_MODE: we redraw all the pads manually
    // XXX TODO: move this at init time
    MPCPadValues[IAMFORCE_LAYOUT_PAD_MODE][0] = COLOR_ORANGE;
    MPCPadValues[IAMFORCE_LAYOUT_PAD_MODE][1] = 0x2F1900;
    MPCPadValues[IAMFORCE_LAYOUT_PAD_MODE][2] = 0x191919;
    MPCPadValues[IAMFORCE_LAYOUT_PAD_MODE][3] = 0x191919;
    MPCPadValues[IAMFORCE_LAYOUT_PAD_MODE][4] = 0x191919;
    MPCPadValues[IAMFORCE_LAYOUT_PAD_MODE][5] = COLOR_CLOVER;
    MPCPadValues[IAMFORCE_LAYOUT_PAD_MODE][6] = 0x330233;
    MPCPadValues[IAMFORCE_LAYOUT_PAD_MODE][7] = 0x191919;
    MPCPadValues[IAMFORCE_LAYOUT_PAD_MODE][8] = 0x7F1515;
    MPCPadValues[IAMFORCE_LAYOUT_PAD_MODE][9] = 0x7F1515;
    MPCPadValues[IAMFORCE_LAYOUT_PAD_MODE][10] = COLOR_GREEN;
    MPCPadValues[IAMFORCE_LAYOUT_PAD_MODE][11] = 0x000600;
    MPCPadValues[IAMFORCE_LAYOUT_PAD_MODE][12] = 0x504315;
    MPCPadValues[IAMFORCE_LAYOUT_PAD_MODE][13] = 0x504315;
    MPCPadValues[IAMFORCE_LAYOUT_PAD_MODE][14] = COLOR_GREEN;
    MPCPadValues[IAMFORCE_LAYOUT_PAD_MODE][15] = 0x000600;

    switch (IAMForceStatus.permanent_pad_layout)
    {
    case IAMFORCE_LAYOUT_PAD_XFDR:
        MPCPadValues[IAMFORCE_LAYOUT_PAD_MODE][0] = 0x7F7F7F;
        break;
    case IAMFORCE_LAYOUT_PAD_MUTE:
        MPCPadValues[IAMFORCE_LAYOUT_PAD_MODE][1] = 0x7F7F7F;
        break;
    case IAMFORCE_LAYOUT_PAD_COLS:
        MPCPadValues[IAMFORCE_LAYOUT_PAD_MODE][4] = 0x7F7F7F;
        break;
    case IAMFORCE_LAYOUT_PAD_SCENE:
        MPCPadValues[IAMFORCE_LAYOUT_PAD_MODE][5] = 0x7F7F7F;
        break;
    case IAMFORCE_LAYOUT_PAD_BANK_A:
        switch (IAMForceStatus.force_mode)
        {
        case MPC_FORCE_MODE_NOTE:
            MPCPadValues[IAMFORCE_LAYOUT_PAD_MODE][2] = 0x7F7F7F;
            break;
        case MPC_FORCE_MODE_STEPSEQ:
            MPCPadValues[IAMFORCE_LAYOUT_PAD_MODE][8] = 0x7F7F7F;
            break;
        case MPC_FORCE_MODE_LAUNCH:
            MPCPadValues[IAMFORCE_LAYOUT_PAD_MODE][0x0a] = 0x7F7F7F;
            break;
        }
        break;
    case IAMFORCE_LAYOUT_PAD_BANK_B:
        switch (IAMForceStatus.force_mode)
        {
        case MPC_FORCE_MODE_NOTE:
            MPCPadValues[IAMFORCE_LAYOUT_PAD_MODE][3] = 0x7F7F7F;
            break;
        case MPC_FORCE_MODE_STEPSEQ:
            MPCPadValues[IAMFORCE_LAYOUT_PAD_MODE][9] = 0x7F7F7F;
            break;
        case MPC_FORCE_MODE_LAUNCH:
            MPCPadValues[IAMFORCE_LAYOUT_PAD_MODE][0x0b] = 0x7F7F7F;
            break;
        }
        break;
    case IAMFORCE_LAYOUT_PAD_BANK_C:
        switch (IAMForceStatus.force_mode)
        {
        case MPC_FORCE_MODE_NOTE:
            MPCPadValues[IAMFORCE_LAYOUT_PAD_MODE][6] = 0x7F7F7F;
            break;
        case MPC_FORCE_MODE_STEPSEQ:
            MPCPadValues[IAMFORCE_LAYOUT_PAD_MODE][0x0c] = 0x7F7F7F;
            break;
        case MPC_FORCE_MODE_LAUNCH:
            MPCPadValues[IAMFORCE_LAYOUT_PAD_MODE][0x0d] = 0x7F7F7F;
            break;
        }
        break;
    case IAMFORCE_LAYOUT_PAD_BANK_D:
        switch (IAMForceStatus.force_mode)
        {
        case MPC_FORCE_MODE_NOTE:
            MPCPadValues[IAMFORCE_LAYOUT_PAD_MODE][7] = 0x7F7F7F;
            break;
        case MPC_FORCE_MODE_STEPSEQ:
            MPCPadValues[IAMFORCE_LAYOUT_PAD_MODE][0x0d] = 0x7F7F7F;
            break;
        case MPC_FORCE_MODE_LAUNCH:
            MPCPadValues[IAMFORCE_LAYOUT_PAD_MODE][0x0e] = 0x7F7F7F;
            break;
        }
        break;
    }
}

// Set layout, give status according to 'permanent' parameter
// If false, we switch layout temporarily
inline void setLayout(uint8_t pad_layout, bool permanent)
{
    LOG_DEBUG("setLayout(%02x, %d)", pad_layout, permanent);
    LOG_DEBUG("    [before] current layout: %02x", IAMForceStatus.pad_layout);
    LOG_DEBUG("    [before] permanent layout: %02x", IAMForceStatus.permanent_pad_layout);
    LOG_DEBUG("    [before] Force native mode: %02x", IAMForceStatus.force_mode);
    LOG_DEBUG("    [before] mode buttons: %02x", IAMForceStatus.mode_buttons);

    // // Check if we are already in the right layout
    // XXX Carefuly re-implement this, taking 'permanent' state into account
    // if (IAMForceStatus.project_loaded && IAMForceStatus.pad_layout == pad_layout)
    //     return;

    // Set the new layout and button modes
    IAMForceStatus.pad_layout = pad_layout;
    if (permanent)
        IAMForceStatus.permanent_pad_layout = pad_layout;

    if (pad_layout == IAMFORCE_LAYOUT_PAD_MODE)
    {
        _setLayoutPadMode();
    }

    // Unlock if necessary (ie when we access bank A-D in lock mode)
    if (pad_layout <= IAMFORCE_LAYOUT_PAD_BANK_D)
        IAMForceStatus.mode_buttons &= ~MODE_BUTTONS_TOP_LOCK;

    // Set pads and button colors
    showLayout(pad_layout);
    setModeButtons();

    return;
}

// Show layout but without switching at all
inline void showLayout(uint8_t pad_layout)
{
    // Redraw the pads
    for (uint8_t i = 0; i < 16; i++)
    {
        setPadColorFromColorInt(i, MPCPadValues[pad_layout][i]);
    }
}

// Set proper mode button colors according to the current layout
inline void setModeButtons()
{
    // Save button states
    uint8_t button_colors[8];

    // Set button colors
    if (IAMForceStatus.mode_buttons & MODE_BUTTONS_TOP_MODE)
    {
        button_colors[0] = BUTTON_COLOR_LIGHT_YELLOW;
        button_colors[1] = BUTTON_COLOR_LIGHT_YELLOW;
        button_colors[2] = BUTTON_COLOR_LIGHT_YELLOW;
        button_colors[3] = BUTTON_COLOR_LIGHT_YELLOW;
    }
    else
    {
        button_colors[0] = BUTTON_COLOR_LIGHT_RED;
        button_colors[1] = BUTTON_COLOR_LIGHT_RED;
        button_colors[2] = BUTTON_COLOR_LIGHT_RED;
        button_colors[3] = BUTTON_COLOR_LIGHT_RED;
    }
    if (IAMForceStatus.mode_buttons & MODE_BUTTONS_BOTTOM_MODE)
    {
        button_colors[4] = BUTTON_COLOR_LIGHT_YELLOW; // Don't ask why...
        button_colors[5] = BUTTON_COLOR_LIGHT_YELLOW;
        button_colors[6] = BUTTON_COLOR_LIGHT_YELLOW;
        button_colors[7] = BUTTON_COLOR_LIGHT_YELLOW;
    }
    else
    {
        button_colors[4] = BUTTON_COLOR_OFF;
        button_colors[5] = BUTTON_COLOR_OFF;
        button_colors[6] = BUTTON_COLOR_OFF;
        button_colors[7] = BUTTON_COLOR_OFF;
    }

    // Set specific pad colors for the TOP row
    switch (IAMForceStatus.pad_layout)
    {
    case IAMFORCE_LAYOUT_PAD_BANK_A:
        button_colors[0] = BUTTON_COLOR_RED;
        break;
    case IAMFORCE_LAYOUT_PAD_BANK_B:
        button_colors[1] = BUTTON_COLOR_RED;
        break;
    case IAMFORCE_LAYOUT_PAD_BANK_C:
        button_colors[2] = BUTTON_COLOR_RED;
        break;
    case IAMFORCE_LAYOUT_PAD_BANK_D:
        button_colors[3] = BUTTON_COLOR_RED;
        break;
    case IAMFORCE_LAYOUT_PAD_MODE:
        button_colors[0] = BUTTON_COLOR_YELLOW;
        break;
    case IAMFORCE_LAYOUT_PAD_MUTE:
        button_colors[1] = BUTTON_COLOR_YELLOW;
        break;
    case IAMFORCE_LAYOUT_PAD_COLS:
        button_colors[2] = BUTTON_COLOR_YELLOW;
        break;
    case IAMFORCE_LAYOUT_PAD_SCENE:
        button_colors[3] = BUTTON_COLOR_YELLOW;
        break;
    }

    // Same for the bottom row (it's a little bit more tricky here)
    if (IAMForceStatus.force_mode == MPC_FORCE_MODE_NOTE)
        button_colors[5] = BUTTON_COLOR_YELLOW;
    if (IAMForceStatus.force_mode == MPC_FORCE_MODE_STEPSEQ)
        button_colors[6] = BUTTON_COLOR_YELLOW;
    if (IAMForceStatus.force_mode == MPC_FORCE_MODE_LAUNCH)
        button_colors[7] = BUTTON_COLOR_YELLOW;

    // Commit colors
    // XXX TODO avoid changing all button colors everytime
    setButtonColor(LIVEII_BT_BANK_A, button_colors[0]);
    setButtonColor(LIVEII_BT_BANK_B, button_colors[1]);
    setButtonColor(LIVEII_BT_BANK_C, button_colors[2]);
    setButtonColor(LIVEII_BT_BANK_D, button_colors[3]);
    setButtonColor(LIVEII_BT_NOTE_REPEAT, button_colors[4]);
    setButtonColor(LIVEII_BT_FULL_LEVEL, button_colors[5]);
    setButtonColor(LIVEII_BT_16_LEVEL, button_colors[6]);
    setButtonColor(LIVEII_BT_ERASE, button_colors[7]);

    return;
}

inline void setButtonColor(uint8_t button, uint8_t color)
{
    // Tweak the color because if shitty, annoying, buggy, stupid, useless, crap colors
    switch (button)
    {
    case LIVEII_BT_NOTE_REPEAT:
    case LIVEII_BT_16_LEVEL:
    case LIVEII_BT_ERASE:
        if (color == BUTTON_COLOR_LIGHT_YELLOW)
            color = BUTTON_COLOR_LIGHT_RED;
    }

    // Send the message
    uint8_t button_light[] = {0xB0, button, color};
    tklog_trace("additional dump snd_rawmidi_write\n");
    ShowBufferHexDump(button_light, sizeof(button_light), 0x00);
    orig_snd_rawmidi_write(rawvirt_outpriv, button_light, sizeof(button_light));
    return;
}

// If instant_set is True, we will redraw the pad immediately (if we are in the same bank)
// Return the number of the pad (useable in a SYSEX message) or 0xff

inline int_fast8_t setLayoutPad(uint8_t pad_layout, uint8_t note_number, PadColor_t rgb, bool instant_set)
{
    uint8_t pad_number = 0xff;

    // If note_number is <16 we can be sure it's a pad number, not a note number!
    if (note_number < 16)
        pad_number = note_number;
    else
        pad_number = getMPCPadNumber(note_number);

    // Update the matrix
    LOG_DEBUG("      update matrix for pad %02X.%02X to %08x (current layout=%02X)", pad_layout, pad_number, rgb, IAMForceStatus.pad_layout);
    MPCPadValues[pad_layout][pad_number] = rgb;

    // Do we *have* to redraw it?
    if (IAMForceStatus.pad_layout == pad_layout)
    {
        if (instant_set)
        {
            // We are in the same bank, redraw the pad
            setPadColorFromColorInt(pad_number, rgb);
            return 0xff;
        }
        else
        {
            // LOG_DEBUG("     ...we should draw it but we are not allowed to do so! Return %02X", pad_number);
            return pad_number;
        }
    }

    // We don't have to redraw it
    return 0xff;
}

/**************************************************************************
 *                                                                        *
 *  Core functions                                                        *
 *                                                                        *
 **************************************************************************/

///////////////////////////////////////////////////////////////////////////////
// (fake) load mapping tables from config file
///////////////////////////////////////////////////////////////////////////////
void LoadMapping()
{
    // Say we'll map stuff
    LOG_DEBUG("Load mapping...");

    // Initialize global mapping tables
    invertMPCToForceMapping();

    // Dump MPC->Force mapping (MPCControlToForce)
    for (int i = 0; i < IAMFORCE_LAYOUT_N; i++)
    {
        for (int j = 0; j < 16; j++)
        {
            LOG_DEBUG("MPCPadToForce[%02X.%02X] = %02X // %p",
                      i,
                      j,
                      MPCPadToForce[i][j].note_number,
                      MPCPadToForce[i][j].callback);
        }
    }
    for (int i = 0; i < (sizeof(MPCButtonToForce) / sizeof(MPCButtonToForce[0])); i++)
    {

        LOG_DEBUG("MPCButtonToForce[%02X] = %02X // %p",
                  i,
                  MPCButtonToForce[i].note_number,
                  MPCButtonToForce[i].callback);
    }

    // Dump Force->MPC mapping (ForceControlToMPC)
    for (int i = 0; i < CONTROL_TABLE_SIZE; i++)
    {
        LOG_DEBUG("ForceControlToMPC[%02X] = %02X.%02X // %p %p",
                  i,
                  ForceControlToMPC[i].bank,
                  ForceControlToMPC[i].note_number,
                  ForceControlToMPC[i].callback,
                  ForceControlToMPC[i].next_control);
    }

    // Initialize all pads caches (at load-time, we consider they are black)
    for (int i = 0; i < IAMFORCE_LAYOUT_N; i++)
    {
        for (int j = 0; j < 16; j++)
            MPCPadValues[i][j] = MPCPadToForce[i][j].color;
    }

    // Initialize additional MPC status data
    clock_gettime(CLOCK_MONOTONIC_RAW, &IAMForceStatus.started_button_down);
}

// ///////////////////////////////////////////////////////////////////////////////
// // Prepare a fake midi message in the Private midi context
// ///////////////////////////////////////////////////////////////////////////////
// // XXX COMPLETELY REMOVE THIS FUNCTION TO DISCARD MESSAGE FROM THE OUTPUT
// size_t FakeMidiMessage(uint8_t buf[], size_t size)
// {
//     // LOG_DEBUG("FakeMidiMessage(buf=%p, size=%d)
//     // Just put all the bytes to 0
//     if (size > 3)
//     {
//         LOG_ERROR("FakeMidiMessage : size is too big (%d)", size);
//         exit(-1);
//     }
//     memset(buf, 0x00, size);
//     return size;
// }

// Set pad colors
// 2 implementations : call with a 32 bits color int value or with r,g,b values
// Pad number starts from top left (0), 8 pads per line
inline void setPadColor(const uint8_t pad_number, const uint8_t r, const uint8_t g, const uint8_t b)
{
    uint8_t sysexBuff[12];
    int_fast8_t mpc_pad_number;
    char sysexBuffDebug[128];
    int p = 0;

    // Log event
    // LOG_DEBUG("               Set pad color: %02X r g b %02X %02X %02X", pad_number, r, g, b);

    // Double-check input data
    if (pad_number > 16)
    {
        LOG_ERROR("MPC Pad Line refresh : wrong pad number %d", pad_number);
        return;
    }

    // F0 47 7F [3B] 65 00 04 [Pad #] [R] [G] [B] F7
    memcpy(sysexBuff, AkaiSysex, sizeof(AkaiSysex));
    p += sizeof(AkaiSysex);

    // Add the current product id
    sysexBuff[p++] = DeviceInfoBloc[MPCOriginalId].sysexId;

    // Add the pad color fn and pad number and color
    memcpy(&sysexBuff[p], MPCSysexPadColorFn, sizeof(MPCSysexPadColorFn));
    p += sizeof(MPCSysexPadColorFn);

    // Number starts from lower left.
    // However we expect that they start from UPPER left, so we convert it.
    // XXX I should find a less CPU-intensive way to do this (or should I?)
    if (pad_number < 4)
        mpc_pad_number = pad_number + 12;
    else if (pad_number < 8)
        mpc_pad_number = pad_number + 4;
    else if (pad_number < 12)
        mpc_pad_number = pad_number - 4;
    else
        mpc_pad_number = pad_number - 12;
    sysexBuff[p++] = mpc_pad_number;

    // Issue the color message
    sysexBuff[p++] = r;
    sysexBuff[p++] = g;
    sysexBuff[p++] = b;
    sysexBuff[p++] = 0xF7;

    // Use tklog to debug the whole sysexBuff as a list of hexadecimal values.
    // We first create a string with the whole sysexBuff and then we print it.
    // This is a bit more efficient than printing each byte separately.
    // We store this in sysexBuffDebug string
    sysexBuffDebug[0] = '\0';
    for (int i = 0; i < p; i++)
    {
        sprintf(sysexBuffDebug + strlen(sysexBuffDebug), "%02X ", sysexBuff[i]);
    }
    // sprintf(sysexBuffDebug + strlen(sysexBuffDebug), "");
    // LOG_DEBUG("%s", sysexBuffDebug);

    // Send the sysex to the MPC controller
    // XXX disabled to test
    tklog_trace("additional dump snd_rawmidi_write\n");
    ShowBufferHexDump(sysexBuff, p, 0x00);
    orig_snd_rawmidi_write(rawvirt_outpriv, sysexBuff, p);
}

inline void setPadColorFromColorInt(const uint8_t pad_number, const PadColor_t rgbColorValue)
{
    // Colors R G B max value is 7f in SYSEX. So the bit 8 is always set to 0.
    uint8_t r = (rgbColorValue >> 16) & 0x7F;
    uint8_t g = (rgbColorValue >> 8) & 0x7F;
    uint8_t b = rgbColorValue & 0x7F;
    setPadColor(pad_number, r, g, b);
}

// Given a MIDI note number, we convert it to a PAD number,
// from 0 (top left) to 15 (bottom right)
uint8_t getMPCPadNumber(uint8_t note_number)
{
    switch (note_number & 0x7F)
    {
    case LIVEII_PAD_TL0:
        return 0;
    case LIVEII_PAD_TL1:
        return 1;
    case LIVEII_PAD_TL2:
        return 2;
    case LIVEII_PAD_TL3:
        return 3;
    case LIVEII_PAD_TL4:
        return 4;
    case LIVEII_PAD_TL5:
        return 5;
    case LIVEII_PAD_TL6:
        return 6;
    case LIVEII_PAD_TL7:
        return 7;
    case LIVEII_PAD_TL8:
        return 8;
    case LIVEII_PAD_TL9:
        return 9;
    case LIVEII_PAD_TL10:
        return 10;
    case LIVEII_PAD_TL11:
        return 11;
    case LIVEII_PAD_TL12:
        return 12;
    case LIVEII_PAD_TL13:
        return 13;
    case LIVEII_PAD_TL14:
        return 14;
    case LIVEII_PAD_TL15:
        return 15;
    default:
        LOG_ERROR("Note number %02x can't be converted to pad number", note_number);
        return 0;
    }
}

// We convert a pad number (from the sysex) to a note number suitable for our mapping table
uint8_t getForcePadNoteNumber(uint8_t pad_number, bool add_extra_bit)
{
    if (pad_number < 0x40) // 0x40 = 64, the core 'note' rows
        return pad_number + FORCEPADS_TABLE_IDX_OFFSET + (add_extra_bit ? 0x80 : 0);
    else if (pad_number < 0x48)
        return FORCE_BT_MUTE_PAD1 + (pad_number - 0x40);
    else if (pad_number < 0x50)
        return FORCE_BT_COLUMN_PAD1 + (pad_number - 0x48);
    else
        LOG_ERROR("Pad number %02x can't be converted to note number", pad_number);
    exit(-1);
}

// This is the reverse of getMPCPadNumber
inline uint8_t getMPCPadNoteNumber(uint8_t pad_number)
{
    switch (pad_number)
    {
    case 0:
        return LIVEII_PAD_TL0;
    case 1:
        return LIVEII_PAD_TL1;
    case 2:
        return LIVEII_PAD_TL2;
    case 3:
        return LIVEII_PAD_TL3;
    case 4:
        return LIVEII_PAD_TL4;
    case 5:
        return LIVEII_PAD_TL5;
    case 6:
        return LIVEII_PAD_TL6;
    case 7:
        return LIVEII_PAD_TL7;
    case 8:
        return LIVEII_PAD_TL8;
    case 9:
        return LIVEII_PAD_TL9;
    case 10:
        return LIVEII_PAD_TL10;
    case 11:
        return LIVEII_PAD_TL11;
    case 12:
        return LIVEII_PAD_TL12;
    case 13:
        return LIVEII_PAD_TL13;
    case 14:
        return LIVEII_PAD_TL14;
    case 15:
        return LIVEII_PAD_TL15;
    default:
        LOG_ERROR("Invalid pad number: %02X", pad_number);
        return 0xFF;
    }
}

// Project startup!!!
// We allow this function to be pretty slow.
void initProject()
{
    // We pass through all the modes
    // X-fader: we simulate a center fader move
    static const ForceControlToMPC_t fader_move = {
        .callback = cb_xfader,
        .next_control = NULL};
    uint8_t fake_buffer[] = {0x00, 0x40, 0x00};
    cb_xfader(NULL, &fader_move, source_cc_change, 0, fake_buffer, 3);

    // Display BANK A at start
    setLayout(IAMFORCE_LAYOUT_PAD_BANK_A, true);
}

// NOTA: this is not thread safe!
void StoreButtonPress(uint8_t mpc_button_number, bool key_down)
{
    struct timespec now;
    uint64_t down_duration;

    clock_gettime(CLOCK_MONOTONIC_RAW, &now);
    IAMForceStatus.is_click = false;
    IAMForceStatus.is_double_click = false;
    if (IAMForceStatus.last_button_down == mpc_button_number)
    {
        down_duration = (now.tv_sec - IAMForceStatus.started_button_down.tv_sec) * 1000 + (now.tv_nsec - IAMForceStatus.started_button_down.tv_nsec) / 1000000;
        if (key_down)
        {
            if (down_duration < DOUBLE_CLICK_DELAY)
            {
                IAMForceStatus.is_double_click = true;
                IAMForceStatus.started_button_down.tv_sec = 0; // Avoid mixing double clicks and click at release
            }
        }
        else
        {
            if (down_duration < HOLD_DELAY)
            {
                // LOG_DEBUG("It's a click!!!");
                IAMForceStatus.is_click = true;
            }
        }
    }

    IAMForceStatus.last_button_down = mpc_button_number;
    clock_gettime(CLOCK_MONOTONIC_RAW, &IAMForceStatus.started_button_down);
}

// With the midi_buffer, given the 'normal_advance' that should occur, either
// move the pointer forward and return its position, or clean the buffer up to
// 'normal_advance' but return its position after cleaning.
size_t cleanBufferAndAdvance(uint8_t *midi_buffer, size_t buffer_size, size_t normal_advance, size_t callback_advance)
{
    // Normal case: callback_advance == normal_advance
    if (callback_advance == normal_advance)
        return normal_advance <= buffer_size ? normal_advance : buffer_size;

    // Now we have to clean the buffer:
    // We copy the remaining bytes to the beginning of the buffer, and return the
    // new size of the buffer
    if (callback_advance > buffer_size)
        callback_advance = buffer_size;
    if (normal_advance > buffer_size)
        normal_advance = buffer_size;
    if (callback_advance > normal_advance)
    {
        LOG_ERROR("  ERROR: callback_advance %d > normal_advance %d", callback_advance, normal_advance);
        exit(-1);
    }
    // LOG_DEBUG("  (...erase MIDI message...)");
    size_t new_size = buffer_size - (normal_advance - callback_advance);
    // tklog_trace("    ....buffer before cleaning\n");
    // ShowBufferHexDump(midi_buffer, buffer_size, 0x00);
    memmove(&midi_buffer[0], &midi_buffer[normal_advance - callback_advance], new_size);
    // tklog_trace("    ....buffer AFTER cleaning\n");
    // ShowBufferHexDump(midi_buffer, new_size, 0x00);
    return callback_advance;
}

// MIDI READ - APP ON MPC READING AS FORCE
// Here we read the MIDI messages from the MPC and we send them to the Force
// That will be mostly button and pad presses!
// It's pretty simple:
// - We remap what's coming from the PADs according to the mode we're on
// - We discard presses from "bank" buttons
size_t Mpc_MapReadFromForce(void *midiBuffer, size_t maxSize, size_t size)
{
    SourceType_t source_type = source_unkown;
    uint8_t *midi_buffer = (uint8_t *)midiBuffer;
    size_t i = 0;
    size_t callback_advance = 0;
    size_t normal_advance = 0;
    uint8_t note_number = 0xFF;
    uint8_t pad_number = 0xFF;
    MPCControlToForce_t *mpc_to_force_mapping_p;
    // LOG_DEBUG("We have a %d/%d bytes buffer", size, maxSize);

    while (i < size)
    {
        switch (midi_buffer[i])
        {
        // AKAI SYSEX ------------------------------------------------------------
        // IDENTITY REQUEST
        case 0xF0:
            // Spoof identity
            if (memcmp(&midi_buffer[i], IdentityReplySysexHeader, sizeof(IdentityReplySysexHeader)) == 0)
            {
                // If so, substitue sysex identity request by the faked one
                // LOG_DEBUG("Identity request");
                memcpy(&midi_buffer[i + sizeof(IdentityReplySysexHeader)], DeviceInfoBloc[MPCId].sysexIdReply, sizeof(DeviceInfoBloc[MPCId].sysexIdReply));
                i += sizeof(IdentityReplySysexHeader) + sizeof(DeviceInfoBloc[MPCId].sysexIdReply);
            }

            // Consume the rest of the message until 0xF7 is found (end of sysex)
            while (i < size && midi_buffer[i] != 0xF7)
                i++;
            if (i < size) // check if 0xF7 was found before reaching the end of the buffer
                i++;      // increment i one more time to move to the octet after 0xF7
            break;

        // KNOBS TURN (UNMAPPED BECAUSE ARE ALL EQUIVALENT ON ALL DEVICES) ------
        case 0xB0:
            // If it's a shift + knob turn, add an offset
            //  B0 [10-31] [7F - n]
            if (IAMForceStatus.shift_hold && DeviceInfoBloc[MPCOriginalId].qlinkKnobsCount < 16 && midi_buffer[i + 1] >= 0x10 && midi_buffer[i + 1] <= 0x31)
                midi_buffer[i + 1] += DeviceInfoBloc[MPCOriginalId].qlinkKnobsCount;
            i += 3;
            break;

        // BUTTONS -----------------------------------------------------------------
        case 0x90:
            // DEBUG MODE: DUMP THE WHOLE MAPPING when pressing the "+" button
            // if (midi_buffer[i + 1] == LIVEII_BT_PLUS && midi_buffer[i + 2] == 0x7F)
            // {
            //     LOG_DEBUG("Dumping mapping");
            //     for (int i = 0; i < IAMFORCE_LAYOUT_N; i++)
            //     {
            //         char mapping_string[256];

            //         // Put in mapping_string
            //         mpc_to_force_mapping_p = &MPCButtonToForce[i];
            //         LOG_DEBUG("Button %02x => %p (CB=%p)", i, mpc_to_force_mapping_p, mpc_to_force_mapping_p->callback);
            //     }
            // }

            // Apply mapping, call the callback function
            source_type = midi_buffer[i + 2] == 0x7f ? source_button_on : source_button_off;
            note_number = midi_buffer[i + 1];
            normal_advance = 3;
            StoreButtonPress(note_number, midi_buffer[i + 2] == 0x7f ? true : false);
            mpc_to_force_mapping_p = &MPCButtonToForce[note_number];
            // LOG_DEBUG("Button %02x %02x => %p (CB=%p)", note_number, midi_buffer[i + 2], mpc_to_force_mapping_p, mpc_to_force_mapping_p->callback);
            if (mpc_to_force_mapping_p->callback != NULL)
                callback_advance = mpc_to_force_mapping_p->callback(
                    mpc_to_force_mapping_p,
                    NULL,
                    source_type,
                    note_number,
                    &midi_buffer[i],
                    size - i);
            else
                callback_advance = 0;

            // Clean buffer
            // LOG_DEBUG("Before cleaning: i=%d, size=%d, callback says %d", i, size, callback_advance);
            normal_advance = 3;
            i += cleanBufferAndAdvance(&midi_buffer[i], size - i, normal_advance, callback_advance);
            size = size - (normal_advance - callback_advance);
            // LOG_DEBUG("After cleaning: i=%d, size=%d", i, size);
            break;

        // PADS -------------------------------------------------------------------
        case 0x99:
        case 0x89:
        case 0xA9:
            note_number = midi_buffer[i + 1];
            pad_number = getMPCPadNumber(note_number);
            StoreButtonPress(note_number, midi_buffer[i] == 0x99 ? true : false);
            if (midi_buffer[i] == 0x99)
                source_type = source_pad_note_on;
            else if (midi_buffer[i] == 0x89)
                source_type = source_pad_note_off;
            else if (midi_buffer[i] == 0xA9)
                source_type = source_pad_aftertouch;
            mpc_to_force_mapping_p = &MPCPadToForce[IAMForceStatus.pad_layout][pad_number];
            if (mpc_to_force_mapping_p->callback != NULL)
                callback_advance = mpc_to_force_mapping_p->callback(
                    mpc_to_force_mapping_p,
                    NULL,
                    source_type,
                    note_number,
                    &midi_buffer[i],
                    size - i);
            else
                callback_advance = 0;

            // Clean buffer
            // LOG_DEBUG("Before cleaning: i=%d, size=%d, callback says %d", i, size, callback_advance);
            normal_advance = 3;
            i += cleanBufferAndAdvance(&midi_buffer[i], size - i, normal_advance, callback_advance);
            size = size - (normal_advance - callback_advance);
            // LOG_DEBUG("After cleaning: i=%d, size=%d", i, size);

        default:
            // Nothing to do here, we consume the byte and go to the next
            // TODO: implement a "discard" function looking at the status byte?
            i += 1;
            break;

            // XXX TODO: add qlink management
            // Qlink management is hard coded
            // SHIFT "KNOB TOUCH" button :  add the offset when possible
            // MPC : 90 [54-63] 7F      FORCE : 90 [53-5A] 7F  (no "untouch" exists)
            // if (midi_buffer[i + 1] >= 0x54 && midi_buffer[i + 1] <= 0x63)
            // {
            //     midi_buffer[i + 1]--; // Map to force Qlink touch

            //     if (shiftHoldMode && DeviceInfoBloc[MPCOriginalId].qlinkKnobsCount < 16)
            //         midi_buffer[i + 1] += DeviceInfoBloc[MPCOriginalId].qlinkKnobsCount;

            //     // LOG_DEBUG("Qlink 0x%02X touch\n",midi_buffer[i+1] );
            //     i += 3;
            //     continue; // next msg
            // }
        }
    }

    // Regular function return
    // LOG_DEBUG(" ...   read returns size=%d", size);
    return size;
}

///////////////////////////////////////////////////////////////////////////////
// MIDI WRITE - APP ON MPC MAPPING TO FORCE
// This is where we (mostly) command the pad colors
// Here we must:
// - Remap the Force pads and buttons to MPC pads and buttons
// - Process Force buttons that are not currently visible (keep them in cache)
// - Discard messages that would affect our "bank" buttons
// - Discard messages that would affect pads that are not visible
///////////////////////////////////////////////////////////////////////////////
size_t Mpc_MapAppWriteToForce(const void *midiBuffer, size_t size)
{
    uint8_t *midi_buffer = (uint8_t *)midiBuffer;
    uint8_t note_number;
    size_t i = 0;
    size_t normal_advance = 0;
    size_t callback_advance = 0;
    ForceControlToMPC_t *force_to_mpc_mapping_p;
    SourceType_t source_type = source_unkown;

    while (i < size)
    {
        // LOG_DEBUG("Entering event loop, i=%d, size=%d", i, size);
        // AKAI SYSEX
        // If we detect the Akai sysex header, change the harwware id by our true hardware id.
        // Messages are compatibles. Some midi msg are not always interpreted (e.g. Oled)
        if (midi_buffer[i] == 0xF0 && memcmp(&midi_buffer[i], AkaiSysex, sizeof(AkaiSysex)) == 0)
        {
            // Update the sysex id in the sysex for our original hardware
            // LOG_DEBUG("Inside Akai Sysex\n");
            midi_buffer[i + sizeof(AkaiSysex)] = DeviceInfoBloc[MPCOriginalId].sysexId;

            // SET PAD COLORS SYSEX ------------------------------------------------
            //                      v----- We start our midi buffer HERE, our pad # will be at i + sizeof(MPCSysexPadColorFn)
            // FN  F0 47 7F [40] -> 65 00 04 [Pad #] [R] [G] [B] F7
            // Here, "pad #" is 0 for top-right pad, etc.
            if (memcmp(&midi_buffer[i + 4], MPCSysexPadColorFn, sizeof(MPCSysexPadColorFn)) == 0 && (size - i >= 12))
            {
                // It's a pad, so we set the last bit to 1
                source_type = source_pad_sysex;
                note_number = getForcePadNoteNumber(midi_buffer[i + 7], true);
                LOG_DEBUG("Entering pad write (Force->MPC) for pad %02X, mapping force-MPC note is %02x", midi_buffer[i + 7], note_number);

                // XXX TODO: init project
                // (that is, project was not init and note color != 0)

                // Call the callback
                force_to_mpc_mapping_p = &ForceControlToMPC[note_number];
                while (force_to_mpc_mapping_p != NULL)
                {
                    if (force_to_mpc_mapping_p->callback == NULL)
                    {
                        LOG_DEBUG("NULL callback for Force note %02X, we swallow MIDI message", note_number);
                        force_to_mpc_mapping_p = NULL;
                        callback_advance = 0;
                    }
                    else
                    {
                        // LOG_DEBUG("Calling callback for PAD %02X change (note number=%02X)", midi_buffer[i + 3], note_number);
                        callback_advance = force_to_mpc_mapping_p->callback(
                            NULL,
                            force_to_mpc_mapping_p,
                            source_type,
                            note_number & 0x7F,
                            &midi_buffer[i + 4],
                            size - (i + 4));
                        force_to_mpc_mapping_p = force_to_mpc_mapping_p->next_control;
                        // LOG_DEBUG(".....callback return len=%d", callback_i);
                    }
                }
                // Advance the cursor
                // XXX Do we have messages with several pads updated in the same SYSEX?
                normal_advance = sizeof(AkaiSysex) + 1 + sizeof(MPCSysexPadColorFn) + 4 + 1;
                callback_advance = callback_advance > 0 ? 12 : 0;
                // LOG_DEBUG("Before cleaning: i=%d, size=%d", i, size);
                i += cleanBufferAndAdvance(&midi_buffer[i], size - i, normal_advance, callback_advance);
                size = size - (normal_advance - callback_advance);
                // LOG_DEBUG("After cleaning: i=%d, size=%d", i, size);
                continue;
            }
            else if (
                (midi_buffer[i + 4] == 0x0B) ||
                (midi_buffer[i + 4] == 0x66) ||
                (midi_buffer[i + 4] == 0x67) ||
                (midi_buffer[i + 4] == 0x68) ||
                (midi_buffer[i + 4] == 0x69) ||
                (midi_buffer[i + 4] == 0x6A))
            {
                // We erase the message if we have no callback
                LOG_DEBUG("(Discard %02x message, it's probably the OLED screens)", midi_buffer[i + 4]);
                size_t end_of_erase = i;
                while (midi_buffer[end_of_erase] != 0xF7)
                    end_of_erase++;
                if (end_of_erase < size) // check if 0xF7 was found before reaching the end of the buffer
                    end_of_erase++;      // increment i one more time to move to the octet after 0xF7
                // LOG_DEBUG("Before cleaning: i=%d, size=%d, end of erase=%d", i, size, end_of_erase);
                i += cleanBufferAndAdvance(&midi_buffer[i], size - i, end_of_erase, 0);
                size = size - (end_of_erase - i);
                // LOG_DEBUG("After cleaning: i=%d, size=%d", i, size);
            }
            else
            {
                // LOG_DEBUG("Sysex not handled\n");
                // We erase the message until 'F7' is read
                i++;
            }
        }

        // Buttons LEDs. Just remap with the reverse table,
        // iterating on each callback if needed
        else if (midi_buffer[i] == 0xB0)
        {
            note_number = midi_buffer[i + 1];
            // if (midi_buffer[i + 1] == 0x03 && midi_buffer[i + 2] == 0x03)
            //     exit(-1);
            source_type = source_led;
            force_to_mpc_mapping_p = &ForceControlToMPC[note_number];
            while (force_to_mpc_mapping_p != NULL)
            {
                if (force_to_mpc_mapping_p->callback == NULL)
                {
                    LOG_DEBUG("NULL callback for Force note %02X", note_number);
                    force_to_mpc_mapping_p = NULL;
                    callback_advance = 0;
                    continue;
                }
                callback_advance = force_to_mpc_mapping_p->callback(
                    NULL,
                    force_to_mpc_mapping_p,
                    source_type,
                    note_number,
                    &midi_buffer[i],
                    size - i);
                // LOG_DEBUG("Callback for LED %02X returned %d", note_number, callback_advance);
                force_to_mpc_mapping_p = force_to_mpc_mapping_p->next_control;
            }

            // Advance the cursor
            // XXX Do we have messages with several pads updated in the same SYSEX?
            normal_advance = 3;
            // LOG_DEBUG("Before cleaning: i=%d, size=%d, callback says %d", i, size, callback_advance);
            i += cleanBufferAndAdvance(&midi_buffer[i], size - i, normal_advance, callback_advance);
            size = size - (normal_advance - callback_advance);
            // LOG_DEBUG("After cleaning: i=%d, size=%d", i, size);
            continue;
        }

        else
            i++;
    }

    // Return what we've read
    // LOG_DEBUG("    => Returning %d bytes", size);
    // if (size == 120 && midi_buffer[8] != 0x00 && midi_buffer[0] == 0xF0) exit(-1);      // TESTING HERE
    return size;
}
