#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <kern/pci.h>

#define debug 1

#define E1000_VENDOR_ID_82540EM	    0x8086
#define E1000_DEV_ID_82540EM		0x100E

typedef uint32_t e1000_status;  
int attach_e1000(struct pci_func *pcif);
/* 128 bit */
struct tx_desc{
	uint64_t addr;
	uint16_t length;
	uint8_t cso;
	uint8_t cmd;
	uint8_t status;
	uint8_t css;
	uint16_t special;
} __attribute__((__packed__));


#define TDLEN  			(PGSIZE/sizeof(struct tx_desc))
/* E1000 register */
#define E1000_STATUS   0x00008  /* Device Status - RO */
#define E1000_TDBAL    0x03800  /* TX Descriptor Base Address Low - RW */
#define E1000_TDBAH    0x03804  /* TX Descriptor Base Address Low - RW */
#define E1000_TCTL     0x00400  /* TX Control - RW */
#define E1000_TIPG     0x00410  /* TX Inter-packet gap -RW */
#define E1000_TDLEN    0x03808  /* TX Descriptor Length - RW */
#define E1000_TDH      0x03810  /* TX Descriptor Head - RW */
#define E1000_TDT      0x03818  /* TX Descripotr Tail - RW */


/* Transmit Control */
#define E1000_TCTL_RST    0x00000001    /* software reset */
#define E1000_TCTL_EN     0x00000002    /* enable tx */
#define E1000_TCTL_BCE    0x00000004    /* busy check enable */
#define E1000_TCTL_PSP    0x00000008    /* pad short packets */
#define E1000_TCTL_CT     0x00000ff0    /* collision threshold */
#define E1000_TCTL_CT_SHIFT			4
#define E1000_TCTL_CT_VAL   (0x10 << E1000_TCTL_CT_SHIFT)
#define E1000_TCTL_COLD   0x003ff000    /* collision distance */
#define E1000_TCTL_COLD_SHIFT		12
#define E1000_TCTL_COLD_VAL (0x40 << E1000_TCTL_COLD_SHIFT)
#define E1000_TCTL_SWXOFF 0x00400000    /* SW Xoff transmission */
#define E1000_TCTL_PBE    0x00800000    /* Packet Burst Enable */
#define E1000_TCTL_RTLC   0x01000000    /* Re-transmit on late collision */
#define E1000_TCTL_NRTU   0x02000000    /* No Re-transmit on underrun */
#define E1000_TCTL_MULR   0x10000000    /* Multiple request support */

/* TIPG */
#define E1000_TIPG_IPGT_SHIFT     0
#define E1000_TIPG_IPGT_VAL       (10 << E1000_TIPG_IPGT_SHIFT)
#define E1000_TIPG_IPGR1_SHIFT    10
#define E1000_TIPG_IPGR1_VAL      (8 << E1000_TIPG_IPGR1_SHIFT)
#define E1000_TIPG_IPGR2_SHIFT    20
#define E1000_TIPG_IPGR2_VAL      (12 << E1000_TIPG_IPGR2_SHIFT)

/* Device Status */
#define E1000_STATUS_FD         0x00000001      /* Full duplex.0=half,1=full */
#define E1000_STATUS_LU         0x00000002      /* Link up.0=no,1=link */
#define E1000_STATUS_FUNC_MASK  0x0000000C      /* PCI Function Mask */
#define E1000_STATUS_FUNC_SHIFT 2
#define E1000_STATUS_FUNC_0     0x00000000      /* Function 0 */
#define E1000_STATUS_FUNC_1     0x00000004      /* Function 1 */
#define E1000_STATUS_TXOFF      0x00000010      /* transmission paused */
#define E1000_STATUS_TBIMODE    0x00000020      /* TBI mode */
#define E1000_STATUS_SPEED_MASK 0x000000C0
#define E1000_STATUS_SPEED_10   0x00000000      /* Speed 10Mb/s */
#define E1000_STATUS_SPEED_100  0x00000040      /* Speed 100Mb/s */
#define E1000_STATUS_SPEED_1000 0x00000080      /* Speed 1000Mb/s */
#define E1000_STATUS_LAN_INIT_DONE 0x00000200   /* Lan Init Completion
                                                   by EEPROM/Flash */
#define E1000_STATUS_ASDV       0x00000300      /* Auto speed detect value */
#define E1000_STATUS_DOCK_CI    0x00000800      /* Change in Dock/Undock state. Clear on write '0'. */
#define E1000_STATUS_GIO_MASTER_ENABLE 0x00080000 /* Status of Master requests. */
#define E1000_STATUS_MTXCKOK    0x00000400      /* MTX clock running OK */
#define E1000_STATUS_PCI66      0x00000800      /* In 66Mhz slot */
#define E1000_STATUS_BUS64      0x00001000      /* In 64 bit slot */
#define E1000_STATUS_PCIX_MODE  0x00002000      /* PCI-X mode */
#define E1000_STATUS_PCIX_SPEED 0x0000C000      /* PCI-X bus speed */
#define E1000_STATUS_BMC_SKU_0  0x00100000 /* BMC USB redirect disabled */
#define E1000_STATUS_BMC_SKU_1  0x00200000 /* BMC SRAM disabled */
#define E1000_STATUS_BMC_SKU_2  0x00400000 /* BMC SDRAM disabled */
#define E1000_STATUS_BMC_CRYPTO 0x00800000 /* BMC crypto disabled */
#define E1000_STATUS_BMC_LITE   0x01000000 /* BMC external code execution disabled */
#define E1000_STATUS_RGMII_ENABLE 0x02000000 /* RGMII disabled */
#define E1000_STATUS_FUSE_8       0x04000000
#define E1000_STATUS_FUSE_9       0x08000000
#define E1000_STATUS_SERDES0_DIS  0x10000000 /* SERDES disabled on port 0 */
#define E1000_STATUS_SERDES1_DIS  0x20000000 /* SERDES disabled on port 1 */
#define INITIAL_STATUS			  0x80080783


#endif	// JOS_KERN_E1000_H
