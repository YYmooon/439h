##Reid McKenzie - RDM2755 - Lab 2 Writeup
###Question 1
Okay, eval the code

    mystery_t x;
    char* value = return_a_pointer();
    *value = 10;
    x = (mystery_t) value;

As the OS is executed in virtual memory mode, all pointer references and 
derefferences are executed in and via virtual memory. Consequently, any value 
which the OS assigns to by dereference (here the char* with the symbol "value") 
must exist in the OS's virtual memory system and cannot be hardware addresses. 
Thus the type mystery_t must be uintptr_t or T*, it doesn't matter which as the 
memory address would be the same and a dereference would behave the same 
(without respect to the type system).

###Question 2
According to the "print mapped regions" function I added to the Kernel's list of 
commands, the virtual address range [0, 315] is fully mapped to the hardware 
address range [0x000A0000, 0x0013B00]. The page range [958, 1023] is mapped to  
[0x003BE000, 0x003FF000] , with the pages [316, 957] unmapped by this 
accounting.

###Question 3
While the kernel is mapped into the same address space as "user processes" 
(although those don't exist yet), the permission flags in the page table provide 
access control to the memory which the kernel itself inhabits. As the hardware 
respects the permission flags set in the software page table, unless the 
approporiate flags are set the hardware will deny code not running in supervisor 
mode read and write access as appropriate.

###Question 4
Maximum mapped memory? Depends on the size of a page and the structure of the 
page table which hardware supports. Using the "traditional" x86 32-bitmachine,
the maximum addressable memory quantity is 2^32b, or 4Gb. In order to increase 
the ammount of mapped memory beyond 4Gb, a machine with a larger address length
(say 40 or 64 bit) is required. Changing the page table structure cannot alter 
this limit.

Now, the size of a page can be increased, and the x86 architecture as of the 
first Pentium class chip supports this behavior. Page Size Extension as this 
mode is known causes the processor to use pages of 2M size instead of the 
traditional 4K, and the sun SPARC goes full retard allowing for pages of sizes 
8, 64, 4096, 262144 and 2097152Kb.

###Question 5
In the case of having all 4Gb of hardware memory fully mapped, 2^10 page tables 
are required, and one page directory. As on x86 the page directory is of the 
same size as the page table, this gives a total of ((2^10)+1)*(2^10) or 
1049600 entries at 64b each, gives a little more than 64Mb of storage as being 
required to fully map the 32-bit address space under the Intel two-level page 
table scheme.

###Questi 6
After setting the cr0 paged mode bit, the miracles of instruction pipelining 
give us exactly one instruction from the current unpaged state with which to 
brace ourselves for the arrival of paged memory by jumping to a paged memory 
address with the code we wish to begin executing.

If the next instruction was __not__ a jump into a paged address range, then when 
the processor fetches the following instruction, it will instantly tripple fault  
as the targeted address region is not mapped __in the page table which we use__. 
It is possible to create a page table equivalent to the hardware memory 
addresses, but that's not how JOS's loading step works.
