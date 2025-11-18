#include "neotrellis.h"
#include "seesaw.h"
#include "pico/stdlib.h"
#include <string.h>
#include <stdio.h>

// Key lookup table - maps NeoTrellis raw keynum to logical button index
const uint8_t neotrellis_key_lut[16] = {
    0, 1, 2, 3,
    8, 9, 10, 11,
    16, 17, 18, 19,
    24, 25, 26, 27
};

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
        printf("BUF_LENGTH write failed\n");
        return false;
    } else {
        printf("BUF length set successfully to 0x%04X (%u)  [MSB=0x%02X LSB=0x%02X]\n", len, len, len_be[0], len_be[1]);
    }
    sleep_ms(200);

    uint8_t speed = 0x01;
    if (!seesaw_write(NEOTRELLIS_ADDR, SEESAW_NEOPIXEL_BASE, NEOPIXEL_SPEED, &speed, 1)) {
        printf("SPEED set fail\n");
        return false;
    }
    printf("SPEED set successfully\n");
    sleep_ms(200);

    uint8_t pin = 3;
    if (!seesaw_write(NEOTRELLIS_ADDR, SEESAW_NEOPIXEL_BASE, NEOPIXEL_PIN, &pin, 1)) {
        printf("PIN set fail\n");
    } else {
        printf("PIN set successfully to %d\n", pin);
    }
    sleep_ms(200);

    if (!neotrellis_wait_ready(300)) {
        printf("HW_ID never became 0x55\n");
        return false;
    }
    printf("HW_ID OK (0x55)\n");

    uint8_t probe_id = 0;
    bool probe_ok = neotrellis_status(&probe_id, NULL);
    printf("Another status check @0x%02X: %s, HW_ID=0x%02X\n", NEOTRELLIS_ADDR, probe_ok ? "OK" : "FAIL", probe_id);

    return true;
}

bool neopixel_show(void) {
    uint8_t hdr[2] = { SEESAW_NEOPIXEL_BASE, NEOPIXEL_SHOW };
    int wrote = i2c_write_blocking(NEOTRELLIS_I2C, NEOTRELLIS_ADDR, hdr, 2, false);
    
    if (wrote != 2) {
        printf("SHOW command FAILED!\n");
        return false;
    }
    
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
        if (!ok) {
            return false;
        }
        
        start += (uint16_t)n;
        data  += n;
        len   -= n;
    }
    
    return true;
}

bool neopixel_set_one_and_show(int idx, uint8_t r, uint8_t g, uint8_t b) {
    if ((unsigned)idx >= 16) {
        printf("idx out of range\n");
        return false;
    }

    uint16_t start = (uint16_t)(idx * 3);
    uint8_t  grb[3] = { g, r, b };

    uint8_t zeros[48] = {0};
    neopixel_buf_write(0, zeros, 28);
    sleep_ms(2);
    neopixel_buf_write(28, zeros + 28, 20);
    sleep_ms(2);
    neopixel_show();
    
    if (!neopixel_buf_write(start, grb, 3)) {
        printf("BUF write failed (idx=%d start=%u)\n", idx, start);
        return false;
    }

    sleep_us(300);
    int ok = neopixel_show();
    return ok;
}

