
#define USE_OPENAL
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef USE_OPENAL
#include <AL/al.h>
#include <AL/alc.h>
#endif
#include "ibm.h"
#include "sound.h"

FILE *allog;
#ifdef USE_OPENAL
ALuint buffers[4]; // front and back buffers
ALuint buffers_cd[4]; // front and back buffers
ALuint buffers_midi[4]; // front and back buffers
static ALuint source[3];     // audio source
#endif
#define FREQ 48000

int SOUNDBUFLEN = 48000/20;

#define BUFLEN SOUNDBUFLEN

static int midi_freq = 44100;
static int midi_buf_size = 4410;
static int initialized = 0;

void al_set_midi(int freq, int buf_size)
{
        midi_freq = freq;
        midi_buf_size = buf_size;
}

void closeal();
ALvoid  alutInit(ALint *argc,ALbyte **argv)
{
	ALCcontext *Context;
	ALCdevice *Device;
	
	//Open device
// 	Device=alcOpenDevice((ALubyte*)"");
 	Device=alcOpenDevice((void *)"");
	//Create context(s)
	Context=alcCreateContext(Device,NULL);
	//Set active context
	alcMakeContextCurrent(Context);
	//Register extensions
}

ALvoid  alutExit(ALvoid) 
{
	ALCcontext *Context;
	ALCdevice *Device;

	//Unregister extensions

	//Get active context
	Context=alcGetCurrentContext();
	//Get device for active context
	Device=alcGetContextsDevice(Context);
	//Disable context
	alcMakeContextCurrent(NULL);
	//Release context(s)
	alcDestroyContext(Context);
	//Close device
	alcCloseDevice(Device);
}
void initalmain(int argc, char *argv[])
{
#ifdef USE_OPENAL
        alutInit(0,0);
//        printf("AlutInit\n");
        atexit(alutExit);
//        printf("AlutInit\n");
#endif
}

void closeal()
{
        if (!initialized) return;
#ifdef USE_OPENAL
        int i;
        for (i = 0; i < 3; ++i)
        {
                alSourceStop(source[i]);
        }
        alDeleteSources(3, source);

        alDeleteBuffers(4, buffers_midi);
        alDeleteBuffers(4, buffers_cd);
        alDeleteBuffers(4, buffers);

        initialized = 0;
#endif
}

void check()
{
#ifdef USE_OPENAL
        ALenum error;
        if ((error = alGetError()) != AL_NO_ERROR)
        {
//                printf("Error : %08X\n", error);
//                exit(-1);
        }
#endif
}

void inital()
{
        if (initialized) return;
#ifdef USE_OPENAL
        int c;
        int16_t buf[BUFLEN*2];
        int16_t cd_buf[CD_BUFLEN*2];
        void* midi_buf = malloc(midi_buf_size);
        
//        printf("1\n");
        check();

//        printf("2\n");
        alGenBuffers(4, buffers);
        check();
        alGenBuffers(4, buffers_cd);
        check();
        alGenBuffers(4, buffers_midi);
        check();
        
//        printf("3\n");
        alGenSources(3, source);
        check();

//        printf("4\n");
        alSource3f(source[0], AL_POSITION,        0.0, 0.0, 0.0);
        alSource3f(source[0], AL_VELOCITY,        0.0, 0.0, 0.0);
        alSource3f(source[0], AL_DIRECTION,       0.0, 0.0, 0.0);
        alSourcef (source[0], AL_ROLLOFF_FACTOR,  0.0          );
        alSourcei (source[0], AL_SOURCE_RELATIVE, AL_TRUE      );
        check();
        alSource3f(source[1], AL_POSITION,        0.0, 0.0, 0.0);
        alSource3f(source[1], AL_VELOCITY,        0.0, 0.0, 0.0);
        alSource3f(source[1], AL_DIRECTION,       0.0, 0.0, 0.0);
        alSourcef (source[1], AL_ROLLOFF_FACTOR,  0.0          );
        alSourcei (source[1], AL_SOURCE_RELATIVE, AL_TRUE      );
        check();
        alSource3f(source[2], AL_POSITION,        0.0, 0.0, 0.0);
        alSource3f(source[2], AL_VELOCITY,        0.0, 0.0, 0.0);
        alSource3f(source[2], AL_DIRECTION,       0.0, 0.0, 0.0);
        alSourcef (source[2], AL_ROLLOFF_FACTOR,  0.0          );
        alSourcei (source[2], AL_SOURCE_RELATIVE, AL_TRUE      );
        check();

        memset(buf,0,BUFLEN*4);
        memset(cd_buf, 0, CD_BUFLEN*4);
        memset(midi_buf, 0, midi_buf_size);

//        printf("5\n");
        for (c = 0; c < 4; c++)
        {
                alBufferData(buffers[c], AL_FORMAT_STEREO16, buf, BUFLEN*2*2, FREQ);
                alBufferData(buffers_cd[c], AL_FORMAT_STEREO16, cd_buf, CD_BUFLEN*2*2, CD_FREQ);
                alBufferData(buffers_midi[c], AL_FORMAT_STEREO16, midi_buf, midi_buf_size, midi_freq);
        }

        alSourceQueueBuffers(source[0], 4, buffers);
        check();
        alSourceQueueBuffers(source[1], 4, buffers_cd);
        check();
        alSourceQueueBuffers(source[2], 4, buffers_midi);
        check();
//        printf("6 %08X\n",source);
        alSourcePlay(source[0]);
        check();
        alSourcePlay(source[1]);
        check();
        alSourcePlay(source[2]);
        check();

        free(midi_buf);

        initialized = 1;
//        printf("InitAL!!! %08X\n",source);
#endif
}

