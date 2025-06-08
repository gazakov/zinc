#include "habit_manager.h"
#include "imodule.h"
#include "../src/minimal_tui.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static HabitData habit_data;

void habits_init() {
    habit_data.head_count = 0;
    habit_data.selected_head = 0;
    habit_data.selected_task = 0;
    habit_data.edit_mode = false;
    habit_data.move_mode = false;
}

// helper function to get string input in a popup window
static bool prompt_for_string(WINDOW* parent_win, const char* prompt, char* buffer, int buffer_size) {
    int parent_h, parent_w;
    getmaxyx(parent_win, parent_h, parent_w);
    int h = 3;
    int w = strlen(prompt) + 20; // a bit of padding
    int y = (parent_h - h) / 2;
    int x = (parent_w - w) / 2;

    WINDOW* win = newwin(h, w, y, x);
    box(win, 0, 0);
    mvwprintw(win, 1, 2, "%s", prompt);
    wrefresh(win);

    echo();
    curs_set(1);
    wgetnstr(win, buffer, buffer_size - 1);
    curs_set(0);
    noecho();

    delwin(win);
    // this is a bit of a hack to "clear" the parent window where the popup was
    // without doing a full redraw of the app, which would be slow.
    touchwin(parent_win); 
    wnoutrefresh(parent_win);

    // check if user entered something or just pressed enter/esc
    return strlen(buffer) > 0;
}

static void ensure_task_selected() {
    if (habit_data.selected_task != -1 && habit_data.head_count > 0 && habit_data.heads[habit_data.selected_head].task_count > 0) return;
    if (habit_data.head_count == 0) return;

    for (int i = 0; i < habit_data.head_count; i++) {
        if (habit_data.heads[i].task_count > 0) {
            habit_data.selected_head = i;
            habit_data.selected_task = 0;
            return;
        }
    }
}

void habits_cleanup() {
    // nothing to clean up with static allocation
}

void habits_module_render(struct IModule* self, WINDOW* win) {
    werase(win);
    mvwprintw(win, 1, 2, "Habits %s", habit_data.edit_mode ? "[EDIT MODE]" : "");

    int y = 3;
    for (int h = 0; h < habit_data.head_count; h++) {
        if (h == habit_data.selected_head && habit_data.selected_task == -1) {
            wattron(win, A_REVERSE);
        }
        mvwprintw(win, y++, 2, "%s:", habit_data.heads[h].name);
        if (h == habit_data.selected_head && habit_data.selected_task == -1) {
            wattroff(win, A_REVERSE);
        }

        for (int t = 0; t < habit_data.heads[h].task_count; t++) {
            if (h == habit_data.selected_head && t == habit_data.selected_task) {
                wattron(win, A_REVERSE);
            }
            mvwprintw(win, y++, 4, "%d. [%c] %s (%d)",
                     t + 1,
                     habit_data.heads[h].tasks[t].done_today ? 'X' : ' ',
                     habit_data.heads[h].tasks[t].name,
                     habit_data.heads[h].tasks[t].streak);
            if (h == habit_data.selected_head && t == habit_data.selected_task) {
                wattroff(win, A_REVERSE);
            }
        }
        y++; // add space between heads
    }

    // draw help text
    int max_y = getmaxy(win);
    if (habit_data.edit_mode) {
        int help_y = max_y - 8;
        if (help_y < y) help_y = y;
        mvwprintw(win, help_y++, 2, "Edit Mode %s:", habit_data.move_mode ? "[MOVING]" : "");
        mvwprintw(win, help_y++, 4, "R+U: New Head");
        mvwprintw(win, help_y++, 4, "R+I: New Task");
        mvwprintw(win, help_y++, 4, "D: Delete Item");
        mvwprintw(win, help_y++, 4, "S: Toggle Move");
        mvwprintw(win, help_y++, 4, "E+I: Toggle Edit");
        mvwprintw(win, help_y++, 4, "ESC: Exit Edit/Move");
    } else {
        int help_y = max_y - 5;
        if (help_y < y) help_y = y;
        mvwprintw(win, help_y++, 2, "Navigation:");
        mvwprintw(win, help_y++, 4, "↑/↓: Move");
        mvwprintw(win, help_y++, 4, "Space: Toggle");
        mvwprintw(win, help_y++, 4, "E+I: Edit Mode");
    }
    
    wnoutrefresh(win);
}

