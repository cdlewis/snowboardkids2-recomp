#include "patches.h"

extern void recomp_puts(const char* data, u32 size);

typedef __builtin_va_list recomp_va_list;
typedef void* outfun(void*, const char*, size_t);

int _Printf(outfun prout, void* arg, const char* fmt, recomp_va_list args);

void* proutPrintf(void* dst, const char* fmt, size_t size) {
    recomp_puts(fmt, size);
    return (void*) 1;
}

RECOMP_EXPORT int recomp_printf(const char* fmt, ...) {
    recomp_va_list args;
    __builtin_va_start(args, fmt);

    int ret = _Printf(&proutPrintf, NULL, fmt, args);

    __builtin_va_end(args);

    return ret;
}
