#include <stdint.h>
#include <stddef.h>
#include <sys/syscalls.h>

Handle *ConR, *ConW;

/* =========================
   Minimal libc helpers
   ========================= */

size_t __strlen(const char* s) {
    size_t n = 0;
    while (*s++) n++;
    return n;
}

void __memset(void* ptr, int value, size_t n) {
    unsigned char* p = (unsigned char*)ptr;
    while (n--) *p++ = (unsigned char)value;
}

int __strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) { a++; b++; }
    return *(unsigned char*)a - *(unsigned char*)b;
}

int __strncmp(const char* a, const char* b, size_t n) {
    while (n-- && *a && (*a == *b)) { a++; b++; }
    return n == (size_t)-1 ? 0 : *(unsigned char*)a - *(unsigned char*)b;
}

char __tolower(char c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    return c;
}

int __strcasecmp(const char* a, const char* b) {
    while (*a && *b) {
        char c1 = __tolower(*a);
        char c2 = __tolower(*b);
        if (c1 != c2) return c1 - c2;
        a++; b++;
    }
    return __tolower(*a) - __tolower(*b);
}

#define print(s) HlWriteConsole(ConW, s, __strlen(s))

/* =========================
   Config parsing helpers
   ========================= */

/* Fixed: read full quoted string */
void extract_quoted_value(const char* line, char* dest, size_t max) {
    const char* p = line;
    while (*p == ' ' || *p == '\t') p++;

    if (*p != '"') { dest[0] = '\0'; return; }
    p++; // skip opening quote

    size_t i = 0;
    while (*p && *p != '"' && i < max - 1) {
        dest[i++] = *p++;
    }
    dest[i] = '\0';
}

/* Generic placeholder replacement */
void replace_placeholder(const char* src, const char* ph, const char* val, char* out) {
    size_t ph_len = __strlen(ph);

    while (*src) {
        if (__strncmp(src, ph, ph_len) == 0) {
            while (*val) *out++ = *val++;
            src += ph_len;
        } else {
            *out++ = *src++;
        }
    }
    *out = '\0';
}

/* Parse prompt template with {USER}, {HOST}, {CWD} */
void parse_prompt(const char* tmpl, const char* user, const char* host, const char* cwd, char* out) {
    char tmp[512];
    replace_placeholder(tmpl, "{USER}", user, tmp);
    char tmp2[512];
    replace_placeholder(tmp, "{HOST}", host, tmp2);
    replace_placeholder(tmp2, "{CWD}", cwd, out);
}

/* =========================
   Entry point
   ========================= */

void HlMain(void) {
	char* errmsg;

    ConR = HlCreateNewHandle();
    ConW = HlCreateNewHandle();
    HlOpenConsole(ConR, ConW);

    char user[64] = "root";
    char host[64] = "terra";
    char prompt_template[256] =
        "\\x1B[92m{USER}@\\x1B[92m{HOST}"
        "\\x1B[90m:{CWD}\\x1B[0m$ ";

    /* ---- Open config file ---- */
    Handle* cfg = HlCreateNewHandle();
    if (!cfg) {
    	errmsg = "Failed to create config handle\n\r";
    	goto err;
    }
    
    HlOpenFile(cfg, "/initrd/init_conf.conf", O_RDWR);

    char cfg_buf[4096];
    __memset(cfg_buf, 0, sizeof(cfg_buf));

    int64_t rd = HlReadFile(cfg, cfg_buf, sizeof(cfg_buf) - 1);
    if (rd <= 0) {
        errmsg = "Failed to read init_conf.conf\n\r";
        //HlCloseFile(cfg);
        //HlDestroyHandle(cfg);
        goto err;
    }
    cfg_buf[rd] = '\0';

    //HlCloseFile(cfg);
    //HlDestroyHandle(cfg);

    /* ---- Parse config with debug prints ---- */
    char* line = cfg_buf;
    while (*line) {
        while (*line == '\n' || *line == '\r') line++;
        if (*line == '\0') break;

        char* end = line;
        while (*end && *end != '\n' && *end != '\r') end++;

        char save = *end;
        *end = '\0';

        print("DEBUG: line read: '"); print(line); print("'\n\r");

        if (*line == '#') { print("DEBUG: skipped comment\n\r"); line = end + 1; continue; }

        if (__strncmp(line, "USER=", 5) == 0) {
            extract_quoted_value(line + 5, user, sizeof(user));
            print("DEBUG: USER set to: '"); print(user); print("'\n\r");
        } else if (__strncmp(line, "HOST=", 5) == 0) {
            extract_quoted_value(line + 5, host, sizeof(host));
            print("DEBUG: HOST set to: '"); print(host); print("'\n\r");
        } else if (__strncmp(line, "PROMPT=", 7) == 0) {
            extract_quoted_value(line + 7, prompt_template, sizeof(prompt_template));
            print("DEBUG: PROMPT set to: '"); print(prompt_template); print("'\n\r");
        } else {
            print("DEBUG: unrecognized line\n\r");
        }

        if (!save) break;
        line = end + 1;
    }

    /* ---- Final debug ---- */
    print("DEBUG: final parsed values:\n\r");
    print("USER='"); print(user); print("'\n\r");
    print("HOST='"); print(host); print("'\n\r");
    print("PROMPT='"); print(prompt_template); print("'\n\r");

    if (__strcasecmp(host, "terra") != 0)
        print("\x1B[93mWarning: intended for Terra\x1B[0m\n\r");

    print("Terra running...\n\r");

    /* ---- Build final prompt ---- */
    char cwd[64] = "~";  // default cwd
    char prompt[512];
    parse_prompt(prompt_template, user, host, cwd, prompt);

    char final_prompt[512];
    char* s = prompt;
    char* d = final_prompt;
    while (*s) {
        if (*s == '\\' && s[1] == 'x') {
            s += 2;
            int v = 0;
            for (int i = 0; i < 2; i++) {
                v <<= 4;
                char c = *s++;
                if (c >= '0' && c <= '9') v |= c - '0';
                else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
                else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
            }
            *d++ = (char)v;
        } else {
            *d++ = *s++;
        }
    }
    *d = '\0';

    /* ---- Shell loop ---- */
    char buf[4096];
    while (1) {
        print(final_prompt);
        __memset(buf, 0, sizeof(buf));
        int64_t n = HlReadConsole(ConR, buf, sizeof(buf) - 1);
        if (n > 0) { print(buf); print("\n\r"); }
    }

err:
	print("Error: ");
	print(errmsg);

done:

    __builtin_unreachable();
}
