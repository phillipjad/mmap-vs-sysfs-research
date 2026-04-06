#ifndef SERVO_CONTROL_H
#define SERVO_CONTROL_H

#include "project_types.h"

/**
 * @brief Function to initialize servo
 *
 * @param[in] servo_chip PWM chip
 * @param[in] servo_channel PWM channel
 * @param[in] use_sysfs flag for implementation type
 * @return void* Returns NULL
 */
void servo_init(uint8_t servo_chip, char servo_channel, bool use_sysfs);

/**
 * @brief Function to raise servo
 *
 * @return void* Returns NULL
 */
void servo_raise(void);

/**
 * @brief Function to lower servo
 *
 * @return void* Returns NULL
 */
void servo_lower(void);

/**
 * @brief Function to shutdown servo
 *
 * @return void* Returns NULL
 */
void servo_shutdown(void);

#endif /* SERVO_CONTROL_H */
