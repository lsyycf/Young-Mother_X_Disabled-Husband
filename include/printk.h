#ifndef _printk_h_
#define _printk_h_

#include <machine.h>
#include <stdarg.h>

void printk(const char *fmt, ...);

void _panic(const char *, int, const char *, const char *, ...)
#ifdef MOS_HANG_ON_PANIC
    __attribute__((noreturn))
#endif
    ;

#define panic(...) _panic(__FILE__, __LINE__, __func__, __VA_ARGS__)

#define panic_on(expr)                                                         \
    do {                                                                       \
        int _r = (expr);                                                       \
        if (_r != 0) {                                                         \
            panic("'" #expr "' returned %d", _r);                              \
        }                                                                      \
    } while (0)

#endif /* _printk_h_ */
