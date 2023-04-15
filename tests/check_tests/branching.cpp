/*🍲Ketl🍲*/
#include "check_tests.h"

static auto registerTests = []() {

	registerCheckTest("If else statement true", []() {
		Ketl::VirtualMachine vm(4096);

		int64_t sum = 0;
		vm.declareGlobal("sum", &sum);

		auto compilationResult = vm.compile(R"(
			sum = 3;

			if (sum == 3) {
				sum = 5;
			} else {
				sum = 7;
			}
		)");

		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return false;
		}

		auto& command = std::get<0>(compilationResult);
		command();

		return sum == 5;
		});

	registerCheckTest("If else statement false", []() {
		Ketl::VirtualMachine vm(4096);

		int64_t sum = 0;
		vm.declareGlobal("sum", &sum);

		auto compilationResult = vm.compile(R"(
			sum = 4;

			if (sum == 3) {
				sum = 5;
			} else {
				sum = 7;
			}
		)");

		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return false;
		}

		auto& command = std::get<0>(compilationResult);
		command();

		return sum == 7;
		});

	registerCheckTest("Single if statement true", []() {
		Ketl::VirtualMachine vm(4096);

		int64_t sum = 0;
		vm.declareGlobal("sum", &sum);

		auto compilationResult = vm.compile(R"(
			sum = 3;

			if (sum == 3) {
				sum = 5;
			}
		)");

		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return false;
		}

		auto& command = std::get<0>(compilationResult);
		command();

		return sum == 5;
		});

	registerCheckTest("Empty if statement false", []() {
		Ketl::VirtualMachine vm(4096);

		int64_t sum = 0;
		vm.declareGlobal("sum", &sum);

		auto compilationResult = vm.compile(R"(
			sum = 3;

			if (sum == 3) {} else {
				sum = 5;
			}
		)");

		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return false;
		}

		auto& command = std::get<0>(compilationResult);
		command();

		return sum == 3;
		});

	registerCheckTest("Empty if statement true", []() {
		Ketl::VirtualMachine vm(4096);

		int64_t sum = 0;
		vm.declareGlobal("sum", &sum);

		auto compilationResult = vm.compile(R"(
			sum = 4;

			if (sum == 3) {} else {
				sum = 5;
			}
		)");

		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return false;
		}

		auto& command = std::get<0>(compilationResult);
		command();

		return sum == 5;
		});

	registerCheckTest("Multiple if statement layers", []() {
		Ketl::VirtualMachine vm(4096);

		int64_t sum = 0;
		vm.declareGlobal("sum", &sum);

		auto compilationResult = vm.compile(R"(
			sum = 0;
			var first = 3;
			var second = 4;

			if (first == 3) 
				if (second == 4)
					sum = 4;
		)");

		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return false;
		}

		auto& command = std::get<0>(compilationResult);
		command();

		return sum == 4;
		});

	registerCheckTest("Simple while statement", []() {
		Ketl::VirtualMachine vm(4096);

		int64_t sum = 0;
		vm.declareGlobal("sum", &sum);

		auto compilationResult = vm.compile(R"(
			sum = 0;

			while (sum != 3) {
				sum = sum + 1;
			}
		)");

		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return false;
		}

		auto& command = std::get<0>(compilationResult);
		command();

		return sum == 3;
		});

	registerCheckTest("Simple while statement (no loop)", []() {
		Ketl::VirtualMachine vm(4096);

		int64_t sum = 0;
		vm.declareGlobal("sum", &sum);
		int64_t test = 0;
		vm.declareGlobal("test", &test);

		auto compilationResult = vm.compile(R"(
			sum = 3;
			test = 0;

			while (sum != 3) {
				sum = sum + 1;
				test = 1;
			}
		)");

		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return false;
		}

		auto& command = std::get<0>(compilationResult);
		command();

		return sum == 3 && test == 0;
		});

	registerCheckTest("While else statement (else missed)", []() {
		Ketl::VirtualMachine vm(4096);

		int64_t sum = 0;
		vm.declareGlobal("sum", &sum);
		int64_t test = 0;
		vm.declareGlobal("test", &test);

		auto compilationResult = vm.compile(R"(
			sum = 0;
			test = 0;

			while (sum != 3) {
				sum = sum + 1;
				test = 1;
			} else {
				sum = 7;
				test = 2;
			}
		)");

		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return false;
		}

		auto& command = std::get<0>(compilationResult);
		command();

		return sum == 3 && test == 1;
		});

	registerCheckTest("While else statement (else called)", []() {
		Ketl::VirtualMachine vm(4096);

		int64_t sum = 0;
		vm.declareGlobal("sum", &sum);
		int64_t test = 0;
		vm.declareGlobal("test", &test);

		auto compilationResult = vm.compile(R"(
			sum = 3;
			test = 0;

			while (sum != 3) {
				sum = sum + 1;
				test = 1;
			} else {
				sum = 7;
				test = 2;
			}
		)");

		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return false;
		}

		auto& command = std::get<0>(compilationResult);
		command();

		return sum == 7 && test == 2;
		});

	return false;
	};
	
BEFORE_MAIN_ACTION(registerTests);