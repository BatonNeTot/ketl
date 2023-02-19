/*🍲Ketl🍲*/
#ifndef compiler_semantic_analyzer_h
#define compiler_semantic_analyzer_h

#include "common.h"
#include "parser.h"
#include "context.h"

#include <stack>

namespace Ketl {

	class SemanticAnalyzer {
	public:
		

		FunctionImpl compile(std::unique_ptr<IRNode>&& block, Context& context);

	};

	class AnalyzerContext {
	public:

		AnalyzerContext(Context& context)
			: _context(context) {}

		void bakeContext();

		void propagateGlobalVars(Context& context);

		AnalyzerVar* createTemporaryVar(uint64_t size);
		AnalyzerVar* createLiteralVar(const std::string_view& value);

		AnalyzerVar* createGlobalVar(const std::string_view& value);
		AnalyzerVar* createGlobalTypedVar();

		Context& context() {
			return _context;
		}

		uint64_t scopeLayer = 0u;
		uint64_t currentStackOffset = 0u;
		uint64_t maxOffsetValue = 0u;

		std::vector<std::unique_ptr<AnalyzerVar>> vars;

		struct ScopeVar {
			uint64_t tmpOffset;
			uint64_t stackOffset;
		};

		std::stack<ScopeVar> scopeVars;
		std::unordered_map<std::string_view, AnalyzerVar*> globalVars;

		Context& _context;
	};

}

#endif /*compiler_semantic_analyzer_h*/