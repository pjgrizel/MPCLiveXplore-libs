
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
static bool bankAHoldMode = false;

// To navigate in matrix when MPC spoofing a Force
static int MPCPadMode = PAD_BANK_A_A;

// FORCE starts from top-left, MPC start from BOTTOM-left
// These matrix are MPC => Force (not the other way around)
static const uint8_t MPCToForceA_A[] = {
    FORCEPADS_TABLE_IDX_OFFSET + 32, FORCEPADS_TABLE_IDX_OFFSET + 33, FORCEPADS_TABLE_IDX_OFFSET + 34, FORCEPADS_TABLE_IDX_OFFSET + 35,
    FORCEPADS_TABLE_IDX_OFFSET + 40, FORCEPADS_TABLE_IDX_OFFSET + 41, FORCEPADS_TABLE_IDX_OFFSET + 42, FORCEPADS_TABLE_IDX_OFFSET + 43,
    FORCEPADS_TABLE_IDX_OFFSET + 48, FORCEPADS_TABLE_IDX_OFFSET + 49, FORCEPADS_TABLE_IDX_OFFSET + 50, FORCEPADS_TABLE_IDX_OFFSET + 51,
    FORCEPADS_TABLE_IDX_OFFSET + 56, FORCEPADS_TABLE_IDX_OFFSET + 57, FORCEPADS_TABLE_IDX_OFFSET + 58, FORCEPADS_TABLE_IDX_OFFSET + 59};

static const uint8_t MPCToForceA_B[] = {
    FORCEPADS_TABLE_IDX_OFFSET + 36, FORCEPADS_TABLE_IDX_OFFSET + 37, FORCEPADS_TABLE_IDX_OFFSET + 38, FORCEPADS_TABLE_IDX_OFFSET + 39,
    FORCEPADS_TABLE_IDX_OFFSET + 44, FORCEPADS_TABLE_IDX_OFFSET + 45, FORCEPADS_TABLE_IDX_OFFSET + 46, FORCEPADS_TABLE_IDX_OFFSET + 47,
    FORCEPADS_TABLE_IDX_OFFSET + 52, FORCEPADS_TABLE_IDX_OFFSET + 53, FORCEPADS_TABLE_IDX_OFFSET + 54, FORCEPADS_TABLE_IDX_OFFSET + 55,
    FORCEPADS_TABLE_IDX_OFFSET + 60, FORCEPADS_TABLE_IDX_OFFSET + 61, FORCEPADS_TABLE_IDX_OFFSET + 62, FORCEPADS_TABLE_IDX_OFFSET + 63};

static const uint8_t MPCToForceA_C[] = {
    FORCEPADS_TABLE_IDX_OFFSET + 0, FORCEPADS_TABLE_IDX_OFFSET + 1, FORCEPADS_TABLE_IDX_OFFSET + 2, FORCEPADS_TABLE_IDX_OFFSET + 3,
    FORCEPADS_TABLE_IDX_OFFSET + 8, FORCEPADS_TABLE_IDX_OFFSET + 9, FORCEPADS_TABLE_IDX_OFFSET + 10, FORCEPADS_TABLE_IDX_OFFSET + 11,
    FORCEPADS_TABLE_IDX_OFFSET + 16, FORCEPADS_TABLE_IDX_OFFSET + 17, FORCEPADS_TABLE_IDX_OFFSET + 18, FORCEPADS_TABLE_IDX_OFFSET + 19,
    FORCEPADS_TABLE_IDX_OFFSET + 24, FORCEPADS_TABLE_IDX_OFFSET + 25, FORCEPADS_TABLE_IDX_OFFSET + 26, FORCEPADS_TABLE_IDX_OFFSET + 27};

static const uint8_t MPCToForceA_D[] = {
    FORCEPADS_TABLE_IDX_OFFSET + 4, FORCEPADS_TABLE_IDX_OFFSET + 5, FORCEPADS_TABLE_IDX_OFFSET + 6, FORCEPADS_TABLE_IDX_OFFSET + 7,
    FORCEPADS_TABLE_IDX_OFFSET + 12, FORCEPADS_TABLE_IDX_OFFSET + 13, FORCEPADS_TABLE_IDX_OFFSET + 14, FORCEPADS_TABLE_IDX_OFFSET + 15,
    FORCEPADS_TABLE_IDX_OFFSET + 20, FORCEPADS_TABLE_IDX_OFFSET + 21, FORCEPADS_TABLE_IDX_OFFSET + 22, FORCEPADS_TABLE_IDX_OFFSET + 23,
    FORCEPADS_TABLE_IDX_OFFSET + 28, FORCEPADS_TABLE_IDX_OFFSET + 29, FORCEPADS_TABLE_IDX_OFFSET + 30, FORCEPADS_TABLE_IDX_OFFSET + 31};

