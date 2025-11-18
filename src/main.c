#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "seesaw.h"
#include "neotrellis.h"

//============================================================================
// PWM Audio Configuration
//============================================================================

#define AUDIO_PIN 15

// Musical note frequencies (in Hz) - C4 to D#5 chromatic scale
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

// Envelope parameters
#define ENVELOPE_STEPS 10
#define ENVELOPE_DELAY_MS 0.5

// PWM state tracking
uint slice_num;
uint channel;
uint16_t current_top_value = 0;
uint16_t current_frequency = 0;
bool note_is_playing = false;
int currently_playing_button = -1;

//============================================================================
// PWM Audio Functions
//============================================================================

void set_note_frequency(uint16_t freq) {
    if (freq == 0) {
        pwm_set_gpio_level(AUDIO_PIN, 0);
        current_top_value = 0;
        current_frequency = 0;
        note_is_playing = false;
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

void apply_attack_envelope() {
if (current_top_value == 0) return;
    
    uint16_t target_level = current_top_value / 2;
    
    for (int i = 1; i <= ENVELOPE_STEPS; i++) {
        uint16_t level = (target_level * i) / ENVELOPE_STEPS;
        pwm_set_gpio_level(AUDIO_PIN, level);
        sleep_ms(ENVELOPE_DELAY_MS);
    }
}

void apply_decay_envelope() {
    if (current_top_value == 0) return;
    
    uint16_t start_level = current_top_value / 2;
    
    for (int i = ENVELOPE_STEPS; i >= 0; i--) {
        uint16_t level = (start_level * i) / ENVELOPE_STEPS;
        pwm_set_gpio_level(AUDIO_PIN, level);
        sleep_ms(ENVELOPE_DELAY_MS);
    }
}

void play_note(uint16_t freq) {
    set_note_frequency(freq);
    if (freq > 0) {
        apply_attack_envelope();
    }
}

void stop_note() {
    apply_decay_envelope();
    set_note_frequency(0);
}

uint16_t button_to_frequency(int idx) {
    const uint16_t frequencies[16] = {
        NOTE_C4, NOTE_CS4, NOTE_D4, NOTE_DS4,
        NOTE_E4, NOTE_F4, NOTE_FS4, NOTE_G4,
        NOTE_GS4, NOTE_A4, NOTE_AS4, NOTE_B4,
        NOTE_C5, NOTE_CS5, NOTE_D5, NOTE_DS5
    };
    
    if (idx >= 0 && idx < 16) {
        return frequencies[idx];
    }
    return 0;
}

const char* button_to_note_name(int idx) {
    const char* note_names[16] = {
        "C4", "C#4", "D4", "D#4",
        "E4", "F4", "F#4", "G4",
        "G#4", "A4", "A#4", "B4",
        "C5", "C#5", "D5", "D#5"
    };
    
    if (idx >= 0 && idx < 16) {
        return note_names[idx];
    }
    return "???";
}

void init_pwm() {
    gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);
    slice_num = pwm_gpio_to_slice_num(AUDIO_PIN);
    channel = pwm_gpio_to_channel(AUDIO_PIN);
    
    pwm_set_enabled(slice_num, true);
}

//============================================================================
// I2C Scanning Helper
//============================================================================

static void scan_i2c(void) {
    printf("I2C scan:\n");
    for (uint8_t a = 0x08; a <= 0x77; a++) {
        uint8_t dummy = 0;
        int r = i2c_write_blocking(NEOTRELLIS_I2C, a, &dummy, 1, false);
        if (r >= 0) printf("  Found 0x%02X\n", a);
    }
}

//============================================================================
// Main Program
//============================================================================

int main() {
    stdio_init_all();
    setvbuf(stdout, NULL, _IONBF, 0);
    sleep_ms(500);
    
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("    🎹 NeoTrellis Musical Keypad 🎹\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    // Initialize PWM for audio
    printf("Initializing PWM audio...\n");
    init_pwm();
    printf("   PWM configured on GPIO %d\n", AUDIO_PIN);
    printf("   Formula: f_note = f_clk / (TOP + 1)\n");
    printf("   Duty cycle: 50%% (minimal distortion)\n");
    printf("   Envelope: %d steps × %dms = %dms\n\n", 
           ENVELOPE_STEPS, ENVELOPE_DELAY_MS, ENVELOPE_STEPS * ENVELOPE_DELAY_MS);
    
    // Initialize I2C bus
    printf("🔌 Initializing I2C bus...\n");
    seesaw_bus_init(100000);  // 100 kHz
    printf("   I2C initialized at 100 kHz\n");
    printf("   SDA: GPIO %d\n", NEOTRELLIS_SDA);
    printf("   SCL: GPIO %d\n\n", NEOTRELLIS_SCL);
    
    // Scan for I2C devices
    scan_i2c();
    printf("\n");
    
    // Reset NeoTrellis
    printf("Resetting NeoTrellis...\n");
    if (!neotrellis_reset()) {
        printf("Failed to reset NeoTrellis!\n");
        printf("   Check wiring and I2C address (0x%02X)\n", NEOTRELLIS_ADDR);
        while (1) tight_loop_contents();
    }
    printf("   Reset successful\n\n");
    
    sleep_ms(1000);
    
    // Wait for device ready
    printf("Waiting for device ready...\n");
    if (!neotrellis_wait_ready(500)) {
        printf("Device not ready (HW_ID timeout)\n");
        printf("   Expected HW_ID: 0x55\n");
        while (1) tight_loop_contents();
    }
    printf("   Device ready (HW_ID: 0x55)\n\n");
    
    // Initialize NeoPixels
    printf("Initializing NeoPixels...\n");
    if (!neopixel_begin(3)) {
        printf("neopixel_begin() failed\n");
        printf("   Check wiring and address.\n");
        while (1) tight_loop_contents();
    }
    printf("   NeoPixel init OK\n\n");
    
    // Set NeoPixel speed
    uint8_t speed = 0x01;
    seesaw_write(NEOTRELLIS_ADDR, SEESAW_NEOPIXEL_BASE, NEOPIXEL_SPEED, &speed, 1);
    sleep_ms(300);
    
    // Rainbow startup animation
    printf("Playing rainbow startup animation...\n");
    neotrellis_rainbow_startup();
    sleep_ms(200);
    printf("   Animation complete\n\n");
    
    // Initialize keypad scanning
    printf("Initializing keypad...\n");
    neotrellis_keypad_init();
    sleep_ms(200);
    printf("   Keypad init OK\n\n");
    
    // Clear any stale events
    neotrellis_clear_fifo();
    
    // Display key layout
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("Chromatic Scale Layout (4×4 grid):\n\n");
    printf("  ┌─────┬──────┬─────┬──────┐\n");
    printf("  │  0  │  1   │  2  │  3   │\n");
    printf("  │ C4  │ C#4  │ D4  │ D#4  │\n");
    printf("  ├─────┼──────┼─────┼──────┤\n");
    printf("  │  4  │  5   │  6  │  7   │\n");
    printf("  │ E4  │  F4  │ F#4 │  G4  │\n");
    printf("  ├─────┼──────┼─────┼──────┤\n");
    printf("  │  8  │  9   │ 10  │  11  │\n");
    printf("  │ G#4 │  A4  │ A#4 │  B4  │\n");
    printf("  ├─────┼──────┼─────┼──────┤\n");
    printf("  │ 12  │  13  │ 14  │  15  │\n");
    printf("  │ C5  │ C#5  │  D5 │ D#5  │\n");
    printf("  └─────┴──────┴─────┴──────┘\n\n");
    printf("Press keys to play notes! Each key lights up with a unique color.\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    // Main event loop
    printf("Starting main loop...\n\n");
    while (1) {
        ButtonEvent evt = neotrellis_poll_buttons_audio();
        
        if (evt.valid) {
            uint16_t freq = button_to_frequency(evt.button_idx);
            const char* note = button_to_note_name(evt.button_idx);
            
            if (evt.is_pressed) {
                // Button pressed - play note
                printf("Button %2d PRESSED  → Playing %s (%d Hz) | TOP=%d\n", 
                       evt.button_idx, note, freq, 
                       (clock_get_hz(clk_sys) / freq) - 1);
                
                play_note(freq);
                currently_playing_button = evt.button_idx;
            } else {
                // Button released - stop note
                printf("   Button %2d RELEASED → Stopping %s\n", 
                       evt.button_idx, note);
                
                if (currently_playing_button == evt.button_idx) {
                    stop_note();
                    currently_playing_button = -1;
                }
            }
        }
        
        sleep_ms(10);  // Poll at ~100Hz
    }
    
    return 0;
}