/*🍲Ketl🍲*/
#ifndef common_h
#define common_h

#include <typeindex>
#include <string_view>
#include <map>

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
	class VirtualMachine;

	class TypedPtr {
	public:

		TypedPtr() {}
		TypedPtr(void* ptr, const TypeObject& type)
			: _ptr(ptr), _type(&type) {}

		void* as(std::type_index typeIndex, VirtualMachine& vm) const;

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
		friend class Variable;

		void* _ptr = nullptr;
		const TypeObject* _type = nullptr;

	};

	bool memequ(const void* lhs, const void* rhs, size_t size);

	struct VarPureTraits {
		bool isConst = false;
		bool isRef = false;

		VarPureTraits() = default;
		VarPureTraits(bool isConst_, bool isRef_)
			: isConst(isConst_), isRef(isRef_) {}

		bool convertableTo(const VarPureTraits& other) const;

		bool sameTraits(const VarPureTraits& other) const;
	};

	bool operator==(const VarPureTraits& lhs, const VarPureTraits& rhs);

	struct VarTraits : public VarPureTraits {
		const TypeObject* type = nullptr;

		VarTraits() = default;
		VarTraits(const TypeObject& type_)
			: VarPureTraits(), type(&type_) {}
		VarTraits(const TypeObject& type_, bool isConst_, bool isRef_)
			: VarPureTraits(isConst_, isRef_), type(&type_) {}
		VarTraits(const TypeObject& type_, const VarPureTraits& traits)
			: VarPureTraits(traits), type(&type_) {}
	};

	bool operator<(const VarTraits& lhs, const VarTraits& rhs);
	bool operator==(const VarTraits& lhs, const VarTraits& rhs);

	template <typename K, typename V, V DEFAULT> 
	struct MultiKeyMap {
		std::map<K, MultiKeyMap> nodes;
		V leaf = DEFAULT;
	};

	inline std::size_t CombineHash(std::size_t base) {
		return base;
	}

	template <typename... Rest>
	inline std::size_t CombineHash(std::size_t base, std::size_t first, Rest&&... rest) {
		base ^= first + 0x9e3779b9 + (base << 6) + (base >> 2);
		return CombineHash(base, std::forward<Rest>(rest)...);
	}

}

namespace std {
	template <>
	struct hash<Ketl::VarPureTraits> {
		std::size_t operator()(const Ketl::VarPureTraits& traits) const {
			return traits.isConst * 2 + traits.isRef;
		}
	};
}

namespace std {
	template <>
	struct hash<Ketl::VarTraits> {
		std::size_t operator()(const Ketl::VarTraits& traits) const {
			return Ketl::CombineHash(
				std::hash<const Ketl::TypeObject*>()(traits.type),
				std::hash<Ketl::VarPureTraits>()(traits));
		}
	};
}

#endif /*common_h*/