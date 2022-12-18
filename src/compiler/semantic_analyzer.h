/*🍲Ketl🍲*/
#ifndef compiler_semantic_analyzer_h
#define compiler_semantic_analyzer_h

#include "common.h"
#include "parser.h"

namespace Ketl {

	class Context;

	class SemanticAnalyzer {
	public:
		

		FunctionImpl compile(std::unique_ptr<IRNode>&& block, Context& context);

	};

}

#endif /*compiler_semantic_analyzer_h*/