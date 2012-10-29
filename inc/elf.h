#ifndef JOS_INC_ELF_H
#define JOS_INC_ELF_H

#define ELF_MAGIC 0x464C457FU	/* "\x7FELF" in little endian */

struct Elf {
	uint32_t e_magic;       // Equal to the ELF_MAGIC constant or an invalid header
	uint8_t  e_elf[12];     // the hell is this?
	uint16_t e_type;        // 0) none 1) relocatable 2) executable 3) shared .obj 4) core
	uint16_t e_machine;     // 0) None 1) AT&T WE32100 2) SPARC 3) i86 4) Motorola 68k 5) Motorola 88k
	uint32_t e_version;     // 0) out of date 1) current
	uint32_t e_entry;       // start EIP
	uint32_t e_phoff;       // program header offset from file start
	uint32_t e_shoff;       // section header offset from file start
	uint32_t e_flags;       // "processor flags"
	uint16_t e_ehsize;      // the header's size in bytes from the start of the file
	uint16_t e_phentsize;   // header table entry size
	uint16_t e_phnum;       // number of header table entries
	uint16_t e_shentsize;   // section header's size
	uint16_t e_shnum;       // number of section header entries
	uint16_t e_shstrndx;    // index of the section header which contains strings
};

struct Proghdr {
	uint32_t p_type;        // 0) NULL, ignored *) loaded to mem for now
	uint32_t p_offset;      // offset from start of file where data resides
	uint32_t p_va;          // start virtual address targeted for loading
	uint32_t p_pa;          // start physical address targeted for loading
	uint32_t p_filesz;      // 
	uint32_t p_memsz;
	uint32_t p_flags;
	uint32_t p_align;
};

struct Secthdr {
	uint32_t sh_name;
	uint32_t sh_type;
	uint32_t sh_flags;
	uint32_t sh_addr;
	uint32_t sh_offset;
	uint32_t sh_size;
	uint32_t sh_link;
	uint32_t sh_info;
	uint32_t sh_addralign;
	uint32_t sh_entsize;
};

// Values for Proghdr::p_type
#define ELF_PROG_LOAD		1

// Flag bits for Proghdr::p_flags
#define ELF_PROG_FLAG_EXEC	1
#define ELF_PROG_FLAG_WRITE	2
#define ELF_PROG_FLAG_READ	4

// Values for Secthdr::sh_type
#define ELF_SHT_NULL		0
#define ELF_SHT_PROGBITS	1
#define ELF_SHT_SYMTAB		2
#define ELF_SHT_STRTAB		3

// Values for Secthdr::sh_name
#define ELF_SHN_UNDEF		0

#endif /* !JOS_INC_ELF_H */
