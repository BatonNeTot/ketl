/*🍲Ketl🍲*/
#include "common.h"

#include "type.h"
#include "semantic_analyzer.h"

namespace Ketl {
	
	void RawInstruction::propagadeInstruction(Instruction& instruction, AnalyzerContext& context) {
		instruction._code = code;
		instruction._outputOffset = 0;
		std::tie(instruction._outputType, instruction._output) = outputVar->getArgument(context);
		std::tie(instruction._firstType, instruction._first) = firstVar->getArgument(context);
		std::tie(instruction._secondType, instruction._second) = secondVar->getArgument(context);
	}

}