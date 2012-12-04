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
e1000_reg_read(uint32_t reg)
{
  return *((unsigned*)(e1000_reg_map + reg));
}

static int
desc_alloc(int sideffct)
{
  volatile struct tx_desc* c;
  uint32_t v = (cursor+1)%E1000_RING_SIZE;

  while(v != (cursor + E1000_RING_SIZE)%E1000_RING_SIZE) {
    c =  (struct tx_desc*) tx_descriptors + v;
    if(!c->status | E1000_TXD_STAT_DD) {
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
  descriptor->addr      = 0;
  descriptor->length    = 0;
  descriptor->cmd       = 0;
  descriptor->status    = 0;
  descriptor->css       = 0;
  descriptor->special   = 0;
}

int
pci_e1000_attach(struct pci_func* f)
{
  cprintf("because Dillon doesn't trust gcc %08x %08x\n", PADDR(&tx_datablocks[0].data), PADDR(&tx_datablocks[0]));

  int i;
  for(i = 0; i < E1000_RING_SIZE; i++)
    cprintf("tx_desc %d is at paddr %08x\n", i, PADDR(&tx_descriptors[i]));

  for(i = 0; i < E1000_RING_SIZE; i++)
    cprintf("tx_desc memory buffer %d is at %08x\n", i, PADDR(&tx_datablocks[i]));

  // Set up the allocation cursor
  cursor = -1;

  // Map the registers using MMIO
  e1000_reg_map = (char*) mmio_map_region(f->reg_base[0], 
                                          f->reg_size[0]);

  // Set the paddr where the ring buffer resides 
  e1000_reg_write(E1000_TDBAL,  PADDR(&tx_descriptors[0].addr));
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
  volatile struct tx_desc* c;

  for(i = 0; i < 32; i++) {
    c =  (struct tx_desc*) tx_descriptors + i;
    c->status |= E1000_TXD_STAT_DD;
    c->cmd    |= E1000_TXD_CMD_RS;
  }

  assert((uint32_t) &tx_descriptors[0] % 16 == 0);

  return 0;
}

int
pci_e1000_tx(void* buffer, unsigned length, unsigned blocking)
{
  int r = e1000_reg_read(E1000_TDT);
  NET_DEBUG("%08x %08x\n", buffer, &tx_datablocks[r]);
  memcpy((void*)tx_datablocks[r].data, buffer, (length < E1000_MAX_TX) ? length :  E1000_MAX_TX);
  if(r < 0) {
    NET_ERR_DEBUG("failed to a allocate a packet descriptor!\n");
    return -E_UNSPECIFIED;
  } else {
    NET_DEBUG("got a descriptor %d, packing and setting it\n", r);
    volatile struct tx_desc* descriptor = (struct tx_desc*) &tx_descriptors[r];

    NET_DEBUG("%08x %08x\n", descriptor, descriptor->addr);
    NET_DEBUG("instructing card to write %d bytes from paddr %08x\n", length, PADDR(tx_datablocks[r].data));

    memset((void*) descriptor, 0, sizeof(struct tx_desc));

    descriptor->addr      = PADDR(tx_datablocks[r].data);
    descriptor->length    = length;

    descriptor->cmd       |= 1;
    descriptor->cmd       |= 1<<3;

    INCMOD(((unsigned*)(e1000_reg_map + E1000_TDT)), E1000_RING_SIZE);

    //DEBUG("[pci_e1000_tx] returning from call...\n");

    while(blocking && (r = desc_alloc(0) < 0))
      continue;

    return 0;
  }
}
