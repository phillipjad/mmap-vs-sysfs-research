#include "servo_controller.h"

#include "logger.h"
#include "project_constants.h"
#include "project_types.h"
#include <pthread.h>

#ifdef USE_MMAP
#include "mmap_control.h"
#include "pwm_io_logic.h" /* sysfs needed for PM lifecycle even in mmap mode */
#endif
#ifdef USE_SYSFS
#include "pwm_io_logic.h"
#endif

/**
 * @brief This function ensures that the pin is muxed for pwm to prevent having
 * to manually run config-pin on the BBB
 *
 * @param[in] servo_chip The servo chip passed in by user input/config
 * @param[in] servo_channel The servo channel passed in by user input/config
 */
static void configure_pwm_pinmux(uint8_t servo_chip, char servo_channel) {
	const char *pin_name = NULL;
	if (servo_chip == 1) {
		if ((servo_channel == 'a') || (servo_channel == 'A')) {
			pin_name = "P9_14"; /* EHRPWM1A */
		} else if ((servo_channel == 'b') || (servo_channel == 'B')) {
			pin_name = "P9_16"; /* EHRPWM1B */
		} else {
			/* MISRA requires else */
		}
	} else if (servo_chip == 2) {
		if ((servo_channel == 'a') || (servo_channel == 'A')) {
			pin_name = "P8_19"; /* EHRPWM2A */
		} else if ((servo_channel == 'b') || (servo_channel == 'B')) {
			pin_name = "P8_13"; /* EHRPWM2B */
		} else {
			/* MISRA requires else */
		}
	} else {
		/* MISRA requires else */
	}

	if (pin_name == NULL) {
		LOG_AND_EXIT("ERROR: Could not resolve BBB pin for chip %d channel %c", servo_chip, servo_channel);
		return;
	}

	char path[MAX_FILENAME_LENGTH + 1U] = { 0 };
	(void)snprintf(path, MAX_FILENAME_LENGTH, "/sys/devices/platform/ocp/ocp:%s_pinmux/state", pin_name);

	FILE *pinmux_file = fopen(path, "w");
	if (pinmux_file == NULL) {
		LOG_AND_EXIT("ERROR: Failed to open pinmux state file: %s", path);
		return;
	}
	if (fprintf(pinmux_file, "pwm") < 0) {
		(void)fclose(pinmux_file);
		LOG_AND_EXIT("ERROR: Failed to write pwm to pinmux state file: %s", path);
		return;
	}
	(void)fclose(pinmux_file);
}

/* Servo Constants */
#define SERVO_PERIOD_NS (20000000U) /* 50 Hz */
#define SERVO_CENTER_NS (1500000U)  /* 1.5 ms — center position */

/* ============================================================================
 * USE_MMAP IMPLEMENTATION
 * Hybrid approach: sysfs keeps the PWMSS module clock alive (PM reference),
 * mmap provides direct register access for low-latency duty-cycle writes.
 * ============================================================================ */
#ifdef USE_MMAP

static epwm_mmap_handle_t *g_mmap_handle = NULL;
static uint8_t g_pwmchip = 0U;
static uint8_t g_sysfs_channel = 0U;

