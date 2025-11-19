#include "audio.h"
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include <stdio.h>

// Chromatic scale starting at C4 (Middle C)
// Increasing index = increasing pitch (Left->Right, Top->Bottom)
static const float note_freqs[16] = {
    261.63, 277.18, 293.66, 311.13, // Row 0: C4 to D#4
    329.63, 349.23, 369.99, 392.00, // Row 1: E4 to G4
    415.30, 440.00, 466.16, 493.88, // Row 2: G#4 to B4
    523.25, 554.37, 587.33, 622.25  // Row 3: C5 to D#5
};

static uint slice_num;

void audio_init(void) {
    // Assign GPIO to PWM
    gpio_set_function(SPEAKER_PIN, GPIO_FUNC_PWM);
    
    // Get PWM slice number for this pin
    slice_num = pwm_gpio_to_slice_num(SPEAKER_PIN);

    // Get default config
    pwm_config config = pwm_get_default_config();
    
    // Set divider to 1.0 (run at full system clock speed)
    pwm_config_set_clkdiv(&config, 1.0f);
    
    // Initialize and start
    pwm_init(slice_num, &config, true);
    
    // Start silent (0% duty)
    pwm_set_gpio_level(SPEAKER_PIN, 0);
    
    printf("[AUDIO] Init complete on Pin %d (Slice %d)\n", SPEAKER_PIN, slice_num);
}

void audio_play_note(float freq) {
    if (freq < 10.0f) {
        audio_stop();
        return;
    }

    // Formula: f_note = f_clk / (TOP + 1)
    // Therefore: TOP = (f_clk / f_note) - 1
    uint32_t clock_hz = clock_get_hz(clk_sys);
    uint32_t top = (uint32_t)(clock_hz / freq) - 1;

    // Set the wrap (TOP) value
    pwm_set_wrap(slice_num, top);

    // Set 50% duty cycle for max volume/min distortion
    // Duty = (TOP + 1) / 2
    pwm_set_gpio_level(SPEAKER_PIN, (top + 1) / 2);

    // DEBUG PRINT for disconnected speaker
    printf(">>> [AUDIO PLAY] Freq: %.2f Hz | PWM TOP: %lu\n", freq, top);
}

void audio_stop(void) {
    // 0% duty cycle = silence
    pwm_set_gpio_level(SPEAKER_PIN, 0);
    
    // DEBUG PRINT
    printf("||| [AUDIO STOP] Silence\n");
}

float get_note_for_index(int idx) {
    if (idx < 0 || idx > 15) return 0.0f;
    return note_freqs[idx];
}