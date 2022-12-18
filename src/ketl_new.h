/*🐟Ketl🐟*/
#ifndef eel_new_h
#define eel_new_h

#include <cinttypes>
#include <string>
#include <unordered_map>
#include <memory>
#include <optional>
#include <typeindex>
#include <iostream>

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

enum class InstructionCode : uint8_t {
	AddInt,
	DefineInt,
};

enum class ArgumentType : uint8_t {
	None,
	Global,
	Stack,
	Literal
};

struct Argument {
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

	void call(StackAllocator& stack, uint8_t* stackPtr) {
		switch (_code) {
		case InstructionCode::AddInt: {
			output<int64_t>(stackPtr) = first<int64_t>(stackPtr) + second<int64_t>(stackPtr);
			break;
		}
		}
	}

private:

	template <class T>
	T& output(uint8_t* stackPtr) {
		return *reinterpret_cast<T*>(getArgument(stackPtr, _outputType, _output));
	}

	template <class T>
	T& first(uint8_t* stackPtr) {
		return *reinterpret_cast<T*>(getArgument(stackPtr, _firstType, _first));
	}

	template <class T>
	T& second(uint8_t* stackPtr) {
		return *reinterpret_cast<T*>(getArgument(stackPtr, _secondType, _second));
	}

	static void* getArgument(uint8_t* stackPtr, ArgumentType type, Argument& value) {
		switch (type) {
		case ArgumentType::Global: {
			return value.globalPtr;
		}
		case ArgumentType::Stack: {
			return stackPtr + value.stack;
		}
		case ArgumentType::Literal: {
			return &value;
		}
		}
		return nullptr;
	}

	friend class Linker;

	InstructionCode _code;
	ArgumentType _outputType;
	ArgumentType _firstType;
	ArgumentType _secondType;

	Argument _output;
	Argument _first;
	Argument _second;
};


class Function {
public:

	Function(Allocator& alloc, uint64_t stackSize, uint64_t instructionsCount)
		: _alloc(alloc)
		, _stackSize(stackSize)
		, _instructionsCount(instructionsCount)
		, _instructions(_alloc.allocate<Instruction>(_instructionsCount)) {}
	Function(const Function& function)
		: _alloc(function._alloc)
		, _stackSize(function._stackSize)
		, _instructionsCount(function._instructionsCount)
		, _instructions(_alloc.allocate<Instruction>(_instructionsCount)) {
		memcpy(_instructions, function._instructions, sizeof(Instruction) * _instructionsCount);
	}
	Function(Function&& function) noexcept
		: _alloc(function._alloc)
		, _stackSize(function._stackSize)
		, _instructionsCount(function._instructionsCount)
		, _instructions(function._instructions) {
		function._instructions = nullptr;
	}
	~Function() {
		if (_instructions != nullptr) {
			_alloc.deallocate(_instructions);
		}
	}

	void call(StackAllocator& stack, uint8_t* stackPtr) {
		for (auto i = 0u; i < _instructionsCount; ++i) {
			_instructions[i].call(stack, stackPtr);
		}
	}

	uint64_t stackSize() const {
		return _stackSize;
	}

	Allocator& alloc() {
		return _alloc;
	}

private:
	friend class Linker;

	Allocator& _alloc;
	uint64_t _stackSize = 0;

	uint64_t _instructionsCount;
	Instruction* _instructions;
};

class FunctionHolder {
public:
	FunctionHolder(Allocator& alloc, uint64_t stackSize)
		: _stack(alloc, stackSize) {}

	void operator()() {
		invoke();
	}

	void invoke() {
		auto stackPtr = _stack.allocate(function().stackSize());
		function().call(_stack, stackPtr);
		_stack.deallocate(function().stackSize());
	}

private:

	virtual Function& function() = 0;

	StackAllocator _stack;
};

class StandaloneFunction : public FunctionHolder {
public:
	StandaloneFunction(Function& function, uint64_t stackSize) 
	 : FunctionHolder(function.alloc(), stackSize), _function(std::move(function)) {}

private:

