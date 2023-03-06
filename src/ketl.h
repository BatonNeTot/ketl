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
			Stack,
			Literal,
			Return,
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
			DefinePrimitive,
			AssignPrimitive,
			AllocateFunctionStack,
			DefineFuncParameter,
			CallFunction,
		};

		Instruction() {}
		Instruction(
			Code code_,

			Argument::Type outputType_,
			Argument::Type firstType_,
			Argument::Type secondType_,

			Argument output_,
			Argument first_,
			Argument second_)
			:
			code(code_),

			outputType(outputType_),
			firstType(firstType_),
			secondType(secondType_),

			output(output_),
			first(first_),
			second(second_) {}

		Instruction::Code code = Instruction::Code::AddInt64;

		Argument::Type outputType = Argument::Type::None;
		Argument::Type firstType = Argument::Type::None;
		Argument::Type secondType = Argument::Type::None;

		Argument output;
		Argument first;
		Argument second;
	};

	class HeapObject {
	public:

		HeapObject() = default;
		virtual ~HeapObject() = default;

	protected:

		template <typename T>
		void registerLink(T** link) {
			// this code is genius or madness
			auto* ptr = *link;
			auto heapObject = static_cast<const HeapObject*>(ptr);
			uint64_t diff = reinterpret_cast<const uint8_t*>(heapObject) - reinterpret_cast<const uint8_t*>(ptr);
			_links.emplace_back(reinterpret_cast<const uint8_t* const*>(link), diff);
		}

	private:

		friend class Context;

		struct Link {
			const uint8_t* const* holder;
			uint64_t diff;

			const HeapObject* ptr() const {
				return reinterpret_cast<const HeapObject*>(*holder + diff);
			}
		};

		virtual void* rootPtr() { return this; }

		mutable bool _usageFlag = false;
		std::vector<Link> _links;
	};

	class FunctionImpl : public HeapObject {
	public:

		FunctionImpl() {}
		FunctionImpl(Allocator& alloc, uint64_t stackSize, uint64_t instructionsCount)
			: _alloc(&alloc)
			, _stackSize(stackSize + sizeof(uint64_t))
			, _instructionsCount(instructionsCount)
			, _instructions(_alloc->allocate<Instruction>(_instructionsCount)) {}
		FunctionImpl(const FunctionImpl& function) = delete;
		FunctionImpl(FunctionImpl&& function) noexcept
			: _alloc(function._alloc)
			, _stackSize(function._stackSize)
			, _instructionsCount(function._instructionsCount)
			, _instructions(function._instructions) {
			function._instructions = nullptr;
		}
		~FunctionImpl() {
			_alloc->deallocate(_instructions);
		}

		FunctionImpl& operator =(FunctionImpl&& other) noexcept {
			_alloc = other._alloc;
			_stackSize = other._stackSize;
			_instructionsCount = other._instructionsCount;
			_instructions = other._instructions;

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

		void* rootPtr() override { return this; }

		Allocator* _alloc = nullptr;
		uint64_t _stackSize = 0;

		uint64_t _instructionsCount = 0;
		Instruction* _instructions = nullptr;
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

	class TypeObject : public HeapObject {
	public:
		TypeObject() = default;
		virtual ~TypeObject() = default;

		virtual std::string id() const = 0;

		virtual uint64_t sizeOf() const = 0;

		virtual bool isLight() const = 0;

		friend bool operator==(const TypeObject& lhs, const TypeObject& rhs) {
			return lhs.id() == rhs.id();
		}

	private:

		void* rootPtr() override { return this; }

	};
}

#endif /*ketl_h*/