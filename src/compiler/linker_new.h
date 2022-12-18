/*🍲Ketl🍲*/
#ifndef linker_new_h
#define linker_new_h

#include "ketl_new.h"
#include "analyzer_new.h"

class Linker {
public:

	Linker() : _analyzer() {}

	StandaloneFunction proceed(Environment& env, const std::string& source);

private:

	void convertArgument(Environment& env, ArgumentType& type, Argument& value, const Analyzer::Variable& variable);

	void proceedCommand(Environment& env, Instruction& instruction, const Analyzer::RawInstruction& rawInstruction);

	Analyzer _analyzer;
};

#endif /*linker_new_h*/