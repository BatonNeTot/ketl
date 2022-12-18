/*🍲Ketl🍲*/
#include "analyzer.h"

#include "parser_nodes.h"

#include "linker.h"

#include "ketl.h"

#include <unordered_set>
#include <unordered_map>
#include <sstream>

namespace Ketl {

	static std::unordered_map<std::string, bool> precedenceLeftToRight = {
		std::make_pair<std::string, bool>("precedence-5-expression", false),
		std::make_pair<std::string, bool>("precedence-4-expression", false),
		std::make_pair<std::string, bool>("precedence-3-expression", true),
		std::make_pair<std::string, bool>("precedence-2-expression", true),
	};

	std::vector<uint8_t> Analyzer::proceed(const std::string& source) {
		auto commandsNode = _parser.proceed(source);
		Scope scope;
		return proceed(scope, commandsNode.get());
	}

	std::vector<uint8_t> Analyzer::proceed(Scope& scope, const Node* commandsNode) {
		std::list<std::unique_ptr<ByteVariable>> variables;
		return proceed(scope, variables, commandsNode);
	}

	std::vector<uint8_t> Analyzer::proceed(Scope& scope, std::list<std::unique_ptr<ByteVariable>>& variables, const Node* commandsNode) {
		std::vector<ByteVariable*> stack;
		std::vector<uint64_t> stackData;
		uint64_t lastStackSize = 0u;

		// collect arguments information
		for (auto it = commandsNode->children().begin(), end = commandsNode->children().end(); it != end; ++it) {
			proceedCommands(*it->get()->children().front(), scope, variables, stack);

			stackData.emplace_back(stack.size() - lastStackSize);
			for (auto i = lastStackSize; i < stack.size(); ++i) {
				stackData.emplace_back(stack[i]->index);
			}
			stackData.emplace_back(stack.size());
			stack.clear();
			// TODO empty stack only till 'id' part
			// empty id part when scope is ended
		}

		std::vector<uint8_t> byteData;

		insert(byteData, uint64_t(variables.size()));
		for (const auto& variable : variables) {
			variable->binarize(byteData);
		}

		for (const auto& data : stackData) {
			insert(byteData, data);
		}

		return byteData;
	}

	Analyzer::ByteVariable* Analyzer::proceedCommands(const Node& nodeId, 
		Scope& scope, std::list<std::unique_ptr<ByteVariable>>& args, std::vector<ByteVariable*>& stack) {
		if (nodeId.id() == "define-variable") {
			auto& definitionNode = nodeId.children().front();

			auto type = proceedType(*definitionNode->children()[0]);
			auto variableId = definitionNode->children()[1]->value();

			auto variable = std::make_unique<ByteVariableDefineVariable>(0);
			variable->id = variableId;
			variable->type = std::move(type);

			if (definitionNode->children().size() == 3) {
				auto& argumentsNode = definitionNode->children()[2]->children().front()->children().front();
				for (auto& argumentNode : argumentsNode->children()) {
					auto argumentVariable = proceedCommands(*argumentNode, scope, args, stack);
					variable->args.emplace_back(argumentVariable->index);
				}
			}

			variable->index = args.size();
			args.emplace_back(std::move(variable));

			return nullptr;
		}

		if (nodeId.id() == "function-definition") {
			auto& definitionNode = nodeId.children().front();
			// declaration
			auto& declarationNode = definitionNode->children().front()->children().front();

			// id and return type;
			auto functionOutputTypeId = declarationNode->children()[0]->value();
			auto functionId = declarationNode->children()[1]->value();

			auto variable = std::make_unique<ByteVariableDefineFunction>(args.size());
			variable->id = functionId;
			variable->returnType = proceedType(*declarationNode->children()[0]);
			// return argument

			/*
			auto returnVar = std::make_unique<ByteVariable>();
			returnVar->type = functionOutputType;
			functionAnalyze->arguments.emplace_back(std::move(returnVar));
			*/ // TODO return

			// arguments
			Scope functionScope;
			std::list<std::unique_ptr<ByteVariable>> variables;

			if (declarationNode->children().size() == 3) {
				auto& functionArgumentsNode = declarationNode->children()[2]->children().front();
				for (auto it = functionArgumentsNode->children().begin(), end = functionArgumentsNode->children().end(); it != end; ++it) {
					auto& argument = it->get()->children().front();
					auto argumentType = proceedType(*argument->children()[0]);
					auto argumentId = std::string(argument->children()[1]->value());

					auto argumentVar = std::make_unique<ByteVariableFunctionArgument>(variables.size());

					auto& scopeVariable = functionScope.map.try_emplace(argumentId).first->second;
					scopeVariable.byteVariable = variables.emplace_back(std::move(argumentVar)).get();

					variable->types.emplace_back(std::move(argumentType));
				}
			}

			// commands
			auto& commands = definitionNode->children()[1];

			variable->bytedata = proceed(functionScope, variables, commands->children().front().get());

			return args.emplace_back(std::move(variable)).get();
		}

		if (nodeId.id() == "primary") {
			auto* nodePtr = nodeId.children().front().get();
			switch (nodePtr->type()) {
			case Ketl::ValueType::Id: {
				// TODO push id right to the left of == not on top of the stack, but on top of 'ids' part;
				auto variableId = std::string(nodePtr->value());
				auto it = scope.map.find(variableId);
				if (it != scope.map.end()) {
					return it->second.byteVariable;
				}

				auto variable = std::make_unique<ByteVarialbeId>(args.size());
				variable->id = variableId;
				return args.emplace_back(std::move(variable)).get();
			}
			case Ketl::ValueType::Number: {
				auto variable = std::make_unique<ByteVariableLiteralFloat64>(args.size());
				variable->value = std::stod(std::string(nodePtr->value()));
				return args.emplace_back(std::move(variable)).get();
			}
			case Ketl::ValueType::Operator: {
				return proceedCommands(*nodePtr, scope, args, stack);
			}
			}

			return nullptr;
		}

		if (nodeId.id() == "precedence-1-expression") {
			auto& node = *nodeId.children().front();
			if (node.children().size() == 1) {
				return proceedCommands(*node.children().front(), scope, args, stack);
			}
			auto& nodeOperator = node.children().front()->children().front();

			// function call
			if (nodeOperator->children().front()->value() == "(") {
				auto function = std::make_unique<ByteVariableFunction>(0);

				stack.emplace_back(function.get());
				auto* argumentTarget = proceedCommands(*node.children().back(), scope, args, stack);
				function->function = argumentTarget->index;

				auto& nodeArguments = nodeOperator->children().back();
				for (auto& functionArgumentNode : nodeArguments->children().front()->children()) {
					auto* functionArgument = proceedCommands(*functionArgumentNode, scope, args, stack);
					function->args.emplace_back(functionArgument->index);
				}
				
				function->index = args.size();
				auto functionVariablePtr = args.emplace_back(std::move(function)).get();


				return functionVariablePtr;
			}

			return nullptr;
		}

		std::string id(nodeId.id());
		auto leftToRight = precedenceLeftToRight.find(id)->second;
		auto& node = *nodeId.children().front();

		if (leftToRight) {
			return proceedLtrBinary(node.children().crbegin(), node.children().crend(), scope, args, stack);
		}
		else {
			return proceedRtlBinary(node.children().cbegin(), node.children().cend(), scope, args, stack);
		}
	}

