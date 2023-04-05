/*
__ __| |           |  /_) |     ___|             |           |
  |   __ \   _ \  ' /  | |  / |      _ \ __ \   |      _` | __ \   __|
  |   | | |  __/  . \  |   <  |   |  __/ |   |  |     (   | |   |\__ \
 _|  _| |_|\___| _|\_\_|_|\_\\____|\___|_|  _| _____|\__,_|_.__/ ____/
-----------------------------------------------------------------------------
TKGL_MIDIMAPPER  custom mapping library .
This is a custom midi mapping library for TKGL_MIDIMAPPER LD_PRELOAD library

----------------------------------------------------------------------------
KIKPADMK3 - KIKPAD WITH LAUNCHPAD MINI MK3 FIRMWARE EMULATION FOR IAMFORCE
----------------------------------------------------------------------------

  Disclaimer.
  This work is licensed under the Creative Commons Attribution-NonCommercial 4.0 International License.
  To view a copy of this license, visit http://creativecommons.org/licenses/by-nc/4.0/
  or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
  NON COMMERCIAL - PERSONAL USE ONLY : You may not use the material for pure
  commercial closed code solution without the licensor permission.
  You are free to copy and redistribute the material in any medium or format,
  adapt, transform, and build upon the material.
  You must give appropriate credit, a link to the github site
  https://github.com/TheKikGen/USBMidiKliK4x4 , provide a link to the license,
  and indicate if changes were made. You may do so in any reasonable manner,
  but not in any way that suggests the licensor endorses you or your use.
  You may not apply legal terms or technological measures that legally restrict
  others from doing anything the license permits.
  You do not have to comply with the license for elements of the material
  in the public domain or where your use is permitted by an applicable exception
  or limitation.
  No warranties are given. The license may not give you all of the permissions
  necessary for your intended use.  This program is distributed in the hope that
  it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*/

/*
-------------------------------------------------------------------------------
MAPPING CONFIGURATION WITH FORCE BUTTONS AND FUNCTIONS - KIKPAD MK3


- UP BUTTON aka "Controller SHIFT"
The UP button is used as a "controller SHIFT" button.  This is not the same
"shift" as the one present on the MPC, i.e. it is Laucnhchpad MINI specific.

- DOWN BUTTON :
. Go down in the clip matrix.
. if controller shift holded : go UP in the matrix.


- LEFT BUTTON :
. equivalent to Force copy clip button
. if controller shift holded : go left in the pad matrix

- RIGHT BUTTON :
. equivalent to Force delete clip button
. if controller shift holded : go right in the pad matrix


- "STOP SOLO MUTE" button aka SSM

If "Controller SHIFT" is holded, the SSM button will send a Force LAUNCH 8.

If SSM is holded, the 2 "columns pads" lines are showed on line 7 and 8.  It is
then possible to select the track or mute/solo/stop clip/rec arm tracks by
pressing the right pad.  First line is corresponding to track select, and second
is solo modes.

SSM holded :
+ Pads = Select track and swith solo modes on/Off

+ Pad on the first line = will show track edit on the selected pad column

+ Launch 7 = change current solo mode . The SSM button color will change
accordingly to the current mode : red = rec arm, orange = mute, blue = solo,
green = clip stop.

+ LAUNCH 6 = STOP ALL CLIPS. Same behavious as Force

+ Launch 5 = MPC SHIFT key.  This can be used to show options settings on the 2
last lines rather than solo modes (same behaviour as Force).  You can use that
to set metronome on/off or transpose the keyboard for example.

+ Launch 4 = lock the SSM mode ON/OFF. The 2 last lines of the Launchpad will
show columns pads permanenently.

- LAUNCH 1-7
. If not in controller shift mode or SSM mode, equivalent to Force launch buttons.

- LAUNCH 8 (SSM button)
. if controller shift mode is holded, that will generate a Force launch 8 button.

- SESSION button :
. if controller shift mode is holded : MASTER
. else : LAUNCH

- USER button : STEP SEQ

- KEYS button :   NOTE

-------------------------------------------------------------------------------
*/

#define IAMFORCE_DRIVER_VERSION "1.0"
#define IAMFORCE_DRIVER_ID "KIKPADMK3"
#define IAMFORCE_DRIVER_NAME "The KiGen Labs Kikpad Mini Mk3 emulation"
#define IAMFORCE_ALSASEQ_DEFAULT_CLIENT_NAME "Launchpad Mini MK3.*KIKPAD.*"
#define IAMFORCE_ALSASEQ_DEFAULT_PORT 0

