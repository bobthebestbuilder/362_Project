#include "seesaw.h"
#include "pico/stdlib.h"
#include <string.h>


void seesaw_bus_init(uint32_t hz) {
    i2c_init(NEOTRELLIS_I2C, hz);
    gpio_set_function(NEOTRELLIS_SDA, GPIO_FUNC_I2C);
    gpio_set_function(NEOTRELLIS_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(NEOTRELLIS_SDA);
    gpio_pull_up(NEOTRELLIS_SCL);
    
}



bool seesaw_write(uint8_t addr, uint8_t module, uint8_t reg,
                  const uint8_t *data, uint16_t len) {
    uint8_t buf[2 + len];
    buf[0] = module;
    buf[1] = reg;
    for (uint16_t i = 0; i < len; i++) buf[2 + i] = data[i];

    int written = i2c_write_blocking(NEOTRELLIS_I2C, addr, buf, 2 + len, false);
    return written == (int)(2 + len);
}



bool seesaw_write_buf(uint8_t addr, uint8_t module, uint8_t reg,
                  const uint8_t *data, size_t len)
{
    
    size_t total = 2 + len;
    uint8_t frame[2 + 30];            
    frame[0] = module;
    frame[1] = reg;
    if (len) memcpy(&frame[2], data, len);

    size_t used = total;     
    size_t cap  = 2 + 30;        
    if (used < cap) {
        memset(&frame[used], 0, cap - used);
    }



    int wrote = i2c_write_blocking(NEOTRELLIS_I2C, addr, frame, total, false);


    
    return wrote == (int)total;
}





bool seesaw_read(uint8_t addr, uint8_t module, uint8_t reg,
                 uint8_t *data, uint16_t len) {
    uint8_t hdr[2] = { module, reg };

    if (i2c_write_blocking(NEOTRELLIS_I2C, addr, hdr, 2, true) < 0) return false;

    sleep_us(300);
    return i2c_read_blocking(NEOTRELLIS_I2C, addr, data, len, false) >= 0;
}