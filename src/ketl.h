/*🍲Ketl🍲*/
#ifndef ketl_h
#define ketl_h

#include <cinttypes>
#include <string>
#include <unordered_map>
#include <memory>
#include <optional>
#include <typeindex>
#include <iostream>
#include <functional>

namespace Ketl {
	class Allocator {
	public:

		template <class T>
		T* allocate(size_t count = 1) {
			return reinterpret_cast<T*>(allocate(sizeof(T) * count));
		}

		uint8_t* allocate(size_t size) {
			return reinterpret_cast<uint8_t*>(::operator new(size));
		}

		void deallocate(void* ptr) {
			::operator delete(ptr);
		}
	};

	class StackAllocator {
	public:

		StackAllocator(Allocator& alloc, size_t blockSize)
			: _alloc(alloc), _blockSize(blockSize) {}
		StackAllocator(const StackAllocator& stack) = delete;
		StackAllocator(const StackAllocator&& stack) = delete;
		StackAllocator(StackAllocator&& stack) noexcept
			: _alloc(stack._alloc), _blockSize(stack._blockSize), _currentOffset(stack._currentOffset)
			, _lastBlock(stack._lastBlock), _currentBlock(stack._currentBlock) {
			stack._lastBlock = nullptr;
			stack._currentBlock = nullptr;
		}
		~StackAllocator() {
			auto it = _lastBlock;
			while (it) {
				auto dealloc = it;
				it = it->prev;
				_alloc.deallocate(dealloc->memory);
				_alloc.deallocate(dealloc);
			}
		}

		template <class T>
		T* allocate(size_t count = 1) {
			return reinterpret_cast<T*>(allocate(sizeof(T) * count));
		}

		uint8_t* allocate(size_t size) {
			if (size == 0) {
				return nullptr;
			}
			if (size > _blockSize) {
				std::cerr << "Not enough space in blockSize" << std::endl;
				return nullptr;
			}

			if (_lastBlock == nullptr) {
				_lastBlock = reinterpret_cast<Block*>(_alloc.allocate(sizeof(Block)));
				new(_lastBlock) Block(_alloc.allocate(_blockSize));
				_currentBlock = _lastBlock;
			}
			else if (_currentBlock->offset + size > _blockSize) {
				_currentBlock = _currentBlock->next;
				if (_currentBlock == nullptr) {
					auto newBlock = reinterpret_cast<Block*>(_alloc.allocate(sizeof(Block)));
					new(newBlock) Block(_alloc.allocate(_blockSize), _lastBlock);

					_lastBlock = newBlock;
					_currentBlock = _lastBlock;
				}
			}

			auto ptr = _currentBlock->memory + _currentBlock->offset;
			_currentBlock->offset += size;
			_currentOffset += size;
			return ptr;
		}

		void deallocate(ptrdiff_t offset) {
			if (offset && _currentBlock == nullptr) {
				std::cerr << "Tried deallocate more than allocated" << std::endl;
				return;
			}

			while (offset) {
				auto removedOffset = std::min(_currentBlock->offset, offset);
				_currentBlock->offset -= removedOffset;
				offset -= removedOffset;
				_currentOffset -= removedOffset;

				if (removedOffset == 0) {
					std::cerr << "Tried deallocate more than allocated" << std::endl;
					break;
				}

				if (_currentBlock->offset == 0 && _currentBlock->prev != nullptr) {
					_currentBlock = _currentBlock->prev;
				}
			}
		}

		uint64_t blockSize() const {
			return _blockSize;
		}

		uint64_t currentOffset() const {
			return _currentOffset;
		}

	private:
		Allocator& _alloc;
		size_t _blockSize;

		struct Block {
			Block(uint8_t* memory_)
				: memory(memory_) {}
			Block(uint8_t* memory_, Block* prev_)
				: memory(memory_), prev(prev_) {
				prev_->next = this;
			}

			Block* next = nullptr;
			Block* prev = nullptr;
			uint8_t* memory;
			ptrdiff_t offset = 0;
		};

		uint64_t _currentOffset = 0;
		Block* _lastBlock = nullptr;
		Block* _currentBlock = nullptr;
	};

	struct Argument {
		Argument() {}

		enum class Type : uint8_t {
			None,
			Global,
			DerefGlobal,
			//TODO this is something really stupid, i hope better solution'll come later  
			DerefDerefGlobal,
			Stack,
			DerefStack,
			//TODO same 
			DerefDerefStack,
			Literal,
			DerefLiteral,
			Return,
			DerefReturn,
		}; 
		
