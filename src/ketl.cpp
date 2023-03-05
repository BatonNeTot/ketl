/*🍲Ketl🍲*/
#include "ketl.h"

namespace Ketl {

	inline static uint8_t* getArgument(uint8_t* stackPtr, uint8_t* returnPtr, Argument::Type type, Argument& value) {
		switch (type) {
		case Argument::Type::Global: {
			return reinterpret_cast<uint8_t*>(value.globalPtr);
		}
		case Argument::Type::Stack: {
			return stackPtr + value.stack;
		}
		case Argument::Type::Literal: {
			return reinterpret_cast<uint8_t*>(&value);
		}
		case Argument::Type::Return: {
			return returnPtr;
		}
		case Argument::Type::FunctionParameter: {
			return *reinterpret_cast<uint8_t**>(stackPtr + value.stack);
		}
		}
		return nullptr;
	}

	template <class T>
	inline static T& output(Instruction& instruction, uint8_t* stackPtr, uint8_t* returnPtr) {
		return *reinterpret_cast<T*>(getArgument(stackPtr, returnPtr, instruction.outputType, instruction.output));
	}

	template <class T>
	inline static T& first(Instruction& instruction, uint8_t* stackPtr, uint8_t* returnPtr) {
		return *reinterpret_cast<T*>(getArgument(stackPtr, returnPtr, instruction.firstType, instruction.first));
	}

	template <class T>
	inline static T& second(Instruction& instruction, uint8_t* stackPtr, uint8_t* returnPtr) {
		return *reinterpret_cast<T*>(getArgument(stackPtr, returnPtr, instruction.secondType, instruction.second));
	}

	void FunctionImpl::call(StackAllocator& stack, uint8_t* stackPtr, uint8_t* returnPtr) const {
		// TODO remove if
		if (_instructionsCount == CFUNC_INSTRUCTION_COUNT) {
			_cfunc(stack, stackPtr, returnPtr);
		}
		else {
			uint8_t* functionStack = stackPtr;
			uint64_t& index = *reinterpret_cast<uint64_t*>(stackPtr);
			stackPtr += sizeof(uint64_t);
			for (index = 0u; index < _instructionsCount;) {
				auto& instruction = _instructions[index];
				switch (instruction.code) {
				case Instruction::Code::AddInt64: {
					output<int64_t>(instruction, stackPtr, returnPtr) = first<int64_t>(instruction, stackPtr, returnPtr) + second<int64_t>(instruction, stackPtr, returnPtr);
					break;
				}
				case Instruction::Code::MinusInt64: {
					output<int64_t>(instruction, stackPtr, returnPtr) = first<int64_t>(instruction, stackPtr, returnPtr) - second<int64_t>(instruction, stackPtr, returnPtr);
					break;
				}
				case Instruction::Code::MultyInt64: {
					output<int64_t>(instruction, stackPtr, returnPtr) = first<int64_t>(instruction, stackPtr, returnPtr) * second<int64_t>(instruction, stackPtr, returnPtr);
					break;
				}
				case Instruction::Code::DivideInt64: {
					output<int64_t>(instruction, stackPtr, returnPtr) = first<int64_t>(instruction, stackPtr, returnPtr) / second<int64_t>(instruction, stackPtr, returnPtr);
					break;
				}
				case Instruction::Code::AddFloat64: {
					output<double>(instruction, stackPtr, returnPtr) = first<double>(instruction, stackPtr, returnPtr) + second<double>(instruction, stackPtr, returnPtr);
					break;
				}
				case Instruction::Code::MinusFloat64: {
					output<double>(instruction, stackPtr, returnPtr) = first<double>(instruction, stackPtr, returnPtr) - second<double>(instruction, stackPtr, returnPtr);
					break;
				}
				case Instruction::Code::MultyFloat64: {
					output<double>(instruction, stackPtr, returnPtr) = first<double>(instruction, stackPtr, returnPtr) * second<double>(instruction, stackPtr, returnPtr);
					break;
				}
				case Instruction::Code::DivideFloat64: {
					output<double>(instruction, stackPtr, returnPtr) = first<double>(instruction, stackPtr, returnPtr) / second<double>(instruction, stackPtr, returnPtr);
					break;
				}
				case Instruction::Code::DefinePrimitive: {
					first<Argument>(instruction, stackPtr, returnPtr) = second<Argument>(instruction, stackPtr, returnPtr);
					break;
				}
				case Instruction::Code::AssignPrimitive: {
					output<Argument>(instruction, stackPtr, returnPtr) = first<Argument>(instruction, stackPtr, returnPtr) = second<Argument>(instruction, stackPtr, returnPtr);
					break;
				}
				case Instruction::Code::AllocateFunctionStack: {
					auto& function = *first<FunctionImpl*>(instruction, stackPtr, returnPtr);
					functionStack = stack.allocate(function.stackSize()) + sizeof(uint64_t);
					break;
				}
				case Instruction::Code::DefineFuncParameter: {
					*reinterpret_cast<void**>(functionStack + first<uint64_t>(instruction, stackPtr, returnPtr)) = &second<uint8_t>(instruction, stackPtr, returnPtr);
					break;
				}
				case Instruction::Code::CallFunction: {
					auto& pureFunction = *first<FunctionImpl*>(instruction, stackPtr, returnPtr);
					auto& stackStart = output<uint8_t*>(instruction, stackPtr, returnPtr);
					auto funcReturnPtr = &output<uint8_t>(instruction, stackPtr, returnPtr);
					pureFunction.call(stack, functionStack - sizeof(uint64_t), funcReturnPtr);
					stack.deallocate(pureFunction.stackSize());
					break;
				}
				}
				++index;
			}
		}
	}
}