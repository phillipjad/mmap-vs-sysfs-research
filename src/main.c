#include "logger.h"
#include "project_constants.h"
#include "project_types.h"
#include "servo_controller.h"
#include <stdint.h>
#include <sys/stat.h>

#define SERVO_CHIP (2U)
#define SERVO_CHANNEL ('b')
#define SERVO_0_NS (500000U)       /* 1.0 ms — 0 deg position */
#define SERVO_CENTER_NS (1500000U) /* 1.5 ms — center position */
#define SERVO_180_NS (2500000U)    /* 2.0 ms — 180 deg position */
#define TEST_SAMPLES (100)
#define NSEC_PER_SEC (1000000000)

// Test results for init and degrees
static int64_t degrees_0_latencies[TEST_SAMPLES] = { 0 };
static int64_t degrees_180_latencies[TEST_SAMPLES] = { 0 };
struct timespec start_time = { 0 };
struct timespec end_time = { 0 };

/*------------------------
 * Function: time_taken
 *------------------------*/
static int64_t time_taken(struct timespec *start, struct timespec *end) {
	time_t seconds = end->tv_sec - start->tv_sec;
	int64_t seconds_as_long = (int64_t)seconds;
	int64_t seconds_as_ns = seconds_as_long * NSEC_PER_SEC;
	int64_t nanoseconds = end->tv_nsec - start->tv_nsec;
	nanoseconds += seconds_as_ns;
	return nanoseconds;
}


int main(void) {
	servo_init(SERVO_CHIP, SERVO_CHANNEL);

	// Angle latencies test
	// 90 is starting
	// 0 is first test
	// back to 90
	// 180 is second test
	struct timespec servo_movement_delay = { .tv_sec = 0, .tv_nsec = 500000000 };
	for (int i = 0; i < TEST_SAMPLES; ++i) {
		// Test 1
		LOG("Rotating to 90 degrees");
		servo_set_duty_ns(SERVO_CENTER_NS);
		(void)nanosleep(&servo_movement_delay, NULL);
		LOG("Rotating to 0 degrees");
		(void)clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);
		servo_set_duty_ns(SERVO_0_NS);
		(void)clock_gettime(CLOCK_MONOTONIC_RAW, &end_time);
		degrees_0_latencies[i] = time_taken(&start_time, &end_time);
		(void)nanosleep(&servo_movement_delay, NULL);

		// Test 2
		LOG("Rotating to 90 degrees");
		servo_set_duty_ns(SERVO_CENTER_NS);
		(void)nanosleep(&servo_movement_delay, NULL);
		LOG("Rotating to 180 degrees");
		(void)clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);
		servo_set_duty_ns(SERVO_180_NS);
		(void)clock_gettime(CLOCK_MONOTONIC_RAW, &end_time);
		degrees_180_latencies[i] = time_taken(&start_time, &end_time);
		(void)nanosleep(&servo_movement_delay, NULL);
	}

	/* Revert servo angle back to 0 degrees */
	LOG("Rotating to 0 degrees");
	servo_set_duty_ns(SERVO_0_NS);
	servo_shutdown();

	// Export latencies to file
	(void)mkdir("outputs", 0755);
	struct timespec wall_time = { 0 };
	(void)clock_gettime(CLOCK_REALTIME, &wall_time);
	struct tm *tm_info = localtime(&wall_time.tv_sec);
	char filename[128] = { 0 };
#ifdef USE_MMAP
	(void)mkdir("outputs/mmap", 0755);
	(void)strftime(filename, sizeof(filename), "outputs/mmap/mmap_latencies_%Y-%m-%d:%H:%M:%S.csv", tm_info);
#else
	(void)mkdir("outputs/sysfs", 0755);
	(void)strftime(filename, sizeof(filename), "outputs/sysfs/sysfs_latencies_%Y-%m-%d:%H:%M:%S.csv", tm_info);
#endif
	FILE *fp = fopen(filename, "w");
	if (fp == NULL) {
		LOG_AND_EXIT("Failed to create latencies export file");
	} else {
		(void)fprintf(fp, "Iteration, 0deg_ns, 180deg_ns\n");
		for (uint8_t ii = 0U; ii < TEST_SAMPLES; ++ii) {
			(void)fprintf(fp, "%u, %" PRId64 ", %" PRId64 "\n", (ii + 1), degrees_0_latencies[ii], degrees_180_latencies[ii]);
		}
		(void)fclose(fp);
	}
	LOG("Finished exporting %s", filename);
	return 0;
}
