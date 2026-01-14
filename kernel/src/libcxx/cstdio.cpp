#include "cstdio"
#include <config.hpp>
#include <ramfs/ramfs.hpp>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cctype>

struct message {
    const char* text;
    bool available;
};

struct message_entry {
    MESSAGE_ID mid;
    message lang[LANG_COUNT];
};

struct message_table {
    int messages;
    message_entry entries[10];
} table = {0};

static message_entry* get_entry(uint16_t id) {
    for (int i = 0; i < table.messages; i++)
        if (table.entries[i].mid == id)
            return &table.entries[i];

    if (table.messages >= 10) return nullptr;

    message_entry& e = table.entries[table.messages++];
    e.mid = static_cast<MESSAGE_ID>(id);
    for (int l = 0; l < LANG_COUNT; l++)
        e.lang[l].available = false;
    return &e;
}

namespace messages {

bool ok = false;

void print_message(MESSAGE_ID id, LANG lang) {
#ifdef CONFIG_ENABLE_KERNEL_MESSAGES
    if (!ok) {
        printf("message system not initialised\n\r");
        return;
    }

    for (int i = 0; i < table.messages; i++) {
        if (table.entries[i].mid == id) {
            if (table.entries[i].lang[lang].available) {
                printf("%s\n\r", table.entries[i].lang[lang].text);
                return;
            }
            if (table.entries[i].lang[EN_UK].available) {
                printf("%s\n\r", table.entries[i].lang[EN_UK].text);
                return;
            }
        }
    }
    printf("message not found for id %d\n\r", id);
#else
#   warning "It is recommended to enable kernel messages!"
#endif
}

static bool parse_eid(const char*& line, uint16_t& out_id) {
    if (strncmp(line, "EID", 3) != 0) return false;
    line += 3;
    uint32_t value = 0;
    while (*line >= '0' && *line <= '9') {
        value = value * 10 + (*line - '0');
        line++;
    }
    if (*line != ':') return false;
    line++;
    out_id = static_cast<uint16_t>(value);
    return true;
}

static bool parse_lang_line(const char* line, LANG& lang, char*& msg, char* storage, int& storage_off) {
    while (*line && isspace(*line)) line++;

    if (!strncmp(line, "EN_UK", 5)) { lang = EN_UK; line += 5; }
    else if (!strncmp(line, "DE", 2)) { lang = DE; line += 2; }
    else if (!strncmp(line, "FR", 2)) { lang = FR; line += 2; }
    else if (!strncmp(line, "ES", 2)) { lang = ES; line += 2; }
    else if (!strncmp(line, "IT", 2)) { lang = IT; line += 2; }
    else if (!strncmp(line, "PT", 2)) { lang = PT; line += 2; }
    else if (!strncmp(line, "NL", 2)) { lang = NL; line += 2; }
    else if (!strncmp(line, "SV", 2)) { lang = SV; line += 2; }
    else if (!strncmp(line, "NO", 2)) { lang = NO; line += 2; }
    else if (!strncmp(line, "DK", 2)) { lang = DK; line += 2; }
    else if (!strncmp(line, "FI", 2)) { lang = FI; line += 2; }
    else if (!strncmp(line, "IS", 2)) { lang = IS; line += 2; }
    else return false;

    while (*line && isspace(*line)) line++;

    if (*line != '"') return false;
    line++;

    msg = &storage[storage_off];
    while (*line && *line != '"') {
        storage[storage_off++] = *line++;
        if (storage_off >= 1024 - 2) break;
    }
    storage[storage_off++] = '\0';

    if (*line != '"') return false;
    line++;
    return true;
}

void initialise() {
    int fd = ramfs::open("/initrd/messages", O_RDONLY);
    if (fd < 0) {
        printf("no such file or directory\n\r");
        return;
    }

    static char buffer[2048];
    static char storage[1024];
    int storage_off = 0;

    int n = ramfs::read(fd, buffer, sizeof(buffer) - 1);
    if (n <= 0) {
        printf("read failed\n\r");
        return;
    }

    buffer[n] = 0;
    const char* p = buffer;
    const char* end = buffer + n;

    while (p < end) {
        while (p < end && (isspace(*p) || *p == '#')) {
            if (*p == '#') { while (p < end && *p != '\n') p++; }
            else p++;
        }
        if (p >= end) break;

        uint16_t id;
        if (!parse_eid(p, id)) {
            while (p < end && *p != '\n') p++;
            if (*p == '\n') p++;
            continue;
        }

        message_entry* e = get_entry(id);
        if (!e) continue;

        while (p < end && strncmp(p, "EID", 3) != 0) {
            LANG lang;
            char* msg;
            if (parse_lang_line(p, lang, msg, storage, storage_off)) {
                e->lang[lang].text = msg;
                e->lang[lang].available = true;
            }

            while (p < end && *p != '\n') p++;
            if (*p == '\n') p++;
        }
    }

    ok = true;
}

}
