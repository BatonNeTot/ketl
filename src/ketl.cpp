/*🍲Ketl🍲*/
#include "ketl.h"

namespace Ketl {

	void Instruction::call(uint64_t& index, StackAllocator& stack, uint8_t* stackPtr, uint8_t* returnPtr) {
		switch (_code) {
		case Code::AddInt64: {
			output<int64_t>(stackPtr, returnPtr) = first<int64_t>(stackPtr, returnPtr) + second<int64_t>(stackPtr, returnPtr);
			break;
		}
		case Code::MinusInt64: {
			output<int64_t>(stackPtr, returnPtr) = first<int64_t>(stackPtr, returnPtr) - second<int64_t>(stackPtr, returnPtr);
			break;
		}
		case Code::MultyInt64: {
			output<int64_t>(stackPtr, returnPtr) = first<int64_t>(stackPtr, returnPtr) * second<int64_t>(stackPtr, returnPtr);
			break;
		}
		case Code::DivideInt64: {
			output<int64_t>(stackPtr, returnPtr) = first<int64_t>(stackPtr, returnPtr) / second<int64_t>(stackPtr, returnPtr);
			break;
		}
		case Code::AddFloat64: {
			auto f = first<double>(stackPtr, returnPtr);
			auto s = second<double>(stackPtr, returnPtr);
			output<double>(stackPtr, returnPtr) = f + s;
			break;
		}
		case Code::MinusFloat64: {
			output<double>(stackPtr, returnPtr) = first<double>(stackPtr, returnPtr) - second<double>(stackPtr, returnPtr);
			break;
		}
		case Code::MultyFloat64: {
			output<double>(stackPtr, returnPtr) = first<double>(stackPtr, returnPtr) * second<double>(stackPtr, returnPtr);
			break;
		}
		case Code::DivideFloat64: {
			output<double>(stackPtr, returnPtr) = first<double>(stackPtr, returnPtr) / second<double>(stackPtr, returnPtr);
			break;
		}
		case Code::Define: {
			first<Argument>(stackPtr, returnPtr) = second<Argument>(stackPtr, returnPtr);
			break;
		}
		case Code::Assign: {
			output<Argument>(stackPtr, returnPtr) = first<Argument>(stackPtr, returnPtr) = second<Argument>(stackPtr, returnPtr);
			break;
		}
		case Code::Reference: {
			output<uint8_t*>(stackPtr, returnPtr) = &first<uint8_t>(stackPtr, returnPtr);
			break;
		}
		case Code::AllocateFunctionStack: {
			auto& function = *first<const FunctionImpl*>(stackPtr, returnPtr);
			output<uint8_t*>(stackPtr, returnPtr) = stack.allocate(function.stackSize());
			break;
		}
		case Code::AllocateDynamicFunctionStack: {
			auto& function = first<FunctionContainer>(stackPtr, returnPtr);
			auto funcIndex = second<uint64_t>(stackPtr, returnPtr);
			output<uint8_t*>(stackPtr, returnPtr) = stack.allocate(function.functions[funcIndex].stackSize());
			break;
		}
		case Code::CallFunction: {
			auto& pureFunction = *first<const FunctionImpl*>(stackPtr, returnPtr);
			auto& stackStart = output<uint8_t*>(stackPtr, returnPtr);
			auto funcReturnPtr = &output<uint8_t>(stackPtr, returnPtr);
			pureFunction.call(stack, stackStart, funcReturnPtr);
			stack.deallocate(pureFunction.stackSize());
			break;
		}
		case Code::CallDynamicFunction: {
			auto& function = first<FunctionContainer>(stackPtr, returnPtr);
			auto funcIndex = second<uint64_t>(stackPtr, returnPtr);
			auto& pureFunction = function.functions[funcIndex];
			auto& stackStart = output<uint8_t*>(stackPtr, returnPtr);
			auto funcReturnPtr = &output<uint8_t>(stackPtr, returnPtr);
			pureFunction.call(stack, stackStart, funcReturnPtr);
			stack.deallocate(pureFunction.stackSize());
			break;
		}
		}
		++index;
	}
}