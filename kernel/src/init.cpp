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
#include <subsystems/ramfs/ramfs.hpp>
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
#include <threadsafety/spinlocks.hpp>
#include <drivers/display/edid/edid.hpp>
#include <drivers/display/gfx.hpp>
#include <subsystems/hlec/hlec.hpp>
#include <drivers/net/netgeneric.hpp>
#include <drivers/blockio/diskgeneric.hpp>
#include <cctype>
#include <drivers/fs/fsgeneric.hpp>
#include <drivers/fs/fat32/fat32.hpp>
#include <vfs/vfs.hpp>
#include <subsystems/sched/sched.hpp>
#include <drivers/timers/hpet/hpet.hpp>
#include <drivers/net/utils.hpp>
#include <drivers/net/dns/dns.hpp>
#include <drivers/net/udp/udp.hpp>
#include <drivers/net/arp/arp.hpp>
#include <drivers/net/icmp/icmp.hpp>
#include <drivers/net/tcpip/tcpip.hpp>

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

__attribute__((section(".limine_requests")))
volatile struct limine_mp_request mp_request = {
	.id = LIMINE_MP_REQUEST_ID,
};

Spinlock* console_lock = nullptr;

struct bootstrap_args {
    uint64_t cr3;
    uint64_t gdtr;
};

extern "C" void prepare_cpu_asm(uint64_t cr3, uint64_t gdtr);
extern "C" uint64_t get_cpu_gdtr();

extern "C" void cpu_entry(struct limine_mp_info *cpu_info) {
    console_lock->acquire(cpu_info->processor_id);
    bootstrap_args* args = (bootstrap_args*)cpu_info->extra_argument;
    
    //prepare_cpu_asm(args->cr3, args->gdtr);

    Log::force_enable();
    Log::printf_status("OK", "CPU#%d Online", cpu_info->processor_id + 1);
    
    console_lock->release();
    asm volatile ("cli; hlt");
}

uint64_t counters[10] = {0};

void f0() {
    printf("begin f0\n\r");
    while (true) {
        counters[0]++;
        printf("%zu", counters[0]);
    }
}

void test_sched() {
    pcb* p0 = sched::new_process((void(*)())f0, "p0", false);

    if (!p0) {
        printf("[test_sched] FAIL: process allocation failed\n");
        return;
    }

    if (p0->num_threads != 1) {
        printf("[test_sched] FAIL: thread counts wrong: p0=%llu\n",
            p0->num_threads);
        return;
    }

    printf("[test_sched] procs: p0(pid=%lld, %llu threads)\n",
        p0->pid, p0->num_threads
    );

    printf("[test_sched] PASS: 1 proc, 1 threads total\n");

    sched::sched_ready();

	asm ("sti");
    while (true) asm volatile("hlt");
}

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

	arch::x86_64::cpu::sse::initialise();
	Log::printf_status("OK", "SSE Initialised");

    mem::pmm::initialise();
    Log::printf_status("OK", "PMM Initialised");

    mem::vmm::initialise();
    Log::printf_status("OK", "VMM Initialised");

    mem::vmm::remap_fb();
    Log::printf_status("OK", "Framebuffer remapped, write combining enabled");

	for (size_t x = 0; x < fbx(); x++) {
    	for (size_t y = 0; y < fby(); y++) {
    		ppx(x, y, 0);
    	}
    }
    refresh_tty();

skip_redraw:;

    mem::heap::initialise();
    Log::printf_status("OK", "Heap Initialised");
    
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

	drivers::timers::hpet::initialise();
    Log::printf_status("OK", "HPET Initialised");

#ifndef CONFIG_PRINT_INFO
#ifndef CONFIG_PRINT_STATUS
#ifndef CONFIG_DEV_MODE
    fb_clrscr(0);

    boot_resources::bgrt::initialise();
    boot_resources::bgrt::display_bgrt();
