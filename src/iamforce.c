
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

// Buttons and controls Mapping tables
// SHIFT values have bit 7 set
// int map_ButtonsLeds[MAPPING_TABLE_SIZE];
// int map_ButtonsLeds_Inv[MAPPING_TABLE_SIZE]; // Inverted table

// Force Matrix pads color cache
// static ForceMPCPadColor_t PadSysexColorsCache[256];
// static ForceMPCPadColor_t PadSysexColorsCacheBankB[16];
// static ForceMPCPadColor_t PadSysexColorsCacheBankC[16];
// static ForceMPCPadColor_t PadSysexColorsCacheBankD[16];

// Global status / Rest status
// Initial status
IAMForceStatus_t IAMForceStatus = {
    .pad_layout = IAMFORCE_LAYOUT_N,
    .force_mode = MPC_FORCE_MODE_NONE,
    .mode_buttons = 0,
    .tap_status = false,
    .last_button_down = 0,
    .started_button_down = {0, 0},
    .project_loaded = false,
};
IAMForceStatus_t IAMForceRestStatus;

// FORCE starts from top-left, MPC start from BOTTOM-left
// These matrix are MPC => Force (not the other way around)
// Also, the numbers are those of a NOTE NUMBER, not the pad sysex!
// Pad sysex will be deduced at runtime.
MPCControlToForce_t MPCPadToForce[IAMFORCE_LAYOUT_N][16] = {
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
        [0x01].note_number = FORCE_BT_SOLO,
        [0x02].note_number = FORCE_BT_REC_ARM,
        [0x03].note_number = FORCE_BT_CLIP_STOP,
        [0x04].note_number = FORCE_BT_LEFT,
        [0x05].note_number = FORCE_BT_ASSIGN_A,
        [0x06].note_number = FORCE_BT_ASSIGN_B,
        [0x07].note_number = FORCE_BT_RIGHT,
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
        [0x02].note_number = FORCE_BT_UNSET,
        [0x03].note_number = FORCE_BT_UNSET,
        [0x04].note_number = FORCE_BT_LEFT,
        [0x05].note_number = FORCE_BT_DOWN,
        [0x06].note_number = FORCE_BT_UNSET,
        [0x07].note_number = FORCE_BT_RIGHT,
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

// Buttons mapping.
static const MPCControlToForce_t MPCButtonToForce[127] = {
    // Global stuff
    [LIVEII_BT_ENCODER].note_number = FORCE_BT_ENCODER,
    [LIVEII_BT_SHIFT].note_number = FORCE_BT_SHIFT,
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

    // Edition buttons. These are double-function buttons so we use a callback here
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
static ForceControlToMPC_t ForceControlToMPC[CONTROL_TABLE_SIZE];
static uint8_t ForceControlToMPCExtraNext = 0x80;                             // Next available index
static uint8_t ForceControlToMPCExtraMax = 0x80 + FORCEPADS_TABLE_IDX_OFFSET; // Max index

// These are the matrices where actual RGB values are stored
// They are initialized at start time and populated in the Write function
// And they are given with their index
// Use IAMFORCE_LAYOUT_PAD_* variables to address a specific array
static ForceMPCPadColor_t MPCPadValues[IAMFORCE_LAYOUT_N][16];

void invertMPCToForceMapping()
{
    // Various variables helping us
    size_t MPCPadToForce_n = sizeof(MPCPadToForce) / sizeof(MPCPadToForce[0]);
    size_t MPCButtonToForce_n = sizeof(MPCButtonToForce) / sizeof(MPCButtonToForce[0]);
    ForceControlToMPC_t _empty = {0xFF, 0xFF, NULL, NULL};
    ForceControlToMPC_t current_force_to_mpc = _empty;
    uint8_t mpc_note_number = 0xFF;

    // Initialize global mapping tables to 0 (just in case)
    for (uint8_t i = 0; i < CONTROL_TABLE_SIZE; i++)
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
                    tklog_error("Can't increment ForceControlToMPCExtraNext, you have too many buttons with multiple combinations");
            }

            // Save mapping
            reverse_mapping_p->note_number = mpc_note_number;
            reverse_mapping_p->bank = i;
            reverse_mapping_p->callback = mapping_p->callback;
            reverse_mapping_p->next_control = NULL;
        }
    }

    // Now we do the same with MPCButtonToForce!
    for (uint8_t note_number = 0 ; note_number < MPCButtonToForce_n ; note_number++)
    {
        // Get the current element and tweak values if necessary
        MPCControlToForce_t *mapping_p = &MPCButtonToForce[note_number];
        if (mapping_p->note_number == 0xFF)
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
                tklog_error("Can't increment ForceControlToMPCExtraNext, you have too many buttons with multiple combinations");
        }

        // Save mapping
        reverse_mapping_p->note_number = mpc_note_number;
        reverse_mapping_p->bank = IAMFORCE_LAYOUT_NONE;
        reverse_mapping_p->callback = mapping_p->callback;
        reverse_mapping_p->next_control = NULL;
    }
}

