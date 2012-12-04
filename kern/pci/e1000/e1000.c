#include <inc/lib.h>
#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pci/e1000/e1000.h>
#include <kern/pci/e1000/e1000_hw.h>

#include <debug.h>

#define DEV_STATUS() (*(uint32_t*)(e1000_reg_map + E1000_STATUS)) 
#define incmod(x, b) (x = ((x + 1) % b))

/*===========================================================================
 * define the static variables which the following code will rely on
 * were this a "real" driver, then these should be packeged in a struct
 * which describes a single card, thus allowing the driver to serve multiple
 * cards of the same type. See the MINIX E1000 driver for a perfect example.
 *===========================================================================*/
volatile char* e1000_reg_map;
volatile char* e1000_flash_map;

struct tx_buffer tx_buffers[E1000_RING_SIZE];
struct tx_desc   tx_descriptors[E1000_RING_SIZE];

/*=============================================================================
 * helper code that I'll use to keep everything neat
 *===========================================================================*/
static void
e1000_reg_assign(uint32_t reg, uint32_t val)
{
  *(uint32_t*)(e1000_reg_map + reg) = val;
}

static void
e1000_reg_set(uint32_t reg, uint32_t val)
{
  *(uint32_t*)(e1000_reg_map + reg) |= val;
}

static void
e1000_reg_unset_except(uint32_t reg, uint32_t val)
{ 
  *(uint32_t*)(e1000_reg_map + reg) &= val;
}

static void
e1000_reg_unset(uint32_t reg, uint32_t val)
{ 
  *(uint32_t*)(e1000_reg_map + reg) &= ~val;
}

static void
e1000_reg_clear(uint32_t reg)
{ 
  *(uint32_t*)(e1000_reg_map + reg) &= 0;
}

static uint32_t
e1000_reg_read(uint32_t reg)
{
  return *(uint32_t*)(e1000_reg_map + reg);
}

/*===========================================================================
 * The pci_attatch method,
 * Takes a pci descriptor as generated in kern/pci.c and configures this device
 * as a single-instance kernel mode driver to interact therewith
 *===========================================================================*/
int
pci_e1000_attach(struct pci_func* f)
{
  // reset just to be sure...
  e1000_reg_set(E1000_CTRL, E1000_CTRL_RST);

  // MMIO map the appropriate chunk to a local static pointer 
  e1000_reg_map = (char*) mmio_map_region(f->reg_base[0], 
                                          f->reg_size[0]);

  DVR_DEBUG("trying to access MMIO'd memory...\n");
  assert(*(uint32_t*)(e1000_reg_map + E1000_STATUS) == 0x80080783); 
  DVR_DEBUG("sucess! device status is %08x\n", DEV_STATUS()); 

  // Set up the transmit ring registers
  // this code equivalent to the MINIX E1000 driver
  e1000_reg_assign(E1000_TDBAL, PADDR(&tx_descriptors[0]));
  e1000_reg_assign(E1000_TDBAH, 0);
  e1000_reg_assign(E1000_TDLEN, E1000_RING_SIZE * sizeof(struct tx_desc));

  e1000_reg_assign(E1000_TDH, 0);
  e1000_reg_assign(E1000_TDT, 0);
  e1000_reg_set(E1000_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP);

  // As the descriptor allocator relies on the dd bit being set,
  // iterate through the descriptors and mark em all free
  int i = 0;
  for(i = 0; i < E1000_RING_SIZE; i++) {
    tx_descriptors[i].status |= E1000_TXD_STAT_DD;
  }

  return 1;
}

/*===========================================================================
 * pci_tx configures a
 * 
 *===========================================================================*/
int 
pci_e1000_tx(void* buffer, uint32_t length)
{
  uint32_t head = e1000_reg_read(E1000_TDH),
           tail = e1000_reg_read(E1000_TDT),
           next = tail;

  length = ((length < PGSIZE) ? length : PGSIZE);

  struct tx_desc* d = &tx_descriptors[next];
  for(; 
      (d->status | E1000_TXD_STAT_DD); 
      incmod(next, E1000_RING_SIZE)) {
    d = &tx_descriptors[next];
  }

  assert(d->status | E1000_TXD_STAT_DD);

  // copy the data to an internal buffer, truncating to pagesize
  memcpy(&tx_buffers[next], buffer, length);

  // unset the DD bit
  d->status &= ~E1000_TXD_STAT_DD;
  // set the address
  d->addr = PADDR(&tx_buffers[next]);
  // set the length
  d->length = length;
  // set the command bits
  d->cmd |=  E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;

  // increment the tail value to whatever next is...
  e1000_reg_assign(E1000_TDT, next);

  return length;
}
