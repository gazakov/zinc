#include "pomodoro_manager.h"
#include "imodule.h"
#include <ctype.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// static assets
static const char *smoke_frames[4][2] = {
    {"   ( (", "    ) )"}, {"    ) )", "   ( ("},
    {"   ( (", "    ) )"}, {"    ) )", "   ( ("}};
static const char *cup[] = {" ........", " |      |]", " \\      /", "  `----'"};

// forward declaration
static void start_new_session(PomodoroData *data, PomodoroSessionState state);

// --- Initialization ---
void pomodoro_init_data(PomodoroData *data) {
  data->work_duration = 25 * 60;
  data->rest_duration = 5 * 60;
  data->progressive_step = 15 * 60;
  data->current_work_duration = 0; // will be set by start_new_session
  data->cycle_mode = POMO_MODE_STANDARD;
  data->editing_state = POMO_STATE_WORK; // default
  data->frame = 0; // initialize frame
  start_new_session(data, POMO_STATE_WORK);
}

// --- UI Rendering ---
static void draw_edit_window(PomodoroData *data, WINDOW *parent_win) {
  int parent_h, parent_w;
  getmaxyx(parent_win, parent_h, parent_w);
  int h = 5, w = 24;
  int y = (parent_h - h) / 2;
  int x = (parent_w - w) / 2;

  WINDOW *edit_win = newwin(h, w, y, x);
  box(edit_win, 0, 0);
  const char *title = (data->editing_state == POMO_STATE_WORK)
                          ? "Set Work Time (MMSS)"
                          : "Set Rest Time (MMSS)";
  mvwprintw(edit_win, 1, 2, "%s", title);

  char display_buf[5] = "____";
  strncpy(display_buf, data->edit_buffer, 4);
  mvwprintw(edit_win, 2, (w - 5) / 2, "%c%c:%c%c", display_buf[0],
            display_buf[1], display_buf[2], display_buf[3]);

  int cursor_x_offset = (w - 5) / 2 + data->edit_cursor_pos;
  if (data->edit_cursor_pos >= 2) cursor_x_offset++;
  wmove(edit_win, 2, cursor_x_offset);
  curs_set(1);

  wrefresh(edit_win);
  delwin(edit_win);
}

void pomodoro_module_render(struct IModule *self, WINDOW *win) {
  PomodoroData *data = (PomodoroData *)self->data;
  werase(win);
  box(win, 0, 0);

  const char *session_str = (data->current_state == POMO_STATE_WORK) ? "Work" : "Rest";
  mvwprintw(win, 1, 2, "Pomodoro: %s", session_str);

  int y = 3;
  if (getmaxy(win) > 12) {
    for (int i = 0; i < 2; ++i) mvwprintw(win, y++, 6, "%s", smoke_frames[data->frame][i]);
    for (int i = 0; i < 4; ++i) mvwprintw(win, y++, 4, "%s", cup[i]);
  }
  y++;

  long minutes = data->total_seconds / 60;
  long seconds = data->total_seconds % 60;
  mvwprintw(win, y, 7, "%02ld:%02ld", minutes, seconds);

  y += 2;
  const char *mode_str =
      data->cycle_mode == POMO_MODE_STANDARD ? "standard" : "progressive";
  if (data->cycle_mode == POMO_MODE_STANDARD) {
    mvwprintw(win, y++, 2, "[i/o] Edit W/R [r] Reset [m] Mode: %s", mode_str);
  } else {
    mvwprintw(win, y++, 2, "[r] Reset [m] Mode: %s", mode_str);
  }
  mvwprintw(win, y++, 2, "[space] Start/Pause [b] Back");

  wnoutrefresh(win);

  if (data->ui_mode == POMO_UI_EDITING) {
    draw_edit_window(data, win);
  } else {
    curs_set(0);
  }
}

