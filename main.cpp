#include <math.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#include <portaudio.h>

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
            printf("index: %d, %d\n", i, &buf[i]);
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

static int patestCallback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData )
{
    Nsf_Emu *emu = (Nsf_Emu *)userData;
	emu->play( framesPerBuffer, (short *)outputBuffer );
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

int main()
{
	const long sample_rate = 48000;

    Nsf_Emu emu;
    Quadraphonic_Buffer buf;
    emu.set_buffer(&buf);
    emu.set_sample_rate( sample_rate );
    emu.ignore_silence();
    emu.load_file("megaman3.nsf");

    printf("Game: %s\n", emu.header().game);
    printf("Author: %s\n", emu.header().author);
    emu.start_track(8);

    PaError err;
    const PaDeviceInfo *deviceInfo;
    int numDevices, device_index;
    err = Pa_Initialize();
    if( err != paNoError ) goto error;

    numDevices = Pa_GetDeviceCount();

    for( device_index = 0; device_index < numDevices; device_index ++ )
    {
        deviceInfo = Pa_GetDeviceInfo(device_index);
        printf("%d: Name = %s\t", device_index, deviceInfo->name);
        printf("Max inputs = %d", deviceInfo->maxInputChannels);
        printf(", Max outputs = %d\n", deviceInfo->maxOutputChannels);
        // "Scarlett 8i6 USB"
        // "MacBook Pro Speakers"
        // "ZOOM U-44 Driver"
        if ( !strcmp(deviceInfo->name, "Scarlett 8i6 USB") ) 
        {
            printf("Matched: %d\n", device_index);
            break;
        }
    }

    PaStream *stream;
    PaStreamParameters outputParameters;
    outputParameters.channelCount = 4;
    outputParameters.device = device_index;
    outputParameters.hostApiSpecificStreamInfo = NULL;
    outputParameters.sampleFormat = paInt16;
    outputParameters.suggestedLatency = 0;
    err = Pa_OpenStream(&stream,
                        NULL,
                        &outputParameters,
                        sample_rate,
                        256,        // frames per buffer
                        0, // stream flags
                        patestCallback, // this is your callback function
                        &emu ); // This is a pointer that will be passed to your callback
    if( err != paNoError ) goto error;

    err = Pa_StartStream( stream );
    if( err != paNoError ) goto error;

    if ( signal(SIGINT, sig_handler) == SIG_ERR )
        printf("Could set signal handler!\n");

    while ( !done )
        Pa_Sleep(1000);

    printf("Shutting down...\n");

    err = Pa_StopStream( stream );
    if( err != paNoError ) goto error;

    err = Pa_CloseStream( stream );
    if( err != paNoError ) goto error;

    err = Pa_Terminate();
    if ( err != paNoError ) goto error;	
	return 0;
error:
    printf(  "PortAudio error: %s\n", Pa_GetErrorText( err ) );
    return -1;
}
