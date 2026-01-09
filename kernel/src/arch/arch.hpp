#ifndef ARCH_HPP
#define ARCH_HPP 1

#include <cstdint>

struct interrupt_frame {
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

#define CPUID_FEAT_FPU       (1 << 0)   // Floating Point Unit
#define CPUID_FEAT_VME       (1 << 1)   // Virtual 8086 Mode Enhancements
#define CPUID_FEAT_DE        (1 << 2)   // Debugging Extensions
#define CPUID_FEAT_PSE       (1 << 3)   // Page Size Extensions
#define CPUID_FEAT_TSC       (1 << 4)   // Time Stamp Counter
#define CPUID_FEAT_MSR       (1 << 5)   // Model-Specific Registers
#define CPUID_FEAT_PAE       (1 << 6)   // Physical Address Extensions
#define CPUID_FEAT_MCE       (1 << 7)   // Machine Check Exception
#define CPUID_FEAT_CX8       (1 << 8)   // CMPXCHG8 Instruction
#define CPUID_FEAT_APIC      (1 << 9)   // Local APIC present
#define CPUID_FEAT_SEP       (1 << 11)  // SYSENTER/SYSEXIT
#define CPUID_FEAT_MTRR      (1 << 12)  // Memory Type Range Registers
#define CPUID_FEAT_PGE       (1 << 13)  // Page Global Enable
#define CPUID_FEAT_MCA       (1 << 14)  // Machine Check Architecture
#define CPUID_FEAT_CMOV      (1 << 15)  // Conditional Move Instruction
#define CPUID_FEAT_PAT       (1 << 16)  // Page Attribute Table
#define CPUID_FEAT_PSE36     (1 << 17)  // 36-bit Page Size Extension
#define CPUID_FEAT_PSN       (1 << 18)  // Processor Serial Number
#define CPUID_FEAT_CLFSH     (1 << 19)  // CLFLUSH instruction
#define CPUID_FEAT_DS        (1 << 21)  // Debug Store
#define CPUID_FEAT_ACPI      (1 << 22)  // Thermal Monitor / ACPI
#define CPUID_FEAT_MMX       (1 << 23)  // MMX instruction
#define CPUID_FEAT_FXSR      (1 << 24)  // FXSAVE/FXRSTOR
#define CPUID_FEAT_SSE       (1 << 25)  // SSE
#define CPUID_FEAT_SSE2      (1 << 26)  // SSE2
#define CPUID_FEAT_SS        (1 << 27)  // Self Snoop
#define CPUID_FEAT_HTT       (1 << 28)  // Hyperthreading
#define CPUID_FEAT_TM        (1 << 29)  // Thermal Monitor
#define CPUID_FEAT_PBE       (1 << 31)  // Pending Break Enable
#define CPUID_FEAT_SSE3      (1 << 0)   // SSE3
#define CPUID_FEAT_PCLMULQDQ (1 << 1)   // PCLMULQDQ
#define CPUID_FEAT_DTES64    (1 << 2)   // 64-bit debug store
#define CPUID_FEAT_MONITOR   (1 << 3)   // MONITOR/MWAIT instructions
#define CPUID_FEAT_DS_CPL    (1 << 4)   // CPL Qualified Debug Store
#define CPUID_FEAT_VMX       (1 << 5)   // VMX (Intel VT-x)
#define CPUID_FEAT_SMX       (1 << 6)   // SMX
#define CPUID_FEAT_EIST      (1 << 7)   // Enhanced Intel SpeedStep
#define CPUID_FEAT_TM2       (1 << 8)   // Thermal Monitor 2
#define CPUID_FEAT_SSSE3     (1 << 9)   // Supplemental SSE3
#define CPUID_FEAT_CID       (1 << 10)  // L1 context ID
#define CPUID_FEAT_FMA       (1 << 12)  // Fused Multiply Add
#define CPUID_FEAT_CX16      (1 << 13)  // CMPXCHG16B
#define CPUID_FEAT_ETPRD     (1 << 14)  // PIT etc.
#define CPUID_FEAT_PDCM      (1 << 15)  // Perf/Debug Capabilities
#define CPUID_FEAT_PCID      (1 << 17)  // Process-context identifiers
#define CPUID_FEAT_DCA       (1 << 18)  // Direct Cache Access
#define CPUID_FEAT_SSE41     (1 << 19)  // SSE4.1
#define CPUID_FEAT_SSE42     (1 << 20)  // SSE4.2
#define CPUID_FEAT_X2APIC    (1 << 21)  // x2APIC
#define CPUID_FEAT_MOVBE     (1 << 22)  // MOVBE instruction
#define CPUID_FEAT_POPCNT    (1 << 23)  // POPCNT instruction
#define CPUID_FEAT_TSCDEADLINE (1 << 24)// TSC Deadline Timer
#define CPUID_FEAT_AES       (1 << 25)  // AES instruction
#define CPUID_FEAT_XSAVE     (1 << 26)  // XSAVE/XRESTOR
#define CPUID_FEAT_OSXSAVE   (1 << 27)  // OS enabled XSAVE
#define CPUID_FEAT_AVX       (1 << 28)  // AVX
#define CPUID_FEAT_F16C      (1 << 29)  // Half-precision FP
#define CPUID_FEAT_RDRAND    (1 << 30)  // RDRAND instruction
#define CPUID_FEAT_HYPERVISOR (1 << 31) // Running in Hypervisor
#define CPUID_FEAT_BMI1      (1 << 3)
#define CPUID_FEAT_BMI2      (1 << 8)
#define CPUID_FEAT_ADX       (1 << 19)
#define CPUID_FEAT_MPX       (1 << 14)
#define CPUID_FEAT_AVX2      (1 << 5)
#define CPUID_FEAT_SMEP      (1 << 7)
#define CPUID_FEAT_SMAP      (1 << 20)
#define CPUID_FEAT_RDSEED    (1 << 18)
#define CPUID_FEAT_PREFETCHWT1 (1 << 0)
#define CPUID_FEAT_SHA       (1 << 29)
#define CPUID_FEAT_CLFLUSHOPT (1 << 23)
#define CPUID_FEAT_NX        (1 << 20)  // No-execute bit
#define CPUID_FEAT_LM        (1 << 29)  // Long mode (64-bit)
#define CPUID_FEAT_SVM       (1 << 2)   // Secure Virtual Machine (AMD VT)

namespace arch {
namespace x86_64 {
	namespace io {
		void outb(uint16_t port, uint8_t value);
		uint8_t inb(uint16_t port);
		void outw(uint16_t port, uint16_t value);
		uint16_t inw(uint16_t port);
		void outl(uint16_t port, uint32_t value);
		uint32_t inl(uint16_t port);

