/*🍲Ketl🍲*/
#include "speed_tests.h"

static auto registerTests = []() {

	registerSpeedTest("Fibonacci", [](uint64_t N, double& ketlTime, double& luaTime) {
		auto randValue = []() {
			return 10 + (rand() % 10);
		};

		Ketl::VirtualMachine vm(4096);

		int64_t index;
		vm.declareGlobal("index", &index);

		auto evaluationResult = vm.eval(R"(
			return () -> {
				var first = 0;
				var second = 1;
				var counter = 0;

				while (counter != index) {
					var third = first + second;
					first = second;
					second = third;
					counter = counter + 1;
				}
			};
		)");

		if (std::holds_alternative<std::string>(evaluationResult)) {
			std::cerr << std::get<std::string>(evaluationResult) << std::endl;
			return;
		}

		auto& command = std::get<0>(evaluationResult);
		{
			auto start = std::chrono::high_resolution_clock::now();
			for (auto i = 0; i < N; ++i) {
				index = randValue();
				command();
			}
			auto finish = std::chrono::high_resolution_clock::now();
			ketlTime = std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count() / 1000000.0;
		}

		lua_State* L;
		L = luaL_newstate();

		luaL_loadstring(L, R"(
			local first = 0
			local second = 1

			for i=1,(index - 1) do
				local third = first + second
				first = second
				second = third
			end
		)");


		{
			auto start = std::chrono::high_resolution_clock::now();
			for (auto i = 0; i < N; ++i) {
				lua_pushinteger(L, randValue());
				lua_setglobal(L, "index");
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