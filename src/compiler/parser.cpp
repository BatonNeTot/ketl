/*🐟Ketl🐟*/
#include "parser.h"

#include <functional>
#include <unordered_set>
#include <unordered_map>
#include <stack>
#include <sstream>

namespace Ketl {

	namespace {
		enum class OperatorDirection : uint8_t {
			FromRightToLeft,
			FromLeftToRight
		};

		struct OperatorData {
			int level;
			int argc;
			OperatorDirection direction;

			OperatorData(int level, int argc, OperatorDirection direction) :
				level(level), argc(argc), direction(direction) {}

			friend bool operator<(const OperatorData& lhs, const OperatorData& rhs);
			friend bool operator>(const OperatorData& lhs, const OperatorData& rhs);
		};

		bool operator<(const OperatorData& lhs, const OperatorData& rhs) {
			return lhs.level != rhs.level ? lhs.level < rhs.level : lhs.direction == OperatorDirection::FromLeftToRight;
		}
		bool operator>(const OperatorData& lhs, const OperatorData& rhs) {
			return lhs.level != rhs.level ? lhs.level > rhs.level : lhs.direction == OperatorDirection::FromRightToLeft;
		}

		struct Operator {
			std::string value;
			bool isToRight;

			Operator(const std::string& value, bool isToRight = false) :
				value(value), isToRight(isToRight) {};

			friend bool operator==(const Operator& lhs, const Operator& rhs);
		};

		bool operator==(const Operator& lhs, const Operator& rhs) {
			return lhs.value == rhs.value && lhs.isToRight == rhs.isToRight;
		}

	}
}

template <>
struct ::std::hash<Ketl::Operator> {
	std::size_t operator()(const Ketl::Operator& k) const {
		return std::hash<std::string>{}(k.value) * 2 + k.isToRight;
	}
};

namespace Ketl {
	namespace {
		static const std::unordered_map<Operator, OperatorData> operators = {
			{{"::"}, {1, 2, OperatorDirection::FromLeftToRight}},
			//{{"++"}, {2, 1, OperatorDirection::FromLeftToRight}},
			//{{"--"}, {2, 1, OperatorDirection::FromLeftToRight}},
			{{"()"}, {2, 1, OperatorDirection::FromLeftToRight}},
			//{{"["}, {2, OperatorDirection::FromLeftToRight}},
			//{{"]"}, {2, OperatorDirection::FromLeftToRight}},
			{{"."}, {2, 2, OperatorDirection::FromLeftToRight}},
			{{"->"}, {2, 2, OperatorDirection::FromLeftToRight}},

			{{"++", true}, {3, 1, OperatorDirection::FromRightToLeft}},
			{{"--", true}, {3, 1, OperatorDirection::FromRightToLeft}},
			{{"+", true}, {3, 1, OperatorDirection::FromRightToLeft}},
			{{"-", true}, {3, 1, OperatorDirection::FromRightToLeft}},
			{{"!", true}, {3, 1, OperatorDirection::FromRightToLeft}},
			{{"~", true}, {3, 1, OperatorDirection::FromRightToLeft}},
			//{{"*", true}, {3, 1, OperatorDirection::FromRightToLeft}},
			//{{"&", true}, {3, 1, OperatorDirection::FromRightToLeft}},

			{{"*"}, {5, 2, OperatorDirection::FromLeftToRight}},
			{{"/"}, {5, 2, OperatorDirection::FromLeftToRight}},
			{{"%"}, {5, 2, OperatorDirection::FromLeftToRight}},
			{{"+"}, {6, 2, OperatorDirection::FromLeftToRight}},
			{{"-"}, {6, 2, OperatorDirection::FromLeftToRight}},
			{{"<<"}, {7, 2, OperatorDirection::FromLeftToRight}},
			{{">>"}, {7, 2, OperatorDirection::FromLeftToRight}},
			{{"<"}, {8, 2, OperatorDirection::FromLeftToRight}},
			{{"<="}, {8, 2, OperatorDirection::FromLeftToRight}},
			{{">"}, {8, 2, OperatorDirection::FromLeftToRight}},
			{{">="}, {8, 2, OperatorDirection::FromLeftToRight}},
			{{"=="}, {9, 2, OperatorDirection::FromLeftToRight}},
			{{"!="}, {9, 2, OperatorDirection::FromLeftToRight}},
			{{"&"}, {10, 2, OperatorDirection::FromLeftToRight}},
			{{"^"}, {11, 2, OperatorDirection::FromLeftToRight}},
			{{"|"}, {12, 2, OperatorDirection::FromLeftToRight}},
			{{"&&"}, {13, 2, OperatorDirection::FromLeftToRight}},
			{{"||"}, {14, 2, OperatorDirection::FromLeftToRight}},

			//{{"?"}, {15, OperatorDirection::FromRightToLeft}},
			//{{":"}, {15, OperatorDirection::FromRightToLeft}},
			{{"="}, {15, 2, OperatorDirection::FromRightToLeft}},
			{{"+="}, {15, 2, OperatorDirection::FromRightToLeft}},
			{{"-="}, {15, 2, OperatorDirection::FromRightToLeft}},
			{{"*="}, {15, 2, OperatorDirection::FromRightToLeft}},
			{{"/="}, {15, 2, OperatorDirection::FromRightToLeft}},
			{{"%="}, {15, 2, OperatorDirection::FromRightToLeft}},
			{{"<<="}, {15, 2, OperatorDirection::FromRightToLeft}},
			{{">>="}, {15, 2, OperatorDirection::FromRightToLeft}},
			{{"&="}, {15, 2, OperatorDirection::FromRightToLeft}},
			{{"^="}, {15, 2, OperatorDirection::FromRightToLeft}},
			{{"|="}, {15, 2, OperatorDirection::FromRightToLeft}},

			{{","}, {15, 2, OperatorDirection::FromLeftToRight}},
		};

