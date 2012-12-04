#include <debug.h>
#include "ns.h"

extern union Nsipc nsipcbuf;
static uint32_t queue_status;
static int cursor;

#define ind2pg(v) ((void*) 0x0ffff000 - v * PGSIZE)

static void
output_thread(uint32_t v)
{
  NET_DEBUG("called with index %d\n", v);
  void* mapva = ind2pg(v);
  struct jif_pkt* packet = (struct jif_pkt*) mapva;
  NET_DEBUG("computed pointer: %08x\n", packet);
  // send the syscall
  sys_net_send(&packet->jp_data[0], packet->jp_len);
  // when this returns we _know_ there exists a free packet descriptor
  // so we can just blindly continue because the presence of a
  // descriptor implies the presence of an unmapped page
  sys_page_unmap(0, mapva);
}

void
output(envid_t ns_envid)
{
  binaryname = "ns_output";

  // LAB 6: Your code here:
  //  - read a packet from the network server
  //  - send the packet to the device driver

  cursor = 0;
  queue_status = ~0;
    
  while(1) {
    sys_ipc_recv((void*) REQVA);
    uint32_t i = (cursor+1)%QUEUE_SIZE;
    void* mapva = ind2pg(i);

    // move that data elsewhere...
    sys_page_map(0, (void*) REQVA, 
                 0, mapva, 
                 (PTE_P | PTE_U | PTE_W));
    sys_page_unmap(0, (void*) REQVA);
    output_thread(i);
    cursor = i;
  } 
}
