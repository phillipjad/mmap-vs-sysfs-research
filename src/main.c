#include "mmap_control.h"
#include "servo_controller.h"

#define SERVO_CHIP (2U)
#define SERVO_CHANNEL ('b')
#define PWMSS_INSTANCE (2U) /* PWMSS2 → P8_13 (same pin as sysfs path) */
#define SERVO_PERIOD_NS (20000000U) /* 50 Hz */
#define SERVO_CENTER_NS (1500000U)  /* 1.5 ms — center position */

#ifdef NDEBUG
static epwm_mmap_handle_t *mmap_pwm_setup(void) {
  epwm_mmap_handle_t *handle = epwm_mmap_init(PWMSS_INSTANCE, EPWM_CHANNEL_B);
  epwm_mmap_set_period_ns(handle, SERVO_PERIOD_NS);
  epwm_mmap_set_duty_ns(handle, SERVO_CENTER_NS);
  epwm_mmap_enable(handle, true);
  return handle;
}
#endif /* NDEBUG */

int main(void) {
#ifdef NDEBUG
  servo_init(SERVO_CHIP, SERVO_CHANNEL);

  epwm_mmap_handle_t *pwm = mmap_pwm_setup();
  epwm_mmap_close(pwm);
#endif /* NDEBUG */

  return 0;
}
