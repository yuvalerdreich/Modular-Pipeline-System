#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>

typedef const char* (*plugin_init_func_t)(int);
typedef const char* (*plugin_fini_func_t)(void);
typedef const char* (*plugin_place_work_func_t)(const char*);
typedef void (*plugin_attach_func_t)(const char* (*)(const char*));
typedef const char* (*plugin_wait_finished_func_t)(void);
typedef const char* (*plugin_get_name_func_t)(void);

typedef struct {
    plugin_init_func_t init;
    plugin_fini_func_t fini;
    plugin_place_work_func_t place_work;
    plugin_attach_func_t attach;
    plugin_wait_finished_func_t wait_finished;
    char* name;
    void* handle;
} plugin_handle_t;

void print_usage(char* program_name) {
    printf("Usage: %s <queue_size> <plugin1> <plugin2> ... <pluginN>\n", program_name);
    printf("Arguments:\n");
    printf("  queue_size    Maximum number of items in each plugin's queue\n");
    printf("  plugin1..N    Names of plugins to load (without .so extension)\n");
    printf("Available plugins:\n");
    printf("  logger        - Logs all strings that pass through\n");
    printf("  typewriter    - Simulates typewriter effect with delays\n");
    printf("  uppercaser    - Converts strings to uppercase\n");
    printf("  rotator       - Move every character to the right. Last character moves to the beginning.\n");
    printf("  flipper       - Reverses the order of characters\n");
    printf("  expander      - Expands each character with spaces\n");
    printf("Example:\n");
    printf("  %s 20 uppercaser rotator logger\n", program_name);
}

int load_plugin(const char* plugin_name, plugin_handle_t* plugin, char* program_name) {
    char filename[256];
    void* handle_first_option = NULL;
    void* handle_second_option = NULL; 
    void* handle_third_option = NULL;
    
    snprintf(filename, sizeof(filename), "./%s.so", plugin_name);
    handle_first_option = dlopen(filename, RTLD_NOW | RTLD_LOCAL);
    
    if (!handle_first_option) {
        snprintf(filename, sizeof(filename), "output/%s.so", plugin_name);
        handle_second_option = dlopen(filename, RTLD_NOW | RTLD_LOCAL);

        if (!handle_second_option) {
            snprintf(filename, sizeof(filename), "./output/%s.so", plugin_name);
            handle_third_option = dlopen(filename, RTLD_NOW | RTLD_LOCAL);
            
            if (!handle_third_option) {
                fprintf(stderr, "Error loading plugin %s: %s\n", plugin_name, dlerror());
                return -1;

            } else {
                plugin->handle = handle_third_option;
            }

        } else {
            plugin->handle = handle_second_option;
        }

    } else {
        plugin->handle = handle_first_option;
    }
    
    dlerror();
    
    plugin->init = (plugin_init_func_t)dlsym(plugin->handle, "plugin_init");
    if (!plugin->init) {
        fprintf(stderr, "Error loading plugin_init from %s: %s\n", plugin_name, dlerror());
        print_usage(program_name);
        dlclose(plugin->handle);
        return 1;
    }
    
    plugin->fini = (plugin_fini_func_t)dlsym(plugin->handle, "plugin_fini");
    if (!plugin->fini) {
        fprintf(stderr, "Error loading plugin_fini from %s: %s\n", plugin_name, dlerror());
        print_usage(program_name);
        dlclose(plugin->handle);
        return 1;
    }
    
    plugin->place_work = (plugin_place_work_func_t)dlsym(plugin->handle, "plugin_place_work");
    if (!plugin->place_work) {
        fprintf(stderr, "Error loading plugin_place_work from %s: %s\n", plugin_name, dlerror());
        print_usage(program_name);
        dlclose(plugin->handle);
        return 1;
    }
    
    plugin->attach = (plugin_attach_func_t)dlsym(plugin->handle, "plugin_attach");
    if (!plugin->attach) {
        fprintf(stderr, "Error loading plugin_attach from %s: %s\n", plugin_name, dlerror());
        print_usage(program_name);
        dlclose(plugin->handle);
        return 1;
    }
    
    plugin->wait_finished = (plugin_wait_finished_func_t)dlsym(plugin->handle, "plugin_wait_finished");
    if (!plugin->wait_finished) {
        fprintf(stderr, "Error loading plugin_wait_finished from %s: %s\n", plugin_name, dlerror());
        print_usage(program_name);
        dlclose(plugin->handle);
        return 1;
    }
    
    plugin->name = strdup(plugin_name);
    return 0;
}

void cleanup_plugins(plugin_handle_t* plugins, int count) {
    for (int i = 0; i < count; i++) {
        if (plugins[i].fini) {
            plugins[i].fini();
        }

        if (plugins[i].handle) {
            dlclose(plugins[i].handle);
        }

        if (plugins[i].name) {
            free(plugins[i].name);
        }
    }
}

int main(int argc, char* argv[]) {
    char* program_name = argv[0];

    if (argc < 3) {
        fprintf(stderr, "Error: Invalid number of arguments\n");
        print_usage(program_name);
        return 1;
    }

    // Check that the first argument contains only digits
    for (char *char_in_arg = argv[1]; *char_in_arg; ++char_in_arg) {
        if (*char_in_arg < '0' || *char_in_arg > '9') {
            fprintf(stderr, "Error: Invalid queue size\n");
            print_usage(program_name);
            return 1;
        }
    }

    // Check that the first argument contains positive number
    int queue_size = atoi(argv[1]);
    if (queue_size <= 0) {
        fprintf(stderr, "Error: Invalid queue size\n");
        print_usage(program_name);
        return 1;
    }
    
    int num_plugins = argc - 2;
    plugin_handle_t* plugins = malloc(num_plugins * sizeof(plugin_handle_t));
    if (!plugins) {
        fprintf(stderr, "Error: Failed to allocate memory for plugins\n");
        return 1;
    }
    
    // Initialize plugins array
    memset(plugins, 0, num_plugins * sizeof(plugin_handle_t));
    
    // Load all plugins
    for (int i = 0; i < num_plugins; i++) {
        if (load_plugin(argv[i + 2], &plugins[i], program_name) != 0) {
            cleanup_plugins(plugins, i);
            free(plugins);
            print_usage(program_name);
            return 1;
        }
    }
    
    // Initialize all plugins
    for (int i = 0; i < num_plugins; i++) {
        const char* error = plugins[i].init(queue_size);
        if (error) {
            fprintf(stderr, "Error initializing plugin %s: %s\n", plugins[i].name, error);
            cleanup_plugins(plugins, num_plugins);
            free(plugins);
            return 2;
        }
    }
    
    // Attach plugins together
    for (int i = 0; i < num_plugins - 1; i++) {
        plugins[i].attach(plugins[i + 1].place_work);
    }
    
    // Read input and process
    char line[1025];
    while (fgets(line, sizeof(line), stdin)) {
        int len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        
        if (num_plugins > 0) {
            const char* error = plugins[0].place_work(line);
            if (error) {
                fprintf(stderr, "Error placing work: %s\n", error);
                break;
            }
        }
        
        if (strcmp(line, "<END>") == 0) {
            break;
        }
    }
    
    // Wait for all plugins to finish
    for (int i = 0; i < num_plugins; i++) {
        const char* error = plugins[i].wait_finished();
        if (error) {
            fprintf(stderr, "Error waiting for plugin %s: %s\n", plugins[i].name, error);
        }
    }
    
    // Cleanup
    cleanup_plugins(plugins, num_plugins);
    free(plugins);
    
    // Finalize
    printf("Pipeline shutdown complete\n");
    return 0;
}