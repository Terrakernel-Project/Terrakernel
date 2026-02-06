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
constexpr uint32_t PT_INTERP  = 3;
constexpr uint32_t PT_TLS     = 7;

constexpr uint32_t PF_X = 0x1;
constexpr uint32_t PF_W = 0x2;
constexpr uint32_t PF_R = 0x4;

constexpr int64_t DT_NULL       = 0;
constexpr int64_t DT_NEEDED     = 1;
constexpr int64_t DT_PLTRELSZ   = 2;
constexpr int64_t DT_PLTGOT     = 3;
constexpr int64_t DT_HASH       = 4;
constexpr int64_t DT_STRTAB     = 5;
constexpr int64_t DT_SYMTAB     = 6;
constexpr int64_t DT_RELA       = 7;
constexpr int64_t DT_RELASZ     = 8;
constexpr int64_t DT_RELAENT    = 9;
constexpr int64_t DT_STRSZ      = 10;
constexpr int64_t DT_SYMENT     = 11;
constexpr int64_t DT_INIT       = 12;
constexpr int64_t DT_FINI       = 13;
constexpr int64_t DT_SONAME     = 14;
constexpr int64_t DT_RPATH      = 15;
constexpr int64_t DT_SYMBOLIC   = 16;
constexpr int64_t DT_REL        = 17;
constexpr int64_t DT_RELSZ      = 18;
constexpr int64_t DT_RELENT     = 19;
constexpr int64_t DT_PLTREL     = 20;
constexpr int64_t DT_DEBUG      = 21;
constexpr int64_t DT_TEXTREL    = 22;
constexpr int64_t DT_JMPREL     = 23;
constexpr int64_t DT_BIND_NOW   = 24;
constexpr int64_t DT_INIT_ARRAY = 25;
constexpr int64_t DT_FINI_ARRAY = 26;
constexpr int64_t DT_INIT_ARRAYSZ = 27;
constexpr int64_t DT_FINI_ARRAYSZ = 28;
constexpr int64_t DT_FLAGS      = 30;
constexpr int64_t DT_PREINIT_ARRAY = 32;
constexpr int64_t DT_PREINIT_ARRAYSZ = 33;

constexpr uint64_t R_X86_64_NONE       = 0;
constexpr uint64_t R_X86_64_64         = 1;
constexpr uint64_t R_X86_64_PC32       = 2;
constexpr uint64_t R_X86_64_GOT32      = 3;
constexpr uint64_t R_X86_64_PLT32      = 4;
constexpr uint64_t R_X86_64_COPY       = 5;
constexpr uint64_t R_X86_64_GLOB_DAT   = 6;
constexpr uint64_t R_X86_64_JUMP_SLOT  = 7;
constexpr uint64_t R_X86_64_RELATIVE   = 8;
constexpr uint64_t R_X86_64_GOTPCREL   = 9;
constexpr uint64_t R_X86_64_32         = 10;
constexpr uint64_t R_X86_64_32S        = 11;
constexpr uint64_t R_X86_64_16         = 12;
constexpr uint64_t R_X86_64_PC16       = 13;
constexpr uint64_t R_X86_64_8          = 14;
constexpr uint64_t R_X86_64_PC8        = 15;
constexpr uint64_t R_X86_64_DTPMOD64   = 16;
constexpr uint64_t R_X86_64_DTPOFF64   = 17;
constexpr uint64_t R_X86_64_TPOFF64    = 18;
constexpr uint64_t R_X86_64_TLSGD      = 19;
constexpr uint64_t R_X86_64_TLSLD      = 20;
constexpr uint64_t R_X86_64_DTPOFF32   = 21;
constexpr uint64_t R_X86_64_GOTTPOFF   = 22;
constexpr uint64_t R_X86_64_TPOFF32    = 23;
constexpr uint64_t R_X86_64_PC64       = 24;
constexpr uint64_t R_X86_64_GOTOFF64   = 25;
constexpr uint64_t R_X86_64_GOTPC32    = 26;
constexpr uint64_t R_X86_64_SIZE32     = 32;
constexpr uint64_t R_X86_64_SIZE64     = 33;
constexpr uint64_t R_X86_64_IRELATIVE  = 37;

constexpr size_t GUARD_PAGES = 1;
constexpr size_t GUARD_SIZE = GUARD_PAGES * PAGE_SIZE;

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

static inline uint32_t elf_reloc_type(uint64_t info) {
    return info & 0xFFFFFFFF;
}

