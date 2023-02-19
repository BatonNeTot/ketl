/*🍲Ketl🍲*/
#include "compiler.h"

#include "context.h"

namespace Ketl {

	FunctionImpl Compiler::compile(const std::string& str, Context& context) {
		auto block = _parser.parseTree(str);

		if (!block) {
			std::cerr << _parser.errorMsg() << std::endl;
			return {};
		}

		return _analyzer.compile(std::move(block), context);
	}
}