#ifndef PARSE_KARG_HPP
#define PARSE_KARG_HPP 1

#include <cstdint>
#include <cstddef>

struct karg_context {
    void*  base;
    size_t size;

    const char* INIT_PATH_symbol;
    const char* default_init_path;
};

const char* check_init_path(const karg_context* ctx);

#endif