static inline uint32_t elf_reloc_symbol(uint64_t info) {
    return info >> 32;
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

struct dynamic_info {
    Elf64_Sym*  symtab;
    const char* strtab;
    size_t      strtab_size;
    
    Elf64_Rela* rela;
    size_t      rela_size;
    size_t      rela_ent;
    
    Elf64_Rel*  rel;
    size_t      rel_size;
    size_t      rel_ent;
    
    Elf64_Rela* jmprel;
    size_t      pltrelsz;
    uint64_t    pltrel_type;
    
    uint64_t*   init_array;
    size_t      init_array_sz;
    
    uint64_t*   fini_array;
    size_t      fini_array_sz;
    
    uint64_t*   preinit_array;
    size_t      preinit_array_sz;
    
    uint64_t    init_func;
    uint64_t    fini_func;
    
    bool        has_textrel;
    bool        bind_now;
};

static Elf64_Dyn* find_dynamic_section(uint64_t load_base, Elf64_Phdr* phdrs, uint16_t phnum) {
    for (uint16_t i = 0; i < phnum; i++) {
        if (phdrs[i].p_type == PT_DYNAMIC) {
            return reinterpret_cast<Elf64_Dyn*>(load_base + phdrs[i].p_vaddr);
        }
    }
    return nullptr;
}

static bool parse_dynamic_section(uint64_t load_base, Elf64_Phdr* phdrs, uint16_t phnum, 
                                  dynamic_info& dyn) {
    mem::memset(&dyn, 0, sizeof(dyn));
    
    Elf64_Dyn* dynamic = find_dynamic_section(load_base, phdrs, phnum);
    if (!dynamic) {
        return true;
    }

    for (Elf64_Dyn* entry = dynamic; entry->d_tag != DT_NULL; entry++) {
        switch (entry->d_tag) {
            case DT_SYMTAB:
                dyn.symtab = reinterpret_cast<Elf64_Sym*>(load_base + entry->d_val);
                break;
            case DT_STRTAB:
                dyn.strtab = reinterpret_cast<const char*>(load_base + entry->d_val);
                break;
            case DT_STRSZ:
                dyn.strtab_size = entry->d_val;
                break;
            case DT_RELA:
                dyn.rela = reinterpret_cast<Elf64_Rela*>(load_base + entry->d_val);
                break;
            case DT_RELASZ:
                dyn.rela_size = entry->d_val;
                break;
            case DT_RELAENT:
                dyn.rela_ent = entry->d_val;
                break;
            case DT_REL:
                dyn.rel = reinterpret_cast<Elf64_Rel*>(load_base + entry->d_val);
                break;
            case DT_RELSZ:
                dyn.rel_size = entry->d_val;
                break;
            case DT_RELENT:
                dyn.rel_ent = entry->d_val;
                break;
            case DT_JMPREL:
                dyn.jmprel = reinterpret_cast<Elf64_Rela*>(load_base + entry->d_val);
                break;
            case DT_PLTRELSZ:
                dyn.pltrelsz = entry->d_val;
                break;
            case DT_PLTREL:
                dyn.pltrel_type = entry->d_val;
                break;
            case DT_INIT:
                dyn.init_func = load_base + entry->d_val;
                break;
            case DT_FINI:
                dyn.fini_func = load_base + entry->d_val;
                break;
            case DT_INIT_ARRAY:
                dyn.init_array = reinterpret_cast<uint64_t*>(load_base + entry->d_val);
                break;
            case DT_INIT_ARRAYSZ:
                dyn.init_array_sz = entry->d_val;
                break;
            case DT_FINI_ARRAY:
                dyn.fini_array = reinterpret_cast<uint64_t*>(load_base + entry->d_val);
                break;
            case DT_FINI_ARRAYSZ:
                dyn.fini_array_sz = entry->d_val;
                break;
            case DT_PREINIT_ARRAY:
                dyn.preinit_array = reinterpret_cast<uint64_t*>(load_base + entry->d_val);
                break;
            case DT_PREINIT_ARRAYSZ:
                dyn.preinit_array_sz = entry->d_val;
                break;
            case DT_TEXTREL:
                dyn.has_textrel = true;
                break;
            case DT_BIND_NOW:
                dyn.bind_now = true;
                break;
        }
    }
    
    return true;
}

static const char* reloc_type_name(uint32_t type) {
    switch (type) {
        case R_X86_64_NONE:       return "NONE";
        case R_X86_64_64:         return "64";
        case R_X86_64_PC32:       return "PC32";
        case R_X86_64_GOT32:      return "GOT32";
        case R_X86_64_PLT32:      return "PLT32";
        case R_X86_64_COPY:       return "COPY";
        case R_X86_64_GLOB_DAT:   return "GLOB_DAT";
        case R_X86_64_JUMP_SLOT:  return "JUMP_SLOT";
        case R_X86_64_RELATIVE:   return "RELATIVE";
        case R_X86_64_GOTPCREL:   return "GOTPCREL";
        case R_X86_64_32:         return "32";
        case R_X86_64_32S:        return "32S";
        case R_X86_64_16:         return "16";
        case R_X86_64_PC16:       return "PC16";
        case R_X86_64_8:          return "8";
        case R_X86_64_PC8:        return "PC8";
        case R_X86_64_IRELATIVE:  return "IRELATIVE";
        default:                  return "UNKNOWN";
    }
}

static bool apply_relocation_rela(uint64_t load_base, const Elf64_Rela* reloc, 
                                  const dynamic_info& dyn) {
    uint32_t reloc_type = elf_reloc_type(reloc->r_info);
    uint32_t reloc_sym = elf_reloc_symbol(reloc->r_info);
    
    uint64_t* target = reinterpret_cast<uint64_t*>(load_base + reloc->r_offset);
    uint64_t symbol_value = 0;
    
    if (reloc_sym != 0) {
        if (!dyn.symtab) {
            Log::errf("Symbol table missing for relocation type %s", 
                     reloc_type_name(reloc_type));
            return false;
        }
        
        Elf64_Sym* sym = &dyn.symtab[reloc_sym];
        
        if (sym->st_shndx != 0) {
            symbol_value = load_base + sym->st_value;
        } else {
            Log::warnf("Undefined symbol in relocation (index %u)", reloc_sym);
        }
    }
    
    switch (reloc_type) {
        case R_X86_64_NONE:
            break;
            
        case R_X86_64_64:
            *target = symbol_value + reloc->r_addend;
            break;
            
        case R_X86_64_PC32:
        case R_X86_64_PLT32: {
            int64_t value = symbol_value + reloc->r_addend - (load_base + reloc->r_offset);
            if (value < INT32_MIN || value > INT32_MAX) {
                Log::errf("PC32 relocation overflow: %lld", value);
                return false;
            }
            *reinterpret_cast<int32_t*>(target) = static_cast<int32_t>(value);
            break;
        }
            
        case R_X86_64_RELATIVE:
            *target = load_base + reloc->r_addend;
            break;
            
        case R_X86_64_GLOB_DAT:
        case R_X86_64_JUMP_SLOT:
            *target = symbol_value;
            break;
            
        case R_X86_64_32:
        case R_X86_64_32S: {
            uint64_t value = symbol_value + reloc->r_addend;
            if (reloc_type == R_X86_64_32S) {
                if (static_cast<int64_t>(value) < INT32_MIN || 
                    static_cast<int64_t>(value) > INT32_MAX) {
                    Log::errf("32S relocation overflow");
                    return false;
                }
            } else if (value > UINT32_MAX) {
                Log::errf("32 relocation overflow");
                return false;
            }
            *reinterpret_cast<uint32_t*>(target) = static_cast<uint32_t>(value);
            break;
        }
            
        case R_X86_64_GOTPCREL:
        case R_X86_64_GOT32: {
            int64_t value = symbol_value + reloc->r_addend - (load_base + reloc->r_offset);
            *reinterpret_cast<int32_t*>(target) = static_cast<int32_t>(value);
            break;
        }
            
        case R_X86_64_IRELATIVE: {
            uint64_t resolver = load_base + reloc->r_addend;
            typedef uint64_t (*resolver_func_t)();
            uint64_t resolved = reinterpret_cast<resolver_func_t>(resolver)();
            *target = resolved;
            break;
        }
            
        default:
            Log::warnf("Unsupported relocation type: %s (%u)", 
                      reloc_type_name(reloc_type), reloc_type);
            return false;
    }
    
    return true;
}

static bool apply_relocation_rel(uint64_t load_base, const Elf64_Rel* reloc, 
                                 const dynamic_info& dyn) {
    uint64_t* target = reinterpret_cast<uint64_t*>(load_base + reloc->r_offset);
    
    Elf64_Rela rela;
    rela.r_offset = reloc->r_offset;
    rela.r_info = reloc->r_info;
    rela.r_addend = *target;
    
    return apply_relocation_rela(load_base, &rela, dyn);
}

static bool process_relocations(uint64_t load_base, Elf64_Phdr* phdrs, uint16_t phnum) {
    dynamic_info dyn;
    if (!parse_dynamic_section(load_base, phdrs, phnum, dyn)) {
        return false;
    }
    
    size_t total_relocs = 0;
    
    if (dyn.rela && dyn.rela_size > 0) {
        size_t rela_count = dyn.rela_size / (dyn.rela_ent ? dyn.rela_ent : sizeof(Elf64_Rela));
        Log::infof("Processing %zu RELA relocations", rela_count);
        
        for (size_t i = 0; i < rela_count; i++) {
            if (!apply_relocation_rela(load_base, &dyn.rela[i], dyn)) {
                Log::errf("Failed to apply RELA relocation %zu", i);
                return false;
            }
        }
        total_relocs += rela_count;
    }
    
    if (dyn.rel && dyn.rel_size > 0) {
        size_t rel_count = dyn.rel_size / (dyn.rel_ent ? dyn.rel_ent : sizeof(Elf64_Rel));
        Log::infof("Processing %zu REL relocations", rel_count);
        
        for (size_t i = 0; i < rel_count; i++) {
            if (!apply_relocation_rel(load_base, &dyn.rel[i], dyn)) {
                Log::errf("Failed to apply REL relocation %zu", i);
                return false;
            }
        }
        total_relocs += rel_count;
    }
    
    if (dyn.jmprel && dyn.pltrelsz > 0) {
        if (dyn.pltrel_type == DT_RELA) {
            size_t plt_count = dyn.pltrelsz / sizeof(Elf64_Rela);
            Log::infof("Processing %zu PLT RELA relocations", plt_count);
            
            for (size_t i = 0; i < plt_count; i++) {
                if (!apply_relocation_rela(load_base, &dyn.jmprel[i], dyn)) {
                    Log::errf("Failed to apply PLT relocation %zu", i);
                    return false;
                }
            }
            total_relocs += plt_count;
        } else if (dyn.pltrel_type == DT_REL) {
            Elf64_Rel* plt_rel = reinterpret_cast<Elf64_Rel*>(dyn.jmprel);
            size_t plt_count = dyn.pltrelsz / sizeof(Elf64_Rel);
            Log::infof("Processing %zu PLT REL relocations", plt_count);
            
            for (size_t i = 0; i < plt_count; i++) {
                if (!apply_relocation_rel(load_base, &plt_rel[i], dyn)) {
                    Log::errf("Failed to apply PLT relocation %zu", i);
                    return false;
                }
            }
            total_relocs += plt_count;
        }
    }
    
    if (total_relocs > 0) {
        Log::infof("Successfully applied %zu relocations", total_relocs);
    }
    
    return true;
}

static bool validate_elf_header(const Elf64_Ehdr* ehdr, size_t file_size) {
    if (file_size < sizeof(Elf64_Ehdr)) {
        Log::errf("File too small to contain ELF header");
        return false;
    }
    
    if (!is_valid_elf_magic(ehdr)) {
        Log::errf("Invalid ELF magic number");
        return false;
    }
    
    if (ehdr->e_ident[4] != 2) {
        Log::errf("Not a 64-bit ELF");
        return false;
    }
    
    if (ehdr->e_ident[5] != 1) {
        Log::errf("Not little-endian ELF");
        return false;
    }
    
    if (ehdr->e_machine != 62) {
        Log::errf("Not an x86-64 ELF");
        return false;
    }
    
    if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
        Log::errf("ELF type must be ET_EXEC or ET_DYN");
        return false;
    }
    
    if (ehdr->e_phoff + ehdr->e_phnum * ehdr->e_phentsize > file_size) {
        Log::errf("Program headers extend beyond file size");
        return false;
    }
    
    return true;
}

