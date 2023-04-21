/*🍲Ketl🍲*/
#ifndef compiler_raw_instructions_h
#define compiler_raw_instructions_h

#include "semantic_analyzer.h"

namespace Ketl {

	class FullInstruction;

	class InstructionSequence : public RawInstruction {
	public:

		InstructionSequence(SemanticAnalyzer& analyzer)
			: _analyzer(analyzer) {}

		FullInstruction* createFullInstruction();
		CompilerVar createDefine(const std::string_view& id, const TypeObject& type, RawArgument* expression);

		RawArgument* createFunctionCall(RawArgument* caller, std::vector<RawArgument*>&& arguments);

		void createIfElseBranches(const IRNode& condition, const IRNode* trueBlock, const IRNode* falseBlock);
		void createWhileElseBranches(const IRNode& condition, const IRNode* loopBlock, const IRNode* elseBlock);

		void createReturnStatement(UndeterminedDelegate expression);

		uint64_t propagadeInstruction(InstructionIterator instructions) const override;

		uint64_t countInstructions() const {
			uint64_t sum = 0u;
			for (const auto& rawInstruction : _rawInstructions) {
				sum += rawInstruction->countInstructions();
			}
			// TODO
			/*
			if (!_returnExpression.getUVar().empty()) {
				sum += Instruction::getCodeSize(Instruction::Code::ReturnValue);
			}
			else if (_mainSequence) {
				sum += Instruction::getCodeSize(Instruction::Code::Return);
			}
			*/
			return sum;
		}

		bool collectReturnStatements(std::list<UndeterminedDelegate>& returnInstructions) const override;

	private:

		bool verifyReturn();

		SemanticAnalyzer& _analyzer;
		std::vector<std::unique_ptr<RawInstruction>> _rawInstructions;

	};

	class FullInstruction : public RawInstruction {
	public:
		Instruction::Code code = Instruction::Code::None;
		std::list<RawArgument*> arguments;

		uint64_t propagadeInstruction(InstructionIterator instructions) const override {
			if (countInstructions() != arguments.size() + 1) {
				__debugbreak();
			}
			instructions->argument(0).uinteger = 0;
			instructions->code() = code;
			auto counter = 0u;
			for (const auto& argument : arguments) {
				++counter;
				std::tie(instructions->argumentType(counter), instructions->argument(counter)) = argument->getArgument();
			}

			return FullInstruction::countInstructions();
		}
		uint64_t countInstructions() const final override { return Instruction::getCodeSize(code); }

	};

	class ReturnInstruction : public RawInstruction {
	public:

		ReturnInstruction() = default;
		ReturnInstruction(UndeterminedDelegate&& expression)
			: _expression(std::move(expression)) {}

		uint64_t propagadeInstruction(InstructionIterator instructions) const {
			if (_expression.getUVar().empty()) {
				instructions->argument(0).uinteger = 0;
				instructions->code() = Instruction::Code::Return;

				return Instruction::getCodeSize(instructions->code());
			} else {
				instructions->argument(0).uinteger = 0;
				instructions->code() = Instruction::Code::ReturnValue;

				instructions->argumentType<1>() = Argument::Type::Literal;
				instructions->argument(1).uinteger = _expression.getUVar().getVarAsItIs().argument->getType()->sizeOf();

				std::tie(instructions->argumentType<2>(), instructions->argument(2)) = _expression.getUVar().getVarAsItIs().argument->getArgument();

				return Instruction::getCodeSize(instructions->code());
			}
		}

		uint64_t countInstructions() const override {
			const auto code = _expression.getUVar().empty() ? Instruction::Code::Return : Instruction::Code::ReturnValue;
			return Instruction::getCodeSize(code); 
		}

		bool collectReturnStatements(std::list<UndeterminedDelegate>& returnInstructions) const override {
			if (!_expression.getUVar().empty()) {
				returnInstructions.emplace_back(_expression);
			}
			return true;
		}

	private:

		UndeterminedDelegate _expression;
	};

	class CallInstruction : public RawInstruction {
	public:
		RawArgument* caller;
		std::vector<RawArgument*> arguments;

		uint64_t propagadeInstruction(InstructionIterator  instructions) const override {
			__debugbreak();
			return CallInstruction::countInstructions();
		}
		uint64_t countInstructions() const final override {
			return arguments.size() + Instruction::getCodeSize(Instruction::Code::AllocateFunctionStack) + Instruction::getCodeSize(Instruction::Code::AllocateFunctionStack); // allocate; set all arguments; call
		}
	};

	class IfElseInstruction : public RawInstruction {
	public:

		IfElseInstruction(SemanticAnalyzer& analyzer)
			: _conditionSeq(analyzer), _trueBlockSeq(analyzer), _falseBlockSeq(analyzer) {}

