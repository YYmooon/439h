/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/syscall.h>
#include <kern/console.h>
#include <kern/sched.h>
#include <kern/time.h>
#include <kern/e1000.h>

#include <debug.h>

#define ZERO_CALL_SUPPORT(x)                            \
do {                                                    \
    if(x == 0) {                                        \
        x = curenv->env_id;                             \
    }                                                   \
} while(0);

// Print a string to the system console.
// The string is exactly 'len' characters long.
// Destroys the environment on memory errors.
static void
sys_cputs(const char *s, size_t len)
{
    // Check that the user has permission to read memory [s, s+len).
    // Destroy the environment if not.

    // LAB 3: Your code here.
    user_mem_assert(curenv, s, len, PTE_U);
    // Print the string supplied by the user.
    cprintf("%.*s", len, s);
}

// Read a character from the system console without blocking.
// Returns the character, or 0 if there is no input waiting.
static int
sys_cgetc(void)
{
    return cons_getc();
}

// Returns the current environment's envid.
static envid_t
sys_getenvid(void)
{
    return curenv->env_id;
}

// Destroy a given environment (possibly the currently running environment).
//
// Returns 0 on success, < 0 on error.  Errors are:
//  -E_BAD_ENV if environment envid doesn't currently exist,
//      or the caller doesn't have permission to change envid.
static int
sys_env_destroy(envid_t envid)
{
    ZERO_CALL_SUPPORT(envid);
    int r;
    struct Env *e;

    if ((r = envid2env(envid, &e, 1)) < 0)
        return r;
    if (e == curenv)
        cprintf("[%08x] exiting gracefully\n", curenv->env_id);
    else
        cprintf("[%08x] destroying %08x\n", curenv->env_id, e->env_id);
    env_destroy(e);
    return 0;
}

// Deschedule current environment and pick a different one to run.
static void
sys_yield(void)
{
    if(cpunum() >= NCPU)
      K_DEBUG("yielding\n");
    sched_yield();
}

// Allocate a new environment.
// Returns envid of new environment, or < 0 on error.  Errors are:
//  -E_NO_FREE_ENV if no free environment is available.
//  -E_NO_MEM on memory exhaustion.
static envid_t
sys_exofork(void)
{
    // Create the new environment with env_alloc(), from kern/env.c.
    // It should be left as env_alloc created it, except that
    // status is set to ENV_NOT_RUNNABLE, and the register set is copied
    // from the current environment -- but tweaked so sys_exofork
    // will appear to return 0.

    // LAB 4: Your code here.
    struct Env* e;
    unsigned res = env_alloc(&e, curenv->env_id);
    if(res < 0) {
        if(res == -E_NO_FREE_ENV) {
            K_DEBUG("no free envs!\n");
        } if(res == -E_NO_MEM) {
            K_DEBUG("no free mem!\n");
        } return res; // -E_NO_FREE_ENV, -E_NO_MEM
    }
    e->env_status = ENV_NOT_RUNNABLE;
    e->env_type   = ENV_TYPE_USER;
    e->env_tf = curenv->env_tf;                 // copy register state
    K_DEBUG("child epi %08x\n",
            e->env_tf.tf_eip);
    e->env_tf.tf_regs.reg_eax = 0;              // set child return code
    return e->env_id;                           // return the child's env. id
}

// Set envid's env_status to status, which must be ENV_RUNNABLE
// or ENV_NOT_RUNNABLE.
//
// Returns 0 on success, < 0 on error.  Errors are:
//  -E_BAD_ENV if environment envid doesn't currently exist,
//      or the caller doesn't have permission to change envid.
//  -E_INVAL if status is not a valid status for an environment.
static int
sys_env_set_status(envid_t envid, int status)
{
    ZERO_CALL_SUPPORT(envid);
    // Hint: Use the 'envid2env' function from kern/env.c to translate an
    // envid to a struct Env.
    // You should set envid2env's third argument to 1, which will
    // check whether the current environment has permission to set
    // envid's status.

    // LAB 4: Your code here.
    struct Env* e;
    int res = envid2env(envid, &e, 1);
    if(res < 0) return res;             // -E_BAD_ENV

    if(status == ENV_FREE) {
        // a user should not be able to mark an environment as free,
        // that should be done by syscalling sys_env_destroy.
        K_DEBUG("ERROR: cannot free an env this way\n");
        return -E_INVAL;
    }

    if(e->env_type == ENV_TYPE_IDLE) {
        if((status == ENV_DYING) ||
           (status == ENV_RUNNABLE)) {
            // This code prevents users from setting idle environments'
            // status flags to the following values:
            //  - ENV_DYING     a user shouldn't be able to kill a kernel env
            //  - ENV_RUINNABLE idle envs can only be entered by the kernel
            K_DEBUG("ERROR: cannot set status of an idle env\n");
            return -E_INVAL;
        }
    } 
    if(e->env_type == ENV_TYPE_USER) {
        if((status == ENV_DYING)        ||
           (status == ENV_RUNNABLE)     ||
           (status == ENV_RUNNING)      ||
           (status == ENV_NOT_RUNNABLE)) {
            // if the status is legal...
            e->env_status = status;
            return 0;
        } else {
            // don't let envs enter states which aren't valid states
            // according to the env states enum
            K_DEBUG("unknown status %08x\n", status);
            return -E_INVAL;
        }
    } 
    K_DEBUG("ERROR: fell throug!h\n");
    return -E_INVAL;
}

