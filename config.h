#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "log.h"

#define MAXBUF 1024
#define DELIM "="

struct config {
    int port;
    int quality;
    int fps;
    char log_level_key[MAXBUF];
    int log_level;
};

struct config get_config(char *filename);
