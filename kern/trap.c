#include <inc/mmu.h>
#include <inc/x86.h>
#include <inc/assert.h>

#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/env.h>
#include <kern/syscall.h>
#include <kern/sched.h>
#include <kern/kclock.h>
#include <kern/picirq.h>
#include <kern/cpu.h>
#include <kern/spinlock.h>
#include <kern/kdebug.h>

#include <debug.h>

static struct Taskstate ts;
static int faultcount;

/* For debugging, so print_trapframe can distinguish between printing
 * a saved trapframe and printing the current trapframe and print some
 * additional information in the latter case.
 */
static struct Trapframe *last_tf;

/* Interrupt descriptor table.  (Must be built at run time because
 * shifted function addresses can't be represented in relocation records.)
 */
struct Gatedesc idt[256] = { { 0 } };
struct Pseudodesc idt_pd = {
    sizeof(idt) - 1, (uint32_t) idt
};


static const char *trapname(int trapno)
{
    static const char * const excnames[] = {
        "Divide error",
        "Debug",
        "Non-Maskable Interrupt",
        "Breakpoint",
        "Overflow",
        "BOUND Range Exceeded",
        "Invalid Opcode",
        "Device Not Available",
        "Double Fault",
        "Coprocessor Segment Overrun",
        "Invalid TSS",
        "Segment Not Present",
        "Stack Fault",
        "General Protection",
        "Page Fault",
        "(unknown trap)",
        "x87 FPU Floating-Point Error",
        "Alignment Check",
        "Machine-Check",
        "SIMD Floating-Point Exception"
    };

    if (trapno < sizeof(excnames)/sizeof(excnames[0]))
        return excnames[trapno];
    if (trapno == T_SYSCALL)
        return "System call";
    if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + 16)
        return "Hardware Interrupt";
    return "(unknown trap)";
}



void
trap_init(void)
{
    extern struct Segdesc gdt[];
    faultcount = 0;

    // LAB 3: Your code here.
    extern void divide();
    extern void debug();
    extern void nmi();
    extern void brkpt();
    extern void oflow();
    extern void bound();
    extern void illop();
    extern void device();
    extern void dblflt();
    extern void tss();
    extern void segnp();
    extern void stack();
    extern void gpflt();
    extern void pgflt();
    extern void fperr();
    extern void align();
    extern void mchk();
    extern void simderr();
    extern void system_call();

    SETGATE(idt[T_DIVIDE], 0, GD_KT, divide, 0);  
    SETGATE(idt[T_DEBUG], 0, GD_KT, debug, 0);  
    SETGATE(idt[T_NMI], 0, GD_KT, nmi, 0);  
    SETGATE(idt[T_BRKPT], 0, GD_KT, brkpt, 3);  
    SETGATE(idt[T_OFLOW], 0, GD_KT, oflow, 0);  
    SETGATE(idt[T_BOUND], 0, GD_KT, bound, 0);  
    SETGATE(idt[T_ILLOP], 0, GD_KT, illop, 0);  
    SETGATE(idt[T_DEVICE], 0, GD_KT, device, 0);  
    SETGATE(idt[T_DBLFLT], 0, GD_KT, dblflt, 0);  
    SETGATE(idt[T_TSS], 0, GD_KT, tss, 0);  
    SETGATE(idt[T_SEGNP], 0, GD_KT, segnp, 0);  
    SETGATE(idt[T_STACK], 0, GD_KT, stack, 0);  
    SETGATE(idt[T_GPFLT], 0, GD_KT, gpflt, 0);  
    SETGATE(idt[T_PGFLT], 0, GD_KT, pgflt, 0);  
    SETGATE(idt[T_FPERR], 0, GD_KT, fperr, 0);  
    SETGATE(idt[T_ALIGN], 0, GD_KT, align, 0);  
    SETGATE(idt[T_MCHK], 0, GD_KT, mchk, 0);  
    SETGATE(idt[T_SIMDERR], 0, GD_KT, simderr, 0);
    SETGATE(idt[T_SYSCALL], 0, GD_KT, system_call, 3);

    extern void irq0();
    extern void irq1();
    extern void irq2();
    extern void irq3();
    extern void irq4();
    extern void irq5();
    extern void irq6();
    extern void irq7();
    extern void irq8();
    extern void irq9();
    extern void irq10();
    extern void irq11();
    extern void irq12();
    extern void irq13();
    extern void irq14();
    extern void irq15();

    SETGATE(idt[IRQ_OFFSET], 0, GD_KT, irq0, 0);
    SETGATE(idt[IRQ_OFFSET + 1], 0, GD_KT, irq1, 0);
    SETGATE(idt[IRQ_OFFSET + 2], 0, GD_KT, irq2, 0);
    SETGATE(idt[IRQ_OFFSET + 3], 0, GD_KT, irq3, 0);
    SETGATE(idt[IRQ_OFFSET + 4], 0, GD_KT, irq4, 0);
    SETGATE(idt[IRQ_OFFSET + 5], 0, GD_KT, irq5, 0);
    SETGATE(idt[IRQ_OFFSET + 6], 0, GD_KT, irq6, 0);
    SETGATE(idt[IRQ_OFFSET + 7], 0, GD_KT, irq7, 0);
    SETGATE(idt[IRQ_OFFSET + 8], 0, GD_KT, irq8, 0);
    SETGATE(idt[IRQ_OFFSET + 9], 0, GD_KT, irq9, 0);
    SETGATE(idt[IRQ_OFFSET + 10], 0, GD_KT, irq10, 0);
    SETGATE(idt[IRQ_OFFSET + 11], 0, GD_KT, irq11, 0);
    SETGATE(idt[IRQ_OFFSET + 12], 0, GD_KT, irq12, 0);
    SETGATE(idt[IRQ_OFFSET + 13], 0, GD_KT, irq13, 0);
    SETGATE(idt[IRQ_OFFSET + 14], 0, GD_KT, irq14, 0);
    SETGATE(idt[IRQ_OFFSET + 15], 0, GD_KT, irq15, 0);

    // Per-CPU setup 
    trap_init_percpu();
}

