#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <audsrv.h>
#include <math.h>
#include <stdlib.h>
#include "audio.h"

#define MAX_AUDIO_SAMPLES 4800

static int audio_ready = 0;
// Buffer estatico alinhado para hardware real do PS2 (evita fragmentacao de heap)
static short pcm_buffer[MAX_AUDIO_SAMPLES] __attribute__((aligned(64)));

void audio_init(void) {
    SifLoadModule("rom0:LIBSD", 0, NULL);
    if (SifLoadModule("rom0:AUDSRV", 0, NULL) < 0) {
        audio_ready = 0;
        return;
    }
    if (audsrv_init() == 0) {
        struct audsrv_fmt_t format;
        format.bits = 16;
        format.freq = 22050;
        format.channels = 1;
        audsrv_set_format(&format);
        audsrv_set_volume(MAX_VOLUME);
        audio_ready = 1;
    }
}

static void play_synth_tone(float freq_start, float freq_end, int samples, int noise_mix) {
    if (!audio_ready) return;
    if (samples > MAX_AUDIO_SAMPLES) samples = MAX_AUDIO_SAMPLES;

    float phase = 0.0f;
    for (int i = 0; i < samples; i++) {
        float t = (float)i / (float)samples;
        float freq = freq_start + (freq_end - freq_start) * t;
        phase += (2.0f * 3.14159265f * freq) / 22050.0f;
        float env = (1.0f - t);
        float wave = sinf(phase) * 18000.0f;
        if (noise_mix) {
            float n = ((rand() % 32768) - 16384) * 0.9f;
            wave = wave * 0.4f + n * 0.6f;
        }
        pcm_buffer[i] = (short)(wave * env);
    }
    audsrv_play_audio((char *)pcm_buffer, samples * sizeof(short));
}

void audio_play_slap(int anger_level) {
    float base = 260.0f + anger_level * 35.0f;
    play_synth_tone(base, 90.0f, 1800, 1);
}

void audio_play_ding(int correct) {
    if (correct) play_synth_tone(520.0f, 880.0f, 3200, 0);
    else play_synth_tone(180.0f, 95.0f, 4500, 1);
}

void audio_play_soda(void) {
    play_synth_tone(900.0f, 300.0f, 4000, 1);
}

void audio_play_jump(void) {
    play_synth_tone(340.0f, 620.0f, 1600, 0);
}

void audio_play_detention(void) {
    play_synth_tone(440.0f, 220.0f, 4800, 0);
}
