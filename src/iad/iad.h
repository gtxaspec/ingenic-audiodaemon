// iad.h

#ifndef IAD_H
#define IAD_H

#include <pthread.h>
#include "audio/output.h"
#include "audio/input.h"
#include "network/network.h"
#include "utils/utils.h"
#include "utils/cmdline.h"
#include "version.h"
#include "config.h"

// Function declarations
void perform_cleanup(void);
void handle_sigint(int sig);

// Constants (example)

#endif // IAD_H
