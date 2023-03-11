/*🍲Ketl🍲*/
#include "check_tests.h"

static auto registerTests = []() {

	registerTest("Creating function and using in C", []() {
		Ketl::Allocator allocator;
		Ketl::Context context(allocator, 4096);
		Ketl::Compiler compiler;

		auto& longType = *context.getVariable("Int64").as<Ketl::TypeObject>();

		int64_t sum = 0;
		context.declareGlobal("sum", &sum, longType);

		auto compilationResult = compiler.compile(R"(
			var adder = () -> {
				sum = 11;
			};
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

		auto adderPtr = *context.getVariable("adder").as<Ketl::FunctionImpl*>();
		if (adderPtr == nullptr) {
			return false;
		}

		auto& adder = *adderPtr;

		{
			auto stackPtr = context._globalStack.allocate(adder.stackSize());
			adder.call(context._globalStack, stackPtr, nullptr);
			context._globalStack.deallocate(adder.stackSize());
		}

		return sum == 11u;
		});

	registerTest("Creating function and calling it", []() {
		Ketl::Allocator allocator;
		Ketl::Context context(allocator, 4096);
		Ketl::Compiler compiler;

		auto& longType = *context.getVariable("Int64").as<Ketl::TypeObject>();

		int64_t sum = 0;
		context.declareGlobal("sum", &sum, longType);

		auto compilationResult = compiler.compile(R"(
			var adder = () -> {
				sum = 11;
			};

			adder();
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

		return sum == 11u;
		});

	registerTest("Creating function and calling it with separate compilation", []() {
		Ketl::Allocator allocator;
		Ketl::Context context(allocator, 4096);
		Ketl::Compiler compiler;

		auto& longType = *context.getVariable("Int64").as<Ketl::TypeObject>();

		int64_t sum = 0;
		context.declareGlobal("sum", &sum, longType);

		{
			auto compilationResult = compiler.compile(R"(
			var adder = () -> {
				sum = 11;
			};
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
		}

		{
			auto compilationResult = compiler.compile(R"(
			adder();
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
		}

		return sum == 11u;
		});

	registerTest("Creating function with parameters and calling it", []() {
		Ketl::Allocator allocator;
		Ketl::Context context(allocator, 4096);
		Ketl::Compiler compiler;

		auto& longType = *context.getVariable("Int64").as<Ketl::TypeObject>();

		int64_t sum = 0;
		context.declareGlobal("sum", &sum, longType);

		auto compilationResult = compiler.compile(R"(
			var adder = (Int64 x, Int64 y) -> {
				sum = x + y;
			};

			adder(5, 13);
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

	registerTest("Sugar creating function with parameters and calling it", []() {
		Ketl::Allocator allocator;
		Ketl::Context context(allocator, 4096);
		Ketl::Compiler compiler;

		auto& longType = *context.getVariable("Int64").as<Ketl::TypeObject>();

		int64_t sum = 0;
		context.declareGlobal("sum", &sum, longType);

		auto compilationResult = compiler.compile(R"(
			Void adder(Int64 x, Int64 y) {
				sum = x + y;
			};

			adder(5, 13);
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

	registerTest("Calling function with return", []() {
		Ketl::Allocator allocator;
		Ketl::Context context(allocator, 4096);
		Ketl::Compiler compiler;

		auto& longType = *context.getVariable("Int64").as<Ketl::TypeObject>();

		int64_t sum = 0;
		context.declareGlobal("sum", &sum, longType);

		auto compilationResult = compiler.compile(R"(
			Int64 adder(Int64 x, Int64 y) {
				return x + y;
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