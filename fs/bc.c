#include "fs.h"

// Return the virtual address of this disk block.
void*
diskaddr(uint32_t blockno)
{
    if (blockno == 0 || (super && blockno >= super->s_nblocks))
        panic("bad block number %08x in diskaddr", blockno);
    return (char*) (DISKMAP + blockno * BLKSIZE);
}

// Is this virtual address mapped?
bool
va_is_mapped(void *va)
{
    return (vpd[PDX(va)] & PTE_P) && (vpt[PGNUM(va)] & PTE_P);
}

// Is this virtual address dirty?
bool
va_is_dirty(void *va)
{
    return (vpt[PGNUM(va)] & PTE_D) != 0;
}

// util function
bool
va_mark_dirty(void *va)
{
    return vpt[PGNUM(va)] |= PTE_D;
}
// Fault any disk block that is read or written in to memory by
// loading it from disk.
// Hint: Use ide_read and BLKSECTS.
static void
bc_pgfault(struct UTrapframe *utf)
{
    void *addr = (void *) utf->utf_fault_va;
    uint32_t blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE;
    int r;

    // Check that the fault was within the block cache region
    if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
        panic("page fault in FS: eip %08x, va %08x, err %04x",
              utf->utf_eip, addr, utf->utf_err);

    // Sanity check the block number.
    if (super && blockno >= super->s_nblocks)
        panic("reading non-existent block %08x\n", blockno);

    // Allocate a page in the disk map region, read the contents
    // of the block from the disk into that page, and mark the
    // page not-dirty (since reading the data from disk will mark
    // the page dirty).
    //
    // LAB 5: Your code here
    //
    if(va_is_mapped(pg_aligned_va)) {
        //// the file is mapped
        if(utf->utf_err == T_PGFLT) {
          // someone tried to write to the memory range...
          // we don't really care if the page was dirty or not, it's dirty now.
          va_mark_dirty(pg_aligned_va);
          sys_page_map(0, pg_aligned_va, 0, pg_aligned_va, PTE_P | PTE_U | PTE_W);
        } else {
          // how the shit does this happen...
          // don't do anything interesting
        }

    } else {
        //// the file is unmapped
        // Load the sector in from memory
        // Node that this is bloody stupid as it only allows us access to the first
        // 3GB of space
        sys_page_alloc(0, addr, PTE_P | PTE_U | PTE_W);
        ide_read(blockno, addr, BLKSECTS);  // the sector size is the block size but w/e
        sys_page_map(0, addr, 0, addr, PTE_P | PTE_U);
    }

    // Check that the block we read was allocated. (exercise for
    // the reader: why do we do this *after* reading the block
    // in?)
    if (bitmap && block_is_free(blockno))
        panic("reading free block %08x\n", blockno);
      void* pg_aligned_va = (void*) ROUNDDOWN(addr, PGSIZE);


    // Check that the block we read was allocated. (exercise for
    // the reader: why do we do this *after* reading the block
    // in?)
    if (bitmap && block_is_free(blockno))
    	panic("reading free block %08x\n", blockno);
}

// Flush the contents of the block containing VA out to disk if
// necessary, then clear the PTE_D bit using sys_page_map.
// If the block is not in the block cache or is not dirty, does
// nothing.
// Hint: Use va_is_mapped, va_is_dirty, and ide_write.
// Hint: Use the PTE_SYSCALL constant when calling sys_page_map.
// Hint: Don't forget to round addr down.
void
flush_block(void *addr)
{
    uint32_t blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE;

    if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
        panic("flush_block of bad va %08x", addr);

    // LAB 5: Your code here.
    if(va_is_dirty(addr) && va_is_mapped(addr))
        ide_write(blockno, addr, BLKSECTS);
}

// Test that the block cache works, by smashing the superblock and
// reading it back.
static void
check_bc(void)
{
    struct Super backup;

    // back up super block
    memmove(&backup, diskaddr(1), sizeof backup);

    // smash it
    strcpy(diskaddr(1), "OOPS!\n");
    flush_block(diskaddr(1));
    assert(va_is_mapped(diskaddr(1)));
    assert(!va_is_dirty(diskaddr(1)));

    // clear it out
    sys_page_unmap(0, diskaddr(1));
    assert(!va_is_mapped(diskaddr(1)));

    // read it back in
    assert(strcmp(diskaddr(1), "OOPS!\n") == 0);

    // fix it
    memmove(diskaddr(1), &backup, sizeof backup);
    flush_block(diskaddr(1));

    cprintf("block cache is good\n");
}

void
bc_init(void)
{
    set_pgfault_handler(bc_pgfault);
    check_bc();
}

