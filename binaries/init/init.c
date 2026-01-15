#include <stdint.h>
#include <stddef.h>
#include <sys/syscalls.h>

Handle* ConR, *ConW;

void list_dir(const char* __restrict path, int lvl) {
    Handle* dir = HlCreateNewHandle();
    HlOpenDirectory(dir, path, 0);

    uint8_t buf[1024];
    HlListDirectory(dir, buf);

    uint64_t count = *(uint64_t*)buf;
    char* names = (char*)buf + sizeof(uint64_t);

    for (uint64_t i = 0; i < count; ++i) {
        const char* name = names + (i * (NAME_MAX + 1));

        uint64_t name_len;
        while (name[name_len] != '\0') {
            name_len++;
        }

        for (int j = 0; j < lvl; ++j) {
            HlWriteConsole(ConW, "  ", 2); // Indentation for hierarchy
        }

        HlWriteConsole(ConW, name, name_len);
        list_dir(name, lvl + 1); // Recursive call to list subdirectory
    }

    HlCloseDirectory(dir);
    HlDestroyHandle(dir);
}

int _start() {
    ConR = HlCreateNewHandle();
    ConW = HlCreateNewHandle();
    HlOpenConsole(ConR, ConW);

    HlWriteConsole(ConW, "Listing root directory:\n", 24);
    
    list_dir("/", 0);

    HlExit(0);
    return 0;
}
