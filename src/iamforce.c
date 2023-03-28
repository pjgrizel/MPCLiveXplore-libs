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
    .pad_layout = IAMFORCE_LAYOUT_NONE,
    .force_mode = MPC_FORCE_MODE_NONE,
    .launch_mode_layout = IAMFORCE_LAYOUT_PAD_BANK_A,
    .stepseq_mode_layout = IAMFORCE_LAYOUT_PAD_BANK_A,
    .note_mode_layout = IAMFORCE_LAYOUT_PAD_BANK_C,
    .mode_buttons = 0,
    .tap_status = false,
    .last_button_down = 0,
    .started_button_down = {0, 0},
    .project_loaded = false,
};
IAMForceStatus_t IAMForceRestStatus;

// Default length of messages
uint_fast8_t SOURCE_MESSAGE_LENGTH[7] = {
    3, // source_button
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
    // Pad bank B
    {
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
    // Pad bank C
    {
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
    // Pad bank D
    {
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
    // PAD LAYOUT MODE, this is 100% custom here (because of the reverse relationship)
    {
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
    // SCENE MODE
    {
        // 1st line
        [0x00].note_number = FORCE_BT_STOP_ALL,
        [0x01].note_number = FORCE_BT_UP,
        [0x02].note_number = FORCE_BT_LAUNCH_1,
        [0x03].note_number = FORCE_BT_LAUNCH_5,
        // 2d line
        [0x04].note_number = FORCE_BT_STOP_ALL,
        [0x05].note_number = FORCE_BT_DOWN,
        [0x06].note_number = FORCE_BT_LAUNCH_2,
        [0x07].note_number = FORCE_BT_LAUNCH_6,
        // 3rd line
        [0x08].note_number = FORCE_BT_UNSET,
        [0x09].note_number = FORCE_BT_UNSET,
        [0x0a].note_number = FORCE_BT_LAUNCH_3,
        [0x0b].note_number = FORCE_BT_LAUNCH_7,
        // Last line
        [0x0c].note_number = FORCE_BT_UNSET,
        [0x0d].note_number = FORCE_BT_UNSET,
        [0x0e].note_number = FORCE_BT_LAUNCH_4,
        [0x0f].note_number = FORCE_BT_LAUNCH_8},
    // IAMFORCE_LAYOUT_PAD_MUTE
    {
        [0x00].note_number = FORCE_BT_MUTE,
        [0x00].color = COLOR_YELLOW,
        [0x01].note_number = FORCE_BT_SOLO,
        [0x01].color = COLOR_BLUE,
        [0x02].note_number = FORCE_BT_REC_ARM,
        [0x02].color = COLOR_RED,
        [0x03].note_number = FORCE_BT_CLIP_STOP,
        [0x03].color = COLOR_GREEN,
        [0x04].note_number = FORCE_BT_LEFT,
        [0x04].color = COLOR_GREY,
        [0x05].note_number = FORCE_BT_ASSIGN_A,
        [0x05].color = COLOR_ORANGE,
        [0x06].note_number = FORCE_BT_ASSIGN_B,
        [0x06].color = COLOR_RED,
        [0x07].note_number = FORCE_BT_RIGHT,
        [0x07].color = COLOR_GREY,
        [0x08].note_number = FORCE_BT_MUTE_PAD1, // XXX which channel is it?
        [0x09].note_number = FORCE_BT_MUTE_PAD2,
        [0x0a].note_number = FORCE_BT_MUTE_PAD3,
        [0x0b].note_number = FORCE_BT_MUTE_PAD4,
        [0x0c].note_number = FORCE_BT_MUTE_PAD5,
        [0x0d].note_number = FORCE_BT_MUTE_PAD6,
        [0x0e].note_number = FORCE_BT_MUTE_PAD7,
        [0x0f].note_number = FORCE_BT_MUTE_PAD8},
    // IAMFORCE_LAYOUT_PAD_COLS
    {
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
        [0x08].note_number = FORCE_BT_COLUMN_PAD1, // XXX Which channel is it?
        [0x09].note_number = FORCE_BT_COLUMN_PAD2,
        [0x0a].note_number = FORCE_BT_COLUMN_PAD3,
        [0x0b].note_number = FORCE_BT_COLUMN_PAD4,
        [0x0c].note_number = FORCE_BT_COLUMN_PAD5,
        [0x0d].note_number = FORCE_BT_COLUMN_PAD6,
        [0x0e].note_number = FORCE_BT_COLUMN_PAD7,
        [0x0f].note_number = FORCE_BT_COLUMN_PAD8},
    // IAMFORCE_LAYOUT_PAD_XFDR
    {
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
    [LIVEII_BT_PLAY_START].note_number = FORCE_BT_PLAY,

    // Edition buttons. These are double-function buttons so we use a callbacks here
    [LIVEII_BT_NOTE_REPEAT].callback = cb_edit_button,
    [LIVEII_BT_FULL_LEVEL].callback = cb_edit_button,
    [LIVEII_BT_16_LEVEL].callback = cb_edit_button,
    [LIVEII_BT_ERASE].callback = cb_edit_button,

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
    // XXX Should I update colors as well?
    [FORCE_BT_LAUNCH].callback = cb_edit_button,
    [FORCE_BT_STEP_SEQ].callback = cb_edit_button,
    [FORCE_BT_NOTE].callback = cb_edit_button,
    [FORCE_BT_MUTE].callback = cb_edit_button,
    [FORCE_BT_REC_ARM].callback = cb_edit_button,
    [FORCE_BT_CLIP_STOP].callback = cb_edit_button};

static uint8_t ForceControlToMPCExtraNext = 0x80;                             // Next available index
static uint8_t ForceControlToMPCExtraMax = 0x80 + FORCEPADS_TABLE_IDX_OFFSET; // Max index


// These are the matrices where actual RGB values are stored
// They are initialized at start time and populated in the Write function
// And they are given with their index
// Use IAMFORCE_LAYOUT_PAD_* variables to address a specific array
static PadColor_t MPCPadValues[IAMFORCE_LAYOUT_N][16];

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
            // XXX TODO: don't override default (already set) values
            reverse_mapping_p->note_number = mpc_note_number;
            reverse_mapping_p->bank = i;
            reverse_mapping_p->callback = mapping_p->callback;
            reverse_mapping_p->next_control = NULL;
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
        LOG_DEBUG("   => Mapping Force note %02x to callback %p", mapping_p->note_number, mapping_p->callback);
        // XXX TODO: don't override default (already set) values
        reverse_mapping_p->note_number = mpc_note_number;
        reverse_mapping_p->bank = IAMFORCE_LAYOUT_NONE;
        reverse_mapping_p->callback = mapping_p->callback;
        reverse_mapping_p->next_control = NULL;
    }
}

/**************************************************************************
 *                                                                        *
 *  MPC Pads management                                                   *
 *                                                                        *
 **************************************************************************/

// If instant_redraw is True, we will redraw the pad immediately (if we are in the same bank)
// This function returns True if the pad has to be redrawn, False otherwise
bool SetLayoutPad(uint8_t matrix, uint8_t note_number, PadColor_t rgb, bool instant_redraw)
{
    // XXX TODO, including refresh of the pad color if necessary!
    return false;
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

    // Dump mapping (ForceControlToMPC)
    for (int i = 0; i < CONTROL_TABLE_SIZE; i++)
    {
        LOG_DEBUG("ForceControlToMPC[%02x] = %02x.%02x // %p %p",
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
            MPCPadValues[i][j] = COLOR_BLACK;
    }

    // Initialize additional MPC status data
    clock_gettime(CLOCK_MONOTONIC_RAW, &IAMForceStatus.started_button_down);
}

///////////////////////////////////////////////////////////////////////////////
// Prepare a fake midi message in the Private midi context
///////////////////////////////////////////////////////////////////////////////
void FakeMidiMessage(uint8_t buf[], size_t size)
{
    // LOG_DEBUG("FakeMidiMessage(buf=%p, size=%d)
    // Just put all the bytes to 0
    memset(buf, 0x00, size);
}


// Set pad colors
// 2 implementations : call with a 32 bits color int value or with r,g,b values
// Pad number starts from top left (0), 8 pads per line
inline void SetPadColor(const uint8_t padL, const u_int8_t padC, const uint8_t r, const uint8_t g, const uint8_t b)
{

    uint8_t sysexBuff[128];
    char sysexBuffDebug[128];
    int p = 0;

    // Log event
    LOG_DEBUG("               Set pad color: L=%d C=%d r g b %02X %02X %02X", padL, padC, r, g, b);

    // Double-check input data
    if (padL > 3 || padC > 3)
    {
        LOG_ERROR("MPC Pad Line refresh : wrong pad number %d %d", padL, padC);
        return;
    }

    // Set pad number correctly
    uint8_t padNumber = (3 - padL) * 4 + padC;

    // F0 47 7F [3B] 65 00 04 [Pad #] [R] [G] [B] F7
    memcpy(sysexBuff, AkaiSysex, sizeof(AkaiSysex));
    p += sizeof(AkaiSysex);

    // Add the current product id
    sysexBuff[p++] = DeviceInfoBloc[MPCOriginalId].sysexId;

    // Add the pad color fn and pad number and color
    memcpy(&sysexBuff[p], MPCSysexPadColorFn, sizeof(MPCSysexPadColorFn));
    p += sizeof(MPCSysexPadColorFn);
    sysexBuff[p++] = padNumber;

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
    orig_snd_rawmidi_write(rawvirt_outpriv, sysexBuff, p);
}

inline void SetPadColorFromColorInt(const uint8_t padL, const u_int8_t padC, const PadColor_t rgbColorValue)
{
    // Colors R G B max value is 7f in SYSEX. So the bit 8 is always set to 0.
    uint8_t r = (rgbColorValue >> 16) & 0x7F;
    uint8_t g = (rgbColorValue >> 8) & 0x7F;
    uint8_t b = rgbColorValue & 0x7F;
    SetPadColor(padL, padC, r, g, b);
}

// Given a MIDI note number, we convert it to a PAD number,
// from 0 (top left) to 15 (bottom right)
uint8_t getMPCPadNumber(uint8_t note_number)
{
    switch (note_number)
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
        return 0;
    }
}


// This is the reverse of getMPCPadNumber
uint8_t getForcePadNoteNumber(uint8_t pad_number, bool extra_bit)
{
    return pad_number + FORCEPADS_TABLE_IDX_OFFSET + (extra_bit ? 0x80 : 0);
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
        LOG_ERROR("Invalid pad number: %02x", pad_number);
        return 0xFF;
    }
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
    uint8_t note_number = 0xFF;
    MPCControlToForce_t *mpc_to_force_mapping_p;
    LOG_DEBUG("We have a %d/%d bytes buffer", size, maxSize);

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
                LOG_DEBUG("Identity request");
                memcpy(&midi_buffer[i + sizeof(IdentityReplySysexHeader)], DeviceInfoBloc[MPCId].sysexIdReply, sizeof(DeviceInfoBloc[MPCId].sysexIdReply));
                i += sizeof(IdentityReplySysexHeader) + sizeof(DeviceInfoBloc[MPCId].sysexIdReply);
            }

            // Consume the rest of the message until 0xF7 is found (end of sysex)
            while (i < size && midi_buffer[i] != 0xF7)
                i++;
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
            // Apply mapping, call the callback function
            source_type = source_button;
            note_number = midi_buffer[i + 1];
            mpc_to_force_mapping_p = &MPCButtonToForce[note_number];
            if (mpc_to_force_mapping_p->callback != NULL)
                i += mpc_to_force_mapping_p->callback(
                    mpc_to_force_mapping_p,
                    NULL,
                    source_type,
                    note_number,
                    &midi_buffer[i],
                    size - i);
            break;

        // PADS -------------------------------------------------------------------
        case 0x99:
        case 0x89:
        case 0xA9:
            note_number = getMPCPadNoteNumber(midi_buffer[i + 1]);
            if (midi_buffer[i] == 0x99)
                source_type = source_pad_note_on;
            else if (midi_buffer[i] == 0x89)
                source_type = source_pad_note_off;
            else if (midi_buffer[i] == 0xA9)
                source_type = source_pad_aftertouch;
            mpc_to_force_mapping_p = &MPCPadToForce[IAMForceStatus.pad_layout][getMPCPadNumber(midi_buffer[i + 1])];
            if (mpc_to_force_mapping_p->callback != NULL)
                i += mpc_to_force_mapping_p->callback(
                    mpc_to_force_mapping_p,
                    NULL,
                    source_type,
                    note_number,
                    &midi_buffer[i],
                    size - i);

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

            //     // LOG_DEBUG("Qlink 0x%02x touch\n",midi_buffer[i+1] );
            //     i += 3;
            //     continue; // next msg
            // }
        }
    }

    // Regular function return
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
void Mpc_MapAppWriteToForce(const void *midiBuffer, size_t size)
{
    uint8_t *midi_buffer = (uint8_t *)midiBuffer;
    uint8_t note_number;
    size_t i = 0;
    size_t callback_i = 0;
    ForceControlToMPC_t *force_to_mpc_mapping_p;
    SourceType_t source_type = source_unkown;

    while (i < size)
    {
        // AKAI SYSEX
        // If we detect the Akai sysex header, change the harwware id by our true hardware id.
        // Messages are compatibles. Some midi msg are not always interpreted (e.g. Oled)
        if (midi_buffer[i] == 0xF0 && memcmp(&midi_buffer[i], AkaiSysex, sizeof(AkaiSysex)) == 0)
        {
            // Update the sysex id in the sysex for our original hardware
            // LOG_DEBUG("Inside Akai Sysex\n");
            i += sizeof(AkaiSysex);
            midi_buffer[i] = DeviceInfoBloc[MPCOriginalId].sysexId;
            i++;

            // SET PAD COLORS SYSEX ------------------------------------------------
            //                      v----- We start our midi buffer HERE, our pad # will be at i + sizeof(MPCSysexPadColorFn)
            // FN  F0 47 7F [3B] -> 65 00 04 [Pad #] [R] [G] [B] F7
            // Here, "pad #" is 0 for top-right pad, etc.
            if (memcmp(&midi_buffer[i], MPCSysexPadColorFn, sizeof(MPCSysexPadColorFn)) == 0)
            {
                // XXX TODO: triple-check against buffer overflow!
                // It's a pad, so we set the last bit to 1
                source_type = source_pad_sysex;
                LOG_DEBUG("Entering pad write (Force->MPC) for pad %02x", midi_buffer[i + 3]);
                note_number = getForcePadNoteNumber(midi_buffer[i + 3], true);
                LOG_DEBUG("Force note number (with extra bit): %02x", note_number);

                // XXX TODO: init project
                // (that is, project was not init and note color != 0)

                // Call the callback
                force_to_mpc_mapping_p = &ForceControlToMPC[note_number];
                while (force_to_mpc_mapping_p != NULL)
                {
                    if (force_to_mpc_mapping_p->callback == NULL)
                    {
                        LOG_DEBUG("NULL callback for Force note %02x", note_number);
                        callback_i = 7;
                        break;
                    }
                    callback_i = force_to_mpc_mapping_p->callback(
                        NULL,
                        force_to_mpc_mapping_p,
                        source_type,
                        note_number & 0x7F,
                        &midi_buffer[i],
                        size - i);
                    force_to_mpc_mapping_p = force_to_mpc_mapping_p->next_control;
                }
                i += callback_i; // Only advance once even if we called several callbacks
            }
        }

        // Buttons LEDs. Just remap with the reverse table,
        // iterating on each callback if needed
        else if (midi_buffer[i] == 0xB0)
        {
            note_number = midi_buffer[i + 1];
            source_type = source_led;
            force_to_mpc_mapping_p = &ForceControlToMPC[note_number];
            while (force_to_mpc_mapping_p != NULL)
            {
                if (force_to_mpc_mapping_p->callback == NULL)
                {
                    LOG_DEBUG("NULL callback for Force note %02x", note_number);
                    callback_i = 3;
                    break;
                }
                callback_i = force_to_mpc_mapping_p->callback(
                    NULL,
                    force_to_mpc_mapping_p,
                    source_type,
                    note_number,
                    &midi_buffer[i],
                    size - i);
                force_to_mpc_mapping_p = force_to_mpc_mapping_p->next_control;
            }
            i += callback_i; // Only advance once even if we called several callbacks
        }

        else
            i++;
    }
}

