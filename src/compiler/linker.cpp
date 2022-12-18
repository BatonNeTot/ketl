/*🍲Ketl🍲*/
#include "linker.h"

namespace Ketl {
	StandaloneFunction Linker::proceedStandalone(Environment& env, const std::string& source) {
		return { proceed(env, source) };
	}

	PureFunction Linker::proceed(Environment& env, const std::string& source) {
		auto byteData = _analyzer.proceed(source);
		return proceed(env, byteData.data(), byteData.size());
	}

	class VariableGlobal : public Linker::Variable {
	public:
		void propogateArgument(Argument& argument, Argument::Type& type) override {
			type = Argument::Type::Global;
			argument.globalPtr = ptr;
		}
		void* ptr = nullptr;
		std::string id;
	};

	class VariableFloat64 : public Linker::Variable {
	public:
		void propogateArgument(Argument& argument, Argument::Type& type) override {
			type = Argument::Type::Literal;
			argument.floating = value;
		}
		double value;
	};

	class VariableInstruction : public Linker::Variable {
	public:
		bool addInstructions(std::vector<Instruction>& instructions) override {
			Argument output;
			output.stack = this->stackOffset;

			Argument first;
			Argument::Type firstType;
			this->first->propogateArgument(first, firstType);

			Argument second;
			Argument::Type secondType;
			this->second->propogateArgument(second, secondType);

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
		void propogateArgument(Argument& argument, Argument::Type& type) override {
			type = Argument::Type::Stack;
			argument.stack = stackOffset;
		}
		uint64_t stackUsage() const override { return type->sizeOf(); }

		Instruction::Code code;
		Variable* first;
		Variable* second;
	};

	class VariableFuncStack : public Linker::Variable {
		void propogateArgument(Argument& argument, Argument::Type& type) override {
			type = Argument::Type::Stack;
			argument.stack = stackOffset;
		}
		uint64_t stackUsage() const override { return sizeof(void *); }
	};

	class VariableFuncCall : public Linker::Variable {
	public:
		bool addInstructions(std::vector<Instruction>& instructions) override {
			Argument output;
			output.stack = this->stackOffset;

			Argument first;
			Argument::Type firstType;
			this->first->propogateArgument(first, firstType);

			Argument second;
			Argument::Type secondType;
			this->second->propogateArgument(second, secondType);

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
		void propogateArgument(Argument& argument, Argument::Type& type) override {
			type = Argument::Type::Stack;
			argument.stack = stackOffset;
		}
		uint64_t stackUsage() const override { return type->sizeOf(); }

		Instruction::Code code;
		Variable* first;
		Variable* second;
	};

	class VariableFuncArgument : public Linker::Variable {
		void propogateArgument(Argument& argument, Argument::Type& type) override {
			type = Argument::Type::Stack;
			argument.stack = stackOffset;
		}
		uint64_t stackUsage() const override { return type->sizeOf(); }
	};

	PureFunction Linker::proceed(Environment& env, const uint8_t* bytecode, uint64_t size) {
		std::vector<std::unique_ptr<Variable>> variables;
		std::vector<Variable*> stack;

		return proceed(env, variables, stack, bytecode, size);
	}

