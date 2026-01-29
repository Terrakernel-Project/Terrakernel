#include <lib/Flanterm/gfx.h>
#include <cstdint>

namespace boot_resources::loading {

static inline void draw_circle_point(
    uint32_t cx, uint32_t cy,
    int32_t x, int32_t y,
    uint32_t colour
) {
    for (int ty = -2; ty <= 2; ty++) {
        for (int tx = -2; tx <= 2; tx++) {
            if (tx * tx + ty * ty <= 6) {
                putpx(cx + x + tx, cy + y + ty, colour);
            }
        }
    }
}

static void draw_full_circle(
    uint32_t cx, uint32_t cy,
    uint32_t radius,
    uint32_t colour
) {
    int x = radius;
    int y = 0;
    int d = 1 - radius;

    while (x >= y) {
        draw_circle_point(cx, cy,  x,  y, colour);
        draw_circle_point(cx, cy,  y,  x, colour);
        draw_circle_point(cx, cy, -y,  x, colour);
        draw_circle_point(cx, cy, -x,  y, colour);
        draw_circle_point(cx, cy, -x, -y, colour);
        draw_circle_point(cx, cy, -y, -x, colour);
        draw_circle_point(cx, cy,  y, -x, colour);
        draw_circle_point(cx, cy,  x, -y, colour);

        y++;
        if (d <= 0) {
            d += 2 * y + 1;
        } else {
            x--;
            d += 2 * (y - x) + 1;
        }
    }
}

static inline int normalize_angle(int a) {
    a %= 360;
    if (a < 0) a += 360;
    return a;
}

static inline int point_angle_deg(int x, int y) {
    int ax = x < 0 ? -x : x;
    int ay = y < 0 ? -y : y;

    int base = (ay * 45) / (ax + ay + 1);

    if (x >= 0 && y >= 0) return base;
    if (x < 0  && y >= 0) return 180 - base;
    if (x < 0  && y < 0)  return 180 + base;
    return 360 - base;
}

static void draw_arc(
    uint32_t cx, uint32_t cy,
    uint32_t radius,
    int center_angle,
    uint32_t colour
) {
    int x = radius;
    int y = 0;
    int d = 1 - radius;

    uint8_t br = (colour >> 16) & 0xFF;
    uint8_t bg = (colour >> 8)  & 0xFF;
    uint8_t bb = colour & 0xFF;

    while (x >= y) {
        int xs[8] = { x,  y, -y, -x, -x, -y,  y,  x };
        int ys[8] = { y,  x,  x,  y, -y, -x, -x, -y };

        for (int i = 0; i < 8; i++) {
            int ang = point_angle_deg(xs[i], ys[i]);
            int diff = ang - center_angle;
            if (diff < 0) diff = -diff;
            if (diff > 180) diff = 360 - diff;

            if (diff <= 30) {
                int fade = 255 - (diff * 220) / 30;
                uint32_t c =
                    ((br * fade / 255) << 16) |
                    ((bg * fade / 255) << 8)  |
                    (bb * fade / 255);

                draw_circle_point(cx, cy, xs[i], ys[i], c);
            }
        }

        y++;
        if (d <= 0) {
            d += 2 * y + 1;
        } else {
            x--;
            d += 2 * (y - x) + 1;
        }
    }
}

void loading_circle(
    uint32_t centre_x,
    uint32_t centre_y,
    uint32_t radius,
    uint32_t colour,
    uint32_t angle
) {
    angle = normalize_angle(angle);

    uint8_t r = (colour >> 16) & 0xFF;
    uint8_t g = (colour >> 8)  & 0xFF;
    uint8_t b = colour & 0xFF;

    uint32_t dim =
        ((r / 4) << 16) |
        ((g / 4) << 8)  |
        (b / 4);

    draw_full_circle(centre_x, centre_y, radius, dim);
    draw_arc(centre_x, centre_y, radius, angle, colour);
}

}
