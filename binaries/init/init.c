#include <stdint.h>
#include <stddef.h>
#include <sys/syscalls.h>

int _start(void) {
    Handle* dir = HlCreateNewHandle();
    HlOpenDirectory(dir, "/dev", 0);

    uint8_t buf[1024];
    HlListDirectory(dir, buf);

    uint64_t count = *(uint64_t*)buf;
    char* names = (char*)buf + sizeof(uint64_t);

    for (uint64_t i = 0; i < count; ++i) {
        const char* name = names + (i * (NAME_MAX + 1));
        HlKernelMessage(name);
        HlKernelMessage("\n");
    }

    HlCloseDirectory(dir);
    HlDestroyHandle(dir);

    HlExit(0);
    return 0;
}
