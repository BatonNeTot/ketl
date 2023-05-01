/*🍲Ketl🍲*/
#include "compiler.h"

#include "compiler/semantic_analyzer.h"
#include "context.h"
#include "ketl.h"

namespace Ketl {

	Compiler::Product Compiler::eval(const std::string_view& str, VirtualMachine& vm) {
		auto block = _parser.parseTree(str);

		if (std::holds_alternative<std::string>(block)) {
			return std::get<std::string>(block);
		}

		SemanticAnalyzer analyzer(vm, nullptr, true);
		auto compilationResult = std::move(analyzer).compile(*std::get<0>(block), nullptr);
		
		if (std::holds_alternative<std::string>(compilationResult)) {
			return std::get<std::string>(compilationResult);
		}

		auto& [function, returnType] = std::get<0>(compilationResult);

		std::vector<VarTraits> parameters;

		auto functionType = vm.findOrCreateFunctionType(*returnType, std::move(parameters));

		auto functionHolder = reinterpret_cast<FunctionObject**>(vm.allocateOnHeap(functionType->sizeOf()));
		*functionHolder = function;

		vm._memory.registerAbsLink(*functionHolder, functionType);

		Variable functionVariable(vm, TypedPtr(functionHolder, *functionType));
		vm.collectGarbage();
		return functionVariable();
	}
}