	Analyzer::ByteVariable* Analyzer::proceedLtrBinary(
		std::vector<std::unique_ptr<Node>>::const_reverse_iterator begin, std::vector<std::unique_ptr<Node>>::const_reverse_iterator end,
		Scope& scope, std::list<std::unique_ptr<Analyzer::ByteVariable>>& args, std::vector<Analyzer::ByteVariable*>& stack) {
		auto it = begin;
		auto rightArgIt = it;
		++it;

		if (it == end) {
			return proceedCommands(**rightArgIt, scope, args, stack);
		}

		auto op = it->get()->value();
		++it;

		auto opArg = std::make_unique<ByteVariableBinaryOperator>(0);
		opArg->code = OperatorCodes::Plus; // TODO actual code choosing

		stack.emplace_back(opArg.get());
			
		auto leftArg = proceedLtrBinary(it, end, scope, args, stack);
		auto rightArg = proceedCommands(**rightArgIt, scope, args, stack);

		opArg->firstArg = leftArg->index;
		opArg->secondArg = rightArg->index;
		opArg->index = args.size();

		return args.emplace_back(std::move(opArg)).get();
	}

	Analyzer::ByteVariable* Analyzer::proceedRtlBinary(
		std::vector<std::unique_ptr<Node>>::const_iterator begin, std::vector<std::unique_ptr<Node>>::const_iterator end,
		Scope& scope, std::list<std::unique_ptr<Analyzer::ByteVariable>>& args, std::vector<Analyzer::ByteVariable*>& stack) {
		auto it = begin;
		auto leftArgIt = it;
		++it;

		if (it == end) {
			return proceedCommands(**leftArgIt, scope, args, stack);
		}

		auto op = it->get()->value();
		++it;
		
		auto opArg = std::make_unique<ByteVariableBinaryOperator>(0);
		opArg->code = OperatorCodes::Assign; // TODO actual code choosing

		stack.emplace_back(opArg.get());

		auto rightArg = proceedRtlBinary(it, end, scope, args, stack);
		auto leftArg = proceedCommands(**leftArgIt, scope, args, stack);

		opArg->firstArg = leftArg->index;
		opArg->secondArg = rightArg->index;
		opArg->index = args.size();

		return args.emplace_back(std::move(opArg)).get();
	}

	std::unique_ptr<Analyzer::ByteType> Analyzer::proceedType(const Node& typeNode) {
		return std::make_unique<ByteTypeBody>(std::string(typeNode.value()));
	}

}
