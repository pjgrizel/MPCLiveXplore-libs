
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

#include "mapping.h"
#include "tkgl_mpcmapper.h"

// Buttons and controls Mapping tables
// SHIFT values have bit 7 set
int map_ButtonsLeds[MAPPING_TABLE_SIZE];
int map_ButtonsLeds_Inv[MAPPING_TABLE_SIZE]; // Inverted table

// Force Matrix pads color cache
// static ForceMPCPadColor_t PadSysexColorsCache[256];
// static ForceMPCPadColor_t PadSysexColorsCacheBankB[16];
// static ForceMPCPadColor_t PadSysexColorsCacheBankC[16];
// static ForceMPCPadColor_t PadSysexColorsCacheBankD[16];

// SHIFT Holded mode
// Holding shift will activate the shift mode
static bool shiftHoldMode = false;

// To navigate in matrix when MPC spoofing a Force
static uint8_t MPCPadMode = PAD_BANK_A;
static uint8_t RestBanksMask = PAD_BANK_A | PAD_BANK_F;
static uint8_t LastPressedBankButton = 0;

// FORCE starts from top-left, MPC start from BOTTOM-left
// These matrix are MPC => Force (not the other way around)
static const uint8_t MPCToForceA[] = {
    FORCEPADS_TABLE_IDX_OFFSET + 32, FORCEPADS_TABLE_IDX_OFFSET + 33, FORCEPADS_TABLE_IDX_OFFSET + 34, FORCEPADS_TABLE_IDX_OFFSET + 35,
    FORCEPADS_TABLE_IDX_OFFSET + 40, FORCEPADS_TABLE_IDX_OFFSET + 41, FORCEPADS_TABLE_IDX_OFFSET + 42, FORCEPADS_TABLE_IDX_OFFSET + 43,
    FORCEPADS_TABLE_IDX_OFFSET + 48, FORCEPADS_TABLE_IDX_OFFSET + 49, FORCEPADS_TABLE_IDX_OFFSET + 50, FORCEPADS_TABLE_IDX_OFFSET + 51,
    FORCEPADS_TABLE_IDX_OFFSET + 56, FORCEPADS_TABLE_IDX_OFFSET + 57, FORCEPADS_TABLE_IDX_OFFSET + 58, FORCEPADS_TABLE_IDX_OFFSET + 59};

static const uint8_t MPCToForceB[] = {
    FORCEPADS_TABLE_IDX_OFFSET + 36, FORCEPADS_TABLE_IDX_OFFSET + 37, FORCEPADS_TABLE_IDX_OFFSET + 38, FORCEPADS_TABLE_IDX_OFFSET + 39,
    FORCEPADS_TABLE_IDX_OFFSET + 44, FORCEPADS_TABLE_IDX_OFFSET + 45, FORCEPADS_TABLE_IDX_OFFSET + 46, FORCEPADS_TABLE_IDX_OFFSET + 47,
    FORCEPADS_TABLE_IDX_OFFSET + 52, FORCEPADS_TABLE_IDX_OFFSET + 53, FORCEPADS_TABLE_IDX_OFFSET + 54, FORCEPADS_TABLE_IDX_OFFSET + 55,
    FORCEPADS_TABLE_IDX_OFFSET + 60, FORCEPADS_TABLE_IDX_OFFSET + 61, FORCEPADS_TABLE_IDX_OFFSET + 62, FORCEPADS_TABLE_IDX_OFFSET + 63};

static const uint8_t MPCToForceC[] = {
    FORCEPADS_TABLE_IDX_OFFSET + 0, FORCEPADS_TABLE_IDX_OFFSET + 1, FORCEPADS_TABLE_IDX_OFFSET + 2, FORCEPADS_TABLE_IDX_OFFSET + 3,
    FORCEPADS_TABLE_IDX_OFFSET + 8, FORCEPADS_TABLE_IDX_OFFSET + 9, FORCEPADS_TABLE_IDX_OFFSET + 10, FORCEPADS_TABLE_IDX_OFFSET + 11,
    FORCEPADS_TABLE_IDX_OFFSET + 16, FORCEPADS_TABLE_IDX_OFFSET + 17, FORCEPADS_TABLE_IDX_OFFSET + 18, FORCEPADS_TABLE_IDX_OFFSET + 19,
    FORCEPADS_TABLE_IDX_OFFSET + 24, FORCEPADS_TABLE_IDX_OFFSET + 25, FORCEPADS_TABLE_IDX_OFFSET + 26, FORCEPADS_TABLE_IDX_OFFSET + 27};

