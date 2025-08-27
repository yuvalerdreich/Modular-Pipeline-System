#include "plugin_common.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

const char* plugin_transform(const char* input) {
    if (!input) {
        return NULL;
    }

    int length_of_input = strlen(input);
    if (length_of_input == 0) {
        return strdup(input);
    }

    int length_after_transform = length_of_input + (length_of_input - 1);
    char* result_of_transform = malloc(length_after_transform + 1);
    if (!result_of_transform) {
        return NULL;
    }

    int result_idx = 0;
    for (int i = 0; i < length_of_input; i++) {
        result_of_transform[result_idx++] = input[i];
        if (i < length_of_input - 1) { 
            result_of_transform[result_idx++] = ' ';
        }
    }
    result_of_transform[length_after_transform] = '\0';
    
    return result_of_transform; 
}

const char* plugin_init(int queue_size) {
    return common_plugin_init(plugin_transform, "expander", queue_size);
}

const char* plugin_get_name(void) {
    return "expander";
}