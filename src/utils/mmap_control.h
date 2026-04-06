#ifndef MMAP_CONTROL_H
#define MMAP_CONTROL_H

#include "project_types.h"

void pwm_mmap_init(uint8_t chip, uint8_t channel);

void mmap_map_close(void);

void mmap_set_pwm_period(uint32_t period_ns);

void mmap_set_duty_cycle(uint32_t duty_ns);

void mmap_enable_pwm(bool enable);

void mmap_unexport_pwm_channel(void);

#endif /* MMAP_CONTROL_H */
