#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdlib.h>

/* Local project includes after system libraries */
#include "project_types.h" // IWYU pragma: keep | We are trying to pull stdint and stdbool from this intrinisically

/**
 * @brief printf-like logging macro
 */
#define LOG(format, ...) project_log(stdout, true, __FILE__, __LINE__, format, ##__VA_ARGS__)

#ifdef DEBUG
/**
 * @brief printf-like logging macro only for debug mode
 */
#define DEBUG_LOG(format, ...) project_log(stdout, true, __FILE__, __LINE__, format, ##__VA_ARGS__)
#else
/**
 * @brief disabled for release mode
 */
#define DEBUG_LOG(format, ...)
#endif

/**
 * @brief printf-like logging macro, with no newline
 */
#define LOG_WITHOUT_NEWLINE(format, ...) project_log(stdout, false, __FILE__, __LINE__, format, ##__VA_ARGS__)

/**
 * @brief Panics the message in \p format to stderr and then exits with a
 * non-zero status code
 */
#define LOG_AND_EXIT(format, ...)                                         \
	project_log(stderr, true, __FILE__, __LINE__, format, ##__VA_ARGS__); \
	exit(EXIT_FAILURE)

/**
 * @brief Logs the message in \p format alongside useful metadata such as the
 * filename and lineno
 *
 * @param[in] stream The output stream to write to (stdout/stderr)
 * @param[in] include_newline Determines if a newline should be included in the
 * log statement.
 * @param[in] filename The file that this function was called from
 * @param[in] line_no The line number where this function was called
 * @param[in] format LOG-like format string to use when printing
 * @param[in] ... Variadic arguments to print according to \p format
 */
void project_log(FILE *stream, bool include_newline, const char *filename, uint32_t line_no, const char *format, ...)
    /* GCC format attribute for printf-style format checking */
    __attribute__((format(printf, 5, 6)));

/**
 * @brief Logs the time difference between \p t1 and \p t2 to do \p action
 *
 * @param[in] t1 The minuend of the difference
 * @param[in] t2 The subtrahend of the difference
 * @param[in] action The action that time is being diffed from. Should be a
 * present-tense verb phrase
 */
void log_time_difference_ms(struct timespec t1, struct timespec t2, const char *action);

#endif /* LOGGER_H */
