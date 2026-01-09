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
- [x] Write an APIC and Multiprocessing support (no MP for now)
- [x] Write an APIC timer
- [x] Switch to fully graphical (flanterm) messages and logs
- [x] Port uACPI
- [x] (Other) Write a VFS and TMPFS and parse a USTAR Initrd archive
- [ ] Scheduling and multithreading (delayed)
- [x] Switching to userspace
- [x] Write some basic syscalls
- [x] Load x86_64 ELF binaries, static and relocatable (copy from old version of TK) (delayed)
- [ ] End of x86_64 stuff (almost) (almost)

### Other
- [ ] Write a VFS and TMPFS and parse a USTAR Initrd archive
- [x] Write a PCI driver (PCIe as well)
- [x] Write a PS2 keyboard driver and PS2 mouse driver
- [x] Line discipline
- [ ] Try to write an XHCI driver for USB device support (optional)
- [ ] Write some disk drivers, probably AHCI only for now
- [ ] Some filesystem drivers, probably FAT32 and maybe, maybe, maybe EXT3 or EXT4
- [ ] End of other

### Porting software
- [ ] Write or port a LibC
- [ ] Port binutils and coreutils
- [ ] Port DOOM
- [ ] Port a window manager (window server) (probably Xorg)
- [ ] Port anything else
- [ ] End of porting software
- [ ] End of project... or at least this version...

### Building the kernel
Check [BUILD_INSTRUCTIONS.md](https://github.com/Atlas-Software-Org/Terrakernel/blob/master/BUILD_INSTRUCTIONS.md)

### How many LoC?

```x86asm
     250 text files.
     229 unique files.
       5 files ignored.

github.com/AlDanial/cloc v 1.98  T=1.90 s (120.3 files/s, 43849.6 lines/s)
-------------------------------------------------------------------------------
Language                     files          blank        comment           code
-------------------------------------------------------------------------------
C                               55           6540           4142          36386
C/C++ Header                   138           3408          11639          15335
C++                             30           1083              6           4663
Assembly                         5             36              0            227
CMake                            1              0              0             21
-------------------------------------------------------------------------------
SUM:                           229          11067          15787          56632 (55777 old)
-------------------------------------------------------------------------------
```

### Screenshot

![Burn on flash drive](./images/recommended_burn.png)
