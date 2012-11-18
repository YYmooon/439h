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
    void* pg_aligned_va = (void*)ROUNDDOWN(addr, PGSIZE);
    unsigned nblocks    = (pg_aligned_va < addr) ? 2 : 1;
    int bar;
    
    if(va_is_mapped(pg_aligned_va)) {
        //// the file is mapped
        cprintf("[bc_pgfault] Fault on mapped page..\n");
        if(utf->utf_err == T_PGFLT) {
            // someone tried to write to the memory range...
            // we don't really care if the page was dirty or not, it's dirty now.
            cprintf("[bc_pgfault] Attempting to remap with write permissions..\n");
            for(r=0; r<nblocks; r++){
                cprintf("[bc_pgfault] Marking va %08x dirty..\n", pg_aligned_va);
                bar = sys_page_map(0, pg_aligned_va, 0, pg_aligned_va, PTE_P | PTE_U | PTE_W);
                if(bar < 0)
                    panic("[bc_pgfault] could not remap page.. error code: %e\n", bar);
                cprintf("[bc_pgfault] ... ok\n");
                pg_aligned_va += PGSIZE;
            }
            cprintf("[bc_pgfault] Remapped page %x dirty and writable\n", blockno);
        } else {
            // how the shit does this happen...
            // don't do anything interesting
            cprintf("[bc_pgfault] Caught an error of ID %x, something is wrong\n", utf->utf_err);
        }

    } else {
        //// the file is unmapped
        // Load the sector in from memory
        // Node that this is bloody stupid as it only allows us access to the first
        // 3GB of space
        
        cprintf("[bc_pgfault] got request for block %x, mapping %d pgs\n", blockno, nblocks);

        for(r=0; r<nblocks; r++)
            sys_page_alloc(0, pg_aligned_va+(r*PGSIZE), PTE_P | PTE_U | PTE_W);

        cprintf("[bc_pgfault] pages mapped, reading data from block %x\n", blockno);

        ide_read(blockno, addr, BLKSECTS);  // the sector size is the block size but w/e
        
        cprintf("[bc_pgfault] block loaded, remapping pages\n", blockno);

        for(r=0; r<nblocks; r++)
            sys_page_map(0, pg_aligned_va+(r*PGSIZE), 0, pg_aligned_va+(r*BLKSIZE), PTE_P | PTE_U);
        
        cprintf("[bc_pgfault] Loaded block %x read only\n", blockno);
        for(r=0; r<nblocks; r++)
            assert(va_is_mapped(pg_aligned_va+(r*PGSIZE)));
    }

    // Check that the block we read was allocated. (exercise for
    // the reader: why do we do this *after* reading the block
    // in?)
    if (bitmap && block_is_free(blockno))
        panic("reading free block %08x\n", blockno);


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
    if(va_is_dirty(addr) && va_is_mapped(addr)) {
        ide_write(blockno, addr, BLKSECTS);
        if(sys_page_map(0, addr, 0, addr, PTE_P | PTE_U) < 0)
            panic("[flush_block] Failed to remap flushed block\n");
        cprintf("[flush_block] wrote block %08x, now mapped r/o\n", blockno);
    } else {
        panic("[flush_block] Tried to flush block in invalid state\n            dirty: %x\n            mapped: %x\n",
                va_is_dirty(addr), va_is_mapped(addr));
    }
}

// Test that the block cache works, by smashing the superblock and
// reading it back.
static void
check_bc(void)
{
    cprintf("[check_bc] starting..\n");
    struct Super backup;

    // back up super block
    memmove(&backup, diskaddr(1), sizeof backup);
    cprintf("[check_bc] Wrote superblock to disk ram block..\n");

    // smash it
    strcpy(diskaddr(1), "OOPS!\n");
    flush_block(diskaddr(1));
    assert(va_is_mapped(diskaddr(1)));
    assert(!va_is_dirty(diskaddr(1)));
    cprintf("[check_bc] Smashed disk superblock..\n");

    // clear it out
    sys_page_unmap(0, diskaddr(1));
    assert(!va_is_mapped(diskaddr(1)));
    cprintf("[check_bc] Unmapped superblock va..\n");

    // read it back in
    assert(strcmp(diskaddr(1), "OOPS!\n") == 0);
    cprintf("[check_bc] re-read superblock va..\n");

    // fix it
    memmove(diskaddr(1), &backup, sizeof backup);
    flush_block(diskaddr(1));
    cprintf("[check_bc] Fixed superblock..\n");

    cprintf("block cache is good\n");
}

void
bc_init(void)
{
    cprintf("[bc_init] Initializing the block count\n");
    set_pgfault_handler(bc_pgfault);
    check_bc();
}