/**************************************************************************
 *                                                                        *
 *  Callbacks                                                             *
 *                                                                        *
 **************************************************************************/

size_t cb_default(MPCControlToForce_t *force_target, ForceControlToMPC_t *mpc_target, uint8_t *midi_buffer, size_t buffer_size)
{
    size_t i = 0;
    // Default callback behavior
    // (which is just to propagate to the appropriate pad or button

    // XXX TODO We swallow Aftertouch messages
    if (midi_buffer[i] == 0xA9)
    {
        PrepareFakeMidiMsg(&midi_buffer[i]);
        i += 3;
    }
}

size_t cb_tap_tempo(MPCControlToForce_t *force_target, ForceControlToMPC_t *mpc_target, uint8_t *midi_buffer, size_t buffer_size)
{
    // The tools of the trade
    int fd;
    uint8_t capacity = 0;
    uint8_t blink = 0;
    uint8_t other_leds = 0x01;
    char buffer[16];
    char intBuffer[4];
    uint8_t battery_status;
    ssize_t bytesRead;
    uint8_t scale;

    // If buffer is too small, we can't do much about it
    if (buffer_size < 3)
    {
        tklog_error("Buffer too small for tap callback");
        return;
    }

    // Store status
    IAMForceStatus.tap_status = buffer[2] == BUTTON_COLOR_RED_LIGHT ? false : true;

    // Once every 10 taps, update battery light
    if (IAMForceStatus.tap_counter == 0)
    {
        // Read battery_ value from fp capacity
        // Convert the string to an integer
        fd = orig_open64(POWER_SUPPLY_CAPACITY_PATH, O_RDONLY);
        bytesRead = read(fd, buffer, 6);
        orig_close(fd);
        memcpy(intBuffer, buffer, 3);
        intBuffer[bytesRead] = '\0';
        capacity = atoi(intBuffer);
        scale = capacity * 8 / 100;

        // Read battery status from POWER_SUPPLY_STATUS_PATH
        // If it's "Charging", we display a charging icon (kind of)
        fd = orig_open64(POWER_SUPPLY_STATUS_PATH, O_RDONLY);
        bytesRead = read(fd, buffer, 9);
        orig_close(fd);
        buffer[bytesRead] = '\0';
        if (strcmp(buffer, "Charging\n") == 0)
        {
            battery_status = BATTERY_CHARGING;
            other_leds = 0x02;
        }
        else if (strcmp(buffer, "Full\n") == 0)
        {
            battery_status = BATTERY_FULL;
            other_leds = 0x02;
        }

        // If battery is currently charging, we make the last light blink
        // by decreasing the scale by 1
        if (battery_status == BATTERY_CHARGING && !IAMForceStatus.tap_status && scale > 0)
            blink = 1;

        // Reset lights
        uint8_t light_1[] = {0xB0, 0x5A, 0x00};
        uint8_t light_2[] = {0xB0, 0x5B, 0x00};
        uint8_t light_3[] = {0xB0, 0x5C, 0x00};
        uint8_t light_4[] = {0xB0, 0x5D, 0x00};

        // Handle 'current' light
        switch (scale)
        {
        case 0:
            break;
        case 1:
            light_1[2] = 0x01 - blink;
            break;
        case 2:
            light_1[2] = 0x02 - blink;
            break;
        case 3:
            light_1[2] = other_leds;
            light_2[2] = 0x01 - blink;
            break;
        case 4:
            light_1[2] = other_leds;
            light_2[2] = 0x02 - blink;
            break;
        case 5:
            light_1[2] = other_leds;
            light_2[2] = other_leds;
            light_3[2] = 0x01 - blink;
            break;
        case 6:
            light_1[2] = other_leds;
            light_2[2] = other_leds;
            light_3[2] = 0x02 - blink;
            break;
        case 7:
            light_1[2] = other_leds;
            light_2[2] = other_leds;
            light_3[2] = other_leds;
            light_4[2] = 0x01 - blink;
            break;
        case 8:
            light_1[2] = other_leds;
            light_2[2] = other_leds;
            light_3[2] = other_leds;
            light_4[2] = 0x02 - blink;
            break;
        }

        // Let's write it to the private ports
        if (
            IAMForceStatus.battery_status != battery_status || IAMForceStatus.battery_capacity != capacity)
        {
            // Update lights
            orig_snd_rawmidi_write(rawvirt_outpriv, light_1, sizeof(light_1));
            orig_snd_rawmidi_write(rawvirt_outpriv, light_2, sizeof(light_2));
            orig_snd_rawmidi_write(rawvirt_outpriv, light_3, sizeof(light_3));
            orig_snd_rawmidi_write(rawvirt_outpriv, light_4, sizeof(light_4));

            // Store current status
            IAMForceStatus.battery_status = battery_status;
            IAMForceStatus.battery_capacity = capacity;
        }
    }

    // Handle counter
    IAMForceStatus.tap_counter++;
    if (IAMForceStatus.tap_counter == BATTERY_CHECK_INTERVAL)
        IAMForceStatus.tap_counter = 0;
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
    // Initialize global mapping tables
    invertMPCToForceMapping();

    // Initialize all pads caches (at load-time, we consider they are black)
    for (int i = 0; i < IAMFORCE_LAYOUT_N; i++)
    {
        for (int j = 0; j < 16; j++)
        {
            MPCPadValues[i][j].r = 0x00;
            MPCPadValues[i][j].g = 0x00;
            MPCPadValues[i][j].b = 0x00;
        }
    }

    // Initialize additional MPC status data
    clock_gettime(CLOCK_MONOTONIC_RAW, &IAMForceStatus.started_button_down);
}