		static Type deref(const Type type) {
			switch (type) {
			case Type::Stack: {
				return Type::DerefStack;
			}
			case Type::DerefStack: {
				return Type::DerefDerefStack;
			}
			case Type::Global: {
				return Type::DerefGlobal;
			}
			case Type::DerefGlobal: {
				return Type::DerefDerefGlobal;
			}
			case Type::Literal: {
				return Type::DerefLiteral;
			}
			case Type::Return: {
				return Type::DerefReturn;
			}
			}

			return Type::None;
		}

		union {
			void* globalPtr = nullptr;
			uint64_t stack;

			int64_t integer;
			uint64_t uinteger;
			double floating;
			void* pointer;
			const void* cPointer;
		};
	};

	class Instruction {
	public:

		enum class Code : uint8_t {
			AddInt64,
			MinusInt64,
			MultyInt64,
			DivideInt64,
			AddFloat64,
			MinusFloat64,
			MultyFloat64,
			DivideFloat64,
			Define,
			Assign,
			Reference,
			AllocateFunctionStack,
			CallFunction,
		};

		Instruction() {}
		Instruction(
			Code code,
			Argument::Type outputType,
			Argument::Type firstType,
			Argument::Type secondType,

			Argument output,
			Argument first,
			Argument second)
			:
			_code(code),
			_outputType(outputType),
			_firstType(firstType),
			_secondType(secondType),

			_output(output),
			_first(first),
			_second(second) {}
		Instruction(
			Code code,
			uint16_t outputOffset,
			Argument::Type outputType,
			Argument::Type firstType,
			Argument::Type secondType,

			Argument output,
			Argument first,
			Argument second)
			:
			_code(code),
			_outputType(outputType),
			_firstType(firstType),
			_secondType(secondType),

			_outputOffset(outputOffset),

			_output(output),
			_first(first),
			_second(second) {}

		void call(uint64_t& index, StackAllocator& stack, uint8_t* stackPtr, uint8_t* returnPtr);

	public: // TODO

		template <class T>
		inline T& output(uint8_t* stackPtr, uint8_t* returnPtr) {
			return *reinterpret_cast<T*>(getArgument(stackPtr, returnPtr, _outputType, _output) + _outputOffset);
		}

		template <class T>
		inline T& first(uint8_t* stackPtr, uint8_t* returnPtr) {
			return *reinterpret_cast<T*>(getArgument(stackPtr, returnPtr, _firstType, _first));
		}

		template <class T>
		inline T& second(uint8_t* stackPtr, uint8_t* returnPtr) {
			return *reinterpret_cast<T*>(getArgument(stackPtr, returnPtr, _secondType, _second));
		}

		static inline uint8_t* getArgument(uint8_t* stackPtr, uint8_t* returnPtr, Argument::Type type, Argument& value) {
			switch (type) {
			case Argument::Type::Global: {
				return reinterpret_cast<uint8_t*>(value.globalPtr);
			}
			case Argument::Type::DerefGlobal: {
				return *reinterpret_cast<uint8_t**>(value.globalPtr);
			}
			case Argument::Type::DerefDerefGlobal: {
				return **reinterpret_cast<uint8_t***>(value.globalPtr);
			}
			case Argument::Type::Stack: {
				return stackPtr + value.stack;
			}
			case Argument::Type::DerefStack: {
				return *reinterpret_cast<uint8_t**>(stackPtr + value.stack);
			}
			case Argument::Type::DerefDerefStack: {
				return **reinterpret_cast<uint8_t***>(stackPtr + value.stack);
			}
			case Argument::Type::Literal: {
				return reinterpret_cast<uint8_t*>(&value);
			}
			case Argument::Type::DerefLiteral: {
				return *reinterpret_cast<uint8_t**>(&value);
			}
			case Argument::Type::Return: {
				return returnPtr;
			}
			case Argument::Type::DerefReturn: {
				return *reinterpret_cast<uint8_t**>(returnPtr);
			}
			}
			return nullptr;
		}

		friend class Linker;

		Instruction::Code _code = Instruction::Code::AddInt64;
		Argument::Type _outputType = Argument::Type::None;

		Argument::Type _firstType = Argument::Type::None;
		Argument::Type _secondType = Argument::Type::None;

		uint16_t _outputOffset = 0;

		Argument _output;
		Argument _first;
		Argument _second;
	};

	using CFunction = void(*)(StackAllocator& stack, uint8_t* stackPtr, uint8_t* returnPtr);


	class FunctionImpl {
	public:

