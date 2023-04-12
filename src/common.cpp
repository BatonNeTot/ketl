/*🍲Ketl🍲*/
#include "common.h"

#include "type.h"

namespace Ketl {

	void* TypedPtr::as(std::type_index typeIndex, Context& context) const {
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

}