#define LP_DEVICE_ID 0x0D // Launchpad Mini MK3

#define LP_SYSEX_HEADER 0xF0,0x00,0x20,0x29,0x02, LP_DEVICE_ID

// Kikpad buttons

#define CTRL_BT_VOLUME 0x50
#define CTRL_BT_SEND_A 0x46
#define CTRL_BT_SEND_B 0x3C
#define CTRL_BT_PAN 0x32
#define CTRL_BT_CONTROL_1 0X28
#define CTRL_BT_CONTROL_2 0x1E
#define CTRL_BT_CONTROL_3 0x14
#define CTRL_BT_CONTROL_4 0x0A

#define CTRL_BT_UP 0x5B
#define CTRL_BT_DOWN 0x5C
#define CTRL_BT_LEFT 0x5D
#define CTRL_BT_RIGHT 0x5E
#define CTRL_BT_CLIP 0x5F
#define CTRL_BT_MODE_1 0x60
#define CTRL_BT_MODE_2 0x61
#define CTRL_BT_SET 0x62

#define CTRL_BT_LAUNCH_1 0x59
#define CTRL_BT_LAUNCH_2 0x4F
#define CTRL_BT_LAUNCH_3 0x45
#define CTRL_BT_LAUNCH_4 0x3B
#define CTRL_BT_LAUNCH_5 0x31
#define CTRL_BT_LAUNCH_6 0x27
#define CTRL_BT_LAUNCH_7 0x1D
#define CTRL_BT_LAUNCH_8 0x13

// Pads start at 0 from bottom left to upper right
#define CTRL_PAD_NOTE_OFFSET 0
#define CTRL_PAD_MAX_LINE 8
#define CTRL_PAD_MAX_COL  8

// Knobs are relative
#define CTRL_KNOB_1 0x00
#define CTRL_KNOB_2 0x01
#define CTRL_KNOB_3 0x02
#define CTRL_KNOB_4 0x03
#define CTRL_KNOB_5 0x04
#define CTRL_KNOB_6 0x05
#define CTRL_KNOB_7 0x06
#define CTRL_KNOB_8 0x07

// Standard colors
#define CTRL_COLOR_BLACK 0
#define CTRL_COLOR_DARK_GREY 1
#define CTRL_COLOR_GREY 2
#define CTRL_COLOR_WHITE 3
#define CTRL_COLOR_RED 5
#define CTRL_COLOR_RED_LT 0x07
#define CTRL_COLOR_AMBER 9
#define CTRL_COLOR_YELLOW 13
#define CTRL_COLOR_LIME 17
#define CTRL_COLOR_GREEN 21
#define CTRL_COLOR_SPRING 25
#define CTRL_COLOR_TURQUOISE 29
#define CTRL_COLOR_CYAN 33
#define CTRL_COLOR_SKY 37
#define CTRL_COLOR_OCEAN 41
#define CTRL_COLOR_BLUE 45
#define CTRL_COLOR_ORCHID 49
#define CTRL_COLOR_MAGENTA 53
#define CTRL_COLOR_PINK 57

// SYSEX for Launchpad mini Mk3

const uint8_t SX_DEVICE_INQUIRY[] = { 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 };
const uint8_t SX_LPMK3_INQUIRY_REPLY_APP[] = { 0xF0, 0x7E, 0x00, 0x06, 0x02, 0x00, 0x20, 0x29, 0x13, 0x01, 0x00, 0x00 } ;
//const uint8_t SX_LPMK3_PGM_MODE = { 0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D, 0x00, 0x7F, 0xF7} ;
const uint8_t SX_LPMK3_PGM_MODE[]  = { LP_SYSEX_HEADER, 0x0E, 0x01, 0xF7 } ;
const uint8_t SX_LPMK3_DAW_MODE[]  = { LP_SYSEX_HEADER, 0x10, 0x01, 0xF7 } ;
const uint8_t SX_LPMK3_STDL_MODE[] = { LP_SYSEX_HEADER, 0x10, 0x00, 0xF7 } ;
const uint8_t SX_LPMK3_DAW_CLEAR[] = { LP_SYSEX_HEADER, 0x12, 0x01, 0x00, 0X01, 0xF7 } ;

