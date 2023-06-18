//🍲ketl
#ifndef stack_h
#define stack_h

#include "ketl/common.h"

#include <inttypes.h>
#include <stdbool.h>

KETL_FORWARD(KETLStack);
KETL_FORWARD(KETLStackPoolBase);

struct KETLStack {
	size_t objectSize;
	size_t poolSize;
	KETLStackPoolBase* currentPool;
	size_t occupiedObjects;
};

void ketlInitStack(KETLStack* stack, size_t objectSize, size_t poolSize);

void ketlDeinitStack(KETLStack* stack);

bool ketlIsEmpty(KETLStack* stack);

void* ketlPushOnStack(KETLStack* stack);

void* ketlPeekStack(KETLStack* stack);

void ketlPopStack(KETLStack* stack);

void ketlResetStack(KETLStack* stack);

#endif /*stack_h*/
