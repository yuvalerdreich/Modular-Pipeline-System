#include "plugin_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static plugin_context_t plugin_context = {0};

void* plugin_consumer_thread(void* arg) {
    plugin_context_t* context = (plugin_context_t*)arg;
    
    while (1) {
        char* item = consumer_producer_get(context->queue);

        if (!item) {
            break;
        }
        
        if (strcmp(item, "<END>") == 0) {
            if (context->next_place_work) {
                context->next_place_work(item);
            }
            
            free(item);
            break;
        }
        
        const char* processed = context->process_function(item);
        
        // Move to the next plugin if exists
        if (context->next_place_work && processed) {
            context->next_place_work(processed);
        }
        
        // Free processed string when it is different from original
        if (processed && processed != item) {
            free((void*)processed);
        }
        
        free(item);
    }
    
    consumer_producer_signal_finished(context->queue);
    context->finished = 1;
    
    return NULL;
}
// put in comment before submission
void log_error(plugin_context_t* context, const char* message) {
    fprintf(stderr, "[ERROR][%s] - %s\n", context->name, message);
}

void log_info(plugin_context_t* context, const char* message) {
    fprintf(stdout, "[INFO][%s] - %s\n", context->name, message);
}
// put in comment before submission

const char* common_plugin_init(const char* (*process_function)(const char*), const char* name, int queue_size) {
    if (!process_function || !name || queue_size <= 0) {
        return "Invalid parameters entered to plugin_init";
    }
    
    if (plugin_context.initialized) {
        return "Plugin already initialized";
    }
    
    // Initialize context
    plugin_context.name = name;
    plugin_context.process_function = process_function;
    plugin_context.next_place_work = NULL;
    plugin_context.finished = 0;
    
    plugin_context.queue = (consumer_producer_t*)malloc(sizeof(consumer_producer_t)); 

    if (!plugin_context.queue) {
        return "Failed to allocate memory for queue";
    }
    
    const char* queue_error = consumer_producer_init(plugin_context.queue, queue_size);

    if (queue_error) {
        free(plugin_context.queue);
        plugin_context.queue = NULL;
        return queue_error;
    }
    
    // Create consumer thread
    if (pthread_create(&plugin_context.consumer_thread, NULL, plugin_consumer_thread, &plugin_context) != 0) {
        consumer_producer_destroy(plugin_context.queue);
        free(plugin_context.queue);
        plugin_context.queue = NULL;
        return "Failed to create consumer thread";
    }
    
    plugin_context.initialized = 1;
    return NULL;
}

const char* plugin_fini(void) {
    if (!plugin_context.initialized) {
        return "Plugin not initialized";
    }
    
    // Wait for consumer thread to finish
    pthread_join(plugin_context.consumer_thread, NULL);
    
    if (plugin_context.queue) {
        consumer_producer_destroy(plugin_context.queue);
        free(plugin_context.queue);
        plugin_context.queue = NULL;
    }
    
    plugin_context.initialized = 0;

    return NULL;
}

const char* plugin_place_work(const char* str) {
    if (!plugin_context.initialized || !str) {
        return "Plugin not initialized or invalid string";
    }
    
    return consumer_producer_put(plugin_context.queue, str);
}

void plugin_attach(const char* (*next_place_work)(const char*)) {
    if (plugin_context.initialized) {
        plugin_context.next_place_work = next_place_work;
    }
}

const char* plugin_wait_finished(void) {
    if (!plugin_context.initialized) {
        return "Plugin not initialized";
    }
    
    if (consumer_producer_wait_finished(plugin_context.queue) != 0) {
        return "Failed to wait for finished signal";
    }
    
    return NULL;
}