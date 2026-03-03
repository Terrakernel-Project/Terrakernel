#ifndef UTILS_HPP
#define UTILS_HPP 1

#include "unions.hpp"

static inline char* __libc_parse_number(char* input, int* output) {
	int idx = 0;
	int number_system_base = 10;
	if (input[0] == '0') {
		if (input[1] == 'x') {
			number_system_base = 16;
			idx = 2;
		} else if (input[1] == 'b') {
			number_system_base = 2;
			idx = 2;
		}
	}
	int _number = 0;
	while (input[idx] != '\0') {
		if (input[idx] >= '0' && input[idx] <= '9') {
			_number = _number * number_system_base + (input[idx] - '0');
		} else if (input[idx] >= 'a' && input[idx] <= 'f') {
			_number = _number * number_system_base + (input[idx] - 'a' + 10);
		} else if (input[idx] >= 'A' && input[idx] <= 'F') {
			_number = _number * number_system_base + (input[idx] - 'A' + 10);
		} else {
			break;
		}
		idx++;
	}
	*output = _number;
	return &input[idx];
}

static inline ip_u parse_ip(const char* in) {
	ip_u ip = { 0 };
	char* curr = (char*) in;
	for (int i = 0; i < 4; i++) {
		int r = 0;
		curr = __libc_parse_number(curr, &r);
		if ((*curr != '.' && *curr != 0) || (*curr == 0 && i != 3)) {
			return (ip_u) {.ip = 0};
		}
		curr++;
		ip.ip_p[i] = r;
	}
	return ip;
}

#endif
