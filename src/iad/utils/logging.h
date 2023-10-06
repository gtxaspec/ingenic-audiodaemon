#ifndef LOGGING_H
#define LOGGING_H

/*
#include <stdarg.h>
*/

void handle_audio_error_with_tag(const char *tag, const char *msg, ...);
void handle_audio_error_without_tag(const char *msg, ...);

#define HANDLE_AUDIO_ERROR_1(ARG1) handle_audio_error_without_tag(ARG1)
#define HANDLE_AUDIO_ERROR_2(ARG1, ARG2) handle_audio_error_with_tag(ARG1, ARG2)
#define GET_3RD_ARG(arg1, arg2, arg3, ...) arg3
#define HANDLE_AUDIO_ERROR_MACRO_CHOOSER(...) GET_3RD_ARG(__VA_ARGS__, HANDLE_AUDIO_ERROR_2, HANDLE_AUDIO_ERROR_1)
#define handle_audio_error(...) HANDLE_AUDIO_ERROR_MACRO_CHOOSER(__VA_ARGS__)(__VA_ARGS__)

#endif // LOGGING_H
