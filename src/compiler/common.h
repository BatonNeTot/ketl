/*🍲Ketl🍲*/
#ifndef compiler_common_h
#define compiler_common_h

#include "ketl.h"
#include "type.h"

namespace Ketl {

	struct StringHash {
		using is_transparent = std::true_type;

		std::size_t operator()(const std::string_view& str) const noexcept
		{
			return std::hash<std::string_view>()(str);
		}
	};

	struct StringEqualTo {
		using is_transparent = std::true_type;

		bool operator()(const std::string_view& lhs, const std::string_view& rhs) const noexcept
		{
			return lhs == rhs;
		}
	};

	class Context;

	enum class OperatorCode : uint8_t {
		None,
		Constructor,
		Destructor,
		Plus,
		Minus,
		Multiply,
		Divide,
		Assign,
		
	};

	inline OperatorCode parseOperatorCode(const std::string_view& opStr) {
		switch (opStr.length()) {
		case 1: {
			switch (opStr.data()[0]) {
			case '+': return OperatorCode::Plus;
			case '-': return OperatorCode::Minus;
			case '*': return OperatorCode::Multiply;
			case '/': return OperatorCode::Divide;
			case '=': return OperatorCode::Assign;
			}
		}
		}

		return OperatorCode::None;
	}


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

		friend class Context;

		void* _data = nullptr;
		const TypeObject* _type = nullptr;

	};

}

#endif /*compiler_common_h*/