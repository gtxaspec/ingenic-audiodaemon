#include "logging.h"
#include <stdio.h>

void handle_audio_error(const char *msg) {
    fprintf(stderr, "[ERROR] %s\n", msg);
}
