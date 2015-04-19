#ifndef JOS_INC_SYSCALL_H
#define JOS_INC_SYSCALL_H

/* system call numbers */
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
	SYS_net_try_transmit,		//15
	SYS_net_try_receive,
	NSYSCALLS
};

/* syscall name table, for debug */
static char *syscall_name_table[] = {
	"SYS_cputs",
	"SYS_cgetc",
	"SYS_getenvid",
	"SYS_env_destroy",
	"SYS_page_alloc",
	"SYS_page_map",					//5
	"SYS_page_unmap",
	"SYS_exofork",
	"SYS_env_set_status",
	"SYS_env_set_trapframe",
	"SYS_env_set_pgfault_upcall",	//10
	"SYS_yield",					
	"SYS_ipc_try_send",
	"SYS_ipc_recv",
	"SYS_time_msec",
	"SYS_net_try_transmit",		//15
	"SYS_net_try_receive",
	"NSYSCALLS"
};

#endif /* !JOS_INC_SYSCALL_H */
