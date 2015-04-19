#include "ns.h"
#include <inc/assert.h>
#include <inc/lib.h>
extern union Nsipc nsipcbuf;

void
input(envid_t ns_envid)
{
	binaryname = "ns_input";
	int r;
	// LAB 6: Your code here:
	// 	- read a packet from the device driver
	//	- send it to the network server
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.
	while(1) {
		/* safe: alloc a page on the same va multi times will unmap previous page */
		if((r = sys_page_alloc(0, &nsipcbuf, PTE_U | PTE_P | PTE_W)) != 0){
			panic("sys_page_alloc: %e", r);
		}

		while((r = sys_net_try_receive(nsipcbuf.pkt.jp_data)) == -E_NO_RX) {
			sys_yield();
		}
		if(r < 0) {
			panic("sys_net_try_receive: %e", r);
		}
		nsipcbuf.pkt.jp_len = r;
		ipc_send(ns_envid, NSREQ_INPUT, &nsipcbuf, PTE_U | PTE_P | PTE_W);
	}
}
