/*
__ __| |           |  /_) |     ___|             |           |
  |   __ \   _ \  ' /  | |  / |      _ \ __ \   |      _` | __ \   __|
  |   | | |  __/  . \  |   <  |   |  __/ |   |  |     (   | |   |\__ \
 _|  _| |_|\___| _|\_\_|_|\_\\____|\___|_|  _| _____|\__,_|_.__/ ____/
-----------------------------------------------------------------------------
TKGL_MPCMAPPER  LD_PRELOAD library.
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

#include "tkgl_mpcmapper.h"
#include "iamforce.h"

// Log utilities ---------------------------------------------------------------

static const char *tklog_level_strings[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "***ERROR", "***FATAL"};

void tklog(int level, const char *fmt, ...)
{

    va_list ap;

    // time_t timestamp = time( NULL );
    // struct tm * now = localtime( & timestamp );

    // char buftime[16];
    // buftime[strftime(buftime, sizeof(buftime), "%H:%M:%S", now)] = '\0';

    fprintf(stdout, "[tkgl %-8s]  ", tklog_level_strings[level]);

    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);

    fflush(stdout);
}

// Function prototypes ---------------------------------------------------------

// Globals ---------------------------------------------------------------------

// Our MPC product id (index in the table)
int MPCOriginalId = -1;
// The one used
int MPCId = -1;
// and the spoofed one,
int MPCSpoofedID = -1;

// Raw midi dump flag (for debugging purpose)
static uint8_t rawMidiDumpFlag = 0;     // Before transformation
static uint8_t rawMidiDumpPostFlag = 0; // After our tranformation

// Config file name
static char *configFileName = NULL;

// rawvirt in/out priv/pub
snd_rawmidi_t *rawvirt_inpriv = NULL;
snd_rawmidi_t *rawvirt_outpriv = NULL;
snd_rawmidi_t *rawvirt_outpub = NULL;

// End user virtual port name
static char *user_virtual_portname = NULL;

// End user virtual port handles
static snd_rawmidi_t *rawvirt_user_in = NULL;
static snd_rawmidi_t *rawvirt_user_out = NULL;

// Internal product code file handler to change on the fly when the file will be opened
// That avoids all binding stuff in shell
static int product_code_file_handler = -1;
static int product_compatible_file_handler = -1;
// Power supply file handlers
static int pws_online_file_handler = -1;
static int pws_voltage_file_handler = -1;
// static FILE *fpws_voltage_file_handler;

static int pws_present_file_handler = -1;
static int pws_status_file_handler = -1;
static int pws_capacity_file_handler = -1;

// MPC alsa informations
static int mpc_midi_card = -1;
static int mpc_seq_client = -1;
static int mpc_seq_client_private = -1;
// static int  mpc_seq_client_public = -1;

static char mpc_midi_private_alsa_name[20];
static char mpc_midi_public_alsa_name[20];

// Midi our controller seq client
// static int anyctrl_midi_card = -1;
static int anyctrl_seq_client = -1;
// static int anyctrl_seq_client_port=-1;

// static snd_rawmidi_t *rawmidi_inanyctrl  = NULL;
// static snd_rawmidi_t *rawmidi_outanyctrl = NULL;
// static char * anyctrl_name = NULL;

// Virtual rawmidi pointers to fake the MPC app
// snd_rawmidi_t *rawvirt_inpriv = NULL;
// snd_rawmidi_t *rawvirt_outpriv = NULL;
// snd_rawmidi_t *rawvirt_outpub = NULL;

// Sequencers virtual client addresses
static int seqvirt_client_inpriv = -1;
static int seqvirt_client_outpriv = -1;
static int seqvirt_client_outpub = -1;

// Alsa API hooks declaration
static typeof(&snd_rawmidi_open) orig_snd_rawmidi_open;
static typeof(&snd_rawmidi_close) orig_snd_rawmidi_close;
static typeof(&snd_seq_create_simple_port) orig_snd_seq_create_simple_port;
static typeof(&snd_midi_event_decode) orig_snd_midi_event_decode;
static typeof(&snd_seq_open) orig_snd_seq_open;
static typeof(&snd_seq_close) orig_snd_seq_close;
static typeof(&snd_seq_port_info_set_name) orig_snd_seq_port_info_set_name;
typeof(&snd_rawmidi_read) orig_snd_rawmidi_read;
typeof(&snd_rawmidi_write) orig_snd_rawmidi_write;
typeof(&open64) orig_open64;
typeof(&close) orig_close;
// static typeof(&snd_seq_event_input) orig_snd_seq_event_input;

// Globals used to rename a virtual port and get the client id.  No other way...
static int snd_seq_virtual_port_rename_flag = 0;
static char snd_seq_virtual_port_newname[30];
static int snd_seq_virtual_port_clientid = -1;

///////////////////////////////////////////////////////////////////////////////
// Match string against a regular expression
///////////////////////////////////////////////////////////////////////////////
int match(const char *string, const char *pattern)
{
    int status;
    regex_t re;

    if (regcomp(&re, pattern, REG_EXTENDED | REG_NOSUB) != 0)
    {
        return (0); /* Report error. */
    }
    status = regexec(&re, string, (size_t)0, NULL, 0);
    regfree(&re);
    if (status != 0)
    {
        return (0); /* Report error. */
    }
    return (1);
}

