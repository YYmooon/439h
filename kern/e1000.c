#include <inc/assert.h>
#include <kern/e1000.h>
#include <kern/env.h>
#include <debug.h>

#define DEV_STATUS() (*(uint32_t*)(e1000_reg_map + E1000_STATUS)) 

int
pci_e1000_attach(struct pci_func* f)
{
  // Map the registers using MMIO
  e1000_reg_map =(char*) mmio_map_region(f->reg_base[0], 
                                           f->reg_size[0]);
  DVR_DEBUG("trying to access MMIO'd memory...\n");
  assert(*(uint32_t*)(e1000_reg_map + E1000_STATUS) == 0x80080783);  
  DVR_DEBUG("sucess! device status is %08x\n", DEV_STATUS()); 

  // Set the transmission h
  *((unsigned*)(e1000_reg_map + E1000_TDLEN)) = 32*sizeof(struct tx_desc);
  *((unsigned*)(e1000_reg_map + E1000_TDH)) = 0;
  *((unsigned*)(e1000_reg_map + E1000_TDT)) = 0;

  *((unsigned*)(e1000_reg_map + E1000_TCTL)) = (E1000_TCTL_EN | E1000_TCTL_PSP | 
                                                E1000_TCTL_CT | E1000_TCTL_COLD);

  return 1;
}

// LAB 6: Your driver code here
