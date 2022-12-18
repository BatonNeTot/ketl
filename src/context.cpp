/*🍲Ketl🍲*/
#include "context.h"

namespace Ketl {

	Variable Context::_emptyVar;

	BasicTypeBody* Context::declareType(const std::string& id, uint64_t sizeOf) {
		auto theTypeVar = getVariable("Type");
		auto theTypePtr = theTypeVar.as<BasicTypeBody>();
		auto typePtr = reinterpret_cast<BasicTypeBody*>(allocateGlobal(BasicType(theTypePtr)));
		new(typePtr) BasicTypeBody(id, sizeOf);
		_globals.try_emplace(id, typePtr, std::make_unique<BasicType>(theTypePtr, false, false, true));
		return typePtr;
	}

	static void constructFloat64(StackAllocator&, uint8_t* stackPtr, uint8_t* returnPtr) {
		auto value = **reinterpret_cast<double**>(stackPtr);
		*reinterpret_cast<double*>(returnPtr) = value;
	}

	Context::Context(Allocator& allocator, uint64_t globalStackSize)
		: _alloc(allocator), _globalStack(allocator, globalStackSize) {
		// creation of The Type
		BasicTypeBody theType("Type", sizeof(BasicTypeBody));
		auto theTypePtr = reinterpret_cast<BasicTypeBody*>(allocateGlobal(BasicType(&theType)));
		new(theTypePtr) BasicTypeBody(std::move(theType));
		_globals.try_emplace("Type", theTypePtr, std::make_unique<BasicType>(theTypePtr, false, false, true));

		declareType<void>("Void");
		declareType<int64_t>("Int64");
		declareType<uint64_t>("UInt64");
		//declareType<double>("Float64");

		{
			auto typeOfType = std::make_unique<TypeOfType>(nullptr);
			auto typePtr = reinterpret_cast<BasicTypeBody*>(allocateGlobal(*typeOfType));
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
}