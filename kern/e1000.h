#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H
#include <inc/types.h>
#include <kern/pci.h>
#include <kern/pmap.h>

#define E1000_DEV_ID_82542  0x1000
#define E1000_STATUS        0x00008  /* Device Status - RO */

volatile char* e1000_reg_map;
volatile char* e1000_flash_map;

int pci_e1000_attach(struct pci_func* f);

#endif  // JOS_KERN_E1000_H
