/*🍲Ketl🍲*/
#ifndef context_h
#define context_h

#include "type.h"

#include <cinttypes>
#include <string>
#include <unordered_map>
#include <memory>
#include <optional>
#include <typeindex>
#include <iostream>
#include <functional>

namespace Ketl {

	class TypeObject;
	class Context;

	class Variable {
	public:

		Variable() {}
		Variable(void* data, const TypeObject& type)
			: _data(data), _type(&type) {}

		template <class T, class... Args>
		T* call(StackAllocator& stack, Args&&... args) {
			/*
			auto* pureFunction = as<FunctionImpl>();

			auto stackPtr = stack.allocate(pureFunction->stackSize());
			uint8_t returnData[16]; // TODO
			pureFunction->call(stack, stackPtr, returnData);
			stack.deallocate(pureFunction->stackSize());
			return reinterpret_cast<T*>(returnData);
			*/
			return nullptr;
		}

		void* as(std::type_index typeIndex, Context& context) const;

		Variable operator[](const std::string_view& key) const {

		}

		const TypeObject& type() const {
			return *_type;
		}

		void data(void* data) {
			_data = data;
		}

	private:

		void* _data = nullptr;
		const TypeObject* _type;

	};

	class ContextedVariable {
	public:

		ContextedVariable(Context& context, Variable& variable)
			: _context(context), _var(variable) {}

		template <class... Args>
		void operator()(Args&&... args);

		template <class T>
		T* as() const {
			return reinterpret_cast<T*>(_var.as(typeid(T), _context));
		}

		const TypeObject& type() const {
			return _var.type();
		}

	private:

		Context& _context;
		Variable& _var;
	};

	class Context {
	public:

		Context(Allocator& allocator, uint64_t globalStackSize);

		// maybe would be better to return pointer and make it null in case of lack
		ContextedVariable getVariable(const std::string& id) {
			auto it = _globals.find(id);
			return ContextedVariable(*this, it == _globals.end() ? _emptyVar : it->second);
		}

		// TODO std::unique_ptr<Type> and std::unique_ptr<const Type> need to deal with it somehow
		bool declareGlobal(const std::string& id, void* stackPtr, const TypeObject& type) {
			auto [it, success] = _globals.try_emplace(id, stackPtr, type);
			return success;
		}

		template <class T>
		T* declareGlobal(const std::string& id, const TypeObject& type) {
			auto [it, success] = _globals.try_emplace(id, nullptr, type);
			if (success) {
				auto valuePtr = allocateGlobal(type);
				it->second.data(valuePtr);
			}
			return it->second.as<T>();
		}
		////////////////////////

		uint8_t* allocateGlobal(const TypeObject& type) {
			return allocateOnHeap(type.sizeOf());
		}

		std::pair<Instruction::Code, std::string_view> deducePrimaryOperator(const std::string_view& functionNotation) const {
			auto it = _primaryOperators.find(functionNotation);
			return it != _primaryOperators.end() ? it->second : std::make_pair<Instruction::Code, std::string_view>(Instruction::Code::None, "");
		}

	public: // TODO

		uint8_t* allocateOnStack(uint64_t size) {
			return _globalStack.allocate(size);
		}

		void deallocateOnStack(uint64_t size) {
			_globalStack.deallocate(size);
		}

		uint8_t* allocateOnHeap(uint64_t size) {
			return _alloc.allocate(size);
		}

		void deallocateOnHeap(uint8_t* ptr) {
			_alloc.deallocate(ptr);
		}

		template <typename T>
		void declarePrimitiveType(const std::string& id) {
			declarePrimitiveType(id, sizeof(T), typeid(T));
		}

		void declarePrimitiveType(const std::string& id, uint64_t size, std::type_index typeIndex);

		static Variable _emptyVar;
		Allocator& _alloc;
		StackAllocator _globalStack;
		std::unordered_map<std::string, Variable> _globals;

		std::unordered_map<std::type_index, Variable> _userTypes;

		std::unordered_map<std::string_view, std::pair<Instruction::Code, std::string_view>> _primaryOperators;
	};

	template <class... Args>
	void ContextedVariable::operator()(Args&&... args) {
		_var.call<void>(_context._globalStack, std::forward<Args>(args)...);
	}
}

#endif /*context_h*/