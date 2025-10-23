#include "pico/stdlib.h"
#include <hardware/gpio.h>
#include <stdio.h>
#include "queue.h"

// Global column variable
int col = -1;

// Global key state
static bool state[16]; // Are keys pressed/released

// Keymap for the keypad
const char keymap[17] = "DCBA#9630852*741";

void keypad_drive_column();
void keypad_isr();

/********************************************************* */
// Implement the functions below.

void keypad_init_pins() {
    for (int i = 6; i <= 9; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_OUT);
        gpio_put(i, 0);
    }
    for (int i = 2; i <= 5; i++) {
        gpio_init(i);
        gpio_set_dir(i, GPIO_IN);
    }
}

void keypad_init_timer() {
    timer0_hw->inte |= (1 << 0);
    timer0_hw->inte |= (1 << 1);
    irq_set_exclusive_handler(TIMER0_IRQ_0, keypad_drive_column);
    timer0_hw->alarm[0] = timer0_hw->timerawl + 1000000;
    irq_set_enabled(TIMER0_IRQ_0, true);

    irq_set_exclusive_handler(TIMER0_IRQ_1, keypad_isr);
    timer0_hw->alarm[1] = timer0_hw->timerawl + 1100000;
    irq_set_enabled(TIMER0_IRQ_1, true);
}

void keypad_drive_column() {
    hw_clear_bits(&timer0_hw->intr, 1u << 0);
    col = (col + 1) % 4;
    
    sio_hw->gpio_clr = (0xFu << 6);
    sio_hw->gpio_set = (1u << (6 + col));
    timer0_hw->alarm[0] = timer0_hw->timerawl + 1000;
}

uint8_t keypad_read_rows() {
    return (sio_hw->gpio_in >> 2) & 0xF;
}

void keypad_isr() {
    hw_clear_bits(&timer0_hw->intr, 1u << 1);
    uint8_t rows = keypad_read_rows();
    
    for (int i = 0; i < 4; i++) {
        int key_index = col * 4 + i;
        bool row_on = (rows >> i) & 1;
        
        if (row_on && !state[key_index]) {
            state[key_index] = true;
            char key = keymap[key_index];
            uint16_t event = (1u << 8) | (uint8_t)key;
            key_push(event);
        }
        else if (!row_on && state[key_index]) {
            state[key_index] = false;
            char key = keymap[key_index];
            uint16_t event = (0u << 8) | (uint8_t)key;
            key_push(event);
        }
    }
    timer0_hw->alarm[1] = timer0_hw->timerawl + 25000;
}