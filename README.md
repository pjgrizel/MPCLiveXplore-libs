    __ __| |           |  /_) |     ___|             |           |
      |   __ \   _ \  ' /  | |  / |      _ \ __ \   |      _` | __ \   __|
      |   | | |  __/  . \  |   <  |   |  __/ |   |  |     (   | |   |\__ \
     _|  _| |_|\___| _|\_\_|_|\_\\____|\___|_|  _| _____|\__,_|_.__/ ____/
 
-----------------------------------------------------------------------------
## MPCLiveXplore-libs
Special libraries and code for MPC and Force devices
NB : You need SSH images uopdate to use these libraries.

### Docker to cross-build

```
docker build --rm -t pjgrizel/mpc-crosstool .
# Grab a loooong coffee
docker run -v `/bin/pwd`/src:/src/ -ti --rm pjgrizel/mpc-crosstool /bin/bash

cd /src/
export CC=/root/x-tools/armv7-unknown-linux-gnueabihf/bin/armv7-unknown-linux-gnueabihf-cc
LIBRARY=tkgl_mpcmapper ; $CC -Wall -L /root/x-tools/armv7-unknown-linux-gnueabihf/lib ./$LIBRARY.c -o $LIBRARY.so -I /root/alsa-lib/include/ -shared -fPIC -ldl -lasound && scp $LIBRARY.so root@192.168.1.85:/media/Akai/cross-pj/

```

### TKGL_MIDIMAPPER

This LD_PRELOAD library allows you to reconfigure the keys inaccessible in midi learn like PLAY START, STOP on the MPC, or LAUNCH, MATRIX, MIXER on the Force for example. A plugin system allows to load the configuration designed for the midi device you are using. 

```
---------------------------------------------------------
  TKGL_MIDIMAPPER Version : BETA2
  (c) The KikGen Labs.
  https://github.com/TheKikGen/MPC-LiveXplore
  ---------------------------------------------------------
  MPC args : --tkplg=./tmm-plugins/tmm-IamForce-KIKPADMK3.so --tkhelp

  --tkplg specified. File ./tmm-plugins/tmm-IamForce-KIKPADMK3.so will be used for midi mapping

  --tkhelp                       : Show this help
  --tkclient=<client name regex> : Alsa sequencer client name regex used to find the midi controller client.
                                 : Use aconnect -l to find your controller port. 
  --tkport=<port>                : Alsa client sequencer port. Use aconnect -l to find your controller port.
  --tkplg=<plugin file name>     : Use plugin <file name> to transform & route midi events
  --tkdump                       : Dump original raw midi flow
  --tkdumpP                      : Dump raw midi flow after transformation

Example : # LD_PRELOAD=./tkgl_midimapper.so MPC --tkclient=".*MyControllerName.*" --tkport=1 --tkplg=./tmm-plugins/tmm-IamForce2LPMK3.so
```

To create your own plugin, you need to use C language, used for performance reasons. You need  an ARM toolchain and a C compiler (this will be the subject of a later post). For example, I created a plugin for my A800 master keyboard to launch scenes 1 to 8 from pads 1 to 8, and mapped the play, stop, rec keys to the PLAY, STOP ALL, REC keys of my Force. It's very nice to be able to control the workflow from my master keyboard.

IamForce2 is also fully written as a plugin of midimapper. Currently, the following IamForce2 plugins are available (beta versions) :
- Iamforce-APCKEY25MK2 Iamforce2 for the Akai APC Key 25 Mini Mk2
- Iamforce-APCMINIMK2 Iamforce2 for the Akai APC Mini Mk2
- Iamforce-KIKPADMK3 Iamforce2 for the Kikgen Kikpad LMPK3 emaulation
- Iamforce-LPMK3 IamForce2 for the Novation Launchpad mini MK3
- Iamforce-LPX Iamforce for the Novation Launchpad X
- Iamforce-NONE Iamforce in standalone mode no controller mapping)

See detailed documentation in the WIKI (TBD).

NB: you need ssh access to your MPC.

### TKGL_ANYCTRL  TKGL_ANYCTRL_LT (deprecated)

These 2 "low-level" libraries allows you to set up any controller as a control surface to drive the MPC standalone application. 

NB: you need ssh access to your MPC.

When pressing buttons  (START,STOP, MAIN, SHIFT, qlinks, pads, etc...), the hardware MPC/Force surface controller sends a corresponding note on / note off or a cc  midi msg.
By a simple midi message mapping in your own controller, it is possible now to simulate "buttons" press, and get more shortut like those of the MPC X 
(track mute, pad mixer, solo, mute, etc...) or to add more pads or qlinks. 

#### How is it done ?

	# amidi -l
	Dir Device    Name
	IO  hw:1,0,0  MPC Public
	IO  hw:1,0,1  MPC Private
	IO  hw:1,0,2  MPC MIDI Port A
	IO  hw:1,0,3  MPC MIDI Port B
	IO  hw:2,0,0  Midi Out
	IO  hw:2,0,1  Midi Out
	IO  hw:2,0,2  Midi Out
	IO  hw:2,0,3  Midi Out