static const uint8_t MPCToForceB[] = {
    0, FORCE_BT_ASSIGN_A, FORCE_BT_ASSIGN_B, FORCE_BT_MASTER,
    FORCE_BT_MUTE, FORCE_BT_SOLO, FORCE_BT_REC_ARM, FORCE_BT_CLIP_STOP,
    FORCE_BT_MUTE_PAD5, FORCE_BT_MUTE_PAD6, FORCE_BT_MUTE_PAD7, FORCE_BT_MUTE_PAD8,
    FORCE_BT_MUTE_PAD1, FORCE_BT_MUTE_PAD2, FORCE_BT_MUTE_PAD3, FORCE_BT_MUTE_PAD4};

static const uint8_t MPCToForceC[] = {
    0, FORCE_BT_UP, 0, 0,
    FORCE_BT_LEFT, FORCE_BT_DOWN, FORCE_BT_RIGHT, 0,
    FORCE_BT_COLUMN_PAD5, FORCE_BT_COLUMN_PAD6, FORCE_BT_COLUMN_PAD7, FORCE_BT_COLUMN_PAD8,
    FORCE_BT_COLUMN_PAD1, FORCE_BT_COLUMN_PAD2, FORCE_BT_COLUMN_PAD3, FORCE_BT_COLUMN_PAD4};

static const uint8_t MPCToForceD[] = {
    0, FORCE_BT_UP, 0, FORCE_BT_STOP_ALL,
    FORCE_BT_LEFT, FORCE_BT_DOWN, FORCE_BT_RIGHT, 0,
    FORCE_BT_LAUNCH_5, FORCE_BT_LAUNCH_6, FORCE_BT_LAUNCH_7, FORCE_BT_LAUNCH_8,
    FORCE_BT_LAUNCH_1, FORCE_BT_LAUNCH_2, FORCE_BT_LAUNCH_3, FORCE_BT_LAUNCH_4};

