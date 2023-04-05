/*🍲Ketl🍲*/
#include "speed_tests.h"

static auto registerTests = []() {

	registerSpeedTest("Random if statement", [](uint64_t N, double& ketlTime, double& luaTime) {
		auto randValue = []() {
			return (rand() >> 1) % 2;
		};

		Ketl::Allocator allocator;
		Ketl::Context context(allocator, 4096);
		Ketl::Compiler compiler;

		int64_t value = 0u;
		context.declareGlobal("value", &value);

		auto compilationResult = compiler.compile(R"(
			var testValue = 0;

			if (value) {
				testValue = 1;
			} else {
				testValue = 2;
			}
		)", context);

		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return;
		}

		auto& command = std::get<0>(compilationResult);
		{
			auto start = std::chrono::high_resolution_clock::now();
			for (auto i = 0; i < N; ++i) {
				value = randValue();
				command();
			}
			auto finish = std::chrono::high_resolution_clock::now();
			ketlTime = std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count() / 1000000.0;
		}

		lua_State* L;
		L = luaL_newstate();

		luaL_loadstring(L, R"(
			local testValue = 0

			if value then
				testValue = 1
			else
				testValue = 2
			end
		)");


		{
			auto start = std::chrono::high_resolution_clock::now();
			for (auto i = 0; i < N; ++i) {
				lua_pushinteger(L, randValue());
				lua_setglobal(L, "value");
				lua_pushvalue(L, -1);
				lua_call(L, 0, 0);
			}
			auto finish = std::chrono::high_resolution_clock::now();
			luaTime = std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count() / 1000000.0;
		}
		});

	return false;
};

BEFORE_MAIN_ACTION(registerTests);