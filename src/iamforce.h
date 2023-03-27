#ifndef MPC_MAPPING_H
#define MPC_MAPPING_H
#define MAPPING_TABLE_SIZE 256

extern int map_ButtonsLeds[MAPPING_TABLE_SIZE];
extern int map_ButtonsLeds_Inv[MAPPING_TABLE_SIZE]; // Inverted table
extern int map_Ctrl[MAPPING_TABLE_SIZE];
extern int map_Ctrl_Inv[MAPPING_TABLE_SIZE]; // Inverted table

void LoadMapping();
size_t Mpc_MapReadFromForce(void *midiBuffer, size_t maxSize, size_t size);
void Mpc_MapAppWriteToForce(const void *midiBuffer, size_t size);

extern void SetPadColorFromColorInt(const uint8_t padL, const u_int8_t padC, const uint32_t rgbColorValue);
extern void SetPadColor(const uint8_t padL, const u_int8_t padC, const uint8_t r, const uint8_t g, const uint8_t b);
extern void displayBatteryStatus();
extern void MPCSwitchMatrix(uint8_t new_mode, bool permanently);

// Press mode delay in ms. If we keep pressing for more than this time,
// consider this mode as a temporary toggle
#define DOUBLE_CLICK_DELAY 500      // release -> pressed of the same button considered a dbl click
#define HOLD_DELAY 500              // Less than HOLD_DELAY is considered a click

// Pads Color cache captured from sysex events
#define PadColor_t  uint32_t

// Force pads structure
#define CONTROL_TABLE_SIZE 256          // We consider we can't have more than 128 controls
                                        // Which is consistant with MIDI specs for note numbers
                                        // and the use of '0x80' flag to indicate a pad
#define EXTRA_TABLE_SIZE    48          // Number of controls with multiple functions we authorize

// The table map an MPC control to a FORCE control.
// We'll use this to construct the opposite table
#define CT_NONE 0 // Nothing there
#define CT_BTN 1 // Button
#define CT_PAD 2 // Pad
#define CT_CUS 3 // Custom
#define CT_EXTRA 4 // Extra
#define FORCE_BT_UNSET  0xFF
typedef struct
{
    uint8_t note_number;            // Use IAMFORCE_BT_UNSET to disable it (which is the default value BTW)
    PadColor_t  color;              // The pad color if it has to be redefined.
                                    // In that direction it doesn't seem to have much meaning,
                                    // but in practice different MPC pads of the same control
                                    // can have different colors.
                                    // Only partial values will be authorized for buttons.
                                    // This is color ON, by the way; OFF will be deduced.
    // pointer to a callback function taking a pointer to the message data as input.
    // mpc_to_force indicates if we're remapping from MPC to Force or Force To MPC
    size_t (*callback)(MPCControlToForce_t *force_target, ForceControlToMPC_t *mpc_target, uint8_t *midi_buffer, size_t buffer_size);
} MPCControlToForce_t;

#define BATTERY_CHARGING 0x00
#define BATTERY_CHARGED 0x01
#define BATTERY_DISCHARGING 0x02
#define BATTERY_NOT_CHARGING 0x03
#define BATTERY_FULL 0x04

// Ok, now we are talking about the reverse mode: how a Force control
// maps to an MPC control. It's not a bijection!
#define FORCE_PAD_FLAG    0x80
typedef struct
{
    // uint8_t type;                    // Destination is either BTN, PAD or CUS
    uint8_t     note_number;            // note number ; 8th bit to 1 if PAD
    uint8_t     bank;                   // ...only if 'PAD'
    PadColor_t  color;                  // The pad color if it has to be redefined.
                                        // Only partial values will be authorized for buttons
                                        // If it's a button, only RED, LIGHT_RED, 
                                        // YELLOW, LIGHT_YELLOW and ORANGE are allowed
    size_t (*callback)(MPCControlToForce_t *force_target, ForceControlToMPC_t *mpc_target, uint8_t *midi_buffer, size_t buffer_size);
    ForceControlToMPC_t *next_control;  // allow easy chaining of controls
} ForceControlToMPC_t;

