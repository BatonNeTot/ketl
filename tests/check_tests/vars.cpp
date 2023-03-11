/*🍲Ketl🍲*/
#include "check_tests.h"

static auto registerTests = []() {
	
	registerTest("Creating var", []() {
		Ketl::Allocator allocator;
		Ketl::Context context(allocator, 4096);
		Ketl::Compiler compiler;

		auto compilationResult = compiler.compile(R"(
				var testValue2 = 1 + 2 * 3 + 4;
			)", context);

		if (std::holds_alternative<std::string>(compilationResult)) {
			return false;
		}


		auto& command = std::get<0>(compilationResult);

		{
			auto stackPtr = context._globalStack.allocate(command->stackSize());
			command->call(context._globalStack, stackPtr, nullptr);
			context._globalStack.deallocate(command->stackSize());
		}

		auto resultPtr = context.getVariable("testValue2").as<int64_t>();
		if (resultPtr == nullptr) {
			return false;
		}

		return *resultPtr == 11u;
		});

	registerTest("Using existing var", []() {
		Ketl::Allocator allocator;
		Ketl::Context context(allocator, 4096);
		Ketl::Compiler compiler;

		int64_t result = 0;
		auto& longType = *context.getVariable("Int64").as<Ketl::TypeObject>();
		context.declareGlobal("testValue2", &result, longType);

		auto compilationResult = compiler.compile(R"(
				testValue2 = 1 + 2 * 3 + 4;
			)", context);


		if (std::holds_alternative<std::string>(compilationResult)) {
			return false;
		}

		auto& command = std::get<0>(compilationResult);

		{
			auto stackPtr = context._globalStack.allocate(command->stackSize());
			command->call(context._globalStack, stackPtr, nullptr);
			context._globalStack.deallocate(command->stackSize());
		}

		auto resultPtr = context.getVariable("testValue2").as<int64_t>();
		if (resultPtr != &result) {
			return false;
		}

		return result == 11u;
		});

	registerTest("Creating var with error", []() {
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
		});

	registerTest("Using existing var with error", []() {
		Ketl::Allocator allocator;
		Ketl::Context context(allocator, 4096);
		Ketl::Compiler compiler;

		auto compilationResult = compiler.compile(R"(
				testValue2 = 1 + 2 * 3 + 4;
			)", context);

		return std::holds_alternative<std::string>(compilationResult);
		});

	registerTest("Using local variable", []() {
		Ketl::Allocator allocator;
		Ketl::Context context(allocator, 4096);
		Ketl::Compiler compiler;

		auto& longType = *context.getVariable("Int64").as<Ketl::TypeObject>();

		int64_t sum = 0;
		context.declareGlobal("sum", &sum, longType);

		auto compilationResult = compiler.compile(R"(
			Int64 adder(Int64 x, Int64 y) {
				var sum = x + y;
				return sum;
			};

			sum = adder(5, 13);
		)", context);

		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return false;
		}

		auto& command = std::get<0>(compilationResult);

		{
			auto stackPtr = context._globalStack.allocate(command->stackSize());
			command->call(context._globalStack, stackPtr, nullptr);
			context._globalStack.deallocate(command->stackSize());
		}

		return sum == 18u;
		});

	return false;
	};

BEFORE_MAIN_ACTION(registerTests);
