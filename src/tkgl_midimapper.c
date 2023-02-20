/*
__ __| |           |  /_) |     ___|             |           |
  |   __ \   _ \  ' /  | |  / |      _ \ __ \   |      _` | __ \   __|
  |   | | |  __/  . \  |   <  |   |  __/ |   |  |     (   | |   |\__ \
 _|  _| |_|\___| _|\_\_|_|\_\\____|\___|_|  _| _____|\__,_|_.__/ ____/
-----------------------------------------------------------------------------
TKGL_MIDIMAPPER  LD_PRELOAD library.
This "low-level" library allows you to hijack the MPC/Force application to add
your own midi mapping to input and output midi messages.

-----------------------------------------------------------------------------

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

#define _GNU_SOURCE
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#include <alsa/asoundlib.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>

#include <dlfcn.h>
#include <libgen.h>
#include <regex.h>
#include <time.h>
#include <pthread.h>

// midimapper defines -----------------------------------------------------------

#include "tkgl_midimapper.h"

// Log utilities ---------------------------------------------------------------

#include "tkgl_logutil.h"

// Globals ---------------------------------------------------------------------

// Ports and clients used by mpcmapper and router thread
TkRouter_t TkRouter = {

    // Mpc surface controller
    .MpcHW.card = -1, .MpcHW.cli = -1,  .MpcHW.portPriv =-1, .MpcHW.portPub =-1,

    // Tkgl virtual ports exposing MPC rawmidi app ports
    .VirtRaw.cliPrivOut =-1, .VirtRaw.cliPrivIn = -1, .VirtRaw.cliPubOut = -1,

    // External midi controller
    .Ctrl.card =-1, .Ctrl.cli = -1 , .Ctrl.port = -1,

    // External MPC mirroring midi controller
    .MPCCtrl.cli = -1 , .MPCCtrl.portIn = -1, .MPCCtrl.portOut = -1,

    // Router ports
    .cli = -1,
    .portPriv = -1, .portCtrl = -1, .portPub = -1,
    .portMpcPriv = -1, .portMpcPub = -1,
    .portAppCtrl = -1,

    // Seq
    .seq = NULL,
};

// Working midi buffer
static uint8_t MidiWkBuffer[MIDI_DECODER_SIZE];

// Midi router Thread blocker
pthread_t    MidiRouterThread;

// Raw midi dump flag (for debugging purpose)
static uint8_t rawMidiDumpFlag = 0 ;     // Before transformation
static uint8_t rawMidiDumpPostFlag = 0 ; // After our tranformation

// Midi transformation dynamic library
static ssize_t (*MidiMapper) ( uint8_t sender, snd_seq_event_t *ev );
static void (*MidiMapperStart) () ;
static void (*MidiMapperSetup) () ;
static void *midiMapperLibHandle = NULL ;
static char *midiMapperLibFileName = NULL;

// MPC id
static int MPC_Id = -1;

// MPC alsa names
static char mpc_midi_private_alsa_name[20];
static char mpc_midi_public_alsa_name[20];

// Our midi controller port name
static char anyctrl_name[128] ;
static int anyctrl_midiport = -1; // port to capture. Zero by default.


// Virtual rawmidi pointers to fake the MPC app
static snd_rawmidi_t *rawvirt_inpriv  = NULL;
static snd_rawmidi_t *rawvirt_outpriv = NULL ;
static snd_rawmidi_t *rawvirt_outpub  = NULL ;

// Alsa API hooks declaration
static typeof(&snd_rawmidi_open) orig_snd_rawmidi_open;
static typeof(&snd_rawmidi_close) orig_snd_rawmidi_close;
static typeof(&snd_rawmidi_read) orig_snd_rawmidi_read;
static typeof(&snd_rawmidi_write) orig_snd_rawmidi_write;
static typeof(&snd_seq_create_simple_port) orig_snd_seq_create_simple_port;
static typeof(&snd_midi_event_decode) orig_snd_midi_event_decode;
static typeof(&snd_seq_open) orig_snd_seq_open;
static typeof(&snd_seq_close) orig_snd_seq_close;
static typeof(&snd_seq_port_info_set_name) orig_snd_seq_port_info_set_name;
//static typeof(&snd_seq_event_input) orig_snd_seq_event_input;

// Globals used to rename a virtual port and get the client id.  No other way...
static int  snd_seq_virtual_port_rename_flag  = 0;
static char snd_seq_virtual_port_newname [30];
static int  snd_seq_virtual_port_clientid=-1;

///////////////////////////////////////////////////////////////////////////////
// Match string against a regular expression
///////////////////////////////////////////////////////////////////////////////
int match(const char *string, const char *pattern)
{
    int    status;
    regex_t    re;

    if (regcomp(&re, pattern, REG_EXTENDED|REG_NOSUB) != 0) {
        return(0);      /* Report error. */
    }
    status = regexec(&re, string, (size_t) 0, NULL, 0);
    regfree(&re);
    if (status != 0) {
        return(0);      /* Report error. */
    }
    return(1);
}

///////////////////////////////////////////////////////////////////////////////
// Get an ALSA card from a matching regular expression pattern
///////////////////////////////////////////////////////////////////////////////
int GetCardFromShortName(const char *pattern) {
   int card = -1;
   char* shortname = NULL;

   if ( snd_card_next(&card) < 0) return -1;
   while (card >= 0) {
   	  if ( snd_card_get_name(card, &shortname) == 0  ) {
       if (match(shortname,pattern) ) return card;
     }
   	 if ( snd_card_next(&card) < 0) break;
   }

   return -1;
}

///////////////////////////////////////////////////////////////////////////////
// Get an ALSA sequencer client , port and alsa card  from a regexp pattern
///////////////////////////////////////////////////////////////////////////////
// Will return 0 if found,.  if trueName or card are NULL, they are ignored.
int GetSeqClientFromPortName(const char * pattern, char trueName[], int *card, int *clientId, int *portId) {

	if ( pattern == NULL) return -1;
	char port_name[128];
  int c= -1;
  int r= -1;

	snd_seq_t *seq;
	if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
		tklog_error("Impossible to open default seq (GetSeqClientFromPortName).\n");
		return -1;
	}

	snd_seq_client_info_t *cinfo;
	snd_seq_port_info_t *pinfo;

	snd_seq_client_info_alloca(&cinfo);
	snd_seq_port_info_alloca(&pinfo);
	snd_seq_client_info_set_client(cinfo, -1);
	while (snd_seq_query_next_client(seq, cinfo) >= 0) {
		/* reset query info */
		snd_seq_port_info_set_client(pinfo, snd_seq_client_info_get_client(cinfo));
		snd_seq_port_info_set_port(pinfo, -1);
    while (snd_seq_query_next_port(seq, pinfo) >= 0) {
			sprintf(port_name,"%s %s",snd_seq_client_info_get_name(cinfo),snd_seq_port_info_get_name(pinfo));
      if (match(port_name,pattern) ) {
        //printf("Port scan MATCH  %s\n",port_name);
        if ( card != NULL ) {
          c = GetCardFromShortName(snd_seq_client_info_get_name(cinfo));
          if ( c < 0 ) break;
          *card = c;
        }
        *clientId = snd_seq_port_info_get_client(pinfo);
        *portId = snd_seq_port_info_get_port(pinfo);
        if ( trueName != NULL ) strcpy(trueName,snd_seq_port_info_get_name(pinfo));
        //tklog_debug("(hw:%d) Client %d:%d - %s%s\n",c, snd_seq_port_info_get_client(pinfo),snd_seq_port_info_get_port(pinfo),port_name );
        r = 0;
        break;
			}
		}
	}

	snd_seq_close(seq);
	return  r;
}