///////////////////////////////////////////////////////////////////////////////
// Get an ALSA card from a matching regular expression pattern
///////////////////////////////////////////////////////////////////////////////
int GetCardFromShortName(const char *pattern)
{
    int card = -1;
    int found_card = -1;
    char *shortname = NULL;

    if (snd_card_next(&card) < 0)
        return -1;
    while (card >= 0)
    {
        if (snd_card_get_name(card, &shortname) == 0)
        {
            tklog_debug("[getcardshortname] Card %d : %s <> %s\n", card, shortname, pattern);
            // Check if both strings are equal
            if (strcmp(shortname, pattern) == 0)
            {
                found_card = card;
            };
        }
        if (snd_card_next(&card) < 0)
            break;
    }
    return found_card;
}

///////////////////////////////////////////////////////////////////////////////
// Get an ALSA sequencer client , port and alsa card  from a regexp pattern
///////////////////////////////////////////////////////////////////////////////
// Will return 0 if found,
int GetSeqClientFromPortName(const char *name, int *card, int *clientId, int *portId)
{

    if (name == NULL)
        return -1;
    char port_name[128];
    int c = -1;

    snd_seq_t *seq;
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0)
    {
        fprintf(stderr, "*** Error : impossible to open default seq\n");
        return -1;
    }

    snd_seq_client_info_t *cinfo;
    snd_seq_port_info_t *pinfo;

    snd_seq_client_info_alloca(&cinfo);
    snd_seq_port_info_alloca(&pinfo);
    snd_seq_client_info_set_client(cinfo, -1);
    while (snd_seq_query_next_client(seq, cinfo) >= 0)
    {
        /* reset query info */
        snd_seq_port_info_set_client(pinfo, snd_seq_client_info_get_client(cinfo));
        snd_seq_port_info_set_port(pinfo, -1);
        while (snd_seq_query_next_port(seq, pinfo) >= 0)
        {
            sprintf(port_name, "%s %s", snd_seq_client_info_get_name(cinfo), snd_seq_port_info_get_name(pinfo));
            tklog_debug("Scanning port %s\n", port_name);
            if (match(port_name, name))
            {
                c = GetCardFromShortName(snd_seq_client_info_get_name(cinfo));
                if (c < 0)
                    return -1;
                *card = c;
                *clientId = snd_seq_port_info_get_client(pinfo);
                *portId = snd_seq_port_info_get_port(pinfo);
                tklog_debug("(hw:%d) Client %d:%d - %s\n", c, snd_seq_port_info_get_client(pinfo), snd_seq_port_info_get_port(pinfo), port_name);
                return 0;
            }
        }
    }

    snd_seq_close(seq);
    return -1;
}

///////////////////////////////////////////////////////////////////////////////
// Get the last port ALSA sequencer client
///////////////////////////////////////////////////////////////////////////////
int GetLastPortSeqClient()
{

    int r = -1;

    snd_seq_t *seq;
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0)
    {
        fprintf(stderr, "*** Error : impossible to open default seq\n");
        return -1;
    }

    snd_seq_client_info_t *cinfo;
    snd_seq_port_info_t *pinfo;

    snd_seq_client_info_alloca(&cinfo);
    snd_seq_port_info_alloca(&pinfo);
    snd_seq_client_info_set_client(cinfo, -1);
    while (snd_seq_query_next_client(seq, cinfo) >= 0)
    {
        /* reset query info */
        snd_seq_port_info_set_client(pinfo, snd_seq_client_info_get_client(cinfo));
        snd_seq_port_info_set_port(pinfo, -1);
        while (snd_seq_query_next_port(seq, pinfo) >= 0)
        {
            r = snd_seq_client_info_get_client(cinfo);
            // fprintf(stdout,"client %s -- %d port %s %d\n",snd_seq_client_info_get_name(cinfo) ,r, snd_seq_port_info_get_name(pinfo), snd_seq_port_info_get_port(pinfo) );
        }
    }

    snd_seq_close(seq);
    return r;
}

///////////////////////////////////////////////////////////////////////////////
// ALSA aconnect utility API equivalent
///////////////////////////////////////////////////////////////////////////////
int aconnect(int src_client, int src_port, int dest_client, int dest_port)
{
    int queue = 0, convert_time = 0, convert_real = 0, exclusive = 0;
    snd_seq_port_subscribe_t *subs;
    snd_seq_addr_t sender, dest;
    int client;
    char addr[10];

    snd_seq_t *seq;
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0)
    {
        tklog_error("Impossible to open default seq\n");
        return -1;
    }

    if ((client = snd_seq_client_id(seq)) < 0)
    {
        tklog_error("Impossible to get seq client id\n");
        snd_seq_close(seq);
        return -1;
    }

    /* set client info */
    if (snd_seq_set_client_name(seq, "ALSA Connector") < 0)
    {
        tklog_error("Set client name failed\n");
        snd_seq_close(seq);
        return -1;
    }

    /* set subscription */
    sprintf(addr, "%d:%d", src_client, src_port);
    if (snd_seq_parse_address(seq, &sender, addr) < 0)
    {
        snd_seq_close(seq);
        tklog_error("Invalid source address %s\n", addr);
        return -1;
    }

    sprintf(addr, "%d:%d", dest_client, dest_port);
    if (snd_seq_parse_address(seq, &dest, addr) < 0)
    {
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

    if (snd_seq_get_port_subscription(seq, subs) == 0)
    {
        snd_seq_close(seq);
        tklog_info("Connection of midi port %d:%d to %d:%d already subscribed\n", src_client, src_port, dest_client, dest_port);
        return 0;
    }

    if (snd_seq_subscribe_port(seq, subs) < 0)
    {
        snd_seq_close(seq);
        tklog_error("Connection of midi port %d:%d to %d:%d failed !\n", src_client, src_port, dest_client, dest_port);
        return 1;
    }

    tklog_info("Connection of midi port %d:%d to %d:%d successfull\n", src_client, src_port, dest_client, dest_port);

    snd_seq_close(seq);
    return 0;
}