void givealbuffer(int32_t *buf)
{
        if (!initialized) return;
#ifdef USE_OPENAL
        int16_t buf16[BUFLEN*2];
        int processed;
        int state;
        
        //return;
        
//        printf("Start\n");
        check();
        
//        printf("GiveALBuffer %08X\n",source);
        
        alGetSourcei(source[0], AL_SOURCE_STATE, &state);

        check();
        
        if (state==0x1014)
        {
                alSourcePlay(source[0]);
//                printf("Resetting sound\n");
        }
//        printf("State - %i %08X\n",state,state);
        alGetSourcei(source[0], AL_BUFFERS_PROCESSED, &processed);

//        printf("P ");
        check();
//        printf("Processed - %i\n",processed);

        if (processed>=1)
        {
                int c;
                ALuint buffer;

                alSourceUnqueueBuffers(source[0], 1, &buffer);
//                printf("U ");
                check();

                for (c=0;c<BUFLEN*2;c++)
                {
                        if (buf[c] < -32768)
                                buf16[c] = -32768;
                        else if (buf[c] > 32767)
                                buf16[c] = 32767;
                        else
                                buf16[c] = buf[c];
                }
//                for (c=0;c<BUFLEN*2;c++) buf[c]^=0x8000;
                alBufferData(buffer, AL_FORMAT_STEREO16, buf16, BUFLEN*2*2, FREQ);
//                printf("B ");
               check();

                alSourceQueueBuffers(source[0], 1, &buffer);
//                printf("Q ");
                check();
                
//                printf("\n");

//                if (!allog) allog=fopen("al.pcm","wb");
//                fwrite(buf,BUFLEN*2,1,allog);
        }
//        printf("\n");
#endif
}

void givealbuffer_cd(int16_t *buf)
{
        if (!initialized) return;
#ifdef USE_OPENAL
        int processed;
        int state;
        
        //return;
        
//        printf("Start\n");
        check();
        
//        printf("GiveALBuffer %08X\n",source);
        
        alGetSourcei(source[1], AL_SOURCE_STATE, &state);

        check();
        
        if (state==0x1014)
        {
                alSourcePlay(source[1]);
//                printf("Resetting sound\n");
        }
//        printf("State - %i %08X\n",state,state);
        alGetSourcei(source[1], AL_BUFFERS_PROCESSED, &processed);

//        printf("P ");
        check();
//        printf("Processed - %i\n",processed);

        if (processed>=1)
        {
                ALuint buffer;

                alSourceUnqueueBuffers(source[1], 1, &buffer);
//                printf("U ");
                check();

//                for (c=0;c<BUFLEN*2;c++) buf[c]^=0x8000;
                alBufferData(buffer, AL_FORMAT_STEREO16, buf, CD_BUFLEN*2*2, CD_FREQ);
//                printf("B ");
               check();

                alSourceQueueBuffers(source[1], 1, &buffer);
//                printf("Q ");
                check();
                
//                printf("\n");

//                if (!allog) allog=fopen("al.pcm","wb");
//                fwrite(buf,BUFLEN*2,1,allog);
        }
//        printf("\n");
#endif
}

void givealbuffer_midi(void *buf, uint32_t size)
{
        if (!initialized) return;
#ifdef USE_OPENAL
        int processed;
        int state;

        //return;

//        printf("Start\n");
        check();

//        printf("GiveALBuffer %08X\n",source);

        alGetSourcei(source[2], AL_SOURCE_STATE, &state);

        check();

        if (state==0x1014)
        {
                alSourcePlay(source[2]);
//                printf("Resetting sound\n");
        }
//        printf("State - %i %08X\n",state,state);
        alGetSourcei(source[2], AL_BUFFERS_PROCESSED, &processed);

//        printf("P ");
        check();
//        printf("Processed - %i\n",processed);

        if (processed>=1)
        {
                ALuint buffer;

                alSourceUnqueueBuffers(source[2], 1, &buffer);
//                printf("U ");
                check();

//                for (c=0;c<BUFLEN*2;c++) buf[c]^=0x8000;
                alBufferData(buffer, AL_FORMAT_STEREO16, buf, size, midi_freq);
//                printf("B ");
               check();

                alSourceQueueBuffers(source[2], 1, &buffer);
//                printf("Q ");
                check();

//                printf("\n");

//                if (!allog) allog=fopen("al.pcm","wb");
//                fwrite(buf,BUFLEN*2,1,allog);
        }
//        printf("\n");
#endif
}
