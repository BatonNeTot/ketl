/*🍲Ketl🍲*/
#include <iostream>
#include <vector>
#include <chrono>
#include <sstream>
#include <exception>
#include <functional>
#include <cassert>
#include <typeindex>

#include "compiler/compiler.h"
#include "ketl.h"
#include "context.h"
#include "type.h"

#include "lua.hpp"

#include "check_tests.h"

void testSpeed() {
	const uint64_t N = 1000000;

	Ketl::Allocator allocator;
	Ketl::Context context(allocator, 4096);
	Ketl::Compiler compiler;

	auto command = std::get<0>(compiler.compile(R"(
	var testValue2 = 1 + 2;

	Int64 adder(in Int64 x, in Int64 y) {
		return x + y;
	}

	testValue = adder(testValue2, 9);
)", context));

	for (auto i = 0; i < N; ++i) {
		command();
	}

	lua_State* L;
	L = luaL_newstate();

	luaL_loadstring(L, R"(
	testValue2 = 1 + 2

	function test(x, y)
		return x + y
	end

	testValue = test(testValue2, 9)
)");


	for (auto i = 0; i < N; ++i) {
		lua_pushvalue(L, -1);
		lua_call(L, 0, 0);
	}
}


int main(int argc, char** argv) {
	launchCheckTests();

	Ketl::Allocator allocator;
	Ketl::Context context(allocator, 4096);
	Ketl::Compiler compiler;

	int64_t sum = 0;
	context.declareGlobal("sum", &sum);

	auto compilationResult = compiler.compile(R"(
	Int64 adder(Int64 x) {
		return x + 10;
	}

	/*
	struct Test {
	public:
		Int64 x, y;
		public Int64 z;
	}

	Test test;
	test.x = 5;
	sum = test.x.(adder)();
	*/

	sum = 3.adder();

)", context);

	if (std::holds_alternative<std::string>(compilationResult)) {
		std::cerr << std::get<std::string>(compilationResult) << std::endl;
		return -1;
	}

	auto& command = std::get<0>(compilationResult);
	command();

	assert(sum == 13u);

	return 0;
}