static bool calculate_load_size(const Elf64_Ehdr* ehdr, const Elf64_Phdr* phdrs,
                                uint64_t& out_min_vaddr, uint64_t& out_max_vaddr) {
    out_min_vaddr = UINT64_MAX;
    out_max_vaddr = 0;
    
    bool found_load = false;
    
    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            found_load = true;
            
            uint64_t seg_start = phdrs[i].p_vaddr;
            uint64_t seg_end = phdrs[i].p_vaddr + phdrs[i].p_memsz;
            
            if (seg_start < out_min_vaddr) {
                out_min_vaddr = seg_start;
            }
            if (seg_end > out_max_vaddr) {
                out_max_vaddr = seg_end;
            }
        }
    }
    
    if (!found_load) {
        Log::errf("No loadable segments found");
        return false;
    }
    
    out_min_vaddr = align_down(out_min_vaddr, PAGE_SIZE);
    out_max_vaddr = align_up(out_max_vaddr, PAGE_SIZE);
    
    return true;
}

static bool allocate_address_space(uint64_t load_base, size_t total_size, 
                                   bool user_mode, size_t& out_num_pages) {
    out_num_pages = total_size / PAGE_SIZE;
    
    Log::infof("Allocating %zu pages (%zu bytes) at base %p", 
               out_num_pages, total_size, (void*)load_base);
    
    for (size_t i = 0; i < out_num_pages; i++) {
        void* phys_page = mem::pmm::palloc(1);
        if (!phys_page) {
            Log::errf("Failed to allocate physical page %zu/%zu", i, out_num_pages);
            
            for (size_t j = 0; j < i; j++) {
                void* va = reinterpret_cast<void*>(load_base + j * PAGE_SIZE);
                uint64_t pa = mem::vmm::va_to_pa(reinterpret_cast<uint64_t>(va));
                mem::vmm::munmap(va, 1);
                mem::pmm::free(reinterpret_cast<void*>(pa), 1);
            }
            return false;
        }
        
        uint64_t phys_as_va = mem::vmm::pa_to_va(reinterpret_cast<uint64_t>(phys_page));
        mem::memset(reinterpret_cast<void*>(phys_as_va), 0, PAGE_SIZE);
        
        void* va = reinterpret_cast<void*>(load_base + i * PAGE_SIZE);
        uint64_t flags = PAGE_PRESENT | PAGE_RW;
        if (user_mode) {
            flags |= PAGE_USER;
        }
        
        uint64_t result = mem::vmm::mmap(phys_page, va, 1, flags);
        if (result == 0) {
            Log::errf("Failed to map page %zu at %p", i, va);
            mem::pmm::free(phys_page, 1);
            
            for (size_t j = 0; j < i; j++) {
                void* cleanup_va = reinterpret_cast<void*>(load_base + j * PAGE_SIZE);
                uint64_t cleanup_pa = mem::vmm::va_to_pa(reinterpret_cast<uint64_t>(cleanup_va));
                mem::vmm::munmap(cleanup_va, 1);
                mem::pmm::free(reinterpret_cast<void*>(cleanup_pa), 1);
            }
            return false;
        }
    }
    
    return true;
}

