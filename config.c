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
                // memcpy(result.port, v_str, strlen(v_str));
            } else if (strcmp(k_str, "quality") == 0) {
                result.quality = (int) strtol(v_str, (char **)NULL, 10);
                // memcpy(result.quality, v_str, strlen(v_str));
            }
        }
    }
    fclose(file);
    return result;
}