// Set envid's trap frame to 'tf'.
// tf is modified to make sure that user environments always run at code
// protection level 3 (CPL 3) with interrupts enabled.
//
// Returns 0 on success, < 0 on error.  Errors are:
//  -E_BAD_ENV if environment envid doesn't currently exist,
//      or the caller doesn't have permission to change envid.
static int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf)
{
    // LAB 5: Your code here.
    // Remember to check whether the user has supplied us with a good
    // address!
    
    ZERO_CALL_SUPPORT(envid); // you need this for exec()

    struct Env* t_e = &envs[ENVX(envid)];
    if(t_e->env_status == ENV_FREE)
      return -E_BAD_ENV;
    
    if((curenv->env_id != t_e->env_parent_id) && 
       (curenv->env_id != t_e->env_id))
      return -E_BAD_ENV;
    
    tf->tf_cs |= 3;
    memcpy(&t_e->env_tf, tf, sizeof(struct Trapframe));

    return 0;
}

// Set the page fault upcall for 'envid' by modifying the corresponding struct
// Env's 'env_pgfault_upcall' field.  When 'envid' causes a page fault, the
// kernel will push a fault record onto the exception stack, then branch to
// 'func'.
//
// Returns 0 on success, < 0 on error.  Errors are:
//  -E_BAD_ENV if environment envid doesn't currently exist,
//      or the caller doesn't have permission to change envid.
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
    ZERO_CALL_SUPPORT(envid);
    // LAB 4: Your code here.
    struct Env* e;
    int res = envid2env(envid, &e, 1);
    if(res < 0) return res; // permission faults...

    user_mem_assert(curenv, func, PGSIZE, PTE_U | PTE_P);
    // okay fine do it
    e->env_pgfault_upcall = func;
    return 0;
}

// allow the process to escape N clock preemptions
// used by the debug printing code to escape preemption
// while printing debugging info.
// May also be used by the filesystem server to ensure that it is
// never preempted.
int
sys_env_escape_preempt(unsigned times)
{
   if(curenv->env_escape_preempt && curenv->env_escape_preempt <= times)
      KDEBUG("WARNING: environment %08x is disabling preemption while preemption protected",
             curenv->env_id);

    curenv->env_escape_preempt = times;
    return 0;
}

int
sys_env_recovered()
{
    assert(curenv);
    curenv->env_fault_count = 0;
    return 0;
}