static void load_segment_data(uint64_t load_base, const Elf64_Phdr* phdr, 
                              const uint8_t* elf_data) {
    uint64_t segment_vaddr = load_base + phdr->p_vaddr;
    
    Log::infof("Loading segment: vaddr=%p memsz=%zu filesz=%zu flags=%c%c%c",
               (void*)segment_vaddr, phdr->p_memsz, phdr->p_filesz,
               (phdr->p_flags & PF_R) ? 'R' : '-',
               (phdr->p_flags & PF_W) ? 'W' : '-',
               (phdr->p_flags & PF_X) ? 'X' : '-');
    
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
}

static void update_segment_permissions(uint64_t load_base, const Elf64_Phdr* phdr, 
                                       bool user_mode) {
    
}

static void categorize_segment(const Elf64_Phdr* phdr, uint64_t load_base,
                               proc_address_space& addr_space) {
    uint64_t seg_start = load_base + phdr->p_vaddr;
    uint64_t seg_end = seg_start + phdr->p_memsz;
    size_t seg_size = phdr->p_memsz;
    
    if (phdr->p_flags & PF_X) {
        if (!addr_space.code_base) {
            addr_space.code_base = reinterpret_cast<void*>(seg_start);
            addr_space.code_size = seg_size;
        } else {
            uint64_t current_end = reinterpret_cast<uint64_t>(addr_space.code_base) + addr_space.code_size;
            if (seg_end > current_end) {
                addr_space.code_size = seg_end - reinterpret_cast<uint64_t>(addr_space.code_base);
            }
        }
    }
    else if (phdr->p_flags & PF_W) {
        if (!addr_space.data_base) {
            addr_space.data_base = reinterpret_cast<void*>(seg_start);
            addr_space.data_size = seg_size;
        } else {
            uint64_t current_end = reinterpret_cast<uint64_t>(addr_space.data_base) + addr_space.data_size;
            if (seg_end > current_end) {
                addr_space.data_size = seg_end - reinterpret_cast<uint64_t>(addr_space.data_base);
            }
        }
    }
    else {
        if (!addr_space.extra_base) {
            addr_space.extra_base = reinterpret_cast<void*>(seg_start);
            addr_space.extra_size = seg_size;
        } else {
            uint64_t current_end = reinterpret_cast<uint64_t>(addr_space.extra_base) + addr_space.extra_size;
            if (seg_end > current_end) {
                addr_space.extra_size = seg_end - reinterpret_cast<uint64_t>(addr_space.extra_base);
            }
        }
    }
}

