#include <sys/syscalls.h>

static inline void print(const char* msg) {
    size_t len = 0;
    while (msg[len] != '\0') len++;
    sys_write(1, msg, len);
}

int main() {
    print("Terrakernel syscall test!\n");

    pid_t pid = sys_getpid();
    char pid_buf[32];
    int i = 0;
    if (pid == 0) {
        pid_buf[i++] = '0';
    } else {
        unsigned int tmp = pid;
        char tmp_buf[16];
        int j = 0;
        while (tmp > 0) {
            tmp_buf[j++] = '0' + (tmp % 10);
            tmp /= 10;
        }
        while (j-- > 0) {
            pid_buf[i++] = tmp_buf[j];
        }
    }
    pid_buf[i++] = '\n';
    sys_write(1, pid_buf, i);

    struct timespec t;
    t.tv_sec = 1;
    t.tv_nsec = 0;
    print("Sleeping 1 second...\n");
    sys_nanosleep(&t, (void*)0);
    print("Done sleeping!\n");

    sys_exit(0);

    return 0;
}
