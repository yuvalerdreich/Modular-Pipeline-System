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
    
    char* result_of_transform = malloc(length_of_input + 1);

    if (!result_of_transform) {
        return NULL;
    }
    
    result_of_transform[0] = input[length_of_input - 1];

    for (int i = 1; i < length_of_input; i++) {
        result_of_transform[i] = input[i - 1];
    }

    result_of_transform[length_of_input] = '\0';
    
    return result_of_transform;
}

const char* plugin_init(int queue_size) {
    return common_plugin_init(plugin_transform, "rotator", queue_size);
}

const char* plugin_get_name(void) {
    return "rotator";
}