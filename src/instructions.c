//🍲ketl
#include "ketl/instructions.h"

uint8_t KETL_CODE_SIZES[] = {
		1,	//None,
		4,	//AddInt64,
		4,	//MinusInt64,
		4,	//MultyInt64,
		4,	//DivideInt64,
		3,	//Assign8bytes,
		1,	//Return,
};