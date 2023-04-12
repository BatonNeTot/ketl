/*🍲Ketl🍲*/
#ifndef compiler_h
#define compiler_h

#include "common.h"

#include "compiler/parser.h"
#include "compiler/semantic_analyzer.h"

#include <variant>

namespace Ketl {

	class Context;

	class Compiler {
	public:

		std::variant<Variable, std::string> compile(const std::string_view& str, Context& context);

	private:
		Parser _parser;
	};

}

#endif /*compiler_compiler_h*/