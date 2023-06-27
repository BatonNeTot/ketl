//🍲ketl
#include "ketl/object_pool.h"

#include <stdlib.h>

struct KETLObjectPoolBase {
	KETLObjectPoolBase* next;
};

static inline KETLObjectPoolBase* createObjectPoolBase(KETLObjectPool* pool) {
	KETLObjectPoolBase* poolBase = malloc(sizeof(KETLObjectPoolBase) + pool->objectSize * pool->poolSize);
	poolBase->next = NULL;
	return poolBase;
}

void ketlInitObjectPool(KETLObjectPool* pool, size_t objectSize, size_t poolSize) {
	pool->objectSize = objectSize;
	pool->poolSize = poolSize;

	pool->firstPool = pool->lastPool = NULL;
	pool->occupiedObjects = 0;
}

void ketlDeinitObjectPool(KETLObjectPool* pool) {
	KETLObjectPoolBase* it = pool->firstPool;
	while (it) {
		KETLObjectPoolBase* next = it->next;
		free(it);
		it = next;
	}
}

void* ketlGetFreeObjectFromPool(KETLObjectPool* pool) {
	if (pool->firstPool == NULL) {
		pool->firstPool = pool->lastPool = createObjectPoolBase(pool);
	}
	if (pool->occupiedObjects >= pool->poolSize) {
		if (pool->lastPool->next) {
			pool->lastPool = pool->lastPool->next;
		}
		else {
			KETLObjectPoolBase* poolBase = createObjectPoolBase(pool);
			pool->lastPool->next = poolBase;
			pool->lastPool = poolBase;
		}
		pool->occupiedObjects = 0;
	}
	return ((char*)(pool->lastPool + 1)) + pool->objectSize * pool->occupiedObjects++;
}

void ketlResetPool(KETLObjectPool* pool) {
	pool->lastPool = pool->firstPool;
	pool->occupiedObjects = 0;
}

void ketlInitPoolIterator(KETLObjectPoolIterator* iterator, KETLObjectPool* pool) {
	iterator->pool = pool;
	iterator->currentPool = pool->firstPool;
	iterator->nextObjectIndex = 0;
}

bool ketlIteratorPoolHasNext(KETLObjectPoolIterator* iterator) {
	return iterator->currentPool != NULL && (iterator->currentPool != iterator->pool->lastPool || iterator->nextObjectIndex < iterator->pool->occupiedObjects);
}

void* ketlIteratorPoolGetNext(KETLObjectPoolIterator* iterator) {
	if (iterator->nextObjectIndex >= iterator->pool->poolSize) {
		iterator->currentPool = iterator->currentPool->next;
		iterator->nextObjectIndex = 0;
	}
	return ((char*)(iterator->currentPool + 1)) + iterator->pool->objectSize * iterator->nextObjectIndex++;
}