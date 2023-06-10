//🍲ketl
#ifndef compiler_token_h
#define compiler_token_h

typedef uint8_t KETLTokenType;

#define KETL_TOKEN_TYPE_ID 0
#define KETL_TOKEN_TYPE_STRING 1
#define KETL_TOKEN_TYPE_NUMBER 2
#define KETL_TOKEN_TYPE_SPECIAL 3

struct KETLToken_t {
	const char* value;
	uint16_t length;
	KETLTokenType type;
	ptrdiff_t positionInSource;
};

#endif /*compiler_token_h*/