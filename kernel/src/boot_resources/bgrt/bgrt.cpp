#include "bgrt.hpp"
#include <uacpi/uacpi.h>
#include <uacpi/tables.h>
#include <uacpi/types.h>
#include <lib/Flanterm/gfx.h>
#include <cstdint>
#include <cstdio>
#include <ramfs/ramfs.hpp>
#include <mem/mem.hpp>

extern uint64_t g_scr_width, g_scr_height;

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
    uint8_t version;
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
    
    if (bgrt) {
        Log::infof("BGRT found: version=%u status=0x%02x type=%u addr=%p offset=(%u,%u)",
                   bgrt->version, bgrt->status, bgrt->image_type,
                   bgrt->image_address, bgrt->image_offset_x, bgrt->image_offset_y);
    }
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

static bool validate_bgrt() {
    if (!bgrt || !bgrt->image_address)
        return false;
    if (bgrt->status & 0x02)
        return false;
    return true;
}

static bool parse_bmp_headers(uint8_t* image, bmp_file_header** out_fh, 
                             bmp_info_header** out_ih) {
    bmp_file_header fh_local;
    mem::memcpy(&fh_local, image, sizeof(fh_local));

    if (fh_local.type != 0x4D42) {
        Log::warnf("BGRT image is not BMP (magic=0x%04x)", fh_local.type);
        return false;
    }

    bmp_info_header ih_local;
    mem::memcpy(&ih_local, image + sizeof(bmp_file_header), sizeof(ih_local));

    if (ih_local.bpp != 24 && ih_local.bpp != 32) {
        Log::warnf("Unsupported BMP bpp: %u", ih_local.bpp);
        return false;
    }

    if (ih_local.compression != 0) {
        Log::warnf("Compressed BMP not supported (compression=%u)", ih_local.compression);
        return false;
    }

    *out_fh = (bmp_file_header*)image;
    *out_ih = (bmp_info_header*)(image + sizeof(bmp_file_header));
    return true;
}

static void draw_bmp(uint8_t* image, uint32_t x_offset, uint32_t y_offset, bool clear = false) {
    bmp_file_header* fh;
    bmp_info_header* ih;
    if (!parse_bmp_headers(image, &fh, &ih))
        return;

    int width = ih->width;
    int height = ih->height;
    bool top_down = false;
    if (height < 0) {
        height = -height;
        top_down = true;
    }

    uint8_t* pixels = image + fh->off_bits;
    uint32_t bytes_per_pixel = ih->bpp / 8;
    uint32_t stride = ((width * bytes_per_pixel) + 3) & ~3;

    for (int y = 0; y < height; y++) {
        int src_y = top_down ? y : (height - 1 - y);
        uint8_t* row = pixels + src_y * stride;
        for (int x = 0; x < width; x++) {
            uint8_t* px = row + x * bytes_per_pixel;
            uint32_t color = clear ? 0 : argb((ih->bpp == 32) ? px[3] : 0xFF, px[2], px[1], px[0]);
            putpx(x_offset + x, y_offset + y, color);
        }
    }
}

void display_bgrt() {
    if (!validate_bgrt()) {
        display_tk_no_bgrt();
        return;
    }
    draw_bmp((uint8_t*)(uintptr_t)bgrt->image_address, bgrt->image_offset_x, bgrt->image_offset_y);
}

void display_tk_no_bgrt() {
    int fd = ramfs::open("/initrd/tk_no_bgrt_256x256.bmp", O_RDONLY);
    if (fd < 0) return;

    stat s;
    ramfs::fstat(fd, &s);
    if (s.st_size < sizeof(bmp_file_header) + sizeof(bmp_info_header)) {
        ramfs::close(fd);
        return;
    }

    uint8_t* image = (uint8_t*)mem::heap::malloc(s.st_size);
    size_t read_bytes = ramfs::read(fd, image, s.st_size);
    ramfs::close(fd);
    if (read_bytes < sizeof(bmp_file_header) + sizeof(bmp_info_header)) {
        mem::heap::free(image);
        return;
    }

    bmp_file_header fh_local;
    mem::memcpy(&fh_local, image, sizeof(fh_local));
    if (fh_local.type != 0x4D42) {
        Log::warnf("tk_no_bgrt BMP is invalid (magic=0x%04x)", fh_local.type);
        mem::heap::free(image);
        return;
    }

    bmp_info_header ih_local;
    mem::memcpy(&ih_local, image + sizeof(bmp_file_header), sizeof(ih_local));

    bmp_file_header* fh = (bmp_file_header*)image;
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

    uint64_t start_x = (g_scr_width / 2) - (width / 2);
    uint64_t start_y = (g_scr_height / 2) - (height / 2);

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
            putpx(start_x + x, start_y + y, color);
        }
    }

    mem::heap::free(image);
}

void clear_bgrt() {
    if (!validate_bgrt()) {
        display_tk_no_bgrt();
        return;
    }
    draw_bmp((uint8_t*)(uintptr_t)bgrt->image_address, bgrt->image_offset_x, bgrt->image_offset_y, true);
}

void clear_tk_no_bgrt() {
    int fd = ramfs::open("/initrd/tk_no_bgrt_256x256.bmp", O_RDONLY);
    if (fd < 0) return;

    stat s;
    ramfs::fstat(fd, &s);
    uint8_t* image = (uint8_t*)mem::heap::malloc(s.st_size);
    ramfs::read(fd, image, s.st_size);
    ramfs::close(fd);

    uint64_t start_x = (g_scr_width / 2) - 128;
    uint64_t start_y = (g_scr_height / 2) - 128;
    draw_bmp(image, start_x, start_y, true);

    mem::heap::free(image);
}

}
