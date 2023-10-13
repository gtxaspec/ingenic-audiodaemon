#ifndef CMDLINE_H
#define CMDLINE_H

// This structure will hold the command line options
typedef struct {
    char *config_file_path;
    int disable_ai;
    int disable_ao;
    int daemonize;
} CmdOptions;

// Function to parse command line arguments
int parse_cmdline(int argc, char *argv[], CmdOptions *options);

#endif // CMDLINE_H
