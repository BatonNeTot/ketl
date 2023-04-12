/*🍲Ketl🍲*/
#ifndef ketl_h
#define ketl_h

#include "memory.h"
#include "context.h"
#include "compiler.h"

namespace Ketl {

	class VirtualMachine {
	public:

		VirtualMachine(uint64_t stackBlockSize = 4096)
			: _memory(stackBlockSize), _context(_memory.alloc(), stackBlockSize) {}

		template <typename T>
		bool declareGlobal(const std::string_view& id, T* ptr) {
			return _context.declareGlobal(id, ptr);
		}

		inline auto compile(const std::string_view& source) {
			return _compiler.compile(source, _context);
		}

	private:

		Ketl::MemoryManager _memory;
		Ketl::Context _context;
		Ketl::Compiler _compiler;
	};

}

#endif /*ketl_h*/