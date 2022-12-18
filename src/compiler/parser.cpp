/*🍲Ketl🍲*/
#include "parser.h"

#include "bnf_nodes.h"
#include "lexer.h"

#include <format>

namespace Ketl {

	Parser::Parser() {

		_manager.insert("type-extra-arguments", std::make_unique<BnfNodeOr>(
			std::make_unique<BnfNodeConcat>(
				std::make_unique<BnfNodeLiteral>(",", true),
				std::make_unique<BnfNodeId>("type-arguments")
				),
			std::make_unique<BnfNodeLiteral>("", true)
			));
		_manager.insert("type-arguments", std::make_unique<BnfNodeConcat>(
			std::make_unique<BnfNodeId>("type"),
			std::make_unique<BnfNodeId>("type-extra-arguments")
			));

		_manager.insert("type", std::make_unique<BnfNodeConcat>(
			std::make_unique<BnfNodeOr>(
				std::make_unique<BnfNodeLeaf>(BnfNodeLeaf::Type::Id),
				std::make_unique<BnfNodeConcat>(
					std::make_unique<BnfNodeLiteral>("const"),
					std::make_unique<BnfNodeId>("type")
					),
				std::make_unique<BnfNodeConcat>(
					std::make_unique<BnfNodeLiteral>("(", true),
					std::make_unique<BnfNodeId>("type"),
					std::make_unique<BnfNodeLiteral>(")", true)
					)
				),
			std::make_unique<BnfNodeOr>(
				std::make_unique<BnfNodeLiteral>("&&"),
				std::make_unique<BnfNodeLiteral>("&"),
				std::make_unique<BnfNodeConcat>(
					std::make_unique<BnfNodeLiteral>("(", true),
					std::make_unique<BnfNodeOr>(
						std::make_unique<BnfNodeId>("type-arguments"),
						std::make_unique<BnfNodeLiteral>("", true)
						),
					std::make_unique<BnfNodeLiteral>(")", true)
					),
				std::make_unique<BnfNodeLiteral>("", true)
				)
			));

		_manager.insert("function-declaration-argument", std::make_unique<BnfNodeOr>(
			std::make_unique<BnfNodeConcat>(
				std::make_unique<BnfNodeId>("type"),
				std::make_unique<BnfNodeLeaf>(BnfNodeLeaf::Type::Id)
				),
			std::make_unique<BnfNodeId>("type")
			));

		_manager.insert("function-declaration-extra-arguments", std::make_unique<BnfNodeOr>(
			std::make_unique<BnfNodeConcat>(
				std::make_unique<BnfNodeLiteral>(",", true),
				std::make_unique<BnfNodeId>("function-declaration-argument"),
				std::make_unique<BnfNodeId>("function-declaration-extra-arguments")
				),
			std::make_unique<BnfNodeLiteral>("", true)
			));

		_manager.insert("function-declaration-arguments", std::make_unique<BnfNodeConcat>(
			std::make_unique<BnfNodeId>("function-declaration-argument"),
			std::make_unique<BnfNodeId>("function-declaration-extra-arguments")
			));

		_manager.insert("function-declaration", std::make_unique<BnfNodeConcat>(
			std::make_unique<BnfNodeId>("type"),
			std::make_unique<BnfNodeLeaf>(BnfNodeLeaf::Type::Id),
			std::make_unique<BnfNodeLiteral>("(", true),
			std::make_unique<BnfNodeOr>(
				std::make_unique<BnfNodeId>("function-declaration-arguments"),
				std::make_unique<BnfNodeLiteral>("", true)
				),
			std::make_unique<BnfNodeLiteral>(")", true)
			));

		_manager.insert("function-definition", std::make_unique<BnfNodeConcat>(
			std::make_unique<BnfNodeId>("function-declaration"),
			std::make_unique<BnfNodeId>("brackets-commands")
			));

		_manager.insert("primary", std::make_unique<BnfNodeOr>(
			std::make_unique<BnfNodeLeaf>(BnfNodeLeaf::Type::Id),
			std::make_unique<BnfNodeLeaf>(BnfNodeLeaf::Type::Number),
			std::make_unique<BnfNodeLeaf>(BnfNodeLeaf::Type::String),
			std::make_unique<BnfNodeConcat>(
				std::make_unique<BnfNodeLiteral>("(", true),
				std::make_unique<BnfNodeId>("expression"),
				std::make_unique<BnfNodeLiteral>(")", true)
				),
			std::make_unique<BnfNodeId>("function-definition")
			));

		_manager.insert("function-extra-arguments", std::make_unique<BnfNodeOr>(
			std::make_unique<BnfNodeConcat>(
				std::make_unique<BnfNodeLiteral>(",", true),
				std::make_unique<BnfNodeId>("primary"),
				std::make_unique<BnfNodeId>("function-extra-arguments")
				),
			std::make_unique<BnfNodeLiteral>("", true)
			));
		_manager.insert("function-arguments", std::make_unique<BnfNodeConcat>(
			std::make_unique<BnfNodeId>("expression"),
			std::make_unique<BnfNodeId>("function-extra-arguments")
			));
		_manager.insert("precedence-1-operator", std::make_unique<BnfNodeOr>(
			std::make_unique<BnfNodeConcat>(
				std::make_unique<BnfNodeLiteral>("("),
				std::make_unique<BnfNodeOr>(
					std::make_unique<BnfNodeId>("function-arguments"),
					std::make_unique<BnfNodeLiteral>("", true)
					),
				std::make_unique<BnfNodeLiteral>(")", true)
				)
			));
		_manager.insert("precedence-1-expression", std::make_unique<BnfNodeConcat>(
			std::make_unique<BnfNodeId>("primary"),
			std::make_unique<BnfNodeOr>(
				std::make_unique<BnfNodeId>("precedence-1-operator"),
				std::make_unique<BnfNodeLiteral>("", true)
				)
			));

		insertPredence(std::make_unique<BnfNodeOr>(
			std::make_unique<BnfNodeLiteral>("*"),
			std::make_unique<BnfNodeLiteral>("/")
			), "precedence-2-expression", "precedence-2-extra", "precedence-1-expression");

		insertPredence(std::make_unique<BnfNodeOr>(
			std::make_unique<BnfNodeLiteral>("+"),
			std::make_unique<BnfNodeLiteral>("-")
			), "precedence-3-expression", "precedence-3-extra", "precedence-2-expression");

		insertPredence(std::make_unique<BnfNodeOr>(
			std::make_unique<BnfNodeLiteral>("=="),
			std::make_unique<BnfNodeLiteral>("!=")
			), "precedence-4-expression", "precedence-4-extra", "precedence-3-expression");

		insertPredence(std::make_unique<BnfNodeOr>(
			std::make_unique<BnfNodeLiteral>("=")
			), "precedence-5-expression", "precedence-5-extra", "precedence-4-expression");

		_manager.insert("expression", std::make_unique<BnfNodeId>("precedence-5-expression"));

		_manager.insert("expression-with-end-symbol", std::make_unique<BnfNodeConcat>(
			std::make_unique<BnfNodeOr>(
				std::make_unique<BnfNodeId>("expression"),
				std::make_unique<BnfNodeLiteral>("")
				),
			std::make_unique<BnfNodeLiteral>(";", true)
			));
		_manager.insert("return", std::make_unique<BnfNodeConcat>(
			std::make_unique<BnfNodeLiteral>("return", true),
			std::make_unique<BnfNodeOr>(
				std::make_unique<BnfNodeId>("expression"),
				std::make_unique<BnfNodeLiteral>("")
				),
			std::make_unique<BnfNodeLiteral>(";", true)
			));

		_manager.insert("define-variable-extra-arguments", std::make_unique<BnfNodeOr>(
			std::make_unique<BnfNodeConcat>(
				std::make_unique<BnfNodeLiteral>(",", true),
				std::make_unique<BnfNodeId>("define-variable-arguments")
				),
			std::make_unique<BnfNodeLiteral>("", true)
			));
		_manager.insert("define-variable-arguments", std::make_unique<BnfNodeConcat>(
			std::make_unique<BnfNodeId>("expression"),
			std::make_unique<BnfNodeId>("define-variable-extra-arguments")
			));

		_manager.insert("define-variable", std::make_unique<BnfNodeConcat>(
			std::make_unique<BnfNodeId>("type"),
			std::make_unique<BnfNodeLeaf>(BnfNodeLeaf::Type::Id),
			std::make_unique<BnfNodeOr>(
				std::make_unique<BnfNodeConcat>(
					std::make_unique<BnfNodeLiteral>("{", true),
					std::make_unique<BnfNodeOr>(
						std::make_unique<BnfNodeId>("define-variable-arguments"),
						std::make_unique<BnfNodeLiteral>("", true)
						),
					std::make_unique<BnfNodeLiteral>("}", true)
					),
				std::make_unique<BnfNodeLiteral>("")
				),
			std::make_unique<BnfNodeLiteral>(";", true)
			));

		_manager.insert("command", std::make_unique<BnfNodeOr>(
			std::make_unique<BnfNodeId>("expression-with-end-symbol"),
			std::make_unique<BnfNodeId>("return"),
			std::make_unique<BnfNodeId>("define-variable"),
			std::make_unique<BnfNodeId>("function-definition"),
			std::make_unique<BnfNodeId>("brackets-commands")
			));

		_manager.insert("brackets-commands", std::make_unique<BnfNodeConcat>(
			std::make_unique<BnfNodeLiteral>("{", true),
			std::make_unique<BnfNodeId>("several-commands"),
			std::make_unique<BnfNodeLiteral>("}", true)
			));

		_manager.insert("several-commands", std::make_unique<BnfNodeOr>(
			std::make_unique<BnfNodeConcat>(
				std::make_unique<BnfNodeId>("command"),
				std::make_unique<BnfNodeId>("several-commands")
				),
			std::make_unique<BnfNodeLiteral>("")
			));

		_manager.insert("block", std::make_unique<BnfNodeOr>(
			std::make_unique<BnfNodeId>("brackets-commands"),
			std::make_unique<BnfNodeId>("command")
			));

		_manager.preprocessNodes();
	}

