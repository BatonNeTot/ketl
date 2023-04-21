/*🍲Ketl🍲*/
#ifndef context_h
#define context_h

#include "common.h"
#include "type.h"
#include "memory/gc_allocator.h"

#include <cinttypes>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <memory>
#include <optional>
#include <typeindex>
#include <algorithm>

namespace Ketl {

	class Context {
	public:

		Context() = default;
		~Context() = default;

		template <typename T>
		const TypeObject* typeOf() const {
			auto userIt = _userTypes.find(typeid(T));
			if (userIt == _userTypes.end()) {
				return nullptr;
			}
			return userIt->second;
		}

		std::vector<TypedPtr> getVariable(const std::string_view& id) {
			auto it = _globals.find(id);
			if (it == _globals.end()) {
				return {};
			}
			return it->second;
		}

	public: // TODO private

		void registerPrimaryOperator(OperatorCode op, const std::string_view& argumentsNotation, Instruction::Code code, const std::string_view& outputType) {
			_primaryOperators[op].try_emplace(argumentsNotation, code, outputType);
		}

		std::pair<Instruction::Code, std::string_view> deducePrimaryOperator(OperatorCode op, const std::string_view& argumentsNotation) const {
			auto opIt = _primaryOperators.find(op);
			if (opIt == _primaryOperators.end()) {
				return std::make_pair<Instruction::Code, std::string_view>(Instruction::Code::None, "");
			}
			auto it = opIt->second.find(argumentsNotation);
			return it != opIt->second.end() ? it->second : std::make_pair<Instruction::Code, std::string_view>(Instruction::Code::None, "");
		}

		static std::vector<TypedPtr> _emptyVars;
		std::unordered_map<std::string, std::vector<TypedPtr>, StringHash, StringEqualTo> _globals;

		std::unordered_map<std::type_index, const TypeObject*> _userTypes;

		std::unordered_map<OperatorCode, std::unordered_map<std::string_view, std::pair<Instruction::Code, std::string_view>>> _primaryOperators;
	};
}

#endif /*context_h*/