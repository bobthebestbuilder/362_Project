#ifndef KEYPAD_H
#define KEYPAD_H

#include <stdint.h>
#include <stdbool.h>

// Initialize keypad GPIO pins
void keypad_init_pins(void);

// Initialize keypad timer interrupts
void keypad_init_timer(void);

// Drive the next column (called by timer ISR)
void keypad_drive_column(void);

// Read the row inputs
uint8_t keypad_read_rows(void);

// Handle keypad scanning (called by timer ISR)
void keypad_isr(void);

#endif // KEYPAD_H