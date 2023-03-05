/*🍲Ketl🍲*/
#include "common.h"

#include "type.h"
#include "semantic_analyzer.h"

namespace Ketl {
	
	void RawInstruction::propagadeInstruction(Instruction& instruction, SemanticAnalyzer& context) {
		instruction.code = code;
		if (outputVar) {
			std::tie(instruction.outputType, instruction.output) = outputVar->getArgument(context);
		}
		else {
			instruction.outputType = Argument::Type::None;
		}
		std::tie(instruction.firstType, instruction.first) = firstVar->getArgument(context);
		if (secondVar) {
			std::tie(instruction.secondType, instruction.second) = secondVar->getArgument(context);
		}
		else {
			instruction.secondType = Argument::Type::None;
		}
	}

}