#include "neotrellis.h"
#include "seesaw.h"
#include "pico/stdlib.h"
#include <string.h>
#include <stdio.h>

// ==========================================
// Basic Seesaw/NeoPixel Commands
// ==========================================

bool neotrellis_reset(void) {
    uint8_t dum = 0xFF;
    bool ok = seesaw_write(NEOTRELLIS_ADDR, SEESAW_STATUS_BASE, SEESAW_STATUS_SWRST, &dum, 1);
    sleep_ms(10);                 
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
    sleep_ms(10);

    uint16_t len = 48;                                
    uint8_t len_be[2] = { 0x00, 0x30 };
    if (!seesaw_write(NEOTRELLIS_ADDR, SEESAW_NEOPIXEL_BASE, NEOPIXEL_BUF_LENGTH, len_be, 2)) {
        printf("BUF_LENGTH write failed\n"); return false;
    } 
    sleep_ms(20);

    uint8_t speed = 0x01;  
    if (!seesaw_write(NEOTRELLIS_ADDR, SEESAW_NEOPIXEL_BASE, NEOPIXEL_SPEED, &speed, 1)) {
        return false;
    }
    sleep_ms(20);

    uint8_t pin = internal_pin;  
    if (!seesaw_write(NEOTRELLIS_ADDR, SEESAW_NEOPIXEL_BASE, NEOPIXEL_PIN, &pin, 1)) {
         printf("PIN set fail\n");
    } 
    sleep_ms(20);

    if (!neotrellis_wait_ready(300)) {
        printf("HW_ID never became 0x55\n");
        return false;
    }
    return true;
}

bool neopixel_show(void) {
    uint8_t hdr[2] = { SEESAW_NEOPIXEL_BASE, NEOPIXEL_SHOW };
    int wrote = i2c_write_blocking(NEOTRELLIS_I2C, NEOTRELLIS_ADDR, hdr, 2, false);
    if (wrote != 2) {
        return false;
    }
    sleep_ms(5); 
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
        if (!seesaw_write_buf(NEOTRELLIS_ADDR, SEESAW_NEOPIXEL_BASE, NEOPIXEL_BUF, payload, total)) {
            return false;
        }
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
        *r = 255 - pos * 3; *g = pos * 3; *b = 0;
    } else if (pos < 170) {
        pos -= 85; *r = 0; *g = 255 - pos * 3; *b = pos * 3;
    } else {
        pos -= 170; *r = pos * 3; *g = 0; *b = 255 - pos * 3;
    }
}

void neotrellis_rainbow_startup(void) {
    uint8_t frame[48];        
    const int frames = 32;    
    const int delay_ms = 20;  

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

// ==========================================
// KEYPAD LOGIC
// ==========================================

static const uint8_t neotrellis_key_lut[16] = {
    0, 1, 2, 3,
    8, 9, 10, 11,
    16, 17, 18, 19,
    24, 25, 26, 27
};

bool neotrellis_keypad_init(void) {
    // 1. Enable interrupts
    uint8_t val = 0x01;
    if (!seesaw_write(NEOTRELLIS_ADDR, SEESAW_KEYPAD_BASE, KEYPAD_INTEN, &val, 1)) return false;

    // 2. Configure keys
    for (int i = 0; i < 16; i++) {
        uint8_t key = neotrellis_key_lut[i];

        // 0x0D = (1<<0 | 1<<2 | 1<<3) -> Enable | Falling | Rising
        // This avoids enabling "Low Level" (1<<1) which caused the spam.
        uint8_t cmd[2] = { key, 0x0D }; 

        if (!seesaw_write(NEOTRELLIS_ADDR, SEESAW_KEYPAD_BASE, KEYPAD_ENABLE, cmd, 2)) return false;
        
        // Delay is critical to prevent dropping commands
        sleep_ms(20); 
    }
    return true;
}

void set_led_for_idx(int idx, bool on) {
    if (!on) {
        neopixel_set_one_and_show(idx, 0x00, 0x00, 0x00);
        return;
    }
    if (idx < 4) neopixel_set_one_and_show(idx, 0x20, 0x00, 0x00); 
    else if (idx < 8) neopixel_set_one_and_show(idx, 0x00, 0x20, 0x00); 
    else if (idx < 12) neopixel_set_one_and_show(idx, 0x00, 0x00, 0x20); 
    else neopixel_set_one_and_show(idx, 0x20, 0x00, 0x20); 
}

bool neotrellis_poll_buttons(int *idx_out, int *edge_out) {
    uint8_t count = 0;
    if (!seesaw_read(NEOTRELLIS_ADDR, SEESAW_KEYPAD_BASE, KEYPAD_COUNT, &count, 1)) return false;
    if (count == 0) return false;
    if (count > 8) count = 8;
    
    for (uint8_t e = 0; e < count; e++) {
        uint8_t evt;
        if (!seesaw_read(NEOTRELLIS_ADDR, SEESAW_KEYPAD_BASE, KEYPAD_FIFO, &evt, 1)) return false;
        
        uint8_t keynum = evt >> 2;
        uint8_t edge = evt & 0x03;
        if (evt == 0xFF) continue;

        // Map Hardware Key to 0-15 Index
        int idx = -1;
        for (int i = 0; i < 16; i++) {
            if (neotrellis_key_lut[i] == keynum) {
                idx = i;
                break;
            }
        }
        if (idx < 0) continue; // Unknown key

        // Logic: 3=Rising(Pressed), 2=Falling(Released)
        if (edge == SEESAW_KEYPAD_EDGE_RISING) {
            set_led_for_idx(idx, true);    
            if (idx_out) *idx_out = idx;
            if (edge_out) *edge_out = edge;
            return true;
        }
        else if (edge == SEESAW_KEYPAD_EDGE_FALLING) {
            set_led_for_idx(idx, false);     
            if (idx_out) *idx_out = idx;
            if (edge_out) *edge_out = edge;
            return true;
        }
    }
    return false;
}

void neotrellis_clear_fifo(void) {
    while (1) {
        uint8_t count = 0;
        if (!seesaw_read(NEOTRELLIS_ADDR, SEESAW_KEYPAD_BASE, KEYPAD_COUNT, &count, 1)) break;
        if (count == 0 || count == 0xFF) break;
        if (count > 8) count = 8; 
        uint8_t dump[4 * 8];      
        if (!seesaw_read(NEOTRELLIS_ADDR, SEESAW_KEYPAD_BASE, KEYPAD_FIFO, dump, 4 * count)) break;
    }
}