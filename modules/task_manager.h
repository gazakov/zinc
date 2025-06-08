#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include <ncurses.h>
#include "structs.h"

struct IModule;
struct MinimalTui;

void tasks_init();
void tasks_cleanup();

void tasks_module_render(struct IModule* self, WINDOW* win);
void tasks_module_handle_input(struct IModule* self, int ch, struct MinimalTui* tui);

int tasks_load(const char* filename);
int tasks_save(const char* filename);

#endif 