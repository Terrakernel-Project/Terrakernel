#include <stdint.h>
#include <stddef.h>
#include <sys/syscalls.h>

Handle *ConR, *ConW;

static int is_dot_or_dotdot(const char *name) {
    return name[0] == '.' &&
          (name[1] == '\0' ||
          (name[1] == '.' && name[2] == '\0'));
}

void list_dir(const char * __restrict path, int lvl) {
    Handle *dir = HlCreateNewHandle();
    if (!dir)
        return;

    HlOpenDirectory(dir, path, 0);

    uint8_t buf[1024]; // because it must be 1KiB according to HlApi
    HlListDirectory(dir, buf);

    uint64_t count = *(uint64_t *)buf;
    char *names = (char *)buf + sizeof(uint64_t);

    for (uint64_t i = 0; i < count; ++i) {
        const char *name = names + i * (NAME_MAX + 1);

        if (is_dot_or_dotdot(name))
            continue;

        uint64_t len = 0;
        while (len < NAME_MAX && name[len])
            len++;

        for (int j = 0; j < lvl; ++j)
            HlWriteConsole(ConW, "  ", 2);

        HlWriteConsole(ConW, name, len);
        HlWriteConsole(ConW, "\n", 1);

        char fullpath[PATH_MAX];
        size_t p = 0;

        while (path[p] && p < PATH_MAX - 1)
            fullpath[p] = path[p], p++;

        if (p > 1 && fullpath[p - 1] != '/')
            fullpath[p++] = '/';

        for (size_t k = 0; k < len && p < PATH_MAX - 1; ++k)
            fullpath[p++] = name[k];

        fullpath[p] = '\0';

        list_dir(fullpath, lvl + 1);
    }

    HlCloseDirectory(dir);
    HlDestroyHandle(dir);
}

int _start(void) {
    ConR = HlCreateNewHandle();
    ConW = HlCreateNewHandle();

    HlOpenConsole(ConR, ConW);

    HlWriteConsole(ConW, "Listing root directory:\n", 24);
    list_dir("/", 0);

	char buf[4096];
	while (1) {
		HlWriteConsole(ConW, "> ", 2);
		int64_t read = HlReadConsole(ConR, buf, 4096);
        HlWriteConsole(ConW, buf, read);
	}

    __builtin_unreachable();
}
