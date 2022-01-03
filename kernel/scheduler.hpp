/* ***
 * Copyright (c) 2021  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
 */
#ifndef SCHEDULER_HAI
#define SCHEDULER_HAI
#include "interrupt.hpp"
#include "memory.hpp"

struct TaskList;
struct TaskBlock;

typedef void (*TaskEntry)();

struct Scheduler {
	TaskBlock *core_task;
	TaskBlock *active_task;
	TaskList *task_lists;
	void init();
	void run();
	void start(TaskEntry entry);
};

extern Scheduler scheduler;

#endif

