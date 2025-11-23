#include <3ds.h>
#include <math.h>
#include <string.h>
#include "audio.h"

#define SAMPLERATE      22050 //
#define SAMPLESPERBUF   (SAMPLERATE / 30)
#define BYTESPERSAMPLE  4   

static ndspWaveBuf waveBuf[2];
static u32* audioBuffer;

static bool fillBlock = false;
static size_t stream_offset = 0;

static float currentVolume = 0.0f;
static float currentFreq   = 440.0f;

static int currentKeyIndex  = 0; 
static int currentNoteIndex = 0; 
static int currentWaveform  = 0;  

static const char* WAVEFORM_NAMES[4] = {
    "Sine", "Square", "Saw", "Triangle"
};

static const char* KEY_NAMES[24] = {
    "C Major",  "C# Major", "D Major",  "D# Major",
    "E Major",  "F Major",  "F# Major", "G Major",
    "G# Major", "A Major",  "A# Major", "B Major",

    "C Minor",  "C# Minor", "D Minor",  "D# Minor",
    "E Minor",  "F Minor",  "F# Minor", "G Minor",
    "G# Minor", "A Minor",  "A# Minor", "B Minor"
};

static const char* MODE_NAMES[2] = {
    "Ionian (Major)",
    "Aeolian (Minor)"
};

static int ionian[7]  = {0, 2, 4, 5, 7, 9, 11};
static int aeolian[7] = {0, 2, 3, 5, 7, 8, 10};

static float semitone_to_freq(float semitone)
{
    return 440.0f * powf(2.0f, (semitone - 69) / 12.0f);
}

// Waveform generator
static inline float wave_sample(float t)
{
    float phase = fmodf(t * currentFreq, 1.0f);

    switch (currentWaveform)
    {
        case 1: return (phase < 0.5f) ? 1.0f : -1.0f;        // square
        case 2: return 2.0f * phase - 1.0f;                 // saw
        case 3: return 1.0f - fabsf(4.0f * (phase - 0.25f));// triangle
        default: return sinf(2 * M_PI * currentFreq * t);   // sine
    }
}

// buffer is sent to DSP
static void fill_buffer(void *audioBuffer, size_t offset, size_t size)
{
    u32 *dest = (u32*)audioBuffer;

    for (int i = 0; i < size; i++)
    {
        float t = (offset + i) / (float)SAMPLERATE;
        float s = currentVolume * wave_sample(t);
        s16 sample = (s16)(INT16_MAX * s);

        dest[i] = (sample << 16) | (sample & 0xFFFF);
    }

    DSP_FlushDataCache(audioBuffer, size);
}

void audio_init()
{
    audioBuffer = (u32*)linearAlloc(SAMPLESPERBUF * BYTESPERSAMPLE * 2);

    ndspInit();
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);

    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, SAMPLERATE);
    ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);

    float mix[12] = {0};
    mix[0] = 1.0f;
    mix[1] = 1.0f;
    ndspChnSetMix(0, mix);

    memset(waveBuf, 0, sizeof(waveBuf));

    waveBuf[0].data_vaddr = &audioBuffer[0];
    waveBuf[0].nsamples   = SAMPLESPERBUF;

    waveBuf[1].data_vaddr = &audioBuffer[SAMPLESPERBUF];
    waveBuf[1].nsamples   = SAMPLESPERBUF;

    fill_buffer(waveBuf[0].data_pcm16, 0, SAMPLESPERBUF);
    fill_buffer(waveBuf[1].data_pcm16, SAMPLESPERBUF, SAMPLESPERBUF);

    ndspChnWaveBufAdd(0, &waveBuf[0]);
    ndspChnWaveBufAdd(0, &waveBuf[1]);

    stream_offset = SAMPLESPERBUF * 2;
}

void audio_set_key_index(int index)
{
    currentKeyIndex = (index % 24 + 24) % 24;
}

void audio_set_waveform(int type)
{
    currentWaveform = (type % 4 + 4) % 4;
}

int audio_update(int x, int y, float microOffset)
{
    // account for deadzone
    if (fabsf(microOffset) < 0.1f)
        microOffset = 0.0f;

    // adjust volume
    currentVolume = 1.0f - (y / 240.0f);
    if (currentVolume < 0) currentVolume = 0;
    if (currentVolume > 1) currentVolume = 1;

    // determine note from x position
    int slice = x / (320.0f / 12.0f);
    if (slice < 0) slice = 0;
    if (slice > 11) slice = 11;

    int degree = slice;
    int octaveShift = (degree / 7) * 12;
    int scaleIndex  = degree % 7;

    int keyRoot = currentKeyIndex % 12;
    int* intervals = (currentKeyIndex < 12) ? ionian : aeolian;

    float semitone = keyRoot + intervals[scaleIndex] + octaveShift;

    semitone += microOffset;

    // returned note is nearest chromatic note
    currentNoteIndex = ((int)roundf(semitone)) % 12;
    if (currentNoteIndex < 0) currentNoteIndex += 12;

    currentFreq = semitone_to_freq(72 + semitone);

    if (waveBuf[fillBlock].status == NDSP_WBUF_DONE)
    {
        fill_buffer(waveBuf[fillBlock].data_pcm16,
                    stream_offset,
                    waveBuf[fillBlock].nsamples);

        ndspChnWaveBufAdd(0, &waveBuf[fillBlock]);
        stream_offset += waveBuf[fillBlock].nsamples;

        fillBlock = !fillBlock;
    }

    return currentNoteIndex;
}

void audio_mute()
{
    currentVolume = 0.0f;
}

void audio_exit()
{
    ndspExit();
    linearFree(audioBuffer);
}

const char* audio_get_key_name()      { return KEY_NAMES[currentKeyIndex]; }
const char* audio_get_mode_name()     { return (currentKeyIndex < 12) ? MODE_NAMES[0] : MODE_NAMES[1]; }
const char* audio_get_waveform_name() { return WAVEFORM_NAMES[currentWaveform]; }
