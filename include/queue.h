
#ifndef QUEUE_H
#define QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"

// Queue structure for key events
typedef struct {
    uint16_t q[32];
    uint8_t head;
    uint8_t tail;
} KeyEvents;

extern KeyEvents kev;

// Queue functions
void key_init(void);
bool key_empty(void);
uint16_t key_pop(void);
void key_push(uint16_t value);

#endif // QUEUE_H