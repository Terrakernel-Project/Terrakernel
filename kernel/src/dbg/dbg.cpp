#include "dbg.hpp"
#include <mem/mem.hpp>
#include <drivers/tty/ldisc/ldisc.hpp>
#include <cstdio>
#include <cinttypes>
#include <Zydis/Zydis.h>
#include <cstring>
#include <limine.h>
#include <exec/elf.hpp>
#include <cctype>
#include <config.hpp>

__attribute__((section(".limine_requests")))
volatile limine_executable_file_request executable_file_request = {
	.id = LIMINE_EXECUTABLE_FILE_REQUEST_ID,
	.revision = 0,
	.response = nullptr // shut up gcc
};

const char* find_symbol(uint64_t addr, uint64_t* offset = nullptr) {
    limine_file* exe = executable_file_request.response->executable_file;
    if (!executable_file_request.response->executable_file) {
        return "??";
    }
    if (!exe) {
        return "??";
    }

    Elf64_Ehdr* hdr = (Elf64_Ehdr*)exe->address;
    Elf64_Shdr* shdr = (Elf64_Shdr*)((uint64_t)exe->address + hdr->e_shoff);

    Elf64_Shdr* shstrtab_hdr = &shdr[hdr->e_shstrndx];
    const char* shstrtab = (const char*)exe->address + shstrtab_hdr->sh_offset;

    Elf64_Shdr* symtab_hdr = nullptr;
    Elf64_Shdr* strtab_hdr = nullptr;

    for (int i = 0; i < hdr->e_shnum; i++) {
        const char* name = shstrtab + shdr[i].sh_name;
        if (strcmp(name, ".symtab") == 0) {
            symtab_hdr = &shdr[i];
        } else if (strcmp(name, ".strtab") == 0) {
            strtab_hdr = &shdr[i];
        }
    }

    if (!symtab_hdr || !strtab_hdr) {
        return "??";
    }

    Elf64_Sym* symtab = (Elf64_Sym*)((uint64_t)exe->address + symtab_hdr->sh_offset);
    const char* strtab = (const char*)exe->address + strtab_hdr->sh_offset;

    int count = symtab_hdr->sh_size / sizeof(Elf64_Sym);

    for (int i = 0; i < count; i++, symtab++) {
        uint64_t start = symtab->st_value;
        uint64_t end   = symtab->st_value + symtab->st_size;

        if (addr >= start && addr < end) {
            if (offset) *offset = addr - start;
            const char* name = strtab + symtab->st_name;
            return name;
        }
    }

    return "??";
}

const char* unmangle_symbol(const char* mangled_name) {
    static char buffer[512];
    
    if (!mangled_name || mangled_name[0] == '\0') {
        return "??";
    }

    static char stripped[256];
    size_t i = 0;
    while (mangled_name[i] && mangled_name[i] != '@' && i < sizeof(stripped) - 1) {
        stripped[i] = mangled_name[i];
        i++;
    }
    stripped[i] = '\0';

    if (stripped[0] != '_' || stripped[1] != 'Z') {
        return stripped;
    }

    size_t pos = 2;
    size_t out = 0;
    bool first_component = true;
    
    while (pos < i && stripped[pos] && out < sizeof(buffer) - 1) {
        if (isdigit(stripped[pos])) {
            int len = 0;
            size_t start = pos;
            
            while (pos < i && isdigit(stripped[pos])) {
                len = len * 10 + (stripped[pos] - '0');
                pos++;
            }
            
            if (!first_component && out > 0) {
                if (out + 2 < sizeof(buffer) - 1) {
                    buffer[out++] = ':';
                    buffer[out++] = ':';
                }
            }
            first_component = false;
            
            for (int j = 0; j < len && pos < i && out < sizeof(buffer) - 1; j++, pos++) {
                buffer[out++] = stripped[pos];
            }
        } else if (stripped[pos] == 'E') {
            pos++;
            break;
        } else {
            pos++;
        }
    }
    
    buffer[out] = '\0';
    
    if (out == 0) {
        return stripped;
    }
    
    return buffer;
}

