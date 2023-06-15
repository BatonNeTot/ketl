//🍲ketl
#ifndef linked_list_h
#define linked_list_h

#include "ketl/common.h"

#include <inttypes.h>
#include <stdbool.h>

KETL_FORWARD(LinkedListNodeBase);
KETL_FORWARD(LinkedListPoolBase);
KETL_FORWARD(LinkedListBase);

struct LinkedListNodeBase {
	LinkedListNodeBase* next;
	LinkedListNodeBase* prev;
};

struct LinkedListPoolBase {
	LinkedListPoolBase* next;
};

struct LinkedListBase {
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

bool ketlIsEmpty(LinkedListBase* list, LinkedListNodeBase* node);

#define KETL_

#endif /*cached_linked_list_h*/