// MIDI message lengths
#define NOTE_MESSAGE_LENGTH 3
#define PAD_SYSEX_MESSAGE_LENGTH 6      // Doesn't include the last F7 byte

// The things we display on the pads
// NOTA: KEEP THESE SEQUENITAL, they're going to be used as indexes
#define IAMFORCE_LAYOUT_N                9
#define IAMFORCE_LAYOUT_PAD_BANK_A       0x00
#define IAMFORCE_LAYOUT_PAD_BANK_B       0x01
#define IAMFORCE_LAYOUT_PAD_BANK_C       0x02
#define IAMFORCE_LAYOUT_PAD_BANK_D       0x03
#define IAMFORCE_LAYOUT_PAD_MODE         0x04
#define IAMFORCE_LAYOUT_PAD_SCENE        0x05
#define IAMFORCE_LAYOUT_PAD_MUTE         0x06
#define IAMFORCE_LAYOUT_PAD_COLS         0x07
#define IAMFORCE_LAYOUT_PAD_XFDR         0x08
#define IAMFORCE_LAYOUT_NONE             0xFF

// Bank buttons states.
#define MODE_BUTTONS_TOP_MODE       0x01     // If this bit is 1, then the top row is yellow
#define MODE_BUTTONS_TOP_LOCK       0x03     // If this bit is 1, then the top row is locked
#define MODE_BUTTONS_BOTTOM_MODE    0x04     // If this bit is 1, then the bottom row is yellow
#define MODE_BUTTONS_BOTTOM_LOCK    0x0c     // If this bit is 1, then the bottom row is locked

// The Force mode we could be in. The LED on the MODE button of the Force
// will indicate the current mode.
#define MPC_FORCE_MODE_NONE         0x00
#define MPC_FORCE_MODE_LAUNCH       0x01
#define MPC_FORCE_MODE_STEPSEQ      0x02
#define MPC_FORCE_MODE_NOTE         0x03

// Expression of the MPC state
typedef struct IAMForceStatus_t
{
    uint8_t pad_layout;         // The mode layout we're in (MPC_PAD_LAYOUT_*)
    uint8_t force_mode;         // The FORCE mode (MPC_FORCE_MODE_*)
    uint8_t mode_buttons;       // The behaviour of mode buttons (MODE_BUTTONS_*)

    // We keep the TAP value in memory (useful for flashing effects)
    bool tap_status;
    uint8_t tap_counter;
    uint8_t battery_status;
    uint8_t battery_capacity;

    // Other parameters
    bool project_loaded;
    bool shift_hold;

    // This is how we handle "clicks" and "double clicks"
    uint8_t last_button_down;
    struct timespec started_button_down;
} IAMForceStatus_t;
extern IAMForceStatus_t IAMForceStatus;

// Prototypes of the callback functions
size_t cb_default(MPCControlToForce_t *force_target, ForceControlToMPC_t *mpc_target, uint8_t *midi_buffer, size_t buffer_size);
size_t cb_mode_e(MPCControlToForce_t *force_target, ForceControlToMPC_t *mpc_target, uint8_t *midi_buffer, size_t buffer_size);
size_t cb_xfader(MPCControlToForce_t *force_target, ForceControlToMPC_t *mpc_target, uint8_t *midi_buffer, size_t buffer_size);
size_t cb_tap_tempo(MPCControlToForce_t *force_target, ForceControlToMPC_t *mpc_target, uint8_t *midi_buffer, size_t buffer_size);
size_t cb_shift(MPCControlToForce_t *force_target, ForceControlToMPC_t *mpc_target, uint8_t *midi_buffer, size_t buffer_size);
size_t cb_edit_button(MPCControlToForce_t *force_target, ForceControlToMPC_t *mpc_target, uint8_t *midi_buffer, size_t buffer_size);
size_t cb_play(MPCControlToForce_t *force_target, ForceControlToMPC_t *mpc_target, uint8_t *midi_buffer, size_t buffer_size);

// Misc stuff
BATTERY_CHECK_INTERVAL = 10; // Check battery status every 10 tap ticks