	void Parser::insertPredence(std::unique_ptr<BnfNode>&& operators, const std::string& expression,
		const std::string& extra, const std::string& lowExpression) {
		_manager.insert(extra, std::make_unique<BnfNodeOr>(
			std::make_unique<BnfNodeConcat>(
				operators,
				std::make_unique<BnfNodeId>(lowExpression),
				std::make_unique<BnfNodeId>(extra)
				),
			std::make_unique<BnfNodeLiteral>("")
			));
		_manager.insert(expression, std::make_unique<BnfNodeConcat>(
			std::make_unique<BnfNodeId>(lowExpression),
			std::make_unique<BnfNodeId>(extra)
			));
	}

	std::unique_ptr<Node> Parser::proceed(const std::string& str) {
		static const std::string empty;
		_error = empty;
		Lexer lexer(str);

		std::list<Lexer::Token> list;
		while (lexer.hasNext()) {
			auto&& token = lexer.proceedNext();
			if (!token.value.empty()) {
				list.emplace_back(std::move(token));
			}
		}

		ProcessNode processNode;
		auto end = list.end();
		BnfIterator iterator(list.begin(), end);
		auto node = _manager.getById("several-commands");
		auto success = node->process(iterator, processNode);

		if (!success) {
			uint64_t row = 0u;
			uint64_t end = iterator ? static_cast<uint64_t>(iterator) : str.length();
			uint64_t lastAfterLine = 0u;
			uint64_t line = 0u;
			for (; row < end; ++row) {
				if (str[row] == '\n') {
					++line;
					lastAfterLine = row + 1;
				}
			}
			row -= lastAfterLine;

			if (end == str.length()) {
				_error = std::format("({},{}): unexpected EOF, expected {}",
					line, row, processNode.node->errorMsg());
			}
			else {
				auto itStr = iterator.value();
				if (iterator.type() == Lexer::Token::Type::Other) {
					itStr = itStr.substr(0, 1);
				}
				if (processNode.node == nullptr) {
					_error = std::format("({},{}): unexpected end of form, parsing {}",
						line, row, itStr);
				}
				else {
					_error = std::format("({},{}): expected {}, got {}",
						line, row, processNode.node->errorMsg(), itStr);
				}
			}

			return {};
		}

		return std::move(processNode.outputChildrenNodes.back());
	}

