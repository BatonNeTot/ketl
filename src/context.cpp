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

	std::vector<Variable> Context::_emptyVars;

	void Context::declarePrimitiveType(const std::string& id, uint64_t size, std::type_index typeIndex) {
		auto classTypeHolder = reinterpret_cast<TypeObject**>(_globals.find("ClassType")->second[0]._data);
		auto* classTypePtr = *classTypeHolder;

		// actual type
		auto [primitiveTypePtr, refHolder] = createObject<PrimitiveTypeObject>(id, size);
		refHolder->registerAbsLink(classTypePtr);
		// create holder for class
		auto primitiveTypeHolder = reinterpret_cast<PrimitiveTypeObject**>(allocateOnHeap(sizeof(void*)));
		*primitiveTypeHolder = primitiveTypePtr;
		// register root
		_gc.registerRefRoot(primitiveTypeHolder);
		// put holder in global
		_globals[id].emplace_back(primitiveTypeHolder, *classTypePtr);
		_userTypes.try_emplace(typeIndex, primitiveTypePtr, *classTypePtr);
	}

	Context::Context(Allocator& allocator, uint64_t globalStackSize)
		: _alloc(allocator), _globalStack(allocator, globalStackSize), _gc(_alloc) {
		// TYPE BASE

		// class Type
		ClassTypeObject** classTypeHolder;
		auto [classTypePtr, classTypeRefs] = createObject<ClassTypeObject>("ClassType", sizeof(ClassTypeObject));
		{
			classTypeHolder = reinterpret_cast<ClassTypeObject**>(allocateOnHeap(sizeof(void*)));
			*classTypeHolder = classTypePtr;
			_gc.registerRefRoot(classTypeHolder);
			_globals[classTypePtr->id()].emplace_back(classTypeHolder, *classTypePtr);
		}

		// interface Type
		auto [interfaceTypePtr, interfaceTypeRefs] = createObject<ClassTypeObject>("InterfaceType", sizeof(InterfaceTypeObject));
		interfaceTypeRefs->registerAbsLink(classTypePtr);
		auto interfaceTypeHolder = reinterpret_cast<ClassTypeObject**>(allocateOnHeap(sizeof(void*)));
		*interfaceTypeHolder = interfaceTypePtr;
		_gc.registerRefRoot(interfaceTypeHolder);
		_globals[interfaceTypePtr->id()].emplace_back(interfaceTypeHolder, *classTypePtr);

		// The Type
		auto [theTypePtr, theTypeRefs] = createObject<InterfaceTypeObject>("Type"/*, 0*/);
		theTypeRefs->registerAbsLink(interfaceTypePtr);
		auto theTypeHolder = reinterpret_cast<InterfaceTypeObject**>(allocateOnHeap(sizeof(void*)));
		*theTypeHolder = theTypePtr;
		_gc.registerRefRoot(theTypeHolder);
		_globals[theTypePtr->id()].emplace_back(theTypeHolder, *interfaceTypePtr);
		_userTypes.try_emplace(std::type_index(typeid(TypeObject)), theTypePtr, *interfaceTypePtr);

		classTypePtr->_interfaces.emplace_back(theTypePtr);
		classTypeRefs->registerAbsLink(theTypePtr);
		interfaceTypePtr->_interfaces.emplace_back(theTypePtr);
		interfaceTypeRefs->registerAbsLink(theTypePtr);

		// PRIMITIVES

		// primitive type
		auto [primitiveTypePtr, primitiveTypeRefs] = createObject<ClassTypeObject>("PrimitiveType", sizeof(PrimitiveTypeObject), std::vector<InterfaceTypeObject*>{theTypePtr});
		primitiveTypeRefs->registerAbsLink(interfaceTypePtr);
		primitiveTypeRefs->registerAbsLink(theTypePtr);
		//_rootObjects.emplace(primitiveTypePtr);
		auto primitiveTypeHolder = reinterpret_cast<ClassTypeObject**>(allocateOnHeap(sizeof(void*)));
		*primitiveTypeHolder = primitiveTypePtr;
		_gc.registerRefRoot(primitiveTypeHolder);
		_globals[primitiveTypePtr->id()].emplace_back(primitiveTypeHolder, *classTypePtr);

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