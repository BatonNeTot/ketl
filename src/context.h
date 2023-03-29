/*🍲Ketl🍲*/
#ifndef context_h
#define context_h

#include "compiler/common.h"
#include "type.h"
#include "garbage_collector.h"

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

	class Variable {
	public:

		Variable(Context& context)
			: _context(context) {}
		Variable(Context& context, const TypedPtr& var)
			: _context(context) {
			_vars.emplace_back(var);
		}
		Variable(Context& context, std::vector<TypedPtr>&& vars)
			: _context(context), _vars(std::move(vars)) {}

		template <typename ...Args>
		Variable operator()(Args&& ...args) const;

		template <typename T>
		T* as() {
			return reinterpret_cast<T*>(_vars[0].as(typeid(T), _context));
		}

		bool empty() const {
			return _vars.empty();
		}

	private:
		friend class SemanticAnalyzer;

		Context& _context;
		std::vector<TypedPtr> _vars;
	};

	class Context {
	public:

		Context(Allocator& allocator, uint64_t globalStackSize);
		~Context() {}

		template <typename T>
		const TypeObject* typeOf() const {
			auto userIt = _userTypes.find(typeid(T));
			if (userIt == _userTypes.end()) {
				return nullptr;
			}
			return userIt->second;
		}

		Variable getVariable(const std::string_view& id) {
			auto it = _globals.find(id);
			if (it == _globals.end()) {
				return Variable(*this);
			}
			std::vector<TypedPtr> vars;
			vars = it->second;
			return Variable(*this, std::move(vars));
		}

		template <typename T>
		bool declareGlobal(const std::string_view& id, T* ptr) {
			auto* type = typeOf<T>();
			if (type == nullptr) {
				return false;
			}

			auto& vars = _globals[std::string(id)];
			for (const auto& var : vars) {
				if (var.type() == *type) {
					return false;
				}
			}

			vars.emplace_back(ptr, *type);
			if (type->isLight()) {
				_gc.registerRefRoot(reinterpret_cast<void**>(ptr));
			}
			else {
				_gc.registerAbsRoot(ptr);
			}
			return true;
		}

	public: // TODO private

		template <typename T = void>
		T* allocateGlobal(const std::string_view& id, const TypeObject& type) {
			if (!std::is_void_v<T> && !std::is_pointer_v<T> && type.isLight()) {
				return nullptr;
			}

			auto& vars = _globals[std::string(id)];
			for (const auto& var : vars) {
				if (var.type() == type) {
					return reinterpret_cast<T*>(var.rawData());
				}
			}
			auto ptr = allocateOnStack(type.sizeOf());

			vars.emplace_back(ptr, type);
			if (type.isLight()) {
				_gc.registerRefRoot(reinterpret_cast<void**>(ptr));
			}
			else {
				_gc.registerAbsRoot(ptr);
			}
			return reinterpret_cast<T*>(ptr);
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

		template <typename T>
		static void dtor(void* ptr) {
			reinterpret_cast<T*>(ptr)->~T();
		}

		template <typename T, typename... Args>
		inline auto createObject(Args&&... args) {
			constexpr auto size = sizeof(T);
			auto ptr = reinterpret_cast<T*>(_alloc.allocate(size));
			new(ptr) T(std::forward<Args>(args)...);
			auto& links = _gc.registerMemory(ptr, size, &dtor<T>);
			return std::make_pair(ptr, &links);
		}

		static std::vector<TypedPtr> _emptyVars;
		Allocator& _alloc;
		StackAllocator _globalStack;
		std::unordered_map<std::string, std::vector<TypedPtr>, StringHash, StringEqualTo> _globals;

		std::unordered_map<std::type_index, const TypeObject*> _userTypes;

		std::unordered_map<OperatorCode, std::unordered_map<std::string_view, std::pair<Instruction::Code, std::string_view>>> _primaryOperators;

		GarbageCollector _gc;
	};

	template <typename ...Args>
	Variable Variable::operator()(Args&& ...args) const {
		//std::vector<const TypeObject*> argTypes = { _context.typeOf<Args>()... };


		auto function = *reinterpret_cast<FunctionImpl**>(_vars[0].rawData());

		auto stackPtr = _context._globalStack.allocate(function->stackSize());
		function->call(_context._globalStack, stackPtr, nullptr);
		_context._globalStack.deallocate(function->stackSize());

		return Variable(_context);
	}
}

#endif /*context_h*/