	class IRBlock : public IRNode {
	public:

		IRBlock(std::vector<std::unique_ptr<IRNode>>&& commands) 
			: _commands(std::move(commands)) {}

		uint64_t childCount() const override {
			return _commands.size();
		}

		const std::unique_ptr<IRNode>& child(uint64_t index) const override {
			return _commands[index];
		}

	private:
		std::vector<std::unique_ptr<IRNode>> _commands;
	};

	std::unique_ptr<IRNode> Parser::parseTree(const std::string& str) {
		auto parsedRoot = proceed(str);
		if (!parsedRoot) {
			return {};
		}

		return parseBlock(*parsedRoot);
	}

	std::unique_ptr<IRNode> Parser::parseBlock(const Node& block) {
		// collect arguments information
		std::vector<std::unique_ptr<IRNode>> commands;
		for (auto it = block.children().begin(), end = block.children().end(); it != end; ++it) {
			auto command = parseCommand(*it->get()->children().front());
			if (!command) {
				return {};
			}
			commands.emplace_back(std::move(command));
		}
		return std::make_unique<IRBlock>(std::move(commands));
	}

	static std::unordered_map<std::string, bool> precedenceLeftToRight = {
		std::make_pair<std::string, bool>("precedence-5-expression", false),
		std::make_pair<std::string, bool>("precedence-4-expression", false),
		std::make_pair<std::string, bool>("precedence-3-expression", true),
		std::make_pair<std::string, bool>("precedence-2-expression", true),
	};

