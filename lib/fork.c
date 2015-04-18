// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

#define PGNUM(la)	(((uintptr_t) (la)) >> PTXSHIFT)
//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	//check
	if(!(err & FEC_WR)) {
		panic("error in errno, %x",err);
	}
	unsigned pn = PGNUM(addr);
	pte_t ptep = uvpt[pn];
	assert(ptep & PTE_COW);

	//Allocate a new page, map it at a temporary location (PFTEMP),
	/* int sys_page_alloc(envid_t envid, void *va, int perm) */
	r = sys_page_alloc(0, (void *)(PFTEMP), PTE_U|PTE_P|PTE_W);
	if(r != 0) {
		panic("fail to alloc a new page, %e", r);
	}
	addr = ROUNDDOWN(addr, PGSIZE);
	memmove((void *)PFTEMP, addr, PGSIZE);
	/* int sys_page_map(envid_t srcenvid, void *srcva, envid_t dstenvid, void *dstva, int perm) */
	r = sys_page_map(0, (void *)(PFTEMP), 0, addr, PTE_U|PTE_P|PTE_W);
	if(r != 0) {
		panic("fail to map page PFTEMP, %e", r);
	}
	//panic("pgfault not implemented");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	//panic("duppage not implemented");
	pte_t ptep = uvpt[pn];
	assert(ptep & PTE_U);
	assert(ptep & PTE_P);
	

	
	void *va = (void *)(pn * PGSIZE);
	if(ptep & PTE_SHARE) {
		r = sys_page_map(0, va, envid, va, ptep & PTE_SYSCALL);
		if(r != 0) {
			panic("sys_page_map failed, %e",r);
		}
	}
	else if((ptep & (PTE_COW | PTE_W)) != 0){
		/* int sys_page_map(envid_t srcenv, void *srcva, envid_t dstenv, void *dstva, int perm) */
		r = sys_page_map(0, va, envid, va, PTE_COW | PTE_U | PTE_P);
		if(r != 0) {
			panic("sys_page_map failed, %e",r);
		}
		r = sys_page_map(0, va, 0, va, PTE_COW | PTE_U | PTE_P);
		if(r != 0) {
			panic("sys_page_map failed, %e",r);
		}
	} 
	else {
		r = sys_page_map(0, va, envid, va, ptep & PTE_SYSCALL);
		if(r != 0) {
			panic("sys_page_map failed, %e",r);
		}
	}
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	//panic("fork not implemented");
	
	int r;
	// set up our page fault handler appropriately.
	assert(pgfault != NULL);
	set_pgfault_handler(pgfault);
	assert(thisenv->env_pgfault_upcall != NULL);
	// Create a child.
	envid_t envid;
	envid = sys_exofork();
	if (envid < 0)
		panic("sys_exofork: %e", envid);
	if (envid == 0) {
		// We're the child.
		// The copied value of the global variable 'thisenv'
		// is no longer valid (it refers to the parent!).
		// Fix it and return 0.
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	// Copy our address space to the child.
	uint8_t *addr;
	for (addr = 0; addr < (uint8_t *)UTOP; addr += PGSIZE){
		unsigned pn = PGNUM(addr);
		if (!(uvpd[PDX(pn<<PGSHIFT)] & PTE_P)) { 
			continue;
		}
		pte_t ptep = uvpt[pn];
		if((ptep & (PTE_U | PTE_P)) != (PTE_P|PTE_U)) {
			continue;
		}
		if(pn * PGSIZE == UXSTACKTOP - PGSIZE){
			assert((ptep & (PTE_U | PTE_P | PTE_W)) == (PTE_U | PTE_P | PTE_W));
			continue;
		}
		if(pn * PGSIZE == USTACKTOP - PGSIZE) {
			assert((ptep & (PTE_U | PTE_P | PTE_W)) == (PTE_U | PTE_P | PTE_W));
		}
		r = duppage(envid, pn);
		assert(r == 0);
	}
	
	//set page fault handler
	r = sys_page_alloc(envid, (void *)(UXSTACKTOP - PGSIZE), PTE_U|PTE_P|PTE_W);
	if(r != 0) {
		panic("fail to alloc UXSTACK, %e",r);
	}
	/* int sys_env_set_pgfault_upcall(envid_t envid, void *func) */
	assert(thisenv->env_pgfault_upcall != NULL);
	r = sys_env_set_pgfault_upcall(envid, thisenv->env_pgfault_upcall);
	if(r != 0) {
		panic("fail to sys_env_set_pgfault_upcall, %e", r);
	}

	r = sys_env_set_status(envid, ENV_RUNNABLE);
	if(r != 0) {
		panic("fail to sys_env_set_status, %e", r);
	}

	return envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