///////////////////////////////////////////////////////////////////////////////
// Prepare a fake midi message in the Private midi context
///////////////////////////////////////////////////////////////////////////////
void PrepareFakeMidiMsg(uint8_t buf[])
{
    buf[0] = 0x00;
    buf[1] = 0x00;
    buf[2] = 0x00;
}

// Completely redraw a matrix according to its cache value
void DrawPadLayoutFromCache(uint8_t layout, uint8_t pad_number)
{
    if (layout >= IAMFORCE_LAYOUT_N || pad_number >= 16)
    {
        tklog_error("DrawMatrixPadFromCache(matrix=%02x, pad_number=%02x) - invalid matrix or pad number   \n", layout, pad_number);
        return;
    }
    // tklog_debug("DrawMatrixPadFromCache(matrix=%02x, pad_number=%02x)\n", matrix, pad_number);

    // Get pad coordinates
    uint8_t padL = pad_number / 4;
    uint8_t padC = pad_number % 4;

    // Do we *HAVE* to color this pad?
    if (layout != IAMForceStatus.pad_layout)
    {
        tklog_debug("  ...We ignore repainting message for matrix %02x, pad number %02x\n", layout, pad_number);
        return;
    }

    // Find the pad color as it's stored
    SetPadColor(padL, padC,
                MPCPadValues[layout][pad_number].r,
                MPCPadValues[layout][pad_number].g,
                MPCPadValues[layout][pad_number].b);
}

// //////////////////////////////////////////////////////////////////
// Bank buttons management
// //////////////////////////////////////////////////////////////////
void MPCSwitchBankMode(uint8_t bank_button, bool key_down)
{
    struct timespec now;
    uint8_t selected_bank_mask;
    uint64_t down_duration;
    bool is_click = false;        // down -> up in less than 0.5s
    bool is_double_click = false; // down -> up -> down in less than 0.5s
    uint8_t current_bank_layer_mask = (MPCPadMode & 0x0f) ? PAD_BANK_ABCD : PAD_BANK_EFGH;
    int8_t permanent_mode_mask = PermanentMode & PAD_BANK_ABCD ? PAD_BANK_ABCD : PAD_BANK_EFGH;

    // Convert bank_button to bank_pad
    switch (bank_button)
    {
    case LIVEII_BT_BANK_A:
        selected_bank_mask = PAD_BANK_A | PAD_BANK_E;
        break;
    case LIVEII_BT_BANK_B:
        selected_bank_mask = PAD_BANK_B | PAD_BANK_F;
        break;
    case LIVEII_BT_BANK_C:
        selected_bank_mask = PAD_BANK_C | PAD_BANK_G;
        break;
    case LIVEII_BT_BANK_D:
        selected_bank_mask = PAD_BANK_D | PAD_BANK_H;
        break;
    }

    // Handle click / hold / doubleclick stuff
    clock_gettime(CLOCK_MONOTONIC_RAW, &now);
    if (LastKeyDownBankButton == bank_button)
    {
        down_duration = (now.tv_sec - started_down.tv_sec) * 1000 + (now.tv_nsec - started_down.tv_nsec) / 1000000;
        if (key_down)
        {
            if (down_duration < DOUBLE_CLICK_DELAY)
            {
                is_double_click = true;
                started_down.tv_sec = 0; // Avoid mixing double clicks and click at release
            }
        }
        else
        {
            if (down_duration < HOLD_DELAY)
                is_click = true;
        }
    }
    LastKeyDownBankButton = bank_button;
    if (key_down)
    {
        clock_gettime(CLOCK_MONOTONIC_RAW, &started_down);
        DownBankMask |= selected_bank_mask;
        DownBankMask &= 0xf;
    }
    else
    {
        DownBankMask &= ~selected_bank_mask;
        DownBankMask &= 0xf;
    }

    tklog_debug("MPCSwitchBankMode(bank_button=%02x, key_down=%d) => down_duration=%lld, is_click=%d, is_double_click=%d, shift=%d, current bank mask=%02x, permanent=%02x, DownMask=%02x\n",
                bank_button, key_down, down_duration, is_click, is_double_click, shiftHoldMode, current_bank_layer_mask, PermanentMode, DownBankMask);
    tklog_debug("               ...(bank_button=%02x, key_down=%d)\n", bank_button, key_down);
    tklog_debug("               ...DownBankMask = %02x\n", (DownBankMask & ~PAD_BANK_A) << 4);

    // Hold modes, "high level" first, low level last
    if (is_click && (PermanentMode & selected_bank_mask))
    {
        // If PERMANENT BANK is the same as the button that's pressed,
        // then we switch layers (permanently)
        // NOTA: actually this doesn't make sense as A/B/C/D banks have no relation with
        // their EFGH counterparts
        MPCSwitchMatrix(selected_bank_mask & ~permanent_mode_mask, PAD_BANK_PERMANENTLY);
    }
    else if (is_click)
    {
        // We leave the 'overlay' mode, so we must switch to the permanent mode
        MPCSwitchMatrix(selected_bank_mask & permanent_mode_mask, PAD_BANK_PERMANENTLY);
    }
    else if (key_down)
    {
        // If SHIFT button is down at the same time, consider the change as permanent and immediate
        // Otherwise we assume it's temporary
        if (shiftHoldMode)
            MPCSwitchMatrix(selected_bank_mask & ~current_bank_layer_mask, PAD_BANK_PERMANENTLY);
        // Press/hold => we switch to the OTHER layer momentary (except for bank A)
        // if (bank_button == PAD_BANK_A && current_bank_layer_mask == PAD_BANK_EFGH)
        //     MPCSwitchMatrix(RestBanksMask & PAD_BANK_ABCD, PAD_BANK_MOMENTARY);
        else
            MPCSwitchMatrix(selected_bank_mask & ~current_bank_layer_mask, PAD_BANK_MOMENTARY);
    }
    // Release => we switch back to the saved layer (where we came from)
    else
    {
        // Unless it's a click, return to whatever was before the button was pressed (?)
        MPCSwitchMatrix(PAD_BANK_RESTORE, PAD_BANK_PERMANENTLY);
    }
}

