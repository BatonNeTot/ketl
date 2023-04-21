/*🍲Ketl🍲*/
#include "check_tests.h"

static auto registerTests = []() {

	registerCheckTest("Creating function and using in C", []() {
		Ketl::VirtualMachine vm(4096);

		int64_t sum = 0;
		vm.declareGlobal("sum", &sum);

		auto compilationResult = vm.eval(R"(
			var adder = () -> {
				sum = 11;
			};
		)");

		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return false;
		}

		auto adder = vm.getVariable("adder");
		if (adder.empty()) {
			return false;
		}

		adder();

		return sum == 11u;
		});

	registerCheckTest("Creating function and calling it", []() {
		Ketl::VirtualMachine vm(4096);

		int64_t sum = 0;
		vm.declareGlobal("sum", &sum);

		auto compilationResult = vm.eval(R"(
			var adder = () -> {
				sum = 11;
			};

			adder();
		)");

		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return false;
		}

		return sum == 11u;
		});

	registerCheckTest("Creating function and calling it with separate compilation", []() {
		Ketl::VirtualMachine vm(4096);

		int64_t sum = 0;
		vm.declareGlobal("sum", &sum);

		{
			auto compilationResult = vm.eval(R"(
			var adder = () -> {
				sum = 11;
			};
		)");

			if (std::holds_alternative<std::string>(compilationResult)) {
				std::cerr << std::get<std::string>(compilationResult) << std::endl;
				return false;
			}
		}

		{
			auto compilationResult = vm.eval(R"(
			adder();
		)");

			if (std::holds_alternative<std::string>(compilationResult)) {
				std::cerr << std::get<std::string>(compilationResult) << std::endl;
				return false;
			}
		}

		return sum == 11u;
		});

	registerCheckTest("Creating function with parameters and calling it", []() {
		Ketl::VirtualMachine vm(4096);

		int64_t sum = 0;
		vm.declareGlobal("sum", &sum);

		auto compilationResult = vm.eval(R"(
			var adder = (Int64 x, Int64 y) -> {
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

	registerCheckTest("Sugar creating function with parameters and calling it", []() {
		Ketl::VirtualMachine vm(4096);

		int64_t sum = 0;
		vm.declareGlobal("sum", &sum);

		auto compilationResult = vm.eval(R"(
			Void adder(Int64 x, Int64 y) {
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

	registerCheckTest("Calling function with return", []() {
		Ketl::VirtualMachine vm(4096);

		int64_t sum = 0;
		vm.declareGlobal("sum", &sum);

		auto compilationResult = vm.eval(R"(
			Int64 adder(Int64 x, Int64 y) {
				return x + y;
			};

			sum = adder(5, 13);
		)");

		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return false;
		}

		return sum == 18u;
		});

	registerCheckTest("Creating two function with same name and calling one", []() {
		Ketl::VirtualMachine vm(4096);

		int64_t sum = 0;
		vm.declareGlobal("sum", &sum);

		auto compilationResult = vm.eval(R"(
			Int64 adder(Int64 x, Int64 y) {
				var sum = x + y;
				return sum;
			};
	
			var adder(Int64 x) {
				return x + 20;
			};

			sum = adder(5, 10);
		)");

		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return false;
		}

		return sum == 15u;
		});

	registerCheckTest("Pseudo-currying function (currying and immediately calling with necessary arguments)", []() {
		Ketl::VirtualMachine vm(4096);

		int64_t sum = 0;
		vm.declareGlobal("sum", &sum);

		auto compilationResult = vm.eval(R"(
			Int64 adder(Int64 x, Int64 y) {
				var sum = x + y;
				return sum;
			};

			sum = adder(5)(10);
		)");

		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return false;
		}

		return sum == 15u;
		});

	registerCheckTest("Dot operator calling function", []() {
		Ketl::VirtualMachine vm(4096);

		int64_t sum = 0;
		vm.declareGlobal("sum", &sum);

		auto compilationResult = vm.eval(R"(
			Int64 adder(Int64 x, Int64 y) {
				return x + y;
			};

			sum = 5.adder(10);
		)");

		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return false;
		}

		return sum == 15u;
		});

	return false;
	};
	
BEFORE_MAIN_ACTION(registerTests);