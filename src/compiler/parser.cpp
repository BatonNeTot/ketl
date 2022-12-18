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
				std::make_unique<BnfNodeId>("type-arguments", false, false)
				),
			std::make_unique<BnfNodeLiteral>("", true)
			));
		_manager.insert("type-arguments", std::make_unique<BnfNodeConcat>(
			std::make_unique<BnfNodeId>("type", false, true),
			std::make_unique<BnfNodeId>("type-extra-arguments", true, false)
			));

		_manager.insert("type", std::make_unique<BnfNodeOr>(
			std::make_unique<BnfNodeConcat>(
				std::make_unique<BnfNodeLeaf>(BnfNodeLeaf::Type::Id),
				std::make_unique<BnfNodeLiteral>("(", true),
				std::make_unique<BnfNodeOr>(
					std::make_unique<BnfNodeId>("type-arguments", false, false),
					std::make_unique<BnfNodeLiteral>("", true)
					),
				std::make_unique<BnfNodeLiteral>(")", true)
				),
			std::make_unique<BnfNodeLeaf>(BnfNodeLeaf::Type::Id)
			));

		_manager.insert("function-declaration-argument", std::make_unique<BnfNodeOr>(
			std::make_unique<BnfNodeConcat>(
				std::make_unique<BnfNodeId>("type", false, true),
				std::make_unique<BnfNodeLeaf>(BnfNodeLeaf::Type::Id)
				),
			std::make_unique<BnfNodeId>("type", false, true)
			));

		_manager.insert("function-declaration-extra-arguments", std::make_unique<BnfNodeOr>(
			std::make_unique<BnfNodeConcat>(
				std::make_unique<BnfNodeLiteral>(",", true),
				std::make_unique<BnfNodeId>("function-declaration-argument", false, false),
				std::make_unique<BnfNodeId>("function-declaration-extra-arguments", true, false)
				),
			std::make_unique<BnfNodeLiteral>("", true)
			));

		_manager.insert("function-declaration-arguments", std::make_unique<BnfNodeConcat>(
			std::make_unique<BnfNodeId>("function-declaration-argument", false, false),
			std::make_unique<BnfNodeId>("function-declaration-extra-arguments", true, false)
			));

		_manager.insert("function-declaration", std::make_unique<BnfNodeConcat>(
			std::make_unique<BnfNodeId>("type", false, true),
			std::make_unique<BnfNodeLeaf>(BnfNodeLeaf::Type::Id),
			std::make_unique<BnfNodeLiteral>("(", true),
			std::make_unique<BnfNodeOr>(
				std::make_unique<BnfNodeId>("function-declaration-arguments", false, false),
				std::make_unique<BnfNodeLiteral>("", true)
				),
			std::make_unique<BnfNodeLiteral>(")", true)
			));

		_manager.insert("function-definition", std::make_unique<BnfNodeConcat>(
			std::make_unique<BnfNodeId>("function-declaration", false, false),
			std::make_unique<BnfNodeId>("brackets-commands", false, false)
			));

		_manager.insert("primary", std::make_unique<BnfNodeOr>(
			std::make_unique<BnfNodeLeaf>(BnfNodeLeaf::Type::Id),
			std::make_unique<BnfNodeLeaf>(BnfNodeLeaf::Type::Number),
			std::make_unique<BnfNodeLeaf>(BnfNodeLeaf::Type::String),
			std::make_unique<BnfNodeConcat>(
				true,
				std::make_unique<BnfNodeLiteral>("(", true),
				std::make_unique<BnfNodeId>("expression", false, true),
				std::make_unique<BnfNodeLiteral>(")", true)
				),
			std::make_unique<BnfNodeId>("function-definition", false, false)
			));

		_manager.insert("function-extra-arguments", std::make_unique<BnfNodeOr>(
			std::make_unique<BnfNodeConcat>(
				std::make_unique<BnfNodeLiteral>(",", true),
				std::make_unique<BnfNodeId>("primary", false, false),
				std::make_unique<BnfNodeId>("function-extra-arguments", true, false)
				),
			std::make_unique<BnfNodeLiteral>("", true)
			));
		_manager.insert("function-arguments", std::make_unique<BnfNodeConcat>(
			std::make_unique<BnfNodeId>("expression", false, true),
			std::make_unique<BnfNodeId>("function-extra-arguments", true, false)
			));
		_manager.insert("precedence-1-operator", std::make_unique<BnfNodeOr>(
			std::make_unique<BnfNodeConcat>(
				std::make_unique<BnfNodeLiteral>("("),
				std::make_unique<BnfNodeOr>(
					std::make_unique<BnfNodeId>("function-arguments", false, false),
					std::make_unique<BnfNodeLiteral>("", true)
					),
				std::make_unique<BnfNodeLiteral>(")", true)
				)
			));
		_manager.insert("precedence-1-expression", std::make_unique<BnfNodeConcat>(
			std::make_unique<BnfNodeId>("primary", false, false),
			std::make_unique<BnfNodeOr>(
				std::make_unique<BnfNodeId>("precedence-1-operator", false, false),
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

		_manager.insert("expression", std::make_unique<BnfNodeId>("precedence-5-expression", false, false));

		_manager.insert("expression-with-end-symbol", std::make_unique<BnfNodeConcat>(
			std::make_unique<BnfNodeOr>(
				std::make_unique<BnfNodeId>("expression", false, true),
				std::make_unique<BnfNodeLiteral>("")
				),
			std::make_unique<BnfNodeLiteral>(";", true)
			));
		_manager.insert("return", std::make_unique<BnfNodeConcat>(
			std::make_unique<BnfNodeLiteral>("return", true),
			std::make_unique<BnfNodeOr>(
				std::make_unique<BnfNodeId>("expression", false, true),
				std::make_unique<BnfNodeLiteral>("")
				),
			std::make_unique<BnfNodeLiteral>(";", true)
			));

		_manager.insert("define-variable-extra-arguments", std::make_unique<BnfNodeOr>(
			std::make_unique<BnfNodeConcat>(
				std::make_unique<BnfNodeLiteral>(",", true),
				std::make_unique<BnfNodeId>("define-variable-arguments", false, false)
				),
			std::make_unique<BnfNodeLiteral>("", true)
			));
		_manager.insert("define-variable-arguments", std::make_unique<BnfNodeConcat>(
			std::make_unique<BnfNodeId>("expression", false, true),
			std::make_unique<BnfNodeId>("define-variable-extra-arguments", true, false)
			));

		_manager.insert("define-variable", std::make_unique<BnfNodeConcat>(
			std::make_unique<BnfNodeId>("type", false, true),
			std::make_unique<BnfNodeLeaf>(BnfNodeLeaf::Type::Id),
			std::make_unique<BnfNodeOr>(
				std::make_unique<BnfNodeConcat>(
					std::make_unique<BnfNodeLiteral>("{", true),
					std::make_unique<BnfNodeOr>(
						std::make_unique<BnfNodeId>("define-variable-arguments", false, false),
						std::make_unique<BnfNodeLiteral>("", true)
						),
					std::make_unique<BnfNodeLiteral>("}", true)
					),
				std::make_unique<BnfNodeLiteral>("")
				),
			std::make_unique<BnfNodeLiteral>(";", true)
			));

		_manager.insert("command", std::make_unique<BnfNodeOr>(
			std::make_unique<BnfNodeId>("expression-with-end-symbol", true, false),
			std::make_unique<BnfNodeId>("return", false, false),
			std::make_unique<BnfNodeId>("define-variable", false, false),
			std::make_unique<BnfNodeId>("function-definition", false, false),
			std::make_unique<BnfNodeId>("brackets-commands", false, false)
			));

		_manager.insert("brackets-commands", std::make_unique<BnfNodeConcat>(
			std::make_unique<BnfNodeLiteral>("{", true),
			std::make_unique<BnfNodeId>("several-commands", true, false),
			std::make_unique<BnfNodeLiteral>("}", true)
			));

		_manager.insert("several-commands", std::make_unique<BnfNodeOr>(
			std::make_unique<BnfNodeConcat>(
				std::make_unique<BnfNodeId>("command", false, false),
				std::make_unique<BnfNodeId>("several-commands", true, false)
				),
			std::make_unique<BnfNodeLiteral>("")
			));

		_manager.insert("block", std::make_unique<BnfNodeOr>(
			std::make_unique<BnfNodeId>("brackets-commands", false, false),
			std::make_unique<BnfNodeId>("command", true, false)
			));

		_manager.preprocessNodes();
	}

	void Parser::insertPredence(std::unique_ptr<BnfNode>&& operators, const std::string& expression,
		const std::string& extra, const std::string& lowExpression) {
		_manager.insert(extra, std::make_unique<BnfNodeOr>(
			std::make_unique<BnfNodeConcat>(
				operators,
				std::make_unique<BnfNodeId>(lowExpression, false, false),
				std::make_unique<BnfNodeId>(extra, true, false)
				),
			std::make_unique<BnfNodeLiteral>("")
			));
		_manager.insert(expression, std::make_unique<BnfNodeConcat>(
			false,
			std::make_unique<BnfNodeId>(lowExpression, false, false),
			std::make_unique<BnfNodeId>(extra, true, false)
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
					line, row,
					processNode.node->errorMsg());
			} else if (processNode.node == nullptr) {
				_error = std::format("({},{}): unexpected end of form, parsing {}",
					line, row,
					static_cast<char>(iterator));
			}
			else {
				_error = std::format("({},{}): expected {}, got {}",
					line, row,
					processNode.node->errorMsg(),
					static_cast<char>(iterator));
			}

			return {};
		}

		return std::move(processNode.outputChildrenNodes.back());
	}

}