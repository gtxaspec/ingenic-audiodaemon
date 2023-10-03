#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "logging.h"
#include <stdarg.h>

void _handle_audio_error_with_tag(const char *tag, const char *msg, ...) {
    va_list args;

    fprintf(stderr, "%s: ", tag);

    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);

    if (errno) {
        fprintf(stderr, ": %s\n", strerror(errno));
    } else {
        fprintf(stderr, "\n");
    }
}

void _handle_audio_error_without_tag(const char *msg, ...) {
    va_list args;

    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);

    if (errno) {
        fprintf(stderr, ": %s\n", strerror(errno));
    } else {
        fprintf(stderr, "\n");
    }
}
