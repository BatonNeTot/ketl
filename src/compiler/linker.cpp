/*🍲Ketl🍲*/
#include "linker.h"

#include "common.h"
#include "type.h"

namespace Ketl {
	static bool propagateFunctionCall(std::vector<Instruction>& instructions, Instruction& allocateInstruction,
		const std::vector<Linker::Variable*>& args, const std::vector<std::unique_ptr<const Type>>& argTypes);
	static bool propagateConstructor(std::vector<Instruction>& instructions, Argument output, Argument::Type outputType,
		const Type& type, Linker::Variable* arg) {
		std::vector<std::unique_ptr<const Type>> argTypes(1);
		argTypes[0] = Type::clone(arg->type);
		auto funcInfo = type.deduceFunction(argTypes);
		if (!funcInfo.returnType) {
			return false;
		}

		if (!funcInfo.function) {
			return false;
		}

		Instruction allocationInstruction;
		allocationInstruction._first.cPointer = funcInfo.function;
		allocationInstruction._firstType = Argument::Type::DerefLiteral;

		allocationInstruction._code = Instruction::Code::AllocateFunctionStack;
		allocationInstruction._output = output;
		allocationInstruction._outputType = outputType;

		std::vector<Linker::Variable*> args { arg };

		return propagateFunctionCall(instructions, allocationInstruction, args, *funcInfo.argTypes);
	}
	static bool propagateFunctionCall(std::vector<Instruction>& instructions, Instruction& allocateInstruction,
		const std::vector<Linker::Variable*>& args, const std::vector<std::unique_ptr<const Type>>& argTypes) {

		// allocate stack for function
		instructions.emplace_back(allocateInstruction);

		// add arguments instructions
		uint16_t currentOffset = 0;
		for (uint64_t i = 0u; i < args.size(); ++i) {
			auto& arg = args[i];
			auto& targetType = argTypes[i];

			auto outputTypeArg = Argument::deref(allocateInstruction._outputType);
			auto outputOffset = currentOffset;

			Argument first;
			Argument::Type firstType = Argument::Type::None;
			arg->propagateArgument(first, firstType);

			if (!arg->type->isRef && targetType->isRef) {
				instructions.emplace_back(
					Instruction::Code::Reference,
					outputOffset,
					outputTypeArg,
					firstType,
					Argument::Type::None,
					allocateInstruction._output,
					first,
					Argument()
				);

				currentOffset += sizeof(void*);
				continue;
			}

			if (arg->type->isRef && targetType->isRef) {
				instructions.emplace_back(
					Instruction::Code::Assign,
					outputOffset,
					outputTypeArg,
					firstType,
					Argument::Type::None,
					allocateInstruction._output,
					first,
					Argument()
				);

				currentOffset += sizeof(void*);
				continue;
			}

			if (!propagateConstructor(instructions, allocateInstruction._output, outputTypeArg, *targetType, arg)) {
				return false;
			}

			currentOffset += static_cast<uint16_t>(targetType->sizeOf());
		}

		// call function
		allocateInstruction._code = Instruction::Code::CallFunction;
		instructions.emplace_back(allocateInstruction);

		return true;
	}
	static bool propagateFunctionCall(std::vector<Instruction>& instructions, Argument output, Argument::Type outputType, 
		const Linker::Variable& variable, const std::vector<Linker::Variable*>& args) {
		std::vector<std::unique_ptr<const Type>> argTypes;
		argTypes.reserve(args.size());
		for (auto& arg : args) {
			argTypes.emplace_back(Type::clone(arg->type));
		}

		auto funcInfo = variable.type->deduceFunction(argTypes);
		if (!funcInfo.returnType) {
			return false;
		}

		Instruction allocationInstruction;
		allocationInstruction._code = Instruction::Code::AllocateFunctionStack;
		allocationInstruction._output = output;
		allocationInstruction._outputType = outputType;
		if (!funcInfo.function) {
			variable.propagateArgument(allocationInstruction._first, allocationInstruction._firstType);
		}
		else {
			allocationInstruction._first.cPointer = funcInfo.function;
			allocationInstruction._firstType = Argument::Type::DerefLiteral;
		}

		return propagateFunctionCall(instructions, allocationInstruction, args, *funcInfo.argTypes);
	}

	StandaloneFunction Linker::proceedStandalone(Environment& env, const std::string& source) {
		return { proceed(env, source) };
	}

