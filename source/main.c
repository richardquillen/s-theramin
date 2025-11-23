/*
 * Author: R.J. Quillen
 * Date: November 23, 2025
 * Project: S-Theramin, basic synth for 3DS
 */

#include <3ds.h>
#include <stdio.h>
#include "audio.h"
#include <math.h>

const char* NOTE_NAMES[12] = {
    "C", "C#", "D", "D#",
    "E", "F", "F#", "G",
    "G#", "A", "A#", "B"
};

int main()
{
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    audio_init();

    printf("\x1b[0;0HTouch = play note");
    printf("\x1b[1;0HL/R = waveform");
    printf("\x1b[2;0HA/B = change key");
    printf("\x1b[3;0HCircle Pad = microtone bend");

    bool lastTouching = false;
    touchPosition touch;

    int keyIndex = 0;
    int waveform = 0;

    audio_set_key_index(keyIndex);
    audio_set_waveform(waveform);

    int currentNote = -1;

    while (aptMainLoop())
    {
        hidScanInput();

        u32 kDown = hidKeysDown();
        u32 held  = hidKeysHeld();

        if (kDown & KEY_START) break;

        // Control Key
        if (kDown & KEY_A) audio_set_key_index(--keyIndex);
        if (kDown & KEY_B) audio_set_key_index(++keyIndex);

        // // Control wave form (sine, square, saw, triangle)
        if (kDown & KEY_L) audio_set_waveform(--waveform);
        if (kDown & KEY_R) audio_set_waveform(++waveform);

        // control pitch w/ analog
        circlePosition cpad;
        hidCircleRead(&cpad);
        float microOffset = (cpad.dx / 160.0f); // roughly ±1.0

        // account for offset (my stick has drift)
        if (fabsf(microOffset) < 0.1f)
            microOffset = 0.0f;

        printf("\x1b[4;0HKey:      %-12s", audio_get_key_name());
        printf("\x1b[5;0HMode:     %-16s", audio_get_mode_name());
        printf("\x1b[6;0HWaveform: %-10s", audio_get_waveform_name());
        printf("\x1b[7;0HBend:     %+0.2f st   ", microOffset);

        bool touching = held & KEY_TOUCH;
        hidTouchRead(&touch);

        if (touching)
        {
            currentNote = audio_update(touch.px, touch.py, microOffset);
            printf("\x1b[8;0HNote: %-4s", NOTE_NAMES[currentNote]);
        }
        else if (lastTouching)
        {
            // turn off audio when stylus lifted
            microOffset = 0.0f;
            audio_mute(); // i originally tried to disable audio, but muting works better
            printf("\x1b[8;0HNote: --- ");
        }

        lastTouching = touching;

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    audio_exit();
    gfxExit();
    return 0;
}
