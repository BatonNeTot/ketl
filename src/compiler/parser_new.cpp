/*🍲Ketl🍲*/
#include "parser_new.h"

#include "ebnf.h"

#include <unordered_set>
#include <unordered_map>
#include <stack>
#include <algorithm>
#include <sstream>

namespace Ketl {

	std::unique_ptr<Parser::Node> Parser::proceed(const std::string& str) {
		ebnf::Ebnf form(
			"non-zero-digit='1'|'2'|'3'|'4'|'5'|'6'|'7'|'8'|'9'."
			"zero='0'."
			"digit=zero|non-zero-digit."
			"integer=non-zero-digit{digit}."
			"float-number=[non-zero-digit{digit}|zero]'.'digit{digit}."
			"number$=integer|float-number."

			"lower-alphabet='a'|'b'|'c'|'d'|'e'|'f'|'g'|'h'|'i'|'j'|'k'|'l'|'m'|'n'|'o'|'p'|'q'|'r'|'s'|'t'|'u'|'v'|'w'|'x'|'y'|'z'."
			"upper-alphabet='A'|'B'|'C'|'D'|'E'|'F'|'G'|'H'|'I'|'J'|'K'|'L'|'M'|'N'|'O'|'P'|'Q'|'R'|'S'|'T'|'U'|'V'|'W'|'X'|'Y'|'Z'."
			"id-first-symbol='_'|lower-alphabet|upper-alphabet."
			"id-symbol=digit|id-first-symbol."
			"id$=id-first-symbol{id-symbol}."

			"space~={' '|'\f'|'\n'|'\r'|'\t'|'\v'}."

			"type$=id|id space'('space {type space}')'."
			"define-id=type space id|id."

			"function-declaration=type space id space'('space [(type|type space id) {space ',' space (type|type space id)} space]')'."
			"function-definition=function-declaration space '{'space {command space} '}'."

			"brackets='('space expression space')'."
			"primary=brackets|number|define-id."

			"precedence-1-operator='*'|'/'."
			"precedence-1-expression=primary{space precedence-1-operator space primary}."

			"precedence-2-operator='+'|'-'."
			"precedence-2-expression=precedence-1-expression{space precedence-2-operator space precedence-1-expression}."

			"precedence-3-operator='=='|'!='."
			"precedence-3-expression={precedence-2-expression space precedence-3-operator space} precedence-2-expression."

			"precedence-4-operator='='."
			"precedence-4-expression={precedence-3-expression space precedence-4-operator space} precedence-3-expression."

			"expression=precedence-4-expression."
			"command=[expression]space';' | function-definition."
			"block='{'space {command space} '}' | command."
		);

		auto [success, tokenTree] = form.parseAs(str, "block");
		if (!success) {
			auto test = 0;
		}
		return proceed(&tokenTree);
	}

	std::unique_ptr<Parser::Node> Parser::proceed(const ebnf::Ebnf::Token* token) {
		auto expPos = token->id.find("expression");
		if (std::string::npos != expPos && 0 != expPos) {
			if (!token->parts.front()->id.empty()) {
				auto lhs = proceed(token->parts.front().get());

				if (token->parts.size() > 1) {
					auto& parts = token->parts.back()->parts;
					for (auto it = parts.cbegin(), end = parts.cend(); it != end; ++it) {
						const auto& part = *it;

						auto rhs = proceed(part->parts[1].get());
						const auto& opStr = part->parts[0]->value;

						auto op = std::make_unique<Node>(Node::Type::Operator);
						op->value = opStr;

						op->insert(std::move(lhs));
						op->insert(std::move(rhs));
						lhs = std::move(op);
					}
				}
				return lhs;
			}
			else {
				auto rhs = proceed(token->parts.back().get());

				if (token->parts.size() > 1) {
					auto& parts = token->parts.front()->parts;
					for (auto it = parts.crbegin(), end = parts.crend(); it != end; ++it) {
						const auto& part = *it;

						auto lhs = proceed(part->parts[0].get());
						const auto& opStr = part->parts[1]->value;

						auto op = std::make_unique<Node>(Node::Type::Operator);
						op->value = opStr;

						op->insert(std::move(lhs));
						op->insert(std::move(rhs));
						rhs = std::move(op);
					}
				}
				return rhs;
			}
		}

		if (token->id == "primary") {
			return proceed(token->parts.front().get());
		}

		if (token->id == "brackets") {
			return proceed(token->parts[1]->parts[0].get());
		}

		if (token->id == "define-id") {
			auto id = std::make_unique<Node>(Node::Type::Id);
			id->value = token->parts.back()->value;

			return id;
		}

		if (token->id == "number") {
			auto id = std::make_unique<Node>(Node::Type::Number);
			id->value = std::stol(token->value);

			return id;
		}

		if (token->id == "command") {
			if (token->parts.size() == 1 && token->parts[0]->value == ";") {
				return {};
			}
			return proceed(token->parts[0]->parts[0].get());
		}

		if (token->id == "block") {
			auto block = std::make_unique<Node>(Node::Type::Block);

			if (token->parts.size() == 3) {
				for (const auto& part : token->parts[1]->parts) {
					block->insert(proceed(part->parts[0].get()));
				}
			}
			else {
				block->insert(proceed(nullptr));
			}
			return block;
		}

		if (token->parts.size() == 1) {
			return proceed(token->parts.front().get());
		}

		for (auto& part : token->parts) {
			//proceed(part.get());
		}

		return {};
	}

}