	FunctionImpl Linker::proceed(Environment& env, const std::string& source) {
		static const std::string empty;
		_error = empty;

		auto byteData = _analyzer.proceed(source);
		if (byteData.empty()) {
			_error = _analyzer.errorMsg();
			return {};
		}

		return proceed(env, byteData.data(), byteData.size());
	}

	class VariableGlobal : public Linker::Variable {
	public:
		void propagateArgument(Argument& argument, Argument::Type& type) const override {
			type = Argument::Type::Global;
			if (this->type->isRef) {
				type = Argument::deref(type);
			}

			argument.globalPtr = ptr;
		}
		void* ptr = nullptr;
		std::string id;
	};

	class VariableFloat64 : public Linker::Variable {
	public:
		void propagateArgument(Argument& argument, Argument::Type& type) const override {
			type = Argument::Type::Literal;
			argument.floating = value;
		}
		double value;
	};

	class VariableDefainer : public Linker::Variable {
	public:
		bool addInstructions(Environment& env, std::vector<Instruction>& instructions) override {
			env._context.declareGlobal<void>(id, type);

			ptr = env._context.declareGlobal<void>(id, type);

			Argument output;
			auto outputType = Argument::Type::Global;
			output.globalPtr = ptr;

			propagateFunctionCall(instructions, output, outputType, *this, operationArgs);

			return true;
		}
		void propagateArgument(Argument& argument, Argument::Type& type) const override {
			type = Argument::Type::Global;
			argument.globalPtr = ptr;
		}
		uint64_t stackUsage() const override { return type->sizeOf(); }

		std::string id;
		void* ptr;
	};

	class VariableBinaryOperator : public Linker::Variable {
	public:
		bool addInstructions(Environment& env, std::vector<Instruction>& instructions) override {
			if (opCode == OperatorCode::Assign) {
				auto* global = dynamic_cast<VariableGlobal*>(operationArgs[0]);
				if (global->ptr == nullptr) {
					if (!global->type) {
						global->type = Type::clone(operationArgs[1]->type);
					}

					std::vector<std::unique_ptr<const Type>> argTypes(1);
					argTypes[0] = Type::clone(operationArgs[1]->type);

					auto funcInfo = global->type->deduceFunction(argTypes);
					if (!funcInfo.returnType) {
						return {};
					}

					// TODO pass id another way, deal with dynamic cast somehow butiful
					global->ptr = env._context.declareGlobal<void>(global->id, global->type);

					type = Type::clone(operationArgs[1]->type);

					argType = Argument::Type::Global;
					arg.globalPtr = global->ptr;

					propagateFunctionCall(instructions, arg, argType, *this, operationArgs);

					return true;
				}
			}

			if (!operationArgs[0]->type || !operationArgs[1]->type) {
				return false;
			}

			if (std::unique_ptr<const Type> floatType = std::make_unique<BasicType>(env._context.getVariable("Float64").as<BasicTypeBody>());
				floatType->id() == operationArgs[0]->type->id() && floatType->id() == operationArgs[1]->type->id()) {
				// TODO check type casting etc

				type = std::move(floatType);
				Instruction::Code code;
				// TODO insert here function type analog
				switch (opCode) {
				case OperatorCode::Plus: {
					code = Instruction::Code::AddFloat64;
					break;
				}
				case OperatorCode::Assign: {
					code = Instruction::Code::Assign;
					break;
				}
				}

				Argument output;
				output.stack = this->stackOffset;
				auto outputType = Argument::Type::Stack;

				Argument first;
				Argument::Type firstType;
				this->operationArgs[0]->propagateArgument(first, firstType);

				Argument second;
				Argument::Type secondType;
				this->operationArgs[1]->propagateArgument(second, secondType);

				auto& instruction = instructions.emplace_back(
					code,
					outputType,
					firstType,
					secondType,
					output,
					first,
					second
				);
			}
			// TODO decide function get it or throw
			return true;
		}
		void propagateArgument(Argument& argument, Argument::Type& type) const override {
			type = argType;
			argument = arg;
		}
		uint64_t stackUsage() const override { return type->sizeOf(); }

		OperatorCode opCode;
		Argument::Type argType;
		Argument arg;
	};

