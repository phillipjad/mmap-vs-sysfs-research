#ifndef PROJECT_TYPES_H
#define PROJECT_TYPES_H

#include <inttypes.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define STATUS_SUCCESS (0)
#define STATUS_FAIL (-1)

typedef double float64_t;

typedef struct {
	uint8_t servo_chip; /**< chip number for servo */
	char servo_channel; /**< channel number for servo */
} servo_t;

#endif /* PROJECT_TYPES_H */