// Colors
// Colors R G B (nb . max r g b value is 7f. The bit 8 is always unset )
#define COLOR_WHITE             0x7F7F7F
#define COLOR_BLACK             0x000000
#define COLOR_RED               0x7F0000
#define COLOR_BLUE              0x00007F
#define COLOR_GREEN             0x007F00
#define COLOR_YELLOW            0x7F7F00
#define COLOR_GREY              0x3f3f3f
#define COLOR_LIGHT_RED         0x7f3f3f
#define COLOR_LIGHT_YELLOW      0x7f7f3f
#define COLOR_ORANGE            0x7F5019

// #define COLOR_CYAN 0x007F7F
// #define COLOR_MAGENTA 0x7F007F
// #define COLOR_CORAL 0xFF0077
// #define COLOR_PINK          0xFFC0CB
// #define COLOR_FIRE          0x060101
// #define COLOR_TANGERINE     0x060201
// #define COLOR_APRICOT       0x7F5019
// #define COLOR_CANARY        0x7F6E19
// #define COLOR_LEMON         0x757F19
// #define COLOR_CHARTREUSE    0x5A7F19
// #define COLOR_NEON          0x3B7F19
// #define COLOR_LIME          0x207F19
// #define COLOR_CLOVER        0x197F2E
// #define COLOR_SEA           0x197F4C
// #define COLOR_MINT          0x197F68
// #define COLOR_CYAN          0x197C7F
// #define COLOR_SKY           0x195D7F
// #define COLOR_AZURE         0x19427F
// #define COLOR_MIDNIGHT      0x19277F
// #define COLOR_INDIGO        0x3A197F
// #define COLOR_VIOLET        0x46197F
// #define COLOR_GRAPE         0x60197F
// #define COLOR_FUSHIA        0x7F197F
// #define COLOR_MAGENTA       0x7F1964
// #define COLOR_CORAL         0x7F1949



// Basic assignments
#define FORCE_BT_ENCODER 0x6F       // ok
#define FORCE_BT_NAVIGATE 0         // ok
#define FORCE_BT_KNOBS 1            // ok
#define FORCE_BT_MENU 2             // ok
#define FORCE_BT_MATRIX 3           // ok
#define FORCE_BT_NOTE 4             // ok
#define FORCE_BT_MASTER 5
#define FORCE_BT_CLIP 9             // ok
#define FORCE_BT_MIXER 11           // ok
#define FORCE_BT_LOAD 35            // ok
#define FORCE_BT_SAVE 36            // ok
#define FORCE_BT_EDIT 37            // ok
#define FORCE_BT_DELETE 38          // ok
#define FORCE_BT_SHIFT 0x31         // ok
#define FORCE_BT_SELECT 52          // ok
#define FORCE_BT_TAP_TEMPO 53       // ok
#define FORCE_BT_PLUS 54            // ok
#define FORCE_BT_MINUS 55           // ok
#define FORCE_BT_LAUNCH_1 56
#define FORCE_BT_LAUNCH_2 57
#define FORCE_BT_LAUNCH_3 58
#define FORCE_BT_LAUNCH_4 59
#define FORCE_BT_LAUNCH_5 60
#define FORCE_BT_LAUNCH_6 61
#define FORCE_BT_LAUNCH_7 62
#define FORCE_BT_LAUNCH_8 63
#define FORCE_BT_UNDO 67            // ok
#define FORCE_BT_REC 73             // ok
#define FORCE_BT_STOP 81            // ok
#define FORCE_BT_PLAY 82            // ok
#define FORCE_BT_QLINK1_TOUCH 83
#define FORCE_BT_QLINK2_TOUCH 84
#define FORCE_BT_QLINK3_TOUCH 85
#define FORCE_BT_QLINK4_TOUCH 86
#define FORCE_BT_QLINK5_TOUCH 87
#define FORCE_BT_QLINK6_TOUCH 88
#define FORCE_BT_QLINK7_TOUCH 89
#define FORCE_BT_QLINK8_TOUCH 90
#define FORCE_BT_STOP_ALL 95
#define FORCE_BT_UP 112
#define FORCE_BT_DOWN 113
#define FORCE_BT_LEFT 114
#define FORCE_BT_RIGHT 115
#define FORCE_BT_LAUNCH 116         // ok
#define FORCE_BT_STEP_SEQ 117       // ok
#define FORCE_BT_ARP 118
#define FORCE_BT_COPY 122           // ok
#define FORCE_BT_COLUMN_PAD1 96
#define FORCE_BT_COLUMN_PAD2 97
#define FORCE_BT_COLUMN_PAD3 98
#define FORCE_BT_COLUMN_PAD4 99
#define FORCE_BT_COLUMN_PAD5 100
#define FORCE_BT_COLUMN_PAD6 101
#define FORCE_BT_COLUMN_PAD7 102
#define FORCE_BT_COLUMN_PAD8 103
#define FORCE_BT_MUTE_PAD1 41
#define FORCE_BT_MUTE_PAD2 42
#define FORCE_BT_MUTE_PAD3 43
#define FORCE_BT_MUTE_PAD4 44
#define FORCE_BT_MUTE_PAD5 45
#define FORCE_BT_MUTE_PAD6 46
#define FORCE_BT_MUTE_PAD7 47
#define FORCE_BT_MUTE_PAD8 48
#define FORCE_BT_ASSIGN_A 123
#define FORCE_BT_ASSIGN_B 124
#define FORCE_BT_MUTE 91
#define FORCE_BT_SOLO 92
#define FORCE_BT_REC_ARM 93
#define FORCE_BT_CLIP_STOP 94

