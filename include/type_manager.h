/*🍲Ketl🍲*/
#ifndef type_manager_h
#define type_manager_h

#include "type.h"

#include <vector>

namespace Ketl {

	class TypeManager {
	public:

		template <typename T>
		const TypeObject* typeOf() const {
			auto userIt = _userTypes.find(typeid(T));
			if (userIt == _userTypes.end()) {
				return nullptr;
			}
			return userIt->second;
		}

		const TypeObject* findOrCreateFunctionType(MemoryManager& memory, const TypeObject& returnType, std::vector<VarTraits>&& parameters);

	private:

		template <typename T>
		const TypeObject& createPrimitiveType(MemoryManager& memory, const std::string_view& id) {
			return createPrimitiveType(memory, id, sizeof(T), typeid(T));
		}
		const TypeObject& createPrimitiveType(MemoryManager& memory, const std::string_view& id, uint64_t size, std::type_index typeIndex);

		friend class VirtualMachine;
		std::unordered_map<std::string_view, const TypeObject*> _types;
		std::unordered_map<std::type_index, const TypeObject*> _userTypes;

		std::unordered_map<const TypeObject*, MultiKeyMap<VarTraits, const TypeObject*, nullptr>> _functionTypesByReturn;

	};

}

#endif /*type_manager_h*/