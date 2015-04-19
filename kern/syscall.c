/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/syscall.h>
#include <kern/console.h>
#include <kern/sched.h>
#include <kern/time.h>
#include <kern/e1000.h>

/* print syscall's name */
inline char* get_syscall_name(uint32_t syscallno) {
	assert(syscallno <= (sizeof(syscall_name_table)/sizeof(syscall_name_table[0])));
	return syscall_name_table[syscallno];
}
// Print a string to the system console.
// The string is exactly 'len' characters long.
// Destroys the environment on memory errors.
static void
sys_cputs(const char *s, size_t len)
{
	// Check that the user has permission to read memory [s, s+len).
	// Destroy the environment if not.

	// LAB 3: Your code here.
	user_mem_assert(curenv, (void *)s, len, PTE_U);
	// Print the string supplied by the user.
	cprintf("%.*s", len, s);
}

// Read a character from the system console without blocking.
// Returns the character, or 0 if there is no input waiting.
static int
sys_cgetc(void)
{
	return cons_getc();
}

// Returns the current environment's envid.
static envid_t
sys_getenvid(void)
{
	return curenv->env_id;
}

// Destroy a given environment (possibly the currently running environment).
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_destroy(envid_t envid)
{
	int r;
	struct Env *e;

	if ((r = envid2env(envid, &e, 1)) < 0)
		return r;
	env_destroy(e);
	
	return 0;
}

// Deschedule current environment and pick a different one to run.
static void
sys_yield(void)
{
	sched_yield();
}

// Allocate a new environment.
// Returns envid of new environment, or < 0 on error.  Errors are:
//	-E_NO_FREE_ENV if no free environment is available.
//	-E_NO_MEM on memory exhaustion.
static envid_t
sys_exofork(void)
{
	// Create the new environment with env_alloc(), from kern/env.c.
	// It should be left as env_alloc created it, except that
	// status is set to ENV_NOT_RUNNABLE, and the register set is copied
	// from the current environment -- but tweaked so sys_exofork
	// will appear to return 0.

	// LAB 4: Your code here.
	/* int env_alloc(struct Env **newenv_store, envid_t parent_id) */
	struct Env *new_env = NULL;
	int r = env_alloc(&new_env, curenv->env_id);
	if(r == 0) { /* success */
		assert(new_env);
		new_env->env_status = ENV_NOT_RUNNABLE;
		memcpy( &(new_env->env_tf), &(curenv->env_tf), sizeof(struct Trapframe) );
		new_env->env_tf.tf_regs.reg_eax = 0;
		r = new_env->env_id;
	}

	return r;
}

// Set envid's env_status to status, which must be ENV_RUNNABLE
// or ENV_NOT_RUNNABLE.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if status is not a valid status for an environment.
static int
sys_env_set_status(envid_t envid, int status)
{
	// Hint: Use the 'envid2env' function from kern/env.c to translate an
	// envid to a struct Env.
	// You should set envid2env's third argument to 1, which will
	// check whether the current environment has permission to set
	// envid's status.

	// LAB 4: Your code here.
	assert(status == ENV_RUNNABLE || status == ENV_NOT_RUNNABLE);
	struct Env *target_env = NULL;
	int r = envid2env(envid, &target_env, true);
	if( r != 0 ) { /* fail */
		return r;
	}
	assert(target_env != NULL);
	assert(target_env->env_id == envid);
	target_env->env_status = status;
	return 0;
}

// Set envid's trap frame to 'tf'.
// tf is modified to make sure that user environments always run at code
// protection level 3 (CPL 3) with interrupts enabled.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf)
{
	// LAB 5: Your code here.
	// Remember to check whether the user has supplied us with a good
	// address!
	struct Env *env = NULL;
	int r = envid2env(envid, &env, true);
	if( r != 0 ) { /* fail */
		return r;
	}

	user_mem_assert(env, (void *)tf, sizeof (struct Trapframe), PTE_U | PTE_W | PTE_P);
	tf->tf_eflags |= FL_IF;
	tf->tf_ds = GD_UD | 3;
	tf->tf_es = GD_UD | 3;
	tf->tf_ss = GD_UD | 3;
	tf->tf_cs = GD_UT | 3;
	env->env_tf = *tf;
	return 0;
}

// Set the page fault upcall for 'envid' by modifying the corresponding struct
// Env's 'env_pgfault_upcall' field.  When 'envid' causes a page fault, the
// kernel will push a fault record onto the exception stack, then branch to
// 'func'.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
	// LAB 4: Your code here.
	struct Env *target_env = NULL;
	int r = envid2env(envid, &target_env, true);
	if( r != 0 ) { /* fail */
		return r;
	}
	assert(target_env != NULL);
	assert((envid == 0 && target_env == curenv) || target_env->env_id == envid);
	assert(func != NULL);
	target_env->env_pgfault_upcall = func;
	return 0;
}

