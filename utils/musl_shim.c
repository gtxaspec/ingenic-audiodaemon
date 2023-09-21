#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>     // for sysconf
#include <fcntl.h>      // for fcntl
#include <stdio.h>      // for perror
/**

 * shim to create missing function call in ingenic library

 */

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


extern void *mmap64(void *addr, size_t length, int prot, int flags, int fd, off_t offset);

#ifndef MMAP_SHIM_DEFINED
#define MMAP_SHIM_DEFINED

void *mmap(void *__addr, size_t __len, int __prot, int __flags, int __fd, off_t __offset) {
    void *result;

    printf("mmap called with arguments:\n");
    printf("__addr: %p\n", __addr);
    printf("__len: %zu\n", __len);
    printf("__prot: %d\n", __prot);
    printf("__flags: %d\n", __flags);
    printf("__fd: %d\n", __fd);
    printf("__offset: %lld\n", (long long)__offset);

    // Identify what the file descriptor points to
    char path[256], target[256];
    snprintf(path, sizeof(path), "/proc/self/fd/%d", __fd);
    ssize_t len = readlink(path, target, sizeof(target)-1);
    if (len != -1) {
        target[len] = '\0';
        printf("File descriptor %d points to: %s\n", __fd, target);
    } else {
        perror("readlink");
    }


    // Ensure the offset is page-aligned.
    off_t aligned_offset = __offset & ~(sysconf(_SC_PAGE_SIZE) - 1);
    if (aligned_offset != __offset) {
        printf("Adjusted offset from %lld to %lld for page alignment.\n", (long long)__offset, (long long)aligned_offset);
        __offset = aligned_offset;
    }

    // Check if the file descriptor is valid and open for the necessary mode.
    int fd_flags = fcntl(__fd, F_GETFL);
    if (fd_flags == -1) {
        perror("fd is not valid");
        return MAP_FAILED;
    }
    if ((__prot & PROT_WRITE) && !(fd_flags & O_RDWR) && !(fd_flags & O_WRONLY)) {
        fprintf(stderr, "fd is not opened for writing but PROT_WRITE is requested.\n");
        return MAP_FAILED;
    }

    // Check the size of the file associated with the file descriptor.
    struct stat st;
    if (fstat(__fd, &st) == -1) {
        perror("Error getting file size");
        return MAP_FAILED;
    }

    if (__offset + __len > st.st_size) {
        fprintf(stderr, "Trying to map beyond the end of the file. File size: %lld\n", (long long)st.st_size);
        return MAP_FAILED;
    }

    asm volatile(
        "addiu $sp, $sp, -32\n\t"
        "sw $ra, 28($sp)\n\t"
        "sw $gp, 24($sp)\n\t"
        "sw $a0, 16($sp)\n\t"
        "sw $a1, 20($sp)\n\t"

        "lw $a0, 16($sp)\n\t"
        "lw $a1, 20($sp)\n\t"
        "sw %6, 0($sp)\n\t"

        "lw $t9, %%got(mmap64)($gp)\n\t"
        "jalr $t9\n\t"
        "move %0, $v0\n\t"

        "lw $ra, 28($sp)\n\t"
        "lw $gp, 24($sp)\n\t"
        "addiu $sp, $sp, 32\n\t"
        : "=r"(result)
        : "r"(__addr), "r"(__len), "r"(__prot), "r"(__flags), "r"(__fd), "r"(__offset)
        : "$t9"
    );

    // Add error handling.
    if (result == MAP_FAILED) {
        perror("mmap64 error");
        fprintf(stderr, "Detailed error: %s\n", strerror(errno));
    }

    // Print the return value
    printf("mmap64 returned: %p\n", result);

    return result;
}


#endif