static const uint8_t MPCToForceD[] = {
    FORCEPADS_TABLE_IDX_OFFSET + 4, FORCEPADS_TABLE_IDX_OFFSET + 5, FORCEPADS_TABLE_IDX_OFFSET + 6, FORCEPADS_TABLE_IDX_OFFSET + 7,
    FORCEPADS_TABLE_IDX_OFFSET + 12, FORCEPADS_TABLE_IDX_OFFSET + 13, FORCEPADS_TABLE_IDX_OFFSET + 14, FORCEPADS_TABLE_IDX_OFFSET + 15,
    FORCEPADS_TABLE_IDX_OFFSET + 20, FORCEPADS_TABLE_IDX_OFFSET + 21, FORCEPADS_TABLE_IDX_OFFSET + 22, FORCEPADS_TABLE_IDX_OFFSET + 23,
    FORCEPADS_TABLE_IDX_OFFSET + 28, FORCEPADS_TABLE_IDX_OFFSET + 29, FORCEPADS_TABLE_IDX_OFFSET + 30, FORCEPADS_TABLE_IDX_OFFSET + 31};

static const uint8_t MPCToForceF[] = {
    0, FORCE_BT_ASSIGN_A, FORCE_BT_ASSIGN_B, FORCE_BT_MASTER,
    FORCE_BT_MUTE, FORCE_BT_SOLO, FORCE_BT_REC_ARM, FORCE_BT_CLIP_STOP,
    FORCE_BT_MUTE_PAD5, FORCE_BT_MUTE_PAD6, FORCE_BT_MUTE_PAD7, FORCE_BT_MUTE_PAD8,
    FORCE_BT_MUTE_PAD1, FORCE_BT_MUTE_PAD2, FORCE_BT_MUTE_PAD3, FORCE_BT_MUTE_PAD4};

static const uint8_t MPCToForceG[] = {
    0, FORCE_BT_UP, 0, 0,
    FORCE_BT_LEFT, FORCE_BT_DOWN, FORCE_BT_RIGHT, 0,
    FORCE_BT_COLUMN_PAD5, FORCE_BT_COLUMN_PAD6, FORCE_BT_COLUMN_PAD7, FORCE_BT_COLUMN_PAD8,
    FORCE_BT_COLUMN_PAD1, FORCE_BT_COLUMN_PAD2, FORCE_BT_COLUMN_PAD3, FORCE_BT_COLUMN_PAD4};

static const uint8_t MPCToForceH[] = {
    0, FORCE_BT_UP, 0, FORCE_BT_STOP_ALL,
    FORCE_BT_LEFT, FORCE_BT_DOWN, FORCE_BT_RIGHT, 0,
    FORCE_BT_LAUNCH_5, FORCE_BT_LAUNCH_6, FORCE_BT_LAUNCH_7, FORCE_BT_LAUNCH_8,
    FORCE_BT_LAUNCH_1, FORCE_BT_LAUNCH_2, FORCE_BT_LAUNCH_3, FORCE_BT_LAUNCH_4};

// Here we convert the Force pad number to MPC bank.
static const uint8_t ForcePadNumberToMPCBank[] = {
    // Line
    PAD_BANK_C,
    PAD_BANK_C,
    PAD_BANK_C,
    PAD_BANK_C,
    PAD_BANK_D,
    PAD_BANK_D,
    PAD_BANK_D,
    PAD_BANK_D,
    // Line
    PAD_BANK_C,
    PAD_BANK_C,
    PAD_BANK_C,
    PAD_BANK_C,
    PAD_BANK_D,
    PAD_BANK_D,
    PAD_BANK_D,
    PAD_BANK_D,
    // Line
    PAD_BANK_C,
    PAD_BANK_C,
    PAD_BANK_C,
    PAD_BANK_C,
    PAD_BANK_D,
    PAD_BANK_D,
    PAD_BANK_D,
    PAD_BANK_D,
    // Line
    PAD_BANK_C,
    PAD_BANK_C,
    PAD_BANK_C,
    PAD_BANK_C,
    PAD_BANK_D,
    PAD_BANK_D,
    PAD_BANK_D,
    PAD_BANK_D,
    // Line
    PAD_BANK_A,
    PAD_BANK_A,
    PAD_BANK_A,
    PAD_BANK_A,
    PAD_BANK_B,
    PAD_BANK_B,
    PAD_BANK_B,
    PAD_BANK_B,
    // Line
    PAD_BANK_A,
    PAD_BANK_A,
    PAD_BANK_A,
    PAD_BANK_A,
    PAD_BANK_B,
    PAD_BANK_B,
    PAD_BANK_B,
    PAD_BANK_B,
    // Line
    PAD_BANK_A,
    PAD_BANK_A,
    PAD_BANK_A,
    PAD_BANK_A,
    PAD_BANK_B,
    PAD_BANK_B,
    PAD_BANK_B,
    PAD_BANK_B,
    // Line
    PAD_BANK_A,
    PAD_BANK_A,
    PAD_BANK_A,
    PAD_BANK_A,
    PAD_BANK_B,
    PAD_BANK_B,
    PAD_BANK_B,
    PAD_BANK_B,
    // Line 9 (mute modes)
    PAD_BANK_F,
    PAD_BANK_F,
    PAD_BANK_F,
    PAD_BANK_F,
    PAD_BANK_F,
    PAD_BANK_F,
    PAD_BANK_F,
    PAD_BANK_F,
    // Line 10 (track select)
    PAD_BANK_G,
    PAD_BANK_G,
    PAD_BANK_G,
    PAD_BANK_G,
    PAD_BANK_G,
    PAD_BANK_G,
    PAD_BANK_G,
    PAD_BANK_G,
};