	class IRVariable : public IRNode {
	public:

		IRVariable(const std::string_view& id)
			: _id(id) {}
		IRVariable(const std::string_view& id, std::shared_ptr<TypeTemplate> type)
			: _id(id), _type(std::move(type)) {}

		const std::shared_ptr<TypeTemplate>& type() const override {
			return _type;
		}

		uint64_t childCount() const override {
			return 0;
		}

	private:
		std::string _id;
		std::shared_ptr<TypeTemplate> _type;
	};

	template <class T>
	class IRLiteral : public IRNode {
	public:

		IRLiteral(T&& value)
			: _value(std::move(value)) {}

		uint64_t childCount() const override {
			return 0;
		}

	private:
		T _value;
	};

	class IRCall : public IRNode {
	public:

		IRCall(std::unique_ptr<IRNode>&& callable, std::vector<std::unique_ptr<IRNode>>&& args)
			: _callable(std::move(callable)), _args(std::move(args)) {}

		uint64_t childCount() const override {
			return _args.size() + 1;
		}

		const std::unique_ptr<IRNode>& child(uint64_t index) const override {
			return index == _args.size() ? _callable : _args[index];
		}

	private:
		std::unique_ptr<IRNode> _callable;
		std::vector<std::unique_ptr<IRNode>> _args;
	};

	class IRReturn : public IRNode {
	public:

		IRReturn(std::unique_ptr<IRNode>&& value)
			: _value(std::move(value)) {}

		uint64_t childCount() const override {
			return 1;
		}

		const std::unique_ptr<IRNode>& child(uint64_t index) const override {
			return _value;
		}

	private:
		std::unique_ptr<IRNode> _value;
	};

	class IRDefinition : public IRNode {
	public:

		IRDefinition(const std::string_view& id, std::shared_ptr<TypeTemplate> type, std::vector<std::unique_ptr<IRNode>>&& args)
			: _id(id), _type(std::move(type)), _args(std::move(args)) {}

		uint64_t childCount() const override {
			return _args.size();
		}

		const std::unique_ptr<IRNode>& child(uint64_t index) const override {
			return _args[index];
		}

	private:
		std::string_view _id;
		std::shared_ptr<TypeTemplate> _type;
		std::vector<std::unique_ptr<IRNode>> _args;
	};

	class IRFunction : public IRNode {
	public:

		struct Argument {
			Argument(std::shared_ptr<TypeTemplate> type_, const std::string_view& id_)
				: type(std::move(type_)), id(id_) {}

			std::shared_ptr<TypeTemplate> type;
			std::string_view id;
		};

		IRFunction(std::shared_ptr<TypeTemplate> returnType, std::vector<Argument>&& args)
			: _returnType(std::move(returnType)), _args(std::move(args)) {}

		uint64_t childCount() const override {
			return 0;
		}

	private:
		std::shared_ptr<TypeTemplate> _returnType;
		std::vector<Argument> _args;
	};

	class TypeFunction : public TypeTemplate {
	public:

		TypeFunction(std::shared_ptr<TypeTemplate> returnType, std::vector<std::shared_ptr<TypeTemplate>>&& argTypes)
			: _returnType(std::move(returnType)), _argTypes() {}

	private:
		std::shared_ptr<TypeTemplate> _returnType;
		std::vector<std::shared_ptr<TypeTemplate>> _argTypes;
	};

