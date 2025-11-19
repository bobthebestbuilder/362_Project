
#include <stdio.h>
#include "pico/stdlib.h"
#include "seesaw.h"
#include "neotrellis.h"
#include "tusb_config.h"


static void scan_i2c(void) {
    printf("I2C scan:\n");
    for (uint8_t a = 0x08; a <= 0x77; a++) {
        uint8_t dummy = 0;
        int r = i2c_write_blocking(NEOTRELLIS_I2C, a, &dummy, 1, false);
        if (r >= 0) printf("  Found 0x%02X\n", a);
    }
}

int main() {
    stdio_init_all();
    setvbuf(stdout, NULL, _IONBF, 0);   
    sleep_ms(500);    
    printf("\n=== NeoTrellis bring-up ===\n");

    seesaw_bus_init(100000);  
    scan_i2c();
    
    if (!neotrellis_reset()) {
    printf("Failed to reset NeoTrellis!\n");
    while (1); 
}
    else { printf("neotrellis reset successfullly\n");}


    sleep_ms(1000);

    if (!neotrellis_wait_ready(500)) {
    printf("Device not ready.\n");
    return 0;
}



    if (!neopixel_begin(3)) {
        printf("neopixel_begin() failed. Check wiring and address.\n");
        while (1) { tight_loop_contents(); }
    }
    printf("NeoPixel init OK.\n");

        uint8_t speed = 0x01;  
    seesaw_write(NEOTRELLIS_ADDR, SEESAW_NEOPIXEL_BASE, NEOPIXEL_SPEED, &speed, 1);
    sleep_ms(300);


    neotrellis_rainbow_startup();
    sleep_ms(200);



printf("Before keypad init\n");
fflush(stdout);

    neotrellis_keypad_init();

    sleep_ms(200);

    neotrellis_clear_fifo(); 

printf("=== Starting main loop ===\n");


while (1) {
    int idx = -1;
    
    if (neotrellis_poll_buttons(&idx)) {
        printf("Button %d pressed!\n", idx);
    }
    
    //sleep_ms(5);  // Poll at 20Hz
}





    return 0;
}
