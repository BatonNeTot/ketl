/*🍲Ketl🍲*/
#ifndef linker_h
#define linker_h

#include "env.h"
#include "analyzer.h"

namespace Ketl {

	class Linker {
	public:

		Linker() : _analyzer() {}

		StandaloneFunction proceedStandalone(Environment& env, const std::string& source);

		FunctionImpl proceed(Environment& env, const std::string& source);

		FunctionImpl proceed(Environment& env, const Node* commandsNode);

		FunctionImpl proceed(Environment& env, const uint8_t* bytecode, uint64_t size);

		class Variable {
		public:
			virtual ~Variable() {}
			virtual bool addInstructions(Environment& env, std::vector<Instruction>& instructions) const { return false; }
			virtual void propagateArgument(Argument& argument, Argument::Type& type) const {}
			virtual uint64_t stackUsage() const { return 0; }
			virtual bool temporary() const { return true; }
			std::vector<Variable*> operationArgs;
			std::unique_ptr<const Type> type;
			uint64_t stackOffset = 0;
		};

		FunctionImpl proceed(Environment& env, std::vector<std::unique_ptr<Variable>>& variables, std::vector<Variable*>& stack, const Type* returnType, const uint8_t* bytecode, uint64_t size);

	private:

		Analyzer _analyzer;
	};
}

#endif /*linker_h*/