#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include <stdio.h>
#include "keypad.h"
#include "queue.h"

// ============================ PIN DEFINITIONS ============================
#define AUDIO_PIN 15
#define LED_R 37
#define LED_G 38
#define LED_B 39

// ========================== NOTE DEFINITIONS =============================
#define NOTE_C4   262
#define NOTE_CS4  277
#define NOTE_D4   294
#define NOTE_DS4  311
#define NOTE_E4   330
#define NOTE_F4   349
#define NOTE_FS4  370
#define NOTE_G4   392
#define NOTE_GS4  415
#define NOTE_A4   440
#define NOTE_AS4  466
#define NOTE_B4   494
#define NOTE_C5   523
#define NOTE_CS5  554
#define NOTE_D5   587
#define NOTE_DS5  622

// ========================== ENVELOPE CONSTANTS ===========================
#define ENVELOPE_STEPS 20
#define ENVELOPE_DELAY_MS 50
#define STATUS_UPDATE_MS 500

// ========================== GLOBAL VARIABLES =============================
uint slice_num;
uint channel;
uint16_t current_top_value = 0;
uint16_t current_frequency = 0;
bool note_is_playing = false;

// RGB PWM slices
uint slice_r, slice_g, slice_b;

// Brightness scaling (2% max)
#define BRIGHTNESS_SCALE 0.1f
#define MAX_PWM 255

// ========================== RGB LED FUNCTIONS ============================

void init_rgb_led() {
    gpio_set_function(LED_R, GPIO_FUNC_PWM);
    gpio_set_function(LED_G, GPIO_FUNC_PWM);
    gpio_set_function(LED_B, GPIO_FUNC_PWM);

    slice_r = pwm_gpio_to_slice_num(LED_R);
    slice_g = pwm_gpio_to_slice_num(LED_G);
    slice_b = pwm_gpio_to_slice_num(LED_B);

    pwm_set_wrap(slice_r, MAX_PWM);
    pwm_set_wrap(slice_g, MAX_PWM);
    pwm_set_wrap(slice_b, MAX_PWM);

    // Invert polarity for common-anode LED (0 = fully ON)
    pwm_set_output_polarity(slice_r, true, true);
    pwm_set_output_polarity(slice_g, true, true);
    pwm_set_output_polarity(slice_b, true, true);

    pwm_set_enabled(slice_r, true);
    pwm_set_enabled(slice_g, true);
    pwm_set_enabled(slice_b, true);

    // Start LED OFF
    pwm_set_gpio_level(LED_R, MAX_PWM);
    pwm_set_gpio_level(LED_G, MAX_PWM);
    pwm_set_gpio_level(LED_B, MAX_PWM);
}

void set_rgb_color(uint8_t r, uint8_t g, uint8_t b) {
    // Apply brightness scaling and invert for common-anode
    uint16_t r_val = (uint16_t)((1.0f - r / 255.0f) * BRIGHTNESS_SCALE * MAX_PWM);
    uint16_t g_val = (uint16_t)((1.0f - g / 255.0f) * BRIGHTNESS_SCALE * MAX_PWM);
    uint16_t b_val = (uint16_t)((1.0f - b / 255.0f) * BRIGHTNESS_SCALE * MAX_PWM);

    pwm_set_gpio_level(LED_R, r_val);
    pwm_set_gpio_level(LED_G, g_val);
    pwm_set_gpio_level(LED_B, b_val);
}

// map note frequency to RGB base color
void set_note_color(uint16_t freq, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (freq < 330)       { *r = 255; *g = 0;   *b = 0;   } // low → red
    else if (freq < 440)  { *r = 0;   *g = 255; *b = 0;   } // mid → green
    else                  { *r = 0;   *g = 0;   *b = 255; } // high → blue
}

// ========================== AUDIO PWM FUNCTIONS ==========================

void set_note_frequency(uint16_t freq) {
    if (freq == 0) {
        pwm_set_gpio_level(AUDIO_PIN, 0);
        current_top_value = 0;
        current_frequency = 0;
        note_is_playing = false;
        set_rgb_color(0, 0, 0);
        return;
    }

    uint32_t clock_freq = clock_get_hz(clk_sys);
    uint32_t top_value = (clock_freq / freq) - 1;

    if (top_value > 65535) {
        uint32_t divider = 2;
        while (top_value > 65535 && divider < 256) {
            top_value = (clock_freq / (freq * divider)) - 1;
            divider++;
        }
        pwm_set_clkdiv(slice_num, (float)divider);
    } else {
        pwm_set_clkdiv(slice_num, 1.0f);
    }

    pwm_set_wrap(slice_num, top_value);
    pwm_set_gpio_level(AUDIO_PIN, top_value / 2);

    current_top_value = top_value;
    current_frequency = freq;
    note_is_playing = (freq > 0);
}

// ========================== ENVELOPE FUNCTIONS ===========================

