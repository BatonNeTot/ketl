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

	Float64 adder(Float64 x, Float64 y) {
		return x + y;
	}

	testValue = adder(testValue2, 9);
)", context));

	for (auto i = 0; i < N; ++i) {
		auto stackPtr = context._globalStack.allocate(command->stackSize());
		command->call(context._globalStack, stackPtr, nullptr);
		context._globalStack.deallocate(command->stackSize());
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
	//*
	Ketl::Allocator allocator;
	Ketl::Context context(allocator, 4096);
	Ketl::Compiler compiler;

	auto& longType = *context.getVariable("Int64").as<Ketl::TypeObject>();

	int64_t sum = 0;
	context.declareGlobal("sum", &sum, longType);

	auto compilationResult = compiler.compile(R"(
	struct Test {
	public:
		Int64 x, y;
		public Int64 z;
	}

	var adder(Int64 x, Int64 y) {
		var sum = x + y;
		return sum;
	};

	sum = adder(5, 10);

)", context);

	if (std::holds_alternative<std::string>(compilationResult)) {
		std::cerr << std::get<std::string>(compilationResult) << std::endl;
		return -1;
	}

	auto& command = std::get<0>(compilationResult);

	{
		auto stackPtr = context._globalStack.allocate(command->stackSize());
		command->call(context._globalStack, stackPtr, nullptr);
		context._globalStack.deallocate(command->stackSize());
	}

	assert(sum == 15u);

	//*/

	launchCheckTests();

	return 0;
}