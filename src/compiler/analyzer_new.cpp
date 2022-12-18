/*🍲Ketl🍲*/
#include "analyzer_new.h"

#include "ketl.h"

#include <unordered_set>
#include <unordered_map>
#include <sstream>

const Analyzer::Result& Analyzer::proceed(Environment& env, const std::string& source) {
	if (_result) {
		return *_result;
	}

	_result = std::make_unique<Result>();

	AnalyzerInfo info;

	auto blockNode = _parser.proceed(source);

	for (auto& command : blockNode->args) {
		proceedCommands(env, _result->instructions, info, *command);
	}

	_result->stackSize = info.nextFreeStack;

	return *_result;
}

Analyzer::Variable Analyzer::proceedCommands(Environment& env, std::list<Analyzer::RawInstruction>& list, Analyzer::AnalyzerInfo& info, const Ketl::Parser::Node& node) {
	switch (node.type) {
	case Ketl::Parser::Node::Type::Operator: {
		std::vector<Variable> args;
		args.reserve(node.args.size());
		for (auto& arg : node.args) {
			auto var = proceedCommands(env, list, info, *arg);
			args.emplace_back(var);
		}

		if (node.value.str == "=" && args[0].valueType == nullptr) {
			args[0].valueType = args[1].valueType;
			env.declareGlobal(args[0].id, args[0].valueType);
		}

		if (node.value.str == "=") {
			auto* outputType = env.getGlobalType(args[0].id);

			if (outputType != args[1].valueType) {

			}
		}

		std::vector<const Type*> argTypes;
		argTypes.reserve(args.size());
		for (auto& arg : args) {
			argTypes.emplace_back(arg.valueType);
		}

		auto& funcInfo = *env.estimateFunction("operator " + node.value.str, argTypes);

		for (auto i = 0u; i < argTypes.size(); ++i) {
			if (args[i].valueType != funcInfo.argTypes[i]) {
				// TODO
				auto& command = list.emplace_back();
				command.info = nullptr; // funcInfo.argTypes[i]->castTargetStr();

				command.output.type = Variable::Type::Stack;
				command.output.valueType = funcInfo.argTypes[i];
				command.output.stack = info.getFreeStack(command.output.valueType->sizeOf());

				command.args[i] = args[i];
				args[i] = command.output;
			}
		}

		auto& command = list.emplace_back();
		command.info = &funcInfo;

		command.output.type = Variable::Type::Stack;
		command.output.valueType = funcInfo.returnType;
		command.output.stack = info.getFreeStack(command.output.valueType->sizeOf()); //TODO

		command.args[0] = args[0];
		command.args[1] = args[1];

		return command.output;
	}
	case Ketl::Parser::Node::Type::Number: {
		Variable variable;
		variable.type = Variable::Type::Literal;
		variable.literal = node.value;
		auto* type = env.getType(node.value.asStrType());
		variable.valueType = type;
		return variable;
	}
	case Ketl::Parser::Node::Type::Id: {
		Variable variable;
		variable.type = Variable::Type::Global;
		variable.id = node.value.str;
		auto* type = env.getGlobalType(variable.id);
		variable.valueType = type;
		return variable;
	}
	}

	return {};
}
