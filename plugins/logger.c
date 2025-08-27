#include "plugin_common.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

const char* plugin_transform(const char* input) {
    if (!input) {
        return NULL;
    }
    
    printf("[logger] %s\n", input);
    fflush(stdout);
    
    return strdup(input);
}

const char* plugin_init(int queue_size) {
    return common_plugin_init(plugin_transform, "logger", queue_size);
}

const char* plugin_get_name(void) {
    return "logger";
}