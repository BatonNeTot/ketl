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

		std::vector<TypedPtr> getVariable(const std::string_view& id) {
			auto it = _globals.find(id);
			if (it == _globals.end()) {
				return {};
			}
			return it->second;
		}

		void registerUserOperator(OperatorCode op, std::vector<VarTraits>&& parameters, const FunctionObject& function, const TypeObject& outputType);

		template <typename V>
		struct DeductionResult {
			const TypeObject* resultType;
			uint64_t score; // lower - better
			V value;

			friend bool operator<(const DeductionResult& lhs, const DeductionResult& rhs) {
				return lhs.score < rhs.score;
			}
		};

		DeductionResult<Instruction::Code> deduceBuiltInOperator(OperatorCode op, const std::vector<VarTraits>& args) const;
		DeductionResult<const FunctionObject*> deduceUserOperator(OperatorCode op, const std::vector<VarTraits>& args) const;

		void registerUserCast(const TypeObject& target, const TypeObject& source, const FunctionObject& function);

		Instruction::Code findBuiltInCast(const TypeObject& target, const TypeObject& source) const;
		const FunctionObject* findUserCast(const TypeObject& target, const TypeObject& source) const;

		void canBeCastedTo(const TypeObject& source, VarPureTraits traits, std::vector<VarTraits>& casts) const;
		bool canBeCastedTo(const TypeObject& source, const TypeObject& target) const;
	public: // TODO private

		void registerBuiltItOperator(OperatorCode op, std::vector<VarTraits>&& parameters, Instruction::Code code, const TypeObject& outputType);
		void registerBuiltItCast(const TypeObject& target, const TypeObject& source, OperatorCode op);


		static std::vector<TypedPtr> _emptyVars;
		std::unordered_map<std::string, std::vector<TypedPtr>, StringHash, StringEqualTo> _globals;

		struct OperatorDefinition {
			std::vector<VarTraits> parameters;
			const TypeObject* outputType;

			OperatorDefinition(std::vector<VarTraits>&& parameters_, const TypeObject& outputType_)
				: parameters(std::move(parameters_)), outputType(&outputType_) {}
		};

		struct BuiltInOperator : public OperatorDefinition {
			Instruction::Code instructionCode;

			BuiltInOperator(std::vector<VarTraits>&& parameterTypes_, const TypeObject& outputType_, Instruction::Code instructionCode_)
				: OperatorDefinition(std::move(parameterTypes_), outputType_), instructionCode(instructionCode_) {}
		};

		struct UserOperator : public OperatorDefinition {
			const FunctionObject* functionObject;

			UserOperator(std::vector<VarTraits>&& parameterTypes_, const TypeObject& outputType_, const FunctionObject& functionObject_)
				: OperatorDefinition(std::move(parameterTypes_), outputType_), functionObject(&functionObject_) {}
		};

		template <typename FunctionType>
		struct FunctionMap {
			std::unordered_map<uint64_t, std::vector<FunctionType>> functionsByParCount;
		};

		std::unordered_map<OperatorCode, FunctionMap<BuiltInOperator>> _operatorsBuiltIn;
		std::unordered_map<OperatorCode, FunctionMap<UserOperator>> _operatorsUser;

		struct BuiltInCast {
			Instruction::Code instructionCode;
		};

		struct UserCast  {
			const FunctionObject* functionObject;
		};

		template <typename FunctionType>
		struct CastFunctionMap {
			std::unordered_map<const TypeObject*, FunctionType> functionsByTarget;
		};

		std::unordered_map<const TypeObject*, CastFunctionMap<BuiltInCast>> _castsBuiltInBySource;
		std::unordered_map<const TypeObject*, CastFunctionMap<UserCast>> _castsUserBySource;
	};
}

#endif /*context_h*/