// Allocate a page of memory and map it at 'va' with permission
// 'perm' in the address space of 'envid'.
// The page's contents are set to 0.
// If a page is already mapped at 'va', that page is unmapped as a
// side effect.
//
// perm -- PTE_U | PTE_P must be set, PTE_AVAIL | PTE_W may or may not be set,
//         but no other bits may be set.  See PTE_SYSCALL in inc/mmu.h.
//
// Return 0 on success, < 0 on error.  Errors are:
//  -E_BAD_ENV if environment envid doesn't currently exist,
//      or the caller doesn't have permission to change envid.
//  -E_INVAL if va >= UTOP, or va is not page-aligned.
//  -E_INVAL if perm is inappropriate (see above).
//  -E_NO_MEM if there's no memory to allocate the new page,
//      or to allocate any necessary page tables.
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
    // Hint: This function is a wrapper around page_alloc() and
    //   page_insert() from kern/pmap.c.
    //   Most of the new code you write should be to check the
    //   parameters for correctness.
    //   If page_insert() fails, remember to free the page you
    //   allocated!

    // LAB 4: Your code here.
    ZERO_CALL_SUPPORT(envid);
    struct Env* target;
    struct Page* p;

    int res = envid2env(envid, &target, 1); 
    if(res < 0) return res; // permissions error case

    int perm_check = (perm ^ (PTE_AVAIL | PTE_W)) & ~(PTE_W | PTE_AVAIL | PTE_U | PTE_P);
    if(perm_check) {
        K_DEBUG("ERROR: the permission bits are off\n");
        K_DEBUG("argument: %08x delta: %08x legal perms: %08x\n",
                perm, perm_check, PTE_AVAIL | PTE_W | PTE_P | PTE_U);
        // the permission bits are wrong..
        return -E_INVAL;
    }

    if(((unsigned) va % PGSIZE) != 0) {
        // the VA is not page-aligned
        K_DEBUG("ERROR: the VA is not page alligned\n");
        return -E_INVAL;
    }

    if((unsigned) va >= UTOP) {
        // the VA is above UTOP
        K_DEBUG("ERROR: the VA is out of user space\n");
        return -E_INVAL;
    }

    if((p = page_alloc(ALLOC_ZERO))) {
        // nonzero return value, all is well so far
        int i = page_insert(target->env_pgdir, p, va, PTE_P | PTE_U | perm);
        if(i == 0) {
            // all is well
            K_DEBUG("insert returned %d, all well\n", i);
            return 0;
        } else {
            page_free(p);
            K_DEBUG("ERROR: page_insert returned %d\n", i);
            return i;
        }
    } else {
        // no page was allocated, die
        K_DEBUG("ERROR: no free memory\n");
        return -E_NO_MEM;
    }
}

// Map the page of memory at 'srcva' in srcenvid's address space
// at 'dstva' in dstenvid's address space with permission 'perm'.
// Perm has the same restrictions as in sys_page_alloc, except
// that it also must not grant write access to a read-only
// page.
//
// Return 0 on success, < 0 on error.  Errors are:
//  -E_BAD_ENV if srcenvid and/or dstenvid doesn't currently exist,
//      or the caller doesn't have permission to change one of them.
//  -E_INVAL if srcva >= UTOP or srcva is not page-aligned,
//      or dstva >= UTOP or dstva is not page-aligned.
//  -E_INVAL is srcva is not mapped in srcenvid's address space.
//  -E_INVAL if perm is inappropriate (see sys_page_alloc).
//  -E_INVAL if (perm & PTE_W), but srcva is read-only in srcenvid's
//      address space.
//  -E_NO_MEM if there's no memory to allocate any necessary page tables.
static int
sys_page_map(envid_t srcenvid, void *srcva, 
             envid_t dstenvid, void *dstva, 
             int perm)
{
    ZERO_CALL_SUPPORT(srcenvid);
    ZERO_CALL_SUPPORT(dstenvid);
    // Hint: This function is a wrapper around page_lookup() and
    //   page_insert() from kern/pmap.c.
    //   Again, most of the new code you write should be to check the
    //   parameters for correctness.
    //   Use the third argument to page_lookup() to
    //   check the current permissions on the page.

    // LAB 4: Your code here.
    int res;
    struct Env * src, * dst;
    res =  envid2env(srcenvid, &src, 1);
    if(res) return res;
    res = envid2env(dstenvid, &dst, 1);
    if(res) return res;

    if(((uint32_t) srcva >= UTOP || PGOFF(srcva)) || 
       ((uint32_t) dstva >= UTOP || PGOFF(dstva))) {
        // not page alligned or out of legal range
        return -E_INVAL;
    }

    int perm_check = (perm ^ (PTE_AVAIL | PTE_W)) & 
                     ~(PTE_W | PTE_AVAIL | PTE_U | PTE_P);
    if(perm_check) {
        // the permission bits are wrong..
        // will only catch perm bits that should never be set
        K_DEBUG("Permission error\n");
        return -E_INVAL;
    }

    pte_t *pte;
    struct Page *page = page_lookup(src->env_pgdir, srcva, &pte);
    if(!page) {
        return -E_INVAL;
    }

    return page_insert(dst->env_pgdir, page, dstva, PTE_P | PTE_U | perm);

}

// Unmap the page of memory at 'va' in the address space of 'envid'.
// If no page is mapped, the function silently succeeds.
//
// Return 0 on success, < 0 on error.  Errors are:
//  -E_BAD_ENV if environment envid doesn't currently exist,
//      or the caller doesn't have permission to change envid.
//  -E_INVAL if va >= UTOP, or va is not page-aligned.
static int
sys_page_unmap(envid_t envid, void *va)
{
    ZERO_CALL_SUPPORT(envid);
    // Hint: This function is a wrapper around page_remove().
    // LAB 4: Your code here.
    struct Env *env;
    int res;
    res = envid2env(envid, &env, 1);

    if(res < 0)
        // -E_BAD_ENV
        return res;

    if((unsigned) va >= UTOP || PGOFF(va))
        // not page alligned
        return -E_INVAL;

    page_remove(env->env_pgdir, va);
    return 0;
}

