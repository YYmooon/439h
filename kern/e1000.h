#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H
#include <inc/types.h>
#include <kern/pci.h>
#include <kern/pmap.h>

#define E1000_DEV_ID_82542   0x1000
#define E1000_STATUS         0x00008      /* Device Status - RO */
#define E1000_TCTL           0x00400      /* TX Control - RW */
#define E1000_TDT            0x03818      /* TX Descriptor Tail - RW */
#define E1000_TDH            0x03810      /* TX Descriptor Head - RW */
#define E1000_TXDCTL         0x03828      /* TX Descriptor Control - RW */
#define E1000_TDLEN          0x03808      /* TX Descriptor Length - RW */
#define E1000_TDBAL1         0x03900      /* TX Desc Base Address Low (1) - RW */
#define E1000_TDBAH1         0x03904      /* TX Desc Base Address High (1) - RW */

// transmission
#define E1000_TXD_DTYP_D     0x00100000   /* Data Descriptor */
#define E1000_TXD_DTYP_C     0x00000000   /* Context Descriptor */
#define E1000_TXD_POPTS_IXSM 0x00000001   /* Insert IP checksum */
#define E1000_TXD_POPTS_TXSM 0x00000002   /* Insert TCP/UDP checksum */
#define E1000_TXD_CMD_EOP    0x01000000   /* End of Packet */
#define E1000_TXD_CMD_IFCS   0x02000000   /* Insert FCS (Ethernet CRC) */
#define E1000_TXD_CMD_IC     0x04000000   /* Insert Checksum */
#define E1000_TXD_CMD_RS     0x08000000   /* Report Status */
#define E1000_TXD_CMD_RPS    0x10000000   /* Report Packet Sent */
#define E1000_TXD_CMD_DEXT   0x20000000   /* Descriptor extension (0 = legacy) */
#define E1000_TXD_CMD_VLE    0x40000000   /* Add VLAN tag */
#define E1000_TXD_CMD_IDE    0x80000000   /* Enable Tidv register */
#define E1000_TXD_STAT_DD    0x00000001   /* Descriptor Done */
#define E1000_TXD_STAT_EC    0x00000002   /* Excess Collisions */
#define E1000_TXD_STAT_LC    0x00000004   /* Late Collisions */
#define E1000_TXD_STAT_TU    0x00000008   /* Transmit underrun */
#define E1000_TXD_CMD_TCP    0x01000000   /* TCP packet */
#define E1000_TXD_CMD_IP     0x02000000   /* IP packet */
#define E1000_TXD_CMD_TSE    0x04000000   /* TCP Seg enable */
#define E1000_TXD_STAT_TC    0x00000004   /* Tx Underrun */
#define E1000_TCTL_RST       0x00000001   /* software reset */
#define E1000_TCTL_EN        0x00000002   /* enable tx */
#define E1000_TCTL_BCE       0x00000004   /* busy check enable */
#define E1000_TCTL_PSP       0x00000008   /* pad short packets */
#define E1000_TCTL_CT        0x00000ff0   /* collision threshold */
#define E1000_TCTL_COLD      0x003ff000   /* collision distance */
#define E1000_TCTL_SWXOFF    0x00400000   /* SW Xoff transmission */
#define E1000_TCTL_PBE       0x00800000   /* Packet Burst Enable */
#define E1000_TCTL_RTLC      0x01000000   /* Re-transmit on late collision */
#define E1000_TCTL_NRTU      0x02000000   /* No Re-transmit on underrun */
#define E1000_TCTL_MULR      0x10000000   /* Multiple request support */
#define E1000_CTRL_FD        0x00000001   /* Full duplex.0=half; 1=full */

struct e1000_sta {
  unsigned dd     :1;
  unsigned ec     :1;
  unsigned lc     :1;
  union {
    unsigned rsv  :1;
    unsigned tu   :1;
  };
};

struct e1000_dcmd {
  unsigned eop    :1;
  unsigned ifcs   :1;
  unsigned tse    :1;
  unsigned rs     :1;
  union {
    unsigned rsv  :1;
    unsigned rps  :1;
  };
  unsigned dext   :1;
  unsigned vle    :1;
  unsigned ide    :1;
};

struct tx_desc
{
  uint64_t          addr;
  uint16_t          length;
  uint8_t           cso;
  struct e1000_dcmd cmd;
  struct e1000_sta  status;
  uint8_t           css;
  uint16_t          special;
};

extern volatile char* e1000_reg_map;
extern volatile char* e1000_flash_map;
extern struct tx_desc tx_descriptors[32];

int pci_e1000_attach(struct pci_func* f);
int pci_e1000_tx(void*, unsigned);

#endif  // JOS_KERN_E1000_H
