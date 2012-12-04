#include <inc/error.h>
#include <inc/string.h>

#include <kern/env.h>
#include <kern/e1000.h>
#include <debug.h>

#define DEV_STATUS()            (*(uint32_t*)(e1000_reg_map + E1000_STATUS))
#define INCMOD(x, b)            do{*x = (*x + 1)%b;}while(0);
#define E1000_SET_REG(reg, val) 
#define debug 1

volatile char* e1000_reg_map;
volatile char* e1000_flash_map;
struct tx_desc tx_descriptors[E1000_RING_SIZE];
struct e1000_page tx_datablocks[E1000_RING_SIZE];
static int cursor;

static int
e1000_reg_write(uint32_t reg, uint32_t val)
{
  return (*((unsigned*)(e1000_reg_map + reg)) = val);
}

static int
desc_alloc(int sideffct)
{
  volatile struct tx_desc* c;
  uint32_t v = (cursor+1)%E1000_RING_SIZE, bit, mask;

  while(v != (cursor + E1000_RING_SIZE)%E1000_RING_SIZE) {
    c =  (struct tx_desc*) tx_descriptors + v;
    if(!c->status.dd) {
      if(sideffct)
        cursor = v;
      //NET_DEBUG("allocated descriptor %d\n", v);
      return v;
    } else {
      //NET_ERR_DEBUG("descriptor %d is taken...\n", v);
    }

    INCMOD(&v, E1000_RING_SIZE);
  }
  return -E_UNSPECIFIED;
}

void
zero_desc(volatile struct tx_desc* descriptor)
{
  descriptor->addr                  = 0;
  descriptor->length                = 0;
  *((uint8_t*)&descriptor->cmd)     = 0;
  *((uint32_t*)&descriptor->status) = 0;
  *((uint8_t*)&descriptor->css)     = 0;
  descriptor->special               = 0;
}

int
pci_e1000_attach(struct pci_func* f)
{
  // Set up the allocation cursor
  cursor = -1;

  // Map the registers using MMIO
  e1000_reg_map = (char*) mmio_map_region(f->reg_base[0], 
                                          f->reg_size[0]);

  // Set the paddr where the ring buffer resides 
  e1000_reg_write(E1000_TDBAL,  PADDR(&tx_descriptors[0]));
  // Set the high bits of the paddr where the ring buffer resides
  e1000_reg_write(E1000_TDBAH,  0);
 
  // Set the transmission ringbuffer size
  e1000_reg_write(E1000_TDLEN,  E1000_RING_SIZE*sizeof(struct tx_desc) << 7);

  // Set the head and tail values... 
  e1000_reg_write(E1000_TDH,    0);
  e1000_reg_write(E1000_TDT,    0);

  // Set the transmission control properties
  e1000_reg_write(E1000_TCTL,   (E1000_TCTL_EN  | // set the card online for TX
                                 E1000_TCTL_PSP | // pad short packets
                                 //(0x10 << 4)    | // set the CT value
                                 //((0x40) << 12) | // set the COLD value
                                 0));
  
  e1000_reg_write(E1000_TIPG, 10);

  // Set the DD bit, and corresponding command bit in all descriptors
  int i;
  volatile struct tx_desc* c;

  for(i = 0; i < 32; i++) {
    c =  (struct tx_desc*) tx_descriptors + i;
    c->status.dd = 0;
    c->cmd.rs = 1;
  }

  assert((uint32_t) &tx_descriptors[0] % 16 == 0);

  return 0;
}

int
pci_e1000_tx(void* buffer, unsigned length, unsigned blocking)
{
  int r = desc_alloc(1);
  memcpy(buffer, (void*)&tx_datablocks[r], (length < PGSIZE) ? length : PGSIZE);
  if(r < 0) {
    NET_ERR_DEBUG("failed to a allocate a packet descriptor!\n");
    return -E_UNSPECIFIED;
  } else {
    NET_DEBUG("got a descriptor %d, packing and setting it\n", r);
    volatile struct tx_desc* descriptor = (struct tx_desc*) &tx_descriptors[r];

    zero_desc(descriptor); 

    NET_DEBUG("instructing card to write %d bytes from paddr %08x\n", length, PADDR(&tx_datablocks[r]));

    descriptor->addr      = PADDR(&tx_datablocks[r]);
    descriptor->length    = length;

    descriptor->cmd.eop   = 1;
    descriptor->cmd.rs    = 1;

    INCMOD(((unsigned*)(e1000_reg_map + E1000_TDT)), E1000_RING_SIZE);

    //DEBUG("[pci_e1000_tx] returning from call...\n");

    while(blocking && (r = desc_alloc(0) < 0))
      continue;

    return 0;
  }
}
