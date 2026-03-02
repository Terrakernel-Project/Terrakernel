#ifndef TKSA_HPP
#define TKSA_HPP 1

// TKSA stands for Terrakernel Sound Architecture

#include <cstdint>
#include <cstddef>

namespace tksa {

void initialise();
bool play_audio_data(const void* data_base, size_t data_size);


}

#endif
