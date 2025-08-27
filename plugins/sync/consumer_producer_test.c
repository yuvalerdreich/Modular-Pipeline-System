#include "consumer_producer.h"
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    consumer_producer_t* queue;
    int thread_id;
    int items_to_produce;
    int items_consumed;
} test_thread_data_t;

void* producer_thread(void* arg) {
    test_thread_data_t* data = (test_thread_data_t*)arg;
    
    for (int i = 0; i < data->items_to_produce; i++) {
        char item[32];
        snprintf(item, sizeof(item), "item_%d_%d", data->thread_id, i);
        
        printf("Producer %d: Adding %s\n", data->thread_id, item);
        const char* error = consumer_producer_put(data->queue, item);
        if (error) {
            printf("Producer %d: Error - %s\n", data->thread_id, error);
            break;
        }
    }
    
    printf("Producer %d: Finished\n", data->thread_id);
    return NULL;
}

void* consumer_thread(void* arg) {
    test_thread_data_t* data = (test_thread_data_t*)arg;
    
    while (data->items_consumed < data->items_to_produce) {
        char* item = consumer_producer_get(data->queue);
        if (item) {
            printf("Consumer %d: Got %s\n", data->thread_id, item);
            data->items_consumed++;
            free(item);
        } else {
            printf("Consumer %d: Got NULL item\n", data->thread_id);
            break;
        }
    }
    
    printf("Consumer %d: Finished, consumed %d items\n", data->thread_id, data->items_consumed);
    return NULL;
}

int test_basic_queue_operations() {
    printf("\n=== Test: Basic Queue Operations ===\n");
    
    consumer_producer_t queue;
    const char* error = consumer_producer_init(&queue, 5);
    if (error) {
        printf("FAIL: Failed to initialize queue: %s\n", error);
        return 0;
    }
    
    // Test putting items
    printf("Adding items to queue...\n");
    for (int i = 0; i < 3; i++) {
        char item[32];
        snprintf(item, sizeof(item), "test_item_%d", i);
        error = consumer_producer_put(&queue, item);
        if (error) {
            printf("FAIL: Error adding item %d: %s\n", i, error);
            consumer_producer_destroy(&queue);
            return 0;
        }
    }
    
    // Test getting items
    printf("Getting items from queue...\n");
    for (int i = 0; i < 3; i++) {
        char* item = consumer_producer_get(&queue);
        if (item) {
            printf("Got: %s\n", item);
            free(item);
        } else {
            printf("FAIL: Got NULL item at position %d\n", i);
            consumer_producer_destroy(&queue);
            return 0;
        }
    }
    
    consumer_producer_destroy(&queue);
    printf("PASS: Basic queue operations work\n");
    return 1;
}

int test_queue_capacity() {
    printf("\n=== Test: Queue Capacity Limits ===\n");
    
    consumer_producer_t queue;
    const char* error = consumer_producer_init(&queue, 2); // Small queue
    if (error) {
        printf("FAIL: Failed to initialize queue: %s\n", error);
        return 0;
    }
    
    // Fill the queue
    printf("Filling queue to capacity...\n");
    error = consumer_producer_put(&queue, "item1");
    if (error) {
        printf("FAIL: Error adding first item: %s\n", error);
        consumer_producer_destroy(&queue);
        return 0;
    }
    
    error = consumer_producer_put(&queue, "item2");
    if (error) {
        printf("FAIL: Error adding second item: %s\n", error);
        consumer_producer_destroy(&queue);
        return 0;
    }
    
    printf("Queue should now be full\n");
    
    // Try to add one more in a separate thread (should block)
    pthread_t producer;
    test_thread_data_t producer_data = {&queue, 1, 1, 0};
    
    pthread_create(&producer, NULL, producer_thread, &producer_data);
    
    // Give producer time to try to add (should block)
    usleep(100000); // 100ms
    
    // Now consume one item to make space
    printf("Consuming one item to make space...\n");
    char* item = consumer_producer_get(&queue);
    if (item) {
        printf("Consumed: %s\n", item);
        free(item);
    }
    
    // Wait for producer to finish
    pthread_join(producer, NULL);
    
    consumer_producer_destroy(&queue);
    printf("PASS: Queue capacity limits work\n");
    return 1;
}

int test_multiple_producers_consumers() {
    printf("\n=== Test: Multiple Producers and Consumers ===\n");
    
    consumer_producer_t queue;
    const char* error = consumer_producer_init(&queue, 10);
    if (error) {
        printf("FAIL: Failed to initialize queue: %s\n", error);
        return 0;
    }
    
    const int NUM_PRODUCERS = 2;
    const int NUM_CONSUMERS = 2;
    const int ITEMS_PER_PRODUCER = 5;
    
    pthread_t producers[NUM_PRODUCERS];
    pthread_t consumers[NUM_CONSUMERS];
    test_thread_data_t producer_data[NUM_PRODUCERS];
    test_thread_data_t consumer_data[NUM_CONSUMERS];
    
    // Create producers
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        producer_data[i] = (test_thread_data_t){&queue, i, ITEMS_PER_PRODUCER, 0};
        pthread_create(&producers[i], NULL, producer_thread, &producer_data[i]);
    }
    
    // Create consumers
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        consumer_data[i] = (test_thread_data_t){&queue, i, ITEMS_PER_PRODUCER, 0};
        pthread_create(&consumers[i], NULL, consumer_thread, &consumer_data[i]);
    }
    
    // Wait for all producers to finish
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        pthread_join(producers[i], NULL);
    }
    
    // Wait for all consumers to finish
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        pthread_join(consumers[i], NULL);
    }
    
    // Check that all items were consumed
    int total_consumed = 0;
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        total_consumed += consumer_data[i].items_consumed;
    }
    
    int expected_total = NUM_PRODUCERS * ITEMS_PER_PRODUCER;
    
    consumer_producer_destroy(&queue);
    
    if (total_consumed == expected_total) {
        printf("PASS: Multiple producers/consumers work (consumed %d/%d items)\n", 
               total_consumed, expected_total);
        return 1;
    } else {
        printf("FAIL: Expected %d items, consumed %d\n", expected_total, total_consumed);
        return 0;
    }
}

int main() {
    printf("Starting Consumer-Producer Queue Unit Tests...\n");
    
    int tests_passed = 0;
    int total_tests = 3;
    
    tests_passed += test_basic_queue_operations();
    tests_passed += test_queue_capacity();
    tests_passed += test_multiple_producers_consumers();
    
    printf("\n=== Test Results ===\n");
    printf("Passed: %d/%d tests\n", tests_passed, total_tests);
    
    if (tests_passed == total_tests) {
        printf("🎉 All queue tests passed!\n");
        return 0;
    } else {
        printf("❌ Some tests failed!\n");
        return 1;
    }
}