///////////////////////////////////////////////////////////////////////////////
// Get MPC hardware name from sysex id
///////////////////////////////////////////////////////////////////////////////

static int GetIndexOfMPCId(uint8_t id)
{
    for (int i = 0; i < _END_MPCID; i++)
        if (DeviceInfoBloc[i].sysexId == id)
            return i;
    return -1;
}

const char *GetHwNameFromMPCId(uint8_t id)
{
    int i = GetIndexOfMPCId(id);
    if (i >= 0)
        return DeviceInfoBloc[i].productString;
    else
        return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// Show MPCMAPPER HELP
///////////////////////////////////////////////////////////////////////////////
void ShowHelp(void)
{

    tklog_info("\n");
    tklog_info("--tgkl_help               : Show this help\n");
    tklog_info("--tkgl_ctrlname=<name>    : Use external controller containing <name>\n");
    tklog_info("--tkgl_iamX               : Emulate MPC X\n");
    tklog_info("--tkgl_iamLive            : Emulate MPC Live\n");
    tklog_info("--tkgl_iamForce           : Emulate Force\n");
    tklog_info("--tkgl_iamOne             : Emulate MPC One\n");
    tklog_info("--tkgl_iamLive2           : Emulate MPC Live Mk II\n");
    tklog_info("--tkgl_virtualport=<name> : Create end user virtual port <name>\n");
    tklog_info("--tkgl_mididump           : Dump original raw midi flow\n");
    tklog_info("--tkgl_mididumpPost       : Dump raw midi flow after transformation\n");
    tklog_info("--tkgl_configfile=<name>  : Use configuration file <name>\n");
    tklog_info("\n");
    exit(0);
}

///////////////////////////////////////////////////////////////////////////////
// Setup tkgl anyctrl
///////////////////////////////////////////////////////////////////////////////
static void tkgl_init()
{

    // System call hooks
    orig_open64 = dlsym(RTLD_NEXT, "open64");
    // orig_open   = dlsym(RTLD_NEXT, "open");
    orig_close = dlsym(RTLD_NEXT, "close");
    // orig_read   = dlsym(RTLD_NEXT, "read");

    // Alsa hooks
    orig_snd_rawmidi_open = dlsym(RTLD_NEXT, "snd_rawmidi_open");
    orig_snd_rawmidi_close = dlsym(RTLD_NEXT, "snd_rawmidi_close");
    orig_snd_rawmidi_read = dlsym(RTLD_NEXT, "snd_rawmidi_read");
    orig_snd_rawmidi_write = dlsym(RTLD_NEXT, "snd_rawmidi_write");
    orig_snd_seq_create_simple_port = dlsym(RTLD_NEXT, "snd_seq_create_simple_port");
    orig_snd_midi_event_decode = dlsym(RTLD_NEXT, "snd_midi_event_decode");
    orig_snd_seq_open = dlsym(RTLD_NEXT, "snd_seq_open");
    orig_snd_seq_close = dlsym(RTLD_NEXT, "snd_seq_close");
    orig_snd_seq_port_info_set_name = dlsym(RTLD_NEXT, "snd_seq_port_info_set_name");
    // orig_snd_seq_event_input        = dlsym(RTLD_NEXT, "snd_seq_event_input");

    // Read product code
    char product_code[4];
    int fd = open(PRODUCT_CODE_PATH, O_RDONLY);
    read(fd, &product_code, 4);

    // Find the id in the product code table
    for (int i = 0; i < _END_MPCID; i++)
    {
        if (strncmp(DeviceInfoBloc[i].productCode, product_code, 4) == 0)
        {
            MPCOriginalId = i;
            break;
        }
    }
    if (MPCOriginalId < 0)
    {
        tklog_fatal("Error when reading the product-code file\n");
        exit(1);
    }
    tklog_info("Original Product code : %s (%s)\n", DeviceInfoBloc[MPCOriginalId].productCode, DeviceInfoBloc[MPCOriginalId].productString);

    if (MPCSpoofedID >= 0)
    {
        tklog_info("Product code spoofed to %s (%s)\n", DeviceInfoBloc[MPCSpoofedID].productCode, DeviceInfoBloc[MPCSpoofedID].productString);
        MPCId = MPCSpoofedID;
    }
    else
        MPCId = MPCOriginalId;

    // Fake the power supply ?
    if (DeviceInfoBloc[MPCOriginalId].fakePowerSupply)
        tklog_info("The power supply will be faked to allow battery mode.\n");

    // read mapping config file if any
    LoadMapping();

    // Retrieve MPC midi card info
    if (GetSeqClientFromPortName(CTRL_MPC_ALL_PRIVATE, &mpc_midi_card, &mpc_seq_client, &mpc_seq_client_private) < 0)
    {
        tklog_fatal("Error : MPC controller card/seq client not found (regex pattern is '%s')\n", CTRL_MPC_ALL_PRIVATE);
        exit(1);
    }

    sprintf(mpc_midi_private_alsa_name, "hw:%d,0,1", mpc_midi_card);
    sprintf(mpc_midi_public_alsa_name, "hw:%d,0,0", mpc_midi_card);
    tklog_info("MPC controller card id hw:%d found\n", mpc_midi_card);
    tklog_info("MPC controller Private port is %s\n", mpc_midi_private_alsa_name);
    tklog_info("MPC controller Public port is %s\n", mpc_midi_public_alsa_name);
    tklog_info("MPC controller seq client is %d\n", mpc_seq_client);

    // Create 3 virtuals ports : Private I/O, Public O
    // This will trig our hacked snd_seq_create_simple_port during the call.
    // NB : the snd_rawmidi_open is hacked here to return the client id of the virtual port.
    // So, return is either < 0 if error, or > 0, being the client number if everything is ok.
    // The port is always 0. This is the standard behaviour.

    seqvirt_client_inpriv = snd_rawmidi_open(&rawvirt_inpriv, NULL, "[virtual]TKGL Virtual In Private", 2);
    seqvirt_client_outpriv = snd_rawmidi_open(NULL, &rawvirt_outpriv, "[virtual]TKGL Virtual Out Private", 3);
    seqvirt_client_outpub = snd_rawmidi_open(NULL, &rawvirt_outpub, "[virtual]TKGL Virtual Out Public", 3);

    if (seqvirt_client_inpriv < 0 || seqvirt_client_outpriv < 0 || seqvirt_client_outpub < 0)
    {
        tklog_fatal("Impossible to create one or many virtual ports\n");
        exit(1);
    }

    tklog_info("Virtual private input port %d  created.\n", seqvirt_client_inpriv);
    tklog_info("Virtual private output port %d created.\n", seqvirt_client_outpriv);
    tklog_info("Virtual public output port %d created.\n", seqvirt_client_outpub);

    // Make connections of our virtuals ports
    // MPC APP <---> VIRTUAL PORTS <---> MPC CONTROLLER PRIVATE & PUBLIC PORTS

    // Private MPC controller port 1 out to virtual In priv 0
    aconnect(mpc_seq_client, 1, seqvirt_client_inpriv, 0);

    // Virtual out priv 0 to Private MPC controller port 1 in
    aconnect(seqvirt_client_outpriv, 0, mpc_seq_client, 1);

    // Virtual out pub to Public MPC controller port 1 in
    // No need to cable the out...
    aconnect(seqvirt_client_outpub, 0, mpc_seq_client, 0);

    // Connect our controller if used
    if (anyctrl_seq_client >= 0)
    {
        // port 0 to virtual In 0,
        aconnect(anyctrl_seq_client, 0, seqvirt_client_inpriv, 0);

        // Virtual out priv 0 to our controller port 0
        aconnect(seqvirt_client_outpriv, 0, anyctrl_seq_client, 0);

        // Virtual out public to our controller port 0
        aconnect(seqvirt_client_outpub, 0, anyctrl_seq_client, 0);
    }

    // Create a user virtual port if asked on the command line
    if (user_virtual_portname != NULL)
    {

        char temp_portname[64];
        sprintf(temp_portname, "[virtual]%s", user_virtual_portname);
        if (snd_rawmidi_open(&rawvirt_user_in, &rawvirt_user_out, temp_portname, 0) < 0)
        {
            tklog_fatal("Impossible to create virtual user port %s\n", user_virtual_portname);
            exit(1);
        }
        tklog_info("Virtual user port %s succesfully created.\n", user_virtual_portname);
        // snd_rawmidi_open(&read_handle, &write_handle, "virtual", 0);
    }

    // // Show battery status
    // displayBatteryStatus();

    // Show 'PJ' on the pads!
    /*
        [x] [x] [ ] [y]
        [x] [x] [ ] [y]
        [x] [ ] [y] [y]
        [x] [ ] [y] [y]
    */
    tklog_info("Setting pads colors...\n");
    setPadColorFromColorInt(0x0, COLOR_CYAN);
    setPadColorFromColorInt(0x1, COLOR_CYAN);
    setPadColorFromColorInt(0x2, COLOR_BLACK);
    setPadColorFromColorInt(0x3, COLOR_PINK);
    setPadColorFromColorInt(0x4, COLOR_CYAN);
    setPadColorFromColorInt(0x5, COLOR_CYAN);
    setPadColorFromColorInt(0x6, COLOR_BLACK);
    setPadColorFromColorInt(0x7, COLOR_PINK);
    setPadColorFromColorInt(0x8, COLOR_CYAN);
    setPadColorFromColorInt(0x9, COLOR_BLACK);
    setPadColorFromColorInt(0xa, COLOR_PINK);
    setPadColorFromColorInt(0xb, COLOR_PINK);
    setPadColorFromColorInt(0xc, COLOR_CYAN);
    setPadColorFromColorInt(0xd, COLOR_BLACK);
    setPadColorFromColorInt(0xe, COLOR_PINK);
    setPadColorFromColorInt(0xf, COLOR_PINK);
    tklog_info("DONE...\n");

    fflush(stdout);
}

////////////////////////////////////////////////////////////////////////////////
// Clean DUMP of a buffer to screen
////////////////////////////////////////////////////////////////////////////////
void ShowBufferHexDump(const uint8_t *data, size_t sz, uint8_t nl)
{
    uint8_t b;
    uint16_t idx = 0;
    // char asciiBuff[33];
    // uint8_t c = 0;

    tklog_trace("");
    for (idx = 0; idx < sz; idx++)
    {
        b = (*data++);
        fprintf(stdout, "%02X ", b);
        if (b == 0xF7)
        {
            idx++;
            break;
        }
        // asciiBuff[c++] = (b >= 0x20 && b < 127 ? b : '.');
        // if (c == nl || idx == sz - 1)
        // {
        //     asciiBuff[c] = 0;
        //     for (; c < nl; c++)
        //         fprintf(stdout, "   ");
        //     c = 0;
        //     fprintf(stdout, " | %s\n", asciiBuff);
        // }
    }

    fprintf(stdout, " -... \n");
    if (idx < sz)
    {
        ShowBufferHexDump(data, sz - idx, nl);
    }

}

////////////////////////////////////////////////////////////////////////////////
// RawMidi dump
////////////////////////////////////////////////////////////////////////////////
static void RawMidiDump(snd_rawmidi_t *rawmidi, char io, char rw, const uint8_t *data, size_t sz)
{

    const char *name = snd_rawmidi_name(rawmidi);

    // Skip B0 35 messages
    if (sz > 2 && data[0] == 0xb0 && data[1] == 0x35)
        return;

    tklog_trace("%s dump snd_rawmidi_%s from controller %s\n", io == 'i' ? "Entry" : "Post", rw == 'r' ? "read" : "write", name);
    ShowBufferHexDump(data, sz, 16);
    tklog_trace("\n");
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
    fprintf(stdout, "\n%s", TKGL_LOGO);
    tklog_info("---------------------------------------------------------\n");
    tklog_info("TKGL_MPCMAPPER Version : %s\n", VERSION);
    tklog_info("(c) The KikGen Labs.\n");
    tklog_info("https://github.com/TheKikGen/MPC-LiveXplore\n");
    tklog_info("---------------------------------------------------------\n");

    // Show the command line
    tklog_info("MPC args : ");

    for (int i = 1; i < argc; i++)
    {
        fprintf(stdout, "%s ", argv[i]);
    }
    fprintf(stdout, "\n");
    tklog_info("\n");

    // Scan command line
    char *tkgl_SpoofArg = NULL;

    for (int i = 1; i < argc; i++)
    {

        // help
        if ((strcmp("--tkgl_help", argv[i]) == 0))
        {
            ShowHelp();
        }
        else
            // Spoofed product id
            if ((strcmp("--tkgl_iamX", argv[i]) == 0))
            {
                MPCSpoofedID = MPC_X;
                tkgl_SpoofArg = argv[i];
            }
            else if ((strcmp("--tkgl_iamLive", argv[i]) == 0))
            {
                MPCSpoofedID = MPC_LIVE;
                tkgl_SpoofArg = argv[i];
            }
            else if ((strcmp("--tkgl_iamForce", argv[i]) == 0))
            {
                MPCSpoofedID = MPC_FORCE;
                tkgl_SpoofArg = argv[i];
            }
            else if ((strcmp("--tkgl_iamOne", argv[i]) == 0))
            {
                MPCSpoofedID = MPC_ONE;
                tkgl_SpoofArg = argv[i];
            }
            else if ((strcmp("--tkgl_iamLive2", argv[i]) == 0))
            {
                MPCSpoofedID = MPC_LIVE_MK2;
                tkgl_SpoofArg = argv[i];
            }
            else
                // End user virtual port visible from the MPC app
                if ((strncmp("--tkgl_virtualport=", argv[i], 19) == 0) && (strlen(argv[i]) > 19))
                {
                    user_virtual_portname = argv[i] + 19;
                    tklog_info("--tkgl_virtualport specified as %s port name\n", user_virtual_portname);
                }
                else
                    // Dump rawmidi
                    if ((strcmp("--tkgl_mididump", argv[i]) == 0))
                    {
                        rawMidiDumpFlag = 1;
                        tklog_info("--tkgl_mididump specified : dump original raw midi message (ENTRY)\n");
                    }
                    else if ((strcmp("--tkgl_mididumpPost", argv[i]) == 0))
                    {
                        rawMidiDumpPostFlag = 1;
                        tklog_info("--tkgl_mididumpPost specified : dump raw midi message after transformation (POST)\n");
                    }
                    else
                        // Config file name
                        if ((strncmp("--tkgl_configfile=", argv[i], 18) == 0) && (strlen(argv[i]) > 18))
                        {
                            configFileName = argv[i] + 18;
                            tklog_info("--tkgl_configfile specified. File %s will be used for mapping\n", configFileName);
                        }
    }

    if (MPCSpoofedID >= 0)
    {
        tklog_info("%s specified. %s spoofing.\n", tkgl_SpoofArg, DeviceInfoBloc[MPCSpoofedID].productString);
    }

    // Initialize everything
    tkgl_init();

    int r = orig(main, argc, argv, init, fini, rtld_fini, stack_end);

    tklog_info("End of mcpmapper\n");

    // ... and call main again
    return r;
}

///////////////////////////////////////////////////////////////////////////////
// ALSA snd_rawmidi_open hooked
///////////////////////////////////////////////////////////////////////////////
// This function allows changing the name of a virtual port, by using the
// naming convention "[virtual]port name"
// and if creation succesfull, will return the client number. Port is always 0 if virtual.

int snd_rawmidi_open(snd_rawmidi_t **inputp, snd_rawmidi_t **outputp, const char *name, int mode)
{

    // tklog_debug("snd_rawmidi_open name %s mode %d\n",name,mode);

    // Rename the virtual port as we need
    // Port Name must not be emtpy - 30 chars max
    if (strncmp(name, "[virtual]", 9) == 0)
    {
        int l = strlen(name);
        if (l <= 9 || l > 30 + 9)
            return -1;

        // Prepare the renaming of the virtual port
        strcpy(snd_seq_virtual_port_newname, name + 9);
        snd_seq_virtual_port_rename_flag = 1;

        // Create the virtual port via the fake Alsa rawmidi virtual open
        int r = orig_snd_rawmidi_open(inputp, outputp, "virtual", mode);
        if (r < 0)
            return r;

        // Get the port id that was populated in the port creation sub function
        // and reset it
        r = snd_seq_virtual_port_clientid;
        snd_seq_virtual_port_clientid = -1;

        // tklog_debug("PORT ID IS %d\n",r);

        return r;
    }

    // Substitute the hardware private input port by our input virtual ports

    else if (strcmp(mpc_midi_private_alsa_name, name) == 0)
    {

        // Private In
        if (inputp)
            *inputp = rawvirt_inpriv;
        else if (outputp)
            *outputp = rawvirt_outpriv;
        else
            return -1;
        tklog_info("%s substitution by virtual rawmidi successfull\n", name);

        return 0;
    }

    else if (strcmp(mpc_midi_public_alsa_name, name) == 0)
    {

        if (outputp)
            *outputp = rawvirt_outpub;
        else
            return -1;
        tklog_info("%s substitution by virtual rawmidi successfull\n", name);

        return 0;
    }

    return orig_snd_rawmidi_open(inputp, outputp, name, mode);
}

///////////////////////////////////////////////////////////////////////////////
// ALSA snd_rawmidi_close hooked
///////////////////////////////////////////////////////////////////////////////
//

int snd_rawmidi_close(snd_rawmidi_t *rawmidi)
{

    // tklog_debug("snd_rawmidi_close handle :%p\n",rawmidi);

    return orig_snd_rawmidi_close(rawmidi);
}

///////////////////////////////////////////////////////////////////////////////
// Alsa Rawmidi read
///////////////////////////////////////////////////////////////////////////////
ssize_t snd_rawmidi_read(snd_rawmidi_t *rawmidi, void *buffer, size_t size)
{

    // tklog_debug("snd_rawmidi_read %p : size : %u ", rawmidi, size);

    ssize_t r = orig_snd_rawmidi_read(rawmidi, buffer, size);
    if (r < 0)
    {
        tklog_error("snd_rawmidi_read error : (%p) size : %u  error %d\n", rawmidi, size, r);
        return r;
    }

    if (rawMidiDumpFlag)
        RawMidiDump(rawmidi, 'i', 'r', buffer, r);

    // Map in all cases if the app writes to the controller
    if (rawmidi == rawvirt_inpriv)
    {

        // We are running on a Force
        if (MPCOriginalId == MPC_FORCE)
        {
            // We want to map things on Force it self
            if (MPCId == MPC_FORCE)
            {
                tklog_fatal("Force => Force - I disabled this, sorry\n");
                exit(1);
            }
            // Simulate a MPC on a Force
            else
            {
                tklog_fatal("Force => MPC - I disabled this, sorry\n");
                exit(1);
            }
        }
        // We are running on a MPC
        else
        {
            // We need to remap on a MPC it self
            if (MPCId != MPC_FORCE)
            {
                tklog_fatal("MPC => MPC - I disabled this, sorry\n");
                exit(1);
            }
            // Simulate a Force on a MPC
            else
            {
                r = Mpc_MapReadFromForce(buffer, size, r);
            }
        }
    }

    if (rawMidiDumpPostFlag)
        RawMidiDump(rawmidi, 'o', 'r', buffer, r);

    return r;
}

///////////////////////////////////////////////////////////////////////////////
// Alsa Rawmidi write
///////////////////////////////////////////////////////////////////////////////
ssize_t snd_rawmidi_write(snd_rawmidi_t *rawmidi, const void *buffer, size_t size)
{
    size_t new_size = 0;

    if (rawMidiDumpFlag)
        RawMidiDump(rawmidi, 'i', 'w', buffer, size);

    // Map in all cases if the app writes to the controller
    if (rawmidi == rawvirt_outpriv || rawmidi == rawvirt_outpub)
    {

        // We are running on a Force
        if (MPCOriginalId == MPC_FORCE)
        {
            // We want to map things on Force it self
            if (MPCId == MPC_FORCE)
            {
                tklog_fatal("I disabled this, sorry\n");
                exit(1);
            }
            // Simulate a MPC on a Force
            else
            {
                tklog_fatal("I disabled this, sorry\n");
                exit(1);
            }
        }
        // We are running on a MPC
        else
        {
            // We need to remap on a MPC it self
            if (MPCId != MPC_FORCE)
            {
                tklog_fatal("I disabled this, sorry\n");
                exit(1);
            }
            // Simulate a Force on a MPC
            else
            {
                new_size += Mpc_MapAppWriteToForce(buffer, size);
                // LOG_DEBUG("   Input len: %d, output len: %d", size, new_size);
            }
        }
    }

    if (rawMidiDumpPostFlag)
        RawMidiDump(rawmidi, 'o', 'w', buffer, new_size);

    return orig_snd_rawmidi_write(rawmidi, buffer, new_size);
}

///////////////////////////////////////////////////////////////////////////////
// Alsa open sequencer
///////////////////////////////////////////////////////////////////////////////
int snd_seq_open(snd_seq_t **handle, const char *name, int streams, int mode)
{

    // tklog_debug("snd_seq_open %s (%p) \n",name,handle);

    return orig_snd_seq_open(handle, name, streams, mode);
}

///////////////////////////////////////////////////////////////////////////////
// Alsa close sequencer
///////////////////////////////////////////////////////////////////////////////
int snd_seq_close(snd_seq_t *handle)
{

    // tklog_debug("snd_seq_close. Hanlde %p \n",handle);

    return orig_snd_seq_close(handle);
}

///////////////////////////////////////////////////////////////////////////////
// Alsa set a seq port name
///////////////////////////////////////////////////////////////////////////////
void snd_seq_port_info_set_name(snd_seq_port_info_t *info, const char *name)
{
    // tklog_debug("snd_seq_port_info_set_name %s (%p) \n",name);

    return snd_seq_port_info_set_name(info, name);
}

///////////////////////////////////////////////////////////////////////////////
// Alsa create a simple seq port
///////////////////////////////////////////////////////////////////////////////
int snd_seq_create_simple_port(snd_seq_t *seq, const char *name, unsigned int caps, unsigned int type)
{
    tklog_info("Port creation : %s\n", name);

    // Rename virtual port correctly. Impossible with the native Alsa...
    if (strncmp(" Virtual RawMIDI", name, 16) && snd_seq_virtual_port_rename_flag)
    {
        // tklog_info("Virtual port renamed to %s \n",snd_seq_virtual_port_newname);
        snd_seq_virtual_port_rename_flag = 0;
        int r = orig_snd_seq_create_simple_port(seq, snd_seq_virtual_port_newname, caps, type);
        if (r < 0)
            return r;

        // Get port information
        snd_seq_port_info_t *pinfo;
        snd_seq_port_info_alloca(&pinfo);
        snd_seq_get_port_info(seq, 0, pinfo);
        snd_seq_virtual_port_clientid = snd_seq_port_info_get_client(pinfo);

        return r;
    }

    // We do not allow ports creation by MPC app for our device or our virtuals ports
    // Because this could lead to infinite midi loop in the MPC midi end user settings
    // In some specific cases, public and private ports could appear in the APP when spoofing,
    // because port names haven't the same prefixes (eg. Force vs MPC). The consequence is
    // that the MPC App receives midi message of buttons and encoders in midi tracks.
    // So we mask here Private and Public ports eventually requested by MPC App, which
    // should be only internal rawmidi ports.

    // This match will also catch our TKGL virtual ports having Private or Public suffix.
    if ((match(name, ".* Public$|.* Private$")))
    {
        tklog_info("Port %s creation canceled.\n", name);
        return -1;
    }

    return orig_snd_seq_create_simple_port(seq, name, caps, type);
}

///////////////////////////////////////////////////////////////////////////////
// Decode a midi seq event
///////////////////////////////////////////////////////////////////////////////
long snd_midi_event_decode(snd_midi_event_t *dev, unsigned char *buf, long count, const snd_seq_event_t *ev)
{

    // Disable running status to be a true "raw" midi. Side effect : disabled for all ports...
    snd_midi_event_no_status(dev, 1);
    long r = orig_snd_midi_event_decode(dev, buf, count, ev);

    return r;
}

///////////////////////////////////////////////////////////////////////////////
// close
///////////////////////////////////////////////////////////////////////////////
int close(int fd)
{

    if (fd == product_code_file_handler)
        product_code_file_handler = -1;
    else if (fd == product_compatible_file_handler)
        product_compatible_file_handler = -1;
    else if (fd == pws_online_file_handler)
        pws_online_file_handler = -1;
    else if (fd == pws_voltage_file_handler)
        pws_voltage_file_handler = -1;
    else if (fd == pws_present_file_handler)
        pws_present_file_handler = -1;
    else if (fd == pws_status_file_handler)
        pws_status_file_handler = -1;
    else if (fd == pws_capacity_file_handler)
        pws_capacity_file_handler = -1;

    return orig_close(fd);
}

///////////////////////////////////////////////////////////////////////////////
// fake_open : use memfd to create a fake file in memory
///////////////////////////////////////////////////////////////////////////////
int fake_open(const char *name, char *content, size_t contentSize)
{
    int fd = memfd_create(name, MFD_ALLOW_SEALING);
    write(fd, content, contentSize);
    lseek(fd, 0, SEEK_SET);
    return fd;
}

///////////////////////////////////////////////////////////////////////////////
// open64
///////////////////////////////////////////////////////////////////////////////
// Intercept all file opening to fake those we want to.

int open64(const char *pathname, int flags, ...)
{
    // If O_CREAT is used to create a file, the file access mode must be given.
    // We do not fake the create function at all.
    if (flags & O_CREAT)
    {
        va_list args;
        va_start(args, flags);
        int mode = va_arg(args, int);
        va_end(args);
        return orig_open64(pathname, flags, mode);
    }

    // Existing file
    // Fake files sections

    // product code
    if (product_code_file_handler < 0 && strcmp(pathname, PRODUCT_CODE_PATH) == 0)
    {
        // Create a fake file in memory
        product_code_file_handler = fake_open(pathname, DeviceInfoBloc[MPCId].productCode, strlen(DeviceInfoBloc[MPCId].productCode));
        return product_code_file_handler;
    }

    // product compatible
    if (product_compatible_file_handler < 0 && strcmp(pathname, PRODUCT_COMPATIBLE_PATH) == 0)
    {
        char buf[64];
        sprintf(buf, PRODUCT_COMPATIBLE_STR, DeviceInfoBloc[MPCId].productCompatible);
        product_compatible_file_handler = fake_open(pathname, buf, strlen(buf));
        return product_compatible_file_handler;
    }

    // Fake power supply files if necessary only (this allows battery mode)
    else if (DeviceInfoBloc[MPCOriginalId].fakePowerSupply)
    {

        if (pws_voltage_file_handler < 0 && strcmp(pathname, POWER_SUPPLY_VOLTAGE_NOW_PATH) == 0)
        {
            pws_voltage_file_handler = fake_open(pathname, POWER_SUPPLY_VOLTAGE_NOW, strlen(POWER_SUPPLY_VOLTAGE_NOW));
            return pws_voltage_file_handler;
        }

        if (pws_online_file_handler < 0 && strcmp(pathname, POWER_SUPPLY_ONLINE_PATH) == 0)
        {
            pws_online_file_handler = fake_open(pathname, POWER_SUPPLY_ONLINE, strlen(POWER_SUPPLY_ONLINE));
            return pws_online_file_handler;
        }

        if (pws_present_file_handler < 0 && strcmp(pathname, POWER_SUPPLY_PRESENT_PATH) == 0)
        {
            pws_present_file_handler = fake_open(pathname, POWER_SUPPLY_PRESENT, strlen(POWER_SUPPLY_PRESENT));
            return pws_present_file_handler;
        }

        if (pws_status_file_handler < 0 && strcmp(pathname, POWER_SUPPLY_STATUS_PATH) == 0)
        {
            pws_status_file_handler = fake_open(pathname, POWER_SUPPLY_STATUS, strlen(POWER_SUPPLY_STATUS));
            return pws_status_file_handler;
        }

        if (pws_capacity_file_handler < 0 && strcmp(pathname, POWER_SUPPLY_CAPACITY_PATH) == 0)
        {
            pws_capacity_file_handler = fake_open(pathname, POWER_SUPPLY_CAPACITY, strlen(POWER_SUPPLY_CAPACITY));
            return pws_capacity_file_handler;
        }
    }

    return orig_open64(pathname, flags);
}

// ///////////////////////////////////////////////////////////////////////////////
// // Process an input midi seq event
// ///////////////////////////////////////////////////////////////////////////////
// int snd_seq_event_input( snd_seq_t* handle, snd_seq_event_t** ev )
// {
//
//   int r = orig_snd_seq_event_input(handle,ev);
//   // if ((*ev)->type != SND_SEQ_EVENT_CLOCK ) {
//   //      dump_event(*ev);
//   //
//   //
//   //    // tklog_info("[tkgl] Src = %02d:%02d -> Dest = %02d:%02d \n",(*ev)->source.client,(*ev)->source.port,(*ev)->dest.client,(*ev)->dest.port);
//   //    // ShowBufferHexDump(buf, r,16);
//   //    // tklog_info("[tkgl] ----------------------------------\n");
//   //  }
//
//
//   return r;
//
// }

// ///////////////////////////////////////////////////////////////////////////////
// // open
// ///////////////////////////////////////////////////////////////////////////////
// int open(const char *pathname, int flags,...) {
//
// //  printf("(tkgl) Open %s\n",pathname);
//
//    // If O_CREAT is used to create a file, the file access mode must be given.
//    if (flags & O_CREAT) {
//        va_list args;
//        va_start(args, flags);
//        int mode = va_arg(args, int);
//        va_end(args);
//        return orig_open(pathname, flags, mode);
//    } else {
//        return orig_open(pathname, flags);
//    }
// }

//
// ///////////////////////////////////////////////////////////////////////////////
// // read
// ///////////////////////////////////////////////////////////////////////////////
// ssize_t read(int fildes, void *buf, size_t nbyte) {
//
//   return orig_read(fildes,buf,nbyte);
// }
