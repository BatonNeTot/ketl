/*🍲Ketl🍲*/
#include <iostream>
#include <vector>
#include <sstream>
#include <exception>
#include <functional>
#include <cassert>
#include <typeindex>

#include "compiler/compiler.h"
#include "ketl.h"
#include "context.h"
#include "type.h"

#include "check_tests.h"
#include "speed_tests.h"

int main(int argc, char** argv) {
	launchCheckTests();
	launchSpeedTests(10000000);

	Ketl::Allocator allocator;
	Ketl::Context context(allocator, 4096);
	Ketl::Compiler compiler;

	int64_t sum = 0;
	context.declareGlobal("sum", &sum);

	auto compilationResult = compiler.compile(R"(
	sum = 0;

	while (sum != 3) {
		sum = sum + 1;
	} else {
		sum = 7;
	}
)", context);

	if (std::holds_alternative<std::string>(compilationResult)) {
		std::cerr << std::get<std::string>(compilationResult) << std::endl;
		return -1;
	}

	auto& command = std::get<0>(compilationResult);
	command();

	assert(sum == 3);

	return 0;
}