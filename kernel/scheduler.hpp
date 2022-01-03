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

typedef void (*TaskEntry)(void *);

enum TaskState : uint32_t {
	TASK_EMPTY,
	TASK_NEW,
	TASK_RUN,
	TASK_RUNNING,
	TASK_WAIT_INT,
};

struct TaskSlot {
	TaskBlock *task;
	TaskState state;
	void init();
};

struct Scheduler {
	TaskSlot core_task;
	TaskSlot *active_task;
	TaskList *task_lists;
	void init();
	void run();
	void start(TaskEntry entry, void *param);
	void wake_tasks(uint32_t int_mask, uint32_t offset);
};

extern Scheduler scheduler;

#endif

