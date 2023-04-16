/*🍲Ketl🍲*/
#ifndef variable_h
#define variable_h

#include "common.h"

#include <vector>

namespace Ketl {

	class VirtualMachine;
	class Variable {
	public:

		Variable(VirtualMachine& vm);
		Variable(VirtualMachine& vm, const TypedPtr& var);
		Variable(VirtualMachine& vm, std::vector<TypedPtr>&& vars);
		Variable(const Variable& variable);
		Variable(Variable&& variable);
		~Variable();

		template <typename ...Args>
		Variable operator()(Args&& ...args) const;

		template <typename T>
		T* as() {
			return reinterpret_cast<T*>(_vars[0].as(typeid(T), _vm));
		}

		bool empty() const {
			return _vars.empty();
		}

	private:
		friend class SemanticAnalyzer;

		VirtualMachine& _vm;
		std::vector<TypedPtr> _vars;
	};
}

#endif /*ketl_h*/