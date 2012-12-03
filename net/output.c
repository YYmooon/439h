#include <arch/thread.h>
#include "ns.h"

//#define threading

extern union Nsipc nsipcbuf;
static uint32_t queue_status;
static int cursor;


static int
queue_alloc()
{
  uint32_t v=cursor, bit, mask;
  while(v < cursor + QUEUE_SIZE) {
    bit = 1<<((v+1)%QUEUE_SIZE);
    mask = ~bit;
    if(queue_status & bit) {
      queue_status &= mask;
      cursor = v;
      return v%QUEUE_SIZE;
    }
    v++;
  }
  return -1;
}

static void
queue_pg_free(uint32_t i)
{
  queue_status |= 1<<i;
}

#define ind2pg(v) ((void*) 0x0ffff000 - v * PGSIZE)

static void
output_thread(uint32_t v)
{
  cprintf("[output_thread] called with index %d\n", v);
  void* mapva = ind2pg(v);
  struct jif_pkt* packet = (struct jif_pkt*) mapva;
  cprintf("[output_thread] computed pointer: %08x\n", packet);
  // send the syscall
  sys_net_blocking_send(&packet->jp_data[0], packet->jp_len);
  // when this returns we _know_ there exists a free packet descriptor
  // so we can just blindly continue because the presence of a
  // descriptor implies the presence of an unmapped page
  sys_page_unmap(0, mapva);
  queue_pg_free(v);
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
#ifdef threading
    for (i = 0; thread_wakeups_pending() && i < 32; ++i)
      thread_yield();
#endif
    sys_ipc_recv((void*) REQVA);
    uint32_t i = queue_alloc();
    void* mapva = ind2pg(i);

    // move that data elsewhere...
    sys_page_map(0, (void*) REQVA, 
                 0, mapva, 
                 (PTE_P | PTE_U | PTE_W));
    sys_page_unmap(0, (void*) REQVA);
#ifdef threading
    thread_create(0, "output_thread", output_thread, i);
    thread_yield();
#else
    output_thread(i);
#endif
  } 
}
