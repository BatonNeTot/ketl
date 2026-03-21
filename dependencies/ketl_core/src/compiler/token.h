//🍲ketl
#ifndef compiler_token_h
#define compiler_token_h

#include "ketl/utils.h"

#include <inttypes.h>

typedef uint8_t KETLTokenType;

#define KETL_TOKEN_TYPE_ID 0
#define KETL_TOKEN_TYPE_STRING 1
#define KETL_TOKEN_TYPE_NUMBER 2
#define KETL_TOKEN_TYPE_SPECIAL 3

KETL_DEFINE(KETLToken) {
	ptrdiff_t positionInSource;
	const char* value;
	uint16_t length;
	KETLTokenType type;
	KETLToken* next;
};

#endif /*compiler_token_h*/