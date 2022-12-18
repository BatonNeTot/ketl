/*🍲Ketl🍲*/
#include "lexer.h"

namespace Ketl {

	Lexer::Token Lexer::proceedNext() {
		auto* itSymbolPtr = &nextSymbol();

		while (isSpace(*itSymbolPtr)) {
			itSymbolPtr = &nextSymbol();
		}

		auto initialCarret = _carret - 1;
		
		while (*itSymbolPtr != '\0') {
			if (isNumberDot(*itSymbolPtr) && isNumber(peekSymbol())) {
				if (initialCarret < _carret - 1) { _carret--; return _source.substr(initialCarret, _carret - initialCarret); }
				auto start = _carret - 1;
				while (isNumber(nextSymbol())) {};
				--_carret;
				return { _source.substr(start, _carret - start), 0u };
			}

			if (isNumber(*itSymbolPtr)) {
				if (initialCarret < _carret - 1) { _carret--; return _source.substr(initialCarret, _carret - initialCarret); }
				auto start = _carret - 1;
				while (true) {
					auto next = nextSymbol();
					if (isNumber(next)) {
						continue;
					}
					if (isNumberDot(next)) {
						while (isNumber(nextSymbol())) {};
						--_carret;
						return { _source.substr(start, _carret - start), 0u };
					}
					break;
				};
				--_carret;
				return { _source.substr(start, _carret - start), 0u };
			}

			if (isQuote(*itSymbolPtr)) {
				if (initialCarret < _carret - 1) { _carret--; return _source.substr(initialCarret, _carret - initialCarret); }
				auto start = _carret;
				while (!isQuote(nextSymbol())) {};
				return { _source.substr(start, _carret - start - 1), '\0' };
			}

			if (isProperStartingIdSymbol(*itSymbolPtr)) {
				if (initialCarret < _carret - 1) { _carret--; return _source.substr(initialCarret, _carret - initialCarret); }
				auto start = _carret - 1;
				while (isProperIdSymbol(nextSymbol())) {};
				--_carret;
				return { _source.substr(start, _carret - start), nullptr };
			}

			if (isSpace(*itSymbolPtr)) {
				return _source.substr(initialCarret, _carret - initialCarret - 1);
			}

			if (*itSymbolPtr == '/' && peekSymbol() == '/') {
				if (initialCarret < _carret - 1) { _carret--; return _source.substr(initialCarret, _carret - initialCarret); }
				for (; *itSymbolPtr != '\0' && *itSymbolPtr != '\n'; itSymbolPtr = &nextSymbol()) {};
				while (isSpace(*itSymbolPtr)) { itSymbolPtr = &nextSymbol(); }
				initialCarret = _carret - 1;
				continue;
			}

			itSymbolPtr = &nextSymbol();
		}

		return _source.substr(initialCarret, _carret - initialCarret);
	}

	bool Lexer::hasNext() const {
		return _source.length() > _carret;
	}

	static char nonSymbol = '\0';

	const char& Lexer::nextSymbol() {
		if (!hasNext()) {
			_carret = _source.length() + 1;
			throwError("Unexpected end of a string");
			return nonSymbol;
		}
		return _source.at(_carret++);
	}

	const char& Lexer::peekSymbol() {
		if (!hasNext()) {
			throwError("Unexpected end of a string");
			return nonSymbol;
		}
		return _source.at(_carret);
	}

	bool Lexer::isSpace(char symbol) {
		return symbol == ' '
			|| (symbol >= '\t' && symbol <= '\r');
	}

	bool Lexer::isQuote(char symbol) {
		return symbol == '"';
	}

	bool Lexer::isNumberDot(char symbol) {
		return symbol == '.';
	}

	bool Lexer::isNumber(char symbol) {
		return symbol >= '0' && symbol <= '9';
	}

	bool Lexer::isProperIdSymbol(char symbol) {
		return isProperStartingIdSymbol(symbol)
			|| symbol == '-'
			|| isNumber(symbol);
	}
	bool Lexer::isProperStartingIdSymbol(char symbol) {
		return symbol == '_'
			|| (symbol >= 'a' && symbol <= 'z')
			|| (symbol >= 'A' && symbol <= 'Z');
	}

	bool Lexer::throwError(const std::string& message /*= ""*/) {
		_errorFlag = true;
		_errorMsg = message;
		return true;
	}

}