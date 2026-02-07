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
- [ ] IPC
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
- [ ] AHCI disk driver
- [ ] NVMe disk driver
- [ ] FAT32 file system
- [ ] HLFS file system
- [ ] Partitions with MBR
- [ ] Partitions with GPT
- [ ] Userspace graphics driver
- [x] Network cards drivers
- [ ] DHCP support
- [ ] UDP support
- [ ] TCP/IP support
- [ ] HTTP protocol support
- [ ] End of other

### Writing and Porting software
- [ ] Write a better init process
- [ ] Write a LibC
- [ ] Write a window manager
- [ ] Port something
- [ ] Write a package manager, tpkgs (Terra PKGs)
- [ ] End of writing and porting software
- [ ] End of project... or at least this version...

### Building the kernel
Check [BUILD_INSTRUCTIONS.md](https://github.com/Terrakernel-Project/Terrakernel/blob/master/BUILD_INSTRUCTIONS.md)

### How many LoC?

```x86asm
     275 text files.
     254 unique files.
       5 files ignored.

github.com/AlDanial/cloc v 1.98  T=1.04 s (244.7 files/s, 83712.6 lines/s)
-------------------------------------------------------------------------------
Language                     files          blank        comment           code
-------------------------------------------------------------------------------
C                               55           6554           4143          36441
C/C++ Header                   151           3509          11643          15436
C++                             40           1656             37           7102
Assembly                         6             55              1            281
CMake                            1              0              0             21
Markdown                         1              0              0              2
-------------------------------------------------------------------------------
SUM:                           254          11774          15824          59283 (58940 old / +343 LoC)
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

