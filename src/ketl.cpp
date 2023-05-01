/*🍲Ketl🍲*/
#include "ketl.h"

namespace Ketl {

	VirtualMachine::VirtualMachine(uint64_t stackBlockSize)
		: _memory(stackBlockSize) {
		// TYPE BASE

		/*
		// The Type
		auto [theTypePtr, theTypeRefs] = createObject<InterfaceTypeObject>("Type", 0);
		auto theTypeHolder = allocateGlobal<InterfaceTypeObject*>(theTypePtr->id(), *interfaceTypePtr);
		*theTypeHolder = theTypePtr;
		_types._userTypes.try_emplace(std::type_index(typeid(TypeObject)), theTypePtr);
		*/

		// PRIMITIVES

		auto& Void = _types.createPrimitiveType(_memory, "Void", 0, typeid(void));
		auto& Int64 = _types.createPrimitiveType<int64_t>(_memory, "Int64");
		auto& UInt64 = _types.createPrimitiveType<uint64_t>(_memory, "UInt64");
		auto& Float64 = _types.createPrimitiveType<double>(_memory, "Float64");

		_context.registerBuiltItOperator(OperatorCode::Plus, { {Int64, true, true}, {Int64, true, true} }, Instruction::Code::AddInt64, Int64);
		_context.registerBuiltItOperator(OperatorCode::Minus, { {Int64, true, true}, {Int64, true, true} }, Instruction::Code::MinusInt64, Int64);
		_context.registerBuiltItOperator(OperatorCode::Multiply, { {Int64, true, true}, {Int64, true, true} }, Instruction::Code::MultyInt64, Int64);
		_context.registerBuiltItOperator(OperatorCode::Divide, { {Int64, true, true}, {Int64, true, true} }, Instruction::Code::DivideInt64, Int64);

		_context.registerBuiltItOperator(OperatorCode::Equal, { {Int64, true, true}, {Int64, true, true} }, Instruction::Code::IsStructEqual, Int64);
		_context.registerBuiltItOperator(OperatorCode::NonEqual, { {Int64, true, true}, {Int64, true, true} }, Instruction::Code::IsStructNonEqual, Int64);

		_context.registerBuiltItOperator(OperatorCode::Assign, { {Int64, true, true}, {Int64, true, true} }, Instruction::Code::Assign, Int64);
	}
}