/*🍲Ketl🍲*/
#ifndef analyzer_new_h
#define analyzer_new_h

#include "parser_new.h"

#include "ketl.h"
#include "ketl_new.h"

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
		const ::Type* valueType;
	};

	struct RawInstruction {
		const Environment::FunctionInfo* info;
		Variable args[2];
		Variable output;
	};

	struct Result {
		uint64_t stackSize; // TODO
		std::list<RawInstruction> instructions;
	};

	Analyzer() : _parser() {}

	const Result& proceed(Environment& env, const std::string& source);

private:

	Ketl::Parser _parser;

	std::unique_ptr<Result> _result;

	enum class State : unsigned char {
		Default,
		DeclarationVar,
	};

	struct AnalyzerInfo {
		uint64_t nextFreeStack = 0;

		uint64_t getFreeStack(size_t size) {
			auto output = nextFreeStack;
			nextFreeStack += size;
			return output;
		}
	};

	Variable proceedCommands(Environment& env, std::list<RawInstruction>& list, AnalyzerInfo& info, const Ketl::Parser::Node& node);

};

#endif /*analyzer_new_h*/