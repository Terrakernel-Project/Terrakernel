#ifndef CURSOR_IMAGE_HPP
#define CURSOR_IMAGE_HPP 1

#include <cstdint>

#define CURSOR_WIDTH 32
#define CURSOR_HEIGHT 32

#define PxW 0xFFFFFFFF
#define PxB 0xFF000000
#define PxE 0 // empty

const uint32_t cursor_image[CURSOR_WIDTH][CURSOR_HEIGHT] = {
    {PxB},
    {PxB,PxB},
    {PxB,PxW,PxB},
    {PxB,PxW,PxW,PxB},
    {PxB,PxW,PxW,PxW,PxB},
    {PxB,PxW,PxW,PxW,PxW,PxB},
    {PxB,PxW,PxW,PxW,PxW,PxW,PxB},
    {PxB,PxW,PxW,PxW,PxW,PxW,PxW,PxB},
    {PxB,PxW,PxW,PxW,PxW,PxW,PxW,PxW,PxB},
    {PxB,PxW,PxW,PxW,PxW,PxW,PxW,PxW,PxW,PxB},
    {PxB,PxW,PxW,PxW,PxW,PxW,PxW,PxW,PxW,PxW,PxB},
    {PxB,PxW,PxW,PxW,PxW,PxW,PxW,PxB,PxB,PxB,PxB,PxB},
    {PxB,PxW,PxW,PxW,PxB,PxW,PxW,PxB},
    {PxB,PxW,PxW,PxB,PxB,PxW,PxW,PxB},
    {PxB,PxW,PxB,PxE,PxE,PxB,PxW,PxW,PxB},
    {PxB,PxB,PxE,PxE,PxE,PxB,PxW,PxW,PxB},
    {PxB,PxE,PxE,PxE,PxE,PxE,PxB,PxW,PxW,PxB},
    {PxE,PxE,PxE,PxE,PxE,PxE,PxB,PxW,PxW,PxB},
    {PxE,PxE,PxE,PxE,PxE,PxE,PxE,PxB,PxW,PxW,PxB},
    {PxE,PxE,PxE,PxE,PxE,PxE,PxE,PxB,PxW,PxW,PxB},
    {PxE,PxE,PxE,PxE,PxE,PxE,PxE,PxE,PxB,PxB,PxW},
};

#endif
