#include "bgrt.hpp"
#include <uacpi/uacpi.h>
#include <uacpi/tables.h>
#include <uacpi/types.h>
#include <lib/Flanterm/gfx.h>
#include <cstdint>
#include <cstdio>

namespace boot_resources::bgrt {

struct bgrt_table {
    struct acpi_sdt_hdr {
        char signature[4];
        uint32_t length;
        uint8_t revision;
        uint8_t checksum;
        char oemid[6];
        char oem_table_id[8];
        uint32_t oem_revision;
        uint32_t creator_id;
        uint32_t creator_revision;
    } header;
    uint16_t version;
    uint8_t status;
    uint8_t image_type;
    volatile uint32_t* image_address;
    uint32_t image_offset_x;
    uint32_t image_offset_y;
} __attribute__((packed));

uacpi_table bgrt_handle;
bgrt_table* bgrt = nullptr;

void initialise() {
    uacpi_status status = uacpi_table_find_by_signature("BGRT", &bgrt_handle);
    if (uacpi_unlikely_error(status))
        return;
    bgrt = (bgrt_table*)bgrt_handle.ptr;
}

#pragma pack(push,1)
struct bmp_file_header {
    uint16_t type;
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t off_bits;
};

struct bmp_info_header {
    uint32_t size;
    int32_t  width;
    int32_t  height;
    uint16_t planes;
    uint16_t bpp;
    uint32_t compression;
    uint32_t image_size;
    int32_t  xppm;
    int32_t  yppm;
    uint32_t clr_used;
    uint32_t clr_important;
};
#pragma pack(pop)

static inline uint32_t argb(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    return (a << 24) | (r << 16) | (g << 8) | b;
}

void display_bgrt() {
    if (!bgrt) {
        Log::warnf("BGRT unavailable");
        return;
    }

    if (!(bgrt->status & 1)) {
        Log::warnf("Invalid BGRT resource");
        return;
    }

    uint8_t* image = (uint8_t*)(uintptr_t)bgrt->image_address;

    bmp_file_header* fh = (bmp_file_header*)image;
    if (fh->type != 0x4D42) { // "BM"
        Log::warnf("BGRT image is not BMP");
        return;
    }

    bmp_info_header* ih = (bmp_info_header*)(image + sizeof(bmp_file_header));

    int width  = ih->width;
    int height = ih->height;
    bool top_down = false;

    if (height < 0) {
        height = -height;
        top_down = true;
    }

    uint8_t* pixels = image + fh->off_bits;
    uint32_t bytes_per_pixel = ih->bpp / 8;

    uint32_t stride = ((width * bytes_per_pixel) + 3) & ~3;

    if (ih->bpp != 24 && ih->bpp != 32) {
        Log::warnf("Unsupported BMP bpp: %u", ih->bpp);
        return;
    }

    for (int y = 0; y < height; y++) {
        int src_y = top_down ? y : (height - 1 - y);
        uint8_t* row = pixels + src_y * stride;

        for (int x = 0; x < width; x++) {
            uint8_t* px = row + x * bytes_per_pixel;

            uint8_t b = px[0];
            uint8_t g = px[1];
            uint8_t r = px[2];
            uint8_t a = (ih->bpp == 32) ? px[3] : 0xFF;

            uint32_t color = argb(a, r, g, b);

            putpx(
                bgrt->image_offset_x + x,
                bgrt->image_offset_y + y,
                color
            );
        }
    }
}

void clear_bgrt() {
    if (!bgrt) {
        Log::warnf("BGRT unavailable");
        return;
    }

    if (!(bgrt->status & 1)) {
        Log::warnf("Invalid BGRT resource");
        return;
    }

    uint8_t* image = (uint8_t*)(uintptr_t)bgrt->image_address;

    bmp_file_header* fh = (bmp_file_header*)image;
    if (fh->type != 0x4D42) { // "BM"
        Log::warnf("BGRT image is not BMP");
        return;
    }

    bmp_info_header* ih = (bmp_info_header*)(image + sizeof(bmp_file_header));

    int width  = ih->width;
    int height = ih->height;
    bool top_down = false;

    if (height < 0) {
        height = -height;
        top_down = true;
    }

    uint8_t* pixels = image + fh->off_bits;
    uint32_t bytes_per_pixel = ih->bpp / 8;

    uint32_t stride = ((width * bytes_per_pixel) + 3) & ~3;

    if (ih->bpp != 24 && ih->bpp != 32) {
        Log::warnf("Unsupported BMP bpp: %u", ih->bpp);
        return;
    }

    for (int y = 0; y < height; y++) {
        int src_y = top_down ? y : (height - 1 - y);
        uint8_t* row = pixels + src_y * stride;

        for (int x = 0; x < width; x++) {
            uint8_t* px = row + x * bytes_per_pixel;

            uint8_t b = px[0];
            uint8_t g = px[1];
            uint8_t r = px[2];
            uint8_t a = (ih->bpp == 32) ? px[3] : 0xFF;

            uint32_t color = argb(a, r, g, b);

            putpx(
                bgrt->image_offset_x + x,
                bgrt->image_offset_y + y,
                0x000000
            );
        }
    }
}

} 
