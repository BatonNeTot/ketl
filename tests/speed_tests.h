/*🍲Ketl🍲*/
#ifndef speed_tests_h
#define speed_tests_h

#include "compile_tricks.h"

#include "compiler/compiler.h"
#include "ketl.h"
#include "context.h"
#include "type.h"

#include "lua.hpp"

#include <chrono>
#include <functional>

using SpeedTestFunction = std::function<void(uint64_t, double&, double&)>;

void registerSpeedTest(std::string_view&& name, SpeedTestFunction&& test);

void launchSpeedTests(uint64_t N);

#endif // speed_tests_h