/*🍲Ketl🍲*/
#include "speed_tests.h"

#include <vector>

using CheckTestContainer = std::vector<std::pair<std::string_view, SpeedTestFunction>>;

static CheckTestContainer& getTests() {
	static CheckTestContainer container;
	return container;
}

void registerSpeedTest(std::string_view&& name, SpeedTestFunction&& test) {
	getTests().emplace_back(std::move(name), std::move(test));
}

void launchSpeedTests(uint64_t N) {
	std::cout << "Launching speed tests with 'N = " << N << "':" << std::endl;

	double averageRatio = 0;

	for (const auto& [name, test] : getTests()) {
		auto ketlTime = std::numeric_limits<double>::max();
		auto luaTime = std::numeric_limits<double>::max();
		test(N, ketlTime, luaTime);
		auto ratio = luaTime / ketlTime;
		std::cout << name << ": Lua = " << luaTime << ", Ketl = " << ketlTime << ", ration = " << ratio << std::endl;
		averageRatio += ratio;
	}

	std::cout << "Average of ratios: " << averageRatio / getTests().size() << std::endl;
}
