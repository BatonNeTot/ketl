/*🍲Ketl🍲*/
#include "parser.h"

#include "lexer.h"
#include "ir_nodes.h"
#include "bnf_nodes.h"

#include "semantic_analyzer.h"

#include <format>

namespace Ketl {

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

		// define variable by assignment
		_nodes.try_emplace("defineVariableAssignment", std::make_unique<NodeConcat>(false,
			std::make_unique<NodeOr>(
				std::make_unique<NodeLiteral>(&emptyTree, false, "var"),
				std::make_unique<NodeId>(false, "type")
				),
			std::make_unique<NodeLeaf>(NodeLeaf::Type::Id),
			std::make_unique<NodeLiteral>(true, "="),
			std::make_unique<NodeId>(true, "expression"),
			std::make_unique<NodeLiteral>(true, ";")
			));

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
			// define variable by =
			std::make_unique<NodeId>(&createDefineVariableByAssignmentTree, false, "defineVariableAssignment"),
			// define variable by {}
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
			_top->value.firstChild = nullptr;
			_top->value.prevSibling = nullptr;
			_top->value.nextSibling = nullptr;
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
			_top->value.firstChild = nullptr;
			_top->value.prevSibling = nullptr;
			_top->value.nextSibling = nullptr;
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

	std::variant<std::unique_ptr<IRNode>, std::string> Parser::parseTree(const std::string& str) {
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
					return std::format("({},{}): parsed all, no expected elements, somehow still an error...",
						line, row);
				}
				else {
					return std::format("({},{}): unexpected EOF, expected {}",
						line, row, errorProcessNode->toString());
				}
			}
			else {
				auto itStr = iterator.type() == Lexer::Token::Type::Other ?
					iterator.value().substr(0, 1):
					iterator.value();
				if (errorProcessNode == nullptr) {
					return std::format("({},{}): unexpected end of form, parsing {}",
						line, row, itStr);
				}
				else {
					return std::format("({},{}): expected {}, got {}",
						line, row, errorProcessNode->toString(), itStr);
				}
			}
		}

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