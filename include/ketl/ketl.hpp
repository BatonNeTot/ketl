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
	class Type;
	class Field;
	class Value;

	ketl_state* __get_state_impl(const State& state);
	
	namespace {
		template <class T>
		struct __TypeHelper;

		template <>
		struct __TypeHelper<void*> {
			static ketl_type* getType(ketl_state* pState) {
				return ketl_state_get_raw_type(pState);
			}
		};

		template <>
		struct __TypeHelper<int8_t> {
			static ketl_type* getType(ketl_state* pState) {
				return ketl_state_get_i8(pState);
			}
		};

		template <>
		struct __TypeHelper<int16_t> {
			static ketl_type* getType(ketl_state* pState) {
				return ketl_state_get_i16(pState);
			}
		};

		template <>
		struct __TypeHelper<int32_t> {
			static ketl_type* getType(ketl_state* pState) {
				return ketl_state_get_i32(pState);
			}
		};

		template <>
		struct __TypeHelper<int64_t> {
			static ketl_type* getType(ketl_state* pState) {
				return ketl_state_get_i64(pState);
			}
		};
	
		template <class T>
		struct __ValueGetter;

		template <>
		struct __ValueGetter<void*> {
			static void* as(ketl_state* p_state, ketl_value* p_value) {
				return ketl_value_as_raw(p_state, p_value);
			}
		};

		template <>
		struct __ValueGetter<int8_t> {
			static int8_t as(ketl_state* p_state, ketl_value* p_value) {
				return ketl_value_as_i8(p_state, p_value);
			}
		};

		template <>
		struct __ValueGetter<int16_t> {
			static int16_t as(ketl_state* p_state, ketl_value* p_value) {
				return ketl_value_as_i16(p_state, p_value);
			}
		};

		template <>
		struct __ValueGetter<int32_t> {
			static int32_t as(ketl_state* p_state, ketl_value* p_value) {
				return ketl_value_as_i32(p_state, p_value);
			}
		};

		template <>
		struct __ValueGetter<int64_t> {
			static int64_t as(ketl_state* p_state, ketl_value* p_value) {
				return ketl_value_as_i64(p_state, p_value);
			}
		};

	}

	class Type {
	private:
		Type(ketl_type* p_type, const State& state)
			: _p_type(p_type), _state(state) {}

		Type(const Type& other)
			: _p_type(other._p_type), _state(other._state) {}

	public:
		~Type() = default;

		size_t get_size() const {
			return ketl_type_get_size(_p_type);
		}

		bool is_array() const {
			return ketl_type_is_array(_p_type);
		}

		friend bool operator==(const Type& lhs, const Type& rhs) {
			return lhs._p_type == rhs._p_type;
		}

		const State& get_state() const {
			return _state;
		}

	private:
		ketl_type* _p_type;
		const State& _state;
		friend State;
		friend Value;
	};

	class Field {
	private:
		Field(ketl_type* p_type, const std::string_view& name, const State& state)
			: _p_type(p_type), _state(state), _name(name) {}

		Field(const Field& other)
			: _p_type(other._p_type), _state(other._state), _name(other._name) {}

	public:
		~Field() = default;

		const State& get_state() const {
			return _state;
		}

	private:
		ketl_type* _p_type;
		const State& _state;
		std::string _name;

		friend State;
	};

	class Value {
	private:
		Value(ketl_value* p_value, const State& state)
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

		Type get_type() const {
			return { ketl_value_get_type(__get_state_impl(_state), _p_value), _state };
		}

		template<class T>
		T as() const {
			return __ValueGetter<T>::as(__get_state_impl(_state), _p_value);
		}

		uint64_t get_array_size() const {
			return ketl_value_get_array_size(__get_state_impl(_state), _p_value);
		}

		Value operator[](int64_t index) const {
			return Value{ ketl_value_index(__get_state_impl(_state), _p_value, index), _state };
		}

		const State& get_state() const {
			return _state;
		}

	private:
		ketl_value* _p_value;
		const State& _state;
		friend State;
	};

	class State {
	public:

		State(const ketl_allocator* p_allocator)
			: _p_state(ketl_state_create(p_allocator)) {}

		State(const State& other) = delete;
		State(State&& other) = delete;

		~State() {
			ketl_state_destroy(_p_state);
		}

		State& operator=(const State& other) = delete;
		State& operator=(State&& other) = delete;

		template <class R, class... Args>
		void define_cfunction(const std::string_view& name, R (*pFunc)(Args...)) {
			ketl_variable_type_info_t aParameters[] = {
				ketl_variable_type_info_t{__TypeHelper<R>::getType(_p_state)}, (ketl_variable_type_info_t{__TypeHelper<Args>::getType(_p_state)})...
			};
			ketl_function_parameters funcParameters = {
				aParameters, 1 + sizeof...(Args)
			};
			ketl_type* pFuncType = ketl_state_get_cfunction_type(_p_state, &funcParameters);
			ketl_state_define_function(_p_state, name.data(), static_cast<uint32_t>(name.length()), pFuncType, reinterpret_cast<void*>(pFunc));
		}

		template <class... Fields>
		void define_class(const std::string_view& name, Fields&&... fields) {
			ketl_named_variable_type_info_t a_class_fields[] = {
				ketl_named_variable_type_info_t{fields._p_type, fields._name.c_str(), static_cast<uint32_t>(fields._name.length())}...
			};
			ketl_state_define_class(_p_state, name.data(), static_cast<uint32_t>(name.length()), a_class_fields, sizeof...(fields));
		}

		template <class T>
		Type get_type() const {
			return { __TypeHelper<T>::getType(_p_state), *this };
		}

		template <class T>
		Field create_field(const std::string_view& name) const {
			return { __TypeHelper<T>::getType(_p_state), name, *this };
		}

		Value eval(const std::string_view& source) {
			return Value{ ketl_state_eval(_p_state, source.data(), static_cast<uint32_t>(source.length())), *this };
		}

	private:
		ketl_state* _p_state;

		friend ketl_state* __get_state_impl(const State& state) {
			return state._p_state;
		}

		friend Field;
		friend Value;
	};
}

std::ostream& operator<<(std::ostream& os, const KETL::Value& ketl_value)
{
	// TODO replace later to a to_string call or something
	if (ketl_value.get_type() == ketl_value.get_state().get_type<void*>()) {
		os << "0x" << ketl_value.as<void*>();
	} else if (ketl_value.get_type() == ketl_value.get_state().get_type<int8_t>()) {
		os << (int64_t)ketl_value.as<int8_t>();
	} else if (ketl_value.get_type() == ketl_value.get_state().get_type<int16_t>()) {
		os << ketl_value.as<int16_t>();
	} else if (ketl_value.get_type() == ketl_value.get_state().get_type<int32_t>()) {
		os << ketl_value.as<int32_t>();
	} else if (ketl_value.get_type() == ketl_value.get_state().get_type<int64_t>()) {
		os << ketl_value.as<int64_t>();
	} else if (ketl_value.get_type().is_array()) {
		int64_t size = static_cast<int64_t>(ketl_value.get_array_size());
		if (size == 0) {
			os << "{}";
		} else {
			os << "{ ";
			for (int64_t i = 0; i < size;) {
				os << ketl_value[i++];
				if (i < size) {
					os << ", ";
				}
			}
			os << " }";
		}
	} else {
		ANN_ASSERT(false);
	}
    return os;
}

#endif // ketl_ketl_hpp
