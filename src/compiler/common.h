/*🍲Ketl🍲*/
#ifndef compiler_common_h
#define compiler_common_h

#include "ketl.h"
#include "type.h"

namespace Ketl {

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
		virtual bool resolveType() { return false; };
		virtual bool processInstructions(std::vector<Instruction>& instructions) const { return false; };
		virtual const std::shared_ptr<TypeTemplate>& type() const { return {}; };

	protected:
		std::vector<std::shared_ptr<IRNode>> children;
	};

}

#endif /*compiler_common_h*/