	class VariableInstruction : public Linker::Variable {
	public:
		bool addInstructions(Environment& env, std::vector<Instruction>& instructions) override {
			Argument output;
			output.stack = this->stackOffset;
			auto outputType = Argument::Type::Stack;

			Argument first;
			Argument::Type firstType;
			this->operationArgs[0]->propagateArgument(first, firstType);

			Argument second;
			Argument::Type secondType;
			this->operationArgs[1]->propagateArgument(second, secondType);

			auto& instruction = instructions.emplace_back(
				code,
				outputType,
				firstType,
				secondType,
				output,
				first,
				second
			);
			return true;
		}
		void propagateArgument(Argument& argument, Argument::Type& type) const override {
			type = Argument::Type::Stack;
			if (this->type->isRef) {
				type = Argument::deref(type);
			}

			argument.stack = stackOffset;
		}
		uint64_t stackUsage() const override { return type->sizeOf(); }

		Instruction::Code code;
	};

	class VariableConstructor : public Linker::Variable {
	public:
		bool addInstructions(Environment& env, std::vector<Instruction>& instructions) override {
			Argument output;
			auto outputType = Argument::Type::Global;
			output.globalPtr = ptr;

			propagateFunctionCall(instructions, output, outputType, *this, operationArgs);

			return true;
		}
		void propagateArgument(Argument& argument, Argument::Type& type) const override {
			type = Argument::Type::Global;
			argument.globalPtr = ptr;
		}
		uint64_t stackUsage() const override { return 0; }

		void* ptr;
		const FunctionImpl* function;
	};

	class VariableReturn : public Linker::Variable {
	public:
		bool addInstructions(Environment& env, std::vector<Instruction>& instructions) override {
			Argument output;
			auto outputType = Argument::Type::Return;

			propagateFunctionCall(instructions, output, outputType, *this, operationArgs);

			return true;
		}
		void propagateArgument(Argument& argument, Argument::Type& type) const override {
			type = Argument::Type::Return;
			if (this->type->isRef) {
				type = Argument::deref(type);
			}
		}
		uint64_t stackUsage() const override { return 0; }
	};

	class VariableFuncCall : public Linker::Variable {
	public:
		bool addInstructions(Environment& env, std::vector<Instruction>& instructions) override {
			Argument output;
			output.stack = stackOffset;
			auto outputType = Argument::Type::Stack;

			return propagateFunctionCall(instructions, output, outputType, *function, operationArgs);
		}
		void propagateArgument(Argument& argument, Argument::Type& type) const override {
			type = Argument::Type::Stack;
			if (this->type->isRef) {
				type = Argument::deref(type);
			}

			argument.stack = stackOffset;
		}
		uint64_t stackUsage() const override { return std::max(type->sizeOf(), sizeof(uint8_t*)); }

		Variable* function;
	};

	class VariableFuncArgument : public Linker::Variable {
		void propagateArgument(Argument& argument, Argument::Type& type) const override {
			type = Argument::Type::Stack;
			if (this->type->isRef) {
				type = Argument::deref(type);
			}

			argument.stack = stackOffset;
		}
		bool temporary() const override { return false; }
		uint64_t stackUsage() const override { return type->sizeOf(); }
	};

	FunctionImpl Linker::proceed(Environment& env, const uint8_t* bytecode, uint64_t size) {
		std::vector<std::unique_ptr<Variable>> variables;
		std::vector<Variable*> stack;

		return proceed(env, variables, stack, nullptr, bytecode, size);
	}

	static std::unique_ptr<Type> readType(Environment& env, uint64_t& iter, const uint8_t* bytecode, uint64_t size) {
		switch (static_cast<TypeCodes>(bytecode[iter++])) {
		case TypeCodes::Body: {
			std::string typeId(reinterpret_cast<const char*>(bytecode + iter));
			iter += typeId.length() + 1;
			auto var = env._context.getVariable(typeId);
			return std::make_unique<BasicType>(var.as<BasicTypeBody>());
		}
		case TypeCodes::Const: {
			auto type = readType(env, iter, bytecode, size);
			type->isConst = true;
			return type;
		}
		case TypeCodes::Pointer: {

			return {};
		}
		case TypeCodes::LRef: {
			auto type = readType(env, iter, bytecode, size);
			type->isRef = true;
			type->hasAddress = true;
			return type;
		}
		case TypeCodes::RRef: {
			auto type = readType(env, iter, bytecode, size);
			type->isRef = true;
			type->hasAddress = false;
			return type;
		}
		case TypeCodes::Function: {

			return {};
		}
		default: {
			return {};
		}
		}
	}