////////
// Completely redraw the pads according to the mode we're in.
// Also take care of the "PAD BANK" button according to the proper mode.
// If 'permanently' is True, it will replace the 'RestBankMode'.
////////
void MPCSwitchMatrix(uint8_t new_mode, bool permanently)
{
    // Are we restoring to the previous mode?
    if (new_mode == PAD_BANK_RESTORE)
    {
        tklog_debug("    ...restoring...\n");
        new_mode = PermanentMode;
        permanently = true;
    }

    // Debug + handle special case (0x10 is conveniently remapped to whatever current layer of 0x0F is)
    tklog_debug("     ...Switching to mode %02x (from current mode: %02x), permanent=%d\n", new_mode, MPCPadMode, permanently);

    // Reset all "PAD BANK" buttons
    uint8_t bt_bank_a[] = {0xB0, BANK_A, BUTTON_COLOR_OFF};
    uint8_t bt_bank_b[] = {0xB0, BANK_B, BUTTON_COLOR_OFF};
    uint8_t bt_bank_c[] = {0xB0, BANK_C, BUTTON_COLOR_OFF};
    uint8_t bt_bank_d[] = {0xB0, BANK_D, BUTTON_COLOR_OFF};

    // React according to the mode we switched to
    switch (new_mode)
    {
    case PAD_BANK_A:
        bt_bank_a[2] = BUTTON_COLOR_RED;
        bt_bank_b[2] = BUTTON_COLOR_RED_LIGHT;
        bt_bank_c[2] = BUTTON_COLOR_RED_LIGHT;
        bt_bank_d[2] = BUTTON_COLOR_RED_LIGHT;
        break;
    case PAD_BANK_B:
        bt_bank_a[2] = BUTTON_COLOR_RED_LIGHT;
        bt_bank_b[2] = BUTTON_COLOR_RED;
        bt_bank_c[2] = BUTTON_COLOR_RED_LIGHT;
        bt_bank_d[2] = BUTTON_COLOR_RED_LIGHT;
        break;
    case PAD_BANK_C:
        bt_bank_a[2] = BUTTON_COLOR_RED_LIGHT;
        bt_bank_b[2] = BUTTON_COLOR_RED_LIGHT;
        bt_bank_c[2] = BUTTON_COLOR_RED;
        bt_bank_d[2] = BUTTON_COLOR_RED_LIGHT;
        break;
    case PAD_BANK_D:
        bt_bank_a[2] = BUTTON_COLOR_RED_LIGHT;
        bt_bank_b[2] = BUTTON_COLOR_RED_LIGHT;
        bt_bank_c[2] = BUTTON_COLOR_RED_LIGHT;
        bt_bank_d[2] = BUTTON_COLOR_RED;
        break;
    case PAD_BANK_E:
        bt_bank_a[2] = BUTTON_COLOR_YELLOW;
        bt_bank_b[2] = BUTTON_COLOR_YELLOW_LIGHT;
        bt_bank_c[2] = BUTTON_COLOR_YELLOW_LIGHT;
        bt_bank_d[2] = BUTTON_COLOR_YELLOW_LIGHT;
        break;
    case PAD_BANK_F:
        bt_bank_a[2] = BUTTON_COLOR_YELLOW_LIGHT;
        bt_bank_b[2] = BUTTON_COLOR_YELLOW;
        bt_bank_c[2] = BUTTON_COLOR_YELLOW_LIGHT;
        bt_bank_d[2] = BUTTON_COLOR_YELLOW_LIGHT;
        break;
    case PAD_BANK_G:
        bt_bank_a[2] = BUTTON_COLOR_YELLOW_LIGHT;
        bt_bank_b[2] = BUTTON_COLOR_YELLOW_LIGHT;
        bt_bank_c[2] = BUTTON_COLOR_YELLOW;
        bt_bank_d[2] = BUTTON_COLOR_YELLOW_LIGHT;
        break;
    case PAD_BANK_H:
        bt_bank_a[2] = BUTTON_COLOR_YELLOW_LIGHT;
        bt_bank_b[2] = BUTTON_COLOR_YELLOW_LIGHT;
        bt_bank_c[2] = BUTTON_COLOR_YELLOW_LIGHT;
        bt_bank_d[2] = BUTTON_COLOR_YELLOW;
        break;
    }

    // Set button lights
    orig_snd_rawmidi_write(rawvirt_outpriv, bt_bank_a, sizeof(bt_bank_a));
    orig_snd_rawmidi_write(rawvirt_outpriv, bt_bank_b, sizeof(bt_bank_b));
    orig_snd_rawmidi_write(rawvirt_outpriv, bt_bank_c, sizeof(bt_bank_c));
    orig_snd_rawmidi_write(rawvirt_outpriv, bt_bank_d, sizeof(bt_bank_d));

    // Save the new mode if it's permanently
    if (permanently)
    {
        PermanentMode = new_mode;
        // if (new_mode & PAD_BANK_ABCD)
        //     RestBanksMask = ((RestBanksMask & ~PAD_BANK_ABCD) | new_mode);
        // else
        //     RestBanksMask = ((RestBanksMask & ~PAD_BANK_EFGH) | new_mode);
        tklog_debug("     ...Saving new PermanentMode: %02x\n", PermanentMode);
    }

    // Actually draw pads IF WE REALLY CHANGED MODES
    if (new_mode != MPCPadMode)
    {
        MPCPadMode = new_mode;
        for (int c = 0; c < 16; c++)
        {
            DrawMatrixPadFromCache(new_mode, c);
        }
    }
}