// Initialize and load the per-CPU TSS and IDT
void
trap_init_percpu(void)
{
    // The example code here sets up the Task State Segment (TSS) and
    // the TSS descriptor for CPU 0. But it is incorrect if we are
    // running on other CPUs because each CPU has its own kernel stack.
    // Fix the code so that it works for all CPUs.
    //
    // Hints:
    //   - The macro "thiscpu" always refers to the current CPU's
    //     struct Cpu;
    //   - The ID of the current CPU is given by cpunum() or
    //     thiscpu->cpu_id;
    //   - Use "thiscpu->cpu_ts" as the TSS for the current CPU,
    //     rather than the global "ts" variable;
    //   - Use gdt[(GD_TSS0 >> 3) + i] for CPU i's TSS descriptor;
    //   - You mapped the per-CPU kernel stacks in mem_init_mp()
    //
    // ltr sets a 'busy' flag in the TSS selector, so if you
    // accidentally load the same TSS on more than one CPU, you'll
    // get a triple fault.  If you set up an individual CPU's TSS
    // wrong, you may not get a fault until you try to return from
    // user space on that CPU.
    //
    // LAB 4: Your code here:

    // Setup a TSS so that we get the right stack
    // when we trap to the kernel.
    thiscpu->cpu_ts.ts_esp0 = KSTACKTOP - cpunum() * (KSTKSIZE + KSTKGAP);
    thiscpu->cpu_ts.ts_ss0 = GD_KD;
    // Initialize the TSS slot of the gdt.
    gdt[(GD_TSS0 >> 3) + cpunum()] = SEG16(STS_T32A, (uint32_t) (&thiscpu->cpu_ts),
        sizeof(struct Taskstate), 0);
    gdt[(GD_TSS0 >> 3) + cpunum()].sd_s = 0;
    // Load the TSS selector (like other segment selectors, the
    // bottom three bits are special; we leave them 0)
    ltr(((GD_TSS0 >> 3) + cpunum()) << 3);
    // Load the IDT
    lidt(&idt_pd);
}