///////////////////////////////////////////////////////////////////////////////
// Get the last port ALSA sequencer client
///////////////////////////////////////////////////////////////////////////////
int GetSeqClientLast() {

	int r = -1;

	snd_seq_t *seq;
	if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
		tklog_error("Error : impossible to open default seq\n");
		return -1;
	}

	snd_seq_client_info_t *cinfo;
	snd_seq_port_info_t *pinfo;

	snd_seq_client_info_alloca(&cinfo);
	snd_seq_port_info_alloca(&pinfo);
	snd_seq_client_info_set_client(cinfo, -1);
	while (snd_seq_query_next_client(seq, cinfo) >= 0) {
		/* reset query info */
		snd_seq_port_info_set_client(pinfo, snd_seq_client_info_get_client(cinfo));
		snd_seq_port_info_set_port(pinfo, -1);
		while (snd_seq_query_next_port(seq, pinfo) >= 0) {
				r = snd_seq_client_info_get_client(cinfo) ;
        //fprintf(stdout,"client %s -- %d port %s %d\n",snd_seq_client_info_get_name(cinfo) ,r, snd_seq_port_info_get_name(pinfo), snd_seq_port_info_get_port(pinfo) );
		}
	}

	snd_seq_close(seq);
	return  r;
}

///////////////////////////////////////////////////////////////////////////////
// ALSA aconnect utility API equivalent
///////////////////////////////////////////////////////////////////////////////
int aconnect(int src_client, int src_port, int dest_client, int dest_port) {
	int queue = 0, convert_time = 0, convert_real = 0, exclusive = 0;
	snd_seq_port_subscribe_t *subs;
	snd_seq_addr_t sender, dest;
	int client;
	char addr[10];

	snd_seq_t *seq;
	if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
		tklog_error("Impossible to open default seq\n");
		return -1;
	}

	if ((client = snd_seq_client_id(seq)) < 0) {
		tklog_error("Impossible to get seq client id\n");
		snd_seq_close(seq);
		return - 1;
	}

	/* set client info */
	if (snd_seq_set_client_name(seq, "ALSA Connector") < 0) {
		tklog_error("Set client name failed\n");
		snd_seq_close(seq);
		return -1;
	}

	/* set subscription */
	sprintf(addr,"%d:%d",src_client,src_port);
	if (snd_seq_parse_address(seq, &sender, addr) < 0) {
		snd_seq_close(seq);
		tklog_error("Invalid source address %s\n", addr);
		return -1;
	}

	sprintf(addr,"%d:%d",dest_client,dest_port);
	if (snd_seq_parse_address(seq, &dest, addr) < 0) {
		snd_seq_close(seq);
		tklog_error("Invalid destination address %s\n", addr);
		return -1;
	}

	snd_seq_port_subscribe_alloca(&subs);
	snd_seq_port_subscribe_set_sender(subs, &sender);
	snd_seq_port_subscribe_set_dest(subs, &dest);
	snd_seq_port_subscribe_set_queue(subs, queue);
	snd_seq_port_subscribe_set_exclusive(subs, exclusive);
	snd_seq_port_subscribe_set_time_update(subs, convert_time);
	snd_seq_port_subscribe_set_time_real(subs, convert_real);

	if (snd_seq_get_port_subscription(seq, subs) == 0) {
		snd_seq_close(seq);
    tklog_info("Connection of midi port %d:%d to %d:%d already subscribed\n",src_client,src_port,dest_client,dest_port);
		return -1;
	}

  if (snd_seq_subscribe_port(seq, subs) < 0) {
		snd_seq_close(seq);
    tklog_error("Connection of midi port %d:%d to %d:%d failed !\n",src_client,src_port,dest_client,dest_port);
		return -1;
	}

  //tklog_info("Connection of midi port %d:%d to %d:%d successfull\n",src_client,src_port,dest_client,dest_port);


	snd_seq_close(seq);
  return 0;

}

///////////////////////////////////////////////////////////////////////////////
// Get MPC hardware name from sysex id
///////////////////////////////////////////////////////////////////////////////
static int GetIndexOfMPC_Id(uint8_t id){
	for (int i = 0 ; i < _END_MPCID ; i++ )
		if ( DeviceInfoBloc[i].sysexId == id ) return i;
	return -1;
}

