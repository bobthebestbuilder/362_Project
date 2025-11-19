
#include <stdio.h>
#include "pico/stdlib.h"
#include "seesaw.h"
#include "neotrellis.h"
#include "tusb_config.h"
<<<<<<< HEAD

=======
#include "audio.h"
>>>>>>> b254e24f7baaa6ee361e9fabb7ffe31b75a24716

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
<<<<<<< HEAD
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





=======
    sleep_ms(2000);   
    printf("\n=== NeoTrellis Music Instrument ===\n");

    audio_init();
    
    seesaw_bus_init(100000);  
    scan_i2c(); 
    
    if (!neotrellis_reset()) {
        printf("Error: Failed to reset NeoTrellis!\n");
        while (1) tight_loop_contents(); 
    }
    printf("NeoTrellis reset OK.\n");
    sleep_ms(1000);

    if (!neotrellis_wait_ready(500)) {
        printf("Error: Device not ready.\n");
        return 0;
    }

    if (!neopixel_begin(3)) {
        printf("Error: neopixel_begin() failed.\n");
        while (1) tight_loop_contents();
    }
    
    neotrellis_rainbow_startup(); 
    sleep_ms(200);

    // Init Keypad (Now using the safe slow-init method)
    if (!neotrellis_keypad_init()) {
        printf("Error: Keypad Init Failed.\n");
    } else {
        printf("Keypad Init OK.\n");
    }

    sleep_ms(200);
    neotrellis_clear_fifo(); 

    printf("=== Ready! Press Buttons... ===\n");

    while (1) {
        int idx = -1;
        int edge = -1;
        
        if (neotrellis_poll_buttons(&idx, &edge)) {
            float freq = get_note_for_index(idx);
            
            if (edge == SEESAW_KEYPAD_EDGE_RISING) {
                printf("Key %d PRESSED (%.2f Hz)\n", idx, freq);
                audio_play_note(freq);
            }
            else if (edge == SEESAW_KEYPAD_EDGE_FALLING) {
                printf("Key %d RELEASED\n", idx);
                audio_stop();
            }
        }
        
        sleep_ms(10); 
    }
>>>>>>> b254e24f7baaa6ee361e9fabb7ffe31b75a24716
    return 0;
}
