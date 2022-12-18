/*🍲Ketl🍲*/
#ifndef lexer_h
#define lexer_h

#include <string>
#include <list>

namespace Ketl {

	class Lexer {
	public:

		struct Token {
			enum class Type : uint8_t {
				Operator,
				Id,
				Number,
				String,
			};

			std::string value = "";
			Type type = Type::Operator;

			bool isOperator() const { return type == Token::Type::Operator && value != ")"; }
			bool isValue() const { return type != Token::Type::Operator; }
			bool isId() const { return type == Token::Type::Id; }
			bool mayId() const { return isId() || (!isValue() && value.empty()); }
			bool isNumber() const { return type == Token::Type::Number; }
			bool mayNumber() const { return isNumber() || (!isValue() && (value.empty() || value == ".")); }
			bool isString() const { return type == Token::Type::String; }
		};

		Lexer(const std::string& source) : _source(source) {}

		const Token& getNext() {
			if (!proceedToken()) { throw std::exception(""); }
			return _tokens.back();
		}

		bool hasNext() {
			return _iter < _source.length();
		}

	private:

		bool proceedToken();
		bool checkValue(Token& token);
		bool checkOperator(Token& token, char nextSymbol = '\0');

		std::string _source;
		std::list<Token> _tokens;
		size_t _iter = 0;

	};

}

#endif /*lexer_h*/