// Now for each bank we provide a Force to MPC pad mapping
static const uint8_t ForcePadNumberToMPCPadNumber[] = {
    0, 1, 2, 3, 0, 1, 2, 3,         // First line
    4, 5, 6, 7, 4, 5, 6, 7,         // 2d line
    8, 9, 10, 11, 8, 9, 10, 11,     // 3d line
    12, 13, 14, 15, 12, 13, 14, 15, // 4th line, etc
    0, 1, 2, 3, 0, 1, 2, 3,         // First line
    4, 5, 6, 7, 4, 5, 6, 7,         // 2d line
    8, 9, 10, 11, 8, 9, 10, 11,     // 3d line
    12, 13, 14, 15, 12, 13, 14, 15, // 4th line, etc
    12, 13, 14, 15, 8, 9, 10, 11,   // Line 8 = mute modes
    12, 13, 14, 15, 8, 9, 10, 11    // Line 9 = track selection
};

// These are the matrices where actual RGB values are stored
// They are initialized at start time and populated in the Write function
static ForceMPCPadColor_t MPCPadValuesA[16];
static ForceMPCPadColor_t MPCPadValuesB[16];
static ForceMPCPadColor_t MPCPadValuesC[16];
static ForceMPCPadColor_t MPCPadValuesD[16];
static ForceMPCPadColor_t MPCPadValuesF[16];
static ForceMPCPadColor_t MPCPadValuesG[16];
static ForceMPCPadColor_t MPCPadValuesH[16];

// We keep the TAP value here to allow for a synchronized 'flash' effect
// when battery is charging
static bool TapStatus = false;

// Create a button press timer: we keep track of how long a BANK button was pressed
// and if it was pressed for more than 1 second, we switch to the next bank
static struct timespec started_press;

