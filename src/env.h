/*🍲Ketl🍲*/
#ifndef env_h
#define env_h

#include "ketl.h"
#include "context.h"

namespace Ketl {
	class Environment {
	public:

		Environment();

	public: //TODO private

		Allocator _alloc;
		Context _context;

		std::unordered_map<std::type_index, std::string> _typeIdByIndex;
	};
}

#endif /*env_h*/