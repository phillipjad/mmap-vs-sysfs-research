#ifndef PWM_IO_LOGIC_H
#define PWM_IO_LOGIC_H

#include "project_types.h"

void init_pwm_channel(uint8_t chip, uint8_t channel);

void export_pwm_channel(uint8_t chip, uint8_t channel);

void unexport_pwm_channel(uint8_t chip, uint8_t channel);

void set_pwm_period(uint8_t chip, uint8_t channel, uint32_t period_ns);

void set_pwm_duty_cycle(uint8_t chip, uint8_t channel, uint32_t duty_ns);

void enable_pwm(uint8_t chip, uint8_t channel, bool enable);

#endif /* PWM_IO_LOGIC_H */
