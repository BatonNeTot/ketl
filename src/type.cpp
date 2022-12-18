/*🍲Ketl🍲*/
#include "type.h"

namespace Ketl {

	Type::FunctionInfo BasicTypeBody::deduceConstructor(const std::vector<std::unique_ptr<const Type>>& argumentTypes) const {
		Type::FunctionInfo outputInfo;

		for (uint64_t cstrIt = 0u; cstrIt < _cstrs.size(); ++cstrIt) {
			auto& cstr = _cstrs[cstrIt];
			if (argumentTypes.size() != cstr.argTypes.size()) {
				continue;
			}
			bool next = false;
			for (uint64_t typeIt = 0u; typeIt < cstr.argTypes.size(); ++typeIt) {
				if (!argumentTypes[typeIt]->convertableTo(*cstr.argTypes[typeIt])) {
					next = true;
					break;
				}
			}
			if (next) {
				continue;
			}

			outputInfo.isDynamic = false;
			outputInfo.function = &cstr.func;
			outputInfo.returnType = std::make_unique<BasicType>(this);

			outputInfo.argTypes.reserve(cstr.argTypes.size());
			for (uint64_t typeIt = 0u; typeIt < cstr.argTypes.size(); ++typeIt) {
				outputInfo.argTypes.emplace_back(Type::clone(cstr.argTypes[typeIt]));
			}

			return outputInfo;
		}
		return outputInfo;
	}

}