	std::unique_ptr<IRNode> Parser::parseCommand(const Node& commandId) {
		/// define section
		if (commandId.id() == "define-variable") {
			auto& definitionNode = commandId.children().front();

			auto type = parseType(*definitionNode->children()[0]);
			auto variableId = definitionNode->children()[1]->value();

			std::vector<std::unique_ptr<IRNode>> args;

			if (definitionNode->children().size() == 3) {
				auto& argumentsNode = definitionNode->children()[2]->children().front()->children().front();
				for (auto& argumentNode : argumentsNode->children()) {
					auto argumentVariable = parseExpression(*argumentNode);
					args.emplace_back(std::move(argumentVariable));
				}
			}

			auto definition = std::make_unique<IRDefinition>(variableId, std::move(type), std::move(args));

			return definition;
		}

		if (commandId.id() == "function-definition") {
			auto& definitionNode = commandId.children().front();
			// declaration
			auto& declarationNode = definitionNode->children().front()->children().front();

			// id and return type;
			auto functionId = declarationNode->children()[1]->value();
			auto returnType = parseType(*declarationNode->children()[0]);

			std::vector<std::shared_ptr<TypeTemplate>> functionArgTypes;
			std::vector<IRFunction::Argument> args;

			// arguments
			if (declarationNode->children().size() == 3) {
				auto& functionArgumentsNode = declarationNode->children()[2]->children().front();

				functionArgTypes.reserve(functionArgumentsNode->children().size());
				args.reserve(functionArgumentsNode->children().size());
				for (auto it = functionArgumentsNode->children().begin(), end = functionArgumentsNode->children().end(); it != end; ++it) {
					auto& argument = it->get()->children().front();
					auto argumentType = parseType(*argument->children()[0]);
					auto argumentId = argument->children()[1]->value();

					functionArgTypes.emplace_back(argumentType);
					args.emplace_back(std::move(argumentType), argumentId);
				}
			}

			// commands
			auto commands = parseBlock(*definitionNode->children()[1]->children().front().get());

			auto function = std::make_unique<IRFunction>(returnType, std::move(args));
			std::vector<std::unique_ptr<IRNode>> definitionArgs;
			definitionArgs.reserve(1);
			definitionArgs.emplace_back(std::move(function));
			auto functionDefinition = std::make_unique<IRDefinition>(functionId,
				std::make_shared<TypeFunction>(std::move(returnType), std::move(functionArgTypes)),
				std::move(definitionArgs));

			return functionDefinition;
		}

		/// return
		if (commandId.id() == "return") {
			auto* nodePtr = commandId.children().front().get();

			auto returnVariable = parseExpression(*nodePtr->children().front().get());

			return std::make_unique<IRReturn>(std::move(returnVariable));
		}

		return parseExpression(commandId);
	}

	std::unique_ptr<IRNode> Parser::parseExpression(const Node& expressionId) {
		/// expressions

		if (expressionId.id() == "primary") {
			auto* nodePtr = expressionId.children().front().get();
			switch (nodePtr->type()) {
			case Ketl::ValueType::Id: {
				// TODO push id right to the left of == not on top of the stack, but on top of 'ids' part;
				auto variable = std::make_unique<IRVariable>(nodePtr->value());
				return variable;
			}
			case Ketl::ValueType::Number: {
				auto variable = std::make_unique<IRLiteral<double>>(std::stod(std::string(nodePtr->value())));
				return variable;
			}
			case Ketl::ValueType::Operator: {
				return parseExpression(*nodePtr);
			}
			}

			return nullptr;
		}

		if (expressionId.id() == "precedence-1-expression") {
			auto& node = *expressionId.children().front();
			if (node.children().size() == 1) {
				return parseExpression(*node.children().front());
			}
			auto& nodeOperator = node.children()[1]->children().front();

			// function call
			if (nodeOperator->children()[0]->value() == "(") {
				auto callable = parseExpression(*node.children()[0]);

				std::vector<std::unique_ptr<IRNode>> args;
				if (nodeOperator->children().size() > 1) {
					auto& nodeArguments = nodeOperator->children().back();
					auto& actualNodeArguments = nodeArguments->children().front();
					args.reserve(actualNodeArguments->children().size());
					for (auto& argumentNode : actualNodeArguments->children()) {
						auto argument = parseExpression(*argumentNode);
						args.emplace_back(std::move(argument));
					}
				}

				auto call = std::make_unique<IRCall>(std::move(callable), std::move(args));

				return call;
			}

			return nullptr;
		}

		std::string id(expressionId.id());
		auto leftToRight = precedenceLeftToRight.find(id)->second;
		auto& node = *expressionId.children().front();

		if (leftToRight) {
			return parseLtrBinary(node.children().crbegin(), node.children().crend());
		}
		else {
			return parseRtlBinary(node.children().cbegin(), node.children().cend());
		}
	}

	class IRBinaryOperator : public IRNode {
	public:

		IRBinaryOperator(OperatorCode op, std::unique_ptr<IRNode>&& lhs, std::unique_ptr<IRNode>&& rhs)
			: _op(op), _lhs(std::move(lhs)), _rhs(std::move(rhs)) {}

		bool resolveType(Context& context) override {
			auto lhsSuccess = _lhs->resolveType(context);
			auto rhsSuccess = _rhs->resolveType(context);

			if (!lhsSuccess && rhsSuccess && _op == OperatorCode::Assign) {
				auto var = dynamic_cast<IRVariable*>(_lhs.get());
				if (var) {
					auto test = 0;
					return true;
				}
			} 
			
			if (!lhsSuccess || !rhsSuccess) {
				return false;
			}
			auto test = 0;


			return true;
		};

		const std::shared_ptr<TypeTemplate>& type() const override {
			return _type;
		}

		uint64_t childCount() const override {
			return 2;
		}

		const std::unique_ptr<IRNode>& child(uint64_t index) const override {
			return index == 0 ? _lhs : _rhs;
		}

	private:

		OperatorCode _op;
		std::shared_ptr<TypeTemplate> _type;
		std::unique_ptr<IRNode> _lhs;
		std::unique_ptr<IRNode> _rhs;
	};

	std::unique_ptr<IRNode> Parser::parseLtrBinary(
		std::vector<std::unique_ptr<Node>>::const_reverse_iterator begin, std::vector<std::unique_ptr<Node>>::const_reverse_iterator end) {
		auto it = begin;
		auto rightArgIt = it;
		++it;

		if (it == end) {
			return parseExpression(**rightArgIt);
		}

		auto op = it->get()->value();
		++it;

		auto code = OperatorCode::Plus; // TODO actual code choosing

		auto leftArg = parseLtrBinary(it, end);
		auto rightArg = parseExpression(**rightArgIt);

		auto opNode = std::make_unique<IRBinaryOperator>(code, std::move(leftArg), std::move(rightArg));

		return opNode;
	}

	std::unique_ptr<IRNode> Parser::parseRtlBinary(
		std::vector<std::unique_ptr<Node>>::const_iterator begin, std::vector<std::unique_ptr<Node>>::const_iterator end) {
		auto it = begin;
		auto leftArgIt = it;
		++it;

		if (it == end) {
			return parseExpression(**leftArgIt);
		}

		auto op = it->get()->value();
		++it;

		auto code = OperatorCode::Assign; // TODO actual code choosing

		auto rightArg = parseRtlBinary(it, end);
		auto leftArg = parseExpression(**leftArgIt);

		auto opNode = std::make_unique<IRBinaryOperator>(code, std::move(leftArg), std::move(rightArg));

		return opNode;
	}

	class TypeId : public TypeTemplate {
	public:

		TypeId(const std::string_view& id)
			: _id(id) {}

	private:

		std::string_view _id;
	};

	class TypeWrapper : public TypeTemplate {
	public:

		TypeWrapper(std::shared_ptr<TypeTemplate> type)
			: _type(std::move(type)) {}

	private:

		std::shared_ptr<TypeTemplate> _type;
	};

	class TypeConst : public TypeWrapper {
	public:
		TypeConst(std::shared_ptr<TypeTemplate> type)
			: TypeWrapper(std::move(type)) {}
	};

	class TypeLRef : public TypeWrapper {
	public:
		TypeLRef(std::shared_ptr<TypeTemplate> type)
			: TypeWrapper(std::move(type)) {}
	};

	class TypeRRef : public TypeWrapper {
	public:
		TypeRRef(std::shared_ptr<TypeTemplate> type)
			: TypeWrapper(std::move(type)) {}
	};

	std::shared_ptr<TypeTemplate> Parser::parseType(const Node& typeNode) {
		std::shared_ptr<TypeTemplate> type;

		auto& typeBaseNode = *typeNode.children()[0];
		if (typeBaseNode.children().size() == 0) {
			type = std::make_shared<TypeId>(typeBaseNode.value());
		}
		else if (typeBaseNode.children().size() == 2) {
			type = std::make_shared<TypeConst>(parseType(*typeBaseNode.children()[1]));
		}
		if (typeNode.children().size() == 2) {
			auto& opNode = *typeNode.children()[1];
			if (opNode.value() == "&") {
				type = std::make_shared<TypeLRef>(std::move(type));
			}
			else if (opNode.value() == "&&") {
				type = std::make_shared<TypeRRef>(std::move(type));
			}
		}
		return type;
	}

}