//F0h 00h 20h 29h 02h 0Dh 07h [<loop> <speed> 0x01 R G B [<text>] ] F7h
const uint8_t SX_LPMK3_TEXT_SCROLL[] = { LP_SYSEX_HEADER, 0x07 };

// 0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D, 0x03, 0x03 (Led Index) ( r) (g)  (b)
uint8_t SX_LPMK3_LED_RGB_COLOR[] = { LP_SYSEX_HEADER, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00, 0xF7 };

///////////////////////////////////////////////////////////////////////////////
// KikPad Mini Mk3 Specifics
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// KIKPAD Set a pad or button static color
///////////////////////////////////////////////////////////////////////////////
static void ControllerSetLedColor(uint8_t padCt, uint8_t color) {

  snd_seq_event_t ev;
  snd_seq_ev_clear	(&ev);

  SetMidiEventDestination(&ev, TO_CTRL_EXT );
  ev.type=SND_SEQ_EVENT_NOTEON;
  ev.data.note.channel = 0;
  ev.data.note.note = padCt ;
  ev.data.note.velocity = color;
  SendMidiEvent(&ev );

}

///////////////////////////////////////////////////////////////////////////////
// KIKPAD Set a pad RGB Colors
///////////////////////////////////////////////////////////////////////////////
static void ControllerSetPadColorRGB(uint8_t padCt, uint8_t r, uint8_t g, uint8_t b) {

  // 0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D, 0x03, 0x03 (Led Index) ( r) (g)  (b)
  //xxrgbRGB

uint8_t p =2;

  //if ( r > 43*2  && r < 110  ) r = 43*2;
  //if ( g > 0 && g < 127 - 43 ) g += 43;
  //if ( b > 0 && b < 127 - 43 ) b += 43;

  SX_LPMK3_LED_RGB_COLOR[8]  = padCt;
  SX_LPMK3_LED_RGB_COLOR[9]  = r ;
  SX_LPMK3_LED_RGB_COLOR[10] = g ;
  SX_LPMK3_LED_RGB_COLOR[11] = b ;

  
  SeqSendRawMidi( TO_CTRL_EXT,SX_LPMK3_LED_RGB_COLOR,sizeof(SX_LPMK3_LED_RGB_COLOR) );

}

///////////////////////////////////////////////////////////////////////////////
// Show a line from Force pad color cache (source = Force pad#, Dest = ctrl pad #)
///////////////////////////////////////////////////////////////////////////////
static void ControllerDrawPadsLineFromForceCache(uint8_t SrcForce, uint8_t DestCtrl) {

    if ( DestCtrl >= CTRL_PAD_MAX_LINE ) return;

    uint8_t pf = SrcForce * 8 ;
    uint8_t pl = ( DestCtrl * 10 ) + 11;

    for ( uint8_t i = 0  ; i <  8 ; i++) {
      ControllerSetPadColorRGB(pl, Force_PadColorsCache[pf].c.r, Force_PadColorsCache[pf].c.g,Force_PadColorsCache[pf].c.b);
      pf++; pl++;
    }
}


///////////////////////////////////////////////////////////////////////////////
// Set color for overall pad matrix 
///////////////////////////////////////////////////////////////////////////////
static void ControllerSetPadMatrixRGB(uint8_t r, uint8_t g, uint8_t b) {

    for ( int i = 0 ; i< 64 ; i++) {
      ControllerSetPadColorRGB(i, Force_PadColorsCache[i].c.r, Force_PadColorsCache[i].c.g,Force_PadColorsCache[i].c.b);
    }
}

///////////////////////////////////////////////////////////////////////////////
// Refresh the pad surface from Force pad cache
///////////////////////////////////////////////////////////////////////////////
static void ControllerRefreshMatrixFromForceCache() {

    for ( int i = 0 ; i< 64 ; i++) {

      // Set pad for external controller eventually
      uint8_t padCt = ( ( 7 - i / 8 ) * 10 + 11 + i % 8 );
      ControllerSetPadColorRGB(padCt, Force_PadColorsCache[i].c.r, Force_PadColorsCache[i].c.g,Force_PadColorsCache[i].c.b);
    }
}

