/*🍲Ketl🍲*/
#include "context.h"

namespace Ketl {

	void* TypedPtr::as(std::type_index typeIndex, Context& context) const {
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
			return *reinterpret_cast<void**>(_ptr);
		}

		return _ptr;
	}

	std::vector<TypedPtr> Context::_emptyVars;

	void Context::declarePrimitiveType(const std::string& id, uint64_t size, std::type_index typeIndex) {
		auto* classTypePtr = getVariable("ClassType").as<TypeObject>();

		// actual type
		auto [primitiveTypePtr, refHolder] = createObject<PrimitiveTypeObject>(id, size);
		refHolder->registerAbsLink(classTypePtr);
		// create holder for class
		auto primitiveTypeHolder = allocateGlobal<PrimitiveTypeObject*>(id, *classTypePtr);
		*primitiveTypeHolder = primitiveTypePtr;
		_userTypes.try_emplace(typeIndex, primitiveTypePtr);
	}

	Context::Context(Allocator& allocator, uint64_t globalStackSize)
		: _alloc(allocator), _globalStack(allocator, globalStackSize), _gc(_alloc) {
		// TYPE BASE

		// class Type
		ClassTypeObject** classTypeHolder;
		auto [classTypePtr, classTypeRefs] = createObject<ClassTypeObject>("ClassType", sizeof(ClassTypeObject));
		classTypeHolder = allocateGlobal<ClassTypeObject*>(classTypePtr->id(), *classTypePtr);
		*classTypeHolder = classTypePtr;

		// interface Type
		auto [interfaceTypePtr, interfaceTypeRefs] = createObject<ClassTypeObject>("InterfaceType", sizeof(InterfaceTypeObject));
		interfaceTypeRefs->registerAbsLink(classTypePtr);
		auto interfaceTypeHolder = allocateGlobal<ClassTypeObject*>(interfaceTypePtr->id(), *classTypePtr);
		*interfaceTypeHolder = interfaceTypePtr;

		// The Type
		auto [theTypePtr, theTypeRefs] = createObject<InterfaceTypeObject>("Type"/*, 0*/);
		theTypeRefs->registerAbsLink(interfaceTypePtr);
		auto theTypeHolder = allocateGlobal<InterfaceTypeObject*>(theTypePtr->id(), *interfaceTypePtr);
		*theTypeHolder = theTypePtr;
		_userTypes.try_emplace(std::type_index(typeid(TypeObject)), theTypePtr);

		classTypePtr->_interfaces.emplace_back(theTypePtr);
		classTypeRefs->registerAbsLink(theTypePtr);
		interfaceTypePtr->_interfaces.emplace_back(theTypePtr);
		interfaceTypeRefs->registerAbsLink(theTypePtr);

		// struct Type
		auto [structTypePtr, structTypeRefs] = createObject<ClassTypeObject>("StructType", sizeof(StructTypeObject), std::vector<InterfaceTypeObject*>{theTypePtr});
		structTypeRefs->registerAbsLink(theTypePtr);
		auto structTypeHolder = allocateGlobal<ClassTypeObject*>(structTypePtr->id(), *classTypePtr);
		*structTypeHolder = structTypePtr;

		// PRIMITIVES

		// primitive type
		auto [primitiveTypePtr, primitiveTypeRefs] = createObject<ClassTypeObject>("PrimitiveType", sizeof(PrimitiveTypeObject), std::vector<InterfaceTypeObject*>{theTypePtr});
		primitiveTypeRefs->registerAbsLink(theTypePtr);
		auto primitiveTypeHolder = allocateGlobal<ClassTypeObject*>(primitiveTypePtr->id(), *classTypePtr);
		*primitiveTypeHolder = primitiveTypePtr;

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
	}
}