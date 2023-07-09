//🍲ketl
#include "ketl/utils.h"

int32_t ketlStrToI32(const char* str, size_t length) {
	int32_t value = 0;

	for (size_t i = 0u; i < length; ++i) {
		value = value * 10 + str[i] - '0';
	}

	return value;
}