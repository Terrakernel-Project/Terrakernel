#include "elf.hpp"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <mem/vmm.hpp>
#include <mem/pmm.hpp>
#include <arch/arch.hpp>
#include <cstdio>

constexpr size_t PAGE_SIZE = 0x1000;
constexpr size_t PAGE_MASK = PAGE_SIZE - 1;

constexpr uint16_t ET_EXEC = 2;
constexpr uint16_t ET_DYN  = 3;

constexpr uint32_t PT_LOAD    = 1;
constexpr uint32_t PT_DYNAMIC = 2;

constexpr uint32_t PF_X = 0x1;
constexpr uint32_t PF_W = 0x2;
constexpr uint32_t PF_R = 0x4;

constexpr int64_t DT_NULL    = 0;
constexpr int64_t DT_RELA    = 7;
constexpr int64_t DT_RELASZ  = 8;
constexpr int64_t DT_RELAENT = 9;

constexpr uint64_t R_X86_64_RELATIVE = 8;

static inline uint64_t align_down(uint64_t addr, uint64_t alignment) {
    return addr & ~(alignment - 1);
}

static inline uint64_t align_up(uint64_t addr, uint64_t alignment) {
    return (addr + alignment - 1) & ~(alignment - 1);
}

static inline bool is_valid_elf_magic(const Elf64_Ehdr* ehdr) {
    return ehdr->e_ident[0] == 0x7F &&
           ehdr->e_ident[1] == 'E' &&
           ehdr->e_ident[2] == 'L' &&
           ehdr->e_ident[3] == 'F';
}

static inline uint64_t elf_reloc_type(uint64_t info) {
    return info & 0xFFFFFFFF;
}

static inline uint64_t convert_flags_to_page_flags(uint32_t elf_flags, bool user_mode) {
    uint64_t flags = PAGE_PRESENT;
    
    if (elf_flags & PF_W) {
        flags |= PAGE_RW;
    }
    
    if (user_mode) {
        flags |= PAGE_USER;
    }
    
    return flags;
}

static Elf64_Dyn* find_dynamic_section(uint64_t load_base, Elf64_Phdr* phdrs, uint16_t phnum) {
    for (uint16_t i = 0; i < phnum; i++) {
        if (phdrs[i].p_type == PT_DYNAMIC) {
            return reinterpret_cast<Elf64_Dyn*>(load_base + phdrs[i].p_vaddr);
        }
    }
    return nullptr;
}

static void process_relocations(uint64_t load_base, Elf64_Phdr* phdrs, uint16_t phnum) {
    Elf64_Dyn* dynamic_section = find_dynamic_section(load_base, phdrs, phnum);
    if (!dynamic_section) {
        return;
    }

    Elf64_Rela* rela_table = nullptr;
    size_t rela_total_size = 0;
    size_t rela_entry_size = sizeof(Elf64_Rela);

    for (Elf64_Dyn* entry = dynamic_section; entry->d_tag != DT_NULL; entry++) {
        switch (entry->d_tag) {
            case DT_RELA:
                rela_table = reinterpret_cast<Elf64_Rela*>(load_base + entry->d_val);
                break;
            case DT_RELASZ:
                rela_total_size = entry->d_val;
                break;
            case DT_RELAENT:
                rela_entry_size = entry->d_val;
                break;
        }
    }

    if (!rela_table || rela_total_size == 0) {
        return;
    }

    size_t num_relocations = rela_total_size / rela_entry_size;
    
    for (size_t i = 0; i < num_relocations; i++) {
        Elf64_Rela* reloc = &rela_table[i];
        uint64_t reloc_type = elf_reloc_type(reloc->r_info);
        
        if (reloc_type == R_X86_64_RELATIVE) {
            uint64_t* target = reinterpret_cast<uint64_t*>(load_base + reloc->r_offset);
            *target = load_base + reloc->r_addend;
        }
    }
}

