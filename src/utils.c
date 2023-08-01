//🍲ketl
#include "ketl/utils.h"

double ketlStrToF64(const char* str, size_t length) {
	// TODO
	uint64_t wholePart = 0;

	size_t i = 0u;
	for (; i < length && str[i] >= '0' && str[i] <= '9'; ++i) {
		wholePart = wholePart * 10 + str[i] - '0';
	}

	return 0.0;
}

int64_t ketlStrToI64(const char* str, size_t length) {
	int64_t value = 0;

	for (size_t i = 0u; i < length; ++i) {
		value = value * 10 + str[i] - '0';
	}

	return value;
}