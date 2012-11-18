#define DEBUG(...) \
do {                                        \
    cprintf("[%08x %-10s:%3d in %-16s] ",   \
            sys_getenvid(),                 \
            __FILE__,                       \
            __LINE__,                       \
            __func__);                      \
    cprintf(__VA_ARGS__);                   \
    cprintf("\n");                          \
} while(0);
