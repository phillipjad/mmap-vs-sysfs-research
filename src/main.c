#include "logger.h"
#include "mmap_control.h"
#include "pwm_io_logic.h"
#include "servo_controller.h"
#include <unistd.h>

#define SERVO_CHIP (2U)
#define SERVO_CHANNEL ('b')
#define PWMSS_INSTANCE_2 (2U)       /* PWMSS2 → P8_13 (same pin as sysfs path) */
#define SERVO_PERIOD_NS (20000000U) /* 50 Hz */
#define SERVO_0_NS (1000000U)       /* 1.0 ms — 0 deg position */
#define SERVO_CENTER_NS (1500000U)  /* 1.5 ms — center position */
#define SERVO_180_NS (2000000U)     /* 2.0 ms — 180 deg position */

static epwm_mmap_handle_t *mmap_pwm_setup(void) {
	epwm_mmap_handle_t *handle = epwm_mmap_init(PWMSS_INSTANCE_2, EPWM_CHANNEL_B);
	epwm_mmap_set_period_ns(handle, SERVO_PERIOD_NS);
	epwm_mmap_set_duty_ns(handle, SERVO_0_NS);
	epwm_mmap_enable(handle, true);
	LOG("PWM MMAP Configured");
	return handle;
}

static void shutdown_pwm(epwm_mmap_handle_t *pwm) {
	// servo module needs to be rewritten to use mmap
	// servo_shutdown();
	epwm_mmap_close(pwm);
}

int main(void) {
	servo_init(SERVO_CHIP, SERVO_CHANNEL);
	unexport_pwm_channel(SERVO_CHIP, 1U);

	epwm_mmap_handle_t *pwm = mmap_pwm_setup();
	LOG("0");
	(void)sleep(2U);
	LOG("90");
	epwm_mmap_set_duty_ns(pwm, SERVO_CENTER_NS);
	(void)sleep(2U);
	LOG("180");
	epwm_mmap_set_duty_ns(pwm, SERVO_180_NS);
	(void)sleep(2U);

	shutdown_pwm(pwm);
	return 0;
}
