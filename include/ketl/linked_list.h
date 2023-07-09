//🍲ketl
#ifndef linked_list_h
#define linked_list_h

#include "ketl/utils.h"

#include <inttypes.h>
#include <stdbool.h>

KETL_DEFINE(LinkedListNodeBase) {
	LinkedListNodeBase* next;
	LinkedListNodeBase* prev;
};

KETL_DEFINE(LinkedListPoolBase) {
	LinkedListPoolBase* next;
};

KETL_DEFINE(LinkedListBase) {
	LinkedListPoolBase* pool;
	LinkedListNodeBase* free;
	LinkedListNodeBase* end;
};

#define KETL_CACHED_LINKED_LIST_POOL_SIZE 16

void ketlInitLinkedListPool(LinkedListBase* list, size_t typeSize);

void ketlDeinitLinkedListPool(LinkedListBase* list);

LinkedListNodeBase* ketlPushNode(LinkedListBase* list);

LinkedListNodeBase* ketlLastNode(LinkedListBase* list);

LinkedListNodeBase* ketlNextNode(LinkedListNodeBase* node);

LinkedListNodeBase* ketlPrevNode(LinkedListNodeBase* node);

bool ketlIsListEmpty(LinkedListBase* list, LinkedListNodeBase* node);

#define KETL_

#endif /*cached_linked_list_h*/