void SetForceMatrixButton(uint8_t force_pad_note_number, bool on)
{
    // Set default colors for each button type
    // XXX This should be initialized at startup time
    ForceMPCPadColor_t color_on;
    ForceMPCPadColor_t color_off;
    switch (force_pad_note_number)
    {
    // Blacked-out pads
    case 0x00:
        color_on.r = 0x3F;
        color_on.g = 0x3F;
        color_on.b = 0x3F;
        color_off.r = 0x00;
        color_off.g = 0x00;
        color_off.b = 0x00;
        break;

    // Pads that are vivid orange
    case FORCE_BT_ASSIGN_A:
    case FORCE_BT_MUTE:
    case FORCE_BT_MASTER:
        color_on.r = 0x7F;
        color_on.g = 0x7F;
        color_on.b = 0x00;
        color_off.r = 0x3F;
        color_off.g = 0x3F;
        color_off.b = 0x00;
        break;

    // Pads that are vivid red
    case FORCE_BT_REC_ARM:
    case FORCE_BT_ASSIGN_B:
    case FORCE_BT_STOP_ALL:
        color_on.r = 0x7F;
        color_on.g = 0x00;
        color_on.b = 0x00;
        color_off.r = 0x3F;
        color_off.g = 0x00;
        color_off.b = 0x00;
        break;

    // Blue pads (yeah there are some of them)
    case FORCE_BT_SOLO:
        color_on.r = 0x00;
        color_on.g = 0x00;
        color_on.b = 0x7F;
        color_off.r = 0x00;
        color_off.g = 0x00;
        color_off.b = 0x3F;
        break;

    // Pads that are vivid green
    case FORCE_BT_CLIP_STOP:
    case FORCE_BT_LAUNCH_1:
    case FORCE_BT_LAUNCH_2:
    case FORCE_BT_LAUNCH_3:
    case FORCE_BT_LAUNCH_4:
    case FORCE_BT_LAUNCH_5:
    case FORCE_BT_LAUNCH_6:
    case FORCE_BT_LAUNCH_7:
    case FORCE_BT_LAUNCH_8:
        color_on.r = 0x00;
        color_on.g = 0x7F;
        color_on.b = 0x00;
        color_off.r = 0x00;
        color_off.g = 0x3F;
        color_off.b = 0x00;
        break;

    // Default color is white/gray
    default:
        color_on.r = 0x7F;
        color_on.g = 0x7F;
        color_on.b = 0x7F;
        color_off.r = 0x3F;
        color_off.g = 0x3F;
        color_off.b = 0x3F;
        break;
    }

    // XXX SUBOPTIMAL we look into the whole arrays for the note
    // We don't bother looking into A_A matrices because it's not used for buttons
    // We look into each matrix becaue one button could be present several times!
    for (u_int8_t i = 0; i < 16; i++)
    {
        if (MPCPadToForceF[i] == force_pad_note_number)
        {
            // tklog_debug("SetForceMatrixButton: %02x to value %d\n", force_pad_note_number, on);
            // tklog_debug("   -> found pad %02x in matrix B\n", i);
            if (on)
            {
                MPCPadValuesF[i].r = color_on.r;
                MPCPadValuesF[i].g = color_on.g;
                MPCPadValuesF[i].b = color_on.b;
            }
            else
            {
                MPCPadValuesF[i].r = color_off.r;
                MPCPadValuesF[i].g = color_off.g;
                MPCPadValuesF[i].b = color_off.b;
            }
            if (MPCPadMode == PAD_BANK_F)
                DrawMatrixPadFromCache(PAD_BANK_F, i);
        }
        if (MPCPadToForceG[i] == force_pad_note_number)
        {
            // tklog_debug("SetForceMatrixButton: %02x to value %d\n", force_pad_note_number, on);
            // tklog_debug("   -> found pad %02x in matrix C\n", i);
            if (on)
            {
                MPCPadValuesG[i].r = color_on.r;
                MPCPadValuesG[i].g = color_on.g;
                MPCPadValuesG[i].b = color_on.b;
            }
            else
            {
                MPCPadValuesG[i].r = color_off.r;
                MPCPadValuesG[i].g = color_off.g;
                MPCPadValuesG[i].b = color_off.b;
            }
            if (MPCPadMode == PAD_BANK_G)
                DrawMatrixPadFromCache(PAD_BANK_G, i);
        }
        if (MPCPadToForceH[i] == force_pad_note_number)
        {
            // tklog_debug("SetForceMatrixButton: %02x to value %d\n", force_pad_note_number, on);
            // tklog_debug("   -> found pad %02x in matrix D\n", i);
            if (on)
            {
                MPCPadValuesH[i].r = color_on.r;
                MPCPadValuesH[i].g = color_on.g;
                MPCPadValuesH[i].b = color_on.b;
            }
            else
            {
                MPCPadValuesH[i].r = color_off.r;
                MPCPadValuesH[i].g = color_off.g;
                MPCPadValuesH[i].b = color_off.b;
            }
            if (MPCPadMode == PAD_BANK_H)
                DrawMatrixPadFromCache(PAD_BANK_H, i);
        }
    }

    return;
}

