
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
int map_Ctrl[MAPPING_TABLE_SIZE];
int map_Ctrl_Inv[MAPPING_TABLE_SIZE]; // Inverted table

// Force Matrix pads color cache
// ForceMPCPadColor_t PadSysexColorsCache[256];

// SHIFT Holded mode
// Holding shift will activate the shift mode
static bool shiftHoldedMode = false;

// To navigate in matrix quadran when MPC spoofing a Force
static int MPCPad_OffsetL = 0;
static int MPCPad_OffsetC = 0;

// MPC Current pad bank.  A-H = 0-7
// static int MPC_PadBank = BANK_A;

// Columns modes in Force simulated on a MPC
static int ForceColumnMode = -1;


static ForceMPCPadColor_t PadSysexColorsCache[256];

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
    map_ButtonsLeds[LIVEII_BT_MUTE] = FORCE_BT_STEP_SEQ;    // Or ARP? Meh.
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

///////////////////////////////////////////////////////////////////////////////
// Show the current MPC quadran within the Force matrix
///////////////////////////////////////////////////////////////////////////////
static void Mpc_ShowForceMatrixQuadran(uint8_t forcePadL, uint8_t forcePadC)
{

    uint8_t sysexBuff[12] = {0xF0, 0x47, 0x7F, 0x40, 0x65, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0xF7};
    //                                                                 [Pad #] [R]   [G]   [B]
    sysexBuff[3] = DeviceInfoBloc[MPCOriginalId].sysexId;

    uint8_t q = (forcePadL == 4 ? 0 : 1) * 4 + (forcePadC == 4 ? 3 : 2);

    for (int l = 0; l < 2; l++)
    {
        for (int c = 0; c < 2; c++)
        {
            sysexBuff[7] = l * 4 + c + 2;
            if (sysexBuff[7] == q)
            {
                sysexBuff[8] = 0x7F;
                sysexBuff[9] = 0x7F;
                sysexBuff[10] = 0x7F;
            }
            else
            {
                sysexBuff[8] = 0x7F;
                sysexBuff[9] = 0x00;
                sysexBuff[10] = 0x00;
            }
            // tklog_debug("[tkgl] MPC Pad quadran : l,c %d,%d Pad %d r g b %02X %02X %02X\n",forcePadL,forcePadC,sysexBuff[7],sysexBuff[8],sysexBuff[9],sysexBuff[10]);

            orig_snd_rawmidi_write(rawvirt_outpriv, sysexBuff, sizeof(sysexBuff));
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
// Refresh MPC pads colors from Force PAD Colors cache
///////////////////////////////////////////////////////////////////////////////
void Mpc_ResfreshPadsColorFromForceCache(uint8_t padL, uint8_t padC, uint8_t nbLine)
{

    // Write again the color like a Force.
    // The midi modification will be done within the corpse of the hooked fn.
    // Pads from 64 are columns pads

    uint8_t sysexBuff[12] = {0xF0, 0x47, 0x7F, 0x40, 0x65, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0xF7};
    //                                                                 [Pad #] [R]   [G]   [B]

    for (int l = 0; l < nbLine; l++)
    {

        for (int c = 0; c < 4; c++)
        {

            int padF = (l + padL) * 8 + c + padC;
            sysexBuff[7] = padF;
            sysexBuff[8] = PadSysexColorsCache[padF].r;
            sysexBuff[9] = PadSysexColorsCache[padF].g;
            sysexBuff[10] = PadSysexColorsCache[padF].b;
            // Send the sysex to the MPC controller
            snd_rawmidi_write(rawvirt_outpriv, sysexBuff, sizeof(sysexBuff));
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
// Draw a pad line on MPC pads from a Force PAD line in the current Colors cache
///////////////////////////////////////////////////////////////////////////////
void Mpc_DrawPadLineFromForceCache(uint8_t forcePadL, uint8_t forcePadC, uint8_t mpcPadL)
{

    uint8_t sysexBuff[12] = {0xF0, 0x47, 0x7F, 0x40, 0x65, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0xF7};
    //                                                                 [Pad #] [R]   [G]   [B]
    sysexBuff[3] = DeviceInfoBloc[MPCOriginalId].sysexId;

    uint8_t p = forcePadL * 8 + forcePadC;

    for (int c = 0; c < 4; c++)
    {
        sysexBuff[7] = mpcPadL * 4 + c;
        p = forcePadL * 8 + c + forcePadC;
        sysexBuff[8] = PadSysexColorsCache[p].r;
        sysexBuff[9] = PadSysexColorsCache[p].g;
        sysexBuff[10] = PadSysexColorsCache[p].b;

        // tklog_debug("[tkgl] MPC Pad Line refresh : %d r g b %02X %02X %02X\n",sysexBuff[7],sysexBuff[8],sysexBuff[9],sysexBuff[10]);

        orig_snd_rawmidi_write(rawvirt_outpriv, sysexBuff, sizeof(sysexBuff));
    }
}

///////////////////////////////////////////////////////////////////////////////
// MIDI READ - APP ON MPC READING AS FORCE
///////////////////////////////////////////////////////////////////////////////
size_t Mpc_MapReadFromForce(void *midiBuffer, size_t maxSize, size_t size)
{

    uint8_t *myBuff = (uint8_t *)midiBuffer;

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
            if (shiftHoldedMode && DeviceInfoBloc[MPCOriginalId].qlinkKnobsCount < 16 && myBuff[i + 1] >= 0x10 && myBuff[i + 1] <= 0x31)
                myBuff[i + 1] += DeviceInfoBloc[MPCOriginalId].qlinkKnobsCount;
            i += 3;
            continue;
        }

        // BUTTONS PRESS / RELEASE------------------------------------------------
        if (myBuff[i] == 0x90)
        {
            // tklog_debug("Button 0x%02x %s\n",myBuff[i+1], (myBuff[i+2] == 0x7F ? "pressed":"released") );

            // SHIFT pressed/released (nb the SHIFT button can't be mapped)
            // Double click on SHIFT is not managed at all. Avoid it.
            if (myBuff[i + 1] == SHIFT_KEY_VALUE)
            {
                shiftHoldedMode = (myBuff[i + 2] == 0x7F ? true : false);
                // Kill the shift  event because we want to manage this here and not let
                // the MPC app to know that shift is pressed
                // PrepareFakeMidiMsg(&myBuff[i]);
                i += 3;
                continue; // next msg
            }

            // tklog_debug("Shift + key mode is %s \n",shiftHoldedMode ? "active":"inactive");

            // Exception : Qlink management is hard coded
            // SHIFT "KNOB TOUCH" button :  add the offset when possible
            // MPC : 90 [54-63] 7F      FORCE : 90 [53-5A] 7F  (no "untouch" exists)
            if (myBuff[i + 1] >= 0x54 && myBuff[i + 1] <= 0x63)
            {
                myBuff[i + 1]--; // Map to force Qlink touch

                if (shiftHoldedMode && DeviceInfoBloc[MPCOriginalId].qlinkKnobsCount < 16)
                    myBuff[i + 1] += DeviceInfoBloc[MPCOriginalId].qlinkKnobsCount;

                // tklog_debug("Qlink 0x%02x touch\n",myBuff[i+1] );

                i += 3;
                continue; // next msg
            }

            // Simple key press
            // tklog_debug("Mapping for %d = %d - shift = %d \n", myBuff[i+1], map_ButtonsLeds[ myBuff[i+1] ],map_ButtonsLeds[ myBuff[i+1] +  0x80 ] );

            int mapValue = map_ButtonsLeds[myBuff[i + 1] + (shiftHoldedMode ? 0x80 : 0)];

            // No mapping. Next msg
            if (mapValue < 0)
            {
                PrepareFakeMidiMsg(&myBuff[i]);
                i += 3;
                // tklog_debug("No mapping found for 0x%02x \n",myBuff[i+1]);

                continue; // next msg
            }

            // tklog_debug("Mapping found for 0x%02x : 0x%02x \n",myBuff[i+1],mapValue);

            // Manage shift mapping at destination
            // Not a shift mapping.  Disable the current shift mode
            if (mapValue < 0x80)
            {
                if (myBuff[i + 2] == 0x7f)
                { // Key press
                    if (shiftHoldedMode)
                    {
                        if (size > maxSize - 3)
                            fprintf(stdout, "Warning : midi buffer overflow when inserting SHIFT key release !!\n");
                        memcpy(&myBuff[i + 3], &myBuff[i], size - i);
                        size += 3;
                        myBuff[i + 1] = SHIFT_KEY_VALUE;
                        myBuff[i + 2] = 0x00; // Button SHIFT release insert
                        i += 3;
                    }
                }
            }
            else
            { // Shift at destination
                if (myBuff[i + 2] == 0x7f)
                { // Key press
                    if (!shiftHoldedMode)
                    {
                        if (size > maxSize - 3)
                            fprintf(stdout, "Warning : midi buffer overflow when inserting SHIFT key press !!\n");
                        memcpy(&myBuff[i + 3], &myBuff[i], size - i);
                        size += 3;
                        myBuff[i + 1] = SHIFT_KEY_VALUE;
                        myBuff[i + 2] = 0x07; // Button SHIFT press insert
                        i += 3;
                    }
                }
                mapValue -= 0x80;
            }

            // Key press Post mapping
            // Activate the special column mode when Force spoofed on a MPC
            // Colum mode Button pressed
            switch (mapValue)
            {
            case FORCE_MUTE:
            case FORCE_SOLO:
            case FORCE_REC_ARM:
            case FORCE_CLIP_STOP:
                if (myBuff[i + 2] == 0x7F)
                { // Key press
                    ForceColumnMode = mapValue;
                    Mpc_DrawPadLineFromForceCache(8, MPCPad_OffsetC, 3);
                    Mpc_ShowForceMatrixQuadran(MPCPad_OffsetL, MPCPad_OffsetC);
                }
                else
                {
                    ForceColumnMode = -1;
                    Mpc_ResfreshPadsColorFromForceCache(MPCPad_OffsetL, MPCPad_OffsetC, 4);
                }
                break;
            }
            myBuff[i + 1] = mapValue;

            i += 3;
            continue; // next msg
        }

        // PADS ------------------------------------------------------------------
        if (myBuff[i] == 0x99 || myBuff[i] == 0x89 || myBuff[i] == 0xA9)
        {

            // Remap MPC hardware pad
            // Get MPC pad id in the true order
            uint8_t padM = MPCPadsTable[myBuff[i + 1] - MPCPADS_TABLE_IDX_OFFSET];
            uint8_t padL = padM / 4;
            uint8_t padC = padM % 4;

            // Compute the Force pad id without offset
            uint8_t padF = (3 - padL + MPCPad_OffsetL) * 8 + padC + MPCPad_OffsetC;

            if (shiftHoldedMode || ForceColumnMode >= 0)
            {
                // Ignore aftertouch in special pad modes
                if (myBuff[i] == 0xA9)
                {
                    PrepareFakeMidiMsg(&myBuff[i]);
                    i += 3;
                    continue; // next msg
                }

                uint8_t buttonValue = 0x7F;
                uint8_t offsetC = MPCPad_OffsetC;
                uint8_t offsetL = MPCPad_OffsetL;

                // Columns solo mute mode on first pad line
                if (ForceColumnMode >= 0 && padL == 3)
                    padM = 0x7F; // Simply to pass in the switch case
                // LAUNCH ROW SHIFT + PAD  in column 0 = Launch the corresponding line
                else if (shiftHoldedMode && padC == 0)
                    padM = 0x7E; // Simply to pass in the switch case

                switch (padM)
                {
                // COlumn pad mute,solo   Pads botom line  90 29-30 00/7f
                case 0x7F:
                    buttonValue = 0x29 + padC + MPCPad_OffsetC;
                    break;
                // Launch row.
                case 0x7E:
                    buttonValue = padF / 8 + 56; // Launch row
                    break;
                // Matrix Navigation left Right  Up Down need shift
                case 9:
                    if (shiftHoldedMode)
                        buttonValue = FORCE_LEFT;
                    break;
                case 11:
                    if (shiftHoldedMode)
                        buttonValue = FORCE_RIGHT;
                    break;
                case 14:
                    if (shiftHoldedMode)
                        buttonValue = FORCE_UP;
                    break;
                case 10:
                    if (shiftHoldedMode)
                        buttonValue = FORCE_DOWN;
                    break;
                // PAd as quadran
                case 6:
                    if (ForceColumnMode >= 0)
                    {
                        offsetL = offsetC = 0;
                    }
                    break;
                case 7:
                    if (ForceColumnMode >= 0)
                    {
                        offsetL = 0;
                        offsetC = 4;
                    }
                    break;
                case 2:
                    if (ForceColumnMode >= 0)
                    {
                        offsetL = 4;
                        offsetC = 0;
                    }
                    break;
                case 3:
                    if (ForceColumnMode >= 0)
                    {
                        offsetL = 4;
                        offsetC = 4;
                    }
                    break;
                default:
                    PrepareFakeMidiMsg(&myBuff[i]);
                    i += 3;
                    continue; // next msg
                }

                // Simulate a button press/release
                // to navigate in the matrix , to start a raw, to manage solo mute
                if (buttonValue != 0x7F)
                {
                    // tklog_debug("Matrix shit pad fn = %d \n", buttonValue) ;
                    myBuff[i + 2] = (myBuff[i] == 0x99 ? 0x7F : 0x00);
                    myBuff[i] = 0x90; // MPC Button
                    myBuff[i + 1] = buttonValue;
                    i += 3;
                    continue; // next msg
                }

                // Column button + pad as quadran
                if ((MPCPad_OffsetL != offsetL) || (MPCPad_OffsetC != offsetC))
                {
                    MPCPad_OffsetL = offsetL;
                    MPCPad_OffsetC = offsetC;
                    // tklog_debug("Quadran nav = %d \n", buttonValue) ;
                    Mpc_ResfreshPadsColorFromForceCache(MPCPad_OffsetL, MPCPad_OffsetC, 4);
                    Mpc_ShowForceMatrixQuadran(MPCPad_OffsetL, MPCPad_OffsetC);
                    PrepareFakeMidiMsg(&myBuff[i]);
                    i += 3;
                    continue; // next msg
                }

                // Should not be here
                PrepareFakeMidiMsg(&myBuff[i]);
            }

            // Pad as usual
            else
                myBuff[i + 1] = padF + FORCEPADS_TABLE_IDX_OFFSET;

            i += 3;
        }

        else
            i++;
    }

    return size;
}

///////////////////////////////////////////////////////////////////////////////
// MIDI WRITE - APP ON MPC MAPPING TO FORCE
///////////////////////////////////////////////////////////////////////////////
void Mpc_MapAppWriteToForce(const void *midiBuffer, size_t size)
{

    uint8_t *myBuff = (uint8_t *)midiBuffer;

    bool refreshMutePadLine = false;
    bool refreshOptionPadLines = false;

    size_t i = 0;
    while (i < size)
    {

        // AKAI SYSEX
        // If we detect the Akai sysex header, change the harwware id by our true hardware id.
        // Messages are compatibles. Some midi msg are not always interpreted (e.g. Oled)
        if (myBuff[i] == 0xF0 && memcmp(&myBuff[i], AkaiSysex, sizeof(AkaiSysex)) == 0)
        {
            // Update the sysex id in the sysex for our original hardware
            i += sizeof(AkaiSysex);
            myBuff[i] = DeviceInfoBloc[MPCOriginalId].sysexId;
            i++;

            // SET PAD COLORS SYSEX ------------------------------------------------
            // FN  F0 47 7F [3B] -> 65 00 04 [Pad #] [R] [G] [B] F7
            if (memcmp(&myBuff[i], MPCSysexPadColorFn, sizeof(MPCSysexPadColorFn)) == 0)
            {
                i += sizeof(MPCSysexPadColorFn);

                uint8_t padF = myBuff[i];
                uint8_t padL = padF / 8;
                uint8_t padC = padF % 8;
                uint8_t padM = 0x7F;

                // Update Force pad color cache
                PadSysexColorsCache[padF].r = myBuff[i + 1];
                PadSysexColorsCache[padF].g = myBuff[i + 2];
                PadSysexColorsCache[padF].b = myBuff[i + 3];

                // tklog_debug("Setcolor for Force pad %d (%d,%d)  %02x%02x%02x\n",padF,padL,padC,myBuff[i + 1 ],myBuff[i + 2 ],myBuff[i + 3 ]);

                // Transpose Force pad to Mpc pad in the 4x4 current quadran
                if (padL >= MPCPad_OffsetL && padL < MPCPad_OffsetL + 4)
                {
                    if (padC >= MPCPad_OffsetC && padC < MPCPad_OffsetC + 4)
                    {
                        padM = (3 - (padL - MPCPad_OffsetL)) * 4 + (padC - MPCPad_OffsetC);
                    }
                }

                // Take care of the pad mutes mode line 0 on the MPC, line 8 on Force
                // Shift must not be pressed else it shows the sub option of lines 8/9
                // and not the state of solo/mut/rec arm etc...
                if (padL == 8 && !shiftHoldedMode && ForceColumnMode >= 0)
                    refreshMutePadLine = true;
                else if ((padL == 8 || padL == 9) && shiftHoldedMode && ForceColumnMode < 0)
                    refreshOptionPadLines = true;

                // tklog_debug("Mpc pad transposed : %d \n",padM);

                // Update the pad# in the midi buffer
                myBuff[i] = padM;

                i += 5; // Next msg
            }
        }

        // Buttons-Leds.  In that direction, it's a LED ON / OFF for the button
        // Check if we must remap...

        else if (myBuff[i] == 0xB0)
        {
            if (map_ButtonsLeds_Inv[myBuff[i + 1]] >= 0)
            {
                // tklog_debug("MAP INV %d->%d\n",myBuff[i+1],map_ButtonsLeds_Inv[ myBuff[i+1] ]);
                myBuff[i + 1] = map_ButtonsLeds_Inv[myBuff[i + 1]];
            }
            i += 3;
        }

        else
            i++;
    }

    // Check if mute pad line changed
    if (refreshMutePadLine)
        Mpc_DrawPadLineFromForceCache(8, MPCPad_OffsetC, 3);
    else if (refreshOptionPadLines)
    {

        // Mpc_DrawPadLineFromForceCache(9, 0, 3);
        // Mpc_DrawPadLineFromForceCache(9, 4, 3);
        // Mpc_DrawPadLineFromForceCache(8, 0, 2);
        // Mpc_DrawPadLineFromForceCache(8, 4, 2);
    }
}
