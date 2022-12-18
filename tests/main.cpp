/*🍲Ketl🍲*/
#include <iostream>
#include <vector>
#include <chrono>
#include <sstream>
#include <exception>
#include <functional>
#include <cassert>
#include <typeindex>

#include "compiler/parser.h"
#include "compiler/linker.h"
#include "ketl.h"

#include "lua.hpp"

double testValue = 0;
double testValue2 = 0;

void test(double x) {
	testValue2 = x + 1;
}

void testLanguages() {
	Ketl::Environment env;
	Ketl::Linker linker;

	/*
	auto command = linker.proceedStandalone(env, R"(
	test(test(5));
)");
//*/
//*
	auto command = linker.proceedStandalone(env, R"(
	testValue2 = 1 + 2;

	Float64 adder(Float64 x, Float64 y) {
		return x + y;
	}

	testValue = adder(testValue2, 9);
)");
	//*/
		/*
		auto command = linker.proceedStandalone(env, R"(
		testValue2 = testValue = 1 + 2;

		Float test(Float x, Float y) {
			testValue2 = x + y + 1;
		}
	)");
	//*/
	/*
	auto command = linker.proceedStandalone(env, R"(
	testValue2 = testValue = 1 + 2;
	testValue2 = testValue = 1 + 2;
)");
//*/
/*
auto command = linker.proceedStandalone(env, R"(
Float test(Float x) {
	testValue2 = x + 1;
}
)");
//*/

	auto& testTestF = *env.getGlobal<double>("testValue");
	auto& testTest2F = *env.getGlobal<double>("testValue2");

	for (auto i = 0; i < 1000000; ++i) {
		command.invoke(command.stack());
	}

	test(0);
	lua_State* L;
	L = luaL_newstate();

	test(0);

	for (auto i = 0; i < 1000000; ++i) {
		//lua_pushvalue(L, -1);

		luaL_loadstring(L, R"(
	testValue2 = 1 + 2

	function test(x)
		return testValue2 = x + 1
	end

	testValue = test(5)
)");
		lua_call(L, 0, 0);
	}

	test(0);

	for (auto i = 0; i < 1000000; ++i) {
		testValue2 = testValue = 1. + 2.;

		test(5.);
	}
}

template <class T>
void insert(std::vector<uint8_t>& bytes, const T& value) {
	for (auto i = 0u; i < sizeof(T); ++i) {
		bytes.emplace_back();
	}
	*reinterpret_cast<T*>(bytes.data() + bytes.size() - sizeof(T)) = value;
}

void insert(std::vector <uint8_t>& bytes, const char* str) {
	while (*str != '\0') {
		bytes.emplace_back(*str);
		++str;
	}
	bytes.emplace_back('\0');
}

int main(int argc, char** argv) {

	Ketl::Environment env;
	Ketl::Linker linker;

	auto command = linker.proceedStandalone(env, R"(
	Float64 testValue2{1 + 2};
	//testValue2 = 5 + 6;

	Void adder() {
		testValue2 = 5 + 1;
	}
)");
	

	auto& testTestF = *env.getGlobal<double>("testValue");
	auto& testTest2F = *env.getGlobal<double>("testValue2");

	command.invoke(command.stack());

	auto* function = env.getGlobal<Ketl::Function>("adder");
	auto* pureFunction = function->functions;

	auto stackPtr = command.stack().allocate(pureFunction->stackSize());
	pureFunction->call(command.stack(), stackPtr, nullptr);
	command.stack().deallocate(pureFunction->stackSize());

	test(0);
	//getc(stdin);
	return 0;
}