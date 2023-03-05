/*🍲Ketl🍲*/
#ifndef check_tests_h
#define check_tests_h

#include "compiler/compiler.h"
#include "ketl.h"
#include "context.h"
#include "type.h"

#include <vector>
#include <functional>

using TestContainer = std::vector<std::pair<std::string_view, std::function<bool()>>>;

void registerTest(std::string_view&& name, std::function<bool()>&& test);

#define BEFORE_MAIN_ACTION(action) \
namespace {\
	static bool success = action(); \
}

void launchCheckTests();

#endif // check_tests_h