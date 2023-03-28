#ifndef MPC_MAPPING_H
#define MPC_MAPPING_H

#include <libgen.h>

// Debug macro
// #define DEBUG 1
// #ifdef DEBUG
#define LOG_DEBUG(fmt, ...) printf("[ DEBUG       ] %s:%s:%d " fmt "\n", basename(__FILE__), __PRETTY_FUNCTION__, __LINE__, ##__VA_ARGS__)
// #else
// #define LOG_DEBUG(fmt, ...) do {} while (0)
// #endif
#define LOG_ERROR(fmt, ...) printf("[ ** ERROR ** ] %s:%s:%d " fmt "\n", basename(__FILE__), __PRETTY_FUNCTION__, __LINE__, ##__VA_ARGS__)

#define PadColor_t uint32_t

void LoadMapping();
size_t Mpc_MapReadFromForce(void *midiBuffer, size_t maxSize, size_t size);
void Mpc_MapAppWriteToForce(const void *midiBuffer, size_t size);

extern void SetPadColorFromColorInt(const uint8_t padL, const u_int8_t padC, const uint32_t rgbColorValue);
extern void SetPadColor(const uint8_t padL, const u_int8_t padC, const uint8_t r, const uint8_t g, const uint8_t b);
extern void displayBatteryStatus();
extern void MPCSwitchMatrix(uint8_t new_mode, bool permanently);
uint8_t getMPCPadNoteNumber(uint8_t pad_number);
uint8_t getMPCPadNumber(uint8_t note_number);
void FakeMidiMessage(uint8_t buf[], size_t size);
bool SetLayoutPad(uint8_t matrix, uint8_t note_number, PadColor_t rgb, bool instant_redraw);

// Press mode delay in ms. If we keep pressing for more than this time,
// consider this mode as a temporary toggle
#define DOUBLE_CLICK_DELAY 500 // release -> pressed of the same button considered a dbl click
#define HOLD_DELAY 500         // Less than HOLD_DELAY is considered a click

// Force pads structure
#define CONTROL_TABLE_SIZE 256 // We consider we can't have more than 128 controls
                               // Which is consistant with MIDI specs for note numbers
                               // and the use of '0x80' flag to indicate a pad
#define EXTRA_TABLE_SIZE 48    // Number of controls with multiple functions we authorize

// The table map an MPC control to a FORCE control.
// We'll use this to construct the opposite table
#define CT_NONE 0  // Nothing there
#define CT_BTN 1   // Button
#define CT_PAD 2   // Pad
#define CT_CUS 3   // Custom
#define CT_EXTRA 4 // Extra
#define FORCE_BT_UNSET 0xFF

#define BATTERY_CHARGING 0x00
#define BATTERY_CHARGED 0x01
#define BATTERY_DISCHARGING 0x02
#define BATTERY_NOT_CHARGING 0x03
#define BATTERY_FULL 0x04

// Ok, now we are talking about the reverse mode: how a Force control
// maps to an MPC control. It's not a bijection!
#define FORCE_PAD_FLAG 0x80

// The things we display on the pads
// NOTA: KEEP THESE SEQUENITAL, they're going to be used as indexes
#define IAMFORCE_LAYOUT_N 9
#define IAMFORCE_LAYOUT_PAD_BANK_A 0x00
#define IAMFORCE_LAYOUT_PAD_BANK_B 0x01
#define IAMFORCE_LAYOUT_PAD_BANK_C 0x02
#define IAMFORCE_LAYOUT_PAD_BANK_D 0x03
#define IAMFORCE_LAYOUT_PAD_MODE 0x04
#define IAMFORCE_LAYOUT_PAD_SCENE 0x05
#define IAMFORCE_LAYOUT_PAD_MUTE 0x06
#define IAMFORCE_LAYOUT_PAD_COLS 0x07
#define IAMFORCE_LAYOUT_PAD_XFDR 0x08
#define IAMFORCE_LAYOUT_NONE 0xFF

// Bank buttons states.
#define MODE_BUTTONS_TOP_MODE 0x01    // If this bit is 1, then the top row is yellow
#define MODE_BUTTONS_TOP_LOCK 0x03    // If this bit is 1, then the top row is locked
#define MODE_BUTTONS_BOTTOM_MODE 0x04 // If this bit is 1, then the bottom row is yellow
#define MODE_BUTTONS_BOTTOM_LOCK 0x0c // If this bit is 1, then the bottom row is locked

// The Force mode we could be in. The LED on the MODE button of the Force
// will indicate the current mode.
#define MPC_FORCE_MODE_NONE 0x00
#define MPC_FORCE_MODE_LAUNCH 0x01
#define MPC_FORCE_MODE_STEPSEQ 0x02
#define MPC_FORCE_MODE_NOTE 0x03

// Create a source type enum that is an int8_t integer
// and create an new SourceType_t alias type
enum
{
    source_button,       // This is button press
    source_led,          // This is button *LED*
    source_pad_note_on,  // Channel will be different
    source_pad_note_off, // Channel will be different
    source_pad_aftertouch,   // Channel will be different
    source_pad_sysex,
    source_unkown
};
typedef int8_t SourceType_t;

