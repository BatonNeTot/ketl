/*🍲Ketl🍲*/
#include "speed_tests.h"

static auto registerTests = []() {

	registerSpeedTest("Function call", [](uint64_t N, double& ketlTime, double& luaTime) {
		Ketl::Allocator allocator;
		Ketl::Context context(allocator, 4096);
		Ketl::Compiler compiler;

		auto compilationResult = compiler.compile(R"(
			var testValue2 = 1 + 2;

			Int64 adder(in Int64 x, in Int64 y) {
				return x + y;
			}

			var testValue = adder(testValue2, 9);
		)", context);

		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return;
		}

		auto& command = std::get<0>(compilationResult);
		{
			auto start = std::chrono::high_resolution_clock::now();
			for (auto i = 0; i < N; ++i) {
				command();
			}
			auto finish = std::chrono::high_resolution_clock::now();
			ketlTime = std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count() / 1000000.0;
		}

		lua_State* L;
		L = luaL_newstate();

		luaL_loadstring(L, R"(
			local testValue2 = 1 + 2

			local function test(x, y)
				return x + y
			end

			local testValue = test(testValue2, 9)
		)");


		{
			auto start = std::chrono::high_resolution_clock::now();
			for (auto i = 0; i < N; ++i) {
				lua_pushvalue(L, -1);
				lua_call(L, 0, 0);
			}
			auto finish = std::chrono::high_resolution_clock::now();
			luaTime =  std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count() / 1000000.0;
		}
		});

	return false;
	};
	
BEFORE_MAIN_ACTION(registerTests);