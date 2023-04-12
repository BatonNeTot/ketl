/*🍲Ketl🍲*/
#ifndef memory_h
#define memory_h

#include "memory/default_allocator.h"
#include "memory/stack_allocator.h"
#include "memory/gc_allocator.h"

namespace Ketl {

	class MemoryManager {
	public:

		MemoryManager(uint64_t stackBlockSize = 4096)
			: _stack(_alloc, stackBlockSize), _heap(_alloc) {}

		Allocator& alloc() {
			return _alloc;
		}

	private:
		Allocator _alloc;
		StackAllocator _stack;
		GCAllocator _heap;
	};

}

#endif /*memory_h*/