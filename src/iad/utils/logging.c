#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include "logging.h"

/**
 * @brief Internal function to handle audio error logging.
 * 
 * This function prints an error message to stderr, and if errno is set, it appends the
 * corresponding error string.
 * 
 * @param tag Optional tag to prepend to the error message.
 * @param msg Error message format string.
 * @param args Variable argument list for the format string.
 */
static void handle_audio_error_internal(const char *tag, const char *msg, va_list args) {
    if (tag) {
        fprintf(stderr, "%s: ", tag);
    }

    vfprintf(stderr, msg, args);

    if (errno) {
        fprintf(stderr, ": %s\n", strerror(errno));
    } else {
        fprintf(stderr, "\n");
    }
}

void handle_audio_error_with_tag(const char *tag, const char *msg, ...) {
    va_list args;
    va_start(args, msg);
    handle_audio_error_internal(tag, msg, args);
    va_end(args);
}

void handle_audio_error_without_tag(const char *msg, ...) {
    va_list args;
    va_start(args, msg);
    handle_audio_error_internal(NULL, msg, args);
    va_end(args);
}
