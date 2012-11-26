#ifndef JOS_DEBUG
#define JOS_DEBUG

#define COLOR_RESET   "\e[00m"
#define COLOR_BLUE    "\e[0;34m"
#define COLOR_GREEN   "\e[0;32m"
#define COLOR_CYAN    "\e[0;36m"
#define COLOR_RED     "\e[0;31m"
#define COLOR_PURPLE  "\e[0;35m"
#define COLOR_BROWN   "\e[0;33m"
#define COLOR_YELLOW  "\e[1;33m"
#define COLOR_WHITE   "\e[1;37m"

#ifndef JOS_KERNEL

#define CDEBUG(color, ...)                    \
do {                                          \
    cprintf("[%s%08x %-16s:%3d in %-24s%s] ", \
            (color),                          \
            sys_getenvid(),                   \
            __FILE__,                         \
            __LINE__,                         \
            __func__,                         \
            (COLOR_RESET));                   \
    cprintf(__VA_ARGS__);                     \
    cprintf("\n");                            \
} while(0);

#else

#define CDEBUG(color, ...)                    \
do {                                          \
    cprintf("[%s%08x %-16s:%3d in %-24s%s] ", \
            (color),                          \
            (curenv ? curenv->env_id:0),      \
            __FILE__,                         \
            __LINE__,                         \
            __func__,                         \
            (COLOR_RESET));                   \
    cprintf(__VA_ARGS__);                     \
    cprintf("\n");                            \
} while(0);
#endif

#define FS_DEBUG(...) CDEBUG(COLOR_CYAN,   __VA_ARGS__)
#define K_DEBUG(...)  CDEBUG(COLOR_PURPLE, __VA_ARGS__)
#define DEBUG(...)    CDEBUG(COLOR_RESET,  __VA_ARGS__)
#endif
