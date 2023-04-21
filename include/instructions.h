/*🍲Ketl🍲*/
#ifndef instructions_h
#define instructions_h

#include <cinttypes>

namespace Ketl {

	struct Argument {
		Argument() {}

		enum class Type : uint8_t {
			None,
			Global,
			Stack,
			Literal,
			FunctionParameter,
		};

		union {
			void* globalPtr = nullptr;
			uint64_t stack;

			int64_t integer;
			uint64_t uinteger;
			double floating;
			void* pointer;
		};
	};

	class Instruction {
	public:

		enum class Code : uint8_t {
			None,
			AddInt64,
			MinusInt64,
			MultyInt64,
			DivideInt64,
			AddFloat64,
			MinusFloat64,
			MultyFloat64,
			DivideFloat64,
			Assign,
			IsStructEqual,
			IsStructNonEqual,
			AllocateFunctionStack,
			DefineFuncParameter,
			CallFunction,
			Jump,
			JumpIfZero,
			JumpIfNotZero,
			Return,
			ReturnValue,
		};

		constexpr static uint8_t CodeSizes[] = {
			1,	//None,
			4,	//AddInt64,
			4,	//MinusInt64,
			4,	//MultyInt64,
			4,	//DivideInt64,
			4,	//AddFloat64,
			4,	//MinusFloat64,
			4,	//MultyFloat64,
			4,	//DivideFloat64,
			4,	//Assign,
			5,	//IsStructEqual,
			5,	//IsStructNonEqual,
			3,	//AllocateFunctionStack,
			4,	//DefineFuncParameter,
			4,	//CallFunction,
			2,	//Jump,
			3,	//JumpIfZero,
			3,	//JumpIfNotZero,
			1,	//Return,
			3,	//ReturnValue,
		};

		static constexpr inline uint8_t getCodeSize(Code code) {
			return Instruction::CodeSizes[static_cast<uint8_t>(code)];
		}

		Instruction() {}

		inline Instruction::Code& code() { return _code; }
		template <unsigned N>
		inline Argument::Type& argumentType() { return _argumentTypes[N]; }
		inline Argument::Type& argumentType(uint8_t index) { return _argumentTypes[index]; }
		inline Argument& argument(uint8_t index) { return (this + index)->_argument; }

	private:
		union {
			Instruction::Code _code = Instruction::Code::None;
			Argument::Type _argumentTypes[8]; // zero one reserved
			Argument _argument;
		};
	};
}

#endif /*instructions_h*/