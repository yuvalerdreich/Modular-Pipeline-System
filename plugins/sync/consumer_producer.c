#include "consumer_producer.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

const char* consumer_producer_init(consumer_producer_t* queue, int capacity) {
    if (!queue) {
        return "Queue pointer cannot be null";
    }
    if (capacity <= 0) {
        return "Queue capacity must be positive";
    }
    
    queue->items = (char**)malloc(capacity * sizeof(char*));

    if (!queue->items) {
        return "Failed to allocate memory for queue items";
    }
    
    queue->capacity = capacity;
    queue->count = 0;
    queue->head = 0;
    queue->tail = 0;
    queue->finished = 0;
    
    // Initialize monitors
    if (monitor_init(&queue->not_full_monitor) != 0) {
        free(queue->items);
        return "Failed to initialize not_full_monitor";
    }
    
    if (monitor_init(&queue->not_empty_monitor) != 0) {
        monitor_destroy(&queue->not_full_monitor);
        free(queue->items);
        return "Failed to initialize not_empty_monitor";
    }
    
    if (monitor_init(&queue->finished_monitor) != 0) {
        monitor_destroy(&queue->not_empty_monitor);
        monitor_destroy(&queue->not_full_monitor);
        free(queue->items);
        return "Failed to initialize finished_monitor";
    }
    
    // Initialize mutex
    if (pthread_mutex_init(&queue->mutex, NULL) != 0) {
        monitor_destroy(&queue->finished_monitor);
        monitor_destroy(&queue->not_empty_monitor);
        monitor_destroy(&queue->not_full_monitor);
        free(queue->items);
        return "Failed to initialize mutex";
    }
    
    // Initially queue is not full
    monitor_signal(&queue->not_full_monitor);
    
    return NULL;
}

void consumer_producer_destroy(consumer_producer_t* queue) {
    if (!queue) {
        return;
    }
    
    // Free remaining items
    if (queue->items) {
        for (int i = 0; i < queue->count; i++) {
            int index = (queue->head + i) % queue->capacity;
            if (queue->items[index]) {
                free(queue->items[index]);
            }
        }
        free(queue->items);
    }
    
    // Destroy mutex and monitors
    pthread_mutex_destroy(&queue->mutex);
    monitor_destroy(&queue->not_full_monitor);
    monitor_destroy(&queue->not_empty_monitor);
    monitor_destroy(&queue->finished_monitor);
}

const char* consumer_producer_put(consumer_producer_t* queue, const char* item) {
    if (!queue || !item) {
        return "Invalid queue or item";
    }
    
    // Check if queue is finished 
    pthread_mutex_lock(&queue->mutex);
    if (queue->finished) {
        pthread_mutex_unlock(&queue->mutex);
        return "Queue is finished, cannot accept more items";
    }
    pthread_mutex_unlock(&queue->mutex);
    
    // Wait until queue is not full
    while (1) {
        pthread_mutex_lock(&queue->mutex);
        
        // Check finished state again after acquiring lock
        if (queue->finished) {
            pthread_mutex_unlock(&queue->mutex);
            return "Queue is finished, cannot accept more items";
        }
        
        if (queue->count < queue->capacity) {
            // Queue has space, proceed with adding
            break;
        }
        pthread_mutex_unlock(&queue->mutex);
        
        // Queue is full, wait for space
        if (monitor_wait(&queue->not_full_monitor) != 0) {
            return "Failed to wait for not_full condition";
        }
    }
    
    // Create a copy of the string
    char* item_copy = strdup(item); // check if allowed
    if (!item_copy) {
        pthread_mutex_unlock(&queue->mutex);
        return "Failed to allocate memory for item";
    }
    
    // Add item to queue
    queue->items[queue->tail] = item_copy;
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count++;
    
    // If this was the first item, signal not_empty
    if (queue->count == 1) {
        monitor_signal(&queue->not_empty_monitor);
    }
    
    // If queue is now full, reset not_full_monitor
    if (queue->count == queue->capacity) {
        monitor_reset(&queue->not_full_monitor);
    }
    
    pthread_mutex_unlock(&queue->mutex);
    return NULL;
}

char* consumer_producer_get(consumer_producer_t* queue) {
    if (!queue) {
        return NULL;
    }
    
    // Wait until queue is not empty or finished
    while (1) {
        pthread_mutex_lock(&queue->mutex);
        
        if (queue->count > 0) {
            break;
        }
        
        // If queue is empty and finished, return NULL
        if (queue->finished) {
            pthread_mutex_unlock(&queue->mutex);
            return NULL;
        }
        
        pthread_mutex_unlock(&queue->mutex);
        
        // Queue is empty but not finished, wait for items
        if (monitor_wait(&queue->not_empty_monitor) != 0) {
            return NULL;
        }
    }
    
    char* item = queue->items[queue->head];
    queue->items[queue->head] = NULL;
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
    
    // Signal not_full if queue was full 
    if (queue->count == queue->capacity - 1) {
        monitor_signal(&queue->not_full_monitor);
    }
    
    // Reset not_empty_monitor if queue is now empty 
    if (queue->count == 0) {
        monitor_reset(&queue->not_empty_monitor);
    }
    
    pthread_mutex_unlock(&queue->mutex);
    return item;
}

void consumer_producer_signal_finished(consumer_producer_t* queue) {
    if (!queue) {
        return;
    }
    
    pthread_mutex_lock(&queue->mutex);
    queue->finished = 1;
    pthread_mutex_unlock(&queue->mutex);
    
    // Signal both monitors to wake up any waiting threads
    monitor_signal(&queue->finished_monitor);
    monitor_signal(&queue->not_empty_monitor);  // Wake up consumers waiting for items
}

int consumer_producer_wait_finished(consumer_producer_t* queue) {
    if (!queue) {
        return -1;
    }
    
    return monitor_wait(&queue->finished_monitor);
}