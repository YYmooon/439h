#include <inc/error.h>
#include <kern/env.h>
#include <kern/e1000.h>
#include <debug.h>

#define DEV_STATUS() (*(uint32_t*)(e1000_reg_map + E1000_STATUS)) 
#define debug 1
#define QUEUE_SIZE 32 

volatile char* e1000_reg_map;
volatile char* e1000_flash_map;
struct tx_desc tx_descriptors[32];
static int cursor;

static int
desc_alloc(int sideffct)
{
  volatile struct tx_desc* c;
  uint32_t v = (cursor+1)%QUEUE_SIZE, bit, mask;

  while(v != (cursor + QUEUE_SIZE)%QUEUE_SIZE) {
    c =  (struct tx_desc*) tx_descriptors + v;
    if(!c->status.dd) {
      if(sideffct)
        cursor = v;
      NET_DEBUG("allocated descriptor %d\n", v);
      return v;
    } else {
      NET_ERR_DEBUG("descriptor %d is taken...\n", v);
    }

    v=(v+1)%QUEUE_SIZE;
  }
  return -E_UNSPECIFIED;
}

void
zero_desc(volatile struct tx_desc* descriptor)
{
  descriptor->addr    = 0;
  descriptor->length  = 0;
  descriptor->cso     = 0;
  *((uint8_t*)&descriptor->cmd) &= (1<<3); // unset everything but the RS bit
  *((uint32_t*)&descriptor->status) = 0;
  descriptor->css     = 0;
  descriptor->special = 0;
}

int
pci_e1000_attach(struct pci_func* f)
{
  // Set up the allocation cursor
  cursor = 0;

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
  volatile struct tx_desc* c;

  for(i = 0; i < 32; i++) {
    c =  (struct tx_desc*) tx_descriptors + i;
    c->status.dd = 0;
    c->cmd.rs = 1;
  }

  return 0;
}

int
pci_e1000_tx(void* buffer, unsigned length, unsigned blocking)
{
  int r = desc_alloc(1);
  if(r < 0) {
    NET_ERR_DEBUG("failed to a allocate a packet descriptor!\n");
    return -E_UNSPECIFIED;
  } else {
    NET_DEBUG("got a descriptor, packing and setting it\n");
    volatile struct tx_desc* descriptor = (struct tx_desc*) &tx_descriptors[r];

    zero_desc(descriptor); 

    pte_t *entry = pgdir_walk(curenv ? curenv->env_pgdir : kern_pgdir, buffer, 0);

    if(entry == 0)
      NET_ERR_DEBUG("bad page table entry!\n");

    descriptor->addr = *entry;
    descriptor->length = length;
    
    *((unsigned*)(e1000_reg_map + E1000_TDT)) += 1;

    //DEBUG("[pci_e1000_tx] returning from call...\n");
    
    while(blocking && (desc_alloc(0) < 0))
      continue;
    
    return 0;
  }
}
