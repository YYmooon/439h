// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW     0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
    void *address = (void *) utf->utf_fault_va;
    uint32_t err = utf->utf_err;

    // Check that the faulting access was (1) a write, and (2) to a
    // copy-on-write page.  If not, panic.
    // Hint:
    //   Use the read-only page table mappings at vpt
    //   (see <inc/memlayout.h>).

    if((err & FEC_WR) && (vpt[PGNUM(address)] & PTE_COW)){

    // Allocate a new page, map it at a temporary location (PFTEMP),
    // copy the data from the old page to the new page, then move the new
    // page to the old page's address.
    // Hint:
    //   You should make three system calls.
    //   No need to explicitly delete the old page's mapping.

    // LAB 4: Your code here.
        if(sys_page_alloc(0, (void *) PFTEMP, PTE_W | PTE_U | PTE_P) < 0)
                    panic("pgfault failed!");

        memmove(PFTEMP, (void *) ROUNDDOWN(address, PGSIZE), PGSIZE);

        if(sys_page_map(0, PFTEMP, 0, (void *) ROUNDDOWN(address, PGSIZE), PTE_W | PTE_U | PTE_P) < 0)
            panic("pgfault failed!");
        if(sys_page_unmap(0, (void *) PFTEMP) < 0)
            panic("pgfault failed!");
        return;
    }
    panic("pgfault failed!");

}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
    pte_t pte = vpt[pn];
    void * address = (void *) (pn * PGSIZE);
    int result;

    if((pte & PTE_W) || (pte & PTE_COW)){
        result = sys_page_map(0, address, envid, address, PTE_U | PTE_P | PTE_COW);
        if(result < 0)
                    panic("duppage failed!");
        result = sys_page_map(0, address, 0, address, PTE_U | PTE_P | PTE_COW);
                if (result < 0)
                        panic("duppage failed!");
        } else {
                int result = sys_page_map(0, address, envid, address, PTE_U | PTE_P | PTE_COW);
                if(result < 0)
                    panic("[duppage] failed to handle page that is present but not writable or copy-on-write");
        }
       
        return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use vpd, vpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
    // LAB 4: Your code here.
    extern void _pgfault_upcall(void);
    extern unsigned char end[];

    // Set up our page fault handler appropriately.

        // 1) The parent installs pgfault() as the C-level page fault handler.
        set_pgfault_handler(pgfault);
      
    // 2) The parent calls sys_exofork() to create a child environment. 
    envid_t envid = sys_exofork();

        if(envid < 0){
            panic("sys_exofork failed!");
        }
    // If child
        else if(envid == 0){
                thisenv = &envs[ENVX(sys_getenvid())];
                return 0;
        }

    /**
     * 3) For each writable or copy-on-write page in its address space below UTOP, the parent calls duppage,
     * which should map the page copy-on-write into the address space of the child and then remap the page 
     * copy-on-write in its own address space. duppage sets both PTEs so that the page is not writeable, 
     * and to contain PTE_COW in the "avail" field to distinguish copy-on-write pages from genuine read-only pages.
     *
     * The exception stack is not remapped this way, however. Instead you need to allocate a fresh page in the child 
     * for the exception stack. Since the page fault handler will be doing the actual copying and the page fault 
     * handler runs on the exception stack, the exception stack cannot be made copy-on-write: who would copy it?
     *
     * fork() also needs to handle pages that are present, but not writable or copy-on-write.
     */

    uint32_t pdx, ptx, px;

        // For each page directory entry from UTEXT TO UXSTACKTOP
        for(pdx = PDX(UTEXT); pdx < PDX(UXSTACKTOP); pdx++){
        // If the page table is present
                if(vpd[pdx] & PTE_P){
            // For each page table entry in this page table
                        for(ptx = 0; ptx < NPTENTRIES; ptx++){
                // Get page number field of this page table entry
                                px = PGNUM(PGADDR(pdx, ptx, 0));

                // Skip the exception stack.
                                if(px >= PGNUM(UXSTACKTOP - PGSIZE)){
                                    break;
                                }

                // If this page is present
                                if(vpt[px] & PTE_P){
                                    duppage(envid, px);
                }
                        }
                }
        }

        // Allocate a fresh page in the child for an exception stack.
        if(sys_page_alloc(envid, (void *) (UXSTACKTOP - PGSIZE), PTE_U | PTE_P | PTE_W) < 0){
                panic("fork failed while allocating exception stack for child!");
        }

        // Map child's exception stack to a temporary page.
        if(sys_page_map(envid, (void *) (UXSTACKTOP - PGSIZE), 0, UTEMP,
           PTE_U | PTE_P | PTE_W) < 0){
                panic("fork failed while mapping temporary page!");
        }

        // Copy this exception stack to temporary page.
        memmove(UTEMP, (void *) (UXSTACKTOP - PGSIZE), PGSIZE);

        // Unmap the temporary page.
        if(sys_page_unmap(0, UTEMP) < 0){
            panic("fork failed while unmapping temporary page!");
        }

    // 4) The parent sets the user page fault entrypoint for the child to look like its own.
        if(sys_env_set_pgfault_upcall(envid, _pgfault_upcall) < 0){
                panic("fork failed while trying to set user page fault entrypoint for the child to look like its own.");
        }

        // 5) The child is now ready to run, so the parent marks it as runnable.
        if(sys_env_set_status(envid, ENV_RUNNABLE) < 0){
        panic("fork failed while marking child env as runnable!");
        }

        return envid;
}


// Challenge!
    int
sfork(void)
{
    panic("sfork not implemented");
    return -E_INVAL;
}
