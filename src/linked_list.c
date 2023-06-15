//🍲ketl
#include "ketl/linked_list.h"

#include <stdlib.h>

static LinkedListPoolBase* createLinkedListPool(size_t typeSize) {
	// TODO consider alignment
	const size_t fullTypeSize = sizeof(LinkedListNodeBase) + typeSize;
	char* pool = malloc(sizeof(LinkedListPoolBase) + fullTypeSize * KETL_CACHED_LINKED_LIST_POOL_SIZE);
	if (!pool) {
		// TODO assert
		return NULL;
	}

	char* nodeArray = pool + sizeof(LinkedListPoolBase);

	size_t i = KETL_CACHED_LINKED_LIST_POOL_SIZE - 1;
	LinkedListNodeBase* it = (LinkedListNodeBase*)(nodeArray + fullTypeSize * i);
	it->next = NULL;
	LinkedListNodeBase* next;
	while (i > 0) {
		next = (LinkedListNodeBase*)(nodeArray + fullTypeSize * --i);
		next->next = it;
		it = next;
	}

	return (LinkedListPoolBase*)pool;
}

#define POOL_GET_FIRST_NODE(x) (LinkedListNodeBase*)((x) + 1)

void ketlInitLinkedListPool(LinkedListBase* list, size_t typeSize) {
	list->pool = createLinkedListPool(typeSize);
	if (!list->pool) {
		return;
	}

	list->free = POOL_GET_FIRST_NODE(list->pool);
	list->end = list->free;

	list->free = list->free->next;

	list->end->next = list->end;
	list->end->prev = list->end;
}

void ketlDeinitLinkedListPool(LinkedListBase* list) {
	LinkedListPoolBase* pool = list->pool;
	while (pool) {
		LinkedListPoolBase* next = pool->next;
		free(pool);
		pool = next;
	}
}


LinkedListNodeBase* ketlPushNode(LinkedListBase* list) {
	if (!list->free) {
		//TODO create free
	}

	LinkedListNodeBase* node = list->free;
	list->free = list->free->next;

	LinkedListNodeBase* last = list->end->prev;
	last->next = node;
	node->prev = last;

	list->end->prev = node;
	node->next = list->end;

	return node + 1;
}

LinkedListNodeBase* ketlLastNode(LinkedListBase* list) {
	return list->end->prev + 1;
}

LinkedListNodeBase* ketlNextNode(LinkedListNodeBase* node) {
	return (node - 1)->next + 1;
}

LinkedListNodeBase* ketlPrevNode(LinkedListNodeBase* node) {
	return (node - 1)->next + 1;
}

bool ketlIsEmpty(LinkedListBase* list, LinkedListNodeBase* node) {
	return list->end == (node - 1);
}