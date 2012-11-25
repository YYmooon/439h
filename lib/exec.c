#include <inc/lib.h>
#include <inc/elf.h>
#include "spawn.c"

// This is really the spawn() command hacked so that 
// it skips the exofork() step and effects the current
// environment instead of creating a new one.
int
exec(const char *prog, const char **argv)
{
    unsigned char elf_buf[512];
    struct Trapframe child_tf;
    envid_t child;

    int fd, i, r;
    struct Elf *elf;
    struct Proghdr *ph;
    int perm;

    if ((r = open(prog, O_RDONLY)) < 0)
        return r;
    fd = r;

    // Read elf header
    elf = (struct Elf*) elf_buf;
    if (readn(fd, elf_buf, sizeof(elf_buf)) != sizeof(elf_buf)
        || elf->e_magic != ELF_MAGIC) {
        close(fd);
        cprintf("elf magic %08x want %08x\n", elf->e_magic, ELF_MAGIC);
        return -E_NOT_EXEC;
    }

    child = sys_getenvid();

    // Set up trap frame, including initial stack.
    child_tf = envs[ENVX(child)].env_tf;
    child_tf.tf_eip = elf->e_entry;

    if ((r = init_stack(child, argv, &child_tf.tf_esp)) < 0)
        return r;

    // Set up program segments as defined in ELF header.
    ph = (struct Proghdr*) (elf_buf + elf->e_phoff);
    for (i = 0; i < elf->e_phnum; i++, ph++) {
        if (ph->p_type != ELF_PROG_LOAD)
            continue;
        perm = PTE_P | PTE_U;
        if (ph->p_flags & ELF_PROG_FLAG_WRITE)
            perm |= PTE_W;
        if ((r = map_segment(child, ph->p_va, ph->p_memsz,
                     fd, ph->p_filesz, ph->p_offset, perm)) < 0)
            goto error;
    }
    close(fd);
    fd = -1;

    if ((r = sys_env_set_trapframe(child, &child_tf)) < 0)
        panic("sys_env_set_trapframe: %e", r);

    if ((r = sys_env_set_status(child, ENV_RUNNABLE)) < 0)
        panic("sys_env_set_status: %e", r);

    return child;

error:
    sys_env_destroy(child);
    close(fd);
    return r;
}

