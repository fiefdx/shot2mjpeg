#include "config.h"

struct config get_config(char *filename) {
    struct config result;
    FILE *file = fopen(filename, "r");
    if (file != NULL) {
        char line[MAXBUF];
        int i = 0;
        while(fgets(line, sizeof(line), file) != NULL) {
            char *v_str, k_str[20];
            int v_length = 0, k_length = 0;
            v_str = strstr((char *)line, DELIM);
            v_str = v_str + strlen(DELIM);
            v_length = strlen(v_str);
            k_length = strlen(line) - strlen(v_str) - strlen(DELIM);
            memcpy(k_str, line, k_length);
            k_str[k_length] = 0;
            v_str[strcspn(v_str, "\r\n")] = 0;
            if (strcmp(k_str, "port") == 0) {
                result.port = (int) strtol(v_str, (char **)NULL, 10);
            } else if (strcmp(k_str, "quality") == 0) {
                result.quality = (int) strtol(v_str, (char **)NULL, 10);
            } else if (strcmp(k_str, "fps") == 0) {
                result.fps = (int) strtol(v_str, (char **)NULL, 10);
            } else if (strcmp(k_str, "log_level") == 0) {
                memcpy(result.log_level_key, v_str, strlen(v_str));
                if (strcmp(v_str, "fatal") == 0) { // LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL 
                    result.log_level = LOG_FATAL;
                } else if (strcmp(v_str, "error") == 0) {
                    result.log_level = LOG_ERROR;
                } else if (strcmp(v_str, "warn") == 0) {
                    result.log_level = LOG_WARN;
                } else if (strcmp(v_str, "info") == 0) {
                    result.log_level = LOG_INFO;
                } else if (strcmp(v_str, "debug") == 0) {
                    result.log_level = LOG_DEBUG;
                } else {
                    result.log_level = LOG_TRACE;
                }
            }
        }
    }
    fclose(file);
    return result;
}