const char * GetHwNameFromMPC_Id(uint8_t id){
	int i = GetIndexOfMPC_Id(id);
	if ( i >= 0) return DeviceInfoBloc[i].productString ;
	else return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// Clean dump of a midi seq event
///////////////////////////////////////////////////////////////////////////////
void dump_event(const snd_seq_event_t *ev)
{
	tklog_trace("Source port : %3d:%-3d\n", ev->source.client, ev->source.port);
	switch (ev->type) {
	case SND_SEQ_EVENT_NOTEON:
		tklog_trace("Note on                %2d %3d %3d\n",
		       ev->data.note.channel, ev->data.note.note, ev->data.note.velocity);
		break;
	case SND_SEQ_EVENT_NOTEOFF:
		tklog_trace("Note off               %2d %3d %3d\n",
		       ev->data.note.channel, ev->data.note.note, ev->data.note.velocity);
		break;
	case SND_SEQ_EVENT_KEYPRESS:
		tklog_trace("Polyphonic aftertouch  %2d %3d %3d\n",
		       ev->data.note.channel, ev->data.note.note, ev->data.note.velocity);
		break;
	case SND_SEQ_EVENT_CONTROLLER:
		tklog_trace("Control change         %2d %3d %3d\n",
		       ev->data.control.channel, ev->data.control.param, ev->data.control.value);
		break;
	case SND_SEQ_EVENT_PGMCHANGE:
		tklog_trace("Program change         %2d %3d\n",
		       ev->data.control.channel, ev->data.control.value);
		break;
	case SND_SEQ_EVENT_CHANPRESS:
		tklog_trace("Channel aftertouch     %2d %3d\n",
		       ev->data.control.channel, ev->data.control.value);
		break;
	case SND_SEQ_EVENT_PITCHBEND:
		tklog_trace("Pitch bend             %2d  %6d\n",
		       ev->data.control.channel, ev->data.control.value);
		break;
	case SND_SEQ_EVENT_CONTROL14:
		tklog_trace("Control change         %2d %3d %5d\n",
		       ev->data.control.channel, ev->data.control.param, ev->data.control.value);
		break;
	case SND_SEQ_EVENT_NONREGPARAM:
		tklog_trace("Non-reg. parameter     %2d %5d %5d\n",
		       ev->data.control.channel, ev->data.control.param, ev->data.control.value);
		break;
	case SND_SEQ_EVENT_REGPARAM:
		tklog_trace("Reg. parameter         %2d %5d %5d\n",
		       ev->data.control.channel, ev->data.control.param, ev->data.control.value);
		break;
	case SND_SEQ_EVENT_SONGPOS:
		tklog_trace("Song position pointer     %5d\n",
		       ev->data.control.value);
		break;
	case SND_SEQ_EVENT_SONGSEL:
		tklog_trace("Song select               %3d\n",
		       ev->data.control.value);
		break;
	case SND_SEQ_EVENT_QFRAME:
		tklog_trace("MTC quarter frame         %02xh\n",
		       ev->data.control.value);
		break;
	case SND_SEQ_EVENT_TIMESIGN:
		// XXX how is this encoded?
		tklog_trace("SMF time signature        (%#08x)\n",
		       ev->data.control.value);
		break;
	case SND_SEQ_EVENT_KEYSIGN:
		// XXX how is this encoded?
		tklog_trace("SMF key signature         (%#08x)\n",
		       ev->data.control.value);
		break;
	case SND_SEQ_EVENT_START:
		if (ev->source.client == SND_SEQ_CLIENT_SYSTEM &&
		    ev->source.port == SND_SEQ_PORT_SYSTEM_TIMER)
			tklog_trace("Queue start               %d\n",
			       ev->data.queue.queue);
		else
			tklog_trace("Start\n");
		break;
	case SND_SEQ_EVENT_CONTINUE:
		if (ev->source.client == SND_SEQ_CLIENT_SYSTEM &&
		    ev->source.port == SND_SEQ_PORT_SYSTEM_TIMER)
			tklog_trace("Queue continue            %d\n",
			       ev->data.queue.queue);
		else
			tklog_trace("Continue\n");
		break;
	case SND_SEQ_EVENT_STOP:
		if (ev->source.client == SND_SEQ_CLIENT_SYSTEM &&
		    ev->source.port == SND_SEQ_PORT_SYSTEM_TIMER)
			tklog_trace("Queue stop                %d\n",
			       ev->data.queue.queue);
		else
			tklog_trace("Stop\n");
		break;
	case SND_SEQ_EVENT_SETPOS_TICK:
		tklog_trace("Set tick queue pos.       %d\n", ev->data.queue.queue);
		break;
	case SND_SEQ_EVENT_SETPOS_TIME:
		tklog_trace("Set rt queue pos.         %d\n", ev->data.queue.queue);
		break;
	case SND_SEQ_EVENT_TEMPO:
		tklog_trace("Set queue tempo           %d\n", ev->data.queue.queue);
		break;
	case SND_SEQ_EVENT_CLOCK:
		tklog_trace("Clock\n");
		break;
	case SND_SEQ_EVENT_TICK:
		tklog_trace("Tick\n");
		break;
	case SND_SEQ_EVENT_QUEUE_SKEW:
		tklog_trace("Queue timer skew          %d\n", ev->data.queue.queue);
		break;
	case SND_SEQ_EVENT_TUNE_REQUEST:
		/* something's fishy here ... */
		tklog_trace("Tuna request\n");
		break;
	case SND_SEQ_EVENT_RESET:
		tklog_trace("Reset\n");
		break;
	case SND_SEQ_EVENT_SENSING:
		tklog_trace("Active Sensing\n");
		break;
	case SND_SEQ_EVENT_CLIENT_START:
		tklog_trace("Client start              %d\n",
		       ev->data.addr.client);
		break;
	case SND_SEQ_EVENT_CLIENT_EXIT:
		tklog_trace("Client exit               %d\n",
		       ev->data.addr.client);
		break;
	case SND_SEQ_EVENT_CLIENT_CHANGE:
		tklog_trace("Client changed            %d\n",
		       ev->data.addr.client);
		break;
	case SND_SEQ_EVENT_PORT_START:
		tklog_trace("Port start                %d:%d\n",
		       ev->data.addr.client, ev->data.addr.port);
		break;
	case SND_SEQ_EVENT_PORT_EXIT:
		tklog_trace("Port exit                 %d:%d\n",
		       ev->data.addr.client, ev->data.addr.port);
		break;
	case SND_SEQ_EVENT_PORT_CHANGE:
		tklog_trace("Port changed              %d:%d\n",
		       ev->data.addr.client, ev->data.addr.port);
		break;
	case SND_SEQ_EVENT_PORT_SUBSCRIBED:
		tklog_trace("Port subscribed           %d:%d -> %d:%d\n",
		       ev->data.connect.sender.client, ev->data.connect.sender.port,
		       ev->data.connect.dest.client, ev->data.connect.dest.port);
		break;
	case SND_SEQ_EVENT_PORT_UNSUBSCRIBED:
		tklog_trace("Port unsubscribed         %d:%d -> %d:%d\n",
		       ev->data.connect.sender.client, ev->data.connect.sender.port,
		       ev->data.connect.dest.client, ev->data.connect.dest.port);
		break;
	case SND_SEQ_EVENT_SYSEX:
		{
			unsigned int i;
			tklog_trace("System exclusive         \n");
      ShowBufferHexDump((uint8_t*)ev->data.ext.ptr, ev->data.ext.len,16 );
			tklog_trace("\n");
		}
		break;
	default:
		tklog_trace("Event type %d\n",  ev->type);
	}
}

///////////////////////////////////////////////////////////////////////////////
// Alsa SEQ raw write to a seq port
///////////////////////////////////////////////////////////////////////////////
// (rawmidi_virt.c snd_rawmidi_virtual_write inspiration !)
int SeqSendRawMidi(uint8_t destId,  const uint8_t *buffer, size_t size ) {

  static snd_midi_event_t * midiParser = NULL;
  snd_seq_event_t ev;
	ssize_t result = 0;
	ssize_t size1;
	int err;

  if ( rawMidiDumpPostFlag && size > 0 ) {
    snd_seq_client_info_t *cinfo;
    snd_seq_client_info_alloca(&cinfo);
    snd_seq_get_client_info (TkRouter.seq, cinfo);

    tklog_trace("TKGL_Router dump POST -  SeqSendRawMidi (%d bytes) - To (%d:%d)\n",size,snd_seq_client_info_get_client(cinfo),GetSeqPortFromDestinationId(destId));
    ShowBufferHexDump(buffer,size,16);
    tklog_trace("\n");
  }

  // Start the MIDI parser
  if (midiParser == NULL ) {
   if (snd_midi_event_new(   size > MIDI_DECODER_SIZE ? size : MIDI_DECODER_SIZE, &midiParser ) < 0) return -1 ;
  }

  snd_midi_event_init(midiParser);
  snd_midi_event_no_status(midiParser,1); // No running status

  snd_seq_ev_clear (&ev);

  while (size > 0) {
		size1 = snd_midi_event_encode (midiParser, buffer, size, &ev);
    snd_midi_event_reset_encode (midiParser);

		if (size1 <= 0) break;
		size -= size1;
		result += size1;
		buffer += size1;
		if (ev.type == SND_SEQ_EVENT_NONE) continue;

    snd_seq_ev_set_subs (&ev);
    SetMidiEventDestination(&ev, destId );
    //snd_seq_ev_set_source(&ev, port);
		snd_seq_ev_set_direct (&ev);
		err = snd_seq_event_output (TkRouter.seq, &ev);

		if (err < 0) {
			return result > 0 ? result : err;
		}
	}

	if (result > 0) {
    snd_seq_drain_output (TkRouter.seq);
  }

	return result;
}

///////////////////////////////////////////////////////////////////////////////
// Alsa SEQ : decode an event to a byte stream
///////////////////////////////////////////////////////////////////////////////
ssize_t SeqDecodeEvent(snd_seq_event_t *ev, void *buffer) {

  static snd_midi_event_t * midiParser = NULL;
  int r = -1;

  if (midiParser == NULL ) {
    if (snd_midi_event_new( MIDI_DECODER_SIZE, &midiParser ) < 0) return -1;
  }

  snd_midi_event_init(midiParser);
  snd_midi_event_no_status(midiParser,1); // No running status

  if (ev->type == SND_SEQ_EVENT_SYSEX) {
      // With sysex, we need to forward in the event queue to get all the data
      r = ev->data.ext.len;
      if ( r > MIDI_DECODER_SIZE ) {
        tklog_error("Sysex buffer overflow in SeqDecodeEvent : %d / %d.\n",r,MIDI_DECODER_SIZE);
      }
      memcpy(buffer,ev->data.ext.ptr,r) ;
  }
  else {
      r = snd_midi_event_decode (midiParser, buffer, MIDI_DECODER_SIZE, ev);
      if ( r > MIDI_DECODER_SIZE) {
        tklog_error("Midi decoder buffer overflow in SeqDecodeEvent : %d / %d.\n",r,MIDI_DECODER_SIZE);
      }
      snd_midi_event_reset_encode (midiParser);
  }

  return r;

}


///////////////////////////////////////////////////////////////////////////////
// MIDI EVENT SET DESTINATION
///////////////////////////////////////////////////////////////////////////////
int SetMidiEventDestination(snd_seq_event_t *ev, uint8_t destId ) {

  return snd_seq_ev_set_source(ev, GetSeqPortFromDestinationId(destId) ) ;
}

///////////////////////////////////////////////////////////////////////////////
// GET PORT from DESTINATION ID
///////////////////////////////////////////////////////////////////////////////
int GetSeqPortFromDestinationId(uint8_t destId ) {

  switch (destId) {

    // Event to public MPC hardware
    case TO_CTRL_MPC_PUBLIC:
        return TkRouter.portMpcPub;
        break;

    // Event to private MPC hardware
    case TO_CTRL_MPC_PRIVATE:
        return TkRouter.portMpcPub;
        break;

    // Event to private raw midi MPC software port
    case TO_MPC_PRIVATE:
        return TkRouter.portPriv;
        break;

    // Event to external captured controller hardware
    case TO_CTRL_EXT:
        return TkRouter.portCtrl;
        break;

    // Event to MPC application port mapped with external controller
    case TO_MPC_EXTCTRL:
        return TkRouter.portAppCtrl;
        break;

  }

  return -1;

}

///////////////////////////////////////////////////////////////////////////////
// MIDI EVENT PROCESS AND ROUTE (IN A sub THREAD)
///////////////////////////////////////////////////////////////////////////////
void threadMidiProcessAndRoute() {

  snd_seq_event_t *ev;
  bool send = true;

  while (snd_seq_event_input(TkRouter.seq, &ev) >= 0) {

    if ( rawMidiDumpFlag  ) {
      int s = SeqDecodeEvent(ev, MidiWkBuffer);
      if ( s > 0 ) {
        tklog_trace("TKGL_Router dump -  threadMidiProcessAndRoute (%d bytes) - From (%d:%d)\n",s,ev->source.client,ev->source.port);
        ShowBufferHexDump(MidiWkBuffer,s,16);
        tklog_trace("\n");
      }
    }

    send = true ;
    ev->flags |= SND_SEQ_PRIORITY_NORMAL;
    snd_seq_ev_set_subs(ev);
    snd_seq_ev_set_direct(ev);

    // -----------------------------------------------------------------------
    // Event from MPC hw device. Only private port can write
    // -----------------------------------------------------------------------
    if ( ev->source.client == TkRouter.MpcHW.cli ) {
        SetMidiEventDestination(ev, TO_MPC_PRIVATE );
        if ( midiMapperLibHandle ) send = MidiMapper( FROM_CTRL_MPC, ev );
    }

    // -----------------------------------------------------------------------
    // Events from MPC application
    // -----------------------------------------------------------------------

    // APP Writes to PUBLIC PORT (used for SYSEX)
    else
    if ( ev->source.client == TkRouter.VirtRaw.cliPubOut ) {
        ev->flags |= SND_SEQ_PRIORITY_HIGH; // Because of sysex
        SetMidiEventDestination(ev, TO_CTRL_MPC_PUBLIC );
        if ( midiMapperLibHandle ) send = MidiMapper( FROM_MPC_PUBLIC, ev );
    }

    // APP Writes to Private PORT
    else
    if ( ev->source.client == TkRouter.VirtRaw.cliPrivOut ) {
        SetMidiEventDestination(ev, TO_CTRL_MPC_PRIVATE );
        if ( midiMapperLibHandle ) send = MidiMapper( FROM_MPC_PRIVATE, ev );
    }

    // APP Writes to port mirroring external controller
    else
    if ( ev->source.client == TkRouter.MPCCtrl.cli && ev->source.port == TkRouter.MPCCtrl.portOut) {
        SetMidiEventDestination(ev, TO_CTRL_EXT );
        if ( midiMapperLibHandle ) send = MidiMapper( FROM_MPC_EXTCTRL, ev );
    }

    // -----------------------------------------------------------------------
    // Events from external controller
    // -----------------------------------------------------------------------
    else
    if ( ev->source.client == TkRouter.Ctrl.cli && ev->source.port == TkRouter.Ctrl.port) {
        SetMidiEventDestination(ev, TO_MPC_EXTCTRL );
        if ( midiMapperLibHandle ) send = MidiMapper( FROM_CTRL_EXT, ev );
    }

    if ( rawMidiDumpPostFlag  ) {
      int s = SeqDecodeEvent(ev, MidiWkBuffer);
      if ( s > 0 ) {
        tklog_trace("TKGL_Router dump POST -  threadMidiProcessAndRoute (%d bytes) - To (%d:%d)\n",s,ev->source.client,ev->source.port);
        ShowBufferHexDump(MidiWkBuffer,s,16);
        tklog_trace("\n");
      }
    }

    if (send) snd_seq_event_output(TkRouter.seq, ev);
    snd_seq_drain_output(TkRouter.seq);

  }

}

///////////////////////////////////////////////////////////////////////////////
// MIDI ROUTER MAIN THREAD
///////////////////////////////////////////////////////////////////////////////
void* threadMidiRouter(void * pdata) {

  int npfd;
  struct pollfd *pfd;

  // MidiMapper setup
  if ( midiMapperLibHandle ) MidiMapperSetup();

  // Polling
  npfd = snd_seq_poll_descriptors_count(TkRouter.seq, POLLIN);
  pfd = (struct pollfd *)alloca(npfd * sizeof(struct pollfd));
  snd_seq_poll_descriptors(TkRouter.seq, pfd, npfd, POLLIN);

  tklog_info("Midi router thread started. Waiting for events...\n");

  while (1) {
    if (poll(pfd, npfd, 100000) > 0) {
      threadMidiProcessAndRoute();
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// Create SEQ simple port
///////////////////////////////////////////////////////////////////////////////
int  CreateSimplePort(snd_seq_t *seq, const char* name ) {

  int p = -1;
  if ( (  p = snd_seq_create_simple_port(seq, name,
        SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE |
        SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ |
        SND_SEQ_PORT_CAP_DUPLEX,
        SND_SEQ_PORT_TYPE_HARDWARE | SND_SEQ_PORT_TYPE_MIDI_GENERIC
      ) )  < 0) {
    tklog_fatal("Error creating sequencer port %s.\n",name);
    exit(1);
  }
  return p;
}

///////////////////////////////////////////////////////////////////////////////
// Show MPCMAPPER HELP
///////////////////////////////////////////////////////////////////////////////
void ShowHelp(void) {

  tklog_info("\n") ;
  tklog_info("--tgkl_help                : Show this help\n") ;
  tklog_info("--tkgl_ctrlname=<name>     : Use external controller containing <name>\n") ;
  tklog_info("--tkgl_ctrlmidiport=<#>    : External midi port number to capture (0 if not provided)\n") ;
  tklog_info("--tkgl_mididump            : Dump original raw midi flow\n") ;
  tklog_info("--tkgl_mididumpPost        : Dump raw midi flow after transformation\n") ;
  tklog_info("--tkgl_midilib=<file name> : Use dynamic library <file name> to transform & route midi events\n") ;
  tklog_info("\n") ;
  exit(0);
}

///////////////////////////////////////////////////////////////////////////////
// Make ld_preload hooks
///////////////////////////////////////////////////////////////////////////////
static void makeLdHooks() {
  // System call hooks
  //orig_open64 = dlsym(RTLD_NEXT, "open64");
  //orig_open   = dlsym(RTLD_NEXT, "open");
  //orig_close  = dlsym(RTLD_NEXT, "close");
  //orig_read   = dlsym(RTLD_NEXT, "read");

	// Alsa hooks
	orig_snd_rawmidi_open           = dlsym(RTLD_NEXT, "snd_rawmidi_open");
  orig_snd_rawmidi_close          = dlsym(RTLD_NEXT, "snd_rawmidi_close");
	orig_snd_rawmidi_read           = dlsym(RTLD_NEXT, "snd_rawmidi_read");
	orig_snd_rawmidi_write          = dlsym(RTLD_NEXT, "snd_rawmidi_write");
	orig_snd_seq_create_simple_port = dlsym(RTLD_NEXT, "snd_seq_create_simple_port");
	orig_snd_midi_event_decode      = dlsym(RTLD_NEXT, "snd_midi_event_decode");
  orig_snd_seq_open               = dlsym(RTLD_NEXT, "snd_seq_open");
  orig_snd_seq_close              = dlsym(RTLD_NEXT, "snd_seq_close");
  orig_snd_seq_port_info_set_name = dlsym(RTLD_NEXT, "snd_seq_port_info_set_name");
  //orig_snd_seq_event_input        = dlsym(RTLD_NEXT, "snd_seq_event_input");

}

///////////////////////////////////////////////////////////////////////////////
// Setup tkgl anyctrl
///////////////////////////////////////////////////////////////////////////////
static void tkgl_init()
{

  makeLdHooks();

  // Read product code
  char product_code[4];
  int fd = open(PRODUCT_CODE_PATH,O_RDONLY);
  read(fd,&product_code,4);

  // Find the id in the product code table
  for (int i = 0 ; i < _END_MPCID; i++) {
    if ( strncmp(DeviceInfoBloc[i].productCode,product_code,4) == 0 ) {
      MPC_Id = i;
      break;
    }
  }
  if ( MPC_Id < 0) {
    tklog_fatal("Error while reading the product-code file\n");
    exit(1);
  }
  tklog_info("Product code : %s (%s)\n",DeviceInfoBloc[MPC_Id].productCode,DeviceInfoBloc[MPC_Id].productString);


  // Midi transformation dynamic library
  // Open MidiMapper dll if any

  if ( midiMapperLibFileName != NULL ) {
    midiMapperLibHandle = dlopen(midiMapperLibFileName, RTLD_LAZY);
    if ( midiMapperLibHandle == NULL) {
      tklog_fatal("%s midipapper library loading error or not found\n",midiMapperLibFileName);
      exit(1);
    }
    MidiMapper = dlsym(midiMapperLibHandle, "MidiMapper");
    MidiMapperStart = dlsym(midiMapperLibHandle, "MidiMapperStart");
    MidiMapperSetup = dlsym(midiMapperLibHandle, "MidiMapperSetup");
    tklog_info("Midi mapping %s library succefully loaded.\n",midiMapperLibFileName);
    MidiMapperStart(); // Initialize plugin
  }

  // Retrieve MPC midi card info
  if ( GetSeqClientFromPortName(CTRL_MPC_ALL_PRIVATE,NULL,&TkRouter.MpcHW.card,&TkRouter.MpcHW.cli,&TkRouter.MpcHW.portPriv) < 0 ) {
    tklog_fatal("Error : MPC controller card/seq client not found (regex pattern is '%s')\n",CTRL_MPC_ALL_PRIVATE);
    exit(1);
  }
  TkRouter.MpcHW.portPub = 0 ; // Public port is 0
	sprintf(mpc_midi_private_alsa_name,"hw:%d,0,%d",TkRouter.MpcHW.card,TkRouter.MpcHW.portPriv);
	sprintf(mpc_midi_public_alsa_name,"hw:%d,0,%d",TkRouter.MpcHW.card,TkRouter.MpcHW.portPub);
	tklog_info("MPC controller Private hardware port Alsa name is %s. Sequencer id is %d:%d.\n",mpc_midi_private_alsa_name,TkRouter.MpcHW.cli,TkRouter.MpcHW.portPriv);
	tklog_info("MPC controller Public  hardware port Alsa name is %s. Sequencer id is %d:%d.\n",mpc_midi_public_alsa_name,TkRouter.MpcHW.cli,TkRouter.MpcHW.portPub);



	// Get our controller seq  port client
	if ( strlen(anyctrl_name) > 0 ) {

		// Initialize card id for public and private
    if (  GetSeqClientFromPortName(anyctrl_name,anyctrl_name,&TkRouter.Ctrl.card,&TkRouter.Ctrl.cli,&TkRouter.Ctrl.port) < 0 ) {
      tklog_error("Error : %s controller card/seq client not found. Parameter ignored.\n",anyctrl_name);
      anyctrl_name[0] = 0; TkRouter.Ctrl.cli = -1;
    }
    else {
       if ( anyctrl_midiport >=0 ) TkRouter.Ctrl.port = anyctrl_midiport;
       tklog_info("%s controller hardware port Alsa name is hw:%d,0,%d. Sequencer id is %d:%d.\n",anyctrl_name,TkRouter.Ctrl.card,TkRouter.Ctrl.port,TkRouter.Ctrl.cli,TkRouter.Ctrl.port);
    }
	}

	// Create 3 virtuals rawmidi ports : Private I/O, Public O that will expose rawmidi internal ports
  // The port is always 0. This is the standard behaviour of Alsa virtual rawmidi.
  TkRouter.VirtRaw.cliPrivIn  = snd_rawmidi_open(&rawvirt_inpriv, NULL,  "[virtual]Virt MPC Rawmidi In Private", 2);
  TkRouter.VirtRaw.cliPrivOut = snd_rawmidi_open(NULL, &rawvirt_outpriv, "[virtual]Virt MPC Rawmidi Out Private", 3);
  TkRouter.VirtRaw.cliPubOut  = snd_rawmidi_open(NULL, &rawvirt_outpub,  "[virtual]Virt MPC Rawmidi Out Public", 3);

	if ( TkRouter.VirtRaw.cliPrivIn < 0 || TkRouter.VirtRaw.cliPrivOut < 0 || TkRouter.VirtRaw.cliPubOut < 0 ) {
		tklog_fatal("Impossible to create one or many virtual rawmidi ports\n");
		exit(1);
	}

  tklog_info("Virtual rawmidi private input port %d:0  created.\n",TkRouter.VirtRaw.cliPrivIn);
	tklog_info("Virtual rawmidi private output port %d:0 created.\n",TkRouter.VirtRaw.cliPrivOut);
	tklog_info("Virtual rawmidi public output port %d:0 created.\n",TkRouter.VirtRaw.cliPubOut);

  // Create Router Private and public port for external access with alsa sequencer
  // This will allow to process all midi flow with Alsa sequencer within the Router thread
  // and avoid dirty patching within snd_rawmidi_read/write

  if (snd_seq_open(&TkRouter.seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
    tklog_fatal("Error opening router ALSA sequencer (tkgl_init).\n");
    exit(1);
  }

  // Change the name of the seq with our for a better view in aconnect...
  snd_seq_set_client_name(TkRouter.seq, ROUTER_SEQ_NAME);
  TkRouter.cli = snd_seq_client_id(TkRouter.seq) ;

  // Create router  ports
  TkRouter.portPriv    = CreateSimplePort(TkRouter.seq, "Priv Private" );
  TkRouter.portCtrl    = CreateSimplePort(TkRouter.seq, "Ctrl Private" );
  TkRouter.portPub     = CreateSimplePort(TkRouter.seq, "Pub Private" );
  TkRouter.portMpcPriv = CreateSimplePort(TkRouter.seq, "Mpc Priv Private" );
  TkRouter.portMpcPub  = CreateSimplePort(TkRouter.seq, "Mpc Pub Private" );

  // Alsa connections I/O to that port will be done by the MPC app automatically
  if ( TkRouter.Ctrl.cli >= 0 ) TkRouter.portAppCtrl = CreateSimplePort(TkRouter.seq, ROUTER_CTRL_PORT_NAME );

  tklog_info("Midi Router created as client %d.\n",TkRouter.cli);

  // Router ports connections

 //   MPC HARDWARE                       ROUTER
 //        PUBLIC
 //           - IN  <------------------OUT HW PUB
 //        PRIVATE PORT
 //           - IN  <------------------OUT HW PRIV
 //           - OUT ------------------>IN HW PRIV

  // Private HW Out to Router Private
  if ( aconnect(	TkRouter.MpcHW.cli,TkRouter.MpcHW.portPriv , TkRouter.cli,TkRouter.portMpcPriv) < 0 ) {
   tklog_fatal("Alsa error while connecting private MPC harware to router.\n");
   exit(1);
  }
  tklog_info("MPC hardware Private Out (%d:%d) connected to Router Hw Private IN (%d:%d).\n",TkRouter.MpcHW.cli,TkRouter.MpcHW.portPriv , TkRouter.cli,TkRouter.portPriv);

  // Router Private Out to MPC harware Private
  if ( aconnect(	TkRouter.cli,TkRouter.portMpcPriv, TkRouter.MpcHW.cli,TkRouter.MpcHW.portPriv  ) < 0 ) {
   tklog_fatal("Alsa error while connecting router to private MPC hardware.\n");
   exit(1);
  }
  tklog_info("Router HW Private Out (%d:%d) connected to MPC hardware Private In (%d:%d).\n",TkRouter.cli,TkRouter.portMpcPriv, TkRouter.MpcHW.cli,TkRouter.MpcHW.portPriv );

  // Router Public Out to MPC hardware Public
  if ( aconnect(	TkRouter.cli,TkRouter.portMpcPub, TkRouter.MpcHW.cli,TkRouter.MpcHW.portPub  ) < 0 ) {
   tklog_fatal("Alsa error while connecting router to public MPC hardware.\n");
   exit(1);
  }
  tklog_info("Router HW Public Out (%d:%d) connected to MPC hardware Public In (%d:%d).\n",TkRouter.cli,TkRouter.portMpcPub, TkRouter.MpcHW.cli,TkRouter.MpcHW.portPub );


  // MPC APPLICATION PORTS connections

  //   MPC APP                            ROUTER
  //        PUBLIC
  //           - OUT  ------------------> IN APP PUB
  //        PRIVATE PORT
  //           - IN  <------------------  OUT APP PRIV
  //           - OUT ------------------>  IN APP PRIV

  // MPC application private to private mpc app router rawmidi

  if ( aconnect(	TkRouter.VirtRaw.cliPrivOut,0,TkRouter.cli,TkRouter.portPriv ) < 0 ) {
   tklog_fatal("Alsa error while connecting MPC virtual rawmidi private port to router.\n");
   exit(1);
  }
  tklog_info("MPC App virtual rawmidi private Out (%d:%d) connected to Router MPC app Private in (%d:%d).\n",TkRouter.VirtRaw.cliPrivOut,0,TkRouter.cli,TkRouter.portPriv );

  // Router private mpc to MPC private app rawmidi

  if ( aconnect(	TkRouter.cli,TkRouter.portPriv, TkRouter.VirtRaw.cliPrivIn, 0 ) < 0 ) {
   tklog_fatal("Alsa error while connecting router private port to MPC virtual rawmidi private.\n");
   exit(1);
  }
  tklog_info("Router MPC app Private out (%d:%d) connected to MPC App virtual rawmidi private IN (%d:%d).\n",	TkRouter.cli,TkRouter.portPriv, TkRouter.VirtRaw.cliPrivIn,0 );

  // MPC application public to public mpc app router
  if ( aconnect(	TkRouter.VirtRaw.cliPubOut,0,TkRouter.cli,TkRouter.portPub ) < 0 ) {
   tklog_fatal("Alsa error while connecting MPC virtual rawmidi public port to router.\n");
   exit(1);
  }
  tklog_info("MPC App virtual rawmidi public Out (%d:%d) connected to Router MPC app Public in (%d:%d).\n",TkRouter.VirtRaw.cliPubOut,0,TkRouter.cli,TkRouter.portPub );

  // Our controller hardware PORTS connections
  //   EXT CONTROLLER                      ROUTER
  //           - IN  <------------------  OUT CTRL
  //           - OUT ------------------>  IN CTRL

	// Connect our controller if used
	if (TkRouter.Ctrl.cli >= 0) {

    if ( aconnect(	TkRouter.Ctrl.cli, TkRouter.Ctrl.port, TkRouter.cli,TkRouter.portCtrl) < 0 ) {
     tklog_fatal("Alsa error while connecting external controller to router.\n");
     exit(1);
    }
    tklog_info("Ext controller port (%d:%d) connected to Router controller port in (%d:%d).\n",TkRouter.Ctrl.cli, TkRouter.Ctrl.port, TkRouter.cli,TkRouter.portCtrl );

    if ( aconnect( TkRouter.cli,TkRouter.portCtrl,	TkRouter.Ctrl.cli, TkRouter.Ctrl.port) < 0 ) {
     tklog_fatal("Alsa error while connecting router to external controller.\n");
     exit(1);
    }
    tklog_info("Router controller port in (%d:%d) connected to Ext controller port (%d:%d).\n",TkRouter.cli,TkRouter.portCtrl,	TkRouter.Ctrl.cli, TkRouter.Ctrl.port );
	}

  // Start Router thread
  if ( pthread_create(&MidiRouterThread, NULL, threadMidiRouter, NULL) < 0) {
    tklog_fatal("Unable to create midi router thread\n");
    exit(1);
  }

  fflush(stdout);
}

////////////////////////////////////////////////////////////////////////////////
// Clean DUMP of a buffer to screen
////////////////////////////////////////////////////////////////////////////////
void ShowBufferHexDump(const uint8_t* data, ssize_t sz, uint8_t nl)
{
    uint8_t b;
    char asciiBuff[33];
    uint8_t c=0;

    for (uint16_t idx = 0 ; idx < sz; idx++) {
		  	if ( c == 0 && idx >= 0) tklog_trace("");
        b = (*data++);
  		  fprintf(stdout,"%02X ",b);
        asciiBuff[c++] = ( b >= 0x20 && b< 127? b : '.' ) ;
        if ( c == nl || idx == sz -1 ) {
          asciiBuff[c] = 0;
		      for (  ; c < nl; c++  ) fprintf(stdout,"   ");
          c = 0;
          fprintf(stdout," | %s\n", asciiBuff);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
// MPC Main hook
///////////////////////////////////////////////////////////////////////////////
int __libc_start_main(
    int (*main)(int, char **, char **),
    int argc,
    char **argv,
    int (*init)(int, char **, char **),
    void (*fini)(void),
    void (*rtld_fini)(void),
    void *stack_end)
{

    // Find the real __libc_start_main()...
    typeof(&__libc_start_main) orig = dlsym(RTLD_NEXT, "__libc_start_main");

    // Banner
    fprintf(stdout,"\n%s",TKGL_LOGO);
    tklog_info("---------------------------------------------------------\n");
  	tklog_info("TKGL_MIDIMAPPER Version : %s\n",VERSION);
    tklog_info("(c) The KikGen Labs.\n");
    tklog_info("https://github.com/TheKikGen/MPC-LiveXplore\n");
  	tklog_info("---------------------------------------------------------\n");

    // Show the command line
    tklog_info("MPC args : ") ;

    for ( int i = 1 ; i < argc ; i++ ) {
      fprintf(stdout, "%s ", argv[i] ) ;
    }
    fprintf(stdout, "\n") ;
    tklog_info("\n") ;

    // Scan command line

    anyctrl_name[0] = 0;

    for ( int i = 1 ; i < argc ; i++ ) {

      // help
      if ( ( strcmp("--tkgl_help",argv[i]) == 0 ) ) {
         ShowHelp();
      }
      else
      if ( ( strncmp("--tkgl_ctrlname=",argv[i],16) == 0 ) && ( strlen(argv[i]) >16 ) ) {
         strcpy(anyctrl_name,argv[i] + 16);
         tklog_info("--tgkl_ctrlname specified for %s midi controller\n",anyctrl_name) ;
      }

      else
      if ( ( strncmp("--tkgl_ctrlmidiport=",argv[i],20) == 0 ) && ( strlen(argv[i]) > 20 ) ) {
         anyctrl_midiport = atoi(argv[i] + 20);
         if ( anyctrl_midiport < 0 || anyctrl_midiport > 15 ) {
           tklog_info("--tkgl_ctrlmidiport specified. Unknown port %d. Port must be an integer beetween 0 and 15.\n",anyctrl_midiport) ;
           anyctrl_midiport = -1 ;
         } else tklog_info("--tkgl_ctrlmidiport specified. Midi port %d will be captured\n",anyctrl_midiport) ;
      }

      else
      if ( ( strcmp("--tkgl_mididump",argv[i]) == 0 ) ) {
        rawMidiDumpFlag = 1 ;
        tklog_info("--tkgl_mididump specified : dump original raw midi message (ENTRY)\n") ;
      }

      else
      if ( ( strcmp("--tkgl_mididumpPost",argv[i]) == 0 ) ) {
        rawMidiDumpPostFlag = 1 ;
        tklog_info("--tkgl_mididumpPost specified : dump raw midi message after transformation (POST)\n") ;
      }
      else
      // Midi specific library file name
      if ( ( strncmp("--tkgl_midilib=",argv[i],15) == 0 ) && ( strlen(argv[i]) >15 )  ) {
        midiMapperLibFileName = argv[i] + 15 ;
        tklog_info("--tkgl_midilib specified. File %s will be used for midi mapping\n",midiMapperLibFileName) ;
      }

    }

    // Initialize everything
    tkgl_init();

    int r =  orig(main, argc, argv, init, fini, rtld_fini, stack_end);

    tklog_info("End of midimapper.\n" ) ;

    // ... and call main again
    return r ;
}

///////////////////////////////////////////////////////////////////////////////
// ALSA snd_rawmidi_open hooked
///////////////////////////////////////////////////////////////////////////////
// This function allows changing the name of a virtual port, by using the
// naming convention "[virtual]port name"
// and if creation succesfull, will return the client number. Port is always 0 if virtual.

int snd_rawmidi_open(snd_rawmidi_t **inputp, snd_rawmidi_t **outputp, const char *name, int mode)
{
	tklog_debug("snd_rawmidi_open name %s mode %d\n",name,mode);

  // Rename the virtual port as we need
  // Port Name must not be emtpy - 30 chars max
  if ( strncmp(name,"[virtual]",9) == 0 ) {
    int l = strlen(name);
    if ( l <= 9 || l > 30 + 9 ) return -1;

    // Prepare the renaming of the virtual port
    strcpy(snd_seq_virtual_port_newname,name + 9);
    snd_seq_virtual_port_rename_flag = 1;

    // Create the virtual port via the fake Alsa rawmidi virtual open
    int r = orig_snd_rawmidi_open(inputp, outputp, "virtual", mode);
    if ( r < 0 ) return r;

    // Get the port id that was populated in the port creation sub function
    // and reset it
    r = snd_seq_virtual_port_clientid;
    snd_seq_virtual_port_clientid=-1;

    //tklog_debug("PORT ID IS %d\n",r);

    return r;

  }

	// Substitute the hardware private input port by our input virtual ports

	else if ( strcmp(mpc_midi_private_alsa_name,name) == 0   ) {

		// Private In
		if (inputp) *inputp = rawvirt_inpriv;
		else if (outputp) *outputp = rawvirt_outpriv ;
		else return -1;
		tklog_info("%s substitution by virtual rawmidi successfull\n",name);

		return 0;
	}

	else if ( strcmp(mpc_midi_public_alsa_name,name) == 0   ) {

		if (outputp) *outputp = rawvirt_outpub;
		else return -1;
		tklog_info("%s substitution by virtual rawmidi successfull\n",name);

		return 0;
	}

	return orig_snd_rawmidi_open(inputp, outputp, name, mode);
}

///////////////////////////////////////////////////////////////////////////////
// ALSA snd_rawmidi_close hooked
///////////////////////////////////////////////////////////////////////////////
int snd_rawmidi_close	(	snd_rawmidi_t * 	rawmidi	)
{

	//tklog_debug("snd_rawmidi_close handle :%p\n",rawmidi);

	return orig_snd_rawmidi_close(rawmidi);
}

///////////////////////////////////////////////////////////////////////////////
// Alsa Rawmidi read
///////////////////////////////////////////////////////////////////////////////
ssize_t snd_rawmidi_read(snd_rawmidi_t *rawmidi, void *buffer, size_t size) {

  //tklog_debug("snd_rawmidi_read %p : size : %u ", rawmidi, size);
	ssize_t r = orig_snd_rawmidi_read(rawmidi, buffer, size);
  // if ( rawMidiDumpFlag  ) RawMidiDump(rawmidi, 'i','r' , buffer, r);
	return r;
}

///////////////////////////////////////////////////////////////////////////////
// Alsa Rawmidi write
///////////////////////////////////////////////////////////////////////////////
ssize_t snd_rawmidi_write(snd_rawmidi_t *rawmidi,const void *buffer,size_t size) {

  // if ( rawMidiDumpFlag ) RawMidiDump(rawmidi, 'i','w' , buffer, size);
	return 	orig_snd_rawmidi_write(rawmidi, buffer, size);
}

///////////////////////////////////////////////////////////////////////////////
// Alsa open sequencer
///////////////////////////////////////////////////////////////////////////////
int snd_seq_open (snd_seq_t **handle, const char *name, int streams, int mode) {

  //tklog_debug("snd_seq_open %s (%p) \n",name,handle);

  return orig_snd_seq_open(handle, name, streams, mode);

}

///////////////////////////////////////////////////////////////////////////////
// Alsa close sequencer
///////////////////////////////////////////////////////////////////////////////
int snd_seq_close (snd_seq_t *handle) {

  //tklog_debug("snd_seq_close. Hanlde %p \n",handle);

  return orig_snd_seq_close(handle);

}

///////////////////////////////////////////////////////////////////////////////
// Alsa set a seq port name
///////////////////////////////////////////////////////////////////////////////
void snd_seq_port_info_set_name	(	snd_seq_port_info_t * 	info, const char * 	name )
{
  //tklog_debug("snd_seq_port_info_set_name %s (%p) \n",name);


  return snd_seq_port_info_set_name	(	info, name );
}

///////////////////////////////////////////////////////////////////////////////
// Alsa create a simple seq port
///////////////////////////////////////////////////////////////////////////////
int snd_seq_create_simple_port	(	snd_seq_t * 	seq, const char * 	name, unsigned int 	caps, unsigned int 	type )
{

  static int extCtrlPortCountW = 0;
  static int extCtrlPortCountR = 0;  

  tklog_info("Port creation : %s\n",name);

  snd_seq_client_info_t *cinfo;
  snd_seq_client_info_alloca(&cinfo);
  snd_seq_get_client_info (seq, cinfo);

  // Rename virtual port correctly. Impossible with the native Alsa...
  if ( strncmp (" Virtual RawMIDI",name,16) && snd_seq_virtual_port_rename_flag  )
  {
    //tklog_info("Virtual port renamed to %s \n",snd_seq_virtual_port_newname);
    snd_seq_virtual_port_rename_flag = 0;
    int r = orig_snd_seq_create_simple_port(seq,snd_seq_virtual_port_newname,caps,type);
    if ( r < 0 ) return r;

    // Get port information
    snd_seq_port_info_t *pinfo;
    snd_seq_port_info_alloca(&pinfo);
    snd_seq_get_port_info(seq, 0, pinfo);
    snd_seq_virtual_port_clientid =  snd_seq_port_info_get_client(pinfo);

    return r;
  }

  // In some specific cases, public and private ports could appear in the APP when spoofing,
  // because port names haven't the same prefixes (eg. Force vs MPC). The consequence is
  // that the MPC App receives midi message of buttons and encoders in midi tracks.
  // So we mask here Private and Public ports eventually requested by MPC App, which
  // should be only internal rawmidi ports.

  // This match will also catch our TKGL virtual ports having Private or Public suffix.

  // We do not allow MPC ports creation by MPC app for our device or our virtuals ports
  // Because this could lead to infinite midi loop in the MPC midi end user settings

	if (  strncmp("MPC",snd_seq_client_info_get_name(cinfo),3) == 0  ) {

    if ( TkRouter.MPCCtrl.cli < 0 ) {
      TkRouter.MPCCtrl.cli = snd_seq_client_info_get_client(cinfo);
      tklog_info("MPC seq client is %d\n",TkRouter.MPCCtrl.cli);
    }

    if ( match(name,".* Public$|.* Private$" )  ) {
       tklog_info("  => MPC app Port %s creation canceled.\n",name);
       return -1;
    }

    // External controller port capture  (MPC wants a direct connection to but we dont't)

    if ( TkRouter.Ctrl.cli >= 0  ) {

        // Do not create connection to our controller/port
        if ( match(name,anyctrl_name ) )  {
            uint8_t cancelPort = 0;
            if ( caps & SND_SEQ_PORT_CAP_WRITE ) {
                if ( TkRouter.Ctrl.port == extCtrlPortCountW++ ) cancelPort = 1;
            }
            else {
                if ( TkRouter.Ctrl.port == extCtrlPortCountR++ ) cancelPort = 1;
            }

            if ( cancelPort ) {
             tklog_info("  => MPC app Port %s creation canceled.\n",name);
             return -1;
            }
        }
        else
        // Our special MPC port to manage midi flow to external controller
        if ( strcmp( name + strlen(ROUTER_SEQ_NAME) + 1, ROUTER_CTRL_PORT_NAME) == 0 ) {
          int r = orig_snd_seq_create_simple_port(seq,name,caps,type);
          if ( r< 0 ) return r;
          if ( caps & SND_SEQ_PORT_CAP_WRITE ) {
            TkRouter.MPCCtrl.portIn = r;
            tklog_info("MPC app controller mirroring port In is %d.\n",TkRouter.MPCCtrl.portIn);
          }
          else {
            TkRouter.MPCCtrl.portOut = r;
            tklog_info("MPC app controller mirroring port Out is %d.\n",TkRouter.MPCCtrl.portOut);
          }
          return r;
        }
    }

  }


 return orig_snd_seq_create_simple_port(seq,name,caps,type);
}

///////////////////////////////////////////////////////////////////////////////
// Decode a midi seq event
///////////////////////////////////////////////////////////////////////////////
long snd_midi_event_decode	(	snd_midi_event_t * 	dev,unsigned char * 	buf,long 	count, const snd_seq_event_t * 	ev )
{

	// Disable running status to be a true "raw" midi. Side effect : disabled for all ports...
	snd_midi_event_no_status(dev,1);
  long r = orig_snd_midi_event_decode(dev,buf,count,ev);

	return r ;

}