// Default length of messages
extern uint_fast8_t SOURCE_MESSAGE_LENGTH[7];

// Forward declaration of the MPCControlToForce_t structure
typedef struct MPCControlToForce_s MPCControlToForce_t;
typedef struct ForceControlToMPC_s ForceControlToMPC_t;

// Define the callback function signature
// source_type is one of the source_* enums
// Note number is the ACTUAL note number (ie < 0x80) of the SOURCE
typedef size_t (*MPCControlCallback_t)(MPCControlToForce_t *force_target, ForceControlToMPC_t *mpc_target, SourceType_t source_type, uint8_t note_number, uint8_t *midi_buffer, size_t buffer_size);

// Expression of the MPC state
typedef struct IAMForceStatus_t
{
    uint8_t pad_layout;   // The mode layout we're in (MPC_PAD_LAYOUT_*)
    uint8_t force_mode;   // The FORCE mode (MPC_FORCE_MODE_*)
    uint8_t mode_buttons; // The behaviour of mode buttons (MODE_BUTTONS_*)

    // Specific layouts for native Force modes
    uint_fast8_t launch_mode_layout;
    uint_fast8_t stepseq_mode_layout;
    uint_fast8_t note_mode_layout;

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

// The core types we're going to need
typedef struct MPCControlToForce_s
{
    uint8_t note_number;
    PadColor_t color;
    MPCControlCallback_t callback;
    // size_t (*callback)(MPCControlToForce_t *force_target, ForceControlToMPC_t *mpc_target, uint8_t *midi_buffer, size_t buffer_size);
} MPCControlToForce_t;

typedef struct ForceControlToMPC_s
{
    // uint8_t type;                    // Destination is either BTN, PAD or CUS
    uint8_t note_number; // note number ; 8th bit to 1 if PAD
    uint8_t bank;        // ...only if 'PAD'
    PadColor_t color;    // The pad color if it has to be redefined.
                         // Only partial values will be authorized for buttons
                         // If it's a button, only RED, LIGHT_RED,
                         // YELLOW, LIGHT_YELLOW and ORANGE are allowed
    MPCControlCallback_t callback;
    ForceControlToMPC_t *next_control; // allow easy chaining of controls
} ForceControlToMPC_t;


// Prototypes of the callback functions.
// See MPCControlCallback_t for the signature
size_t cb_default(MPCControlToForce_t *force_target, ForceControlToMPC_t *mpc_target, SourceType_t source_type, uint8_t note_number, uint8_t *midi_buffer, size_t buffer_size);
size_t cb_mode_e(MPCControlToForce_t *force_target, ForceControlToMPC_t *mpc_target, SourceType_t source_type, uint8_t note_number, uint8_t *midi_buffer, size_t buffer_size);
size_t cb_xfader(MPCControlToForce_t *force_target, ForceControlToMPC_t *mpc_target, SourceType_t source_type, uint8_t note_number, uint8_t *midi_buffer, size_t buffer_size);
size_t cb_tap_tempo(MPCControlToForce_t *force_target, ForceControlToMPC_t *mpc_target, SourceType_t source_type, uint8_t note_number, uint8_t *midi_buffer, size_t buffer_size);
size_t cb_shift(MPCControlToForce_t *force_target, ForceControlToMPC_t *mpc_target, SourceType_t source_type, uint8_t note_number, uint8_t *midi_buffer, size_t buffer_size);
size_t cb_edit_button(MPCControlToForce_t *force_target, ForceControlToMPC_t *mpc_target, SourceType_t source_type, uint8_t note_number, uint8_t *midi_buffer, size_t buffer_size);
size_t cb_play(MPCControlToForce_t *force_target, ForceControlToMPC_t *mpc_target, SourceType_t source_type, uint8_t note_number, uint8_t *midi_buffer, size_t buffer_size);

// A few shortcut functions
extern uint8_t getForcePadNoteNumber(uint8_t pad_number, bool extra_bit);

// Misc stuff
#define BATTERY_CHECK_INTERVAL 10 // Check battery status every 10 tap ticks

// Basic assignments
#define FORCE_BT_ENCODER 0x6F  // ok
#define FORCE_BT_NAVIGATE 0x00 // ok
#define FORCE_BT_KNOBS 0x01    // ok
#define FORCE_BT_MENU 0x02     // ok
#define FORCE_BT_MATRIX 0x03   // ok
#define FORCE_BT_NOTE 0x04     // ok
#define FORCE_BT_MASTER 0x05
#define FORCE_BT_CLIP 0x09
#define FORCE_BT_MIXER 0x0B
#define FORCE_BT_LOAD 0x23
#define FORCE_BT_SAVE 0x24
#define FORCE_BT_EDIT 0x25
#define FORCE_BT_DELETE 0x26
#define FORCE_BT_SHIFT 0x31
#define FORCE_BT_SELECT 0x34
#define FORCE_BT_TAP_TEMPO 0x35
#define FORCE_BT_PLUS 0x36
#define FORCE_BT_MINUS 0x37
#define FORCE_BT_LAUNCH_1 0x38
#define FORCE_BT_LAUNCH_2 0x39
#define FORCE_BT_LAUNCH_3 0x3A
#define FORCE_BT_LAUNCH_4 0x3B
#define FORCE_BT_LAUNCH_5 0x3C
#define FORCE_BT_LAUNCH_6 0x3D
#define FORCE_BT_LAUNCH_7 0x3E
#define FORCE_BT_LAUNCH_8 0x3F
#define FORCE_BT_UNDO 0x43
#define FORCE_BT_REC 0x49
#define FORCE_BT_STOP 0x51
#define FORCE_BT_PLAY 0x52
#define FORCE_BT_QLINK1_TOUCH 0x53
#define FORCE_BT_QLINK2_TOUCH 0x54
#define FORCE_BT_QLINK3_TOUCH 0x55
#define FORCE_BT_QLINK4_TOUCH 0x56
#define FORCE_BT_QLINK5_TOUCH 0x57
#define FORCE_BT_QLINK6_TOUCH 0x58
#define FORCE_BT_QLINK7_TOUCH 0x59
#define FORCE_BT_QLINK8_TOUCH 0x5A
#define FORCE_BT_STOP_ALL 0x5F
#define FORCE_BT_UP 0x70
#define FORCE_BT_DOWN 0x71
#define FORCE_BT_LEFT 0x72
#define FORCE_BT_RIGHT 0x73
#define FORCE_BT_LAUNCH 0x74
#define FORCE_BT_STEP_SEQ 0x75
#define FORCE_BT_ARP 0x76
#define FORCE_BT_COPY 0x7A
#define FORCE_BT_COLUMN_PAD1 0x60
#define FORCE_BT_COLUMN_PAD2 0x61
#define FORCE_BT_COLUMN_PAD3 0x62
#define FORCE_BT_COLUMN_PAD4 0x63
#define FORCE_BT_COLUMN_PAD5 0x64
#define FORCE_BT_COLUMN_PAD6 0x65
#define FORCE_BT_COLUMN_PAD7 0x66
#define FORCE_BT_COLUMN_PAD8 0x67
#define FORCE_BT_MUTE_PAD1 0x29
#define FORCE_BT_MUTE_PAD2 0x2A
#define FORCE_BT_MUTE_PAD3 0x2B
#define FORCE_BT_MUTE_PAD4 0x2C
#define FORCE_BT_MUTE_PAD5 0x2D
#define FORCE_BT_MUTE_PAD6 0x2E
#define FORCE_BT_MUTE_PAD7 0x2F
#define FORCE_BT_MUTE_PAD8 0x30
#define FORCE_BT_ASSIGN_A 0x7B
#define FORCE_BT_ASSIGN_B 0x7C
#define FORCE_BT_MUTE 0x5B
#define FORCE_BT_SOLO 0x5C
#define FORCE_BT_REC_ARM 0x5D
#define FORCE_BT_CLIP_STOP 0x5E

#define LIVEII_BT_ENCODER 0x6F
#define LIVEII_BT_BANK_A 0x23
#define LIVEII_BT_BANK_B 0x24
#define LIVEII_BT_BANK_C 0x25
#define LIVEII_BT_BANK_D 0x26
#define LIVEII_BT_NOTE_REPEAT 0x0B
#define LIVEII_BT_FULL_LEVEL 0x27
#define LIVEII_BT_16_LEVEL 0x28
#define LIVEII_BT_ERASE 0x09
#define LIVEII_BT_QLINK_SELECT 0x00
#define LIVEII_BT_SHIFT 0x31
#define LIVEII_BT_MENU 0x7B
#define LIVEII_BT_MAIN 0x34
#define LIVEII_BT_UNDO 0x43
#define LIVEII_BT_COPY 0x7A
#define LIVEII_BT_TAP_TEMPO 0x35
#define LIVEII_BT_REC 0x49
#define LIVEII_BT_OVERDUB 0x50
#define LIVEII_BT_STOP 0x51
#define LIVEII_BT_PLAY 0x52
#define LIVEII_BT_PLAY_START 0x53
#define LIVEII_BT_PLUS 0x36
#define LIVEII_BT_MINUS 0x37
#define LIVEII_BT_MIX 0x74
#define LIVEII_BT_MUTE 0x4C
#define LIVEII_BT_NEXT_SEQ 0x2A
#define LIVEII_BT_STEP_SEQ 0x29
#define LIVEII_BT_TC 0x7D
#define LIVEII_ROTARY 0x6F
#define LIVEII_PAD_COLOR 0x70
#define LIVEII_PAD_MIXER 0x73
#define LIVEII_TRACK_MUTE 0x75
#define LIVEII_TRACK_MIXER 0x74

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