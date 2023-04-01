#define _GNU_SOURCE
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <dlfcn.h>
#include <alsa/asoundlib.h>
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

/**************************************************************************
 *                                                                        *
 *  Callbacks                                                             *
 *                                                                        *
 **************************************************************************/

// Read portion of the default callback
void cb_default_read(const MPCControlToForce_t *force_target, SourceType_t source_type, uint8_t note_number, uint8_t *midi_buffer, size_t buffer_size)
{
    // SET PAD COLORS SYSEX ------------------------------------------------
    //                      v----- We start our midi buffer HERE, our pad # will be at i + sizeof(MPCSysexPadColorFn)
    // FN  F0 47 7F [3B] -> 65 00 04 [Pad #] [R] [G] [B] F7
    // Here, "pad #" is 0 for top-right pad, etc.
    switch (source_type)
    {
    // We remap the note number AND channel.
    case source_pad_note_on:
        midi_buffer[0] = (force_target->note_number >= 0x80 ? 0x99 : 0x90);
        midi_buffer[1] = (force_target->note_number >= 0x80 ? force_target->note_number - 0x80 : force_target->note_number);
        LOG_DEBUG("PAD ON: %02x %02x", midi_buffer[0], midi_buffer[1]);
        break;

    case source_pad_note_off: // XXX do we *HAVE* to do this??
        midi_buffer[0] = (force_target->note_number >= 0x80 ? 0x89 : 0x80);
        midi_buffer[1] = (force_target->note_number >= 0x80 ? force_target->note_number - 0x80 : force_target->note_number);
        break;

    case source_button_on:
        // Then we remap the note number.
        // NOTA: it makes no sense to send a button message to a Force Pad, so we ignore this case
        if (force_target->note_number >= 0x80)
        {
            LOG_DEBUG("Button message from MPC to Force Pad => ignored");
            FakeMidiMessage(midi_buffer, SOURCE_MESSAGE_LENGTH[source_type]);
        }
        else
        {
            midi_buffer[0] = 0x90;
            midi_buffer[1] = force_target->note_number;
        }
        break;

    case source_button_off:
        if (force_target->note_number >= 0x80)
        {
            // NOTA: it makes no sense to send a button message to a Force Pad, so we ignore this case
            LOG_DEBUG("Button message from MPC to Force Pad => ignored");
            FakeMidiMessage(midi_buffer, SOURCE_MESSAGE_LENGTH[source_type]);
        }
        else
        {
            midi_buffer[0] = 0x90;
            midi_buffer[1] = force_target->note_number;
        }
        break;

    case source_pad_aftertouch:
        // We only transmit aftertouch messages for PAD destinations
        if (force_target->note_number >= 0x80)
        {
            midi_buffer[0] = 0xA9;
            midi_buffer[1] = force_target->note_number - 0x80;
        }
        else
        {
            LOG_DEBUG("Aftertouch message from Force to MPC => ignored");
            FakeMidiMessage(midi_buffer, SOURCE_MESSAGE_LENGTH[source_type]);
        }
        break;

    case source_led:
        // Why would we even transmit LED to Force?!
        LOG_DEBUG("LED message from MPC to Force => ignored");
        FakeMidiMessage(midi_buffer, SOURCE_MESSAGE_LENGTH[source_type]);
        break;

    case source_pad_sysex:
        // Why would we even transmit PAD COLOR to Force?!
        LOG_DEBUG("LED message from MPC to Force => ignored");
        FakeMidiMessage(midi_buffer, SOURCE_MESSAGE_LENGTH[source_type]);
        break;

    default:
        // Meh. We transmit anyway (why not?)
        break;
    }
}

