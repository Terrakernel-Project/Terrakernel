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
#include <boot_resources/bgrt/bgrt.hpp>
#include <config.hpp>
#include <boot_resources/loading/loading.hpp>

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
    
    drivers::timers::pit::initialise();
    Log::printf_status("OK", "PIT Initialised (FREQ=300)");
    
    Log::info("Disabling COM1 serial output, falling back to graphical interface");
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

    Log::printf_status("OK", "uACPI Initialised");

#ifndef CONFIG_PRINT_INFO
#ifndef CONFIG_PRINT_STATUS
    fb_clrscr(0);

    boot_resources::bgrt::initialise();
    boot_resources::bgrt::display_bgrt();
#endif
#endif

   	for (int i = 0; i < 16; i++) arch::x86_64::cpu::idt::irq_set_mask(i);

	arch::x86_64::apic::initialise();
	arch::x86_64::ioapic::initialise();
	Log::printf_status("OK", "APIC Initialised");

	drivers::timers::pit::initialise();
	Log::printf_status("OK", "PIT Reinitialised");

	asm ("sti");
	drivers::timers::apic::initialise();
	Log::printf_status("OK", "APIC Timer Initialised");
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

	asm ("sti");

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

#ifndef CONFIG_PRINT_INFO
#ifndef CONFIG_PRINT_STATUS
	printf("\033[?25l");

    extern uint64_t g_scr_height, g_scr_width;

	int theta = 0;
	int count = 0;
	int time = 3000; // millisecond wait
	while ((count++) < (time/2)) {
		if (theta >= 360) theta = 0;
		boot_resources::loading::loading_circle(g_scr_width / 2, g_scr_height - (g_scr_height / 6), 32, 0xFFFFFFFF, theta);
		theta++;
		drivers::timers::apic::sleep_ms(2);
	}

    boot_resources::bgrt::clear_bgrt();

    fb_clrscr(0);

    boot_resources::loading::loading_circle(g_scr_width / 2, g_scr_height - (g_scr_height / 6), 32, 0, 0);
#endif
#endif

    Log::end_kernel_messages(); // now no messages print

    run_elf(exe_buf, s.st_size, true);

    while (1) {
        asm volatile("hlt");
    }
    
    __builtin_unreachable();
}
