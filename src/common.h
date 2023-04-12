/*🍲Ketl🍲*/
#ifndef common_h
#define common_h

#include <typeindex>
#include <string_view>

namespace Ketl {

	enum class OperatorCode : uint8_t {
		None,
		Constructor,
		Destructor,
		Call,
		Plus,
		Minus,
		Multiply,
		Divide,
		Equal,
		NonEqual,
		Assign,

	};

	inline OperatorCode parseOperatorCode(const std::string_view& opStr) {
		switch (opStr.length()) {
		case 1:
			switch (opStr[0]) {
			case '+': return OperatorCode::Plus;
			case '-': return OperatorCode::Minus;
			case '*': return OperatorCode::Multiply;
			case '/': return OperatorCode::Divide;
			case '=': return OperatorCode::Assign;
			} break;
		case 2:
			switch (opStr[0]) {
			case '=':
				switch (opStr[1]) {
				case '=': return OperatorCode::Equal;
				} break;
			case '!':
				switch (opStr[1]) {
				case '=': return OperatorCode::NonEqual;
				} break;
			} break;
		}

		return OperatorCode::None;
	}

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

	class TypeObject;
	class Context;

	class TypedPtr {
	public:

		TypedPtr() {}
		TypedPtr(void* ptr, const TypeObject& type)
			: _ptr(ptr), _type(&type) {}

		void* as(std::type_index typeIndex, Context& context) const;

		const TypeObject& type() const {
			return *_type;
		}

		void* rawData() const {
			return _ptr;
		}

		void data(void* ptr) {
			_ptr = ptr;
		}

	private:

		friend class Context;

		void* _ptr = nullptr;
		const TypeObject* _type = nullptr;

	};

	struct VarTraits {
		bool isConst = false;
		bool isRef = false;
	};

}

#endif /*common_h*/