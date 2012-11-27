#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>
#include <kern/spinlock.h>
#include <debug.h>

// Choose a user environment to run and run it.
void
sched_yield(void)
{
    if(!holding(&kernel_lock))
        lock_kernel();

    struct Env *idle;
    int i;

    // Implement simple round-robin scheduling.
    //
    // Search through 'envs' for an ENV_RUNNABLE environment in
    // circular fashion starting just after the env this CPU was
    // last running.  Switch to the first such environment found.
    //
    // If no envs are runnable, but the environment previously
    // running on this CPU is still ENV_RUNNING, it's okay to
    // choose that environment.
    //
    // Never choose an environment that's currently running on
    // another CPU (env_status == ENV_RUNNING) and never choose an
    // idle environment (env_type == ENV_TYPE_IDLE).  If there are
    // no runnable environments, simply drop through to the code
    // below to switch to this CPU's idle environment.

    // LAB 4: Your code here.
    int c = (curenv ? curenv->env_id : 0) % NENV;
    i = (c + 1) % NENV;

    if(curenv && curenv->env_escape_preempt > 0) {
        if(curenv->env_escape_preempt != 0xBAD1DEA)
            curenv->env_escape_preempt--;

        env_run(curenv);
    }

    while(i != c) {
        if ((envs[i].env_type != ENV_TYPE_IDLE) &&
            (envs[i].env_status == ENV_RUNNABLE)) {
            cprintf("env %08x launching env %08x\n", c, envs[i].env_id);
            env_run(&envs[i]);
        } else { 
            i = (i + 1) % NENV;
        }
    }
    
    if(curenv->env_status == ENV_RUNNING && curenv->env_type != ENV_TYPE_IDLE) {
        env_run(curenv);
    } else if (cpunum() == 0) {
        K_DEBUG("No more runnable environments!");
        while (1)
            monitor(NULL);
    } else {
        // Run this CPU's idle environment when nothing else is runnable.
        idle = &envs[cpunum()];
        if (!(idle->env_status == ENV_RUNNABLE || idle->env_status == ENV_RUNNING))
            panic("CPU %d: No idle environment!", cpunum());
        env_run(idle);
    }
}
