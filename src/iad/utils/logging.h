#ifndef LOGGING_H
#define LOGGING_H

// This header provides an interface for audio error logging. It allows for logging with or without a tag.
// Additionally, it offers a flexible macro to choose the correct logging function based on the number of arguments provided.

/**
 * @brief Logs an audio error with a given tag.
 *
 * This function prints an error message to stderr with the provided tag.
 * If errno is set, the corresponding system error string is appended.
 *
 * @param tag Tag to prepend to the error message.
 * @param msg Error message format string.
 * @param ... Variable number of arguments for the format string.
 */
void handle_audio_error_with_tag(const char *tag, const char *msg, ...);

/**
 * @brief Logs an audio error without a tag.
 *
 * This function prints an error message to stderr. If errno is set,
 * the corresponding system error string is appended.
 *
 * @param msg Error message format string.
 * @param ... Variable number of arguments for the format string.
 */
void handle_audio_error_without_tag(const char *msg, ...);

// Macro definitions to provide flexibility in calling the error handling functions

// Calls the error handling function without a tag.
#define HANDLE_AUDIO_ERROR_1(ARG1) handle_audio_error_without_tag(ARG1)

// Calls the error handling function with a tag.
#define HANDLE_AUDIO_ERROR_2(ARG1, ARG2) handle_audio_error_with_tag(ARG1, ARG2)

// Utility macro to choose the third argument from its argument list.
#define GET_3RD_ARG(arg1, arg2, arg3, ...) arg3

// Chooses the correct error handling function based on the number of provided arguments.
#define HANDLE_AUDIO_ERROR_MACRO_CHOOSER(...) GET_3RD_ARG(__VA_ARGS__, HANDLE_AUDIO_ERROR_2, HANDLE_AUDIO_ERROR_1)

// Main macro to be used for error logging. Decides whether to include a tag based on argument count.
#define handle_audio_error(...) HANDLE_AUDIO_ERROR_MACRO_CHOOSER(__VA_ARGS__)(__VA_ARGS__)

#endif // LOGGING_H