static bool map_segment_pages(uint64_t segment_base, size_t segment_size, uint64_t page_flags) {
    uint64_t page_start = align_down(segment_base, PAGE_SIZE);
    uint64_t page_end = align_up(segment_base + segment_size, PAGE_SIZE);
    size_t num_pages = (page_end - page_start) / PAGE_SIZE;

    for (size_t i = 0; i < num_pages; i++) {
        void* virtual_addr = reinterpret_cast<void*>(page_start + i * PAGE_SIZE);
        
        void* physical_page = mem::pmm::palloc(1);
        if (!physical_page) {
            Log::errf("Failed to allocate physical memory for segment");
            return false;
        }
        
        uint64_t phys_as_va = mem::vmm::pa_to_va(reinterpret_cast<uint64_t>(physical_page));
        mem::memset(reinterpret_cast<void*>(phys_as_va), 0, PAGE_SIZE);
        
        uint64_t result = mem::vmm::mmap(physical_page, virtual_addr, 1, page_flags);
        if (result == 0) {
            Log::errf("Failed to map page at virtual address %p", virtual_addr);
            mem::pmm::free(physical_page, 1);
            return false;
        }
    }
    
    return true;
}

static bool load_elf_segment(const Elf64_Phdr* phdr, uint64_t load_base, 
                             const uint8_t* elf_data, bool user_mode) {
    uint64_t segment_vaddr = (load_base != 0) 
                            ? load_base + phdr->p_vaddr 
                            : phdr->p_vaddr;
    
    uint64_t page_flags = convert_flags_to_page_flags(phdr->p_flags, user_mode);
    
    if (!map_segment_pages(segment_vaddr, phdr->p_memsz, page_flags)) {
        Log::errf("Failed to map segment pages");
        return false;
    }
    
    if (phdr->p_filesz > 0) {
        mem::memcpy(reinterpret_cast<void*>(segment_vaddr),
                   elf_data + phdr->p_offset,
                   phdr->p_filesz);
    }
    
    if (phdr->p_memsz > phdr->p_filesz) {
        size_t bss_size = phdr->p_memsz - phdr->p_filesz;
        mem::memset(reinterpret_cast<void*>(segment_vaddr + phdr->p_filesz), 
                   0, bss_size);
    }
    
    return true;
}

constexpr uint64_t STACK_START = 0x7FFFFFFF0000ULL;

struct stack_entry {
    void* bottom;
    void* top;
    size_t npages;
    size_t nbytes;
    bool user;
    stack_entry* next;
    stack_entry* free_next;
};

struct stack_table {
    uint64_t num_stacks;
    stack_entry* first_stack;
    stack_entry* free_stacks;
    uint64_t current_top;
} stable = {0, nullptr, nullptr, STACK_START};

stack_entry* allocate_stack_entry(size_t num_pages, bool user) {
    stack_entry* e = nullptr;

    stack_entry** prev_ptr = &stable.free_stacks;
    stack_entry* curr = stable.free_stacks;
    while (curr) {
        if (curr->npages >= num_pages) {
            *prev_ptr = curr->free_next;
            e = curr;
            break;
        }
        prev_ptr = &curr->free_next;
        curr = curr->free_next;
    }

    if (!e) {
        e = (stack_entry*)mem::heap::malloc(sizeof(stack_entry));
        if (!e) return nullptr;
        e->npages = num_pages;
        e->nbytes = num_pages * PAGE_SIZE;
        e->user = true;
    }

    e->next = nullptr;
    e->free_next = nullptr;

    uint64_t top = stable.current_top;
    uint64_t bottom = top - num_pages * PAGE_SIZE;

    for (size_t i = 0; i < num_pages; i++) {
        void* phys = mem::pmm::palloc(1);
        if (!phys) {
            if (!curr) mem::heap::free(e);
            return nullptr;
        }
        void* va = reinterpret_cast<void*>(bottom + i * PAGE_SIZE);
        mem::vmm::mmap(phys, va, 1, PAGE_PRESENT | PAGE_RW | (user ? PAGE_USER : 0));
        mem::memset(va, 0, PAGE_SIZE);
    }

    e->bottom = reinterpret_cast<void*>(bottom);
    e->top = reinterpret_cast<void*>(top);

    stable.current_top = bottom;

    if (!stable.first_stack) stable.first_stack = e;
    else {
        stack_entry* last = stable.first_stack;
        while (last->next) last = last->next;
        last->next = e;
    }

    stable.num_stacks++;
    return e;
}

void* stack_manager_get_new_stack(size_t num_pages, bool user) {
    stack_entry* e = allocate_stack_entry(num_pages, user);
    return e ? e->top : nullptr;
}

