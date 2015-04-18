#include <kern/e1000.h>
#include <kern/pmap.h>
#include <inc/stdio.h>
#include <inc/assert.h>
#include <inc/error.h>
// LAB 6: Your driver code here

volatile uint32_t e1000_bar0;
e1000_status status;
volatile struct tx_desc *tx_desc_rings;
char tx_buff[TDLEN][TX_BUFF_SIZE];

static inline uint32_t get_e1000_register(unsigned reg_inx){
	return *(uint32_t *)(e1000_bar0 + reg_inx);
}
static inline void set_e1000_register(unsigned reg_inx, unsigned val) {
	*(uint32_t *)(e1000_bar0 + reg_inx) = val;
}

static void check_e1000(struct pci_func *pcif, int is_enable){
	if(!is_enable){
		assert(!pcif->reg_base[0]);
		assert(!pcif->reg_size[0]);
		return;
	}

	assert(pcif->reg_base[0]);
	assert(pcif->reg_size[0]);
	assert(e1000_bar0);
	assert(get_e1000_register(E1000_STATUS) == INITIAL_STATUS);
	assert(get_e1000_register(E1000_TDH) == 0);
	assert(get_e1000_register(E1000_TDT) == 0);
	assert(tx_desc_rings);
	assert((uint32_t)tx_desc_rings % PGSIZE == 0);
	/*
	assert(sizeof(struct tx_desc) == 128/8);
	uint32_t rings_addr = (uint32_t)tx_desc_rings;
	rings_addr += PGSIZE - sizeof(struct tx_desc)/8;
	if(rings_addr != (uint32_t)(&tx_desc_rings[TDLEN-2])){
		cprintf("rings_addr is %u, tx is %u %u %u\n", rings_addr, \
			(uint32_t)(&tx_desc_rings[0]), (uint32_t)(&tx_desc_rings[1]), (uint32_t)(&tx_desc_rings[TDLEN-2]));
		assert(0);
	}
	*/
}

static inline void init_e1000_tctl(){
	uint32_t tctl_value = 0;
	tctl_value |= E1000_TCTL_EN;
	tctl_value |= E1000_TCTL_PSP;
	tctl_value |= E1000_TCTL_COLD_VAL;
	tctl_value |= E1000_TCTL_CT_VAL;
	set_e1000_register(E1000_TCTL, tctl_value);
}

static inline void init_e1000_tipg(){
	uint32_t tipg_value = 0;
	tipg_value |= E1000_TIPG_IPGT_VAL | E1000_TIPG_IPGR1_VAL | E1000_TIPG_IPGR2_VAL;
	set_e1000_register(E1000_TIPG, tipg_value);
}

static inline void init_e1000_tdba(){
	//cprintf("debug begin\n");
	struct PageInfo *pp = page_alloc(ALLOC_ZERO);
	assert(pp);
	pp->pp_ref++;
	tx_desc_rings = (struct tx_desc *)page2kva(pp);
	set_e1000_register(E1000_TDBAL, page2pa(pp));
	set_e1000_register(E1000_TDBAH, 0);
	int i;
	for(i = 0; i < TDLEN; i++) {
		tx_desc_rings[i].addr = PADDR(tx_buff[i]);
		tx_desc_rings[i].cmd.rs = 1;
		tx_desc_rings[i].status.dd = 1;
	}
}

void check_transmit(){
	int i = 0;
	for(i = 0; i < 100; i++){
		int idx = get_e1000_register(E1000_TDT);
		
		assert(tx_desc_rings[idx].cmd.rs == 1);
		while(!tx_desc_rings[idx].status.dd);	/* wait a free tx_desc */
		assert(tx_desc_rings[idx].status.dd);
		uint32_t buff_addr = tx_desc_rings[idx].addr; /* 64bit to 32bit */
		char *buf = (char *)KADDR(buff_addr);
		buf[0] = '0'+idx;
		tx_desc_rings[idx].length = 1;
		tx_desc_rings[idx].status.dd = 0;
		tx_desc_rings[idx].cmd.eop = 1;
		tx_desc_rings[idx].cso = 0;
		tx_desc_rings[idx].special = 0;
		
		idx = (idx + 1) % TDLEN;
		set_e1000_register(E1000_TDT, idx);
	}
}

static inline int get_tx_ring_tail() {
	int tail = get_e1000_register(E1000_TDT);
	assert(tail < TDLEN);
	return tail;
}

int e1000_try_transmit(const char *data, int size){
	if(size > TX_BUFF_SIZE){
		return -E_INVAL;
	}
	int tail = get_tx_ring_tail();
	struct tx_desc desc = tx_desc_rings[tail];
	return 0;
	//assert()
}

static void init_e1000() {
	init_e1000_tdba();
	/* 大坑!!! LEN 是字节数 */
	set_e1000_register(E1000_TDLEN, TDLEN * sizeof(struct tx_desc));
	set_e1000_register(E1000_TDH, 0);
	set_e1000_register(E1000_TDT, 0);
	init_e1000_tctl();
	init_e1000_tipg();
	cprintf("e1000 init successed!\n");
}

int attach_e1000(struct pci_func *pcif) {
	if(debug)
		cprintf("begin attach_e1000\n");
	check_e1000(pcif, 0);
	pci_func_enable(pcif);

	e1000_bar0 = (uint32_t)mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);
	init_e1000();
	check_e1000(pcif, 1);
	check_transmit();
	return 0;
}