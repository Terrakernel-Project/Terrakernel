#include "parse_karg.hpp"
#include <mem/mem.hpp>
#include <cstring>
#include <cstdio>

static char init_path_buf[256];

const char* check_init_path(const karg_context* ctx) {
	const char* base = static_cast<const char*>(ctx->base);
	Log::infof("Base of karg-s is %p", base);
	const char* sym = ctx->INIT_PATH_symbol;
	Log::infof("Base of INIT symbol is %p", sym);
	size_t sym_len = strlen(sym);
	Log::infof("Length of INIT symbol is %zu", sym_len);

	const char* found = strstr(base, sym);
	Log::infof("Found INIT symbol at %p (%s)", found, found ? found : "ramfs:/initrd/init");
	if (!found || found[sym_len] != '=') {
		return ctx->default_init_path ? ctx->default_init_path : "ramfs:/initrd/init";
	}

	const char* val = found + sym_len + 1;
	size_t len = 0;
	while (val[len] && val[len] != ' ' && val[len] != '\t' && len < sizeof(init_path_buf) - 1) {
		len++;
	}

	Log::infof("Copying symbol to buffer");
	mem::memcpy(init_path_buf, val, len);
	init_path_buf[len] = '\0';

	return init_path_buf;
}
