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

double testValue = 0;
double testValue2 = 0;

void test(double x) {
	testValue2 = x + 1;
}

void testLanguages() {
	const uint64_t N = 1000000;

	Ketl::Allocator allocator;
	Ketl::Context context(allocator, 4096);
	Ketl::Compiler compiler;

	auto command = compiler.compile(R"(
	testValue2 = 1 + 2;

	Float64 adder(Float64 x, Float64 y) {
		return x + y;
	}

	testValue = adder(testValue2, 9);
)", context);

	test(0);

	for (auto i = 0; i < N; ++i) {
		auto stackPtr = context._globalStack.allocate(command.stackSize());
		command.call(context._globalStack, stackPtr, nullptr);
		context._globalStack.deallocate(command.stackSize());
	}

	test(0);
	lua_State* L;
	L = luaL_newstate();

	luaL_loadstring(L, R"(
	testValue2 = 1 + 2

	function test(x, y)
		return x + y
	end

	testValue = test(testValue2, 9)
)");

	test(0);

	for (auto i = 0; i < N; ++i) {
		lua_pushvalue(L, -1);
		lua_call(L, 0, 0);
	}

	test(0);
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

Ketl::Argument literal(uint64_t value) {
	Ketl::Argument argument;
	argument.integer = value;
	return argument;
}

Ketl::Argument stack(uint64_t offset) {
	Ketl::Argument argument;
	argument.stack = offset;
	return argument;
}

Ketl::Argument pointer(void* ptr) {
	Ketl::Argument argument;
	argument.pointer = ptr;
	return argument;
}

int main(int argc, char** argv) {
	Ketl::Allocator allocator;
	Ketl::Context context(allocator, 4096);
	Ketl::Compiler compiler;

	int64_t result = 0;

	auto& longType = *context.getVariable("Int64").as<Ketl::TypeObject>();

	context.getVariable("testValue2").as<int64_t>();
	context.declareGlobal("testValue2", &result, longType);

	//*
	auto command = compiler.compile(R"(
	var testValue2 = 1 + 2 * 3 + 4;
)", context);

	//*/
	
	/*
	auto command = compiler.compile(R"(
	testValue2 = 1 + 2;

	Float64 adder(Float64 x, Float64 y) {
		return x + y;
	}

	testValue = adder(testValue2, 9);
)", context);
	*/

	auto stackPtr = context._globalStack.allocate(command.stackSize());
	command.call(context._globalStack, stackPtr, nullptr);
	context._globalStack.deallocate(command.stackSize());

	assert(result == 11u);

	/*
	// TODO thats nonsense
	auto doubleTypeVar = env._context.getVariable("Float64");
	std::unique_ptr<const Ketl::Type> doubleType = std::make_unique<Ketl::BasicType>(doubleTypeVar.as<Ketl::BasicTypeBody>(), true, false, true);
	auto& outofnowhere = *env._context.declareGlobal<double>("outofnowhere", doubleType);
	outofnowhere = 14.;

	auto command = linker.proceedStandalone(env, R"(
	Float64 testValue {outofnowhere + 2};
	testValue = (testValue2 = testValue + 6) + (7 + 8);

	Float64 adder(const Float64&& x) {
		return testValue2 + x;
	}

	Void inc() {
		testValue2 = testValue2 + 1;
	}

	testValue2 = adder(5);
)");

	if (!command) {
		std::cerr << linker.errorMsg() << std::endl;
		assert(false);
	}

	auto& testTestF = *env._context.getVariable("testValue").as<double>();
	auto& testTest2F = *env._context.getVariable("testValue2").as<double>();

	auto inc = env._context.getVariable("inc");

	command.invoke(env._context._globalStack);

	inc();

	test(0);

	assert(testTestF == 37.);
	assert(testTest2F == 28.);
	*/
	//getc(stdin);
	return 0;
}