// Try to send 'value' to the target env 'envid'.
// If srcva < UTOP, then also send page currently mapped at 'srcva',
// so that receiver gets a duplicate mapping of the same page.
//
// The send fails with a return value of -E_IPC_NOT_RECV if the
// target is not blocked, waiting for an IPC.
//
// The send also can fail for the other reasons listed below.
//
// Otherwise, the send succeeds, and the target's ipc fields are
// updated as follows:
//    env_ipc_recving is set to 0 to block future sends;
//    env_ipc_from is set to the sending envid;
//    env_ipc_value is set to the 'value' parameter;
//    env_ipc_perm is set to 'perm' if a page was transferred, 0 otherwise.
// The target environment is marked runnable again, returning 0
// from the paused sys_ipc_recv system call.  (Hint: does the
// sys_ipc_recv function ever actually return?)
//
// If the sender wants to send a page but the receiver isn't asking for one,
// then no page mapping is transferred, but no error occurs.
// The ipc only happens when no errors occur.
//
// Returns 0 on success, < 0 on error.
// Errors are:
//  -E_BAD_ENV if environment envid doesn't currently exist.
//      (No need to check permissions.)
//  -E_IPC_NOT_RECV if envid is not currently blocked in sys_ipc_recv,
//      or another environment managed to send first.
//  -E_INVAL if srcva < UTOP but srcva is not page-aligned.
//  -E_INVAL if srcva < UTOP and perm is inappropriate
//      (see sys_page_alloc).
//  -E_INVAL if srcva < UTOP but srcva is not mapped in the caller's
//      address space.
//  -E_INVAL if (perm & PTE_W), but srcva is read-only in the
//      current environment's address space.
//  -E_NO_MEM if there's not enough memory to map srcva in envid's
//      address space.
static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
    ZERO_CALL_SUPPORT(envid);
    assert(envid);
    assert(curenv);

    // LAB 4: Your code here.
    struct Env * env;
    struct Page * page;
    pte_t * pte;

    if(envid2env(envid, &env, 0) < 0)
        return -E_BAD_ENV;

    if(env->env_ipc_recving == 0)
        return -E_IPC_NOT_RECV;

    if(srcva && (uintptr_t) srcva < UTOP){
        if((uintptr_t) srcva % PGSIZE)
            return -E_INVAL;

        if(!(perm & PTE_P) || !(perm & PTE_U))
            return -E_INVAL;

        if((perm & 0xfff) & ~(PTE_AVAIL | PTE_P | PTE_W | PTE_U))
            return -E_INVAL;
    }

    if(srcva && env->env_ipc_dstva && ((uintptr_t) srcva < UTOP)){
        if((page = page_lookup(curenv->env_pgdir, srcva, &pte)) == NULL)
            return -E_INVAL;

        if((perm & PTE_W) && !(*pte & PTE_W))
            panic("Are you sure you want to mapping a read-only page to a status that can be written?");

        int result = page_insert(env->env_pgdir, page, env->env_ipc_dstva, perm);
        if(result < 0)
            return result;

        env->env_ipc_perm = perm;
    }
    else {
        env->env_ipc_perm = 0;
    }

    env->env_ipc_recving  = 0;
    env->env_ipc_from     = sys_getenvid();
    env->env_ipc_value    = value;
    env->env_status       = ENV_RUNNABLE;

    KDEBUG("\e[0;31m%08x unblocked\e[0;00m\n", env->env_id);

    return 0;
}

// Block until a value is ready.  Record that you want to receive
// using the env_ipc_recving and env_ipc_dstva fields of struct Env,
// mark yourself not runnable, and then give up the CPU.
//
// If 'dstva' is < UTOP, then you are willing to receive a page of data.
// 'dstva' is the virtual address at which the sent page should be mapped.
//
// This function only returns on error, but the system call will eventually
// return 0 on success.
// Return < 0 on error.  Errors are:
//  -E_INVAL if dstva < UTOP but dstva is not page-aligned.
static int
sys_ipc_recv(void *dstva)
{
    // LAB 4: Your code here.
    if((uintptr_t) dstva < UTOP && ((uintptr_t) dstva % PGSIZE)) {
        return -E_INVAL;
    }

    curenv->env_ipc_value   = 0;
    curenv->env_ipc_from    = 0;
    curenv->env_ipc_perm    = 0;
    curenv->env_ipc_recving = 1;
    curenv->env_ipc_dstva   = dstva;
    curenv->env_status      = ENV_NOT_RUNNABLE;

    KDEBUG("\e[0;31m%08x blocked\e[0;00m\n", curenv->env_id);

    return 0;
}