void habits_module_handle_input(struct IModule* self, int ch, struct MinimalTui* tui) {
    if (habit_data.edit_mode) {
        if (habit_data.move_mode) {
            if (ch == 's' || ch == 'S' || ch == 27) {
                habit_data.move_mode = false;
            } else if (ch == KEY_UP) {
                if (habit_data.selected_task == -1) { // moving a head
                    if (habit_data.selected_head > 0) {
                        HabitHead temp = habit_data.heads[habit_data.selected_head];
                        habit_data.heads[habit_data.selected_head] = habit_data.heads[habit_data.selected_head - 1];
                        habit_data.heads[habit_data.selected_head - 1] = temp;
                        habit_data.selected_head--;
                    }
                } else { // moving a task
                    int head_idx = habit_data.selected_head;
                    int task_idx = habit_data.selected_task;
                    HabitHead* current_head = &habit_data.heads[head_idx];

                    if (task_idx > 0) { // move up within the same head
                        Task temp = current_head->tasks[task_idx];
                        current_head->tasks[task_idx] = current_head->tasks[task_idx - 1];
                        current_head->tasks[task_idx - 1] = temp;
                        habit_data.selected_task--;
                    } else if (task_idx == 0 && head_idx > 0) { // move task to the previous head
                        HabitHead* prev_head = &habit_data.heads[head_idx - 1];
                        if (prev_head->task_count < MAX_TASKS_PER_HEAD) {
                            Task task_to_move = current_head->tasks[task_idx];
                            
                            prev_head->tasks[prev_head->task_count] = task_to_move;
                            prev_head->task_count++;
                            
                            memmove(&current_head->tasks[0], &current_head->tasks[1], (current_head->task_count - 1) * sizeof(Task));
                            current_head->task_count--;
                            
                            habit_data.selected_head--;
                            habit_data.selected_task = prev_head->task_count - 1;

                            if (current_head->task_count == 0) {
                                habit_data.selected_task = -1; // select the head if it's now empty
                            }
                        }
                    }
                }
            } else if (ch == KEY_DOWN) {
                if (habit_data.selected_task == -1) { // moving a head
                    if (habit_data.selected_head < habit_data.head_count - 1) {
                        HabitHead temp = habit_data.heads[habit_data.selected_head];
                        habit_data.heads[habit_data.selected_head] = habit_data.heads[habit_data.selected_head + 1];
                        habit_data.heads[habit_data.selected_head + 1] = temp;
                        habit_data.selected_head++;
                    }
                } else { // moving a task
                    int head_idx = habit_data.selected_head;
                    int task_idx = habit_data.selected_task;
                    HabitHead* current_head = &habit_data.heads[head_idx];

                    if (task_idx < current_head->task_count - 1) { // move down within the same head
                        Task temp = current_head->tasks[task_idx];
                        current_head->tasks[task_idx] = current_head->tasks[task_idx + 1];
                        current_head->tasks[task_idx + 1] = temp;
                        habit_data.selected_task++;
                    } else if (task_idx == current_head->task_count - 1 && head_idx < habit_data.head_count - 1) { // move task to the next head
                        HabitHead* next_head = &habit_data.heads[head_idx + 1];
                        if (next_head->task_count < MAX_TASKS_PER_HEAD) {
                            Task task_to_move = current_head->tasks[task_idx];

                            memmove(&next_head->tasks[1], &next_head->tasks[0], next_head->task_count * sizeof(Task));
                            
                            next_head->tasks[0] = task_to_move;
                            next_head->task_count++;
                            
                            current_head->task_count--;
                            
                            habit_data.selected_head++;
                            habit_data.selected_task = 0;
                        }
                    }
                }
            }
            return; 
        }

        // standard edit mode commands
        switch (ch) {
            case 'E': case 'e': {
                int next_ch = getch();
                if (next_ch == 'I' || next_ch == 'i') {
                    habit_data.edit_mode = false;
                    ensure_task_selected();
                }
                break;
            }
            case 'R': case 'r': {
                int next_ch = getch();
                if (next_ch == 'U' || next_ch == 'u') {
                    if (habit_data.head_count < MAX_HEADS) {
                        char new_head_name[MAX_NAME_LENGTH] = {0};
                        if (prompt_for_string(tui->wm.panel_win, "Enter head name: ", new_head_name, MAX_NAME_LENGTH)) {
                            habit_data.heads[habit_data.head_count].id = habit_data.head_count + 1;
                            strncpy(habit_data.heads[habit_data.head_count].name, new_head_name, MAX_NAME_LENGTH - 1);
                            habit_data.heads[habit_data.head_count].task_count = 0;
                            habit_data.head_count++;
                            habit_data.selected_head = habit_data.head_count - 1;
                            habit_data.selected_task = -1;
                        }
                    }
                } else if (next_ch == 'I' || next_ch == 'i') {
                    int head_idx = habit_data.selected_head;
                    HabitHead* head = &habit_data.heads[head_idx];

                    if (head_idx >= 0 && head->task_count < MAX_TASKS_PER_HEAD) {
                        int insert_pos = (habit_data.selected_task == -1) ? 0 : habit_data.selected_task + 1;

                        char new_task_name[MAX_NAME_LENGTH] = {0};
                        if (prompt_for_string(tui->wm.panel_win, "Enter task name: ", new_task_name, MAX_NAME_LENGTH)) {
                            if (insert_pos < head->task_count) {
                                memmove(&head->tasks[insert_pos + 1], 
                                        &head->tasks[insert_pos], 
                                        (head->task_count - insert_pos) * sizeof(Task));
                            }
                            
                            head->task_count++;
                            Task* new_task = &head->tasks[insert_pos];
                            strncpy(new_task->name, new_task_name, MAX_NAME_LENGTH - 1);
                            new_task->name[MAX_NAME_LENGTH - 1] = '\0';
                            new_task->id = head->task_count;
                            new_task->streak = 0;
                            new_task->done_today = false;

                            habit_data.selected_task = insert_pos;
                        }
                    }
                }
                habit_data.move_mode = false;
                ensure_task_selected();
                break;
            }
            case 's': case 'S':
                if (habit_data.head_count > 0) habit_data.move_mode = true;
                break;
            case 'd': case 'D':
                 if (habit_data.selected_task == -1) { // a head is selected
                    if (habit_data.head_count > 0) {
                        int head_to_delete = habit_data.selected_head;
                        for (int i = head_to_delete; i < habit_data.head_count - 1; i++) {
                            habit_data.heads[i] = habit_data.heads[i + 1];
                        }
                        habit_data.head_count--;
                        if (habit_data.selected_head >= habit_data.head_count && habit_data.head_count > 0) {
                            habit_data.selected_head = habit_data.head_count - 1;
                        } else if (habit_data.head_count == 0) {
                            habit_data.selected_head = 0;
                        }
                    }
                } else { // a task is selected
                    int head_idx = habit_data.selected_head;
                    if (head_idx >= 0 && habit_data.heads[head_idx].task_count > 0) {
                        int task_to_delete = habit_data.selected_task;
                        for (int i = task_to_delete; i < habit_data.heads[head_idx].task_count - 1; i++) {
                            habit_data.heads[head_idx].tasks[i] = habit_data.heads[head_idx].tasks[i + 1];
                        }
                        habit_data.heads[head_idx].task_count--;
                        if (habit_data.selected_task >= habit_data.heads[head_idx].task_count) {
                            habit_data.selected_task = habit_data.heads[head_idx].task_count - 1;
                        }
                         if (habit_data.heads[head_idx].task_count == 0) {
                            habit_data.selected_task = -1;
                        }
                    }
                }
                break;
            case KEY_UP:
                if (habit_data.selected_task > 0) {
                    habit_data.selected_task--;
                } else if (habit_data.selected_task == 0) {
                    habit_data.selected_task = -1;
                } else { // a head is selected
                    if (habit_data.selected_head > 0) {
                        habit_data.selected_head--;
                    } else {
                        habit_data.selected_head = habit_data.head_count - 1; // loop to the end
                    }
                    // select the last task of the new head, or the head itself if empty
                    if (habit_data.head_count > 0 && habit_data.heads[habit_data.selected_head].task_count > 0) {
                        habit_data.selected_task = habit_data.heads[habit_data.selected_head].task_count - 1;
                    } else {
                        habit_data.selected_task = -1;
                    }
                }
                break;
            case KEY_DOWN:
                if (habit_data.selected_task == -1) { // a head is selected
                    if (habit_data.head_count > 0 && habit_data.heads[habit_data.selected_head].task_count > 0) {
                        habit_data.selected_task = 0; // move to first task
                    } else { // empty head, move to next head
                        if (habit_data.selected_head < habit_data.head_count - 1) {
                            habit_data.selected_head++;
                        } else {
                            habit_data.selected_head = 0; // loop
                        }
                        habit_data.selected_task = -1;
                    }
                } else if (habit_data.selected_task < habit_data.heads[habit_data.selected_head].task_count - 1) {
                    habit_data.selected_task++;
                } else { // last task, move to next head
                    if (habit_data.selected_head < habit_data.head_count - 1) {
                        habit_data.selected_head++;
                    } else {
                        habit_data.selected_head = 0; // loop
                    }
                    habit_data.selected_task = -1;
                }
                break;
            case 27: // esc
            habit_data.edit_mode = false;
                habit_data.move_mode = false;
                break;
        }
    } else {
        // normal mode commands
        int total_tasks = 0;
        for (int i = 0; i < habit_data.head_count; i++) {
            total_tasks += habit_data.heads[i].task_count;
        }
        if (total_tasks == 0) {
             if (ch == 'E' || ch == 'e') {
                int next_ch = getch();
                if (next_ch == 'I' || next_ch == 'i') habit_data.edit_mode = true;
            }
            return; // no tasks to navigate
        }

        ensure_task_selected();

        switch (ch) {
            case 'E': case 'e': {
                int next_ch = getch();
                if (next_ch == 'I' || next_ch == 'i') habit_data.edit_mode = true;
                break;
            }
            case KEY_UP:
                if (habit_data.selected_task > 0) {
                    habit_data.selected_task--;
                } else { // first task of current head, find previous task
                    bool found_prev = false;
                    for (int h = habit_data.selected_head - 1; h >= 0; h--) {
                        if (habit_data.heads[h].task_count > 0) {
                            habit_data.selected_head = h;
                            habit_data.selected_task = habit_data.heads[h].task_count - 1;
                            found_prev = true;
                            break;
                        }
                    }
                    if (!found_prev) { // loop from the end
                        for (int h = habit_data.head_count - 1; h >= 0; h--) {
                            if (habit_data.heads[h].task_count > 0) {
                                habit_data.selected_head = h;
                                habit_data.selected_task = habit_data.heads[h].task_count - 1;
                                break;
                            }
                        }
                    }
                }
                break;
            case KEY_DOWN:
                if (habit_data.selected_task < habit_data.heads[habit_data.selected_head].task_count - 1) {
                    habit_data.selected_task++;
                } else { // last task of current head, find next task
                    bool found_next = false;
                    for (int h = habit_data.selected_head + 1; h < habit_data.head_count; h++) {
                        if (habit_data.heads[h].task_count > 0) {
                            habit_data.selected_head = h;
                            habit_data.selected_task = 0;
                            found_next = true;
                            break; 
                        }
                    }
                    if (!found_next) { // loop from the beginning
                        for (int h = 0; h < habit_data.head_count; h++) {
                            if (habit_data.heads[h].task_count > 0) {
                                habit_data.selected_head = h;
                                habit_data.selected_task = 0;
                                break;
                            }
                        }
                    }
                }
                break;
            case ' ':
                // toggle task completion
                if (habit_data.selected_head >= 0 && habit_data.selected_task >= 0) {
                    int head_idx = habit_data.selected_head;
                    int task_idx = habit_data.selected_task;
                    Task* task = &habit_data.heads[head_idx].tasks[task_idx];
                    
                    task->done_today = !task->done_today;
                    if (task->done_today) {
                        task->streak++;
                    } else {
                        if (task->streak > 0) {
                            task->streak--;
                        }
                    }
                }
                break;
        }
    }
}

