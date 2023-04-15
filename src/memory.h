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

		StackAllocator& stack() {
			return _stack;
		}

		uint8_t* allocateOnStack(uint64_t size) {
			return _stack.allocate(size);
		}

		template <typename T>
		inline void registerAbsRoot(const T* ptr) {
			_heap.registerAbsRoot(ptr);
		}

		template <typename T>
		inline void registerRefRoot(const T* const* pptr) {
			_heap.registerRefRoot(pptr);
		}

		auto& registerMemory(void* ptr, size_t size, auto finalizer) {
			return _heap.registerMemory(ptr, size, finalizer);
		}

	private:
		Allocator _alloc;
		StackAllocator _stack;
		GCAllocator _heap;
	};

}

#endif /*memory_h*/