#define LIVEII_BT_ENCODER 0x6F
#define LIVEII_BT_BANK_A 35
#define LIVEII_BT_BANK_B 36
#define LIVEII_BT_BANK_C 37
#define LIVEII_BT_BANK_D 38
#define LIVEII_BT_NOTE_REPEAT 11
#define LIVEII_BT_FULL_LEVEL 39
#define LIVEII_BT_16_LEVEL 40
#define LIVEII_BT_ERASE 9
#define LIVEII_BT_QLINK_SELECT 0
#define LIVEII_BT_SHIFT 49
#define LIVEII_BT_MENU 123
#define LIVEII_BT_MAIN 52
#define LIVEII_BT_UNDO 67
#define LIVEII_BT_COPY 122
#define LIVEII_BT_TAP_TEMPO 53
#define LIVEII_BT_REC 73
#define LIVEII_BT_OVERDUB 80
#define LIVEII_BT_STOP 81
#define LIVEII_BT_PLAY 82
#define LIVEII_BT_PLAY_START 83
#define LIVEII_BT_PLUS 54
#define LIVEII_BT_MINUS 55
#define LIVEII_BT_MIX 116
#define LIVEII_BT_MUTE 76
#define LIVEII_BT_NEXT_SEQ 42
#define LIVEII_BT_STEP_SEQ 41
#define LIVEII_BT_TC 125
#define LIVEII_ROTARY 111
#define LIVEII_PAD_COLOR 112
#define LIVEII_PAD_MIXER 115
#define LIVEII_TRACK_MUTE 117
#define LIVEII_TRACK_MIXER 116

// Pads in top-left=0 order
#define LIVEII_PAD_TL0 0x31
#define LIVEII_PAD_TL1 0x37
#define LIVEII_PAD_TL2 0x33
#define LIVEII_PAD_TL3 0x35
#define LIVEII_PAD_TL4 0x30
#define LIVEII_PAD_TL5 0x2F
#define LIVEII_PAD_TL6 0x2D
#define LIVEII_PAD_TL7 0x2B
#define LIVEII_PAD_TL8 0x28
#define LIVEII_PAD_TL9 0x26
#define LIVEII_PAD_TL10 0x2E
#define LIVEII_PAD_TL11 0x2C
#define LIVEII_PAD_TL12 0x25
#define LIVEII_PAD_TL13 0x24
#define LIVEII_PAD_TL14 0x2A
#define LIVEII_PAD_TL15 0x52


#endif