uint64_t hexstr_to_u64(const char* str) {
    if (!str) return 0;

    while (*str && isspace(*str)) str++;

    if (*str == '0' && (str[1] == 'x' || str[1] == 'X')) str += 2;

    uint64_t result = 0;
    while (*str) {
        char c = *str++;
        uint8_t value = 0;

        if (c >= '0' && c <= '9') value = c - '0';
        else if (c >= 'a' && c <= 'f') value = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') value = c - 'A' + 10;
        else break;

        result = (result << 4) | value;
    }

    return result;
}

namespace dbg {
namespace memview {

void print_memory_contents_at(uint64_t addr, uint64_t len, uint64_t paging_len, uint64_t highlight_addr) {
    if (paging_len == 0 || paging_len % 16 != 0)
        paging_len = 256;

    for (uint64_t a = 0; a < len; a++) {
        if (a % paging_len == 0 && a != 0) {
            printf("Press any key to print the next page\n\r");
            char c;
            drivers::tty::ldisc::read(false, &c, 1);
            printf("\033[2J\033[H");
        }
        
        if (a % paging_len == 0) {
            printf(" ADDR             | 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n\r");
            printf(" -----------------+------------------------------------------------\n\r");
        }
        
        if (a % 16 == 0) {
            if (highlight_addr == (addr + a)) {
                printf(">%016llX | ", (unsigned long long)(addr + a));
            } else {
                printf(" %016llX | ", (unsigned long long)(addr + a));
            }
        }
        
        if (mem::vmm::is_mapped((void*)(addr + a))) {
            printf("%02X ", *(uint8_t*)(addr + a));
        } else {
            printf("?? ");
        }
        
        if ((a + 1) % 16 == 0)
            printf("\n\r");
    }
    
    if (len % 16 != 0)
        printf("\n\r");
}

}

namespace disasm {

void disasm_at_memory(uint64_t addr, uint64_t len, uint64_t paging_len, uint64_t highlight_addr) {
    if (paging_len == 0 || paging_len % 16 != 0)
        paging_len = 256;

    ZyanU8* data = (ZyanU8*)addr;
    ZyanU64 runtime_address = addr;
    ZyanUSize offset = 0;
    ZydisDisassembledInstruction instruction;
    ZyanUSize instructions_printed = 0;

    printf(" ----==== DISASSEMBLY ====---- ------------------------------------\n\r");
    printf("\n\rThis disassembly uses %s syntax\n\r",
#ifdef CONFIG_EXCEPTION_DEBUGGER_SYNTAX
           "Intel"
#else
           "AT&T"
#endif
    );

#ifdef CONFIG_DISASM_STOP_AT_RET
    printf("NOTE! This disassembly will stop at return instructions\n\r");
#endif

    printf("\n\rThis disassembly was generated by Zydis v%d.%d.%d\n\r",
           ZYDIS_VERSION_MAJOR(ZYDIS_VERSION),
           ZYDIS_VERSION_MINOR(ZYDIS_VERSION),
           ZYDIS_VERSION_PATCH(ZYDIS_VERSION));
    
    uint64_t off;
    printf("DISASSEMBLING <%s", unmangle_symbol(find_symbol(addr, &off)));
    if (off != 0)
        printf(" + 0x%llx", (unsigned long long)off);
    printf(">\n\r");
    printf("\n\r");

#ifdef CONFIG_EXCEPTION_DEBUGGER_SYNTAX
    bool use_intel_syntax = true;
#else
    bool use_intel_syntax = false;
#endif

    while (offset < len && use_intel_syntax ? ZYAN_SUCCESS(ZydisDisassembleIntel(
        ZYDIS_MACHINE_MODE_LONG_64,
        runtime_address,
        data + offset,
        len - offset,
        &instruction
    )) : ZYAN_SUCCESS(ZydisDisassembleATT(
        ZYDIS_MACHINE_MODE_LONG_64,
        runtime_address,
        data + offset,
        len - offset,
        &instruction
    ))) {
        if (instructions_printed > 0 && instructions_printed % paging_len == 0) {
            printf("Press any key to continue...\n\r");
            char c;
            drivers::tty::ldisc::read(false, &c, 1);
            printf("\033[2J\033[H");
        }

        if (highlight_addr >= runtime_address &&
            highlight_addr < runtime_address + instruction.info.length) {
            printf("\x1B[93m>\x1B[0m");
        } else {
            printf(" ");
        }
        printf("%016llX  %s", (unsigned long long)runtime_address, instruction.text); /* no crlf because we will need that later */

#ifdef CONFIG_DISASM_STOP_AT_RET
        if (strncmp(instruction.text, "ret", 3) == 0 ||
            strncmp(instruction.text, "iret", 4) == 0 ||
            strncmp(instruction.text, "iretq", 5) == 0 ||
            strncmp(instruction.text, "sysret", 6) == 0 ||
            strncmp(instruction.text, "sysret", 6) == 0) {
            printf("    <== RETURN INSTRUCTION - STOPPING DISASSEMBLY HERE\n\r");
            break;
        }
#endif

        if (instruction.text[0] == 'j' || instruction.text[0] == 'J' ||
			strncmp(instruction.text, "call", 4) == 0 || strncmp(instruction.text, "CALL", 4) == 0
        ) {
        	uint64_t offset = 0;
        	uint64_t target = hexstr_to_u64(strchr(instruction.text, ' ') + 1);
            const char* func = unmangle_symbol(find_symbol(target, &offset));
                    
            if (offset != 0)
                printf("\t\t <%s + 0x%llx>\n\r", func, (unsigned long long)offset);
            else
                printf("\t\t <%s>\n\r", func);
        } else {
        	printf("\n\r");
        }
        
        offset += instruction.info.length;
        runtime_address += instruction.info.length;
        instructions_printed++;
    }
}

}

namespace stacktrace {

static inline bool is_canonical(uint64_t addr) {
    uint64_t top = addr >> 47;
    return top == 0 || top == 0x1FFFF;
}

static inline bool is_mapped(uint64_t addr) {
    return mem::vmm::va_to_pa(addr) != (uint64_t)-1;
}

static bool safe_read64(uint64_t addr, uint64_t& out) {
    if (!addr || (addr & 7))         return false;
    if (!is_canonical(addr))         return false;
    if (!is_mapped(addr))            return false;
    out = *reinterpret_cast<uint64_t*>(addr);
    return true;
}

void stacktrace(uint64_t rbp, uint64_t max_frames, uint64_t page_size) {
    if (page_size == 0 || page_size % 16 != 0) page_size = 16;

    printf(" ----==== STACKTRACE ====----\n\r");
    printf("%-4s %-32s %-18s %s\n\r", "#", "FUNCTION", "RIP", "OFFSET");
    printf("------------------------------------------------------------\n\r");

    int frame = 0;

    while (frame < (int)max_frames) {
        if (!rbp) break;

        if (!is_canonical(rbp)) {
            printf("#%-3d <non-canonical rbp=0x%016llX — chain broken>\n\r",
                   frame, (unsigned long long)rbp);
            break;
        }

        if (!is_mapped(rbp) || !is_mapped(rbp + 8)) {
            printf("#%-3d <unmapped frame at rbp=0x%016llX — chain broken>\n\r",
                   frame, (unsigned long long)rbp);
            break;
        }

        uint64_t ret_addr = 0, next_rbp = 0;
        if (!safe_read64(rbp + 8, ret_addr) || !safe_read64(rbp, next_rbp)) {
            printf("#%-3d <read fault at rbp=0x%016llX — chain broken>\n\r",
                   frame, (unsigned long long)rbp);
            break;
        }

        if (!ret_addr || !is_canonical(ret_addr)) {
            if (ret_addr)
                printf("#%-3d <non-canonical ret=0x%016llX — base or broken frame>\n\r",
                       frame, (unsigned long long)ret_addr);
            break;
        }

        uint64_t offset = 0;
        const char* func = find_symbol(ret_addr, &offset);
        printf("#%-3d %-32s 0x%016llX +0x%llX\n\r",
               frame,
               func ? func : "???",
               (unsigned long long)ret_addr,
               (unsigned long long)offset);

        rbp = next_rbp;
        frame++;

        if (page_size && frame > 0 && frame % (int)page_size == 0) {
            printf("-- press any key for more --\n\r");
            char c;
            drivers::tty::ldisc::read(false, &c, 1);
            printf("\033[2J\033[H");
        }
    }

    if (frame == 0)
        printf("ERROR: could not unwind at all — bad rbp or no frame at entry point\n\r");
    else
        printf("---- %d frame(s) ----\n\r", frame);
}

}

}
