#include "queue.h"

KeyEvents kev = { .q = {0}, .head = 0, .tail = 0 };

void key_init(void) {
    kev.head = 0;
    kev.tail = 0;
}

bool key_empty(void) {
    return kev.head == kev.tail;
}

uint16_t key_pop(void) {
    // Wait while queue is empty
    while (kev.head == kev.tail) {
        sleep_ms(10);   // Wait for an event to be pushed
    }
    uint16_t value = kev.q[kev.tail];
    kev.tail = (kev.tail + 1) % 32;
    return value;
}

void key_push(uint16_t value) {
    // Queue is full, drop new keys
    if ((kev.head + 1) % 32 == kev.tail) {
        return; 
    }
    kev.q[kev.head] = value;
    kev.head = (kev.head + 1) % 32;
}