#include <panic.hpp>
#include <cstring>
#include <lib/Flanterm/gfx.h>
#include <drivers/serial/serial.hpp>
#include <drivers/serial/printf.h>
#include <cstdio>
#include <arch/arch.hpp>
#include <mem/mem.hpp>
#include <drivers/timers/pit/pit.hpp>
#include <uacpi/uacpi.h>
#include <uacpi/event.h>
#include <uacpi/tables.h>
#include <ramfs/ramfs.hpp>
#include <pci/pci.hpp>
#include <exec/elf.hpp>
#include <drivers/input/ps2k/ps2k.hpp>
#include <drivers/input/ps2k/ps2k_key_event.hpp>
#include <pcie/pcie.hpp>
#include <drivers/tty/ldisc/ldisc.hpp>
#include <drivers/input/ps2m/ps2m.hpp>
#include <dbg/dbg.hpp>
#include <arch/x86_64/syscall/handlers.hpp>
#include <arch/x86_64/apic/apic.hpp>
#include <drivers/timers/apic/apic.hpp>
#include <parse_karg.hpp>
#include <arch/x86_64/acpi/kernel_api.hpp>

#define UACPI_ERROR(name, isinit) \
if (uacpi_unlikely_error(uacpi_result)) { \
    Log::errf("uACPI %s Failed: %s", \
              name, uacpi_status_to_string(uacpi_result)); \
    asm volatile ("cli; hlt;"); \
} \
else \
    Log::printf_status("OK", "uACPI %s%s", \
                       name, ((isinit) ? "d" : " Initialised"))

#include <limine.h>
__attribute__((section(".limine_requests")))
volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST_ID,
    .revision = 0,
    .response = nullptr, // shut up gcc
};

__attribute__((section(".limine_requests")))
volatile struct limine_executable_cmdline_request executable_cmdline_request = {
    .id = LIMINE_EXECUTABLE_CMDLINE_REQUEST_ID,
    .revision = 0,
    .response = nullptr,
};

extern "C" void init() {
	asm ("cli");
    if (module_request.response == nullptr || module_request.response->module_count < 1) {
        asm volatile ("cli;hlt");
    }
    if (executable_cmdline_request.response == nullptr || executable_cmdline_request.response->cmdline == nullptr) {
        asm volatile ("cli;hlt");
    }

    flanterm_initialise();

    serial::serial_enable();
    Log::printf_status("OK", "Flanterm Initialised"); // late
    Log::printf_status("OK", "Serial Initialised");
    
    arch::x86_64::cpu::gdt::initialise();
    Log::printf_status("OK", "GDT Initialised");

    arch::x86_64::cpu::idt::initialise();
    Log::printf_status("OK", "IDT Initialised");

    mem::pmm::initialise();
    Log::printf_status("OK", "PMM Initialised");

    mem::vmm::initialise();
    Log::printf_status("OK", "VMM Initialised");

    mem::heap::initialise();
    Log::printf_status("OK", "Heap Initialised");

    Log::infof("Disabling COM1 serial output, falling back to graphical interface");
    serial::serial_putc('\033');
    serial::serial_putc('[');
    serial::serial_putc('2');
    serial::serial_putc('J');
    serial::serial_putc('\033');
    serial::serial_putc('[');
    serial::serial_putc('H');
    //serial::serial_disable();
    Log::printf_status("OK", "Serial Disabled");

    uacpi_status uacpi_result = uacpi_initialize(0);
    UACPI_ERROR("Initialise", 1);

    uint8_t mask_master = arch::x86_64::io::inb(0x21);
    uint8_t mask_slave  = arch::x86_64::io::inb(0xA1);
	for (int i = 0; i < 16; i++) arch::x86_64::cpu::idt::irq_set_mask(i);

	arch::x86_64::apic::initialise();
	arch::x86_64::ioapic::initialise();

	drivers::timers::pit::initialise();
    Log::printf_status("OK", "PIT Initialised");

	asm ("sti");
	drivers::timers::apic::initialise();
	Log::printf_status("OK", "APIC Initialised");
	asm ("cli");

	//acpi_reload_interrupts();
	//Log::printf_status("OK", "Reloaded all ACPI interrupts");

	ramfs::initialise();
	Log::printf_status("OK", "RamFS Initialised");
	ramfs::mkdir("/dev", 0777);
	const int stdin = ramfs::open("/dev/stdin", O_CREAT | O_RDWR);
	const int stdout = ramfs::open("/dev/stdout", O_CREAT | O_RDWR);
	const int stderr = ramfs::open("/dev/stderr", O_CREAT | O_RDWR);
	ramfs::load_archive(LOAD_ARCHIVE_TYPE_USTAR, module_request.response->modules[0]->address, module_request.response->modules[0]->size, "/initrd/");

    uint64_t npci = pci::initialise();
    Log::printf_status("OK", "Detected %zu PCI devices (Normal PCI is deprecated, use PCIe)", npci);

	uint64_t npcie = pcie::initialise();
	Log::printf_status("OK", "Detected %zu PCIe devices", npcie);

	arch::x86_64::syscall::initialise();
    Log::printf_status("OK", "Syscalls Initialised");

	drivers::input::ps2k::initialise();
	Log::printf_status("OK", "PS2K Initialised");

    drivers::input::ps2m::initialise();
    Log::printf_status("OK", "PS2M Initialised");

	drivers::tty::ldisc::initialise();
	Log::printf_status("OK", "Line Discipline Initialised");

	size_t nsc = initialise_syscall_handlers();
	Log::printf_status("OK", "Syscall handlers Initialised, there are %zu valid syscalls", nsc);

    auto list_dirs = [&](const char *path, const auto& self) -> void {
        DIR *dir = ramfs::opendir(path);
        if (!dir)
            return;

        printf("%s\n\r", path);

        dirent *entry;
        char fullpath[4096];

        while ((entry = ramfs::readdir(dir)) != nullptr) {
            // Skip . and ..
            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0)
                continue;

            snprintf(fullpath, sizeof(fullpath),
                    "%s/%s", path, entry->d_name);

            // Print entry (file or directory)
            printf("  %s\n\r", fullpath);

            // Recurse only if it's a directory
            DIR *sub = ramfs::opendir(fullpath);
            if (sub) {
                ramfs::closedir(sub);
                self(fullpath, self);
            }
        }

        ramfs::closedir(dir);
    };

    list_dirs("/", list_dirs);

    karg_context* karg_ctx = (karg_context*)mem::heap::malloc(sizeof(karg_context));
    if (!karg_ctx) panic("no memory");

    karg_ctx->base = (void*)executable_cmdline_request.response->cmdline;
    karg_ctx->size = strlen((const char*)karg_ctx->base);
    karg_ctx->INIT_PATH_symbol = "INIT_PATH";
    karg_ctx->default_init_path = "/initrd/init";

    int fd = ramfs::open(check_init_path(karg_ctx), O_RDONLY);
    if (fd < 0) {
        panic("could not find init");
    }

    Log::printf_status("OK", "Parsed command-line");

    stat s;
    ramfs::fstat(fd, &s);
    if (s.st_size < 1) panic("file empty");

    void* exe_buf = mem::heap::malloc(s.st_size);
    if (!exe_buf) panic("no memory");
    
    if (ramfs::read(fd, exe_buf, s.st_size) != s.st_size) panic("failed to read full file");
    
	Log::infof("Entering userspace-init process");

    //Log::end_kernel_messages(); // now no messages print

	asm ("sti");

    run_elf(exe_buf, s.st_size, true);

    while (1) {
        asm volatile("hlt");
    }
    
    __builtin_unreachable();
}