		enum class State : unsigned char {
			Default,
			DeclarationVar,
		};
	}

	void clarifyPrev(Parser::Node*& prev, const std::string& token) {
		if (prev == nullptr) { throw std::exception(""); }
		while (prev->parent && prev->parent->isOperator()) {
			auto rightOperator = operators.find({ token });
			auto leftOperator = operators.find({ prev->parent->value.str });
			if (!(rightOperator != operators.end() &&
				leftOperator != operators.end())) {
				throw std::exception("Can't found operator priority");
			}
			auto isLessPrority = leftOperator->second > rightOperator->second;
			if (!isLessPrority) {
				prev = prev->parent;
			}
			else {
				break;
			}
		}
	}

	const Parser::Node& Parser::getNext() {
		Parser::Node* root = nullptr;
		Parser::Node* prev = nullptr;
		Lexer::Token last;
		State state = State::Default;

		std::list<std::string> typeTokens;

		while (true) {
			bool newIsRoot = false;

			if (!_lexer.hasNext()) {
				throw std::exception("Lexer is out of tokens for Parser");
			}

			auto& token = _lexer.getNext();
			if (token.value.empty() && !token.isString()) {
				throw std::exception("Empty non-String token");
			}

			if (token.value == ";") {
				break;
			}
			else {
				Node* node = nullptr;
				switch (state) {
				case State::Default:
				{
					switch (token.type) {
					case Lexer::Token::Type::Operator:
						if (token.value == "(") {
							std::string brackets = "()";

							node = new Node(Node::Type::Operator, 1);
							node->value = brackets;
						}
						else if (token.value == ")") {
							std::string brackets = "()";

							prev = prev->findToken(brackets);
						}
						else {
							if (last.isOperator()) {
								throw std::exception("TODO");
							}
							else {
								clarifyPrev(prev, token.value);

								node = new Node(Node::Type::Operator, 2);
								node->value = token.value;
								newIsRoot = true;
							}
						}
						break;
					case Lexer::Token::Type::Number:
						if (!last.isOperator()) { throw std::exception("Operator was expected"); }

						node = constructNumber(token);
						break;
					case Lexer::Token::Type::String:

						break;
					case Lexer::Token::Type::Id:
						node = checkId(token, typeTokens);

						if (node == nullptr) {
							state = State::DeclarationVar;
						}

						break;
					}
					break;
				}
				case State::DeclarationVar:
				{
					node = checkId(token, typeTokens);

					if (node != nullptr) {
						state = State::Default;
					}

					break;
				}
				}

				if (node != nullptr) {
					if (root == nullptr) {
						root = node;
					}
					else {
						if (!newIsRoot) {
							prev->insert(node);
						}
						else {
							prev->replace(node);

							if (prev == root) {
								root = node;
							}
						}
					}

					prev = node;
				}
			}

			last = token;
		}

		_comands.emplace_back(root);
		return *_comands.back();
	}

	Parser::Node* Parser::checkId(const Lexer::Token& token, std::list<std::string>& typeTokens) {
		{
			static const std::unordered_set<std::string> keywords = {
			   "const",	"*", "&", "&&",
			   "(", ")", ",",
			};

			auto it = keywords.find(token.value);
			if (it != keywords.end()) {
				typeTokens.emplace_back(token.value);
				return nullptr;
			}
		}

		auto node = new Node(Node::Type::Id, 0);
		node->value = token.value;

		typeTokens.clear();

		return node;
	}

	Parser::Node* Parser::constructNumber(const Lexer::Token& token) {
		std::ostringstream builder;
		Node* node = new Node(Node::Type::Number, 0);

		if (token.value[0] == '.') {
			builder << '0';
		}

		bool isFloat = false;
		unsigned char longCount = 0;

		for (auto symbol : token.value) {
			switch (symbol) {
			case '.':
				isFloat = true;
				++longCount;
				builder << symbol;
				break;
			case 'f': case 'F':
				isFloat = true;
				break;
			case 'l': case 'L':
				++longCount;
			case '_':
				break;
			default:
				builder << symbol;
			}
		}

		if (token.value.back() == '.') {
			builder << '0';
		}

		if (isFloat) {
			switch (longCount) {
			case 0:
				node->value = std::stof(builder.str(), nullptr);
				break;
			case 1:
				node->value = std::stod(builder.str(), nullptr);
				break;
			default:
				throw std::exception("It's too much, man");
			}
		}
		else {
			switch (longCount) {
			case 0:
				node->value = std::stoi(builder.str(), nullptr);
				break;
			case 1:
				node->value = std::stol(builder.str(), nullptr);
				break;
			default:
				throw std::exception("It's too much, man");
			}
		}

		return node;
	}

}