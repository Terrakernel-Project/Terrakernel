#ifndef STACK_HPP
#define STACK_HPP 1

#include "stack.hpp"
#include <cstdint>
#include <cstddef>

void* stack_manager_get_new_stack(size_t num_pages, bool user);
bool destroy_stack(void* stack_top);

#endif
