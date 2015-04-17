#include <inc/assert.h>
#include <inc/x86.h>
#include <kern/spinlock.h>
#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>

void sched_halt(void);

// Choose a user environment to run and run it.
void
sched_yield(void)
{
	struct Env *idle;

	// Implement simple round-robin scheduling.
	//
	// Search through 'envs' for an ENV_RUNNABLE environment in
	// circular fashion starting just after the env this CPU was
	// last running.  Switch to the first such environment found.
	//
	// If no envs are runnable, but the environment previously
	// running on this CPU is still ENV_RUNNING, it's okay to
	// choose that environment.
	//
	// Never choose an environment that's currently running on
	// another CPU (env_status == ENV_RUNNING). If there are
	// no runnable environments, simply drop through to the code
	// below to halt the cpu.

	// LAB 4: Your code here.
	//cprintf("in sched\n"); //debug
	struct Env * next;
	if(curenv == NULL) {
		//cprintf("curenv is NULL\n");
		next = &envs[0];
	}
	else {
		next = curenv + 1;
	}
	int times = 0;
	while(times < NENV) {
		times++;
		if(next >= (envs + NENV)) {
			next = envs;
		}
		//cprintf("debug i is %d curenv is %d\n", next - envs, curenv == NULL ? -1 : curenv - envs); //debug
		assert(next != NULL && (next - envs) >= 0 && (next - envs) < NENV);
		if(next->env_status == ENV_RUNNABLE) {
			break;
		}
		next++;
	}
	//cprintf("break the while\n");
	if(next != NULL) {
		//cprintf("debug\n");
		if(next->env_status == ENV_RUNNABLE){
			//cprintf("choose %d to run\n", next->env_id);
			env_run(next);
		}
		else if (curenv && curenv->env_status == ENV_RUNNING) {
			env_run(curenv);
		}
	}
	// sched_halt never returns
	//cprintf("no runnable??\n");	//debug
	sched_halt();
}

// Halt this CPU when there is nothing to do. Wait until the
// timer interrupt wakes it up. This function never returns.
//
void
sched_halt(void)
{
	int i;
	//cprintf("in sched halt\n");
	// For debugging and testing purposes, if there are no runnable
	// environments in the system, then drop into the kernel monitor.
	for (i = 0; i < NENV; i++) {
		/*
		cprintf("i is %d in %p, status is %s\n", i, &envs[i],\
		 (char *[]){"FREE", "DYING", "RUNNABLE", "RUNNING", "NOT RUNABLE"}[envs[i].env_status]);
		 */
		if ((envs[i].env_status == ENV_RUNNABLE ||
		     envs[i].env_status == ENV_RUNNING ||
		     envs[i].env_status == ENV_DYING))
			break;
	}
	if (i == NENV) {
		cprintf("No runnable environments in the system!\n");
		while (1)
			monitor(NULL);
	}
	/*
	else {
		cprintf("there is a %s env\n", \
		(char *[]){"FREE", "DYING", "RUNNABLE", "RUNNING", "NOT RUNABLE"}[envs[i].env_status]);
	}
	*/
	// Mark that no environment is running on this CPU
	curenv = NULL;
	lcr3(PADDR(kern_pgdir));

	// Mark that this CPU is in the HALT state, so that when
	// timer interupts come in, we know we should re-acquire the
	// big kernel lock
	xchg(&thiscpu->cpu_status, CPU_HALTED);

	// Release the big kernel lock as if we were "leaving" the kernel
	unlock_kernel();

	// Reset stack pointer, enable interrupts and then halt.
	asm volatile (
		"movl $0, %%ebp\n"
		"movl %0, %%esp\n"
		"pushl $0\n"
		"pushl $0\n"
		"sti\n"
		"1:\n"
		"hlt\n"
		"jmp 1b\n"
	: : "a" (thiscpu->cpu_ts.ts_esp0));
}

