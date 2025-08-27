#include "monitor.h"

int monitor_init(monitor_t* monitor) {
    if (!monitor) {
        return -1;
    }
    
    int mutex_result = pthread_mutex_init(&monitor->mutex, NULL);
    if (mutex_result != 0) {
        return -1;
    }
    
    int condition_result = pthread_cond_init(&monitor->condition, NULL);
    if (condition_result != 0) {
        pthread_mutex_destroy(&monitor->mutex);
        return -1;
    }
    
    monitor->signaled = 0;
    
    return 0;
}
 
void monitor_destroy(monitor_t* monitor) {
    if (!monitor) {
        return;
    }
    
    pthread_cond_destroy(&monitor->condition);
    pthread_mutex_destroy(&monitor->mutex);
}

void monitor_signal(monitor_t* monitor) {
    if (!monitor) {
        return;
    }
    
    pthread_mutex_lock(&monitor->mutex);
    monitor->signaled = 1;
    pthread_cond_broadcast(&monitor->condition);
    pthread_mutex_unlock(&monitor->mutex);
}

void monitor_reset(monitor_t* monitor) {
    if (!monitor) {
        return;
    }
    
    pthread_mutex_lock(&monitor->mutex);
    monitor->signaled = 0;
    pthread_mutex_unlock(&monitor->mutex);
}

int monitor_wait(monitor_t* monitor) {
    if (!monitor) {
        return -1;
    }
    
    pthread_mutex_lock(&monitor->mutex);
    
    while (!monitor->signaled) {
        int wait_result = pthread_cond_wait(&monitor->condition, &monitor->mutex);
        if (wait_result != 0) {
            pthread_mutex_unlock(&monitor->mutex);
            return -1;
        }
    }
    
    pthread_mutex_unlock(&monitor->mutex);
    return 0;
}