// Force ====> MPC
// This is more complicated because it's where SYSEX magic happens.
size_t cb_default_write(const ForceControlToMPC_t *mpc_target, SourceType_t source_type, uint8_t note_number, uint8_t *midi_buffer, size_t buffer_size)
{
    switch (source_type)
    {
    // Ok, pad note or buttons. This just doesn't make sense. Ignore.
    case source_pad_note_on:
    case source_pad_note_off:
    case source_pad_aftertouch:
    case source_button_on:
    case source_button_off:
        LOG_DEBUG("Pad note message from Force to MPC => ignored");
        FakeMidiMessage(midi_buffer, SOURCE_MESSAGE_LENGTH[source_type]);
        break;

    // Led. Interesting case.
    // If destination is a *note*, we just remap and adjust colors.
    // If destination is a *pad*, we store in the pad color buffer.
    case source_led:
        if (mpc_target->note_number < 0x80)
        {
            // We just remap the note number and color
            midi_buffer[1] = mpc_target->note_number;

            switch (mpc_target->color)
            {
            case COLOR_RED:
                midi_buffer[2] = BUTTON_COLOR_RED;
                break;
            case COLOR_YELLOW:
                midi_buffer[2] = BUTTON_COLOR_YELLOW;
                break;
            case COLOR_LIGHT_RED:
                midi_buffer[2] = BUTTON_COLOR_LIGHT_RED;
                break;
            case COLOR_LIGHT_YELLOW:
                midi_buffer[2] = BUTTON_COLOR_LIGHT_YELLOW;
                break;
            case COLOR_ORANGE:
                midi_buffer[2] = BUTTON_COLOR_YELLOW_RED;
                break;
            default:
                LOG_DEBUG("    Unexpected source LED value for %02x: %02x", mpc_target->note_number, mpc_target->color);
                midi_buffer[2] = BUTTON_COLOR_RED;
                break;
            }
        }
        else
        {
            // Target is a pad! We store the color in the pad color buffer.
            // We are in LED update mode so we can refresh it on the spot.
            setLayoutPad(
                mpc_target->bank,
                mpc_target->note_number - 0x80,
                mpc_target->color,
                true);
        }
        break;

    case source_pad_sysex:
        // FORCE PAD ==========> MPC PAD (easy)
        // We are in PAD COLOR update mode so we won't draw it,
        // but we will store it in the pad color buffer.
        if (mpc_target->bank != IAMFORCE_LAYOUT_NONE)
        {
            PadColor_t pad_color = midi_buffer[4] << 16 | midi_buffer[5] << 8 | midi_buffer[6];
            uint_fast8_t pad_number = setLayoutPad(
                mpc_target->bank,
                mpc_target->note_number & 0x7f,
                pad_color,
                false);

            // Transpose MPC pad number or discard message
            if (pad_number == 0xFF)
            {
                // LOG_DEBUG("    WARNING: Discarded");
                return 0;
                // The FakeMidiMessage here doesn't actually work
                // FakeMidiMessage(midi_buffer, SOURCE_MESSAGE_LENGTH[source_type]);
            }
            else
            {
                if (pad_number < 4)
                    pad_number = pad_number + 12;
                else if (pad_number < 8)
                    pad_number = pad_number + 4;
                else if (pad_number < 12)
                    pad_number = pad_number - 4;
                else
                    pad_number = pad_number - 12;
                LOG_DEBUG("PAD SYSEX message from Force note %02x to MPC pad %02x", note_number, pad_number);
                midi_buffer[3] = pad_number;
            }
        }
        // FORCE PAD =======> MPC BUTTON
        else
        {
            LOG_ERROR("PAD SYSEX message from Force pad %02x to MPC button %02x => ignored", note_number, mpc_target->note_number);
        }
        break;

    default:
        // Meh. We transmit anyway (why not?)
        break;
    }

    return SOURCE_MESSAGE_LENGTH[source_type];
}

