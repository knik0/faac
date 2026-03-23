/*
 * FAAC - Freeware Advanced Audio Coder
 * Copyright (C) 2026 Nils Schimmelmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <faac.h>
#include <assert.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void test_api_config(unsigned long samplerate, unsigned int channels, int tns, int pns, int joint) {
    faacEncHandle h;
    unsigned long inputSamples, maxOutputBytes;
    faacEncConfigurationPtr config;

    printf("Testing API config: %lu Hz, %u ch, TNS:%d, PNS:%d, Joint:%d\n", samplerate, channels, tns, pns, joint);
    h = faacEncOpen(samplerate, channels, &inputSamples, &maxOutputBytes);
    assert(h != NULL);

    config = faacEncGetCurrentConfiguration(h);
    assert(config != NULL);

    /* Test configuration matrix for varied encoder parameters */
    config->bitRate = 128000 / channels;
    config->inputFormat = FAAC_INPUT_FLOAT;
    config->outputFormat = 1; /* ADTS */
    config->useTns = tns;
    config->pnslevel = pns;
    config->jointmode = joint;

    int ret = faacEncSetConfiguration(h, config);
    assert(ret == 1);

    faacEncClose(h);
}

void test_encode_max_signal(unsigned long samplerate, unsigned int channels) {
    faacEncHandle h;
    unsigned long inputSamples, maxOutputBytes;
    faacEncConfigurationPtr config;

    printf("Testing encode max signal: %lu Hz, %u ch\n", samplerate, channels);
    h = faacEncOpen(samplerate, channels, &inputSamples, &maxOutputBytes);
    assert(h != NULL);

    config = faacEncGetCurrentConfiguration(h);
    config->bitRate = 128000 / channels;
    config->inputFormat = FAAC_INPUT_FLOAT;
    faacEncSetConfiguration(h, config);

    float *input_f = malloc(inputSamples * sizeof(float));
    int32_t *input_i = (int32_t *)input_f;
    unsigned char *output = malloc(maxOutputBytes);

    /* 0 dBFS sine sweep (full dynamic range) */
    for (unsigned long i = 0; i < inputSamples; i++) {
        input_f[i] = (float)sin(2.0 * M_PI * 1000.0 * i / samplerate);
    }

    for (int i = 0; i < 10; i++) {
        int bytes = faacEncEncode(h, input_i, inputSamples, output, maxOutputBytes);
        assert(bytes >= 0);
    }

    /* Verify DC stability at full scale */
    for (unsigned long i = 0; i < inputSamples; i++) {
        input_f[i] = 1.0f;
    }

    for (int i = 0; i < 5; i++) {
        int bytes = faacEncEncode(h, input_i, inputSamples, output, maxOutputBytes);
        assert(bytes >= 0);
    }

    free(input_f);
    free(output);
    faacEncClose(h);
}

void test_encode_silence(unsigned long samplerate, unsigned int channels) {
    faacEncHandle h;
    unsigned long inputSamples, maxOutputBytes;
    faacEncConfigurationPtr config;

    printf("Testing encode silence: %lu Hz, %u ch\n", samplerate, channels);
    h = faacEncOpen(samplerate, channels, &inputSamples, &maxOutputBytes);
    assert(h != NULL);

    config = faacEncGetCurrentConfiguration(h);
    config->bitRate = 64000;
    config->inputFormat = FAAC_INPUT_FLOAT;
    faacEncSetConfiguration(h, config);

    float *input_f = calloc(inputSamples, sizeof(float));
    int32_t *input_i = (int32_t *)input_f;
    unsigned char *output = malloc(maxOutputBytes);

    for (int i = 0; i < 10; i++) {
        int bytes = faacEncEncode(h, input_i, inputSamples, output, maxOutputBytes);
        assert(bytes >= 0);
    }

    /* Verify encoder flush behavior */
    int bytes;
    do {
        bytes = faacEncEncode(h, NULL, 0, output, maxOutputBytes);
        assert(bytes >= 0);
    } while (bytes > 0);

    free(input_f);
    free(output);
    faacEncClose(h);
}

void test_encode_noise(unsigned long samplerate, unsigned int channels) {
    faacEncHandle h;
    unsigned long inputSamples, maxOutputBytes;
    faacEncConfigurationPtr config;

    printf("Testing encode noise: %lu Hz, %u ch\n", samplerate, channels);
    h = faacEncOpen(samplerate, channels, &inputSamples, &maxOutputBytes);
    assert(h != NULL);

    config = faacEncGetCurrentConfiguration(h);
    config->bitRate = 128000 / channels;
    config->inputFormat = FAAC_INPUT_FLOAT;
    faacEncSetConfiguration(h, config);

    float *input_f = malloc(inputSamples * sizeof(float));
    int32_t *input_i = (int32_t *)input_f;
    unsigned char *output = malloc(maxOutputBytes);

    /* Perceptual model stress test with random signals */
    for (unsigned long i = 0; i < inputSamples; i++) {
        input_f[i] = (float)((rand() % 20000) - 10000) / 10000.0f;
    }

    for (int i = 0; i < 5; i++) {
        int bytes = faacEncEncode(h, input_i, inputSamples, output, maxOutputBytes);
        assert(bytes >= 0);
    }

    free(input_f);
    free(output);
    faacEncClose(h);
}

int main() {
    unsigned long rates[] = {8000, 16000, 44100, 48000};
    unsigned int channels[] = {1, 2, 6};

    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 3; c++) {
            test_api_config(rates[r], channels[c], 1, 4, 1);
            test_api_config(rates[r], channels[c], 0, 0, 0);
            test_api_config(rates[r], channels[c], 1, 10, 2);

            test_encode_silence(rates[r], channels[c]);
            test_encode_noise(rates[r], channels[c]);
            test_encode_max_signal(rates[r], channels[c]);
        }
    }

    printf("All API tests passed!\n");
    return 0;
}