///////////////////////////////////////////////////////////////////////////////
// Controller initialization
///////////////////////////////////////////////////////////////////////////////
static int ControllerInitialize() {

  tklog_info("IamForce : %s implementation, version %s.\n",IAMFORCE_DRIVER_NAME,IAMFORCE_DRIVER_VERSION);

  ControllerSetPadMatrixRGB(0,0,0 ) ;

  snd_seq_event_t ev;
  snd_seq_ev_clear	(&ev);
  SetMidiEventDestination(&ev, TO_CTRL_EXT );
  ev.type=SND_SEQ_EVENT_NOTEON;
  ev.data.note.channel = 0;

  for ( int i = 0 ; i < 64 ; i++ ) {
    ev.data.note.note = ControllerGetPadIndex(i) ;
    ev.data.note.velocity = i;
    SendMidiEvent(&ev );
  } 


  // ControllerSetPadMatrixRGB(0,0,0 ) ;
  // uint8_t p = 43;

  // for ( int i = p ; i < p+64 ; i++ ) {
  //     ControllerSetPadColorRGB(ControllerGetPadIndex(i - p), i,0,0);
  //     //tklog_debug("Moyenne %d = %f \n",i,( i+i+i)/3.0);
  // } 
//exit(0);



}

///////////////////////////////////////////////////////////////////////////////
// Get a Force pad index from a Launchpad index
///////////////////////////////////////////////////////////////////////////////
static uint8_t ControllerGetForcePadIndex(uint8_t padCt) {
  // Convert pad to Force pad #
  padCt -=  11;
  uint8_t padL  =  7 - padCt  /  10 ;
  uint8_t padC  =  padCt  %  10 ;
  return  padL * 8 + padC ;
}

///////////////////////////////////////////////////////////////////////////////
// Get a Force pad note from a Launchpad index
///////////////////////////////////////////////////////////////////////////////
static uint8_t ControllerGetForcePadNote(uint8_t padCt) {
  return  ControllerGetForcePadIndex(padCt)  + FORCEPADS_NOTE_OFFSET;
}

///////////////////////////////////////////////////////////////////////////////
// Get a Controller pad index from a Force pad index
///////////////////////////////////////////////////////////////////////////////
static int ControllerGetPadIndex(uint8_t padF) {

  if ( padF >= 64 ) return -1;

  return  ( ( 7 - padF / 8 ) * 10 + 11 + padF % 8 ) ;
}

