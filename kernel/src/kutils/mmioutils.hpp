#ifndef MMIOUTILS_HPP
#define MMIOUTILS_HPP

#include <cstdint>

namespace mmioutils {
    static inline uint8_t  read8(uint64_t p_address)  { return *((volatile uint8_t*)p_address); }
    static inline uint16_t read16(uint64_t p_address) { return *((volatile uint16_t*)p_address); }
    static inline uint32_t read32(uint64_t p_address) { return *((volatile uint32_t*)p_address); }
    static inline uint64_t read64(uint64_t p_address) { return *((volatile uint64_t*)p_address); }

    static inline void write8(uint64_t p_address, uint8_t p_value)    { *((volatile uint8_t*)p_address)   = p_value; }
    static inline void write16(uint64_t p_address, uint16_t p_value)  { *((volatile uint16_t*)p_address)  = p_value; }
    static inline void write32(uint64_t p_address, uint32_t p_value)  { *((volatile uint32_t*)p_address)  = p_value; }
    static inline void write64(uint64_t p_address, uint64_t p_value)  { *((volatile uint64_t*)p_address)  = p_value; }
}

#endif
