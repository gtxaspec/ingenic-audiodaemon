#include <stdio.h>
#include "logging.h"

void handle_audio_error(const char *msg) {
    fprintf(stderr, "[ERROR] %s\n", msg);
}
