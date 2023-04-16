/*🍲Ketl🍲*/
#ifndef function_h
#define function_h

#include "instructions.h"
#include "memory.h"

namespace Ketl {

	class FunctionObject {
	public:

		FunctionObject(bool isPure, uint64_t stackSize, Instruction* instructions, uint32_t instructionsCount)
			: _stackSize(stackSize + sizeof(uint64_t))
			, _instructionsCount(instructionsCount)
			, _isPure(isPure)
			, _instructions(instructions) {}
		FunctionObject(const FunctionObject& function) = delete;
		FunctionObject(FunctionObject&& function) = delete;
		~FunctionObject() = default;

		void call(StackAllocator<Allocator>& stack, uint8_t* stackPtr, uint8_t* returnPtr) const;

		uint64_t stackSize() const {
			return _stackSize;
		}

		explicit operator bool() const {
			return _instructions != nullptr;
		}

	public:

		uint64_t _stackSize = 0;

		uint32_t _instructionsCount = 0;
		bool _isPure = true;
		Instruction* _instructions = nullptr;
	};
}

#endif /*function_h*/