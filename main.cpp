#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <jack/jack.h>

#include "gme/Multi_Buffer.h"
#include "gme/Nsf_Emu.h"

class Quadraphonic_Buffer : public Multi_Buffer {
    static const int CHANNELS = 5;
	Blip_Buffer buf[CHANNELS];
	channel_t chan[CHANNELS];

public:
	Quadraphonic_Buffer() : Multi_Buffer( 1 )
    {
        for ( int i = 0; i < CHANNELS; i ++ )
        {
            chan[i].center = &buf[i];
            chan[i].left = &buf[i];
            chan[i].right = &buf[i];
        }
    }
	~Quadraphonic_Buffer() {};
	blargg_err_t set_sample_rate( long rate, int msec = blip_default_length )
    {
        for ( int i = 0; i < CHANNELS; i ++ )
            buf[i].set_sample_rate( rate, msec );
        return Multi_Buffer::set_sample_rate( buf[0].sample_rate(), buf[0].length() );
    }
	void clock_rate( long rate ) {
        for ( int i = 0; i < CHANNELS; i ++ )
            buf[i].clock_rate( rate );
    }
	void bass_freq( int freq ) {
        for ( int i = 0; i < CHANNELS; i ++ )
            buf[i].bass_freq( freq );
    }
	void clear() {
        for ( int i = 0; i < CHANNELS; i ++ )
            buf[i].clear();
    }
	long samples_avail() const {
        // UNUSED?
        return buf[0].samples_avail();
    }
	long read_samples( blip_sample_t* out, long count )
    {
        long avail = buf[0].samples_avail();
        if ( count > avail )
            count = avail;
        if ( count % 2 == 1 ) // make even
            count --;
        mix(out, count);
        for ( int i = 0; i < CHANNELS; i ++ )
            buf[i].remove_samples( count );
        return count;
    }
	channel_t channel( int index, int type )
    {
        return chan[index];
    }
	void end_frame( blip_time_t t ) {
        for ( int i = 0; i < CHANNELS; i ++ )
            buf[i].end_frame(t);
    }

    void mix( blip_sample_t* out_, blargg_long count )
    {
        blip_sample_t* BLIP_RESTRICT out = out_;
        BLIP_READER_BEGIN( square1, buf[0] );
        BLIP_READER_BEGIN( square2, buf[1] );
        BLIP_READER_BEGIN( triangle, buf[2] );
        BLIP_READER_BEGIN( noise, buf[3] );
        
        for ( ; count; --count )
        {
            blargg_long c1 = BLIP_READER_READ( square1 );
            blargg_long c2 = BLIP_READER_READ( square2 );
            blargg_long c3 = BLIP_READER_READ( triangle );
            blargg_long c4 = BLIP_READER_READ( noise );

            BLIP_READER_NEXT( square1, blip_reader_default_bass );
            BLIP_READER_NEXT( square2, blip_reader_default_bass );
            BLIP_READER_NEXT( triangle, blip_reader_default_bass );
            BLIP_READER_NEXT( noise, blip_reader_default_bass );
            out[0] = c1;
            out[1] = c2;
            out[2] = c3;
            out[3] = c4;
            out += 4;
        }
        
        BLIP_READER_END( noise, buf[3] );
        BLIP_READER_END( triangle, buf[2] );
        BLIP_READER_END( square2, buf[1] );
        BLIP_READER_END( square1, buf[0] );
    }
};

jack_port_t *port0;
jack_port_t *port1;
jack_port_t *port2;
jack_port_t *port3;
jack_nframes_t sample_rate;

static int process(jack_nframes_t frames, void *arg)
{
    jack_default_audio_sample_t *port_buffer_0 = (jack_default_audio_sample_t *)jack_port_get_buffer(port0, frames);
    jack_default_audio_sample_t *port_buffer_1 = (jack_default_audio_sample_t *)jack_port_get_buffer(port1, frames);
    jack_default_audio_sample_t *port_buffer_2 = (jack_default_audio_sample_t *)jack_port_get_buffer(port2, frames);
    jack_default_audio_sample_t *port_buffer_3 = (jack_default_audio_sample_t *)jack_port_get_buffer(port3, frames);

    Nsf_Emu *emu = (Nsf_Emu *)arg;
    short buffer[frames*4];
	emu->play( frames, buffer );

    for ( int i = 0; i < frames; i ++ )
    {
        port_buffer_0[i] = buffer[i*4+0] / 32767.0;
        port_buffer_1[i] = buffer[i*4+1] / 32767.0;
        port_buffer_2[i] = buffer[i*4+2] / 32767.0;
        port_buffer_3[i] = buffer[i*4+3] / 32767.0;
    }

    return 0;
}

