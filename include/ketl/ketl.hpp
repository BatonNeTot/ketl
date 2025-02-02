//🫖ketl
#ifndef ketl_ketl_hpp
#define ketl_ketl_hpp

extern "C" {
#include "ketl/ketl.h"
#include "ketl/function.h"
}

#include <iostream>

namespace KETL {

	class State {
	public:

		State(ketl_allocator* allocator)
			: _stateImpl(ketl_state_create(allocator)) {}

		~State() {
			ketl_state_destroy(_stateImpl);
		}
		

	private:
		ketl_state* _stateImpl;
	};

}

#endif // ketl_ketl_hpp
