#include <inc/error.h>

#include <kern/e1000.h>

#define DEV_STATUS() (*(uint32_t*)(e1000_reg_map + E1000_STATUS)) 
#define debug 1
#define DEBUG(...) if(debug)do{cprintf(__VA_ARGS__);}while(0);

volatile char* e1000_reg_map;
volatile char* e1000_flash_map;
struct tx_desc tx_descriptors[32];

static int
free_desc()
{
  int i;
  volatile struct tx_desc* cursor;
  for(i = 0; i < 32; i++) {
    cursor =  (struct tx_desc*) tx_descriptors + i;
    if(cursor->status & E1000_TXD_STAT_DD) {
      return i;
    }
  }
  return -E_UNSPECIFIED;
}

void
zero_desc(volatile struct tx_desc* descriptor)
{
  descriptor->addr    = 0;
  descriptor->length  = 0;
  descriptor->cso     = 0;
  *((uint8_t*)&descriptor->cmd) &= (1<<3); // unset everything but the DD enable bit
  descriptor->status  = 0;
  descriptor->css     = 0;
  descriptor->special = 0;
}

int
pci_e1000_attach(struct pci_func* f)
{
  // Map the registers using MMIO
  e1000_reg_map = (char*) mmio_map_region(f->reg_base[0], 
                                          f->reg_size[0]);

  // Set the transmission h
  *((unsigned*)(e1000_reg_map + E1000_TDLEN)) = 32*sizeof(struct tx_desc);
  *((unsigned*)(e1000_reg_map + E1000_TDH))   = 0;
  *((unsigned*)(e1000_reg_map + E1000_TDT))   = 0;
  *((unsigned*)(e1000_reg_map + E1000_TCTL))  = (E1000_TCTL_EN | E1000_TCTL_PSP | E1000_CTRL_FD);

  pte_t *entry = pgdir_walk(kern_pgdir, &tx_descriptors, 0);
  *((unsigned*)(e1000_reg_map + E1000_TDBAL1)) = *entry;

  // Set the DD bit, and corresponding command bit in all descriptors
  int i;
  volatile struct tx_desc* cursor;

  for(i = 0; i < 32; i++) {
    cursor =  (struct tx_desc*) tx_descriptors + i;
    cursor->status |= E1000_TXD_STAT_DD;
    cursor->cmd.rs = 1;
  }

  return 0;
}

int
pci_e1000_tx(void* buffer, unsigned length)
{
  int r = free_desc();
  if(r < 0) {
    DEBUG("[pci_e1000_tx] failed to a allocate a packet descriptor!\n");
    return -E_UNSPECIFIED;
  } else {
    DEBUG("[pci_e1000_tx] got a descriptor, packing and setting it\n");
    volatile struct tx_desc* descriptor = (struct tx_desc*) &tx_descriptors[r];

    zero_desc(descriptor); 

    pte_t *entry = pgdir_walk(kern_pgdir, buffer, 0);
    descriptor->addr = *entry;
    descriptor->length = length;
    
    *((unsigned*)(e1000_reg_map + E1000_TDT)) += 1;

    DEBUG("[pci_e1000_tx] returning from call...\n");
    return 0;
  }
}