// Allocate a page of memory and map it at 'va' with permission
// 'perm' in the address space of 'envid'.
// The page's contents are set to 0.
// If a page is already mapped at 'va', that page is unmapped as a
// side effect.
//
// perm -- PTE_U | PTE_P must be set, PTE_AVAIL | PTE_W may or may not be set,
//         but no other bits may be set.  See PTE_SYSCALL in inc/mmu.h.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
//	-E_INVAL if perm is inappropriate (see above).
//	-E_NO_MEM if there's no memory to allocate the new page,
//		or to allocate any necessary page tables.
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
	// Hint: This function is a wrapper around page_alloc() and
	//   page_insert() from kern/pmap.c.
	//   Most of the new code you write should be to check the
	//   parameters for correctness.
	//   If page_insert() fails, remember to free the page you
	//   allocated!

	// LAB 4: Your code here.
	if((uint32_t)va >= UTOP || ((uint32_t)va % PGSIZE) != 0) {
		return -E_INVAL;
	}

	if((perm & PTE_U) == 0 || (perm & PTE_P) == 0 || (perm & ~PTE_SYSCALL) != 0) {
		return -E_INVAL;
	}

	struct Env *target_env = NULL;
	int r = envid2env(envid, &target_env, true);
	if( r != 0 ) { /* fail */
		return r;
	}

	assert(target_env != NULL);
	assert(envid == 0 || target_env->env_id == envid);

	struct PageInfo *new_page = page_alloc(ALLOC_ZERO);
	if(new_page == NULL) {
		-E_NO_MEM;
	}
	/* int page_insert(pde_t *pgdir, struct PageInfo *pp, void *va, int perm) */
	return page_insert(target_env->env_pgdir, new_page, va, perm);
	//panic("sys_page_alloc not implemented");
}

// Map the page of memory at 'srcva' in srcenvid's address space
// at 'dstva' in dstenvid's address space with permission 'perm'.
// Perm has the same restrictions as in sys_page_alloc, except
// that it also must not grant write access to a read-only
// page.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if srcenvid and/or dstenvid doesn't currently exist,
//		or the caller doesn't have permission to change one of them.
//	-E_INVAL if srcva >= UTOP or srcva is not page-aligned,
//		or dstva >= UTOP or dstva is not page-aligned.
//	-E_INVAL is srcva is not mapped in srcenvid's address space.
//	-E_INVAL if perm is inappropriate (see sys_page_alloc).
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in srcenvid's
//		address space.
//	-E_NO_MEM if there's no memory to allocate any necessary page tables.
static int
sys_page_map(envid_t srcenvid, void *srcva,
	     envid_t dstenvid, void *dstva, int perm)
{
	// Hint: This function is a wrapper around page_lookup() and
	//   page_insert() from kern/pmap.c.
	//   Again, most of the new code you write should be to check the
	//   parameters for correctness.
	//   Use the third argument to page_lookup() to
	//   check the current permissions on the page.

	// LAB 4: Your code here.
	//panic("sys_page_map not implemented");
	struct Env *src_env = NULL;
	int r = envid2env(srcenvid, &src_env, true);
	if( r != 0 ) { /* fail */
		return r;
	}
	assert(src_env != NULL);
	assert(srcenvid == 0 || src_env->env_id == srcenvid);
	struct Env *dst_env = NULL;
	r = envid2env(dstenvid, &dst_env, true);
	if( r != 0 ) { /* fail */
		return r;
	}
	assert(dst_env != NULL);
	assert(dstenvid == 0 || dst_env->env_id == dstenvid);

	if((uint32_t)srcva >= UTOP || (uint32_t)dstva >= UTOP || \
		((uint32_t)srcva % PGSIZE) != 0 || ((uint32_t)dstva % PGSIZE) != 0) {
		//cprintf("debug 1\n");
		return -E_INVAL;
	}

	if(( perm & PTE_U) == 0 || (perm & PTE_P) == 0 || (perm & ~PTE_SYSCALL) != 0) {
		//cprintf("debug 2\n");
		return -E_INVAL;
	}
	/* struct PageInfo *page_lookup(pde_t *pgdir, void *va, pte_t **pte_store) */
	pte_t *src_ptep = NULL;
	struct PageInfo *pp = page_lookup(src_env->env_pgdir, srcva, &src_ptep);
	if(!pp || ((perm & PTE_W) && ((*src_ptep & PTE_W) != PTE_W))) {
		//cprintf("debug 3\n");
		return -E_INVAL;
	}

	return page_insert(dst_env->env_pgdir, pp, dstva, perm);
	//pte_t *dst_ptep = NULL;

}

