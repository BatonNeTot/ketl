/*🍲Ketl🍲*/
#include <iostream>
#include <vector>
#include <sstream>
#include <exception>
#include <functional>
#include <cassert>
#include <typeindex>

#include "ketl.h"

#include "check_tests.h"
#include "speed_tests.h"

int main(int argc, char** argv) {
	launchCheckTests();
	launchSpeedTests(10000000);

	Ketl::VirtualMachine vm(4096);

	int64_t sum = 0;
	vm.declareGlobal("sum", &sum);

	auto compilationResult = vm.compile(R"(
	sum = 0;

	while (sum != 3) {
		sum = sum + 1;
	} else {
		sum = 7;
	}
)");

	if (std::holds_alternative<std::string>(compilationResult)) {
		std::cerr << std::get<std::string>(compilationResult) << std::endl;
		return -1;
	}

	auto& command = std::get<0>(compilationResult);
	command();

	assert(sum == 3);

	return 0;
}