///////////////////////////////////////////////////////////////////////////////
// Map LED lightning messages from Force with Launchpad buttons leds colors
///////////////////////////////////////////////////////////////////////////////
static void ControllerSetMapButtonLed(snd_seq_event_t *ev) {

    int mapVal = -1 ;
    int mapVal2 = -1;

    // TAP LED is better at first ! Always updated....
    if ( ev->data.control.param == FORCE_BT_TAP_TEMPO )  {
          //mapVal = CTRL_BT_LOGO;
          //mapVal2 = ev->data.control.value == 3 ?  CTRL_COLOR_RED_LT:00 ;
    }
  
    if ( ev->data.control.param == FORCE_BT_NOTE )       mapVal = CTRL_BT_MODE_2;
    if ( ev->data.control.param == FORCE_BT_STEP_SEQ )   mapVal = CTRL_BT_SET;

    else if ( ev->data.control.param == FORCE_BT_LAUNCH )     mapVal = CTRL_BT_CLIP ;
    else if ( ev->data.control.param == FORCE_BT_MASTER )     mapVal = CTRL_BT_CLIP ;


    else if ( ev->data.control.param == FORCE_BT_LAUNCH_1 )   mapVal = CTRL_BT_LAUNCH_1  ;
    else if ( ev->data.control.param == FORCE_BT_LAUNCH_2 )   mapVal = CTRL_BT_LAUNCH_2  ;
    else if ( ev->data.control.param == FORCE_BT_LAUNCH_3 )   mapVal = CTRL_BT_LAUNCH_3  ;
    else if ( ev->data.control.param == FORCE_BT_LAUNCH_4 )   mapVal = CTRL_BT_LAUNCH_4  ;
    else if ( ev->data.control.param == FORCE_BT_LAUNCH_5 )   mapVal = CTRL_BT_LAUNCH_5  ;
    else if ( ev->data.control.param == FORCE_BT_LAUNCH_6 )   mapVal = CTRL_BT_LAUNCH_6  ;
    else if ( ev->data.control.param == FORCE_BT_LAUNCH_7 )   mapVal = CTRL_BT_LAUNCH_7  ;

    else if ( ev->data.control.param == FORCE_BT_RIGHT )  mapVal = CTRL_BT_RIGHT    ;
    else if ( ev->data.control.param == FORCE_BT_LEFT )   mapVal = CTRL_BT_LEFT   ;
    else if ( ev->data.control.param == FORCE_BT_UP )      ;
    else if ( ev->data.control.param == FORCE_BT_DOWN )     mapVal = CTRL_BT_DOWN ;

    else if ( ev->data.control.param == FORCE_BT_MUTE )   {
      if ( ev->data.control.value == 3 ) {
        mapVal = CTRL_BT_LAUNCH_8   ;
        
        // Set color of "Stop Solo Mode button"
        mapVal2 = CTRL_COLOR_AMBER ;
      }
    }

    else if ( ev->data.control.param == FORCE_BT_SOLO )   {
      if ( ev->data.control.value == 3 ) {
        mapVal = CTRL_BT_LAUNCH_8   ;
     
        // Set color of "Stop Solo Mode button"
        mapVal2 = CTRL_COLOR_BLUE ;
      }
    }

    else if ( ev->data.control.param == FORCE_BT_REC_ARM ) {
      if ( ev->data.control.value == 3 ) {
        mapVal = CTRL_BT_LAUNCH_8   ;
     
        // Set color of "Stop Solo Mode button"
        mapVal2 = CTRL_COLOR_RED ;
      }
    }

    else if ( ev->data.control.param == FORCE_BT_CLIP_STOP )   {
      if ( ev->data.control.value == 3 ) {
        mapVal = CTRL_BT_LAUNCH_8   ;
    
        // Set color of "Stop Solo Mode button"
        mapVal2 = CTRL_COLOR_GREEN ;
      }
    }

    if ( mapVal >=0 ) {
        snd_seq_event_t ev2 = *ev;
        ev2.data.control.param = mapVal;
        if ( mapVal2 >= 0 ) ev2.data.control.value = mapVal2 ;
        else {
          uint8_t colMap[]={0x75,0x01,0X13,0x29,0x57};
          ev2.data.control.value = colMap[ev->data.control.value] ;
        }

        SetMidiEventDestination(&ev2, TO_CTRL_EXT );
        SendMidiEvent(&ev2 );
    }

}

///////////////////////////////////////////////////////////////////////////////
// Refresh columns mode lines 7 & 8 on the LaunchPad
///////////////////////////////////////////////////////////////////////////////
static void ControllerRefreshColumnsPads(bool show) {
  if ( show ) {
    // Line 9 = track selection
    // Line 8 = mute modes
    ControllerDrawPadsLineFromForceCache(9,1);
    ControllerDrawPadsLineFromForceCache(8,0);
  }
  else {
    ControllerDrawPadsLineFromForceCache(6,1);
    ControllerDrawPadsLineFromForceCache(7,0);
  }
}