// Here we convert the Force pad number to MPC bank.
static const uint8_t ForcePadNumberToMPCBank[] = {
    // Line 0
    PAD_BANK_A_A,
    PAD_BANK_A_A,
    PAD_BANK_A_A,
    PAD_BANK_A_A,
    PAD_BANK_A_B,
    PAD_BANK_A_B,
    PAD_BANK_A_B,
    PAD_BANK_A_B,
    // Line 1
    PAD_BANK_A_A,
    PAD_BANK_A_A,
    PAD_BANK_A_A,
    PAD_BANK_A_A,
    PAD_BANK_A_B,
    PAD_BANK_A_B,
    PAD_BANK_A_B,
    PAD_BANK_A_B,
    // Line 2
    PAD_BANK_A_A,
    PAD_BANK_A_A,
    PAD_BANK_A_A,
    PAD_BANK_A_A,
    PAD_BANK_A_B,
    PAD_BANK_A_B,
    PAD_BANK_A_B,
    PAD_BANK_A_B,
    // Line 3
    PAD_BANK_A_A,
    PAD_BANK_A_A,
    PAD_BANK_A_A,
    PAD_BANK_A_A,
    PAD_BANK_A_B,
    PAD_BANK_A_B,
    PAD_BANK_A_B,
    PAD_BANK_A_B,
    // Line 4
    PAD_BANK_A_C,
    PAD_BANK_A_C,
    PAD_BANK_A_C,
    PAD_BANK_A_C,
    PAD_BANK_A_D,
    PAD_BANK_A_D,
    PAD_BANK_A_D,
    PAD_BANK_A_D,
    // Line 5
    PAD_BANK_A_C,
    PAD_BANK_A_C,
    PAD_BANK_A_C,
    PAD_BANK_A_C,
    PAD_BANK_A_D,
    PAD_BANK_A_D,
    PAD_BANK_A_D,
    PAD_BANK_A_D,
    // Line 6
    PAD_BANK_A_C,
    PAD_BANK_A_C,
    PAD_BANK_A_C,
    PAD_BANK_A_C,
    PAD_BANK_A_D,
    PAD_BANK_A_D,
    PAD_BANK_A_D,
    PAD_BANK_A_D,
    // Line 7
    PAD_BANK_A_C,
    PAD_BANK_A_C,
    PAD_BANK_A_C,
    PAD_BANK_A_C,
    PAD_BANK_A_D,
    PAD_BANK_A_D,
    PAD_BANK_A_D,
    PAD_BANK_A_D,
    // Line 9 (mute modes)
    PAD_BANK_B,
    PAD_BANK_B,
    PAD_BANK_B,
    PAD_BANK_B,
    PAD_BANK_B,
    PAD_BANK_B,
    PAD_BANK_B,
    PAD_BANK_B,
    // Line 10 (track select)
    PAD_BANK_C,
    PAD_BANK_C,
    PAD_BANK_C,
    PAD_BANK_C,
    PAD_BANK_C,
    PAD_BANK_C,
    PAD_BANK_C,
    PAD_BANK_C,
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

// The is the ForceMatrixSelector, indicating an OR'ed value of the mode matrix
// where we can find a controller.
// The actual values are constructed at start time
// static uint8_t ForceToMPCPadBank[256];

// // These are the reversed matrices, mapping a Force button number to an MPC pad number
// // They are initialized at start time and populated in the Write function
// // This is not very memory efficient (...256 where only 16 are used)
// // but we probably can afford to waste a few (kilo)bytes here.
// static uint8_t ForceToMPCA_A[256];
// static uint8_t ForceToMPCA_B[256];
// static uint8_t ForceToMPCA_C[256];
// static uint8_t ForceToMPCA_D[256];
// static uint8_t ForceToMPCB[256];
// static uint8_t ForceToMPCC[256];
// static uint8_t ForceToMPCD[256];

// These are the matrices where actual RGB values are stored
// They are initialized at start time and populated in the Write function
static ForceMPCPadColor_t MPCPadValuesA_A[16];
static ForceMPCPadColor_t MPCPadValuesA_B[16];
static ForceMPCPadColor_t MPCPadValuesA_C[16];
static ForceMPCPadColor_t MPCPadValuesA_D[16];
static ForceMPCPadColor_t MPCPadValuesB[16];
static ForceMPCPadColor_t MPCPadValuesC[16];
static ForceMPCPadColor_t MPCPadValuesD[16];

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
        MPCPadValuesA_A[i].r = 0x00;
        MPCPadValuesA_A[i].g = 0x00;
        MPCPadValuesA_A[i].b = 0x00;
        MPCPadValuesA_B[i].r = 0x00;
        MPCPadValuesA_B[i].g = 0x00;
        MPCPadValuesA_B[i].b = 0x00;
        MPCPadValuesA_C[i].r = 0x00;
        MPCPadValuesA_C[i].g = 0x00;
        MPCPadValuesA_C[i].b = 0x00;
        MPCPadValuesA_D[i].r = 0x00;
        MPCPadValuesA_D[i].g = 0x00;
        MPCPadValuesD[i].b = 0x00;
        MPCPadValuesB[i].r = 0x00;
        MPCPadValuesB[i].g = 0x00;
        MPCPadValuesB[i].b = 0x00;
        MPCPadValuesC[i].r = 0x00;
        MPCPadValuesC[i].g = 0x00;
        MPCPadValuesC[i].b = 0x00;
        MPCPadValuesD[i].r = 0x00;
        MPCPadValuesD[i].g = 0x00;
        MPCPadValuesD[i].b = 0x00;
    }
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
    tklog_debug("DrawMatrixPadFromCache(matrix=%02x, pad_number=%02x)\n", matrix, pad_number);

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
    case PAD_BANK_A_A:
        SetPadColor(padL, padC,
                    MPCPadValuesA_A[pad_number].r,
                    MPCPadValuesA_A[pad_number].g,
                    MPCPadValuesA_A[pad_number].b);
        break;
    case PAD_BANK_A_B:
        SetPadColor(padL, padC,
                    MPCPadValuesA_B[pad_number].r,
                    MPCPadValuesA_B[pad_number].g,
                    MPCPadValuesA_B[pad_number].b);
        break;
    case PAD_BANK_A_C:
        SetPadColor(padL, padC,
                    MPCPadValuesA_C[pad_number].r,
                    MPCPadValuesA_C[pad_number].g,
                    MPCPadValuesA_C[pad_number].b);
        break;
    case PAD_BANK_A_D:
        SetPadColor(padL, padC,
                    MPCPadValuesA_D[pad_number].r,
                    MPCPadValuesA_D[pad_number].g,
                    MPCPadValuesA_D[pad_number].b);
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
    }
}

