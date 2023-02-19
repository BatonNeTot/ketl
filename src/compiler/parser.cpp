/*🍲Ketl🍲*/
#include "parser.h"

#include "lexer.h"
#include "bnf_nodes.h"

#include "semantic_analyzer.h"

#include <format>

namespace Ketl {

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

		AnalyzerVar* produceInstructions(std::vector<RawInstruction>& instructions, AnalyzerContext& context) const override {
			for (auto& command : _commands) {
				command->produceInstructions(instructions, context);
			}
			return {}; 
		};

	private:
		std::vector<std::unique_ptr<IRNode>> _commands;
	};

	std::unique_ptr<IRNode> createBlockTree(const ProcessNode* info) {
		std::vector<std::unique_ptr<IRNode>> commands;

		auto it = info->firstChild;
		while (it) {
			commands.emplace_back(it->node->createIRTree(it));
			it = it->nextSibling;
		}

		return std::make_unique<IRBlock>(std::move(commands));
	}

	std::unique_ptr<IRNode> proxyTree(const ProcessNode* info) {
		auto child = info->firstChild;
		return child->node->createIRTree(child);
	}

	class IRBinaryOperator : public IRNode {
	public:

		IRBinaryOperator(OperatorCode op, bool ltr, std::unique_ptr<IRNode>&& lhs, std::unique_ptr<IRNode>&& rhs)
			: _op(op), _ltr(ltr), _lhs(std::move(lhs)), _rhs(std::move(rhs)) {}

		bool resolveType(Context& context) override {
			auto lhsSuccess = _lhs->resolveType(context);
			auto rhsSuccess = _rhs->resolveType(context);

			if (!lhsSuccess && rhsSuccess && _op == OperatorCode::Assign) {
				auto varId = _lhs->id();
				if (!varId.empty()) {
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

		AnalyzerVar* produceInstructions(std::vector<RawInstruction>& instructions, AnalyzerContext& context) const override {
			AnalyzerVar* lhsVar;
			AnalyzerVar* rhsVar;

			if (_ltr) {
				lhsVar = _lhs->produceInstructions(instructions, context);
				rhsVar = _rhs->produceInstructions(instructions, context);
			}
			else {
				rhsVar = _rhs->produceInstructions(instructions, context);
				lhsVar = _lhs->produceInstructions(instructions, context);
			}

			auto& instruction = instructions.emplace_back();
			instruction.firstVar = lhsVar;
			instruction.secondVar = rhsVar;
			instruction.outputVar = context.createTemporaryVar(sizeof(int64_t));

			switch (_op) {
			case OperatorCode::Plus: {
				instruction.code = Instruction::Code::AddInt64;
				break;
			}
			case OperatorCode::Multiply: {
				instruction.code = Instruction::Code::MultyInt64;
				break;
			}
			case OperatorCode::Assign: {
				instruction.code = Instruction::Code::Assign;
				break;
			}
			default: {
				auto test = 0;
				(void)test;
				break;
			}
			}
			return instruction.outputVar;
		};

	private:

		OperatorCode _op;
		bool _ltr;
		std::shared_ptr<TypeTemplate> _type;
		std::unique_ptr<IRNode> _lhs;
		std::unique_ptr<IRNode> _rhs;
	};

	std::unique_ptr<IRNode> createRtlTree(const ProcessNode* info) {
		auto rightArgNode = info->firstChild;

		while (rightArgNode->nextSibling) {
			rightArgNode = rightArgNode->nextSibling;
		}

		auto rightArg = rightArgNode->node->createIRTree(rightArgNode);
		
		do {
			auto opNode = rightArgNode->prevSibling;
			auto op = opNode->node->value(opNode->iterator);
			(void)op; // TODO deduce operator code

			auto code = OperatorCode::Assign;

			auto leftArgNode = opNode->prevSibling;
			auto leftArg = leftArgNode->node->createIRTree(leftArgNode);

			rightArg = std::make_unique<IRBinaryOperator>(code, false, std::move(leftArg), std::move(rightArg));
			
			rightArgNode = leftArgNode;
		} while (rightArgNode->prevSibling);

		return rightArg;
	}

	std::unique_ptr<IRNode> createLtrTree(const ProcessNode* info) {
		auto leftArgNode = info->firstChild;

		auto leftArg = leftArgNode->node->createIRTree(leftArgNode);

		do {
			auto opNode = leftArgNode->nextSibling;
			auto op = opNode->node->value(opNode->iterator);
			(void)op; // TODO deduce operator code

			auto code = OperatorCode::Plus;
			if (op == "*") {
				code = OperatorCode::Multiply;
			}

			auto rightArgNode = opNode->nextSibling;
			auto rightArg = rightArgNode->node->createIRTree(rightArgNode);

			leftArg = std::make_unique<IRBinaryOperator>(code, true, std::move(leftArg), std::move(rightArg));

			leftArgNode = rightArgNode;
		} while (leftArgNode->nextSibling);

		return leftArg;
	}

	class IRVariable : public IRNode {
	public:

		IRVariable(const std::string_view& id)
			: _id(id) {}
		IRVariable(const std::string_view& id, std::shared_ptr<TypeTemplate> type)
			: _id(id), _type(std::move(type)) {}

		const std::shared_ptr<TypeTemplate>& type() const override {
			return _type;
		}

		const std::string& id() const override { 
			return _id; 
		}

		uint64_t childCount() const override {
			return 0;
		}

		AnalyzerVar* produceInstructions(std::vector<RawInstruction>& instructions, AnalyzerContext& context) const override {
			return context.createGlobalVar(_id);
		};

	private:
		std::string _id;
		std::shared_ptr<TypeTemplate> _type;
	};

	std::unique_ptr<IRNode> createVariable(const ProcessNode* info) {
		return std::make_unique<IRVariable>(info->node->value(info->iterator));
	}

	class IRLiteral : public IRNode {
	public:

		IRLiteral(const std::string_view& value)
			: _value(value) {}

		uint64_t childCount() const override {
			return 0;
		}

		AnalyzerVar* produceInstructions(std::vector<RawInstruction>& instructions, AnalyzerContext& context) const override {
			return context.createLiteralVar(_value);
		};

	private:
		std::string_view _value;
	};

	std::unique_ptr<IRNode> createLiteral(const ProcessNode* info) {
		return std::make_unique<IRLiteral>(info->node->value(info->iterator));
	}

	Parser::Parser() {
		// expression
		_nodes.try_emplace("primary", std::make_unique<NodeOr>(
			std::make_unique<NodeLeaf>(&createVariable, NodeLeaf::Type::Id),
			std::make_unique<NodeLeaf>(&createLiteral, NodeLeaf::Type::Number),
			std::make_unique<NodeLeaf>(NodeLeaf::Type::String),
			std::make_unique<NodeConcat>(true,
				std::make_unique<NodeLiteral>(true, "("),
				std::make_unique<NodeId>(true, "expression"),
				std::make_unique<NodeLiteral>(true, ")")
				)
			));

		_nodes.try_emplace("precedence-1-expression", std::make_unique<NodeConcat>(true,
			std::make_unique<NodeId>(true, "primary"),
			std::make_unique<NodeConditional>(
				std::make_unique<NodeConcat>(true,
					std::make_unique<NodeLiteral>(true, "("),
					std::make_unique<NodeConditional>(
						std::make_unique<NodeConcat>(true,
							std::make_unique<NodeId>(true, "expression"),
							std::make_unique<NodeRepeat>(
								std::make_unique<NodeConcat>(true,
									std::make_unique<NodeLiteral>(true, ","),
									std::make_unique<NodeId>(true, "expression")
									)
								)
							)
						),
					std::make_unique<NodeLiteral>(true, ")")
					)
				)
			)); 

		_nodes.try_emplace("precedence-2-expression", std::make_unique<NodeConcat>(true,
			std::make_unique<NodeId>(&proxyTree, true, "precedence-1-expression"),
			std::make_unique<NodeRepeat>(
				std::make_unique<NodeConcat>(true,
					std::make_unique<NodeOr>(
						std::make_unique<NodeLiteral>(false, "*"),
						std::make_unique<NodeLiteral>(false, "/")
						),
					std::make_unique<NodeId>(&proxyTree, true, "precedence-1-expression")
					)
				)
			));

		_nodes.try_emplace("precedence-3-expression", std::make_unique<NodeConcat>(true,
			std::make_unique<NodeId>(&createLtrTree, true, "precedence-2-expression"),
			std::make_unique<NodeRepeat>(
				std::make_unique<NodeConcat>(true,
					std::make_unique<NodeOr>(
						std::make_unique<NodeLiteral>(false, "+"),
						std::make_unique<NodeLiteral>(false, "-")
						),
					std::make_unique<NodeId>(&createLtrTree, true, "precedence-2-expression")
					)
				)
			));

		_nodes.try_emplace("precedence-4-expression", std::make_unique<NodeConcat>(true,
			std::make_unique<NodeId>(&createLtrTree, true, "precedence-3-expression"),
			std::make_unique<NodeRepeat>(
				std::make_unique<NodeConcat>(true,
					std::make_unique<NodeOr>(
						std::make_unique<NodeLiteral>(false, "=="),
						std::make_unique<NodeLiteral>(false, "!=")
						),
					std::make_unique<NodeId>(&createLtrTree, true, "precedence-3-expression")
					)
				)
			));

		_nodes.try_emplace("precedence-5-expression", std::make_unique<NodeConcat>(true,
			std::make_unique<NodeId>(&createLtrTree, true, "precedence-4-expression"),
			std::make_unique<NodeRepeat>(
				std::make_unique<NodeConcat>(true,
					std::make_unique<NodeOr>(
						std::make_unique<NodeLiteral>(false, "=")
						),
					std::make_unique<NodeId>(&createLtrTree, true, "precedence-4-expression")
					)
				)
			));

		_nodes.try_emplace("expression", std::make_unique<NodeId>(&createRtlTree, true, "precedence-5-expression"));

		// type
		_nodes.try_emplace("type", std::make_unique<NodeConcat>(false,
			std::make_unique<NodeOr>(
				std::make_unique<NodeLeaf>(NodeLeaf::Type::Id),
				std::make_unique<NodeConcat>(false,
					std::make_unique<NodeLiteral>(false, "const"),
					std::make_unique<NodeId>(false, "type")
					),
				std::make_unique<NodeConcat>(true,
					std::make_unique<NodeLiteral>(true, "("),
					std::make_unique<NodeId>(false, "type"),
					std::make_unique<NodeLiteral>(true, ")")
					)
				),
			std::make_unique<NodeConditional>(
				std::make_unique<NodeLiteral>(false, "&&"),
				std::make_unique<NodeLiteral>(false, "&")
				)
			));

		_nodes.try_emplace("command", std::make_unique<NodeOr>(
			// expression
			std::make_unique<NodeConcat>(false,
				// return
				std::make_unique<NodeConditional>(
					std::make_unique<NodeLiteral>(false, "return")
				),
				std::make_unique<NodeConditional>(
					std::make_unique<NodeId>(true, "expression")),
				std::make_unique<NodeLiteral>(true, ";")
				),
			// define variable
			std::make_unique<NodeConcat>(false,
				std::make_unique<NodeId>(false, "type"),
				std::make_unique<NodeLeaf>(NodeLeaf::Type::Id),
				std::make_unique<NodeConditional>(
					std::make_unique<NodeConcat>(false,
						std::make_unique<NodeLiteral>(true, "{"),
						std::make_unique<NodeConditional>(
							std::make_unique<NodeConcat>(true,
								std::make_unique<NodeId>(true, "expression"),
								std::make_unique<NodeRepeat>(
									std::make_unique<NodeConcat>(true,
										std::make_unique<NodeLiteral>(true, ","),
										std::make_unique<NodeId>(true, "expression")
										)
									)
								)
							),
						std::make_unique<NodeLiteral>(true, "}")
						)
					),
				std::make_unique<NodeLiteral>(true, ";")
				),
			// define function
			std::make_unique<NodeConcat>(false,
				std::make_unique<NodeId>(false, "type"),
				std::make_unique<NodeLeaf>(NodeLeaf::Type::Id),
				std::make_unique<NodeLiteral>(true, "("),
				std::make_unique<NodeConditional>(
					std::make_unique<NodeConcat>(false,
						std::make_unique<NodeId>(false, "type"),
						std::make_unique<NodeConditional>(
							std::make_unique<NodeLeaf>(NodeLeaf::Type::Id)
							),
						std::make_unique<NodeRepeat>(
							std::make_unique<NodeConcat>(true,
								std::make_unique<NodeLiteral>(true, ","),
								std::make_unique<NodeConcat>(false,
									std::make_unique<NodeId>(false, "type"),
									std::make_unique<NodeConditional>(
										std::make_unique<NodeLeaf>(NodeLeaf::Type::Id)
										)
									)
								)
							)
						)
					),
				std::make_unique<NodeLiteral>(true, ")"),
				std::make_unique<NodeLiteral>(true, "{"),
				std::make_unique<NodeId>(false, "several-commands"),
				std::make_unique<NodeLiteral>(true, "}")
				),
			// block
			std::make_unique<NodeConcat>(false,
				std::make_unique<NodeLiteral>(true, "{"),
				std::make_unique<NodeId>(false, "several-commands"),
				std::make_unique<NodeLiteral>(true, "}")
				)
			));

		_root = _nodes.try_emplace("several-commands", std::make_unique<NodeRepeat>( &createBlockTree,
			std::make_unique<NodeId>(true, "command")
			)).first->second.get();

		for (auto& nodePair : _nodes) {
			nodePair.second->postConstruct(_nodes);
		}
	}

	class ProcessStack {
	private:

		struct Node {
			ProcessNode value;
			Node* next = nullptr;
			Node* prev = nullptr;
		};

	public:

		ProcessStack() {
			_first = new Node();
			_top = _first;
		}
		~ProcessStack() {
			Node* tmp;
			for (auto it = _first; it != nullptr;) {
				tmp = it->next;
				delete it;
				it = tmp;
			}
		}

		void reset(Parser::Node* node, const BnfIterator& iterator) {
			_top = _first;
			_top->value.node = node;
			_top->value.iterator = iterator;
			_top->value.parent = nullptr;
		}

		void push(Parser::Node* node, const BnfIterator& iterator, ProcessNode* parent) {
			if (_top->next == nullptr) {
				_top->next = new Node();
				_top->next->prev = _top;
			}
			_top = _top->next;
			_top->value.node = node;
			_top->value.iterator = iterator;
			_top->value.parent = parent;
		}

		bool pop() {
			if (_top == nullptr) {
				return false;
			}
			_top = _top->prev;
			return _top != nullptr;
		}

		ProcessNode* top() const {
			return _top ? &_top->value : nullptr;
		}

		bool empty() const {
			return _top == nullptr;
		}

	private:
		Node* _first = nullptr;
		Node* _top = nullptr;
	};

	std::unique_ptr<IRNode> Parser::parseTree(const std::string& str) {
		_error = {};
		Lexer lexer(str);

		TokenList list;
		while (lexer.hasNext()) {
			auto&& token = lexer.proceedNext();
			if (!token.value.empty()) {
				list.emplace_back(std::move(token));
			}
		}

		static ProcessStack processStack;
		BnfIterator maxIter(list.begin());
		processStack.reset(_root, maxIter);
		Parser::Node* errorProcessNode = nullptr;

		auto success = false;

		while (!processStack.empty()) {
			auto* current = processStack.top();

			auto iterator = current->iterator;
			if (!current->node->iterate(iterator, current->state)) {
				if (maxIter == current->iterator) {
					errorProcessNode = current->node;

				} else if (maxIter < current->iterator) {
					maxIter = current->iterator;
					errorProcessNode = current->node;
				}
				while (current) {
					processStack.pop();
					auto parent = current->parent;
					if (parent && parent->node->childRejected(parent->state)) {
						iterator = current->iterator;
						current = parent;
						break;
					}
					current = processStack.top();
				}
				if (current == nullptr) {
					break;
				}
			}

			Parser::Node* next;
			while (current) {
				next = current->node->nextChild(current->state);

				if (next != nullptr) {
					processStack.push(next, iterator, current);
					break;
				}

				current = current->parent;
			}
			if (current == nullptr) {
				success = !iterator;
				break;
			}
		}

		if (!success) {
			auto& iterator = maxIter;

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
				if (errorProcessNode == nullptr) {
					_error = std::format("({},{}): parsed all, no expected elements, somehow still an error...",
						line, row);
				}
				else {
					_error = std::format("({},{}): unexpected EOF, expected {}",
						line, row, errorProcessNode->toString());
				}
			}
			else {
				auto itStr = iterator.type() == Lexer::Token::Type::Other ?
					iterator.value().substr(0, 1):
					iterator.value();
				if (errorProcessNode == nullptr) {
					_error = std::format("({},{}): unexpected end of form, parsing {}",
						line, row, itStr);
				}
				else {
					_error = std::format("({},{}): expected {}, got {}",
						line, row, errorProcessNode->toString(), itStr);
				}
			}

			return {};
		}

		{
			auto* current = processStack.top();
			while (current->parent != nullptr) {
				auto& parent = current->parent;
				auto nextSibling = parent->firstChild;
				if (!current->node->excludeFromTree(current)) {
					parent->firstChild = current;
					if (nextSibling != nullptr) {
						nextSibling->prevSibling = current;
						current->nextSibling = nextSibling;
					}
				}
				else {
					if (current->firstChild) {
						parent->firstChild = current->firstChild;
						auto lastChild = current->firstChild;
						while (lastChild->nextSibling) {
							lastChild = lastChild->nextSibling;
						}
						if (nextSibling != nullptr) {
							nextSibling->prevSibling = lastChild;
							lastChild->nextSibling = nextSibling;
						}
					}
				}

				processStack.pop();
				current = processStack.top();
			}

			auto irTree = current->node->createIRTree(current);
			return irTree;
		}
	}

}