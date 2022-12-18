/*🐟Ketl🐟*/
#include "analyzer_new.h"

#include "eel.h"

#include <unordered_set>
#include <unordered_map>
#include <sstream>

const Analyzer::Result& Analyzer::proceed(Environment& env) {
	if (_result != nullptr) {
		return *_result;
	}

	_result = new Result();

	AnalyzerInfo info;

	while (_parser.hasNext()) {
		auto& node = _parser.getNext();
		proceedCommands(env, _result->instructions, info, node);
	}

	return *_result;
}

Analyzer::Variable Analyzer::proceedCommands(Environment& env, std::list<Analyzer::RawInstruction>& list, Analyzer::AnalyzerInfo& info, const Ketl::Parser::Node& node) {
	switch (node.type) {
	case Ketl::Parser::Node::Type::Operator: {
		if (node.value.str == "()") {
			return proceedCommands(env, list, info, *node.args[0]);
		}

		std::vector<Variable> args;
		args.reserve(node.args.size());
		for (auto& arg : node.args) {
			auto var = proceedCommands(env, list, info, *arg);
			args.emplace_back(var);
		}

		if (node.value.str == "=" && args[0].valueType.empty()) {
			args[0].valueType = args[1].valueType;
			env.declareGlobal(args[0].id, args[0].valueType);
		}

		if (node.value.str == "=") {
			auto resultType = env.getGlobalType(args[0].id)->id();

			if (resultType != args[1].valueType) {

			}

			auto& lastCommand = list.back();
			lastCommand.result = args[0];

			return lastCommand.result;
		}

		std::vector<Type*> argTypes;
		argTypes.reserve(args.size());
		for (auto& arg : args) {
			argTypes.emplace_back(env.getType(arg.valueType));
		}

		auto& funcInfo = *env.estimateFunction("operator " + node.value.str, argTypes);

		for (auto i = 0u; i < argTypes.size(); ++i) {
			if (args[i].valueType != funcInfo.argTypes[i]->id()) {
				auto stack = info.getFreeStack();
				auto& command = list.emplace_back();
				command.name = funcInfo.argTypes[i]->castTargetStr();

				command.result.type = Variable::Type::Stack;
				command.result.stack = stack;
				command.result.valueType = funcInfo.argTypes[i]->id();

				command.args[i] = args[i];
				args[i] = command.result;
			}
		}

		auto& command = list.emplace_back();
		command.name = "operator " + node.value.str;

		command.result.type = Variable::Type::Stack;
		command.result.stack = info.getFreeStack();
		command.result.valueType = funcInfo.returnType->id();

		command.args[0] = args[0];
		command.args[1] = args[1];

		return command.result;
	}
	case Ketl::Parser::Node::Type::Number: {
		Variable variable;
		variable.type = Variable::Type::Literal;
		variable.literal = node.value;
		variable.valueType = node.value.asStrType();
		return variable;
	}
	case Ketl::Parser::Node::Type::Id: {
		Variable variable;
		variable.type = Variable::Type::Global;
		variable.id = node.value.str;
		auto* type = env.getGlobalType(variable.id);
		variable.valueType = type != nullptr ? type->id() : "";
		return variable;
	}
	}

	return {};
}
