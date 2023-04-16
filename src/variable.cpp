/*🍲Ketl🍲*/
#include "variable.h"

#include "ketl.h"

namespace Ketl {

	Variable::Variable(VirtualMachine& vm)
		: _vm(vm) {}
	Variable::Variable(VirtualMachine& vm, const TypedPtr& var)
		: _vm(vm) {
		auto& it = _vars.emplace_back(var);
		_vm._memory.registerAbsRoot(&it.type());
		if (it.type().isLight()) {
			_vm._memory.registerRefRoot(reinterpret_cast<void**>(it.rawData()));
		} else {
			_vm._memory.registerAbsRoot(it.rawData());
		}
	}
	Variable::Variable(VirtualMachine& vm, std::vector<TypedPtr>&& vars)
		: _vm(vm), _vars(std::move(vars)) {
		for (auto& var : _vars) {
			_vm._memory.registerAbsRoot(&var.type());
			if (var.type().isLight()) {
				_vm._memory.registerRefRoot(reinterpret_cast<void**>(var.rawData()));
			}
			else {
				_vm._memory.registerAbsRoot(var.rawData());
			}
		}
	}
	Variable::Variable(const Variable& variable)
		: _vm(variable._vm), _vars(variable._vars) {
		for (auto& var : _vars) {
			_vm._memory.registerAbsRoot(&var.type());
			if (var.type().isLight()) {
				_vm._memory.registerRefRoot(reinterpret_cast<void**>(var.rawData()));
			}
			else {
				_vm._memory.registerAbsRoot(var.rawData());
			}
		}
	}
	Variable::Variable(Variable&& variable)
		: _vm(variable._vm), _vars(std::move(variable._vars)) {}
	Variable::~Variable() {
		for (auto& var : _vars) {
			_vm._memory.registerAbsRoot(&var.type());
			if (var.type().isLight()) {
				_vm._memory.unregisterRefRoot(reinterpret_cast<void**>(var.rawData()));
			}
			else {
				_vm._memory.unregisterAbsRoot(var.rawData());
			}
		}
	}
}