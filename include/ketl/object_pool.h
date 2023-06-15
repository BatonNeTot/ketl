//🍲ketl
#ifndef object_pool_h
#define object_pool_h

#include "ketl/common.h"

#include <inttypes.h>
#include <stdbool.h>

KETL_FORWARD(KETLObjectPool);
KETL_FORWARD(KETLObjectPoolBase);

struct KETLObjectPool {
	size_t objectSize;
	size_t poolSize;
	KETLObjectPoolBase* firstPool;
	KETLObjectPoolBase* lastPool;
	uint32_t occupiedObjects;
};

void ketlInitObjectPool(KETLObjectPool* pool, size_t objectSize, size_t poolSize);

void ketlDeinitObjectPool(KETLObjectPool* pool);

void* ketlGetFreeObjectFromPool(KETLObjectPool* pool);

void ketlResetPool(KETLObjectPool* pool);

#define KETL_

#endif /*object_pool_h*/