// Return the current time.
static int
sys_time_msec(void)
{
    // LAB 6: Your code here.
    return time_msec();
}

// Takes a user buffer start address and a size assumed to be a valid packet,
// and takes the appropriate steps to transmit it over the network.
//
// [Returns]
//     -E_INVAL if the buffer would cause a page falt,
//     -E_INVAL if the descriptor is zero,
//     -E_INVAL if the buffer is larger than hardware can send,
//     -E_UNSPECIFIED if the driver could not queue the packet up,
//     0 if the packet was sent successfully.
//
// [Note]
//    This code does _not_ duplicate the data buffer passed, it assumes that
//    the buffer is static with respect to the duration of this system call.
//    To hep make this assumption easier with, this function will backend two
//    system calls one of which blocks silently until the packet is transmitted
//    the other of which will return once the packet has been queued and relies
//    upon the user to ensure the consistency of the data specified.
static int
sys_net_send(void* buffer, unsigned buffsize)
{
    if(!buffer) return -E_INVAL;
    if(!buffsize) return -E_INVAL;

    pte_t *pte;
    struct Page *page;
   
    if(!page_lookup(curenv->env_pgdir, buffer, &pte))
        return -E_INVAL;

    if(!page_lookup(curenv->env_pgdir, buffer+buffsize, &pte))
        return -E_INVAL;

    return pci_e1000_tx(buffer, buffsize);
}

// Does the same thing in reverse, slurping the first packet off of the wire and
// dumping it to the user provided buffer. Same error cases as above, again to
// guard against a possible segfault in the kernel.
static int
sys_net_recieve(void* buffer, unsigned buffsize)
{
    if(!buffer) return -E_INVAL;
    if(!buffsize) return -E_INVAL;

    pte_t *pte;
    struct Page *page;
   
    if(!page_lookup(curenv->env_pgdir, buffer, &pte))
        return -E_INVAL;

    if(!page_lookup(curenv->env_pgdir, buffer+buffsize, &pte))
        return -E_INVAL;

    return pci_e1000_rx(buffer, buffsize);
}

// Dispatches to the correct kernel function, passing the arguments.
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
    // Call the function corresponding to the 'syscallno' parameter.
    // Return any appropriate return value.
    // LAB 3: Your code here.
    switch(syscallno) {
        case SYS_cputs:
            sys_cputs((const char *) a1, (size_t) a2);
            return 0;

        case SYS_cgetc:
            return sys_cgetc();

        case SYS_getenvid:
            return sys_getenvid();

        case SYS_env_destroy:
            return sys_env_destroy((envid_t) a1);

        case SYS_page_alloc:
            return sys_page_alloc((envid_t) a1, (void*) a2, (int) a3);

        case SYS_page_map:
            return sys_page_map((envid_t)   a1, 
                                (void*)     a2, 
                                (envid_t)   a3, 
                                (void*)     a4, 
                                (int)       a5);
        case SYS_page_unmap:
            return sys_page_unmap((envid_t) a1, 
                                  (void*)   a2);

        case SYS_exofork:
            return sys_exofork();

        case SYS_env_set_status:
            return sys_env_set_status((envid_t) a1, 
                                      (int)     a2);

        case SYS_env_set_trapframe:
            return sys_env_set_trapframe((envid_t)  a1,
                                         (struct Trapframe*) a2);

        case SYS_env_set_pgfault_upcall:
            return sys_env_set_pgfault_upcall((envid_t) a1, 
                                              (void*)   a2);

        case SYS_env_escape_preempt:
            return sys_env_escape_preempt((unsigned) a1);

        case SYS_yield:
            sys_yield();
            return 0; // unreachable but keep the compiler happy

        case SYS_ipc_try_send:
            return sys_ipc_try_send((envid_t)   a1,
                                    (uint32_t)  a2, 
                                    (void*)     a3, 
                                    (unsigned)  a4);
            
        case SYS_ipc_recv:
            return sys_ipc_recv((void*) a1);

        case SYS_env_recovered:
            return sys_env_recovered();

        case SYS_time_msec:
            return sys_time_msec();

        case SYS_net_send:
            return sys_net_send((void*)    a1, 
                                (uint32_t) a2);

        case SYS_net_recieve:
            return sys_net_recieve((void*)    a1, 
                                   (uint32_t) a2);

        default:
            return -E_INVAL;
    }
}