static int parse_csv_line(char *line, char **fields, int max_fields) {
    int field_count = 0;
    char *p = line;
    
    while (*p && field_count < max_fields) {
        char *start;
        if (*p == '"') {
            p++; // Skip "
            start = p;
            while (*p && *p != '"') {
                p++;
            }
            if (*p == '"') {
                *p = '\0';
                p++;
            }
        } else {
            start = p;
            while (*p && *p != ',') {
                p++;
            }
        }
        
        if (*p == ',') {
            *p = '\0';
            p++;
        }
        
        fields[field_count++] = start;
    }
    
    return field_count;
}

int habits_load(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        return 1;
    }

    char line[512];
    habits_init(); 

    // Skip header
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return 0;
    }

    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) == 0) continue;

        char *fields[4];
        int num_fields = parse_csv_line(line, fields, 4);

        if (num_fields < 2) continue;
        
        char* head_name = fields[0];
        char* task_name = fields[1];
        char* streak_str = (num_fields > 2) ? fields[2] : "0";
        char* done_today_str = (num_fields > 3) ? fields[3] : "0";

        int head_idx = -1;
        for (int i = 0; i < habit_data.head_count; i++) {
            if (strcmp(habit_data.heads[i].name, head_name) == 0) {
                head_idx = i;
                break;
            }
        }

        if (head_idx == -1) {
            if (habit_data.head_count >= MAX_HEADS) continue;
            head_idx = habit_data.head_count;
            HabitHead* new_head = &habit_data.heads[head_idx];
            strncpy(new_head->name, head_name, MAX_NAME_LENGTH - 1);
            new_head->name[MAX_NAME_LENGTH-1] = '\0';
            new_head->task_count = 0;
            habit_data.head_count++;
        }

        if (strlen(task_name) == 0) {
            continue;
        }

        HabitHead* head = &habit_data.heads[head_idx];
        if (head->task_count < MAX_TASKS_PER_HEAD) {
            int task_idx = head->task_count;
            Task* task = &head->tasks[task_idx];
            strncpy(task->name, task_name, MAX_NAME_LENGTH - 1);
            task->name[MAX_NAME_LENGTH - 1] = '\0';
            task->streak = atoi(streak_str);
            task->done_today = atoi(done_today_str);
            task->id = task_idx + 1;
            head->task_count++;
        }
    }

    fclose(file);
    return 0;
}

