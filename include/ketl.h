/*🍲Ketl🍲*/
#ifndef ketl_h
#define ketl_h

#include "memory.h"
#include "type_manager.h"
#include "context.h"
#include "compiler.h"

namespace Ketl {

	class VirtualMachine {
	public:

		VirtualMachine(uint64_t stackBlockSize = 4096);

		template <typename T>
		bool declareGlobal(const std::string_view& id, T* ptr) {
			auto* type = _types.typeOf<T>();
			if (type == nullptr) {
				return false;
			}

			auto& vars = _context._globals[std::string(id)];
			for (const auto& var : vars) {
				if (var.type() == *type) {
					return false;
				}
			}

			vars.emplace_back(ptr, *type);
			if (type->isLight()) {
				_memory.registerRefRoot(reinterpret_cast<void**>(ptr));
			}
			else {
				_memory.registerAbsRoot(ptr);
			}
			return true;
		}

		template <typename T>
		const TypeObject* typeOf() const {
			return _types.typeOf<T>();
		}

		template <typename T = void> 
		T* allocateGlobal(const std::string_view& id, const TypeObject& type) {
			if (!std::is_void_v<T> && !std::is_pointer_v<T> && type.isLight()) {
				return nullptr;
			}

			auto& vars = _context._globals[std::string(id)];
			for (const auto& var : vars) {
				if (var.type() == type) {
					// TODO error?
					return reinterpret_cast<T*>(var.rawData());
				}
			}
			auto ptr = _memory.allocateOnStack(type.sizeOf());

			vars.emplace_back(ptr, type);
			_memory.registerAbsRoot(&type);
			if (type.isLight()) {
				_memory.registerRefRoot(reinterpret_cast<void**>(ptr));
			}
			else {
				_memory.registerAbsRoot(ptr);
			}
			return reinterpret_cast<T*>(ptr);
		}

		inline Compiler::Product eval(const std::string_view& source) {
			return _compiler.eval(source, *this);
		}

		inline Variable getVariable(const std::string_view& id) {
			return Variable(*this, _context.getVariable(id));
		}

		template <typename T, typename... Args>
		inline auto createObject(Args&&... args) {
			return _memory.createObject<T>(std::forward<Args>(args)...);
		}

		template <typename T>
		inline auto createArray(size_t size) {
			return _memory.createArray<T>(size);
		}

		inline const TypeObject* findOrCreateFunctionType(const TypeObject& returnType, std::vector<VarTraits>&& parameters) {
			return _types.findOrCreateFunctionType(_memory, returnType, std::move(parameters));
		}

		size_t collectGarbage() {
			return _memory.collectGarbage();
		}

	private:

		uint8_t* allocateOnHeap(uint64_t size) {
			return _memory.alloc().allocate(size);
		}

		template <typename T>
		void declarePrimitiveType(const std::string& id) {
			declarePrimitiveType(id, sizeof(T), typeid(T));
		}

		void declarePrimitiveType(const std::string& id, uint64_t size, std::type_index typeIndex);

		friend Compiler;
		friend Variable;
		friend class SemanticAnalyzer;

		Ketl::MemoryManager _memory;
		Ketl::TypeManager _types;
		Ketl::Context _context;
		Ketl::Compiler _compiler;
	};


	template <typename ...Args>
	Variable Variable::operator()(Args&& ...args) const {
		//std::vector<const TypeObject*> argTypes = { _context.typeOf<Args>()... };

		auto& var = _vars[0];
		auto function = *reinterpret_cast<FunctionObject**>(var.rawData());
		auto* returnType = var.type().getReturnType();

		if (!returnType || returnType->sizeOf() == 0) {
			auto stackPtr = _vm._memory.stack().allocate(function->stackSize());
			function->call(_vm._memory.stack(), stackPtr, nullptr);
			_vm._memory.stack().deallocate(function->stackSize());

			return Variable(_vm);
		}
		else {
			auto returnHolder = reinterpret_cast<uint8_t*>(_vm.allocateOnHeap(returnType->sizeOf()));

			auto stackPtr = _vm._memory.stack().allocate(function->stackSize());
			function->call(_vm._memory.stack(), stackPtr, returnHolder);
			_vm._memory.stack().deallocate(function->stackSize());

			return Variable(_vm, TypedPtr(returnHolder, *returnType));
		}
	}
}

#endif /*ketl_h*/