////////
// Completely redraw the pads according to the mode we're in.
// Also take care of the "PAD BANK" button according to the proper mode
// XXX TODO: rename this into "switch mode" and handle button lights more sparsingly
////////
void MPCSwitchMatrix(uint8_t new_mode)
{
    tklog_debug("Switching to mode %02x (from current mode: %02x)\n", new_mode, MPCPadMode);

    // Are we actually changing mode??? If not, just return
    if (new_mode == MPCPadMode)
        return;

    // Reset all "PAD BANK" buttons
    uint8_t bt_bank_a[] = {0xB0, BANK_A, BUTTON_COLOR_OFF};
    uint8_t bt_bank_b[] = {0xB0, BANK_B, BUTTON_COLOR_OFF};
    uint8_t bt_bank_c[] = {0xB0, BANK_C, BUTTON_COLOR_OFF};
    uint8_t bt_bank_d[] = {0xB0, BANK_D, BUTTON_COLOR_OFF};

    // React according to the mode we switched to
    switch (new_mode)
    {
    case PAD_BANK_A_A:
        bt_bank_a[2] = BUTTON_COLOR_RED;
        break;
    case PAD_BANK_A_B:
        bt_bank_b[2] = BUTTON_COLOR_RED;
        break;
    case PAD_BANK_A_C:
        bt_bank_c[2] = BUTTON_COLOR_RED;
        break;
    case PAD_BANK_A_D:
        bt_bank_d[2] = BUTTON_COLOR_RED;
        break;
    case PAD_BANK_B:
        bt_bank_a[2] = BUTTON_COLOR_YELLOW_LIGHT;
        bt_bank_b[2] = BUTTON_COLOR_YELLOW;
        break;
    case PAD_BANK_C:
        bt_bank_a[2] = BUTTON_COLOR_YELLOW_LIGHT;
        bt_bank_c[2] = BUTTON_COLOR_YELLOW;
        break;
    case PAD_BANK_D:
        bt_bank_a[2] = BUTTON_COLOR_YELLOW_LIGHT;
        bt_bank_d[2] = BUTTON_COLOR_YELLOW;
        break;
    }

    // Set button lights
    orig_snd_rawmidi_write(rawvirt_outpriv, bt_bank_a, sizeof(bt_bank_a));
    orig_snd_rawmidi_write(rawvirt_outpriv, bt_bank_b, sizeof(bt_bank_b));
    orig_snd_rawmidi_write(rawvirt_outpriv, bt_bank_c, sizeof(bt_bank_c));
    orig_snd_rawmidi_write(rawvirt_outpriv, bt_bank_d, sizeof(bt_bank_d));

    // Save the new mode
    MPCPadMode = new_mode;

    // Actually draw pads
    for (int c = 0; c < 16; c++)
    {
        DrawMatrixPadFromCache(new_mode, c);
    }
}

void SetForceMatrixButton(uint8_t force_pad_note_number, bool on)
{
    // Set default colors for each button type
    // XXX TODO: make this configurable
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
        if (MPCToForceB[i] == force_pad_note_number)
        {
            tklog_debug("SetForceMatrixButton: %02x to value %d\n", force_pad_note_number, on);
            tklog_debug("   -> found pad %02x in matrix B\n", i);
            if (on)
            {
                MPCPadValuesB[i].r = color_on.r;
                MPCPadValuesB[i].g = color_on.g;
                MPCPadValuesB[i].b = color_on.b;
            }
            else
            {
                MPCPadValuesB[i].r = color_off.r;
                MPCPadValuesB[i].g = color_off.g;
                MPCPadValuesB[i].b = color_off.b;
            }
            if (MPCPadMode == PAD_BANK_B)
                DrawMatrixPadFromCache(PAD_BANK_B, i);
        }
        if (MPCToForceC[i] == force_pad_note_number)
        {
            tklog_debug("SetForceMatrixButton: %02x to value %d\n", force_pad_note_number, on);
            tklog_debug("   -> found pad %02x in matrix C\n", i);
            if (on)
            {
                MPCPadValuesC[i].r = color_on.r;
                MPCPadValuesC[i].g = color_on.g;
                MPCPadValuesC[i].b = color_on.b;
            }
            else
            {
                MPCPadValuesC[i].r = color_off.r;
                MPCPadValuesC[i].g = color_off.g;
                MPCPadValuesC[i].b = color_off.b;
            }
            if (MPCPadMode == PAD_BANK_C)
                DrawMatrixPadFromCache(PAD_BANK_C, i);
        }
        if (MPCToForceD[i] == force_pad_note_number)
        {
            tklog_debug("SetForceMatrixButton: %02x to value %d\n", force_pad_note_number, on);
            tklog_debug("   -> found pad %02x in matrix D\n", i);
            if (on)
            {
                MPCPadValuesD[i].r = color_on.r;
                MPCPadValuesD[i].g = color_on.g;
                MPCPadValuesD[i].b = color_on.b;
            }
            else
            {
                MPCPadValuesD[i].r = color_off.r;
                MPCPadValuesD[i].g = color_off.g;
                MPCPadValuesD[i].b = color_off.b;
            }
            if (MPCPadMode == PAD_BANK_D)
                DrawMatrixPadFromCache(PAD_BANK_D, i);
        }
    }

    return;
}

