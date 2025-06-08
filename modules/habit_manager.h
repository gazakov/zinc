#ifndef HABIT_MANAGER_H
#define HABIT_MANAGER_H

#include <ncurses.h>
#include "structs.h"

struct IModule;
struct MinimalTui;

void habits_init();
void habits_cleanup();

void habits_module_render(struct IModule* self, WINDOW* win);
void habits_module_handle_input(struct IModule* self, int ch, struct MinimalTui* tui);
void habits_daily_update(void);

int habits_load(const char* filename);
int habits_save(const char* filename);
int get_habit_count();
void habits_toggle_today(int head_idx, int task_idx);

#endif 