

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "hardware/i2c.h"

// Pick one bus & its pins (both pins must be on the same I2C block)
#ifndef NEOTRELLIS_I2C
#define NEOTRELLIS_I2C       i2c0        // change to i2c1 if you’re on GP20/21
#endif
#ifndef NEOTRELLIS_SDA
#define NEOTRELLIS_SDA       4           // 4/5  OR  20/21
#endif
#ifndef NEOTRELLIS_SCL
#define NEOTRELLIS_SCL       5
#endif

// Default NeoTrellis (seesaw) 7-bit I2C address
#ifndef NEOTRELLIS_ADDR
#define NEOTRELLIS_ADDR      0x2E
#endif

void seesaw_bus_init(uint32_t hz);
bool seesaw_write(uint8_t addr, uint8_t module, uint8_t reg,
                  const uint8_t *data, uint16_t len);
bool seesaw_read(uint8_t addr, uint8_t module, uint8_t reg,
                 uint8_t *data, uint16_t len);

                 bool seesaw_write_buf(uint8_t addr, uint8_t module, uint8_t reg,
                  const uint8_t *data, size_t len);
// Convenience
static inline bool seesaw_write_u8(uint8_t addr, uint8_t module, uint8_t reg, uint8_t v) {
    return seesaw_write(addr, module, reg, &v, 1);
}
