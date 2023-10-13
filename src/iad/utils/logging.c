#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include "logging.h"

/**
 * @brief Internal function to handle audio error logging.
 *
 * This function prints an error message to stderr. If errno is set, it appends the
 * corresponding system error string. Optionally, a tag can be prepended to the message.
 *
 * @param tag Optional tag to prepend to the error message. If NULL, no tag is printed.
 * @param msg Error message format string.
 * @param args Variable argument list corresponding to the format string.
 */
static void handle_audio_error_internal(const char *tag, const char *msg, va_list args) {
    // If a tag is provided, prepend it to the error message.
    if (tag) {
        fprintf(stderr, "%s: ", tag);
    }

    // Print the formatted error message
    vfprintf(stderr, msg, args);

    // If errno is set, append the corresponding system error string.
    if (errno) {
        fprintf(stderr, ": %s\n", strerror(errno));
    } else {
        fprintf(stderr, "\n");
    }
}

/**
 * @brief Logs an audio error with a given tag.
 *
 * This function prints an error message to stderr with the given tag.
 * If errno is set, it also appends the corresponding system error string.
 *
 * @param tag Tag to prepend to the error message.
 * @param msg Error message format string.
 * @param ... Variable number of arguments for the format string.
 */
void handle_audio_error_with_tag(const char *tag, const char *msg, ...) {
    va_list args;
    va_start(args, msg);
    handle_audio_error_internal(tag, msg, args);
    va_end(args);
}

/**
 * @brief Logs an audio error without a tag.
 *
 * This function prints an error message to stderr. If errno is set,
 * it also appends the corresponding system error string.
 *
 * @param msg Error message format string.
 * @param ... Variable number of arguments for the format string.
 */
void handle_audio_error_without_tag(const char *msg, ...) {
    va_list args;
    va_start(args, msg);
    handle_audio_error_internal(NULL, msg, args);
    va_end(args);
}