// Unmap the page of memory at 'va' in the address space of 'envid'.
// If no page is mapped, the function silently succeeds.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
static int
sys_page_unmap(envid_t envid, void *va)
{
	// Hint: This function is a wrapper around page_remove().

	// LAB 4: Your code here.
	if((uint32_t)va >= UTOP || ((uint32_t)va % PGSIZE) != 0) {
		return -E_INVAL;
	}
	
	struct Env *target_env = NULL;
	int r = envid2env(envid, &target_env, true);
	if( r != 0 ) { /* fail */
		return r;
	}

	/* void page_remove(pde_t *pgdir, void *va) */
	page_remove(target_env->env_pgdir, va);
	return 0;
	//panic("sys_page_unmap not implemented");
}

// Try to send 'value' to the target env 'envid'.
// If srcva < UTOP, then also send page currently mapped at 'srcva',
// so that receiver gets a duplicate mapping of the same page.
//
// The send fails with a return value of -E_IPC_NOT_RECV if the
// target is not blocked, waiting for an IPC.
//
// The send also can fail for the other reasons listed below.
//
// Otherwise, the send succeeds, and the target's ipc fields are
// updated as follows:
//    env_ipc_recving is set to 0 to block future sends;
//    env_ipc_from is set to the sending envid;
//    env_ipc_value is set to the 'value' parameter;
//    env_ipc_perm is set to 'perm' if a page was transferred, 0 otherwise.
// The target environment is marked runnable again, returning 0
// from the paused sys_ipc_recv system call.  (Hint: does the
// sys_ipc_recv function ever actually return?)
//
// If the sender wants to send a page but the receiver isn't asking for one,
// then no page mapping is transferred, but no error occurs.
// The ipc only happens when no errors occur.
//
// Returns 0 on success, < 0 on error.
// Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist.
//		(No need to check permissions.)
//	-E_IPC_NOT_RECV if envid is not currently blocked in sys_ipc_recv,
//		or another environment managed to send first.
//	-E_INVAL if srcva < UTOP but srcva is not page-aligned.
//	-E_INVAL if srcva < UTOP and perm is inappropriate
//		(see sys_page_alloc).
//	-E_INVAL if srcva < UTOP but srcva is not mapped in the caller's
//		address space.
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in the
//		current environment's address space.
//	-E_NO_MEM if there's not enough memory to map srcva in envid's
//		address space.
static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
	// LAB 4: Your code here.

	struct Env *recv_env;
	int r = envid2env(envid, &recv_env, 0);
	if(r < 0) {
		return -E_BAD_ENV;
	}
	assert(recv_env);
	assert(recv_env->env_id == envid);
	if(srcva < (void *)UTOP) {
		if((uintptr_t)srcva % PGSIZE != 0) {
			return -E_INVAL;
		}
		if((perm & PTE_U) == 0 || (perm & PTE_P) == 0 || (perm & ~PTE_SYSCALL) != 0) {
			return -E_INVAL;
		}
	}
	
	if(recv_env->env_ipc_recving == false) {
		return -E_IPC_NOT_RECV;
	}
	//cprintf("%d try to fuck env %d\n", curenv->env_id, envid);
	//cprintf("%d is %s\n", recv_env->env_id,(char *[]){"FREE", "DYING", "RUNNABLE", "RUNNING", "NOT RUNABLE"}[recv_env->env_status]);
	assert(recv_env->env_status == ENV_NOT_RUNNABLE);

	
	recv_env->env_ipc_perm = 0;
	void *dstva = recv_env->env_ipc_dstva;
	if(srcva < (void *)UTOP && perm != 0 && dstva < (void *)UTOP) {
		pte_t *sender_ptep = NULL;
		struct PageInfo *pp = page_lookup(curenv->env_pgdir, srcva, &sender_ptep);
		if(!pp || !(*sender_ptep & PTE_U) || !(*sender_ptep & PTE_P) || (!(*sender_ptep & PTE_W) && perm &PTE_W) ) {
			return -E_INVAL;
		}

		
		assert(!((uintptr_t)dstva % PGSIZE));
		r = page_insert(recv_env->env_pgdir, pp, (void *)dstva, perm);
		if(r < 0) {
			return -E_NO_MEM;
		}
		recv_env->env_ipc_perm = perm;
	}

	//cprintf("%d is fucking env %d\n", curenv->env_id,envid);
	recv_env->env_ipc_recving = false;
	recv_env->env_ipc_from = curenv->env_id;
	//assert(value != 0);
	recv_env->env_ipc_value = value;
	recv_env->env_status = ENV_RUNNABLE;
	//recv_env->env_tf.tf_regs.reg_eax = 0;
	//cprintf("%d finish fucking env %d\n", curenv->env_id,envid);
	return 0;

}

