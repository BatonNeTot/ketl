/*🍲Ketl🍲*/
#include "env.h"

namespace Ketl {
	Environment::Environment()
		: _alloc(), _context(_alloc, 4096) {
	}
}