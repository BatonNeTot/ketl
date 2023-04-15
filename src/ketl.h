/*🍲Ketl🍲*/
#ifndef ketl_h
#define ketl_h

#include "memory.h"
#include "context.h"
#include "compiler.h"

namespace Ketl {

	class VirtualMachine {
	public:

		VirtualMachine(uint64_t stackBlockSize = 4096);

		template <typename T>
		bool declareGlobal(const std::string_view& id, T* ptr) {
			auto* type = _context.typeOf<T>();
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
			if (type.isLight()) {
				_memory.registerRefRoot(reinterpret_cast<void**>(ptr));
			}
			else {
				_memory.registerAbsRoot(ptr);
			}
			return reinterpret_cast<T*>(ptr);
		}

		inline auto compile(const std::string_view& source) {
			return _compiler.compile(source, *this);
		}

		inline Variable getVariable(const std::string_view& id) {
			return Variable(*this, _context.getVariable(id));
		}

		template <typename T, typename... Args>
		inline auto createObject(Args&&... args) {
			constexpr auto size = sizeof(T);
			auto ptr = reinterpret_cast<T*>(_memory.alloc().allocate(size));
			new(ptr) T(std::forward<Args>(args)...);
			auto& links = _memory.registerMemory(ptr, size, &dtor<T>);
			return std::make_pair(ptr, &links);
		}

	private:

		template <typename T>
		static void dtor(void* ptr) {
			reinterpret_cast<T*>(ptr)->~T();
		}

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
		Ketl::Context _context;
		Ketl::Compiler _compiler;
	};


	template <typename ...Args>
	Variable Variable::operator()(Args&& ...args) const {
		//std::vector<const TypeObject*> argTypes = { _context.typeOf<Args>()... };


		auto function = *reinterpret_cast<FunctionObject**>(_vars[0].rawData());

		auto stackPtr = _vm._memory.stack().allocate(function->stackSize());
		function->call(_vm._memory.stack(), stackPtr, nullptr);
		_vm._memory.stack().deallocate(function->stackSize());

		return Variable(_vm);
	}
}

#endif /*ketl_h*/