bool destroy_stack(void* stack_top) {
    stack_entry* prev = nullptr;
    stack_entry* curr = stable.first_stack;

    while (curr) {
        if (curr->top == stack_top) {
            for (size_t i = 0; i < curr->npages; i++) {
                void* va = reinterpret_cast<void*>(reinterpret_cast<uint64_t>(curr->bottom) + i * PAGE_SIZE);
                uint64_t pa = mem::vmm::va_to_pa((uint64_t)va);
                mem::vmm::munmap(va, 1);
            }

            if (prev) prev->next = curr->next;
            else stable.first_stack = curr->next;

            curr->free_next = stable.free_stacks;
            stable.free_stacks = curr;

            stable.num_stacks--;
            return true;
        }

        prev = curr;
        curr = curr->next;
    }

    return false;
}

void run_elf(void* elf_base, size_t elf_file_size, bool user_mode) {
    auto* ehdr = reinterpret_cast<Elf64_Ehdr*>(elf_base);
    
    if (!is_valid_elf_magic(ehdr)) {
        Log::errf("Executable header is invalid");
        return;
    }
    
    if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
        Log::errf("Executable is neither ET_EXEC nor ET_DYN");
        return;
    } else {
    	Log::infof("Executable is %s\n\r", ehdr->e_type == ET_EXEC ? "ET_EXEC" : "ET_DYN");
    }
    
    const uint8_t* elf_data = reinterpret_cast<const uint8_t*>(elf_base);
    Elf64_Phdr* phdrs = (Elf64_Phdr*)(elf_data + ehdr->e_phoff);
    
    uint64_t load_base = 0;
    bool is_pie = (ehdr->e_type == ET_DYN);
    
    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            if (!load_elf_segment(&phdrs[i], load_base, elf_data, user_mode)) {
                Log::errf("Failed to load segment %u", i);
                return;
            }
        }
    }
    
    if (is_pie) {
        process_relocations(load_base, phdrs, ehdr->e_phnum);
    }
    
    uint64_t entry_point = is_pie ? (load_base + ehdr->e_entry) : ehdr->e_entry;
    
    void* stack_top = stack_manager_get_new_stack(2, user_mode);
    if (!stack_top) {
        Log::errf("Failed to allocate user stack");
        return;
    }

    Log::infof("ELF loaded: entry=%p stack=%p user_mode=%d", 
               (void*)entry_point, stack_top, user_mode);
    
    if (user_mode) {
        arch::x86_64::ringctl::execute_ring3(
            reinterpret_cast<void(*)()>(entry_point),
            reinterpret_cast<uint8_t*>(stack_top)
        );
    } else {
        uint64_t saved_rsp;
        asm volatile("mov %%rsp, %0" : "=r"(saved_rsp));
        
        asm volatile(
            "mov %0, %%rsp\n"
            "call *%1\n"
            "mov %2, %%rsp"
            :
            : "r"(reinterpret_cast<uint64_t>(stack_top)),
              "r"(entry_point),
              "r"(saved_rsp)
            : "memory"
        );
    }
}

void* get_elf_entry_point_user(void* elf_base, size_t elf_file_size, void* stack_top, void** ret_stack_top) {
    auto* ehdr = reinterpret_cast<Elf64_Ehdr*>(elf_base);
    
    if (!is_valid_elf_magic(ehdr)) {
        Log::errf("Executable header is invalid");
        return nullptr;
    }
    
    if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
        Log::errf("Executable is neither ET_EXEC nor ET_DYN");
        return nullptr;
    }
    
    const uint8_t* elf_data = reinterpret_cast<const uint8_t*>(elf_base);
    Elf64_Phdr* phdrs = (Elf64_Phdr*)(elf_data + ehdr->e_phoff);
    
    uint64_t load_base = 0;
    bool is_pie = (ehdr->e_type == ET_DYN);
    
    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            if (!load_elf_segment(&phdrs[i], load_base, elf_data, true)) {
                Log::errf("Failed to load segment %u", i);
                return nullptr;
            }
        }
    }
    
    if (is_pie) {
        process_relocations(load_base, phdrs, ehdr->e_phnum);
    }
    
    uint64_t entry_point = is_pie ? (load_base + ehdr->e_entry) : ehdr->e_entry;

	if (!stack_top) {    
    	stack_top = stack_manager_get_new_stack(2, true);
    	if (!stack_top) {
	        Log::errf("Failed to allocate user stack");
        	return nullptr;
    	}
    }
    
    Log::infof("ELF loaded: entry=%p stack=%p user_mode=%d", 
               (void*)entry_point, stack_top, true);

	if (ret_stack_top) {
		*ret_stack_top = stack_top;
	}
    
    return reinterpret_cast<void*>(entry_point);
}
