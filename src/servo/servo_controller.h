#ifndef SERVO_CONTROL_H
#define SERVO_CONTROL_H

#include "project_types.h"

/**
 * @brief Function to initialize servo
 *
 * @param[in] servo_chip PWM chip
 * @param[in] servo_channel PWM channel
 * @return void* Returns NULL
 */
void servo_init(uint8_t servo_chip, char servo_channel);

/**
 * @brief Set servo duty cycle in nanoseconds
 *
 * @param[in] duty_ns Pulse width in nanoseconds
 */
void servo_set_duty_ns(uint32_t duty_ns);

/**
 * @brief Function to shutdown servo
 *
 * @return void* Returns NULL
 */
void servo_shutdown(void);

#endif /* SERVO_CONTROL_H */