// Block until a value is ready.  Record that you want to receive
// using the env_ipc_recving and env_ipc_dstva fields of struct Env,
// mark yourself not runnable, and then give up the CPU.
//
// If 'dstva' is < UTOP, then you are willing to receive a page of data.
// 'dstva' is the virtual address at which the sent page should be mapped.
//
// This function only returns on error, but the system call will eventually
// return 0 on success.
// Return < 0 on error.  Errors are:
//	-E_INVAL if dstva < UTOP but dstva is not page-aligned.
static int
sys_ipc_recv(void *dstva)
{
	// LAB 4: Your code here.
	if( (uintptr_t)dstva < UTOP && (uintptr_t)dstva % PGSIZE != 0) {
		cprintf("dstva is not page-aligned\n");
		return -E_INVAL;
	}

	//assert(curenv->env_ipc_recving == false);
	curenv->env_status = ENV_NOT_RUNNABLE;
	curenv->env_ipc_dstva = dstva;
	//cprintf("debug: ready to sched\n");
	curenv->env_tf.tf_regs.reg_eax = 0;
	//cprintf("%d is ready to be fucked\n", curenv->env_id);
	curenv->env_ipc_recving = true;
	sched_yield();
	return 0;
}

// Return the current time.
static int
sys_time_msec(void)
{
	// LAB 6: Your code here.
	return time_msec();
}

static int sys_net_try_transmit(const char *s, int len){
	// Check that the user has permission to read memory [s, s+len).
	// Destroy the environment if not.
	user_mem_assert(curenv, (void *)s, len, PTE_U|PTE_P);
	return e1000_transmit(s, len);
}

static int sys_net_try_receive(char *s){
	//extern RX_BUFF_SIZE;
	user_mem_assert(curenv, (void *)s, MIN_RECEIVE_BUFF_SIZE, PTE_U|PTE_P|PTE_W);
	//cprintf("in sys_net_try_receive\n");
	return e1000_receive(s);
}

// Dispatches to the correct kernel function, passing the arguments.
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	// Call the function corresponding to the 'syscallno' parameter.
	// Return any appropriate return value.
	// LAB 3: Your code here.

	//panic("syscall not implemented");
	/*
	enum {
	SYS_cputs = 0,
	SYS_cgetc,
	SYS_getenvid,
	SYS_env_destroy,
	SYS_page_alloc,
	SYS_page_map,				//5
	SYS_page_unmap,
	SYS_exofork,
	SYS_env_set_status,
	SYS_env_set_trapframe,
	SYS_env_set_pgfault_upcall, //10
	SYS_yield,					
	SYS_ipc_try_send,
	SYS_ipc_recv,
	SYS_time_msec,
	NSYSCALLS
	};
	*/
	//cprintf("syscall is %s\n", get_syscall_name(syscallno));	//print syscall, for debug
	int32_t r = 0;
	switch (syscallno) {
	case SYS_cputs: {
		sys_cputs((char *)a1, a2);
		break;
	}	
	case SYS_cgetc: {
		r = sys_cgetc();
		break;
	}
	case SYS_getenvid: {
		r = sys_getenvid();
		break;
	}
	case SYS_env_destroy: {
		r = sys_env_destroy(a1);
		break;
	}
	case SYS_page_alloc: {
		r = sys_page_alloc(a1, (void *)a2, a3);
		break;
	}
	case SYS_page_map: {		// 5
		r = sys_page_map(a1, (void *)a2, a3, (void *)a4, a5);
		break;
	}
	case SYS_page_unmap: {
		r = sys_page_unmap(a1, (void *)a2);
		break;
	}
	case SYS_exofork: {
		r = sys_exofork();
		break;
	}
	case SYS_env_set_status: {
		r = sys_env_set_status(a1, a2);
		break;
	}
	case SYS_env_set_trapframe: {
		r = sys_env_set_trapframe(a1, (struct Trapframe *)a2);
		break;
	}
	case SYS_env_set_pgfault_upcall: { //10
		r = sys_env_set_pgfault_upcall(a1, (void *)a2);
		break;
	}
	case SYS_yield: {	
		sched_yield();
		break;
	}
	case SYS_ipc_try_send: {
		r = sys_ipc_try_send(a1, a2, (void *)a3, a4);
		break;
	}
	case SYS_ipc_recv: {
		r = sys_ipc_recv((void *)a1);
		break;
	}
	case SYS_time_msec: {
		r = sys_time_msec();
		break;
	}
	case SYS_net_try_transmit: {
		r = sys_net_try_transmit((char *)a1, a2);
		break;
	}
	case SYS_net_try_receive: {
		r = sys_net_try_receive((char *)a1);
		break;
	}
	default:
		cprintf("syscallno is %d\n", syscallno);	//for debug
		r = -E_INVAL;
	}

	return r;
}

