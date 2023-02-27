/*🍲Ketl🍲*/
#include "context.h"

namespace Ketl {

	void* Variable::as(std::type_index typeIndex, Context& context) const {
		auto typeVarIt = context._userTypes.find(typeIndex);
		if (typeVarIt == context._userTypes.end()) {
			return nullptr;
		}

		return _data;
	}

	Variable Context::_emptyVar;

	static void constructFloat64(StackAllocator&, uint8_t* stackPtr, uint8_t* returnPtr) {
		auto value = **reinterpret_cast<double**>(stackPtr);
		*reinterpret_cast<double*>(returnPtr) = value;
	}

	void Context::declarePrimitiveType(const std::string& id, uint64_t size, std::type_index typeIndex) {
		auto classTypePtr = getVariable("ClassType").as<ClassTypeObject>();

		auto primitiveTypePtr = reinterpret_cast<PrimitiveTypeObject*>(allocateGlobal(*classTypePtr));
		new(primitiveTypePtr) PrimitiveTypeObject(id, size);
		_globals.try_emplace(id, primitiveTypePtr, *classTypePtr);
		_userTypes.try_emplace(typeIndex, primitiveTypePtr, *classTypePtr);
	}

	Context::Context(Allocator& allocator, uint64_t globalStackSize)
		: _alloc(allocator), _globalStack(allocator, globalStackSize) {
		// TYPE BASE

		// class Type
		ClassTypeObject* classTypePtr;
		{
			ClassTypeObject classType("ClassType", sizeof(ClassTypeObject));
			classTypePtr = reinterpret_cast<ClassTypeObject*>(allocateGlobal(classType));
			new(classTypePtr) ClassTypeObject(std::move(classType));
			_globals.try_emplace(classTypePtr->id(), classTypePtr, *classTypePtr);
			_userTypes.try_emplace(std::type_index(typeid(ClassTypeObject)), classTypePtr, *classTypePtr);
		}

		// interface Type
		auto interfaceTypePtr = reinterpret_cast<ClassTypeObject*>(allocateGlobal(*classTypePtr));
		new(interfaceTypePtr) ClassTypeObject("InterfaceType", sizeof(InterfaceTypeObject));
		_globals.try_emplace(interfaceTypePtr->id(), interfaceTypePtr, *classTypePtr);
		_userTypes.try_emplace(std::type_index(typeid(InterfaceTypeObject)), interfaceTypePtr, *classTypePtr);

		// The Type
		auto theTypePtr = reinterpret_cast<InterfaceTypeObject*>(allocateGlobal(*interfaceTypePtr));
		new(theTypePtr) InterfaceTypeObject("Type"/*, 0*/);
		_globals.try_emplace(theTypePtr->id(), theTypePtr, *interfaceTypePtr);
		_userTypes.try_emplace(std::type_index(typeid(TypeObject)), theTypePtr, *interfaceTypePtr);

		classTypePtr->_interfaces.emplace_back(theTypePtr);
		interfaceTypePtr->_interfaces.emplace_back(theTypePtr);

		// PRIMITIVES

		// primitive type
		auto primitiveTypePtr = reinterpret_cast<ClassTypeObject*>(allocateGlobal(*classTypePtr));
		new(primitiveTypePtr) ClassTypeObject("PrimitiveType", sizeof(PrimitiveTypeObject), std::vector<InterfaceTypeObject*>{theTypePtr});
		_globals.try_emplace(primitiveTypePtr->id(), primitiveTypePtr, *classTypePtr);
		_userTypes.try_emplace(std::type_index(typeid(PrimitiveTypeObject)), primitiveTypePtr, *classTypePtr);

		declarePrimitiveType("Void", 0, typeid(void));
		declarePrimitiveType<int64_t>("Int64");

		registerPrimaryOperator(OperatorCode::Plus,		"Int64,Int64", Instruction::Code::AddInt64,		"Int64");
		registerPrimaryOperator(OperatorCode::Minus,	"Int64,Int64", Instruction::Code::MinusInt64,	"Int64");
		registerPrimaryOperator(OperatorCode::Multiply,	"Int64,Int64", Instruction::Code::MultyInt64,	"Int64");
		registerPrimaryOperator(OperatorCode::Divide,	"Int64,Int64", Instruction::Code::DivideInt64,	"Int64");

		// TODO temporary
		registerPrimaryOperator(OperatorCode::Assign,	"Int64,Int64", Instruction::Code::Assign, "Int64");

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