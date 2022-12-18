/*🍲Ketl🍲*/
#ifndef compiler_type_resolver_h
#define compiler_type_resolver_h

#include "common.h"
#include "parser.h"

namespace Ketl {

	class Context;

	class Compiler {
	public:
		

		FunctionImpl compile(const std::string& str, Context& context);

	private:
		Parser _parser;
	};

}

#endif /*compiler_type_resolver_h*/