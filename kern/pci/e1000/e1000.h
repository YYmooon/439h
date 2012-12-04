#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H
#include <inc/types.h>
#include <kern/pci.h>
#include <kern/pmap.h>

#define E1000_RING_SIZE 32

struct tx_buffer {
  char data[PGSIZE];
}__attribute__((align(PGSIZE)));

struct tx_desc
{
  uint64_t addr;
  uint16_t length;
  uint8_t cso;
  uint8_t cmd;
  uint8_t status;
  uint8_t css;
  uint16_t special;
}__attribute__((align(16)));

extern volatile char* e1000_reg_map;
extern volatile char* e1000_flash_map;
extern struct tx_buffer tx_buffers[E1000_RING_SIZE];
extern struct tx_desc   tx_descriptors[E1000_RING_SIZE];

int pci_e1000_attach(struct pci_func* f);
int pci_e1000_tx(void* buffer, uint32_t length);

#endif  // JOS_KERN_E1000_H
