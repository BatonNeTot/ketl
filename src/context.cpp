/*🍲Ketl🍲*/
#include "context.h"

namespace Ketl {

	Variable Context::_emptyVar;

	TypeObject* Context::declareType(const std::string& id, uint64_t sizeOf) {
		/*
		auto theTypeVar = getVariable("Type");
		auto theTypePtr = theTypeVar.as<BasicTypeBody>();
		auto typePtr = reinterpret_cast<BasicTypeBody*>(allocateGlobal(BasicType(theTypePtr)));
		new(typePtr) BasicTypeBody(id, sizeOf);
		_globals.try_emplace(id, typePtr, std::make_unique<BasicType>(theTypePtr, false, false, true));
		return typePtr;
		*/
		return nullptr;
	}

	static void constructFloat64(StackAllocator&, uint8_t* stackPtr, uint8_t* returnPtr) {
		auto value = **reinterpret_cast<double**>(stackPtr);
		*reinterpret_cast<double*>(returnPtr) = value;
	}

	Context::Context(Allocator& allocator, uint64_t globalStackSize)
		: _alloc(allocator), _globalStack(allocator, globalStackSize) {
		// class Type
		ClassTypeObject* classTypePtr;
		{
			ClassTypeObject classType("ClassType", sizeof(ClassTypeObject));
			classTypePtr = reinterpret_cast<ClassTypeObject*>(allocateGlobal(classType));
			new(classTypePtr) ClassTypeObject(std::move(classType));
			_globals.try_emplace(classTypePtr->id(), classTypePtr, *classTypePtr);
		}

		// interface Type
		auto interfaceTypePtr = reinterpret_cast<ClassTypeObject*>(allocateGlobal(*classTypePtr));
		new(interfaceTypePtr) ClassTypeObject("InterfaceType", sizeof(InterfaceTypeObject));
		_globals.try_emplace(interfaceTypePtr->id(), interfaceTypePtr, *classTypePtr);

		// The Type
		auto theTypePtr = reinterpret_cast<InterfaceTypeObject*>(allocateGlobal(*interfaceTypePtr));
		new(theTypePtr) InterfaceTypeObject("Type"/*, 0*/);
		_globals.try_emplace(theTypePtr->id(), theTypePtr, *interfaceTypePtr);

		// privitive type
		auto primitiveTypePtr = reinterpret_cast<ClassTypeObject*>(allocateGlobal(*classTypePtr));
		new(primitiveTypePtr) ClassTypeObject("PrivitiveType", sizeof(PrimitiveTypeObject));
		_globals.try_emplace(primitiveTypePtr->id(), primitiveTypePtr, *classTypePtr);

		// Int64 type
		auto longTypePtr = reinterpret_cast<PrimitiveTypeObject*>(allocateGlobal(*classTypePtr));
		new(longTypePtr) PrimitiveTypeObject("Int64", sizeof(int64_t));
		_globals.try_emplace(longTypePtr->id(), longTypePtr, *classTypePtr);

		//declareType<void>("Void");
		//declareType<int64_t>("Int64");
		//declareType<uint64_t>("UInt64");
		//declareType<double>("Float64");

		/*
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
		*/
	}
}