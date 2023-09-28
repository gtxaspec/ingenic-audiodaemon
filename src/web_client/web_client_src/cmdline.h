#ifndef CMDLINE_H
#define CMDLINE_H

void print_usage(char *program_name);
void parse_cmdline_args(int argc, char *argv[], char **ip_address, int *port, int *debug, int *silent);

#endif // CMDLINE_H
