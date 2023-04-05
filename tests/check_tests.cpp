/*🍲Ketl🍲*/
#include "check_tests.h"

#include <vector>

using CheckTestContainer = std::vector<std::pair<std::string_view, CheckTestFunction>>;

static CheckTestContainer& getTests() {
	static CheckTestContainer container;
	return container;
}

void registerCheckTest(std::string_view&& name, CheckTestFunction&& test) {
	getTests().emplace_back(std::move(name), std::move(test));
}

void launchCheckTests() {
	int passed = 0;

	std::cout << "Launching check tests:" << std::endl;
	for (const auto& [name, test] : getTests()) {
		auto result = test();
		passed += result;
		auto status = result ? "SUCCEED" : "FAILED";
		std::cout << name << ": " << status << std::endl;
	}

	std::cout << "Tests passed: " << passed << "/" << getTests().size() << std::endl;
}
