/*🍲Ketl🍲*/
#include "type_resolver.h"

#include "context.h"

namespace Ketl {

	FunctionImpl Compiler::compile(const std::string& str, Context& context) {
		auto root = _parser.parseTree(str);

		return {};
	}
}