size_t cb_default(const MPCControlToForce_t *force_target, const ForceControlToMPC_t *mpc_target, SourceType_t source_type, uint8_t note_number, uint8_t *midi_buffer, size_t buffer_size)
{
    // MPC ======> FORCE (this is the easy side!)
    // We skip unconfigured buttons
    if (force_target != NULL && force_target->note_number != 0xff)
    {
        LOG_DEBUG("...map MPC %02x (source type=%d) ===> to Force %02x", note_number, source_type, force_target->note_number);
        cb_default_read(force_target, source_type, note_number, midi_buffer, buffer_size);
    }
    else if (mpc_target != NULL && mpc_target->note_number != 0xff)
    {
        LOG_DEBUG("...map Force %02x (source type=%d) ===> to MPC %02x.%02x", note_number, source_type, mpc_target->bank, mpc_target->note_number);
        return cb_default_write(mpc_target, source_type, note_number, midi_buffer, buffer_size);
    }
    else
    {
        LOG_ERROR("What's this message???");
    }

    return SOURCE_MESSAGE_LENGTH[source_type];
}

size_t cb_tap_tempo(const MPCControlToForce_t *force_target, const ForceControlToMPC_t *mpc_target, SourceType_t source_type, uint8_t note_number, uint8_t *midi_buffer, size_t buffer_size)
{
    // The tools of the trade
    int fd;
    static uint8_t capacity = 0;
    static uint8_t scale = 0;
    static uint8_t blink = 0;
    static uint8_t other_leds = 0x01;
    char buffer[16];
    char intBuffer[4];
    static uint8_t battery_status = BATTERY_UNKNOWN;
    ssize_t bytesRead;

    // If buffer is too small, we can't do much about it
    if (buffer_size < 3)
    {
        LOG_ERROR("Buffer too small for tap callback");
        return 1;
    }

    // Store status
    IAMForceStatus.tap_status = midi_buffer[2] == BUTTON_COLOR_LIGHT_RED ? false : true;

    // Init project if it was not already done
    if (!IAMForceStatus.project_loaded)
    {
        initProject();
        IAMForceStatus.project_loaded = true;
    }

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
            scale += 1;
            other_leds = 0x01;
        }
        else if (strcmp(buffer, "Full\n") == 0)
        {
            battery_status = BATTERY_FULL;
            other_leds = 0x02;
        }
        else
        {
            battery_status = BATTERY_UNKNOWN;
            other_leds = 0x01;
        }
    }

    // If battery is currently charging, we make the last light blink
    // by decreasing the scale by 1
    if (battery_status == BATTERY_CHARGING && !IAMForceStatus.tap_status && scale > 0)
        blink = 1;
    else
        blink = 0;
    // LOG_DEBUG("Battery status: %d - Tap status: %d - Scale: %d => blink=%d", battery_status, IAMForceStatus.tap_status, scale, blink);

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

    // Update lights
    orig_snd_rawmidi_write(rawvirt_outpriv, light_1, sizeof(light_1));
    orig_snd_rawmidi_write(rawvirt_outpriv, light_2, sizeof(light_2));
    orig_snd_rawmidi_write(rawvirt_outpriv, light_3, sizeof(light_3));
    orig_snd_rawmidi_write(rawvirt_outpriv, light_4, sizeof(light_4));

    // Store current status
    IAMForceStatus.battery_status = battery_status;
    IAMForceStatus.battery_capacity = capacity;

    // Handle counter, increment buffer
    IAMForceStatus.tap_counter++;
    if (IAMForceStatus.tap_counter == BATTERY_CHECK_INTERVAL)
        IAMForceStatus.tap_counter = 0;

    return SOURCE_MESSAGE_LENGTH[source_type];
}

