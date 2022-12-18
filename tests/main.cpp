/*🐟Ketl🐟*/
#include <iostream>
#include <vector>
#include <chrono>
#include <sstream>
#include <exception>
#include <functional>
#include <cassert>
#include <typeindex>

#include "test.h"

#include "eel_new.h"
#include "compiler/linker_new.h"


template <class T>
T add(const T& a, const T& b) {
	return a + b;
}

template <class T>
using Monoid = T(*)(const T&, const T&);

template <class T, Monoid<T> Action>
void instructionMonoid(void* output, void* const* argv, uint64_t argc) {
	*reinterpret_cast<T*>(output) = Action(*reinterpret_cast<T*>(argv[0]), *reinterpret_cast<T*>(argv[1]));
}


void testAll() {
	auto testCount = 1000000;
	double resultFlea = 0;

	{
		Environment env;

		//env.initGlobalVariable<int64_t>("c");
		//auto& c = *env.getGlobalVariable<int64_t>("c");
		//c = -16;

		Linker linker("c = 5 + 5;");
		auto function = linker.proceed(env);

		function();
		auto& c = *env.getGlobalVariable<int64_t>("c");
		assert(10 == c);

		auto start = std::chrono::high_resolution_clock::now();
		for (int counter = 0; counter < testCount; ++counter) {
			function();
		}

		auto finish = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> elapsed = finish - start;
		std::cout << "Result Ketl: " << elapsed.count() << std::endl;
		resultFlea = elapsed.count();
	}

	double resultLua = testLua(testCount, "result = 5.0 + 5");//*/ \n result2 = result + 5.0");
	std::cout << std::endl;
	std::cout << "Result Ketl / Lua " << resultFlea / resultLua << std::endl;
	std::cout << "Result Lua / Ketl " << resultLua / resultFlea << std::endl;
	std::cout << std::endl;
}

class TestClass {
public:
	int a;

	int getA() {
		return a;
	}
};

int main(int argc, char** argv) {

	void (*func)(void*, void*);

	std::cout << sizeof(func) << std::endl;

	testAll();
	//testLuaSpeed();

	getc(stdin);
	return 0;
}