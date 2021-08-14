#include <stdint.h>
#include <stdio.h>

#include "heater.h" /* Generated by our specification */

uint8_t temperature;

static uint8_t readbyte(void)
{
    return 0;
}

void heaton (float temp) {

    printf("%f => on\n", temp);
}

void heatoff (float temp) {
    
    printf("%f => off\n", temp);
}

int main (int argc, char *argv[]) {
    for (;;) {
        temperature = readbyte(); /* Dummy function to read a byte from a sensor. */
        step();
    }
    return 0;
}
