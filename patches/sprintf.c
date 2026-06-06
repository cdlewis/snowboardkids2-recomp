#include "patches.h"

typedef __builtin_va_list recomp_va_list;
typedef void* outfun(void*, const char*, size_t);

int _Printf(outfun prout, void* arg, const char* fmt, recomp_va_list args);

static void* proutSprintfCallback(void* dst, const char* src, size_t size) {
    char* out = (char*) dst;
    size_t i;

    for (i = 0; i < size; i++) {
        out[i] = src[i];
    }

    return out + size;
}

int _Sprintf(char* buffer, const char* fmt, ...) {
    recomp_va_list args;
    int ret;

    __builtin_va_start(args, fmt);
    ret = _Printf(proutSprintfCallback, buffer, fmt, args);
    __builtin_va_end(args);

    buffer[ret] = '\0';
    return ret;
}
