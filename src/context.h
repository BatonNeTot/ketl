/*🍲Ketl🍲*/
#ifndef context_h
#define context_h

#include "compiler/common.h"
#include "type.h"

#include <cinttypes>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <memory>
#include <optional>
#include <typeindex>
#include <iostream>
#include <functional>
#include <algorithm>

namespace Ketl {

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

		// TODO
	public:

		Context& _context;
		Variable& _var;
	};

	class Context {
	public:

		Context(Allocator& allocator, uint64_t globalStackSize);
		~Context() {
			_rootObjects.clear();
			collectGarbage();
		}

		// maybe would be better to return pointer and make it null in case of lack
		ContextedVariable getVariable(const std::string_view& id) {
			auto it = _globals.find(id);
			return ContextedVariable(*this, it == _globals.end() ? _emptyVar : it->second);
		}

		// TODO std::unique_ptr<Type> and std::unique_ptr<const Type> need to deal with it somehow
		bool declareGlobal(const std::string_view& id, void* stackPtr, const TypeObject& type) {
			auto [it, success] = _globals.try_emplace(std::string(id), stackPtr, type);
			return success;
		}

		template <class T>
		T* declareGlobal(const std::string_view& id, const TypeObject& type) {
			auto [it, success] = _globals.try_emplace(std::string(id), nullptr, type);
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

		void registerPrimaryOperator(OperatorCode op, const std::string_view& argumentsNotation, Instruction::Code code, const std::string_view& outputType) {
			_primaryOperators[op].try_emplace(argumentsNotation, code, outputType);
		}

		std::pair<Instruction::Code, std::string_view> deducePrimaryOperator(OperatorCode op, const std::string_view& argumentsNotation) const {
			auto opIt = _primaryOperators.find(op);
			if (opIt == _primaryOperators.end()) {
				return std::make_pair<Instruction::Code, std::string_view>(Instruction::Code::None, "");
			}
			auto it = opIt->second.find(argumentsNotation);
			return it != opIt->second.end() ? it->second : std::make_pair<Instruction::Code, std::string_view>(Instruction::Code::None, "");
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

		void markObject(const HeapObject* ptr) {
			if (ptr->_usageFlag == _currentUsedFlag) {
				return;
			}
			ptr->_usageFlag = _currentUsedFlag;
			for (const auto& link : ptr->_links) {
				markObject(link());
			}
		}

		void registerObject(HeapObject* ptr) {
			_heapObjects.emplace(ptr);
			ptr->_usageFlag = _currentUsedFlag;
		}

		void* allocateObject(size_t size) {
			auto ptr = _alloc.allocate(size);
			return ptr;
		}

		template <typename T>
		inline T* allocateObject() {
			return reinterpret_cast<T*>(allocateObject(sizeof(T)));
		}

		template <typename T, typename... Args>
		inline T* createObject(Args&&... args) {
			auto ptr = allocateObject<T>();
			new(ptr) T(std::forward<Args>(args)...);
			return ptr;
		}

		// TODO right now, _rootObjects and links in info does not filling
		// so, do not call until destructor for now
		void collectGarbage() {
			_currentUsedFlag = !_currentUsedFlag;

			for (const auto& ptr : _rootObjects) {
				markObject(ptr);
			}

			for (auto it = _heapObjects.begin(), end = _heapObjects.end(); it != end;) {
				auto* ptr = *it;
				if (ptr->_usageFlag != _currentUsedFlag) {
					// there must be dtor, so we can skip wasting time on checking 
					auto root = ptr->rootPtr();
					ptr->~HeapObject();
					_alloc.deallocate(root);
					it = _heapObjects.erase(it);
				}
				else {
					++it;
				}
			}
		}

		static Variable _emptyVar;
		Allocator& _alloc;
		StackAllocator _globalStack;
		std::unordered_map<std::string, Variable, StringHash, StringEqualTo> _globals;

		std::unordered_map<std::type_index, Variable> _userTypes;

		std::unordered_map<OperatorCode, std::unordered_map<std::string_view, std::pair<Instruction::Code, std::string_view>>> _primaryOperators;

		bool _currentUsedFlag = false;

		std::unordered_set<HeapObject*> _rootObjects;
		std::unordered_set<HeapObject*> _heapObjects;
	};

	template <class... Args>
	void ContextedVariable::operator()(Args&&... args) {
		_var.call<void>(_context._globalStack, std::forward<Args>(args)...);
	}
}

#endif /*context_h*/