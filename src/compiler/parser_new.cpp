/*🍲Ketl🍲*/
#include "parser_new.h"

#include "lexer.h"
#include "bnf_nodes_new.h"

#include <format>

namespace Ketl {

	class IRBlockNew : public IRNode {
	public:

		IRBlockNew(std::vector<std::unique_ptr<IRNode>>&& commands)
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

	std::unique_ptr<IRNode> createBlockTree(const ProcessNodeNew* info) {
		std::vector<std::unique_ptr<IRNode>> commands;

		auto it = info->firstChild;
		while (it) {
			commands.emplace_back(it->node->createIRTree(it));
			it = it->nextSibling;
		}

		return std::make_unique<IRBlockNew>(std::move(commands));
	}

	std::unique_ptr<IRNode> proxyTree(const ProcessNodeNew* info) {
		auto child = info->firstChild;
		return child->node->createIRTree(child);
	}

	class IRBinaryOperatorNew : public IRNode {
	public:

		IRBinaryOperatorNew(OperatorCode op, bool ltr, std::unique_ptr<IRNode>&& lhs, std::unique_ptr<IRNode>&& rhs)
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

	private:

		OperatorCode _op;
		bool _ltr;
		std::shared_ptr<TypeTemplate> _type;
		std::unique_ptr<IRNode> _lhs;
		std::unique_ptr<IRNode> _rhs;
	};

	std::unique_ptr<IRNode> createRtlTree(const ProcessNodeNew* info) {
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

			rightArg = std::make_unique<IRBinaryOperatorNew>(code, false, std::move(leftArg), std::move(rightArg));
			
			rightArgNode = leftArgNode;
		} while (rightArgNode->prevSibling);

		return rightArg;
	}

	std::unique_ptr<IRNode> createLtrTree(const ProcessNodeNew* info) {
		auto leftArgNode = info->firstChild;

		auto leftArg = leftArgNode->node->createIRTree(leftArgNode);

		do {
			auto opNode = leftArgNode->nextSibling;
			auto op = opNode->node->value(opNode->iterator);
			(void)op; // TODO deduce operator code

			auto code = OperatorCode::Plus;

			auto rightArgNode = opNode->nextSibling;
			auto rightArg = rightArgNode->node->createIRTree(rightArgNode);

			leftArg = std::make_unique<IRBinaryOperatorNew>(code, true, std::move(leftArg), std::move(rightArg));

			leftArgNode = rightArgNode;
		} while (leftArgNode->nextSibling);

		return leftArg;
	}

	class IRVariableNew : public IRNode {
	public:

		IRVariableNew(const std::string_view& id)
			: _id(id) {}
		IRVariableNew(const std::string_view& id, std::shared_ptr<TypeTemplate> type)
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

	private:
		std::string _id;
		std::shared_ptr<TypeTemplate> _type;
	};

	std::unique_ptr<IRNode> createVariable(const ProcessNodeNew* info) {
		return std::make_unique<IRVariableNew>(info->node->value(info->iterator));
	}

	class IRLiteralNew : public IRNode {
	public:

		IRLiteralNew(const std::string_view& id)
			: _id(id) {}

		uint64_t childCount() const override {
			return 0;
		}

	private:
		std::string_view _id;
	};

	std::unique_ptr<IRNode> createLiteral(const ProcessNodeNew* info) {
		return std::make_unique<IRLiteralNew>(info->node->value(info->iterator));
	}

