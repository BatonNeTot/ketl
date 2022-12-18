/*🍲Ketl🍲*/
#ifndef ebnf_exp_lexer_h
#define ebnf_exp_lexer_h

#include <string>

namespace Ketl {

	class Lexer {
	public:

		Lexer(const std::string_view& source)
			: _source(source) {}

		struct Token {

			enum class Type : uint8_t {
				Id,
				Literal,
				Number,
				Other
			};

			Token() = default;
			Token(const char& symbol)
				: value(&symbol, 1), type(Type::Other) {};
			Token(const std::string_view& value_)
				: value(value_), type(Type::Other) {};
			Token(const std::string_view& value_, nullptr_t)
				: value(value_), type(Type::Id) {};
			Token(const std::string_view& value_, unsigned int)
				: value(value_), type(Type::Number) {};
			Token(const std::string_view& value_, char)
				: value(value_), type(Type::Literal) {};

			std::string_view value;
			Type type = Type::Id;
		};

		Token proceedNext();

		bool hasNext() const;

		uint64_t carret() const {
			return _carret;
		}

	private:

		const char& nextSymbol();
		const char& peekSymbol();

		static bool isNumberDot(char symbol);
		static bool isNumber(char symbol);

		static bool isProperIdSymbol(char symbol);
		static bool isProperStartingIdSymbol(char symbol);

		static bool isSpace(char symbol);

		static bool isQuote(char symbol);

		bool throwError(const std::string& message = "");

		std::string_view _source;

		uint64_t _carret = 0;

		bool _errorFlag = false;
		std::string _errorMsg;
	};

}

#endif // !ebnf_exp_lexer_h
