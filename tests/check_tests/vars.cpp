/*🍲Ketl🍲*/
#include "check_tests.h"

static auto registerTests = []() {
	
	registerCheckTest("Creating var", []() {
		Ketl::VirtualMachine vm(4096);

		auto compilationResult = vm.eval(R"(
				var testValue2 = 1 + 2 * 3 + 4;
			)");

		if (std::holds_alternative<std::string>(compilationResult)) {
			return false;
		}

		auto resultPtr = vm.getVariable("testValue2").as<int64_t>();
		if (resultPtr == nullptr) {
			return false;
		}

		return *resultPtr == 11u;
		});

	registerCheckTest("Using existing var", []() {
		Ketl::VirtualMachine vm(4096);

		int64_t result = 0;
		vm.declareGlobal("testValue2", &result);

		auto compilationResult = vm.eval(R"(
				testValue2 = 1 + 2 * 3 + 4;
			)");


		if (std::holds_alternative<std::string>(compilationResult)) {
			return false;
		}

		auto resultPtr = vm.getVariable("testValue2").as<int64_t>();
		if (resultPtr != &result) {
			return false;
		}

		return result == 11u;
		});

	registerCheckTest("Creating var with error", []() {
		Ketl::VirtualMachine vm(4096);

		int64_t result = 0;
		vm.declareGlobal("testValue2", &result);

		auto compilationResult = vm.eval(R"(
				var testValue2 = 1 + 2 * 3 + 4;
			)");

		return std::holds_alternative<std::string>(compilationResult);
		});

	registerCheckTest("Using existing var with error", []() {
		Ketl::VirtualMachine vm(4096);

		auto compilationResult = vm.eval(R"(
				testValue2 = 1 + 2 * 3 + 4;
			)");

		return std::holds_alternative<std::string>(compilationResult);
		});

	registerCheckTest("Using local variable", []() {
		Ketl::VirtualMachine vm(4096);

		int64_t sum = 0;
		vm.declareGlobal("sum", &sum);

		auto compilationResult = vm.eval(R"(
			Int64 adder(Int64 x, Int64 y) {
				var sum = x + y;
				return sum;
			};

			sum = adder(5, 13);
		)");

		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return false;
		}

		return sum == 18u;
		});

	registerCheckTest("Using function type with no parameters variable", []() {
		Ketl::VirtualMachine vm(4096);

		int64_t sum = 0;
		vm.declareGlobal("sum", &sum);

		auto compilationResult = vm.eval(R"(
			Void() adder = () -> {
				sum = 7;
			};

			adder();
		)");

		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return false;
		}

		return sum == 7u;
		});

	registerCheckTest("Using function type variable", []() {
		Ketl::VirtualMachine vm(4096);

		int64_t sum = 0;
		vm.declareGlobal("sum", &sum);

		auto compilationResult = vm.eval(R"(
			Void(Int64, Int64) adder = (Int64 x, Int64 y) -> {
				sum = x + y;
			};

			adder(5, 13);
		)");

		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return false;
		}

		return sum == 18u;
		});

	return false;
	};

BEFORE_MAIN_ACTION(registerTests);
