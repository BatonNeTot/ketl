/*🍲Ketl🍲*/
#include "linker.h"

#include "type.h"

namespace Ketl {
	static void propagateConstructor(Environment& env, std::vector<Instruction>& instructions, Argument output, Argument::Type outputType,
		const Type& type, const std::vector<Linker::Variable*>& args, const std::vector<std::unique_ptr<const Type>>& argTypes) {
		auto funcInfo = type.deduceFunction(argTypes);
		if (!funcInfo.returnType) {
			return;
		}

		Argument functionArgument;
		Argument::Type functionType;
		if (funcInfo.isDynamic) {
			return;
		}
		else {
			functionArgument.cPointer = funcInfo.function;
			functionType = Argument::Type::Literal;
		}

		// allocate stack for function
		instructions.emplace_back(
			Instruction::Code::AllocateFunctionStack,
			outputType,
			functionType,
			Argument::Type::None,
			output,
			functionArgument,
			Argument()
		);

		// add arguments instructions
		uint16_t currentOffset = 0;
		for (uint64_t i = 0u; i < args.size(); ++i) {
			auto& arg = args[i];
			auto& targetType = funcInfo.argTypes[i];

			auto outputTypeArg = Argument::Type::None; 
			switch (outputType) {
			case Argument::Type::Stack: {
				outputTypeArg = Argument::Type::DerefStack;
				break;
			}
			case Argument::Type::DerefStack: {
				outputTypeArg = Argument::Type::DerefDerefStack;
				break;
			}
			case Argument::Type::Global: {
				outputTypeArg = Argument::Type::DerefGlobal;
				break;
			}
			case Argument::Type::DerefGlobal: {
				outputTypeArg = Argument::Type::DerefDerefGlobal;
				break;
			}
			case Argument::Type::Return: {
				outputTypeArg = Argument::Type::DerefReturn;
				break;
			}
			}
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
					output,
					first,
					Argument()
				);
				continue;
			}
		}

		// call function
		instructions.emplace_back(
			Instruction::Code::CallFunction,
			outputType,
			functionType,
			Argument::Type::None,
			output,
			functionArgument,
			Argument()
		);
	}
	static void propagateFunctionCall(Environment& env, std::vector<Instruction>& instructions, Argument output, Argument::Type outputType, 
		const Linker::Variable& variable, const std::vector<Linker::Variable*>& args, const std::vector<std::unique_ptr<const Type>>& argTypes) {
		auto funcInfo = variable.type->deduceFunction(argTypes);
		if (!funcInfo.returnType) {
			return;
		}

		Argument functionArgument;
		Argument::Type functionType;
		if (funcInfo.isDynamic) {
			variable.propagateArgument(functionArgument, functionType);
		}
		else {
			functionArgument.cPointer = funcInfo.function;
			functionType = Argument::Type::Literal;
		}

		// allocate stack for function
		instructions.emplace_back(
			Instruction::Code::AllocateFunctionStack,
			outputType,
			functionType,
			Argument::Type::None,
			output,
			functionArgument,
			Argument()
		);

		// add arguments instructions
		uint16_t currentOffset = 0;
		for (uint64_t i = 0u; i < args.size(); ++i) {
			auto& arg = args[i];
			auto& targetType = argTypes[i];

			auto outputTypeArg = Argument::Type::None;
			switch (outputType) {
			case Argument::Type::Stack: {
				outputTypeArg = Argument::Type::DerefStack;
				break;
			}
			case Argument::Type::DerefStack: {
				outputTypeArg = Argument::Type::DerefDerefStack;
				break;
			}
			case Argument::Type::Global: {
				outputTypeArg = Argument::Type::DerefGlobal;
				break;
			}
			case Argument::Type::DerefGlobal: {
				outputTypeArg = Argument::Type::DerefDerefGlobal;
				break;
			}
			case Argument::Type::Return: {
				outputTypeArg = Argument::Type::DerefReturn;
				break;
			}
			}
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
					output,
					first,
					Argument()
				);
				continue;
			}
		}

		// call function
		instructions.emplace_back(
			Instruction::Code::CallFunction,
			outputType,
			functionType,
			Argument::Type::None,
			output,
			functionArgument,
			Argument()
		);
	}

	StandaloneFunction Linker::proceedStandalone(Environment& env, const std::string& source) {
		return { proceed(env, source) };
	}

	FunctionImpl Linker::proceed(Environment& env, const std::string& source) {
		auto byteData = _analyzer.proceed(source);
		return proceed(env, byteData.data(), byteData.size());
	}

	class VariableGlobal : public Linker::Variable {
	public:
		void propagateArgument(Argument& argument, Argument::Type& type) const override {
			type = Argument::Type::Global;
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

	class VariableInstruction : public Linker::Variable {
	public:
		bool addInstructions(Environment& env, std::vector<Instruction>& instructions) const override {
			Argument output;
			output.stack = this->stackOffset;

			Argument first;
			Argument::Type firstType;
			this->operationArgs[0]->propagateArgument(first, firstType);

			Argument second;
			Argument::Type secondType;
			this->operationArgs[1]->propagateArgument(second, secondType);

			auto& instruction = instructions.emplace_back(
				code,
				Argument::Type::Stack,
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
			argument.stack = stackOffset;
		}
		uint64_t stackUsage() const override { return type->sizeOf(); }

		Instruction::Code code;
	};

	class VariableConstructor : public Linker::Variable {
	public:
		bool addInstructions(Environment& env, std::vector<Instruction>& instructions) const override {
			Argument output;
			auto outputType = Argument::Type::Global;
			output.globalPtr = ptr;

			propagateFunctionCall(env, instructions, output, outputType, *this, operationArgs, argTypes);

			return true;
		}
		void propagateArgument(Argument& argument, Argument::Type& type) const override {
			type = Argument::Type::Global;
			argument.globalPtr = ptr;
		}
		uint64_t stackUsage() const override { return 0; }

		void* ptr;
		const FunctionImpl* function;
		std::vector<std::unique_ptr<const Type>> argTypes;
	};

	class VariableReturn : public Linker::Variable {
	public:
		bool addInstructions(Environment& env, std::vector<Instruction>& instructions) const override {
			Argument output;
			auto outputType = Argument::Type::Return;

			propagateFunctionCall(env, instructions, output, outputType, *this, operationArgs, argTypes);

			return true;
		}
		void propagateArgument(Argument& argument, Argument::Type& type) const override {
			type = Argument::Type::Return;
		}
		uint64_t stackUsage() const override { return 0; }

		const FunctionImpl* function;
		std::vector<std::unique_ptr<const Type>> argTypes;
	};

	class VariableFuncCall : public Linker::Variable {
	public:
		bool addInstructions(Environment& env, std::vector<Instruction>& instructions) const override {
			Argument output;
			output.stack = stackOffset;

			Argument first;
			Argument::Type firstType;
			function->propagateArgument(first, firstType);

			// allocate stack for function
			instructions.emplace_back(
				Instruction::Code::AllocateDynamicFunctionStack,
				functionIndex,
				firstType,
				Argument::Type::None,
				output,
				first,
				Argument()
			);

			// add arguments instructions
			uint16_t currentOffset = 0;
			for (uint64_t i = 0u; i < operationArgs.size(); ++i) {
				auto& arg = operationArgs[i];
				auto& targetType = argTypes[i];

				auto outputTypeArg = Argument::Type::DerefStack;
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
						output,
						first,
						Argument()
					);
					continue;
				}

				std::vector<Linker::Variable*> constructorArg{ arg };
				std::vector<std::unique_ptr<const Type>> constructorArgType(1);
				constructorArgType[0] = Type::clone(arg->type);
				propagateConstructor(env, instructions, output, outputTypeArg, *targetType, constructorArg, constructorArgType);
			}

			// call function
			instructions.emplace_back(
				Instruction::Code::CallDynamicFunction,
				functionIndex,
				firstType,
				Argument::Type::None,
				output,
				first,
				Argument()
			);

			return true;
		}
		void propagateArgument(Argument& argument, Argument::Type& type) const override {
			type = Argument::Type::Stack;
			argument.stack = stackOffset;
		}
		uint64_t stackUsage() const override { return std::max(type->sizeOf(), sizeof(uint8_t*)); }

		uint8_t functionIndex;
		Variable* function;
		std::vector<std::unique_ptr<const Type>> argTypes;
	};

	class VariableFuncArgument : public Linker::Variable {
		void propagateArgument(Argument& argument, Argument::Type& type) const override {
			type = Argument::Type::Stack;
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

			return {};
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
				auto* type = var.type();

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
				auto funcInfo = type->deduceFunction(argTypes);
				if (!funcInfo.returnType) {
					return {};
				}
				
				auto instruction = std::make_unique<VariableConstructor>();
				instruction->ptr = env._context.declareGlobal<void>(id, *type);
				instruction->function = funcInfo.function;
				instruction->operationArgs = std::move(args);
				instruction->argTypes.reserve(funcInfo.argTypes.size());
				for (auto& argType : funcInfo.argTypes) {
					instruction->argTypes.emplace_back(Type::clone(argType));
				}
				type->hasAddress = true;
				instruction->type = std::move(type);
				variables[i] = std::move(instruction);

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

				if (operatorCode == OperatorCode::Assign) {
					auto* global = dynamic_cast<VariableGlobal*>(args[0]);
					if (global->ptr == nullptr) {
						if (!global->type) {
							global->type = Type::clone(args[1]->type);
						}

						std::vector<std::unique_ptr<const Type>> argTypes(1);
						argTypes[0] = Type::clone(args[1]->type);

						auto funcInfo = global->type->deduceFunction(argTypes);
						if (!funcInfo.returnType) {
							return {};
						}

						global->ptr = env._context.declareGlobal<void>(global->id, *global->type);

						auto instruction = std::make_unique<VariableConstructor>();
						instruction->ptr = global->ptr;
						instruction->function = funcInfo.function;
						instruction->operationArgs.reserve(1);
						instruction->operationArgs.emplace_back(args[1]);
						instruction->argTypes.reserve(funcInfo.argTypes.size());
						for (auto& argType : funcInfo.argTypes) {
							instruction->argTypes.emplace_back(Type::clone(argType));
						}
						auto type = Type::clone(global->type);
						type->hasAddress = true;
						instruction->type = std::move(type);
						variables[i] = std::move(instruction);

						continue;
					}
					// TODO pass id another way, deal with dynamic cast somehow butiful
				}

				if (!args[0]->type || !args[1]->type) {
					return {};
				}

				if (auto floatType = std::make_unique<BasicType>(env._context.getVariable("Float64").as<BasicTypeBody>());
					floatType->id() == args[0]->type->id() && floatType->id() == args[1]->type->id()) {
					// TODO check type casting etc

					auto instruction = std::make_unique<VariableInstruction>();
					auto outputType = Type::clone(floatType.get());
					instruction->type = std::move(outputType);
					instruction->operationArgs = std::move(args);
					// TODO insert here function type analog
					switch (operatorCode) {
					case OperatorCode::Plus: {
						instruction->code = Instruction::Code::AddFloat64;
						break;
					}
					case OperatorCode::Assign: {
						instruction->code = Instruction::Code::Assign;
						break;
					}
					}
					variables[i] = std::move(instruction);
				}
				// TODO decide function get it or throw
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
				instruction->functionIndex = funcInfo.functionIndex;
				instruction->function = function;
				instruction->operationArgs = std::move(args);
				instruction->argTypes.reserve(funcInfo.argTypes.size());
				for (auto& argType : funcInfo.argTypes) {
					instruction->argTypes.emplace_back(Type::clone(argType));
				}
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

				auto functionPtr = env._context.getVariable(functionId).as<FunctionContainer>();
				if (functionPtr) {
					// TODO expand function;
				}
				else {
					auto functionType = std::make_unique<FunctionType>();
					auto& info = functionType->infos.emplace_back();
					info.returnType = std::move(returnType);
					info.argTypes = std::move(argTypes);

					functionPtr = env._context.declareGlobal<FunctionContainer>(functionId, *functionType);
					new(functionPtr) FunctionContainer(env._alloc);
					functionPtr->emplaceFunction(std::move(pureFunction));
				}

				break;
			}
			case ByteInstruction::Return: {
				auto argIndex = *reinterpret_cast<const uint64_t*>(bytecode + iter);
				iter += sizeof(uint64_t);
				auto arg = variables[argIndex].get();
				std::vector<std::unique_ptr<const Type>> argTypes(1);
				argTypes[0] = Type::clone(arg->type);

				auto funcInfo = globalReturnType->deduceFunction(argTypes);
				if (!funcInfo.returnType) {
					return {};
				}

				auto instruction = std::make_unique<VariableReturn>();
				instruction->function = funcInfo.function;
				instruction->operationArgs.reserve(1);
				instruction->operationArgs.emplace_back(arg);
				instruction->argTypes.reserve(funcInfo.argTypes.size());
				for (auto& argType : funcInfo.argTypes) {
					instruction->argTypes.emplace_back(Type::clone(argType));
				}
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
				continue;
			}
		}

		FunctionImpl func(env._alloc, maxStackUsage, instructions.size());
		std::memcpy(func._instructions, instructions.data(), instructions.size() * sizeof(Instruction));

		return func;
	}
}