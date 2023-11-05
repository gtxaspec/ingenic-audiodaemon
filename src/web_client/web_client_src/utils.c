#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "utils.h"
#include "config.h"
#include "output.h"
#include "input.h"

#define PID_FILE "/var/run/web_client.pid"

/**
 * @brief Remove PID file.
 *
 * This function removes the PID file when called, typically during cleanup.
 */
void remove_pid_file() {
    unlink(PID_FILE);
}

/**
 * @brief Signal handler for SIGINT.
 *
 * This function handles the SIGINT signal (typically sent from the
 * command line via CTRL+C). It ensures that the daemon exits gracefully.
 *
 * @param sig Signal number (expected to be SIGINT).
 */
void handle_sigint(int sig) {
    printf("Caught signal %d. Exiting gracefully...\n", sig);
    remove_pid_file();
    signal(sig, SIG_DFL);
    raise(sig);
    exit(0);
}

/**
 * @brief Set up signal handling.
 *
 * This function sets up signal handlers for various signals the program might receive.
 */
void setup_signal_handling() {
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

/**
 * @brief Daemonize the process.
 *
 * This function forks the current process to create a daemon. The parent process exits, and the child process continues.
 * The child process becomes the session leader, and it detaches from the controlling terminal.
 */
void daemonize() {
    if (is_already_running()) {
        exit(1);
    }

    printf("Starting the program in the background as a daemon...\n");

    pid_t pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    if (setsid() < 0) {
        exit(EXIT_FAILURE);
    }

    signal(SIGCHLD, SIG_IGN);

    chdir("/");

    umask(0);

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    open("/dev/null", O_RDWR);
    dup(0);
    dup(0);
}

/**
 * @brief Check if an instance is already running.
 *
 * This function checks if another instance of the program is already running by attempting to acquire a lock on the PID file.
 *
 * @return int Returns 1 if another instance is running, 0 otherwise.
 */
int is_already_running() {
    int fd = open(PID_FILE, O_RDWR | O_CREAT, 0666);
    char pid_str[20];

    if (fd < 0) {
        perror("Failed to open PID file");
        exit(1);
    }

    if (lockf(fd, F_TLOCK, 0) < 0) {
        if (errno == EACCES || errno == EAGAIN) {
            ssize_t bytes_read = read(fd, pid_str, sizeof(pid_str) - 1);
            if (bytes_read > 0) {
                pid_str[bytes_read] = '\0';
                fprintf(stderr, "Another instance is already running with PID: %s", pid_str);
            } else {
                fprintf(stderr, "Another instance is already running, but couldn't read its PID.");
            }
            close(fd);
            return 1;
        }
        perror("Failed to lock PID file");
        exit(1);
    }

    ftruncate(fd, 0);
    sprintf(pid_str, "%ld\n", (long)getpid());
    write(fd, pid_str, strlen(pid_str));

    atexit(remove_pid_file);

    return 0;
}
