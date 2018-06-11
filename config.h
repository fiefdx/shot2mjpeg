#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXBUF 1024
#define DELIM "="

struct config {
    int port;
    int quality;
};

struct config get_config(char *filename);
