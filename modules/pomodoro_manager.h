#ifndef POMODORO_MANAGER_H
#define POMODORO_MANAGER_H

#include <ncurses.h>

struct IModule;
struct MinimalTui;

typedef enum {
    POMO_STATE_WORK,
    POMO_STATE_REST
} PomodoroSessionState;

typedef enum {
    POMO_MODE_STANDARD,
    POMO_MODE_PROGRESSIVE
} PomodoroCycleMode;

typedef enum {
    POMO_UI_NORMAL,
    POMO_UI_EDITING
} PomodoroUiMode;

typedef struct {
    long work_duration;  // in seconds
    long rest_duration;  // in seconds
    long progressive_step; // in seconds
    long current_work_duration; // in seconds
    long total_seconds;

    PomodoroSessionState current_state;
    PomodoroCycleMode cycle_mode;
    PomodoroUiMode ui_mode;
    PomodoroSessionState editing_state; // To track if we are editing work or rest

    int is_running;
    int frame;

    // For editing time
    int edit_cursor_pos;
    char edit_buffer[5]; // "MMSS\0"
} PomodoroData;

void pomodoro_init_data(PomodoroData* data);
void pomodoro_module_render(struct IModule* self, WINDOW* win);
void pomodoro_module_handle_input(struct IModule* self, int ch, struct MinimalTui* tui);
void pomodoro_module_tick(struct IModule* self);

#endif 