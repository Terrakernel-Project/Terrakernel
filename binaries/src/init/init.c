#include <stdint.h>
#include <stddef.h>
#include <sys/syscalls.h>

Handle* hndconw;
Handle* hndconr;
Handle* hndfb;
HlFb* fb;
volatile uint32_t* fbbase;

static inline void print(const char* msg) {
    size_t len = 0;
    while (msg[len] != 0) len++;
    HlWriteConsole(hndconw, msg, len);
}

void ppx(int x, int y, uint32_t colour) {
    if (!fb || !fbbase) return;
    if (x < 0 || y < 0) return;
    if ((uint32_t)x >= fb->Width || (uint32_t)y >= fb->Height) return;
    fbbase[y * (fb->Pitch / (fb->BitsPerPixel / 8)) + x] = colour;
}

void HlMain(void) {
    hndconw = HlCreateNewHandle();
    hndconr = HlCreateNewHandle();
    hndfb   = HlCreateNewHandle();

    HlOpenConsole(hndconr, hndconw);
    print("Hello, World! from a userspace init process\r\n");

	HlSleepMs(2500);

	print("Sleeped\r\n");

    fb = (HlFb*)HlMemoryPoolAllocate(sizeof(HlFb));
    if (!fb) {
        print("ERROR: Failed to allocate framebuffer descriptor\r\n");
        while (1);
    }

    HlObtainFramebuffer(hndfb);
    HlStatFramebuffer(hndfb, fb);

    fbbase = (volatile uint32_t*)fb->BaseAddress;
    if (!fbbase) {
        print("ERROR: Framebuffer base address is NULL\r\n");
        while (1);
    }

    for (int i = 0; i < 100; i++) {
    	HlSleepMs(10000);
        ppx(i, i, 0xFFFFFF);
    }

    while (1);
}