///////////////////////////////////////////////////////////////////////////////
// (fake) load mapping tables from config file
///////////////////////////////////////////////////////////////////////////////
void LoadMapping()
{
    // Initialize global mapping tables
    for (int i = 0; i < MAPPING_TABLE_SIZE; i++)
    {
        map_ButtonsLeds[i] = -1;
        map_ButtonsLeds_Inv[i] = -1;
    }

    // Hardcoded mapping file (it's 'Force Button => LiveII Button')
    map_ButtonsLeds[LIVEII_BT_ENCODER] = FORCE_BT_ENCODER;
    map_ButtonsLeds[LIVEII_BT_QLINK_SELECT] = FORCE_BT_KNOBS;
    map_ButtonsLeds[LIVEII_BT_SHIFT] = FORCE_BT_SHIFT;
    map_ButtonsLeds[LIVEII_BT_TAP_TEMPO] = FORCE_BT_TAP_TEMPO;
    map_ButtonsLeds[LIVEII_BT_PLUS] = FORCE_BT_PLUS;
    map_ButtonsLeds[LIVEII_BT_MINUS] = FORCE_BT_MINUS;

    // Transport / mode buttons
    map_ButtonsLeds[LIVEII_BT_MENU] = FORCE_BT_MENU;
    map_ButtonsLeds[LIVEII_BT_MAIN] = FORCE_BT_MATRIX;
    map_ButtonsLeds[LIVEII_BT_MIX] = FORCE_BT_MIXER;
    map_ButtonsLeds[LIVEII_BT_MUTE] = FORCE_BT_STEP_SEQ; // Or ARP? Meh.
    map_ButtonsLeds[LIVEII_BT_NEXT_SEQ] = FORCE_BT_LAUNCH;
    map_ButtonsLeds[LIVEII_BT_REC] = FORCE_BT_REC;
    map_ButtonsLeds[LIVEII_BT_OVERDUB] = FORCE_BT_CLIP;
    map_ButtonsLeds[LIVEII_BT_STOP] = FORCE_BT_STOP;
    map_ButtonsLeds[LIVEII_BT_PLAY] = FORCE_BT_PLAY;
    map_ButtonsLeds[LIVEII_BT_PLAY_START] = FORCE_BT_NOTE;

    // Edition buttons
    map_ButtonsLeds[LIVEII_BT_NOTE_REPEAT] = FORCE_BT_SELECT;
    map_ButtonsLeds[LIVEII_BT_FULL_LEVEL] = FORCE_BT_EDIT;
    map_ButtonsLeds[LIVEII_BT_16_LEVEL] = FORCE_BT_COPY;
    map_ButtonsLeds[LIVEII_BT_ERASE] = FORCE_BT_DELETE;

    // Quadrant zone
    map_ButtonsLeds[LIVEII_BT_UNDO] = FORCE_BT_UNDO;
    map_ButtonsLeds[LIVEII_BT_TC] = FORCE_BT_LOAD;
    map_ButtonsLeds[LIVEII_BT_COPY] = FORCE_BT_SAVE;
    map_ButtonsLeds[LIVEII_BT_STEP_SEQ] = FORCE_BT_NAVIGATE;

    // Construct the inverted mapping table
    for (int i = 0; i < MAPPING_TABLE_SIZE; i++)
    {
        if (map_ButtonsLeds[i] >= 0)
            map_ButtonsLeds_Inv[map_ButtonsLeds[i]] = i;
    }

    // Initialize all pads caches (at load-time, we consider they are black)
    // for (int i = 0; i < 256; i++)
    // {
    //     PadSysexColorsCache[i].r = 0x00;
    //     PadSysexColorsCache[i].g = 0x7f;
    //     PadSysexColorsCache[i].b = 0x00;
    // }
    for (int i = 0; i < 16; i++)
    {
        MPCPadValuesA[i].r = 0x00;
        MPCPadValuesA[i].g = 0x00;
        MPCPadValuesA[i].b = 0x00;
        MPCPadValuesB[i].r = 0x00;
        MPCPadValuesB[i].g = 0x00;
        MPCPadValuesB[i].b = 0x00;
        MPCPadValuesC[i].r = 0x00;
        MPCPadValuesC[i].g = 0x00;
        MPCPadValuesC[i].b = 0x00;
        MPCPadValuesD[i].r = 0x00;
        MPCPadValuesD[i].g = 0x00;
        MPCPadValuesH[i].b = 0x00;
        MPCPadValuesF[i].r = 0x00;
        MPCPadValuesF[i].g = 0x00;
        MPCPadValuesF[i].b = 0x00;
        MPCPadValuesG[i].r = 0x00;
        MPCPadValuesG[i].g = 0x00;
        MPCPadValuesG[i].b = 0x00;
        MPCPadValuesH[i].r = 0x00;
        MPCPadValuesH[i].g = 0x00;
        MPCPadValuesH[i].b = 0x00;
    }

    // Initialize timer
    clock_gettime(CLOCK_MONOTONIC_RAW, &started_press);
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

void DrawMatrixPadFromCache(uint8_t matrix, uint8_t pad_number)
{
    // tklog_debug("DrawMatrixPadFromCache(matrix=%02x, pad_number=%02x)\n", matrix, pad_number);

    // Get pad coordinates
    uint8_t padL = pad_number / 4;
    uint8_t padC = pad_number % 4;

    // Do we *HAVE* to color this pad?
    if (matrix != MPCPadMode)
    {
        tklog_debug("  ...We ignore repainting message for matrix %02x, pad number %02x\n", matrix, pad_number);
        return;
    }

    // Find the pad color as it's stored
    switch (matrix)
    {
    case PAD_BANK_A:
        SetPadColor(padL, padC,
                    MPCPadValuesA[pad_number].r,
                    MPCPadValuesA[pad_number].g,
                    MPCPadValuesA[pad_number].b);
        break;
    case PAD_BANK_B:
        SetPadColor(padL, padC,
                    MPCPadValuesB[pad_number].r,
                    MPCPadValuesB[pad_number].g,
                    MPCPadValuesB[pad_number].b);
        break;
    case PAD_BANK_C:
        SetPadColor(padL, padC,
                    MPCPadValuesC[pad_number].r,
                    MPCPadValuesC[pad_number].g,
                    MPCPadValuesC[pad_number].b);
        break;
    case PAD_BANK_D:
        SetPadColor(padL, padC,
                    MPCPadValuesD[pad_number].r,
                    MPCPadValuesD[pad_number].g,
                    MPCPadValuesD[pad_number].b);
        break;
    case PAD_BANK_F:
        SetPadColor(padL, padC,
                    MPCPadValuesF[pad_number].r,
                    MPCPadValuesF[pad_number].g,
                    MPCPadValuesF[pad_number].b);
        break;
    case PAD_BANK_G:
        SetPadColor(padL, padC,
                    MPCPadValuesG[pad_number].r,
                    MPCPadValuesG[pad_number].g,
                    MPCPadValuesG[pad_number].b);
        break;
    case PAD_BANK_H:
        SetPadColor(padL, padC,
                    MPCPadValuesH[pad_number].r,
                    MPCPadValuesH[pad_number].g,
                    MPCPadValuesH[pad_number].b);
        break;
    }
}

// //////////////////////////////////////////////////////////////////
// Bank buttons management
// //////////////////////////////////////////////////////////////////
void MPCSwitchBankMode(uint8_t bank_button, bool pressed)
{
    struct timespec now;
    uint8_t asked_bank_mask;
    uint64_t press_duration;
    bool is_click = false;        // press -> release in less than 0.5s
    bool is_double_click = false; // press -> release -> press -> release in less than 0.5s
    uint8_t current_bank_layer_mask = (MPCPadMode & 0x0f) ? PAD_BANK_ABCD : PAD_BANK_EFGH;

    // Convert bank_button to bank_pad
    switch (bank_button)
    {
    case LIVEII_BT_BANK_A:
        asked_bank_mask = PAD_BANK_A | PAD_BANK_E;
        break;
    case LIVEII_BT_BANK_B:
        asked_bank_mask = PAD_BANK_B | PAD_BANK_F;
        break;
    case LIVEII_BT_BANK_C:
        asked_bank_mask = PAD_BANK_C | PAD_BANK_G;
        break;
    case LIVEII_BT_BANK_D:
        asked_bank_mask = PAD_BANK_D | PAD_BANK_H;
        break;
    }

    // Handle click / hold / doubleclick stuff
    clock_gettime(CLOCK_MONOTONIC_RAW, &now);
    if (LastPressedBankButton == bank_button)
    {
        press_duration = (now.tv_sec - started_press.tv_sec) * 1000 + (now.tv_nsec - started_press.tv_nsec) / 1000000;
        if (pressed)
        {
            if (press_duration < DOUBLE_CLICK_DELAY)
            {
                is_double_click = true;
                started_press.tv_sec = 0; // Avoid mixing double clicks and click at release
            }
        }
        else
        {
            if (press_duration < HOLD_DELAY)
                is_click = true;
        }
    }
    LastPressedBankButton = bank_button;
    if (pressed)
        clock_gettime(CLOCK_MONOTONIC_RAW, &started_press);

    tklog_debug("MPCSwitchBankMode(bank_button=%02x, pressed=%d) => press_duration=%lld, is_click=%d, is_double_click=%d, shift=%d, current bank mask=%02x, rest mask=%02x\n",
                bank_button, pressed, press_duration, is_click, is_double_click, shiftHoldMode, current_bank_layer_mask, RestBanksMask);


    // Shifted => we switch to the OTHER layer permanently when we press
    // if (shiftHoldMode)
    // {
    //     if (pressed)
    //     {
    //         if (bank_button == PAD_BANK_A)
    //         {
    //             // XXX Use this to toggle the 'momentary' mode bank?
    //         }
    //         else
    //             MPCSwitchMatrix(asked_bank_mask & ~current_bank_layer_mask, PAD_BANK_PERMANENTLY);
    //     }
    //     else
    //         // We already were in the "momentary hold" so we just confirm it here
    //         MPCSwitchMatrix(asked_bank_mask & current_bank_layer_mask, PAD_BANK_PERMANENTLY);
    // }
    // Click => we switch to the A/B/C/D layer permanently
    // The tricky part: when receiving the click we are in the OPPOSITE layer!
    if (is_double_click)
    {
        MPCSwitchMatrix(asked_bank_mask & current_bank_layer_mask, PAD_BANK_PERMANENTLY);
    }
    else if (is_click)
    {
        // if (bank_button == PAD_BANK_A && current_bank_layer_mask == PAD_BANK_EFGH)
        //     MPCSwitchMatrix(RestBanksMask & PAD_BANK_ABCD, PAD_BANK_PERMANENTLY);
        MPCSwitchMatrix(asked_bank_mask & ~current_bank_layer_mask, PAD_BANK_PERMANENTLY);
    }
    // Hold modes
    else if (pressed)
    {
        // Press/hold => we switch to the OTHER layer momentary (except for bank A)
        // if (bank_button == PAD_BANK_A && current_bank_layer_mask == PAD_BANK_EFGH)
        //     MPCSwitchMatrix(RestBanksMask & PAD_BANK_ABCD, PAD_BANK_MOMENTARY);
        MPCSwitchMatrix(asked_bank_mask & ~current_bank_layer_mask, PAD_BANK_MOMENTARY);
    }
    // Release => we switch back to the OTHER layer (where we came from)
    else
    {
        MPCSwitchMatrix(asked_bank_mask & ~current_bank_layer_mask, PAD_BANK_PERMANENTLY);
    }
}

////////
// Completely redraw the pads according to the mode we're in.
// Also take care of the "PAD BANK" button according to the proper mode.
// If 'permanently' is True, it will replace the 'RestBankMode'.
////////
void MPCSwitchMatrix(uint8_t new_mode, bool permanently)
{
    // Debug + handle special case (0x10 is conveniently remapped to whatever current layer of 0x0F is)
    tklog_debug("     ...Switching to mode %02x (from current mode: %02x), permanent=%d\n", new_mode, MPCPadMode, permanently);
    if (new_mode == 0x10)
    {
        if (permanently)
            new_mode = 0x01;
        else
            new_mode = RestBanksMask & 0x0f;
        tklog_debug("     OOOPS: actually switching to %02x\n", new_mode);
    }

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
        break;
    case PAD_BANK_B:
        bt_bank_a[2] = BUTTON_COLOR_RED_LIGHT;
        bt_bank_b[2] = BUTTON_COLOR_RED;
        break;
    case PAD_BANK_C:
        bt_bank_a[2] = BUTTON_COLOR_RED_LIGHT;
        bt_bank_c[2] = BUTTON_COLOR_RED;
        break;
    case PAD_BANK_D:
        bt_bank_a[2] = BUTTON_COLOR_RED_LIGHT;
        bt_bank_d[2] = BUTTON_COLOR_RED;
        break;
    case PAD_BANK_F:
        bt_bank_a[2] = BUTTON_COLOR_YELLOW;
        bt_bank_b[2] = BUTTON_COLOR_YELLOW;
        break;
    case PAD_BANK_G:
        bt_bank_a[2] = BUTTON_COLOR_YELLOW;
        bt_bank_c[2] = BUTTON_COLOR_YELLOW;
        break;
    case PAD_BANK_H:
        bt_bank_a[2] = BUTTON_COLOR_YELLOW;
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
        if (new_mode & PAD_BANK_ABCD)
            RestBanksMask = ((RestBanksMask & ~PAD_BANK_ABCD) | new_mode);
        else
            RestBanksMask = ((RestBanksMask & ~PAD_BANK_EFGH) | new_mode);
        tklog_debug("     ...Saving new RestBanksMask: %02x\n", RestBanksMask);
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
        if (MPCToForceF[i] == force_pad_note_number)
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
        if (MPCToForceG[i] == force_pad_note_number)
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
        if (MPCToForceH[i] == force_pad_note_number)
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

void CacheForcePad(uint8_t force_pad_number, uint8_t r, uint8_t g, uint8_t b)
{
    // force_pad_number starts from 0.
    // Find which matrix is this pad located in
    // tklog_debug("CacheForcePad: %02x %02x %02x %02x\n", force_pad_number, r, g, b);
    uint8_t mpc_bank = ForcePadNumberToMPCBank[force_pad_number];
    uint8_t mpc_pad_number = ForcePadNumberToMPCPadNumber[force_pad_number];
    // tklog_debug("   -> mpc_bank: %02x, mpc_pad_number (/16): %02x\n", mpc_bank, mpc_pad_number);

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
    // In that case we just propagate the message
    if (MPCPadMode == mpc_bank)
    {
        DrawMatrixPadFromCache(mpc_bank, mpc_pad_number);
    }
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
    // tklog_debug("Set pad color : L=%d C=%d r g b %02X %02X %02X\n", padL, padC, r, g, b);

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
    // tklog_debug("%s", sysexBuffDebug);

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
uint8_t getMpcPadNumber(uint8_t note_number)
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

    uint8_t *myBuff = (uint8_t *)midiBuffer;
    // uint8_t note_number;
    size_t i = 0;
    while (i < size)
    {

        // AKAI SYSEX ------------------------------------------------------------
        // IDENTITY REQUEST
        if (myBuff[i] == 0xF0 && memcmp(&myBuff[i], IdentityReplySysexHeader, sizeof(IdentityReplySysexHeader)) == 0)
        {
            // If so, substitue sysex identity request by the faked one
            memcpy(&myBuff[i + sizeof(IdentityReplySysexHeader)], DeviceInfoBloc[MPCId].sysexIdReply, sizeof(DeviceInfoBloc[MPCId].sysexIdReply));
            i += sizeof(IdentityReplySysexHeader) + sizeof(DeviceInfoBloc[MPCId].sysexIdReply);
            continue;
        }

        // KNOBS TURN (UNMAPPED BECAUSE ARE ALL EQUIVALENT ON ALL DEVICES) ------
        // If it's a shift + knob turn, add an offset
        //  B0 [10-31] [7F - n]
        if (myBuff[i] == 0xB0)
        {
            if (shiftHoldMode && DeviceInfoBloc[MPCOriginalId].qlinkKnobsCount < 16 && myBuff[i + 1] >= 0x10 && myBuff[i + 1] <= 0x31)
                myBuff[i + 1] += DeviceInfoBloc[MPCOriginalId].qlinkKnobsCount;
            i += 3;
            continue;
        }

        // BUTTONS PRESS / RELEASE------------------------------------------------
        if (myBuff[i] == 0x90)
        {
            tklog_debug("Button 0x%02x %s\n", myBuff[i + 1], (myBuff[i + 2] == 0x7F ? "pressed" : "released"));

            // SHIFT pressed/released (nb the SHIFT button can't be mapped)
            // Double click on SHIFT is not managed at all. Avoid it.
            // NOTA: SHIFT IS NOW ONLY USED FOR QLINK KNOBS
            if (myBuff[i + 1] == SHIFT_KEY_VALUE)
            {
                // We keep track of the shift mode, but we propagate the shift event to the Force
                shiftHoldMode = (myBuff[i + 2] == 0x7F ? true : false);
            }

            // Select bank mode.
            // Here we use BankA button as a pseudo-shift.
            // In any case if we're manipulating BANK buttons, we kill the event.
            switch (myBuff[i + 1])
            {
            case LIVEII_BT_BANK_A:
            case LIVEII_BT_BANK_B:
            case LIVEII_BT_BANK_C:
            case LIVEII_BT_BANK_D:
                // Consider switching modes
                MPCSwitchBankMode(myBuff[i + 1], myBuff[i + 2] == 0x7F ? true : false);

                // Kill the message, ignore the rest
                PrepareFakeMidiMsg(&myBuff[i]);
                i += 3;
                continue;
            }

            // Qlink management is hard coded
            // SHIFT "KNOB TOUCH" button :  add the offset when possible
            // MPC : 90 [54-63] 7F      FORCE : 90 [53-5A] 7F  (no "untouch" exists)
            if (myBuff[i + 1] >= 0x54 && myBuff[i + 1] <= 0x63)
            {
                myBuff[i + 1]--; // Map to force Qlink touch

                if (shiftHoldMode && DeviceInfoBloc[MPCOriginalId].qlinkKnobsCount < 16)
                    myBuff[i + 1] += DeviceInfoBloc[MPCOriginalId].qlinkKnobsCount;

                // tklog_debug("Qlink 0x%02x touch\n",myBuff[i+1] );
                i += 3;
                continue; // next msg
            }

            // We remap it! Start with simple buttons mapping
            // tklog_debug("Mapping for %d = %d - shift = %d \n", myBuff[i+1], map_ButtonsLeds[ myBuff[i+1] ],map_ButtonsLeds[ myBuff[i+1] +  0x80 ] );
            // int mapValue = map_ButtonsLeds[myBuff[i + 1] + (shiftHoldMode ? 0x80 : 0)];
            int mapValue = map_ButtonsLeds[myBuff[i + 1]];

            // No mapping. Next msg
            if (mapValue < 0)
            {
                PrepareFakeMidiMsg(&myBuff[i]);
                i += 3;
                // tklog_debug("No mapping found for 0x%02x \n",myBuff[i+1]);

                continue; // next msg
            }

            // Remap the key
            myBuff[i + 1] = mapValue;
            i += 3;
            continue; // next msg
        }

        // PADS ------------------------------------------------------------------
        if (
            myBuff[i] == 0x99    // Note on
            || myBuff[i] == 0x89 // Note off
            || myBuff[i] == 0xA9 // Aftertouch
        )
        {
            // For the pads...
            // Either we're in banks A_x, in which case we just remap the pad number
            // Or we're in banks B, C, D, in which case we remap the message completely!
            int pad_number = getMpcPadNumber(myBuff[i + 1]);
            tklog_debug("Pad number is %d \n", pad_number);
            switch (MPCPadMode)
            {
            case PAD_BANK_A:
                myBuff[i + 1] = MPCToForceA[pad_number];
                break;
            case PAD_BANK_B:
                myBuff[i + 1] = MPCToForceB[pad_number];
                break;
            case PAD_BANK_C:
                myBuff[i + 1] = MPCToForceC[pad_number];
                break;
            case PAD_BANK_D:
                myBuff[i + 1] = MPCToForceD[pad_number];
                break;

            default:
                // We swallow Aftertouch messages
                if (myBuff[i] == 0xA9)
                {
                    PrepareFakeMidiMsg(&myBuff[i]);
                    i += 3;
                    continue; // next msg
                }

                // Convert pad presses into button presses
                // Convert myBuff[i+1] to pad number 0-15
                // If we have a pad number, we can remap it, otherwise we just ignore the message
                // myBuff[i] == 0x90;
                switch (MPCPadMode)
                {
                case PAD_BANK_F:
                    tklog_debug("...converting input %02x %02x %02x ...\n",
                                myBuff[i], myBuff[i + 1], myBuff[i + 2]);
                    myBuff[i + 2] = myBuff[i] == 0x99 ? 0x7F : 0x00;
                    myBuff[i + 1] = MPCToForceF[pad_number];
                    myBuff[i] = 0x90;
                    tklog_debug("......to %02x %02x %02x\n",
                                myBuff[i], myBuff[i + 1], myBuff[i + 2]);
                    break;
                case PAD_BANK_G:
                    myBuff[i + 2] = myBuff[i] == 0x99 ? 0x7F : 0x00;
                    myBuff[i + 1] = MPCToForceG[pad_number];
                    myBuff[i] = 0x90;
                    break;
                case PAD_BANK_H:
                    myBuff[i + 2] = myBuff[i] == 0x99 ? 0x7F : 0x00;
                    myBuff[i + 1] = MPCToForceH[pad_number];
                    myBuff[i] = 0x90;
                    break;
                }
            }

            // // Propagate message
            // PrepareFakeMidiMsg(&myBuff[i]);
        }
        i += 3;
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
    uint8_t *myBuff = (uint8_t *)midiBuffer;
    size_t i = 0;
    while (i < size)
    {
        // AKAI SYSEX
        // If we detect the Akai sysex header, change the harwware id by our true hardware id.
        // Messages are compatibles. Some midi msg are not always interpreted (e.g. Oled)
        if (myBuff[i] == 0xF0 && memcmp(&myBuff[i], AkaiSysex, sizeof(AkaiSysex)) == 0)
        {
            // Update the sysex id in the sysex for our original hardware
            // tklog_debug("Inside Akai Sysex\n");
            i += sizeof(AkaiSysex);
            myBuff[i] = DeviceInfoBloc[MPCOriginalId].sysexId;
            i++;

            // SET PAD COLORS SYSEX ------------------------------------------------
            // FN  F0 47 7F [3B] -> 65 00 04 [Pad #] [R] [G] [B] F7
            // Here, "pad #" is 0 for top-right pad, etc.
            if (memcmp(&myBuff[i], MPCSysexPadColorFn, sizeof(MPCSysexPadColorFn)) == 0)
            {
                i += sizeof(MPCSysexPadColorFn);

                // Regular Pad
                uint8_t padF = myBuff[i];
                // tklog_debug("  [write] Inside Pad Color Sysex for pad %02x\n", padF);
                // uint8_t padL = padF / 8;
                // uint8_t padC = padF % 8;
                // uint8_t padM = 0x7F;

                // Update Force pad color cache
                // XXX Those lines below are completely wrong, I should transpose first!
                // XXX => use this Transpose Force pad to Mpc pad in the 4x4 current quadran
                // if (padL >= MPCPad_OffsetL && padL < MPCPad_OffsetL + 4)
                // {
                //     if (padC >= MPCPad_OffsetC && padC < MPCPad_OffsetC + 4)
                //     {
                //         padM = (3 - (padL - MPCPad_OffsetL)) * 4 + (padC - MPCPad_OffsetC);
                //     }
                // }

                // Set matrix pad cache, update if we ought to update
                CacheForcePad(
                    padF,
                    myBuff[i + 1],
                    myBuff[i + 2],
                    myBuff[i + 3]);

                // Update the pad# in the midi buffer
                // XXX Why would I do that?
                // XXX We give a random pad number and voilà
                // myBuff[i] = padM;
                myBuff[i] = 0x7f;

                i += 5; // Next msg
            }
        }

        // Buttons-Leds.  In that direction, it's a LED ON / OFF for the button
        // AND/OR an additional PAD sysex!
        // Check if we must remap...
        else if (myBuff[i] == 0xB0)
        {
            // if (myBuff[i + 1] != 0x35)
                // tklog_debug("App wants to write to the Force button %02x value %02x...\n", myBuff[i + 1], myBuff[i + 2]);

            // Very specific TAP button treatment
            if (myBuff[i + 1] == FORCE_BT_TAP_TEMPO)
            {
                TapStatus = myBuff[i + 2] == BUTTON_COLOR_RED_LIGHT ? false : true;
                displayBatteryStatus();
            }

            // Simple remapping
            if (map_ButtonsLeds_Inv[myBuff[i + 1]] >= 0)
            {
                // tklog_debug("MAP INV %d->%d\n",myBuff[i+1],map_ButtonsLeds_Inv[ myBuff[i+1] ]);
                myBuff[i + 1] = map_ButtonsLeds_Inv[myBuff[i + 1]];
                if (myBuff[i + 1] != 0x35)
                    tklog_debug("...remapped to button %02x value %02x...\n", myBuff[i + 1], myBuff[i + 2]);
            }
            else
            {
                // Complex remapping (from Force *NOTE NUMBER* to pad)
                tklog_debug("...complex remapping of button %02x value %02x...\n", myBuff[i + 1], myBuff[i + 2]);
                if (myBuff[i + 2] == 0x7F)
                    SetForceMatrixButton(myBuff[i + 1], true);
                else
                    SetForceMatrixButton(myBuff[i + 1], false);
            }

            // Next message
            i += 3;
        }

        else
            i++;
    }
}

// Lightbulb, anyone?
// This is triggered every time we update the "TAP" button!
void displayBatteryStatus()
{
    // We start by opening the capacity file
    int fd;
    int battery_;
    bool is_charging = false;
    uint8_t blink = 0;
    uint8_t other_leds = 0x01;
    char buffer[16];
    char intBuffer[4];
    ssize_t bytesRead;
    uint8_t scale;

    // Read battery_ value from fp capacity
    // Convert the string to an integer
    fd = orig_open64(POWER_SUPPLY_CAPACITY_PATH, O_RDONLY);
    bytesRead = read(fd, buffer, 6);
    orig_close(fd);
    memcpy(intBuffer, buffer, 3);
    intBuffer[bytesRead] = '\0';
    battery_ = atoi(intBuffer);
    scale = battery_ * 8 / 100;

    // Read battery status from POWER_SUPPLY_STATUS_PATH
    // If it's "Charging", we display a charging icon (kind of)
    fd = orig_open64(POWER_SUPPLY_STATUS_PATH, O_RDONLY);
    bytesRead = read(fd, buffer, 9);
    orig_close(fd);
    buffer[bytesRead] = '\0';
    if (strcmp(buffer, "Charging\n") == 0)
    {
        is_charging = true;
        other_leds = 0x02;
    }
    else if (strcmp(buffer, "Full\n") == 0)
    {
        is_charging = false;
        other_leds = 0x02;
    }

    // If battery is currently charging, we make the last light blink
    // by decreasing the scale by 1
    if (is_charging && !TapStatus && scale > 0)
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
    orig_snd_rawmidi_write(rawvirt_outpriv, light_1, sizeof(light_1));
    orig_snd_rawmidi_write(rawvirt_outpriv, light_2, sizeof(light_2));
    orig_snd_rawmidi_write(rawvirt_outpriv, light_3, sizeof(light_3));
    orig_snd_rawmidi_write(rawvirt_outpriv, light_4, sizeof(light_4));
}
