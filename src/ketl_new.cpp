/*🍲Ketl🍲*/
#include "ketl_new.h"

Environment::Environment() {
	registerType<void>("Void");
	registerType<int64_t>("Int");
	registerType<uint64_t>("UInt");
	registerType<double>("Float");
	
	registerInstruction("operator +", InstructionCode::AddInt, getType("Int"), getType("Int"), getType("Int"));
	registerInstruction("operator -", InstructionCode::MinusInt, getType("Int"), getType("Int"), getType("Int"));
	registerInstruction("operator *", InstructionCode::MultyInt, getType("Int"), getType("Int"), getType("Int"));
	registerInstruction("operator /", InstructionCode::DivideInt, getType("Int"), getType("Int"), getType("Int"));
	registerInstruction("operator =", InstructionCode::DefineInt, getType("Int"), getType("Int"), getType("Int"));
}