#ifndef MINIMAL_TUI_H
#define MINIMAL_TUI_H

#include <ncurses.h>
#include <stdbool.h>
#include <time.h>
#include "../modules/imodule.h"

#define NUM_MODULES 5

typedef enum {
    UI_PANEL,
    UI_MODULE,
    UI_SETTINGS,
    UI_EXIT
} UiState;

typedef struct {
    int rows;
    int cols;
    WINDOW *panel_win;
    WINDOW *status_win;
} WindowManager;

typedef struct MinimalTui {
    WindowManager wm;
    UiState state;
    int selected;
    bool running;
    IModule* modules[NUM_MODULES];
    IModule* active_module;
    time_t last_tick;
} MinimalTui;

#ifdef __cplusplus
extern "C" {
#endif

void minimal_tui_init(MinimalTui* tui);
void minimal_tui_cleanup(MinimalTui* tui);
void minimal_tui_resize(MinimalTui* tui);
void minimal_tui_render(MinimalTui* tui);
void minimal_tui_handle_input(MinimalTui* tui, int ch);
bool minimal_tui_is_running(const MinimalTui* tui);

#ifdef __cplusplus
}
#endif

#endif 