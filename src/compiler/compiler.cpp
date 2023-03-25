/*🍲Ketl🍲*/
#include "compiler.h"

#include "context.h"

namespace Ketl {

	std::variant<Variable, std::string> Compiler::compile(const std::string& str, Context& context) {
		auto block = _parser.parseTree(str);

		if (std::holds_alternative<std::string>(block)) {
			return std::get<std::string>(block);
		}

		SemanticAnalyzer analyzer(context);
		auto function = std::move(analyzer).compile(*std::get<0>(block));
		
		if (std::holds_alternative<std::string>(function)) {
			return std::get<std::string>(function);
		}

		std::vector<FunctionTypeObject::Parameter> parameters;

		auto classType = context.getVariable("ClassType").as<TypeObject>();
		auto voidType = context.getVariable("Void").as<TypeObject>();
		auto [functionType, typeRefHolder] = context.createObject<FunctionTypeObject>(*voidType, std::move(parameters));
		typeRefHolder->registerAbsLink(classType);
		typeRefHolder->registerAbsLink(voidType);

		auto functionHolder = reinterpret_cast<FunctionImpl**>(context.allocateOnHeap(functionType->sizeOf()));
		*functionHolder = std::get<0>(function);

		return Variable(context, TypedPtr(functionHolder, *functionType));
	}
}