#include "gpio.h"
#include <stdio.h>

#define NUM_GPIO_PINS 16

int gpio_state[NUM_GPIO_PINS];

void gpio_write(int pin, int value) {
    if (pin < NUM_GPIO_PINS) {
        gpio_state[pin] = value;
        printf("GPIO[%d] = %d\n", pin, value);
    } else {
        printf("Erreur : GPIO %d hors limites\n", pin);
    }
}

int gpio_read(int pin) {
    if (pin < NUM_GPIO_PINS) {
        return gpio_state[pin];
    } else {
        printf("Erreur : GPIO %d hors limites\n", pin);
        return 0;
    }
}
