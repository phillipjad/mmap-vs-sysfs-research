#include "logger.h"
#include "mmap_control.h"
#include "pwm_io_logic.h"
#include "servo_controller.h"
#include <stdint.h>
#include <unistd.h>

#define SERVO_CHIP (2U)
#define SERVO_CHANNEL ('b')
#define PWMSS_INSTANCE_2 (2U)       /* PWMSS2 → P8_13 (same pin as sysfs path) */
#define SERVO_PERIOD_NS (20000000U) /* 50 Hz */
#define SERVO_0_NS (1000000U)       /* 1.0 ms — 0 deg position */
#define SERVO_CENTER_NS (1500000U)  /* 1.5 ms — center position */
#define SERVO_180_NS (2000000U)     /* 2.0 ms — 180 deg position */
#define TEST_SAMPLES (100)

// Test results for init and degrees
static float64_t init_latencies[TEST_SAMPLES];
static float64_t degrees_0_latencies[TEST_SAMPLES];
static float64_t degrees_180_latencies[TEST_SAMPLES];
struct timespec start_time = { 0 };
struct timespec end_time = { 0 };

/*------------------------
 * Function: time_taken
 *------------------------*/
static float64_t time_taken(struct timespec *start, struct timespec *end) {
	time_t seconds = end->tv_sec - start->tv_sec;
	int64_t nanoseconds = (end->tv_nsec - start->tv_nsec) / (int64_t)SEC_TO_NSEC;
	float64_t seconds_as_float = (float64_t)seconds;
	float64_t nseconds_as_float = (float64_t)nanoseconds;
	return seconds_as_float + nseconds_as_float;
}

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


	// Init latencies
	for (int i = 0; i < TEST_SAMPLES; ++i) {
		(void)clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);
		epwm_mmap_handle_t *pwm = mmap_pwm_setup();
		(void)clock_gettime(CLOCK_MONOTONIC_RAW, &end_time);
		init_latencies[i] = time_taken(&end_time, &start_time);
		shutdown_pwm(pwm);
	}

	// Setup again
	epwm_mmap_handle_t *pwm = mmap_pwm_setup();
	(void)sleep(2U);
	// Angle latencies test
	// 90 is starting
	// 0 is first test
	// back to 90
	// 180 is second test
	for (int i = 0; i < TEST_SAMPLES; ++i) {
		// Test 1
		LOG("90");
		epwm_mmap_set_duty_ns(pwm, SERVO_CENTER_NS);
		(void)sleep(2U);
		LOG("0");
		(void)clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);
		epwm_mmap_set_duty_ns(pwm, SERVO_0_NS);
		(void)clock_gettime(CLOCK_MONOTONIC_RAW, &end_time);
		degrees_0_latencies[i] = time_taken(&end_time, &start_time);

		// Test 2
		LOG("90");
		epwm_mmap_set_duty_ns(pwm, SERVO_CENTER_NS);
		(void)sleep(2U);
		LOG("180");
		(void)clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);
		epwm_mmap_set_duty_ns(pwm, SERVO_180_NS);
		(void)clock_gettime(CLOCK_MONOTONIC_RAW, &end_time);
		degrees_180_latencies[i] = time_taken(&end_time, &start_time);
	}

	shutdown_pwm(pwm);

	// Export latencies to file
	const char *filename = "mmap_latencies.csv";
	FILE *fp = fopen(filename, "w");
	if (fp == NULL) {
		LOG_AND_EXIT("Failed to create latencies export file");
	} else {
		(void)fprintf(fp, "Iteration, init_ns, 0deg_ns, 180deg_ns\n");
		for (int i = 0; i < TEST_SAMPLES; ++i) {
			(void)fprintf(fp, "%u, %.2f, %.2f, %.2f\n", i + 1, init_latencies[i], degrees_0_latencies[i],
			    degrees_180_latencies[i]);
		}
		(void)fclose(fp);
	}
	printf("Finished exporting %s", filename);
	return 0;
}
