##Reid McKenzie - RDM2755 - HW1

###Question 1 - The 32-bit jump
In evaluating `boot/boot.s` at line 48 the `lgdt` instruction loads a hard-coded
redrglobal descriptor table. The provided GDT doesn't do much, the addresses 
mapping it creates is the same as the hardware address mapping. However, it's 
required to mark the `.code32` segment of the file as executable in protected 
mode. The actual shift from raw to protected mode occurs on line 51 when `cr0`
is set by oring the existing condition codes with the protected mode mask named
`CR0_PE_ON`. 

###Question 2 - Jumping to main.c
The last instruction in `boot/boot.s` to be executed is going to be line 69 
where it "calls" `void bootmain` from main.c with no intent of returning.
Because returning is possible, the `spin` loop exists which causes the computer
to behave nicely and do nothing for the rest of eternity instead of wandering
off into oblivion as it attempts to execute all of main memory.

The last instruction which is defined in the bootloader is executed from main.c
and it is going to be an arbitrary jump from the bootloader to the code which
the bootloader read in from disk.

###Question 3 - Where does the Kernel go?
The bootloader reads the kernel from disk to ram starting at 0x10000 and growing
up towards the top of main memory. This is done in `main.c` on line 32 where the
address 0x10000 is `#define`ed with a cast to `Elf *`. 

###Question 4 - How do it know...
The ELF header is defined at a constant size and start address. The bootloader
reads in the real OS by loading data equal to the size of an ELF header from
disk to a constant memory address, and doing a sanity check that the read data
does constitue a valid ELF header using a magic constant of 0x464C457FU embedded
as the first member of the ELF header struct. If the ELF header checks out using
that magic number, then the loader assumes that the entire ELF structure is 
sane.

The ELF header is followed on disk by another structure known as the Program 
Header (Proghdr) accessed by the bootloader with an offset from the start of the
ELF header. The Proghdr describes where on disk the actual kernel's executable 
code resides, and how large it is. The for loop in `bootmain` uses that 
information to read the sectors indicated by the Proghdr into memory, then jumps
blindly to the entry point specified in the ELF header.

###Question 5 - How does 0x10000 change?
Well... it starts out as garbage (but probably zeroed) data when the BIOS jumps
to the bootloader, then when the OS takes over, that same eight word address 
range is "full" of data. My GDB dump shows the following data after the OS loads

    0x10000:        0x464c457f      0x00010101      0x00000000      0x00000000
    0x10010:        0x00030002      0x00000001      0x0010000c      0x00000034

###Question 6 - What's there at the second breakpoint?
Prior to the second breakpoint, the 

###Question 7 - Position dependance
In `kern/entry.S`, there isn't any position dependant code. What happens is that
the entry code (which uses the existing hardware equivalent page table) sets the
protected mdoe bit and jumps to the C code written in `kern/init.c`. This code 
is assembled by the compiler in position-dependant mode and would fail without 
the mapping established as part of entering protected mode. to be explicit, the
call to `memset` on line 30 of `kern/init.c` presumes the presence of the page
table and will fail if protected mode is not enabled.

###Question 8
`kern/console.c` exports the function `cputchar` which is the raw interface for
writing a character to the terminal. This is the function which `kern/printf.c`
uses to actually put the characters which printf renders to the screen.

###Question 9
This code is resposable for implementing scrolling of the terminal screen. When
the putc function goes to write a character which is beyond the bounds of the
terminal, this logic detects that the character is out of the screen's bounds 
and memcopys the bottom (n-1) lines of the screen up to the top (so overwrites 
the top line shifting everything up) and then continues to write the new 
character to the now empty line at the bottom of the screen.

###Question 10
`fmt` is a pointer to the "format" string, being the first argument of the
`cprintf` call. `ap` is a pointer to the argument list, and the way that this
`cprintf` implementation deals with the fact that it is a varargs function.

`cprintf` loops over the format string, and for each character which isn't "%",
it calls `cons_putc`, being one interface for raw writing to the terminal.
however, every time `cprintf` encounters the "%" character, it goes into giant 
case switch block which serves to determine the next character or series of 
characters which `cprintf` will display based on whether the trailing character 
is a 'valid' format character, or a second percentage sign.

For each format directive which uses an argument, `va_arg` is called to 
retreive the refferenced argument from the varargs "array" and return it for the
format generating code in the switch statement. Note that with each call, 
`va_arg` effectively bumps a cursor variable ensuring that it returns each 
argument only once and in the order that the were passed.

 - `cons_putc` takes as its argument the character code of the char to be 
   printed
 - `vcprintf` takes as its arguments the varargs list and the format string

###Problem 11
This code prints the text "He110 Wrld", with the text "ex" and "rks" encoded in 
the integer literal "57616" and the hex lit "0x00646c72". When the integer 
literal is printed as hex, the data to hex rendering takes care of the 
little-endian to big endian conversion and will print "e110", being the hex of 
that integer in a big-endian system. The hex literal however, when viewed in 
memory, won't be the 0x00646c72 specified in the code, but its little endian 
representation 0x726c6400. This representation, when taken as a seqence of 
characters (which is the '%s' directive), is the sequence {'r' 'l' 'd' '\0'}.

If this code were compiled to a big-endian system, then in order for the 
provided code snippet to print the same text, the hex literal used to generate 
the "e110" would have to be re-ordered by bytes to become 0x726c6400 so that 
when `cprintf` accesses the word by bytes it encounters the same sequence of 
byte quantities. This is required because the compiler doesn't know that you are
counting on the behavior of the big endian (literal) -> little endian conversion
and therefore won't adjust for it when you go big endian (literal) -> big 
endian.

###Problem 12
Each time `cprintf` uses a percent format statement, it blindly uses `va_arg` to
get the next element of data up the stack. In this case, the first thing which 
`va_arg` will fetch is the value 3, as listed in the args however when it hits 
the second formatter (the '%x' following "y=") `va_arg` will yeild the value of 
an element of the stack which the call to `cprintf` did not set. Consequently, 
you will get stack garbage for the "value of y".

###Problem 13
`cprintf` would not have to be midified if this were the case, but the 
implementation of `va_arg` would have to be adjusted. Because the varargs are 
pushed in the reverse order, one cannot simply walk back up the stack to find 
the nth argument knowing that the first argument is at a constant offset from 
the top of the stack.

As a result, the varargs convention would have to be adjusted to include some 
manner of a length field so that the `va_arg` iterator knows how far back up the
stack to start. `va_arg` iteration is then acheived by decrementing the offset 
up the stack at which the next data field is read.

So such a change is possible, but would require that extra data be passed with 
each varargs function call and therefor wouldn't really serve a purpose.

###Problem 14
In `kern/init.S` the OS's init code uses its' data segment to create a range of 
memory addresses between `bootstacktop` and `bootstack`

###Problem 15
At address f01000e8, the ASM for `test_backtrace` subtracts `0x14` from the stack
pointer, thereby making room for five eight byte quantities on the stack. 
However, it has also pushed three full words onto the stack: %ebp and %ebx as 
part of the function header, and implicitly %eip in the `call` instruction which 
passed control to `test_backtrace`, so the total is 8 four byte quantities 
pushed onto the stack per recursion for a total of a 0x20 difference between 
%esp from one level to the next. This 0x20 difference is supported by 
experimentation in GDB.

###Problem 16
"Lab 1 Files" is the commit message for the first git commit on this source 
tree.

###Problem 17
The commit message on my merge was something like "merge branches because". The 
merge ref is 1c9a3c8326d82e4f79cbd52158e44a6246b61ccc.
