#ifndef MMAP_CONTROL_H
#define MMAP_CONTROL_H

#include "project_types.h"

/* mmap Types */
#define MMAP_IN ("In")
#define MMAP_OUT ("Out")

void mmap_map_init(void);

void mmap_map_close(void);

void mmap_set(uint8_t pin, bool value);

uint8_t mmap_read(uint8_t pin);

void mmap_clear(uint8_t pin);

void mmap_set_direction(uint8_t pin, const char *direction);

void mmap_set_direction_out(uint8_t pin);

void mmap_set_direction_in(uint8_t pin);

#endif /* MMAP_CONTROL_H */
