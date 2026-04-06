#ifndef MMAP_CONTROL_H
#define MMAP_CONTROL_H

#include <stdint.h>

/**
 * @brief Opaque handle for a single ePWM channel within a PWMSS instance.
 *
 * Obtain via epwm_mmap_init(). Pass to all other functions.
 * Release via epwm_mmap_close().
 */
typedef struct epwm_mmap_handle epwm_mmap_handle_t;

/**
 * @brief ePWM output channel selector.
 */
typedef enum { EPWM_CHANNEL_A, EPWM_CHANNEL_B } epwm_channel_t;

/**
 * @brief Open /dev/mem, mmap the PWMSS and ePWM registers for the given
 *        PWMSS instance, and enable the ePWM clock. Calls LOG_AND_EXIT on
 *        any failure; never returns NULL.
 *
 * @param[in] pwmss_instance  PWMSS instance: 0, 1, or 2.
 * @param[in] channel         EPWM_CHANNEL_A or EPWM_CHANNEL_B.
 * @return Allocated handle — must be released with epwm_mmap_close().
 */
epwm_mmap_handle_t *epwm_mmap_init(uint8_t pwmss_instance, epwm_channel_t channel);

/**
 * @brief Disable the PWM output, unmap registers, close /dev/mem, and free
 *        the handle.
 *
 * @param[in] handle  Handle returned by epwm_mmap_init(). Must not be NULL.
 */
void epwm_mmap_close(epwm_mmap_handle_t *handle);

/**
 * @brief Program the PWM period. Automatically selects the prescaler
 *        combination with the highest resolution that fits TBPRD in 16 bits.
 *        Calls LOG_AND_EXIT if the period cannot be achieved.
 *
 * @param[in] handle     Handle returned by epwm_mmap_init().
 * @param[in] period_ns  Desired period in nanoseconds (e.g. 20000000 for 50
 * Hz).
 */
void epwm_mmap_set_period_ns(epwm_mmap_handle_t *handle, uint32_t period_ns);

/**
 * @brief Program the duty cycle. Must be called after
 * epwm_mmap_set_period_ns(). duty_ns is clamped to [0, period_ns].
 *
 * @param[in] handle   Handle returned by epwm_mmap_init().
 * @param[in] duty_ns  Desired on-time in nanoseconds.
 */
void epwm_mmap_set_duty_ns(epwm_mmap_handle_t *handle, uint32_t duty_ns);

/**
 * @brief Enable or disable the PWM output.
 *
 * Enable:  clears AQCSFRC force bits, starts up-count mode.
 * Disable: applies AQCSFRC continuous-low force, freezes counter.
 *
 * @param[in] handle  Handle returned by epwm_mmap_init().
 * @param[in] enable  true to enable, false to disable.
 */
void epwm_mmap_enable(epwm_mmap_handle_t *handle, bool enable);

#endif /* MMAP_CONTROL_H */
