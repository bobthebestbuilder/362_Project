#include "neotrellis.h"
#include "seesaw.h"
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include <string.h>
#include <stdio.h>

// === AUDIO CONFIGURATION ===
#define AUDIO_PIN 15  // Using Pin 15 as requested

static uint audio_slice_num;
static uint audio_channel;

// Standard Frequencies for buttons 0-15 (C4 upwards)
static const uint16_t note_freqs[16] = {
    262, 277, 294, 311, 330, 349, 370, 392, // C4 - G4
    415, 440, 466, 494, 523, 554, 587, 622  // G#4 - D#5
};

// === PWM FUNCTIONS ===

void init_audio_pwm(void) {
    gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);
    audio_slice_num = pwm_gpio_to_slice_num(AUDIO_PIN);
    audio_channel = pwm_gpio_to_channel(AUDIO_PIN);

    // Enable PWM but start silent (duty cycle 0)
    pwm_set_enabled(audio_slice_num, true);
    pwm_set_chan_level(audio_slice_num, audio_channel, 0);
}

void play_note(uint16_t freq) {
    if (freq < 20) return; 

    uint32_t clock_hz = clock_get_hz(clk_sys);
    
    // f_note = f_clk / (TOP + 1)
    uint32_t top = (clock_hz / freq) - 1;

    pwm_set_wrap(audio_slice_num, top);
    pwm_set_chan_level(audio_slice_num, audio_channel, top / 2);
}

void stop_note(void) {
    pwm_set_chan_level(audio_slice_num, audio_channel, 0);
}

// === NEOTRELLIS FUNCTIONS ===

bool neotrellis_reset(void) {
    uint8_t dum = 0xFF;
    bool ok = seesaw_write(NEOTRELLIS_ADDR, SEESAW_STATUS_BASE, SEESAW_STATUS_SWRST, &dum, 1);
    sleep_ms(2);                 
    return ok;
}

bool neotrellis_status(uint8_t *hw_id, uint32_t *version) {
    bool ok = true;
    if (hw_id)  ok &= seesaw_read(NEOTRELLIS_ADDR, SEESAW_STATUS_BASE, SEESAW_STATUS_HW_ID, hw_id, 1);
    if (version) {
        uint8_t buf[4];
        ok &= seesaw_read(NEOTRELLIS_ADDR, SEESAW_STATUS_BASE, SEESAW_STATUS_VERSION, buf, 4);
        *version = (buf[0]<<24) | (buf[1]<<16) | (buf[2]<<8) | buf[3];
    }
    return ok;
}

bool neotrellis_wait_ready(uint32_t timeout_ms) {
    absolute_time_t dl = make_timeout_time_ms(timeout_ms);
    uint8_t id;
    while (!time_reached(dl)) {
        if (seesaw_read(NEOTRELLIS_ADDR, SEESAW_STATUS_BASE, SEESAW_STATUS_HW_ID, &id, 1)) {
            if (id == 0x55) return true;   
        }
        sleep_ms(5);
    }
    return false;
}

bool neopixel_begin(uint8_t internal_pin) {
    sleep_ms(100);                             

    uint8_t hw_id1 = 0;
    if (!seesaw_read(NEOTRELLIS_ADDR, SEESAW_STATUS_BASE, SEESAW_STATUS_HW_ID, &hw_id1, 1)) {
        printf("Status check #1 failed\n");
        return false;
    }
    printf("Status check #1: HW_ID=0x%02X\n", hw_id1);
    sleep_ms(10);

    uint16_t len = 48;                                
    uint8_t len_be[2] = { 0x00, 0x30 };
    if (!seesaw_write(NEOTRELLIS_ADDR, SEESAW_NEOPIXEL_BASE, NEOPIXEL_BUF_LENGTH, len_be, 2)) {
        printf("BUF_LENGTH write failed\n"); return false;
    } 
    sleep_ms(200);

    uint8_t speed = 0x01;  
    if (!seesaw_write(NEOTRELLIS_ADDR, SEESAW_NEOPIXEL_BASE, NEOPIXEL_SPEED, &speed, 1)) {
        printf("SPEED set fail\n");
        return false;
    }
    sleep_ms(200);

    uint8_t pin = 3;  
    if (!seesaw_write(NEOTRELLIS_ADDR, SEESAW_NEOPIXEL_BASE, NEOPIXEL_PIN, &pin, 1)) { 
        printf("PIN set fail\n");
    } 
    sleep_ms(200);

    if (!neotrellis_wait_ready(300)) {
        printf("HW_ID never became 0x55\n");
        return false;
    }
    printf("HW_ID OK (0x55)\n");

    return true;
}

