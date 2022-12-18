/*🍲Ketl🍲*/
#include <iostream>
#include <vector>
#include <chrono>
#include <sstream>
#include <exception>
#include <functional>
#include <cassert>
#include <typeindex>

#include "compiler/parser_new.h"
#include "compiler/linker_new.h"
#include "ketl_new.h"

int main(int argc, char** argv) {

	Environment env;
	Linker linker;

	auto command = linker.proceed(env, R"({
	int test(int x) {
		ttestMe = x + 1;
	}

	testValue = (4 + 5 + 6) * 3;
	testValue2 = testValue/ 9;
})");

	auto& testValue = *env.getGlobalVariable<uint64_t>("testValue");
	auto& testValue2 = *env.getGlobalVariable<uint64_t>("testValue2");

	command.invoke();

	auto test = 0;

	//getc(stdin);
	return 0;
}