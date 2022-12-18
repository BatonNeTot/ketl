/*🍲Ketl🍲*/
#ifndef compiler_compiler_h
#define compiler_compiler_h

#include "common.h"

#include "parser.h"
#include "parser_new.h"
#include "semantic_analyzer.h"

namespace Ketl {

	class Context;

	class Compiler {
	public:
		

		FunctionImpl compile(const std::string& str, Context& context);

	private:
		Parser _parser;
		ParserNew _parserNew;
		SemanticAnalyzer _analyzer;
	};

}

#endif /*compiler_compiler_h*/