		uint64_t propagadeInstruction(InstructionIterator instructions) const override {
			_conditionSeq.propagadeInstruction(instructions);
			auto conditionSize = _conditionSeq.countInstructions();

			auto trueSize = _trueBlockSeq.countInstructions();
			auto falseSize = _falseBlockSeq.countInstructions();

			instructions += conditionSize;

			if (trueSize == 0) {
				instructions->argument(0).uinteger = 0;
				instructions->code() = Instruction::Code::JumpIfNotZero;
				instructions->argument(1).integer = instructions.offset() + falseSize
					+ Instruction::getCodeSize(Instruction::Code::JumpIfNotZero); // plus if jump
				instructions->argumentType<1>() = Argument::Type::Literal;

				std::tie(instructions->argumentType<2>(), instructions->argument(2)) = _conditionVar->getArgument();

				instructions += Instruction::getCodeSize(Instruction::Code::JumpIfNotZero);
				_falseBlockSeq.propagadeInstruction(instructions);
			}
			else {
				instructions->argument(0).uinteger = 0;
				instructions->code() = Instruction::Code::JumpIfZero;

				std::tie(instructions->argumentType<2>(), instructions->argument(2)) = _conditionVar->getArgument();

				if (falseSize == 0) {
					instructions->argument(1).integer = instructions.offset() + trueSize
						+ Instruction::getCodeSize(Instruction::Code::JumpIfZero);	// plus if jump
					instructions->argumentType<1>() = Argument::Type::Literal;

					instructions += Instruction::getCodeSize(Instruction::Code::JumpIfZero);
					_trueBlockSeq.propagadeInstruction(instructions);
				}
				else {
					instructions->argument(1).integer = instructions.offset() + trueSize
						+ Instruction::getCodeSize(Instruction::Code::JumpIfZero)	// plus if jump 
						+ Instruction::getCodeSize(Instruction::Code::Jump);		// plus goto jump
					instructions->argumentType<1>() = Argument::Type::Literal;

					instructions += Instruction::getCodeSize(Instruction::Code::JumpIfZero);
					_trueBlockSeq.propagadeInstruction(instructions);

					instructions += trueSize;
					instructions->argument(0).uinteger = 0;
					instructions->code() = Instruction::Code::Jump;
					instructions->argument(1).integer = instructions.offset() + falseSize
						+ Instruction::getCodeSize(Instruction::Code::Jump);		// plus goto jump
					instructions->argumentType<1>() = Argument::Type::Literal;

					_falseBlockSeq.propagadeInstruction(instructions + Instruction::getCodeSize(Instruction::Code::Jump));
				}
			}

			auto countBase = conditionSize
				+ trueSize
				+ falseSize; // jump instruction
			if (trueSize != 0 && falseSize != 0) {
				countBase
					+= Instruction::getCodeSize(Instruction::Code::JumpIfZero)	// plus if jump 
					+ Instruction::getCodeSize(Instruction::Code::Jump);		// plus goto jump
			}
			else {
				countBase
					+= Instruction::getCodeSize(Instruction::Code::JumpIfZero);	// plus if jump 
			}
			return countBase;
		}
		uint64_t countInstructions() const override {
			auto conditionSize = _conditionSeq.countInstructions();
			auto trueSize = _trueBlockSeq.countInstructions();
			auto falseSize = _falseBlockSeq.countInstructions();
			auto countBase = conditionSize
				+ trueSize
				+ falseSize; // jump instruction
			if (trueSize != 0 && falseSize != 0) {
				countBase 
					+= Instruction::getCodeSize(Instruction::Code::JumpIfZero)	// plus if jump 
					+ Instruction::getCodeSize(Instruction::Code::Jump);		// plus goto jump
			}
			else {
				countBase
					+= Instruction::getCodeSize(Instruction::Code::JumpIfZero);	// plus if jump 
			}
			return countBase;
		}

		InstructionSequence& conditionSeq() {
			return _conditionSeq;
		}

		InstructionSequence& trueBlockSeq() {
			return _trueBlockSeq;
		}

		InstructionSequence& falseBlockSeq() {
			return _falseBlockSeq;
		}

		void setConditionVar(RawArgument* var) {
			_conditionVar = var;
		}

	private:
		InstructionSequence _conditionSeq;
		InstructionSequence _trueBlockSeq;
		InstructionSequence _falseBlockSeq;
		RawArgument* _conditionVar = nullptr;
	};

	class WhileElseInstruction : public RawInstruction {
	public:

		WhileElseInstruction(SemanticAnalyzer& analyzer)
			: _conditionSeq(analyzer), _loopBlockSeq(analyzer), _elseBlockSeq(analyzer) {}