void CacheForcePad(uint8_t force_pad_number, uint8_t r, uint8_t g, uint8_t b)
{
    // force_pad_number starts from 0.
    // Find which matrix is this pad located in
    tklog_debug("CacheForcePad: %02x %02x %02x %02x\n", force_pad_number, r, g, b);
    uint8_t mpc_bank = ForcePadNumberToMPCBank[force_pad_number];
    uint8_t mpc_pad_number = ForcePadNumberToMPCPadNumber[force_pad_number];
    tklog_debug("   -> mpc_bank: %02x, mpc_pad_number (/16): %02x\n", mpc_bank, mpc_pad_number);

    // Select the proper bank cache.
    // Remember that a control can be on several matrices!
    if (mpc_bank & PAD_BANK_A_A)
    {
        MPCPadValuesA_A[mpc_pad_number].r = r;
        MPCPadValuesA_A[mpc_pad_number].g = g;
        MPCPadValuesA_A[mpc_pad_number].b = b;
    }
    if (mpc_bank & PAD_BANK_A_B)
    {
        MPCPadValuesA_B[mpc_pad_number].r = r;
        MPCPadValuesA_B[mpc_pad_number].g = g;
        MPCPadValuesA_B[mpc_pad_number].b = b;
    }
    if (mpc_bank & PAD_BANK_A_C)
    {
        MPCPadValuesA_C[mpc_pad_number].r = r;
        MPCPadValuesA_C[mpc_pad_number].g = g;
        MPCPadValuesA_C[mpc_pad_number].b = b;
    }
    if (mpc_bank & PAD_BANK_A_D)
    {
        MPCPadValuesA_D[mpc_pad_number].r = r;
        MPCPadValuesA_D[mpc_pad_number].g = g;
        MPCPadValuesA_D[mpc_pad_number].b = b;
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
    tklog_debug("Set pad color : L=%d C=%d r g b %02X %02X %02X\n", padL, padC, r, g, b);

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
uint8_t getMpcPadNumber(uint8_t note_number)
{
    switch(note_number)
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
            // if (myBuff[i + 1] == SHIFT_KEY_VALUE)
            // {
            //     shiftHoldMode = (myBuff[i + 2] == 0x7F ? true : false);
            //     // Kill the shift  event because we want to manage this here and not let
            //     // the MPC app to know that shift is pressed
            //     // PrepareFakeMidiMsg(&myBuff[i]);
            //     i += 3;
            //     continue; // next msg
            // }

            // Select bank mode.
            // Here we use BankA button as a pseudo-shift.
            // In any case if we're manipulating BANK buttons, we kill the event.
            bool layerBanksBCD = bankAHoldMode || (MPCPadMode == PAD_BANK_B) || (MPCPadMode == PAD_BANK_C) || (MPCPadMode == PAD_BANK_D);
            if (myBuff[i + 1] == LIVEII_BT_BANK_A)
            {
                if (myBuff[i + 2] == 0x7F)
                {
                    // Bank mode is activated
                    bankAHoldMode = true;
                    MPCSwitchMatrix(PAD_BANK_A_A);
                }
                else
                {
                    bankAHoldMode = false;
                }

                // Kill the event, ignore the rest
                PrepareFakeMidiMsg(&myBuff[i]);
                i += 3;
                continue;
            }
            else if ((myBuff[i + 1] == LIVEII_BT_BANK_B) && layerBanksBCD == false)
            {
                if (myBuff[i + 2] == 0x7F)
                    MPCSwitchMatrix(PAD_BANK_A_B);

                // Kill the event, ignore the rest
                PrepareFakeMidiMsg(&myBuff[i]);
                i += 3;
                continue;
            }
            else if ((myBuff[i + 1] == LIVEII_BT_BANK_C) && layerBanksBCD == false)
            {
                if (myBuff[i + 2] == 0x7F)
                    MPCSwitchMatrix(PAD_BANK_A_C);

                // Kill the event, ignore the rest
                PrepareFakeMidiMsg(&myBuff[i]);
                i += 3;
                continue;
            }
            else if ((myBuff[i + 1] == LIVEII_BT_BANK_D) && layerBanksBCD == false)
            {
                if (myBuff[i + 2] == 0x7F)
                    MPCSwitchMatrix(PAD_BANK_A_D);

                // Kill the event, ignore the rest
                PrepareFakeMidiMsg(&myBuff[i]);
                i += 3;
                continue;
            }
            else if ((myBuff[i + 1] == LIVEII_BT_BANK_B) && layerBanksBCD == true)
            {
                if (myBuff[i + 2] == 0x7F)
                    MPCSwitchMatrix(PAD_BANK_B);

                // Kill the event, ignore the rest
                PrepareFakeMidiMsg(&myBuff[i]);
                i += 3;
                continue;
            }
            else if ((myBuff[i + 1] == LIVEII_BT_BANK_C) && layerBanksBCD == true)
            {
                if (myBuff[i + 2] == 0x7F)
                    MPCSwitchMatrix(PAD_BANK_C);

                // Kill the event, ignore the rest
                PrepareFakeMidiMsg(&myBuff[i]);
                i += 3;
                continue;
            }
            else if ((myBuff[i + 1] == LIVEII_BT_BANK_D) && layerBanksBCD == true)
            {
                if (myBuff[i + 2] == 0x7F)
                    MPCSwitchMatrix(PAD_BANK_D);

                // Kill the event, ignore the rest
                PrepareFakeMidiMsg(&myBuff[i]);
                i += 3;
                continue;
            }

            // tklog_debug("Shift + key mode is %s \n",shiftHoldMode ? "active":"inactive");

            // Exception : Qlink management is hard coded
            // SHIFT "KNOB TOUCH" button :  add the offset when possible
            // MPC : 90 [54-63] 7F      FORCE : 90 [53-5A] 7F  (no "untouch" exists)
            else if (myBuff[i + 1] >= 0x54 && myBuff[i + 1] <= 0x63)
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
            case PAD_BANK_A_A:
                myBuff[i + 1] = MPCToForceA_A[pad_number];
                break;
            case PAD_BANK_A_B:
                myBuff[i + 1] = MPCToForceA_B[pad_number];
                break;
            case PAD_BANK_A_C:
                myBuff[i + 1] = MPCToForceA_C[pad_number];
                break;
            case PAD_BANK_A_D:
                myBuff[i + 1] = MPCToForceA_D[pad_number];
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
                case PAD_BANK_B:
                    tklog_debug("...converting input %02x %02x %02x ...\n",
                                myBuff[i], myBuff[i + 1], myBuff[i + 2]);
                    myBuff[i + 2] = myBuff[i] == 0x99 ? 0x7F : 0x00;
                    myBuff[i + 1] = MPCToForceB[pad_number];
                    myBuff[i] = 0x90;
                    tklog_debug("......to %02x %02x %02x\n",
                                myBuff[i], myBuff[i + 1], myBuff[i + 2]);
                    break;
                case PAD_BANK_C:
                    myBuff[i + 2] = myBuff[i] == 0x99 ? 0x7F : 0x00;
                    myBuff[i + 1] = MPCToForceC[pad_number];
                    myBuff[i] = 0x90;
                    break;
                case PAD_BANK_D:
                    myBuff[i + 2] = myBuff[i] == 0x99 ? 0x7F : 0x00;
                    myBuff[i + 1] = MPCToForceD[pad_number];
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
            tklog_debug("Inside Akai Sysex\n");
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
                tklog_debug("Inside Pad Color Sysex for pad %02x\n", padF);
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
            if (myBuff[i+1] != 0x35)
                tklog_debug("App wants to write to the Force button %02x value %02x...\n", myBuff[i + 1], myBuff[i + 2]);

            // Simple remapping
            if (map_ButtonsLeds_Inv[myBuff[i + 1]] >= 0)
            {
                // tklog_debug("MAP INV %d->%d\n",myBuff[i+1],map_ButtonsLeds_Inv[ myBuff[i+1] ]);
                myBuff[i + 1] = map_ButtonsLeds_Inv[myBuff[i + 1]];
                if (myBuff[i+1] != 0x35)
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
