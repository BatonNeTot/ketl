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

			Token(uint64_t offset_, const char& symbol)
				: offset(offset_), value(&symbol, 1), type(Type::Other) {};
			Token(uint64_t offset_, const std::string_view& value_)
				: offset(offset_), value(value_), type(Type::Other) {};
			Token(uint64_t offset_, const std::string_view& value_, nullptr_t)
				: offset(offset_), value(value_), type(Type::Id) {};
			Token(uint64_t offset_, const std::string_view& value_, unsigned int)
				: offset(offset_), value(value_), type(Type::Number) {};
			Token(uint64_t offset_, const std::string_view& value_, char)
				: offset(offset_), value(value_), type(Type::Literal) {};

			std::string_view value;
			Type type = Type::Id;
			uint64_t offset = 0;
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
