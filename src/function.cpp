/*🍲Ketl🍲*/
#include "function.h"

namespace Ketl {

	inline static uint8_t* getArgument(uint8_t* stackPtr, Argument::Type type, Argument& value) {
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
		case Argument::Type::FunctionParameter: {
			return *reinterpret_cast<uint8_t**>(stackPtr + value.stack);
		}
		}
		return nullptr;
	}

	template <unsigned N, class T>
	inline static T& argument(Instruction& instruction, uint8_t* stackPtr) {
		return *reinterpret_cast<T*>(getArgument(stackPtr, instruction.argumentType<N + 1>(), instruction.argument(N + 1)));
	}

	inline static bool memequ(const void* lhs, const void* rhs, size_t size) {
		return 0 == memcmp(lhs, rhs, size);
	}

	void FunctionObject::call(StackAllocator<Allocator>& stack, uint8_t* stackPtr, uint8_t* returnPtr) const {
		uint64_t& index = *reinterpret_cast<uint64_t*>(stackPtr);
		stackPtr += sizeof(index);
		for (index = 0u;;) {
			auto& instruction = _instructions[index];
			switch (instruction.code()) {
			case Instruction::Code::AddInt64: {
				argument<2, int64_t>(instruction, stackPtr) = argument<0, int64_t>(instruction, stackPtr) + argument<1, int64_t>(instruction, stackPtr);
				break;
			}
			case Instruction::Code::MinusInt64: {
				argument<2, int64_t>(instruction, stackPtr) = argument<0, int64_t>(instruction, stackPtr) - argument<1, int64_t>(instruction, stackPtr);
				break;
			}
			case Instruction::Code::MultyInt64: {
				argument<2, int64_t>(instruction, stackPtr) = argument<0, int64_t>(instruction, stackPtr) * argument<1, int64_t>(instruction, stackPtr);
				break;
			}
			case Instruction::Code::DivideInt64: {
				argument<2, int64_t>(instruction, stackPtr) = argument<0, int64_t>(instruction, stackPtr) / argument<1, int64_t>(instruction, stackPtr);
				break;
			}
			case Instruction::Code::AddFloat64: {
				argument<2, double>(instruction, stackPtr) = argument<0, double>(instruction, stackPtr) + argument<1, double>(instruction, stackPtr);
				break;
			}
			case Instruction::Code::MinusFloat64: {
				argument<2, double>(instruction, stackPtr) = argument<0, double>(instruction, stackPtr) - argument<1, double>(instruction, stackPtr);
				break;
			}
			case Instruction::Code::MultyFloat64: {
				argument<2, double>(instruction, stackPtr) = argument<0, double>(instruction, stackPtr) * argument<1, double>(instruction, stackPtr);
				break;
			}
			case Instruction::Code::DivideFloat64: {
				argument<2, double>(instruction, stackPtr) = argument<0, double>(instruction, stackPtr) / argument<1, double>(instruction, stackPtr);
				break;
			}
			case Instruction::Code::IsStructEqual: {
				argument<3, uint64_t>(instruction, stackPtr) = memequ(
					&argument<1, uint8_t>(instruction, stackPtr), 
					&argument<2, uint8_t>(instruction, stackPtr),
					argument<0, uint64_t>(instruction, stackPtr));
				break;
			}
			case Instruction::Code::IsStructNonEqual: {
				argument<3, uint64_t>(instruction, stackPtr) = !memequ(
					&argument<1, uint8_t>(instruction, stackPtr),
					&argument<2, uint8_t>(instruction, stackPtr),
					argument<0, uint64_t>(instruction, stackPtr));
				break;
			}
			case Instruction::Code::Assign: {
				memcpy(
					&argument<2, uint8_t>(instruction, stackPtr), 
					&argument<1, uint8_t>(instruction, stackPtr),
					argument<0, uint64_t>(instruction, stackPtr));
				break;
			}
			case Instruction::Code::AllocateFunctionStack: {
				auto& function = *argument<0, FunctionObject*>(instruction, stackPtr);
				argument<1, uint8_t*>(instruction, stackPtr) = stack.allocate(function.stackSize());
				break;
			}
			case Instruction::Code::DefineFuncParameter: {
				auto value = &argument<0, uint8_t>(instruction, stackPtr);
				auto& stackStart = argument<1, uint8_t*>(instruction, stackPtr);
				auto stackOffset = sizeof(uint64_t) + argument<2, uint64_t>(instruction, stackPtr);
				*reinterpret_cast<void**>(stackStart + stackOffset) = value;
				break;
			}
			case Instruction::Code::CallFunction: {
				auto& pureFunction = *argument<0, FunctionObject*>(instruction, stackPtr);
				auto& stackStart = argument<1, uint8_t*>(instruction, stackPtr);
				auto funcReturnPtr = &argument<2, uint8_t>(instruction, stackPtr);
				pureFunction.call(stack, stackStart, funcReturnPtr);
				stack.deallocate(pureFunction.stackSize());
				break;
			}
			case Instruction::Code::Jump: {
				index = argument<0, int64_t>(instruction, stackPtr);
				continue;
			}
			case Instruction::Code::JumpIfZero: {
				index += Instruction::CodeSizes[static_cast<uint8_t>(instruction.code())];
				if (argument<1, uint64_t>(instruction, stackPtr) == 0) {
					index = argument<0, int64_t>(instruction, stackPtr) ;
				}
				continue;
			}
			case Instruction::Code::JumpIfNotZero: {
				index += Instruction::getCodeSize(instruction.code());
				if (argument<1, uint64_t>(instruction, stackPtr) != 0) {
					index = argument<0, int64_t>(instruction, stackPtr);
				}
				continue;
			}
			case Instruction::Code::Return: {
				return;
			}
			case Instruction::Code::ReturnValue: {
				memcpy(
					returnPtr,
					&argument<1, uint8_t>(instruction, stackPtr),
					argument<0, uint64_t>(instruction, stackPtr));
				return;
			}
			}
			index += Instruction::getCodeSize(instruction.code());
		}
	}
}