proc_address_space* load_elf_to_address_space(void* elf_base, size_t elf_file_size, 
                                              bool user_mode) {
    auto* ehdr = reinterpret_cast<Elf64_Ehdr*>(elf_base);
    
    if (!validate_elf_header(ehdr, elf_file_size)) {
        return nullptr;
    }
    
    Log::infof("Loading %s ELF into contiguous address space", 
               ehdr->e_type == ET_EXEC ? "ET_EXEC" : "ET_DYN (PIE)");
    
    const uint8_t* elf_data = reinterpret_cast<const uint8_t*>(elf_base);
    Elf64_Phdr* phdrs = reinterpret_cast<Elf64_Phdr*>(
        const_cast<uint8_t*>(elf_data + ehdr->e_phoff)
    );
    
    bool is_pie = (ehdr->e_type == ET_DYN);
    
    uint64_t min_vaddr, max_vaddr;
    if (!calculate_load_size(ehdr, phdrs, min_vaddr, max_vaddr)) {
        return nullptr;
    }
    
    size_t total_size = max_vaddr - min_vaddr;
    size_t total_size_with_guard = total_size + GUARD_SIZE;
    
    Log::infof("ELF memory span: %p - %p (%zu bytes + %zu guard)", 
               (void*)min_vaddr, (void*)max_vaddr, total_size, GUARD_SIZE);
    
    uint64_t load_base = 0;
    
    size_t num_pages;
    if (!allocate_address_space(load_base + min_vaddr, total_size_with_guard, 
                               user_mode, num_pages)) {
        Log::errf("Failed to allocate address space");
        return nullptr;
    }
    
    proc_address_space* addr_space = (proc_address_space*)mem::heap::malloc(sizeof(proc_address_space));
    if (!addr_space) {
        Log::errf("Failed to allocate address space structure");
        // TODO: Cleanup allocated memory
        return nullptr;
    }
    
    mem::memset(addr_space, 0, sizeof(proc_address_space));
    
    addr_space->base = reinterpret_cast<void*>(load_base + min_vaddr);
    addr_space->total_size = total_size_with_guard;
    addr_space->num_pages = num_pages;
    
    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            load_segment_data(load_base, &phdrs[i], elf_data);
            update_segment_permissions(load_base, &phdrs[i], user_mode);
            categorize_segment(&phdrs[i], load_base, *addr_space);
        }
    }
    
    if (is_pie) {
        Log::infof("Processing relocations for PIE binary");
        if (!process_relocations(load_base, phdrs, ehdr->e_phnum)) {
            Log::errf("Failed to process relocations");
            mem::heap::free(addr_space);
            // TODO: Cleanup allocated memory
            return nullptr;
        }
    }
    
    addr_space->heap_base = reinterpret_cast<void*>(max_vaddr);
    addr_space->heap_size = 0;
    
    void* stack_top = stack_manager_get_new_stack(64, user_mode);
    if (!stack_top) {
        Log::errf("Failed to allocate stack");
        mem::heap::free(addr_space);
        // TODO: Cleanup allocated memory
        return nullptr;
    }
    
    addr_space->stack_base = stack_top;
    addr_space->stack_size = 64 * PAGE_SIZE;
    
    uint64_t entry_point = is_pie ? (load_base + ehdr->e_entry) : ehdr->e_entry;
    
    Log::infof("ELF loaded successfully:");
    Log::infof("  Base:       %p", addr_space->base);
    Log::infof("  Total size: %zu bytes (%zu pages)", addr_space->total_size, addr_space->num_pages);
    Log::infof("  Code:       %p - %p (%zu bytes)", 
               addr_space->code_base, 
               (void*)((uint64_t)addr_space->code_base + addr_space->code_size),
               addr_space->code_size);
    Log::infof("  Data:       %p - %p (%zu bytes)", 
               addr_space->data_base,
               (void*)((uint64_t)addr_space->data_base + addr_space->data_size),
               addr_space->data_size);
    if (addr_space->extra_size > 0) {
        Log::infof("  Extra:      %p - %p (%zu bytes)", 
                   addr_space->extra_base,
                   (void*)((uint64_t)addr_space->extra_base + addr_space->extra_size),
                   addr_space->extra_size);
    }
    Log::infof("  Heap base:  %p", addr_space->heap_base);
    Log::infof("  Stack base: %p", addr_space->stack_base);
    Log::infof("  Entry:      %p", (void*)entry_point);
    
    return addr_space;
}

