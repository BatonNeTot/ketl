//🫖ketl
#ifndef ketl_ketl_hpp
#define ketl_ketl_hpp

extern "C" {
#include "ketl/ketl.h"
#include "ketl/function.h"
}

#include <string_view>
#include <iostream>

namespace KETL {

	namespace {
		template <class T>
		struct __TypeHelper;

		template <>
		struct __TypeHelper<int64_t> {
			static ketl_type* getType(ketl_state* pState) {
				return ketl_state_get_i64(pState);
			}
		};
	}

	class State {
	public:

		State(const ketl_allocator* pAllocator)
			: _pStateImpl(ketl_state_create(pAllocator)) {}

		State(const State& other) = delete;
		State(State&& other) = delete;

		~State() {
			ketl_state_destroy(_pStateImpl);
		}

		State& operator=(const State& other) = delete;
		State& operator=(State&& other) = delete;

		template <class R, class... Args>
		void defineCFunction(const std::string_view& name, R (*pFunc)(Args...)) {
			ketl_type_parameter aParameters[] = {
				ketl_type_parameter{__TypeHelper<R>::getType(_pStateImpl)}, (ketl_type_parameter{__TypeHelper<Args>::getType(_pStateImpl)})...
			};
			ketl_function_parameters funcParameters = {
				aParameters, 1 + sizeof...(Args)
			};
			ketl_type* pFuncType = ketl_state_get_cfunction_type(_pStateImpl, &funcParameters);
			ketl_state_define_function(_pStateImpl, name.data(), name.length(), pFuncType, reinterpret_cast<void*>(pFunc));
		}

		int64_t eval(const std::string_view& source) {
			return ketl_state_eval_int64(_pStateImpl, source.data(), source.length());
		}

	private:
		ketl_state* _pStateImpl;
	};

}

#endif // ketl_ketl_hpp
