/*🐟Ketl🐟*/
#ifndef linker_new_h
#define linker_new_h

#include "eel_new.h"
#include "analyzer_new.h"

class Linker {
public:

	Linker(const std::string& source)
		: _analyzer(source) {}

	StandaloneFunction proceed(Environment& env);

private:

	void convertArgument(Environment& env, ArgumentType& type, Argument& value, const Analyzer::Variable& variable);

	void proceedCommand(Instruction& instruction, const Analyzer::RawInstruction& rawInstruction);

	Analyzer _analyzer;
};

#endif /*linker_new_h*/