// Cache force pad, and draw it if it's the current matrix
uint8_t CacheForcePad(uint8_t force_pad_number, uint8_t r, uint8_t g, uint8_t b)
{
    // force_pad_number starts from 0.
    // Find which matrix is this pad located in
    tklog_debug("CacheForcePad: %02x %02x %02x %02x\n", force_pad_number, r, g, b);
    uint8_t mpc_bank = ForcePadNumberToMPCBank[force_pad_number];
    uint8_t mpc_pad_number = ForcePadNumberToMPCPadNumber[force_pad_number];
    tklog_debug("   -> mpc_bank: %02x, mpc_pad_number (/16): %02x\n", mpc_bank, mpc_pad_number);

    // Select the proper bank cache.
    // Remember that a control can be on several matrices!
    if (mpc_bank & PAD_BANK_A)
    {
        MPCPadValuesA[mpc_pad_number].r = r;
        MPCPadValuesA[mpc_pad_number].g = g;
        MPCPadValuesA[mpc_pad_number].b = b;
    }
    if (mpc_bank & PAD_BANK_B)
    {
        MPCPadValuesB[mpc_pad_number].r = r;
        MPCPadValuesB[mpc_pad_number].g = g;
        MPCPadValuesB[mpc_pad_number].b = b;
    }
    if (mpc_bank & PAD_BANK_C)
    {
        MPCPadValuesC[mpc_pad_number].r = r;
        MPCPadValuesC[mpc_pad_number].g = g;
        MPCPadValuesC[mpc_pad_number].b = b;
    }
    if (mpc_bank & PAD_BANK_D)
    {
        MPCPadValuesD[mpc_pad_number].r = r;
        MPCPadValuesD[mpc_pad_number].g = g;
        MPCPadValuesD[mpc_pad_number].b = b;
    }
    if (mpc_bank & PAD_BANK_E)
    {
        MPCPadValuesE[mpc_pad_number].r = r;
        MPCPadValuesE[mpc_pad_number].g = g;
        MPCPadValuesE[mpc_pad_number].b = b;
    }
    if (mpc_bank & PAD_BANK_F)
    {
        MPCPadValuesF[mpc_pad_number].r = r;
        MPCPadValuesF[mpc_pad_number].g = g;
        MPCPadValuesF[mpc_pad_number].b = b;
    }
    if (mpc_bank & PAD_BANK_G)
    {
        MPCPadValuesG[mpc_pad_number].r = r;
        MPCPadValuesG[mpc_pad_number].g = g;
        MPCPadValuesG[mpc_pad_number].b = b;
    }
    if (mpc_bank & PAD_BANK_H)
    {
        MPCPadValuesH[mpc_pad_number].r = r;
        MPCPadValuesH[mpc_pad_number].g = g;
        MPCPadValuesH[mpc_pad_number].b = b;
    }

    // Do we HAVE to update the pad?
    // In that case we return the remapped pad number
    if (MPCPadMode == mpc_bank)
    {
        tklog_debug("     => Ask to repaint pad %02x", mpc_pad_number);
        return mpc_pad_number;
        // DrawMatrixPadFromCache(mpc_bank, mpc_pad_number);
    }
    return 0x03; // We light a pad just for testing purposes
}

