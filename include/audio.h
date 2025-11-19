#pragma once
#include <stdint.h>
#include <stdbool.h>

// Define your Speaker Pin (Ensure this is not used by I2C)
// GP16 is a common choice on Pico/RP2350 for PWM
#define SPEAKER_PIN 16 

// Initialize PWM on the speaker pin
void audio_init(void);

// Play a specific frequency (Hz)
void audio_play_note(float freq);

// Stop audio output
void audio_stop(void);

// Map button index (0-15) to a frequency
float get_note_for_index(int idx);