/*🐟Ketl🐟*/
#include "eel_new.h"

Environment::Environment() {
	registerType<void>("Void");
	registerType<int64_t>("Int");
	registerType<uint64_t>("UInt");
	registerType<double>("Float");
	
	registerInstruction("operator +", InstructionCode::AddInt, getType("Int"), getType("Int"), getType("Int"));
	registerInstruction("operator ==", InstructionCode::DefineInt, getType("Int"), getType("Int"), getType("Int"));
}