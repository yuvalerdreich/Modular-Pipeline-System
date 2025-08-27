#include "plugin_common.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

const char* plugin_transform(const char* input) {
    if (!input) {
        return NULL;
    }

    usleep(100000);
    printf("[typewriter] ");
    fflush(stdout);
    
    for (int i = 0; input[i] != '\0'; i++) {
        printf("%c", input[i]);
        fflush(stdout);
        usleep(100000);
    }

    printf("\n");
    fflush(stdout);
    
    return strdup(input);
}

const char* plugin_init(int queue_size) {
    return common_plugin_init(plugin_transform, "typewriter", queue_size);
}

const char* plugin_get_name(void) {
    return "typewriter";
}