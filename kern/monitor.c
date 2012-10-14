// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>

#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>
#include <inc/mmu.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/pmap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "showmappings", "Display page directory info. Args: begin, end", mon_showmappings },
	{ "resetperms", "Reset specified permission bits in VA range. Args: begin, end, perm", mon_resetperms },
	{ "clearperms", "Clear specified permission bits in VA range. Args: begin, end, perm", mon_clearperms },
	{ "setperms", "Set specified permission bits in VA range. Args: begin, end, perm", mon_setperms },
	{ "dumpvmemory", "Dump memory in VA range. Args: begin, end", mon_dumpvmemory },
	{ "dumppmemory", "Dump memory in PA range. Args: begin, end", mon_dumppmemory },
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

// sets specified permission bits
void
set_perm(pde_t *pgdir, const void *va, int perm)
{
        pde_t *pde = (pde_t *) (pgdir+PDX(va));
        pte_t *pte = (pte_t *) KADDR((physaddr_t)((pte_t *)PTE_ADDR(*pde)+PTX(va)));
        *pte |= perm;
}

// clears specified permission bits
void
clear_perm(pde_t *pgdir, const void *va, int perm)
{
        pde_t *pde = (pde_t *) (pgdir+PDX(va));
        pte_t *pte = (pte_t *) KADDR((physaddr_t)((pte_t *)PTE_ADDR(*pde)+PTX(va)));
        *pte &= ~perm;
}

// resets specified permission bits
void
reset_perm(pde_t *pgdir, const void *va, int perm)
{
        pde_t *pde = (pde_t *) (pgdir+PDX(va));
        pte_t *pte = (pte_t *) KADDR((physaddr_t)((pte_t *)PTE_ADDR(*pde)+PTX(va)));
        *pte &= ~0xFFF;
	*pte |= perm;
}

int
mon_resetperms(int argc, char **argv, struct Trapframe *tf){
	if(argc != 4){
                cprintf("Needs 3 arguments: begin end perm\n");
        }
        else {

                size_t i;

                for( i = 0; i < npages; ++i){

                        struct Page * page = &pages[i];
                        if(page != NULL){
                                char * end1;
                                char * end2;
				char * end3;
                                uintptr_t va_lower_bound = strtol(argv[1], &end1, 16);
                                uintptr_t va_upper_bound = strtol(argv[2], &end2, 16);
				int perm = strtol(argv[3], &end3, 16);

                                if(page2kva(page) >= ((void *) va_lower_bound) && page2kva(page) <= ((void *) va_upper_bound)){
					reset_perm(kern_pgdir, page2kva(page), perm);
				}
			}
		}
	}

	return 0;
	
}

int
mon_setperms(int argc, char **argv, struct Trapframe *tf){
        if(argc != 4){
                cprintf("Needs 3 arguments: begin end perm\n");
        }
        else {

                size_t i;

                for( i = 0; i < npages; ++i){

                        struct Page * page = &pages[i];
                        if(page != NULL){
                                char * end1;
                                char * end2;
                                char * end3;
                                uintptr_t va_lower_bound = strtol(argv[1], &end1, 16);
                                uintptr_t va_upper_bound = strtol(argv[2], &end2, 16);
                                int perm = strtol(argv[3], &end3, 16);

                                if(page2kva(page) >= ((void *) va_lower_bound) && page2kva(page) <= ((void *) va_upper_bound)){
                                       set_perm(kern_pgdir, page2kva(page), perm);
                                }
                        }
                }
        }

        return 0;

}

int
mon_clearperms(int argc, char **argv, struct Trapframe *tf){
        if(argc != 4){
                cprintf("Needs 3 arguments: begin end perm\n");
        }
        else {

                size_t i;

                for( i = 0; i < npages; ++i){

                        struct Page * page = &pages[i];
                        if(page != NULL){
                                char * end1;
                                char * end2;
                                char * end3;
                                uintptr_t va_lower_bound = strtol(argv[1], &end1, 16);
                                uintptr_t va_upper_bound = strtol(argv[2], &end2, 16);
                                int perm = strtol(argv[3], &end3, 16);

                                if(page2kva(page) >= ((void *) va_lower_bound) && page2kva(page) <= ((void *) va_upper_bound)){
                                        clear_perm(kern_pgdir, page2kva(page), perm);
                                }
                        }
                }
        }

        return 0;

}