void free_address_space(proc_address_space* addr_space) {
    if (!addr_space) return;
    
    uint64_t base = reinterpret_cast<uint64_t>(addr_space->base);
    
    for (size_t i = 0; i < addr_space->num_pages; i++) {
        void* va = reinterpret_cast<void*>(base + i * PAGE_SIZE);
        uint64_t pa = mem::vmm::va_to_pa(reinterpret_cast<uint64_t>(va));
        mem::vmm::munmap(va, 1);
        mem::pmm::free(reinterpret_cast<void*>(pa), 1);
    }
    
    mem::heap::free(addr_space);
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
} stable = {
    0,
    nullptr,
    nullptr,
    STACK_START
};

static stack_entry* allocate_stack_entry(size_t num_pages, bool user) {
    stack_entry* e = nullptr;
    bool reused = false;

    stack_entry** prev_ptr = &stable.free_stacks;
    for (stack_entry* curr = stable.free_stacks; curr; curr = curr->free_next) {
        if (curr->npages >= num_pages) {
            *prev_ptr = curr->free_next;
            e = curr;
            reused = true;
            break;
        }
        prev_ptr = &curr->free_next;
    }

    if (!e) {
        e = (stack_entry*)mem::heap::malloc(sizeof(stack_entry));
        if (!e) return nullptr;
    }

    e->npages = num_pages;
    e->nbytes = num_pages * PAGE_SIZE;
    e->user   = user;
    e->next = nullptr;
    e->free_next = nullptr;

    const size_t total_pages = num_pages + 2 * GUARD_PAGES;

    uint64_t top    = stable.current_top;
    uint64_t bottom = top - total_pages * PAGE_SIZE;

    uint64_t stack_bottom = bottom + GUARD_PAGES * PAGE_SIZE;
    uint64_t stack_top    = stack_bottom + num_pages * PAGE_SIZE;

    size_t mapped = 0;
    for (; mapped < num_pages; mapped++) {
        void* phys = mem::pmm::palloc(1);
        if (!phys) {
            for (size_t j = 0; j < mapped; j++) {
                void* va = (void*)(stack_bottom + j * PAGE_SIZE);
                uint64_t pa = mem::vmm::va_to_pa((uint64_t)va);
                mem::vmm::munmap(va, 1);
                mem::pmm::free((void*)pa, 1);
            }
            if (!reused) mem::heap::free(e);
            return nullptr;
        }

        void* va = (void*)(stack_bottom + mapped * PAGE_SIZE);
        mem::vmm::mmap(
            phys,
            va,
            1,
            PAGE_PRESENT | PAGE_RW | (user ? PAGE_USER : 0)
        );
        
        uint64_t phys_as_va = mem::vmm::pa_to_va(reinterpret_cast<uint64_t>(phys));
        mem::memset(reinterpret_cast<void*>(phys_as_va), 0, PAGE_SIZE);
    }

    e->bottom = (void*)stack_bottom;
    e->top    = (void*)stack_top;

    stable.current_top = bottom;

    if (!stable.first_stack) {
        stable.first_stack = e;
    } else {
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
                void* va = (void*)((uint64_t)curr->bottom + i * PAGE_SIZE);
                uint64_t pa = mem::vmm::va_to_pa((uint64_t)va);
                mem::vmm::munmap(va, 1);
                mem::pmm::free((void*)pa, 1);
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

void* get_entry_point_from_elf(void* elf_base, size_t elf_file_size) {
    auto* ehdr = reinterpret_cast<Elf64_Ehdr*>(elf_base);
    
    if (!validate_elf_header(ehdr, elf_file_size)) {
        return nullptr;
    }
    
    bool is_pie = (ehdr->e_type == ET_DYN);
    uint64_t load_base = 0;
    
    uint64_t entry_point = is_pie ? (load_base + ehdr->e_entry) : ehdr->e_entry;
    return reinterpret_cast<void*>(entry_point);
}

void execute_elf(proc_address_space* addr_space, void* elf_base, 
                 size_t elf_file_size, bool user_mode,
                 const char* argv[], const char* envp[])
{
    if (!addr_space) {
        Log::errf("Invalid address space");
        return;
    }
    
    void* entry_point = get_entry_point_from_elf(elf_base, elf_file_size);
    if (!entry_point) {
        Log::errf("Failed to get entry point");
        return;
    }
    
    int argc = 0;
    while (argv && argv[argc]) argc++;
    
    int envc = 0;
    while (envp && envp[envc]) envc++;
    
    Log::infof("Executing ELF: entry=%p stack=%p user_mode=%B argc=%d envc=%d", 
               entry_point, addr_space->stack_base, user_mode,
               argc, envc);
    
    uint64_t stack_ptr = reinterpret_cast<uint64_t>(addr_space->stack_base);
    
    uint64_t* envp_user_ptrs = (uint64_t*)mem::heap::malloc(sizeof(uint64_t)*(envc+1));
    for (int i = envc - 1; i >= 0; i--) {
        size_t len = strlen(envp[i]) + 1;
        stack_ptr -= len;
        mem::memcpy(reinterpret_cast<void*>(stack_ptr), envp[i], len);
        envp_user_ptrs[i] = stack_ptr;
    }
    envp_user_ptrs[envc] = 0;
    
    uint64_t* argv_user_ptrs = (uint64_t*)mem::heap::malloc(sizeof(uint64_t)*(argc+1));
    for (int i = argc - 1; i >= 0; i--) {
        size_t len = strlen(argv[i]) + 1;
        stack_ptr -= len;
        mem::memcpy(reinterpret_cast<void*>(stack_ptr), argv[i], len);
        argv_user_ptrs[i] = stack_ptr;
    }
    argv_user_ptrs[argc] = 0;
    
    size_t total_ptr_bytes = 8 + 8 * (argc + 1) + 8 * (envc + 1);
    
    stack_ptr &= ~0xFULL;
    
    if ((stack_ptr - total_ptr_bytes) % 16 != 0) {
        stack_ptr -= 8;
    }
    
    for (int i = envc; i >= 0; i--) {
        stack_ptr -= 8;
        *reinterpret_cast<uint64_t*>(stack_ptr) = envp_user_ptrs[i];
    }
    uint64_t envp_ptr = stack_ptr;
    
    for (int i = argc; i >= 0; i--) {
        stack_ptr -= 8;
        *reinterpret_cast<uint64_t*>(stack_ptr) = argv_user_ptrs[i];
    }
    uint64_t argv_ptr = stack_ptr;
    
    stack_ptr -= 8;
    *reinterpret_cast<uint64_t*>(stack_ptr) = argc;
    
    mem::heap::free(argv_user_ptrs);
    mem::heap::free(envp_user_ptrs);
    
    if (stack_ptr % 16 != 0) {
        Log::errf("Stack misaligned! RSP=%p (should be 16-byte aligned)", 
                  reinterpret_cast<void*>(stack_ptr));
    }
    
    Log::infof("Stack setup complete: RSP=%p argc=%d argv=%p envp=%p", 
               reinterpret_cast<void*>(stack_ptr),
               argc,
               reinterpret_cast<void*>(argv_ptr),
               reinterpret_cast<void*>(envp_ptr));
    
    if (user_mode) {
        arch::x86_64::ringctl::execute_ring3(
            reinterpret_cast<void(*)()>(entry_point),
            reinterpret_cast<uint8_t*>(stack_ptr)
        );
    } else {
        uint64_t saved_rsp;
        asm volatile("mov %%rsp, %0" : "=r"(saved_rsp));
        
        asm volatile(
            "mov %0, %%rsp\n"
            "call *%1\n"
            "mov %2, %%rsp"
            :
            : "r"(stack_ptr),
              "r"(entry_point),
              "r"(saved_rsp)
            : "memory"
        );
    }
}

void run_elf(void* base, size_t filesz, bool user, const char* argv[], const char* envp[]) {
    proc_address_space* addr_space = load_elf_to_address_space(base, filesz, user);
    if (!addr_space) {
        Log::errf("Failed to load ELF");
        return;
    }
    
    execute_elf(addr_space, base, filesz, user, argv, envp);
    
}