int habits_save(const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        return 1;
    }

    fprintf(file, "head_name,task_name,streak,done_today\n");

    for (int h = 0; h < habit_data.head_count; h++) {
        if (habit_data.heads[h].task_count == 0) {
            fprintf(file, "\"%s\",\"\",0,0\n", habit_data.heads[h].name);
        } else {
            for (int t = 0; t < habit_data.heads[h].task_count; t++) {
                fprintf(file, "\"%s\",\"%s\",%d,%d\n",
                        habit_data.heads[h].name,
                        habit_data.heads[h].tasks[t].name,
                        habit_data.heads[h].tasks[t].streak,
                        (int)habit_data.heads[h].tasks[t].done_today);
            }
        }
    }

    fclose(file);
    return 0;
}

void habits_daily_update(void) {
    for (int h = 0; h < habit_data.head_count; h++) {
        for (int t = 0; t < habit_data.heads[h].task_count; t++) {
            Task* task = &habit_data.heads[h].tasks[t];
            if (!task->done_today) {
                task->streak = 0;
            }
            task->done_today = false;
        }
    }
}

void habits_toggle_today(int head_idx, int task_idx) {
    if (head_idx >= 0 && head_idx < habit_data.head_count &&
        task_idx >= 0 && task_idx < habit_data.heads[head_idx].task_count) {
        habit_data.heads[head_idx].tasks[task_idx].done_today = 
            !habit_data.heads[head_idx].tasks[task_idx].done_today;
    }
} 