#endif
#endif
#endif

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

    Log::infof("stdin fd = %d", stdin);
    Log::infof("stdout fd = %d", stdout);
    Log::infof("stderr fd = %d", stderr);

	mem::vmm::mmap(
		(void*)mem::vmm::va_to_pa((((uint64_t)module_request.response->modules[0]->address + 0xFFF)/1000)),
		(void*)(((uint64_t)module_request.response->modules[0]->address + 0xFFF) / 1000),
		((module_request.response->modules[0]->size + 0xFFF) / 0x1000),
		PAGE_PRESENT
	); // map it since limine sometimes is lazy

	ramfs::load_archive(LOAD_ARCHIVE_TYPE_USTAR, module_request.response->modules[0]->address, module_request.response->modules[0]->size, "/initrd/");
	Log::printf_status("OK", "Loaded initrd");

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

    drivers::display::edid::initialise();
    Log::printf_status("OK", "EDID Driver Initialised");

	drivers::net::netgeneric::initialise();
	Log::printf_status("OK", "Networking Initialised");

	drivers::blockio::diskgeneric::initialise();
	Log::printf_status("OK", "Block I/O Initialised");	

	int n_disk = drivers::blockio::diskgeneric::get_disk_count();
	int boot_disk = -1;
	
	disk_info info;

	for (int i = 0; i < n_disk; i++) {
		if (drivers::blockio::diskgeneric::get_disk_info(i, &info)) {
			if (info.boot_disk) {
				boot_disk = i;
				Log::infof("Found boot disk, boot disk serial number is %d", boot_disk); // serial-number isn't the disk's serial number, but the assigned serial number
			}
		}
	}

	if (boot_disk < 0) {
		Log::warnf("Couldn't find boot disk");
	}

	drivers::blockio::diskgeneric::partitions::initialise();
	Log::printf_status("OK", "Partitions Initialised");

	bool full_disk_fs = false;

	drivers::fs::fsgeneric::initialise(full_disk_fs);
	Log::printf_status("OK", "Filesystem Subsystem Initialised");

	vfs::initialise();
	Log::printf_status("OK", "VFS Initialised");

	sched::initialise();
	Log::printf_status("OK", "Scheduler Initialised");

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

#ifndef CONFIG_PRINT_INFO
#ifndef CONFIG_PRINT_STATUS
#ifndef CONFIG_DEV_MODE
	printf("\033[?25l");

    extern uint64_t g_scr_height, g_scr_width;

	int theta = 0;
	int count = 0;
	int time = CONFIG_BGRT_SLEEP_TIME_MS; // millisecond wait
	while ((count++) < (time/2)) {
		if (theta >= 360) theta = 0;
		boot_resources::loading::loading_circle(g_scr_width / 2, g_scr_height - (g_scr_height / 6), 32, 0xFFFFFFFF, theta);
		theta++;
		drivers::timers::apic::sleep_ms(2);
	}

    boot_resources::bgrt::clear_bgrt();

    fb_clrscr(0);
    refresh_tty();

    boot_resources::loading::loading_circle(g_scr_width / 2, g_scr_height - (g_scr_height / 6), 32, 0, 0);
#endif
#endif
#endif

	console_lock = new Spinlock("CONSOLE.LOCK");

	Log::force_enable();

	Log::infof("CPU#0 is already online (BSP/Boostrap Processor)");

    bootstrap_args* args = (bootstrap_args*)mem::heap::malloc(sizeof(bootstrap_args));
    if (!args) {
        panic("no memory");
    }

    uint64_t cr3 = mem::vmm::get_cr3(), gdtr = get_cpu_gdtr();

    args->cr3 = cr3;
    args->gdtr = gdtr;

	console_lock->release();
	for (int i = 0; i < (int)mp_request.response->cpu_count; i++) {
		if (mp_request.response->cpus[i]->lapic_id == mp_request.response->bsp_lapic_id) continue;

		__atomic_store_n(
		    &mp_request.response->cpus[i]->extra_argument,
		    (uint64_t)args,
		    __ATOMIC_RELEASE
		);
		
		__atomic_store_n(
		    &mp_request.response->cpus[i]->goto_address,
		    (uint64_t)cpu_entry,
		    __ATOMIC_RELEASE
		);
		
		drivers::timers::apic::sleep_ms(100);
	}
	drivers::timers::apic::sleep_ms(100);

    Log::end_kernel_messages(); // now no messages print

	init_graphics();
	Log::printf_status("OK", "Software-Accelerated Graphics Initialised");

    // Finished bootstrapping

	const char* argv[] = {
	    "/initrd/init",
	    "-adam_is_kewl=true",
	    nullptr
	};
	
	const char* envp[] = {
	    nullptr
	};

	int exefd = vfs::open("ramfs:/initrd/init", O_RDONLY);
	if (exefd < 0) {
		exefd = vfs::open("a:/Terra/Terrakernel/Sys64/Boot/TkInit", O_RDONLY);
		if (exefd < 0) {
			panic("Could not open neither TkInit nor initrd/init, please reinstall TK to your media to fix this error");
		}
	}

	uint64_t sz = vfs::stat_sz(exefd);
	void* data = mem::heap::malloc(sz);
	int64_t nreadbytes = vfs::read_file(exefd, data, sz);
	vfs::close(exefd);

	run_elf(data, nreadbytes, true, argv, envp);

	while (1) {
    	asm volatile("hlt");
    }
    
    __builtin_unreachable();
}
