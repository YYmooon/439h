#include <kern/env.h>
#include <kern/e1000.h>
#include <kern/e1000_hw.h>

#include <inc/string.h>
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

struct e1000_buffer tx_buffers[E1000_RING_SIZE];
struct tx_desc      tx_descriptors[E1000_RING_SIZE];

struct e1000_buffer rx_buffers[E1000_RING_SIZE];
struct rx_desc      rx_descriptors[E1000_RING_SIZE];

static char addr[] = {0x52, 0x54, 0x0, 0x12, 0x34, 0x56};

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
void
pci_e1000_attach(struct pci_func* f)
{
  // MMIO map the appropriate chunk to a local static pointer 
  e1000_reg_map = (char*) mmio_map_region(f->reg_base[0], 
                                          f->reg_size[0]);

  e1000_reg_set(E1000_CTRL, E1000_CTRL_RST);

  pci_e1000_init();
}

void
pci_e1000_init()
{
  /*=========================================================================
   * test the MMIO region first...
   *=========================================================================*/
  DVR_DEBUG("trying to access MMIO'd memory...\n");
  assert(*(uint32_t*)(e1000_reg_map + E1000_STATUS) == 0x80080783); 
  DVR_DEBUG("sucess! device status is %08x\n", DEV_STATUS()); 

  /*=========================================================================
   * Set up the transmit ring registers. this code equivalent to the MINIX 
   * E1000 driver, and is derived in large part therefrom in an effort to
   * ensure both readability, and correctness
   *=========================================================================*/
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

  /*=========================================================================
   * set up for packet reception
   *=========================================================================*/

  // do the basic registers first, essentially the same as transmission
  e1000_reg_assign(E1000_RDBAL, PADDR(&rx_descriptors[0]));
  e1000_reg_assign(E1000_RDBAH, 0);
  e1000_reg_assign(E1000_RDLEN, E1000_RING_SIZE * sizeof(struct rx_desc));

  e1000_reg_assign(E1000_RDH, 0);
  e1000_reg_assign(E1000_RDT, 0);
  e1000_reg_set(E1000_RCTL, E1000_RCTL_EN | E1000_RCTL_MPE);

  for(i = 0; i < E1000_RING_SIZE; i++) {
    rx_descriptors[i].addr = PADDR(&rx_buffers[i]);
    rx_descriptors[i].length = E1000_MAX_PACKET;
  }

  /*=========================================================================
   * ensure that an address is set, although software may change it at any time
   *=========================================================================*/

  pci_e1000_set_addr(0);
}

void
pci_e1000_set_addr(char* a)
{
  a = a ? a : addr;
  // set to filter by MAC address
  e1000_reg_set(E1000_RAL, *(uint32_t*)a);
  e1000_reg_set(E1000_RAH, (*(uint16_t*)&a[4]) | E1000_RAH_AV);
}

/*===========================================================================
 * pci_tx allocates and initializes a packet descriptor, then updates the ring
 * buffer's base so that it will be transmitted. Will block until there is an
 * open descriptor as judged by the spec-ensured DD bit.
 *===========================================================================*/
int 
pci_e1000_tx(void* buffer, uint32_t length)
{
  uint32_t head = e1000_reg_read(E1000_TDH),
           tail = e1000_reg_read(E1000_TDT),
           next = tail;

  length = ((length < PGSIZE) ? length : PGSIZE);
  DVR_DEBUG("truncated length is %d bytes\n", length);

  volatile struct tx_desc* d = &tx_descriptors[next];
  for(; 
      !(d->status | E1000_TXD_STAT_DD); 
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
  d->cmd |=  1 | 1<<3;

  DVR_DEBUG("%08x %08x %08x\n", d->addr, d->length, d->status);

  // increment the tail value to whatever next is...
  incmod(next, E1000_RING_SIZE);
  e1000_reg_assign(E1000_TDT, next);

  return length;
}

/*===========================================================================
 * pci_rx takes the first packet off of the inbound ring buffer and dumps it
 * to a user-supplied buffer clearing the recieve buffer and marking the now
 * empty entry as usable.
 *
 * Returns the number of bytes read, and 0 if there were no packets to read
 * or if the buffer indicated is of insifficient size. If there were errors,
 * those are returned and the data is still read.
 *===========================================================================*/
int
pci_e1000_rx(void* buffer, uint32_t length)
{
  uint32_t head = e1000_reg_read(E1000_RDH),
           tail = e1000_reg_read(E1000_RDT),
           cur  = (tail + 1) % E1000_RING_SIZE;

  volatile struct rx_desc* d = &rx_descriptors[cur];

  if(!d->status) return 0;
  if(d->length > length) return 0;

  // get the bytes out of there...
  memcpy(buffer, &rx_buffers[cur], d->length);
  length = d->errors ? d->errors : d->length;

  // reset the descriptor
  d->length = E1000_MAX_PACKET;
  d->status = 0;
  d->errors = 0;

  // bump the tail
  e1000_reg_assign(E1000_RDT, cur);

  return length;
}

