//🍲ketl
#ifndef object_pool_h
#define object_pool_h

#include "ketl/utils.h"

#include <inttypes.h>
#include <stdbool.h>

KETL_FORWARD(KETLObjectPoolBase);

KETL_DEFINE(KETLObjectPool) {
	size_t objectSize;
	size_t poolSize;
	KETLObjectPoolBase* firstPool;
	KETLObjectPoolBase* lastPool;
	size_t occupiedObjects;
};

KETL_DEFINE(KETLObjectPoolIterator) {
	KETLObjectPool* pool;
	KETLObjectPoolBase* currentPool;
	size_t nextObjectIndex;
};

void ketlInitObjectPool(KETLObjectPool* pool, size_t objectSize, size_t poolSize);

void ketlDeinitObjectPool(KETLObjectPool* pool);

void* ketlGetFreeObjectFromPool(KETLObjectPool* pool);

void ketlResetPool(KETLObjectPool* pool);

void ketlInitPoolIterator(KETLObjectPoolIterator* iterator, KETLObjectPool* pool);

bool ketlIteratorPoolHasNext(KETLObjectPoolIterator* iterator);

void* ketlIteratorPoolGetNext(KETLObjectPoolIterator* iterator);

#endif /*object_pool_h*/
