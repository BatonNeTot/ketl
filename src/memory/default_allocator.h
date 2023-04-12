/*🍲Ketl🍲*/
#ifndef default_allocator_h
#define default_allocator_h

#include <cinttypes>

namespace Ketl {
	class Allocator {
	public:

		template <class T>
		T* allocate(size_t count = 1) {
			return reinterpret_cast<T*>(allocate(sizeof(T) * count));
		}

		uint8_t* allocate(size_t size) {
			return reinterpret_cast<uint8_t*>(::operator new(size));
		}

		void deallocate(void* ptr) {
			::operator delete(ptr);
		}
	};
}

#endif /*default_allocator_h*/