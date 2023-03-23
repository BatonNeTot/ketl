/*🍲Ketl🍲*/
#include "common.h"

#include "type.h"
#include "semantic_analyzer.h"

namespace Ketl {
	
	void RawInstruction::propagadeInstruction(Instruction& instruction) {
		instruction.code = code;
		if (outputVar) {
			std::tie(instruction.outputType, instruction.output) = outputVar->getArgument();
		}
		else {
			instruction.outputType = Argument::Type::None;
		}
		std::tie(instruction.firstType, instruction.first) = firstVar->getArgument();
		if (secondVar) {
			std::tie(instruction.secondType, instruction.second) = secondVar->getArgument();
		}
		else {
			instruction.secondType = Argument::Type::None;
		}
	}

}