void servo_init(uint8_t servo_chip, char servo_channel) {
	configure_pwm_pinmux(servo_chip, servo_channel);

	/* --- sysfs init (acquires PM reference, enables PWMSS clocks) --- */
	if (servo_chip == 1) {
		g_pwmchip = 4U; /* EHRPWM1 → pwmchip4 */
	} else if (servo_chip == 2) {
		g_pwmchip = 7U; /* EHRPWM2 → pwmchip7 */
	} else {
		LOG_AND_EXIT("ERROR: Invalid servo chip %u (valid: 1, 2)", servo_chip);
		return;
	}

	if ((servo_channel == 'a') || (servo_channel == 'A')) {
		g_sysfs_channel = 0U;
	} else if ((servo_channel == 'b') || (servo_channel == 'B')) {
		g_sysfs_channel = 1U;
	} else {
		LOG_AND_EXIT("ERROR: Invalid servo channel %c (valid: a, A, b, B)", servo_channel);
		return;
	}

	init_pwm_channel(g_pwmchip, g_sysfs_channel);
	set_pwm_period(g_pwmchip, g_sysfs_channel, SERVO_PERIOD_NS);
	set_pwm_duty_cycle(g_pwmchip, g_sysfs_channel, SERVO_CENTER_NS);
	enable_pwm(g_pwmchip, g_sysfs_channel, true);
	LOG("Sysfs PWM exported (PM reference acquired)");

	/* --- mmap init (now safe — PWMSS clocks are enabled) --- */
	uint8_t pwmss_instance;
	if (servo_chip == 1) {
		pwmss_instance = 1U;
	} else {
		pwmss_instance = 2U;
	}

	epwm_channel_t channel;
	if ((servo_channel == 'a') || (servo_channel == 'A')) {
		channel = EPWM_CHANNEL_A;
	} else {
		channel = EPWM_CHANNEL_B;
	}

	g_mmap_handle = epwm_mmap_init(pwmss_instance, channel);
	epwm_mmap_set_period_ns(g_mmap_handle, SERVO_PERIOD_NS);
	epwm_mmap_set_duty_ns(g_mmap_handle, SERVO_CENTER_NS);
	epwm_mmap_enable(g_mmap_handle, true);
	LOG("Servo initialized (mmap mode, sysfs PM)");
}

void servo_set_duty_ns(uint32_t duty_ns) {
	if (g_mmap_handle != NULL) {
		epwm_mmap_set_duty_ns(g_mmap_handle, duty_ns);
	}
}

void servo_shutdown(void) {
	LOG("Shutting down servo");
	/* Close mmap first */
	if (g_mmap_handle != NULL) {
		epwm_mmap_close(g_mmap_handle);
		g_mmap_handle = NULL;
	}
	/* Release sysfs PM reference last */
	enable_pwm(g_pwmchip, g_sysfs_channel, false);
	unexport_pwm_channel(g_pwmchip, g_sysfs_channel);
}

#endif /* USE_MMAP */

/* ============================================================================
 * USE_SYSFS IMPLEMENTATION
 * ============================================================================ */
#ifdef USE_SYSFS

static uint8_t g_pwmchip = 0U;
static uint8_t g_channel = 0U;

void servo_init(uint8_t servo_chip, char servo_channel) {
	/* Configure pinmux to allow PWM access */
	configure_pwm_pinmux(servo_chip, servo_channel);

	/* Map chip number to sysfs pwmchip */
	if (servo_chip == 1) {
		g_pwmchip = 4U; /* EHRPWM1 maps to pwmchip4 */
	} else if (servo_chip == 2) {
		g_pwmchip = 7U; /* EHRPWM2 maps to pwmchip7 */
	} else {
		LOG_AND_EXIT("ERROR: Invalid servo chip %u (valid: 1, 2)", servo_chip);
		return;
	}

	/* Map channel letter to sysfs channel index */
	if ((servo_channel == 'a') || (servo_channel == 'A')) {
		g_channel = 0U;
	} else if ((servo_channel == 'b') || (servo_channel == 'B')) {
		g_channel = 1U;
	} else {
		LOG_AND_EXIT("ERROR: Invalid servo channel %c (valid: a, A, b, B)", servo_channel);
		return;
	}

	/* Initialize sysfs PWM */
	init_pwm_channel(g_pwmchip, g_channel);
	set_pwm_period(g_pwmchip, g_channel, SERVO_PERIOD_NS);
	set_pwm_duty_cycle(g_pwmchip, g_channel, SERVO_CENTER_NS);
	enable_pwm(g_pwmchip, g_channel, true);
	LOG("Servo initialized (sysfs mode)");
}

void servo_set_duty_ns(uint32_t duty_ns) {
	set_pwm_duty_cycle(g_pwmchip, g_channel, duty_ns);
}

void servo_shutdown(void) {
	LOG("Shutting down servo");
	enable_pwm(g_pwmchip, g_channel, false);
	unexport_pwm_channel(g_pwmchip, g_channel);
}

#endif /* USE_SYSFS */
