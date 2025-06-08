#ifndef STRUCTS_H
#define STRUCTS_H

#include <stdbool.h> // for bool type in c structs

#define MAX_NAME_LENGTH 48
#define MAX_TASKS_PER_HEAD 12
#define MAX_HEADS 8

typedef struct {
    int id;
    char name[MAX_NAME_LENGTH];
    int streak;
    bool done_today;
} Task;

typedef struct {
    int id;
    char name[MAX_NAME_LENGTH];
    Task tasks[MAX_TASKS_PER_HEAD];
    int task_count;
} HabitHead;

typedef struct {
    HabitHead heads[MAX_HEADS];
    int head_count;
    int selected_head;
    int selected_task;
    bool edit_mode;
    bool move_mode;
} HabitData;

typedef struct {
    int id;
    char description[128];
    bool completed;
} TaskItem;

typedef struct {
    int id;
    char name[MAX_NAME_LENGTH];
    TaskItem tasks[MAX_TASKS_PER_HEAD];
    int task_count;
} TaskHead;

typedef struct {
    TaskHead heads[MAX_HEADS];
    int head_count;
    int selected_head;
    int selected_task; // -1 if head is selected
    bool edit_mode;
    bool move_mode;
} TaskManagerData;

typedef struct {
    int id;
    char name[MAX_NAME_LENGTH];
    int streak;
    bool done_today; // relies on <stdbool.h> for c, or c++'s bool
} Habit_c; // c-style for compatibility

#endif // STRUCTS_H