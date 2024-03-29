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
	uint32_t wait_ints[4];
	// TODO memory spaces (CR3)
	// TODO other paging info
	// TODO extra registers (i.e. x86/x64 XSAVE)
	// TODO task IDs
	// TODO permissions (for userspace)
	// TODO user IDs (for userspace)
	// TODO local storage addresses (x86's FS/GS)
	void init() {
		memset(&reg_save, 0, sizeof(reg_save));
		constexpr size_t stack_size = 0x10000;
		stack_base = (uint32_t*)mem::vmm_request(stack_size, nullptr, 0,
			mem::RQ_ALLOC | mem::RQ_RW);
		reg_save.r_esp = ((uint32_t)stack_base) + stack_size;
		memset(wait_ints, 0, sizeof(wait_ints));
	}
	void start(TaskEntry entry, void *param) {
		reg_save.r_eip = (uint32_t)entry;
		reg_save.r_eflags = _ix_eflags();
		reg_save.r_cs = 0x10;
		uint32_t *new_stack = (uint32_t*)reg_save.r_esp;
		*(--new_stack) = (uint32_t)param;
		*(--new_stack) = 0;
		reg_save.r_esp = (uint32_t)new_stack;
	}
};

void TaskSlot::init() {
	task = nullptr;
	state = TASK_EMPTY;
}

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
				_ix_cli();
				tasks_run++;
				active_task = &slot;
				//xiv::printf("switching task %x > %x %x\n", core_task.task, slot.task, slot.task->reg_save.r_cs);
				_ix_task_switch(core_task.task, slot.task);
				if(!_ixa_cmpxchg((uint32_t*)&slot.state, TASK_RUNNING, TASK_RUN)) {
				}
				_ix_sti();
			}
		}
	}
	if(!tasks_run) {
		//xiv::printf("scheduler: no runnable tasks\n");
	}
}

void Scheduler::start(TaskEntry entry, void *param) {
	TaskSlot *slot = task_lists->get_free();
	if(!slot) {
		xiv::printf("scheduler: no free slots\n");
		return;
	}
	TaskBlock *task = (TaskBlock*)mem::vmm_request(sizeof(TaskBlock),
		nullptr, 0, mem::RQ_ALLOC | mem::RQ_RW);
	task->init();
	slot->task = task;
	task->start(entry, param);
	slot->state = TASK_RUN;
}

void Scheduler::wake_tasks(uint32_t int_mask, uint32_t offset) {
	for(size_t i = 0; i < TaskList::TASK_LIMIT; i++) {
		auto &slot = task_lists->tasks[i];
		if(slot.state == TASK_WAIT_INT) {
			if(slot.task->wait_ints[offset] & int_mask) {
				slot.state = TASK_RUN;
			}
		}
	}
}

void Scheduler::init() {
	core_task.task = (TaskBlock*)mem::vmm_request(sizeof(TaskBlock),
		nullptr, 0, mem::RQ_ALLOC | mem::RQ_RW);
	task_lists = (TaskList*)kmalloc(sizeof(TaskList));
	task_lists->init();
	active_task = nullptr;
}

void wake_on_interrupt(uint8_t inum) {
	_ix_cli();
	TaskSlot *active = scheduler.active_task;
	if(inum >= 0x60) {
		active->task->wait_ints[3] |= 1 << (inum - 0x60);
	} else if(inum >= 0x40) {
		active->task->wait_ints[2] |= 1 << (inum - 0x40);
	} else if(inum >= 0x20) {
		active->task->wait_ints[1] |= 1 << (inum - 0x20);
	} else {
		active->task->wait_ints[0] |= 1 << inum;
	}
	_ix_sti();
}

void wait_for_interrupts() {
	_ix_cli();
	TaskSlot *active = scheduler.active_task;
	active->state = TASK_WAIT_INT;
	scheduler.active_task = &scheduler.core_task;
	_ix_task_switch(active->task, scheduler.core_task.task);
	_ix_sti();
}

//__attribute__((noreturn))
void scheduler_from_interrupt(IntCtx *ctx, uint8_t inum) {
	if(!scheduler.active_task) return;
	uint32_t int_mask = 0;
	if(inum >= 0x60) {
		int_mask = 1 << (inum - 0x60);
		scheduler.wake_tasks(int_mask, 3);
	} else if(inum >= 0x40) {
		int_mask = 1 << (inum - 0x40);
		scheduler.wake_tasks(int_mask, 2);
	} else if(inum >= 0x20) {
		int_mask = 1 << (inum - 0x20);
		scheduler.wake_tasks(int_mask, 1);
	} else {
		int_mask = 1 << inum;
		scheduler.wake_tasks(int_mask, 0);
	}
	if(inum != 0x28) return;
	TaskSlot *active = scheduler.active_task;
	scheduler.active_task = &scheduler.core_task;
	_ix_task_switch_from_int(ctx, active->task, scheduler.core_task.task);
}