void
print_trapframe(struct Trapframe *tf)
{
    cprintf("TRAP frame at %p from CPU %d\n", tf, cpunum());
    print_regs(&tf->tf_regs);
    cprintf("  es   0x----%04x\n", tf->tf_es);
    cprintf("  ds   0x----%04x\n", tf->tf_ds);
    cprintf("  trap 0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
    // If this trap was a page fault that just happened
    // (so %cr2 is meaningful), print the faulting linear address.
    if (tf == last_tf && tf->tf_trapno == T_PGFLT)
        cprintf("  cr2  0x%08x\n", rcr2());
    cprintf("  err  0x%08x", tf->tf_err);
    // For page faults, print decoded fault error code:
    // U/K=fault occurred in user/kernel mode
    // W/R=a write/read caused the fault
    // PR=a protection violation caused the fault (NP=page not present).
    if (tf->tf_trapno == T_PGFLT)
        cprintf(" [%s, %s, %s]\n",
            tf->tf_err & 4 ? "user" : "kernel",
            tf->tf_err & 2 ? "write" : "read",
            tf->tf_err & 1 ? "protection" : "not-present");
    else
        cprintf("\n");
    cprintf("  eip  0x%08x\n", tf->tf_eip);
    cprintf("  cs   0x----%04x\n", tf->tf_cs);
    cprintf("  flag 0x%08x\n", tf->tf_eflags);
    if ((tf->tf_cs & 3) != 0) {
        cprintf("  esp  0x%08x\n", tf->tf_esp);
        cprintf("  ss   0x----%04x\n", tf->tf_ss);
    }
}

void
print_regs(struct PushRegs *regs)
{
    cprintf("  edi  0x%08x\n", regs->reg_edi);
    cprintf("  esi  0x%08x\n", regs->reg_esi);
    cprintf("  ebp  0x%08x\n", regs->reg_ebp);
    cprintf("  oesp 0x%08x\n", regs->reg_oesp);
    cprintf("  ebx  0x%08x\n", regs->reg_ebx);
    cprintf("  edx  0x%08x\n", regs->reg_edx);
    cprintf("  ecx  0x%08x\n", regs->reg_ecx);
    cprintf("  eax  0x%08x\n", regs->reg_eax);
}

static void
trap_dispatch(struct Trapframe *tf)
{
    // Handle processor exceptions.
    // LAB 3: Your code here.
    if(tf->tf_trapno == T_PGFLT){
        if(curenv) {
          curenv->env_fault_count++;
        } if(curenv && curenv->env_fault_count > 5) {
            panic("SHIT BE B0RKEN\n");
        }
        page_fault_handler(tf);
    } else if(tf->tf_trapno == T_BRKPT){
        monitor(tf);
    } else if(tf->tf_trapno == T_SYSCALL){
        tf->tf_regs.reg_eax = syscall(tf->tf_regs.reg_eax,
            tf->tf_regs.reg_edx,
            tf->tf_regs.reg_ecx,
            tf->tf_regs.reg_ebx,
            tf->tf_regs.reg_edi,
            tf->tf_regs.reg_esi);

        return;
    }
    // Handle spurious interrupts
    // The hardware sometimes raises these because of noise on the
    // IRQ line or other reasons. We don't care.
    else if (tf->tf_trapno == IRQ_OFFSET + IRQ_SPURIOUS) {
        KT_DEBUG("Spurious interrupt on irq 7\n");
        print_trapframe(tf);
        return;
    }

    // Handle clock interrupts. Don't forget to acknowledge the
    // interrupt using lapic_eoi() before calling the scheduler!
    // LAB 4: Your code here.
    else if(tf->tf_trapno == IRQ_OFFSET + IRQ_TIMER){
        lapic_eoi();
        sched_yield();
        return;
    }

    // Unexpected trap: The user process or the kernel has a bug.
    else if (tf->tf_cs == GD_KT) {
        print_trapframe(tf);
        panic("unhandled trap in kernel");
    } else {
        print_trapframe(tf);
        env_destroy(curenv);
        return;
    }
}

void
trap(struct Trapframe *tf)
{
    // The environment may have set DF and some versions
    // of GCC rely on DF being clear
    asm volatile("cld" ::: "cc");

    // Halt the CPU if some other CPU has called panic()
    extern char *panicstr;
    if (panicstr)
        asm volatile("hlt");

    // Check that interrupts are disabled.  If this assertion
    // fails, DO NOT be tempted to fix it by inserting a "cli" in
    // the interrupt path.
    assert(!(read_eflags() & FL_IF));


    if ((tf->tf_cs & 3) == 3) {
        // Trapped from user mode.
        // Acquire the big kernel lock before doing any
        // serious kernel work.
        // LAB 4: Your code here.
        lock_kernel();
        assert(curenv);

        // Garbage collect if current enviroment is a zombie
        if (curenv->env_status == ENV_DYING) {
            env_free(curenv);
            curenv = NULL;
            sched_yield();
        }

        // Copy trap frame (which is currently on the stack)
        // into 'curenv->env_tf', so that running the environment
        // will restart at the trap point.
        curenv->env_tf = *tf;
        // The trapframe on the stack should be ignored from here on.
        tf = &curenv->env_tf;
    }

    // Record that tf is the last real trapframe so
    // print_trapframe can print some additional information.
    last_tf = tf;

    // Dispatch based on what type of trap occurred
    trap_dispatch(tf);

    // If we made it to this point, then no other environment was
    // scheduled, so we should return to the current environment
    // if doing so makes sense.
    if (curenv && curenv->env_status == ENV_RUNNING)
        env_run(curenv);
    else
        sched_yield();
}


extern void _pgfault_upcall(void);

void
page_fault_handler(struct Trapframe *tf)
{
    uint32_t fault_va;

    // Read processor's CR2 register to find the faulting address
    fault_va = rcr2();

    // Handle kernel-mode page faults.

    // LAB 3: Your code here.
    if((tf->tf_cs & 3) != 3) {
        print_trapframe(tf);
        panic("kernel page fault va %08x ip %08x env %x\n",
                      fault_va, tf->tf_eip, curenv?curenv->env_id:0);
    }

    // We've already handled kernel-mode exceptions, so if we get here,
    // the page fault happened in user mode.

    // Call the environment's page fault upcall, if one exists.  Set up a
    // page fault stack frame on the user exception stack (below
    // UXSTACKTOP), then branch to curenv->env_pgfault_upcall.
    //
    // The page fault upcall might cause another page fault, in which case
    // we branch to the page fault upcall recursively, pushing another
    // page fault stack frame on top of the user exception stack.
    //
    // The trap handler needs one word of scratch space at the top of the
    // trap-time stack in order to return.  In the non-recursive case, we
    // don't have to worry about this because the top of the regular user
    // stack is free.  In the recursive case, this means we have to leave
    // an extra word between the current top of the exception stack and
    // the new stack frame because the exception stack _is_ the trap-time
    // stack.
    //
    // If there's no page fault upcall, the environment didn't allocate a
    // page for its exception stack or can't write to it, or the exception
    // stack overflows, then destroy the environment that caused the fault.
    // Note that the grade script assumes you will first check for the page
    // fault upcall and print the "user fault va" message below if there is
    // none.  The remaining three checks can be combined into a single test.
    //
    // Hints:
    //   user_mem_assert() and env_run() are useful here.
    //   To change what the user environment runs, modify 'curenv->env_tf'
    //   (the 'tf' variable points at 'curenv->env_tf').

    // LAB 4: Your code here.
    else if(curenv && curenv->env_pgfault_upcall) {
        //cprintf("[page_fault_handler] User fault with upcall...\n"); 
        //print_trapframe(tf);
        //cprintf("[page_fault_handler] fault VA was %08x\n", fault_va);

        struct UTrapframe *utf;
        char*  raw_addr;

        if((UXSTACKTOP >= tf->tf_esp) && (UXSTACKTOP-PGSIZE <  tf->tf_esp)) {
            KT_DEBUG("recursive fault, adding an exception stack frame\n");
            raw_addr = (char*) tf->tf_esp - 4;
        } else {
            raw_addr = (char*) UXSTACKTOP - 1;
        }

        user_mem_assert(curenv, (void *) raw_addr - sizeof(struct UTrapframe) - 8, 
                        sizeof(struct UTrapframe) + 8, PTE_U | PTE_W | PTE_P);
        raw_addr -= sizeof(struct UTrapframe);

        utf = (struct UTrapframe*) raw_addr;

        utf->utf_fault_va = fault_va;
        utf->utf_err      = T_PGFLT;
        utf->utf_regs     = tf->tf_regs;
        utf->utf_eip      = tf->tf_eip;
        utf->utf_eflags   = tf->tf_eflags;
        utf->utf_esp      = tf->tf_esp;

        tf->tf_esp = (unsigned) raw_addr;
        tf->tf_eip = (unsigned) curenv->env_pgfault_upcall;

        //cprintf("[page_fault_handler] entering user recovery environment...\n"); 

        env_run(curenv);

    } else {
        KT_DEBUG("User fault with __NO__ upcall...\n"); 
        // Destroy the environment that caused the fault.
        KT_DEBUG("user fault va %08x ip %08x\n", fault_va, tf->tf_eip);
        print_trapframe(tf);
        env_destroy(curenv);
    }
}
