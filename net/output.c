#include "ns.h"
#include <inc/error.h>
#include <inc/assert.h>
#include <inc/lib.h>
extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver
	while (1) {
		int perm = 0;
		int whom = 0;
		int req = ipc_recv((int32_t *) &whom, &nsipcbuf, &perm);
		
		// All requests must contain an argument page
		if (!(perm & PTE_P)) {
			cprintf("Invalid request from %08x: no argument page\n",
				whom);
			continue; // just leave it hanging...
		}
		assert(req == NSREQ_OUTPUT);
		int r;
		while( (r = sys_net_try_transmit(nsipcbuf.pkt.jp_data, nsipcbuf.pkt.jp_len)) == -E_NO_TX);
		if( r < 0) {
			panic("sys_net_try_transmit: %e", r);
		}
		assert(r == nsipcbuf.pkt.jp_len);
		sys_page_unmap(0, &nsipcbuf);
	}
}
