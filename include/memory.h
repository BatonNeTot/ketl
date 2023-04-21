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

		StackAllocator<Allocator>& stack() {
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
		inline void unregisterAbsRoot(const T* ptr) {
			_heap.unregisterAbsRoot(ptr);
		}

		template <typename T>
		inline void registerRefRoot(const T* const* pptr) {
			_heap.registerRefRoot(pptr);
		}

		template <typename T>
		inline void unregisterRefRoot(const T* const* pptr) {
			_heap.unregisterRefRoot(pptr);
		}

		template <typename T1, typename T2>
		inline void registerAbsLink(const T1* t1ptr, const T2* t2ptr) {
			_heap.registerAbsLink(t1ptr, t2ptr);
		}
		template <typename T1, typename T2>
		inline void registerRefLink(const T1* t1ptr, const T2* const* t2pptr) {
			_heap.registerAbsLink(t1ptr, t2pptr);
		}

		auto& registerMemory(void* ptr, size_t size) {
			return _heap.registerMemory(ptr, size);
		}
		auto& registerMemory(void* ptr, size_t size, auto finalizer) {
			return _heap.registerMemory(ptr, size, finalizer);
		}

		size_t collectGarbage() {
			return _heap.collectGarbage();
		}

	private:
		Allocator _alloc;
		StackAllocator<Allocator> _stack;
		GCAllocator<Allocator> _heap;
	};

}

#endif /*memory_h*/