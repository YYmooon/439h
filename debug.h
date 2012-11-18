#define DEBUG(...) \
do {                                        \
    cprintf("[%08x %-16s:%3d in %-24s] ",   \
            sys_getenvid(),                 \
            __FILE__,                       \
            __LINE__,                       \
            __func__);                      \
    cprintf(__VA_ARGS__);                   \
    cprintf("\n");                          \
} while(0);

#define KDEBUG(...) \
do {                                        \
    cprintf("[%08x %-16s:%3d in %-24s] ",   \
            curenv->env_id,                 \
            __FILE__,                       \
            __LINE__,                       \
            __func__);                      \
    cprintf(__VA_ARGS__);                   \
    cprintf("\n");                          \
} while(0);