	static std::unique_ptr<Type> readType(Environment& env, uint64_t& iter, const uint8_t* bytecode, uint64_t size) {
		switch (static_cast<TypeCodes>(bytecode[iter++])) {
		case TypeCodes::Body: {
			std::string typeId(reinterpret_cast<const char*>(bytecode + iter));
			iter += typeId.length() + 1;
			return std::make_unique<BasicType>(env._context.getGlobal<BasicTypeBody>(typeId));
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
			type->handling = Type::Handling::LRef;
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

	PureFunction Linker::proceed(Environment& env, std::vector<std::unique_ptr<Variable>>& variables, std::vector<Variable*>& stack, const uint8_t* bytecode, uint64_t size) {
		auto variableCount = *reinterpret_cast<const uint64_t*>(bytecode);
		uint64_t iter = sizeof(uint64_t);

		uint64_t initialCount = variables.size();
		variables.resize(variableCount);

		for (auto i = initialCount; i < variableCount; ++i) {
			switch (static_cast<ByteInstruction>(bytecode[iter++])) {
			case ByteInstruction::Variable: {
				std::string id(reinterpret_cast<const char*>(bytecode + iter));
				iter += id.length() + 1;
				auto* type = env._context.getGlobalType(id);

				auto global = std::make_unique<VariableGlobal>();
				global->type = Type::clone(type);
				global->ptr = env._context.getGlobal<void>(id);
				global->id = id;
				variables[i] = std::move(global);
				// TODO get Type and Ptr with one call
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
				type->deduceFunction(argTypes);
				/*
				auto function = variables[functionIndex].get();
				auto [funcIndex, returnType] = function->type->deduceFunction(argTypes);
				for (const auto& arg : args) {
					auto instruction = std::make_unique<VariableInstruction>();
					//instruction->type = Type::clone(floatType.get());
					instruction->first = args[0];
					instruction->second = args[1];
				}
				*/
				break;
			}
			case ByteInstruction::Literal: {
				auto type = readType(env, iter, bytecode, size);
				if (type->id() == "Float64") {
					auto value = *reinterpret_cast<const double*>(bytecode + iter);
					iter += sizeof(double);

					auto liter = std::make_unique<VariableFloat64>();
					type->isConst = true;
					type->handling = Type::Handling::RValue;
					liter->type = std::move(type);
					liter->value = value;
					variables[i] = std::move(liter);
				}
				// TODO somehow move this to Type
				break;
			}
			case ByteInstruction::FunctionStack: {
				auto stackVariable = std::make_unique<VariableFuncStack>();
				variables[i] = std::move(stackVariable);
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
				auto operatorCode = static_cast<OperatorCodes>(bytecode[iter++]);
				std::vector<Variable*> args(2);
				for (uint64_t y = 0; y < 2; ++y) {
					auto variableIndex = *reinterpret_cast<const uint64_t*>(bytecode + iter);
					iter += sizeof(uint64_t);
					args[y] = variables[variableIndex].get();
				}

				if (operatorCode == OperatorCodes::Assign && !args[0]->type) {
					// TODO check if type is present but the variable is new and it is initialization
					auto* global = dynamic_cast<VariableGlobal*>(args[0]);
					if (!global) {
						// something's wrong;
						return {};
					}
					args[0]->type = Type::clone(args[1]->type);
					global->type = Type::clone(args[1]->type);
					if (global->type) {
						global->ptr = env._context.declareGlobal<void>(global->id, *global->type);
					}
					// TODO pass id another way, deal with dynamic cast somehow butiful
				}

				if (!args[0]->type || !args[1]->type) {
					return {};
				}

				if (auto floatType = std::make_unique<BasicType>(env._context.getGlobal<BasicTypeBody>("Float64"));
					floatType->id() == args[0]->type->id() && floatType->id() == args[1]->type->id()) {
					// TODO check type casting etc

					auto instruction = std::make_unique<VariableInstruction>();
					auto outputType = Type::clone(floatType.get());
					outputType->handling = Type::Handling::RValue;
					instruction->type = std::move(outputType);
					instruction->first = args[0];
					instruction->second = args[1];
					// TODO insert here function type analog
					switch (operatorCode) {
					case OperatorCodes::Plus: {
						instruction->code = Instruction::Code::AddFloat64;
						break;
					}
					case OperatorCodes::Assign: {
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
				auto [funcIndex, returnType] = function->type->deduceFunction(argTypes);
				for (const auto& arg : args) {
					auto instruction = std::make_unique<VariableInstruction>();
					//instruction->type = Type::clone(floatType.get());
					instruction->first = args[0];
					instruction->second = args[1];
				}
				auto test = 0;
				// TODO decide function get it or throw
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

					auto argument = std::make_unique<VariableFuncArgument>();
					argument->type = std::move(argType);
					argTypes.emplace_back(Type::clone(argument->type));
					
					funcStack.emplace_back(argument.get());
					funcVariables.emplace_back(std::move(argument));
				}
				auto byteDataSize = *reinterpret_cast<const uint64_t*>(bytecode + iter);
				iter += sizeof(uint64_t);

				auto pureFunction = proceed(env, funcVariables, funcStack, bytecode + iter, byteDataSize);
				iter += byteDataSize;

				auto functionPtr = env._context.getGlobal<Function>(functionId);
				if (functionPtr) {
					// TODO expand function;
				}
				else {
					auto functionType = std::make_unique<FunctionType>();
					auto& info = functionType->infos.emplace_back();
					info.returnType = std::move(returnType);
					info.argTypes = std::move(argTypes);

					functionPtr = env._context.declareGlobal<Function>(functionId, *functionType);
					new(functionPtr) Function(env._alloc);
					functionPtr->emplaceFunction(std::move(pureFunction));
				}

				break;
			}
			default: {
				// TODO Unknown bytecode
			}
			}
		}

		uint64_t currentStackUsage = 0u;
		uint64_t maxStackUsage = currentStackUsage;

		for (auto& stackVar : stack) {
			stackVar->stackOffset = currentStackUsage;
			currentStackUsage += stackVar->stackUsage();
		}
		maxStackUsage = currentStackUsage;

		while (iter < size) {
			auto pushedCount = *reinterpret_cast<const uint64_t*>(bytecode + iter);
			iter += sizeof(uint64_t);

			for (uint64_t i = 0u; i < pushedCount; ++i) {
				auto variableIndex = *reinterpret_cast<const uint64_t*>(bytecode + iter);
				iter += sizeof(uint64_t);

				auto* variable = variables[variableIndex].get();
				variable->stackOffset = currentStackUsage;
				currentStackUsage += variable->stackUsage();
				stack.emplace_back(variable);
			}

			if (currentStackUsage > maxStackUsage) {
				maxStackUsage = currentStackUsage;
			}

			auto popedCount = *reinterpret_cast<const uint64_t*>(bytecode + iter);
			iter += sizeof(uint64_t);

			stack.erase(stack.end() - popedCount, stack.end());

			currentStackUsage = stack.empty() ? 0u : stack.back()->stackOffset + stack.back()->stackUsage();
		}

		std::vector<Instruction> instructions;

		for (auto& variable : variables) {
			if (variable && !variable->addInstructions(instructions)) {
				continue;
			}
		}

		PureFunction func(env._alloc, maxStackUsage, instructions.size());
		std::memcpy(func._instructions, instructions.data(), instructions.size() * sizeof(Instruction));

		return func;
	}
}