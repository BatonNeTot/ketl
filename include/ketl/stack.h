//🍲ketl
#ifndef stack_h
#define stack_h

#include "ketl/common.h"

#include <inttypes.h>
#include <stdbool.h>

KETL_FORWARD(KETLStack);
KETL_FORWARD(KETLStackPoolBase);

KETL_FORWARD(KETLStackIterator);

struct KETLStack {
	size_t objectSize;
	size_t poolSize;
	KETLStackPoolBase* currentPool;
	size_t occupiedObjects;
};

struct KETLStackIterator {
	KETLStack* stack;
	KETLStackPoolBase* currentPool;
	size_t nextObjectIndex;
};

void ketlInitStack(KETLStack* stack, size_t objectSize, size_t poolSize);

void ketlDeinitStack(KETLStack* stack);

bool ketlIsStackEmpty(KETLStack* stack);

void* ketlPushOnStack(KETLStack* stack);

void* ketlPeekStack(KETLStack* stack);

void ketlPopStack(KETLStack* stack);

void ketlResetStack(KETLStack* stack);


void ketlInitStackIterator(KETLStackIterator* iterator, KETLStack* stack);

bool ketlIteratorStackHasNext(KETLStackIterator* iterator);

void* ketlIteratorStackGetNext(KETLStackIterator* iterator);

void ketlIteratorStackSkipNext(KETLStackIterator* iterator);

#define KETL_ITERATOR_STACK_PEEK(variableType, variableName, iterator)\
variableType variableName = NULL;\
do { \
KETLStackIterator __temp = (iterator);\
variableName = ketlIteratorStackGetNext(&__temp);\
} while (0)

#endif /*stack_h*/