bool neopixel_show(void) {
    uint8_t hdr[2] = { SEESAW_NEOPIXEL_BASE, NEOPIXEL_SHOW };
    int wrote = i2c_write_blocking(NEOTRELLIS_I2C, NEOTRELLIS_ADDR, hdr, 2, false);
    if (wrote != 2) return false;
    sleep_ms(10);  
    return true;
}

static bool neopixel_buf_write(uint16_t start, const uint8_t *data, size_t len) {
    while (len) {
        size_t n = len > 28 ? 28 : len;  
        uint8_t payload[2 + 28];
        
        payload[0] = (uint8_t)(start >> 8) & 0xFF;
        payload[1] = (uint8_t)(start & 0xFF);
        memcpy(&payload[2], data, n);  
        
        size_t total = 2 + n;  
        
        bool ok = seesaw_write_buf(NEOTRELLIS_ADDR, SEESAW_NEOPIXEL_BASE, 
                                   NEOPIXEL_BUF, payload, total);
        if (!ok) return false;
        
        start += (uint16_t)n;
        data  += n;
        len   -= n;
    }
    return true;
}

bool neopixel_set_one_and_show(int idx, uint8_t r, uint8_t g, uint8_t b) {
    if ((unsigned)idx >= 16) return false;
    uint16_t start = (uint16_t)(idx * 3);
    uint8_t  grb[3] = { g, r, b };           
    if (!neopixel_buf_write(start, grb, 3)) return false;
    sleep_us(300);   
    return neopixel_show();
}

bool neopixel_fill_all_and_show(uint8_t r, uint8_t g, uint8_t b) {
    uint8_t frame[48];
    for (int i = 0; i < 16; ++i) {
        frame[3 * i + 0] = g;
        frame[3 * i + 1] = r;
        frame[3 * i + 2] = b;
    }
    if (!neopixel_buf_write(0, frame, 48)) return false;
    sleep_us(300);
    return neopixel_show();
}

static void color_wheel(uint8_t pos, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (pos < 85) {
        *r = 255 - pos * 3;
        *g = pos * 3;
        *b = 0;
    } else if (pos < 170) {
        pos -= 85;
        *r = 0;
        *g = 255 - pos * 3;
        *b = pos * 3;
    } else {
        pos -= 170;
        *r = pos * 3;
        *g = 0;
        *b = 255 - pos * 3;
    }
}

void neotrellis_rainbow_startup(void) {
    uint8_t frame[48];        
    const int frames = 32;    
    const int delay_ms = 60;  

    for (int step = 0; step < frames; ++step) {
        for (int i = 0; i < 16; ++i) {
            uint8_t r, g, b;
            uint8_t hue = (i * 16 + step * 8) & 0xFF;
            color_wheel(hue, &r, &g, &b);
            frame[3 * i + 0] = g;
            frame[3 * i + 1] = r;
            frame[3 * i + 2] = b;
        }
        neopixel_buf_write(0, frame, 48);
        sleep_us(300);
        neopixel_show();
        sleep_ms(delay_ms);
    }
    neopixel_fill_all_and_show(0, 0, 0);
}

static const uint8_t neotrellis_key_lut[16] = {
    0, 1, 2, 3,
    8, 9, 10, 11,
    16, 17, 18, 19,
    24, 25, 26, 27
};

// Helper to clear any pending events so we start fresh
void neotrellis_clear_fifo(void) {
    while (1) {
        uint8_t count = 0;
        if (!seesaw_read(NEOTRELLIS_ADDR, SEESAW_KEYPAD_BASE, KEYPAD_COUNT, &count, 1)) break;
        if (count == 0 || count == 0xFF) break;
        
        uint8_t dump[32]; 
        size_t read_len = (count > 8) ? 8 : count;
        seesaw_read(NEOTRELLIS_ADDR, SEESAW_KEYPAD_BASE, KEYPAD_FIFO, dump, read_len);
    }
}

