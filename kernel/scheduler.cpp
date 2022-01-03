/* ***
 * Copyright (c) 2021  Meisaka Yukara
 * See LICENSE for full details
 * e6af70ca
 */

#include "scheduler.hpp"
#include "kio.hpp"
#include "ktext.hpp"

struct TaskBlock {
	BaseRegisterSave reg_save;
	uint32_t *stack_base;
	// TODO memory spaces (CR3)
	// TODO other paging info
	// TODO extra registers (i.e. x86/x64 XSAVE)
	// TODO task IDs
	// TODO permissions (for userspace)
	// TODO user IDs (for userspace)
	// TODO local storage addresses (x86's FS/GS)
	void init() {
		memset(&reg_save, 0, sizeof(reg_save));
		stack_base = (uint32_t*)mem::vmm_request(0x10000, nullptr, 0,
			mem::RQ_ALLOC | mem::RQ_RW);
	}
	void start(TaskEntry entry) {
		reg_save.r_eip = (uint32_t)entry;
		reg_save.r_eflags = _ix_eflags();
		reg_save.r_cs = 0x10;
		reg_save.r_esp = (uint32_t)stack_base;
	}
};

enum TaskState : uint32_t {
	TASK_EMPTY,
	TASK_NEW,
	TASK_RUN,
	TASK_RUNNING,
};

struct TaskSlot {
	TaskBlock *task;
	TaskState state;
	void init() {
		task = nullptr;
		state = TASK_EMPTY;
	}
};

struct TaskList {
	static const size_t TASK_LIMIT = 32; // TODO figure out a better number
	TaskSlot tasks[TASK_LIMIT];
	void init() {
		memset(tasks, 0, sizeof(tasks));
		for(size_t i = 0; i < TASK_LIMIT; i++) {
			tasks[i].init();
		}
	}
	TaskSlot* get_free() {
		for(size_t i = 0; i < TASK_LIMIT; i++) {
			if(_ixa_cmpxchg((uint32_t*)&tasks[i].state, TASK_EMPTY, TASK_NEW)) {
				return &tasks[i];
			}
		}
		xiv::printf("no task slots %x\n", this->tasks);
		for(size_t i = 0; i < TASK_LIMIT; i++) {
			xiv::printf("TaskSlot: %03d: %x\n", i, &tasks[i].state);
		}
		return nullptr;
	}
};

void Scheduler::run() {
	size_t tasks_run = 1;
	while(tasks_run) {
		tasks_run = 0;
		for(size_t i = 0; i < TaskList::TASK_LIMIT; i++) {
			auto &slot = task_lists->tasks[i];
			if(_ixa_cmpxchg((uint32_t*)&slot.state, TASK_RUN, TASK_RUNNING)) {
				//xiv::printf("scheduler: switching task %d\n", i);
				_ix_cli();
				tasks_run++;
				active_task = slot.task;
				_ix_task_switch(core_task, slot.task);
				if(!_ixa_cmpxchg((uint32_t*)&slot.state, TASK_RUNNING, TASK_RUN)) {
				}
				_ix_sti();
			}
		}
	}
	if(!tasks_run) {
		xiv::printf("scheduler: no runnable tasks\n");
	}
}

void Scheduler::start(TaskEntry entry) {
	TaskSlot *slot = task_lists->get_free();
	if(!slot) {
		xiv::printf("scheduler: no free slots\n");
		return;
	}
	TaskBlock *task = (TaskBlock*)mem::vmm_request(sizeof(TaskBlock),
		nullptr, 0, mem::RQ_ALLOC | mem::RQ_RW);
	task->init();
	slot->task = task;
	task->start(entry);
	slot->state = TASK_RUN;
}

void Scheduler::init() {
	core_task = (TaskBlock*)mem::vmm_request(sizeof(TaskBlock),
		nullptr, 0, mem::RQ_ALLOC | mem::RQ_RW);
	task_lists = (TaskList*)kmalloc(sizeof(TaskList));
	task_lists->init();
	active_task = nullptr;
}

//__attribute__((noreturn))
void scheduler_from_interrupt(IntCtx *ctx) {
	if(!scheduler.active_task) return;
	TaskBlock *active = scheduler.active_task;
	scheduler.active_task = scheduler.core_task;
	_ix_task_switch_from_int(ctx, active, scheduler.core_task);
}

