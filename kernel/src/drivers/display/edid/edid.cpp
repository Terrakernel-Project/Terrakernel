#include "edid.hpp"
#include <lib/Flanterm/gfx.h>
#include <limine.h>
#include <mem/mem.hpp>
#include <cstdio>

limine_framebuffer* fb = nullptr;

struct edid_record {
    uint64_t padding;
    uint16_t manufacture_id;
    uint16_t edid_id_code;
    uint32_t serial_number;
    uint8_t manufacture_week;
    uint8_t manufacture_year;
    uint8_t edid_version;
    uint8_t edid_revision;
    uint8_t video_input_type;
    uint8_t max_horiontal_size_cm;
    uint8_t max_vertical_size_cm;
    uint8_t gama_factor;
    uint8_t dpms_flags;
    uint8_t chroma_information[10];
    uint8_t established_timings_eins;
    uint8_t established_timings_zwei;
    uint8_t manufactures_reserved_timings;
    uint16_t standard_timing_identification[8];
    uint8_t detailed_timing_description_eins[18];
    uint8_t detailed_timing_description_zwei[18];
    uint8_t detailed_timing_description_drei[18];
    uint8_t detailed_timing_description_vier[18];
    uint8_t unused;
    uint8_t checksum;
};

enum video_input_type {
    SEPERATE_SYNC = 0,
    COMPOSITE_SYNC = 1,
    SYNC_ON_GREEN = 2,
    VOLTAGE_LEVEL_5 = 5,
    VOLTAGE_LEVEL_6 = 6,
    DIGITAL_SIGNAL = 7
};

#define DPMS_FLAGS_UNUSED_0 0
#define DPMS_FLAGS_UNUSED_1 1
#define DPMS_FLAGS_UNUSED_2 2

#define DPMS_FLAGS_DISPLAY_TYPE 3
#define DPMS_FLAGS_DISPLY_TYPE_RGB 1

#define DPMS_ACTIVE_OFF_SUPPORTED 5
#define DPMS_FLAGS_SUSPEND_SUPPORTED 6
#define DPMS_FLAGS_STANDBY_SUPPORTED 7

#define CHROMA_INFO_GREEN_XY_AND_RED_XY 0
#define CHROMA_INFO_WHITE_XY_AND_BLUE_XY 1
#define CHROMA_INFO_RED_Y 2
#define CHROMA_INFO_RED_X 3
#define CHROMA_INFO_GREEN_Y 4
#define CHROMA_INFO_GREEN_X 5
#define CHROMA_INFO_BLUE_Y 6
#define CHROMA_INFO_BLUE_X 7
#define CHROMA_INFO_WHITE_Y 8
#define CHROMA_INFO_WHITE_X 9

#define TIMING_H_FREQ_kHz 0
#define TIMING_V_FREQ_Hz 1
#define TIMING_H_ACTIVE_TIME 2
#define TIMING_H_BLANKING_TIME 3
#define TIMING_H_ACTIVE_TIME_H_BLANKING_TIME 4
#define TIMING_V_ACTIVE_TIME 5
#define TIMING_V_BLANKING_TIME 6
#define TIMING_V_ACTIVE_TIME_V_BLANKING_TIME 7
#define TIMING_H_SYNC_OFFSET 8
#define TIMING_H_SYNC_PULSE_WIDTH 9
#define TIMING_V_SYNC_OFFSET_V_SYNC_PULSE_WIDTH 10
#define TIMING_VH_SYNC_OFFSET_PULSE_WIDTH 11
#define TIMING_H_IMAGE_SIZE_MM 12
#define TIMING_V_IMAGE_SIZE_MM 13
#define TIMING_H_IMAGE_SIZE_V_IMAGE_SIZE 14
#define TIMING_H_BORDER 15
#define TIMING_V_BORDER 16
#define TIMING_TYPE_OF_DISPLAY 17

// ToD = Type of Display
#define ToD_INTERLACED 0b10000000

#define ToD_STEREO_MODE 0b01100000
#define ToD_STEREO_MODE_NONE 0b00
#define ToD_STEREO_MODE_RT_STEREO_SYNC_HI 0b01
#define ToD_STEREO_MODE_LT_STEREO_SYNC_HI 0b10

#define ToD_SYNC_TYPE 0b00011000
#define ToD_SYNC_TYPE_ANALOG_COMPOSITE 0b00
#define ToD_SYNC_TYPE_BIPOLAR_ANALOG_COMPOSITE 0b01
#define ToD_SYNC_TYPE_DIGITAL_COMPOSITE 0b10
#define ToD_SYNC_TYPE_DIGITAL_SEPERATE 0b11

struct video_mode {
    uint64_t pitch;
    uint64_t width, height;
    uint64_t bpp;

    video_mode* next_mode;
};

namespace drivers::display::edid {

edid_record* record;

video_mode* modes;
video_mode* last_mode;

void add_video_mode(limine_video_mode* mode) {
    if (!modes) {
        modes = (video_mode*)mem::heap::malloc(sizeof(video_mode));
        modes->pitch = mode->pitch;
        modes->width = mode->width;
        modes->height = mode->height;
        modes->bpp = mode->bpp;

        last_mode = modes;
    } else {
        last_mode->next_mode = (video_mode*)mem::heap::malloc(sizeof(video_mode));

        last_mode = last_mode->next_mode;
        
        last_mode->pitch = mode->pitch;
        last_mode->width = mode->width;
        last_mode->height = mode->height;
        last_mode->bpp = mode->bpp;
    }
}

void initialise() {
    fb = get_fb();

    record = (edid_record*)fb->edid;

    for (uint64_t i = 0; i < fb->mode_count; i++) {
        add_video_mode(fb->modes[i]);
        Log::infof("Discovered %zux%zux%zu mode\n\r", fb->modes[i]->width, fb->modes[i]->height, fb->modes[i]->bpp);
    }

    Log::warnf("EDID purely discovers modes and is not able to change video modes\n\r");
}

}