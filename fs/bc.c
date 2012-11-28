#include <debug.h>
#include "fs.h"

// Return the virtual address of this disk block.
void*
diskaddr(uint32_t blockno)
{
    //if(super) DEBUG("superblock %08x", super);
    if (blockno == 0 || (super && blockno >= super->s_nblocks))
        panic("bad block number %08x in diskaddr, max is %08x and 0 is protected", 
              blockno, super->s_nblocks);
    char* foo = (char*) (DISKMAP + blockno * BLKSIZE);
    return foo;
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
    void *addr       = (void *) ROUNDDOWN(utf->utf_fault_va, PGSIZE);
    uint32_t sectno  = ((uint32_t)addr - DISKMAP) / SECTSIZE;
    uint32_t blockno = (sectno / BLKSECTS);
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
 
    if(va_is_mapped(addr)) {
        //// the file is mapped
        BC_DEBUG("Fault on mapped page..\n");
        if(utf->utf_err == T_PGFLT) {
            // someone tried to write to the memory range...
            // we don't really care if the page was dirty or not, it's dirty now.
            sys_page_map(0, addr, 
                         0, addr, 
                         (PTE_P | PTE_U | PTE_W));

            BC_DEBUG("Remapped page %x writable\n", blockno);
        } else {
            // how the shit does this happen...
            // don't do anything interesting
            panic("[bc_pgfault] Caught an error of ID %x, something is wrong\n", 
                  utf->utf_err);
        }
    } else {
        //// the file is unmapped
        // Load the sector in from memory
        // Node that this is bloody stupid as it only allows us 
        // access to the first 3GB of space
        
        BC_DEBUG("Loading %08x sectors starting at sector %08x (block %08x)\n", 
              BLKSECTS, sectno, blockno);
    
        sys_page_alloc(0, addr, PTE_P | PTE_U | PTE_W);

        if(ide_read(sectno, addr, BLKSECTS) < 0)
          panic("failed to read data from disk..");

        sys_page_map(0, addr, 
                     0, addr, 
                     (PTE_P | PTE_U));
        
        BC_DEBUG("Loaded block %x read only\n", blockno);
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

    sys_env_recovered();
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
    addr             = (void *) ROUNDDOWN(addr, PGSIZE);
    uint32_t sectno  = ((uint32_t)addr - DISKMAP) / SECTSIZE;
    uint32_t blockno = (sectno / BLKSECTS);

    if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
        panic("flush_block of bad va %08x", addr);

    // LAB 5: Your code here.
    if(va_is_dirty(addr) && va_is_mapped(addr)) {
        ide_write(sectno, addr, BLKSECTS);
        sys_page_map(0, addr, 
                     0, addr,
                     (PTE_SYSCALL));
        BC_DEBUG("Wrote block %08x, now mapped r/o\n", blockno);
    }
}

// Test that the block cache works, by smashing the superblock and
// reading it back.
static void
check_bc(void)
{
    cprintf("Starting..\n");
    struct Super backup;

    // back up super block
    memmove(&backup, diskaddr(1), sizeof(backup));
    BC_DEBUG("Wrote superblock to disk ram block..\n");
    BC_DEBUG("in memory magic number: %08x\n", ((struct Super*)diskaddr(1))->s_magic);

    // smash it
    strcpy(diskaddr(1), "OOPS!\n");
    flush_block(diskaddr(1));
    assert(va_is_mapped(diskaddr(1)));
    assert(!va_is_dirty(diskaddr(1)));
    cprintf("Smashed disk superblock..\n");

    // clear it out
    sys_page_unmap(0, diskaddr(1));
    assert(!va_is_mapped(diskaddr(1)));
    cprintf("Unmapped superblock va..\n");

    // read it back in
    assert(strcmp(diskaddr(1), "OOPS!\n") == 0);
    cprintf("re-read superblock va..\n");

    // fix it
    memmove(diskaddr(1), &backup, sizeof(backup));
    assert(memcmp(diskaddr(1), &backup, sizeof(backup)) == 0);
    
    flush_block(diskaddr(1));

    assert(memcmp(diskaddr(1), &backup, sizeof(backup)) == 0);
    BC_DEBUG("backup magic number   : %08x\n", backup.s_magic);
    BC_DEBUG("in memory magic number: %08x\n", ((struct Super*)diskaddr(1))->s_magic);
    BC_DEBUG("expected magic value  : %08x\n", FS_MAGIC);
    cprintf("Fixed superblock..\n");

    cprintf("block cache is good\n");
}

void
bc_init(void)
{
    BC_DEBUG("Initializing the block count\n");
    set_pgfault_handler(bc_pgfault);
    check_bc();
}

