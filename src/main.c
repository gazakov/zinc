#include <ncurses.h>
#include <locale.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include "../modules/habit_manager.h"
#include "../modules/task_manager.h"
#include "minimal_tui.h"

#define SETTINGS_FILE "data/settings.conf"

int main() {
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    timeout(100);

    mkdir("data", 0755);

    if (habits_load("data/habits.csv") != 0) {
        habits_init();
    }
    if (tasks_load("data/tasks.csv") != 0) {
        tasks_init();
    }

    // handle daily updates
    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    char today_str[11];
    strftime(today_str, sizeof(today_str), "%Y-%m-%d", tm_now);

    char last_update_str[11] = {0};
    FILE* f = fopen(SETTINGS_FILE, "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            char key[64];
            char value[128];
            if (sscanf(line, "%63[^=]=%127s", key, value) == 2) {
                if (strcmp(key, "last_update") == 0) {
                    strncpy(last_update_str, value, sizeof(last_update_str) - 1);
                    break;
                }
            }
        }
        fclose(f);
    }

    if (strcmp(today_str, last_update_str) != 0) {
        habits_daily_update();
        habits_save("data/habits.csv");
        tasks_save("data/tasks.csv");

        FILE* f_write = fopen(SETTINGS_FILE, "w");
        if (f_write) {
            fprintf(f_write, "last_update=%s\n", today_str);
            fclose(f_write);
        }
    }

    MinimalTui tui;
    minimal_tui_init(&tui);

    int ch;
    while (minimal_tui_is_running(&tui)) {
        ch = getch();

        if (ch == KEY_RESIZE) {
            minimal_tui_resize(&tui);
        } else if (ch != ERR) {
            minimal_tui_handle_input(&tui, ch);
        }

        minimal_tui_render(&tui);
    }

    minimal_tui_cleanup(&tui);
    endwin();

    if (habits_save("data/habits.csv") != 0) {
        fprintf(stderr, "Error saving habits.\n");
    }
    if (tasks_save("data/tasks.csv") != 0) {
        fprintf(stderr, "Error saving tasks.\n");
    }

    return 0;
}
