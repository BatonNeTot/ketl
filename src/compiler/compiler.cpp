/*🍲Ketl🍲*/
#include "compiler.h"

#include "context.h"

namespace Ketl {

	std::variant<FunctionImpl, std::string> Compiler::compile(const std::string& str, Context& context) {
		auto block = _parser.parseTree(str);

		if (std::holds_alternative<std::string>(block)) {
			return std::get<std::string>(block);
		}

		return _analyzer.compile(std::move(std::get<0>(block)), context);
	}
}