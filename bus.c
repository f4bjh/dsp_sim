#include "bus.h"
#include "gpio.h"
#include <stdio.h>

#define MEM_SIZE 1024
#define GPIO_BASE 0x8000 // Adresse de base des GPIO

int memory[MEM_SIZE];

int read_from_bus(uint16_t address) {
    if (address < MEM_SIZE) {
        return memory[address]; // Accès mémoire
    } else if (address >= GPIO_BASE) {
        return gpio_read(address - GPIO_BASE); // Accès GPIO
    } else {
        printf("Erreur : Adresse non valide 0x%X\n", address);
        return 0;
    }
}

void write_to_bus(uint16_t address, int value) {
    if (address < MEM_SIZE) {
        memory[address] = value; // Accès mémoire
    } else if (address >= GPIO_BASE) {
        gpio_write(address - GPIO_BASE, value); // Accès GPIO
    } else {
        printf("Erreur : Adresse non valide 0x%X\n", address);
    }
}
