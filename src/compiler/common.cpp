/*🍲Ketl🍲*/
#include "common.h"

#include "type.h"
#include "semantic_analyzer.h"

namespace Ketl {
	
	void RawInstruction::propagadeInstruction(Instruction& instruction, AnalyzerContext& context) {
		instruction._code = code;
		instruction._outputOffset = 0;
		if (outputVar) {
			std::tie(instruction._outputType, instruction._output) = outputVar->getArgument(context);
		}
		else {
			instruction._outputType = Argument::Type::None;
		}
		std::tie(instruction._firstType, instruction._first) = firstVar->getArgument(context);
		std::tie(instruction._secondType, instruction._second) = secondVar->getArgument(context);
	}

}