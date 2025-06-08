#include "minimal_tui.h"
#include "../modules/habit_manager.h" // needed for habit functions
#include "../modules/task_manager.h"
#include "../modules/pomodoro_manager.h"
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// forward declarations for placeholder module functions
void placeholder_render(IModule *self, WINDOW *win);
void placeholder_handle_input(IModule *self, int ch, MinimalTui *tui);

// module definitions
static IModule module_habits = {"Habits", habits_module_render, habits_module_handle_input, NULL};
static IModule module_tasks = {"Tasks", tasks_module_render, tasks_module_handle_input, NULL};
static IModule module_pomodoro = {"Pomodoro", pomodoro_module_render, pomodoro_module_handle_input, NULL};
static IModule module_settings = {"Settings", placeholder_render, placeholder_handle_input, NULL};

static IModule *all_modules[] = {
    &module_habits, &module_tasks, &module_pomodoro,
    &module_settings,
};
static const int NUM_MODULES_IMPL =
    sizeof(all_modules) / sizeof(all_modules[0]);

static void wm_init(WindowManager *wm, int rows, int cols) {
  wm->rows = rows;
  wm->cols = cols;
  wm->panel_win = newwin(rows - 2, cols, 0, 0);
  wm->status_win = newwin(1, cols, rows - 2, 0);
}

static void wm_cleanup(WindowManager *wm) {
  if (wm->panel_win)
    delwin(wm->panel_win);
  if (wm->status_win)
    delwin(wm->status_win);
}

static void draw_panel(WindowManager *wm, int selected) {
  werase(wm->panel_win);
  mvwprintw(wm->panel_win, 1, 2, "Select a module:");

  for (int i = 0; i < NUM_MODULES_IMPL; ++i) {
    if (i == selected) {
      wattron(wm->panel_win, A_REVERSE);
    }
    mvwprintw(wm->panel_win, 3 + i, 4, "%s", all_modules[i]->name);
    if (i == selected) {
      wattroff(wm->panel_win, A_REVERSE);
    }
  }
  wnoutrefresh(wm->panel_win);
}

static void draw_status(WindowManager *wm, const char *message) {
  werase(wm->status_win);
  mvwprintw(wm->status_win, 0, 1, "%s", message);
  wnoutrefresh(wm->status_win);
}

void minimal_tui_init(MinimalTui *tui) {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  wm_init(&tui->wm, rows, cols);
  tui->state = UI_PANEL;
  tui->selected = 0;
  tui->running = true;
  tui->active_module = NULL;
  tui->last_tick = time(NULL);

  for (int i = 0; i < NUM_MODULES_IMPL; ++i) {
    tui->modules[i] = all_modules[i];
  }
  // Data for modules is handled statically within the modules themselves
  
  pomodoro_init();
}

void minimal_tui_cleanup(MinimalTui *tui) {
  wm_cleanup(&tui->wm);
  // No need to free module data as it's static
}

void minimal_tui_resize(MinimalTui *tui) {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  wm_cleanup(&tui->wm);
  wm_init(&tui->wm, rows, cols);
}

void minimal_tui_render(MinimalTui *tui) {
  time_t current_time = time(NULL);
  
  // only update pomodoro if it's active
  if (tui->active_module == &module_pomodoro && current_time != tui->last_tick) {
    pomodoro_module_tick(tui->active_module);
    tui->last_tick = current_time;
  }

  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  
  // only resize if necessary
  if (rows != tui->wm.rows || cols != tui->wm.cols) {
    minimal_tui_resize(tui);
    werase(stdscr);
  }

  if (tui->wm.cols < 60 || tui->wm.rows < 20) {
    werase(stdscr);
    const char *msg1 = "terminal too small!";
    const char *msg2 = "please resize to at least 60x20 :)";
    int y1 = tui->wm.rows / 2 - 1;
    int x1 = (tui->wm.cols - (int)strlen(msg1)) / 2;
    int y2 = tui->wm.rows / 2;
    int x2 = (tui->wm.cols - (int)strlen(msg2)) / 2;
    if (y1 >= 0 && x1 >= 0)
      mvprintw(y1, x1, "%s", msg1);
    if (y2 >= 0 && x2 >= 0 && y2 < tui->wm.rows)
      mvprintw(y2, x2, "%s", msg2);
  } else {
    switch (tui->state) {
    case UI_PANEL:
      draw_panel(&tui->wm, tui->selected);
      draw_status(&tui->wm, "panel: arrows to move, enter to select, q to quit");
      break;
    case UI_MODULE:
      if (tui->active_module && tui->active_module->render) {
        tui->active_module->render(tui->active_module, tui->wm.panel_win);
      }
      draw_status(&tui->wm, "module: b to back, q to quit");
      break;
    case UI_SETTINGS:
      break;
    case UI_EXIT:
      tui->running = false;
      return;
    }
  }
  doupdate();
  refresh();
}

void minimal_tui_handle_input(MinimalTui *tui, int ch) {
  switch (tui->state) {
  case UI_PANEL:
    if (ch == KEY_UP)
      tui->selected = (tui->selected - 1 + NUM_MODULES_IMPL) % NUM_MODULES_IMPL;
    else if (ch == KEY_DOWN)
      tui->selected = (tui->selected + 1) % NUM_MODULES_IMPL;
    else if (ch == '\n' || ch == KEY_ENTER) {
      tui->state = UI_MODULE;
      tui->active_module = tui->modules[tui->selected];
    } else if (ch == 'q')
      tui->state = UI_EXIT;
    break;
  case UI_MODULE:
    if (ch == 'b') {
      tui->state = UI_PANEL;
      tui->active_module = NULL;
    } else if (ch == 'q') {
        tui->state = UI_EXIT;
    }
    else if (tui->active_module && tui->active_module->handle_input) {
      tui->active_module->handle_input(tui->active_module, ch, tui);
    }
    break;
  default:
    break;
  }
}

bool minimal_tui_is_running(const MinimalTui *tui) { return tui->running; }

void placeholder_render(IModule *self, WINDOW *win) {
  werase(win);
  mvwprintw(win, 2, 2, "%s module is under construction!", self->name);
  wnoutrefresh(win);
}

void placeholder_handle_input(IModule *self, int ch, MinimalTui *tui) {
  (void)self;
  (void)ch;
  (void)tui;
}