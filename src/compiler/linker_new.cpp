/*🐟Ketl🐟*/
#include "linker_new.h"

StandaloneFunction Linker::proceed(Environment& env) {
	auto& result = _analyzer.proceed(env);

	Function function(env._alloc, result.stackSize, result.instructions.size());

	auto rawIt = result.instructions.begin();
	for (uint64_t i = 0u; i < function._instructionsCount; ++i, ++rawIt) {
		proceedCommand(function._instructions[i], *rawIt);
	}

	env.defineGlobalVariable<int64_t>("c");
	function._stackSize = 0u;

	function._instructions[0]._code = InstructionCode::AddInt;

	function._instructions[0]._output.globalPtr = env.getGlobalVariable<int64_t>("c");
	function._instructions[0]._outputType = ArgumentType::Global;
	function._instructions[0]._first.integer = 5;
	function._instructions[0]._firstType = ArgumentType::Literal;
	function._instructions[0]._second.integer = 5;
	function._instructions[0]._secondType = ArgumentType::Literal;

	return { function, 8 };
}

void Linker::proceedCommand(Instruction& instruction, const Analyzer::RawInstruction& rawInstruction) {

}

void Linker::convertArgument(Environment& env, ArgumentType& type, Argument& value, const Analyzer::Variable& variable) {
	switch (variable.type) {
	case Analyzer::Variable::Type::Global: {
		type = ArgumentType::Global;
		value.globalPtr = env.getGlobalVariable<>(variable.id);
		break;
	}
	case Analyzer::Variable::Type::Stack: {
		type = ArgumentType::Stack;
		value.stack = variable.stack;
		break;
	}
	case Analyzer::Variable::Type::Literal: {
		type = ArgumentType::Literal;
		switch (variable.literal.number.type) {
		case Ketl::UniValue::Number::Type::Int32: {
			value.integer = variable.literal.number.vInt32;
			break;
		}
		case Ketl::UniValue::Number::Type::Int64: {
			value.integer = variable.literal.number.vInt64;
			break;
		}
		case Ketl::UniValue::Number::Type::Float32: {
			value.floating = variable.literal.number.vFloat32;
			break;
		}
		case Ketl::UniValue::Number::Type::Float64: {
			value.floating = variable.literal.number.vFloat64;
			break;
		}
		}
		break;
	}
	}
}