// Here we are manipulating edit buttons on the MPC. Let's guess what happens if I press them
// (hint: it all boils down to the IAMForceStatus we're currently in)
void cb_edit_button_read(const MPCControlToForce_t *force_target, const SourceType_t source_type, const uint8_t note_number, uint8_t *midi_buffer, const size_t buffer_size)
{
    // Handle PRESS
    if (midi_buffer[2] == 0x7f)
    {
        LOG_DEBUG("It's a button down");
        switch (note_number)
        {
        case LIVEII_BT_BANK_A:
            if (IAMForceStatus.mode_buttons & MODE_BUTTONS_TOP_MODE)
            {
                setLayout(IAMFORCE_LAYOUT_PAD_BANK_A, false);
                FakeMidiMessage(midi_buffer, buffer_size);
            }
            else
            {
                setLayout(IAMFORCE_LAYOUT_PAD_MODE, false);
                FakeMidiMessage(midi_buffer, buffer_size);
            }
            break;

        case LIVEII_BT_BANK_B:
            if (IAMForceStatus.mode_buttons & MODE_BUTTONS_TOP_MODE)
            {
                setLayout(IAMFORCE_LAYOUT_PAD_BANK_B, false);
                FakeMidiMessage(midi_buffer, buffer_size);
            }
            else
            {
                setLayout(IAMFORCE_LAYOUT_PAD_MUTE, false);
                FakeMidiMessage(midi_buffer, buffer_size);
            }
            break;

        case LIVEII_BT_BANK_C:
            if (IAMForceStatus.mode_buttons & MODE_BUTTONS_TOP_MODE)
            {
                setLayout(IAMFORCE_LAYOUT_PAD_BANK_C, false);
                FakeMidiMessage(midi_buffer, buffer_size);
            }
            else
            {
                setLayout(IAMFORCE_LAYOUT_PAD_COLS, false);
                FakeMidiMessage(midi_buffer, buffer_size);
            }
            break;

        case LIVEII_BT_BANK_D:
            if (IAMForceStatus.mode_buttons & MODE_BUTTONS_TOP_MODE)
            {
                setLayout(IAMFORCE_LAYOUT_PAD_BANK_D, false);
                FakeMidiMessage(midi_buffer, buffer_size);
            }
            else
            {
                setLayout(IAMFORCE_LAYOUT_PAD_SCENE, false);
                FakeMidiMessage(midi_buffer, buffer_size);
            }
            break;

        case LIVEII_BT_NOTE_REPEAT:
            if (IAMForceStatus.mode_buttons & MODE_BUTTONS_BOTTOM_MODE)
            {
                setLayout(IAMFORCE_LAYOUT_PAD_XFDR, false);
                FakeMidiMessage(midi_buffer, buffer_size);
            }
            else
            {
                midi_buffer[1] = FORCE_BT_SELECT;
            }
            break;

        case LIVEII_BT_FULL_LEVEL:
            if (IAMForceStatus.mode_buttons & MODE_BUTTONS_BOTTOM_MODE)
            {
                setLayout(IAMForceStatus.launch_mode_layout, false);
                midi_buffer[1] = FORCE_BT_LAUNCH;
            }
            else
            {
                midi_buffer[1] = FORCE_BT_EDIT;
            }
            break;

        case LIVEII_BT_16_LEVEL:
            if (IAMForceStatus.mode_buttons & MODE_BUTTONS_BOTTOM_MODE)
            {
                setLayout(IAMForceStatus.stepseq_mode_layout, false);
                midi_buffer[1] = FORCE_BT_STEP_SEQ;
            }
            else
            {
                midi_buffer[1] = FORCE_BT_COPY;
            }
            break;

        case LIVEII_BT_ERASE:
            if (IAMForceStatus.mode_buttons & MODE_BUTTONS_BOTTOM_MODE)
            {
                setLayout(IAMForceStatus.note_mode_layout, false);
                midi_buffer[1] = FORCE_BT_NOTE;
            }
            else
            {
                midi_buffer[1] = FORCE_BT_DELETE;
            }
            break;

        default:
            LOG_DEBUG("Should not reach here");
        }
    }

    // Handle RELEASE. Either it's a click and we confirm switch,
    // either it was momentary and we don't;
    else
    {
        LOG_DEBUG("It's a button up");
        switch (note_number)
        {
        case LIVEII_BT_BANK_A:
            if (IAMForceStatus.is_click)
            {
                if (IAMForceStatus.permanent_pad_layout <= IAMFORCE_LAYOUT_PAD_BANK_D)
                    setLayout(IAMFORCE_LAYOUT_PAD_BANK_A, true);
            }
            else
                setLayout(IAMForceStatus.permanent_pad_layout, true);
            FakeMidiMessage(midi_buffer, 3);
            break;
        case LIVEII_BT_BANK_B:
            if (IAMForceStatus.is_click)
            {
                if (IAMForceStatus.permanent_pad_layout <= IAMFORCE_LAYOUT_PAD_BANK_D)
                    setLayout(IAMFORCE_LAYOUT_PAD_BANK_B, true);
            }
            else
                setLayout(IAMForceStatus.permanent_pad_layout, true);
            FakeMidiMessage(midi_buffer, 3);
            break;
        case LIVEII_BT_BANK_C:
            if (IAMForceStatus.is_click)
            {
                if (IAMForceStatus.permanent_pad_layout <= IAMFORCE_LAYOUT_PAD_BANK_D)
                    setLayout(IAMFORCE_LAYOUT_PAD_BANK_C, true);
            }
            else
                setLayout(IAMForceStatus.permanent_pad_layout, true);
            FakeMidiMessage(midi_buffer, 3);
            break;
        case LIVEII_BT_BANK_D:
            if (IAMForceStatus.is_click)
            {
                if (IAMForceStatus.permanent_pad_layout <= IAMFORCE_LAYOUT_PAD_BANK_D)
                    setLayout(IAMFORCE_LAYOUT_PAD_BANK_D, true);
            }
            else
                setLayout(IAMForceStatus.permanent_pad_layout, true);
            FakeMidiMessage(midi_buffer, 3);
            break;

        case LIVEII_BT_NOTE_REPEAT:
            midi_buffer[1] = FORCE_BT_SELECT;
            break;
        case LIVEII_BT_FULL_LEVEL:
            midi_buffer[1] = FORCE_BT_EDIT;
            break;
        case LIVEII_BT_16_LEVEL:
            midi_buffer[1] = FORCE_BT_COPY;
            break;
        case LIVEII_BT_ERASE:
            midi_buffer[1] = FORCE_BT_DELETE;
            break;
        }
    }

    // Ok we're done here
    return;
}

