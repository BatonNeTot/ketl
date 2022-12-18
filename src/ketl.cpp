/*🍲Ketl🍲*/
#include "ketl.h"

namespace Ketl {

	Type::FunctionInfo BasicTypeBody::deduceConstructor(std::vector<std::unique_ptr<const Type>>& argumentTypes) const {
		Type::FunctionInfo outputInfo;

		for (uint64_t cstrIt = 0u; cstrIt < _cstrs.size(); ++cstrIt) {
			auto& cstr = _cstrs[cstrIt];
			if (argumentTypes.size() != cstr.argTypes.size()) {
				continue;
			}
			bool next = false;
			for (uint64_t typeIt = 0u; typeIt < cstr.argTypes.size(); ++typeIt) {
				if (!argumentTypes[typeIt]->convertableTo(*cstr.argTypes[typeIt])) {
					next = true;
					break;
				}
			}
			if (next) {
				continue;
			}

			outputInfo.isDynamic = false;
			outputInfo.function = &cstr.func;
			outputInfo.returnType = std::make_unique<BasicType>(this);

			outputInfo.argTypes.reserve(cstr.argTypes.size());
			for (uint64_t typeIt = 0u; typeIt < cstr.argTypes.size(); ++typeIt) {
				outputInfo.argTypes.emplace_back(Type::clone(cstr.argTypes[typeIt]));
			}

			return outputInfo;
		}
		return outputInfo;
	}

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
			outputStack<uint8_t*>(stackPtr, returnPtr) = stack.allocate(function.functions[_funcIndex].stackSize());
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
			auto& stackStart = second<uint8_t*>(stackPtr, returnPtr);
			auto& pureFunction = function.functions[_funcIndex];
			auto funcReturnPtr = &outputStack<uint8_t>(stackPtr, returnPtr);
			pureFunction.call(stack, stackStart, funcReturnPtr);
			stack.deallocate(pureFunction.stackSize());
			break;
		}
		}
		++index;
	}

	static void constructFloat64(StackAllocator&, uint8_t* stackPtr, uint8_t* returnPtr) {
		*reinterpret_cast<double*>(returnPtr) = **reinterpret_cast<double**>(stackPtr);
	}

	Context::Context(Allocator& allocator, uint64_t globalStackSize)
		: _globalStack(allocator, globalStackSize) {
		// creation of The Type
		BasicTypeBody theType("Type", sizeof(BasicTypeBody));
		auto theTypePtr = reinterpret_cast<BasicTypeBody*>(allocateOnGlobalStack(BasicType(&theType)));
		new(theTypePtr) BasicTypeBody(std::move(theType));
		_globals.try_emplace("Type", theTypePtr, std::make_unique<BasicType>(theTypePtr, false, false, true));

		declareType<void>("Void");
		declareType<int64_t>("Int64");
		declareType<uint64_t>("UInt64");
		//declareType<double>("Float64");

		{
			auto typeOfType = std::make_unique<TypeOfType>(nullptr);
			auto typePtr = reinterpret_cast<BasicTypeBody*>(allocateOnGlobalStack(*typeOfType));
			new(typePtr) BasicTypeBody("Float64", sizeof(double));
			_globals.try_emplace("Float64", typePtr, std::move(typeOfType));


			auto& constructor = typePtr->_cstrs.emplace_back();
			auto baseType = std::make_unique<BasicType>(typePtr);
			baseType->isConst = true;
			baseType->isRef = true;
			constructor.argTypes.emplace_back(std::move(baseType));
			constructor.func = FunctionImpl(allocator, sizeof(void*), &constructFloat64);
		}
	}

	Environment::Environment()
		: _alloc(), _context(_alloc, 4096) {
	}
}