/*🍲Ketl🍲*/
#include "check_tests.h"

static TestContainer& getTests() {
	static TestContainer container;
	return container;
}

void registerTest(std::string_view&& name, std::function<bool()>&& test) {
	getTests().emplace_back(std::move(name), std::move(test));
}

void launchCheckTests() {
	int passed = 0;

	std::cout << "Launching tests:" << std::endl;
	for (const auto& [name, test] : getTests()) {
		auto result = test();
		passed += result;
		auto status = result ? "SUCCEED" : "FAILED";
		std::cout << name << ": " << status << std::endl;
	}

	std::cout << "Tests passed: " << passed << "/" << getTests().size() << std::endl;
}
