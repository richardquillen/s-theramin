#pragma once
#include <3ds.h>

void audio_init();
int audio_update(int x, int y, float semitoneOffset);
void audio_mute();
void audio_set_key_index(int index);
void audio_set_waveform(int type);
const char* audio_get_waveform_name();
const char* audio_get_key_name();
const char* audio_get_mode_name();
void audio_exit();
