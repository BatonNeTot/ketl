/*🍲Ketl🍲*/
#include "common.h"

#include "type.h"

namespace Ketl {

	void* TypedPtr::as(std::type_index typeIndex, VirtualMachine& vm) const {
		// TODO
		/*
		auto typeVarIt = context._userTypes.find(typeIndex);
		if (typeVarIt == context._userTypes.end()) {
			return nullptr;
		}
		*/

		// TODO BIG
		// do correct convertation, etc. PrimitiveTypeObject* -> TypeObject*

		if (_type && _type->isLight()) {
			return *reinterpret_cast<void**>(_ptr);
		}

		return _ptr;
	}

	bool memequ(const void* lhs, const void* rhs, size_t size) {
		return 0 == memcmp(lhs, rhs, size);
	}

	bool VarPureTraits::convertableTo(const VarPureTraits& other) const {
		return !other.isRef || !isConst || other.isConst;
	}

	bool VarPureTraits::sameTraits(const VarPureTraits& other) const {
		return memequ(this, &other, sizeof(VarPureTraits));
	}

	bool operator==(const VarPureTraits& lhs, const VarPureTraits& rhs) {
		return memequ(&lhs, &rhs, sizeof(VarPureTraits));
	}

	bool operator<(const VarTraits& lhs, const VarTraits& rhs) {
		if (lhs.type != rhs.type) {
			return lhs.type < rhs.type;
		}
		if (lhs.isConst == rhs.isConst) {
			return lhs.isConst < rhs.isConst;
			;
		}
		return lhs.isRef < rhs.isRef;
	}

	bool operator==(const VarTraits& lhs, const VarTraits& rhs) {
		return memequ(&lhs, &rhs, sizeof(VarTraits));
	}

}