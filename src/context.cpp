/*🍲Ketl🍲*/
#include "context.h"

namespace Ketl {

	std::vector<TypedPtr> Context::_emptyVars;

	void Context::registerUserOperator(OperatorCode op, std::vector<VarTraits>&& parameters, const FunctionObject& function, const TypeObject& outputType) {
		auto& operators = _operatorsUser[op];
		auto& functions = operators.functionsByParCount[parameters.size()];
		functions.emplace_back(std::move(parameters), outputType, function);
	}

	void Context::registerBuiltItOperator(OperatorCode op, std::vector<VarTraits>&& parameters, Instruction::Code code, const TypeObject& outputType) {
		auto& operators = _operatorsBuiltIn[op];
		auto& functions = operators.functionsByParCount[parameters.size()];
		functions.emplace_back(std::move(parameters), outputType, code);
	}

	void Context::registerUserCast(const TypeObject& target, const TypeObject& source, const FunctionObject& function) {
		__debugbreak();
	}

	void Context::registerBuiltItCast(const TypeObject& target, const TypeObject& source, OperatorCode op) {
		__debugbreak();
	}


	Context::DeductionResult<Instruction::Code> Context::deduceBuiltInOperator(OperatorCode op, const std::vector<VarTraits>& args) const {
		auto operatorIt = _operatorsBuiltIn.find(op);
		if (operatorIt == _operatorsBuiltIn.end()) {
			return { nullptr, std::numeric_limits<uint64_t>::max(), Instruction::Code::None };
		}

		auto& functionsByParCount = operatorIt->second.functionsByParCount;
		auto functionsIt = functionsByParCount.find(args.size());
		if (functionsIt == functionsByParCount.end()) {
			return { nullptr, std::numeric_limits<uint64_t>::max(), Instruction::Code::None };
		}

		auto& functions = functionsIt->second;
		
		auto bestScore = std::numeric_limits<uint64_t>::max();
		const BuiltInOperator* bestOperator = nullptr;

		for (auto& function : functions) {
			uint64_t score = 0u;

			uint64_t i = 0u;
			for (; i < args.size(); ++i) {
				auto& arg = args[i];
				auto& parameter = function.parameters[i];

				if (!arg.convertableTo(parameter) || !canBeCastedTo(*arg.type, *parameter.type)) {
					break;
				}

				if (!arg.sameTraits(parameter)) {
					++score;
				}

				if (arg.type != parameter.type) {
					++score;
				}
			}

			if (i < args.size()) {
				continue;
			}

			if (score < bestScore) {
				bestScore = score;
				bestOperator = &function;
			}
			else if (score == bestScore) {
				bestOperator = nullptr;
			}
		}

		if (!bestOperator) {
			return { nullptr, std::numeric_limits<uint64_t>::max(), Instruction::Code::None };
		}

		return { bestOperator->outputType, bestScore, bestOperator->instructionCode };
	}

	Context::DeductionResult<const FunctionObject*> Context::deduceUserOperator(OperatorCode op, const std::vector<VarTraits>& args) const {
		__debugbreak();
		return { nullptr, std::numeric_limits<uint64_t>::max(), nullptr };
	}

	Instruction::Code Context::findBuiltInCast(const TypeObject& target, const TypeObject& source) const {
		__debugbreak();
		return Instruction::Code::None;
	}

	const FunctionObject* Context::findUserCast(const TypeObject& target, const TypeObject& source) const {
		__debugbreak();
		return nullptr;
	}

	void Context::canBeCastedTo(const TypeObject& source, VarPureTraits traits, std::vector<VarTraits>& casts) const {
		casts.emplace_back(source, traits);

		{
			auto it = _castsBuiltInBySource.find(&source);
			if (_castsBuiltInBySource.end() != it) {
				auto& functionsByTarget = it->second.functionsByTarget;
				casts.reserve(casts.size() + functionsByTarget.size());
				for (auto& target : functionsByTarget) {
					casts.emplace_back(*target.first, traits);
				}
			}
		}

		{
			auto it = _castsUserBySource.find(&source);
			if (_castsUserBySource.end() != it) {
				auto& functionsByTarget = it->second.functionsByTarget;
				casts.reserve(casts.size() + functionsByTarget.size());
				for (auto& target : functionsByTarget) {
					casts.emplace_back(*target.first, traits);
				}
			}
		}
	}

	bool Context::canBeCastedTo(const TypeObject& source, const TypeObject& target) const {
		if (&source == &target) {
			return true;
		}

		{
			auto it = _castsBuiltInBySource.find(&source);
			if (_castsBuiltInBySource.end() != it) {
				auto& functionsByTarget = it->second.functionsByTarget;
				if (functionsByTarget.find(&target) != functionsByTarget.end()) {
					return true;
				}
			}
		}

		{
			auto it = _castsUserBySource.find(&source);
			if (_castsUserBySource.end() != it) {
				auto& functionsByTarget = it->second.functionsByTarget;
				if (functionsByTarget.find(&target) != functionsByTarget.end()) {
					return true;
				}
			}
		}

		return false;
	}
}