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
#include <tasking/sched.hpp>

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
    
    //asm ("sti");
    //uacpi_result = uacpi_namespace_load();
    //UACPI_ERROR("namespace loade", 1);
	//asm ("cli");
    
    //uacpi_result = uacpi_namespace_initialize();
    //UACPI_ERROR("namespace", 0);

    //uacpi_result = uacpi_finalize_gpe_initialization();
    //UACPI_ERROR("GPE", 0);

	for (int i = 0; i < 16; i++) arch::x86_64::cpu::idt::irq_set_mask(i);

	arch::x86_64::apic::initialise();
	arch::x86_64::ioapic::initialise();
	Log::printf_status("OK", "APIC Initialised");

	asm ("sti");
	drivers::timers::apic::initialise();
	Log::printf_status("OK", "APIC Timer Initialised");
	asm ("cli");

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

	messages::initialise();
	Log::printf_status("OK", "Messages Initialised");

	messages::print_message(TEST, EN_UK);
	messages::print_message(TEST, DE);
	messages::print_message(TEST, FR);
	messages::print_message(TEST, ES);
	messages::print_message(TEST, IT);
	messages::print_message(TEST, PT);
	messages::print_message(TEST, NL);
	messages::print_message(TEST, SV);
	messages::print_message(TEST, NO);
	messages::print_message(TEST, DK);
	messages::print_message(TEST, FI);
	messages::print_message(TEST, IS);

    tasking::sched::create_task([](){
        while (1) {
            printf("TIC\n");
        }
    });
    tasking::sched::create_task([](){
        while (1) {
            printf("TAC\n");
        }
    }, true);
    tasking::sched::create_task([](){
        while (1) {
            printf("TOE\n");
        }
    }, true);
    tasking::sched::initialise();

	asm ("sti");

	int fd = ramfs::open("/initrd/init", O_RDONLY);
	if (fd < 0) { Log::panic("no init executable was found... Halting..."); }

	stat sb;
	ramfs::fstat(fd, &sb);
	if (sb.st_size < 1) { Log::panic("init executable has no data"); }

	void* exe_buf = mem::vmm::valloc((sb.st_size + 0xFFF) / 0x1000);
	size_t read = (size_t)ramfs::read(fd, exe_buf, sb.st_size);

	if (read != sb.st_size) { Log::panic("failed to read the init executable correctly"); }

	
    while (1) {
		run_elf(exe_buf, sb.st_size, true); // you cannot terminate this program
        asm volatile("hlt");
    }
    
    __builtin_unreachable();
}
