/*🐟Ketl🐟*/
#ifndef analyzer_new_h
#define analyzer_new_h

#include "parser.h"

#include "eel.h"
#include "eel_new.h"

#include <string>
#include <list>
#include <vector>
#include <unordered_map>

class Analyzer {
public:

	struct Variable {
		enum class Type : uint8_t {
			Literal,
			Stack,
			Global
		};

		Type type;
		Ketl::UniValue literal;
		uint64_t stack;
		std::string id;
		std::string valueType;
	};

	struct RawInstruction {
		Variable result;
		std::string name;
		Variable args[2];
	};

	struct Result {
		uint64_t stackSize; // TODO
		std::list<RawInstruction> instructions;
	};

	Analyzer(const std::string& source) : _parser(source) {}

	const Result& proceed(Environment& env);

private:

	Ketl::Parser _parser;

	Result* _result = nullptr;

	enum class State : unsigned char {
		Default,
		DeclarationVar,
	};

	struct AnalyzerInfo {
		uint32_t nextFreeStack = 0;

		uint32_t getFreeStack() {
			return nextFreeStack++;
		}
	};

	Variable proceedCommands(Environment& env, std::list<RawInstruction>& list, AnalyzerInfo& info, const Ketl::Parser::Node& node);

};

#endif /*analyzer_new_h*/