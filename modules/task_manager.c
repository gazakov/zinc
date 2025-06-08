#include "task_manager.h"
#include "imodule.h"
#include "../src/minimal_tui.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static TaskManagerData task_data;

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
    touchwin(parent_win); 
    wnoutrefresh(parent_win);

    return strlen(buffer) > 0;
}

static void ensure_task_selected() {
    if (task_data.selected_task != -1 && task_data.head_count > 0 && task_data.heads[task_data.selected_head].task_count > 0) return;
    if (task_data.head_count == 0) return;

    for (int i = 0; i < task_data.head_count; i++) {
        if (task_data.heads[i].task_count > 0) {
            task_data.selected_head = i;
            task_data.selected_task = 0;
            return;
        }
    }
}


void tasks_init() {
    task_data.head_count = 1; // head 0 is for standalone tasks
    task_data.heads[0].task_count = 0;
    strcpy(task_data.heads[0].name, "");

    task_data.selected_head = 0;
    task_data.selected_task = -1;
    task_data.edit_mode = false;
    task_data.move_mode = false;
}

void tasks_cleanup() {
    // nothing to clean up
}

void tasks_module_render(struct IModule* self, WINDOW* win) {
    werase(win);
    // box(win, 0, 0);
    mvwprintw(win, 1, 2, "Tasks %s", task_data.edit_mode ? "[EDIT MODE]" : "");

    int y = 3;
    for (int h = 0; h < task_data.head_count; h++) {
        // Don't render a header for standalone tasks (h==0)
        if (h > 0) {
            if (h == task_data.selected_head && task_data.selected_task == -1) {
                wattron(win, A_REVERSE);
            }
            mvwprintw(win, y++, 2, "%s:", task_data.heads[h].name);
            if (h == task_data.selected_head && task_data.selected_task == -1) {
                wattroff(win, A_REVERSE);
            }
        }

        for (int t = 0; t < task_data.heads[h].task_count; t++) {
            bool is_selected = (h == task_data.selected_head && t == task_data.selected_task);
            TaskItem* task = &task_data.heads[h].tasks[t];

            if (is_selected) wattron(win, A_REVERSE);
            if (task->completed) wattron(win, A_DIM);
            
            int x_offset = (h > 0) ? 4 : 2;
            mvwprintw(win, y, x_offset, "[%c] %s", task->completed ? 'X' : ' ', task->description);

            if (task->completed) {
                 int task_len = strlen(task->description);
                 for (int i = 0; i < task_len; i++) {
                    mvwaddch(win, y, x_offset + 4 + i, '-');
                 }
            }

            if (is_selected) wattroff(win, A_REVERSE);
            if (task->completed) wattroff(win, A_DIM);
            y++;
        }
        
        if (h < task_data.head_count - 1) {
            y++; 
        }
    }

    int max_y = getmaxy(win);
    if (task_data.edit_mode) {
        int help_y = max_y - 8;
        if (help_y < y) help_y = y;
        mvwprintw(win, help_y++, 2, "Edit Mode %s:", task_data.move_mode ? "[MOVING]" : "");
        mvwprintw(win, help_y++, 4, "R+U: New Head");
        mvwprintw(win, help_y++, 4, "R+I: New Task");
        mvwprintw(win, help_y++, 4, "X: Delete Item");
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

void tasks_module_handle_input(struct IModule* self, int ch, struct MinimalTui* tui) {
    if (task_data.edit_mode) {
        if (task_data.move_mode) {
             if (ch == 's' || ch == 'S' || ch == 27) {
                task_data.move_mode = false;
            } else if (ch == KEY_UP) {
                if (task_data.selected_task == -1) { // moving a head
                    if (task_data.selected_head > 0) {
                        TaskHead temp = task_data.heads[task_data.selected_head];
                        task_data.heads[task_data.selected_head] = task_data.heads[task_data.selected_head - 1];
                        task_data.heads[task_data.selected_head - 1] = temp;
                        task_data.selected_head--;
                    }
                } else { // moving a task
                    int head_idx = task_data.selected_head;
                    int task_idx = task_data.selected_task;
                    TaskHead* current_head = &task_data.heads[head_idx];

                    if (task_idx > 0) { // move up within the same head
                        TaskItem temp = current_head->tasks[task_idx];
                        current_head->tasks[task_idx] = current_head->tasks[task_idx - 1];
                        current_head->tasks[task_idx - 1] = temp;
                        task_data.selected_task--;
                    } else if (task_idx == 0 && head_idx > 0) { // move task to the previous head
                        TaskHead* prev_head = &task_data.heads[head_idx - 1];
                        if (prev_head->task_count < MAX_TASKS_PER_HEAD) {
                            TaskItem task_to_move = current_head->tasks[task_idx];
                            
                            prev_head->tasks[prev_head->task_count] = task_to_move;
                            prev_head->task_count++;
                            
                            memmove(&current_head->tasks[0], &current_head->tasks[1], (current_head->task_count - 1) * sizeof(TaskItem));
                            current_head->task_count--;
                            
                            task_data.selected_head--;
                            task_data.selected_task = prev_head->task_count - 1;

                            if (current_head->task_count == 0) {
                                task_data.selected_task = -1;
                            }
                        }
                    }
                }
            } else if (ch == KEY_DOWN) {
                if (task_data.selected_task == -1) { // moving a head
                    if (task_data.selected_head < task_data.head_count - 1) {
                        TaskHead temp = task_data.heads[task_data.selected_head];
                        task_data.heads[task_data.selected_head] = task_data.heads[task_data.selected_head + 1];
                        task_data.heads[task_data.selected_head + 1] = temp;
                        task_data.selected_head++;
                    }
                } else { // moving a task
                    int head_idx = task_data.selected_head;
                    int task_idx = task_data.selected_task;
                    TaskHead* current_head = &task_data.heads[head_idx];

                    if (task_idx < current_head->task_count - 1) { // move down within the same head
                        TaskItem temp = current_head->tasks[task_idx];
                        current_head->tasks[task_idx] = current_head->tasks[task_idx + 1];
                        current_head->tasks[task_idx + 1] = temp;
                        task_data.selected_task++;
                    } else if (task_idx == current_head->task_count - 1 && head_idx < task_data.head_count - 1) { // move task to the next head
                        TaskHead* next_head = &task_data.heads[head_idx + 1];
                        if (next_head->task_count < MAX_TASKS_PER_HEAD) {
                            TaskItem task_to_move = current_head->tasks[task_idx];

                            memmove(&next_head->tasks[1], &next_head->tasks[0], next_head->task_count * sizeof(TaskItem));
                            
                            next_head->tasks[0] = task_to_move;
                            next_head->task_count++;
                            
                            current_head->task_count--;
                            
                            task_data.selected_head++;
                            task_data.selected_task = 0;
                        }
                    }
                }
            }
            return;
        }

        switch(ch) {
            case 'E': case 'e': {
                int next_ch = getch();
                if (next_ch == 'I' || next_ch == 'i') {
                    task_data.edit_mode = false;
                    ensure_task_selected();
                }
                break;
            }
            case 'R': case 'r': {
                int next_ch = getch();
                if (next_ch == 'U' || next_ch == 'u') { // new head
                     if (task_data.head_count < MAX_HEADS) {
                        char new_head_name[MAX_NAME_LENGTH] = {0};
                        if (prompt_for_string(tui->wm.panel_win, "Enter head name: ", new_head_name, MAX_NAME_LENGTH)) {
                            task_data.heads[task_data.head_count].id = task_data.head_count + 1;
                            strncpy(task_data.heads[task_data.head_count].name, new_head_name, MAX_NAME_LENGTH - 1);
                            task_data.heads[task_data.head_count].task_count = 0;
                            task_data.head_count++;
                            task_data.selected_head = task_data.head_count - 1;
                            task_data.selected_task = -1;
                        }
                    }
                } else if (next_ch == 'I' || next_ch == 'i') { // new task
                    int head_idx = task_data.selected_head;
                    if (head_idx < 0 && task_data.head_count > 0) head_idx = 0;
                    if (head_idx < 0) break;

                    TaskHead* head = &task_data.heads[head_idx];

                    if (head->task_count < MAX_TASKS_PER_HEAD) {
                        int insert_pos = (task_data.selected_task == -1) ? 0 : task_data.selected_task + 1;

                        char new_task_desc[128] = {0};
                        if (prompt_for_string(tui->wm.panel_win, "Enter task description: ", new_task_desc, 128)) {
                            if (insert_pos < head->task_count) {
                                memmove(&head->tasks[insert_pos + 1], 
                                        &head->tasks[insert_pos], 
                                        (head->task_count - insert_pos) * sizeof(TaskItem));
                            }
                            
                            head->task_count++;
                            TaskItem* new_task = &head->tasks[insert_pos];
                            strncpy(new_task->description, new_task_desc, 128 - 1);
                            new_task->description[128 - 1] = '\0';
                            new_task->id = head->task_count;
                            new_task->completed = false;

                            task_data.selected_task = insert_pos;
                        }
                    }
                }
                task_data.move_mode = false;
                ensure_task_selected();
                break;
            }
            case 's': case 'S':
                if (task_data.head_count > 0) task_data.move_mode = true;
                break;
            case 'x': case 'X':
                 if (task_data.selected_task == -1) { // a head is selected
                    if (task_data.head_count > 0 && task_data.selected_head > 0) {
                        int head_to_delete = task_data.selected_head;
                        
                        if (head_to_delete < task_data.head_count - 1) {
                            memmove(&task_data.heads[head_to_delete], 
                                    &task_data.heads[head_to_delete + 1], 
                                    (task_data.head_count - head_to_delete - 1) * sizeof(TaskHead));
                        }
                        task_data.head_count--;

                        if (task_data.selected_head >= task_data.head_count && task_data.head_count > 0) {
                            task_data.selected_head = task_data.head_count - 1;
                        } else if (task_data.head_count == 0) {
                            task_data.selected_head = 0;
                            task_data.selected_task = -1;
                        }
                    }
                } else { // a task is selected
                    int head_idx = task_data.selected_head;
                    if (head_idx >= 0 && task_data.heads[head_idx].task_count > 0) {
                        int task_to_delete = task_data.selected_task;

                        if (task_to_delete < task_data.heads[head_idx].task_count - 1) {
                            memmove(&task_data.heads[head_idx].tasks[task_to_delete], 
                                    &task_data.heads[head_idx].tasks[task_to_delete + 1], 
                                    (task_data.heads[head_idx].task_count - task_to_delete - 1) * sizeof(TaskItem));
                        }
                        task_data.heads[head_idx].task_count--;

                        if (task_data.selected_task >= task_data.heads[head_idx].task_count) {
                            task_data.selected_task = task_data.heads[head_idx].task_count - 1;
                        }
                         if (task_data.heads[head_idx].task_count == 0) {
                            task_data.selected_task = -1;
                        }
                    }
                }
                tasks_save("data/tasks.csv");
                break;
            case KEY_UP:
                 if (task_data.selected_task > 0) {
                    task_data.selected_task--;
                } else if (task_data.selected_task == 0) {
                    task_data.selected_task = -1; // select head
                } else { // a head is selected
                    if (task_data.selected_head > 0) {
                        task_data.selected_head--;
                    } else {
                        task_data.selected_head = task_data.head_count - 1;
                    }
                    if (task_data.head_count > 0 && task_data.heads[task_data.selected_head].task_count > 0) {
                        task_data.selected_task = task_data.heads[task_data.selected_head].task_count - 1;
                    } else {
                        task_data.selected_task = -1;
                    }
                }
                break;
            case KEY_DOWN:
                if (task_data.selected_task == -1) { // head is selected
                    if (task_data.head_count > 0 && task_data.heads[task_data.selected_head].task_count > 0) {
                        task_data.selected_task = 0;
                    } else {
                        if (task_data.selected_head < task_data.head_count - 1) {
                            task_data.selected_head++;
                        } else {
                            task_data.selected_head = 0; // loop
                        }
                        task_data.selected_task = -1;
                    }
                } else if (task_data.selected_task < task_data.heads[task_data.selected_head].task_count - 1) {
                    task_data.selected_task++;
                } else { // last task
                    if (task_data.selected_head < task_data.head_count - 1) {
                        task_data.selected_head++;
                    } else {
                        task_data.selected_head = 0; // loop
                    }
                    task_data.selected_task = -1;
                }
                break;
            case 27: // esc
                task_data.edit_mode = false;
                task_data.move_mode = false;
                break;
        }
    } else {
        // normal mode
        int total_tasks = 0;
        for (int i = 0; i < task_data.head_count; i++) {
            total_tasks += task_data.heads[i].task_count;
        }
        if (total_tasks == 0) {
             if (ch == 'E' || ch == 'e') {
                int next_ch = getch();
                if (next_ch == 'I' || next_ch == 'i') task_data.edit_mode = true;
            }
            return;
        }

        ensure_task_selected();
        
        switch (ch) {
            case 'E': case 'e': {
                int next_ch = getch();
                if (next_ch == 'I' || next_ch == 'i') task_data.edit_mode = true;
                break;
            }
            case KEY_UP:
                if (task_data.selected_task > 0) {
                    task_data.selected_task--;
                } else { // first task, find prev head with tasks
                    bool found_prev = false;
                    for (int h = task_data.selected_head - 1; h >= 0; h--) {
                        if (task_data.heads[h].task_count > 0) {
                            task_data.selected_head = h;
                            task_data.selected_task = task_data.heads[h].task_count - 1;
                            found_prev = true;
                            break;
                        }
                    }
                    if (!found_prev) { // loop around
                         for (int h = task_data.head_count - 1; h >= 0; h--) {
                            if (task_data.heads[h].task_count > 0) {
                                task_data.selected_head = h;
                                task_data.selected_task = task_data.heads[h].task_count - 1;
                                break;
                            }
                        }
                    }
                }
                break;
            case KEY_DOWN:
                if (task_data.selected_task < task_data.heads[task_data.selected_head].task_count - 1) {
                    task_data.selected_task++;
                } else { // last task, find next head with tasks
                    bool found_next = false;
                    for (int h = task_data.selected_head + 1; h < task_data.head_count; h++) {
                        if (task_data.heads[h].task_count > 0) {
                            task_data.selected_head = h;
                            task_data.selected_task = 0;
                            found_next = true;
                            break; 
                        }
                    }
                    if (!found_next) { // loop around
                        for (int h = 0; h < task_data.head_count; h++) {
                            if (task_data.heads[h].task_count > 0) {
                                task_data.selected_head = h;
                                task_data.selected_task = 0;
                                break;
                            }
                        }
                    }
                }
                break;
            case ' ':
                if (task_data.selected_head >= 0 && task_data.selected_task >= 0) {
                    task_data.heads[task_data.selected_head].tasks[task_data.selected_task].completed = !task_data.heads[task_data.selected_head].tasks[task_data.selected_task].completed;
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


int tasks_load(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) return 1;

    char line[512];
    tasks_init(); 

    // Skip header
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return 0;
    }

    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = 0; 
        if (strlen(line) == 0) continue;

        char* fields[3];
        int num_fields = parse_csv_line(line, fields, 3);

        if (num_fields < 2) continue;

        char* head_name = fields[0];
        char* description = fields[1];
        char* completed_str = (num_fields > 2) ? fields[2] : "0";

        int head_idx = -1;
        if (strlen(head_name) == 0) {
            head_idx = 0;
        } else {
            for (int i = 1; i < task_data.head_count; i++) {
                if (strcmp(task_data.heads[i].name, head_name) == 0) {
                    head_idx = i;
                    break;
                }
            }
        }

        if (head_idx == -1) {
            if (task_data.head_count >= MAX_HEADS) continue;
            head_idx = task_data.head_count;
            TaskHead* new_head = &task_data.heads[head_idx];
            strncpy(new_head->name, head_name, MAX_NAME_LENGTH - 1);
            new_head->name[MAX_NAME_LENGTH - 1] = '\0';
            new_head->task_count = 0;
            task_data.head_count++;
        }
        
        if (strlen(description) == 0) {
            continue;
        }

        TaskHead* head = &task_data.heads[head_idx];
        if (head->task_count < MAX_TASKS_PER_HEAD) {
            TaskItem* task = &head->tasks[head->task_count];
            strncpy(task->description, description, 128 - 1);
            task->description[128 - 1] = '\0';
            task->completed = atoi(completed_str);
            head->task_count++;
        }
    }

    fclose(file);
    return 0;
}

int tasks_save(const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) return 1;

    fprintf(file, "head_name,description,completed\n");

    for (int h = 0; h < task_data.head_count; h++) {
        if (task_data.heads[h].task_count == 0) {
            if (h > 0) {
                fprintf(file, "\"%s\",\"\",0\n", task_data.heads[h].name);
            }
        } else {
            for (int t = 0; t < task_data.heads[h].task_count; t++) {
                fprintf(file, "\"%s\",\"%s\",%d\n",
                        task_data.heads[h].name,
                        task_data.heads[h].tasks[t].description,
                        (int)task_data.heads[h].tasks[t].completed);
            }
        }
    }

    fclose(file);
    return 0;
} 