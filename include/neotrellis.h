#pragma once
#include <stdint.h>
#include <stdbool.h>

#define NEOTRELLIS_LED_COUNT   16
#define NEOTRELLIS_BYTES       (NEOTRELLIS_LED_COUNT * 3)

// === Seesaw module IDs (from Adafruit Seesaw) ===
#define SEESAW_STATUS_BASE     0x00
#define SEESAW_NEOPIXEL_BASE   0x0E
#define SEESAW_KEYPAD_BASE     0x10

// Status registers
#define SEESAW_STATUS_HW_ID    0x01
#define SEESAW_STATUS_VERSION  0x02
#define SEESAW_STATUS_SWRST    0x7F

// NeoPixel registers
#define NEOPIXEL_PIN           0x01
#define NEOPIXEL_SPEED         0x02
#define NEOPIXEL_BUF_LENGTH    0x03
#define NEOPIXEL_BUF           0x04
#define NEOPIXEL_SHOW          0x05

// Keypad registers
#define KEYPAD_ENABLE          0x01
#define KEYPAD_INTEN           0x02
#define SEESAW_KEYPAD_INTENSET 0x02
#define KEYPAD_COUNT           0x04
#define KEYPAD_FIFO            0x10

// Keypad edge types
#define SEESAW_KEYPAD_EDGE_HIGH     0
#define SEESAW_KEYPAD_EDGE_LOW      1
#define SEESAW_KEYPAD_EDGE_FALLING  2
#define SEESAW_KEYPAD_EDGE_RISING   3

// Key lookup table (extern - defined in .c file)
extern const uint8_t neotrellis_key_lut[16];

// Function declarations
bool neotrellis_reset(void);
bool neotrellis_status(uint8_t *hw_id, uint32_t *version);
bool neopixel_begin(uint8_t internal_pin);
bool neopixel_show(void);
bool neotrellis_wait_ready(uint32_t timeout_ms);
bool neopixel_set_one_and_show(int index, uint8_t r, uint8_t g, uint8_t b);
bool neopixel_fill_all_and_show(uint8_t r, uint8_t g, uint8_t b);
void neotrellis_rainbow_startup(void);
bool neotrellis_keypad_init(void);
void set_led_for_idx(int idx, bool on);
void neotrellis_clear_fifo(void);
bool neotrellis_poll_buttons(int *idx_out);

// Event structure for audio integration (ADDED FOR AUDIO)
typedef struct {
    int button_idx;      // 0-15 button index
    bool is_pressed;     // true = pressed, false = released
    bool valid;          // true if event is valid
} ButtonEvent;

// Enhanced polling that returns both press and release events (ADDED FOR AUDIO)
ButtonEvent neotrellis_poll_buttons_audio(void);

#ifndef DEBUG_KEYS
#define DEBUG_KEYS 1
#endif
#if DEBUG_KEYS
  #define DKEY(fmt, ...) printf("[KEY] " fmt "\n", ##__VA_ARGS__)
#else
  #define DKEY(...)      ((void)0)
#endif