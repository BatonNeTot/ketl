//🍲ketl
#include "ketl/stack.h"

#include <stdlib.h>

struct KETLStackPoolBase {
	KETLStackPoolBase* next;
	KETLStackPoolBase* prev;
};

static inline KETLStackPoolBase* createPoolBase(KETLStack* stack) {
	KETLStackPoolBase* poolBase = malloc(sizeof(KETLStackPoolBase) + stack->objectSize * stack->poolSize);
	poolBase->next = NULL;
	return poolBase;
}

void ketlInitStack(KETLStack* stack, size_t objectSize, size_t poolSize) {
	stack->objectSize = objectSize;
	stack->poolSize = poolSize;

	stack->currentPool = createPoolBase(stack);
	stack->currentPool->prev = NULL;
	stack->occupiedObjects = 0;
}

void ketlDeinitStack(KETLStack* stack) {
	KETLStackPoolBase* it = stack->currentPool;
	while (it->prev) it = it->prev;
	while (it) {
		KETLStackPoolBase* next = it->next;
		free(it);
		it = next;
	}
}

bool ketlIsEmpty(KETLStack* stack) {
	return stack->occupiedObjects == 0 && stack->currentPool->prev == NULL;
}

void* ketlPeekStack(KETLStack* stack) {
	return ((char*)(stack->currentPool + 1)) + stack->objectSize * (stack->occupiedObjects - 1);
}

void* ketlPushOnStack(KETLStack* stack) {
	if (stack->occupiedObjects >= stack->poolSize) {
		if (stack->currentPool->next) {
			stack->currentPool = stack->currentPool->next;
		}
		else {
			KETLStackPoolBase* poolBase = createPoolBase(stack);
			stack->currentPool->next = poolBase;
			poolBase->prev = stack->currentPool;
			stack->currentPool = poolBase;
		}
		stack->occupiedObjects = 0;
	}
	return ((char*)(stack->currentPool + 1)) + stack->objectSize * stack->occupiedObjects++;
}

void ketlPopStack(KETLStack* stack) {
	if (stack->occupiedObjects == 1) {
		stack->currentPool = stack->currentPool->prev;
		stack->occupiedObjects = stack->poolSize;
	}
	else {
		--stack->occupiedObjects;
	}
}

void ketlResetStack(KETLStack* stack) {
	KETLStackPoolBase* it = stack->currentPool;
	while (it->prev) it = it->prev;
	stack->currentPool = it;
	stack->occupiedObjects = 0;
}