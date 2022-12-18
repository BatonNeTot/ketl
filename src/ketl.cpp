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
			output<double>(stackPtr, returnPtr) = first<double>(stackPtr, returnPtr) + second<double>(stackPtr, returnPtr);
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
		case Code::AllocateStack: {
			auto& function = first<Function>(stackPtr, returnPtr);
			outputStack<uint8_t*>(stackPtr, returnPtr) = stack.allocate(function.functions[_funcIndex].stackSize());
			break;
		}
		case Code::DefineArgument: {
			*reinterpret_cast<Argument*>(outputStack<uint8_t*>(stackPtr, returnPtr) + first<uint64_t>(stackPtr, returnPtr)) 
				= second<Argument>(stackPtr, returnPtr);
			break;
		}
		case Code::Call: {
			auto& function = first<Function>(stackPtr, returnPtr);
			auto& stackStart = second<uint8_t*>(stackPtr, returnPtr);
			auto& pureFunction = function.functions[_funcIndex];
			pureFunction.call(stack, &outputStack<uint8_t>(stackPtr, returnPtr), stackStart);
			stack.deallocate(pureFunction.stackSize());
			break;
		}
		}
		++index;
	}

	Context::Context(Allocator& allocator, uint64_t globalStackSize)
		: _globalStack(allocator, globalStackSize) {
		// creation of The Type
		BasicTypeBody theType("Type", sizeof(BasicTypeBody));
		auto theTypePtr = reinterpret_cast<BasicTypeBody*>(allocateOnGlobalStack(BasicType(&theType)));
		new(theTypePtr) BasicTypeBody(std::move(theType));
		_globals.try_emplace("Type", theTypePtr, std::make_unique<BasicType>(theTypePtr));

		declareType<void>("Void");
		declareType<int64_t>("Int64");
		declareType<uint64_t>("UInt64");
		declareType<double>("Float64");

		/*
		{
			auto typeOfType = std::make_unique<TypeOfType>(nullptr);
			auto typePtr = reinterpret_cast<BasicTypeBody*>(allocateOnGlobalStack(*typeOfType));
			new(typePtr) BasicTypeBody("Float64", sizeof(double));
			_globals.try_emplace("Float64", typePtr, std::move(typeOfType));
		}
		*/
	}

	Environment::Environment()
		: _alloc(), _context(_alloc, 4096) {
	}
}