// The write function just handles proper color display in each mode
inline void cb_edit_button_write(const ForceControlToMPC_t *mpc_target, SourceType_t source_type, uint8_t note_number, uint8_t *midi_buffer, size_t buffer_size)
{
    switch (note_number)
    {
    case FORCE_BT_SELECT:
        if (IAMForceStatus.mode_buttons & !MODE_BUTTONS_BOTTOM_MODE)
            midi_buffer[2] = LIVEII_BT_NOTE_REPEAT;
        break;
    case FORCE_BT_EDIT:
        if (IAMForceStatus.mode_buttons & !MODE_BUTTONS_BOTTOM_MODE)
            midi_buffer[2] = LIVEII_BT_FULL_LEVEL;
        break;
    case FORCE_BT_COPY:
        if (IAMForceStatus.mode_buttons & !MODE_BUTTONS_BOTTOM_MODE)
            midi_buffer[2] = LIVEII_BT_16_LEVEL;
        break;
    case FORCE_BT_DELETE:
        if (IAMForceStatus.mode_buttons & !MODE_BUTTONS_BOTTOM_MODE)
            midi_buffer[2] = LIVEII_BT_ERASE;
        break;
    case FORCE_BT_LAUNCH:
        if (IAMForceStatus.mode_buttons & MODE_BUTTONS_BOTTOM_MODE)
        {
            midi_buffer[2] = LIVEII_BT_FULL_LEVEL;
            IAMForceStatus.force_mode = MPC_FORCE_MODE_LAUNCH;
        }
        break;
    case FORCE_BT_STEP_SEQ:
        if (IAMForceStatus.mode_buttons & MODE_BUTTONS_BOTTOM_MODE)
        {
            midi_buffer[2] = LIVEII_BT_16_LEVEL;
            IAMForceStatus.force_mode = MPC_FORCE_MODE_STEPSEQ;
        }
        break;
    case FORCE_BT_NOTE:
        if (IAMForceStatus.mode_buttons & MODE_BUTTONS_BOTTOM_MODE)
        {
            midi_buffer[2] = LIVEII_BT_ERASE;
            IAMForceStatus.force_mode = MPC_FORCE_MODE_NOTE;
        }
        break;
    }
}

