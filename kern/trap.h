/* See COPYRIGHT for copyright information. */

#ifndef JOS_KERN_TRAP_H
#define JOS_KERN_TRAP_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

#include <inc/trap.h>
#include <inc/mmu.h>

/* The kernel's interrupt descriptor table */
extern struct Gatedesc idt[];
extern struct Pseudodesc idt_pd;

extern void trap_divide(void);
extern void trap_debug(void);
extern void trap_nmi(void);
extern void trap_brkpt(void);
extern void trap_oflow(void);
extern void trap_bound(void);
extern void trap_illop(void);
extern void trap_device(void);
extern void trap_dblflt(void);
extern void trap_tss(void);
extern void trap_segnp(void);
extern void trap_gpflt(void);
extern void trap_pgflt(void);
extern void trap_fperr(void);
extern void trap_align(void);
extern void trap_mchk(void);
extern void trap_simderr(void);
extern void trap_syscall(void);
extern void trap_default(void);

void trap_init(void);
void trap_init_percpu(void);
void print_regs(struct PushRegs *regs);
void print_trapframe(struct Trapframe *tf);
void page_fault_handler(struct Trapframe *);
void backtrace(struct Trapframe *);

#endif /* JOS_KERN_TRAP_H */