bool neopixel_fill_all_and_show(uint8_t r, uint8_t g, uint8_t b) {
    uint8_t frame[48];
    for (int i = 0; i < 16; ++i) {
        frame[3 * i + 0] = g;
        frame[3 * i + 1] = r;
        frame[3 * i + 2] = b;
    }
    if (!neopixel_buf_write(0, frame, 48)) {
        printf("neopixel_fill_all_and_show: buf_write failed\n");
        return false;
    }
    sleep_us(300);
    int ok = neopixel_show();
    return ok;
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

static bool set_keypad_event(uint8_t key, uint8_t edge, bool enable) {
    uint8_t ks = 0;
    if (enable) {
        ks |= 0x01;
        ks |= (1u << (edge + 1));
    }

    uint8_t cmd[2] = { key, ks };

    printf("[neo] enable %s: key=%d cfg=0x%02x\n",
           (edge == SEESAW_KEYPAD_EDGE_RISING) ? "rising" : "falling",
           key, ks);

    return seesaw_write(NEOTRELLIS_ADDR,
                        SEESAW_KEYPAD_BASE,
                        KEYPAD_ENABLE,
                        cmd, sizeof(cmd));
}

bool neotrellis_keypad_init(void) {
    printf("[neo] keypad_init: start\n");
    
    uint8_t val = 0x01;
    if (!seesaw_write(NEOTRELLIS_ADDR, SEESAW_KEYPAD_BASE, KEYPAD_INTEN, &val, 1)) {
        printf("[neo] enableKeypadInterrupt failed\n");
        return false;
    }

    for (int i = 0; i < 16; i++) {
        uint8_t key = neotrellis_key_lut[i];
        
        if (!set_keypad_event(key, SEESAW_KEYPAD_EDGE_RISING, true)) {
            printf("[neo] setKeypadEvent rising failed for key %d\n", key);
            return false;
        }
        
        if (!set_keypad_event(key, SEESAW_KEYPAD_EDGE_FALLING, true)) {
            printf("[neo] setKeypadEvent falling failed for key %d\n", key);
            return false;
        }
    }
    
    printf("[neo] keypad_init OK\n");
    return true;
}

void set_led_for_idx(int idx, bool on)
{
    if (!on) {
        neopixel_set_one_and_show(idx, 0x00, 0x00, 0x00);
        return;
    }

    if (idx == 0)  { neopixel_set_one_and_show(0, 0x20, 0x00, 0x00); }   // red
    if (idx == 1)  { neopixel_set_one_and_show(1, 0x00, 0x20, 0x00); }   // green
    if (idx == 2)  { neopixel_set_one_and_show(2, 0x00, 0x00, 0x20); }   // blue
    if (idx == 3)  { neopixel_set_one_and_show(3, 0x20, 0x20, 0x00); }   // yellow

    if (idx == 4)  { neopixel_set_one_and_show(4, 0x20, 0x00, 0x20); }   // magenta
    if (idx == 5)  { neopixel_set_one_and_show(5, 0x00, 0x20, 0x20); }   // cyan
    if (idx == 6)  { neopixel_set_one_and_show(6, 0x10, 0x10, 0x20); }   // bluish
    if (idx == 7)  { neopixel_set_one_and_show(7, 0x20, 0x10, 0x00); }   // orange

    if (idx == 8)  { neopixel_set_one_and_show(8, 0x10, 0x20, 0x00); }   // yellow-green
    if (idx == 9)  { neopixel_set_one_and_show(9, 0x00, 0x10, 0x20); }   // teal
    if (idx == 10) { neopixel_set_one_and_show(10, 0x20, 0x00, 0x10); }  // pink-red
    if (idx == 11) { neopixel_set_one_and_show(11, 0x10, 0x00, 0x20); }  // violet

    if (idx == 12) { neopixel_set_one_and_show(12, 0x05, 0x20, 0x05); }  // light green
    if (idx == 13) { neopixel_set_one_and_show(13, 0x20, 0x05, 0x05); }  // light red
    if (idx == 14) { neopixel_set_one_and_show(14, 0x05, 0x05, 0x20); }  // light blue
    if (idx == 15) { neopixel_set_one_and_show(15, 0x20, 0x10, 0x20); }  // lavender
}

bool neotrellis_poll_buttons(int *idx_out)
{
    uint8_t count = 0;
    
    if (!seesaw_read(NEOTRELLIS_ADDR, SEESAW_KEYPAD_BASE, KEYPAD_COUNT, &count, 1)) {
        return false;
    }
    
    if (count == 0) {
        return false;
    }
    
    if (count > 8) count = 8;
    
    printf("[neo] KEYPAD_COUNT = %u\n", count);

    for (uint8_t e = 0; e < count; e++) {
        uint8_t evt;
        if (!seesaw_read(NEOTRELLIS_ADDR, SEESAW_KEYPAD_BASE, KEYPAD_FIFO, &evt, 1)) {
            return false;
        }
        
        uint8_t keynum = evt >> 2;
        uint8_t edge = evt & 0x03;
        printf("[neo] event %u: raw0=0x%02x keynum=%u edge=%u\n", e, evt, keynum, edge);

        if (evt == 0xFF || edge == 0) {
            continue;
        }

        int idx = -1;
        for (int i = 0; i < 16; i++) {
            if (neotrellis_key_lut[i] == keynum) {
                idx = i;
                break;
            }
        }
        if (idx < 0) {
            continue;
        }

        if (edge == SEESAW_KEYPAD_EDGE_RISING) {
            set_led_for_idx(idx, true);
            if (idx_out) {
                *idx_out = idx;
                printf("[neo] Button %d PRESSED (keynum=%u)\n", idx, keynum);
                return true;
            }
        }
        else if (edge == SEESAW_KEYPAD_EDGE_FALLING) {
            set_led_for_idx(idx, false);
            printf("[neo] Button %d RELEASED (keynum=%u)\n", idx, keynum);
        }
    }
    
    return false;
}

void neotrellis_clear_fifo(void)
{
    while (1) {
        uint8_t count = 0;

        if (!seesaw_read(NEOTRELLIS_ADDR,
                         SEESAW_KEYPAD_BASE,
                         KEYPAD_COUNT,
                         &count, 1)) {
            printf("[neo] clear_fifo: count read failed\n");
            return;
        }

        if (count == 0 || count == 0xFF) {
            break;
        }

        if (count > 8) count = 8;

        uint8_t dump[4 * 8];

        if (!seesaw_read(NEOTRELLIS_ADDR,
                         SEESAW_KEYPAD_BASE,
                         KEYPAD_FIFO,
                         dump,
                         4 * count)) {
            printf("[neo] clear_fifo: FIFO read failed\n");
            return;
        }
    }

    printf("[neo] FIFO cleared\n");
}

// ADDED FOR AUDIO INTEGRATION

ButtonEvent neotrellis_poll_buttons_audio(void)
{
    ButtonEvent result = {-1, false, false};
    
    uint8_t count = 0;
    if (!seesaw_read(NEOTRELLIS_ADDR, SEESAW_KEYPAD_BASE, KEYPAD_COUNT, &count, 1)) {
        return result;
    }
    
    if (count == 0) return result;
    if (count > 8) count = 8;
    
    for (uint8_t e = 0; e < count; e++) {
        uint8_t evt;
        if (!seesaw_read(NEOTRELLIS_ADDR, SEESAW_KEYPAD_BASE, KEYPAD_FIFO, &evt, 1)) {
            continue;
        }
        
        uint8_t keynum = evt >> 2;
        uint8_t edge = evt & 0x03;
        
        if (evt == 0xFF || edge == 0) continue;
        
        // Convert keynum to button index
        int idx = -1;
        for (int i = 0; i < 16; i++) {
            if (neotrellis_key_lut[i] == keynum) {
                idx = i;
                break;
            }
        }
        
        if (idx < 0) continue;
        
        // Handle RISING (press) and FALLING (release)
        if (edge == SEESAW_KEYPAD_EDGE_RISING) {
            set_led_for_idx(idx, true);
            result.button_idx = idx;
            result.is_pressed = true;
            result.valid = true;
            printf("[neo] Button %d PRESSED (keynum=%u)\n", idx, keynum);
            return result;
        }
        else if (edge == SEESAW_KEYPAD_EDGE_FALLING) {
            set_led_for_idx(idx, false);
            result.button_idx = idx;
            result.is_pressed = false;
            result.valid = true;
            printf("[neo] Button %d RELEASED (keynum=%u)\n", idx, keynum);
            return result;
        }
    }
    
    return result;
}