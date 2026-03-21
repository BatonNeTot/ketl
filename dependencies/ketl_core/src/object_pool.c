//🍲ketl
#include "ketl/object_pool.h"

#include <stdlib.h>

KETL_DEFINE(KETLObjectPoolBase) {
	KETLObjectPoolBase* next;
};

static inline KETLObjectPoolBase* createObjectPoolBase(KETLObjectPool* pool) {
	KETLObjectPoolBase* poolBase = malloc(sizeof(KETLObjectPoolBase) + pool->objectSize * pool->poolSize);
	poolBase->next = NULL;
	return poolBase;
}

void ketlInitObjectPool(KETLObjectPool* pool, size_t objectSize, size_t poolSize) {
	// TODO align by adjusting poolSize
	pool->objectSize = objectSize;
	pool->poolSize = poolSize;

	pool->firstPool = pool->lastPool = createObjectPoolBase(pool);
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

void* ketlGetNFreeObjectsFromPool(KETLObjectPool* pool, uint64_t count) {
	if (count == 0 || count > pool->poolSize) {
		return NULL;
	}
	if (pool->occupiedObjects + count > pool->poolSize) {
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
	void* value = ((char*)(pool->lastPool + 1)) + pool->objectSize * pool->occupiedObjects;
	pool->occupiedObjects += count;
	return value;
}

void* ketlGetObjectFromPool(KETLObjectPool* pool, uint64_t index) {
	KETLObjectPoolBase* baseIterator = pool->firstPool;
	uint64_t poolSize = pool->poolSize;
	while (index >= poolSize) {
		index -= poolSize;
		baseIterator = baseIterator->next;
	}
	return ((char*)(baseIterator + 1)) + pool->objectSize * index;
}

uint64_t ketlGetUsedCountFromPool(KETLObjectPool* pool) {
	KETLObjectPoolBase* baseIterator = pool->firstPool;
	KETLObjectPoolBase* lastPoolNext = pool->lastPool->next;
	uint64_t poolSize = pool->poolSize;
	uint64_t usedCount = 0;
	KETL_FOREVER {
		KETLObjectPoolBase* next = baseIterator->next;
		if (next == lastPoolNext) {
			break;
		}
		usedCount += poolSize;
		baseIterator = next;
	}
	return usedCount + pool->occupiedObjects;
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