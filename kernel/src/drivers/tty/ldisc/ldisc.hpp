// for whoever doesn't know what ldisc means it means line discipline...

#ifndef LDISC_HPP
#define LDISC_HPP 1

#include <types.hpp>
#include <cstddef>
#include "ldisc_qwerty.hpp"

namespace drivers::tty::ldisc {

void initialise();
ssize_t read(bool echo, char* buf, size_t n);
// -1 means write until null terminator, it will be cast to size_t then used as the max value
ssize_t write(const char* buf, ssize_t n = -1);

}

#endif
