/*🍲Ketl🍲*/
#include "type_manager.h"

namespace Ketl {

	const TypeObject* TypeManager::getType(const std::string_view& id) const {
		auto it = _types.find(id);
		return it == _types.end() ? nullptr : it->second;
	}

	const TypeObject* TypeManager::findOrCreateFunctionType(MemoryManager& memory, const TypeObject& returnType, std::vector<VarTraits>&& parameters) {
		auto functions = &_functionTypesByReturn[&returnType];
		size_t i = 0u, end = parameters.size();
		for (; i < end; ++i) {
			functions = &functions->nodes[parameters[i]];
		}

		auto& functionType = functions->leaf;

		if (!functionType) {
			// didn't found
			std::tie(functionType, std::ignore) = memory.createObject<FunctionTypeObject>(returnType, std::move(parameters));
			memory.registerAbsRoot(functionType);
		}

		return functionType;
	}

	const TypeObject& TypeManager::createPrimitiveType(MemoryManager& memory, const std::string_view& id, uint64_t size, std::type_index typeIndex) {
		auto [type, refs] = memory.createObject<PrimitiveTypeObject>(id, size);
		memory.registerAbsRoot(type);
		_types.try_emplace(type->id(), type);
		_userTypes.try_emplace(typeIndex, type);
		return *type;
	}

}