# The Terrakernel Project
## Made for Terra

Terrakernel is a hybrid x86_64 kernel.

During the development of TK the kernel will always be on version v1.0-rc1.
Terra is the operating system I'm planning to use terrakernel for.

# TODO
*: WIP

### Initial stuff
- [x] Port printf implementation
- [x] Support for some COM1 serial output using port 0x3F8
- [x] End of initial stuff

### x86_64 Specific
- [x] Write a GDT
- [x] Write an IDT
- [x] Write a physical memory manager
- [x] Write a virtual memory manager
- [x] Write a heap
- [x] Write a PIT timer (Unused now, use APIC)
- [x] Write an APIC and Multiprocessing support
- [x] Write an APIC timer
- [x] Switch to fully graphical (flanterm) messages and logs
- [x] Port uACPI
- [x] (Other) Write a VFS and RamFS and parse a USTAR Initrd archive
- [x] Multiprocessing
- [ ] Scheduling and multithreading*
- [x] IPC (this isn't unix-like, just open a normal ramfs file)
- [x] Switching to userspace
- [x] Write some basic syscalls
- [x] Load x86_64 ELF binaries, static and relocatable
- [x] BGRT table
- [x] Remap the framebuffer with Write Combining for maximum performance
- [x] Loading cirle
- [x] The HLEC (HL Event Complex)
- [ ] End of x86_64 stuff

### Subsystems
- [x] Write a VFS and RamFS and parse a USTAR Initrd archive
- [x] Write a PCI driver
- [x] Write a PCIe driver and stop using PCI
- [x] Write a PS2 keyboard driver and PS2 mouse driver with an event system
- [x] Line discipline
- [x] EDID driver
- [x] AHCI disk driver
- [x] FAT32 file system
- [x] Partitions with MBR
- [x] Partitions with GPT
- [x] Userspace graphics driver
- [x] Network cards drivers
- [x] DHCP support
- [x] UDP support
- [x] TCP/IP support
- [x] HTTP protocol support
- [x] End of other

### Writing and Porting software
- [ ] Write a better init process
- [x] Port mlibc
- [ ] Write a window manager
- [ ] Port something
- [ ] Write a package manager, tpkgs (Terra PKGs)
- [ ] End of writing and porting software
- [ ] End of project... or at least this version...

### Building the kernel
Check [BUILD_INSTRUCTIONS.md](https://github.com/Terrakernel-Project/Terrakernel/blob/master/BUILD_INSTRUCTIONS.md)

### How many LoC?

```x86asm
     321 text files.
     300 unique files.
       5 files ignored.

github.com/AlDanial/cloc v 1.98  T=0.97 s (310.3 files/s, 98146.4 lines/s)
-------------------------------------------------------------------------------
Language                     files          blank        comment           code
-------------------------------------------------------------------------------
C                               55           6549           4124          36436
C/C++ Header                   175           3731          11702          16179
C++                             62           2762             54          12990
Assembly                         6             56              2            290
CMake                            1              0              0             21
Markdown                         1              0              0              2
-------------------------------------------------------------------------------
SUM:                           300          13098          15882          65918 (62926 old / +2992)
-------------------------------------------------------------------------------
```

### Kernel logos are in `logos/`

Logos are PNG, there is a BMP in initrd/ but it is 256x256

![Logo 256x256](./logos/256x256.png)
![Logo 1024x1024](./logos/1024x1024.png)
![Logo 4096x4096](./logos/4096x4096.png)

![Used as template for the logo](https://www.flaticon.com/free-icons/global)

### Screenshots

![Burn on flash drive](./images/recommended_burn.png)

![TK Running with a userspace shell!](./images/TK_shell_running.png)

