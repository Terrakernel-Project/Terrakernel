#ifndef PANIC_HPP
#define PANIC_HPP 1

__attribute__((noreturn))
void _panic(const char* func, const char* error_code);
#define panic(err_code) _panic(__PRETTY_FUNCTION__, err_code)

void assert(bool expected);
void assert_specific(bool expected, const char* info);

#endif /* PANIC_HPP */