///////////////////////////////////////////////////////////////////////////////
// Set pad colors
///////////////////////////////////////////////////////////////////////////////
// 2 implementations : call with a 32 bits color int value or with r,g,b values
// Pad number starts from top left (0), 8 pads per line
void SetPadColor(const uint8_t padL, const u_int8_t padC, const uint8_t r, const uint8_t g, const uint8_t b)
{

    uint8_t sysexBuff[128];
    char sysexBuffDebug[128];
    int p = 0;

    // Log event
    tklog_debug("               Set pad color: L=%d C=%d r g b %02X %02X %02X\n", padL, padC, r, g, b);

    // Double-check input data
    if (padL > 3 || padC > 3)
    {
        tklog_error("MPC Pad Line refresh : wrong pad number %d %d\n", padL, padC);
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
    sprintf(sysexBuffDebug + strlen(sysexBuffDebug), "\n");
    tklog_debug("%s", sysexBuffDebug);

    // Send the sysex to the MPC controller
    orig_snd_rawmidi_write(rawvirt_outpriv, sysexBuff, p);
}

void SetPadColorFromColorInt(const uint8_t padL, const u_int8_t padC, const uint32_t rgbColorValue)
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
uint8_t getMPCPadNoteNumber(uint8_t pad_number)
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
        tklog_error("Invalid pad number: %02x", pad_number);
        return 0xFF;
    }
}

