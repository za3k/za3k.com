[za3k](/) > drafts > a guide to x86 and x86-64 assembly

Author: za3k
Status: draft in progress

Source list:
- "Beginning X64 Assembly Programming" (on google books). Mostly the "Hello world" program.
- On linux, /usr/include/asm/unistd_32.h and /usr/include/asm/unistd_64.h for the Linux ABI call numbers.
- [A Whirlwind Tutorial on Creating Really Teensy ELF Executables for Linux](https://www.muppetlabs.com/~breadbox/software/tiny/teensy.html): Note, use -sn or -sN with `ld` today to avoid page alignment bloat.
- [Official docs](https://www.nasm.us/xdoc/2.15.05/html/nasmdoc2.html)
- [Interfacing with Linux](https://en.wikibooks.org/wiki/X86_Assembly/Interfacing_with_Linux)
- [x86_64 NASM Cheat Sheet](https://www.cs.uaf.edu/2017/fall/cs301/reference/x86_64.html)
- [x86 Cheat Sheet](https://www.bencode.net/blob/nasmcheatsheet.pdf (x86))

Thanks to muurkha and #swhack on libera for help.

## x86 reference
TODO: Registers, mov
[x86 Cheat Sheet](https://www.bencode.net/blob/nasmcheatsheet.pdf (x86))

## x86-64 reference
TODO: mov

|Name | asm notes       | C ABI     | C ABI | Linux ABI | Linux ABI | 32-bit | 16-bit | 8-bit |
|-----|-----------------|-----------|-------|-----------|-----------|--------|--------|-------|
|rax  | values returned | scratch   | ret   | scratch   | ret       | eax    | ax     | ah al |
|rcx  | can be counter  | scratch   | arg 4 | scratch   |           | ecx    | cx     | ch cl |
|rdx  |                 | scratch   | arg 3 | preserved | arg 3     | edx    | dx     | dh dl |
|rbx  |                 | preserved |       | preserved |           | ebx    | bx     | bh bl |
|rsp  | top of stack    | preserved |       | preserved |           | esp    | sp     | spl   |
|rbp  | (old sp)        | preserved |       | preserved |           | ebp    | bp     | bpl   |
|rsi  |                 | scratch   | arg 2 | preserved | arg 2     | esi    | si     | sil   |
|rdi  |					| scratch   | arg 1 | preserved | arg 1     | edi    | di     | dil   |
|r8   |                 | scratch   | arg 5 | preserved | arg 5     | r8d    | r8w    | r8b   |
|r9   |                 | scratch   | arg 6 | preserved | arg 6     | r9d    | r9w    | r9b   |
|r10  |                 | scratch   |       | preserved | arg 4     | r10d   | r10w   | r10b  |
|r11  |                 | scratch   |       | scratch   |           | r11d   | r11w   | r11b  |
|r12  |                 | preserved |       | preserved |           | r12d   | r12w   | r12b  |
|r13  |                 | preserved |       | preserved |           | r13d   | r13w   | r13b  |
|r14  |                 | preserved |       | preserved |           | r14d   | r14w   | r14b  |
|r15  |                 | preserved |       | preserved |           | r15d   | r15w   | r15b  |

Parts of a 64-bit register

```
[76543210 76543210 76543210 76543210 76543210 76543210 76543210 76543210] bits
[                                  RAX                                  ]
                                    [                EAX                ]                 
                                                      [       AX        ]
                                                      [  AH    ][   AL  ]
```

Register sizes

| C datatype | Bits | Bytes | Register | NASM allocate |
|------------|------|-------|----------|---------------|
| char       | 8    | 1     | al       | db            |
| short      | 16   | 2     | ax       | dw            |
| int        | 32   | 4     | eax      | dd            |
| long       | 64   | 8     | rax      | dq            |

Register moves

| from       | to 64 bit rcx | to 32 bit ecx | to 16 bit cx | to 8 bit cl | Notes
|------------|---------------|---------------|--------------|-------------|
| 64 bit rax | mov rax,rcx   | movsxd rax,ecx| movsx rax,cx | movsx rax,cl| Writes to whole register
| 32 bit eax | mov eax,ecx   | mov eax,ecx   | movsx eax,cx | movsx eax,cl| Top half of destination gets zeroed
| 16 bit ax  | mov ax,cx     | mov ax,cx     | mov ax,cx    | movsx ax,cl | Only affects low 16 bits, rest unchanged
| 8 bit al   | mov al,cl     | mov al,cl     | mov al,cl    | mov al,cl   | Only affects low 8 bits, rest unchanged

Linux syscall
 
| system call number | 1st param | 2nd param | 3rd param | 4th param | 5th param | 6th param | result |
|--------------------|-----------|-----------|-----------|-----------|-----------|-----------|--------|
| rax                | rdi       | rsi       | rdx       | r10       | r8        | r9        | rax    |

Linux C ABI (integers)

| 1st param | 2nd param | 3rd param | 4th param | 5th param | 6th param | 7+ params | result |
|-----------|-----------|-----------|-----------|-----------|-----------|-----------|--------|
| rdi       | rsi       | rdx       | rcx       | r8        | r9        | stack     | rax    |

| instruction | result |
|-------------|--------|
| MOV dest,src| dest = src
| CALL func   | call a function
| RET         | return from a function
| SYSCALL     | call an OS function
| PUSH src    | push onto stack
| POP dest    | pop from stack
| ADD a,b     | a = a + b
| MUL a,b     | a = a * b
| AND a,b     | a = a & b
| OR a,b      | a = a & b
| DIV src     | Divide rax by src. Put the quotient in rax, and the remainder in rdx. rdx MUST be zero on call or you get SIGFPE
| SHR val,bits| val = val >> bits; bits can also be rcx (only)
| CMP a,b     | Compare a and b. Used for conditional jumps
| JMP label   | Unconditional jump
| JL label    | Jump if less than (signed)
| JLE label   | Jump if less than or equal (signed)
| JG label    | Jump if greater than (signed)
| JGE label   | Jump if greater than or equal (signed)
| JNE label   | Jump if not equal
| JE label    | Jump if equal
| RDRAND      | random number (16,32,64-bit only)

## x86 and x86-64 ISA
instructions - are they that different? combine maybe

## Summary of NASM documenation
Based on NASM 2.15.05.
### Chapter 1: What is NASM?
NASM works on x86 and x86-64 only.
### Chapter 2: Command line syntax

`nasm -f elf myfile.asm`: Basic use. Generates a elf file called `myfile.o`

Common `-f` formats:
  - `bin` (default)
  - `elf32` (for x86)
  - `elf64` (for x86-64)

`nasm -f elf myfile.asm -o myfile -l myfile.lst`

- `-o myfile`: Output file is `myfile`, not the default `myfile.o`
- `-l myfile.lst`: Generate a listing. Example listing line below

        14 00000000 B801000000                  mov rax, 1      ; 1 = 'write' syscall`
        L# address  machine code                assembly        comment in .asm

- `-w+float`, `-w-float`: disable or enable a class of warning
- `-i/usr/share/include`: Add a search path for the `%include` directive 

### Chapter 3: The NASM language
The basic format of a NASM line is

        label:    instruction operands        ; comment

NASM lines are designed to be assembled pretty much independently. For example, the exact type of variables declared elsewhere does not influence the generated code. Obviously, references to labels are filled in, but for example the type of jump (local vs long jump) must be supplied by the programmer.

d[bwdqtoyz]: (pseudo-instruction) Declare initialized data
 - db (define byte)    8
 - dw (define word)    16
 - dd (define double)  32
 - dq (define quad)    64
 - dt, do, dy, dz: ? (float only?)

        db 0x55            ; Just one byte
        db 0x55,0x56,0x57  ; Three bytes
        db 'hello',0       ; String constant made of bytes
        db 64 dup 0        ; 64 zero bytes

times <x> <instr>: (meta-instruction) Assemble an instruction the given number of times

        times 64 db 0      ; 64 zero bytes

res[bwdqtoyz]: (pseudo-instruction) Reserve space

        resb 64            ; Reserve 64 uninitialized bytes
        db   ?             ; Another syntax for uninitialized data
        db   64 dup ?      ; Reserve 64 uninitialized bytes

incbin: (pseudo-instruction) Transclude a binary file

        incbin "file.dat"  ; directly copy the given binary file to the output
        incbin "file.dat",100,200 ; copy bytes starting at 100, but only up to 200 bytes total.

equ: (pseudo-instruction) Define a compile-time constant

        message db "Hello, world",0 ; A c-style string
        msglen  equ $-message       ; Length of the string. See $ immediately below.```

Constants and expressions:

- $: the address of the current instruction output
- $$: the address of start-of-section
- [x]: the address given by expression <x>

memory references

        wordvar: dw 123
        mov ax, [wordvar] ; move the value at memory address 123 to ax
        mov ax, [wordvar+1]
        mov eax, [dbx*2+ecx+offset]

constants:

- decimal: 200, 0200, 200d, 0d200
- hex: 0xc8, $0xc8, 0hc8, 0x8h: hex
- octal: 310o, 0o310, 0q310:      octal
- binary: 11001000b, 0b1100_1000:  binary
- string: `hi`, 'hi', "hi".  1-8 byte strings can be considered integers.
- float: -0.2, 1.2e10. see docs for more about floats, i don't care about floats.

seg/wrt: memory segmentation, used for 16-bit programs.

local labels: .labels are labels which can be repeated. references are to the "nearest" local label.

        .loop
            ; code
            jne .loop
            ret

### Chapter 4: The NASM Preprocessor
Single-line macros work similarly to the C preprocessor.

        %include MACROS_MAC ; Recommend using C-style include guards

        %define ctrl 0x1F &
        %define a(x) 1+b(x)
        %define b(x) 2*x
        %undef ctrl             ; Un-define a macro
        %ifdef DEBUG
            writefile 2, "Function performed successfully",13,10
        %endif

%xdefine is similar to %define, but if referenced by another macro it's expanded at definition time.

%+ prevents tokenizing in a macro:

        mov     ax,BDASTART + tBIOSDA.COM1addr 
        %define BDA(x)  BDASTART + tBIOSDA. %+ x
        mov     ax,BDA(COM1addr)

Some additional commands, not found in C:

        %assign i i+1       ; Defines numeric macros only, which take no variables
        %defalias OLD NEW   ; two-way symlink for macros
        %ifidni TARGET x64  ; test exact text equality.
        %ref 64             ; like times, but works for multi-line expressions. needed for multi-line macros below

Multi-line macros take a number of parameters:

        %macro prologue 1
            push    ebp
            mov     dbp,esp
            sub     esp,%1
        %endmacro

Here `1` is the number of parameters, and `%1` is a reference to the first parameter. You can also have optional parameters, greedy parameters, varargs-style parameters, etc. See the docs for more.

Macro-local labels can help with multi-line macros:

        %macro retz 0
        jnz     %%skip
        ret
        %%skip:
        %endmacro
When evaluated, becomes `..@1.skip`, `..@2.skip` etc.

Many more macro features omitted:
- For multi-line macros. Varargs, greedy args, optional args, condition code args (see below)
- more %if variants
- Obscure quoting and self-reference
- Condition codes arguments (E->JE, NZ->JNZ, etc in macro)
- Modifying generation of a listing
- Dependencies
- Contexts and context-local labels
- User-defined errors and warnings
- %pragma

### Chapter 5: Standard Macros
- struc/endstruc/istruc+at: define a C-style structure
- align: aligns code or data

### Chapter 6: Standard Macro Packages

        %use altreg ; Import a standard macro package

- altreg: alternate register names
- martalign: better align macro
- fp: floating point macros
- ifunc: integer functions
- masm: masm compatiblity

### Chapter 7: Assembler Directives
- `BITS 16` / `BITS 32` / `BITS 64`: Explicitly declare a bit mode. Discouraged since it can be done automatically by output format.
- `DEFAULT REL` / `DEFAULT ABS` / `DEFAULT [NO]BND`: Change assembler defaults
- `SECTION .text`, etc.: Start a section (ignored by some formats, like obj)
- `ABSOLUTE 0x1A`: Place the code at this absolute address
- `EXTERN _printf`: Declare that we will be using a symbol not declared here. (from a library)
- `GLOBAL _main`: Opposite of EXTERN--declares a public symbol for use by other libraries.
- `COMMON intvar 4`: Like GLOBAL, but multiple modules declaring something will be merged
- `CPU`: Restrict instruction set
- `FLOAT`: Set various floating point options
- `WARNING`: Enable or disable warnings

### Chapter 8: Output Formats
- `bin`: Generate only machine code. Useful for bootloaders, operating systems, etc.
    - ORG 0x100 ; tell NASM the absolute address where the program will be loaded
- `elf32,elf64,elfx32`: Executable Object file and/or Linkable Object File
- `coff`: Don't use (DJGPP linker)
- `macho??`: mac object format
- `obj,win32,win64`: dos/windows object format
- `aout,aoutb(?),as85,rdf`: Obsolete
- `dbg`: Debugging. Don't use.

### Chapter 9: Writing 16-bit code (DOS, Windows 3/3.1)
16-bit EXEs can be made by linking `.obj` files, or directly with a DOS .exe macro header

16-bit COMs can be made by adding an extra line or two at the start of `bin`. 

.sys (DOS driver) is similar to .com with a different offset.

Interfacing with external libraries: see docs

### Chapter 10: Writing 32-bit code (Unix, Win32, DJGPP)
No memory segmentation--"flat" memory model where you just have a 32-bit, 4GB address space.

C call interface [very helpful, copy from https://www.nasm.us/xdoc/2.15.05/html/nasmdo10.html]

**C ABI for Unix**
- All paramters are pushed on the stack, from right to left
- Call is 
- EBP must be preserved, so usually EBP is pushed to stack, then ESP (stack pointer) copied to EBP, and then the callee reads values off the stack based on that. `[EBP]` is the old `[EBP]` value. `[EBP+4]` is the return address pushed by `CALL`. Parameters start at `[EBP+8]`, starting from the leftmost.
- Return value is in EAX (or AL/AX/ST0).
- At the end, callee restores original ESP, pops EBP if that was pushed, and returns.
- On return, caller moves stack pointer down with a constant (faster), or uses a series of pops.

See later section for syscall to Linux kernel

### Chapter 11: Mixing 16- and 32-bit Code [ obsolete ]

### Chapter 12: Writing 64-bit Code (Unix, Win64)
**C ABI for Unix**
- Stack should be 16-byte aligned
- The first six arguments are passed in RDI, RSI, RDX, RCX, R8, and R9.
- All the above, plus RAX, R10, and R11 are "scratch" registers destroyed by function calls and don't need to be saved.
- Additional arguments are passed on the stack.
- Integer return values are placed in RAX and RDX (up to two integers can be returned, for structs or 128-bit integers)
- Memory, structs, strings, floats, etc are all done differently and you can check https://gitlab.com/x86-psABIs/x86-64-ABI

**C ABI for Windows**
- Integers are passed in RCX, RDX, R8, R9, and then the stack.
- Return value is in RAX (only)
- Floating point, memory, etc work differently and you'll have to read a spec.

See later section for syscall to Linux kernel

### Chapter 13: Troubleshoting [useless]

### Appendix A: Ndisasm (Netwide Disassembler)
    There's a disassembler. It can only read binary (not elf, etc)

### Appendix B: Instruction List [useless]

### Appendix C: Version History [useless]

### Appendix D: Building NASM from Source [useless]

### Appendix E: Contact Information [useless]

## System calls
TODO: [Interfacing with Linux](https://en.wikibooks.org/wiki/X86_Assembly/Interfacing_with_Linux)
