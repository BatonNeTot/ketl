/*🍲Ketl🍲*/
#include "compiler.h"

#include "context.h"
#include "ketl.h"

namespace Ketl {

	std::variant<Variable, std::string> Compiler::compile(const std::string_view& str, VirtualMachine& vm) {
		auto block = _parser.parseTree(str);

		if (std::holds_alternative<std::string>(block)) {
			return std::get<std::string>(block);
		}

		SemanticAnalyzer analyzer(vm);
		auto function = std::move(analyzer).compile(*std::get<0>(block));
		
		if (std::holds_alternative<std::string>(function)) {
			return std::get<std::string>(function);
		}

		std::vector<FunctionTypeObject::Parameter> parameters;

		auto classType = vm.getVariable("ClassType").as<TypeObject>();
		auto voidType = vm.getVariable("Void").as<TypeObject>();
		auto [functionType, typeRefHolder] = vm.createObject<FunctionTypeObject>(*voidType, std::move(parameters));
		typeRefHolder->registerAbsLink(classType);
		typeRefHolder->registerAbsLink(voidType);

		auto functionHolder = reinterpret_cast<FunctionObject**>(vm.allocateOnHeap(functionType->sizeOf()));
		*functionHolder = std::get<0>(function);

		return Variable(vm, TypedPtr(functionHolder, *functionType));
	}
}