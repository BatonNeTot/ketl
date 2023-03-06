/*🍲Ketl🍲*/
#include "context.h"

namespace Ketl {

	void* Variable::as(std::type_index typeIndex, Context& context) const {
		// TODO
		/*
		auto typeVarIt = context._userTypes.find(typeIndex);
		if (typeVarIt == context._userTypes.end()) {
			return nullptr;
		}
		*/

		// TODO BIG
		// do correct convertation, etc. PrimitiveTypeObject* -> TypeObject*

		if (_type && _type->isLight()) {
			return *reinterpret_cast<void**>(_data);
		}

		return _data;
	}

	Variable Context::_emptyVar;

	void Context::declarePrimitiveType(const std::string& id, uint64_t size, std::type_index typeIndex) {
		auto classTypeHolder = reinterpret_cast<TypeObject**>(_globals.find("ClassType")->second._data);
		auto* classTypePtr = *classTypeHolder;

		// actual type
		auto primitiveTypePtr = createObject<PrimitiveTypeObject>(id, size);
		// register links
		primitiveTypePtr->registerLink(classTypeHolder);
		// register root
		_rootObjects.emplace(primitiveTypePtr);
		// create holder for class
		auto primitiveTypeHolder = reinterpret_cast<PrimitiveTypeObject**>(allocateOnHeap(sizeof(void*)));
		*primitiveTypeHolder = primitiveTypePtr;
		// put holder in global
		_globals.try_emplace(id, primitiveTypeHolder, *classTypePtr);
		_userTypes.try_emplace(typeIndex, primitiveTypePtr, *classTypePtr);
	}

	Context::Context(Allocator& allocator, uint64_t globalStackSize)
		: _alloc(allocator), _globalStack(allocator, globalStackSize) {
		// TYPE BASE

		// class Type
		ClassTypeObject** classTypeHolder;
		auto classTypePtr = createObject<ClassTypeObject>("ClassType", sizeof(ClassTypeObject));
		{
			_rootObjects.emplace(classTypePtr);
			classTypeHolder = reinterpret_cast<ClassTypeObject**>(allocateOnHeap(sizeof(void*)));
			*classTypeHolder = classTypePtr;
			_globals.try_emplace(classTypePtr->id(), classTypeHolder, *classTypePtr);
		}

		// interface Type
		auto interfaceTypePtr = createObject<ClassTypeObject>("InterfaceType", sizeof(InterfaceTypeObject));
		interfaceTypePtr->registerLink(classTypeHolder);
		_rootObjects.emplace(interfaceTypePtr);
		auto interfaceTypeHolder = reinterpret_cast<ClassTypeObject**>(allocateOnHeap(sizeof(void*)));
		*interfaceTypeHolder = interfaceTypePtr;
		_globals.try_emplace(interfaceTypePtr->id(), interfaceTypeHolder, *classTypePtr);

		// The Type
		auto theTypePtr = createObject<InterfaceTypeObject>("Type"/*, 0*/);
		theTypePtr->registerLink(interfaceTypeHolder);
		_rootObjects.emplace(theTypePtr);
		auto theTypeHolder = reinterpret_cast<InterfaceTypeObject**>(allocateOnHeap(sizeof(void*)));
		*theTypeHolder = theTypePtr;
		_globals.try_emplace(theTypePtr->id(), theTypeHolder, *interfaceTypePtr);
		_userTypes.try_emplace(std::type_index(typeid(TypeObject)), theTypePtr, *interfaceTypePtr);

		classTypePtr->_interfaces.emplace_back(theTypePtr);
		classTypePtr->registerLink(theTypeHolder);
		interfaceTypePtr->_interfaces.emplace_back(theTypePtr);
		interfaceTypePtr->registerLink(theTypeHolder);

		// PRIMITIVES

		// primitive type
		auto primitiveTypePtr = createObject<ClassTypeObject>("PrimitiveType", sizeof(PrimitiveTypeObject), std::vector<InterfaceTypeObject*>{theTypePtr});
		primitiveTypePtr->registerLink(interfaceTypeHolder);
		primitiveTypePtr->registerLink(theTypeHolder);
		_rootObjects.emplace(primitiveTypePtr);
		auto primitiveTypeHolder = reinterpret_cast<ClassTypeObject**>(allocateOnHeap(sizeof(void*)));
		*primitiveTypeHolder = primitiveTypePtr;
		_globals.try_emplace(primitiveTypePtr->id(), primitiveTypeHolder, *classTypePtr);

		declarePrimitiveType("Void", 0, typeid(void));
		declarePrimitiveType<int64_t>("Int64");
		declarePrimitiveType<uint64_t>("UInt64");
		declarePrimitiveType<double>("Float64");

		registerPrimaryOperator(OperatorCode::Plus,		"Int64,Int64", Instruction::Code::AddInt64,			"Int64");
		registerPrimaryOperator(OperatorCode::Minus,	"Int64,Int64", Instruction::Code::MinusInt64,		"Int64");
		registerPrimaryOperator(OperatorCode::Multiply,	"Int64,Int64", Instruction::Code::MultyInt64,		"Int64");
		registerPrimaryOperator(OperatorCode::Divide,	"Int64,Int64", Instruction::Code::DivideInt64,		"Int64");

		// TODO temporary
		registerPrimaryOperator(OperatorCode::Assign,	"Int64,Int64", Instruction::Code::AssignPrimitive,	"Int64");

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