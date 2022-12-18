/*🍲Ketl🍲*/
#ifndef compiler_common_h
#define compiler_common_h

#include "ketl.h"
#include "type.h"

namespace Ketl {

	class Context;

	enum class OperatorCode : uint8_t {
		Constructor,
		Destructor,
		Plus,
		Minus,
		Multiply,
		Divide,
		Assign,
	};

	class TypeTemplate {
	public:
		virtual ~TypeTemplate() = default;
	};

	// Intermediate representation node
	class IRNode {
	public:

		virtual ~IRNode() = default;
		virtual bool resolveType(Context& context) { return false; };
		virtual bool processInstructions(std::vector<Instruction>& instructions) const { return false; };
		virtual std::shared_ptr<Type> produceInstructions(std::vector<Instruction>& instructions, Context& context) const { return {}; };
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