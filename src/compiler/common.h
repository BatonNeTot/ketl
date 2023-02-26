/*🍲Ketl🍲*/
#ifndef compiler_common_h
#define compiler_common_h

#include "ketl.h"
#include "type.h"

namespace Ketl {

	class Context;

	class TypeTemplate {
	public:
		virtual ~TypeTemplate() = default;
	};

	class AnalyzerContext;

	class AnalyzerVar {
	public:
		virtual ~AnalyzerVar() = default;
		virtual std::pair<Argument::Type, Argument> getArgument(AnalyzerContext& context) const = 0;
	};

	class RawInstruction {
	public:
		Instruction::Code code = Instruction::Code::None;
		AnalyzerVar* outputVar;
		AnalyzerVar* firstVar;
		AnalyzerVar* secondVar;

		void propagadeInstruction(Instruction& instruction, AnalyzerContext& context);
	};

	// Intermediate representation node
	class IRNode {
	public:

		virtual ~IRNode() = default;
		virtual AnalyzerVar* produceInstructions(std::vector<RawInstruction>& instructions, AnalyzerContext& context) const { return {}; };
		virtual const std::string& id() const {
			static const std::string empty;
			return empty;
		}
		virtual const std::shared_ptr<TypeTemplate>& type() const { 
			static const std::shared_ptr<TypeTemplate> empty;
			return empty; 
		};
		virtual uint64_t childCount() const = 0;
		virtual const std::unique_ptr<IRNode>& child(uint64_t index) const {
			static const std::unique_ptr<IRNode> empty;
			return empty;
		}
	};

}

#endif /*compiler_common_h*/