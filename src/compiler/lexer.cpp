/*🍲Ketl🍲*/
#include "lexer.h"

#include <unordered_set>
#include <sstream>

namespace Ketl {

	namespace {

		enum class SymbolType : unsigned char {
			Letter,
			Number,
			Space,
			Other
		};

		static SymbolType getSymbolType(char symbol) {
			if ('a' <= symbol && symbol <= 'z' ||
				'A' <= symbol && symbol <= 'Z') {
				return SymbolType::Letter;
			}
			else if ('0' <= symbol && symbol <= '9') {
				return SymbolType::Number;
			}
			else {
				switch (symbol) {
				case ' ':
				case '\n':
				case '\t':
				case '\r':
				case '\v':
					return SymbolType::Space;
				default:
					return SymbolType::Other;
				}
			}
		}

		static const std::unordered_set<std::string> availableTokens = {
			"::", "++", "--", "(", ")", "[", "]", ".", "->", "+", "-", "!", "~",
			"*", "/", "%", "<<", ">>", "<", "<=", ">", ">=", "==", "!=", "&", "^", "|", "&&", "||",
			"?", ":", "=", "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "^=", "|=", "&&=", "||=",
			",", ";", "{", "}",
		};

		static bool isAvailableToken(const std::string& token) {
			return availableTokens.find(token) == availableTokens.end();
		}

	}


	bool Lexer::proceedToken() {
		Token token;
		for (; _iter < _source.length(); ++_iter) {
			const auto symbol = _source[_iter];

			if (token.isString()) {
				switch (symbol) {
				case '"':
					++_iter;
					_tokens.emplace_back(token);
					return true;
				default:
					token.value += symbol;
				}
			}
			else {
				auto type = getSymbolType(symbol);
				switch (type) {
				case SymbolType::Letter:
					if (checkOperator(token)) {
						return true;
					}
					if (token.mayId()) {
						token.value += symbol;
						token.type = Token::Type::Id;
					}
					break;
				case SymbolType::Number:
					if (token.value != "." && checkOperator(token)) {
						return true;
					}
					if (token.mayNumber()) {
						token.value += symbol;
						token.type = Token::Type::Number;
					}
					else if (token.mayId()) {
						token.value += symbol;
						token.type = Token::Type::Id;
					}
					break;
				case SymbolType::Space:
					if (checkValue(token)) {
						++_iter;
						return true;
					}
					if (checkOperator(token)) {
						++_iter;
						return true;
					}
					break;
				case SymbolType::Other:
					switch (symbol) {
					case '_':
						if (checkOperator(token)) {
							return true;
						}
						if (token.mayId()) {
							token.value += symbol;
							token.type = Token::Type::Id;
						}
						else if (token.mayNumber()) {
							token.value += symbol;
							token.type = Token::Type::Number;
						}
						break;
					case '.':
						if (token.mayNumber()) {
							if (!token.value.empty()) {
								token.type = Token::Type::Number;
							}
							token.value += symbol;
						}
						else {
							if (checkValue(token)) {
								return true;
							}
							if (checkOperator(token, symbol)) {
								return true;
							}
						}
						break;
					case '"':
						if (checkOperator(token)) {
							return true;
						}
						token.type = Token::Type::String;
						break;
					default:
						if (checkValue(token)) {
							return true;
						}
						if (checkOperator(token, symbol)) {
							return true;
						}
						break;
					}
					break;
				}
			}
		}
		if (checkValue(token)) {
			return true;
		}
		if (checkOperator(token)) {
			return true;
		}
		return false;
	}

	bool Lexer::checkValue(Token& token) {
		if (!token.isValue() || token.value.empty()) {
			return false;
		}

		_tokens.emplace_back(token);
		return true;
	}

	bool Lexer::checkOperator(Token& token, char nextSymbol /* '\0' */) {
		if (token.isValue()) {
			return false;
		}
		if (token.value.empty()) {
			if (nextSymbol != '\0') {
				token.value += nextSymbol;
			}
			return false;
		}

		auto extendedToken = token.value + nextSymbol;
		if (nextSymbol == '\0' || isAvailableToken(extendedToken)) {
			if (isAvailableToken(token.value)) { throw std::exception(""); }

			_tokens.emplace_back(token);
			return true;
		}
		else {
			token.value = extendedToken;

			_tokens.emplace_back(token);
			return true;
		}
	}

}