The "PRIVATE" and "PUBLIC"  ports used by the MPC application to send or capture messages from the MPC controller are replaced by a 3 rawmidi virtual seq ports (usually #134-135-136).   These virtual ports are reconnected to the physical ports of the controller with an alsa connection, similar to the "aconnect" command line ALSA utility, before the sending of SYSEX controller identification sequences.  These virtual ports can be then used to connect any other controller in addition to the standard hardware.  
Note that,as a side effect,  midi running status are inhibited.

So , now, you can add "buttons" allowing direct access to the different screens of the MPC application, as the MPC X or ONE do.  
You can even consider making a dedicated DIY usb controller (check my other Kikpad project) to add buttons or qlinks (like SOLO or MUTE buttons on the MPC Live for example).

Note : MPC users can use the ["global mapping"](https://github.com/TheKikGen/MPC-LiveXplore/wiki/MPC-global-midi-mapping-in-standalone-mode-how-to) approach to get the same result. The Force is not with us, and doesn't have (yet) this controller mode.

#### Quick setup

Copy the tkgl_anyctrl.so library on a usb stick of a smartcard.  
tkgl_anyctrl_lt.so is exactly the same, but will map only private port. It is enough for simple button mapping.  

The "ANYCTRL_NAME" environment varaible can contains a regex pattern matching the name of the midi controller + port  you want to use to simulate a MPC controller. You can use a "aconnect -l" command to get your controller exact ports names.  
To avoid crashes due to potential infinite midi loops, the first midi port of your controller will be disabled within the MPC application, so you will not see it anymore in midi devices setting.

If that variable is not defined, the application will start as usual, but will still use virtual ports in place of hardware ports.
You can retrieve the name of your midi controller with a "aconnect -l | grep client" command.
If you do this in an ssh session manually, don't forget to stop the MPC application with a "systemctl stop inmusic-mpc" command.

The LT version doesn't not remap the "Public" port, if you don't need specific MPC sysex.

Example (after a fresh boot) : 
```
# systemctl stop inmusic-mpc"

# aconnect -l | grep client
client 0: 'System' [type=kernel]
client 20: 'MPC Live Controller' [type=kernel,card=1]
client 24: 'KIKPAD' [type=kernel,card=2]
client 130: 'Virtual MIDI Output 1 Input' [type=user,pid=265]
client 131: 'Virtual MIDI Output 2 Input' [type=user,pid=265]
client 132: 'Virtual MIDI Input 1 Output' [type=user,pid=265]
client 133: 'Virtual MIDI Input 2 Output' [type=user,pid=265]

# export ANYCTRL_NAME="KIKPAD";LD_PRELOAD=/media/tkgl/mpcroot/root/preload_libs/tkgl_anyctrl.so /usr/bin/MPC
------------------------------------
TKGL_ANYCTRL V1.0 by the KikGen Labs
------------------------------------
(tkgl_anyctrl) MPC card id hw:1 found
(tkgl_anyctrl) MPC Private port is hw:1,0,1
(tkgl_anyctrl) MPC Public port is hw:1,0,0
(tkgl_anyctrl) MPC seq client is 20
(tkgl_anyctrl) KIKPAD connect port is 24:0
(tkgl_anyctrl) Virtual private input port 134 created.
(tkgl_anyctrl) Virtual private output port 135 created.
(tkgl_anyctrl) Virtual public output port 136 created.
(tkgl_anyctrl) connection 20:1 to 134:0 successfull
(tkgl_anyctrl) connection 135:0 to 20:1 successfull
(tkgl_anyctrl) connection 136:0 to 20:0 successfull
(tkgl_anyctrl) connection 24:0 to 134:0 successfull
(tkgl_anyctrl) connection 135:0 to 24:0 successfull
(tkgl_anyctrl) connection 136:0 to 24:0 successfull
MPC 2.8.1
failed to change state
failed to switch mux to internal
(tkgl_anyctrl) Port creation disabled for first port of : KIKPAD MIDI IN
(tkgl_anyctrl) Port creation disabled for : Client-135 Virtual RawMIDI
(tkgl_anyctrl) Port creation disabled for : Client-136 Virtual RawMIDI
(tkgl_anyctrl) Port creation disabled for : Client-134 Virtual RawMIDI
(tkgl_anyctrl) hw:1,0,1 substitution by virtual rawmidi successfull
(tkgl_anyctrl) hw:1,0,1 substitution by virtual rawmidi successfull
(tkgl_anyctrl) hw:1,0,0 substitution by virtual rawmidi successfull
**** Audio 44100Hz; 2-in; 6-out; 128sample buffer
**** Warning: inefficient input path: hardware=2 filter=4
**** Warning: inefficient output path: hardware=6 filter=8
MPC Live detected
ButtonStates reply from firmware: {0,0,0,0}
```

#### Make

You need to setup a 2.30 glibc (version used by the MPC app) to avoid incompatibility issues and undefined symbols when using LD_PRELOAD.  The best option is to build a chroot system dedicated for that (it is what I do).   If working on a Linux platform (recommended) you will usually cross compile with "arm-linux-gnueabihf-gcc tkgl_anyctrl.c -o tkgl_anyctrl.so -shared -fPIC -ldl" 

You can also use the "tkgl_anyctrl" module of the tkgl_bootstrap (under construction).

### TKGL_CTRLDUMP

This "low-level" library allows you to spy the traffic between the MPC application and the Akai controller.

```
# LD_PRELOAD=/media/tkgl/mpcroot/root/preload_libs/tkgl_ctrldump.so /usr/bin/MPC
MPC 2.9.0
--------------------------------------
TKGL_CTRLDUMP V1.0 by the KikGen Labs
--------------------------------------
(tkgl_ctrldump) MPC card id hw:1 found
(tkgl_ctrldump) MPC Private port is hw:1,0,1
(tkgl_ctrldump) MPC Public port is hw:1,0,0
(tkgl_ctrldump) snd_rawmidi_open name hw:1,0,1 mode 2
(tkgl_ctrldump) snd_rawmidi_open name hw:1,0,1 mode 3
(tkgl_ctrldump) snd_rawmidi_open name hw:1,0,0 mode 3
**** Audio 44100Hz; 2-in; 6-out; 128sample buffer
**** Warning: inefficient input path: hardware=2 filter=4
**** Warning: inefficient output path: hardware=6 filter=8
MPC      --> hw:1,0,0 = F0 7E 00 06 01 F7                                | .~....

hw:1,0,1 --> MPC      = F0 7E 00 06 02 47 3B 00 19 00 01 01              | .~...G;.....


MPC      --> hw:1,0,0 = F0 47 7F 3B 62 00 01 65 F7 F0 47 7F 3B 64 00 03  | .G.;b..e..G.;d..
                        00 01 01 F7 F0 47 7F 3B 64 00 03 01 01 01 F7 F0  | .....G.;d.......
                        47 7F 3B 64 00 03 02 00 00 F7 F0 47 7F 3B 64 00  | G.;d.......G.;d.
                        03 03 00 00 F7 F0 47 7F 3B 63 00 03 00 00 00 F7  | ......G.;c......
                        F0 47 7F 3B 63 00 03 01 00 00 F7 F0 47 7F 3B 41  | .G.;c.......G.;A
                        00 00 F7 F0 7E 00 06 01 F7                       | ....~....

hw:1,0,1 --> MPC      = F0 47 7F 3B 64 00 03 00 01 01 F7 F0 47 7F 3B 64  | .G.;d.......G.;d
                        00 03 01 01 01 F7 F0 47 7F 3B 64 00 03 02 00 00  | .......G.;d.....
                        F7 F0 47 7F 3B 64 00 03 03 00 0A F7              | ..G.;d......

hw:1,0,1 --> MPC      = F0 47 7F 3B 41 00 13 00 00 00 00 00 00 00 00 00  | .G.;A...........
                        00 00 00 00 00 00 00 00 00 00 F7                 | ...........

```

## TKGL_MPCMAPPER

(DEPRECATED - USE TKGL_MIDIMAPPER INSTEAD)

This "low-level" library allows you to hijack the MPC/Force application to add your own midi mapping to input and output midi messages. 
This library is used for the new version of IamForce, as mpcmapper allows to hack MPCs or Force 

You can use the following options on the LD_PRELOAD command line :
--tgkl_help               : Show this help<br>
--tkgl_ctrlname=<name>    : Use external controller containing <name><br>
--tkgl_iamX               : Emulate MPC X on your current hardware<br>
--tkgl_iamLive            : Emulate MPC Live on your current hardware<br>
--tkgl_iamForce           : Emulate Force on your current hardware<br>
--tkgl_iamOne             : Emulate MPC One on your current hardware<br>
--tkgl_iamLive2           : Emulate MPC Live Mk II  on your current hardware<br>
--tkgl_virtualport=<name> : Create end user virtual port that will be visible within the MPC application<br>
--tkgl_mididump           : Dump original raw midi flow (similar to TKGL_CTRLDUMP) <br>
--tkgl_mididumpPost       : Dump raw midi flow after transformation used to dump midi message after transformation<br>
--tkgl_configfile=<name>  : Use configuration file <name> notably to map current harware buttons to specific emulated device funtion<br>


## TKGL_IAMFORCE

A first derivative POC based on the anyctrl library allowed hardware identity spoofing.
I used that to run the [Force MPC application on an MPC Live](https://github.com/TheKikGen/MPC-LiveXplore/wiki/Iamforce-:-a-MPC-Live-like-a-Force-proof-of-concept) connected to a 8x8 pads controlleur. 

The new IamForce project is based on tkgl_midimapper.
See detailed documentation in the wiki (TBD).