	FunctionImpl Linker::proceed(Environment& env, std::vector<std::unique_ptr<Variable>>& variables, std::vector<Variable*>& stack, const Type* globalReturnType, const uint8_t* bytecode, uint64_t size) {
		auto variableCount = *reinterpret_cast<const uint64_t*>(bytecode);
		uint64_t iter = sizeof(uint64_t);

		uint64_t initialCount = variables.size();
		variables.resize(variableCount);

		uint64_t currentStackUsage = 0u;
		uint64_t maxStackUsage = currentStackUsage;

		for (auto& stackVar : stack) {
			stackVar->stackOffset = currentStackUsage;
			currentStackUsage += stackVar->stackUsage();
		}
		maxStackUsage = currentStackUsage;

		for (auto i = initialCount; i < variableCount; ++i) {
			switch (static_cast<ByteInstruction>(bytecode[iter++])) {
			case ByteInstruction::Flush: {
				std::function<uint64_t(Variable*, uint64_t)> propagateStackOffsets;
				propagateStackOffsets = [&](Variable* var, uint64_t stackOffset) {
					if (!var) {
						return uint64_t(0u);
					}
					var->stackOffset = stackOffset;

					for (const auto& arg : var->operationArgs) {
						if (arg->temporary()) {
							stackOffset += propagateStackOffsets(arg, stackOffset);
						}
					}

					return std::max(var->stackUsage(), stackOffset - var->stackOffset);
				};

				auto usage = propagateStackOffsets(variables[i - 1].get(), currentStackUsage);
				maxStackUsage = std::max(maxStackUsage, currentStackUsage + usage);
				break;
			}
			case ByteInstruction::Variable: {
				std::string id(reinterpret_cast<const char*>(bytecode + iter));
				iter += id.length() + 1;
				auto var = env._context.getVariable(id);
				auto& type = var.type();

				auto global = std::make_unique<VariableGlobal>();
				global->type = Type::clone(type);
				global->ptr = var.as<void>();
				global->id = id;
				variables[i] = std::move(global);
				break;
			}
			case ByteInstruction::VariableDefinition: {
				std::string id(reinterpret_cast<const char*>(bytecode + iter));
				iter += id.length() + 1;
				auto type = readType(env, iter, bytecode, size);
				auto argc = *reinterpret_cast<const uint64_t*>(bytecode + iter);
				iter += sizeof(uint64_t);
				std::vector<Variable*> args(argc);
				std::vector<std::unique_ptr<const Type>> argTypes(argc);
				for (uint64_t y = 0; y < argc; ++y) {
					auto variableIndex = *reinterpret_cast<const uint64_t*>(bytecode + iter);
					iter += sizeof(uint64_t);
					args[y] = variables[variableIndex].get();
					argTypes[y] = Type::clone(variables[variableIndex]->type);
				}

				auto definer = std::make_unique<VariableDefainer>();
				definer->id = id;
				definer->operationArgs = std::move(args);
				type->hasAddress = true;
				definer->type = std::move(type);
				variables[i] = std::move(definer);

				break;
			}
			case ByteInstruction::Literal: {
				auto type = readType(env, iter, bytecode, size);
				if (type->id() == "Float64") {
					auto value = *reinterpret_cast<const double*>(bytecode + iter);
					iter += sizeof(double);

					auto liter = std::make_unique<VariableFloat64>();
					type->isConst = true;
					liter->type = std::move(type);
					liter->value = value;
					variables[i] = std::move(liter);
				}
				// TODO somehow move this to Type
				break;
			}
			case ByteInstruction::UnaryOperator: {
				std::string functionId(reinterpret_cast<const char*>(bytecode + iter));
				iter += functionId.length() + 1;
				auto variableIndex = *reinterpret_cast<const uint64_t*>(bytecode + iter);
				iter += sizeof(uint64_t);
				auto argType = Type::clone(variables[variableIndex]->type);
				// TODO decide function/instruction get it or throw
				break;
			}
			case ByteInstruction::BinaryOperator: {
				auto operatorCode = static_cast<OperatorCode>(bytecode[iter++]);
				std::vector<Variable*> args(2);
				for (uint64_t y = 0; y < 2; ++y) {
					auto variableIndex = *reinterpret_cast<const uint64_t*>(bytecode + iter);
					iter += sizeof(uint64_t);
					args[y] = variables[variableIndex].get();
				}
				
				auto varOp = std::make_unique<VariableBinaryOperator>();
				varOp->opCode = operatorCode;
				varOp->operationArgs = std::move(args);
				variables[i] = std::move(varOp);

				break;
			}
			case ByteInstruction::Function: {
				auto functionIndex = *reinterpret_cast<const uint64_t*>(bytecode + iter);
				iter += sizeof(uint64_t);
				auto argc = *reinterpret_cast<const uint64_t*>(bytecode + iter);
				iter += sizeof(uint64_t);
				std::vector<Variable*> args(argc);
				std::vector<std::unique_ptr<const Type>> argTypes(argc);
				for (uint64_t y = 0; y < argc; ++y) {
					auto variableIndex = *reinterpret_cast<const uint64_t*>(bytecode + iter);
					iter += sizeof(uint64_t);
					args[y] = variables[variableIndex].get();
					argTypes[y] = Type::clone(variables[variableIndex]->type);
				}
				auto function = variables[functionIndex].get();
				auto funcInfo = function->type->deduceFunction(argTypes);
				if (!funcInfo.returnType) {
					return {};
				}

				auto instruction = std::make_unique<VariableFuncCall>();
				instruction->function = function;
				instruction->operationArgs = std::move(args);
				instruction->type = std::move(funcInfo.returnType);
				variables[i] = std::move(instruction);

				break;
			}
			case ByteInstruction::FunctionDefinition: {
				std::string functionId(reinterpret_cast<const char*>(bytecode + iter));
				iter += functionId.length() + 1;
				auto returnType = readType(env, iter, bytecode, size);

				std::vector<std::unique_ptr<Variable>> funcVariables; 
				std::vector<Variable*> funcStack;

				auto argc = *reinterpret_cast<const uint64_t*>(bytecode + iter);
				iter += sizeof(uint64_t);
				std::vector<std::unique_ptr<const Type>> argTypes;
				for (uint64_t y = 0; y < argc; ++y) {
					auto argType = readType(env, iter, bytecode, size);
					if (!argType->isRef) {
						argType->hasAddress = true;
					}

					auto argument = std::make_unique<VariableFuncArgument>();
					argument->type = std::move(argType);
					argTypes.emplace_back(Type::clone(argument->type));
					
					funcStack.emplace_back(argument.get());
					funcVariables.emplace_back(std::move(argument));
				}
				auto byteDataSize = *reinterpret_cast<const uint64_t*>(bytecode + iter);
				iter += sizeof(uint64_t);

				auto pureFunction = proceed(env, funcVariables, funcStack, returnType.get(), bytecode + iter, byteDataSize);
				iter += byteDataSize;

				auto functionType = std::make_unique<FunctionType>();
				auto& info = functionType->info;
				info.returnType = std::move(returnType);
				info.argTypes = std::move(argTypes);

				std::unique_ptr<const Type> type = std::move(functionType);
				auto functionPtr = env._context.declareGlobal<FunctionImpl>(functionId, type);
				new(functionPtr) FunctionImpl(std::move(pureFunction));

				break;
			}
			case ByteInstruction::Return: {
				auto argIndex = *reinterpret_cast<const uint64_t*>(bytecode + iter);
				iter += sizeof(uint64_t);
				auto arg = variables[argIndex].get();
				std::vector<std::unique_ptr<const Type>> argTypes(1);
				argTypes[0] = Type::clone(arg->type);

				auto instruction = std::make_unique<VariableReturn>();
				instruction->operationArgs.reserve(1);
				instruction->operationArgs.emplace_back(arg);
				auto type = Type::clone(arg->type);
				type->hasAddress = true;
				instruction->type = std::move(type);
				variables[i] = std::move(instruction);
				break;
			}
			default: {
				// TODO Unknown bytecode
			}
			}
		}

		std::vector<Instruction> instructions;

		for (auto& variable : variables) {
			if (variable && !variable->addInstructions(env, instructions)) {
				return {};
			}
		}

		FunctionImpl func(env._alloc, maxStackUsage, instructions.size());
		std::memcpy(func._instructions, instructions.data(), instructions.size() * sizeof(Instruction));

		return func;
	}
}