	Function& function() override {
		return _function;
	}

	Function _function;
};

class RefferedFunction : public FunctionHolder {
public:
	RefferedFunction(Function& function, uint64_t stackSize)
		: FunctionHolder(function.alloc(), stackSize), _function(function) {}

private:

	Function& function() override {
		return _function;
	}

	Function& _function;
};

class BaseValue {};

template <class T>
class Value : public BaseValue {
public:
	template <class... Args>
	Value(Args&&... args)
		: _value(std::forward<Args>(args)...) {}
private:
	T _value;
};

template <>
class Value<void> : public BaseValue {
public:

	Value() = default;
};

class Type {
public:

	Type(const std::string& id, uint64_t size)
		: _id(id), _sizeof(size) {}

	const std::string& id() const {
		return _id;
	}

	std::string castTargetStr() const {
		return "operator " + id();
	}

	friend bool operator ==(const Type& lhs, const Type& rhs) {
		return lhs._id == rhs._id;
	}

public: //TODO
	std::string _id;
	uint64_t _sizeof;
};

class Environment {
public:

	Environment();

	bool declareGlobal(const std::string& id, const std::string& typeId) {
		return _globals.try_emplace(id, _types.find(typeId)->second).second;
	}

	const Type* getGlobalType(const std::string& id) {
		auto it = _globals.find(id);
		return it != _globals.end() ? &it->second.type : nullptr;
	}

	template <class T, class... Args>
	void defineGlobalVariable(const std::string& id, Args&&... args) {
		auto& type = _types.find(_typeIdByIndex.find(typeid(T))->second)->second;
		auto& variable = _globals.try_emplace(id, type).first->second;
		variable.value = std::make_unique<Value<T>>(std::forward<Args>(args)...);
	}

	template <class T = void>
	T* getGlobalVariable(const std::string& id) {
		return reinterpret_cast<T*>(_globals.find(id)->second.value.get());
	}

	template <class T>
	void registerType(const std::string& id) {
		std::type_index index = typeid(T);
		_types.try_emplace(id, id, sizeof(Value<T>));
		_typeIdByIndex.try_emplace(index, id);
	}

	Type* getType(const std::string& id) {
		auto it = _types.find(id);
		return it != _types.end() ? &it->second : nullptr;
	}

	struct FunctionInfo {
		bool isInstruction;
		union {
			InstructionCode instructionCode;
			Function* function;
		};
		Type* returnType;
		std::vector<Type*> argTypes;
	};

	const FunctionInfo* estimateFunction(const std::string& id, const std::vector<Type*>& args) {
		auto& infoVector = _globalFunctions.find(id)->second;
		for (auto& info : infoVector) {
			if (info.argTypes.size() != args.size()) {
				continue;
			}

			// TODO check argument casts

			return &info;
		}
		return nullptr;
	}

public: //TODO private

	void registerInstruction(const std::string& id, InstructionCode code, Type* returnType, Type* firstArg, Type* secondArg = nullptr) {
		auto& infoVector = _globalFunctions.try_emplace(id).first->second;
		auto& info = infoVector.emplace_back();
		info.isInstruction = true;
		info.instructionCode = code;
		info.returnType = returnType;
		info.argTypes.emplace_back(firstArg);
		if (secondArg) {
			info.argTypes.emplace_back(secondArg);
		}
	}

	Allocator _alloc;

	struct Variable {
		Variable(Type& type_)
			: type(type_) {}

		std::unique_ptr<BaseValue> value;
		Type& type;
	};

	std::unordered_map<std::string, std::vector<FunctionInfo>> _globalFunctions;

	std::unordered_map<std::string, Variable> _globals;

	std::unordered_map<std::string, Type> _types;
	std::unordered_map<std::type_index, std::string> _typeIdByIndex;
};


#endif /*eel_new_h*/