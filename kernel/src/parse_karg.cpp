#include "parse_karg.hpp"
#include <cstring>

const char* check_init_path(const karg_context* ctx) {
    const char* base = static_cast<const char*>(ctx->base);
    const char* sym  = ctx->INIT_PATH_symbol;
    size_t sym_len   = strlen(sym);

    const char* found = strstr(base, sym);
    if (!found) {
        return ctx->default_init_path
            ? ctx->default_init_path
            : "/initrd/init";
    }

    // Expect: INIT_PATH=...
    if (found[sym_len] != '=') {
        return ctx->default_init_path
            ? ctx->default_init_path
            : "/initrd/init";
    }

    // Return pointer to path after '='
    return found + sym_len + 1;
}
