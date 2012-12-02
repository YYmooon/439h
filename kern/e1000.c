#include <inc/error.h>

#include <kern/e1000.h>

#define DEV_STATUS() (*(uint32_t*)(e1000_reg_map + E1000_STATUS)) 

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
  descriptor->cmd     = 0;
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
  *((unsigned*)(e1000_reg_map + E1000_TCTL))  = (E1000_TCTL_EN | E1000_TCTL_PSP | 
                                                 E1000_TCTL_CT | E1000_TCTL_COLD);
  // Enable the DD bit..
  *((unsigned*)(e1000_reg_map + E1000_TXDCTL)) |= E1000_TXD_CMD_RS;

  return 0;
}

int
pci_e1000_tx(void* buffer, unsigned length)
{
  int r = free_desc();
  if(r < 0) {
    return -E_UNSPECIFIED;
  } else {
    volatile struct tx_desc* descriptor = (struct tx_desc*) &tx_descriptors[r];

    zero_desc(descriptor);  

    descriptor->addr = 0;
    descriptor->length = length;
    
    *((unsigned*)(e1000_reg_map + E1000_TDT)) += 1;

    return 0;
  }
}
