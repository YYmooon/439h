#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H
#include <inc/types.h>
#include <kern/pci.h>
#include <kern/pmap.h>

#define E1000_RING_SIZE 32
#define E1000_MAX_PACKET 1518

#define E1000_RAL 0x5400
#define E1000_RAH 0x5404

struct rx_desc {
  uint64_t addr;
  uint16_t length;
  uint8_t  checksum;
  uint8_t  status;
  uint8_t  errors;
  uint16_t special;
} __attribute__((aligned(16)));

struct e1000_buffer {
  char data[E1000_MAX_PACKET];
} __attribute__((aligned(PGSIZE)));

struct tx_desc
{
  uint64_t addr;
  uint16_t length;
  uint8_t cso;
  uint8_t cmd;
  uint8_t status;
  uint8_t css;
  uint16_t special;
} __attribute__((aligned(16)));

extern volatile char*       e1000_reg_map;
extern volatile char*       e1000_flash_map;

extern struct e1000_buffer  tx_buffers[E1000_RING_SIZE];
extern struct tx_desc       tx_descriptors[E1000_RING_SIZE];

extern struct e1000_buffer  rx_buffers[E1000_RING_SIZE];
extern struct rx_desc       rx_descriptors[E1000_RING_SIZE];

void pci_e1000_set_addr(char*);
void pci_e1000_attach(struct pci_func* f);
void pci_e1000_init();

int pci_e1000_tx(void* buffer, uint32_t length);
int pci_e1000_rx(void* buffer, uint32_t length);

#endif  // JOS_KERN_E1000_H