		uint64_t propagadeInstruction(InstructionIterator instructions) const override {
			_conditionSeq.propagadeInstruction(instructions);
			auto conditionSize = _conditionSeq.countInstructions();

			auto loopSize = _loopBlockSeq.countInstructions();
			auto elseSize = _elseBlockSeq.countInstructions();

			instructions += conditionSize;

			if (elseSize == 0) {
				instructions->argument(0).uinteger = 0;
				instructions->code() = Instruction::Code::JumpIfZero;
				instructions->argument(1).integer = instructions.offset() + loopSize
					+ Instruction::getCodeSize(Instruction::Code::JumpIfZero)	// plus if jump
					+ Instruction::getCodeSize(Instruction::Code::Jump);		// plus goto jump
				instructions->argumentType<1>() = Argument::Type::Literal;

				std::tie(instructions->argumentType<2>(), instructions->argument(2)) = _conditionVar->getArgument();

				instructions += Instruction::getCodeSize(Instruction::Code::JumpIfZero);

				_loopBlockSeq.propagadeInstruction(instructions);
				instructions += loopSize;

				auto backJump = -static_cast<int64_t>(loopSize + conditionSize
					+ Instruction::getCodeSize(Instruction::Code::JumpIfZero));	// plus if jump
				instructions->argument(0).uinteger = 0;
				instructions->code() = Instruction::Code::Jump;
				instructions->argument(1).integer = instructions.offset() + backJump;
				instructions->argumentType<1>() = Argument::Type::Literal;
			}
			else {
				instructions->argument(0).uinteger = 0;
				instructions->code() = Instruction::Code::JumpIfNotZero;
				instructions->argument(1).integer = instructions.offset() + elseSize
					+ Instruction::getCodeSize(Instruction::Code::JumpIfNotZero)	// plus if jump
					+ Instruction::getCodeSize(Instruction::Code::Jump);		// plus goto jump
				instructions->argumentType<1>() = Argument::Type::Literal;

				std::tie(instructions->argumentType<2>(), instructions->argument(2)) = _conditionVar->getArgument();

				instructions += Instruction::getCodeSize(Instruction::Code::JumpIfNotZero);

				_elseBlockSeq.propagadeInstruction(instructions);
				instructions += elseSize;

				instructions->argument(0).uinteger = 0;
				instructions->code() = Instruction::Code::Jump;
				instructions->argument(1).integer = instructions.offset() + loopSize + conditionSize
					+ Instruction::getCodeSize(Instruction::Code::JumpIfNotZero)	// plus if jump
					+ Instruction::getCodeSize(Instruction::Code::Jump);		// plus goto jump
				instructions->argumentType<1>() = Argument::Type::Literal;

				instructions += Instruction::getCodeSize(Instruction::Code::Jump); // else goto jump

				_loopBlockSeq.propagadeInstruction(instructions);
				instructions += loopSize;

				_conditionSeq.propagadeInstruction(instructions); // duplicate conditional seq
				instructions += conditionSize;

				auto backJump = -static_cast<int64_t>(loopSize + conditionSize);
				instructions->argument(0).uinteger = 0;
				instructions->code() = Instruction::Code::JumpIfNotZero;
				instructions->argument(1).integer = instructions.offset() + backJump;
				instructions->argumentType<1>() = Argument::Type::Literal;
				std::tie(instructions->argumentType<2>(), instructions->argument(2)) = _conditionVar->getArgument();
			}

			auto countBase = conditionSize
				+ loopSize
				+ Instruction::getCodeSize(Instruction::Code::JumpIfZero)	// plus if jump
				+ Instruction::getCodeSize(Instruction::Code::Jump);		// plus goto jump
			if (elseSize != 0) {
				countBase += conditionSize // separate condition block
					+ Instruction::getCodeSize(Instruction::Code::JumpIfNotZero)	// plus if jump
					+ elseSize;
			}
			return countBase;
		}
		uint64_t countInstructions() const override {
			auto conditionSize = _conditionSeq.countInstructions();
			auto loopSize = _loopBlockSeq.countInstructions();
			auto elseSize = _elseBlockSeq.countInstructions();
			auto countBase = conditionSize
				+ loopSize
				+ Instruction::getCodeSize(Instruction::Code::JumpIfZero)	// plus if jump
				+ Instruction::getCodeSize(Instruction::Code::Jump);		// plus goto jump
			if (elseSize != 0) {
				countBase += conditionSize // separate condition block
					+ Instruction::getCodeSize(Instruction::Code::JumpIfNotZero)	// plus if jump
					+ elseSize;
			}
			return countBase;
		}

		InstructionSequence& conditionSeq() {
			return _conditionSeq;
		}

		InstructionSequence& loopBlockSeq() {
			return _loopBlockSeq;
		}

		InstructionSequence& elseBlockSeq() {
			return _elseBlockSeq;
		}

		void setConditionVar(RawArgument* var) {
			_conditionVar = var;
		}

	private:
		InstructionSequence _conditionSeq;
		InstructionSequence _loopBlockSeq;
		InstructionSequence _elseBlockSeq;
		RawArgument* _conditionVar = nullptr;
	};

}

#endif /*compiler_raw_instructions_h*/