// --- Input Handling ---
void pomodoro_module_handle_input(struct IModule *self, int ch, struct MinimalTui *tui) {
  PomodoroData *data = (PomodoroData *)self->data;

  if (data->ui_mode == POMO_UI_EDITING) {
    if (isdigit(ch) && data->edit_cursor_pos < 4) {
      data->edit_buffer[data->edit_cursor_pos++] = ch;
    } else if ((ch == KEY_BACKSPACE || ch == 127) && data->edit_cursor_pos > 0) {
      data->edit_buffer[--data->edit_cursor_pos] = '\0';
    } else if (ch == '\n' || ch == KEY_ENTER) {
      if (strlen(data->edit_buffer) == 4) {
        long min = (data->edit_buffer[0] - '0') * 10 + (data->edit_buffer[1] - '0');
        long sec = (data->edit_buffer[2] - '0') * 10 + (data->edit_buffer[3] - '0');
        if (sec > 59) sec = 59;
        if (min > 99) min = 99;
        long new_duration = min * 60 + sec;

        if (data->editing_state == POMO_STATE_WORK) {
          data->work_duration = new_duration;
        } else { // editing rest time
          data->rest_duration = new_duration;
        }

        // if we edited the currently active session, restart it with the new time
        if (data->editing_state == data->current_state) {
          start_new_session(data, data->current_state);
        }
      }
      data->ui_mode = POMO_UI_NORMAL;
    } else if (ch == 27 || ch == 'b' || ch == 'i' || ch == 'o') { // esc, b, or i/o to cancel
      data->ui_mode = POMO_UI_NORMAL;
    }
  } else { // normal mode
    switch (ch) {
    case 'i':
    case 'o':
      if (data->cycle_mode == POMO_MODE_STANDARD) {
        data->is_running = 0;
        data->ui_mode = POMO_UI_EDITING;
        data->editing_state = (ch == 'i') ? POMO_STATE_WORK : POMO_STATE_REST;
        data->edit_cursor_pos = 0;
        memset(data->edit_buffer, 0, sizeof(data->edit_buffer));
      }
      break;
    case ' ':
      data->is_running = !data->is_running;
      break;
    case 'r':
      start_new_session(data, data->current_state);
      break;
    case 'm':
      data->cycle_mode = (data->cycle_mode == POMO_MODE_STANDARD) ? POMO_MODE_PROGRESSIVE : POMO_MODE_STANDARD;
      data->current_work_duration = 0; // reset progress
      start_new_session(data, POMO_STATE_WORK);
      break;
    }
  }
  (void)tui;
}

// --- Time & State Logic ---
void pomodoro_module_tick(struct IModule *self) {
  PomodoroData *data = (PomodoroData *)self->data;
  if (data->is_running && data->total_seconds > 0) {
    data->total_seconds--;
  } else if (data->is_running && data->total_seconds == 0) {
    beep(); napms(300); beep(); napms(300); beep(); // play a sound 3 times with a longer delay
    if (data->current_state == POMO_STATE_WORK) {
      start_new_session(data, POMO_STATE_REST);
    } else {
      start_new_session(data, POMO_STATE_WORK);
    }
  }
  if (data->is_running) {
    data->frame = (data->frame + 1) % 4;
  }
}

static void start_new_session(PomodoroData *data, PomodoroSessionState state) {
  data->current_state = state;
  data->is_running = 0;
  data->ui_mode = POMO_UI_NORMAL;
  memset(data->edit_buffer, 0, sizeof(data->edit_buffer));
  data->edit_cursor_pos = 0;

  if (state == POMO_STATE_WORK) {
    // if progressive and we have a duration, increment it. otherwise start from base.
    if (data->cycle_mode == POMO_MODE_PROGRESSIVE) {
      if(data->current_work_duration > 0) {
         data->current_work_duration += data->progressive_step;
      } else {
         data->current_work_duration = data->work_duration;
      }
    } else {
        data->current_work_duration = data->work_duration;
    }
    data->total_seconds = data->current_work_duration;
  } else { // rest state
    data->total_seconds = data->rest_duration;
  }
}