	ParserNew::ParserNew() {
		// expression
		_nodes.try_emplace("primary", std::make_unique<NodeOrNew>(
			std::make_unique<NodeLeafNew>(&createVariable, NodeLeafNew::Type::Id),
			std::make_unique<NodeLeafNew>(&createLiteral, NodeLeafNew::Type::Number),
			std::make_unique<NodeLeafNew>(NodeLeafNew::Type::String),
			std::make_unique<NodeConcatNew>(true,
				std::make_unique<NodeLiteralNew>(true, "("),
				std::make_unique<NodeIdNew>(true, "expression"),
				std::make_unique<NodeLiteralNew>(true, ")")
				)
			));

		_nodes.try_emplace("precedence-1-expression", std::make_unique<NodeConcatNew>(true,
			std::make_unique<NodeIdNew>(true, "primary"),
			std::make_unique<NodeConditionalNew>(
				std::make_unique<NodeConcatNew>(true,
					std::make_unique<NodeLiteralNew>(true, "("),
					std::make_unique<NodeConditionalNew>(
						std::make_unique<NodeConcatNew>(true,
							std::make_unique<NodeIdNew>(true, "expression"),
							std::make_unique<NodeRepeatNew>(
								std::make_unique<NodeConcatNew>(true,
									std::make_unique<NodeLiteralNew>(true, ","),
									std::make_unique<NodeIdNew>(true, "expression")
									)
								)
							)
						),
					std::make_unique<NodeLiteralNew>(true, ")")
					)
				)
			)); 

		_nodes.try_emplace("precedence-2-expression", std::make_unique<NodeConcatNew>(true,
			std::make_unique<NodeIdNew>(&proxyTree, true, "precedence-1-expression"),
			std::make_unique<NodeRepeatNew>(
				std::make_unique<NodeConcatNew>(true,
					std::make_unique<NodeOrNew>(
						std::make_unique<NodeLiteralNew>(false, "*"),
						std::make_unique<NodeLiteralNew>(false, "/")
						),
					std::make_unique<NodeIdNew>(&proxyTree, true, "precedence-1-expression")
					)
				)
			));

		_nodes.try_emplace("precedence-3-expression", std::make_unique<NodeConcatNew>(true,
			std::make_unique<NodeIdNew>(&createLtrTree, true, "precedence-2-expression"),
			std::make_unique<NodeRepeatNew>(
				std::make_unique<NodeConcatNew>(true,
					std::make_unique<NodeOrNew>(
						std::make_unique<NodeLiteralNew>(false, "+"),
						std::make_unique<NodeLiteralNew>(false, "-")
						),
					std::make_unique<NodeIdNew>(&createLtrTree, true, "precedence-2-expression")
					)
				)
			));

		_nodes.try_emplace("precedence-4-expression", std::make_unique<NodeConcatNew>(true,
			std::make_unique<NodeIdNew>(&createLtrTree, true, "precedence-3-expression"),
			std::make_unique<NodeRepeatNew>(
				std::make_unique<NodeConcatNew>(true,
					std::make_unique<NodeOrNew>(
						std::make_unique<NodeLiteralNew>(false, "=="),
						std::make_unique<NodeLiteralNew>(false, "!=")
						),
					std::make_unique<NodeIdNew>(&createLtrTree, true, "precedence-3-expression")
					)
				)
			));

		_nodes.try_emplace("precedence-5-expression", std::make_unique<NodeConcatNew>(true,
			std::make_unique<NodeIdNew>(&createLtrTree, true, "precedence-4-expression"),
			std::make_unique<NodeRepeatNew>(
				std::make_unique<NodeConcatNew>(true,
					std::make_unique<NodeOrNew>(
						std::make_unique<NodeLiteralNew>(false, "=")
						),
					std::make_unique<NodeIdNew>(&createLtrTree, true, "precedence-4-expression")
					)
				)
			));

		_nodes.try_emplace("expression", std::make_unique<NodeIdNew>(&createRtlTree, true, "precedence-5-expression"));

		// type
		_nodes.try_emplace("type", std::make_unique<NodeConcatNew>(false,
			std::make_unique<NodeOrNew>(
				std::make_unique<NodeLeafNew>(NodeLeafNew::Type::Id),
				std::make_unique<NodeConcatNew>(false,
					std::make_unique<NodeLiteralNew>(false, "const"),
					std::make_unique<NodeIdNew>(false, "type")
					),
				std::make_unique<NodeConcatNew>(true,
					std::make_unique<NodeLiteralNew>(true, "("),
					std::make_unique<NodeIdNew>(false, "type"),
					std::make_unique<NodeLiteralNew>(true, ")")
					)
				),
			std::make_unique<NodeConditionalNew>(
				std::make_unique<NodeLiteralNew>(false, "&&"),
				std::make_unique<NodeLiteralNew>(false, "&")
				)
			));

		_nodes.try_emplace("command", std::make_unique<NodeOrNew>(
			// expression
			std::make_unique<NodeConcatNew>(false,
				// return
				std::make_unique<NodeConditionalNew>(
					std::make_unique<NodeLiteralNew>(false, "return")
				),
				std::make_unique<NodeConditionalNew>(
					std::make_unique<NodeIdNew>(true, "expression")),
				std::make_unique<NodeLiteralNew>(true, ";")
				),
			// define variable
			std::make_unique<NodeConcatNew>(false,
				std::make_unique<NodeIdNew>(false, "type"),
				std::make_unique<NodeLeafNew>(NodeLeafNew::Type::Id),
				std::make_unique<NodeConditionalNew>(
					std::make_unique<NodeConcatNew>(false,
						std::make_unique<NodeLiteralNew>(true, "{"),
						std::make_unique<NodeConditionalNew>(
							std::make_unique<NodeConcatNew>(true,
								std::make_unique<NodeIdNew>(true, "expression"),
								std::make_unique<NodeRepeatNew>(
									std::make_unique<NodeConcatNew>(true,
										std::make_unique<NodeLiteralNew>(true, ","),
										std::make_unique<NodeIdNew>(true, "expression")
										)
									)
								)
							),
						std::make_unique<NodeLiteralNew>(true, "}")
						)
					),
				std::make_unique<NodeLiteralNew>(true, ";")
				),
			// define function
			std::make_unique<NodeConcatNew>(false,
				std::make_unique<NodeIdNew>(false, "type"),
				std::make_unique<NodeLeafNew>(NodeLeafNew::Type::Id),
				std::make_unique<NodeLiteralNew>(true, "("),
				std::make_unique<NodeConditionalNew>(
					std::make_unique<NodeConcatNew>(false,
						std::make_unique<NodeIdNew>(false, "type"),
						std::make_unique<NodeConditionalNew>(
							std::make_unique<NodeLeafNew>(NodeLeafNew::Type::Id)
							),
						std::make_unique<NodeRepeatNew>(
							std::make_unique<NodeConcatNew>(true,
								std::make_unique<NodeLiteralNew>(true, ","),
								std::make_unique<NodeConcatNew>(false,
									std::make_unique<NodeIdNew>(false, "type"),
									std::make_unique<NodeConditionalNew>(
										std::make_unique<NodeLeafNew>(NodeLeafNew::Type::Id)
										)
									)
								)
							)
						)
					),
				std::make_unique<NodeLiteralNew>(true, ")"),
				std::make_unique<NodeLiteralNew>(true, "{"),
				std::make_unique<NodeIdNew>(false, "several-commands"),
				std::make_unique<NodeLiteralNew>(true, "}")
				),
			// block
			std::make_unique<NodeConcatNew>(false,
				std::make_unique<NodeLiteralNew>(true, "{"),
				std::make_unique<NodeIdNew>(false, "several-commands"),
				std::make_unique<NodeLiteralNew>(true, "}")
				)
			));

		_root = _nodes.try_emplace("several-commands", std::make_unique<NodeRepeatNew>( &createBlockTree,
			std::make_unique<NodeIdNew>(true, "command")
			)).first->second.get();

		for (auto& nodePair : _nodes) {
			nodePair.second->postConstruct(_nodes);
		}
	}

	class ProcessStack {
	private:

		struct Node {
			ProcessNodeNew value;
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

		void reset(ParserNew::Node* node, const BnfIteratorNew& iterator) {
			_top = _first;
			_top->value.node = node;
			_top->value.iterator = iterator;
			_top->value.parent = nullptr;
		}

		void push(ParserNew::Node* node, const BnfIteratorNew& iterator, ProcessNodeNew* parent) {
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

		ProcessNodeNew* top() const {
			return _top ? &_top->value : nullptr;
		}

		bool empty() const {
			return _top == nullptr;
		}

	private:
		Node* _first = nullptr;
		Node* _top = nullptr;
	};

	std::unique_ptr<IRNode> ParserNew::parseTree(const std::string& str) {
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
		BnfIteratorNew maxIter(list.begin());
		processStack.reset(_root, maxIter);
		ParserNew::Node* errorProcessNode = nullptr;

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

			ParserNew::Node* next;
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