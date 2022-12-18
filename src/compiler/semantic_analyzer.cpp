/*🍲Ketl🍲*/
#include "semantic_analyzer.h"

#include "context.h"

namespace Ketl {

	FunctionImpl SemanticAnalyzer::compile(std::unique_ptr<IRNode>&& block, Context& context) {
		std::vector<Instruction> instructions;
		if (!block->produceInstructions(instructions, context)) {
			return {};
		}

		return {};
	}
}