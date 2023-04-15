/*🍲Ketl🍲*/
#ifndef compiler_h
#define compiler_h

#include "common.h"
#include "variable.h"

#include "compiler/parser.h"
#include "compiler/semantic_analyzer.h"

#include <variant>

namespace Ketl {

	class VirtualMachine;

	class Compiler {
	public:

		std::variant<Variable, std::string> compile(const std::string_view& str, VirtualMachine& vm);

	private:
		Parser _parser;
	};

}

#endif /*compiler_compiler_h*/