		void io_wait();
	}
namespace cpu {
	namespace gdt {
		void load_tss();
		void load_gdt();
		void initialise();

		void update_stack(uint64_t new_rsp = 0);

		void* get_base();
		void* get_tss_base();
	}

	namespace idt {
		void load_idt();
		void initialise();
		void set_descriptor(uint8_t vector, uint64_t isr, uint8_t flags);
		void clear_descriptor(uint8_t vector);
		void irq_clear_mask(uint8_t irq);
		void irq_set_mask(uint8_t irq);
		void send_eoi(uint8_t irq);

		void* get_base();
	}
}
namespace ringctl {
	extern "C" void execute_ring3(void (*entry)(), void* stack_base);
}
namespace syscall {
	void initialise();
}
namespace misc {
	static inline uint64_t rdmsr(uint64_t msr) {
		uint32_t l, h;
		asm (
			"rdmsr"
			: "=a"(l), "=d"(h)
			: "c"(msr)
		);
		return (uint64_t)(((uint64_t)h << 32) | l);
	}
	
	static inline void wrmsr(uint64_t msr, uint64_t val) {
		uint32_t l = val & 0xFFFFFFFF, h = val >> 32;
		asm (
			"wrmsr"
			:
			: "c"(msr), "a"(l), "d"(h)
		);
	}

	struct cpuid_ret {
	    uint32_t eax;
	    uint32_t ebx;
	    uint32_t ecx;
	    uint32_t edx;
	};

	static inline cpuid_ret cpuid(uint32_t func, uint32_t subleaf) {
		cpuid_ret ret;
		asm volatile (
			"cpuid"
			: "=a"(ret.eax),
			  "=b"(ret.ebx),
			  "=c"(ret.ecx),
			  "=d"(ret.edx)
			: "a"(func), "c"(subleaf)
		);

		return ret;
	}

	static inline void cpuid(cpuid_ret* ret, uint32_t func, uint32_t subleaf) {
		asm volatile (
			"cpuid"
			: "=a"(ret->eax),
			  "=b"(ret->ebx),
			  "=c"(ret->ecx),
			  "=d"(ret->edx)
			: "a"(func), "c"(subleaf)
		);
	}
}
}
}

static inline void* get_gdt_base() {
	return arch::x86_64::cpu::gdt::get_base();
}

static inline void* get_idt_base() {
	return arch::x86_64::cpu::idt::get_base();
}

static inline void* get_tss_base() {
	return arch::x86_64::cpu::gdt::get_tss_base();
}

#endif
