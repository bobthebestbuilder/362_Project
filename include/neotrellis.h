

    #pragma once
#include <stdint.h>
#include <stdbool.h>

#define NEOTRELLIS_LED_COUNT   16
#define NEOTRELLIS_BYTES       (NEOTRELLIS_LED_COUNT * 3)

// === Seesaw module IDs (from Adafruit Seesaw) ===
// (Keep these #defines grouped here so you can adjust if needed.)
#define SEESAW_STATUS_BASE     0x00
#define SEESAW_NEOPIXEL_BASE   0x0E
#define SEESAW_KEYPAD_BASE     0x10  // (not used in this minimal LED demo)

// Status registers (common patterns)
#define SEESAW_STATUS_HW_ID    0x01  // read: 1 byte (chip id)
#define SEESAW_STATUS_VERSION  0x02  // read: 4 bytes (fw ver)
#define SEESAW_STATUS_SWRST    0x7F  // write: any byte triggers reset

// NeoPixel registers (typical in Adafruit lib)
#define NEOPIXEL_PIN           0x01  // write: 1 byte (internal pin number)
#define NEOPIXEL_SPEED         0x02  // write: 1=800kHz, 0=400kHz (optional)
#define NEOPIXEL_BUF_LENGTH    0x03  // write: 2 bytes length (MSB,LSB)
#define NEOPIXEL_BUF           0x04  // write: RGB bytes payload
#define NEOPIXEL_SHOW          0x05  // write: any byte to latch/show

#define SEESAW_KEYPAD_BASE       0x10
#define KEYPAD_ENABLE            0x01   // write 1 to enable scanner
#define KEYPAD_INTEN             0x02   // write 1 to enable event IRQ/FIFO
#define SEESAW_KEYPAD_INTENSET   0x02
#define KEYPAD_COUNT             0x04   // read: # of events waiting
#define KEYPAD_FIFO              0x10   // read events (FIFO pop)

#define SEESAW_KEYPAD_EDGE_HIGH     0
#define SEESAW_KEYPAD_EDGE_LOW      1
#define SEESAW_KEYPAD_EDGE_FALLING  2
#define SEESAW_KEYPAD_EDGE_RISING   3





bool neotrellis_reset(void);
bool neotrellis_status(uint8_t *hw_id, uint32_t *version);
bool neopixel_begin(uint8_t internal_pin /* usually 3 */);
bool neopixel_set_bulk(const uint8_t *rgb48);
bool neopixel_show(void);
bool neotrellis_wait_ready(uint32_t timeout_ms);
bool neopixel_set_one_and_show(int index, uint8_t r, uint8_t g, uint8_t b);
bool neopixel_fill_all_and_show(uint8_t r, uint8_t g, uint8_t b);
bool trellis_keypad_begin(void);
bool trellis_read_event(uint8_t *idx, bool *pressed);
bool trellis_handle_events(void);

bool neopixel_test_simple();
bool neopixel_clear_all();
void neotrellis_rainbow_startup(void);
bool neotrellis_read_key_event(uint8_t *raw_key, uint8_t *edge);
void neotrellis_poll_and_light(void);
bool neotrellis_keypad_init(void);
void set_led_for_idx(int idx, bool on);
void neotrellis_clear_fifo(void);
bool neotrellis_poll_buttons(int *idx_out);
// bool neotrellis_poll_buttons(void);

static bool key_is_down[16] = { false };   // our debounced view of each key


#ifndef DEBUG_KEYS
#define DEBUG_KEYS 1
#endif
#if DEBUG_KEYS
  #define DKEY(fmt, ...) printf("[KEY] " fmt "\n", ##__VA_ARGS__)
#else
  #define DKEY(...)      ((void)0)
#endif