#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H
#include <inc/types.h>
#include <kern/pci.h>
#include <kern/pmap.h>

#define E1000_DEV_ID_82542  0x1000
#define E1000_STATUS        0x00008       /* Device Status - RO */

// transmission
#define E1000_TCTL          0x00400       /* TX Control - RW */
#define E1000_TDLEN         0x03808       /* TX Descriptor Length - RW */
#define E1000_TDH           0x03810       /* TX Descriptor Head - RW */
#define E1000_TDT           0x03818       /* TX Descripotr Tail - RW */
#define E1000_TCTL_RST      0x00000001    /* software reset */
#define E1000_TCTL_EN       0x00000002    /* enable tx */
#define E1000_TCTL_BCE      0x00000004    /* busy check enable */
#define E1000_TCTL_PSP      0x00000008    /* pad short packets */
#define E1000_TCTL_CT       0x00000ff0    /* collision threshold */
#define E1000_TCTL_COLD     0x003ff000    /* collision distance */
#define E1000_TCTL_SWXOFF   0x00400000    /* SW Xoff transmission */
#define E1000_TCTL_PBE      0x00800000    /* Packet Burst Enable */
#define E1000_TCTL_RTLC     0x01000000    /* Re-transmit on late collision */
#define E1000_TCTL_NRTU     0x02000000    /* No Re-transmit on underrun */
#define E1000_TCTL_MULR     0x10000000    /* Multiple request support */

struct tx_desc
{
  uint64_t addr;
  uint16_t length;
  uint8_t cso;
  uint8_t cmd;
  uint8_t status;
  uint8_t css;
  uint16_t special;
};

volatile char* e1000_reg_map;
volatile char* e1000_flash_map;
volatile struct tx_desc tx_descriptors[32];

int pci_e1000_attach(struct pci_func* f);

#endif  // JOS_KERN_E1000_H