// Aaaaah, THE big callback! This is where a lot of funny stuff happens!
size_t cb_mode_e(const MPCControlToForce_t *force_target, const ForceControlToMPC_t *mpc_target, SourceType_t source_type, uint8_t note_number, uint8_t *midi_buffer, size_t buffer_size)
{
    LOG_DEBUG("Entering mode_e callback");

    // Read mode: what's pressed is what matters. We switch in temporary mode.
    if (force_target != NULL)
    {
        // Do a pad by pad mapping
        switch (note_number)
        {
        // 1st line
        case LIVEII_PAD_TL0:
            setLayout(IAMFORCE_LAYOUT_PAD_XFDR, false);
            FakeMidiMessage(midi_buffer, 3);
            break;
        case LIVEII_PAD_TL1:
            setLayout(IAMFORCE_LAYOUT_PAD_SCENE, false);
            FakeMidiMessage(midi_buffer, 3);
            break;
        case LIVEII_PAD_TL2:
            setLayout(IAMFORCE_LAYOUT_PAD_BANK_A, false);
            midi_buffer[0] = 0x90;
            midi_buffer[1] = FORCE_BT_LAUNCH;
            break;
        case LIVEII_PAD_TL3:
            setLayout(IAMFORCE_LAYOUT_PAD_BANK_B, false);
            midi_buffer[0] = 0x90;
            midi_buffer[1] = FORCE_BT_LAUNCH;
            break;

        // 2d line
        case LIVEII_PAD_TL4:
            setLayout(IAMFORCE_LAYOUT_PAD_MUTE, false);
            FakeMidiMessage(midi_buffer, 3);
            break;
        case LIVEII_PAD_TL5:
            setLayout(IAMFORCE_LAYOUT_PAD_COLS, false);
            FakeMidiMessage(midi_buffer, 3);
            break;
        case LIVEII_PAD_TL6:
            setLayout(IAMFORCE_LAYOUT_PAD_BANK_C, false);
            midi_buffer[0] = 0x90;
            midi_buffer[1] = FORCE_BT_LAUNCH;
            break;
        case LIVEII_PAD_TL7:
            setLayout(IAMFORCE_LAYOUT_PAD_BANK_D, false);
            midi_buffer[0] = 0x90;
            midi_buffer[1] = FORCE_BT_LAUNCH;
            break;

        // 3rd line
        case LIVEII_PAD_TL8:
            setLayout(IAMFORCE_LAYOUT_PAD_BANK_A, false);
            midi_buffer[0] = 0x90;
            midi_buffer[1] = FORCE_BT_STEP_SEQ;
            break;
        case LIVEII_PAD_TL9:
            setLayout(IAMFORCE_LAYOUT_PAD_BANK_B, false);
            midi_buffer[0] = 0x90;
            midi_buffer[1] = FORCE_BT_STEP_SEQ;
            break;
        case LIVEII_PAD_TL10:
            setLayout(IAMFORCE_LAYOUT_PAD_BANK_A, false);
            midi_buffer[0] = 0x90;
            midi_buffer[1] = FORCE_BT_NOTE;
            break;
        case LIVEII_PAD_TL11:
            setLayout(IAMFORCE_LAYOUT_PAD_BANK_B, false);
            midi_buffer[0] = 0x90;
            midi_buffer[1] = FORCE_BT_NOTE;
            break;

        case LIVEII_PAD_TL12:
            setLayout(IAMFORCE_LAYOUT_PAD_BANK_C, false);
            midi_buffer[0] = 0x90;
            midi_buffer[1] = FORCE_BT_STEP_SEQ;
            break;
        case LIVEII_PAD_TL13:
            setLayout(IAMFORCE_LAYOUT_PAD_BANK_D, false);
            midi_buffer[0] = 0x90;
            midi_buffer[1] = FORCE_BT_STEP_SEQ;
            break;
        case LIVEII_PAD_TL14:
            setLayout(IAMFORCE_LAYOUT_PAD_BANK_C, false);
            midi_buffer[0] = 0x90;
            midi_buffer[1] = FORCE_BT_NOTE;
            break;
        case LIVEII_PAD_TL15:
            setLayout(IAMFORCE_LAYOUT_PAD_BANK_C, false);
            midi_buffer[0] = 0x90;
            midi_buffer[1] = FORCE_BT_NOTE;
            break;

        default:
            LOG_ERROR("cb_mode_e: unknown note number %d", note_number);
            FakeMidiMessage(midi_buffer, 3);
        }
    }
    else
    {
        // Meh.
        LOG_ERROR("cb_mode_e: can't work in WRITE mode!");
        FakeMidiMessage(midi_buffer, buffer_size);
    }

    LOG_ERROR("Mode e callback returns %d", SOURCE_MESSAGE_LENGTH[source_type]);
    return SOURCE_MESSAGE_LENGTH[source_type];
}

