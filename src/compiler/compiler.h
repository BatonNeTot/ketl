/*🍲Ketl🍲*/
#ifndef compiler_compiler_h
#define compiler_compiler_h

#include "common.h"

#include "parser.h"
#include "semantic_analyzer.h"

#include <variant>

namespace Ketl {

	class Context;

	class Compiler {
	public:

		std::variant<FunctionImpl, std::string> compile(const std::string& str, Context& context);

	private:
		Parser _parser;
	};

}

#endif /*compiler_compiler_h*/