///////////////////////////////////////////////////////////////////////////////
// MIDI READ - APP ON MPC READING AS FORCE
// Here we read the MIDI messages from the MPC and we send them to the Force
// That will be mostly button and pad presses!
// It's pretty simple:
// - We remap what's coming from the PADs according to the mode we're on
// - We discard presses from "bank" buttons
///////////////////////////////////////////////////////////////////////////////
size_t Mpc_MapReadFromForce(void *midiBuffer, size_t maxSize, size_t size)
{

    uint8_t *midi_buffer = (uint8_t *)midiBuffer;
    size_t i = 0;
    size_t count = 0;
    typeof(&cb_default) callback;
    MPCControlToForce_t *mpc_to_force_mapping_p;

    while (i < size)
    {

        // AKAI SYSEX ------------------------------------------------------------
        // IDENTITY REQUEST
        if (midi_buffer[i] == 0xF0 && memcmp(&midi_buffer[i], IdentityReplySysexHeader, sizeof(IdentityReplySysexHeader)) == 0)
        {
            // If so, substitue sysex identity request by the faked one
            memcpy(&midi_buffer[i + sizeof(IdentityReplySysexHeader)], DeviceInfoBloc[MPCId].sysexIdReply, sizeof(DeviceInfoBloc[MPCId].sysexIdReply));
            i += sizeof(IdentityReplySysexHeader) + sizeof(DeviceInfoBloc[MPCId].sysexIdReply);
            continue;
        }

        // KNOBS TURN (UNMAPPED BECAUSE ARE ALL EQUIVALENT ON ALL DEVICES) ------
        // If it's a shift + knob turn, add an offset
        //  B0 [10-31] [7F - n]
        else if (midi_buffer[i] == 0xB0)
        {
            if (IAMForceStatus.shift_hold && DeviceInfoBloc[MPCOriginalId].qlinkKnobsCount < 16 && midi_buffer[i + 1] >= 0x10 && midi_buffer[i + 1] <= 0x31)
                midi_buffer[i + 1] += DeviceInfoBloc[MPCOriginalId].qlinkKnobsCount;
            i += 3;
            continue;
        }

        // BUTTONS PRESS / RELEASE (NOT pads) -------------------------------------------
        else if (midi_buffer[i] == 0x90)
        {
            // Apply mapping, call the callback function
            mpc_to_force_mapping_p = &MPCButtonToForce[midi_buffer[i + 1]];
            if (mpc_to_force_mapping_p->callback != NULL)
                i += mpc_to_force_mapping_p->callback(
                    mpc_to_force_mapping_p,
                    NULL,
                    &midiBuffer[i],
                    size - i);

            // Advance to the next message
            continue; // next msg

            // XXX TODO: add qlink management
            // Qlink management is hard coded
            // SHIFT "KNOB TOUCH" button :  add the offset when possible
            // MPC : 90 [54-63] 7F      FORCE : 90 [53-5A] 7F  (no "untouch" exists)
            // if (midi_buffer[i + 1] >= 0x54 && midi_buffer[i + 1] <= 0x63)
            // {
            //     midi_buffer[i + 1]--; // Map to force Qlink touch

            //     if (shiftHoldMode && DeviceInfoBloc[MPCOriginalId].qlinkKnobsCount < 16)
            //         midi_buffer[i + 1] += DeviceInfoBloc[MPCOriginalId].qlinkKnobsCount;

            //     // tklog_debug("Qlink 0x%02x touch\n",midi_buffer[i+1] );
            //     i += 3;
            //     continue; // next msg
            // }
        }

        // PADS ------------------------------------------------------------------
        else if (
            midi_buffer[i] == 0x99    // Note on
            || midi_buffer[i] == 0x89 // Note off
            || midi_buffer[i] == 0xA9 // Aftertouch
        )
        {
            uint8_t pad_number = getMpcPadNumber(midi_buffer[i + 1]);
            mpc_to_force_mapping_p = &MPCPadToForce[IAMForceStatus.pad_layout][pad_number];
            if (mpc_to_force_mapping_p->callback != NULL)
                i += mpc_to_force_mapping_p->callback(
                    mpc_to_force_mapping_p,
                    NULL,
                    &midi_buffer[i],
                    size - i);
        }

        // Advance to the next byte 
        // XXX ...or is it i+=3???
        else
            i += 1;
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
    size_t i = 0;
    size_t callback_i = 0;
    ForceControlToMPC_t *force_to_mpc_mapping_p;

    while (i < size)
    {
        // AKAI SYSEX
        // If we detect the Akai sysex header, change the harwware id by our true hardware id.
        // Messages are compatibles. Some midi msg are not always interpreted (e.g. Oled)
        if (midi_buffer[i] == 0xF0 && memcmp(&midi_buffer[i], AkaiSysex, sizeof(AkaiSysex)) == 0)
        {
            // Update the sysex id in the sysex for our original hardware
            // tklog_debug("Inside Akai Sysex\n");
            i += sizeof(AkaiSysex);
            midi_buffer[i] = DeviceInfoBloc[MPCOriginalId].sysexId;
            i++;

            // SET PAD COLORS SYSEX ------------------------------------------------
            // FN  F0 47 7F [3B] -> 65 00 04 [Pad #] [R] [G] [B] F7
            // Here, "pad #" is 0 for top-right pad, etc.
            if (memcmp(&midi_buffer[i], MPCSysexPadColorFn, sizeof(MPCSysexPadColorFn)) == 0)
            {
                i += sizeof(MPCSysexPadColorFn);

                // Regular Pad
                uint8_t padF = midi_buffer[i];
                // tklog_debug("  [write] Inside Pad Color Sysex for pad %02x / r=%02x g=%02x b=%02x\n", padF,
                //             midi_buffer[i + 1], midi_buffer[i + 2], midi_buffer[i + 3]);

                // Project init! We detect if PAD0 is lit, if so we switch to bank A for good measure
                if (!project_loaded && padF == 0 && (midi_buffer[i + 1] != 0 || midi_buffer[i + 2] != 0 || midi_buffer[i + 3] != 0))
                {
                    tklog_debug("PROJECT INIT!! We go to bank C because we're in matrix mode");
                    {
                        MPCSwitchBankMode(PAD_BANK_C, true);
                    }
                    project_loaded = true;
                }

                // Set matrix pad cache, update if we ought to update
                pad_to_update = CacheForcePad(
                    padF,
                    midi_buffer[i + 1],
                    midi_buffer[i + 2],
                    midi_buffer[i + 3]);

                // We completely change the destination pad
                midi_buffer[i] = pad_to_update;
                i += 5; // Next msg
            }
        }

        // Buttons LEDs. Just remap with the reverse table,
        // iterating on each callback if needed
        else if (midi_buffer[i] == 0xB0)
        {
            force_to_mpc_mapping_p = &ForceControlToMPC[midi_buffer[i + 1]];
            while (force_to_mpc_mapping_p != NULL)
            {
                callback_i = force_to_mpc_mapping_p->callback(
                    NULL,
                    force_to_mpc_mapping_p,
                    &midi_buffer[i],
                    size - i);
                force_to_mpc_mapping_p = force_to_mpc_mapping_p->next_control;
            }
            i += callback_i;        // Only advance once even if we called several callbacks
        }

        else
            i++;
    }
}
