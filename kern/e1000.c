#include <kern/e1000.h>
#include <kern/pmap.h>
#include <inc/stdio.h>
#include <inc/assert.h>
// LAB 6: Your driver code here

volatile uint32_t e1000_bar0;
e1000_status status;
volatile struct tx_desc *tx_desc_rings;


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
	assert(tx_desc_rings)
	assert((uint32_t)tx_desc_rings % PGSIZE == 0);
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
	//cprintf("debug end\n");
}

static void init_e1000() {
	init_e1000_tdba();
	set_e1000_register(E1000_TDLEN, TDLEN);
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
	return 0;
}