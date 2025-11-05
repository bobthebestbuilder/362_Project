#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include <stdio.h>
#include "keypad.h"
#include "queue.h"

//PWM pin for audio output
#define AUDIO_PIN 15

//Musical note frequencies (in Hz) - C4 to D#5
#define NOTE_C4   262  // 1
#define NOTE_CS4  277  // 2 (C#)
#define NOTE_D4   294  // 3
#define NOTE_DS4  311  // A (D#)
#define NOTE_E4   330  // 4
#define NOTE_F4   349  // 5
#define NOTE_FS4  370  // 6 (F#)
#define NOTE_G4   392  // B
#define NOTE_GS4  415  // 7 (G#)
#define NOTE_A4   440  // 8
#define NOTE_AS4  466  // 9 (A#)
#define NOTE_B4   494  // C
#define NOTE_C5   523  // *
#define NOTE_CS5  554  // 0 (C#)
#define NOTE_D5   587  // #
#define NOTE_DS5  622  // D (D#)

//Attack/decay envelope parameters
#define ENVELOPE_STEPS 10
#define ENVELOPE_DELAY_US 500

uint slice_num;
uint channel;
uint16_t current_top_value = 0;

void set_note_frequency(uint16_t freq) {
    if (freq == 0) {
        pwm_set_gpio_level(AUDIO_PIN, 0);
        current_top_value = 0;
        return;
    }
    
    uint32_t clock_freq = clock_get_hz(clk_sys);
    uint32_t top_value = (clock_freq / freq) - 1;
    
    // if TOP exceeds 16-bit limit, apply divider
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
    
    // 50% duty cycle
    pwm_set_gpio_level(AUDIO_PIN, top_value / 2);
    
    current_top_value = top_value;
}

//attack envelope to eliminate clicking sounds
void apply_attack_envelope() {
    if (current_top_value == 0) return;
    
    uint16_t target_level = current_top_value / 2;
    
    for (int i = 1; i <= ENVELOPE_STEPS; i++) {
        uint16_t level = (target_level * i) / ENVELOPE_STEPS;
        pwm_set_gpio_level(AUDIO_PIN, level);
        sleep_us(ENVELOPE_DELAY_US);
    }
}

//decay envelope to eliminate clicking sounds
void apply_decay_envelope() {
    if (current_top_value == 0) return;
    
    uint16_t start_level = current_top_value / 2;
    
    for (int i = ENVELOPE_STEPS; i >= 0; i--) {
        uint16_t level = (start_level * i) / ENVELOPE_STEPS;
        pwm_set_gpio_level(AUDIO_PIN, level);
        sleep_us(ENVELOPE_DELAY_US);
    }
}

//play with attack envelope
void play_note(uint16_t freq) {
    set_note_frequency(freq);
    if (freq > 0) {
        apply_attack_envelope();
    }
}

//stop with decay envelope
void stop_note() {
    apply_decay_envelope();
    set_note_frequency(0);
}

uint16_t key_to_frequency(char key) {
    switch(key) {
        // Row 1: C4 to D#4
        case '1': return NOTE_C4;
        case '2': return NOTE_CS4;
        case '3': return NOTE_D4;
        case 'A': return NOTE_DS4;
        
        // Row 2: E4 to G4
        case '4': return NOTE_E4;
        case '5': return NOTE_F4;
        case '6': return NOTE_FS4;
        case 'B': return NOTE_G4;
        
        // Row 3: G#4 to B4
        case '7': return NOTE_GS4;
        case '8': return NOTE_A4;
        case '9': return NOTE_AS4;
        case 'C': return NOTE_B4;
        
        // Row 4: C5 to D#5
        case '*': return NOTE_C5;
        case '0': return NOTE_CS5;
        case '#': return NOTE_D5;
        case 'D': return NOTE_DS5;
        
        default: return 0;
    }
}

void init_pwm() {
    gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);
    slice_num = pwm_gpio_to_slice_num(AUDIO_PIN);
    channel = pwm_gpio_to_channel(AUDIO_PIN);
    
    pwm_set_enabled(slice_num, true);
}

int main() {
    stdio_init_all();
    sleep_ms(2000);
    
    printf("Keypad Musical Instrument\n");
    
    keypad_init_pins();
    keypad_init_timer();
    init_pwm();
    key_init();
    
    printf("Press keys to play notes.\n");
    printf("Chromatic scale layout:\n");
    printf("  1(C4)  2(C#)  3(D)   A(D#)\n");
    printf("  4(E)   5(F)   6(F#)  B(G)\n");
    printf("  7(G#)  8(A)   9(A#)  C(B)\n");
    printf("  *(C5)  0(C#)  #(D)   D(D#)\n\n");
    
    while (1) {
        //Check for key press
        if (!key_empty()) {
            uint16_t event = key_pop();
            bool pressed = (event >> 8) & 1;
            char key = (char)(event & 0xFF);
            
            if (pressed) {
                uint16_t freq = key_to_frequency(key);
                play_note(freq);
                printf("Key '%c' pressed - Playing %d Hz (TOP=%d)\n", key, freq, current_top_value);
            } else {
                stop_note();
                printf("Key '%c' released - Silence\n", key);
            }
        }
        
        sleep_ms(10);
    }
    
    return 0;
}