		FunctionImpl() {}
		FunctionImpl(Allocator& alloc, uint64_t stackSize, uint64_t instructionsCount)
			: _alloc(&alloc)
			, _stackSize(stackSize)
			, _instructionsCount(instructionsCount)
			, _instructions(_alloc->allocate<Instruction>(_instructionsCount)) {}
		FunctionImpl(Allocator& alloc, uint64_t stackSize, CFunction cfun)
			: _alloc(&alloc)
			, _stackSize(stackSize)
			, _instructionsCount(0)
			, _cfunc(cfun) {}
		FunctionImpl(const FunctionImpl& function) = delete;
		FunctionImpl(FunctionImpl&& function) noexcept
			: _alloc(function._alloc)
			, _stackSize(function._stackSize)
			, _instructionsCount(function._instructionsCount)
			, _instructions(function._instructions) {
			function._instructions = nullptr;
		}
		~FunctionImpl() {
			if (_instructions != nullptr && _instructionsCount > 0) {
				_alloc->deallocate(_instructions);
			}
		}

		FunctionImpl& operator =(FunctionImpl&& other) noexcept {
			_alloc = other._alloc;
			_stackSize = other._stackSize;
			_instructionsCount = other._instructionsCount;
			_instructions = other._instructions;

			other._instructions = nullptr;

			return *this;
		}

		void call(StackAllocator& stack, uint8_t* stackPtr, uint8_t* returnPtr) const {
			if (_instructionsCount > 0) {
				for (uint64_t index = 0u; index < _instructionsCount;) {
					_instructions[index].call(index, stack, stackPtr, returnPtr);
				}
			}
			else {
				_cfunc(stack, stackPtr, returnPtr);
			}
		}

		uint64_t stackSize() const {
			return _stackSize;
		}

		Allocator& alloc() {
			return *_alloc;
		}

		explicit operator bool() const {
			return _alloc != nullptr;
		}

	private:
		friend class Linker;

		Allocator* _alloc = nullptr;
		uint64_t _stackSize = 0;

		uint64_t _instructionsCount = 0;
		union {
			Instruction* _instructions = nullptr;
			CFunction _cfunc;
		};
	};

	class FunctionHolder {
	public:

		void invoke(StackAllocator& stack) {
			auto stackPtr = stack.allocate(function().stackSize());
			function().call(stack, stackPtr, nullptr);
			stack.deallocate(function().stackSize());
		}

	private:

		virtual FunctionImpl& function() = 0;
	};

	class StandaloneFunction : public FunctionHolder {
	public:
		StandaloneFunction(FunctionImpl&& function)
			: _function(std::move(function)) {}

		explicit operator bool() const {
			return static_cast<bool>(_function);
		}

	private:

		FunctionImpl& function() override {
			return _function;
		}

		FunctionImpl _function;
	};

	enum class OperatorCode : uint8_t {
		Constructor,
		Destructor,
		Plus,
		Minus,
		Multiply,
		Divide,
		Assign,
	};

	class Type {
	public:
		Type() = default;
		~Type() = default;

		Type(bool isConst_, bool isRef_, bool hasAddress_)
			: isConst(isConst_), isRef(isRef_), hasAddress(hasAddress_) {}
		uint64_t sizeOf() const { return isRef ? sizeof(void*) : sizeOfImpl(); }
		virtual std::string id() const = 0;

		struct FunctionInfo {
			const FunctionImpl* function = nullptr;
			std::unique_ptr<const Type> returnType;
			// TODO terrible, TEMPORARY
			const std::vector<std::unique_ptr<const Type>>* argTypes = nullptr;
		};

		virtual FunctionInfo deduceFunction(const std::vector<std::unique_ptr<const Type>>& argumentTypes) const {
			return FunctionInfo{};
		}

		friend bool operator==(const Type& lhs, const Type& rhs) {
			return lhs.isConst == rhs.isConst
				&& lhs.isRef == rhs.isRef
				&& lhs.hasAddress == rhs.hasAddress
				&& lhs.id() == rhs.id();
		}

		static std::unique_ptr<Type> clone(const std::unique_ptr<Type>& type) {
			return !type ? nullptr : type->clone();
		}

		static std::unique_ptr<Type> clone(const std::unique_ptr<const Type>& type) {
			return !type ? nullptr : type->clone();
		}

		bool convertableTo(const Type& other) const {
			// TODO casting, inheritance
			if (id() != other.id()) {
				return false;
			}
			if (!other.isRef) {
				return true;
			}
			if (isConst && !other.isConst) {
				return false;
			}
			if (!hasAddress && other.hasAddress) {
				return false;
			}
			return true;
		}

		bool isConst = false;
		bool isRef = false;
		bool hasAddress = false;
 	private:
		virtual uint64_t sizeOfImpl() const = 0;
		virtual std::unique_ptr<Type> clone() const = 0;
	};
}

#endif /*ketl_h*/