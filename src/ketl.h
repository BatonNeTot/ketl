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
			Return
		};

		union {
			void* globalPtr;
			uint64_t stack;

			int64_t integer;
			uint64_t uinteger;
			double floating;
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
			AllocateStack,
			DefineArgument,
			Call,
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
			uint8_t funcIndex,
			Argument::Type firstType,
			Argument::Type secondType,

			Argument output,
			Argument first,
			Argument second)
			:
			_code(code),
			_funcIndex(funcIndex),
			_firstType(firstType),
			_secondType(secondType),

			_output(output),
			_first(first),
			_second(second) {}

		void call(uint64_t& index, StackAllocator& stack, uint8_t* stackPtr, uint8_t* returnPtr);

	private:

		template <class T>
		inline T& outputStack(uint8_t* stackPtr, uint8_t* returnPtr) {
			return *reinterpret_cast<T*>(getArgument(stackPtr, returnPtr, Argument::Type::Stack, _output));
		}
		template <class T>
		inline T& output(uint8_t* stackPtr, uint8_t* returnPtr) {
			return *reinterpret_cast<T*>(getArgument(stackPtr, returnPtr, _outputType, _output));
		}

		template <class T>
		inline T& first(uint8_t* stackPtr, uint8_t* returnPtr) {
			return *reinterpret_cast<T*>(getArgument(stackPtr, returnPtr, _firstType, _first));
		}

		template <class T>
		inline T& second(uint8_t* stackPtr, uint8_t* returnPtr) {
			return *reinterpret_cast<T*>(getArgument(stackPtr, returnPtr, _secondType, _second));
		}

		static void* getArgument(uint8_t* stackPtr, uint8_t* returnPtr, Argument::Type type, Argument& value) {
			switch (type) {
			case Argument::Type::Global: {
				return value.globalPtr;
			}
			case Argument::Type::Stack: {
				return stackPtr + value.stack;
			}
			case Argument::Type::Literal: {
				return &value;
			}
			case Argument::Type::Return: {
				return returnPtr;
			}
			}
			return nullptr;
		}

		friend class Linker;

		Instruction::Code _code = Instruction::Code::AddInt64;
		union {
			Argument::Type _outputType = Argument::Type::None;
			uint8_t _funcIndex;
		};
		Argument::Type _firstType = Argument::Type::None;
		Argument::Type _secondType = Argument::Type::None;

		Argument _output;
		Argument _first;
		Argument _second;
	};


	class PureFunction {
	public:

		PureFunction() {}
		PureFunction(Allocator& alloc, uint64_t stackSize, uint64_t instructionsCount)
			: _alloc(&alloc)
			, _stackSize(stackSize)
			, _instructionsCount(instructionsCount)
			, _instructions(_alloc->allocate<Instruction>(_instructionsCount)) {}
		PureFunction(const PureFunction& function) = delete;
		PureFunction(PureFunction&& function) noexcept
			: _alloc(function._alloc)
			, _stackSize(function._stackSize)
			, _instructionsCount(function._instructionsCount)
			, _instructions(function._instructions) {
			function._instructions = nullptr;
		}
		~PureFunction() {
			if (_instructions != nullptr) {
				_alloc->deallocate(_instructions);
			}
		}

		void call(StackAllocator& stack, uint8_t* stackPtr, uint8_t* returnPtr) {
			for (uint64_t index = 0u; index < _instructionsCount;) {
				_instructions[index].call(index, stack, stackPtr, returnPtr);
			}
		}

		uint64_t stackSize() const {
			return _stackSize;
		}

		Allocator& alloc() {
			return *_alloc;
		}

	private:
		friend class Linker;

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

		virtual PureFunction& function() = 0;
	};

	class StandaloneFunction : public FunctionHolder {
	public:
		StandaloneFunction(PureFunction&& function)
			: _function(std::move(function)), _stack(function.alloc(), function.stackSize()) {}

		StackAllocator& stack() {
			return _stack;
		}

	private:

		PureFunction& function() override {
			return _function;
		}

		PureFunction _function;

		StackAllocator _stack;
	};

	class Type {
	public:
		virtual uint64_t sizeOf() const = 0;
		virtual std::string id() const = 0;
		virtual std::pair<uint64_t, std::unique_ptr<const Type>> deduceFunction(std::vector<std::unique_ptr<const Type>>& argumentTypes) const { 
			return std::make_pair<uint64_t, std::unique_ptr<const Type>>(0, {});
		}

		friend bool operator==(const Type& lhs, const Type& rhs) {
			return lhs.id() == rhs.id();
		}

		static std::unique_ptr<Type> clone(const Type& type) {
			return type.clone();
		}

		static std::unique_ptr<Type> clone(const Type* type) {
			return type == nullptr ? nullptr : type->clone();
		}

		static std::unique_ptr<Type> clone(const std::unique_ptr<Type>& type) {
			return !type ? nullptr : type->clone();
		}

		static std::unique_ptr<Type> clone(const std::unique_ptr<const Type>& type) {
			return !type ? nullptr : type->clone();
		}
	private:
		virtual std::unique_ptr<Type> clone() const = 0;
	};

	class BasicTypeBody {
	public:

		using ConstructorFunc = void(*)(StackAllocator& stack, uint8_t* stackPtr, uint8_t* returnPtr);
		using DestructorFunc = void(*)(StackAllocator& stack, uint8_t* stackPtr, uint8_t* returnPtr);

		BasicTypeBody(const std::string& id, uint64_t size)
			: _id(id), _size(size) {}

		const std::string& id() const {
			return _id;
		}

		uint64_t deduceConstructor(std::vector<std::unique_ptr<const Type>>& argumentTypes) const {
			for (uint64_t cstrIt = 0u; cstrIt < _cstrs.size(); ++cstrIt) {
				auto& cstr = _cstrs[cstrIt];
				if (argumentTypes.size() != cstr.argTypes.size()) {
					continue;
				}
				bool next = false;
				for (uint64_t typeIt = 0u; typeIt < cstr.argTypes.size(); ++typeIt) {
					// TODO light casting (non-const to const, inheritance)
					// TODO lvalue and rvalue (as lreferece and rreference)
					if (*cstr.argTypes[typeIt] != *argumentTypes[typeIt]) {
						next = true;
						break;
					}
				}
				if (next) {
					continue;
				}
				return cstrIt;
			}
			return contructorCount();
		}

		ConstructorFunc construct(uint64_t funcIndex) const {
			return _cstrs[funcIndex].func;
		}

		uint64_t contructorCount() const {
			return _cstrs.size();
		}

		DestructorFunc destruct(StackAllocator& stack, uint8_t* stackPtr, uint8_t* returnPtr) const {
			return _dstr;
		}

		uint64_t sizeOf() const {
			return _size;
		}

	private:

		std::string _id;
		struct Constructor {
			Constructor() {}
			ConstructorFunc func = nullptr;
			std::vector<std::unique_ptr<const Type>> argTypes;
		};
		std::vector<Constructor> _cstrs;
		DestructorFunc _dstr = nullptr;
		uint64_t _size = 0;

	};

	class TypeOfType : public Type {
	public:
		TypeOfType(const BasicTypeBody* body)
			: _body(body) {}

		uint64_t sizeOf() const override { return sizeof(BasicTypeBody); };
		std::string id() const override { return "Type"; };
		std::pair<uint64_t, std::unique_ptr<const Type>> deduceFunction(std::vector<std::unique_ptr<const Type>>& argumentTypes) const override {
			// TODO constructor
			return std::make_pair<uint64_t, std::unique_ptr<const Type>>(0, {});
		}
	private:
		std::unique_ptr<Type> clone() const override {
			return std::make_unique<TypeOfType>(*this);
		}

		const BasicTypeBody* _body;
	};

	class LRefType : public Type {
	public:
		LRefType(std::unique_ptr<Type>&& baseType)
			: _baseType(std::move(baseType)) {}
		LRefType(const LRefType& other)
			: _baseType(Type::clone(other._baseType)) {}

		uint64_t sizeOf() const override { return _baseType->sizeOf(); };
		std::string id() const override { return "(" + _baseType->id() + ")&"; };
		std::pair<uint64_t, std::unique_ptr<const Type>> deduceFunction(std::vector<std::unique_ptr<const Type>>& argumentTypes) const override {
			return _baseType->deduceFunction(argumentTypes);
		}
	private:
		std::unique_ptr<Type> clone() const override {
			return std::make_unique<LRefType>(*this);
		}

		std::unique_ptr<Type> _baseType;
	};

	class ConstType : public Type {
	public:
		ConstType(std::unique_ptr<Type>&& baseType)
			: _baseType(std::move(baseType)) {}
		ConstType(const ConstType& other)
			: _baseType(Type::clone(other._baseType)) {}

		uint64_t sizeOf() const override { return _baseType->sizeOf(); };
		std::string id() const override { return "Const " + _baseType->id(); };
		std::pair<uint64_t, std::unique_ptr<const Type>> deduceFunction(std::vector<std::unique_ptr<const Type>>& argumentTypes) const override {
			return _baseType->deduceFunction(argumentTypes);
		}
	private:
		std::unique_ptr<Type> clone() const override {
			return std::make_unique<ConstType>(*this);
		}

		std::unique_ptr<Type> _baseType;
	};

	class BasicType : public Type {
	public:

		BasicType(const BasicTypeBody* body)
			: _body(body) {}

		uint64_t sizeOf() const override {
			return _body->sizeOf();
		}

		std::string id() const override {
			return _body->id();
		}

	private:

		std::unique_ptr<Type> clone() const override {
			return std::make_unique<BasicType>(*this);
		}

		const BasicTypeBody* _body;
	};

	class Function {
	public:
		Function() {}
		Function(Allocator& alloc_)
			: alloc(&alloc_) {}
		~Function() {
			if (functions) {
				alloc->deallocate(functions);
			}
		}

		void emplaceFunction(PureFunction&& function) {
			auto newFunctions = alloc->allocate<PureFunction>((functionsCount + 1) * sizeof(PureFunction));
			new(newFunctions + functionsCount) PureFunction(std::move(function));
			if (functions != nullptr) {
				memcpy(newFunctions, functions, functionsCount * sizeof(PureFunction));
				alloc->deallocate(functions);
			}
			++functionsCount;
			functions = newFunctions;
		}

		Allocator* alloc = nullptr;
		PureFunction* functions = nullptr;
		uint64_t functionsCount = 0;
	};

	class FunctionType : public Type {
	public:
		FunctionType() {}
		FunctionType(const FunctionType& function)
			: infos(function.infos) {}

		uint64_t sizeOf() const override {
			return sizeof(Function);
		}
		std::string id() const override {
			if (infos.size() > 1) {
				return "MultiCastFunction";
			}
			else {
				auto& func = infos.front();
				auto id = func.returnType->id() + "(*)(";
				for (uint64_t i = 0u; i < func.argTypes.size(); ++i) {
					if (i != 0) {
						id += ", ";
					}
					id += func.argTypes[i]->id();
				}
				return id;
			}
		}
		std::pair<uint64_t, std::unique_ptr<const Type>> deduceFunction(std::vector<std::unique_ptr<const Type>>& argumentTypes) const override {
			for (uint64_t infoIt = 0u; infoIt < infos.size(); ++infoIt) {
				auto& info = infos[infoIt];
				if (argumentTypes.size() != info.argTypes.size()) {
					continue;
				}
				bool next = false;
				for (uint64_t typeIt = 0u; typeIt < info.argTypes.size(); ++typeIt) {
					// TODO light casting (non-const to const, inheritance)
					// TODO lvalue and rvalue (as lreferece and rreference)
					if (*info.argTypes[typeIt] != *argumentTypes[typeIt]) {
						next = true;
						break;
					}
				}
				if (next) {
					continue;
				}
				return std::make_pair<uint64_t, std::unique_ptr<const Type>>(std::move(infoIt), Type::clone(info.returnType));
			}
			return Type::deduceFunction(argumentTypes);
		}

		struct Info {
			Info() {}
			Info(const Info& info)
				: returnType(Type::clone(info.returnType)), argTypes(info.argTypes.size()) {
				for (uint64_t i = 0u; i < info.argTypes.size(); ++i) {
					argTypes[i] = Type::clone(info.argTypes[i]);
				}
			}

			std::unique_ptr<const Type> returnType;
			std::vector<std::unique_ptr<const Type>> argTypes;
		};
		std::vector<Info> infos;

	private:
		std::unique_ptr<Type> clone() const override {
			return std::make_unique<FunctionType>(*this);
		}
	};

	class Context {
	public:

		Context(Allocator& allocator, uint64_t globalStackSize);

		struct Variable {
			void* value;
			std::unique_ptr<const Type> type;
		};

		template <class T>
		BasicTypeBody* declareType(const std::string& id) {
			return declareType(id, sizeof(T));
		}

		template <>
		BasicTypeBody* declareType<void>(const std::string& id) {
			return declareType(id, 0);
		}

		BasicTypeBody* declareType(const std::string& id, uint64_t sizeOf) {
			auto theTypePtr = getGlobal<BasicTypeBody>("Type");
			auto typePtr = reinterpret_cast<BasicTypeBody*>(allocateOnGlobalStack(BasicType(theTypePtr)));
			new(typePtr) BasicTypeBody(id, sizeOf);
			_globals.try_emplace(id, typePtr, std::make_unique<BasicType>(theTypePtr));
			return typePtr;
		}

		template <class T>
		T* declareGlobal(const std::string& id, const Type& type) {
			auto [it, success] = _globals.try_emplace(id, nullptr, Type::clone(type));
			if (success) {
				auto valuePtr = reinterpret_cast<Type*>(allocateOnGlobalStack(*it->second.type));
				it->second.value = valuePtr;
			}
			return reinterpret_cast<T*>(it->second.value);
		}

		template <class T>
		T* getGlobal(const std::string& id) {
			auto it = _globals.find(id);
			return it != _globals.end() ? reinterpret_cast<T*>(it->second.value) : nullptr;
		}

		const Type* getGlobalType(const std::string& id) {
			auto it = _globals.find(id);
			return it != _globals.end() ? it->second.type.get() : nullptr;
		}

		uint8_t* allocateOnGlobalStack(const Type& type) {
			auto ptr = _globalStack.allocate(type.sizeOf());
			//type.construct(ptr);
			return ptr;
		}

	public: // TODO

		StackAllocator _globalStack;
		std::unordered_map<std::string, Variable> _globals;
	};

	class Environment {
	public:

		Environment();

		template <class T>
		T* getGlobal(const std::string& id) {
			return _context.getGlobal<T>(id);
		}

		template <class T>
		T* declareGlobal(const std::string& id, const Type& type) {
			return _context.declareGlobal<T>(id, type);
		}

	public: //TODO private

		Allocator _alloc;
		Context _context;

		std::unordered_map<std::type_index, std::string> _typeIdByIndex;
	};
}

#endif /*ketl_h*/