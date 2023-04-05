/*🍲Ketl🍲*/
#include "check_tests.h"

static auto registerTests = []() {

	registerCheckTest("Creating function and using in C", []() {
		Ketl::Allocator allocator;
		Ketl::Context context(allocator, 4096);
		Ketl::Compiler compiler;

		int64_t sum = 0;
		context.declareGlobal("sum", &sum);

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
		command();

		auto adder = context.getVariable("adder");
		if (adder.empty()) {
			return false;
		}

		adder();

		return sum == 11u;
		});

	registerCheckTest("Creating function and calling it", []() {
		Ketl::Allocator allocator;
		Ketl::Context context(allocator, 4096);
		Ketl::Compiler compiler;

		int64_t sum = 0;
		context.declareGlobal("sum", &sum);

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

		auto command = std::get<0>(compilationResult);
		command();

		return sum == 11u;
		});

	registerCheckTest("Creating function and calling it with separate compilation", []() {
		Ketl::Allocator allocator;
		Ketl::Context context(allocator, 4096);
		Ketl::Compiler compiler;

		int64_t sum = 0;
		context.declareGlobal("sum", &sum);

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
			command();
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
			command();
		}

		return sum == 11u;
		});

	registerCheckTest("Creating function with parameters and calling it", []() {
		Ketl::Allocator allocator;
		Ketl::Context context(allocator, 4096);
		Ketl::Compiler compiler;

		int64_t sum = 0;
		context.declareGlobal("sum", &sum);

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
		command();

		return sum == 18u;
		});

	registerCheckTest("Sugar creating function with parameters and calling it", []() {
		Ketl::Allocator allocator;
		Ketl::Context context(allocator, 4096);
		Ketl::Compiler compiler;

		int64_t sum = 0;
		context.declareGlobal("sum", &sum);

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
		command();

		return sum == 18u;
		});

	registerCheckTest("Calling function with return", []() {
		Ketl::Allocator allocator;
		Ketl::Context context(allocator, 4096);
		Ketl::Compiler compiler;

		int64_t sum = 0;
		context.declareGlobal("sum", &sum);

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
		command();

		return sum == 18u;
		});

	registerCheckTest("Creating two function with same name and calling one", []() {
		Ketl::Allocator allocator;
		Ketl::Context context(allocator, 4096);
		Ketl::Compiler compiler;

		int64_t sum = 0;
		context.declareGlobal("sum", &sum);

		auto compilationResult = compiler.compile(R"(
			Int64 adder(Int64 x, Int64 y) {
				var sum = x + y;
				return sum;
			};
	
			var adder(Int64 x) {
				return x + 20;
			};

			sum = adder(5, 10);
		)", context);

		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return false;
		}

		auto& command = std::get<0>(compilationResult);
		command();

		return sum == 15u;
		});

	registerCheckTest("Pseudo-currying function (currying and immediately calling with necessary arguments)", []() {
		Ketl::Allocator allocator;
		Ketl::Context context(allocator, 4096);
		Ketl::Compiler compiler;

		int64_t sum = 0;
		context.declareGlobal("sum", &sum);

		auto compilationResult = compiler.compile(R"(
			Int64 adder(Int64 x, Int64 y) {
				var sum = x + y;
				return sum;
			};

			sum = adder(5)(10);
		)", context);

		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return false;
		}

		auto& command = std::get<0>(compilationResult);
		command();

		return sum == 15u;
		});

	registerCheckTest("Dot operator calling function", []() {
		Ketl::Allocator allocator;
		Ketl::Context context(allocator, 4096);
		Ketl::Compiler compiler;

		int64_t sum = 0;
		context.declareGlobal("sum", &sum);

		auto compilationResult = compiler.compile(R"(
			Int64 adder(Int64 x, Int64 y) {
				return x + y;
			};

			sum = 5.adder(10);
		)", context);

		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return false;
		}

		auto& command = std::get<0>(compilationResult);
		command();

		return sum == 15u;
		});

	return false;
	};
	
BEFORE_MAIN_ACTION(registerTests);