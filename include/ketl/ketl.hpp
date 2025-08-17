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

	class State;

	ketl_state* __get_state_impl(const State& state);
	
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

	class Value {
	private:
		Value(ketl_value* p_value, State& state)
			: _p_value(p_value), _state(state) {}

		Value(const Value& other) = delete;
		Value(Value&& other)
			: _p_value(other._p_value), _state(other._state) {
			other._p_value = nullptr;
		}

	public:
		~Value() {
			if (_p_value != nullptr) {
				ketl_value_destroy(__get_state_impl(_state), _p_value);
			}
		}

		explicit operator bool() const {
			return !ketl_value_is_none(__get_state_impl(_state), _p_value);
		}

		ketl_type* get_type() const {
			return ketl_value_get_type(__get_state_impl(_state), _p_value);
		}

		template<class T>
		T as() const {
			return __ValueGetter<T>::as(__get_state_impl(_state), _p_value);
		}

		State& get_state() const {
			return _state;
		}

	private:
		ketl_value* _p_value;
		State& _state;

		template <class T>
		struct __ValueGetter;

		template <>
		struct __ValueGetter<int64_t> {
			static int64_t as(ketl_state* p_state, ketl_value* p_value) {
				return ketl_value_as_i64(p_state, p_value);
			}
		};

		friend State;
	};

	class State {
	public:

		State(const ketl_allocator* pAllocator)
			: _p_state(ketl_state_create(pAllocator)) {}

		State(const State& other) = delete;
		State(State&& other) = delete;

		~State() {
			ketl_state_destroy(_p_state);
		}

		State& operator=(const State& other) = delete;
		State& operator=(State&& other) = delete;

		template <class R, class... Args>
		void defineCFunction(const std::string_view& name, R (*pFunc)(Args...)) {
			ketl_type_parameter aParameters[] = {
				ketl_type_parameter{__TypeHelper<R>::getType(_p_state)}, (ketl_type_parameter{__TypeHelper<Args>::getType(_p_state)})...
			};
			ketl_function_parameters funcParameters = {
				aParameters, 1 + sizeof...(Args)
			};
			ketl_type* pFuncType = ketl_state_get_cfunction_type(_p_state, &funcParameters);
			ketl_state_define_function(_p_state, name.data(), static_cast<uint32_t>(name.length()), pFuncType, reinterpret_cast<void*>(pFunc));
		}

		template <class T>
		ketl_type* get_type() const {
			return __TypeHelper<T>::getType(_p_state);
		}

		Value eval(const std::string_view& filename, const std::string_view& source) {
			return Value{ ketl_state_eval(_p_state, filename.data(), source.data(), static_cast<uint32_t>(source.length())), *this };
		}

	private:
		ketl_state* _p_state;

		friend ketl_state* __get_state_impl(const State& state) {
			return state._p_state;
		}

		friend Value;
	};
}

std::ostream& operator<<(std::ostream& os, const KETL::Value& ketl_value)
{
	// TODO replace later to a to_string call or something
	if (ketl_value.get_type() == ketl_value.get_state().get_type<int64_t>()) {
		os << ketl_value.as<int64_t>();
	} else {
		KETL_ASSERT(false);
	}
    return os;
}

#endif // ketl_ketl_hpp
