#define DEBUG(...) \
do {                                        \
    sys_env_disable_preempt();              \
    cprintf("[%08x %-16s:%3d in %-24s] ",   \
            sys_getenvid(),                 \
            __FILE__,                       \
            __LINE__,                       \
            __func__);                      \
    cprintf(__VA_ARGS__);                   \
    cprintf("\n");                          \
    sys_env_enable_preempt();               \
} while(0);

#define KDEBUG(...) \
do {                                        \
    cprintf("[%08x %-16s:%3d in %-24s] ",   \
            (curenv ? curenv->env_id:0),    \
            __FILE__,                       \
            __LINE__,                       \
            __func__);                      \
    cprintf(__VA_ARGS__);                   \
    cprintf("\n");                          \
} while(0);