void apply_attack_envelope(uint8_t base_r, uint8_t base_g, uint8_t base_b) {
    if (current_top_value == 0) return;
    uint16_t target_level = current_top_value / 2;

    for (int i = 1; i <= ENVELOPE_STEPS; i++) {
        uint16_t level = (target_level * i) / ENVELOPE_STEPS;
        pwm_set_gpio_level(AUDIO_PIN, level);

        // smooth color fade in
        uint8_t r = (base_r * i) / ENVELOPE_STEPS;
        uint8_t g = (base_g * i) / ENVELOPE_STEPS;
        uint8_t b = (base_b * i) / ENVELOPE_STEPS;
        set_rgb_color(r, g, b);

        sleep_ms(ENVELOPE_DELAY_MS);
    }
}

void apply_decay_envelope(uint8_t base_r, uint8_t base_g, uint8_t base_b) {
    if (current_top_value == 0) return;
    uint16_t start_level = current_top_value / 2;

    for (int i = ENVELOPE_STEPS; i >= 0; i--) {
        uint16_t level = (start_level * i) / ENVELOPE_STEPS;
        pwm_set_gpio_level(AUDIO_PIN, level);

        // smooth color fade out
        uint8_t r = (base_r * i) / ENVELOPE_STEPS;
        uint8_t g = (base_g * i) / ENVELOPE_STEPS;
        uint8_t b = (base_b * i) / ENVELOPE_STEPS;
        set_rgb_color(r, g, b);

        sleep_ms(ENVELOPE_DELAY_MS);
    }
}

// ========================== NOTE PLAY/STOP ===============================

void play_note(uint16_t freq) {
    uint8_t r, g, b;
    set_note_color(freq, &r, &g, &b);
    set_note_frequency(freq);
    if (freq > 0) apply_attack_envelope(r, g, b);
}

void stop_note() {
    uint8_t r, g, b;
    set_note_color(current_frequency, &r, &g, &b);
    apply_decay_envelope(r, g, b);
    set_note_frequency(0);
}

// ========================== KEYPAD MAPPING ===============================

uint16_t key_to_frequency(char key) {
    switch (key) {
        case '1': return NOTE_C4;
        case '2': return NOTE_CS4;
        case '3': return NOTE_D4;
        case 'A': return NOTE_DS4;
        case '4': return NOTE_E4;
        case '5': return NOTE_F4;
        case '6': return NOTE_FS4;
        case 'B': return NOTE_G4;
        case '7': return NOTE_GS4;
        case '8': return NOTE_A4;
        case '9': return NOTE_AS4;
        case 'C': return NOTE_B4;
        case '*': return NOTE_C5;
        case '0': return NOTE_CS5;
        case '#': return NOTE_D5;
        case 'D': return NOTE_DS5;
        default:  return 0;
    }
}

// ========================== INITIALIZATION ===============================

void init_pwm() {
    gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);
    slice_num = pwm_gpio_to_slice_num(AUDIO_PIN);
    channel = pwm_gpio_to_channel(AUDIO_PIN);
    pwm_set_enabled(slice_num, true);
}

// ========================== MAIN LOOP ====================================

int main() {
    stdio_init_all();
    sleep_ms(2000);

    printf("\n═══════════════════════════════════════════════════════\n");
    printf("     Keypad Musical Instrument - PWM + RGB Envelope\n");
    printf("═══════════════════════════════════════════════════════\n\n");

    keypad_init_pins();
    keypad_init_timer();
    key_init();
    init_pwm();
    init_rgb_led();

    printf("Chromatic scale layout:\n");
    printf("  1(C4)  2(C#)  3(D)   A(D#)\n");
    printf("  4(E)   5(F)   6(F#)  B(G)\n");
    printf("  7(G#)  8(A)   9(A#)  C(B)\n");
    printf("  *(C5)  0(C#)  #(D)   D(D#)\n\n");
    printf("Press and HOLD keys to see color + envelope fade!\n");
    printf("═══════════════════════════════════════════════════════\n\n");

    uint32_t last_status_time = 0;

    while (1) {
        uint32_t current_time = to_ms_since_boot(get_absolute_time());

        if (!key_empty()) {
            uint16_t event = key_pop();
            bool pressed = (event >> 8) & 1;
            char key = (char)(event & 0xFF);

            if (pressed) {
                uint16_t freq = key_to_frequency(key);
                printf("Key '%c' PRESSED - %d Hz\n", key, freq);
                play_note(freq);
                last_status_time = current_time;
            } else {
                printf("Key '%c' RELEASED\n", key);
                stop_note();
            }
        }

        if (note_is_playing && (current_time - last_status_time >= STATUS_UPDATE_MS)) {
            printf("  ♪ Still playing: %d Hz (TOP=%d)\n",
                   current_frequency, current_top_value);
            last_status_time = current_time;
        }

        sleep_ms(10);
    }

    return 0;
}