int
mon_dumpvmemory(int argc, char **argv, struct Trapframe *tf){
        if(argc != 3){
                cprintf("Needs 2 arguments: begin end\n");
        }
        else {

                size_t i;

                for( i = 0; i < npages; ++i){

                        struct Page * page = &pages[i];
                        if(page != NULL){
                                char * end1;
                                char * end2;
                                uintptr_t va_lower_bound = strtol(argv[1], &end1, 16);
                                uintptr_t va_upper_bound = strtol(argv[2], &end2, 16);

                                if(page2kva(page) >= ((void *) va_lower_bound) && page2kva(page) <= ((void *) va_upper_bound)){
                                        cprintf("VA: %08x Content: %08x \n", page2kva(page), *((int *) page2kva(page)));
                                }
                        }
                }
        }

        return 0;

}

int
mon_dumppmemory(int argc, char **argv, struct Trapframe *tf){
        if(argc != 3){
                cprintf("Needs 2 arguments: begin end\n");
        }
        else {

                size_t i;

                for( i = 0; i < npages; ++i){

                        struct Page * page = &pages[i];
                        if(page != NULL){
                                char * end1;
                                char * end2;
                                uintptr_t pa_lower_bound = strtol(argv[1], &end1, 16);
                                uintptr_t pa_upper_bound = strtol(argv[2], &end2, 16);

                                if(page2pa(page) >= ((physaddr_t) pa_lower_bound) && page2pa(page) <= ((physaddr_t) pa_upper_bound)){
                                        cprintf("PA: %08x Content: %08x \n", page2pa(page), *((int *) page2kva(page)));
                                }
                        }
                }
        }

        return 0;

}


pte_t
get_perm(pde_t *pgdir, const void *va)
{
        pde_t *pde = (pde_t *) (pgdir+PDX(va));
        pte_t *pte = (pte_t *) KADDR((physaddr_t)((pte_t *)PTE_ADDR(*pde)+PTX(va)));
      	return *pte & 0xFFF;
}

int
mon_showmappings(int argc, char **argv, struct Trapframe *tf){
	if(argc != 3){
		cprintf("Needs 2 arguments: begin end\n");
	}
	else {

		size_t i;

		for( i = 0; i < npages; ++i){

			struct Page * page = &pages[i];
			if(page != NULL){
				char * end1;
				char * end2;
				uintptr_t va_lower_bound = strtol(argv[1], &end1, 16);
				uintptr_t va_upper_bound = strtol(argv[2], &end2, 16);		
			
				if(page2kva(page) >= ((void *) va_lower_bound) && page2kva(page) <= ((void *) va_upper_bound)){

					char perm_flags[22] = "---------------------";
					pte_t perm = get_perm(kern_pgdir, page2kva(page));
					if(perm & PTE_P){
						perm_flags[0] = 'P';
					}
					if(perm & PTE_W){
                                                perm_flags[2] = 'W';
                                        }
					if(perm & PTE_U){
                                                perm_flags[4] = 'U';
                                        }
					if(perm & PTE_PWT){
                                                perm_flags[6] = 'P';
						perm_flags[7] = 'W';
						perm_flags[8] = 'T';
                                        }
					if(perm & PTE_PCD){
                                                perm_flags[10] = 'P';
						perm_flags[11] = 'C';
						perm_flags[12] = 'D';
                                        }
					if(perm & PTE_A){
                                                perm_flags[14] = 'A';
                                        }
					if(perm & PTE_D){
                                                perm_flags[16] = 'D';
                                        }
					if(perm & PTE_PS){
                                                perm_flags[18] = 'P';
						perm_flags[19] = 'S';
                                        }
					if(perm & PTE_G){
                                                perm_flags[21] = 'G';
                                        }

					cprintf("PGNUM: %d  PDX: %d  PTX: %d  VA: %08x  PA: %08x  Perm: %s \n",
					    PGNUM(page2kva(page)),
					    PDX(page2kva(page)),
					    PTX(page2kva(page)),
					    page2kva(page), page2pa(page),
					    perm_flags);

				}
				
			}

		}

	}

	return 0;
}




int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
  physaddr_t eip;
  __asm __volatile("movl 4(%%ebp), %0":"=r" (eip));
  physaddr_t ebp = read_ebp();

  cprintf("Stack backtrace:\n");  
  while(ebp != 0) {
    physaddr_t* pa = (physaddr_t*) ebp;
    cprintf("  ebp %08x  eip %08x  args %08x %08x %08x %08x %08x\n",
        pa, eip, pa[2], pa[3], pa[3], pa[4], pa[5]);
    ebp = *pa;
  }
  cprintf("...\n");

  return 0;
}



/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");


	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