size_t cb_xfader(const MPCControlToForce_t *force_target, const ForceControlToMPC_t *mpc_target, SourceType_t source_type, uint8_t note_number, uint8_t *midi_buffer, size_t buffer_size)
{
    LOG_DEBUG("Entering xfader callback");

    // Read mode: we convert pad hit to the fader value
    if (force_target != NULL)
    {
    }

    // Write mode (if the fader is moved): we convert the fader value to a pad shade
    if (mpc_target != NULL)
    {
        uint8_t fader_value = 127; // XXX hard-coded for now

        // We have 16 pads. We split fader_value so that:
        // The 8 lower pads represent the values that goes from 0 to 127.
        // The 8 higher numbered pads represent the values that goes from 127 to 255.
        // The lower pads are colored in orange, the higher pads in red.
        uint8_t n_full_pads = fader_value / 16;
        for (int i = 0; i < (n_full_pads < 8 ? n_full_pads : 8); i++)
        {
            setLayoutPad(IAMFORCE_LAYOUT_PAD_XFDR, i, COLOR_APRICOT, true);
        }
        for (int i = 8; i < n_full_pads; i++)
        {
            setLayoutPad(IAMFORCE_LAYOUT_PAD_XFDR, i, COLOR_FIRE, true);
        }

        // The remaining value is split between the orange and red pads
        // XXX TODO
    }

    // Completely swallow the message
    return FakeMidiMessage(midi_buffer, buffer_size);
}

size_t cb_shift(const MPCControlToForce_t *force_target, const ForceControlToMPC_t *mpc_target, SourceType_t source_type, uint8_t note_number, uint8_t *midi_buffer, size_t buffer_size)
{
    LOG_DEBUG("Entering shift callback, just saving shift status");
    IAMForceStatus.shift_hold = (note_number == 0x7F);
    return cb_default(force_target, mpc_target, source_type, note_number, midi_buffer, buffer_size);
}

size_t cb_edit_button(const MPCControlToForce_t *force_target, const ForceControlToMPC_t *mpc_target, SourceType_t source_type, uint8_t note_number, uint8_t *midi_buffer, size_t buffer_size)
{
    LOG_DEBUG("Entering edit_button callback");
    if (force_target != NULL)
        cb_edit_button_read(force_target, source_type, note_number, midi_buffer, buffer_size);
    else if (mpc_target != NULL)
        cb_edit_button_write(mpc_target, source_type, note_number, midi_buffer, buffer_size);
    else
        LOG_ERROR("cb_mode_e: both force_target and mpc_target are NULL!");

    // LOG_ERROR("edit button callback returns %d", SOURCE_MESSAGE_LENGTH[source_type]);
    return SOURCE_MESSAGE_LENGTH[source_type];
}

size_t cb_play(const MPCControlToForce_t *force_target, const ForceControlToMPC_t *mpc_target, SourceType_t source_type, uint8_t note_number, uint8_t *midi_buffer, size_t buffer_size)
{
    // XXX CHECK AGAINST BUFFER OVERFLOW
    FakeMidiMessage(midi_buffer, 3);
    return 3;
}