bool neotrellis_keypad_init(void) {
    printf("[neo] keypad_init: start\n");
    
    init_audio_pwm();
    
    // 1. Enable Keypad Interrupts
    uint8_t val = 0x01;
    if (!seesaw_write(NEOTRELLIS_ADDR, SEESAW_KEYPAD_BASE, KEYPAD_INTEN, &val, 1)) {
        printf("[neo] enableKeypadInterrupt failed\n");
        return false;
    }

    // 2. Enable RISING and FALLING events for all keys
    // RESTORED: 0x01 (High Level / Enable Bit) which was present in original code
    uint8_t event_mask = (1 << SEESAW_KEYPAD_EDGE_RISING) | (1 << SEESAW_KEYPAD_EDGE_FALLING) | 0x01;

    for (int i = 0; i < 16; i++) {
        uint8_t key = neotrellis_key_lut[i];
        uint8_t cmd[2] = { key, event_mask };
        
        if (!seesaw_write(NEOTRELLIS_ADDR, SEESAW_KEYPAD_BASE, KEYPAD_ENABLE, cmd, 2)) {
             printf("[neo] Failed to set event for key %d\n", key);
             return false;
        }
    }
    
    // 3. Clear FIFO now to prevent old events from confusing the loop
    neotrellis_clear_fifo();
    
    printf("[neo] keypad_init OK\n");
    return true;
}

void set_led_for_idx(int idx, bool on)
{
    if (!on) {
        neopixel_set_one_and_show(idx, 0x00, 0x00, 0x00);
        stop_note(); 
        return;
    }

    if (idx == 0)  { neopixel_set_one_and_show(0, 0x20, 0x00, 0x00); play_note(note_freqs[0]); } 
    if (idx == 1)  { neopixel_set_one_and_show(1, 0x00, 0x20, 0x00); play_note(note_freqs[1]); } 
    if (idx == 2)  { neopixel_set_one_and_show(2, 0x00, 0x00, 0x20); play_note(note_freqs[2]); } 
    if (idx == 3)  { neopixel_set_one_and_show(3, 0x20, 0x20, 0x00); play_note(note_freqs[3]); } 

    if (idx == 4)  { neopixel_set_one_and_show(4, 0x20, 0x00, 0x20); play_note(note_freqs[4]); } 
    if (idx == 5)  { neopixel_set_one_and_show(5, 0x00, 0x20, 0x20); play_note(note_freqs[5]); } 
    if (idx == 6)  { neopixel_set_one_and_show(6, 0x10, 0x10, 0x20); play_note(note_freqs[6]); } 
    if (idx == 7)  { neopixel_set_one_and_show(7, 0x20, 0x10, 0x00); play_note(note_freqs[7]); } 

    if (idx == 8)  { neopixel_set_one_and_show(8, 0x10, 0x20, 0x00); play_note(note_freqs[8]); } 
    if (idx == 9)  { neopixel_set_one_and_show(9, 0x00, 0x10, 0x20); play_note(note_freqs[9]); } 
    if (idx == 10) { neopixel_set_one_and_show(10, 0x20, 0x00, 0x10); play_note(note_freqs[10]); } 
    if (idx == 11) { neopixel_set_one_and_show(11, 0x10, 0x00, 0x20); play_note(note_freqs[11]); } 

    if (idx == 12) { neopixel_set_one_and_show(12, 0x05, 0x20, 0x05); play_note(note_freqs[12]); } 
    if (idx == 13) { neopixel_set_one_and_show(13, 0x20, 0x05, 0x05); play_note(note_freqs[13]); } 
    if (idx == 14) { neopixel_set_one_and_show(14, 0x05, 0x05, 0x20); play_note(note_freqs[14]); } 
    if (idx == 15) { neopixel_set_one_and_show(15, 0x20, 0x10, 0x20); play_note(note_freqs[15]); } 
}

bool neotrellis_poll_buttons(int *idx_out)
{
    uint8_t count = 0;
    if (!seesaw_read(NEOTRELLIS_ADDR, SEESAW_KEYPAD_BASE, KEYPAD_COUNT, &count, 1)) return false;
    if (count == 0 ) return false;
    if (count > 16) count = 16; 
    
    for (uint8_t e = 0; e < count; e++) {
        uint8_t evt;
        // 10ms delay to let I2C breathe 
        sleep_us(600); 
        if (!seesaw_read(NEOTRELLIS_ADDR, SEESAW_KEYPAD_BASE, KEYPAD_FIFO, &evt, 1)) return false;
        
        uint8_t keynum = evt >> 2;
        uint8_t edge = evt & 0x03;

        if (evt == 0xFF) continue;

        int idx = -1;
        for (int i = 0; i < 16; i++) {
            if (neotrellis_key_lut[i] == keynum) {
                idx = i;
                break;
            }
        }
        if (idx < 0) continue; 

        if (edge == SEESAW_KEYPAD_EDGE_RISING) {
            printf("Button %d Pressed\n", idx);
            set_led_for_idx(idx, true); 
            if (idx_out) *idx_out = idx;
            return true;
        }
        else if (edge == SEESAW_KEYPAD_EDGE_FALLING) {
            printf("Button %d Released\n", idx);
            set_led_for_idx(idx, false);
        }
    }
    
    return false;
}