///////////////////////////////////////////////////////////////////////////////
// Process an event received from the Kikpad
///////////////////////////////////////////////////////////////////////////////
static bool ControllerEventReceived(snd_seq_event_t *ev) {

//tklog_debug("Controller event received\n");
  switch (ev->type) {
    case SND_SEQ_EVENT_SYSEX:
      break;

    case SND_SEQ_EVENT_CONTROLLER:

      // Knobs 1 - 8
      if ( ev->data.control.channel == 4 ) {

        if ( ev->data.control.param >= CTRL_KNOB_1 && ev->data.control.param  <= CTRL_KNOB_8 ) {

          static uint8_t lastKnob = 0xFF;

          SetMidiEventDestination(ev,TO_MPC_PRIVATE );
          ev->data.control.channel = 0 ;

          uint8_t k = ev->data.control.param - CTRL_KNOB_1 ;
          //tklog_debug("Knob no %d\n",k);

          // Remap controller
          ev->data.control.param = FORCE_KN_QLINK_1 + k;
          //tklog_debug("Knob remap %02x\n",ev->data.control.param);

          if ( lastKnob != k  ) {
            snd_seq_event_t ev2 = *ev;
            ev2.data.note.channel = 0;
            SetMidiEventDestination(&ev2,TO_MPC_PRIVATE );

            if ( lastKnob != 0xFF) {
              // Simulate an "untouch", but not the first time
              ev2.type = SND_SEQ_EVENT_NOTEOFF;
              ev2.data.note.velocity = 0x00;
              ev2.data.note.note = FORCE_BT_QLINK1_TOUCH + lastKnob ;
              SendMidiEvent(&ev2 );
              //tklog_debug("Knob untouch %02x\n",ev2.data.note.note);
            }

            // Simulate a "touch" knob
            ev2.type = SND_SEQ_EVENT_NOTEON;
            ev2.data.note.velocity = 0x7F;
            ev2.data.note.note = FORCE_BT_QLINK1_TOUCH + k ;
            SendMidiEvent(&ev2 );
            //tklog_debug("Knob touch %02x\n",ev2.data.note.note);
          }

          lastKnob = k;
        }
 
        break;

      }

      else
      // Buttons around Kikpad mini pads
      if ( ev->data.control.channel == 0 ) {
        int mapVal = -1;

        if       ( ev->data.control.param == CTRL_BT_SET) { // ^
          // SET holded = shitfmode on the Launchpad
          CtrlShiftMode = ( ev->data.control.value == 0x7F );
          ControllerColumnsPadMode = CtrlShiftMode; 
          ControllerRefreshColumnsPads(ControllerColumnsPadMode);
          return false;
        }


        else if  ( ev->data.control.param == CTRL_BT_MODE_1  ) { // ^
          mapVal = FORCE_BT_MATRIX ;
        }

        else if  ( ev->data.control.param == CTRL_BT_MODE_2  ) { // v
          mapVal = FORCE_BT_MENU ;
        }



        else if  ( ev->data.control.param == CTRL_BT_UP  ) { // ^
          mapVal = CtrlShiftMode ? FORCE_BT_UP : FORCE_BT_COPY ;
        }

        else if  ( ev->data.control.param == CTRL_BT_DOWN  ) { // v
          // Shift down = UP
          mapVal = CtrlShiftMode ? FORCE_BT_DOWN : FORCE_BT_DELETE ;
        }

        else if  ( ev->data.control.param == CTRL_BT_LEFT ) { // <
          mapVal = FORCE_BT_LEFT   ;
        }

        else if  ( ev->data.control.param == CTRL_BT_RIGHT ) { // >
          mapVal = FORCE_BT_RIGHT ;
        }

        // Launch buttons
        else if  ( ev->data.control.param == CTRL_BT_LAUNCH_1 ) { // >
          mapVal = FORCE_BT_LAUNCH_1 ;
        }
        else if  ( ev->data.control.param == CTRL_BT_LAUNCH_2 ) { // >
          mapVal = FORCE_BT_LAUNCH_2 ;
        }
        else if  ( ev->data.control.param == CTRL_BT_LAUNCH_3 ) { // >
          mapVal = FORCE_BT_LAUNCH_3 ;
        }

        else if  ( ev->data.control.param == CTRL_BT_LAUNCH_4 ) { // >
          mapVal = FORCE_BT_LAUNCH_4 ;
        }

        else if  ( ev->data.control.param == CTRL_BT_LAUNCH_5 ) { // >
          if ( ControllerColumnsPadMode  ) {
            // Lock Columns pads mode
            if ( ev->data.control.value !=0 )  ControllerColumnsPadModeLocked = ! ControllerColumnsPadModeLocked;
            return false;
          }
          mapVal = FORCE_BT_LAUNCH_5 ;
        }

        else if  ( ev->data.control.param == CTRL_BT_LAUNCH_6 ) { // >
          // Launch 6 generates a FORCE SHIFT FOR columns options setting
          // in columns pads mode
          mapVal = ControllerColumnsPadMode ? FORCE_BT_SHIFT : FORCE_BT_LAUNCH_6 ;
        }

        else if  ( ev->data.control.param == CTRL_BT_LAUNCH_7 ) { // >
          // Launch 7 generates a STOP ALL in columns pads mode
          mapVal = ControllerColumnsPadMode ? FORCE_BT_STOP_ALL : FORCE_BT_LAUNCH_7 ;
        }

        else if  ( ev->data.control.param == CTRL_BT_LAUNCH_8 ) { // >

          // Launch 8 is used to change column pads mode in columns pads mode
          if ( ControllerColumnsPadMode ) {
            if ( ev->data.control.value != 0 ) {
              if ( ++CurrentSoloMode >= FORCE_SM_END ) CurrentSoloMode = 0;
            }
            mapVal = SoloModeButtonMap[CurrentSoloMode];
          }
          else {
            mapVal = FORCE_BT_LAUNCH_8;
          }
        }

        else if  ( ev->data.control.param == CTRL_BT_CLIP ) { 
          mapVal = FORCE_BT_CLIP ;
        }

        else if  ( ev->data.control.param == CTRL_BT_VOLUME ) { 
          mapVal = FORCE_BT_MIXER ;
        }

        else if  ( ev->data.control.param == CTRL_BT_SEND_A ) { 
          mapVal = FORCE_BT_ASSIGN_A ;
        }

        else if  ( ev->data.control.param == CTRL_BT_SEND_B ) { 
          mapVal = FORCE_BT_ASSIGN_B ;
        }

        else if  ( ev->data.control.param == CTRL_BT_PAN ) { 
          mapVal = FORCE_BT_MASTER;
        }

        else if  ( ev->data.control.param == CTRL_BT_CONTROL_1 ) { 
          mapVal = FORCE_BT_LAUNCH ;
        }

        else if  ( ev->data.control.param == CTRL_BT_CONTROL_2 ) { 
          mapVal = FORCE_BT_NOTE ;
        }

        else if  ( ev->data.control.param == CTRL_BT_CONTROL_3 ) { 
          mapVal = FORCE_BT_STEP_SEQ ;
        }


        if ( mapVal >= 0 ) {

            SendDeviceKeyEvent(mapVal,ev->data.control.value);

            return false;

        }

//        dump_event(ev);

      }
      break;

    case SND_SEQ_EVENT_NOTEON:
    case SND_SEQ_EVENT_NOTEOFF:
    case SND_SEQ_EVENT_KEYPRESS:
      if ( ev->data.note.channel == 0 ) {
        // Kikpad mini pads
        ev->data.note.note = ControllerGetForcePadNote(ev->data.note.note);
        SetMidiEventDestination(ev,TO_MPC_PRIVATE );
        // If controller column mode, simulate the columns and mutes pads
        if ( ControllerColumnsPadMode) {

            if ( ev->type == SND_SEQ_EVENT_KEYPRESS ) return false;

            uint8_t padFL = (ev->data.note.note - FORCEPADS_NOTE_OFFSET ) / 8 ;
            uint8_t padFC = ( ev->data.note.note - FORCEPADS_NOTE_OFFSET ) % 8 ;

            // Edit current track
            if ( padFL == 0 ) {

              ev->data.note.note = FORCE_BT_COLUMN_PAD1 + padFC ;
              ev->data.note.velocity = ( ev->data.note.velocity > 0 ? 0x7F:00);
              
              if ( ev->data.note.velocity == 0x7F ) {
                SendDeviceKeyEvent(FORCE_BT_EDIT, 0x7F) ; // Send Edit Press
                SendMidiEvent(ev); // Send Pad On
                ev->data.note.velocity = 0 ;
                SendMidiEvent(ev); // Send Pad off
              }
              else SendDeviceKeyEvent(FORCE_BT_EDIT, 0); // Send Edit Off

              return false;
            }

            else if ( padFL == 6 ) { // Column pads (on the mini launchpad)
              ev->data.note.note = FORCE_BT_COLUMN_PAD1 + padFC;
              ev->data.note.velocity = ( ev->data.note.velocity > 0 ? 0x7F:00);

            }
            else if ( padFL == 7 ) { // Mute mode pads (on the mini lLaunchpad)
              ev->data.note.note = FORCE_BT_MUTE_PAD1 + padFC;
              ev->data.note.velocity = ( ev->data.note.velocity > 0 ? 0x7F:00);
            }
        }
        else { 
          ev->data.note.channel = 9; // Note Force pad

            // If Shift Mode, simulate Select key
          if ( CtrlShiftMode ) {
            SendDeviceKeyEvent(FORCE_BT_SELECT,0x7F);
            SendMidiEvent(ev);
            SendDeviceKeyEvent(FORCE_BT_SELECT,0);
            return false;
          }
        }
      }
  }

  return true;
}
