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
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at vpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	if(!(err & FEC_WR) && !(vpt[((uintptr_t) addr) >> PGSHIFT] & PTE_COW))
		panic("pgfault failed!");

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	//   No need to explicitly delete the old page's mapping.

	// LAB 4: Your code here.
	if(sys_page_alloc(0, PFTEMP, PTE_W | PTE_U | PTE_P) < 0)
		panic("pgfault failed!");

	memmove(PFTEMP, (void *) ROUNDDOWN(addr, PGSIZE), PGSIZE);

	if(sys_page_map(0, PFTEMP, 0, (void *) ROUNDDOWN(addr, PGSIZE), PTE_P | PTE_W | PTE_U) < 0)
		panic("pgfault failed!");
	if(sys_page_unmap(0, PFTEMP) < 0)
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
	// LAB 4: Your code here.
	void *addr = (void *) (pn << PTXSHIFT);
	pte_t pte = vpt[pn];

	if (pte & (PTE_W | PTE_COW)){
		if((sys_page_map(0, addr, envid, addr, PTE_P | PTE_U | PTE_COW) < 0) ||
				(sys_page_map(0, addr, 0, addr, PTE_P | PTE_U | PTE_COW) < 0)){
			panic("duppage failed!");
		}
	} else if(sys_page_map(0, addr, envid, addr, PTE_P | PTE_U) < 0){
		panic("duppage failed!");
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
		// Get index of this environment
		thisenv = envs + ENVX(sys_getenvid());
		return 0;
	}

	int i;
	for(i = 0; i < UTOP; i += PGSIZE){
		// Somehow we don't want to copy anything on the exception stack???
		if(i == UXSTACKTOP - PGSIZE) continue;
		// If page is writable or copy on write???
		if(duppage(envid, (unsigned) i)){
			panic("duppage failed!");
		}
		else {
			void *va = (void *) i; // Convert page index to va
			if(sys_page_map(0, va, 0, va, PTE_P | PTE_U | PTE_W) == 0) {
				sys_page_map(0, va, envid, va, PTE_U | PTE_COW);
			} else {
				panic("sys_page_map failed!");
			}
		}
	}


	if(sys_page_alloc(envid, (void *) (UXSTACKTOP - PGSIZE), PTE_P | PTE_U | PTE_W)){
		panic("sys_page_alloc failed!");
	}

	// 4) The parent sets the user page fault entrypoint for the child to look like its own.
	sys_env_set_pgfault_upcall(envid, thisenv->env_pgfault_upcall);

	// 5) The child is now ready to run, so the parent marks it as runnable.
	if((sys_env_set_status(envid, ENV_RUNNABLE)) < 0){
		panic("sys_env_set_status failed!");
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
