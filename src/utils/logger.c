#include <limits.h>
#include <stdarg.h>
#include <string.h>

/* Local project includes after system libraries */
#include "logger.h"
#include "project_constants.h"

#define MAX_LOG_LEN (1000U)

/*--------------------------------------
 * Function: project_log
 *--------------------------------------*/
inline void project_log(FILE *stream, bool include_newline,
                        const char *filename, uint32_t line_no,
                        const char *format, ...) {
  /* TODO: Add log level logic */
  char output_buffer[MAX_LOG_LEN + 1U] = {0};
  /* Get current time */
  struct timespec curr_time = {0};
  int32_t result = clock_gettime(CLOCK_REALTIME, &curr_time);
  if (result != 0) {
    printf("Failed to get timestamp");
    exit(EXIT_FAILURE);
  }
  int64_t microseconds = curr_time.tv_nsec / NSEC_PER_USEC;
  int32_t used = snprintf(output_buffer, MAX_LOG_LEN, "%" PRIdMAX ".%.6" PRId64,
                          (intmax_t)curr_time.tv_sec, microseconds);
  const char *file_of_interest = NULL;
  if (strrchr(filename, '/') != NULL) {
    char *file_of_interest_with_slash = strrchr(filename, (int32_t)'/');
    /* Get rid of slash */
    ++file_of_interest_with_slash;

    /* Now assign */
    file_of_interest = file_of_interest_with_slash;
  } else {
    file_of_interest = filename;
  }
  used += snprintf(&output_buffer[used], (MAX_LOG_LEN - (size_t)used),
                   "[%s:%d]: ", file_of_interest, line_no);
  va_list args;
  va_start(args, format);
  used += vsnprintf(&output_buffer[used], (MAX_LOG_LEN - (size_t)used), format,
                    args);
  va_end(args);
  if (include_newline) {
    used += snprintf(&output_buffer[used], (MAX_LOG_LEN - (size_t)used), "\n");
  }

  /* Immediately flush after printing to prevent interleaved output */
  printf("%s", output_buffer);
  (void)fflush(stream);
}

/*--------------------------------------
 * Function: log_time_difference_ms
 *--------------------------------------*/
void log_time_difference_ms(struct timespec t1, struct timespec t2,
                            const char *action) {
  time_t t1_as_ms = 0L;
  t1_as_ms += t1.tv_sec * MSEC_PER_SEC;
  t1_as_ms += t1.tv_nsec / NSEC_PER_MSEC;

  time_t t2_as_ms = 0L;
  t2_as_ms += t2.tv_sec * MSEC_PER_SEC;
  t2_as_ms += t2.tv_nsec / NSEC_PER_MSEC;

  time_t time_diff = t1_as_ms - t2_as_ms;
  LOG("It took %" PRIdMAX "ms to %s", (intmax_t)time_diff, action);
}
