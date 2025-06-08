#ifndef IMODULE_H
#define IMODULE_H

#include <ncurses.h>
#include <stdbool.h>

struct MinimalTui; // Forward declaration

typedef struct IModule {
    const char* name;
    void (*render)(struct IModule* self, WINDOW* win);
    void (*handle_input)(struct IModule* self, int ch, struct MinimalTui* tui);
    void* data; // Pointer to module-specific data
} IModule;

#endif 