void write_register_callback(nes_addr_t addr, int data)
{
    /*const char *notes[] = {
        "A-", // 0
        "A#", // 1
        "B-", // 2
        "C-", // 3
        "C#", // 4
        "D-", // 5
        "D#", // 6
        "E-", // 7
        "F-", // 8
        "F#", // 9
        "G-", // 10
        "G#", // 11
    };
    static uint8_t last_period_low = 0;	
	
    if ( addr == 0x4004 )
        printf("Duty: %d\tLeng: %d\tCvol: %d\tVolm: %d\n",
            (data & 0xC0) >> 6,
            (data & 0x20) >> 5,
            (data & 0x10) >> 4,
            (data & 0x0F) >> 0
        );
    if ( addr == 0x4005 )
        printf("E: %d\tP: %d\tN: %d\tS: %d\n",
            (data & 0x80) >> 7,
            (data & 0x70) >> 4,
            (data & 0x80) >> 3,
            (data & 0x07) >> 0
        );
    if ( addr == 0x4006 )
    {
        last_period_low = data;
        //printf("%d\tLfrq: %d\n", time, data);
    }
    if ( addr == 0x4007 )
    {
        uint16_t period = ((data & 0x07) << 8) | last_period_low;
        if ( period >= 8 )
        {
            float frequency = 1.789773e6/(16*(period+1));
            float pitch = 12 * log2( frequency / 110.0 );
            int note = pitch + 0.5;
        
            printf("Note: %s%d\n", notes[note % 12], (note+12*2)/12);
        }
        else
            printf("Note: Off\n");
    }
    if ( addr == 0x4015 )
        printf("DMC: %d\tNoise: %d\tTriangle: %d\tPulse 2: %d\tPulse 1: %d\n",
            (data & 0x10) >> 4,
            (data & 0x08) >> 3,
            (data & 0x04) >> 2,
            (data & 0x02) >> 1,
            (data & 0x01) >> 0
        );
    if ( addr == 0x4017 )
        printf("Mode: %d\tIRQ: %d\n",
            (data & 0x80) >> 7,
            (data & 0x40) >> 6
        );*/
}

static bool done = false;

static void sig_handler(int signo)
{
    if ( signo == SIGINT )
        done = true;
}

int main(int argc, char *argv[])
{
    if ( argc != 2 )
    {
        fprintf(stderr, "Missing command line argument\n");
        return -1;
    }
    int track_num = atoi(argv[1]);

    jack_client_t *client;
    jack_status_t status;

    printf("Opening client...\n");
    client = jack_client_open("nsf", JackNoStartServer, &status);
    if ( !client )
    {
        fprintf(stderr, "Could not create client!\n");
        return -1;
    }

    jack_nframes_t sample_rate = jack_get_sample_rate(client);
    printf("Sample rate: %d\n", sample_rate);

    printf("Registering ports...\n");
    port0 = jack_port_register(client, "Square1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    port1 = jack_port_register(client, "Square2", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    port2 = jack_port_register(client, "Triangle", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    port3 = jack_port_register(client, "Noise", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

    if ( port0 == NULL || port1 == NULL || port2 == NULL || port3 == NULL )
    {
        fprintf(stderr, "Port registration failed!\n");
        return -1;
    }

    Nsf_Emu emu;
    Quadraphonic_Buffer buf;
    emu.set_buffer(&buf);
    emu.set_sample_rate( sample_rate );
    emu.ignore_silence();
    emu.load_file("megaman3.nsf");

    printf("Game: %s\n", emu.header().game);
    printf("Author: %s\n", emu.header().author);
    emu.start_track(track_num);

    printf("Setting callback...\n");
    jack_set_process_callback(client, process, &emu);

    printf("Activating...\n");
    if ( jack_activate(client) )
    {
        fprintf(stderr, "Could not activate!\n");
        return -1;
    }

    if ( signal(SIGINT, sig_handler) == SIG_ERR )
        printf("Could set signal handler!\n");

    printf("Making connections...\n");
    jack_connect(client, "nsf:Square1", "system:playback_1");
    jack_connect(client, "nsf:Square2", "system:playback_2");
    jack_connect(client, "nsf:Triangle", "system:playback_3");
    jack_connect(client, "nsf:Noise", "system:playback_4");

    printf("Sleeping...\n");
    sleep(-1);

    printf("Shutting down...\n");
    jack_deactivate(client);
    jack_client_close(client);
	return 0;
}
