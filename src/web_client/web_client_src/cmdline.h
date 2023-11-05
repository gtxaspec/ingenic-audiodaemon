#ifndef CMDLINE_H
#define CMDLINE_H

void print_usage(char *program_name);

// This structure will hold the command line options
typedef struct {
    char *ip_address;
    int port;
    int debug;
    int silent;
    int daemonize;
} CmdOptions;

// Function to parse command line arguments
int parse_cmdline(int argc, char *argv[], CmdOptions *options);

#endif // CMDLINE_H
