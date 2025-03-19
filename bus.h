#ifndef BUS_H
#define BUS_H

#include <stdint.h>

int read_from_bus(uint16_t address);
void write_to_bus(uint16_t address, int value);

#endif
