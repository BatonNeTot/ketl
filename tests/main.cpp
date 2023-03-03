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

void testSpeed() {
	const uint64_t N = 1000000;

	Ketl::Allocator allocator;
	Ketl::Context context(allocator, 4096);
	Ketl::Compiler compiler;

	auto command = std::get<0>(compiler.compile(R"(
	testValue2 = 1 + 2;

	Float64 adder(Float64 x, Float64 y) {
		return x + y;
	}

	testValue = adder(testValue2, 9);
)", context));

	for (auto i = 0; i < N; ++i) {
		auto stackPtr = context._globalStack.allocate(command.stackSize());
		command.call(context._globalStack, stackPtr, nullptr);
		context._globalStack.deallocate(command.stackSize());
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


void launchTests() {
	std::vector<std::pair<std::string, std::function<bool()>>> tests = {
		{"Creating var", []() {
			Ketl::Allocator allocator;
			Ketl::Context context(allocator, 4096);
			Ketl::Compiler compiler;

			auto compilationResult = compiler.compile(R"(
				var testValue2 = 1 + 2 * 3 + 4;
			)", context);

			return !std::holds_alternative<std::string>(compilationResult);
	}},
		{"Using existing var", []() {
			Ketl::Allocator allocator;
			Ketl::Context context(allocator, 4096);
			Ketl::Compiler compiler;

			int64_t result = 0;
			auto& longType = *context.getVariable("Int64").as<Ketl::TypeObject>();
			context.declareGlobal("testValue2", &result, longType);

			auto compilationResult = compiler.compile(R"(
				testValue2 = 1 + 2 * 3 + 4;
			)", context);

			return !std::holds_alternative<std::string>(compilationResult);
	}},
		{"Creating var with error", []() {
			Ketl::Allocator allocator;
			Ketl::Context context(allocator, 4096);
			Ketl::Compiler compiler;

			int64_t result = 0;
			auto& longType = *context.getVariable("Int64").as<Ketl::TypeObject>();
			context.declareGlobal("testValue2", &result, longType);

			auto compilationResult = compiler.compile(R"(
				var testValue2 = 1 + 2 * 3 + 4;
			)", context);

			return std::holds_alternative<std::string>(compilationResult);
	}},
		{"Using existing var with error", []() {
			Ketl::Allocator allocator;
			Ketl::Context context(allocator, 4096);
			Ketl::Compiler compiler;

			auto compilationResult = compiler.compile(R"(
				testValue2 = 1 + 2 * 3 + 4;
			)", context);

			return std::holds_alternative<std::string>(compilationResult);
	}},
	};
	int passed = 0;

	std::cout << "Launching tests:" << std::endl;
	for (const auto& [name, test] : tests) {
		auto result = test();
		passed += result;
		auto status = result ? "SUCCEED" : "FAILED";
		std::cout << name << ": " << status << std::endl;
	}

	std::cout << "Tests passed: " << passed << "/" << tests.size() << std::endl;
}


int main(int argc, char** argv) {
	//*
	Ketl::Allocator allocator;
	Ketl::Context context(allocator, 4096);
	Ketl::Compiler compiler;

	auto& longType = *context.getVariable("Int64").as<Ketl::TypeObject>();

	int64_t result = 0;
	context.declareGlobal("testValue2", &result, longType);

	int64_t sum = 0;
	context.declareGlobal("sum", &sum, longType);

	auto compilationResult = compiler.compile(R"(
	testValue2 = 1 + 2 * 3 + 4;

	//Int64 adder(Int64 x, Int64 y) {
	var adder = (Int64 x, Int64 y) -> Int64 {
		sum = x + y;
		//var sum = x + y;
		//return sum;
	};
)", context);

	if (std::holds_alternative<std::string>(compilationResult)) {
		std::cerr << std::get<std::string>(compilationResult) << std::endl;
		return -1;
	}

	auto& command = std::get<0>(compilationResult);

	auto stackPtr = context._globalStack.allocate(command.stackSize());
	command.call(context._globalStack, stackPtr, nullptr);
	context._globalStack.deallocate(command.stackSize());

	assert(result == 11u);
	//*/
	launchTests();

	return 0;
}