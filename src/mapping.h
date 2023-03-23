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
typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} ForceMPCPadColor_t;

// Pad modes
#define PAD_BANK_RESTORE    0x00        // Go back to the previous bank
#define PAD_BANK_A          0x01       // Regular pads, lower left quadrant
#define PAD_BANK_B          0x02       // Regular pads
#define PAD_BANK_C          0x04
#define PAD_BANK_D          0x08
#define PAD_BANK_E          0x10       // NOT USED!
#define PAD_BANK_F          0x20       // Track settings pad
#define PAD_BANK_G          0x40       // Track numbers and arrows
#define PAD_BANK_H          0x80       // Scenes and arrows
#define PAD_BANK_ABCD       0x0F        // Groups of banks
#define PAD_BANK_EFGH       0xF0        // Groups of banks
#define PAD_BANK_MOMENTARY      false
#define PAD_BANK_PERMANENTLY    true


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