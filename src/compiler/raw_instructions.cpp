/*🍲Ketl🍲*/
#include "raw_instructions.h"

#include "ketl.h"

namespace Ketl {

	bool InstructionSequence::collectReturnStatements(std::list<UndeterminedDelegate>& returnInstructions) const {
		for (auto cit = _rawInstructions.cbegin(), cend = _rawInstructions.cend(); cit != cend; ++cit) {
			const auto& rawInstruction = *cit;
			if (rawInstruction->collectReturnStatements(returnInstructions)) {
				++cit;
				if (cit != cend) {
					_analyzer.pushErrorMsg("[ERROR] Instructions after the return statement");
				}
				return true;
			}
		}
		return false;
	}

	FullInstruction* InstructionSequence::createFullInstruction() {
		auto fullInstruction = std::make_unique<FullInstruction>();
		auto fullInstructionPtr = fullInstruction.get();
		_rawInstructions.emplace_back(std::move(fullInstruction));
		return fullInstructionPtr;
	}

	CompilerVar InstructionSequence::createDefine(const std::string_view& id, const TypeObject& type, RawArgument* expression) {
		// TODO get const and ref
		auto var = _analyzer.createVar(id, {type, false, false });

		if (expression) {
			auto instruction = std::make_unique<FullInstruction>();

			instruction->code = Instruction::Code::Assign;
			instruction->arguments.emplace_back(_analyzer.createLiteralVar(expression->getType()->sizeOf()).argument);
			instruction->arguments.emplace_back(expression);
			instruction->arguments.emplace_back(var.argument);

			_rawInstructions.emplace_back(std::move(instruction));
		}

		return var;
	}

	RawArgument* InstructionSequence::createFunctionCall(RawArgument* caller, std::vector<RawArgument*>&& arguments) {
		auto functionVar = caller;
		auto functionType = functionVar->getType();

		// allocating stack
		auto allocInstruction = std::make_unique<FullInstruction>();
		allocInstruction->code = Instruction::Code::AllocateFunctionStack;

		auto stackVar = _analyzer.createTempVar(*_analyzer.vm().typeOf<uint64_t>());
		allocInstruction->arguments.emplace_back(functionVar);
		allocInstruction->arguments.emplace_back(stackVar);

		_rawInstructions.emplace_back(std::move(allocInstruction));

		// evaluating arguments
		auto& parameters = functionType->getParameters();
		for (auto i = 0u; i < arguments.size(); ++i) {
			auto& argumentVar = arguments[i];
			auto& parameter = parameters[i];

			auto isRef = parameter.isRef;
			if (!isRef) {
				// TODO create copy for the function call if needed, for now it's reference only
			}

			auto defineInstruction = std::make_unique<FullInstruction>();
			defineInstruction->code = Instruction::Code::DefineFuncParameter;

			defineInstruction->arguments.emplace_back(argumentVar);
			defineInstruction->arguments.emplace_back(stackVar);
			defineInstruction->arguments.emplace_back(_analyzer.createFunctionArgumentVar(i));

			_rawInstructions.emplace_back(std::move(defineInstruction));
		}

		auto& returnType = *functionType->getReturnType();
		auto outputVar = _analyzer.createTempVar(returnType);

		// calling the function
		auto callInstruction = std::make_unique<FullInstruction>();
		callInstruction->code = Instruction::Code::CallFunction;

		callInstruction->arguments.emplace_back(functionVar);
		callInstruction->arguments.emplace_back(stackVar);
		callInstruction->arguments.emplace_back(outputVar);

		_rawInstructions.emplace_back(std::move(callInstruction));

		return outputVar;
	}

	void InstructionSequence::createIfElseBranches(const IRNode& condition, const IRNode* trueBlock, const IRNode* falseBlock) {
		auto ifElseInstruction = std::make_unique<IfElseInstruction>(_analyzer);

		auto conditionVar = condition.produceInstructions(ifElseInstruction->conditionSeq(), _analyzer);
		ifElseInstruction->setConditionVar(conditionVar.getUVar().getVarAsItIs().argument);

		if (trueBlock) {
			trueBlock->produceInstructions(ifElseInstruction->trueBlockSeq(), _analyzer);
		}
		if (falseBlock) {
			falseBlock->produceInstructions(ifElseInstruction->falseBlockSeq(), _analyzer);
		}

		_rawInstructions.emplace_back(std::move(ifElseInstruction));
	}

	void InstructionSequence::createWhileElseBranches(const IRNode& condition, const IRNode* loopBlock, const IRNode* elseBlock) {
		auto whileElseInstruction = std::make_unique<WhileElseInstruction>(_analyzer);

		auto conditionVar = condition.produceInstructions(whileElseInstruction->conditionSeq(), _analyzer);
		whileElseInstruction->setConditionVar(conditionVar.getUVar().getVarAsItIs().argument);

		if (loopBlock) {
			loopBlock->produceInstructions(whileElseInstruction->loopBlockSeq(), _analyzer);
		}
		if (elseBlock) {
			elseBlock->produceInstructions(whileElseInstruction->elseBlockSeq(), _analyzer);
		}

		_rawInstructions.emplace_back(std::move(whileElseInstruction));
	}

	void InstructionSequence::createReturnStatement(UndeterminedDelegate expression) {
		auto returnInstruction = std::make_unique<ReturnInstruction>(std::move(expression));

		_rawInstructions.emplace_back(std::move(returnInstruction));
	}

	uint64_t InstructionSequence::propagadeInstruction(InstructionIterator instructions) const {
		uint64_t offset = 0u;
		for (auto& rawInstruction : _rawInstructions) {
			offset += rawInstruction->propagadeInstruction(instructions + offset);
		}
		return offset;
	}

}