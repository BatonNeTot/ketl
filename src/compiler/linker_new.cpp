/*🍲Ketl🍲*/
#include "linker_new.h"

StandaloneFunction Linker::proceed(Environment& env, const std::string& source) {
	auto& result = _analyzer.proceed(env, source);

	Function function(env._alloc, result.stackSize, result.instructions.size());

	auto rawIt = result.instructions.begin();
	for (uint64_t i = 0u; i < function._instructionsCount; ++i, ++rawIt) {
		proceedCommand(env, function._instructions[i], *rawIt);
	}

	return { function };
}

void Linker::proceedCommand(Environment& env, Instruction& instruction, const Analyzer::RawInstruction& rawInstruction) {
	if (rawInstruction.info->isInstruction) {
		instruction._code = rawInstruction.info->instructionCode;
	}
	else {
		instruction._code; //TODO
	}

	convertArgument(env, instruction._outputType, instruction._output, rawInstruction.output);
	convertArgument(env, instruction._firstType, instruction._first, rawInstruction.args[0]);
	convertArgument(env, instruction._secondType, instruction._second, rawInstruction.args[1]);
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