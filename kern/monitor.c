// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display a listing of function call frames", mon_backtrace},
	{ "showmappings", "Display VM to PM mapping", mon_showmappings},
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}
/*
push   %ebp
mov    %esp,%ebp

so we have following:

while(ebp != NULL) {
	pre_ebp = *ebp;
	ebp = pre_ebp;
	do something
}

print format:
Stack backtrace:
  ebp f010ff78  eip f01008ae  args 00000001 f010ff8c 00000000 f0110580 00000000
         kern/monitor.c:143: monitor+106
  ebp f010ffd8  eip f0100193  args 00000000 00001aac 00000660 00000000 00000000
         kern/init.c:49: i386_init+59
  ebp f010fff8  eip f010003d  args 00000000 00000000 0000ffff 10cf9a00 0000ffff
         kern/entry.S:70: <unknown>+0
*/

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.
	__asm__ __volatile__("":::"memory");	// prevent reordering
	uint32_t *addr = (uint32_t *)read_ebp();
	cprintf("Stack backtrace:\n");
	while(addr != NULL){
		struct Eipdebuginfo info;
		debuginfo_eip(*(addr + 1), &info);
		
		cprintf("  ebp %08x eip %08x args %08x %08x %08x %08x %08x\n", \
			addr, *(addr + 1), *(addr + 2), *(addr + 3), *(addr + 4), *(addr + 5), *(addr + 6));

		cprintf("        %s:%d: %.*s+%d\n", \
			info.eip_file, info.eip_line, info.eip_fn_namelen, info.eip_fn_name, *(addr + 1) - info.eip_fn_addr);
		addr = (uint32_t *) (*addr);
	}
	return 0;
}
/**
00000000ef000000-00000000ef021000 0000000000021000 ur-
00000000ef7bc000-00000000ef7be000 0000000000002000 ur-
00000000ef7bf000-00000000ef800000 0000000000041000 ur-
00000000efff8000-0000000100000000 0000000010008000 -rw
**/
static void mon_showmappings_helper(size_t start, size_t end) {
	extern pte_t *pgdir_walk(pde_t *pgdir, const void *va, int create);
	extern pde_t *kern_pgdir;

	for(; start < end; start += PGSIZE) {
		cprintf("%08x--%08x:   ", start, start + PGSIZE);
		pte_t *ptep = pgdir_walk(kern_pgdir, (void *)start, 0);
		
		if(!ptep) {
			cprintf("NOT MAPPED\n");
			break;
		}

		cprintf("%08x  ", PTE_ADDR(*ptep));
		char flags[] = {'-', 'r', '-', '\0'};
		if(*ptep & PTE_U) flags[0] = 'u';
		if(*ptep & PTE_W) flags[2] = 'w';
		cprintf("%s\n", flags);
	}
}

int mon_showmappings(int argc, char **argv, struct Trapframe *tf) {
	assert(argc == 3);
	if(argc != 3) {
		cprintf("error in args nums, it should be 2 number!!\n");
		return 0;
	}

	long arg_1 = strtol(argv[1], 0, 0);
	long arg_2 = strtol(argv[2], 0, 0);
	
	size_t start = (arg_1 < arg_2) ? (size_t)arg_1 : (size_t)arg_2;
	size_t end = (arg_1 < arg_2) ? (size_t)(arg_2 - 1) : (size_t)(arg_1 - 1);
	if(end > 0xFFFFFFFF) {
		cprintf("Illegal parameter, out of range\n");
		return 0;
	}
	if(start != ROUNDUP(start, PGSIZE) || end != ROUNDUP(end, PGSIZE) - 1){
		cprintf("Illegal parameter, not aligned\n");
		return 0;
	}
	mon_showmappings_helper(start, end);
	return 0;
}

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
