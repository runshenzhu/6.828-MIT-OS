#include <kern/e1000.h>
#include <kern/pmap.h>
#include <inc/stdio.h>
#include <inc/assert.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/types.h>
// LAB 6: Your driver code here
#define TRANS_CHECK_TIME 1000
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


/**************** check ****************/
void check_transmit(){
	int i = 0;
	for(i = 0; i < 100; i++){
		e1000_transmit("debug", 6);
	}
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
}
/**************** transmit ****************/
static inline int get_tx_ring_tail() {
	int tail = get_e1000_register(E1000_TDT);
	assert(tail < TDLEN);
	return tail;
}

/* are there enough buff to trans size bytes data?? */
static int e1000_transmit_check_free(int size){
	size = ROUNDUP(size, TX_BUFF_SIZE);
	int need_nbuffs = size / TX_BUFF_SIZE;
	need_nbuffs = need_nbuffs > TDLEN ? TDLEN : need_nbuffs;
	int tail = get_tx_ring_tail();

	// check 
	int i;
	for(i = 0; i < need_nbuffs; i++){
		int idx = (tail + i) % TDLEN;
		if(!tx_desc_rings[idx].status.dd)
			return 0;
	}
	return 1;
}

int e1000_transmit(const char *data, int size){
	int i = 0;
	while(i++ < TRANS_CHECK_TIME){
		if(e1000_transmit_check_free(size))
			break;
	}

	if(!e1000_transmit_check_free(size))
		return -E_NO_TX;

	int tail = get_tx_ring_tail();
	struct tx_desc *desc;
	int size_want_to_trans = size;
	int size_has_trans = 0;
	while(size) {
		// wait a free buff
		assert(tail == get_tx_ring_tail());
		desc = (struct tx_desc *)&tx_desc_rings[tail];
		
		// still need to wait, because size may beyond TDLEN * TX_BUFF_SIZE
		while(!desc->status.dd);
		// mark it as in use
		desc->status.dd = 0;

		//prepare this buff
		int size_ready_to_trans = size > TX_BUFF_SIZE ? TX_BUFF_SIZE : size;
		size -= size_ready_to_trans;
		char *buff = (char *)KADDR((uint32_t)desc->addr);
		memmove(buff, data + size_has_trans, size_ready_to_trans);
		size_has_trans += size_ready_to_trans;

		//fill in desc
		desc->length = size_ready_to_trans;
		desc->cso = 0;
		desc->special = 0;
		// end the packet?
		desc->cmd.eop = size == 0? 1 : 0;
		
		// tail++ and send the buff
		tail = (tail + 1) % TDLEN;
		set_e1000_register(E1000_TDT, tail);
	}
	assert(size_has_trans == size_want_to_trans);
	return size_has_trans;
}

/**************** init ****************/
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

static void init_e1000() {
	init_e1000_tdba();
	/* 大坑!!! 存的是字节数 没好好看文档 T_T */
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
	//check_transmit();
	return 0;
}