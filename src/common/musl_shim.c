#include <fcntl.h>       // for off_t, fcntl
#include <stdio.h>       // for fprintf, fgetc, stderr, size_t, FILE
#include <stdlib.h>      // for abort
#include <sys/mman.h>    // for mmap
#include <sys/stat.h>    // for fstat, stat
#include "bits/fcntl.h"  // for F_GETFL

/**
 * Shim to create missing function calls in the ingenic libimp library
 */

#define DEBUG 0  // Set this to 1 to enable debug output or 0 to disable

#if DEBUG
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...) (void)0
#endif

void __pthread_register_cancel(void *buf) {
    fprintf(stderr, "[WARNING] Called __pthread_register_cancel. This is a shim and does nothing.\n");
}

void __pthread_unregister_cancel(void *buf) {
    fprintf(stderr, "[WARNING] Called __pthread_unregister_cancel. This is a shim and does nothing.\n");
}

void __assert(const char *msg, const char *file, int line) {
    fprintf(stderr, "Assertion failed: %s (%s: %d)\n", msg, file, line);
    abort();
}

int __fgetc_unlocked(FILE *__stream) {
    return fgetc(__stream);
}

// GCC version check for version 13
#if defined(__GNUC__) && (__GNUC__ == 13)
// Use mmap for GCC 13
extern void *mmap64(void *__addr, size_t __len, int __prot, int __flags, int __fd, off_t __offset); // Adjusted prototype
#else
// Use mmap64 for other GCC versions or compilers
extern void *mmap64(void *__addr, size_t __len, int __prot, int __flags, int __fd, off_t __offset_high, off_t __offset_low);
#endif

void * mmap(void *__addr, size_t __len, int __prot, int __flags, int __fd, off_t __offset) {
    void* ret_val;
    struct stat file_stat;

    // Fetching file status
    if (fstat(__fd, &file_stat) == 0) {
        DEBUG_PRINT("File descriptor %d has size %lld bytes.\n", __fd, (long long)file_stat.st_size);
    } else {
        DEBUG_PRINT("Failed to get file size for descriptor %d. Error: %s\n", __fd, strerror(errno));
    }

    // Fetching file mode
    int file_flags = fcntl(__fd, F_GETFL);
    if (file_flags != -1) {
        DEBUG_PRINT("File descriptor %d opened with flags: %d\n", __fd, file_flags);
    } else {
        DEBUG_PRINT("Failed to get open flags for descriptor %d. Error: %s\n", __fd, strerror(errno));
    }

    // Adjustments based on identified conditions
    if (file_stat.st_size == 1 && __len == 20480 && __offset > 1024*1024*1024) {
        DEBUG_PRINT("Adjusting mmap call based on identified conditions.\n");
        __offset = 0;  // Resetting the offset to 0
    }

    // Logging mmap call details
    DEBUG_PRINT("mmap called with: addr=%p, len=%zu, prot=%d, flags=%d, fd=%d, offset=%lld\n", __addr, __len, __prot, __flags, __fd, __offset);

    // GCC version check for version 13
    #if defined(__GNUC__) && (__GNUC__ == 13)
    // Use mmap for GCC 13
    ret_val = mmap(__addr, __len, __prot, __flags, __fd, __offset);
    #else
    // Use mmap64 for other GCC versions or compilers
    ret_val = mmap64(__addr, __len, __prot, __flags, __fd, __offset, 0); // Adjust according to your original mmap64 call
    #endif

    if (ret_val == (void *)-1) {
        DEBUG_PRINT("mmap64 failed with error: %s\n", strerror(errno));
    } else {
        DEBUG_PRINT("mmap64 returned: %p\n", ret_val);
    }

    return ret_val;
}
