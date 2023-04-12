/*🍲Ketl🍲*/
#ifndef function_h
#define function_h

#include "instructions.h"
#include "memory.h"

namespace Ketl {

	class FunctionObject {
	public:

		FunctionObject() {}
		FunctionObject(Allocator& alloc, bool isPure, uint64_t stackSize, uint32_t instructionsCount)
			: _alloc(&alloc)
			, _stackSize(stackSize + sizeof(uint64_t))
			, _instructionsCount(instructionsCount)
			, _isPure(isPure)
			, _instructions(_alloc->allocate<Instruction>(instructionsCount)) {}
		FunctionObject(const FunctionObject& function) = delete;
		FunctionObject(FunctionObject&& function) noexcept
			: _alloc(function._alloc)
			, _stackSize(function._stackSize)
			, _instructionsCount(function._instructionsCount)
			, _isPure(function._isPure)
			, _instructions(function._instructions) {
			function._instructions = nullptr;
		}
		~FunctionObject() {
			_alloc->deallocate(_instructions);
		}

		FunctionObject& operator =(FunctionObject&& other) noexcept {
			_alloc = other._alloc;
			_stackSize = other._stackSize;
			_instructionsCount = other._instructionsCount;
			_instructions = other._instructions;
			_isPure = other._isPure;

			other._instructions = nullptr;

			return *this;
		}

		void call(StackAllocator& stack, uint8_t* stackPtr, uint8_t* returnPtr) const;

		uint64_t stackSize() const {
			return _stackSize;
		}

		Allocator& alloc() {
			return *_alloc;
		}

		explicit operator bool() const {
			return _alloc != nullptr;
		}

	public:

		Allocator* _alloc = nullptr;
		uint64_t _stackSize = 0;

		uint32_t _instructionsCount = 0;
		bool _isPure;
		Instruction* _instructions = nullptr;
	};
}

#endif /*function_h*/